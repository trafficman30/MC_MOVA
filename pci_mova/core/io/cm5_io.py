"""
pci_mova.core.io.cm5_io
CM5IO — reads detector/confirm/CRB from the CM5 IOBus Unix socket,
writes stage forces/TO/HI/SYNC back.

Signal mapping is loaded from config/streams.json (auto-generated from
dataset on first load, editable by user for non-standard wiring).

IOBus protocol (line-based UTF-8 on /tmp/cm5_iobus.sock):
  BATCH sig1 sig2 ...  →  0 1 0 ...   (primary read path, one round-trip)
  W <signal> <value>   →  OK           (force/output writes)
  PING                 →  PONG

Detector latch: 150ms rising-edge hold per detector.
MOVA's detscn() runs every 100ms. Worst case a pulse arrives 1ms after a
tick — next tick is 99ms away. 150ms hold gives ~51ms overlap guarantee.
"""

import logging
import socket
import threading
import time
from typing import Dict, List, Optional, Tuple

from pci_mova.core.io.base import AbstractIO
from pci_mova.core.model.buffers import (
    KernelBuffers,
    MAX_DETECTORS, MAX_CONFIRMS, MAX_FORCE,
    DOUT_HI, DOUT_TO, DOUT_SYNC, DOUT_DET_FAULT, DOUT_MOVA_FAULT,
)

logger = logging.getLogger(__name__)

IOBUS_SOCKET   = "/tmp/cm5_iobus.sock"
DET_LATCH_SEC  = 0.150   # 150ms rising-edge hold
CONNECT_TIMEOUT = 5.0
READ_TIMEOUT    = 0.5     # per-tick budget


