# PCI Platform — Claude Code Context

## IMPORTANT: Working rules

- **DO NOT HALLUCINATE.** If you do not know something or need more context — **ask**.
- **DO NOT make changes without understanding why.** Read source before editing.
- **Ask before speculative fixes** — especially kernel C code where a wrong change crashes.
- After modifying Python files, always run `python3 -m py_compile <file>` at minimum.
- Do not add unprompted refactors or features — scope to what was asked.
- **NEVER work in /opt/MOVA or /opt/CM5.** All work happens in /opt/MC_MOVA.
  - Kernel C changes: `M8.0/` → `make -f Makefile.linux` deploys to `pci_mova/core/kernel/libmova.so`
  - CM5 changes: `cm5/` only
  - MOVA Python changes: `pci_mova/` only

---

## What this is

`/opt/MC_MOVA` is the **unified PCI platform** — a merger of the MOVA adaptive controller
(`/opt/MOVA`) and the CM5 UTC outstation (`/opt/CM5`) into a single, clean architecture
where every function runs as an independent process with a shared IOBus backbone.

### Context — why this exists

Both MOVA and CM5 reached the same architectural endpoint independently:
- CM5 is a Python UTC/SNMP outstation managing XKOP/RPDB TLC hardware
- MOVA is a Python wrapper hosting the TRL MOVA M8 adaptive kernel (C library)
- Both were monoliths; both developed GIL/memory problems under load
- Both needed to be split into per-function processes

This repo implements that split cleanly from the start, rather than patching both legacy repos.

### Key differentiators (nobody else on market provides these)
- Adaptive control + UTC + RTIG + dimming + detection in one crash-isolated platform
- Every service independently restartable; a SIGSEGV in the kernel never touches UG405
- Full live data visibility and historical analysis built in
- Extended MOVA outputs: endsat, OSAT, EP, EM/PR as real physical outputs
- MOVA Tools protocol (AML) implemented natively

---

## Platform: Proxmox LXC `192.168.71.22`

| Service | Port / socket |
|---------|--------------|
| Web UI | `http://192.168.71.22:8080` |
| MOVA Tools (stream N) | TCP `6000+N` |
| IOBus | `/tmp/pci_iobus.sock` |
| Kernel push (stream N) | `/tmp/mova_{N}_live.sock` |
| Kernel cmd (stream N) | `/tmp/mova_{N}_cmd.sock` |
| CM5 (legacy) | `http://192.168.71.22:12007` |
| XKOP TLC | `192.168.71.101:8001` |
| SNMP instation | `192.168.71.101:1162` |

---

## Target architecture

```
Physical hardware (XKOP/RPDB loops, GPIO, stage signals)
         │
         ▼  CM5 owns TLC hardware connection
pci-iobus  /tmp/pci_iobus.sock  ← signal bus backbone
         │
         ├─── pci-mova-kernel@0   ← stream 0 tick loop + MOVA Tools :6000
         ├─── pci-mova-kernel@1   ← stream 1 tick loop + MOVA Tools :6001
         │         ...
         ├─── pci-ug405           ← SNMP UTC outstation
         ├─── pci-rtig            ← RTIG real-time feed
         ├─── pci-autodim         ← astronomical dim/bright controller
         ├─── pci-conditioner     ← IO logic/conditioning rules
         ├─── pci-offline-plan    ← fixed-time plan fallback
         ├─── pci-flir            ← FLIR thermal camera
         └─── pci-agd             ← AGD650 radar detection

pci-web  :8080  ← single web UI aggregates all service sockets
```

Each kernel process is fully isolated — GIL is never shared between kernel and web.
Heavy HTTP/log operations in web cannot stave the 10Hz tick loops.

---

## Confirmed architecture decisions

### 1. Main thread = tick loop

In `kernel_main.py`, the **main thread owns the tick loop** (`MovaStream._loop()`).
The IPC socket server (accept/dispatch) runs on background threads.

Reason: `signal.signal()` only fires on the main thread. With IPC on main, SIGTERM
lands while blocked on `accept()`, delaying shutdown. Tick on main → SIGTERM → clean
controlled stop directly on the control loop.

### 2. Hybrid snapshot/event model on the push socket

