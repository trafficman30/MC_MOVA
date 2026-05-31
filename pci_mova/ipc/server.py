"""
IPC server — runs inside a kernel process.

Two Unix domain sockets per stream:
  Push  /tmp/mova_{N}_live.sock  — kernel → web, continuous JSON lines
  Cmd   /tmp/mova_{N}_cmd.sock   — web → kernel, one-shot request/ack

Both sockets run on background threads.
Main thread (owned by the caller) runs the tick loop.

Message protocol:
  All messages carry {"v": VERSION, "t": "<type>", ...}

  Push message types:
    snap       — periodic full state snapshot (1–2 Hz)
    msg        — kernel decision message (immediate on arrival)
    err        — kernel error entry (immediate on arrival)
    <event>    — named state-change events (immediate, e.g. phase_change, fault)

  Command types (newline-terminated ASCII, one per connection):
    PING
    LOAD <path> <stream_id>
    UNLOAD
    SET_IO <index> <value>
    SET_DET <index> <value>
    SET_CONFIRM <index> <value>
    SET_CRB <value>
    FORCE_STAGE <stage>
    SWITCH_PLAN <plan_num>
    CONNECT_XKOP <host> <port>
    DISCONNECT_XKOP
    SET_SPEED <multiplier>
    SET_TOD_OFFSET <hours>
    RESET
"""

import json
import os
import socket
import threading
import time
from typing import Callable, Dict, List, Optional

VERSION = 1