class CM5IO(AbstractIO):
    """
    IO layer connecting MOVA kernel streams to CM5 IOBus.

    signal_map keys:
      det_map     {mova_det_index(0-based): "xkop.i.101"}
      confirm_map {mova_conf_index(0-based): "xkop.i.1"}
      crb         "xkop.i.32"
      force_map   {mova_force_index(0-based): "xkop.o.1"}   (0-based = force 1)
      to          "xkop.o.11"
      hi          "xkop.o.12"
      sync        "xkop.o.13"
    """

    def __init__(
        self,
        signal_map: dict,
        stream_id:  int  = 0,
        socket_path: str = IOBUS_SOCKET,
    ) -> None:
        self._stream_id   = stream_id
        self._socket_path = socket_path

        # Signal maps (index → bus signal name)
        self._det_map:     Dict[int, str] = {int(k): v for k, v in signal_map.get("det_map", {}).items()}
        self._conf_map:    Dict[int, str] = {int(k): v for k, v in signal_map.get("confirm_map", {}).items()}
        self._crb_sig:     Optional[str]  = signal_map.get("crb")
        self._force_map:   Dict[int, str] = {int(k): v for k, v in signal_map.get("force_map", {}).items()}
        self._to_sig:      Optional[str]  = signal_map.get("to")
        self._hi_sig:      Optional[str]  = signal_map.get("hi")
        self._sync_sig:    Optional[str]  = signal_map.get("sync")

        # Pre-build the BATCH read command (all inputs in one call)
        self._batch_signals: List[str] = []
        self._batch_det_idx:  List[int] = []
        self._batch_conf_idx: List[int] = []
        self._batch_crb_pos:  int = -1
        self._build_batch()

        # Detector latch state: monotonic timestamp of last rising edge, per index
        self._det_latch: Dict[int, float] = {}

        # Socket state
        self._sock:       Optional[socket.socket] = None
        self._sock_lock   = threading.Lock()
        self._connected   = False
        self._rx_count    = 0
        self._tx_count    = 0

        # Stagon polarity (set by reset_confirms on dataset load)
        self._stagon = 0

        logger.info(
            "Stream %d: CM5IO — %d dets, %d confirms, CRB=%s, socket=%s",
            stream_id, len(self._det_map), len(self._conf_map),
            self._crb_sig or "always-ready", socket_path,
        )
        self._connect()

    # ── Build BATCH command ────────────────────────────────────────────────

    def _build_batch(self) -> None:
        """Pre-build ordered signal list for BATCH reads."""
        sigs, det_idx, conf_idx = [], [], []

        for i in sorted(self._det_map):
            det_idx.append(i)
            sigs.append(self._det_map[i])

        for i in sorted(self._conf_map):
            conf_idx.append(i)
            sigs.append(self._conf_map[i])

        crb_pos = -1
        if self._crb_sig:
            crb_pos = len(sigs)
            sigs.append(self._crb_sig)

        self._batch_signals  = sigs
        self._batch_det_idx  = det_idx
        self._batch_conf_idx = conf_idx
        self._batch_crb_pos  = crb_pos

    # ── Socket management ──────────────────────────────────────────────────

    def _connect(self) -> bool:
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.settimeout(CONNECT_TIMEOUT)
            s.connect(self._socket_path)
            s.settimeout(READ_TIMEOUT)
            with self._sock_lock:
                if self._sock:
                    try: self._sock.close()
                    except OSError: pass
                self._sock = s
                self._connected = True
            logger.info("Stream %d: CM5IO connected to %s", self._stream_id, self._socket_path)
            return True
        except OSError as exc:
            logger.debug("Stream %d: CM5IO connect failed: %s", self._stream_id, exc)
            self._connected = False
            return False

    def _send(self, cmd: str) -> Optional[str]:
        """Send a command and return the response line, reconnecting on failure."""
        with self._sock_lock:
            sock = self._sock
        if sock is None:
            return None
        try:
            sock.sendall((cmd + "\n").encode())
            self._tx_count += 1
            resp = b""
            while b"\n" not in resp:
                chunk = sock.recv(4096)
                if not chunk:
                    raise OSError("connection closed")
                resp += chunk
            self._rx_count += 1
            return resp.split(b"\n")[0].decode().strip()
        except OSError as exc:
            logger.warning("Stream %d: CM5IO send error: %s — reconnecting", self._stream_id, exc)
            self._connected = False
            with self._sock_lock:
                self._sock = None
            self._connect()
            return None

    # ── AbstractIO implementation ──────────────────────────────────────────

    def read_inputs(self, buffers: KernelBuffers) -> None:
        if not self._batch_signals:
            return

        now = time.monotonic()

        # BATCH read — all inputs in one round-trip
        cmd  = "BATCH " + " ".join(self._batch_signals)
        resp = self._send(cmd)

        if resp is None:
            # Socket down — keep previous latch state, CRB stays False
            buffers.crb = False
            return

        try:
            vals = [int(v) & 1 for v in resp.split()]
        except ValueError:
            logger.debug("Stream %d: CM5IO bad BATCH response: %r", self._stream_id, resp)
            buffers.crb = False
            return

        if len(vals) < len(self._batch_signals):
            logger.debug("Stream %d: CM5IO short BATCH response", self._stream_id)
            buffers.crb = False
            return

        # Detectors — with 150ms rising-edge latch
        for pos, det_idx in enumerate(self._batch_det_idx):
            raw = vals[pos]
            if raw:
                self._det_latch[det_idx] = now   # rising edge: record time
            held = det_idx in self._det_latch and (now - self._det_latch[det_idx]) < DET_LATCH_SEC
            buffers.set_detector(det_idx, 1 if (raw or held) else 0)

        # Confirms
        base = len(self._batch_det_idx)
        for pos, conf_idx in enumerate(self._batch_conf_idx):
            buffers.set_confirm(conf_idx, vals[base + pos])

        # CRB
        if self._batch_crb_pos >= 0:
            buffers.crb = bool(vals[self._batch_crb_pos])
        else:
            buffers.crb = True

    def write_outputs(self, buffers: KernelBuffers) -> None:
        """Write stage forces and control bits back to CM5 IOBus."""
        # Stage forces
        for idx, sig in self._force_map.items():
            force_val = buffers.get_force(idx + 1)   # force_map is 0-based
            self._send(f"W {sig} {force_val}")

        # Control bits
        if self._to_sig:
            self._send(f"W {self._to_sig} {buffers.dout[DOUT_TO]}")
        if self._hi_sig:
            self._send(f"W {self._hi_sig} {buffers.dout[DOUT_HI]}")
        if self._sync_sig:
            self._send(f"W {self._sync_sig} {buffers.dout[DOUT_SYNC]}")

    def snapshot(self) -> dict:
        return {
            "type":      "cm5",
            "connected": self._connected,
            "socket":    self._socket_path,
            "rx":        self._rx_count,
            "tx":        self._tx_count,
        }

    def reset_confirms(self, stagon: int) -> None:
        self._stagon = int(bool(stagon))

    def set_intergreen_matrix(self, matrix: dict) -> None:
        pass   # not used — real TLC manages intergreen independently


# ── Signal map helpers ─────────────────────────────────────────────────────

def default_signal_map(dataset) -> dict:
    """
    Generate standard XKOP signal map from a loaded MovaDataset.

    Convention (matches MOVA Tools M8 wiring standard):
      MOVA det N (1-based)  → xkop.i.{100+N}
      stage confirm N        → xkop.i.{N}
      CRB                    → xkop.i.32
      stage force N          → xkop.o.{N}
      TO                     → xkop.o.11
      HI                     → xkop.o.12
      SYNC                   → xkop.o.13
    """
    c = dataset.constants

    det_map = {}
    for det in dataset.detectors:
        # det.id is 1-based in the dataset; kernel uses 0-based index
        det_map[str(det.id - 1)] = f"xkop.i.{100 + det.id}"

    confirm_map = {}
    for i in range(c.stages):
        confirm_map[str(i)] = f"xkop.i.{i + 1}"

    force_map = {}
    for i in range(c.stages):
        force_map[str(i)] = f"xkop.o.{i + 1}"

    return {
        "type":         "cm5",
        "det_map":      det_map,
        "confirm_map":  confirm_map,
        "crb":          "xkop.i.32",
        "force_map":    force_map,
        "to":           "xkop.o.11",
        "hi":           "xkop.o.12",
        "sync":         "xkop.o.13",
    }