Push socket publishes at two rates:
- **Snapshot:** 1–2 Hz — full state, for web to update `latest_snapshot`
- **Immediate events:** on state change, zero delay — not held for next snapshot

```json
{"v":1,"t":"phase_change","ts":1234.5,"from":2,"to":4}
{"v":1,"t":"fault","ts":1234.5,"error_id":5,"data":0}
{"v":1,"t":"on_control","ts":1234.5,"value":1}
{"v":1,"t":"mova_on","ts":1234.5,"value":1}
{"v":1,"t":"stage_forced","ts":1234.5,"stage":3}
{"v":1,"t":"plan_switched","ts":1234.5,"plan":2}
```

Browsers see responsive behaviour on meaningful changes without 10 full snapshots/sec.

### 3. Version all IPC messages

Every IPC message carries `"v":1` for forward compatibility:
```json
{"v":1,"t":"snap","ts":1234.5, ...}
{"v":1,"t":"msg","ts":1234.5, ...}
{"v":1,"t":"pong"}
{"v":1,"t":"ack","cmd":"LOAD","ok":true}
```

Allows format changes to roll out across running kernel processes without breaking
web processes. Cheap to add now, painful to retrofit later.

### 4. One build, N runtime instances

Single codebase launched N times with a stream index:
```
python -m pci_mova.kernel_main 0
python -m pci_mova.kernel_main 1
```

Per-instance (derived from stream index N at runtime):
- `/tmp/mova_{N}_live.sock` and `/tmp/mova_{N}_cmd.sock`
- Port `6000+N` for MOVA Tools
- `stream_state_{N}.json`
- `mova_stream_{N}_YYYY-MM-DD.jsonl`
- One `MovaStream` object in memory

Shared (read-only at runtime): Python source, `libmova.so`, licence file, dataset dir.

Licence validation: if stream N not licensed → exit cleanly (code 0).
`Restart=on-failure` in systemd does not loop on clean exit.

---

## IPC socket protocol

**Push socket** `/tmp/mova_{N}_live.sock` — kernel listens, web connects and reads.
JSON lines at two rates (see decision 2 above).

**Command socket** `/tmp/mova_{N}_cmd.sock` — web connects per command, one req/ack.
```
PING                           → {"v":1,"t":"pong"}
LOAD <path> <stream_id>        → {"v":1,"t":"ack","cmd":"LOAD","ok":true}
UNLOAD                         → {"v":1,"t":"ack","cmd":"UNLOAD","ok":true}
SET_IO <index> <value>         → {"v":1,"t":"ack","cmd":"SET_IO","ok":true}
SET_DET <index> <value>        → {"v":1,"t":"ack","cmd":"SET_DET","ok":true}
SET_CONFIRM <index> <value>    → {"v":1,"t":"ack","cmd":"SET_CONFIRM","ok":true}
SET_CRB <value>                → {"v":1,"t":"ack","cmd":"SET_CRB","ok":true}
FORCE_STAGE <stage>            → {"v":1,"t":"ack","cmd":"FORCE_STAGE","ok":true}
SWITCH_PLAN <plan_num>         → {"v":1,"t":"ack","cmd":"SWITCH_PLAN","ok":true}
CONNECT_XKOP <host> <port>     → {"v":1,"t":"ack","cmd":"CONNECT_XKOP","ok":true}
DISCONNECT_XKOP                → {"v":1,"t":"ack","cmd":"DISCONNECT_XKOP","ok":true}
SET_SPEED <multiplier>         → {"v":1,"t":"ack","cmd":"SET_SPEED","ok":true}
SET_TOD_OFFSET <hours>         → {"v":1,"t":"ack","cmd":"SET_TOD_OFFSET","ok":true}
RESET                          → {"v":1,"t":"ack","cmd":"RESET","ok":true}
```

No framing, no CRC. Unix domain socket is reliable and local.

---

## Directory structure

