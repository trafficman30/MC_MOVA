# ptcweb.py
# PTC-1 Web Bridge — serves PTC-1 web pages from CM5 via RPDB.
#
# Owns a dedicated RPDB connection to the PTC-1 (separate from io_rpdb.py /
# UG405).  Browser pages connect to:
#
#   GET  /ptc/stream?vars=XSG.CSC,XDET.STS   SSE stream — pushed on change
#   GET  /ptc/cfg/<PARAM>                     One-shot config fetch (cached)
#   POST /ptc/write                           SET_VALUE to PTC-1
#   GET  /ptc/pages/<path>                    Serve modified page HTML
#   GET  /ptc/status                          JSON connection status
#
# RPDB subscription behaviour (PTC-1 confirmed):
#   On-change (0x10) is NOT supported — ONTIME (0x11) with ticks=600 is used.
#   ONTIME pushes immediately on value change AND as heartbeat every 60 s.
#   This is effectively event-driven from the browser's perspective.
#
# Write isolation:
#   SET_VALUE uses a fresh one-shot connection so it never blocks the
#   subscription socket or interferes with the UG405 RPDB connection.

import configparser
import json
import logging
import os
import socket
import struct
import threading
import time
from queue import Queue, Empty

from flask import Flask, Response, jsonify, request, send_from_directory, stream_with_context

log = logging.getLogger('PTCWEB')

# ── RPDB protocol constants ────────────────────────────────────────────────────
MSG_HANDSHAKE = 0x01
MSG_LOGIN     = 0x02
MSG_CLOSE     = 0x0F
MSG_PING      = 0x03
MSG_PONG      = 0x04
MSG_MESSAGE   = 0x05

HS_ACCEPT     = 0x01
LOGIN_LOGIN   = 0x00
LOGIN_ACCEPT  = 0x01

REQ_GET_VALUE              = 0x01
REQ_GET_TEXT               = 0x02
REQ_SET_VALUE              = 0x21
REQ_SUBSCRIBE_VALUE_ONTIME = 0x11
REQ_GET_SYSTEM_TYPE        = 0x61
REQ_GET_SYSTEM_STRING      = 0x62

RESP_SUBSCRIPTION_ACCEPTED = 0x01
RESP_SET_OK                = 0x03
RESP_SYSTEM_TYPE           = 0x11
RESP_SYSTEM_STRING         = 0x12
RESP_VALUE                 = 0x14
RESP_TEXT                  = 0x15
RESP_ERROR_WRONG_URI       = 0x72
RESP_ERROR_NOT_ALLOWED     = 0x75

PING_INTERVAL   = 55.0
RECONNECT_DELAY = 5.0
RECV_TIMEOUT    = 5.0

# How many ticks between heartbeat pushes (1 tick = 100ms on PTC-1)
# 600 = change-driven push + 60s keepalive heartbeat
SUBSCRIBE_TICKS = 600

SYSTEM_TYPES = {
    0x00: 'EC1_PPC_VXWORKS', 0x01: 'EC2',  0x02: 'EC1_PPC_LINUX',
    0x03: 'PTC1',            0x10: 'SIM_EC1', 0x11: 'SIM_EC2',
    0x13: 'SIM_PTC1',        0x80: 'OSGI', 0x81: 'TFT', 0x82: 'WEBSERVER',
}


# ── RPDB wire framing ──────────────────────────────────────────────────────────

def _build(msg_type, payload=b''):
    total = 4 + len(payload)
    return struct.pack('>BBH', 0x00, msg_type, total) + payload


def _recv_exact(sock, n):
    buf = b''
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise ConnectionError('Connection closed by remote')
        buf += chunk
    return buf


def _recv_msg(sock):
    header = _recv_exact(sock, 4)
    _, msg_type, length = struct.unpack('>BBH', header)
    payload = _recv_exact(sock, length - 4) if length > 4 else b''
    return msg_type, payload


