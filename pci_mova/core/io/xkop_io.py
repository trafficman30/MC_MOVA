"""
pci_mova.core.io.xkop_io
XKOP IO layer — TCP client connecting to a PTC-1 Sim (or real TLC) server.

Protocol is identical to CM5 io/io_xkop.py:
  - 17-byte fixed packets  [CA 35][type][idx 00 val ×4][crc1 crc0]
  - Keepalive  CA35 02 ... every 6 s
  - CRC16 over bytes 2–14 (skip first 2 header bytes)
  - Values: raw byte 0–255, & 1 to test set

IO index mapping:
  Inputs  (PTC-1 → kernel):
    xkop.i.101 – xkop.i.164  → detectors 0–63
    xkop.i.1   – xkop.i.31   → confirms  0–30  (31 stage confirms)
    xkop.i.32                 → CRB
  Outputs (kernel → PTC-1):
    xkop.o.1   – xkop.o.10   → stage forces 1–10
    xkop.o.11                 → TO   (Turn-On)
    xkop.o.12                 → HI   (Hold Inhibit)
    xkop.o.13                 → SYNC
    xkop.o.14                 → FLT  (MOVA fault)
    xkop.o.101 – xkop.o.108  → special outputs 0–7
"""

import logging
import socket
import threading
import time
from typing import Optional

from pci_mova.core.io.base import AbstractIO
from pci_mova.core.model.buffers import (
    KernelBuffers, MAX_DETECTORS, MAX_CONFIRMS, MAX_FORCE,
    DOUT_HI, DOUT_TO, DOUT_SYNC, DOUT_MOVA_FAULT,
)

logger = logging.getLogger(__name__)

# ── Protocol constants (identical to CM5) ────────────────────────────────────
PACKET_LENGTH       = 17
HEADER              = bytes([0xCA, 0x35])
DATA_PACKET_TYPE    = 0x00
KEEP_ALIVE_PACKET   = bytearray.fromhex("CA35020000000000000000000000009BA0")
KEEP_ALIVE_INTERVAL = 6.0
DEAD_TIMEOUT        = KEEP_ALIVE_INTERVAL * 3   # 18 s with no data = dead

# ── CRC16 table (copied from CM5 crc_utils.py) ───────────────────────────────
_CRC_TABLE = [
    0x0000,0x0f89,0x1f12,0x109b,0x3e24,0x31ad,0x2136,0x2ebf,
    0x7c48,0x73c1,0x635a,0x6cd3,0x426c,0x4de5,0x5d7e,0x52f7,
    0xf081,0xff08,0xef93,0xe01a,0xcea5,0xc12c,0xd1b7,0xde3e,
    0x8cc9,0x8340,0x93db,0x9c52,0xb2ed,0xbd64,0xadff,0xa276,
    0xe102,0xee8b,0xfe10,0xf199,0xdf26,0xd0af,0xc034,0xcfbd,
    0x9d4a,0x92c3,0x8258,0x8dd1,0xa36e,0xace7,0xbc7c,0xb3f5,
    0x1183,0x1e0a,0x0e91,0x0118,0x2fa7,0x202e,0x30b5,0x3f3c,
    0x6dcb,0x6242,0x72d9,0x7d50,0x53ef,0x5c66,0x4cfd,0x4374,
    0xc204,0xcd8d,0xdd16,0xd29f,0xfc20,0xf3a9,0xe332,0xecbb,
    0xbe4c,0xb1c5,0xa15e,0xaed7,0x8068,0x8fe1,0x9f7a,0x90f3,
    0x3285,0x3d0c,0x2d97,0x221e,0x0ca1,0x0328,0x13b3,0x1c3a,
    0x4ecd,0x4144,0x51df,0x5e56,0x70e9,0x7f60,0x6ffb,0x6072,
    0x2306,0x2c8f,0x3c14,0x339d,0x1d22,0x12ab,0x0230,0x0db9,
    0x5f4e,0x50c7,0x405c,0x4fd5,0x616a,0x6ee3,0x7e78,0x71f1,
    0xd387,0xdc0e,0xcc95,0xc31c,0xeda3,0xe22a,0xf2b1,0xfd38,
    0xafcf,0xa046,0xb0dd,0xbf54,0x91eb,0x9e62,0x8ef9,0x8170,
    0x8408,0x8b81,0x9b1a,0x9493,0xba2c,0xb5a5,0xa53e,0xaab7,
    0xf840,0xf7c9,0xe752,0xe8db,0xc664,0xc9ed,0xd976,0xd6ff,
    0x7489,0x7b00,0x6b9b,0x6412,0x4aad,0x4524,0x55bf,0x5a36,
    0x08c1,0x0748,0x17d3,0x185a,0x36e5,0x396c,0x29f7,0x267e,
    0x650a,0x6a83,0x7a18,0x7591,0x5b2e,0x54a7,0x443c,0x4bb5,
    0x1942,0x16cb,0x0650,0x09d9,0x2766,0x28ef,0x3874,0x37fd,
    0x958b,0x9a02,0x8a99,0x8510,0xabaf,0xa426,0xb4bd,0xbb34,
    0xe9c3,0xe64a,0xf6d1,0xf958,0xd7e7,0xd86e,0xc8f5,0xc77c,
    0x460c,0x4985,0x591e,0x5697,0x7828,0x77a1,0x673a,0x68b3,
    0x3a44,0x35cd,0x2556,0x2adf,0x0460,0x0be9,0x1b72,0x14fb,
    0xb68d,0xb904,0xa99f,0xa616,0x88a9,0x8720,0x97bb,0x9832,
    0xcac5,0xc54c,0xd5d7,0xda5e,0xf4e1,0xfb68,0xebf3,0xe47a,
    0xa70e,0xa887,0xb81c,0xb795,0x992a,0x96a3,0x8638,0x89b1,
    0xdb46,0xd4cf,0xc454,0xcbdd,0xe562,0xeaeb,0xfa70,0xf5f9,
    0x578f,0x5806,0x489d,0x4714,0x69ab,0x6622,0x76b9,0x7930,
    0x2bc7,0x244e,0x34d5,0x3b5c,0x15e3,0x1a6a,0x0af1,0x0578,
]