```
/opt/MC_MOVA/
├── CLAUDE.md                    ← this file
│
├── pci_mova/                    ← unified package
│   ├── kernel_main.py           ← entry: python -m pci_mova.kernel_main N
│   ├── web_main.py              ← entry: python -m pci_mova.web_main
│   │
│   ├── ipc/                     ← ★ Phase 1 — IPC layer (COMPLETE)
│   │   ├── server.py            ← IPCServer (push + cmd sockets, kernel side)
│   │   └── client.py            ← KernelClient + KernelRegistry (web side)
│   │
│   ├── core/                    ← Phase 2 — from /opt/MOVA/pci_mova/core/
│   │   ├── io/                  ← base.py, simulated.py, xkop_io.py, cm5_io.py
│   │   ├── kernel/              ← wrapper.py, libmova.so
│   │   ├── model/               ← buffers.py, enums.py
│   │   └── state/               ← status.py, faults.py, derived.py
│   ├── stream.py                ← MovaStream — 10Hz tick loop
│   ├── dataset/                 ← parser.py (.mxds XML)
│   ├── streams/                 ← manager.py (dataset state persistence)
│   ├── licence/                 ← licence.py, mova.licence
│   ├── protocol/                ← mova_tools.py (AML TCP, ports 6000–6007)
│   ├── log/                     ← logger.py (JSONL per stream)
│   │
│   ├── api/                     ← Phase 3 — web routes + WebSocket
│   │   ├── app.py               ← FastAPI app
│   │   ├── routes/              ← streams.py, dataset.py, licence.py, system.py
│   │   └── ws/                  ← live.py (WebSocket push)
│   │
│   └── web/                     ← static HTML UI (all popup pages)
│       └── static/
│           ├── index.html
│           ├── analysis.html
│           └── ...
│
├── cm5/                         ← ★ CM5 platform (copied from /opt/CM5)
│   ├── main.py                  ← CM5 entry point
│   ├── cm5_web.py               ← Flask/SSE web dashboard — Phase 4 adds MOVA tabs here
│   ├── core/                    ← IOBus, config
│   ├── ug405/                   ← SNMP UTC outstation
│   ├── rtig/                    ← RTIG real-time feed
│   ├── autodim/                 ← astronomical dim/bright
│   ├── conditioner/             ← IO logic/conditioning
│   ├── offline_plan/            ← fixed-time plan fallback
│   ├── flir/                    ← FLIR thermal cameras
│   ├── agd/                     ← AGD650 radar
│   ├── io/                      ← XKOP, RPDB drivers
│   ├── platform.cfg             ← service enables, IOBus, logging
│   └── CLAUDE.md                ← CM5 context (read before CM5 changes)
│
├── config/                      ← platform config
│   ├── platform.cfg             ← service enables, IOBus, logging
│   ├── signals.cfg              ← IOBus signal ownership map
│   └── streams.json             ← per-stream IO signal mappings
│
├── logs/                        ← all service logs
│
├── licence/
│   └── pci.licence
│
└── tests/
    └── test_ipc.py              ← Phase 1 IPC integration test
```

---

## Implementation phases

### Phase 1 — IPC layer ✅ COMPLETE
`pci_mova/ipc/server.py` + `client.py` — standalone, testable now.
```bash
cd /opt/MC_MOVA
python -m pci_mova.kernel_main 0 &
python tests/test_ipc.py
kill %1
```

### Phase 2 — Kernel entry point ✅ COMPLETE (2026-05-31)

`kernel_main.py` — real `MovaStream` + `IPCServer` + MOVA Tools, fully working.

**What was brought in:**
- All kernel modules from `/opt/MOVA/pci_mova/`: `core/`, `stream.py`, `dataset/`, `streams/`, `licence/`, `protocol/`, `log/`
- `libmova.so` (C kernel binary) + rebuilt with `MI_gs_check()` (see below)
- `pci_mova/dataset/parser.py` extended with public `validate_sc_rules()` function

**Verified working:**
- Licence check on startup; clean exit if stream N not licensed
- Dataset load via LOAD IPC command — kernel initialises, tick loop starts
- CRB + MOVA_ON enable via SET_CRB / SET_IO IPC commands
- ON_CONTROL=1, EC=0 achieved within 2 seconds
- Kernel stage cycling active (force bits set, messages flowing via IPC)
- MOVA Tools TCP server on port `6000+N` (asyncio in its own thread)
- Dataset state persistence — saved on LOAD/UNLOAD/SET_IO(27); restored on restart
- SIGTERM clean shutdown
- IPC events published: `phase_change`, `on_control`, `mova_on`, `ec_change`, `status_change`

**SimulatedIO warmup bypass — design note:**

