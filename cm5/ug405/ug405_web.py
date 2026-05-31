# ug405_web.py
# Flask SSE dashboard for UG405 agent
# Uses typed signal names from IOBus

import json
import time
import threading
from flask import Flask, Response, render_template_string

# Shared state — set by start_web() from main.py
io        = None
mapping   = {'scns': [], 'control': {}, 'reply': {}}
live      = {'InstationAddress': '0.0.0.0', 'InstationPort': 1162}
op_mode   = {'value': 1}

app = Flask(__name__)

subscribers      = []
subscribers_lock = threading.Lock()

HTML = '''
<!DOCTYPE html>
<html>
<head>
    <title>UG405 Agent Dashboard</title>
    <style>
        body { font-family: Consolas, monospace; background: #1a1a2e; color: #eee; margin: 20px; }
        h1 { color: #00d4ff; }
        .status { margin-bottom: 20px; padding: 10px; background: #16213e; border-radius: 6px; }
        .status span { margin-right: 30px; }
        .opmode-1 { color: #ff6b6b; }
        .opmode-2 { color: #ffd93d; }
        .opmode-3 { color: #6bcb77; }
        .scn-block { margin-bottom: 30px; }
        .scn-title { color: #00d4ff; font-size: 1.2em; margin-bottom: 8px;
                     padding: 4px 8px; background: #16213e; border-radius: 4px; }
        table { border-collapse: collapse; width: 100%; margin-bottom: 10px; }
        th { background: #16213e; color: #00d4ff; padding: 6px 10px; text-align: left; }
        td { padding: 5px 10px; border-bottom: 1px solid #2a2a4a; font-size: 0.9em; }
        .val-1 { color: #6bcb77; font-weight: bold; }
        .val-0 { color: #555; }
        .tables-row { display: flex; gap: 20px; }
        .tables-row > div { flex: 1; }
        .label { color: #888; font-size: 0.85em; margin-bottom: 4px; }
        .col-ctrl  { background: #0f1a2e; }
        .col-reply { background: #0a1a20; }
    </style>
</head>
<body>
    <h1>UG405 Agent Dashboard</h1>

    <div class="status">
        <span>OpMode: <strong id="opmode" class="opmode-1">1 - Standalone</strong></span>
        <span>Instation: <strong id="instation">-</strong></span>
        <span>Last update: <strong id="lastupdate">-</strong></span>
    </div>

    <div id="scn-container"></div>

    <script>
    const mapping = {{ mapping_json | safe }};

    function buildRows(fields, side) {
        let rows = '';
        for (const [field, bits] of Object.entries(fields)) {
            for (const [bit, sig] of Object.entries(bits)) {
                const id = `${side}-${sig.replace(/\./g, '_')}`;
                rows += `<tr>
                    <td>${field}</td>
                    <td>${bit == '0' ? '-' : bit}</td>
                    <td>${sig}</td>
                    <td id="${id}" class="val-0">0</td>
                </tr>`;
            }
        }
        return rows;
    }

    function buildTables() {
        const container = document.getElementById('scn-container');
        mapping.scns.forEach(scn => {
            const block = document.createElement('div');
            block.className = 'scn-block';
            block.innerHTML = `
                <div class="scn-title">SCN: ${scn}</div>
                <div class="tables-row">
                    <div class="col-ctrl">
                        <div class="label">CONTROL (SNMP → XKOP)</div>
                        <table>
                            <tr><th>Field</th><th>Bit</th><th>Signal</th><th>Value</th></tr>
                            ${buildRows(mapping.control[scn], 'ctrl')}
                        </table>
                    </div>
                    <div class="col-reply">
                        <div class="label">REPLY (XKOP → SNMP)</div>
                        <table>
                            <tr><th>Field</th><th>Bit</th><th>Signal</th><th>Value</th></tr>
                            ${buildRows(mapping.reply[scn], 'reply')}
                        </table>
                    </div>
                </div>`;
            container.appendChild(block);
        });
    }

    const OPMODE_LABELS  = {1: '1 - Standalone', 2: '2 - Monitor', 3: '3 - UTC Control'};
    const OPMODE_CLASSES = {1: 'opmode-1', 2: 'opmode-2', 3: 'opmode-3'};

    function applyUpdate(data) {
        document.getElementById('lastupdate').textContent = new Date().toLocaleTimeString();
        if (data.opmode !== undefined) {
            const el = document.getElementById('opmode');
            el.textContent = OPMODE_LABELS[data.opmode] || data.opmode;
            el.className   = OPMODE_CLASSES[data.opmode] || '';
        }
        if (data.instation !== undefined) {
            document.getElementById('instation').textContent = data.instation;
        }
        if (data.changes) {
            data.changes.forEach(([id, val]) => {
                const el = document.getElementById(id);
                if (el) {
                    el.textContent = val;
                    el.className   = val > 0 ? 'val-1' : 'val-0';
                }
            });
        }
    }

    buildTables();

    const es = new EventSource('/ug405/stream');
    es.onmessage = e => applyUpdate(JSON.parse(e.data));
    es.onerror   = () => {
        document.getElementById('lastupdate').textContent = 'Reconnecting...';
    };
    </script>
</body>
</html>
'''


