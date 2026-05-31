# svc_rtig.py
# RTIG HTTP listener service.
#
# Accepts HTTP POST containing <rtig_tlp .../> XML (as sent by RTIG.py exerciser).
# Matches message against rules defined in rtig_rules.json.
# On match → pulses the mapped IO bus signal for a configurable duration.
#
# Config (platform.cfg):
#   [RTIG]
#   port          = 9010
#   pulse_seconds = 2
#   rules_file    = /opt/CM5/rtig/rtig_rules.json
#
# Rules (rtig_rules.json) — array of rule objects.
# Each rule must have 'signal' (typed IO bus signal name e.g. xkop.o.109).
# All other fields are match criteria — omitted = wildcard.
# priority / schedule_deviation support comparator objects: {"op": "<=", "val": 2}
# Supported ops: =  !=  <  <=  >  >=

import collections
import json
import logging
import os
import threading
import time
import xml.etree.ElementTree as ET
from flask import Flask, request, Response

log = logging.getLogger("RTIG")

# ── Comparator matching ───────────────────────────────────────────────────────

_OPS = {
    "=":  lambda a, b: a == b,
    "!=": lambda a, b: a != b,
    "<":  lambda a, b: a <  b,
    "<=": lambda a, b: a <= b,
    ">":  lambda a, b: a >  b,
    ">=": lambda a, b: a >= b,
}

def _match_field(rule_val, msg_val):
    if isinstance(rule_val, dict):
        op  = rule_val.get("op", "=")
        ref = int(rule_val.get("val", 0))
        fn  = _OPS.get(op)
        if fn is None:
            log.warning("Unknown comparator op '%s' — treating as no-match", op)
            return False
        return fn(msg_val, ref)
    return int(rule_val) == msg_val

# ── Rule loader ───────────────────────────────────────────────────────────────

MATCH_FIELDS = ("traffic_signal", "movement", "trigger_point",
                "priority", "schedule_deviation")

def load_rules(path):
    if not os.path.exists(path):
        log.warning("Rules file not found: %s — no rules loaded", path)
        return []
    with open(path) as f:
        rules = json.load(f)
    valid = _validate_rules(rules)
    log.info("Loaded %d rule(s) from %s", len(valid), path)
    return valid

def _validate_rules(rules):
    valid = []
    for i, r in enumerate(rules):
        if "signal" not in r:
            log.warning("Rule #%d has no 'signal' — skipped", i)
            continue
        valid.append(r)
    return valid

# ── Rule matcher ──────────────────────────────────────────────────────────────

def match_rules(rules, msg):
    matched = []
    for rule in rules:
        ok = True
        for field in MATCH_FIELDS:
            if field not in rule:
                continue
            msg_val = msg.get(field)
            if msg_val is None:
                ok = False
                break
            if not _match_field(rule[field], msg_val):
                ok = False
                break
        if ok:
            matched.append(rule)
    return matched

# ── XML parser ────────────────────────────────────────────────────────────────

def parse_rtig_xml(body):
    try:
        root = ET.fromstring(body.strip())
    except ET.ParseError as e:
        log.warning("XML parse error: %s", e)
        return None

    if root.tag != "rtig_tlp":
        log.warning("Unexpected root tag '%s' (expected rtig_tlp)", root.tag)
        return None

    def _int(attr):
        v = root.get(attr)
        return int(v) if v is not None else None

    return {
        "traffic_signal":     _int("traffic_signal"),
        "movement":           _int("movement"),
        "trigger_point":      _int("trigger_point"),
        "priority":           _int("priority"),
        "schedule_deviation": _int("schedule_deviation"),
    }

# ── Pulse engine ──────────────────────────────────────────────────────────────