In the embedded RTOS, the TLC cycles stages independently and MOVA observes confirms
to count warmup stage visits. In simulation (SimulatedIO, no real TLC), this can't
happen because `genstg.c:handle_consistent_crbs()` explicitly skips force bits when
`ON_CONTROL=0`, so auto_follow has nothing to respond to.

Fix: `MI_gs_check()` added to `M8.0/mova_api.c`. Called from `kernel_main.py` when
CRB and MOVA_ON are both set in SimulatedIO mode. It pre-sets `Tcomshr->snow` and
`p_junction->current_stage` to 1 (the reversionary stage), then calls
`GS_check_and_update_io_flags()` directly — bypassing `warmup_check()`'s
PM-transition gate. This is the same outcome as `handle_error_count()` on fault
recovery, but triggered immediately at startup rather than after an error cycle.

In production (XKOP or CM5 IOBus), this bypass is NOT triggered — real TLC confirms
drive warmup naturally.

### Phase 3 — Web entry point
`web_main.py` — create `KernelRegistry`, mount FastAPI routes, start uvicorn.
Bring in `api/` + `web/` from `/opt/MOVA/pci_mova/`.
Update route handlers to proxy via `KernelClient.send_command()`.
Update WebSocket handlers to read from `KernelClient` push stream.
JSONL reading (History/Analysis) reads files directly — no kernel involved.

### Phase 4 — Route handler updates
Update `api/routes/streams.py` to proxy all commands via `KernelClient.send_command()`.
Update `api/ws/live.py` WebSocket handlers to read from `KernelClient`.
All API paths and response shapes stay identical — browser cannot tell the difference.

### Phase 5 — Systemd + migration
New unit files (see below). Disable old `pci-mova.service`. Enable new services.
Verify full end-to-end operation.

### Phase 6 — CM5 IOBus IO
Implement `cm5_io.py` Unix socket client: BATCH read detectors/confirms/CRB,
W write forces. Apply 150ms rising-edge latch (see IO latch section).
Kernel processes connect to CM5 IOBus automatically if socket present.

### Phase 7 — CM5 service split
Extract UG405, RTIG, AutoDim etc. from CM5 monolith as standalone processes
under `services/`. Each gets its own IPC socket pair for the web service.
Migrate web aggregation from CM5 `cm5_web.py` to the unified web service here.

---

## MOVA kernel — process design

### Threading model (pure threading, no asyncio)
```
main thread       — MovaStream._loop() 10Hz (tick loop owns SIGTERM)
ipc thread        — IPCServer accept/dispatch (push + command sockets)
push thread       — drains snapshot/msg/err queue, writes to IPC clients
MOVA Tools thread — AML TCP server (ports 6000+N)
```

### IO architecture
**No XKOP directly in MOVA kernel processes.** CM5 (or pci-iobus) owns hardware.
Each kernel process reads from and writes to the IOBus signal bus.

Signal mapping per stream in `config/streams.json`:
```json
{
  "0": {
    "io": {
      "type": "cm5",
      "det_pattern":     "rpdb.i.xdet.{n}",
      "confirm_pattern": "xkop.i.{n}",
      "crb_signal":      "xkop.i.32",
      "force_pattern":   "xkop.o.{n}",
      "special_pattern": "xkop.o.{n}"
    }
  }
}
```

SimulatedIO remains available as default when no IOBus socket present.

### 150ms detector latch (required)
On XKOP or IOBus inputs, apply 150ms rising-edge latch before feeding `buffers.din[]`.
Reason: MINON=1 means detector must be seen HIGH on at least one 100ms tick scan.
Worst case a pulse arrives 1ms after a tick — next tick 99ms away. 150ms hold gives
~51ms margin. Implement in `xkop_io.py` and `cm5_io.py`: on 0→1 start a
`time.monotonic()` timer per detector; suppress 1→0 until elapsed ≥ 0.15s.

---

## MOVA kernel C interface

### Library
`pci_mova/core/kernel/libmova.so` — same binary as `/opt/MOVA/pci_mova/core/kernel/libmova.so`.
Each stream gets a private runtime copy (temp file) to isolate C global state.

### Init sequence (confirmed working)
```python
lib = ctypes.CDLL(path, mode=ctypes.RTLD_LOCAL)
lib.XML_DataSet_Initialise()
lib.CI_init_core_module()
lib.CI_init_core_dynmaic_data()
result = lib.XML_DataSet_ReadAllDataPlansFromFile(filepath, controller_id)
# result == 0 means MV_SUCCESS
lib.monitr()  # called every 100ms tick — co-routine, never blocks
```

