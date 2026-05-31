"""
pci_mova.core.kernel.wrapper
MovaKernel — wrapper around the MOVA MI_* API functions.

The MOVA kernel exposes a clean C API via MI_* functions:
  MI_set_detectors_status(uint8[64])     — write detector inputs
  MI_set_stages_confirmation(uint8[32])  — write confirm inputs
  MI_set_crb_bit(bool)                   — write CRB
  MI_get_control_bits_status(uint8[15])  — read force/HI/TO outputs
  MI_get_take_over_bit_status()          — read TO bit

The kernel also exports monitr() which runs one algorithm tick.
Inputs are set via MI_* before calling monitr(), outputs are read
via MI_get_control_bits_status() after.

Control array layout (NumberOfForce=10, NumberOfExtra=5 with M8_ENABLED):
  control[0..9]   = force bits (stage 1-10)
  control[10]     = HI (Hold Inhibit)
  control[11]     = TO (Take Over)
  control[12]     = Sync pulse
  control[13]     = Hold signal   (M8_IMPROVED_LINKING)
  control[14]     = Release signal (M8_IMPROVED_LINKING)
"""

import ctypes
import logging
import os
import shutil
import tempfile
from typing import Optional

from pci_mova.core.model.buffers import KernelBuffers
from pci_mova.core.model.enums import OutputMode
from pci_mova.config import MAX_DETECTORS, MAX_CONFIRMS, MAX_FORCE

logger = logging.getLogger(__name__)

# Path to the compiled MOVA kernel shared library.
# Override with MOVA_KERNEL_LIB environment variable.
_DEFAULT_LIB_PATH = os.getenv(
    "MOVA_KERNEL_LIB",
    os.path.join(os.path.dirname(__file__), "libmova.so")
)

# Control array size: 10 forces + 5 extra (M8_ENABLED with M8_IMPROVED_LINKING)
_CONTROL_SIZE = MAX_FORCE + 5

# ctypes array types
_DetArray     = ctypes.c_uint8 * MAX_DETECTORS
_ConfArray    = ctypes.c_uint8 * MAX_CONFIRMS
_ControlArray = ctypes.c_uint8 * _CONTROL_SIZE
_OutChanArray = ctypes.c_uint8 * 8   # NumberofOutputChans = 8 special outputs


def _arr(raw: list, start: int, count: int, width: int = 0) -> str:
    """Format a slice of raw message fields as space-separated values."""
    vals = raw[start:start + count]
    if width:
        return " ".join(str(v).rjust(width) for v in vals)
    return " ".join(str(v) for v in vals)


