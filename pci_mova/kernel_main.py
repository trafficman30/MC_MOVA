"""
Kernel process entry point — one process per stream.

Usage:
    python -m pci_mova.kernel_main <stream_index>

Threading model:
    main thread       — event loop: IPC snapshot publishing, SIGTERM handling
    stream tick thread — MovaStream._loop() 10Hz (daemon, started by load_dataset)
    ipc-push thread   — IPCServer push socket (daemon)
    ipc-cmd thread    — IPCServer command socket (daemon)
    mova-tools thread — asyncio event loop for AML/TCP server (daemon)

SIGTERM fires on the main thread → stop_event set → clean shutdown.
"""

import asyncio
import logging
import os
import signal
import sys
import threading
import time

log = logging.getLogger(__name__)


# ── MOVA Tools server — asyncio in its own thread ──────────────────────────

def _start_mova_tools(stream) -> threading.Thread:
    """Run the MOVA Tools async server on a private event loop in a background thread."""
    from pci_mova.protocol.mova_tools import MovaToolsServer

    def _run():
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        server = MovaToolsServer(stream)
        try:
            loop.run_until_complete(server.start())
            loop.run_forever()
        finally:
            loop.run_until_complete(server.stop())
            loop.close()

    t = threading.Thread(target=_run, daemon=True, name=f"mova-tools-{stream.stream_id}")
    t.start()
    return t


# ── Dataset state persistence (single-stream variant) ──────────────────────

def _state_path(stream_index: int) -> str:
    from pci_mova.config import DATASET_DIR
    return os.path.join(DATASET_DIR, f".stream_state_{stream_index}.json")


def _save_state(stream, stream_index: int) -> None:
    import json
    from pci_mova.config import DATASET_DIR
    ds = stream._dataset
    if ds is None or not ds.source_path or not os.path.isfile(ds.source_path):
        return
    state = {
        "filename":  os.path.basename(ds.source_path),
        "stream_id": ds.header.stream_id_str,
        "mova_on":   int(stream.buffers.io[27]),
        "running":   int(stream._running.is_set()),
    }
    try:
        import json as _json
        with open(_state_path(stream_index), "w") as fh:
            _json.dump(state, fh, indent=2)
        log.debug("Stream %d: state saved", stream_index)
    except OSError as exc:
        log.warning("Stream %d: could not save state: %s", stream_index, exc)


def _restore_state(stream, stream_index: int) -> None:
    import json as _json
    path = _state_path(stream_index)
    if not os.path.isfile(path):
        return
    try:
        with open(path) as fh:
            state = _json.load(fh)
    except Exception as exc:
        log.warning("Stream %d: could not read state: %s", stream_index, exc)
        return

    from pci_mova.config import DATASET_DIR
    from pci_mova.dataset.parser import load_all
    filename = state.get("filename", "")
    ctrl_id  = state.get("stream_id", "")
    mova_on  = state.get("mova_on", 0)

    ds_path = os.path.join(DATASET_DIR, filename)
    if not os.path.isfile(ds_path):
        log.warning("Stream %d: saved dataset '%s' not found — skipping restore",
                    stream_index, filename)
        return
    try:
        all_streams = load_all(ds_path)
    except Exception as exc:
        log.error("Stream %d: dataset parse failed: %s", stream_index, exc)
        return
    if ctrl_id not in all_streams:
        log.warning("Stream %d: controller stream '%s' not in '%s' — skipping restore",
                    stream_index, ctrl_id, filename)
        return
    was_running = bool(state.get("running", 0))
    ok = stream.load_dataset(all_streams[ctrl_id], auto_start=was_running)
    if ok:
        from pci_mova.core.io.simulated import SimulatedIO as _Sim
        if isinstance(stream.io, _Sim):
            stream.io.auto_follow = True
        log.info("Stream %d: restored dataset '%s' (%s)%s", stream_index, filename, ctrl_id,
                 " (started)" if was_running else " (idle)")
        if mova_on:
            stream.buffers.io[27] = 1
            log.info("Stream %d: MOVA_ON restored", stream_index)
    else:
        log.warning("Stream %d: kernel rejected restored dataset '%s'", stream_index, filename)


# ── Signal map / IO config ─────────────────────────────────────────────────