### MI_* API (complete list)
See `/opt/MOVA/CLAUDE.md` section "MI_* API functions — complete list" for the full table.
Key additions relevant to architecture:
- `MI_set_io_flag(27, value)` — MOVA_ON (must write before monitr each tick)
- `MI_get_all_detectors(ptr, n)` — read kernel_deton[] after tick (incl SC virtual dets)
- `MI_get_current_msg_num()` / `MI_get_msg_field(slot, field)` — decision messages ring buffer

### IPC events to publish from tick loop (Phase 2)
```python
# Phase change (CS transitions)
if new_cs != prev_cs:
    ipc.publish_event("phase_change", from_stage=prev_cs, to_stage=new_cs)

# ON_CONTROL transitions
if new_on_ctrl != prev_on_ctrl:
    ipc.publish_event("on_control", value=new_on_ctrl)

# MOVA_ON transitions
if new_mova_on != prev_mova_on:
    ipc.publish_event("mova_on", value=new_mova_on)

# New kernel errors
for err in new_errors:
    ipc.publish_error(err)

# New kernel messages (decision messages — always immediate)
for msg in new_msgs:
    ipc.publish_message(msg)
```

---

## MOVA Tools (AML protocol)

Ports 6000–6007 (one per stream). TCP servers run inside each kernel process.
MOVA Tools (Windows client) connects directly to the kernel — no web involvement.

Wire framing: `STX(0x02) + ascii_decimal_size + ETX(0x03) + json_body`
All request/response `{"MessageType":"ReqXxx","Params":{"field":val,...}}`

**Current status:** AML protocol fully implemented in `/opt/MOVA/pci_mova/protocol/mova_tools.py`.
Junction mimic does not render — needs Wireshark capture to find post-bypass init sequence.

**Wireshark capture setup:**
- LXC: `tcpdump -i any -w /tmp/movatools.pcap port 6000`
- Or capture on Win11 NIC while MOVA Tools connects to `192.168.71.22:6000`
- Walk through: connect → dataset mismatch → bypass → attempt mimic
- Compare response sequence against our `_dispatch()` to find missing exchange

---

## IOBus signal protocol (from CM5)

Socket `/tmp/pci_iobus.sock` (Unix domain, line-based UTF-8):

| Command | Example | Response |
|---------|---------|----------|
| `R <signal>` | `R xkop.i.5` | `0` or `1` |
| `W <signal> <value>` | `W xkop.o.3 1` | `OK` or `ERR <msg>` |
| `BATCH <s1> <s2>...` | `BATCH xkop.i.1 xkop.i.2` | `0 1` |
| `PING` | `PING` | `PONG` |

BATCH is the primary read path — all detector signals in one round-trip (<0.5ms).

### Signal namespaces (CM5 IOBus)
- `rpdb.i.xdet.*` — RPDB vehicle detector inputs
- `rpdb.i.xout.*` — RPDB stage/output inputs
- `xkop.i.*` — XKOP TLC inputs (stage confirms, CRB at i.32)
- `xkop.o.*` — XKOP TLC outputs (stage forces from MOVA)
- `virt.0–299` — conditioner virtual signals
- `virt.300–399` — FLIR camera signals
- `virt.400–499` — AGD650 radar signals

Each signal has one declared owner. Conflict at startup = hard error.

**MOVA writes:** registered to owner `'mova'` in IOBus config — other owners rejected.

---

## CM5 services reference

All to be ported as standalone processes under `services/`. Key design notes:

### UG405 (SNMP UTC Type 2)
- SNMP GET/SET/INFORM, RBE generation, SCOOT VSn occupancy packing
- opMode authority: 1=standalone, 2=monitor+RBE, 3=UTC control
- Anti-drop control: **set new Fn bits first, clear old bits second**
- On mode drop: zero all ug405-owned outputs (`io.zero_owned_by('ug405')`)
- RBE threads dormant until opMode ≥ 2 — opMode gate is the correct control

