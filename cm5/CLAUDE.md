# CM5 Platform — Claude Code Context

## Working style

- **NO HALLUCINATING** — if you cannot identify the root cause from the code, say so. Do not make repeated blind attempts.
- Never guess syntax, APIs or configs if uncertain — ask for logs or files first
- After modifying Python files, always run `python3 -m py_compile <file>` at minimum
- Use `source /opt/ug405-env/bin/activate` for pip installs — CM5 runs under this venv (`/opt/ug405-env/bin/python`)
- Do not add unprompted refactors or features — scope changes to what was asked
- If a request conflicts with safety-critical behaviour documented below, warn first
- No new third-party packages without explicit approval
- Always check logs before assuming something works — poll loop errors are swallowed at DEBUG level

---

## What this system is

CM5 is a Python-based **UTMC UTC Type 2 SNMP outstation** for real roadside traffic infrastructure running on a Proxmox LXC at `192.168.71.22`. It is **not** generic SNMP middleware. Priorities in order: deterministic behaviour, operational correctness, safety, maintainability.

It provides:
- SNMP GET/SET/INFORM (UTC Type 2 control/reply tables)
- RBE (Reply By Exception) INFORM generation
- SCOOT VSn occupancy packing
- Watchdog-based fail-safe
- opMode authority handling
- IO abstraction layer (XKOP, RPDB, FLIR, AGD650, RTIG, conditioner)
- Web dashboard (Flask/SSE, port 12007)

---

## Key files

| File | Purpose |
|------|---------|
| `main.py` | Platform launcher — loads configs, wires IOBus, starts all services |
| `core/io_bus.py` | Typed signal bus, owner-per-signal, namespace-based addressing |
| `core/config.py` | Config parsers for all .cfg files; `sig_raw()`, `sig_read()` helpers |
| `cm5_web.py` | Unified Flask/SSE dashboard — all tabs, SSE stream, conditioner API |
| `ug405/svc_ug405.py` | SNMP outstation service (pysnmp asyncore) |
| `ug405/svc_ug405_rbe.py` | RBE INFORM sender (hand-built BER, source-agnostic) |
| `ug405/svc_ug405_scoot.py` | SCOOT VSn sampler |
| `ug405/ug405_web.py` | Old per-service web — superseded by cm5_web.py but `sig_to_id` still imported |
| `io/io_xkop.py` | XKOP TCP driver (primary TLC connection) |
| `io/io_rpdb.py` | RPDB driver (Peek PTC-1 / EC-2) |
| `conditioner/io_conditioner.py` | IO logic/conditioning engine |
| `flir/svc_flir.py` | FLIR thermal camera WebSocket client (multi-camera) |
| `flir/flir_mock_server.py` | Mock FLIR server — `python flir_mock_server.py --port 8765` |
| `agd/svc_agd.py` | AGD650 radar detector ZeroMQ subscriber (multi-unit) |
| `agd/agd.py` | AGD650 simulator — `python agd/agd.py --port 9003 --zones 8` |
| `platform.cfg` | Top-level service enables, XKOP connection, logging config |
| `ug405/ug405.cfg` | SCN signal mappings, opMode settings, persisted live config |
| `io/rpdb.cfg` | RPDB host, subscriptions, output mappings |
| `flir/flir.cfg` | FLIR camera definitions and virt signal mappings |
| `agd/agd.cfg` | AGD650 unit definitions and virt signal mappings |
| `autodim/svc_autodim.py` | Astronomical dim/bright controller — astral sunrise/sunset + offsets |
| `autodim/autodim.cfg` | Location (lat/lon), offset minutes, output signal |
| `offline_plan/svc_offline_plan.py` | Fixed-time plan runner — drives Fn/Dn from timetable when opMode < 3 |
| `offline_plan/offline_plan.json` | Plan definitions, timetable, and settings |
| `ug405_observations_for_claude.md` | **Read this before making architectural changes** |

---

## Current site config

