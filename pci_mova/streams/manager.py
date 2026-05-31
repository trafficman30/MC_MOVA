"""
pci_mova.streams.manager
StreamManager — lifecycle management for all MOVA streams.

Enforces the licenced stream count.
Provides the single registry that the API and protocol layers reference.
"""

import asyncio
import json
import logging
import os
import threading
from typing import Callable, Optional

from pci_mova.core.stream import MovaStream
from pci_mova.core.model.enums import MovaStatus
from pci_mova.core.io.simulated import SimulatedIO
try:
    from pci_mova.core.io.cm5_io import CM5IO
except ImportError:
    CM5IO = None
from pci_mova.licence.licence import LicenceResult
from pci_mova.protocol.mova_tools import MovaToolsServer
from pci_mova.config import MAX_STREAMS

logger = logging.getLogger(__name__)


class StreamManager:
    """
    Owns all MovaStream instances for this process.

    Responsibilities:
      - Enforce licence stream limit
      - Create / start / stop individual streams
      - Provide registry lookup by stream_id
      - Start MOVA Tools TCP servers for each stream
      - Broadcast status change events to registered listeners
    """

    def __init__(self, licence: LicenceResult, io_factory=None) -> None:
        self._licence     = licence
        self._io_factory  = io_factory   # optional callable(stream_id) -> AbstractIO
        self._streams: dict[int, MovaStream] = {}
        self._servers: dict[int, MovaToolsServer] = {}
        self._lock     = threading.Lock()
        self._listeners: list[Callable[[int, MovaStatus], None]] = []

        # Clean up any stale per-stream kernel copies left by a previous run
        from pci_mova.core.kernel.wrapper import MovaKernel
        import os
        _lib_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "core", "kernel")
        MovaKernel.cleanup_stale_copies(_lib_dir)

        logger.info(
            "StreamManager initialised — licence valid=%s  max_streams=%d",
            licence.valid, licence.max_streams
        )

    # ── Stream registry ───────────────────────────────────────────────────────

    def get(self, stream_id: int) -> Optional[MovaStream]:
        return self._streams.get(stream_id)

    def all_streams(self) -> list[MovaStream]:
        with self._lock:
            return list(self._streams.values())

    def stream_count(self) -> int:
        return len(self._streams)

    # ── Lifecycle ─────────────────────────────────────────────────────────────

    def create_stream(self, stream_id: int) -> MovaStream:
        """
        Create (but do not start) a stream.
        Raises ValueError if stream_id is out of licence or already exists.
        """
        if stream_id < 0 or stream_id >= MAX_STREAMS:
            raise ValueError(f"stream_id {stream_id} out of range (max {MAX_STREAMS - 1})")

        if not self._licence.allows_stream(stream_id):
            raise PermissionError(
                f"Stream {stream_id} not permitted by licence "
                f"(max {self._licence.max_streams} streams)"
            )

        with self._lock:
            if stream_id in self._streams:
                raise ValueError(f"Stream {stream_id} already exists")

            # Use CM5IO if factory provided, else SimulatedIO
            if self._io_factory is not None:
                try:
                    io_layer = self._io_factory(stream_id)
                    logger.info("Stream %d: using CM5IO", stream_id)
                except Exception as exc:
                    logger.warning("Stream %d: CM5IO factory failed (%s) — falling back to SimulatedIO", stream_id, exc)
                    io_layer = SimulatedIO()
            else:
                io_layer = SimulatedIO()

            stream = MovaStream(
                stream_id         = stream_id,
                io_layer          = io_layer,
                on_status_change  = self._on_status_change,
            )
            self._streams[stream_id] = stream

        logger.debug("Stream %d created", stream_id)
        return stream

    def provision_streams(self) -> None:
        """
        Create all streams permitted by the current licence, then restore
        any datasets that were loaded before the last service restart.
        """
        count = self._licence.max_streams if self._licence.valid else 0
        for i in range(count):
            try:
                self.create_stream(i)
            except Exception as exc:
                logger.error("Failed to create stream %d: %s", i, exc)

        logger.info("Provisioned %d stream(s)", len(self._streams))
        self.restore_stream_state()

    # ── Dataset state persistence ─────────────────────────────────────────────

    def _state_path(self) -> str:
        from pci_mova.config import DATASET_DIR
        return os.path.join(DATASET_DIR, ".stream_state.json")

    def save_stream_state(self) -> None:
        """Persist dataset + MOVA_ON state for each stream so it survives restarts."""
        state = {}
        for sid, stream in self._streams.items():
            ds = stream._dataset
            if ds is not None and ds.source_path and os.path.isfile(ds.source_path):
                state[str(sid)] = {
                    "filename":  os.path.basename(ds.source_path),
                    "stream_id": ds.header.stream_id_str,
                    "mova_on":   int(stream.buffers.io[27]),
                }
        try:
            with open(self._state_path(), "w") as fh:
                json.dump(state, fh, indent=2)
            logger.debug("Stream state saved: %s", state)
        except OSError as exc:
            logger.warning("Could not save stream state: %s", exc)

    def restore_stream_state(self) -> None:
        """Reload datasets from the last saved state after a service restart."""
        path = self._state_path()
        if not os.path.isfile(path):
            return
        try:
            with open(path) as fh:
                state = json.load(fh)
        except Exception as exc:
            logger.warning("Could not read stream state: %s", exc)
            return

        from pci_mova.config import DATASET_DIR
        from pci_mova.dataset.parser import load_all, DatasetError

        for sid_str, entry in state.items():
            try:
                sid      = int(sid_str)
                filename = entry["filename"]
                ctrl_id  = entry["stream_id"]
                stream   = self._streams.get(sid)
                if stream is None:
                    continue
                ds_path = os.path.join(DATASET_DIR, filename)
                if not os.path.isfile(ds_path):
                    logger.warning("Stream %d: saved dataset '%s' not found — skipping", sid, filename)
                    continue
                all_streams = load_all(ds_path)
                if ctrl_id not in all_streams:
                    logger.warning("Stream %d: controller stream '%s' not in '%s' — skipping", sid, ctrl_id, filename)
                    continue
                ok = stream.load_dataset(all_streams[ctrl_id])
                if ok:
                    logger.info("Stream %d: restored dataset '%s' (%s)", sid, filename, ctrl_id)
                    # Re-enable MOVA if it was running before shutdown.
                    # EC is always 0 on a fresh kernel load — safe to restore.
                    if entry.get("mova_on", 0):
                        stream.buffers.io[27] = 1
                        logger.info("Stream %d: MOVA_ON restored", sid)
                else:
                    logger.warning("Stream %d: kernel rejected restored dataset '%s'", sid, filename)
            except Exception as exc:
                logger.error("Stream %d: error restoring dataset: %s", int(sid_str), exc)

    def start_stream(self, stream_id: int) -> None:
        stream = self._get_or_raise(stream_id)
        stream.start()

    def stop_stream(self, stream_id: int) -> None:
        stream = self._get_or_raise(stream_id)
        stream.stop()

    def stop_all(self) -> None:
        for stream in self.all_streams():
            try:
                stream.stop()
            except Exception as exc:
                logger.error("Error stopping stream %d: %s", stream.stream_id, exc)
        logger.info("All streams stopped")

    # ── MOVA Tools servers (async) ────────────────────────────────────────────

    async def start_mova_tools_servers(self) -> None:
        """Start a MOVA Tools TCP listener for every registered stream."""
        for stream_id, stream in self._streams.items():
            server = MovaToolsServer(stream)
            await server.start()
            self._servers[stream_id] = server
            logger.debug("Stream %d: MOVA Tools server started", stream_id)

    async def stop_mova_tools_servers(self) -> None:
        for server in self._servers.values():
            await server.stop()
        self._servers.clear()

    # ── Event listeners ───────────────────────────────────────────────────────

    def add_listener(self, cb: Callable[[int, MovaStatus], None]) -> None:
        self._listeners.append(cb)

    def _on_status_change(self, stream: MovaStream, status: MovaStatus) -> None:
        for cb in self._listeners:
            try:
                cb(stream.stream_id, status)
            except Exception as exc:
                logger.error("Listener error: %s", exc)

    # ── Snapshot ──────────────────────────────────────────────────────────────

    def snapshot(self) -> dict:
        return {
            "licence_valid":   self._licence.valid,
            "max_streams":     self._licence.max_streams,
            "active_streams":  len(self._streams),
            "streams": {
                str(sid): stream.snapshot()
                for sid, stream in self._streams.items()
            },
        }

    # ── Internal ──────────────────────────────────────────────────────────────

    def _get_or_raise(self, stream_id: int) -> MovaStream:
        stream = self._streams.get(stream_id)
        if stream is None:
            raise KeyError(f"Stream {stream_id} does not exist")
        return stream
