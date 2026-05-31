# core/config.py
# CM5 Platform — configuration parsers
# Each app has its own cfg file and loader function
# This module provides:
#   load_platform()  → platform.cfg (services + logging)
#   load_io()        → io/io.cfg
#   load_ug405()     → ug405/ug405.cfg
#   load_rtig()      → rtig/rtig.cfg
#   persist_live()   → writes [LIVE] back to ug405/ug405.cfg
#   signal registry helpers

import re
import os
from collections import defaultdict

# ── MIB column IDs ───────────────────────────────────────────────────────────

REPLY_COLS = {
    'Gn':3, 'GX':4, 'DF':5, 'FC':6, 'SCn':7, 'HC':8, 'WI':9,
    'PC':10, 'PR':11, 'CG':12, 'GR1':13, 'SDn':14, 'MC':15,
    'CF':16, 'LE':17, 'RR':18, 'LFn':19, 'RF1':20, 'RF2':21,
    'EV':22, 'VC':23, 'VQ':24, 'CA':25, 'CR':26, 'CL':27,
    'CSn':28, 'TF':29, 'VSn':30, 'VO':31, 'CO':32, 'EC':33,
    'CS':34, 'FR':35, 'BDn':36, 'TPn':37, 'SB':38, 'LC':39,
    'MR':40, 'MF':41, 'ML':42, 'GPn':25,
}

CONTROL_COLS = {
    'DX':3, 'Dn':4, 'Fn':5, 'SFn':6, 'PV':7, 'PX':8,
    'SO':9, 'SG':10, 'LO':11, 'LL':12, 'TS':13, 'FM':14,
    'TO':15, 'HI':16, 'CP':17, 'EP':18, 'GO':19, 'FF':20, 'MO':21,
}

BITMASK_REPLY   = {'Gn','SDn','LFn','CSn','VSn','BDn','TPn','GPn','SCn'}
BITMASK_CONTROL = {'Fn','Dn','SFn'}

# ── Signal name helpers ───────────────────────────────────────────────────────

def sig_raw(sig):
    """Return the bare bus signal name, stripping any leading '!' inversion marker."""
    return sig[1:] if sig.startswith('!') else sig

def sig_read(io, sig):
    """Read a mapped signal, inverting the value if the name is prefixed with '!'."""
    if sig.startswith('!'):
        return 0 if io.read(sig[1:]) else 1
    return io.read(sig)

SIGNAL_RE = re.compile(
    r'^(xkop|gp|cm5|modb)\.(i|o)\.(\d+)$'
    r'|^virt\.(\d+)$'
    r'|^rpdb\.(i|o)\.(xsg|xdet|xin|xout|stg|mode)\.\d+$'
    r'|^rpdb\.(o)\.(xout)\.\d+$'
    r'|^rpdb\.o\.\d+$'
)

def is_signal(s):
    return bool(SIGNAL_RE.match(s.strip()))

def scn_to_oid_suffix(scn):
    return f"{len(scn)}." + ".".join(str(ord(c)) for c in scn)

def _register_signal(signals, name, owner):
    if name in signals:
        existing = signals[name]
        if owner == 'conditioner_read':
            return
        if existing == 'conditioner_read':
            signals[name] = owner
            return
        if existing != owner:
            raise ValueError(
                f"[CFG] CONFLICT: signal '{name}' "
                f"claimed by '{existing}' and '{owner}'")
    else:
        signals[name] = owner

# ── Platform config ───────────────────────────────────────────────────────────

def load_platform(path):
    """Load platform.cfg — services, XKOP connection, and logging."""
    services = {
        'io':          'enabled',
        'xkop':        'enabled',
        'ug405':       'enabled',
        'rtig':        'disabled',
        'conditioner': 'disabled',
        'rpdb':        'disabled',
    }
    xkop = {'ip': '127.0.0.1', 'port': 8001, 'mode': 'client'}
    logging_cfg = {
        'level':      'INFO',
        'log_max_mb': '10',
        'log_keep':   '10',
    }

    section = None
    with open(path) as f:
        for raw in f:
            line = raw.split('#')[0].strip()
            if not line:
                continue
            if line.startswith('['):
                section = line.strip('[]').upper()
                continue
            if '=' not in line:
                continue
            k, v = [x.strip() for x in line.split('=', 1)]
            if section == 'SERVICES':
                services[k.lower()] = v.lower()
            elif section == 'XKOP':
                if k.lower() in ('ip', 'serverip'): xkop['ip']   = v
                elif k.lower() == 'port':           xkop['port']  = int(v)
                elif k.lower() == 'mode':           xkop['mode']  = v.lower()
            elif section == 'LOGGING':
                logging_cfg[k.lower()] = v

    return {'services': services, 'xkop': xkop, 'logging': logging_cfg}

# ── IO config ─────────────────────────────────────────────────────────────────

