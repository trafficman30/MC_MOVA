"""
pci_mova.core.io.cm5_io
CM5IO — reads detector/confirm/CRB inputs from the CM5 IOBus.

Replaces SimulatedIO when PCI MOVA runs embedded inside CM5.
The CM5 IOBus is populated by RPDB/XKOP drivers from real hardware.

Input mapping is configured per-stream in mova.cfg:

  [INPUTS_0]
  det.0  = rpdb.i.xdet.0    ; detector 0 (AMVD4)
  det.1  = rpdb.i.xdet.1    ; detector 1 (BMVD2)
  conf.0 = rpdb.i.xsg.0     ; stage/phase confirm 0
  crb    = virt.99           ; Controller Ready Bit

Signal values are read directly from the IOBus on every MOVA tick (100ms).
No locking needed — IOBus reads are atomic.
"""

import logging
from typing import Dict, Optional

from pci_mova.core.io.base import AbstractIO
from pci_mova.core.model.buffers import (
    KernelBuffers,
    MAX_DETECTORS, MAX_CONFIRMS, MAX_FORCE,
    DOUT_HI, DOUT_TO, DOUT_SYNC, DOUT_DET_FAULT, DOUT_MOVA_FAULT,
)

logger = logging.getLogger(__name__)


class CM5IO(AbstractIO):
    """
    IO layer that reads inputs from the CM5 IOBus.

    Parameters
    ----------
    io_bus        : CM5 IOBus instance
    detector_map  : {mova_det_index: bus_signal_name}
                    e.g. {0: 'rpdb.i.xdet.0', 1: 'rpdb.i.xdet.1'}
    confirm_map   : {mova_conf_index: bus_signal_name}
                    e.g. {0: 'rpdb.i.xsg.0'}
    crb_signal    : bus signal name for CRB (str or None)
                    if None, CRB defaults to 1 (always ready)
    stream_id     : for logging only
    """

    def __init__(
        self,
        io_bus,
        detector_map:  Dict[int, str] = None,
        confirm_map:   Dict[int, str] = None,
        crb_signal:    Optional[str]  = None,
        stream_id:     int            = 0,
    ) -> None:
        self._io          = io_bus
        self._det_map     = detector_map  or {}
        self._conf_map    = confirm_map   or {}
        self._crb_signal  = crb_signal
        self._stream_id   = stream_id

        # Output state — captured from KernelBuffers by write_outputs()
        self._forces:    list = [0] * MAX_FORCE
        self._hi:        int  = 0
        self._to:        int  = 0
        self._sync:      int  = 0
        self._det_fault: int  = 0
        self._mova_fault: int = 0

        logger.info(
            "Stream %d: CM5IO initialised — %d detectors, %d confirms, CRB=%s",
            stream_id, len(self._det_map), len(self._conf_map),
            crb_signal or "always-ready"
        )

    # ── AbstractIO implementation ─────────────────────────────────────────────

    def read_inputs(self, buffers: KernelBuffers) -> None:
        """Read CM5 IOBus signals into MOVA kernel buffers."""

        # Detectors
        for idx, sig in self._det_map.items():
            val = self._io.read(sig)
            buffers.set_detector(idx, 1 if val else 0)

        # Confirms
        for idx, sig in self._conf_map.items():
            val = self._io.read(sig)
            buffers.set_confirm(idx, 1 if val else 0)

        # CRB
        if self._crb_signal:
            buffers.crb = bool(self._io.read(self._crb_signal))
        else:
            buffers.crb = True   # default: always ready

    def write_outputs(self, buffers: KernelBuffers) -> None:
        """Capture MOVA kernel output decisions into local state."""
        for stage in range(1, MAX_FORCE + 1):
            self._forces[stage - 1] = buffers.get_force(stage)
        self._hi         = buffers.dout[DOUT_HI]
        self._to         = buffers.dout[DOUT_TO]
        self._sync       = buffers.dout[DOUT_SYNC]
        self._det_fault  = buffers.dout[DOUT_DET_FAULT]
        self._mova_fault = buffers.dout[DOUT_MOVA_FAULT]

    def snapshot(self) -> dict:
        return {
            "type":        "cm5",
            "detectors":   [self._io.read(s) for s in self._det_map.values()],
            "confirms":    [self._io.read(s) for s in self._conf_map.values()],
            "crb":         bool(self._io.read(self._crb_signal)) if self._crb_signal else True,
            "forces":      list(self._forces),
            "hi":          self._hi,
            "to":          self._to,
            "sync":        self._sync,
            "det_fault":   self._det_fault,
            "mova_fault":  self._mova_fault,
        }

    # ── Output readback ───────────────────────────────────────────────────────

    def get_forces(self) -> list:
        return list(self._forces)

    def get_force(self, stage: int) -> int:
        return self._forces[stage - 1] if 1 <= stage <= MAX_FORCE else 0