def _format_mova_msg(
    t: int, s: int, raw: list,
    noStages: int, noLinks: int, noLanes: int, noDets: int,
) -> str:
    """
    Reproduce the MOVA commissioning terminal message format (display_msg.c).
    raw[] is the full int32 array for one message slot, raw[0]=type, raw[1]=sub/len.
    """
    def r(i): return raw[i] if i < len(raw) else 0

    if t == 1 and s == 1:
        # S {stage} HH:MM:SS  SMF {lanes_smf}  ={jun_smf}  SAT {lanes_sat}  MX={laosmx}  LAM {lam}/{totlam}  CUT={cutmx}
        stage = r(2); hh, mm, ss = r(3), r(4), r(5)
        i = 6
        smf   = _arr(raw, i, noLanes, 3); i += noLanes
        junsmf = r(i); i += 1
        sat   = _arr(raw, i, noLanes, 1); i += noLanes
        laosmx = r(i); lam = r(i+1); totlam = r(i+2); cutmx = r(i+3)
        return (
            f"1.1  S{stage} {hh:02d}:{mm:02d}:{ss:02d}"
            f"  SMF {smf} ={junsmf}"
            f"  SAT {sat}"
            f"  MX={laosmx}  LAM {lam}/{totlam}  CUT={cutmx}"
        )

    if t == 1 and s == 2:
        # SMCYC {cycle}  DMX {per_stage_drift_max}  SUS {per_det_suspect}
        i = 2
        cyc = r(i); i += 1
        dmx = _arr(raw, i, noStages, 2); i += noStages
        sus = _arr(raw, i, noDets, 1)
        return f"1.2  SMCYC {cyc}  DMX {dmx}  SUS {sus}"

    if t == 2:
        # {sistim}  SMIN {stamin}  LMIN {per_link_var_min}  RCX {per_lane_rcx}
        sistim = r(2); stamin = r(3)
        i = 4
        lmin = _arr(raw, i, noLinks, 1); i += noLinks
        rcx  = _arr(raw, i, noLanes, 1)
        return f"2    {sistim}  SMIN {stamin}  LMIN {lmin}  RCX {rcx}"

    if t == 3 and r(1) != 2:
        # 3.1  {sistim}  NX {next}  ESLI {per_link_endsat}  {lane}LA {gap} {toff} {waste}...
        sistim = r(2); nx = r(3)
        i = 4
        esli = _arr(raw, i, noLinks, 1); i += noLinks
        length = r(1)  # total fields written (overwrites sub)
        la_parts = []
        while i + 3 < length and i + 3 < len(raw):
            lane = r(i); gap = r(i+1); toff = r(i+2); waste = r(i+3)
            la_parts.append(f"{lane}LA {gap} {toff} {waste}")
            i += 4
        la_str = "  ".join(la_parts)
        return f"3.1  {sistim}  NX {nx}  ESLI {esli}  {la_str}"

    if t == 3 and s == 2:
        # 3.2  {sistim}  NX {next}  hold {h}  ext {e}  xtimr {x}  lxtm {...}  revdem {...}
        sistim = r(2); nx = r(3); hold = r(4); ext = r(5); xtimr = r(6)
        i = 7
        lxtm   = _arr(raw, i, noLinks, 2); i += noLinks
        revdem = _arr(raw, i, noLinks, 1)
        return (
            f"3.2  {sistim}  NX {nx}"
            f"  hold {hold}  ext {ext}  xtimr {xtimr}"
            f"  lxtm {lxtm}  revdem {revdem}"
        )

    if t == 4:
        # {sistim}  NX {next}  BDR {ben} {disben} {predred}  {n}LK {netben} {ben} {fred} {xg}...
        sistim = r(2); nx = r(3); ben = r(4); disben = r(5); predred = r(6)
        length = r(1)
        i = 7
        lk_parts = []
        while i + 4 < length and i + 4 < len(raw):
            linkno = r(i); nb = r(i+1); bf = r(i+2); fr = r(i+3); xg = r(i+4)
            lk_parts.append(f"{linkno}LK {nb:4d} {bf:4d} {fr:3d} {xg:3d}")
            i += 5
        lk_str = "  ".join(lk_parts)
        return (
            f"4    {sistim}  NX {nx}"
            f"  BDR {ben:4d} {disben:4d} {predred:4d}"
            f"  {lk_str}"
        )

    if t == 5:
        # All 5.x share: {sistim}  NX {next}  HH:MM:SS  <type-specific>  DEM {...}  RCX {...}
        sistim = r(2); nx = r(3); hh, mm, ss = r(4), r(5), r(6)
        prefix = f"5.{s}  {sistim}  NX {nx}  {hh:02d}:{mm:02d}:{ss:02d}"
        if s == 1:
            ben = r(7); disben = r(8); predred = r(9); cfdelta = r(10)
            idx = 11
            mid = f"  OPT BDR {ben:4d} {disben:4d} {predred:4d}  CF {cfdelta}"
        elif s == 2:
            dmax = r(7); idx = 8
            mid = f"  MAX CHANGE DMX+MXP={dmax}"
        elif s == 3:
            pcs = _arr(raw, 7, noLinks, 2); idx = 7 + noLinks
            mid = f"  OSAT+ES PCS {pcs}"
        elif s == 4:
            enx = r(7); pnx = r(8); idx = 9
            mid = f"  EM/PR TRUNC ENX={enx} PNX={pnx}"
        else:
            idx = 7; mid = ""
        dem = _arr(raw, idx, noLinks, 1); idx += noLinks
        rcx = _arr(raw, idx, noLanes, 1); idx += noLanes
        xg  = r(idx)
        return f"{prefix}{mid}  DEM {dem}  RCX {rcx}  XG={xg}"

    if t == 6 and s == 1:
        # IG:{sec}  SDEM {per_stage_dem}  BON {per_link_bonus}  RCIN {per_lane_rcin}
        sec = r(2); i = 3
        sdem = _arr(raw, i, noStages, 1); i += noStages
        bon  = _arr(raw, i, noLinks,  1); i += noLinks
        rcin = _arr(raw, i, noLanes,  1)
        return f"6.1  IG:{sec}  SDEM {sdem}  BON {bon}  RCIN {rcin}"

    if t == 6 and s == 2:
        # IG:{sec}  EM/PR CHANGE  SDEM {...}  TRUN {...}  SKIP {...}
        sec = r(2); i = 3
        sdem = _arr(raw, i, noStages, 1); i += noStages
        trun = _arr(raw, i, noLinks,  1); i += noLinks
        skip = _arr(raw, i, noLinks,  1)
        return f"6.2  IG:{sec}  EM/PR CHANGE  SDEM {sdem}  TRUN {trun}  SKIP {skip}"

    return f"MSG type={t} sub={s}"