def load_io(path):
    """Load io/io.cfg — driver configurations."""
    xkop = {'ip': '127.0.0.1', 'port': 8001, 'mode': 'client'}

    section = None
    with open(path) as f:
        for raw in f:
            line = raw.split('#')[0].strip()
            if not line:
                continue
            if line.startswith('['):
                section = line.strip('[]').upper()
                continue
            if '=' not in line:
                continue
            k, v = [x.strip() for x in line.split('=', 1)]
            k = k.lower()
            if section == 'XKOP':
                if k in ('serverip', 'ip'): xkop['ip']   = v
                elif k == 'port':           xkop['port']  = int(v)
                elif k == 'mode':           xkop['mode']  = v.lower()

    return {'xkop': xkop}

# ── UG405 config ─────────────────────────────────────────────────────────────

def load_ug405(path):
    """Load ug405/ug405.cfg — SCN mappings, behaviour settings, persisted live."""
    import datetime

    ug405_services = {
        'scoot_inform_on_change_only': 'true',
        'clock_jitter_check':          'disabled',
        'clock_jitter_grace':          '60',
    }

    live = {
        'ConfigLastChanged':             None,
        'InstationAddress':              '0.0.0.0',
        'InstationPort':                 1162,
        'OperationModeTimeout':          60,
        'ScootSampleReportInterval':     4,
        'ReplyByException':              0,
        'ReplyByExceptionRetryDelay':    200,
        'ReplyByExceptionRetryCount':    4,
        'ReplyByExceptionKeepAlive':     0,
        'ReplyByExceptionResendHoldoff': 1,
    }

    scns     = []
    control  = {}
    reply    = {}
    signals  = {}

    section     = None
    current_scn = None

    with open(path) as f:
        for raw in f:
            line = raw.split('#')[0].strip()
            if not line:
                continue

            if line.startswith('['):
                section = line.strip('[]').upper()
                if section == 'SCN':
                    current_scn = None
                continue

            if '=' not in line:
                continue

            k, v = [x.strip() for x in line.split('=', 1)]

            if section == 'SERVICES':
                ug405_services[k.lower()] = v.strip().lower()
                continue

            if section == 'LIVE':
                if k == 'ConfigLastChanged':
                    live['ConfigLastChanged'] = v.strip()
                elif k in live:
                    try:
                        live[k] = int(v) if k != 'InstationAddress' else v.strip()
                    except ValueError:
                        live[k] = v.strip()
                continue

            if section == 'SCN':
                if k.lower() == 'name':
                    current_scn = v.strip()
                    if current_scn not in control:
                        scns.append(current_scn)
                        control[current_scn] = defaultdict(dict)
                        reply[current_scn]   = defaultdict(dict)
                    continue

                if current_scn is None:
                    continue

                # detect != (inverted) operator: k ends with '!' after split on '='
                inverted = k.endswith('!')
                if inverted:
                    k = k[:-1].strip()

                sig = v.strip()
                if not is_signal(sig):
                    print(f"[CFG] Invalid signal '{sig}' for {k}")
                    continue

                m = re.match(r'(utcControl|utcReply)([A-Za-z0-9]+?)(?:\[(\d+)\])?$', k)
                if not m:
                    continue

                direction = m.group(1)
                field     = m.group(2)
                bit       = int(m.group(3)) if m.group(3) else 0

                if direction == 'utcReply':
                    if field not in REPLY_COLS:
                        print(f"[CFG] Unknown reply field: {field}")
                        continue
                    stored_sig = ('!' if inverted else '') + sig
                    reply[current_scn][field][bit] = stored_sig
                    # virt.* signals are owned by conditioner — do not register here
                    if not sig.startswith('virt.'):
                        driver = sig.split('.')[0]
                        _register_signal(signals, sig, f'{driver}_driver')
                else:
                    if inverted:
                        print(f"[CFG] Warning: != not supported on control fields — treating as = for {k}")
                    if field not in CONTROL_COLS:
                        print(f"[CFG] Unknown control field: {field}")
                        continue
                    control[current_scn][field][bit] = sig
                    _register_signal(signals, sig, 'ug405')

    if live['ConfigLastChanged'] is None:
        live['ConfigLastChanged'] = datetime.datetime.utcnow().strftime('%Y%m%d%H%M%S') + 'Z'

    return {
        'services': ug405_services,
        'scns':     scns,
        'control':  control,
        'reply':    reply,
        'signals':  signals,
        'live':     live,
    }

# ── RTIG config ───────────────────────────────────────────────────────────────