_STREAMS_JSON = os.path.join(os.path.dirname(__file__), "..", "config", "streams.json")


def _load_streams_json() -> dict:
    import json as _json
    path = os.path.abspath(_STREAMS_JSON)
    if not os.path.isfile(path):
        return {}
    try:
        with open(path) as fh:
            data = _json.load(fh)
        return {k: v for k, v in data.items() if not k.startswith("_")}
    except Exception as exc:
        log.warning("streams.json read error: %s", exc)
        return {}


def _save_streams_json(data: dict) -> None:
    import json as _json
    path = os.path.abspath(_STREAMS_JSON)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    try:
        existing = _load_streams_json()
        existing.update(data)
        with open(path, "w") as fh:
            _json.dump(existing, fh, indent=2)
    except Exception as exc:
        log.warning("streams.json write error: %s", exc)


def _ensure_signal_map(stream_index: int, dataset) -> None:
    """Auto-generate standard signal map from dataset if not already present."""
    from pci_mova.core.io.cm5_io import default_signal_map
    cfg = _load_streams_json()
    key = str(stream_index)
    if key in cfg and "det_map" in cfg[key].get("io", {}):
        return   # already configured — don't overwrite user edits
    sig_map = default_signal_map(dataset)
    _save_streams_json({key: {"io": sig_map}})
    log.info("Stream %d: signal map auto-generated (%d dets, %d confirms)",
             stream_index, len(sig_map["det_map"]), len(sig_map["confirm_map"]))


def _build_cm5_io(stream_index: int) -> object:
    """
    Build CM5IO from streams.json config for this stream.
    Returns None if no config present or socket unavailable.
    """
    from pci_mova.core.io.cm5_io import CM5IO, IOBUS_SOCKET
    cfg = _load_streams_json()
    io_cfg = cfg.get(str(stream_index), {}).get("io", {})
    if not io_cfg or io_cfg.get("type") not in ("cm5", None):
        return None
    socket_path = io_cfg.get("socket", IOBUS_SOCKET)
    return CM5IO(io_cfg, stream_id=stream_index, socket_path=socket_path)


# ── IPC command handler ─────────────────────────────────────────────────────

