# io_xkop.py
# XKOP IO driver.
# Connects to (or accepts from) a TLC using the XKOP protocol.
# Maps xkop.i.* signals from TCP receive → io bus
# Maps xkop.o.* signals from io bus → TCP send
# Event-driven send — wakes immediately on bus change.

import socket
import time
import threading

import logging
from crc_utils import write_crc16
from io_bus import IOBus, resolve

log = logging.getLogger('XKOP')

PACKET_LENGTH       = 17
HEADER              = bytes([0xCA, 0x35])
DATA_PACKET_TYPE    = 0x00
KEEP_ALIVE_PACKET   = bytearray.fromhex("CA35020000000000000000000000009BA0")
KEEP_ALIVE_INTERVAL = 6.0

# XKOP physical index range
XKOP_MAX = 255


def _make_packet(updates):
    """Build a 17-byte XKOP data packet from up to 4 (signal_name, value) tuples."""
    packet = bytearray(PACKET_LENGTH)
    packet[0:2] = HEADER
    packet[2]   = DATA_PACKET_TYPE
    for i in range(4):
        if i < len(updates):
            name, val = updates[i]
            # Extract physical XKOP index from signal name e.g. xkop.o.101 → 101
            xkop_idx = int(name.split('.')[-1])
            packet[3 + i*3] = xkop_idx
            packet[4 + i*3] = 0
            packet[5 + i*3] = val & 0xFF
        else:
            packet[3 + i*3] = 0xFF
    write_crc16(packet, length=15, skip_bytes=2)
    return packet