class IPCServer:
    """
    IPC server for one kernel stream instance.

    Usage (in kernel_main.py):
        ipc = IPCServer(stream_index)
        ipc.set_command_handler(my_handler)
        ipc.start()
        # main thread: tick loop
        while running:
            stream.tick()
            ipc.update_snapshot(stream.snapshot())
            # publish events as they occur:
            ipc.publish_event("phase_change", from_stage=2, to_stage=4)
        ipc.stop()
    """

    SNAP_INTERVAL = 0.5  # seconds between periodic snapshots (2 Hz)

    def __init__(self, stream_index: int):
        self._idx = stream_index
        self._push_path = f"/tmp/mova_{stream_index}_live.sock"
        self._cmd_path = f"/tmp/mova_{stream_index}_cmd.sock"

        self._push_clients: List[socket.socket] = []
        self._push_lock = threading.Lock()

        self._cmd_handler: Optional[Callable] = None
        self._cmd_handler_lock = threading.Lock()

        self._last_snap: dict = {}
        self._snap_lock = threading.Lock()
        self._last_snap_ts: float = 0.0

        self._running = False
        self._push_srv: Optional[socket.socket] = None
        self._cmd_srv: Optional[socket.socket] = None
        self._push_thread: Optional[threading.Thread] = None
        self._cmd_thread: Optional[threading.Thread] = None

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def start(self) -> None:
        self._running = True
        for path in (self._push_path, self._cmd_path):
            try:
                os.unlink(path)
            except FileNotFoundError:
                pass

        self._push_thread = threading.Thread(
            target=self._serve_push, daemon=True,
            name=f"ipc-push-{self._idx}"
        )
        self._cmd_thread = threading.Thread(
            target=self._serve_cmd, daemon=True,
            name=f"ipc-cmd-{self._idx}"
        )
        self._push_thread.start()
        self._cmd_thread.start()

    def stop(self) -> None:
        self._running = False
        for srv in (self._push_srv, self._cmd_srv):
            if srv:
                try:
                    srv.close()
                except OSError:
                    pass
        with self._push_lock:
            for conn in self._push_clients:
                try:
                    conn.close()
                except OSError:
                    pass
            self._push_clients.clear()

    def set_command_handler(self, handler: Callable) -> None:
        """
        Register a function called for each incoming command.
        Signature: handler(cmd: str, args: list[str]) -> dict
        Return dict is merged with {"v": VERSION, "t": "ack", "cmd": cmd}.
        Return {"t": "pong"} for PING to override the default type.
        """
        with self._cmd_handler_lock:
            self._cmd_handler = handler

    # ------------------------------------------------------------------
    # Push API — called from the tick loop (main thread)
    # ------------------------------------------------------------------

    def update_snapshot(self, snap: dict) -> None:
        """
        Called every tick. Publishes a full snapshot at SNAP_INTERVAL Hz.
        Does NOT publish on every tick — use publish_event for low-latency changes.
        """
        with self._snap_lock:
            self._last_snap = snap
        now = time.monotonic()
        if now - self._last_snap_ts >= self.SNAP_INTERVAL:
            self._last_snap_ts = now
            self._push({"v": VERSION, "t": "snap", "ts": time.time(), **snap})

    def publish_event(self, event_type: str, **kwargs) -> None:
        """
        Publish an immediate named event — does not wait for the next snapshot.
        Use for state changes that matter in real time:
            ipc.publish_event("phase_change", from_stage=2, to_stage=4)
            ipc.publish_event("fault", error_id=5, data=0)
            ipc.publish_event("on_control", value=1)
            ipc.publish_event("mova_on", value=1)
            ipc.publish_event("stage_forced", stage=3)
            ipc.publish_event("plan_switched", plan=2)
        """
        self._push({"v": VERSION, "t": event_type, "ts": time.time(), **kwargs})

    def publish_message(self, msg: dict) -> None:
        """Publish a kernel decision message immediately."""
        self._push({"v": VERSION, "t": "msg", "ts": time.time(), **msg})

    def publish_error(self, error: dict) -> None:
        """Publish a kernel error entry immediately."""
        self._push({"v": VERSION, "t": "err", "ts": time.time(), **error})

    # ------------------------------------------------------------------
    # Internal push
    # ------------------------------------------------------------------

    def _push(self, payload: dict) -> None:
        line = (json.dumps(payload, separators=(',', ':')) + "\n").encode()
        dead: List[socket.socket] = []
        with self._push_lock:
            for conn in self._push_clients:
                try:
                    conn.sendall(line)
                except OSError:
                    dead.append(conn)
            for conn in dead:
                self._push_clients.remove(conn)
                try:
                    conn.close()
                except OSError:
                    pass

    # ------------------------------------------------------------------
    # Push server thread
    # ------------------------------------------------------------------

    def _serve_push(self) -> None:
        srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            srv.bind(self._push_path)
            srv.listen(8)
            srv.settimeout(1.0)
            self._push_srv = srv
            while self._running:
                try:
                    conn, _ = srv.accept()
                except socket.timeout:
                    continue
                # Send current snapshot immediately so new client isn't blind
                with self._snap_lock:
                    snap = self._last_snap.copy()
                if snap:
                    try:
                        line = (json.dumps(
                            {"v": VERSION, "t": "snap", "ts": time.time(), **snap},
                            separators=(',', ':')
                        ) + "\n").encode()
                        conn.sendall(line)
                    except OSError:
                        try:
                            conn.close()
                        except OSError:
                            pass
                        continue
                with self._push_lock:
                    self._push_clients.append(conn)
        finally:
            try:
                srv.close()
            except OSError:
                pass

    # ------------------------------------------------------------------
    # Command server thread
    # ------------------------------------------------------------------

    def _serve_cmd(self) -> None:
        srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            srv.bind(self._cmd_path)
            srv.listen(4)
            srv.settimeout(1.0)
            self._cmd_srv = srv
            while self._running:
                try:
                    conn, _ = srv.accept()
                except socket.timeout:
                    continue
                t = threading.Thread(
                    target=self._handle_cmd, args=(conn,),
                    daemon=True, name=f"ipc-cmd-conn-{self._idx}"
                )
                t.start()
        finally:
            try:
                srv.close()
            except OSError:
                pass

    def _handle_cmd(self, conn: socket.socket) -> None:
        try:
            conn.settimeout(5.0)
            buf = b""
            while b"\n" not in buf:
                chunk = conn.recv(512)
                if not chunk:
                    return
                buf += chunk
            line = buf.split(b"\n")[0].decode("utf-8", errors="replace").strip()
            parts = line.split()
            if not parts:
                return
            cmd = parts[0].upper()
            args = parts[1:]

            if cmd == "PING":
                resp: dict = {"v": VERSION, "t": "pong"}
            else:
                with self._cmd_handler_lock:
                    handler = self._cmd_handler
                if handler:
                    try:
                        result = handler(cmd, args)
                        if "v" not in result:
                            result["v"] = VERSION
                        if "t" not in result:
                            result["t"] = "ack"
                        if "cmd" not in result:
                            result["cmd"] = cmd
                        resp = result
                    except Exception as exc:
                        resp = {"v": VERSION, "t": "ack", "cmd": cmd, "ok": False, "err": str(exc)}
                else:
                    resp = {"v": VERSION, "t": "ack", "cmd": cmd, "ok": False, "err": "no handler"}

            conn.sendall((json.dumps(resp, separators=(',', ':')) + "\n").encode())
        except (OSError, socket.timeout):
            pass
        finally:
            try:
                conn.close()
            except OSError:
                pass
