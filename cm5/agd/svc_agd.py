# agd/svc_agd.py
# AGD650 radar detector client.
# Subscribes to ZeroMQ PUB frames from one or more AGD650 units.
#
# Frame model (from AGD650):
#   Every ~150ms a full snapshot arrives:
#   {"SN": "...", "msUTC": ..., "version": 2,
#    "zones": [{"index": N, "state": 1|2, "roadusers": [{...}]}, ...]}
#   state 1 = detected, state 2 = non-detect
#
# Change detection:
#   Only writes to IOBus when zone state or class presence changes.
#   Frame timeout: if no frame received within <frame_timeout> seconds,
#   calls on_fault() — treated as dead unit.

import json
import time
import logging
import threading
import configparser
import os
from collections import deque

log = logging.getLogger('AGD')

ZONE_TYPES = ['detected'] + []   # class types added dynamically from [CLASSES]


def load_agd(cfg_path):
    cp = configparser.ConfigParser(inline_comment_prefixes=('#',))
    cp.read(cfg_path)

    units = []
    for section in sorted(cp.sections()):
        if not section.upper().startswith('AGD.'):
            continue
        ip    = cp.get(section, 'ip',   fallback='127.0.0.1')
        port  = cp.getint(section, 'port', fallback=9003)
        zones = [int(x.strip()) for x in
                 cp.get(section, 'zones', fallback='1').split(',') if x.strip()]
        units.append({'id': section, 'ip': ip, 'port': port, 'zones': zones})

    # class name → signal suffix mapping
    classes = {}
    if cp.has_section('CLASSES'):
        for cls_name, suffix in cp.items('CLASSES'):
            classes[cls_name.strip().lower()] = suffix.strip()

    frame_timeout = cp.getfloat('SETTINGS', 'frame_timeout', fallback=5.0)
    mapping = dict(cp.items('VIRT_MAPPING')) if cp.has_section('VIRT_MAPPING') else {}

    return {
        'units':         units,
        'classes':       classes,
        'frame_timeout': frame_timeout,
        'mapping':       mapping,
    }


