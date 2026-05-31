"""
pci_mova.core.io.simulated
SimulatedIO — in-process IO with no hardware dependency.

All detector, confirm, and force state is held in plain Python dicts.
The web API and test harness can drive inputs by calling set_detector(),
set_confirm(), set_crb() directly on the SimulatedIO instance.

Thread safety: a simple threading.Lock guards all state mutations.
"""

import threading
from typing import Any

from pci_mova.core.io.base import AbstractIO
from pci_mova.core.model.buffers import (
    KernelBuffers,
    MAX_DETECTORS, MAX_CONFIRMS, MAX_FORCE,
    DOUT_HI, DOUT_TO, DOUT_SYNC, DOUT_DET_FAULT, DOUT_MOVA_FAULT,
)
from pci_mova.config import MAX_DIN, MAX_DOUT


class SimulatedIO(AbstractIO):
    """
    Fully simulated IO layer.

    Inputs and outputs are plain integer arrays.
    External code (API, tests, future virtual IO) mutates inputs via the
    set_* helpers; the kernel sees them on the next tick.
    """

    def __init__(self) -> None:
        self._lock = threading.Lock()

        # ── Simulated input state ─────────────────────────────────────────────
        self._detectors: list[int] = [0] * MAX_DETECTORS  # 0=no vehicle, 1=vehicle
        self._confirms:  list[int] = [0] * MAX_CONFIRMS   # 0=no confirm, 1=confirmed
        self._crb:       int       = 0                     # Controller Ready Bit

        # ── Simulated output state (written by OutputScan) ────────────────────
        self._forces:    list[int] = [0] * MAX_FORCE
        self._hi:        int       = 0
        self._to:        int       = 0
        self._sync:      int       = 0
        self._det_fault: int       = 0
        self._mova_fault: int      = 0

        # ── Dataset polarity ──────────────────────────────────────────────────
        self._stagon: int = 0   # 0=Open contact (active-low), 1=Closed contact

        # ── Auto-follow mode ──────────────────────────────────────────────────
        # When enabled, confirms automatically track the kernel's forced stage
        # after the per-stage intergreen duration from the dataset.
        self.auto_follow:            bool = False
        self.speed_multiplier:       int  = 1   # kept in sync with kernel speed
        self._intergreen_matrix:     dict = {}  # {(from_stage, to_stage): seconds}
        self._last_forced_stage:     int  = 0   # stage currently being forced (0=none/IG)
        self._ig_countdown:          int  = 0   # ticks remaining in simulated intergreen
        self._pending_confirm_stage: int  = 0   # stage waiting to be confirmed

    # ── AbstractIO implementation ─────────────────────────────────────────────

    def read_inputs(self, buffers: KernelBuffers) -> None:
        """Copy simulated inputs into kernel buffers."""
        with self._lock:
            for i, v in enumerate(self._detectors):
                buffers.set_detector(i, v)
            for i, v in enumerate(self._confirms):
                buffers.set_confirm(i, v)
            buffers.crb = bool(self._crb)

    def write_outputs(self, buffers: KernelBuffers) -> None:
        """
        Capture kernel output decisions into simulated output state.
        In auto-follow mode, update confirms to track the forced stage after
        a short settling delay — simulates the TLC responding to MOVA's force.
        """
        with self._lock:
            for stage in range(1, MAX_FORCE + 1):
                self._forces[stage - 1] = buffers.get_force(stage)
            self._hi         = buffers.dout[DOUT_HI]
            self._to         = buffers.dout[DOUT_TO]
            self._sync       = buffers.dout[DOUT_SYNC]
            self._det_fault  = buffers.dout[DOUT_DET_FAULT]
            self._mova_fault = buffers.dout[DOUT_MOVA_FAULT]

            if not self.auto_follow:
                return

            # Find the single stage being forced (0 = intergreen / no force)
            forced = next(
                (s for s in range(1, MAX_FORCE + 1) if buffers.get_force(s)), 0
            )

            if forced != self._last_forced_stage:
                old = self._last_forced_stage
                self._last_forced_stage = forced
                if forced > 0:
                    # New stage demanded — start intergreen countdown
                    ig_sec = self._intergreen_matrix.get((old, forced), 0) if old > 0 else 0
                    ig_ticks = max(1, int(ig_sec * 10 / self.speed_multiplier))  # scale with speed
                    if ig_ticks > 0:
                        self._ig_countdown = ig_ticks
                        self._pending_confirm_stage = forced
                        # Set all confirms to inactive (intergreen state)
                        off = 1 - self._stagon
                        self._confirms = [off] * MAX_CONFIRMS
                    else:
                        # No intergreen (first stage, or 0s IG) — confirm immediately
                        self._ig_countdown = 0
                        self._pending_confirm_stage = 0
                        self._set_confirms_for_stage(forced)
                else:
                    # Force cleared — set all confirms inactive
                    self._ig_countdown = 0
                    self._pending_confirm_stage = 0
                    off = 1 - self._stagon
                    self._confirms = [off] * MAX_CONFIRMS

            elif self._ig_countdown > 0:
                self._ig_countdown -= 1
                if self._ig_countdown == 0 and self._pending_confirm_stage > 0:
                    self._set_confirms_for_stage(self._pending_confirm_stage)
                    self._pending_confirm_stage = 0

    def snapshot(self) -> dict:
        with self._lock:
            return {
                "type":        "simulated",
                "detectors":   list(self._detectors),
                "confirms":    list(self._confirms),
                "crb":         bool(self._crb),
                "forces":      list(self._forces),
                "hi":          self._hi,
                "to":          self._to,
                "sync":        self._sync,
                "det_fault":   self._det_fault,
                "mova_fault":  self._mova_fault,
                "auto_follow": self.auto_follow,
            }

    # ── Input injection (API / test harness) ──────────────────────────────────

    def set_detector(self, index: int, value: int) -> None:
        """Set detector input (0-based index, value 0 or 1)."""
        if 0 <= index < MAX_DETECTORS:
            with self._lock:
                self._detectors[index] = int(bool(value))

    def set_confirm(self, index: int, value: int) -> None:
        """Set stage/phase confirm input (0-based index)."""
        if 0 <= index < MAX_CONFIRMS:
            with self._lock:
                self._confirms[index] = int(bool(value))

    def set_crb(self, value: bool) -> None:
        """Set the Controller Ready Bit."""
        with self._lock:
            self._crb = int(value)

    def set_all_detectors(self, values: list[int]) -> None:
        """Bulk-set all detectors from a list."""
        with self._lock:
            for i, v in enumerate(values[:MAX_DETECTORS]):
                self._detectors[i] = int(bool(v))

    def set_all_confirms(self, values: list[int]) -> None:
        """Bulk-set all confirms from a list."""
        with self._lock:
            for i, v in enumerate(values[:MAX_CONFIRMS]):
                self._confirms[i] = int(bool(v))

    def reset_confirms(self, stagon: int) -> None:
        """Set all confirms to the inactive polarity for this dataset's stagon value."""
        self._stagon = int(bool(stagon))
        off_value = 1 - self._stagon  # stagon=0 → off=1, stagon=1 → off=0
        with self._lock:
            self._confirms = [off_value] * MAX_CONFIRMS
            self._last_forced_stage = 0
            self._ig_countdown = 0
            self._pending_confirm_stage = 0

    def set_intergreen_matrix(self, matrix: dict) -> None:
        """Set intergreen lookup: {(from_stage, to_stage): duration_seconds}."""
        with self._lock:
            self._intergreen_matrix = dict(matrix)

    def _set_confirms_for_stage(self, stage: int) -> None:
        """Set confirms so stage N is active, all others inactive. Must be called inside lock."""
        active   = self._stagon
        inactive = 1 - self._stagon
        for i in range(MAX_CONFIRMS):
            self._confirms[i] = active if i == stage - 1 else inactive

    # ── Output readback (API / test harness) ──────────────────────────────────

    def get_forces(self) -> list[int]:
        with self._lock:
            return list(self._forces)

    def get_force(self, stage: int) -> int:
        """Get stage force output (1-based stage number)."""
        with self._lock:
            return self._forces[stage - 1] if 1 <= stage <= MAX_FORCE else 0