def _build_request(command, uri='', data=b''):
    uri_b       = (uri.encode('ascii') + b'\x00') if uri else b'\x00'
    data_offset = 16 + len(uri_b) if data else 0
    app_hdr     = struct.pack('>BBH', 0x00, command, data_offset)
    padding     = b'\x00' * 12                         # 12 reserved bytes
    return _build(MSG_MESSAGE, app_hdr + padding + uri_b + data)


def _parse_response(payload):
    if len(payload) < 16:
        return None, '', None
    _, command, data_offset = struct.unpack('>BBH', payload[0:4])
    if data_offset > 0:
        uri      = payload[16:data_offset].rstrip(b'\x00').decode('ascii', errors='replace')
        raw_data = payload[data_offset:]
    else:
        uri      = payload[16:].rstrip(b'\x00').decode('ascii', errors='replace')
        raw_data = b''
    if command == RESP_VALUE and raw_data:
        n = len(raw_data) // 4
        return command, uri, list(struct.unpack(f'>{n}i', raw_data[:n * 4]))
    if command == RESP_TEXT and raw_data:
        lines = raw_data.rstrip(b'\x00').decode('ascii', errors='replace').splitlines()
        return command, uri, lines
    return command, uri, None


# ── Session helpers ────────────────────────────────────────────────────────────

def _do_handshake(sock):
    mt, pl = _recv_msg(sock)
    if mt != MSG_HANDSHAKE or len(pl) < 4:
        raise ConnectionError('Bad handshake')
    _, _, major, minor = struct.unpack('>BBBB', pl[:4])
    sock.sendall(_build(MSG_HANDSHAKE, struct.pack('>BBBB', 0x00, HS_ACCEPT, major, minor)))
    return major, minor


def _do_login(sock, username, password):
    payload = bytes([LOGIN_LOGIN]) + username.encode() + b'\x00' + password.encode() + b'\x00'
    sock.sendall(_build(MSG_LOGIN, payload))
    mt, pl = _recv_msg(sock)
    if mt != MSG_LOGIN or not pl or pl[0] != LOGIN_ACCEPT:
        raise PermissionError(f"Login rejected for '{username}'")


def _fresh_connection(host, port, username, password, timeout=5.0):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    sock.connect((host, port))
    _do_handshake(sock)
    _do_login(sock, username, password)
    return sock


# ── One-shot helpers ───────────────────────────────────────────────────────────

def _one_shot_get_value(host, port, username, password, param):
    """Return list of int values for param, or None on error."""
    uri = f'pardb:///{param.upper()}'
    try:
        sock = _fresh_connection(host, port, username, password)
        sock.sendall(_build_request(REQ_GET_VALUE, uri))
        sock.settimeout(5.0)
        mt, pl = _recv_msg(sock)
        cmd, _, data = _parse_response(pl)
        sock.sendall(_build(MSG_CLOSE))
        sock.close()
        if cmd == RESP_VALUE and data is not None:
            return data
    except Exception as e:
        log.warning('GET_VALUE %s failed: %s', param, e)
    return None


def _one_shot_get_text(host, port, username, password, param):
    """Return list of text lines for param, or None on error."""
    uri = f'pardb:///{param.upper()}'
    try:
        sock = _fresh_connection(host, port, username, password)
        sock.sendall(_build_request(REQ_GET_TEXT, uri))
        sock.settimeout(5.0)
        mt, pl = _recv_msg(sock)
        cmd, _, data = _parse_response(pl)
        sock.sendall(_build(MSG_CLOSE))
        sock.close()
        if cmd == RESP_TEXT and data is not None:
            return data
    except Exception as e:
        log.warning('GET_TEXT %s failed: %s', param, e)
    return None