def _make_cmd_handler(stream, stream_index: int, ipc, save_state_fn):
    """
    Returns a function(cmd, args) -> dict for the IPC command socket.
    Called from the IPC cmd thread — uses GIL-safe operations only.
    """
    from pci_mova.config import DATASET_DIR
    from pci_mova.core.io.simulated import SimulatedIO
    from pci_mova.core.io.xkop_io import XKOPio
    from pci_mova.core.io.cm5_io import CM5IO, default_signal_map

    def handler(cmd: str, args: list) -> dict:
        try:
            if cmd == "LOAD":
                if len(args) < 2:
                    return {"t": "ack", "cmd": cmd, "ok": False, "err": "LOAD requires <path> <stream_id>"}
                ds_path, ctrl_id = args[0], args[1]
                from pci_mova.dataset.parser import load_all, DatasetError, validate_sc_rules
                try:
                    validate_sc_rules(ds_path)
                    all_streams = load_all(ds_path)
                except DatasetError as exc:
                    return {"t": "ack", "cmd": cmd, "ok": False, "err": str(exc)}
                if ctrl_id not in all_streams:
                    return {"t": "ack", "cmd": cmd, "ok": False,
                            "err": f"Controller stream '{ctrl_id}' not found in dataset"}
                if stream._thread and stream._thread.is_alive():
                    stream.stop()
                    time.sleep(0.15)
                ok = stream.load_dataset(all_streams[ctrl_id])
                if ok:
                    if isinstance(stream.io, SimulatedIO):
                        stream.io.auto_follow = True
                    # Auto-generate signal map if not already configured
                    _ensure_signal_map(stream_index, all_streams[ctrl_id])
                    save_state_fn()
                    ipc.publish_event("dataset_loaded", path=ds_path, ctrl_id=ctrl_id)
                return {"t": "ack", "cmd": cmd, "ok": ok,
                        "err": "" if ok else "kernel rejected dataset"}

            elif cmd == "UNLOAD":
                stream.stop()
                stream._dataset = None
                save_state_fn()
                ipc.publish_event("dataset_unloaded")
                return {"t": "ack", "cmd": cmd, "ok": True}

            elif cmd == "SET_IO":
                if len(args) < 2:
                    return {"t": "ack", "cmd": cmd, "ok": False, "err": "SET_IO requires <index> <value>"}
                idx, val = int(args[0]), int(args[1])
                if idx == 19:  # ON_CONTROL — kernel-owned
                    return {"t": "ack", "cmd": cmd, "ok": False,
                            "err": "ON_CONTROL is kernel-owned"}
                if idx == 27 and val == 0:  # MOVA_ON off → reset warmup
                    stream.kernel.set_warmup_counter(-1)
                stream.buffers.io[idx] = val
                save_state_fn()
                ipc.publish_event("io_set", index=idx, value=val)
                return {"t": "ack", "cmd": cmd, "ok": True}

            elif cmd == "SET_DET":
                if len(args) < 2:
                    return {"t": "ack", "cmd": cmd, "ok": False, "err": "SET_DET requires <index> <value>"}
                idx, val = int(args[0]), int(args[1])
                if hasattr(stream.io, 'set_detector'):
                    stream.io.set_detector(idx, val)
                else:
                    stream.buffers.din[idx] = val
                return {"t": "ack", "cmd": cmd, "ok": True}

            elif cmd == "SET_CONFIRM":
                if len(args) < 2:
                    return {"t": "ack", "cmd": cmd, "ok": False, "err": "SET_CONFIRM requires <index> <value>"}
                idx, val = int(args[0]), int(args[1])
                if hasattr(stream.io, 'set_confirm'):
                    stream.io.set_confirm(idx, val)
                else:
                    stream.buffers.confin[idx] = val
                return {"t": "ack", "cmd": cmd, "ok": True}

            elif cmd == "SET_CRB":
                if len(args) < 1:
                    return {"t": "ack", "cmd": cmd, "ok": False, "err": "SET_CRB requires <value>"}
                val = bool(int(args[0]))
                if hasattr(stream.io, 'set_crb'):
                    stream.io.set_crb(val)
                else:
                    stream.buffers.crb = val
                return {"t": "ack", "cmd": cmd, "ok": True}

            elif cmd == "FORCE_STAGE":
                if len(args) < 1:
                    return {"t": "ack", "cmd": cmd, "ok": False, "err": "FORCE_STAGE requires <stage>"}
                stream.force_stage(int(args[0]))
                ipc.publish_event("stage_forced", stage=int(args[0]))
                return {"t": "ack", "cmd": cmd, "ok": True}

            elif cmd == "SWITCH_PLAN":
                if len(args) < 1:
                    return {"t": "ack", "cmd": cmd, "ok": False, "err": "SWITCH_PLAN requires <plan_num>"}
                plan = int(args[0])
                ok = stream.kernel.switch_plan(plan)
                if ok:
                    ipc.publish_event("plan_switched", plan=plan)
                return {"t": "ack", "cmd": cmd, "ok": ok}

            elif cmd == "CONNECT_XKOP":
                if len(args) < 2:
                    return {"t": "ack", "cmd": cmd, "ok": False, "err": "CONNECT_XKOP requires <host> <port>"}
                host, port = args[0], int(args[1])
                old_io = stream.io
                new_io = XKOPio(host, port, stream.stream_id)
                if stream._dataset:
                    c = stream._dataset.constants
                    new_io.reset_confirms(c.stagon)
                    ig_matrix = {(ig.from_stage, ig.to_stage): ig.value
                                 for ig in stream._dataset.interstages}
                    new_io.set_intergreen_matrix(ig_matrix)
                stream.io = new_io
                new_io.start()
                if hasattr(old_io, 'stop'):
                    old_io.stop()
                ipc.publish_event("xkop_connected", host=host, port=port)
                return {"t": "ack", "cmd": cmd, "ok": True}

            elif cmd == "DISCONNECT_XKOP":
                if hasattr(stream.io, 'stop'):
                    stream.io.stop()
                new_io = SimulatedIO()
                if stream._dataset:
                    c = stream._dataset.constants
                    new_io.reset_confirms(c.stagon)
                    ig_matrix = {(ig.from_stage, ig.to_stage): ig.value
                                 for ig in stream._dataset.interstages}
                    new_io.set_intergreen_matrix(ig_matrix)
                stream.io = new_io
                ipc.publish_event("xkop_disconnected")
                return {"t": "ack", "cmd": cmd, "ok": True}

            elif cmd == "SET_SPEED":
                if len(args) < 1:
                    return {"t": "ack", "cmd": cmd, "ok": False, "err": "SET_SPEED requires <multiplier>"}
                n = int(args[0])
                if n not in (1, 2, 5):
                    return {"t": "ack", "cmd": cmd, "ok": False, "err": "speed must be 1, 2, or 5"}
                stream.kernel.set_speed(n)
                if isinstance(stream.io, SimulatedIO):
                    stream.io.speed_multiplier = n
                return {"t": "ack", "cmd": cmd, "ok": True}

            elif cmd == "SET_TOD_OFFSET":
                if len(args) < 1:
                    return {"t": "ack", "cmd": cmd, "ok": False, "err": "SET_TOD_OFFSET requires <hours>"}
                stream.kernel.time_offset_hours = float(args[0])
                return {"t": "ack", "cmd": cmd, "ok": True}

            elif cmd == "SET_AUTO_FOLLOW":
                if len(args) < 1:
                    return {"t": "ack", "cmd": cmd, "ok": False, "err": "SET_AUTO_FOLLOW requires <0|1>"}
                if isinstance(stream.io, SimulatedIO):
                    stream.io.auto_follow = bool(int(args[0]))
                return {"t": "ack", "cmd": cmd, "ok": True}

            elif cmd == "START":
                if stream._dataset and not stream._running.is_set():
                    stream.start()
                    ipc.publish_event("stream_started")
                save_state_fn()
                return {"t": "ack", "cmd": cmd, "ok": True}

            elif cmd == "STOP":
                threading.Thread(target=lambda: (stream.stop(), ipc.publish_event("stream_stopped"), save_state_fn()), daemon=True).start()
                return {"t": "ack", "cmd": cmd, "ok": True}

            elif cmd == "RESET":
                stream.stop()
                time.sleep(0.15)
                if stream._dataset:
                    stream.kernel.load_dataset(stream._dataset)
                    stream.buffers.io[27] = 0
                    stream.buffers.io[19] = 0
                    stream.buffers.io[16] = 0
                    stream.start()
                    ipc.publish_event("stream_reset")
                save_state_fn()
                return {"t": "ack", "cmd": cmd, "ok": True}

            else:
                return {"t": "ack", "cmd": cmd, "ok": False, "err": f"unknown command: {cmd}"}

        except Exception as exc:
            log.exception("Stream %d: command %s error", stream_index, cmd)
            return {"t": "ack", "cmd": cmd, "ok": False, "err": str(exc)}

    return handler


