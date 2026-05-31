# flir/svc_flir.py
# FLIR camera client — supports multiple cameras via [CAMERA.N] sections in flir.cfg.
# Zone signal lookup order:
#   1. camera-qualified:  camera.N.zone.Z.type = virt.X  (handles duplicate zone numbers)
#   2. global fallback:   zone.Z.type = virt.X           (backward compat / unique zones)

import json
import time
import logging
import threading
import configparser
import os
from collections import deque

log = logging.getLogger('FLIR')

ZONE_TYPES = ['occupied', 'dilemma', 'has_pedestrian', 'has_bicycle', 'has_vehicle']


def load_flir(cfg_path):
    cp = configparser.ConfigParser(inline_comment_prefixes=('#',))
    cp.read(cfg_path)

    mapping = dict(cp.items('VIRT_MAPPING')) if cp.has_section('VIRT_MAPPING') else {}

    cameras = []
    for section in sorted(cp.sections()):
        if not section.upper().startswith('CAMERA.'):
            continue
        ip    = cp.get(section, 'ip',   fallback='127.0.0.1')
        port  = cp.getint(section, 'port', fallback=8765)
        zones = [int(x.strip()) for x in
                 cp.get(section, 'zones', fallback='1').split(',') if x.strip()]
        cameras.append({'id': section, 'ip': ip, 'port': port, 'zones': zones})

    return {'cameras': cameras, 'mapping': mapping}