def _one_shot_set_value(host, port, username, password, param, index, value):
    """SET_VALUE on a fresh connection. Returns True on success."""
    uri = f'pardb:///{param.upper()}?id={index}'
    data = struct.pack('>i', int(value))
    try:
        sock = _fresh_connection(host, port, username, password)
        sock.sendall(_build_request(REQ_SET_VALUE, uri, data))
        sock.settimeout(3.0)
        mt, pl = _recv_msg(sock)
        _, _, _ = _parse_response(pl)
        sock.sendall(_build(MSG_CLOSE))
        sock.close()
        # Any response counts as success (controller accepted the connection)
        return True
    except Exception as e:
        log.error('SET_VALUE %s[%d]=%d failed: %s', param, index, value, e)
        return False


# ── PtcWebBridge ───────────────────────────────────────────────────────────────

class PtcWebBridge:
    """
    Owns one persistent RPDB subscription connection to the PTC-1.

    Pages request which XLIB params they need; the bridge subscribes on first
    request and distributes pushed values to SSE client queues.

    SET_VALUE and GET_VALUE/TEXT use fresh one-shot connections so they never
    block or interfere with the subscription stream.
    """

    def __init__(self, host, port, username, password, write_password):
        self.host           = host
        self.port           = port
        self.username       = username
        self.password       = password
        self.write_password = write_password

        self.connected = False
        self._running  = False

        # Params already subscribed on the persistent connection
        self._subscribed: set = set()       # upper-case param names

        # Latest known values: param → list[int]
        self._cache: dict = {}

        # Active SSE clients: list of dicts
        self._clients: list = []

        self._lock = threading.Lock()
        self._sock = None

        # Queue for subscribe requests arriving while connected
        # Items: param name (str)
        self._sub_queue: Queue = Queue()

        # Cache for config fetches (text or value lists)
        self._cfg_cache: dict = {}

    # ── Lifecycle ──────────────────────────────────────────────────────────────

    def start(self):
        self._running = True
        threading.Thread(target=self._connect_loop, daemon=True, name='PtcWebRPDB').start()
        log.info('PtcWebBridge starting → %s:%d', self.host, self.port)

    def stop(self):
        self._running = False
        if self._sock:
            try:
                self._sock.close()
            except Exception:
                pass

    def status(self):
        with self._lock:
            return {
                'connected':   self.connected,
                'subscribed':  sorted(self._subscribed),
                'clients':     len(self._clients),
                'cache_keys':  sorted(self._cache.keys()),
            }

    # ── SSE client registration ────────────────────────────────────────────────

    def register_client(self, params: list):
        """
        Register a new SSE client interested in the given XLIB param names.
        Subscribes any params not yet subscribed on the RPDB connection.
        Returns a per-client Queue for the SSE generator to read from.
        """
        params_upper = [p.strip().upper() for p in params if p.strip()]
        q = Queue()
        client = {'queue': q, 'params': set(params_upper)}

        with self._lock:
            self._clients.append(client)
            new_params = [p for p in params_upper if p not in self._subscribed]

        # Request subscriptions for new params
        for p in new_params:
            self._sub_queue.put(p)

        # Send initial snapshot from cache for params we already have
        with self._lock:
            snapshot = {p: self._cache[p] for p in params_upper if p in self._cache}
        if snapshot:
            q.put(snapshot)

        return client

    def unregister_client(self, client):
        with self._lock:
            try:
                self._clients.remove(client)
            except ValueError:
                pass

    # ── Config / write API ─────────────────────────────────────────────────────

    def get_cfg_values(self, param):
        """Return cached int list for param, fetching from PTC-1 if needed."""
        key = ('v', param.upper())
        if key not in self._cfg_cache:
            data = _one_shot_get_value(
                self.host, self.port, self.username, self.password, param)
            if data is not None:
                self._cfg_cache[key] = data
        return self._cfg_cache.get(key)

    def get_cfg_text(self, param):
        """Return cached text lines for param, fetching from PTC-1 if needed."""
        key = ('t', param.upper())
        if key not in self._cfg_cache:
            data = _one_shot_get_text(
                self.host, self.port, self.username, self.password, param)
            if data is not None:
                self._cfg_cache[key] = data
        return self._cfg_cache.get(key)

    def set_value(self, param, index, value):
        """Write a value to PTC-1 on a fresh connection. Non-blocking."""
        threading.Thread(
            target=_one_shot_set_value,
            args=(self.host, self.port, self.username, self.write_password,
                  param, index, value),
            daemon=True,
        ).start()

    # ── Internal: push to SSE clients ─────────────────────────────────────────

    def _push(self, param, values):
        """Called from recv loop — update cache and notify interested clients."""
        with self._lock:
            self._cache[param] = values
            for client in self._clients:
                if param in client['params']:
                    client['queue'].put({param: values})

    # ── Connection loop ────────────────────────────────────────────────────────

    def _connect_loop(self):
        while self._running:
            sock = self._try_connect()
            if not sock:
                time.sleep(RECONNECT_DELAY)
                continue

            self._sock = sock
            self.connected = True

            recv_t = threading.Thread(
                target=self._recv_loop, args=(sock,), daemon=True, name='PtcWebRecv')
            recv_t.start()

            self._sub_pump(sock)    # blocks until socket dies or stop()

            self.connected = False
            self._sock = None
            try:
                sock.close()
            except Exception:
                pass
            log.warning('PtcWebBridge disconnected — retry in %ds', RECONNECT_DELAY)
            time.sleep(RECONNECT_DELAY)

    def _try_connect(self):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5.0)
            sock.connect((self.host, self.port))
            major, minor = _do_handshake(sock)
            _do_login(sock, self.username, self.password)
            sock.settimeout(RECV_TIMEOUT)

            # Log system info
            for req_cmd, resp_cmd, label in [
                (REQ_GET_SYSTEM_TYPE,   RESP_SYSTEM_TYPE,   'type'),
                (REQ_GET_SYSTEM_STRING, RESP_SYSTEM_STRING, 'name'),
            ]:
                sock.sendall(_build_request(req_cmd))
                sock.settimeout(3.0)
                try:
                    mt, pl = _recv_msg(sock)
                    cmd, _, data = _parse_response(pl)
                    if label == 'type' and cmd == resp_cmd and data:
                        t = data[0]
                        log.info('PTC-1 system type: %s', SYSTEM_TYPES.get(t, f'0x{t:02X}'))
                    elif label == 'name' and cmd == resp_cmd and data:
                        log.info('PTC-1 system name: %s', data[0] if isinstance(data, list) else data)
                except socket.timeout:
                    pass
                finally:
                    sock.settimeout(RECV_TIMEOUT)

            # Re-subscribe anything we already knew about (reconnect path)
            with self._lock:
                already = list(self._subscribed)
            for param in already:
                self._send_subscribe(sock, param)

            log.info('PtcWebBridge connected to %s:%d (v%d.%d)',
                     self.host, self.port, major, minor)
            return sock

        except Exception as e:
            log.warning('PtcWebBridge connect failed: %s', e)
            try:
                sock.close()
            except Exception:
                pass
            return None

    # ── Sub pump (runs in connect_loop thread) ─────────────────────────────────

    def _sub_pump(self, sock):
        """
        Drain the subscribe queue and send PING keepalives.
        Runs in connect_loop thread while socket is live.
        Exits when socket dies or stop() is called.
        """
        last_ping = time.monotonic()
        while self._running and sock:
            # Process any pending subscribe requests
            try:
                while True:
                    param = self._sub_queue.get_nowait()
                    param = param.upper()
                    with self._lock:
                        already = param in self._subscribed
                    if not already:
                        if self._send_subscribe(sock, param):
                            with self._lock:
                                self._subscribed.add(param)
                            # Fetch initial value into cache
                            vals = _one_shot_get_value(
                                self.host, self.port,
                                self.username, self.password, param)
                            if vals is not None:
                                self._push(param, vals)
            except Empty:
                pass

            # Keepalive ping
            if time.monotonic() - last_ping >= PING_INTERVAL:
                try:
                    sock.sendall(_build(MSG_PING))
                    last_ping = time.monotonic()
                except Exception:
                    break

            time.sleep(0.2)

    def _send_subscribe(self, sock, param):
        """Send a SUBSCRIBE_VALUE_ONTIME for param. Returns True on send success."""
        uri = f'pardb:///{param.upper()}'
        data = struct.pack('>I', SUBSCRIBE_TICKS)
        try:
            sock.sendall(_build_request(REQ_SUBSCRIBE_VALUE_ONTIME, uri, data))
            log.info('Subscribed: %s (ticks=%d)', param, SUBSCRIBE_TICKS)
            return True
        except Exception as e:
            log.warning('Subscribe %s failed: %s', param, e)
            return False

    # ── Receive loop ──────────────────────────────────────────────────────────

    def _recv_loop(self, sock):
        """Receive RPDB pushes and distribute to SSE clients."""
        while self._running and sock:
            try:
                mt, payload = _recv_msg(sock)
            except socket.timeout:
                continue
            except Exception as e:
                log.error('RPDB recv error: %s', e)
                break

            if mt == MSG_PING:
                try:
                    sock.sendall(_build(MSG_PONG))
                except Exception:
                    break
                continue
            if mt in (MSG_PONG, MSG_CLOSE):
                if mt == MSG_CLOSE:
                    log.info('PTC-1 sent CLOSE')
                    break
                continue
            if mt != MSG_MESSAGE:
                continue

            cmd, uri, data = _parse_response(payload)

            log.info('RPDB MSG: cmd=0x%02x uri=%r data_len=%s', 
               cmd or 0, uri, len(data) if data else 0)

            if cmd == RESP_SUBSCRIPTION_ACCEPTED:
                log.debug('Subscription confirmed: %s', uri)
                continue
            if cmd in (RESP_ERROR_WRONG_URI, RESP_ERROR_NOT_ALLOWED):
                log.warning('Subscription error 0x%02X for %s', cmd, uri)
                continue
            if cmd != RESP_VALUE or not data:
                continue

            # Extract bare param name from URI  e.g. 'pardb:///XSG.CSC' → 'XSG.CSC'
            param = uri.replace('pardb:///', '').split('?')[0].strip().upper()
            if param:
                self._push(param, data)
                log.debug('Push: %s = %s', param, data[:4])


