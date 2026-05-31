"""
pci_mova.core.model.buffers
Kernel IO buffer layout — din[], dout[], io[], detsin[], confin[], control[].

All indices derived from defines.h (EXTMOVA layout).
This is the single source of truth for buffer addressing.
No platform dependencies.
"""

from pci_mova.config import (
    MAX_DIN, MAX_DOUT, MAX_IO,
    MAX_DETECTORS, MAX_CONFIRMS, MAX_FORCE,
)


# ─── din[] index assignments (EXTMOVA) ───────────────────────────────────────
#   0  .. 63   Detectors
#   64 .. 94   Stage/Phase Confirms
#   95         Ready to Control (CRB)
#   96         RAM Battery (unused in PC-MOVA)
#   97         +10v Supply (unused)
#   98         MPU Battery (unused)

DIN_DETECTORS_START  = 0
DIN_DETECTORS_END    = MAX_DETECTORS - 1          # 63
DIN_CONFIRMS_START   = MAX_DETECTORS              # 64
DIN_CONFIRMS_END     = MAX_DETECTORS + MAX_CONFIRMS - 2  # 94
DIN_CRB              = MAX_DETECTORS + MAX_CONFIRMS - 1  # 95


# ─── dout[] index assignments (EXTMOVA) ──────────────────────────────────────
#   0  .. 15   Stage force bits
#   16         TO  (Turn-On)
#   17         HI  (Hold Inhibit)
#   18         Sync pulse
#   19         DET fault LED
#   20         MOVA fault LED

DOUT_FORCE_START     = 0
DOUT_FORCE_END       = MAX_FORCE - 1              # 9  (10 stage forces)
DOUT_TO              = 16
DOUT_HI              = 17
DOUT_SYNC            = 18
DOUT_DET_FAULT       = 19
DOUT_MOVA_FAULT      = 20

# Sync pulse control (index into control[] array for genstg)
CONTROL_SYNC         = 12


# ─── io[] index assignments ───────────────────────────────────────────────────
# Tcomshr->io[] — confirmed from mova_constants.h
#
IO_CLEAR_ERRORS          = 16   # ERROR_COUNT — CLEAR command
IO_PHONE_HOME            = 17   # PHONE_HOME  — >= 99 triggers comms request
IO_WATCHDOG              = 18   # WATCH_DOG   — kernel sets to 19 each scan
IO_ON_CONTROL            = 19   # ON_CONTROL  — 1 when MOVA controlling, 0 when off
IO_MSG_OUTPUT            = 20   # PRINT_ON    — message output routing
IO_CONTROLLER_READY      = 21   # CONTROLLER_READY
IO_FLOW_DISPLAY          = 22   # FLOW_ON     — flow display control
IO_STAGE_STUCK           = 23   # STAGE_STUCK
IO_DEMAND_STAGE          = 24   # DEMAND_STAGE
IO_ASSESS_DISPLAY        = 25   # ASS_LOG_ON  — assessment log display
IO_ERROR_DISPLAY         = 26   # ERROR_LOG   — error log display
IO_MOVA_ONOFF            = 27   # MOVA_ON     — START=1, STOP=0
IO_DISPLAY_REFRESH       = 28   # USER_OUTPUT_FLAG — display refresh mode
IO_PIPE_STATE            = 29   # PHONE_LINE_CONNECTED — pipe/modem state
IO_MULTI_STAGE_CONF      = 30   # MULTI_STAGE_CONF
IO_PHONE_IN_USE          = 31   # INC_WHILE_PHONE_IN_USE