### XKOP driver
- 17-byte fixed packets: `[CA 35][type][idx 00 val ×4][crc1 crc0]`
- Keepalive: `CA35 02 ...` every 6s — dead detection at 18s (3× keepalive)
- On reconnect: zero all xkop-owned inputs (prevents stale state)
- Backoff: 2s → 4s → ... → 300s cap, interruptible via `resume()`

### AutoDim
- Astronomical controller using `astral` library (lat/lon + offsets)
- Single output signal, writes every 30s
- Hot-reload: lat/lon, offsets, enabled, output signal via `/api/autodim`

### Conditioner
- Rules engine — ON_DELAY, OFF_DELAY, PULSE, WATCHDOG, LATCH, TOGGLE, COUNT, SPEED, etc.
- Hot-reload via `/api/conditioner` — unregisters stale signals on reload
- Nesting: inner calls synthesised to `_cond.N` internal bus signals

### Offline Plan
- Fixed-time plan runner: drives Fn/Dn from JSON timetable when opMode < 3
- Hot-reload: file mtime check every tick, reload within 1 second
- Anti-drop sequencing: set new bits first, clear old second

### FLIR thermal cameras
- WebSocket client, multi-camera, zone → virt signal mapping
- Zone lookup: unit-qualified `camera.N.zone.Z.type` takes priority over global
- Signals: `virt.300–351`

### AGD650 radar
- ZeroMQ SUB client, multi-unit, zone → virt signal mapping
- Frame-based (full zone-state every ~150ms), internal change detection
- Signals: `virt.400–493`

---

## Systemd units (target state)

**Template** `/etc/systemd/system/pci-mova-kernel@.service`:
```ini
[Unit]
Description=PCI MOVA Kernel Stream %i
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=/opt/MC_MOVA
ExecStart=/opt/mova-env/bin/python -m pci_mova.kernel_main %i
Restart=on-failure
RestartSec=5
MemoryHigh=128M
MemoryMax=256M
MemorySwapMax=0
Environment=MOVA_KERNEL_LIB=/opt/MC_MOVA/pci_mova/core/kernel/libmova.so
Environment=MOVA_DATASET_DIR=/opt/MC_MOVA/pci_mova/datasets
Environment=MOVA_LICENCE_DIR=/opt/MC_MOVA/licence
Environment=MOVA_LOG_DIR=/opt/MC_MOVA/logs

[Install]
WantedBy=multi-user.target
```

**Web** `/etc/systemd/system/pci-mova-web.service`:
```ini
[Unit]
Description=PCI MOVA Web Service
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=/opt/MC_MOVA
ExecStart=/opt/mova-env/bin/python -m pci_mova.web_main
Restart=on-failure
RestartSec=5
MemoryHigh=384M
MemoryMax=512M
MemorySwapMax=0

[Install]
WantedBy=multi-user.target
```

Enable all 8 kernel streams + web:
```bash
for i in 0 1 2 3 4 5 6 7; do systemctl enable pci-mova-kernel@$i; done
systemctl enable pci-mova-web
```

Licence-denied exits (code 0) do not trigger `Restart=on-failure` — systemd is silent.

---

## Extended IO output mapping (planned)

Physical outputs for MOVA internal state — nobody else provides these:

| Source | Field | Proposed output | Use case |
|--------|-------|-----------------|----------|
| Detector fault | `buffers.dout[19]` | Special output | TLC panel relay |
| Endsat (any link) | `derived.endsat_any` | Special output | Hold ped phase |
| Per-link endsat | `derived.endsat_link[n]` | Special outputs 1–N | Link-specific downstream control |
| OSAT | `derived.oversaturated` | Special output | Control room indicator |
| EP hold | `derived.ep_hold` | Special output | Emergency pre-emption active |
| EP extension | `derived.ep_ext` | Special output | EP extension phase |
| EM/PR truncation | `derived.em_pr_truncation` | Special output | Emergency truncation |

Implementation: output_map config per stream → `stream.py` `_tick()` iterates map,
writes each bit via active IO layer after `derived.update_from_buffers()`.

---

## Memory profile (expected after split)

| Process | Expected RSS |
|---------|-------------|
| `pci-mova-kernel@N` | ~15–20MB flat per stream |
| `pci-mova-web` | 150–250MB under load (log reads, WebSocket, analysis) |
| `pci-ug405` | ~30–50MB |
| Other services | ~10–20MB each |

