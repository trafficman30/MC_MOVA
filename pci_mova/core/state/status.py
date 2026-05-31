"""
pci_mova.core.state.status
StatusMachine — MOVA stream status with transition logic and watchdog timers.

Derived from MovaCradleSetStatus() and MonitorMovaStatusFault() in cradle source.
No platform dependencies.
"""

import logging
import threading
from typing import Callable, Optional

from pci_mova.core.model.enums import MovaStatus
from pci_mova.config import MAX_WARMUP_SECONDS, MAX_MANUAL_FORCE_SECONDS

logger = logging.getLogger(__name__)


class StatusMachine:
    """
    Manages the current MOVA status for a single stream.

    Responsibilities:
      - Enforce legal state transitions
      - Start / reset watchdog counters on entry
      - Call registered fault callbacks on state changes
      - tick() at 1 Hz to increment watchdog counters and raise timeout faults
    """

    def __init__(
        self,
        stream_id: int,
        on_fault: Optional[Callable[[MovaStatus], None]] = None,
        on_clear: Optional[Callable[[MovaStatus], None]] = None,
    ) -> None:
        self.stream_id = stream_id
        self._lock     = threading.Lock()

        self._current:  MovaStatus = MovaStatus.NOT_STARTED
        self._previous: MovaStatus = MovaStatus.NOT_STARTED

        # Watchdog counters (incremented at 1 Hz by tick())
        self._warmup_wait:       int = 0
        self._manual_force_wait: int = 0

        # Callbacks — injected by FaultMonitor
        self._on_fault = on_fault or (lambda s: None)
        self._on_clear = on_clear or (lambda s: None)

    # ── Public API ────────────────────────────────────────────────────────────

    @property
    def current(self) -> MovaStatus:
        with self._lock:
            return self._current

    @property
    def previous(self) -> MovaStatus:
        with self._lock:
            return self._previous

    def set(self, new_status: MovaStatus) -> None:
        """
        Transition to a new status.  No-ops if already in that state.
        Handles counter resets, fault raising/clearing, and logging.
        """
        with self._lock:
            if new_status == self._current:
                return

            prev = self._current
            self._previous = prev
            self._current  = new_status

            logger.info(
                "Stream %d status: %s → %s",
                self.stream_id, prev.label(), new_status.label()
            )

            self._on_entry(new_status, prev)

    def tick(self) -> None:
        """
        1 Hz task — increment watchdog counters and raise faults on timeout.
        Called by the stream's slow-tick path.
        """
        with self._lock:
            self._tick_warmup()
            self._tick_manual_force()

    def snapshot(self) -> dict:
        with self._lock:
            return {
                "current":            self._current.name,
                "current_label":      self._current.label(),
                "previous":           self._previous.name,
                "warmup_wait":        self._warmup_wait,
                "manual_force_wait":  self._manual_force_wait,
            }

    # ── State entry logic ─────────────────────────────────────────────────────

    def _on_entry(self, new: MovaStatus, prev: MovaStatus) -> None:
        """Called inside lock — reset counters and raise/clear faults."""

        # ── Clear previous faults when leaving faulted states ─────────────────
        if prev in (MovaStatus.WARMUP, MovaStatus.MULTI_CONFIRMS):
            self._on_clear(prev)
            self._warmup_wait = 0

        if prev == MovaStatus.MANUAL_FORCE:
            self._manual_force_wait = 0

        # ── New state entry actions ────────────────────────────────────────────
        match new:
            case MovaStatus.WARMUP:
                self._warmup_wait = 1        # Flag: start counting

            case MovaStatus.MANUAL_FORCE:
                self._manual_force_wait = 1  # Flag: start counting

            case MovaStatus.ON_CONTROL:
                self._warmup_wait       = 0
                self._manual_force_wait = 0
                self._on_clear(prev)

            case MovaStatus.OFF:
                self._warmup_wait       = 0
                self._manual_force_wait = 0
                self._on_clear(prev)

            case MovaStatus.MULTI_CONFIRMS:
                self._on_fault(new)

            case MovaStatus.NO_CRB:
                # Not a fault — just informational
                pass

            case _:
                pass

    # ── Watchdog tick helpers ─────────────────────────────────────────────────

    def _tick_warmup(self) -> None:
        if self._warmup_wait <= 0:
            return
        if self._current != MovaStatus.WARMUP:
            self._warmup_wait = 0
            return
        if self._warmup_wait < MAX_WARMUP_SECONDS:
            self._warmup_wait += 1
        elif self._warmup_wait == MAX_WARMUP_SECONDS:
            logger.warning(
                "Stream %d: warmup timeout (%ds) — raising fault",
                self.stream_id, MAX_WARMUP_SECONDS
            )
            self._on_fault(MovaStatus.WARMUP)
            self._warmup_wait += 1  # Prevent repeated raises

    def _tick_manual_force(self) -> None:
        if self._manual_force_wait <= 0:
            return
        if self._current != MovaStatus.MANUAL_FORCE:
            self._manual_force_wait = 0
            return
        if self._manual_force_wait < MAX_MANUAL_FORCE_SECONDS:
            self._manual_force_wait += 1
        else:
            logger.warning(
                "Stream %d: manual force timeout (%ds) — auto-clearing",
                self.stream_id, MAX_MANUAL_FORCE_SECONDS
            )
            # Signal caller to clear force — handled by FaultMonitor
            self._on_fault(MovaStatus.MANUAL_FORCE)
            self._manual_force_wait = 0
