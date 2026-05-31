# autodim/svc_autodim.py
# Auto Dim — astronomical dim/bright controller.
#
# Calculates today's sunrise and sunset (UTC) using astral + lat/lon,
# applies configurable minute offsets, and writes 1 (dim) or 0 (bright)
# to a single IO bus output signal every 30 s.
#
# No hysteresis — clean 0/1. The TLC handles input filtering.

import os
import datetime
import time
import logging
import threading
import configparser

from astral import LocationInfo
from astral.sun import sun

log = logging.getLogger('AUTODIM')

CFG_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'autodim.cfg')


def load_autodim(cfg_path):
    cp = configparser.ConfigParser(inline_comment_prefixes=('#',))
    cp.read(cfg_path)
    return {
        'lat':           cp.getfloat('LOCATION',  'lat',           fallback=51.5072),
        'lon':           cp.getfloat('LOCATION',  'lon',           fallback=-0.1276),
        'dim_offset':    cp.getint('SCHEDULE',    'dim_offset',    fallback=20),
        'bright_offset': cp.getint('SCHEDULE',    'bright_offset', fallback=-30),
        'signal':        cp.get('OUTPUT',         'signal',        fallback='autodim.o.dim').strip(),
        'enabled':       cp.getboolean('OUTPUT',  'enabled',       fallback=True),
    }


def save_autodim(cfg_path, cfg):
    cp = configparser.ConfigParser()
    cp['LOCATION'] = {
        'lat': str(round(cfg['lat'], 6)),
        'lon': str(round(cfg['lon'], 6)),
    }
    cp['SCHEDULE'] = {
        'dim_offset':    str(cfg['dim_offset']),
        'bright_offset': str(cfg['bright_offset']),
    }
    cp['OUTPUT'] = {
        'signal':  cfg['signal'],
        'enabled': str(cfg['enabled']).lower(),
    }
    with open(cfg_path, 'w') as f:
        f.write('# autodim/autodim.cfg — written by web UI\n\n')
        cp.write(f)


def _local_offset():
    """Return local UTC offset as timedelta, accounting for DST."""
    if time.daylight and time.localtime().tm_isdst:
        return datetime.timedelta(seconds=-time.altzone)
    return datetime.timedelta(seconds=-time.timezone)


def _fmt_local(dt_utc):
    if not dt_utc:
        return '—'
    local = (dt_utc + _local_offset()).replace(tzinfo=None)
    return local.strftime('%H:%M')


def _next_event(now_utc, sched):
    """Return (label, countdown_str) for the nearest upcoming switch."""
    dim    = sched.get('dim_utc')
    bright = sched.get('bright_utc')
    if not dim or not bright:
        return '—', '—'

    candidates = []
    if now_utc < dim:
        candidates.append(('Dim', dim))
    if now_utc < bright:
        candidates.append(('Bright', bright))

    if not candidates:
        # Both today's events are past — next is tomorrow's bright (approximate)
        candidates.append(('Bright', bright + datetime.timedelta(days=1)))

    label, t = min(candidates, key=lambda x: x[1])
    delta = max(0, int((t - now_utc).total_seconds()))
    h, rem = divmod(delta, 3600)
    m = rem // 60
    return f'{label} at {_fmt_local(t)}', f'{h}h {m:02d}m'


