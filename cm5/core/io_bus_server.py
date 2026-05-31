# core/io_bus_server.py
# Unix domain socket server — exposes IOBus read/write to external processes.
#
# Designed for MOVA integration: MOVA runs as a separate process and accesses
# CM5 detector/control signals via this socket rather than running inside CM5.
#
# Enable in platform.cfg:  iobus_socket = enabled
# Socket path:             /tmp/cm5_iobus.sock
#
# Protocol (line-based, UTF-8):
#   R <signal>\n              → <value>\n           read one signal
#   W <signal> <value>\n      → OK\n  | ERR <msg>\n write one signal
#   BATCH <sig1> <sig2>...\n  → <v1> <v2>...\n     read multiple signals
#   PING\n                    → PONG\n

import logging
import os
import socket
import threading

log = logging.getLogger('IOBUSSRV')

SOCKET_PATH = '/tmp/cm5_iobus.sock'


class IOBusServer:
    def __init__(self, io):
        self._io = io

    def start(self):
        t = threading.Thread(target=self._run, daemon=True, name='IOBusSrv')
        t.start()
        log.info("IOBus socket server started at %s", SOCKET_PATH)

    def _run(self):
        if os.path.exists(SOCKET_PATH):
            os.unlink(SOCKET_PATH)
        srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            srv.bind(SOCKET_PATH)
            srv.listen(5)
            while True:
                conn, _ = srv.accept()
                threading.Thread(
                    target=self._handle, args=(conn,), daemon=True
                ).start()
        except Exception as e:
            log.error("IOBus socket server error: %s", e)
        finally:
            srv.close()

    def _handle(self, conn):
        try:
            f = conn.makefile('r')
            for line in f:
                line = line.strip()
                if not line:
                    continue
                parts = line.split()
                cmd = parts[0].upper()

                if cmd == 'PING':
                    conn.sendall(b'PONG\n')

                elif cmd == 'R' and len(parts) == 2:
                    try:
                        val = self._io.read(parts[1])
                        conn.sendall(f'{val}\n'.encode())
                    except Exception as e:
                        conn.sendall(f'ERR {e}\n'.encode())

                elif cmd == 'W' and len(parts) == 3:
                    try:
                        self._io.write(parts[1], int(parts[2]), source='mova')
                        conn.sendall(b'OK\n')
                    except Exception as e:
                        conn.sendall(f'ERR {e}\n'.encode())

                elif cmd == 'BATCH' and len(parts) > 1:
                    try:
                        vals = [str(self._io.read(s)) for s in parts[1:]]
                        conn.sendall((' '.join(vals) + '\n').encode())
                    except Exception as e:
                        conn.sendall(f'ERR {e}\n'.encode())

                else:
                    conn.sendall(b'ERR unknown command\n')

        except Exception:
            pass
        finally:
            conn.close()