def _crc16(data: bytearray, length: int, skip: int = 2) -> tuple:
    crc1 = crc0 = 0
    for i in range(skip, length):
        t    = (crc1 ^ data[i]) & 0xFF
        crc1 = (crc0 ^ (_CRC_TABLE[t] & 0xFF)) & 0xFF
        crc0 = (_CRC_TABLE[t] >> 8) & 0xFF
    return crc1, crc0


def _make_packet(updates: list) -> bytearray:
    """Build one 17-byte XKOP data packet from up to 4 (xkop_index, value) tuples."""
    pkt = bytearray(PACKET_LENGTH)
    pkt[0:2] = HEADER
    pkt[2]   = DATA_PACKET_TYPE
    for i in range(4):
        if i < len(updates):
            idx, val      = updates[i]
            pkt[3 + i*3] = idx & 0xFF
            pkt[4 + i*3] = 0
            pkt[5 + i*3] = val & 0xFF
        else:
            pkt[3 + i*3] = 0xFF
    crc1, crc0 = _crc16(pkt, 15, skip=2)
    pkt[15] = crc1
    pkt[16] = crc0
    return pkt


# ── IO index assignments ──────────────────────────────────────────────────────

# Inputs received from PTC-1
DET_BASE     = 101   # xkop.i.101 – i.164  → detectors 0–63
CONF_BASE    = 1     # xkop.i.1   – i.31   → confirms  0–30  (31 stage confirms)
CRB_IDX      = 32    # xkop.i.32            → CRB

# Outputs sent to PTC-1
FORCE_BASE   = 1     # xkop.o.1  – o.10   → stage forces 1–10  (index = FORCE_BASE + stage - 1)
TO_IDX       = 11    # xkop.o.11            → TO
HI_IDX       = 12    # xkop.o.12            → HI
SYNC_IDX     = 13    # xkop.o.13            → SYNC
FLT_IDX      = 14    # xkop.o.14            → FLT (MOVA fault)
SPECIAL_BASE = 101   # xkop.o.101 – o.108  → special outputs 0–7
SPECIAL_COUNT = 8


