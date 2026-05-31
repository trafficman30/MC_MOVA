"""
Kernel process entry point — one process per stream.

Usage:
    python -m pci_mova.kernel_main <stream_index>

Main thread owns the tick loop (SIGTERM lands here for clean shutdown).
IPC server runs on background threads.
MOVA Tools TCP server runs on a background thread.

Current state: Phase 1 stub — IPC sockets work, no kernel attached.
"""

import logging
import signal
import sys
import threading
import time

log = logging.getLogger(__name__)


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

    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(name)s %(levelname)s %(message)s"
    )
    log.info("kernel stream %d: starting", stream_index)

    # --- Phase 1: IPC layer only ---
    from pci_mova.ipc.server import IPCServer

    ipc = IPCServer(stream_index)

    def handle_command(cmd: str, args: list) -> dict:
        """Stub handler — Phase 2 will attach the real MovaStream here."""
        log.debug("kernel %d: cmd %s %s", stream_index, cmd, args)
        return {"t": "ack", "cmd": cmd, "ok": False, "err": "kernel not yet attached (Phase 1 stub)"}

    ipc.set_command_handler(handle_command)
    ipc.start()
    log.info("kernel stream %d: IPC sockets ready (push=%s cmd=%s)",
             stream_index, ipc._push_path, ipc._cmd_path)

    # --- Signal handling (main thread required for signal.signal) ---
    stop_event = threading.Event()

    def on_signal(signum, frame):
        log.info("kernel stream %d: signal %d received, stopping", stream_index, signum)
        stop_event.set()

    signal.signal(signal.SIGTERM, on_signal)
    signal.signal(signal.SIGINT, on_signal)

    # --- Main thread: tick loop ---
    # Phase 2 will replace this stub with the real MovaStream._loop()
    tick = 0
    while not stop_event.is_set():
        tick += 1
        # Publish a periodic snapshot so clients can verify connectivity
        ipc.update_snapshot({
            "stream": stream_index,
            "tick": tick,
            "status": "stub",
            "ts_wall": time.time(),
        })
        # Phase 1: no state changes, no events to publish
        stop_event.wait(0.1)  # 100ms tick, interruptible

    # --- Shutdown ---
    ipc.stop()
    log.info("kernel stream %d: stopped", stream_index)


if __name__ == "__main__":
    main()
