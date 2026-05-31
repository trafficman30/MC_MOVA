# main.py
# CM5 Platform launcher
# Loads per-app configs, registers signals, wires IOBus, starts services

import gc
import logging
import logging.handlers
import os
import signal
import sys
import threading
import time

# ── Root path — works on Linux, Windows, anywhere ────────────────────────────
ROOT = os.path.dirname(os.path.abspath(__file__))

# Add core to path so all modules can import from it
sys.path.insert(0, os.path.join(ROOT, 'core'))
sys.path.insert(0, os.path.join(ROOT, 'io'))
sys.path.insert(0, os.path.join(ROOT, 'ug405'))
sys.path.insert(0, os.path.join(ROOT, 'rtig'))
sys.path.insert(0, os.path.join(ROOT, 'conditioner'))
from config  import load_platform, load_ug405, load_rtig, load_conditioner, load_rpdb
from io_bus  import IOBus

LOG_DIR  = os.path.join(ROOT, 'logs')
LOG_FILE = os.path.join(LOG_DIR, 'cm5.log')


def setup_logging(logging_cfg):
    fmt        = '%(asctime)s [%(name)-8s] %(levelname)-5s %(message)s'
    dfmt       = '%Y-%m-%d %H:%M:%S'
    logging.Formatter.converter = time.localtime
    level_name = logging_cfg.get('level', 'INFO').upper()
    level      = getattr(logging, level_name, logging.INFO)
    max_bytes  = int(logging_cfg.get('log_max_mb', '10'))  * 1024 * 1024
    keep_files = int(logging_cfg.get('log_keep',   '10'))

    root = logging.getLogger()
    root.setLevel(logging.DEBUG)

    console = logging.StreamHandler()
    console.setLevel(level)
    console.setFormatter(logging.Formatter(fmt, datefmt=dfmt))
    root.addHandler(console)

    try:
        os.makedirs(LOG_DIR, exist_ok=True)
        fh = logging.handlers.RotatingFileHandler(
            LOG_FILE, maxBytes=max_bytes, backupCount=keep_files, encoding='utf-8')
        fh.setLevel(logging.DEBUG)
        fh.setFormatter(logging.Formatter(fmt, datefmt=dfmt))
        root.addHandler(fh)
        logging.getLogger('MAIN').info(
            "Logging to %s (console=%s, file=DEBUG, %dMB x %d = %dMB max)",
            LOG_FILE, level_name,
            max_bytes // 1024 // 1024, keep_files,
            max_bytes * keep_files // 1024 // 1024)
    except Exception as e:
        logging.getLogger('MAIN').warning("Could not open log file: %s", e)

    logging.getLogger('werkzeug').setLevel(logging.ERROR)
    logging.getLogger('urllib3').setLevel(logging.WARNING)


log = logging.getLogger('MAIN')


def _start_memory_watchdog(limit_mb):
    limit_kb = limit_mb * 1024

    def _watch():
        while True:
            time.sleep(30)
            try:
                gc.collect()
                rss_kb = 0
                with open('/proc/self/status') as f:
                    for line in f:
                        if line.startswith('VmRSS:'):
                            rss_kb = int(line.split()[1])
                            break
                if rss_kb >= limit_kb:
                    log.critical(
                        "Memory limit exceeded: RSS=%dMB limit=%dMB — terminating",
                        rss_kb // 1024, limit_mb)
                    signal.raise_signal(signal.SIGTERM)
            except Exception as e:
                log.warning("Memory watchdog error: %s", e)

    t = threading.Thread(target=_watch, daemon=True, name='MemWatchdog')
    t.start()
    log.info("Memory watchdog started: limit=%dMB", limit_mb)


def main():
    # ── Load platform config ──────────────────────────────────────────────────
    platform_cfg = load_platform(os.path.join(ROOT, 'platform.cfg'))
    setup_logging(platform_cfg['logging'])

    log.info("=" * 60)
    log.info("CM5 Platform starting  root=%s", ROOT)
    log.info("=" * 60)

    services = platform_cfg['services']
    for svc, state in services.items():
        log.info("Service  %-20s %s", svc + ':', state)

    mem_limit = int(services.get('memory_limit_mb', 300))
    _start_memory_watchdog(mem_limit)

    # ── Shared IOBus ──────────────────────────────────────────────────────────
    io = IOBus()

    # ── UG405 config ──────────────────────────────────────────────────────────
    ug405_cfg = None
    if services.get('ug405', 'enabled') == 'enabled':
        ug405_cfg = load_ug405(os.path.join(ROOT, 'ug405', 'ug405.cfg'))
        log.info("UG405 config loaded: SCNs=%s  signals=%d",
                 ug405_cfg['scns'], len(ug405_cfg['signals']))
        for name, owner in ug405_cfg['signals'].items():
            io.register(name, owner)

    # ── RTIG config ───────────────────────────────────────────────────────────
    rtig_cfg = None
    if services.get('rtig', 'disabled') == 'enabled':
        rtig_cfg = load_rtig(os.path.join(ROOT, 'rtig', 'rtig.cfg'))
        log.info("RTIG config loaded: port=%d  rules=%s",
                 rtig_cfg['rtig']['port'], rtig_cfg['rtig']['rules_file'])
        for name, owner in rtig_cfg['signals'].items():
            try:
                io.register(name, owner)
            except ValueError as e:
                log.warning("%s", e)


    # ── RPDB config ──────────────────────────────────────────────────────────
    rpdb_cfg = None
    if services.get('rpdb', 'disabled') == 'enabled':
        rpdb_path = os.path.join(ROOT, 'io', 'rpdb.cfg')
        if os.path.exists(rpdb_path):
            rpdb_cfg = load_rpdb(rpdb_path)
            log.info("RPDB config loaded: %s:%d  subs=%d",
                     rpdb_cfg['host'], rpdb_cfg['port'],
                     len(rpdb_cfg['subscriptions']))
        else:
            log.warning("RPDB enabled but rpdb.cfg not found at %s", rpdb_path)

    # ── IOBus socket server (for external MOVA process) ──────────────────────
    if services.get('iobus_socket', 'disabled') == 'enabled':
        from core.io_bus_server import IOBusServer
        IOBusServer(io_bus).start()
        log.info("IOBus socket server started")

    # ── Start FLIR driver ────────────────────────────────────────────────────
    flir_driver = None
    if services.get('flir', 'disabled') == 'enabled':
        sys.path.insert(0, os.path.join(ROOT, 'flir'))
        from svc_flir import start as start_flir
        flir_driver = start_flir(io)
        log.info("FLIR driver started")
    else:
        log.info("FLIR driver disabled")

    # ── Start AGD driver ──────────────────────────────────────────────────────
    agd_driver = None
    if services.get('agd', 'disabled') == 'enabled':
        sys.path.insert(0, os.path.join(ROOT, 'agd'))
        from svc_agd import start as start_agd
        agd_driver = start_agd(io)
        log.info("AGD driver started")
    else:
        log.info("AGD driver disabled")

    # ── Start AutoDim driver ──────────────────────────────────────────────────
    autodim_driver = None
    if services.get('autodim', 'disabled') == 'enabled':
        sys.path.insert(0, os.path.join(ROOT, 'autodim'))
        from svc_autodim import start as start_autodim
        autodim_driver = start_autodim(io)
        log.info("AutoDim driver started")
    else:
        log.info("AutoDim driver disabled")

    # ── Validate bus ──────────────────────────────────────────────────────────
    log.info("IOBus: %d signals registered", len(io._slots))
    io.dump()

    # ── Conditioner ───────────────────────────────────────────────────────────
    cond_cfg = None
    if services.get('conditioner', 'disabled') == 'enabled':
        cond_path = os.path.join(ROOT, 'conditioner', 'conditioner.cfg')
        if os.path.exists(cond_path):
            cond_cfg = load_conditioner(cond_path)
            log.info('Conditioner config loaded: %d rules', len(cond_cfg['rules']))
            for name, owner in cond_cfg['signals'].items():
                try:
                    io.register(name, owner)
                except ValueError as e:
                    log.warning('%s', e)

    # ── Start IO driver ───────────────────────────────────────────────────────
    xkop_driver = None
    if services.get('xkop', 'enabled') == 'enabled':
        from io_xkop import XKOPDriver
        xkop_cfg    = platform_cfg['xkop']
        xkop_driver = XKOPDriver(
            io   = io,
            ip   = xkop_cfg['ip'],
            port = xkop_cfg['port'],
            mode = xkop_cfg['mode'],
        )
        xkop_driver.start()
        log.info("XKOP driver started -> %s:%d (%s)",
                 xkop_cfg['ip'], xkop_cfg['port'], xkop_cfg['mode'])
    else:
        log.info("XKOP driver disabled")


    # ── Start RPDB driver ────────────────────────────────────────────────────
    rpdb_driver = None
    if rpdb_cfg:
        from io_rpdb import RPDBDriver
        rpdb_driver = RPDBDriver(io, **rpdb_cfg)
        rpdb_driver.start()
        log.info("RPDB driver started -> %s:%d", rpdb_cfg['host'], rpdb_cfg['port'])

    # ── Start conditioner ────────────────────────────────────────────────────
    if cond_cfg and cond_cfg['rules']:
        from io_conditioner import IOConditioner
        cond = IOConditioner(io, cond_cfg['rules'])
        cond.start()
        log.info('Conditioner started')

    # ── Start RTIG service ────────────────────────────────────────────────────
    if rtig_cfg:
        try:
            from svc_rtig import RTIGService
            rtig = RTIGService(io,
                               port        = rtig_cfg['rtig']['port'],
                               pulse_secs  = rtig_cfg['rtig']['pulse_seconds'],
                               rules_file  = rtig_cfg['rtig']['rules_file'],
                               signal_map  = rtig_cfg['rtig']['signal_map'])
            rtig.start()
            log.info("RTIG service started on port %d", rtig_cfg['rtig']['port'])
        except Exception as e:
            log.error("RTIG failed to start: %s", e)

    # ── Start UG405 service ───────────────────────────────────────────────────
    svc              = None
    offline_plan_svc = None

    if ug405_cfg:
        from svc_ug405 import UG405Service
        from cm5_web import push_update

        svc = UG405Service(
            io          = io,
            ug405_cfg   = ug405_cfg,
            services    = services,
            cfg_path    = os.path.join(ROOT, 'ug405', 'ug405.cfg'),
            push_update = push_update
        )

        # Only wire fault callbacks for drivers whose signals are actually used in
        # the UG405 mapping — an enabled-but-unused driver should not affect opMode.
        ug405_sigs = ug405_cfg['signals']
        if xkop_driver and any(s.startswith('xkop.') for s in ug405_sigs):
            xkop_driver.on_fault     = svc._schedule_standalone
            xkop_driver.on_reconnect = svc._cancel_fault
            log.info("XKOP fault → standalone wired")
        if rpdb_driver and any(s.startswith('rpdb.') for s in ug405_sigs):
            rpdb_driver.on_fault     = svc._schedule_standalone
            rpdb_driver.on_reconnect = svc._cancel_fault
            log.info("RPDB fault → standalone wired")

        if services.get('offline_plan', 'disabled') == 'enabled':
            sys.path.insert(0, os.path.join(ROOT, 'offline_plan'))
            from svc_offline_plan import OfflinePlanService
            plan_path = os.path.join(ROOT, 'offline_plan', 'offline_plan.json')
            offline_plan_svc = OfflinePlanService(io, ug405_cfg, plan_path, svc.op_mode)
            offline_plan_svc.start()
            log.info("Offline plan service started  file=%s", plan_path)

    # ── Start web dashboard (always, regardless of UG405) ────────────────────
    if services.get('web', 'enabled') == 'enabled':
        from cm5_web import start_web
        start_web(12007,
                  _io_bus           = io,
                  _mapping          = {'scns':    ug405_cfg['scns'],
                                       'control': ug405_cfg['control'],
                                       'reply':   ug405_cfg['reply']} if ug405_cfg else None,
                  _live_cfg         = svc.live        if svc         else None,
                  _op_mode_ref      = svc.op_mode     if svc         else None,
                  _rtig             = rtig             if rtig_cfg    else None,
                  _conditioner      = cond             if cond_cfg    else None,
                  _flir_driver      = flir_driver      if flir_driver    else None,
                  _agd_driver       = agd_driver       if agd_driver     else None,
                  _autodim_driver   = autodim_driver   if autodim_driver else None,
                  _offline_plan_drv = offline_plan_svc,
                  _mem_limit        = mem_limit,
                  _xkop             = xkop_driver,
                  _rpdb             = rpdb_driver)

    if svc:
        svc.start()   # blocks on SNMP dispatcher
    else:
        log.info("UG405 disabled — running in IO-only mode")
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            pass

    log.info("CM5 Platform stopped")


if __name__ == '__main__':
    main()