def load_rtig(path):
    """Load rtig/rtig.cfg — RTIG service config and signal map."""
    rtig = {
        'port':          9010,
        'pulse_seconds': 2.0,
        'rules_file':    'rtig_rules.json',
        'signal_map':    {},
    }
    signals = {}

    section = None
    cfg_dir = os.path.dirname(path)

    with open(path) as f:
        for raw in f:
            line = raw.split('#')[0].strip()
            if not line:
                continue
            if line.startswith('['):
                section = line.strip('[]').upper()
                continue
            if '=' not in line:
                continue
            k, v = [x.strip() for x in line.split('=', 1)]
            kl = k.lower()

            if section == 'RTIG':
                if kl == 'port':            rtig['port']          = int(v)
                elif kl == 'pulse_seconds': rtig['pulse_seconds'] = float(v)
                elif kl == 'rules_file':
                    # Make relative to rtig/ dir if not absolute
                    rtig['rules_file'] = (v if os.path.isabs(v)
                                          else os.path.join(cfg_dir, v))

            elif section == 'SIGNAL_MAP':
                sig = v.strip()
                rtig['signal_map'][k] = sig
                _register_signal(signals, sig, 'rtig')

    return {'rtig': rtig, 'signals': signals}

# ── Persist live config ───────────────────────────────────────────────────────

def persist_live(live, path):
    """Write [LIVE] section back to ug405/ug405.cfg."""
    import datetime

    with open(path) as f:
        lines = f.readlines()

    # Remove existing [LIVE] section
    new_lines = []
    in_live   = False
    for line in lines:
        if line.strip().upper() == '[LIVE]':
            in_live = True
            continue
        if in_live and line.strip().startswith('['):
            in_live = False
        if not in_live:
            new_lines.append(line)

    if new_lines and not new_lines[-1].endswith('\n'):
        new_lines[-1] += '\n'

    new_lines.append('\n[LIVE]\n')
    new_lines.append(f'ConfigLastChanged             = {datetime.datetime.utcnow().strftime("%Y%m%d%H%M%S")}Z\n')
    new_lines.append(f'InstationAddress              = {live["InstationAddress"]}\n')
    new_lines.append(f'InstationPort                 = {live["InstationPort"]}\n')
    new_lines.append(f'OperationModeTimeout          = {live["OperationModeTimeout"]}\n')
    new_lines.append(f'ScootSampleReportInterval     = {live["ScootSampleReportInterval"]}\n')
    new_lines.append(f'ReplyByException              = {live["ReplyByException"]}\n')
    new_lines.append(f'ReplyByExceptionRetryDelay    = {live["ReplyByExceptionRetryDelay"]}\n')
    new_lines.append(f'ReplyByExceptionRetryCount    = {live["ReplyByExceptionRetryCount"]}\n')
    new_lines.append(f'ReplyByExceptionKeepAlive     = {live["ReplyByExceptionKeepAlive"]}\n')
    new_lines.append(f'ReplyByExceptionResendHoldoff = {live["ReplyByExceptionResendHoldoff"]}\n')

    with open(path, 'w') as f:
        f.writelines(new_lines)


if __name__ == '__main__':
    import sys
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    p = load_platform(os.path.join(root, 'platform.cfg'))
    print("Services:", p['services'])
    print("Logging:",  p['logging'])

    io = load_io(os.path.join(root, 'io', 'io.cfg'))
    print("XKOP:", io['xkop'])

    u = load_ug405(os.path.join(root, 'ug405', 'ug405.cfg'))
    print("SCNs:", u['scns'])
    print("Signals:", len(u['signals']))

    r = load_rtig(os.path.join(root, 'rtig', 'rtig.cfg'))
    print("RTIG:", r['rtig'])
    print("RTIG signals:", r['signals'])


# ── Conditioner config ────────────────────────────────────────────────────────

def load_conditioner(path):
    """
    Load conditioner/conditioner.cfg — IO logic rules.

    Output signals registered here as owned by 'conditioner'.
    Can be virt.*, xkop.o.*, gp.o.*, cm5.o.*, modb.o.*
    Input signals (xkop.i.*, gp.i.* etc) are NOT registered — they are already
    owned by their respective drivers. The conditioner subscribes to them via
    the bus without needing to register them.
    """
    import re
    rules   = []
    signals = {}   # conditioner-owned output signals

    section = None
    with open(path) as f:
        for raw in f:
            line = raw.split('#')[0].strip()
            if not line:
                continue
            if line.startswith('['):
                section = line.strip('[]').upper()
                continue
            if section != 'IO' or '=' not in line:
                continue

            writer, expr = [x.strip() for x in line.split('=', 1)]

            if not any(writer.startswith(p) for p in
                       ('virt.', 'xkop.o.', 'gp.o.', 'cm5.o.', 'modb.o.')):
                print(f"[CFG] Conditioner writer '{writer}' not a known output signal — skipping")
                continue

            rules.append({'writer': writer, 'expression': expr})

            # Only register the output virt.* signal as conditioner-owned
            # Input signals are left to their real owners
            _register_signal(signals, writer, 'conditioner')

    return {'rules': rules, 'signals': signals}



def load_rpdb(path):
    """Load io/rpdb.cfg for the RPDB driver."""
    import sys, os
    sys.path.insert(0, os.path.dirname(path))
    from io_rpdb import load_rpdb_config
    return load_rpdb_config(path)