class AGDDriver:
    def __init__(self, io_bus, cfg):
        self.io            = io_bus
        self.cfg           = cfg
        self.running       = False
        self.mapping       = cfg['mapping']
        self.classes       = cfg['classes']   # {class_name: signal_suffix}
        self.frame_timeout = cfg['frame_timeout']
        self._events       = deque(maxlen=50)
        self._all_zones    = [z for u in cfg['units'] for z in u['zones']]
        self.on_fault      = None
        self.on_reconnect  = None

        self._register_signals()

    def _sig(self, unit_id, zone, signal_type):
        """Unit-qualified key first, global fallback."""
        unit_key   = f'{unit_id.lower()}.zone.{zone}.{signal_type}'
        global_key = f'zone.{zone}.{signal_type}'
        return self.mapping.get(unit_key) or self.mapping.get(global_key)

    def _global_sig(self, name):
        return self.mapping.get(name)

    def _register_signals(self):
        registered = set()

        def _reg(sig, label=''):
            if sig and sig not in registered:
                try:
                    self.io.register(sig, 'agd_driver')
                    registered.add(sig)
                    log.info("AGD registered %s%s", sig, f' ({label})' if label else '')
                except ValueError as e:
                    log.warning('%s', e)

        for unit in self.cfg['units']:
            for zone in unit['zones']:
                _reg(self._sig(unit['id'], zone, 'detected'), f'zone {zone} detected')
                for suffix in self.classes.values():
                    _reg(self._sig(unit['id'], zone, suffix), f'zone {zone} {suffix}')

        # Global any_* bits
        _reg(self._global_sig('any_detected'), 'any_detected')
        for suffix in self.classes.values():
            _reg(self._global_sig(f'any_{suffix.replace("has_", "")}'),
                 f'any_{suffix.replace("has_", "")}')

    def start(self):
        self.running = True
        for unit in self.cfg['units']:
            t = threading.Thread(
                target=self._unit_loop, args=(unit,),
                daemon=True, name=f"AGD-{unit['id']}")
            t.start()
            log.info("AGD unit %s starting -> tcp://%s:%d  zones=%s",
                     unit['id'], unit['ip'], unit['port'], unit['zones'])

    def stop(self):
        self.running = False

    def _unit_loop(self, unit):
        import zmq
        unit_id    = unit['id']
        unit_zones = set(unit['zones'])
        url        = f"tcp://{unit['ip']}:{unit['port']}"

        # Track previous zone state for change detection
        # {zone: {'detected': 0, 'has_car': 0, ...}}
        prev_state = {}

        faulted    = False
        last_frame = time.time()

        ctx  = zmq.Context.instance()
        sock = ctx.socket(zmq.SUB)
        sock.setsockopt(zmq.SUBSCRIBE, b'')
        sock.setsockopt(zmq.RCVTIMEO, 500)   # 500ms recv timeout for fault check
        sock.connect(url)
        log.info("AGD %s connected to %s", unit_id, url)

        while self.running:
            try:
                raw = sock.recv()
                last_frame = time.time()

                if faulted:
                    faulted = False
                    log.info("AGD %s frames resumed", unit_id)
                    if self.on_reconnect:
                        self.on_reconnect()

                frame = json.loads(raw)
                self._process_frame(frame, unit_id, unit_zones, prev_state)

            except zmq.Again:
                # recv timeout — check for frame timeout
                gap = time.time() - last_frame
                if not faulted and gap > self.frame_timeout:
                    faulted = True
                    log.warning("AGD %s no frames for %.1fs — fault", unit_id, gap)
                    self._zero_unit(unit, prev_state)
                    if self.on_fault:
                        self.on_fault(f"AGD {unit_id} frames stopped ({unit['ip']}:{unit['port']})")
                continue

            except Exception as e:
                log.error("AGD %s error: %s", unit_id, e)
                time.sleep(1)

        sock.close()

    def _process_frame(self, frame, unit_id, unit_zones, prev_state):
        for zone_data in frame.get('zones', []):
            zone  = zone_data.get('index')
            state = zone_data.get('state', 2)   # 1=detect 2=clear

            if zone not in unit_zones:
                continue

            detected   = 1 if state == 1 else 0
            road_users = zone_data.get('roadusers', []) if detected else []

            # Class presence from road users
            classes_present = {}
            for ru in road_users:
                cls    = ru.get('className', '').lower()
                suffix = self.classes.get(cls)
                if suffix:
                    classes_present[suffix] = 1

            # Build new state snapshot for this zone
            new_state = {'detected': detected}
            for suffix in self.classes.values():
                new_state[suffix] = classes_present.get(suffix, 0)

            old_state = prev_state.get(zone, {})

            # Only write to bus on change
            changed = False
            for key, val in new_state.items():
                if old_state.get(key) != val:
                    sig = self._sig(unit_id, zone, key)
                    if sig:
                        self.io.write(sig, val, source='agd_driver')
                    changed = True

            if changed:
                prev_state[zone] = new_state
                self._log_event(unit_id, zone, detected, classes_present)
                self._update_globals()

    def _zero_unit(self, unit, prev_state):
        """Zero all signals for a unit when it goes offline."""
        for zone in unit['zones']:
            sig = self._sig(unit['id'], zone, 'detected')
            if sig:
                self.io.write(sig, 0, source='agd_driver')
            for suffix in self.classes.values():
                sig = self._sig(unit['id'], zone, suffix)
                if sig:
                    self.io.write(sig, 0, source='agd_driver')
            prev_state.pop(zone, None)
        self._update_globals()

    def _log_event(self, unit_id, zone, detected, classes_present):
        self._events.append({
            'ts':       time.strftime('%H:%M:%S'),
            'unit':     unit_id,
            'zone':     zone,
            'detected': detected,
            'classes':  ', '.join(k.replace('has_', '') for k in classes_present) or '—',
        })

    def _read(self, unit_id, zone, sig_type):
        sig = self._sig(unit_id, zone, sig_type)
        return self.io.read(sig) if sig else 0

    def _update_globals(self):
        # any_detected
        sig = self._global_sig('any_detected')
        if sig:
            val = 1 if any(
                self._read(u['id'], z, 'detected')
                for u in self.cfg['units'] for z in u['zones']
            ) else 0
            self.io.write(sig, val, source='agd_driver')

        # any_<class>
        for cls_suffix in self.classes.values():
            global_name = f'any_{cls_suffix.replace("has_", "")}'
            sig = self._global_sig(global_name)
            if sig:
                val = 1 if any(
                    self._read(u['id'], z, cls_suffix)
                    for u in self.cfg['units'] for z in u['zones']
                ) else 0
                self.io.write(sig, val, source='agd_driver')

    def snapshot_zones(self):
        result = []
        for unit in self.cfg['units']:
            zones = []
            for z in unit['zones']:
                zd = {'id': z, 'detected': self._read(unit['id'], z, 'detected')}
                for suffix in self.classes.values():
                    zd[suffix] = self._read(unit['id'], z, suffix)
                zones.append(zd)
            result.append({
                'id':    unit['id'],
                'ip':    unit['ip'],
                'port':  unit['port'],
                'zones': zones,
            })
        return result


def start(io_bus):
    cfg_path = os.path.join(os.path.dirname(__file__), 'agd.cfg')
    if not os.path.exists(cfg_path):
        log.error("AGD config not found: %s", cfg_path)
        raise FileNotFoundError(f"Missing {cfg_path}")
    cfg    = load_agd(cfg_path)
    driver = AGDDriver(io_bus, cfg)
    driver.start()
    return driver
