"""
pci_mova.core.model.enums
All MOVA domain enumerations derived from kernel source.
No platform dependencies.
"""

from enum import IntEnum, auto


class MovaStatus(IntEnum):
    """
    MOVA stream status states.
    Order and values match mova_status_t / MovaStatusString[] in the kernel.
    """
    NOT_STARTED          = 0
    DEBUG_START          = 1
    NO_LICENCE           = 2
    NO_CONFIG            = 3
    NO_DATASET           = 4
    INIT                 = 5
    OFF                  = 6
    MULTI_CONFIRMS       = 7   # Multiple stage confirms
    WARMUP               = 8
    ON_CONTROL           = 9
    SHUTDOWN             = 10
    NO_CRB               = 11  # Controller Ready Bit absent
    MANUAL_FORCE         = 12
    FAILED_RESTART       = 13  # Stopped after consecutive tick failures — requires Reset

    def label(self) -> str:
        _labels = {
            self.NOT_STARTED:    "Not Started",
            self.DEBUG_START:    "Debug Start Loop",
            self.NO_LICENCE:     "No MOVA Licence",
            self.NO_CONFIG:      "No MOVA Configuration",
            self.NO_DATASET:     "No Dataset",
            self.INIT:           "Initial",
            self.OFF:            "Off",
            self.MULTI_CONFIRMS: "Multiple Stage Confirms",
            self.WARMUP:         "Warmup",
            self.ON_CONTROL:     "On Control",
            self.SHUTDOWN:       "Shutdown",
            self.NO_CRB:         "Controller Not Ready",
            self.MANUAL_FORCE:   "Manual Force",
            self.FAILED_RESTART: "Failed — Reset Required",
        }
        return _labels.get(self, "Unknown")


class OutputMode(IntEnum):
    """
    MOVA kernel output mode — passed to OutputScan().
    Values from mova_op.h (confirmed from source).
    """
    SYNC_PULSE_ON      = 1
    SYNC_PULSE_OFF     = 2
    CHANGE_OF_STAGE    = 3
    MOVA_JUST_ON       = 4
    MOVA_OFF           = 5
    MOVA_FAULT_ON      = 6
    MOVA_FAULT_OFF     = 7
    NO_ACTION          = 8
    MOVA_IN_INTERGREEN = 9
    NORMAL             = 10   # Force bits set, not in TO or HI — standard on-control running


class IdsError(IntEnum):
    """
    MOVA kernel IDS error codes — matches idsMessage[] in kernel source.
    Historical faults: raised then immediately auto-cleared.
    """
    INVALID_PARAMETER       = 0
    BACK_ON_LINE            = 1
    FAULTY_DETECTOR         = 2
    STAGE_NOT_ENDED         = 3
    INTERGREEN_NOT_ENDED    = 4
    INVALID_STAGE_DEMANDED  = 5
    WRONG_STAGE_CONFIRMED   = 6
    STAGE_STUCK_ON          = 7   # Deprecated
    WATCHDOG_ROUTINE        = 8
    CRB_OFF                 = 9
    CRB_ON                  = 10
    PROGRAM_ERROR           = 11
    CHECKSUM_ERROR          = 12
    RELOADING_ERROR         = 13
    MULTIPLE_STAGE_CONFIRMS = 14
    MOVA_RESTARTED          = 15
    DIVIDE_ERROR            = 16
    EMPTY_REPOSITORY        = 17
    REPOSITORY_CHECKSUM     = 18
    DEFAULT_LOADED          = 19

    def label(self) -> str:
        _labels = {
            self.INVALID_PARAMETER:       "Invalid Parameter",
            self.BACK_ON_LINE:            "MOVA Back On Line",
            self.FAULTY_DETECTOR:         "Faulty Detector",
            self.STAGE_NOT_ENDED:         "Stage Not Ended",
            self.INTERGREEN_NOT_ENDED:    "Intergreen Not Ended",
            self.INVALID_STAGE_DEMANDED:  "Invalid Stage Demanded",
            self.WRONG_STAGE_CONFIRMED:   "Wrong Stage Confirmed",
            self.STAGE_STUCK_ON:          "Stage Stuck On",
            self.WATCHDOG_ROUTINE:        "Watchdog Routine",
            self.CRB_OFF:                 "Controller Ready Bit Off",
            self.CRB_ON:                  "Controller Ready Bit On",
            self.PROGRAM_ERROR:           "Program Error",
            self.CHECKSUM_ERROR:          "Checksum Error",
            self.RELOADING_ERROR:         "Reloading Error",
            self.MULTIPLE_STAGE_CONFIRMS: "Multiple Stage Confirms",
            self.MOVA_RESTARTED:          "MOVA Restarted",
            self.DIVIDE_ERROR:            "Divide Error",
            self.EMPTY_REPOSITORY:        "Empty Repository Area",
            self.REPOSITORY_CHECKSUM:     "Repository Checksum",
            self.DEFAULT_LOADED:          "Default Loaded",
        }
        return _labels.get(self, "Unknown")


class DisplayMode(IntEnum):
    """
    io[28] display refresh modes — controls what the output thread shows.
    Derived from REFRESH_CTRL handling in cradle source.
    """
    OFF         = 0
    MESSAGES    = 22
    VIEW_MSGS   = 23
    PRINT_MSGS  = 27
    ERRORS      = 26
    ASSESSMENT  = 32


class RestartType(IntEnum):
    WARM = 0
    COLD = 1


class CommsState(IntEnum):
    DISCONNECTED = 0
    CONNECTED    = 1