class AutoDimDriver:
    def __init__(self, io_bus, cfg):
        self.io       = io_bus
        self.cfg      = dict(cfg)
        self.running  = False
        self._lock    = threading.Lock()
        self._signal        = cfg['signal']
        self._sched         = {}   # populated by _recalc
        self._force_recalc  = False
        self._cfg_path = CFG_PATH

        if self._signal:
            try:
                io_bus.register(self._signal, 'autodim_driver')
                log.info("AutoDim registered signal %s", self._signal)
            except ValueError as e:
                log.warning("AutoDim signal conflict — %s — signal will not be written", e)
                self._signal = None

    def start(self):
        self.running = True
        threading.Thread(target=self._loop, daemon=True, name='AutoDim').start()
        log.info("AutoDim started lat=%.4f lon=%.4f dim+%dm bright%+dm sig=%s",
                 self.cfg['lat'], self.cfg['lon'],
                 self.cfg['dim_offset'], self.cfg['bright_offset'],
                 self._signal or '(none)')

    def stop(self):
        self.running = False

    def _recalc(self, date):
        loc     = LocationInfo(latitude=self.cfg['lat'], longitude=self.cfg['lon'])
        s       = sun(loc.observer, date=date)    # UTC-aware datetimes
        dim_utc = s['sunset']  + datetime.timedelta(minutes=self.cfg['dim_offset'])
        brt_utc = s['sunrise'] + datetime.timedelta(minutes=self.cfg['bright_offset'])
        sched = {
            'date':       date,
            'sunrise_utc': s['sunrise'],
            'sunset_utc':  s['sunset'],
            'dim_utc':     dim_utc,
            'bright_utc':  brt_utc,
        }
        with self._lock:
            self._sched = sched
        log.info("AutoDim schedule for %s: sunrise=%s sunset=%s → dim@%s bright@%s",
                 date, _fmt_local(s['sunrise']), _fmt_local(s['sunset']),
                 _fmt_local(dim_utc), _fmt_local(brt_utc))
        return sched

    def _loop(self):
        last_date = None
        sched     = {}

        while self.running:
            today = datetime.date.today()
            if today != last_date or self._force_recalc:
                self._force_recalc = False
                sched     = self._recalc(today)
                last_date = today

            if self.cfg['enabled'] and self._signal and sched:
                now    = datetime.datetime.now(datetime.timezone.utc)
                is_dim = (now >= sched['dim_utc']) or (now < sched['bright_utc'])
                self.io.write(self._signal, 1 if is_dim else 0, source='autodim_driver')

            for _ in range(30):
                if self._force_recalc:
                    break
                time.sleep(1)

    def reload_cfg(self, new_cfg):
        """Update config in-place, including hot-swap of output signal."""
        new_signal = (new_cfg.get('signal') or '').strip()

        with self._lock:
            for k in ('lat', 'lon', 'dim_offset', 'bright_offset', 'enabled'):
                self.cfg[k] = new_cfg[k]
            self.cfg['signal']   = new_signal
            self._sched         = {}
            self._force_recalc  = True   # wake _loop recalc regardless of date

        # Hot-swap signal if it changed
        old_signal = self._signal
        if new_signal != (old_signal or ''):
            if new_signal:
                try:
                    self.io.register(new_signal, 'autodim_driver')
                    if old_signal:
                        try:
                            self.io.write(old_signal, 0, source='autodim_driver')
                        except Exception:
                            pass
                        self.io.unregister(old_signal, 'autodim_driver')
                    self._signal = new_signal
                    log.info("AutoDim signal swapped: %s → %s", old_signal or '(none)', new_signal)
                except ValueError as e:
                    log.warning("AutoDim signal swap failed — %s", e)
            else:
                if old_signal:
                    try:
                        self.io.write(old_signal, 0, source='autodim_driver')
                    except Exception:
                        pass
                    self.io.unregister(old_signal, 'autodim_driver')
                self._signal = None

        log.info("AutoDim config reloaded via web")

    def snapshot(self):
        with self._lock:
            sched = dict(self._sched)
            cfg   = dict(self.cfg)

        now_utc = datetime.datetime.now(datetime.timezone.utc)
        output  = self.io.read(self._signal) if self._signal else 0
        next_lbl, next_in = _next_event(now_utc, sched)

        return {
            'lat':           cfg.get('lat', 0),
            'lon':           cfg.get('lon', 0),
            'dim_offset':    cfg.get('dim_offset', 0),
            'bright_offset': cfg.get('bright_offset', 0),
            'signal':        cfg.get('signal', ''),
            'enabled':       cfg.get('enabled', True),
            'output':        output,
            'sunrise':       _fmt_local(sched.get('sunrise_utc')),
            'sunset':        _fmt_local(sched.get('sunset_utc')),
            'dim_time':      _fmt_local(sched.get('dim_utc')),
            'bright_time':   _fmt_local(sched.get('bright_utc')),
            'next_event':    next_lbl,
            'next_event_in': next_in,
        }


def start(io_bus):
    if not os.path.exists(CFG_PATH):
        log.warning("AutoDim config not found at %s — using defaults", CFG_PATH)
    cfg    = load_autodim(CFG_PATH)
    driver = AutoDimDriver(io_bus, cfg)
    driver.start()
    return driver