# ── Main ────────────────────────────────────────────────────────────────────

def main() -> None:
    if len(sys.argv) < 2:
        print("Usage: python -m pci_mova.kernel_main <stream_index>", file=sys.stderr)
        sys.exit(1)

    try:
        stream_index = int(sys.argv[1])
    except ValueError:
        print(f"Invalid stream index: {sys.argv[1]}", file=sys.stderr)
        sys.exit(1)

    if not (0 <= stream_index <= 7):
        print(f"Stream index must be 0–7, got {stream_index}", file=sys.stderr)
        sys.exit(1)

    # ── Logging ───────────────────────────────────────────────────────────────
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(name)s %(levelname)s %(message)s"
    )
    log.info("kernel stream %d: starting", stream_index)

    # ── Licence check ─────────────────────────────────────────────────────────
    from pci_mova.licence.licence import validate
    licence = validate()
    if not licence.allows_stream(stream_index):
        log.info("kernel stream %d: not licensed — exiting cleanly", stream_index)
        sys.exit(0)  # clean exit — Restart=on-failure does not loop on code 0
    log.info("kernel stream %d: licence OK (max %d streams)", stream_index, licence.max_streams)

    # ── IPC server ────────────────────────────────────────────────────────────
    from pci_mova.ipc.server import IPCServer
    ipc = IPCServer(stream_index)

    # ── IO layer — CM5 IOBus if configured, else SimulatedIO ──────────────────
    io_layer = _build_cm5_io(stream_index)
    if io_layer:
        log.info("kernel stream %d: using CM5IO", stream_index)
    else:
        from pci_mova.core.io.simulated import SimulatedIO
        io_layer = SimulatedIO()
        log.info("kernel stream %d: no CM5 config — using SimulatedIO", stream_index)

    # ── Stream ────────────────────────────────────────────────────────────────
    from pci_mova.core.stream import MovaStream
    from pci_mova.core.model.enums import MovaStatus

    def _on_fault(stream, fault_dict):
        ipc.publish_error(fault_dict)

    stream = MovaStream(
        stream_id = stream_index,
        io_layer  = io_layer,
        on_fault  = _on_fault,
    )

    def save_state():
        _save_state(stream, stream_index)

    # ── Wire IPC ──────────────────────────────────────────────────────────────
    ipc.set_command_handler(_make_cmd_handler(stream, stream_index, ipc, save_state))
    ipc.start()

    # ── Restore state ─────────────────────────────────────────────────────────
    _restore_state(stream, stream_index)
    if stream._dataset is None:
        from pci_mova.core.model.enums import MovaStatus
        stream.status.set(MovaStatus.OFF)

    # ── MOVA Tools server ─────────────────────────────────────────────────────
    _start_mova_tools(stream)
    log.info("kernel stream %d: MOVA Tools server on port %d",
             stream_index, 6000 + stream_index)

    # ── Signal handling (main thread) ─────────────────────────────────────────
    stop_event = threading.Event()

    def on_signal(signum, frame):
        log.info("kernel stream %d: signal %d — stopping", stream_index, signum)
        stop_event.set()

    signal.signal(signal.SIGTERM, on_signal)
    signal.signal(signal.SIGINT, on_signal)

    log.info("kernel stream %d: ready (IPC push=%s cmd=%s)",
             stream_index, ipc._push_path, ipc._cmd_path)

    # ── Main loop: publish IPC events + snapshots ─────────────────────────────
    # Track previous state to detect changes and emit immediate events.
    prev_cs       = -1
    prev_on_ctrl  = -1
    prev_mova_on  = -1
    prev_status   = None
    prev_ec       = -1
    last_msg_seq  = 0

    while not stop_event.is_set():
        snap = stream.snapshot()

        buf      = snap.get("buffers", {})
        io_arr   = buf.get("io", [])
        cs       = buf.get("kernel_current_stage", 0)
        on_ctrl  = int(io_arr[19]) if len(io_arr) > 19 else 0
        mova_on  = int(io_arr[27]) if len(io_arr) > 27 else 0
        ec       = int(io_arr[16]) if len(io_arr) > 16 else 0
        status   = snap.get("status", {}).get("current")

        # Emit immediate events on state transitions
        if prev_cs >= 0 and cs != prev_cs:
            ipc.publish_event("phase_change", from_stage=prev_cs, to_stage=cs)

        if prev_on_ctrl >= 0 and on_ctrl != prev_on_ctrl:
            ipc.publish_event("on_control", value=on_ctrl)

        if prev_mova_on >= 0 and mova_on != prev_mova_on:
            ipc.publish_event("mova_on", value=mova_on)

        if prev_status is not None and status != prev_status:
            ipc.publish_event("status_change", status=status)

        if prev_ec >= 0 and ec != prev_ec:
            ipc.publish_event("ec_change", ec=ec)

        # Drain new kernel decision messages
        new_msgs = stream.messages_since(last_msg_seq)
        for msg in new_msgs:
            ipc.publish_message(msg)
        if new_msgs:
            last_msg_seq = new_msgs[-1]["seq"]

        # Periodic snapshot (rate-limited inside IPCServer to 2 Hz)
        ipc.update_snapshot(snap)

        prev_cs      = cs
        prev_on_ctrl = on_ctrl
        prev_mova_on = mova_on
        prev_status  = status
        prev_ec      = ec

        stop_event.wait(0.1)

    # ── Shutdown ──────────────────────────────────────────────────────────────
    log.info("kernel stream %d: shutting down", stream_index)
    save_state()
    stream.stop()
    ipc.stop()
    log.info("kernel stream %d: stopped", stream_index)


if __name__ == "__main__":
    main()
