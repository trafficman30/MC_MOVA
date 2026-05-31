"""
IPC client — runs inside the web process.

KernelClient: connects to one kernel stream's push + command sockets.
KernelRegistry: holds one KernelClient per licensed stream (0–7).

Push socket reader runs on a background thread.
send_command() opens a short-lived connection per call (thread-safe).

Message types handled:
  snap        — updates latest_snapshot
  msg         — appended to messages deque (maxlen 500)
  err         — appended to errors deque (maxlen 200)
  <other>     — immediate event, forwarded to registered callbacks
"""

import json
import socket
import threading
import time
from typing import Callable, Dict, Deque, List, Optional
from collections import deque

VERSION = 1
_CONNECT_TIMEOUT = 5.0
_CMD_TIMEOUT = 5.0
_MAX_MESSAGES = 500
_MAX_ERRORS = 200
_RECONNECT_BASE = 1.0
_RECONNECT_CAP = 30.0


class KernelClient:
    """
    Maintains a persistent push-socket reader and provides
    send_command() for one-shot kernel control calls.
    """

    def __init__(self, stream_index: int):
        self._idx = stream_index
        self._push_path = f"/tmp/mova_{stream_index}_live.sock"
        self._cmd_path = f"/tmp/mova_{stream_index}_cmd.sock"

        self._snap: dict = {}
        self._snap_lock = threading.Lock()
        self._snap_ts: float = 0.0  # monotonic time of last snap received

        self._messages: Deque[dict] = deque(maxlen=_MAX_MESSAGES)
        self._msg_lock = threading.Lock()

        self._errors: Deque[dict] = deque(maxlen=_MAX_ERRORS)
        self._err_lock = threading.Lock()

        self._event_cbs: List[Callable] = []
        self._cb_lock = threading.Lock()

        self._connected = False
        self._running = False
        self._reader: Optional[threading.Thread] = None

    # ------------------------------------------------------------------
    # Properties
    # ------------------------------------------------------------------

    @property
    def stream_index(self) -> int:
        return self._idx

    @property
    def connected(self) -> bool:
        """True if the push socket reader is currently connected to the kernel."""
        return self._connected

    @property
    def stale(self) -> bool:
        """True if no snapshot received in the last 5 seconds."""
        return self._snap_ts > 0 and (time.monotonic() - self._snap_ts) > 5.0

    # ------------------------------------------------------------------
    # Lifecycle
    # ------------------------------------------------------------------

    def start(self) -> None:
        self._running = True
        self._reader = threading.Thread(
            target=self._reader_loop, daemon=True,
            name=f"ipc-reader-{self._idx}"
        )
        self._reader.start()

    def stop(self) -> None:
        self._running = False

    # ------------------------------------------------------------------
    # State access (thread-safe)
    # ------------------------------------------------------------------

    def latest_snapshot(self) -> dict:
        with self._snap_lock:
            return dict(self._snap)

    def messages_since(self, seq: int) -> List[dict]:
        with self._msg_lock:
            return [m for m in self._messages if m.get("seq", 0) > seq]

    def all_messages(self) -> List[dict]:
        with self._msg_lock:
            return list(self._messages)

    def recent_errors(self) -> List[dict]:
        with self._err_lock:
            return list(self._errors)

    # ------------------------------------------------------------------
    # Event callbacks
    # ------------------------------------------------------------------

    def on_event(self, callback: Callable[[dict], None]) -> None:
        """
        Register a callback for immediate events (phase_change, fault, etc.).
        Called from the reader thread — keep it fast, no blocking.
        """
        with self._cb_lock:
            self._event_cbs.append(callback)

    # ------------------------------------------------------------------
    # Command interface
    # ------------------------------------------------------------------

    def send_command(self, cmd: str, *args) -> dict:
        """
        Send a command to the kernel and return the ack dict.
        Opens a fresh connection for each call — safe to call from any thread.

        Returns {"v": 1, "t": "ack", "ok": False, "err": "..."} on failure.
        """
        line = " ".join([cmd] + [str(a) for a in args]) + "\n"
        try:
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
                s.settimeout(_CMD_TIMEOUT)
                s.connect(self._cmd_path)
                s.sendall(line.encode())
                buf = b""
                while b"\n" not in buf:
                    chunk = s.recv(4096)
                    if not chunk:
                        break
                    buf += chunk
                return json.loads(buf.split(b"\n")[0])
        except (OSError, socket.timeout, json.JSONDecodeError) as exc:
            return {"v": VERSION, "t": "ack", "cmd": cmd, "ok": False, "err": str(exc)}

    def ping(self) -> bool:
        """Returns True if the kernel responds to PING."""
        resp = self.send_command("PING")
        return resp.get("t") == "pong"

    # ------------------------------------------------------------------
    # Reader loop (background thread)
    # ------------------------------------------------------------------

    def _reader_loop(self) -> None:
        backoff = _RECONNECT_BASE
        while self._running:
            try:
                with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
                    s.settimeout(_CONNECT_TIMEOUT)
                    s.connect(self._push_path)
                    s.settimeout(None)  # blocking reads after connect
                    self._connected = True
                    backoff = _RECONNECT_BASE
                    buf = b""
                    while self._running:
                        chunk = s.recv(65536)
                        if not chunk:
                            break
                        buf += chunk
                        while b"\n" in buf:
                            line, buf = buf.split(b"\n", 1)
                            if line:
                                self._handle_line(line)
            except OSError:
                pass
            finally:
                self._connected = False
            if self._running:
                time.sleep(backoff)
                backoff = min(backoff * 2.0, _RECONNECT_CAP)

    def _handle_line(self, raw: bytes) -> None:
        try:
            msg = json.loads(raw)
        except json.JSONDecodeError:
            return
        t = msg.get("t")
        if t == "snap":
            with self._snap_lock:
                self._snap = msg
            self._snap_ts = time.monotonic()
        elif t == "msg":
            with self._msg_lock:
                self._messages.append(msg)
        elif t == "err":
            with self._err_lock:
                self._errors.append(msg)
        else:
            # Immediate event — notify callbacks
            with self._cb_lock:
                for cb in list(self._event_cbs):
                    try:
                        cb(msg)
                    except Exception:
                        pass


class KernelRegistry:
    """
    Holds one KernelClient per stream (0–7).
    Used by the web service to access all kernel state.

    Example:
        registry = KernelRegistry(stream_count=4)
        registry.start_all()
        snap = registry[0].latest_snapshot()
        ok = registry[1].send_command("LOAD", "/path/to.mxds", "J14861")
    """

    def __init__(self, stream_count: int = 8):
        self._clients: Dict[int, KernelClient] = {
            i: KernelClient(i) for i in range(stream_count)
        }

    def start_all(self) -> None:
        for client in self._clients.values():
            client.start()

    def stop_all(self) -> None:
        for client in self._clients.values():
            client.stop()

    def get(self, stream_index: int) -> Optional[KernelClient]:
        return self._clients.get(stream_index)

    def __getitem__(self, stream_index: int) -> KernelClient:
        return self._clients[stream_index]

    def connected_streams(self) -> List[int]:
        """Return indices of streams currently connected to a kernel process."""
        return [i for i, c in self._clients.items() if c.connected]

    def all_clients(self) -> Dict[int, KernelClient]:
        return dict(self._clients)
