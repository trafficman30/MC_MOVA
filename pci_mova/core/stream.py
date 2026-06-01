"""
pci_mova.core.stream
MovaStream — one instance per MOVA traffic stream.

Owns the 10 Hz tick loop and wires together:
  InputScan → Kernel → OutputScan → FaultMonitor → (1 Hz) StatusMachine

No Chameleon, no shared memory, no platform coupling.
"""

import collections
import logging
import threading
import time
from typing import Callable, Optional

from pci_mova.core.model.buffers import KernelBuffers
from pci_mova.core.model.enums import MovaStatus, OutputMode
from pci_mova.core.io.base import AbstractIO
from pci_mova.core.io.simulated import SimulatedIO
from pci_mova.core.kernel.wrapper import MovaKernel
from pci_mova.core.state.status import StatusMachine
from pci_mova.core.state.faults import FaultMonitor
from pci_mova.core.state.derived import DerivedOutputs
from pci_mova.config import TICK_SEC, SLOW_TICK_DIVISOR, LOG_DIR
from pci_mova.log.logger import StreamLogger

logger = logging.getLogger(__name__)


class MovaStream:
    """
    A single MOVA stream — the top-level runtime unit.

    Each stream has its own:
      - KernelBuffers  (IO state)
      - AbstractIO     (simulated by default, swappable)
      - MovaKernel     (monitr() wrapper)
      - StatusMachine  (state transitions)
      - FaultMonitor   (fault detection)
      - Tick thread    (10 Hz loop)
    """

    def __init__(
        self,
        stream_id: int,
        io_layer: Optional[AbstractIO] = None,
        on_status_change: Optional[Callable[["MovaStream", MovaStatus], None]] = None,
        on_fault: Optional[Callable[["MovaStream", dict], None]] = None,
    ) -> None:
        self.stream_id = stream_id

        # ── Core components ───────────────────────────────────────────────────
        self.buffers = KernelBuffers()
        self.io      = io_layer or SimulatedIO()
        self.kernel  = MovaKernel(stream_id)
        self.status  = StatusMachine(stream_id)
        self.faults  = FaultMonitor(
            stream_id     = stream_id,
            buffers       = self.buffers,
            status        = self.status,
            on_phone_home = self._handle_phone_home,
            on_fault_raised  = lambda e: on_fault(self, e.__dict__) if on_fault else None,
            on_fault_cleared = lambda e: None,
        )

        # ── External callbacks ────────────────────────────────────────────────
        self._on_status_change = on_status_change

        # ── Thread control ────────────────────────────────────────────────────
        self._thread:  Optional[threading.Thread] = None
        self._running: threading.Event = threading.Event()
        self._cycle:   int = 0

        # ── Dataset ───────────────────────────────────────────────────────────
        self._dataset = None

        # ── Message log ───────────────────────────────────────────────────────
        self.messages: collections.deque = collections.deque(maxlen=500)
        self._msg_seq: int = 0

        # ── Derived outputs — decoded from decision message stream ────────────
        self.derived: DerivedOutputs = DerivedOutputs()

        # ── Detector metadata (for UI tooltips) ───────────────────────────────
        self._detector_meta: list = []

        # ── TOD plan switching ────────────────────────────────────────────────
        self._timetable: list     = []   # parsed plan schedules from .mxds AnalysisSets
        self._tod_computed: int   = 0    # Python-computed TOD plan (0 = no timetable)
        self._last_stage: int     = -1   # previous tick's kernel stage (for entry detection)

        # ── Fault recovery tracking ───────────────────────────────────────────
        self._prev_ec: int       = 0   # last seen error count
        self._recovery_cooldown: int = 0  # ticks remaining before next GS_check retry

        # ── Detector counts ───────────────────────────────────────────────────────
        # Cached last-read kernel TMA det_flow array (for snapshot / WS).
        self._det_counts: list = []

        # ── Crash guard ───────────────────────────────────────────────────────
        self._crash_count: int = 0
        self._CRASH_LIMIT  = 10   # consecutive tick failures before FAILED_RESTART

        # ── Historical JSONL log ──────────────────────────────────────────────
        self.stream_logger = StreamLogger(stream_id, LOG_DIR)

        self.status.set(MovaStatus.NOT_STARTED)
        logger.debug("Stream %d created (simulation=%s)", stream_id, self.kernel.is_simulation)

    # ── Lifecycle ─────────────────────────────────────────────────────────────

    def start(self) -> None:
        """Start the tick thread."""
        if self._thread and self._thread.is_alive():
            logger.warning("Stream %d: already running", self.stream_id)
            return

        if self._dataset is None:
            self.status.set(MovaStatus.NO_DATASET)
            logger.warning("Stream %d: no dataset — not starting", self.stream_id)
            return

        self._running.set()
        self._thread = threading.Thread(
            target=self._loop,
            name=f"mova-stream-{self.stream_id}",
            daemon=True,
        )
        self._thread.start()
        self.status.set(MovaStatus.WARMUP)
        logger.info("Stream %d: started", self.stream_id)

    def stop(self) -> None:
        """Stop the tick thread gracefully."""
        self._running.clear()
        self.buffers.io[27] = 0  # Signal kernel: MOVA off
        if self._thread:
            self._thread.join(timeout=2.0)
        self.stream_logger.flush()
        self.status.set(MovaStatus.OFF)
        logger.info("Stream %d: stopped", self.stream_id)

    def load_dataset(self, dataset, auto_start: bool = True) -> bool:
        """Load a parsed MovaDataset into the kernel."""
        ok = self.kernel.load_dataset(dataset)
        if ok:
            self._dataset = dataset
            self.io.reset_confirms(dataset.constants.stagon)
            ig_matrix = {(ig.from_stage, ig.to_stage): ig.value for ig in dataset.interstages}
            self.io.set_intergreen_matrix(ig_matrix)
            # Configure derived outputs with dataset dimensions
            c = dataset.constants
            self.derived.configure(c.links, c.lanes, c.stages, c.detectors)
            self.stream_logger.log_session(c.links, c.lanes, c.stages, c.detectors,
                                           self._detector_meta)
            self._det_counts = [0] * c.detectors
            # Build detector metadata for UI tooltips
            self._detector_meta = [
                {
                    "id":   d.id,
                    "type": d.det_type,
                    "link": d.link_id,
                    "lane": d.lane_id,
                    "dist": d.distance,
                }
                for d in dataset.detectors
            ]
            # Parse timetable for Python-side TOD plan switching.
            # The kernel's XML reader doesn't consume root-level AnalysisSets,
            # so DSH_get_active_data_plan_num_based_on_tod_table always returns 1.
            try:
                from pci_mova.dataset.parser import parse_timetable
                self._timetable = parse_timetable(dataset.source_path)
                self._last_stage = -1
                if self._timetable:
                    logger.info("Stream %d: %d-plan timetable loaded",
                                self.stream_id, len(self._timetable))
            except Exception as exc:
                logger.warning("Stream %d: timetable parse failed: %s", self.stream_id, exc)
                self._timetable = []
            # Auto-start the tick loop if not already running
            if auto_start and not (self._thread and self._thread.is_alive()):
                self.status.set(MovaStatus.INIT)
                self.start()
            elif not auto_start:
                self.status.set(MovaStatus.OFF)
        return ok

    # ── Tick loop ─────────────────────────────────────────────────────────────

    def _loop(self) -> None:
        next_tick = time.perf_counter()
        self._crash_count = 0

        while self._running.is_set():
            next_tick += TICK_SEC

            try:
                self._tick()
                self._crash_count = 0  # reset on each successful tick
            except Exception as exc:
                self._crash_count += 1
                logger.exception(
                    "Stream %d: tick exception %d/%d: %s",
                    self.stream_id, self._crash_count, self._CRASH_LIMIT, exc,
                )
                if self._crash_count >= self._CRASH_LIMIT:
                    logger.error(
                        "Stream %d: %d consecutive failures — stopped (FAILED_RESTART). "
                        "Dataset retained; use Reset to clear.",
                        self.stream_id, self._CRASH_LIMIT,
                    )
                    self.status.set(MovaStatus.FAILED_RESTART)
                    self._running.clear()
                    break

            sleep = next_tick - time.perf_counter()
            if sleep > 0:
                time.sleep(sleep)
            else:
                logger.debug("Stream %d: tick overrun %.3f ms", self.stream_id, -sleep * 1000)

    def _tick(self) -> None:
        self._cycle += 1

        # 1. Read physical truth
        self.io.read_inputs(self.buffers)

        # 2. Update status from CRB
        self._update_crb_status()

        # 3. Run MOVA kernel every tick once a dataset is loaded.
        #    The C kernel is a co-routine with an internal state machine.
        #    MONITR_TASK_INIT_START must execute while CRB=False to
        #    complete cold-start initialisation. If we gate on WARMUP/ON_CONTROL
        #    only, the first tick arrives with CRB=True and the uninitialised
        #    state machine SEGVs. Pass CRB via MI_set_crb_bit; the kernel
        #    manages its own response to CRB state internally.
        if self._dataset is not None and self.status.current not in (
            MovaStatus.NOT_STARTED, MovaStatus.NO_DATASET,
            MovaStatus.NO_LICENCE, MovaStatus.NO_CONFIG, MovaStatus.OFF,
        ):
            self.kernel.tick(self.buffers)
            c = self._dataset.constants
            for msg in self.kernel.poll_messages(
                noStages=c.stages,
                noLinks=c.links,
                noLanes=c.lanes,
                noDets=c.detectors,
            ):
                self._msg_seq += 1
                msg["seq"] = self._msg_seq
                self.messages.append(msg)
                self.derived.update(msg)
                # Type 3 (endsat check, every 2s) already in Derived — skip logging
                # Type 4 (optimiser) sampled: log only every 60s (600 ticks)
                mtype = msg.get("type")
                if mtype == 3:
                    pass
                elif mtype == 4:
                    self._t4_log_ctr = getattr(self, '_t4_log_ctr', 0) + 1
                    if self._t4_log_ctr >= 600:
                        self._t4_log_ctr = 0
                        self.stream_logger.log_msg(msg)
                else:
                    self.stream_logger.log_msg(msg)
            self.derived.update_from_buffers(self.buffers, self.kernel.get_active_plan())
            # Surface kernel IDS errors to FaultMonitor
            for err in self.kernel.poll_errors():
                self.faults.raise_ids_error(err["error_id"], err["data"])
                self.stream_logger.log_error(err)

        # 4. Enforce outputs + safety
        self._output_scan()

        # 5. TOD plan switching — check at stage 1 entry (Python-side because
        #    the kernel's DSH timetable structures aren't populated from .mxds load).
        self._check_tod_plan()

        # 6. Fault recovery — if EC just dropped to 0 while off-control, reset
        #    the warmup counter to re-trigger GS_check on the next detscn cycle.
        #    Mirrors handle_error_count() in detscn.c but acts immediately rather
        #    than waiting the 1-hour check_error_count_time() interval.
        self._check_fault_recovery()

        # 6. Fault and watchdog processing
        self.faults.tick()

        # 6. Slow (1 Hz) tasks
        if self._cycle % SLOW_TICK_DIVISOR == 0:
            self.status.tick()
            self._slow_tick()

        # 7. Logger — heartbeat every 10 s, flush every 30 s; det counts every 60 s
        nd = self.derived.no_dets
        if nd > 0:
            tma_info = self.kernel.tma_det_counts(nd)
            self._det_counts = tma_info.get("det_flow", self._det_counts)
            self.stream_logger.sample_tma(tma_info)
        io = self.buffers.io
        self.stream_logger.tick(
            cs      = getattr(self.buffers, "kernel_current_stage",  0) or 0,
            ds      = getattr(self.buffers, "kernel_demanded_stage", 0) or 0,
            ns      = getattr(self.buffers, "kernel_next_stage",     0) or 0,
            ec      = int(io[16]) if len(io) > 16 else 0,
            wc      = self.buffers.warmup_counter,
            plan    = self.kernel.get_active_plan() if self._dataset else 0,
            on_ctrl = int(io[19]) if len(io) > 19 else 0,
            mova_on = int(io[27]) if len(io) > 27 else 0,
        )

    def _update_crb_status(self) -> None:
        """Transition status based on Controller Ready Bit."""
        current = self.status.current
        crb     = self.buffers.crb

        if not crb and current not in (
            MovaStatus.NOT_STARTED, MovaStatus.NO_DATASET,
            MovaStatus.NO_LICENCE, MovaStatus.NO_CONFIG,
            MovaStatus.OFF, MovaStatus.INIT,
        ):
            if current != MovaStatus.NO_CRB:
                self.status.set(MovaStatus.NO_CRB)

        elif crb and current == MovaStatus.NO_CRB:
            # CRB returned — normal TLC operation, no EC penalty.
            # Only reset WC if warmup was already complete (WC > stages).
            # On a cold start WC = -1, leave it alone — full warmup must proceed.
            if self._dataset is not None:
                stages = self._dataset.constants.stages
                if self.buffers.warmup_counter > stages:
                    self.kernel.set_warmup_counter(stages)
                    logger.info("Stream %d: CRB returned — WC reset to re-trigger GS_check",
                                self.stream_id)
            self.status.set(MovaStatus.WARMUP)

        elif crb and current == MovaStatus.WARMUP:
            # Warmup-to-control transition.
            # NORMAL = forces set, TO cleared (real TLC acknowledged).
            # MOVA_JUST_ON = forces + TO both set — the normal kernel state in
            # simulation where no real TLC is present to acknowledge the takeover.
            # Both mean the kernel has completed warmup and is actively forcing.
            # Additionally accept the kernel's own ON_CONTROL flag (io[19]) as
            # authoritative — it's set by GS_check_and_update_io_flags.
            kernel_on_ctrl = self.buffers.io[19] if len(self.buffers.io) > 19 else 0
            if self.buffers.output_mode in (OutputMode.NORMAL, OutputMode.MOVA_JUST_ON) \
                    or kernel_on_ctrl:
                self.status.set(MovaStatus.ON_CONTROL)

    def _output_scan(self) -> None:
        """
        Determine stage outputs based on kernel output_mode and write to IO.
        Safety: if output_mode is MOVA_OFF, clear all forces.
        """
        if self.buffers.output_mode == OutputMode.MOVA_OFF:
            # Clear all force bits — safety
            from pci_mova.config import MAX_FORCE
            for s in range(1, MAX_FORCE + 1):
                self.buffers.set_force(s, 0)
            self.buffers.dout[17] = 0  # HI off
            self.buffers.dout[16] = 0  # TO off

        self.io.write_outputs(self.buffers)

    def _compute_tod_plan(self) -> int:
        """
        Compute which plan number should be active right now based on the
        timetable and the current kernel time (including any TOD offset).
        Returns 0 if no timetable is loaded.
        """
        if not self._timetable:
            return 0
        offset_sec = int(self.kernel.time_offset_hours * 3600)
        t = time.localtime(time.time() + offset_sec)
        weekday    = t.tm_wday  # 0=Mon … 6=Sun
        cur_min    = t.tm_hour * 60 + t.tm_min
        reversionary = 0
        for plan in self._timetable:
            if plan["reversionary"]:
                reversionary = plan["plan_num"]
            for slot in plan["schedule"]:
                if weekday in slot["days"] and slot["start_min"] <= cur_min < slot["end_min"]:
                    return plan["plan_num"]
        return reversionary or (self._timetable[0]["plan_num"] if self._timetable else 1)

    def _check_tod_plan(self) -> None:
        """Switch data plan at stage 1 entry when the timetable says a different plan is due."""
        if not self._timetable or self._dataset is None:
            return
        cur_stage = getattr(self.buffers, "kernel_current_stage", -1) or -1
        self._tod_computed = self._compute_tod_plan()

        # Trigger once on entry to stage 1 (not every tick while in stage 1)
        if cur_stage == 1 and self._last_stage != 1:
            active = self.kernel.get_active_plan()
            if self._tod_computed > 0 and active != self._tod_computed:
                ok = self.kernel.switch_plan(self._tod_computed)
                logger.info(
                    "Stream %d: TOD plan switch %d→%d at stage 1 — %s",
                    self.stream_id, active, self._tod_computed,
                    "ok" if ok else "failed",
                )

        self._last_stage = cur_stage

    # Ticks between automatic GS_check retries when EC is 1–19 and off-control.
    # 100 ticks = 10 s — long enough that a failing genstg call doesn't spiral.
    _RECOVERY_COOLDOWN_TICKS = 100

    def _check_fault_recovery(self) -> None:
        """
        Auto-retry ON_CONTROL when EC is in the recoverable range (1–19).

        Rules (matching kernel design intent):
          EC = 0        : no action needed — GS_check fires normally
          0 < EC < 20   : retry periodically — reset WC to re-trigger GS_check.
                          Each retry is a fresh attempt; if genstg succeeds,
                          ON_CONTROL=1.  If it fails again, EC increments and
                          we try again after the cooldown.
          EC >= 20      : manual Reset required — do not auto-retry.
        """
        if self._dataset is None:
            return

        ec = int(self.buffers.io[16]) if len(self.buffers.io) > 16 else 0
        on_ctrl  = int(self.buffers.io[19]) if len(self.buffers.io) > 19 else 0
        mova_on  = int(self.buffers.io[27]) if len(self.buffers.io) > 27 else 0

        if self._recovery_cooldown > 0:
            self._recovery_cooldown -= 1

        if (0 < ec < 20
                and on_ctrl == 0
                and mova_on == 1
                and self.buffers.crb
                and self._recovery_cooldown == 0):
            stages = self._dataset.constants.stages
            wc     = self.buffers.warmup_counter
            if wc > stages:
                logger.info(
                    "Stream %d: EC=%d, off-control — resetting WC to re-trigger GS_check",
                    self.stream_id, ec,
                )
                self.kernel.set_warmup_counter(stages)
                self._recovery_cooldown = self._RECOVERY_COOLDOWN_TICKS

        self._prev_ec = ec

    def _slow_tick(self) -> None:
        """1 Hz tasks — currently just a hook for future extension."""
        pass

    # ── Force handling ────────────────────────────────────────────────────────

    def force_stage(self, stage: int) -> None:
        """
        Apply a manual stage force (stage=0 clears all forces).
        Mirrors TryForces() in cradle source.
        """
        from pci_mova.config import MAX_FORCE
        if stage == 0:
            for s in range(1, MAX_FORCE + 1):
                self.buffers.set_force(s, 0)
            self.status.set(self.status.previous)
            logger.info("Stream %d: manual force cleared", self.stream_id)
        elif 1 <= stage <= MAX_FORCE:
            for s in range(1, MAX_FORCE + 1):
                self.buffers.set_force(s, 1 if s == stage else 0)
            self.status.set(MovaStatus.MANUAL_FORCE)
            logger.info("Stream %d: manual force → stage %d", self.stream_id, stage)

    # ── Comms / phone-home ────────────────────────────────────────────────────

    def _handle_phone_home(self) -> None:
        """Triggered when kernel requests a comms call (io[17] >= 99)."""
        logger.info("Stream %d: phone-home — comms requested", self.stream_id)
        # Hook for future instation comms integration

    # ── Message access (thread-safe read under CPython GIL) ──────────────────

    def messages_since(self, since_seq: int) -> list:
        """Return messages with seq > since_seq (ascending order)."""
        return [m for m in self.messages if m["seq"] > since_seq]

    # ── Snapshot for web API ──────────────────────────────────────────────────

    def snapshot(self) -> dict:
        return {
            "stream_id":      self.stream_id,
            "status":         self.status.snapshot(),
            "io":             self.io.snapshot(),
            "buffers":        self.buffers.snapshot(),
            "active_faults":  self.faults.active_faults(),
            "simulation":     self.kernel.is_simulation,
            "simulated_io":   isinstance(self.io, SimulatedIO),
            "running":        self._running.is_set(),
            "dataset":        self._dataset.header.to_dict() if self._dataset else None,
            "active_plan":    self.kernel.get_active_plan(),
            "tod_plan":       self._tod_computed or self.kernel.get_tod_plan(),
            "speed":          self.kernel.speed_multiplier,
            "kernel_version":    self.kernel.kernel_version,
            "time_offset_hours": self.kernel.time_offset_hours,
            "server_time":       time.strftime("%H:%M:%S", time.localtime(time.time() + self.kernel.time_offset_hours * 3600)),
            "server_date":       time.strftime("%a %d %b %Y", time.localtime(time.time() + self.kernel.time_offset_hours * 3600)),
            "derived":           self.derived.snapshot(),
            "detector_meta":     self._detector_meta,
            "det_counts":        list(self._det_counts),
        }