# ── Flask app factory ──────────────────────────────────────────────────────────

def create_app(bridge: PtcWebBridge, pages_dir: str) -> Flask:
    app = Flask(__name__)

    # ── SSE stream ─────────────────────────────────────────────────────────────

    @app.route('/ptc/stream')
    def ptc_stream():
        raw = request.args.get('vars', '')
        params = [p.strip().upper() for p in raw.split(',') if p.strip()]
        if not params:
            return 'vars= required', 400

        client = bridge.register_client(params)

        @stream_with_context
        def generate():
            try:
                while True:
                    try:
                        data = client['queue'].get(timeout=30)
                        yield f'data: {json.dumps(data)}\n\n'
                    except Empty:
                        # SSE comment heartbeat — keeps the connection alive
                        # and lets the browser detect disconnects
                        yield ': heartbeat\n\n'
            finally:
                bridge.unregister_client(client)

        return Response(
            generate(),
            mimetype='text/event-stream',
            headers={
                'Cache-Control':   'no-cache',
                'X-Accel-Buffering': 'no',  # Nginx: disable proxy buffering
            },
        )

    # ── Config endpoints ───────────────────────────────────────────────────────

    @app.route('/ptc/cfg/<param>')
    def ptc_cfg(param):
        """
        Fetch a config parameter from PTC-1 (cached after first fetch).
        Returns newline-separated values, same format as PTC-1 /part/ and /parv/.

        For text params (names, identifiers): ?type=text
        For numeric params: ?type=value (default)
        """
        ptype = request.args.get('type', 'value')
        if ptype == 'text':
            data = bridge.get_cfg_text(param)
            if data is None:
                return f'Could not fetch {param}', 502
            return '\n'.join(str(v) for v in data), 200, {'Content-Type': 'text/plain'}
        else:
            data = bridge.get_cfg_values(param)
            if data is None:
                return f'Could not fetch {param}', 502
            return '\n'.join(str(v) for v in data), 200, {'Content-Type': 'text/plain'}

    # ── Write endpoint ─────────────────────────────────────────────────────────

    @app.route('/ptc/write', methods=['POST'])
    def ptc_write():
        """
        Write a value to the PTC-1.
        Body: {"param": "XUKMP.BBO", "index": 5, "value": 1}
        """
        body = request.get_json(silent=True)
        if not body:
            return 'JSON body required', 400
        param = body.get('param', '').upper()
        index = body.get('index')
        value = body.get('value')
        if not param or index is None or value is None:
            return 'param, index and value required', 400
        try:
            index = int(index)
            value = int(value)
        except (TypeError, ValueError):
            return 'index and value must be integers', 400

        bridge.set_value(param, index, value)
        log.info('WEB WRITE %s[%d] = %d', param, index, value)
        return jsonify({'ok': True, 'param': param, 'index': index, 'value': value})

    # ── Page serving ───────────────────────────────────────────────────────────

    @app.route('/ptc/pages/')
    @app.route('/ptc/pages/<path:filename>')
    def ptc_pages(filename=''):
        """Serve page files from the pages_dir directory."""
        if not filename:
            return 'Page path required', 400
        directory = os.path.join(pages_dir, os.path.dirname(filename))
        basename  = os.path.basename(filename)
        if not basename:
            return 'Page path required', 400
        try:
            return send_from_directory(directory, basename)
        except Exception:
            return f'Page not found: {filename}', 404

    # ── Status ─────────────────────────────────────────────────────────────────

    @app.route('/ptc/status')
    def ptc_status():
        return jsonify(bridge.status())

    return app