**SCN X0330** — SCOOT detector occupancy via `xkop.i.*`
**SCN J0331** — TLC stage/green reply (`xkop.i.*`) + control (`xkop.o.*`)

Instation: `192.168.71.101:1162`
XKOP TLC: `192.168.71.101:8001` (client mode)
Web dashboard: `http://192.168.71.22:12007/`

RPDB is configured (`io/rpdb.cfg`) but currently disabled — Peek PTC-1 integration, showcase/testing only.

---

## IO bus signal namespace

Signals follow `driver.direction.index` naming. Key namespaces:
- `xkop.i.*` / `xkop.o.*` — XKOP TLC inputs/outputs
- `rpdb.i.xsg.*`, `rpdb.i.xout.*`, `rpdb.i.xdet.*` — RPDB inputs
- `rpdb.o.*` — RPDB outputs (written by UG405 Fn bits)
- `virt.0-299` — conditioner virtual signals
- `virt.300-399` — FLIR camera signals
- `virt.400-499` — AGD650 radar signals

Signal owner is derived from the namespace prefix: `xkop` → `xkop_driver`, `rpdb` → `rpdb_driver` etc.
`virt.*` is owned by `conditioner`, `flir_driver`, or `agd_driver` depending on range.

Each signal has exactly **one declared owner**. Conflict at startup = hard error.

**Critical**: never pass an empty string to `io.read()` — `resolve('')` raises `ValueError` which the poll loop swallows silently. Always guard: `io.read(sig) if sig else 0`.

---

## Established patterns — do not break

### Signal source abstraction
The UG405/RBE layer must **never** filter by namespace prefix (`xkop.i.*`, `rpdb.i.*` etc).
Use `self._reply_signals` (set built from config at startup) for all change detection.

### Inversion in ug405.cfg
`!=` operator inverts a reply signal's value at the SNMP layer:
```ini
utcReplyGn[1] != xkop.i.12   # SNMP bit = NOT bus value
```
Use `sig_raw(sig)` for the bare bus name, `sig_read(io, sig)` to read with inversion.
Both importable from `config.py`. Web files define local `_sig_raw()`.

### Anti-drop control sequencing
When writing Fn control bits: **set new bits first, clear old bits second**.
Never simplify to clear-all then set-new — causes real-world intergreen glitches.