class MovaKernel:
    """
    Wraps the MOVA kernel via ctypes.

    Lifecycle:
      1. __init__  — load libmova.so and declare function signatures
      2. load_dataset(data) — pass dataset path to kernel
      3. tick(buffers) — called every 100ms; marshals IO and calls monitr()
      4. shutdown() — graceful teardown
    """

    def __init__(self, stream_id: int, lib_path: Optional[str] = None) -> None:
        self.stream_id = stream_id
        self._lib:            Optional[ctypes.CDLL] = None
        self._dataset_loaded: bool  = False
        self._simulation:     bool  = False
        self._lib_tmp_path:   Optional[str] = None
        self._genstg_div:       int          = 0  # gate genstg to every N ticks
        self._genstg_div_max:   int          = 5  # N = 5 at 1x speed (500ms), reduced for faster
        self._sync_state:       int          = 0  # held sync state, toggled by kernel pulse
        self._last_msg_num:     int          = 0  # last seen current_msg_num (1-based)
        self._last_error_count: int          = 1  # last seen ErrorCount (1-based, kernel starts at 1)
        self._speed_multiplier: int          = 1  # 1x=real-time, 2x, 5x supported

        # Reusable ctypes buffers — allocated once, reused every tick
        self._det_buf     = _DetArray()
        self._conf_buf    = _ConfArray()
        self._control_buf = _ControlArray()
        self._outchan_buf = _OutChanArray()

        self._try_load_library(lib_path or _DEFAULT_LIB_PATH)

    def __del__(self) -> None:
        if self._lib_tmp_path and os.path.exists(self._lib_tmp_path):
            try:
                os.unlink(self._lib_tmp_path)
            except OSError:
                pass

    # ── Library loading ───────────────────────────────────────────────────────

    @staticmethod
    def cleanup_stale_copies(lib_dir: str) -> None:
        """Remove any leftover per-stream kernel copies from a previous run."""
        import glob
        for path in glob.glob(os.path.join(lib_dir, "libmova_s*.so")):
            try:
                os.unlink(path)
            except OSError:
                pass

    def _try_load_library(self, path: str) -> None:
        if not os.path.exists(path):
            logger.warning(
                "Stream %d: MOVA kernel library not found at %s — "
                "running in simulation mode",
                self.stream_id, path
            )
            self._simulation = True
            return
        try:
            # Each stream needs its own private copy of the .so so that the
            # C library's global state (Tcomshr/Xcomshr etc.) is not shared
            # between streams. dlopen() deduplicates by inode so we must use
            # distinct file paths — a per-stream temp copy achieves this.
            tmp = tempfile.NamedTemporaryFile(
                prefix=f"libmova_s{self.stream_id}_",
                suffix=".so",
                delete=False,
                dir=os.path.dirname(path),
            )
            shutil.copy2(path, tmp.name)
            self._lib_tmp_path = tmp.name
            tmp.close()

            lib = ctypes.CDLL(self._lib_tmp_path, mode=ctypes.RTLD_LOCAL)
            self._declare_signatures(lib)
            self._lib = lib
            logger.debug(
                "Stream %d: MOVA kernel loaded from %s (private copy, RTLD_LOCAL)",
                self.stream_id, self._lib_tmp_path,
            )
        except OSError as exc:
            logger.error(
                "Stream %d: failed to load MOVA kernel (%s) — "
                "falling back to simulation mode",
                self.stream_id, exc
            )
            self._simulation = True

    def _declare_signatures(self, lib: ctypes.CDLL) -> None:
        """Declare ctypes function signatures for all MI_* API functions."""

        # MI_set_detectors_status(uint8* detectors) → int (MVStatus)
        lib.MI_set_detectors_status.argtypes = [ctypes.POINTER(ctypes.c_uint8)]
        lib.MI_set_detectors_status.restype  = ctypes.c_int

        # MI_set_stages_confirmation(uint8* confirms) → int
        lib.MI_set_stages_confirmation.argtypes = [ctypes.POINTER(ctypes.c_uint8)]
        lib.MI_set_stages_confirmation.restype  = ctypes.c_int

        # MI_set_crb_bit(int status) → int
        lib.MI_set_crb_bit.argtypes = [ctypes.c_int]
        lib.MI_set_crb_bit.restype  = ctypes.c_int

        # MI_get_control_bits_status(uint8* control) → int
        lib.MI_get_control_bits_status.argtypes = [ctypes.POINTER(ctypes.c_uint8)]
        lib.MI_get_control_bits_status.restype  = ctypes.c_int

        # MI_get_take_over_bit_status() → int (MVBool)
        lib.MI_get_take_over_bit_status.argtypes = []
        lib.MI_get_take_over_bit_status.restype  = ctypes.c_int

        # monitr() — main kernel tick, no arguments, no return
        lib.monitr.argtypes = []
        lib.monitr.restype  = None

        # Dataset initialisation and loading
        lib.XML_DataSet_Initialise.argtypes                   = []
        lib.XML_DataSet_Initialise.restype                    = None
        lib.XML_DataSet_ReadAllDataPlansFromFile.argtypes     = [ctypes.c_char_p, ctypes.c_char_p]
        lib.XML_DataSet_ReadAllDataPlansFromFile.restype      = ctypes.c_int
        lib.CI_init_core_module.argtypes                      = []
        lib.CI_init_core_module.restype                       = None
        lib.CI_init_core_dynmaic_data.argtypes                = []
        lib.CI_init_core_dynmaic_data.restype                 = None

        # Kernel state readers
        lib.CI_get_warmup_counter.argtypes = []
        lib.CI_get_warmup_counter.restype  = ctypes.c_int32

        # Direct io[] flag write/read — Tcomshr->io[index]
        lib.MI_set_io_flag.argtypes = [ctypes.c_int, ctypes.c_int]
        lib.MI_set_io_flag.restype  = ctypes.c_int
        lib.MI_get_io_flag.argtypes = [ctypes.c_int]
        lib.MI_get_io_flag.restype  = ctypes.c_int

        # Read the 8 dataset-conditioned special output channels (control[15..22])
        lib.MI_get_output_channels.argtypes = [ctypes.POINTER(ctypes.c_uint8)]
        lib.MI_get_output_channels.restype  = ctypes.c_int

        # Drive the genstg() co-routine — must be called every tick after monitr()
        lib.MI_genstg.argtypes = []
        lib.MI_genstg.restype  = None

        # Stage-machine diagnostics
        lib.MI_get_prog_marker.argtypes    = []
        lib.MI_get_prog_marker.restype     = ctypes.c_int
        lib.MI_get_current_stage.argtypes  = []
        lib.MI_get_current_stage.restype   = ctypes.c_int
        lib.MI_get_demanded_stage.argtypes = []
        lib.MI_get_demanded_stage.restype  = ctypes.c_int
        lib.MI_get_next_stage.argtypes     = []
        lib.MI_get_next_stage.restype      = ctypes.c_int
        lib.MI_get_scan_count.argtypes     = []
        lib.MI_get_scan_count.restype      = ctypes.c_int
        lib.MI_set_demanded_stage.argtypes = [ctypes.c_int]
        lib.MI_set_demanded_stage.restype  = None

        # Extra diagnostics
        lib.MI_get_wait_stage_change_timer.argtypes = []
        lib.MI_get_wait_stage_change_timer.restype  = ctypes.c_int
        lib.MI_is_in_warmup.argtypes = []
        lib.MI_is_in_warmup.restype  = ctypes.c_int
        lib.MI_set_warmup_counter.argtypes = [ctypes.c_int]
        lib.MI_set_warmup_counter.restype  = None
        lib.MI_set_scan_divisor.argtypes   = [ctypes.c_int]
        lib.MI_set_scan_divisor.restype    = None
        lib.MI_get_kernel_stages.argtypes = []
        lib.MI_get_kernel_stages.restype  = ctypes.c_int
        lib.MI_get_kernel_crb.argtypes = []
        lib.MI_get_kernel_crb.restype  = ctypes.c_int
        lib.MI_get_det_fault.argtypes  = []
        lib.MI_get_det_fault.restype   = ctypes.c_int
        lib.MI_get_mova_fault.argtypes = []
        lib.MI_get_mova_fault.restype  = ctypes.c_int

        # Message ring buffer access
        lib.MI_get_current_msg_num.argtypes = []
        lib.MI_get_current_msg_num.restype  = ctypes.c_int
        lib.MI_get_msg_field.argtypes = [ctypes.c_int, ctypes.c_int]
        lib.MI_get_msg_field.restype  = ctypes.c_int

        # Error store access
        lib.MI_get_error_count.argtypes = []
        lib.MI_get_error_count.restype  = ctypes.c_int
        lib.MI_get_error_store.argtypes = [ctypes.c_int, ctypes.c_int]
        lib.MI_get_error_store.restype  = ctypes.c_int

        # Data plan switching
        lib.MI_get_active_plan.argtypes = []
        lib.MI_get_active_plan.restype  = ctypes.c_int
        lib.MI_switch_plan.argtypes     = [ctypes.c_int]
        lib.MI_switch_plan.restype      = ctypes.c_int
        lib.MI_get_tod_plan.argtypes    = []
        lib.MI_get_tod_plan.restype     = ctypes.c_int

        # Kernel version string
        lib.MI_get_kernel_version.argtypes = []
        lib.MI_get_kernel_version.restype  = ctypes.c_char_p

        # Direct GS_check call — simulation warmup bypass
        lib.MI_gs_check.argtypes = []
        lib.MI_gs_check.restype  = None

        # TOD time offset for testing
        lib.MI_set_time_offset.argtypes = [ctypes.c_long]
        lib.MI_set_time_offset.restype  = None
        lib.MI_get_time_offset.argtypes = []
        lib.MI_get_time_offset.restype  = ctypes.c_long

        # Read back actual detector states (includes special conditioning outputs)
        lib.MI_get_all_detectors.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_int]
        lib.MI_get_all_detectors.restype  = None

        # TMA detector counts and period info
        lib.MI_get_tma_det_flow.argtypes      = [ctypes.c_int]
        lib.MI_get_tma_det_flow.restype       = ctypes.c_int
        lib.MI_get_tma_period_start.argtypes  = []
        lib.MI_get_tma_period_start.restype   = ctypes.c_long
        lib.MI_get_tma_period_sec.argtypes    = []
        lib.MI_get_tma_period_sec.restype     = ctypes.c_int
        # Lane-level TMA counts (populated at period end only — kept for future use)
        lib.MI_get_tma_lane_xflow.argtypes    = [ctypes.c_int]
        lib.MI_get_tma_lane_xflow.restype     = ctypes.c_int
        lib.MI_get_tma_lane_inflow.argtypes   = [ctypes.c_int]
        lib.MI_get_tma_lane_inflow.restype    = ctypes.c_int

    # ── Public API ────────────────────────────────────────────────────────────

    @property
    def is_simulation(self) -> bool:
        return self._simulation

    @property
    def dataset_loaded(self) -> bool:
        return self._dataset_loaded

    @property
    def kernel_version(self) -> str:
        if self._lib is None:
            return "M8.0 (sim)"
        v = self._lib.MI_get_kernel_version()
        return v.decode("ascii", errors="replace") if v else "M8.0"

    def set_time_offset(self, hours: float) -> None:
        """Shift the kernel's view of time by N hours (0 = real time). Test use only."""
        if self._lib is None:
            return
        self._lib.MI_set_time_offset(int(hours * 3600))

    @property
    def time_offset_hours(self) -> float:
        if self._lib is None:
            return 0.0
        return self._lib.MI_get_time_offset() / 3600.0

    def load_dataset(self, dataset) -> bool:
        """
        Initialise the kernel modules and load the dataset into the C kernel.
        Must be called before the first tick. Caller must ensure the tick loop
        is not running while this executes (race → SEGV).
        """
        if self._simulation:
            logger.info("Stream %d: [SIM] dataset loaded", self.stream_id)
            self._dataset_loaded = True
            return True

        try:
            # Init sequence mirrors legacy svc_mova_0.py exactly:
            #   1. XML_DataSet_Initialise  — init XML subsystem
            #   2. CI_init_core_module     — init core pointer stubs (empty)
            #   3. CI_init_core_dynmaic_data — allocate dynamic data structures
            #   4. XML_DataSet_ReadAllDataPlansFromFile — populate data from file
            # monitr() then calls CI_init_core_module() again internally on its
            # first tick (MONITR_TASK_INIT_START) to re-link pointers into the
            # now-populated dataset. This double-call is intentional and correct.
            self._lib.XML_DataSet_Initialise()
            self._lib.CI_init_core_module()
            self._lib.CI_init_core_dynmaic_data()

            filepath      = dataset.source_path.encode("utf-8")
            controller_id = (dataset.header.stream_id_str or "1").encode("utf-8")

            result = self._lib.XML_DataSet_ReadAllDataPlansFromFile(filepath, controller_id)
            if result == 0:   # MV_SUCCESS
                self._dataset_loaded = True
                # Reset kernel runtime flags so stale state from a previous run
                # doesn't carry over. Without this, io[27]/io[19] may be 1 from
                # the prior session, causing genstg to run during warmup and
                # issue wrong-stage errors before the TLC confirms anything.
                self._lib.MI_set_io_flag(27, 0)  # MOVA_ON = off
                self._lib.MI_set_io_flag(19, 0)  # ON_CONTROL = off
                self._lib.MI_set_io_flag(16, 0)  # ERROR_COUNT = 0
                logger.info(
                    "Stream %d: kernel dataset loaded — %s (controller=%s)",
                    self.stream_id, dataset.header.title, dataset.header.stream_id_str,
                )
                return True
            else:
                logger.error(
                    "Stream %d: XML_DataSet_ReadAllDataPlansFromFile returned %d — %s",
                    self.stream_id, result, dataset.source_path,
                )
                return False

        except Exception as exc:
            logger.error("Stream %d: dataset load exception: %s", self.stream_id, exc)
            return False

    def tick(self, buffers: KernelBuffers) -> None:
        """
        Single kernel execution step.
        1. Marshal inputs from KernelBuffers into C arrays via MI_* functions
        2. Call monitr()
        3. Read outputs back from C arrays into KernelBuffers
        """
        if self._simulation:
            self._sim_tick(buffers)
            return

        if not self._dataset_loaded:
            logger.debug("Stream %d: tick skipped — no dataset loaded", self.stream_id)
            return

        # ── Write user-controlled flags before kernel runs ─────────────────
        # io[27] (MOVA_ON) is user-controlled via the API/web UI. We write it
        # to the C kernel on every tick so it survives the first-tick cold-start
        # (monitr's cold-start path resets io[27]=0; this restores it on tick+1).
        self._lib.MI_set_io_flag(27, int(buffers.io[27]))

        # ── Write inputs to kernel ──────────────────────────────────────────

        # Detectors
        for i in range(MAX_DETECTORS):
            self._det_buf[i] = int(buffers.detsin[i])
        self._lib.MI_set_detectors_status(self._det_buf)

        # Confirms (excludes CRB — set separately)
        for i in range(MAX_CONFIRMS - 1):
            self._conf_buf[i] = int(buffers.confin[i])
        self._lib.MI_set_stages_confirmation(self._conf_buf)

        # CRB
        self._lib.MI_set_crb_bit(1 if buffers.crb else 0)

        # ── Run kernel ──────────────────────────────────────────────────────
        self._lib.monitr()

        # Drive genstg every 5 ticks (500ms) — matches the embedded system rate.
        # In the RTOS genstg ran at 500ms; its intergreen/stage-ending counters
        # assume 500ms per call. Calling it every 100ms makes all timeouts 5× too
        # fast, causing INTERGREEN_NOT_ENDED errors after ~7s instead of ~38s.
        # Only drive when ON_CONTROL is active (io[19] > 0).
        self._genstg_div = (self._genstg_div + 1) % self._genstg_div_max
        if self._genstg_div == 0 and self._lib.MI_get_io_flag(19) > 0:
            self._lib.MI_genstg()

        # ── Read outputs from kernel ────────────────────────────────────────
        self._lib.MI_get_control_bits_status(self._control_buf)

        # Force bits → dout[0..9]
        for s in range(MAX_FORCE):
            buffers.dout[s] = int(self._control_buf[s])

        # HI (index 10), TO (index 11), Sync (index 12)
        if _CONTROL_SIZE > 10:
            buffers.dout[17] = int(self._control_buf[10])  # HI
        if _CONTROL_SIZE > 11:
            buffers.dout[16] = int(self._control_buf[11])  # TO
        if _CONTROL_SIZE > 12:
            # The kernel emits a 1-tick-wide (100ms) pulse on control[12] every 5
            # ticks (500ms).  Toggle a held state on each pulse edge so the UI sees
            # a proper 50 % duty-cycle 1 Hz square wave instead of a narrow blip.
            if int(self._control_buf[12]):
                self._sync_state ^= 1
            buffers.dout[18] = self._sync_state

        # Fault outputs — driven directly from kernel Tdinout state.
        # DET FAULT (dout[19]): set by handle_faulty_detector() in detscn.c.
        # MOVA FAULT (dout[20]): ON when off-control OR when phone home fires
        #   (io[17]>=99 = phone home active, regardless of EC).
        buffers.dout[19] = self._lib.MI_get_det_fault()
        buffers.dout[20] = self._lib.MI_get_mova_fault()

        # Derive output_mode from HI/TO/force state
        buffers.output_mode = self._derive_output_mode(buffers)

        # Warmup counter — starts at -1, counts stage confirm changes during warmup
        buffers.warmup_counter = self._lib.CI_get_warmup_counter()

        # Read real Tcomshr->io[] back so the snapshot reflects kernel state.
        # io[27] (MOVA_ON) is excluded — it's user-controlled and written to
        # the kernel at the top of each tick; reading it back would overwrite the
        # Python buffer with the kernel's stale 0 from the cold-start reset.
        for i in range(min(64, len(buffers.io))):
            if i != 27:
                buffers.io[i] = self._lib.MI_get_io_flag(i)

        # Read back actual detector states from detsin[] — includes special conditioning
        # outputs (SC_ProcessAllRules writes back via SetDetectorStatus into detsin[]).
        # Stored separately in buffers.kernel_deton so the injected din[] isn't polluted
        # and conditioned virtual detectors are visible to the UI and snapshot.
        n_dets = min(len(buffers.kernel_deton), 64)
        if n_dets > 0:
            self._lib.MI_get_all_detectors(self._det_buf, n_dets)
            for i in range(n_dets):
                buffers.kernel_deton[i] = int(self._det_buf[i])

        # Special outputs — 8 dataset-conditioned channels at control[15..22]
        self._lib.MI_get_output_channels(self._outchan_buf)
        for i in range(8):
            buffers.special_outputs[i] = int(self._outchan_buf[i])

        # Stage-machine diagnostics
        buffers.prog_marker               = self._lib.MI_get_prog_marker()
        buffers.kernel_current_stage      = self._lib.MI_get_current_stage()
        buffers.kernel_demanded_stage     = self._lib.MI_get_demanded_stage()
        buffers.kernel_next_stage         = self._lib.MI_get_next_stage()
        buffers.wait_stage_change_timer   = self._lib.MI_get_wait_stage_change_timer()
        buffers.kernel_in_warmup          = self._lib.MI_is_in_warmup()
        buffers.kernel_stages             = self._lib.MI_get_kernel_stages()
        buffers.kernel_crb                = self._lib.MI_get_kernel_crb()
        buffers.io[63]                    = self._lib.MI_get_scan_count() & 0x7F  # scan counter in spare io slot

        # Current stage — kernel sets this via genstg in control[]
        # Find highest set force bit (1-based stage number)
        for s in range(MAX_FORCE - 1, -1, -1):
            if buffers.dout[s]:
                buffers.current_stage = s + 1
                break

    def poll_messages(
        self,
        noStages: int = 0,
        noLinks:  int = 0,
        noLanes:  int = 0,
        noDets:   int = 0,
    ) -> list:
        """
        Return any kernel decision messages written since the last call.
        Must be called from the tick thread only.
        noStages/noLinks/noLanes/noDets come from the loaded dataset and are
        needed to decode variable-length message fields correctly.
        """
        if self._lib is None or self._simulation:
            return []

        import time
        MAX_MSGS = 50

        cur = self._lib.MI_get_current_msg_num()  # 1-based, wraps 1..50

        if self._last_msg_num == 0:
            # First poll — anchor position, don't emit stale startup history.
            self._last_msg_num = cur
            return []

        if cur == self._last_msg_num:
            return []

        if cur > self._last_msg_num:
            count = cur - self._last_msg_num
        else:
            count = (MAX_MSGS - self._last_msg_num) + cur
        count = min(count, MAX_MSGS)

        now = time.time()
        results = []
        for i in range(count):
            slot = (self._last_msg_num - 1 + i) % MAX_MSGS
            msg = self._decode_msg(slot, now, noStages, noLinks, noLanes, noDets)
            if msg:
                results.append(msg)

        self._last_msg_num = cur
        return results

    def _decode_msg(
        self, slot: int, ts: float,
        noStages: int, noLinks: int, noLanes: int, noDets: int,
    ) -> dict | None:
        # Read enough fields to cover any message type
        # Worst case: msg 5.1 with large junction: [11 + noLinks + noLanes + 1]
        n_fields = max(80, 12 + noLinks + noLanes)
        raw = [self._lib.MI_get_msg_field(slot, i) for i in range(n_fields)]

        msg_type = raw[0]
        msg_sub  = raw[1]

        if msg_type == 0:
            return None  # uninitialised slot

        desc = _format_mova_msg(msg_type, msg_sub, raw, noStages, noLinks, noLanes, noDets)

        return {
            "ts":   ts,
            "type": msg_type,
            "sub":  msg_sub,
            "desc": desc,
            "raw":  raw[:64],   # 64 fields covers type-1/1 lane flows for up to 58 lanes
        }

    def set_demanded_stage(self, stage: int) -> None:
        """Directly set the junction demanded stage (simulation/testing only)."""
        if self._lib is not None and not self._simulation:
            self._lib.MI_set_demanded_stage(stage)

    def poll_errors(self) -> list:
        """
        Read new entries from the kernel ErrorStore ring buffer.
        Returns a list of dicts for entries added since last call.
        Each dict: {error_id, day, month, year, hours, minutes, data}
        ErrorCount is 1-based and wraps at 100; we track the last seen value.
        """
        if self._lib is None or self._simulation:
            return []
        count = self._lib.MI_get_error_count()
        if count == self._last_error_count:
            return []
        errors = []
        # ErrorCount is 1-based: the kernel writes to slot (ErrorCount-1) then increments.
        # After N errors ErrorCount = N+1. New 0-based slot indices are [prev-1 .. count-2].
        prev = self._last_error_count   # 1-based, starts at 1
        n    = count
        MAX  = 100
        if n > prev:
            rows = range(prev - 1, n - 1)
        else:
            # wrapped (n < prev means ErrorCount looped back through 1)
            rows = list(range(prev - 1, MAX)) + list(range(0, n - 1))
        for row in rows:
            raw_id_day = self._lib.MI_get_error_store(row, 0)
            raw_my     = self._lib.MI_get_error_store(row, 1)
            raw_hm     = self._lib.MI_get_error_store(row, 2)
            raw_data   = self._lib.MI_get_error_store(row, 3)
            error_id   = (raw_id_day // 1000) - 10
            day        = raw_id_day % 1000
            month      = raw_my // 100
            year       = raw_my % 100
            hours      = raw_hm // 100
            minutes    = raw_hm % 100
            errors.append({
                "error_id": error_id,
                "day": day, "month": month, "year": year,
                "hours": hours, "minutes": minutes,
                "data": raw_data,
            })
        self._last_error_count = n
        return errors

    def get_active_plan(self) -> int:
        """Return the currently active kernel data plan number (1-based, 0=none)."""
        if self._lib is None or self._simulation:
            return 0
        return self._lib.MI_get_active_plan()

    def switch_plan(self, plan_num: int) -> bool:
        """Switch to a different data plan (1-based). Returns True on success."""
        if self._lib is None or self._simulation:
            return False
        return bool(self._lib.MI_switch_plan(plan_num))

    def get_tod_plan(self) -> int:
        """Return which plan the kernel TOD table says should be active now."""
        if self._lib is None or self._simulation:
            return 0
        return self._lib.MI_get_tod_plan()

    def tma_det_counts(self, no_dets: int) -> dict:
        """
        Return per-detector vehicle counts from the kernel's TMA module for the current
        TMAPRD period. Reads _currPeriod._detFlow[det_id] (0-based).
        Counts include special-condition virtual detector activations — the kernel's
        authoritative view, not just what the IO layer injected.
        Counts are cumulative from period_start; they reset at each TMAPRD boundary.
        Returns empty dict if kernel not available or in simulation mode.
        """
        if self._lib is None or self._simulation or no_dets <= 0:
            return {}
        return {
            "det_flow":    [self._lib.MI_get_tma_det_flow(i) for i in range(no_dets)],
            "period_start": self._lib.MI_get_tma_period_start(),
            "period_sec":   self._lib.MI_get_tma_period_sec(),
        }

    def set_speed(self, multiplier: int) -> None:
        """
        Set kernel simulation speed multiplier (1, 2, or 5).
        Both the InputScan gate (C) and genstg gate (Python) are adjusted so
        stage timers run at multiplier × real speed.
          1x → divisor=5 (500ms scan, real time)
          2x → divisor=2 (200ms scan, ~2.5× faster)
          5x → divisor=1 (100ms scan, 5× faster)
        """
        multiplier = max(1, min(5, multiplier))
        divisor = max(1, 5 // multiplier)
        self._speed_multiplier = multiplier
        self._genstg_div_max   = divisor
        self._genstg_div       = 0   # reset phase
        if self._lib is not None and not self._simulation:
            self._lib.MI_set_scan_divisor(divisor)
        logger.info("Stream %d: speed set to %dx (scan divisor=%d)",
                    self.stream_id, multiplier, divisor)

    @property
    def speed_multiplier(self) -> int:
        return self._speed_multiplier

    def set_warmup_counter(self, value: int) -> None:
        """
        Reset the warmup counter to re-trigger GS_check.
        Set to Tcomshr->stages (not stages+1) so warmup_check() calls GS_check
        on the next detscn cycle — used for fault recovery when EC drops to 0.
        """
        if self._lib is not None and not self._simulation:
            self._lib.MI_set_warmup_counter(value)

    def set_crb_in_kernel(self, value: bool) -> None:
        """Write CRB directly to confin[mxconf-1] in the C kernel. Used in bypass path."""
        if self._lib is not None and not self._simulation:
            self._lib.MI_set_crb_bit(1 if value else 0)

    def gs_check(self) -> None:
        """
        Call GS_check_and_update_io_flags() directly.
        Used in SimulatedIO mode to bypass the warmup_check() PM-transition gate.
        Requires MOVA_ON=1 and WC set before calling.
        """
        if self._lib is not None and not self._simulation:
            self._lib.MI_gs_check()

    def set_io_flag(self, index: int, value: int) -> bool:
        """Write directly to Tcomshr->io[index] in the C kernel (0-63 valid)."""
        if self._simulation:
            return False
        if self._lib is None:
            return False
        result = self._lib.MI_set_io_flag(index, value)
        return result == 0  # MV_SUCCESS = 0

    def shutdown(self) -> None:
        """Release kernel resources and remove the per-stream temp .so copy."""
        self._lib = None
        if self._lib_tmp_path and os.path.exists(self._lib_tmp_path):
            try:
                os.unlink(self._lib_tmp_path)
            except OSError:
                pass
        logger.info("Stream %d: kernel shutdown", self.stream_id)

    # ── Helpers ───────────────────────────────────────────────────────────────

    def _derive_output_mode(self, buffers: KernelBuffers) -> int:
        """Infer output mode from kernel output state."""
        from pci_mova.core.model.enums import OutputMode
        hi = buffers.dout[17]
        to = buffers.dout[16]
        any_force = any(buffers.dout[s] for s in range(MAX_FORCE))

        if to:
            return OutputMode.MOVA_JUST_ON
        if hi:
            return OutputMode.MOVA_IN_INTERGREEN
        if any_force:
            return OutputMode.NORMAL
        return OutputMode.MOVA_OFF

    # ── Simulation stub ───────────────────────────────────────────────────────

    def _sim_tick(self, buffers: KernelBuffers) -> None:
        """
        Minimal simulation — passes CRB through to output mode.
        """
        if buffers.crb and buffers.io[27] == 1:
            buffers.output_mode = OutputMode.NORMAL
        else:
            buffers.output_mode = OutputMode.MOVA_OFF
