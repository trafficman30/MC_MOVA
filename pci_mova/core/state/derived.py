"""
pci_mova.core.state.derived
DerivedOutputs — named state bits decoded from the MOVA M8 decision message stream.

These extend the standard XKOP IO signals (stage forces, TO, HI, SYNC, FLT) with
data the kernel expresses only through its commissioning terminal messages.

Sources:
  Type 1/1  — start-of-green: per-lane smoothed flow, saturation flags
  Type 1/2  — cycle data: cycle time, per-stage drift
  Type 2    — variable min green: VDG active, per-link min green
  Type 3/0  — endsat check: per-link endsat bits (every 2s during green)
  Type 3/2  — EP events: hold marker, extension marker
  Type 4    — optimiser assessment: benefit, disbenefit, predicted red, per-link detail
  Type 5/3  — stage change (OSAT+ES): per-link osat+es probability
  Type 5/4  — stage change (EM/PR truncation): which stages are EM and PR
  Type 6/1  — intergreen start: per-stage demand bits
"""

import time
from typing import List, Optional


class DerivedOutputs:
    """
    Live snapshot of derived kernel state, updated from decision messages as
    they arrive from poll_messages(). Thread access safe under CPython GIL.
    """

    # Thresholds — may later come from dataset config
    OSAT_THRESHOLD = 20    # disbenefit units → oversaturated bit
    FLOW_THRESHOLD = 800   # veh/hr → high_flow bit

    def __init__(self) -> None:
        self.no_links:  int = 0
        self.no_lanes:  int = 0
        self.no_stages: int = 0

        # ── Endsat (type 3/0 — every 2s during a green) ──────────────────────
        self.endsat_link: List[int] = []   # per-link endsat bits
        self.endsat_any:  int = 0          # any link saturated

        # ── EP extension / hold (type 3/2) ────────────────────────────────────
        self.ep_hold:  int = 0   # holding current green (don't change yet)
        self.ep_ext:   int = 0   # extending current green
        self.ep_timer: int = 0   # sistim value when EP checked

        # ── Variable min green (type 2) ───────────────────────────────────────
        self.vdg_active:    int = 0        # type-2 message arrived this stage
        self.vdg_stage_min: int = 0        # stamin (var min green for this stage, tenths)
        self.vdg_link_min:  List[int] = [] # per-link var min green

        # ── Optimiser (type 4 — every 2s) ────────────────────────────────────
        self.benefit:       int = 0
        self.disbenefit:    int = 0
        self.predicted_red: int = 0
        self.oversaturated: int = 0        # disbenefit > OSAT_THRESHOLD

        # Per-link optimiser detail (type 4 per-link block)
        self.link_netben: List[int] = []   # net benefit per link
        self.link_ben:    List[int] = []   # link benefit (unsigned)
        self.link_fred:   List[int] = []   # free red percentage per link
        self.link_xg:     List[int] = []   # excess green percentage per link

        # ── OSAT+ES per link (type 5/3) ───────────────────────────────────────
        self.osat_es: List[int] = []       # osat+es probability per link

        # ── Lane flows (type 1/1 — every start-of-green) ─────────────────────
        self.smoothed_flow:  List[int] = []  # per lane (veh/hr)
        self.lane_sat:       List[int] = []  # per lane saturation flag
        self.junction_flow:  int = 0         # total junction smoothed flow
        self.lao_smx:        int = 0         # lane approach over-sat multiplier
        self.lam:            int = 0         # lane approach management
        self.high_flow:      int = 0         # any lane > FLOW_THRESHOLD
        self.cut_max:        int = 0         # LAM cut max

        # ── Cycle / drift / suspect detectors (type 1/2 — every start-of-green) ─
        self.cycle_time:   int = 0
        self.drift_max:    List[int] = []    # per stage
        self.suspect_dets: List[int] = []    # per detector (0=ok, 1=suspect)
        self.suspect_any:  int = 0
        self.no_dets:      int = 0

        # ── Stage demands during intergreen (type 6/1) ────────────────────────
        self.stage_demand: List[int] = []    # per stage (1=demanded)
        self.link_bonus:   List[int] = []    # per link bonus pulse active

        # ── EM/PR truncation (type 5/4) ───────────────────────────────────────
        self.em_pr_truncation: int = 0
        self.em_stage:         int = 0       # early minimum (EM) demanded stage
        self.pr_stage:         int = 0       # predicted red (PR) demanded stage

        # ── Composite health (from buffers, not messages) ─────────────────────
        self.healthy: int = 0
        self.plan_id: int = 0

        # ── Timestamps for staleness display ─────────────────────────────────
        self._ts_endsat: float = 0.0
        self._ts_ep:     float = 0.0
        self._ts_opt:    float = 0.0
        self._ts_flow:   float = 0.0
        self._ts_vdg:    float = 0.0
        self._ts_demand: float = 0.0

    def configure(self, no_links: int, no_lanes: int, no_stages: int, no_dets: int = 0) -> None:
        """Set dataset dimensions and reset all arrays. Call on dataset load."""
        self.no_links  = max(0, no_links)
        self.no_lanes  = max(0, no_lanes)
        self.no_stages = max(0, no_stages)
        self.no_dets   = max(0, no_dets)

        self.endsat_link  = [0] * self.no_links
        self.vdg_link_min = [0] * self.no_links
        self.link_netben  = [0] * self.no_links
        self.link_ben     = [0] * self.no_links
        self.link_fred    = [0] * self.no_links
        self.link_xg      = [0] * self.no_links
        self.osat_es      = [0] * self.no_links
        self.link_bonus   = [0] * self.no_links
        self.smoothed_flow = [0] * self.no_lanes
        self.lane_sat      = [0] * self.no_lanes
        self.drift_max     = [0] * self.no_stages
        self.stage_demand  = [0] * self.no_stages
        self.suspect_dets  = [0] * self.no_dets
        self._reset_scalars()

    def _reset_scalars(self) -> None:
        self.endsat_any = 0
        self.ep_hold = self.ep_ext = self.ep_timer = 0
        self.vdg_active = self.vdg_stage_min = 0
        self.benefit = self.disbenefit = self.predicted_red = self.oversaturated = 0
        self.junction_flow = self.lao_smx = self.lam = self.cut_max = 0
        self.high_flow = 0
        self.cycle_time = 0
        self.em_pr_truncation = self.em_stage = self.pr_stage = 0
        self.healthy = self.plan_id = 0

    def reset(self) -> None:
        """Clear all state — call on stream stop/reset."""
        self.configure(self.no_links, self.no_lanes, self.no_stages)

    def update(self, msg: dict) -> None:
        """Process one decoded message dict from poll_messages()."""
        t   = msg.get("type", 0)
        s   = msg.get("sub",  0)
        raw = msg.get("raw",  [])
        ts  = msg.get("ts",   0.0)

        def r(i: int) -> int:
            return int(raw[i]) if i < len(raw) else 0

        if t == 1 and s == 1:
            # Start of green — lane flows and saturation
            # raw[6..+noLanes] = SMF per lane, [6+noLanes] = junction total
            # [6+noLanes+1..+noLanes] = SAT per lane
            # [6+2*noLanes+1] = laosmx, [+2] = lam, [+3] = totlam, [+4] = cutmx
            i = 6
            for ln in range(self.no_lanes):
                if ln < len(self.smoothed_flow):
                    self.smoothed_flow[ln] = r(i + ln)
            i += self.no_lanes
            self.junction_flow = r(i); i += 1
            for ln in range(self.no_lanes):
                if ln < len(self.lane_sat):
                    self.lane_sat[ln] = r(i + ln)
            i += self.no_lanes
            self.lao_smx  = r(i)
            self.lam      = r(i + 1)
            self.cut_max  = r(i + 3)
            self.high_flow = 1 if any(f > self.FLOW_THRESHOLD for f in self.smoothed_flow) else 0
            # New green — reset per-green EP/VDG state
            self.ep_hold = self.ep_ext = 0
            self.vdg_active = 0
            self._ts_flow = ts

        elif t == 1 and s == 2:
            # Cycle time, per-stage drift, and per-detector suspect flags
            # Format: raw[2]=cycle, [3..3+noStages-1]=dmx, [3+noStages..+noDets-1]=sus
            self.cycle_time = r(2)
            for st in range(self.no_stages):
                if st < len(self.drift_max):
                    self.drift_max[st] = r(3 + st)
            i = 3 + self.no_stages
            for det in range(self.no_dets):
                if det < len(self.suspect_dets):
                    self.suspect_dets[det] = r(i + det)
            self.suspect_any = 1 if any(self.suspect_dets) else 0

        elif t == 2:
            # Variable min green is active this scan
            self.vdg_stage_min = r(3)
            for lk in range(self.no_links):
                if lk < len(self.vdg_link_min):
                    self.vdg_link_min[lk] = r(4 + lk)
            self.vdg_active = 1
            self._ts_vdg = ts

        elif t == 3 and s != 2:
            # Endsat check (sub = total field count, not 2)
            # raw[4..4+noLinks-1] = esli per link (1 = endsat reached)
            for lk in range(self.no_links):
                if lk < len(self.endsat_link):
                    self.endsat_link[lk] = r(4 + lk)
            self.endsat_any = 1 if any(self.endsat_link) else 0
            self._ts_endsat = ts

        elif t == 3 and s == 2:
            # EP hold / extension check
            self.ep_timer = r(2)
            self.ep_hold  = r(4)
            self.ep_ext   = r(5)
            self._ts_ep = ts

        elif t == 4:
            # Optimiser assessment — overall junction BDR + per-link detail
            self.benefit       = r(4)
            self.disbenefit    = r(5)
            self.predicted_red = r(6)
            self.oversaturated = 1 if self.disbenefit > self.OSAT_THRESHOLD else 0
            # Per-link block: linkno(1-based), netben, ben, fred%, xg%
            length = s  # raw[1] = total fields written for type 4
            i = 7
            while i + 4 < length and i + 4 < len(raw):
                lk_idx = r(i) - 1  # convert 1-based linkno to 0-based
                if 0 <= lk_idx < self.no_links:
                    self.link_netben[lk_idx] = r(i + 1)
                    self.link_ben[lk_idx]    = r(i + 2)
                    self.link_fred[lk_idx]   = r(i + 3)
                    self.link_xg[lk_idx]     = r(i + 4)
                i += 5
            self._ts_opt = ts

        elif t == 5 and s == 3:
            # Stage change with OSAT+ES — per-link probability
            for lk in range(self.no_links):
                if lk < len(self.osat_es):
                    self.osat_es[lk] = r(7 + lk)

        elif t == 5 and s == 4:
            # Stage change with EM/PR truncation
            self.em_pr_truncation = 1
            self.em_stage = r(7)   # EM (early minimum) demanded stage
            self.pr_stage = r(8)   # PR (predicted red) demanded stage

        elif t == 6 and s == 1:
            # Intergreen start — per-stage demand and per-link bonus
            for st in range(self.no_stages):
                if st < len(self.stage_demand):
                    self.stage_demand[st] = r(3 + st)
            for lk in range(self.no_links):
                if lk < len(self.link_bonus):
                    self.link_bonus[lk] = r(3 + self.no_stages + lk)
            self.ep_hold = 0          # EP hold ends on intergreen start
            self.em_pr_truncation = 0 # truncation resolved by stage change
            self._ts_demand = ts

    def update_from_buffers(self, buffers, active_plan: int) -> None:
        """Update composite fields from kernel buffer state (not messages)."""
        ec      = int(buffers.io[16]) if len(buffers.io) > 16 else 0
        on_ctrl = int(buffers.io[19]) if len(buffers.io) > 19 else 0
        in_wu   = int(getattr(buffers, "kernel_in_warmup", 1))
        self.healthy = 1 if (on_ctrl == 1 and ec == 0 and in_wu == 0) else 0
        self.plan_id = active_plan

    def snapshot(self) -> dict:
        """JSON-serialisable snapshot of all derived state."""
        now = time.time()

        def age(ts: float) -> Optional[float]:
            return round(now - ts, 1) if ts else None

        return {
            # Composite
            "healthy":           self.healthy,
            "plan_id":           self.plan_id,
            # Endsat
            "endsat_any":        self.endsat_any,
            "endsat_link":       list(self.endsat_link),
            "endsat_age_s":      age(self._ts_endsat),
            # EP
            "ep_hold":           self.ep_hold,
            "ep_ext":            self.ep_ext,
            "ep_timer":          self.ep_timer,
            "ep_age_s":          age(self._ts_ep),
            # VDG
            "vdg_active":        self.vdg_active,
            "vdg_stage_min":     self.vdg_stage_min,
            "vdg_link_min":      list(self.vdg_link_min),
            "vdg_age_s":         age(self._ts_vdg),
            # Optimiser
            "benefit":           self.benefit,
            "disbenefit":        self.disbenefit,
            "predicted_red":     self.predicted_red,
            "oversaturated":     self.oversaturated,
            "osat_threshold":    self.OSAT_THRESHOLD,
            "link_netben":       list(self.link_netben),
            "link_ben":          list(self.link_ben),
            "link_fred":         list(self.link_fred),
            "link_xg":           list(self.link_xg),
            "osat_es":           list(self.osat_es),
            "opt_age_s":         age(self._ts_opt),
            # Flow
            "smoothed_flow":     list(self.smoothed_flow),
            "lane_sat":          list(self.lane_sat),
            "junction_flow":     self.junction_flow,
            "lao_smx":           self.lao_smx,
            "lam":               self.lam,
            "cut_max":           self.cut_max,
            "high_flow":         self.high_flow,
            "flow_threshold":    self.FLOW_THRESHOLD,
            "flow_age_s":        age(self._ts_flow),
            # Cycle / drift
            "cycle_time":        self.cycle_time,
            "drift_max":         list(self.drift_max),
            # Stage demands
            "stage_demand":      list(self.stage_demand),
            "link_bonus":        list(self.link_bonus),
            "demand_age_s":      age(self._ts_demand),
            # EM/PR
            "em_pr_truncation":  self.em_pr_truncation,
            "em_stage":          self.em_stage,
            "pr_stage":          self.pr_stage,
            # Suspect detectors
            "suspect_dets":      list(self.suspect_dets),
            "suspect_any":       self.suspect_any,
            # Dimensions (for JS rendering)
            "no_links":          self.no_links,
            "no_lanes":          self.no_lanes,
            "no_stages":         self.no_stages,
            "no_dets":           self.no_dets,
        }
