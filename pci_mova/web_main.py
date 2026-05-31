"""
Web process entry point — one process, serves all streams.

Usage:
    python -m pci_mova.web_main

Starts KernelRegistry (connects to all kernel IPC sockets),
then launches FastAPI + uvicorn.

Current state: Phase 3 stub — registry wires up, no HTTP routes yet.
"""

import logging
import sys

log = logging.getLogger(__name__)


def main() -> None:
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(name)s %(levelname)s %(message)s"
    )
    log.info("web: starting")

    from pci_mova.ipc.client import KernelRegistry

    # Determine stream count from licence (Phase 3: read from licence file)
    stream_count = 8
    registry = KernelRegistry(stream_count=stream_count)
    registry.start_all()

    connected = registry.connected_streams()
    log.info("web: KernelRegistry started, connected streams: %s", connected)

    # Phase 3 will mount FastAPI routes and WebSocket handlers here.
    # For now, keep the process alive so the registry reader threads run.
    import threading
    import time

    stop = threading.Event()
    try:
        while not stop.is_set():
            time.sleep(5)
            log.debug("web: connected streams = %s", registry.connected_streams())
    except KeyboardInterrupt:
        pass
    finally:
        registry.stop_all()
        log.info("web: stopped")


if __name__ == "__main__":
    main()