# ── Config loader ──────────────────────────────────────────────────────────────

def load_config(path='/opt/CM5/ptcweb/ptcweb.cfg'):
    cp = configparser.ConfigParser(strict=False)
    cp.read(path)

    rpdb = dict(cp['RPDB']) if cp.has_section('RPDB') else {}
    web  = dict(cp['WEB'])  if cp.has_section('WEB')  else {}

    return {
        'host':           rpdb.get('host',           '192.168.71.97'),
        'port':           int(rpdb.get('port',        '35340')),
        'username':       rpdb.get('username',        'user'),
        'password':       rpdb.get('password',        'Peek'),
        'write_password': rpdb.get('write_password',  '3145'),
        'web_host':       web.get('host',             '0.0.0.0'),
        'web_port':       int(web.get('port',         '9020')),
        'pages_dir':      web.get('pages_dir',        '/opt/CM5/ptcweb/pages'),
        'debug':          web.get('debug',            'false').lower() == 'true',
    }


# ── Entry point ────────────────────────────────────────────────────────────────

if __name__ == '__main__':
    import sys

    logging.basicConfig(
        level=logging.DEBUG,
        format='%(asctime)s %(levelname)-7s %(name)s  %(message)s',
        datefmt='%H:%M:%S',
    )

    cfg_path = sys.argv[1] if len(sys.argv) > 1 else '/opt/CM5/ptcweb/ptcweb.cfg'
    cfg = load_config(cfg_path)

    bridge = PtcWebBridge(
        host           = cfg['host'],
        port           = cfg['port'],
        username       = cfg['username'],
        password       = cfg['password'],
        write_password = cfg['write_password'],
    )
    bridge.start()

    app = create_app(bridge, cfg['pages_dir'])

    log.info('PtcWeb listening on %s:%d', cfg['web_host'], cfg['web_port'])
    log.info('Pages directory: %s', cfg['pages_dir'])

    app.run(
        host    = cfg['web_host'],
        port    = cfg['web_port'],
        debug   = cfg['debug'],
        threaded= True,
        use_reloader=False,
    )
