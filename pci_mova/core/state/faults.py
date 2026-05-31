"""
pci_mova.core.state.faults
FaultMonitor — monitors io[] flags and status transitions, raises/clears faults.

Derived from FaultMonitor(), MonitorMovaStatusFault(), MonitorManualForce()
in cradle source.  All Chameleon FaultLog/FaultAutoClear calls are replaced
with a simple event log and registered callbacks (for web API push).
"""

import logging
import threading
import time
from dataclasses import dataclass, field
from typing import Callable, Optional

from pci_mova.core.model.enums import MovaStatus, IdsError
from pci_mova.core.model.buffers import KernelBuffers, IO_PHONE_HOME
from pci_mova.core.state.status import StatusMachine

logger = logging.getLogger(__name__)

PHONE_HOME_THRESHOLD = 99


@dataclass
class FaultEvent:
    """A single fault event entry."""
    timestamp:   float
    stream_id:   int
    fault_type:  str        # "IDS" | "STATUS" | "WATCHDOG"
    code:        int
    label:       str
    data:        int = 0
    cleared:     bool = False


class FaultMonitor:
    """
    Monitors the MOVA stream for fault conditions at each tick.

    Fault sources:
      1. Kernel IDS errors  — io[] flags written by the kernel
      2. Status faults      — raised on state transitions (WARMUP timeout, MULTI)
      3. Phone-home         — io[PHONE_HOME] >= 99 → request comms
      4. Manual force       — auto-clear on timeout via StatusMachine callback
    """

    def __init__(
        self,
        stream_id: int,
        buffers:   KernelBuffers,
        status:    StatusMachine,
        on_phone_home:    Optional[Callable[[], None]] = None,
        on_fault_raised:  Optional[Callable[[FaultEvent], None]] = None,
        on_fault_cleared: Optional[Callable[[FaultEvent], None]] = None,
    ) -> None:
        self.stream_id = stream_id
        self._buffers  = buffers
        self._status   = status
        self._lock     = threading.Lock()

        # Callbacks
        self._on_phone_home    = on_phone_home    or (lambda: None)
        self._on_fault_raised  = on_fault_raised  or (lambda e: None)
        self._on_fault_cleared = on_fault_cleared or (lambda e: None)

        # Internal state
        self._phone_home_active: bool = False
        self._active_faults: dict[str, FaultEvent] = {}
        self._fault_history: list[FaultEvent] = []

        # Wire up status machine callbacks
        self._status._on_fault = self._handle_status_fault
        self._status._on_clear = self._handle_status_clear

    # ── Tick entry point ──────────────────────────────────────────────────────

    def tick(self) -> None:
        """Called every 100 ms tick from the stream loop."""
        self._check_phone_home()
        self._check_multi_stage_conf()
        self._check_ids_error()

    # ── Phone-home monitoring (io[17]) ────────────────────────────────────────

    def _check_phone_home(self) -> None:
        """
        Mirrors FaultMonitor() in cradle source.
        io[PHONE_HOME] >= 99 → trigger comms request.
        Clears when kernel drops it below 99.
        """
        with self._lock:
            flag = self._buffers.io[IO_PHONE_HOME]

            if flag >= PHONE_HOME_THRESHOLD and not self._phone_home_active:
                logger.info("Stream %d: phone-home requested (io[17]=%d)", self.stream_id, flag)
                self._phone_home_active = True
                self._on_phone_home()
                self._raise_fault("PHONE_HOME", 0, "MOVA Phone Home", 0)

            elif self._phone_home_active:
                if flag < PHONE_HOME_THRESHOLD:
                    logger.info("Stream %d: phone-home cleared", self.stream_id)
                    self._phone_home_active = False
                    self._buffers.io[IO_PHONE_HOME] = 0
                    self._clear_fault("PHONE_HOME")

    # ── Multi-stage confirm monitoring (io[30]) ───────────────────────────────

    def _check_multi_stage_conf(self) -> None:
        """io[30] (MULTI_STAGE_CONF) set by monitr when >1 stage confirm is active."""
        from pci_mova.core.model.buffers import IO_MULTI_STAGE_CONF
        with self._lock:
            flag = self._buffers.io[IO_MULTI_STAGE_CONF] if len(self._buffers.io) > IO_MULTI_STAGE_CONF else 0
            if flag and "MULTI_STAGE_CONF" not in self._active_faults:
                self._raise_fault("MULTI_STAGE_CONF", 14, "Multiple Stage Confirms", 0, fault_type="IDS")
            elif not flag and "MULTI_STAGE_CONF" in self._active_faults:
                self._clear_fault("MULTI_STAGE_CONF")

    # ── IDS error monitoring ──────────────────────────────────────────────────

    def _check_ids_error(self) -> None:
        """
        Historical IDS faults — raised then immediately auto-cleared.
        The kernel signals an IDS error by writing a non-zero value to a
        dedicated io[] slot.  In a full kernel integration monitr() calls
        PCError_SetLastKernel() which routes here.
        """
        # Placeholder — real kernel calls raise_ids_error() directly
        pass

    # IDS error codes that are informational events, not faults
    _INFO_IDS = frozenset({
        IdsError.CRB_OFF,        # 9  — CRB removed, explains why MOVA dropped control
        IdsError.CRB_ON,         # 10 — CRB returned
        IdsError.BACK_ON_LINE,   # 1  — MOVA resuming control
        IdsError.DEFAULT_LOADED, # 19 — dataset plan reloaded (normal on CRB cycle)
        IdsError.MOVA_RESTARTED, # 15 — informational
    })

    def raise_ids_error(self, ids_code: int, data: int = 0) -> None:
        """
        Called by the kernel wrapper when a new ErrorStore entry is detected.
        Informational events use type "INFO" and don't log a warning.
        All IDS entries are auto-cleared immediately (historical record only).
        """
        try:
            error    = IdsError(ids_code)
            label    = error.label()
            is_info  = error in self._INFO_IDS
        except ValueError:
            label    = f"IDS error {ids_code}"
            is_info  = False

        fault_type = "INFO" if is_info else "IDS"
        if is_info:
            logger.info("Stream %d: %s — %s (data=%d)", self.stream_id, fault_type, label, data)
        else:
            logger.warning("Stream %d: IDS %d — %s (data=%d)", self.stream_id, ids_code, label, data)

        key = f"IDS_{ids_code}"
        with self._lock:
            self._raise_fault(key, ids_code, label, data, fault_type=fault_type)
            self._clear_fault(key)

    # ── Status fault callbacks (wired into StatusMachine) ─────────────────────

    def _handle_status_fault(self, status: MovaStatus) -> None:
        """Raised by StatusMachine on WARMUP timeout, MULTI, MANUAL_FORCE timeout."""
        key = f"STATUS_{status.name}"
        with self._lock:
            self._raise_fault(key, status.value, status.label(), 0, fault_type="STATUS")

        # Manual force timeout → auto-clear forces
        if status == MovaStatus.MANUAL_FORCE:
            logger.warning("Stream %d: manual force timeout — auto-clearing", self.stream_id)
            self._buffers.io[27] = 0  # Signal kernel to clear forces

    def _handle_status_clear(self, status: MovaStatus) -> None:
        key = f"STATUS_{status.name}"
        with self._lock:
            self._clear_fault(key)

    # ── Fault record helpers ──────────────────────────────────────────────────

    def _raise_fault(
        self,
        key:        str,
        code:       int,
        label:      str,
        data:       int,
        fault_type: str = "STATUS",
    ) -> None:
        """Must be called inside self._lock."""
        if key in self._active_faults:
            return  # Already raised
        event = FaultEvent(
            timestamp  = time.time(),
            stream_id  = self.stream_id,
            fault_type = fault_type,
            code       = code,
            label      = label,
            data       = data,
        )
        self._active_faults[key] = event
        self._fault_history.append(event)
        self._on_fault_raised(event)

    def _clear_fault(self, key: str) -> None:
        """Must be called inside self._lock."""
        event = self._active_faults.pop(key, None)
        if event:
            event.cleared = True
            self._on_fault_cleared(event)

    # ── Public API ────────────────────────────────────────────────────────────

    def active_faults(self) -> list[dict]:
        with self._lock:
            return [
                {
                    "key":       k,
                    "type":      e.fault_type,
                    "code":      e.code,
                    "label":     e.label,
                    "data":      e.data,
                    "timestamp": e.timestamp,
                }
                for k, e in self._active_faults.items()
            ]

    def fault_history(self, limit: int = 50) -> list[dict]:
        with self._lock:
            return [
                {
                    "type":      e.fault_type,
                    "code":      e.code,
                    "label":     e.label,
                    "data":      e.data,
                    "cleared":   e.cleared,
                    "timestamp": e.timestamp,
                }
                for e in self._fault_history[-limit:]
            ]
