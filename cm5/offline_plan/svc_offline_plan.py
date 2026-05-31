# svc_offline_plan.py
# Offline fixed-time plan runner for UTC Type 2 outstation.
#
# Drives Fn/Dn (and other control) outputs from offline_plan.json when
# the outstation is in standalone or monitor mode (configurable).
# Stands down immediately when the instation raises opMode to 3.

import json
import logging
import os
import threading
import time
from datetime import datetime, timedelta

log = logging.getLogger('OFFPLAN')

BITMASK_CONTROL = {'Fn', 'Dn', 'SFn'}

_SINGLE_FIELDS = [
    ('go', 'GO'), ('fm', 'FM'), ('ts', 'TS'), ('so', 'SO'),
    ('mo', 'MO'), ('dx', 'DX'), ('pv', 'PV'), ('px', 'PX'),
    ('sg', 'SG'), ('lo', 'LO'), ('ll', 'LL'), ('to', 'TO'),
    ('hi', 'HI'), ('cp', 'CP'), ('ep', 'EP'), ('ff', 'FF'),
]

_MODE_LABELS = {1: 'Standalone', 2: 'Monitor', 3: 'UTC Control'}


class OfflinePlanService:
    """
    Runs fixed-time plans from offline_plan.json when opMode is in the
    configured active modes (default: 1=standalone, 2=monitor).

    File structure:
      {
        "settings": {
          "base_time_mode": "datetime" | "time" | "none",
          "base_time": "01012020 0000"  (or "hhmm" for mode=time),
          "active_in_modes": [1, 2]     // opMode values where plans run
        },
        "scns": {
          "J0331": {
            "plans": { "NAME": { "cycle_secs": N, "offsets": [...] } },
            "timetable": [
              { "days": ["Mon",...], "start": "hhmm", "plan": "NAME" | null }
            ],
            "special_dates": [
              { "date": "ddmmyyyy", "plan": "NAME" | null }
            ]
          }
        }
      }

    Timetable evaluation: for the current day, find the entry with the
    latest "start" <= current time. plan:null means no plan runs from
    that start time (used to end a plan period rather than setting end:).
    """

    def __init__(self, io, ug405_cfg, plan_file, op_mode):
        self._io        = io
        self._ug405_cfg = ug405_cfg
        self._plan_file = plan_file
        self._op_mode   = op_mode

        self._settings    = {}
        self._active_modes= {1, 2}  # default
        self._timetable   = []      # global timetable (shared across all SCNs)
        self._plan_config = {}      # keyed by SCN name — plans only
        self._file_mtime  = 0

        # Per-SCN run state
        self._active_plan     = {}   # scn → plan_name | None
        self._activation_time = {}   # scn → monotonic seconds at activation
        self._last_offset_idx = {}   # scn → last applied offset index

        self._load_plan_file()

    # ── File load / hot-reload ─────────────────────────────────────────────────

    def _load_plan_file(self):
        try:
            mtime = os.path.getmtime(self._plan_file)
            with open(self._plan_file) as f:
                raw = json.load(f)

            settings  = raw.get('settings', {})
            timetable = raw.get('timetable', [])
            scns      = raw.get('scns', {})

            # Pre-parse bitmask strings ("0x01" etc.) to int at load time.
            for scn_data in scns.values():
                for plan in scn_data.get('plans', {}).values():
                    for offset in plan.get('offsets', []):
                        for k in ('fn', 'dn', 'sfn'):
                            if k in offset and isinstance(offset[k], str):
                                offset[k] = int(offset[k], 0)

            self._settings     = settings
            self._timetable    = timetable
            self._plan_config  = scns
            self._active_modes = set(settings.get('active_in_modes', [1, 2]))
            self._file_mtime   = mtime
            log.info("Plan file loaded: %s  SCNs=%s  tt_entries=%d  modes=%s  time_mode=%s",
                     self._plan_file, list(scns.keys()), len(timetable),
                     sorted(self._active_modes),
                     settings.get('base_time_mode', 'none'))
        except FileNotFoundError:
            log.warning("Plan file not found: %s — service will be inactive", self._plan_file)
            self._plan_config = {}
        except Exception as e:
            log.error("Plan file error (%s): %s — keeping last good config",
                      self._plan_file, e)

    # ── Lifecycle ──────────────────────────────────────────────────────────────

    def start(self):
        t = threading.Thread(target=self._run, daemon=True, name='OfflinePlan')
        t.start()

    def _run(self):
        log.info("Offline plan service started  file=%s", self._plan_file)
        while True:
            try:
                self._tick()
            except Exception as e:
                log.error("Tick error: %s", e)
            time.sleep(1)

    # ── Main tick ──────────────────────────────────────────────────────────────

    def _tick(self):
        # Hot-reload: check mtime each tick.
        try:
            mtime = os.path.getmtime(self._plan_file)
            if mtime != self._file_mtime:
                log.info("Plan file changed — reloading")
                self._load_plan_file()
                self._active_plan.clear()
                self._activation_time.clear()
                self._last_offset_idx.clear()
        except Exception:
            pass

        now      = datetime.now()
        cur_mode = self._op_mode['value']
        active   = cur_mode < 3 and cur_mode in self._active_modes

        # Resolve timetable once — applies to all SCNs.
        plan_name = None
        if active:
            entry = self._resolve_timetable(now)
            if entry and entry.get('plan') is not None:
                plan_name = entry['plan']

        for scn in self._ug405_cfg['scns']:
            if scn not in self._plan_config:
                continue

            scn_plans = self._plan_config[scn].get('plans', {})

            if active and plan_name and plan_name in scn_plans:
                if self._active_plan.get(scn) != plan_name:
                    prev = self._active_plan.get(scn, 'none')
                    log.info("SCN %s: activating plan %s (was %s)", scn, plan_name, prev)
                    self._active_plan[scn]     = plan_name
                    self._activation_time[scn] = time.monotonic()
                    self._last_offset_idx[scn] = -1
                cycle_pos = self._calc_cycle_pos(scn, plan_name, now)
                self._apply_offsets(scn, plan_name, cycle_pos)
            else:
                reason = 'standing down (opMode=%d)' % cur_mode if not active else 'no active plan'
                if self._active_plan.get(scn) is not None:
                    log.info("SCN %s: %s — zeroing outputs", scn, reason)
                    self._zero_scn(scn)
                self._active_plan[scn] = None
                self._last_offset_idx.pop(scn, None)

    # ── Timetable resolution ───────────────────────────────────────────────────

    def _resolve_timetable(self, now):
        """
        Return the active global timetable entry for the current time, or None.

        Finds the entry with the latest 'start' <= current time for the
        current day. plan:null in the returned entry means no plan runs.

        Midnight carryover: if no entry applies yet today (e.g. 00:05 before
        the first 07:00 entry), the previous day's last entry is carried
        forward. Plans therefore run through midnight unless a plan:null
        entry explicitly ends them.
        """
        day  = now.strftime('%a')
        hhmm = now.strftime('%H%M')

        best = None
        for entry in self._timetable:
            if day not in entry.get('days', []):
                continue
            start = entry.get('start', '0000')
            if start <= hhmm:
                if best is None or start > best.get('start', '0000'):
                    best = entry

        if best is not None:
            return best

        # Nothing applies yet today — carry forward the previous day's last entry.
        prev_day = (now - timedelta(days=1)).strftime('%a')
        prev_best = None
        for entry in self._timetable:
            if prev_day not in entry.get('days', []):
                continue
            start = entry.get('start', '0000')
            if prev_best is None or start > prev_best.get('start', '0000'):
                prev_best = entry

        return prev_best

    # ── Cycle position ─────────────────────────────────────────────────────────

    def _calc_cycle_pos(self, scn, plan_name, now):
        cycle_secs = self._plan_config[scn]['plans'][plan_name]['cycle_secs']
        mode       = self._settings.get('base_time_mode', 'none')
        base_time  = self._settings.get('base_time', '')

        if mode == 'datetime':
            try:
                dp, tp   = base_time.strip().split()
                dd, mm_  = int(dp[0:2]), int(dp[2:4])
                yyyy     = int(dp[4:8])
                hh, mi   = int(tp[0:2]), int(tp[2:4])
                base_dt  = datetime(yyyy, mm_, dd, hh, mi, 0)
                elapsed  = (now - base_dt).total_seconds()
                return elapsed % cycle_secs
            except Exception as e:
                log.warning("datetime parse error (%s): %s", base_time, e)
                return 0.0

        if mode == 'time':
            try:
                hh, mi   = int(base_time[0:2]), int(base_time[2:4])
                base_sec = hh * 3600 + mi * 60
                midnight = now.replace(hour=0, minute=0, second=0, microsecond=0)
                now_sec  = (now - midnight).total_seconds()
                elapsed  = now_sec - base_sec if now_sec >= base_sec \
                           else (86400 - base_sec) + now_sec
                return elapsed % cycle_secs
            except Exception as e:
                log.warning("time parse error (%s): %s", base_time, e)
                return 0.0

        # mode == 'none': time since plan activation on this outstation.
        act = self._activation_time.get(scn, time.monotonic())
        return (time.monotonic() - act) % cycle_secs

    # ── Offset application ─────────────────────────────────────────────────────

    def _apply_offsets(self, scn, plan_name, cycle_pos):
        offsets = self._plan_config[scn]['plans'][plan_name]['offsets']
        control = self._ug405_cfg['control'][scn]

        active_idx = -1
        for i, off in enumerate(offsets):
            if off['at'] <= cycle_pos:
                active_idx = i

        if active_idx == self._last_offset_idx.get(scn, -2):
            return

        self._last_offset_idx[scn] = active_idx
        if active_idx < 0:
            return

        off = offsets[active_idx]
        log.info("SCN %s  %s  @%ds (pos=%.1fs)  fn=%s dn=%s",
                 scn, plan_name, off['at'], cycle_pos,
                 hex(off['fn']) if 'fn' in off else '-',
                 hex(off['dn']) if 'dn' in off else '-')

        # Bitmask fields — anti-drop: set new bits first, clear old bits second.
        # source='ug405': these signals are registered as owned by 'ug405'.
        for field in BITMASK_CONTROL:
            key = field.lower()
            if key not in off or field not in control:
                continue
            mask = int(off[key])
            bits = control[field]
            for bit, sig in bits.items():
                if (mask >> (bit - 1)) & 1:
                    self._io.write(sig, 1, source='ug405')
            for bit, sig in bits.items():
                if not ((mask >> (bit - 1)) & 1):
                    self._io.write(sig, 0, source='ug405')

        # Single-bit fields.
        for json_key, field in _SINGLE_FIELDS:
            if json_key not in off or field not in control:
                continue
            sig = list(control[field].values())[0]
            self._io.write(sig, int(off[json_key]), source='ug405')

    def _zero_scn(self, scn):
        for bits in self._ug405_cfg['control'].get(scn, {}).values():
            for sig in bits.values():
                self._io.write(sig, 0, source='ug405')

    # ── Web snapshot ───────────────────────────────────────────────────────────

    def snapshot(self):
        """Return current state for web display. Called from poll thread."""
        now = datetime.now()

        scns_out  = {}
        plans_out = {}

        for scn in self._ug405_cfg['scns']:
            if scn not in self._plan_config:
                continue

            scn_data = self._plan_config[scn]
            control  = self._ug405_cfg['control'].get(scn, {})

            active_plan = self._active_plan.get(scn)
            cycle_pos   = 0.0
            cycle_secs  = 0
            if active_plan and active_plan in scn_data.get('plans', {}):
                cycle_secs = scn_data['plans'][active_plan]['cycle_secs']
                try:
                    cycle_pos = self._calc_cycle_pos(scn, active_plan, now)
                except Exception:
                    pass

            scns_out[scn] = {
                'active_plan': active_plan,
                'cycle_pos':   round(cycle_pos, 1),
                'cycle_secs':  cycle_secs,
                'offset_idx':  self._last_offset_idx.get(scn, -1),
            }

            # Decoded plan definitions with per-bit columns.
            plans_out[scn] = {}
            for plan_name, plan in scn_data.get('plans', {}).items():
                decoded = []
                for off in plan['offsets']:
                    bits = {}
                    fn_mask  = int(off.get('fn',  0))
                    dn_mask  = int(off.get('dn',  0))
                    sfn_mask = int(off.get('sfn', 0))
                    for bit in sorted(control.get('Fn',  {}).keys()):
                        bits[f'Fn[{bit}]']  = (fn_mask  >> (bit - 1)) & 1
                    for bit in sorted(control.get('Dn',  {}).keys()):
                        bits[f'Dn[{bit}]']  = (dn_mask  >> (bit - 1)) & 1
                    for bit in sorted(control.get('SFn', {}).keys()):
                        bits[f'SFn[{bit}]'] = (sfn_mask >> (bit - 1)) & 1
                    for json_key, field in _SINGLE_FIELDS:
                        if field in control:
                            bits[field] = int(off.get(json_key, 0))
                    decoded.append({'at': off['at'], 'bits': bits})
                plans_out[scn][plan_name] = {
                    'cycle_secs': plan['cycle_secs'],
                    'offsets':    decoded,
                }

        return {
            'settings': {
                'base_time_mode': self._settings.get('base_time_mode', 'none'),
                'base_time':      self._settings.get('base_time', ''),
                'active_in_modes': sorted(self._active_modes),
            },
            'op_mode':   self._op_mode['value'],
            'scns':      scns_out,
            'plans':     plans_out,
            'timetable': self._timetable,
        }