Web process can spike without affecting any kernel process — GIL not shared.
`MemoryMax=256M` per kernel process is a hard kill ceiling — more than enough.

---

## What is unchanged from /opt/MOVA

- All HTML/JS UI files — zero changes
- All API endpoint paths and response shapes — zero changes
- JSONL log format and file paths — zero changes
- MOVA Tools AML protocol (ports 6000–6007) — zero changes
- CM5 IOBus protocol — zero changes
- C kernel, wrapper.py, buffers.py, enums.py — zero changes
- MovaStream internal logic — zero changes
- StreamLogger — zero changes
- Dataset parser — zero changes

The browser cannot tell the difference between the old and new architecture.

---

## Python environments

```
MOVA venv:  /opt/mova-env    (fastapi, uvicorn[standard], python-multipart)
CM5 venv:   /opt/ug405-env   (pysnmp, astral, pyzmq, flask, ...)
```

Use `/opt/mova-env/bin/python` for all MC_MOVA work.
Do NOT mix venvs.

---

## Outstanding work

### Phase 1 — IPC layer ✅ COMPLETE
### Phase 2 — Kernel entry point ✅ COMPLETE (2026-05-31)

See Phase 2 section in Implementation phases for full detail.

### Phase 3 — CM5 IOBus IO ✅ COMPLETE (2026-05-31)

`pci_mova/core/io/cm5_io.py` — full implementation:
- BATCH read all detectors/confirms/CRB in one round-trip per tick
- 150ms rising-edge latch per detector
- W writes for forces/TO/HI/SYNC after each tick
- Reconnect on socket failure
- `default_signal_map(dataset)` auto-generates standard XKOP convention

`config/streams.json` — per-stream signal mapping:
- Auto-generated from dataset on LOAD (standard convention: det N → xkop.i.100+N, etc.)
- User-editable for non-standard wiring
- CM5IO activated automatically if entry present; SimulatedIO fallback if not

`kernel_main.py` — tries CM5IO on startup, falls back to SimulatedIO.
On LOAD, generates signal map if not already present.

**Standard convention:**
- MOVA det N (1-based) → `xkop.i.{100+N}`
- Stage confirm N → `xkop.i.{N}`
- CRB → `xkop.i.32`
- Stage force N → `xkop.o.{N}`
- TO/HI/SYNC → `xkop.o.11/12/13`

**Test path:** XKOPio → PTC-1 Sim (use CONNECT_XKOP IPC command to test before CM5 IOBus is live)

### Phase 4 — MOVA UI in CM5 web (NEXT)

**Decision:** MOVA stream tabs live on the CM5 web tree, not a separate web process.

One tab per licensed stream in CM5 web (`cm5_web.py`):
- Stream runtime view: status, stage, diagnostics, kernel messages, derived, TMA
- Buttons: Start / Stop / MOVA ON / Reset / **I/O MAP**
- I/O MAP opens signal mapping screen — dataset drives the list, user edits xkop channels
- Dataset load in CM5 web → sends LOAD via IPC to kernel → kernel configures CM5IO
- Pop-out button → opens stream in its own window (multi-stream monitoring)
- No SimulatedIO option in the UI — IO is always real hardware

CM5 web connects to kernel IPC push sockets for live state, sends commands via IPC cmd socket.

### Phase 5 — Route + WS handlers
One-for-one proxy of all existing endpoints through IPC.

### Phase 6 — Systemd + migration
New unit files, disable `pci-mova.service` on legacy `/opt/MOVA`.

### Phase 7 — CM5 service split
UG405, RTIG, AutoDim, etc. as standalone processes under `services/`.

### MOVA Tools mimic
Wireshark capture: MOVA Tools on Win11 → `192.168.71.22:6000`.
`tcpdump -i any -w /tmp/movatools.pcap port 6000` on LXC.
Find post-bypass init sequence that gates junction mimic rendering.

---

## Working with this repo

Start Claude Code from `/opt/MC_MOVA/` for all new platform work.
Reference `/opt/MOVA/CLAUDE.md` for detailed MOVA kernel notes (MI_* API, resolved bugs, etc.).
Reference `/opt/CM5/CLAUDE.md` for CM5 service implementation details (UG405, conditioner, etc.).

Keep both legacy repos running until each service is proven here.