class PulseEngine:
    """
    Manages timed output pulses on the IO bus using typed signal names.
    pulse(signal) sets bus.write(signal, 1) then resets to 0 after duration.
    A second pulse on the same active signal restarts the timer.
    """

    def __init__(self, bus, duration):
        self.bus      = bus
        self.duration = duration
        self._timers  = {}       # signal → Timer
        self._lock    = threading.Lock()

    def pulse(self, signal):
        with self._lock:
            existing = self._timers.get(signal)
            if existing:
                existing.cancel()

            self.bus.write(signal, 1, source='rtig')
            log.debug("PULSE ON  → %s  (%.1fs)", signal, self.duration)

            t = threading.Timer(self.duration, self._expire, args=(signal,))
            t.daemon = True
            t.start()
            self._timers[signal] = t

    def _expire(self, signal):
        with self._lock:
            self._timers.pop(signal, None)
        self.bus.write(signal, 0, source='rtig')
        log.debug("PULSE OFF ← %s", signal)

    def active(self):
        """Return set of currently pulsing signal names."""
        with self._lock:
            return set(self._timers.keys())

# ── RTIG Service ──────────────────────────────────────────────────────────────

TLP_LOG_SIZE = 50   # number of recent TLPs to keep in memory

class RTIGService:
    """
    HTTP listener service for RTIG TLP trigger messages.

    Parameters
    ----------
    bus          : IOBus   — shared IO state bus
    port         : int     — TCP port to listen on (default 9010)
    pulse_secs   : float   — output pulse duration in seconds (default 2.0)
    rules_file   : str     — path to rtig_rules.json
    signal_map   : dict    — optional alias map {'RTIG1': 'xkop.o.109', ...}
    """

    def __init__(self, bus, port=9010, pulse_secs=2.0,
                 rules_file='/opt/CM5/rtig/rtig_rules.json',
                 signal_map=None):
        self.bus        = bus
        self.port       = port
        self.pulse_secs = pulse_secs
        self.rules_file = rules_file
        self.signal_map = signal_map or {}

        self.rules  = load_rules(rules_file)
        self.pulser = PulseEngine(bus, pulse_secs)
        self._app   = self._build_app()

        # ── State for web UI ──────────────────────────────────────────────────
        self._state_lock = threading.Lock()
        self.total_rx    = 0
        self.last_tlp    = None   # last received msg dict + metadata
        self.tlp_log     = collections.deque(maxlen=TLP_LOG_SIZE)

    # ── State snapshot (for web UI) ───────────────────────────────────────────

    def snapshot(self):
        """Return a serialisable snapshot of current RTIG state."""
        with self._state_lock:
            return {
                'port':        self.port,
                'pulse_secs':  self.pulse_secs,
                'rules_count': len(self.rules),
                'total_rx':    self.total_rx,
                'last_tlp':    dict(self.last_tlp) if self.last_tlp else None,
                'tlp_log':     list(self.tlp_log),
                'pulsing':     list(self.pulser.active()),
                'rules':       [dict(r, signal=self.signal_map.get(r['signal'], r['signal']))
                               for r in self.rules],
            }

    def _record_tlp(self, msg, matched_signals):
        """Record a received TLP into state (called from receive handler)."""
        ts = time.strftime('%H:%M:%S')
        entry = {
            'ts':                ts,
            'traffic_signal':    msg.get('traffic_signal'),
            'movement':          msg.get('movement'),
            'trigger_point':     msg.get('trigger_point'),
            'priority':          msg.get('priority'),
            'schedule_deviation': msg.get('schedule_deviation'),
            'matched':           matched_signals,   # list of signal names pulsed
        }
        with self._state_lock:
            self.total_rx += 1
            self.last_tlp  = entry
            self.tlp_log.appendleft(entry)

    # ── Flask app ─────────────────────────────────────────────────────────────

    def _build_app(self):
        app = Flask(__name__)
        app.logger.disabled = True
        logging.getLogger('werkzeug').setLevel(logging.ERROR)

        svc = self

        @app.route('/', methods=['POST'])
        @app.route('/<path:_>', methods=['POST'])
        def receive(_=None):
            body = request.get_data(as_text=True)
            log.debug("RX: %s", body.strip())

            msg = parse_rtig_xml(body)
            if msg is None:
                return Response('Bad Request', status=400)

            log.debug(
                "RTIG msg  sig=%-5s mov=%-3s trg=%-3s pri=%-4s dev=%s",
                msg['traffic_signal'], msg['movement'],  msg['trigger_point'],
                msg['priority'],       msg['schedule_deviation']
            )

            matched = match_rules(svc.rules, msg)
            pulsed  = []

            if not matched:
                log.debug("No rule matched")

            for rule in matched:
                name   = rule['signal']
                signal = svc.signal_map.get(name, name)
                if signal == name and not name.startswith(
                        ('xkop.', 'gp.', 'virt.', 'cm5.', 'modb.')):
                    log.warning("RULE HIT but '%s' not in signal_map — skipped", name)
                    continue
                log.debug("RULE HIT  %s → %s  pulse=%.1fs", name, signal, svc.pulse_secs)
                svc.pulser.pulse(signal)
                pulsed.append(signal)

            svc._record_tlp(msg, pulsed)
            return Response('OK', status=200)

        @app.route('/rtig/rules', methods=['GET'])
        def get_rules():
            return Response(json.dumps(svc.rules, indent=2),
                            status=200, mimetype='application/json')

        @app.route('/rtig/rules', methods=['POST'])
        def set_rules():
            try:
                new_rules = request.get_json(force=True)
                if not isinstance(new_rules, list):
                    return Response('Expected JSON array', status=400)
                svc.rules = _validate_rules(new_rules)
                with open(svc.rules_file, 'w') as f:
                    json.dump(svc.rules, f, indent=2)
                log.info("Rules updated via API (%d rules)", len(svc.rules))
                return Response(json.dumps({'saved': len(svc.rules)}),
                                status=200, mimetype='application/json')
            except Exception as e:
                return Response(str(e), status=400)

        @app.route('/rtig/status', methods=['GET'])
        def get_status():
            return Response(json.dumps(svc.snapshot(), indent=2),
                            status=200, mimetype='application/json')

        return app

    def start(self):
        log.info("RTIG listener on 0.0.0.0:%d  pulse=%.1fs  rules=%d",
                 self.port, self.pulse_secs, len(self.rules))
        threading.Thread(
            target=self._app.run,
            kwargs={'host': '0.0.0.0', 'port': self.port, 'use_reloader': False},
            daemon=True
        ).start()

    def reload_rules(self):
        self.rules = load_rules(self.rules_file)
        log.info("Rules reloaded (%d rules)", len(self.rules))