def build_mapping_json():
    out = {'scns': mapping['scns'], 'control': {}, 'reply': {}}
    for scn in mapping['scns']:
        out['control'][scn] = {
            field: {str(bit): sig for bit, sig in bits.items()}
            for field, bits in mapping['control'][scn].items()
        }
        out['reply'][scn] = {
            field: {str(bit): sig for bit, sig in bits.items()}
            for field, bits in mapping['reply'][scn].items()
        }
    return json.dumps(out)


def _sig_raw(sig):
    return sig[1:] if sig.startswith('!') else sig

def sig_to_id(side, sig):
    """Convert signal name to HTML element ID (strips inversion marker)."""
    return f"{side}-{_sig_raw(sig).replace('.', '_')}"


@app.route('/ug405/')
def index():
    return render_template_string(HTML, mapping_json=build_mapping_json())


@app.route('/ug405/stream')
def stream():
    q = []
    with subscribers_lock:
        subscribers.append(q)

    def generate():
        # Full state on connect
        changes = []
        if io:
            for scn in mapping['scns']:
                for field, bits in mapping['control'][scn].items():
                    for bit, sig in bits.items():
                        changes.append([sig_to_id('ctrl', sig), io.read(sig)])
                for field, bits in mapping['reply'][scn].items():
                    for bit, sig in bits.items():
                        raw = _sig_raw(sig)
                        val = io.read(raw)
                        v   = (1 - val) if sig.startswith('!') else val
                        changes.append([sig_to_id('reply', sig), v])

        init = {
            'opmode':    op_mode['value'],
            'instation': f"{live['InstationAddress']}:{live['InstationPort']}",
            'changes':   changes
        }
        yield f"data: {json.dumps(init)}\n\n"

        while True:
            if q:
                yield f"data: {q.pop(0)}\n\n"
            else:
                time.sleep(0.05)

    return Response(generate(), mimetype='text/event-stream',
                    headers={'Cache-Control': 'no-cache',
                             'X-Accel-Buffering': 'no'})


def push_update(changes=None, opmode=None, instation=None):
    """Called by services when state changes."""
    payload = {}
    if changes:    payload['changes']   = changes
    if opmode is not None: payload['opmode'] = opmode
    if instation:  payload['instation'] = instation
    if not payload:
        return
    msg = json.dumps(payload)
    with subscribers_lock:
        for q in subscribers:
            q.append(msg)


def start_web(port=12007, _io=None, _mapping=None, _live=None,
              _op_mode=None, **kwargs):
    global io, mapping, live, op_mode
    if _io       is not None: io       = _io
    if _mapping  is not None: mapping  = _mapping
    if _live     is not None: live     = _live
    if _op_mode  is not None: op_mode  = _op_mode

    threading.Thread(
        target=lambda: app.run(host='0.0.0.0', port=port, threaded=True),
        daemon=True
    ).start()
    print(f"[WEB] Dashboard at http://192.168.71.22:{port}/ug405/")


if __name__ == '__main__':
    from config import load_config
    from io_bus import IOBus

    cfg = load_config('/opt/ug405/platform.cfg')
    _io = IOBus()
    for name, owner in cfg['signals'].items():
        _io.register(name, owner)

    start_web(12007,
              _io      = _io,
              _mapping = {'scns':    cfg['ug405']['scns'],
                          'control': cfg['ug405']['control'],
                          'reply':   cfg['ug405']['reply']},
              _live    = live,
              _op_mode = op_mode)

    while True:
        time.sleep(1)