class KernelBuffers:
    """
    All shared memory buffers between InputScan → Kernel → OutputScan.
    Instantiated once per stream.
    """

    __slots__ = (
        "din",
        "dout",
        "io",
        "detsin",
        "confin",
        "detsin_0s",
        "detsin_1s",
        "kernel_deton",
        "control",
        "current_stage",
        "next_stage",
        "previous_stage",
        "output_mode",
        "warmup_counter",
        "special_outputs",
        "prog_marker",
        "kernel_current_stage",
        "kernel_demanded_stage",
        "kernel_next_stage",
        "wait_stage_change_timer",
        "kernel_in_warmup",
        "kernel_stages",
        "kernel_crb",
    )

    def __init__(self) -> None:
        # Physical input bits
        self.din:      list[int] = [0] * MAX_DIN
        # Physical output bits
        self.dout:     list[int] = [0] * MAX_DOUT
        # Kernel ↔ wrapper IO flags
        self.io:       list[int] = [0] * MAX_IO

        # Detector inputs (normalised — one entry per detector)
        self.detsin:   list[int] = [0] * MAX_DETECTORS
        # Confirm inputs
        self.confin:   list[int] = [0] * MAX_CONFIRMS

        # Transition counters (used by detscn logic)
        self.detsin_0s: list[int] = [1] * MAX_DETECTORS  # 0→1 debounce
        self.detsin_1s: list[int] = [0] * MAX_DETECTORS  # 1→0 debounce

        # Kernel-side detector states read back after each tick.
        # Reflects detsin[] AFTER special conditioning has run — virtual/conditioned
        # detectors set by SC_ProcessAllRules are visible here but not in din[].
        self.kernel_deton: list[int] = [0] * MAX_DETECTORS

        # Stage force output array (written by genstg / OutputScan)
        self.control:  list[int] = [0] * (MAX_FORCE + 5)  # force + HI/TO/sync/etc.

        # Junction state
        self.current_stage:   int = 0
        self.next_stage:      int = 0
        self.previous_stage:  int = 0
        self.output_mode:     int = 0  # OutputMode value
        self.warmup_counter:  int = -1  # From CI_get_warmup_counter(); -1 until dataset loaded
        # Dataset-conditioned special outputs — control[StartofOutputChans..+8]
        self.special_outputs: list[int] = [0] * 8
        # Stage-machine diagnostics (from MI_get_prog_marker etc.)
        self.prog_marker:              int = 0
        self.kernel_current_stage:     int = 0
        self.kernel_demanded_stage:    int = 0
        self.kernel_next_stage:        int = 0
        self.wait_stage_change_timer:  int = 0   # ×10 tenths of a second
        self.kernel_in_warmup:         int = 1   # 1=warmup, 0=post-warmup
        self.kernel_stages:            int = -1  # Tcomshr->stages (from C kernel)
        self.kernel_crb:               int = -1  # confin[mxconf-1] (CRB in C kernel)

    # ── Convenience helpers ───────────────────────────────────────────────────

    @property
    def crb(self) -> bool:
        """Controller Ready Bit."""
        return bool(self.din[DIN_CRB])

    @crb.setter
    def crb(self, value: bool) -> None:
        self.din[DIN_CRB] = int(value)

    def get_detector(self, index: int) -> int:
        """Read a single detector input (0-based)."""
        return self.detsin[index]

    def set_detector(self, index: int, value: int) -> None:
        self.detsin[index] = value
        self.din[DIN_DETECTORS_START + index] = value

    def get_confirm(self, index: int) -> int:
        """Read a single confirm input (0-based)."""
        return self.confin[index]

    def set_confirm(self, index: int, value: int) -> None:
        self.confin[index] = value
        self.din[DIN_CONFIRMS_START + index] = value

    def get_force(self, stage: int) -> int:
        """Read a stage force output bit (1-based stage, matches MOVA convention)."""
        return self.dout[DOUT_FORCE_START + stage - 1] if 1 <= stage <= MAX_FORCE else 0

    def set_force(self, stage: int, value: int) -> None:
        if 1 <= stage <= MAX_FORCE:
            self.dout[DOUT_FORCE_START + stage - 1] = value

    def snapshot(self) -> dict:
        """Return a JSON-serialisable snapshot for the web API."""
        return {
            "current_stage":  self.current_stage,
            "next_stage":     self.next_stage,
            "output_mode":    self.output_mode,
            "crb":            self.crb,
            "warmup_counter": self.warmup_counter,
            "detectors":      self.detsin[:MAX_DETECTORS],
            "kernel_deton":   self.kernel_deton[:MAX_DETECTORS],
            "confirms":       self.confin[:MAX_CONFIRMS - 1],  # 31 stage confirms; index 31 is CRB (internal)
            "forces":         self.dout[DOUT_FORCE_START:DOUT_FORCE_START + MAX_FORCE],
            "io":             self.io[:MAX_IO],
            "special_outputs": list(self.special_outputs),
            "prog_marker":               self.prog_marker,
            "kernel_current_stage":      self.kernel_current_stage,
            "kernel_demanded_stage":     self.kernel_demanded_stage,
            "kernel_next_stage":         self.kernel_next_stage,
            "wait_stage_change_timer":   self.wait_stage_change_timer,
            "kernel_in_warmup":          self.kernel_in_warmup,
            "kernel_stages":             getattr(self, "kernel_stages", -1),
            "kernel_crb":                getattr(self, "kernel_crb", -1),
        }