# ── Standalone test ───────────────────────────────────────────────────────────

if __name__ == '__main__':
    logging.basicConfig(
        level=logging.DEBUG,
        format='%(asctime)s [%(name)s] %(levelname)s  %(message)s',
        datefmt='%H:%M:%S'
    )

    class StubBus:
        def write(self, signal, value, source=None):
            print(f"  BUS WRITE  signal={signal}  value={value}  src={source}")
        def read(self, signal): return 0
        def subscribe(self, _): pass

    import configparser
    cfg = configparser.ConfigParser(strict=False)
    _root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    cfg.read(os.path.join(_root, 'rtig', 'rtig.cfg'))

    port       = cfg.getint('RTIG', 'port', fallback=9010)
    pulse_secs = cfg.getfloat('RTIG', 'pulse_seconds', fallback=2.0)
    rules_file = cfg.get('RTIG', 'rules_file', fallback='rtig_rules.json')

    svc = RTIGService(StubBus(), port=port, pulse_secs=pulse_secs,
                      rules_file=rules_file)
    svc.start()

    print(f"\nRTIG test listener on port {port}")
    print(f'curl -X POST http://localhost:{port}/ \\')
    print('  -H "Content-Type: application/xml" \\')
    print("  -d '<rtig_tlp traffic_signal=\"0\" movement=\"1\" trigger_point=\"0\" priority=\"2\"/>'")

    try:
        threading.Event().wait()
    except KeyboardInterrupt:
        print('\nStopped.')