class XKOPio(AbstractIO):
    """
    XKOP IO layer — TCP client connecting to PTC-1 Sim.

    A background thread manages the TCP connection (with exponential-backoff
    reconnect), keepalive heartbeats, and packet receive.

    read_inputs()  — copies latest received state into kernel buffers (10 Hz)
    write_outputs() — detects changed outputs and sends XKOP packets immediately
    """

    def __init__(self, host: str, port: int) -> None:
        self._host = host
        self._port = port
        self._lock = threading.Lock()

        # ── Received inputs ───────────────────────────────────────────────────
        self._detectors: list = [0] * MAX_DETECTORS
        self._confirms:  list = [0] * MAX_CONFIRMS
        self._crb:       bool = False

        # ── Last-sent output state (detect changes to build packets) ──────────
        self._last_forces:     list = [-1] * MAX_FORCE
        self._last_hi:         int  = -1
        self._last_to:         int  = -1
        self._last_sync:       int  = -1
        self._last_flt:        int  = -1
        self._last_special:    list = [-1] * SPECIAL_COUNT
        self._last_det_fault:  int  = 0
        self._last_mova_fault: int  = 0

        # ── Connection state ──────────────────────────────────────────────────
        self._conn:       Optional[socket.socket] = None
        self._connected:  bool = False
        self._rx_packets: int  = 0
        self._tx_packets: int  = 0
        self._backoff:    float = 2.0
        self._fail_count: int  = 0

        # ── Thread coordination ───────────────────────────────────────────────
        self._running    = threading.Event()
        self._send_event = threading.Event()
        self._thread:    Optional[threading.Thread] = None

        self.start()

    # ── Lifecycle ─────────────────────────────────────────────────────────────

    def start(self) -> None:
        if self._thread and self._thread.is_alive():
            return
        self._running.set()
        self._thread = threading.Thread(
            target=self._connect_loop,
            name=f"xkop-io-{self._host}:{self._port}",
            daemon=True,
        )
        self._thread.start()
        logger.info("XKOPio: connecting to PTC-1 at %s:%d", self._host, self._port)

    def stop(self) -> None:
        self._running.clear()
        self._send_event.set()
        self._close_conn()
        if self._thread:
            self._thread.join(timeout=3.0)

    # ── AbstractIO implementation ─────────────────────────────────────────────

    def read_inputs(self, buffers: KernelBuffers) -> None:
        with self._lock:
            for i, v in enumerate(self._detectors):
                buffers.set_detector(i, v)
            for i, v in enumerate(self._confirms):
                buffers.set_confirm(i, v)
            buffers.crb = self._crb

    def write_outputs(self, buffers: KernelBuffers) -> None:
        """Capture output state; wake send loop if anything changed."""
        changed = False
        with self._lock:
            for stage in range(1, MAX_FORCE + 1):
                v = buffers.get_force(stage)
                if v != self._last_forces[stage - 1]:
                    self._last_forces[stage - 1] = v
                    changed = True
            for name, dout_idx, slot in (
                ('hi',   DOUT_HI,       '_last_hi'),
                ('to',   DOUT_TO,       '_last_to'),
                ('sync', DOUT_SYNC,     '_last_sync'),
                ('flt',  DOUT_MOVA_FAULT, '_last_flt'),
            ):
                v = buffers.dout[dout_idx]
                if v != getattr(self, slot):
                    setattr(self, slot, v)
                    changed = True
            for i in range(SPECIAL_COUNT):
                v = buffers.special_outputs[i] if i < len(buffers.special_outputs) else 0
                if v != self._last_special[i]:
                    self._last_special[i] = v
                    changed = True

        if changed:
            self._send_event.set()

    def snapshot(self) -> dict:
        with self._lock:
            return {
                "type":       "xkop",
                "host":       self._host,
                "port":       self._port,
                "connected":  self._connected,
                "rx_packets": self._rx_packets,
                "tx_packets": self._tx_packets,
                "fail_count": self._fail_count,
                "detectors":  list(self._detectors),
                "confirms":   list(self._confirms),
                "crb":        self._crb,
                "forces":     [max(0, v) for v in self._last_forces],
                "hi":         max(0, self._last_hi),
                "to":         max(0, self._last_to),
                "sync":       max(0, self._last_sync),
                "det_fault":  max(0, self._last_det_fault),
                "mova_fault": max(0, self._last_mova_fault),
            }

    def reset_confirms(self, stagon: int) -> None:
        """No-op — PTC-1 Sim manages confirm state via XKOP packets."""

    # ── Connection loop ───────────────────────────────────────────────────────

    def _connect_loop(self) -> None:
        while self._running.is_set():
            conn = self._try_connect()
            if conn is None:
                if self._fail_count == 0:
                    logger.warning("XKOPio: cannot reach %s:%d — retrying (backoff up to 5 min)",
                                   self._host, self._port)
                self._fail_count += 1
                self._running.wait(timeout=self._backoff)
                self._backoff = min(self._backoff * 2, 300.0)
                continue

            if self._fail_count > 0:
                logger.info("XKOPio: connected to %s:%d after %d attempt(s)",
                            self._host, self._port, self._fail_count)
            self._backoff    = 2.0
            self._fail_count = 0

            with self._lock:
                self._conn      = conn
                self._connected = True
                # Force full state push on reconnect
                self._last_forces  = [-1] * MAX_FORCE
                self._last_hi      = -1
                self._last_to      = -1
                self._last_sync    = -1
                self._last_flt     = -1
                self._last_special = [-1] * SPECIAL_COUNT

            # Send initial keepalive
            try:
                conn.sendall(KEEP_ALIVE_PACKET)
            except OSError:
                pass

            recv_thread = threading.Thread(
                target=self._recv_loop, args=(conn,), daemon=True
            )
            recv_thread.start()
            self._send_loop(conn)   # blocks until disconnected

            with self._lock:
                self._connected = False
                self._conn      = None

            try:
                conn.close()
            except OSError:
                pass

            logger.warning("XKOPio: disconnected from %s:%d — retrying", self._host, self._port)
            self._fail_count += 1
            self._running.wait(timeout=self._backoff)
            self._backoff = min(self._backoff * 2, 300.0)

    def _try_connect(self) -> Optional[socket.socket]:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        try:
            sock.connect((self._host, self._port))
            sock.setblocking(False)
            logger.info("XKOPio: TCP connected to %s:%d", self._host, self._port)
            return sock
        except OSError as exc:
            sock.close()
            logger.debug("XKOPio: connect failed: %s", exc)
            return None

    # ── Send loop ─────────────────────────────────────────────────────────────

    def _send_loop(self, conn: socket.socket) -> None:
        last_ka   = time.monotonic()
        last_recv = time.monotonic()

        while self._running.is_set():
            self._send_event.wait(timeout=KEEP_ALIVE_INTERVAL)
            self._send_event.clear()

            # Health check — nothing received for 3× keepalive = dead
            with self._lock:
                rx = self._rx_packets
            if rx == 0 and time.monotonic() - last_recv > DEAD_TIMEOUT:
                logger.warning("XKOPio: no data received for %.0fs — treating as dead",
                               time.monotonic() - last_recv)
                return

            # Build pending output updates
            pending = []
            with self._lock:
                for stage in range(1, MAX_FORCE + 1):
                    pending.append((FORCE_BASE + stage - 1, max(0, self._last_forces[stage - 1])))
                pending.append((TO_IDX,   max(0, self._last_to)))
                pending.append((HI_IDX,   max(0, self._last_hi)))
                pending.append((SYNC_IDX, max(0, self._last_sync)))
                pending.append((FLT_IDX,  max(0, self._last_flt)))
                for i in range(SPECIAL_COUNT):
                    pending.append((SPECIAL_BASE + i, max(0, self._last_special[i])))

            # Send changed values in batches of 4
            while pending:
                batch   = pending[:4]
                pending = pending[4:]
                try:
                    conn.sendall(_make_packet(batch))
                    last_ka = time.monotonic()
                    with self._lock:
                        self._tx_packets += 1
                    for idx, val in batch:
                        logger.debug("XKOPio: SEND xkop.o.%d = %d", idx, val)
                except OSError as exc:
                    logger.error("XKOPio: send error: %s", exc)
                    return

            # Keepalive
            if time.monotonic() - last_ka >= KEEP_ALIVE_INTERVAL:
                try:
                    conn.sendall(KEEP_ALIVE_PACKET)
                    last_ka = time.monotonic()
                    with self._lock:
                        self._tx_packets += 1
                except OSError as exc:
                    logger.error("XKOPio: keepalive error: %s", exc)
                    return

    # ── Receive loop ──────────────────────────────────────────────────────────

    def _recv_loop(self, conn: socket.socket) -> None:
        buf = bytearray()
        while self._running.is_set():
            try:
                data = conn.recv(4096)
                if not data:
                    break
                buf.extend(data)

                while len(buf) >= PACKET_LENGTH:
                    if buf[0:2] != HEADER:
                        # Re-sync
                        idx = buf.find(bytes(HEADER), 1)
                        buf = buf[idx:] if idx != -1 else bytearray()
                        break

                    pkt = buf[:PACKET_LENGTH]
                    buf = buf[PACKET_LENGTH:]

                    with self._lock:
                        self._rx_packets += 1

                    if pkt[2] != DATA_PACKET_TYPE:
                        continue  # keepalive or unknown — just update last_recv

                    for s in range(4):
                        xidx = pkt[3 + s*3]
                        if xidx == 0xFF:
                            continue
                        val = ((pkt[4 + s*3] << 8) | pkt[5 + s*3]) & 1

                        with self._lock:
                            if DET_BASE <= xidx < DET_BASE + MAX_DETECTORS:
                                self._detectors[xidx - DET_BASE] = val
                            elif CONF_BASE <= xidx < CONF_BASE + MAX_CONFIRMS - 1:
                                self._confirms[xidx - CONF_BASE] = val
                            elif xidx == CRB_IDX:
                                self._crb = bool(val)

                        logger.debug("XKOPio: RECV xkop.i.%d = %d", xidx, val)

            except BlockingIOError:
                time.sleep(0.02)
            except OSError as exc:
                logger.error("XKOPio: recv error: %s", exc)
                break

        self._send_event.set()  # wake send loop so it exits

    # ── Helpers ───────────────────────────────────────────────────────────────

    def _close_conn(self) -> None:
        with self._lock:
            if self._conn:
                try:
                    self._conn.close()
                except OSError:
                    pass
                self._conn = None