class FlirDriver:
    def __init__(self, io_bus, cfg):
        self.io      = io_bus
        self.cfg     = cfg
        self.running = False
        self.mapping = cfg['mapping']
        self._events = deque(maxlen=30)
        self._all_zones = [z for cam in cfg['cameras'] for z in cam['zones']]
        self._register_signals()

    def _sig(self, camera_id, zone, signal_type):
        """Return the virt signal for a zone — camera-qualified key first, global fallback."""
        cam_key    = f'{camera_id.lower()}.zone.{zone}.{signal_type}'
        global_key = f'zone.{zone}.{signal_type}'
        return self.mapping.get(cam_key) or self.mapping.get(global_key)

    def _register_signals(self):
        registered = set()

        def _reg(sig):
            if sig and sig not in registered:
                try:
                    self.io.register(sig, 'flir_driver')
                    registered.add(sig)
                    log.info("FLIR registered %s", sig)
                except ValueError as e:
                    log.warning('%s', e)

        # Per-camera zone signals
        for cam in self.cfg['cameras']:
            for zone in cam['zones']:
                for t in ZONE_TYPES:
                    _reg(self._sig(cam['id'], zone, t))

        # Global any_* signals
        for g in ['any_occupied', 'any_dilemma', 'any_pedestrian', 'any_bicycle', 'any_vehicle']:
            _reg(self.mapping.get(g))

    def start(self):
        self.running = True
        for cam in self.cfg['cameras']:
            t = threading.Thread(
                target=self._camera_loop, args=(cam,),
                daemon=True, name=f"FLIR-{cam['id']}")
            t.start()
            log.info("FLIR camera %s starting -> ws://%s:%d  zones=%s",
                     cam['id'], cam['ip'], cam['port'], cam['zones'])

    def stop(self):
        self.running = False

    def _camera_loop(self, cam):
        import websocket
        ws_url    = f"ws://{cam['ip']}:{cam['port']}"
        cam_zones = set(cam['zones'])

        def on_open(ws):
            ws.send(json.dumps({"messageType": "Subscription",
                                "subscription": {"type": "Event", "action": "Subscribe"}}))
            log.info("FLIR %s connected", cam['id'])

        def on_message(ws, message):
            try:
                data = json.loads(message)
                if data.get("messageType") == "Event":
                    zone = int(data.get("zoneId") or data.get("zone") or 0)
                    if zone in cam_zones:
                        self._publish_event(zone, data.get("type"),
                                            data.get("state"), data.get("class"),
                                            cam['id'])
            except Exception as e:
                log.debug("FLIR %s parse error: %s", cam['id'], e)

        def on_error(ws, error):
            if "Connection refused" not in str(error):
                log.warning("FLIR %s error: %s", cam['id'], error)

        def on_close(ws, *args):
            log.warning("FLIR %s closed — reconnecting in 5s", cam['id'])

        while self.running:
            try:
                ws = websocket.WebSocketApp(ws_url,
                    on_open=on_open, on_message=on_message,
                    on_error=on_error, on_close=on_close)
                ws.run_forever(ping_interval=30, ping_timeout=10)
            except Exception as e:
                log.error("FLIR %s connection failed: %s", cam['id'], e)
            time.sleep(5)

    def _publish_event(self, zone, event_type, state, cls=None, camera=''):
        value = 1 if state == "Begin" else 0
        self._events.append({
            'ts':     time.strftime('%H:%M:%S'),
            'zone':   zone,
            'type':   event_type,
            'state':  state,
            'cls':    cls or '',
            'camera': camera,
        })

        if event_type in ("Presence", "Pedestrian"):
            sig = self._sig(camera, zone, 'occupied')
            if sig: self.io.write(sig, value, source='flir_driver')

        if event_type == "DilemmaZone":
            sig = self._sig(camera, zone, 'dilemma')
            if sig:
                self.io.write(sig, value, source='flir_driver')
                log.info("FLIR DilemmaZone zone %d [%s] -> %s = %d", zone, camera, sig, value)

        if cls:
            cls = cls.upper()
            if cls == "PEDESTRIAN":
                sig = self._sig(camera, zone, 'has_pedestrian')
                if sig: self.io.write(sig, value, source='flir_driver')
            elif cls == "BICYCLE":
                sig = self._sig(camera, zone, 'has_bicycle')
                if sig: self.io.write(sig, value, source='flir_driver')
            elif cls in ("CAR", "TRUCK", "BUS", "VAN"):
                sig = self._sig(camera, zone, 'has_vehicle')
                if sig: self.io.write(sig, value, source='flir_driver')

        self._update_globals()

    def _update_globals(self):
        checks = {
            'any_occupied':   'occupied',
            'any_dilemma':    'dilemma',
            'any_pedestrian': 'has_pedestrian',
            'any_bicycle':    'has_bicycle',
            'any_vehicle':    'has_vehicle',
        }
        for global_key, zone_type in checks.items():
            sig = self.mapping.get(global_key)
            if not sig:
                continue
            val = 1 if any(
                self.io.read(s)
                for cam in self.cfg['cameras']
                for z in cam['zones']
                for s in [self._sig(cam['id'], z, zone_type)]
                if s
            ) else 0
            self.io.write(sig, val, source='flir_driver')

    def _read(self, cam_id, zone, sig_type):
        sig = self._sig(cam_id, zone, sig_type)
        return self.io.read(sig) if sig else 0

    def snapshot_zones(self):
        """Return per-camera zone state for the web dashboard."""
        result = []
        for cam in self.cfg['cameras']:
            zones = []
            for z in cam['zones']:
                zones.append({
                    'id':             z,
                    'occupied':       self._read(cam['id'], z, 'occupied'),
                    'dilemma':        self._read(cam['id'], z, 'dilemma'),
                    'has_pedestrian': self._read(cam['id'], z, 'has_pedestrian'),
                    'has_bicycle':    self._read(cam['id'], z, 'has_bicycle'),
                    'has_vehicle':    self._read(cam['id'], z, 'has_vehicle'),
                })
            result.append({'id': cam['id'], 'ip': cam['ip'], 'port': cam['port'], 'zones': zones})
        return result


def start(io_bus):
    cfg_path = os.path.join(os.path.dirname(__file__), 'flir.cfg')
    if not os.path.exists(cfg_path):
        log.error("FLIR config not found: %s", cfg_path)
        raise FileNotFoundError(f"Missing {cfg_path}")
    cfg    = load_flir(cfg_path)
    driver = FlirDriver(io_bus, cfg)
    driver.start()
    return driver