### opMode semantics (safety-critical)
- Mode 1 = standalone/fail-safe, Mode 2 = monitor+RBE, Mode 3 = UTC control
- Illegal jumps rejected (can't skip a mode up)
- On mode drop: `io.zero_owned_by('ug405')` clears all outputs — intentional
- `_drop_to_standalone()` is the single exit path — do not bypass

### IO fault → standalone (delayed)
Drivers call `on_fault(reason)` on disconnect → `_schedule_standalone()` starts a timer (default 30s).
Drivers call `on_reconnect()` on reconnect → `_cancel_fault()` cancels the timer.
Fault callbacks are **only wired for drivers whose signals appear in the UG405 mapping**.
Timeout configurable in `platform.cfg` as `io_fault_timeout = 30`.

### XKOP reconnect behaviour
On reconnect: `io.zero_owned_by('xkop_driver')` clears all inputs (prevents stale state if TLC restarted).
Receive-side health check: if nothing received for `3 × KEEP_ALIVE_INTERVAL` (18s), connection treated as dead.

**Backoff**: connect failures use exponential backoff — 2s → 4s → 8s → … → 300s cap. Sleep is interruptible
via `_retry_event` so `resume()` takes effect immediately. Only the first failure logs at WARNING; subsequent
retries are DEBUG. Backoff resets to 2s on successful connect.

### RPDB reconnect behaviour
On every (re)connect, `_initial_fetch()` does a one-shot `GET_VALUE` for all subscriptions immediately.
If `_input_signals` is empty at connect time (startup while controller was down), re-runs registration first.

**Backoff**: same exponential backoff pattern as XKOP (starts at `RECONNECT_DELAY` = 5s, caps at 300s).
Call `driver.resume()` to reset backoff and retry immediately — wired to the web UI Reconnect button.

### RBE startup and gating
RBE threads start at boot only if `ReplyByException = 1` is persisted in `ug405.cfg`. All hot paths
(`_on_bus_change`, `push_scoot`, `_send_loop`, `_keepalive_loop`) gate on `op_mode['value'] < 2` and
suppress everything in mode 1 — so the threads are dormant until the instation raises opMode.

The server sends all SNMP config values (including `ReplyByException`) during initialisation while still
in opMode 1, before raising the mode. This means:
- The persisted `ReplyByException = 1` at boot is harmless — server overwrites it before opMode moves
- There is no `_stop_rbe()` path (SET=0 doesn't stop running threads), but this is safe because the
  server only sets it to 0 during init while opMode is still 1, so the loops never fire in that state

Do not add a `ReplyByException` check inside the RBE loops — the opMode gate is the correct control.

### RBE INFORM ACK validation
`_udp_send_recv` validates source IP and request-id. `_build_inform_packet(varbinds, request_id)` takes
an explicit `request_id` — caller generates via `_next_request_id()` before calling.

### SCOOT packing
Fixed nibble packing: byte 0 = D2 high | D1 low, byte 1 = D4 high | D3 low.
Fixed-length payloads even when occupancy is zero. Do not make variable-length.

---

## Simulator management

`/opt/CM5/sims.sh` starts/stops all demo simulators (2× FLIR mock, 2× AGD650).

```bash
./sims.sh start    # start all 4 simulators
./sims.sh stop     # stop all (also kills orphans by port)
./sims.sh restart  # bounce all
./sims.sh status   # show running state
```

PID files and per-simulator logs in `/tmp/cm5-sims/` (cleared on reboot).
Simulators must be running before CM5 starts, or CM5 will log fault timeouts until they come up.

---

## FLIR camera integration

Multi-camera WebSocket client. Each `[CAMERA.N]` section in `flir/flir.cfg` has `ip`, `port`, `zones`.

Zone signal lookup — unit-qualified key takes priority over global:
1. `camera.n.zone.Z.type = virt.X` — required when zone numbers overlap across cameras
2. `zone.Z.type = virt.X` — backward compat when zone numbers are globally unique

`virt.300-351` range. Mock: `python flir/flir_mock_server.py --port 8765`. Can run remotely — update `ip` in cfg.

---

## AGD650 radar integration

Multi-unit ZeroMQ SUB client. Each `[AGD.N]` section in `agd/agd.cfg` has `ip`, `port`, `zones`.
Uses `pyzmq` (installed in venv). Protocol: full zone-state frame every ~150ms (state 1=detect, 2=clear).

Key difference from FLIR: **frame-based not event-based** — driver does change detection internally,
only writes to bus when zone state changes. Fault detection via frame timeout (`frame_timeout` in `[SETTINGS]`).

Zone signal lookup — same unit-qualified pattern as FLIR:
1. `agd.n.zone.Z.type = virt.X` — required when zone numbers overlap (both units use zones 1-8)
2. `zone.Z.type = virt.X` — fallback

`virt.400-493` range. Simulator: `python agd/agd.py --port 9003 --zones 8` (run with venv Python).
Classes configurable in `[CLASSES]` section — add entries when AGD confirms full class list.

---

## Conditioner

Rules in `conditioner/conditioner.cfg` `[IO]` section. Output signals (left side) are
registered automatically. Hot-reload via Conditioner web tab or `/api/conditioner` POST.

### Function reference

| Function | Syntax | Behaviour |
|----------|--------|-----------|
| `ON_DELAY` | `ON_DELAY(sig, s)` | Output ON after input has been ON for s seconds |
| `OFF_DELAY` | `OFF_DELAY(sig, s)` | Output stays ON for s seconds after input drops |
| `PULSE` | `PULSE(sig, s)` | Fixed pulse of s seconds on rising edge (`ONE_SHOT` is alias) |
| `WATCHDOG` | `WATCHDOG(sig, s)` | Output ON if input stuck ON > s seconds |
| `MISSING` | `MISSING(sig, s)` | Output ON if input absent > s seconds |
| `RISING_EDGE` | `RISING_EDGE(sig)` | One-tick pulse on 0→1 |
| `FALLING_EDGE` | `FALLING_EDGE(sig)` | One-tick pulse on 1→0 |
| `LATCH` | `LATCH(set=sig, reset=sig)` | SR latch, reset dominant |
| `TOGGLE` | `TOGGLE(sig)` | Flip state on each rising edge |
| `COUNT` | `COUNT(sig, n=N, reset=sig)` | Output fires after N rising edges |
| `COUNT_UP` | `COUNT_UP(sig, reset=sig)` | Increment integer register on rising edge (`ACCUMULATE` is alias) |
| `COUNT_DOWN` | `COUNT_DOWN(sig, start=N, reset=sig)` | Decrement register on rising edge, floors at 0 |
| `N1COUNT` | `N1COUNT(a=sig, b=sig, c=sig)` | 3-loop vehicle counter |
| `FACTOR` | `FACTOR(virt.N, bit=N)` | Extract bit N from integer virt |
| `SPEED` | `SPEED(det1=sig, det2=sig, distance_m=N)` | Speed in km/h from two loop detectors |

### Bidirectional counter
`COUNT_UP` and `COUNT_DOWN` can share one output register via `OR` on a single rule:
```ini
virt.5 = COUNT_UP(xkop.i.5) OR COUNT_DOWN(xkop.i.6)
```
Both read the current value from the output signal and write the new value. Counter floors at 0.
Multiple UP and DOWN terms allowed. This is the correct pattern — do **not** create two separate
rules targeting the same output (the second registration would be rejected as a conflict).

### Nesting
Inner function calls are resolved automatically to `_cond.N` internal bus signals:
```ini
virt.5 = PULSE(FALLING_EDGE(xkop.i.12), 5.0)
virt.6 = COUNT_UP(RISING_EDGE(xkop.i.5)) OR COUNT_DOWN(FALLING_EDGE(xkop.i.6))
```
No intermediate `virt.*` needed. Inner `_cond.N` signals are registered on the bus (visible
in IO Bus tab) but hidden from the Conditioner tab snapshot. They are NOT user-configurable —
they are synthesised by the parser. `_inner_idx` is a module-level counter; it increments on
each hot-reload. Hot-reload now unregisters all conditioner-owned signals not referenced by
the new block set (including stale `_cond.N` entries), so orphans are cleaned up on reload.

---

## DIM Service integration

Single-output astronomical controller. Uses `astral` (installed in venv) to compute today's
sunrise/sunset from a configured lat/lon, applies ± minute offsets, and writes `1` (dim) or
`0` (bright) to a single IO bus output signal every 30 s.

Config in `autodim/autodim.cfg`:
```ini
[LOCATION]
lat = 53.71     # decimal degrees
lon = -1.92

[SCHEDULE]
dim_offset    = +20   # minutes after sunset to go dim
bright_offset = -30   # minutes before sunrise to go bright (negative = before)

[OUTPUT]
signal = xkop.o.15    # must be a free output bit not owned by another driver
enabled = true
```

**Output signal**: assign directly to a free physical output (`xkop.o.##`, `rpdb.o.##` etc).
Do **not** use an intermediate virtual + conditioner route. The signal must not already be
registered by UG405 or any other driver — conflict at startup logs a WARNING and the signal
is not written. Check `ug405.cfg` for which `xkop.o.*` bits are already taken.

**Hot-reload**: lat/lon, offsets, enabled, and output signal all hot-reload via the web UI
(`/api/autodim` POST) without restarting CM5. Signal hot-swap registers the new signal and
unregisters the old one; if the new signal is already owned, a WARNING is logged and the old
signal is kept.

**Schedule recalc**: a `_force_recalc` flag + 1 s interruptible sleep ensures the new schedule
is applied within ~1 s of a web save — do not change this to a plain `time.sleep(30)`.

**No enable toggle in the web UI** — service is enabled/disabled in `platform.cfg`.

---

## Offline fixed-time plan

`offline_plan/svc_offline_plan.py` drives Fn/Dn (and other UTC control) outputs from a JSON timetable
when the outstation is in standalone or monitor mode. Stands down immediately when opMode reaches 3.

Enable in `platform.cfg`: `offline_plan = enabled`. Disabled by default.

### File structure — `offline_plan/offline_plan.json`

```json
{
  "settings": {
    "base_time_mode": "datetime" | "time" | "none",
    "base_time": "ddmmyyyy hhmm"   // or "hhmm" for mode=time
    "active_in_modes": [1, 2]      // opMode values where plans run; default [1,2]
  },
  "scns": {
    "J0331": {
      "plans": {
        "AM_PEAK": { "cycle_secs": 60, "offsets": [...] }
      },
      "timetable": [
        { "days": ["Mon","Tue",...], "start": "0700", "plan": "AM_PEAK" },
        { "days": ["Mon","Tue",...], "start": "0900", "plan": null }
      ],
      "special_dates": [
        { "date": "25122025", "plan": null }
      ]
    }
  }
}
```

Each SCN is evaluated independently. SCNs in the JSON but absent from `ug405.cfg` are silently skipped.

### Timetable evaluation
No `end` field. Find the entry with the latest `start` ≤ current time for the current day.
`plan: null` means no plan runs from that time — use it to end a plan period. Special dates override the timetable.

### Offset entries
```json
{ "at": 21, "fn": "0x06", "dn": "0x00", "go": 1 }
```
- `fn`, `dn`, `sfn` — bitmask: hex string `"0x06"` or integer `6` (bit N has value 2^(N-1))
- Single-bit fields (`go`, `mo`, `ts`, `so`, `fm`, `dx` etc.) — integer `0` or `1` only, no hex
- Fields absent from `ug405.cfg` for that SCN are silently ignored
- Anti-drop sequencing: set new Fn/Dn bits first, clear old bits second (mirrors `svc_ug405.py`)
- All writes use `source='ug405'` — signals are registered as owned by `'ug405'`

### Base time modes
| Mode | Behaviour |
|------|-----------|
| `datetime` | Absolute reference `ddmmyyyy hhmm`. Fully continuous — preferred for non-integer cycles |
| `time` | Time-of-day anchor `hhmm`. Phase resets daily at that time — visible jump for non-integer cycles |
| `none` | Elapsed since plan activation on this outstation. No external reference |

### Active modes
`active_in_modes: [1, 2]` — run in both standalone and monitor (default).
`[1]` — standalone only. `[2]` — monitor only. Mode 3 always stands down regardless.
The UG405 opMode label on the web dashboard shows `[Offline Plan Running]` when a plan is active.

### Hot-reload
File mtime is checked every tick. Edit and save `offline_plan.json` — reloads within 1 second.
Parse errors keep the last good config and log at ERROR. Restart not required.

---

## Platform config reference

`platform.cfg` sections:
- `[SERVICES]` — `io`, `xkop`, `ug405`, `rtig`, `conditioner`, `rpdb`, `mova`, `flir`, `agd`, `autodim`, `offline_plan`, `io_fault_timeout`, `memory_limit_mb`
- `[XKOP]` — `ip`, `port`, `mode` (replaces old `io/io.cfg` — that file no longer exists)
- `[LOGGING]` — `level`, `log_max_mb`, `log_keep`

All services default to disabled except `io`, `xkop`, `ug405`.

`memory_limit_mb` (default 300): memory watchdog in `main.py` checks process RSS every 30s via
`/proc/self/status`. If RSS ≥ limit it logs CRITICAL and sends SIGTERM — protects the LXC from
unbounded leaks. The watchdog remains as a hard ceiling even though the underlying SSE leak is fixed.

---

## Web dashboard tabs

`cm5_web.py` serves a single-page app with these tabs (all via one SSE stream):
- **UG405** — SCN control/reply tables, opMode, instation. Bottom of tab has three SNMP activity sections:
  - **Instation Config** — persistent field/value table updated in place on each Config SET; shows last-set timestamp per field (InstationAddress, ports, RBE params, timeouts)
  - **opMode Transitions** — fixed rows for all 6 transitions (1→2, 2→3, 3→3, 3→2, 3→1, 2→1), colour-coded, shows last-seen time and source reason: `(instation SET)` for explicit SET, `(opMode timeout)` for watchdog expiry, `(IO fault)` for driver disconnect. Natural timeout drops captured via `_drop_to_standalone()` which emits a set_log_entry — not just instation-driven drops
  - **Control Activity** — rolling 30-entry log of Control SETs (Fn, Dn, GO etc.) with SCN, field, value; multiple varbinds in one PDU appear as separate rows with the same timestamp
  - All three sections populated in real time via SSE push on each SET; also logged at INFO in `cm5.log` for audit. Community string: `UTMC`. Walk with: `snmpwalk -v2c -c UTMC 127.0.0.1 .1.3.6.1`
- **IO Bus** — all registered signals, filterable; includes **Drivers table** (connection status, retry backoff, Reconnect button per driver)
- **Conditioner** — live rule state, inline rule editor (hot-reload via `/api/conditioner`)
- **RTIG / TLP** — TLP rules, recent TLP log
- **FLIR** — per-camera zone matrix, global any_* bits, event log
- **AGD650** — per-unit zone matrix, global any_* bits, detection log
- **DIM Service** — status cards, Nominatim location search, offset config, output signal (`/api/autodim`)
- **Fixed-Time Plan** — per-SCN status (active plan, cycle progress bar, active offset with decoded Fn[N] bits), timetable, plan offset tables
- **MOVA** — external link to MOVA web UI

**FLIR tab zone mapping**: Zone Signal Mapping table appears before Recent Events. Each zone row has one output signal (virt.X input) and checkboxes for which detection types (Occupied, Dilemma, Pedestrian, Bicycle, Vehicle) drive that output. All ticked types write to the same output signal. Routes: `GET/POST /api/flir_mapping`. Writer: `_write_flir_mapping()` rewrites `[VIRT_MAPPING]`, preserving `[CAMERA.N]` sections.

**AGD tab zone mapping**: Detection Classes table (class name → signal suffix — must match strings the real AGD650 firmware sends) + per-unit zone mapping table with checkbox columns per class. "Apply Classes →" rebuilds zone table columns without losing entered values. Routes: `GET/POST /api/agd_mapping`. Writer: `_write_agd_mapping()` rewrites `[CLASSES]` and `[VIRT_MAPPING]`, preserving `[AGD.N]` and `[SETTINGS]` sections. Class names in current config match simulator strings — update left column to match real AGD firmware when confirmed.

**Topbar**: service status pills + **MEM pill** showing live RSS / limit in MB, colour-coded green (<50%) / amber (50–80%) / red (>80%), updated every second via SSE.

**Administration** nav group (static — not SSE-driven):
- **Configuration** — form editor for `platform.cfg` (service enable/disable toggles including UG405, XKOP/RPDB connection, platform limits, logging) and RPDB connection fields in `io/rpdb.cfg`. Also table editors for FLIR camera IP/port/zones and AGD unit IP/port/zones. Save writes the files immediately; a separate Restart CM5 button calls `POST /api/restart` which runs `systemctl restart cm5` after a 0.5 s delay so the HTTP response returns first. The browser detects reconnect via SSE `readyState` polling. Routes: `GET/POST /api/platform_cfg`, `POST /api/restart`, `GET /api/sensors_cfg`, `POST /api/flir_cfg`, `POST /api/agd_cfg`. `_write_platform_cfg()` rewrites `platform.cfg` entirely (comments not preserved). `_write_rpdb_connection()` updates only the `[RPDB]` header fields, preserving SUBSCRIBE/NAMES/OUTPUTS sections. `_write_flir_cameras()` and `_write_agd_units()` rewrite their respective `[CAMERA.N]`/`[AGD.N]` sections, preserving VIRT_MAPPING and other sections.
- **Logs** — file picker for all `cm5.log*` files, last 2000 lines, client-side level filter (DEBUG/INFO/WARN/ERROR). Routes: `GET /api/logs` (file list), `GET /api/logs/<filename>?lines=N` (contents). Security: only `cm5.log*` filenames accepted, no path traversal. **Read-only** — no purge/delete UI; logs roll automatically via `RotatingFileHandler`.

**Driver reconnect API**: `POST /api/driver/<name>/reconnect` — calls `driver.resume()` on `xkop` or `rpdb`. Returns `{"ok": true}` or `{"ok": false, "error": "..."}`.

**SSE subscriber management**: each `/stream` client gets a `deque(maxlen=200)` queue. The `generate()` function removes it from `_subscribers` in a `try/finally` on disconnect — do not change this to a plain list or remove the finally block, as leaked queues accumulate memory at 1 push/second indefinitely.

**SSE disconnect detection**: browser refresh sends TCP FIN, moving the socket to CLOSE-WAIT. Writes to CLOSE-WAIT sockets succeed indefinitely (client ACKs in FIN_WAIT_2), so relying on write failure to detect disconnect doesn't work — threads accumulate at ~1 MB RSS each. Fix: `stream()` captures `wsgi.input.fileno()` at request time and the generator calls `select([fd], [], [], 0)` (non-blocking) each iteration. A readable socket signals FIN/EOF; the generator returns and the `finally` block removes the queue. Do not remove this select check or revert to relying on broken-pipe detection.

**Page template**: `_PAGE_TEMPLATE = app.jinja_env.from_string(HTML)` is compiled once at startup. Route handlers call `_PAGE_TEMPLATE.render(mapping_json=...)` directly — do not switch back to `render_template_string(HTML, ...)` as that recompiles the 52 KB template on every page load.

**UG405 pill** is conditional on `_op_mode is not None` — shows "off" when UG405 is disabled. Web server starts independently of UG405; if UG405 is disabled, `svc` is None and the dashboard runs in IO-only mode (IO Bus, FLIR, AGD, Conditioner, Configuration tabs still work). UG405 is toggleable in the Configuration tab (not locked "always on").

Adding a new SSE tab: add nav item + panel HTML, a `_xxx_snapshot()` function, wire into `_build_full_state()` and `_poll_loop()`, add driver reference to `start_web()`.
Adding a new Administration tab: add nav item + panel HTML + fetch-based JS only — no SSE wiring needed.

---

## Pending work

- **VPN / real server connection** — colleague preparing WireGuard cert. LXC 107 on this Proxmox host is the VPN termination point. CM5 LXC routes instation traffic via LXC 107. Once up, set `InstationAddress` in `ug405.cfg [LIVE]` to the server's VPN tunnel IP.
- **AGD650 class names** — `[CLASSES]` section in `agd/agd.cfg` uses simulator names (`car`, `pedestrian` etc). Update with real AGD class strings when confirmed.
- **Remote demand app** — standalone Flask app (separate process from cm5_web.py) for phone-based junction demands via browser. SQLite DB: users, actions (label + IOBus signal), user_actions (permissions), events (audit trail). Mobile-first UI — renders only buttons the user is permitted. Signal mapping in DB not config. ntfy was evaluated and abandoned (receive-only app, no usable send UX).
- **Recorded plan replay** — capture every instation Fn/Dn SET (tagged dow + tod_secs) into SQLite during live operation. After a full week, replay the recorded pattern locally when opMode < 3 instead of hand-crafted fixed plans. Hook point: `setValue()` in `svc_ug405.py`. Storage: `recordings(dow, tod_secs, scn, field, value)`. Replay sits alongside fixed plan — fixed plan is fallback when recording coverage is insufficient. Key design questions: minimum coverage % before a day's pattern is trusted; gap handling (zero, fixed plan fallback, or nearest neighbour); rolling week-on-week overwrite to stay current. **Revisit once live instation connection is established and real traffic data is flowing.**
- **NTP / clock quality on real hardware** — clock jitter detection (`clock_jitter_check` in `platform.cfg`) is currently disabled; relevant once CM5 runs on dedicated hardware with chrony NTP. When enabled, the watchdog drops to standalone if system clock jumps > 10s (`CLOCK_JITTER_ALLOWED`) between 5s ticks — protects against LXC inheriting a large NTP step-correction that would cause timestamp validation to reject all incoming SETs silently. On real hardware, review: (1) enable `clock_jitter_check = enabled`; (2) tune `CLOCK_JITTER_ALLOWED` — chrony makes small frequent adjustments so 10s is conservative; (3) consider adding NTP status to the dashboard (chrony sync state, stratum, reference, estimated offset) as a card or pill; (4) `clock_jitter_grace` (default 60s) suppresses checking after opMode 1→2 to allow instation initialisation to settle.
- **SNMP agent subprocess isolation** — pysnmp 4.4.x asyncore can leak memory under sustained GET polling. However, the UTC Type 2 instation pattern is: walk once on connect (short burst of GETs), then switch to RBE-driven operation (CM5 pushes INFORMs; instation sends control SETs only). The asyncore loop is therefore mostly idle in steady state — the assumed continuous-polling leak driver does not apply. Memory growth observed so far (19/05/2026) is from browser/Flask warmup only, not SNMP. **Monitor RSS after live instation connects; implement subprocess isolation only if RSS shows sustained growth.** If implemented: SNMP is UDP so subprocess restarts are invisible (one missed poll); XKOP stays in the main process, TLC sees nothing. IPC via `multiprocessing.Queue` for SET values → main process. `gc.collect()` in the memory watchdog (added 18/05/2026) is the interim mitigation.

---

## Known tech debt (do not accidentally fix without understanding)

- **MOVA kernel crashes CM5 (SEGV hazard)** — `libmova_0.so` runs as a C kernel inside the CM5 process on a 10Hz tick loop. Dataset add/delete via the MOVA dataset manager (port 8080) touches memory the C kernel is actively reading — causes SIGSEGV which kills the entire CM5 process. Observed 19/05/2026: 6 rapid crash/restart cycles during dataset operations, then a further crash 2.5 hours later. **MOVA is disabled in `platform.cfg` until subprocess isolation is implemented.** Fix: run the MOVA kernel in a managed subprocess (same pattern as planned SNMP agent isolation) so a kernel SEGV cannot take down XKOP, RBE, or the offline plan service.
- pysnmp asyncore dependency (long-term: migrate, not urgent)
- Distributed mutable state between threads (locking is minimal but functional)
- Raw BER encode path in svc_ug405_rbe.py (intentional — see observations doc)
- Shutdown lifecycle uses daemon threads (no coordinated stop events yet)
- `load_io` function remains in `config.py` but is unused — `io.cfg` was deleted
- `ug405/ug405.cfg.RPDB` — untracked backup file, gitignored, not needed
- New cfg loaders must use `configparser.ConfigParser(inline_comment_prefixes=('#',))` — default ConfigParser includes inline `#` comments as part of values, causing silent parse failures

**Fixed-Time Plan editor — `.cfg-section[data-scn]` selector collision (FIXED 18/05/2026)**:
Both the offplan editor (`#op-plan-editor`) and the SCN Config tab (`#scncfg-container`) render
`<div class="cfg-section" data-scn="...">` elements into the same DOM. The unscoped
`document.querySelectorAll('.cfg-section[data-scn]')` in `_readAllOffplanData()` picked up SCN Config
blocks (which have no `.op-plan-block` children) and wrote empty plan objects, wiping the JSON.
Fix: scope `_readAllOffplanData()` to `#op-plan-editor`, and scope `_readSCNData()` / `_checkSCNConflicts()`
to `#scncfg-container`. Always scope tab-specific `querySelectorAll` to the tab's own container element. 