class XKOPDriver:
    """
    XKOP IO driver.

    Parameters
    ----------
    io   : IOBus  — shared state bus
    ip   : str    — remote IP (client) or bind IP (server)
    port : int    — TCP port
    mode : str    — 'client' or 'server'
    """

    def __init__(self, io, ip='127.0.0.1', port=8001, mode='client'):
        self.io        = io
        self.ip        = ip
        self.port      = port
        self.mode      = mode

        self.conn         = None
        self.running      = False
        self.connected    = False
        self.on_fault     = None   # set by main.py: called when an established connection drops
        self.on_reconnect = None   # set by main.py: called when connection is (re)established

        # Track last sent values by signal name
        self._last_sent   = {}
        self._last_ka     = 0.0
        self._last_recv   = 0.0
        self._send_event  = threading.Event()
        self._retry_event = threading.Event()
        self._backoff     = 2.0
        self._fail_count  = 0

        # Collect output signal names from bus registry
        self._output_signals = []   # populated in start()

    # ── Public interface ──────────────────────────────────────────────────────

    def start(self):
        # Find all xkop.o.* signals registered in bus
        self._output_signals = [
            name for name, owner in self.io.registered_signals()
            if name.startswith('xkop.o.')
        ]
        # Initialise last_sent
        self._last_sent = {name: -1 for name in self._output_signals}

        # Subscribe to bus changes to wake send loop
        self.io.subscribe(self._on_bus_change)

        self.running = True
        threading.Thread(target=self._connect_loop, daemon=True).start()
        log.info("Driver starting (%s) → %s:%d", self.mode, self.ip, self.port)
        log.info("Monitoring %d output signals", len(self._output_signals))

    def stop(self):
        self.running = False
        if self.conn:
            try: self.conn.close()
            except: pass
        log.info("Driver stopped")

    def status(self):
        return {
            'driver':     'xkop',
            'mode':       self.mode,
            'ip':         self.ip,
            'port':       self.port,
            'connected':  self.connected,
            'backoff':    int(self._backoff),
            'fail_count': self._fail_count,
        }

    def resume(self):
        """Reset backoff and immediately retry connection."""
        self._backoff    = 2.0
        self._fail_count = 0
        self._retry_event.set()
        log.info("Manual reconnect requested -> %s:%d", self.ip, self.port)

    # ── Connection loop ───────────────────────────────────────────────────────

    def _connect_loop(self):
        while self.running:
            self.conn = self._connect()
            if not self.conn:
                if self._fail_count == 0:
                    log.warning("Cannot connect to %s:%d — retrying with backoff (max 5 min)",
                                self.ip, self.port)
                self._fail_count += 1
                self._retry_event.clear()
                self._retry_event.wait(timeout=self._backoff)
                self._backoff = min(self._backoff * 2, 300.0)
                continue

            if self._fail_count > 0:
                log.info("Connected to %s:%d after %d attempt(s)",
                         self.ip, self.port, self._fail_count)
            self._backoff    = 2.0
            self._fail_count = 0

            self.connected  = True
            self._last_recv = time.time()   # reset so health check doesn't fire immediately
            # Zero all inputs so stale values don't persist if TLC restarted.
            # TLC will push current state as events arrive.
            self.io.zero_owned_by('xkop_driver')
            if self.on_reconnect:
                self.on_reconnect()
            # Reset to force full state push on connect
            self._last_sent = {name: -1 for name in self._output_signals}
            self._last_ka   = time.time()

            try:
                self.conn.send(KEEP_ALIVE_PACKET)
                self._last_ka = time.time()
            except: pass

            recv_thread = threading.Thread(target=self._recv_loop, daemon=True)
            recv_thread.start()

            self._send_loop()   # blocks until disconnect

            self.connected = False
            if self.conn:
                try: self.conn.close()
                except: pass
            self.conn = None
            log.warning("Disconnected from %s:%d — retrying", self.ip, self.port)
            self._fail_count += 1
            if self.on_fault:
                self.on_fault(f"XKOP lost connection to {self.ip}:{self.port}")
            self._retry_event.clear()
            self._retry_event.wait(timeout=self._backoff)
            self._backoff = min(self._backoff * 2, 300.0)

    def _connect(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        try:
            if self.mode == 'client':
                sock.connect((self.ip, self.port))
                sock.setblocking(False)
                log.info("Connected to %s:%d", self.ip, self.port)
                return sock
            else:
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                sock.bind((self.ip, self.port))
                sock.listen(1)
                log.info("Waiting for connection on %s:%d", self.ip, self.port)
                conn, addr = sock.accept()
                sock.close()
                conn.setblocking(False)
                log.info("Accepted connection from %s", addr)
                return conn
        except Exception as e:
            sock.close()
            log.debug("Connect attempt failed: %s", e)
            return None

    # ── Send loop ─────────────────────────────────────────────────────────────

    def _send_loop(self):
        """Event-driven — wakes on bus change or keepalive timeout."""
        while self.running and self.conn:
            self._send_event.wait(timeout=KEEP_ALIVE_INTERVAL)
            self._send_event.clear()

            if not self.conn:
                break

            # Find changed output signals — include any dynamically added ones
            pending = []
            all_outputs = set(self._output_signals) | set(self._last_sent.keys())
            for name in all_outputs:
                val = self.io.read(name)
                if val != self._last_sent.get(name, -1):
                    pending.append((name, val))
                    self._last_sent[name] = val

            # Send in batches of 4
            while pending and self.conn:
                batch   = pending[:4]
                pending = pending[4:]
                try:
                    self.conn.send(_make_packet(batch))
                    self._last_ka = time.time()
                    for name, val in batch:
                        log.debug("SEND %s = %d", name, val)
                except Exception as e:
                    log.error("Send error: %s", e)
                    return

            # Keepalive
            if time.time() - self._last_ka >= KEEP_ALIVE_INTERVAL:
                try:
                    self.conn.send(KEEP_ALIVE_PACKET)
                    self._last_ka = time.time()
                except Exception as e:
                    log.error("Keepalive error: %s", e)
                    return

            # Receive-side health check — if nothing heard for 3× keepalive
            # interval the connection is silently dead (network cut, server crash).
            if (self._last_recv > 0 and
                    time.time() - self._last_recv > KEEP_ALIVE_INTERVAL * 3):
                log.warning("XKOP no data received for %.0fs — treating as dead",
                            time.time() - self._last_recv)
                return

    def _on_bus_change(self, name, value, source):
        """Wake send loop when an xkop.o.* signal changes."""
        if name.startswith('xkop.o.'):
            # Add to monitored outputs if not already tracked
            if name not in self._last_sent:
                self._last_sent[name] = -1
            self._send_event.set()

    # ── Receive loop ──────────────────────────────────────────────────────────

    def _recv_loop(self):
        """Receive XKOP packets → write to xkop.i.* signals on bus."""
        buf = bytearray()
        while self.running and self.conn:
            try:
                data = self.conn.recv(4096)
                if not data:
                    break
                self._last_recv = time.time()
                buf.extend(data)

                while len(buf) >= PACKET_LENGTH:
                    pkt = buf[:PACKET_LENGTH]
                    buf = buf[PACKET_LENGTH:]

                    if pkt[:2] != HEADER:
                        buf = buf[1:]
                        continue

                    if pkt[2] == DATA_PACKET_TYPE:
                        for s in range(4):
                            xkop_idx = pkt[3 + s*3]
                            if xkop_idx == 0xFF:
                                continue
                            val  = (pkt[4 + s*3] << 8) | pkt[5 + s*3]
                            name = f"xkop.i.{xkop_idx}"
                            # Only write if signal is registered
                            try:
                                self.io.write(name, val, source='xkop_driver')
                                if val != 0:
                                    log.debug("XKOP RECV %s = %s", name, val)
                            except:
                                pass

            except BlockingIOError:
                time.sleep(0.02)
            except Exception as e:
                log.error("Recv error: %s", e)
                break

        # Wake _send_loop so it exits promptly rather than waiting for next keepalive
        self._send_event.set()


if __name__ == '__main__':
    from io_bus import IOBus
    from config import load_config

    cfg = load_config('/opt/ug405/ug405.cfg')
    io  = IOBus()

    # Register all signals
    for name, owner in cfg['signals'].items():
        io.register(name, owner)

    def on_change(name, val, src):
        print(f"[BUS] {name} = {val} ({src})")

    io.subscribe(on_change)

    driver = XKOPDriver(io,
                        ip   = cfg['xkop']['ip'],
                        port = cfg['xkop']['port'],
                        mode = cfg['xkop']['mode'])
    driver.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        driver.stop()