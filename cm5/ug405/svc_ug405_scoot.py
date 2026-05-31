# svc_ug405_scoot.py
# SCOOT occupancy sampler for UG405 service.
#
# Samples xkop.i.* VSn signals at 100ms intervals.
# Accumulates occupancy over ScootSampleReportInterval seconds.
# Packs results into nibble bitmask for SNMP utcType2ScootVSn reply.
#
# Nibble encoding (per detector per sample interval):
#   0x0 = never occupied
#   0xF = always occupied
#   0x8 = ~50% occupied
#   value = int(occupied_ticks / total_ticks * 15)
#
# Byte packing (matches Chameleon / TRANSLATE.py format):
#   Byte 0: D2[high nibble] D1[low nibble]
#   Byte 1: D4[high nibble] D3[low nibble]
#   ...
#
# ScootDetectorCount = total VSn instances across all SCNs

import threading
import time
import logging

log = logging.getLogger('SCOOT')

SAMPLE_INTERVAL_MS = 100   # ms — matches controller tick rate


class ScootSampler:
    """
    SCOOT occupancy sampler.

    Parameters
    ----------
    io      : IOBus  — shared typed IO bus
    mapping : dict   — ug405 section from config
    live    : dict   — live config (ScootSampleReportInterval)
    """

    def __init__(self, io, mapping, live, on_sample=None):
        self.io        = io
        self.mapping   = mapping
        self.live      = live
        self.on_sample = on_sample  # callback(scn, packed_bytes) on each interval

        # Build per-SCN VSn signal lists in bit order
        # { scn: [sig_for_bit1, sig_for_bit2, ...] }
        self._vsn_signals = {}
        self._total_count = 0

        for scn in mapping['scns']:
            reply = mapping['reply'][scn]
            if 'VSn' not in reply:
                continue
            bits = reply['VSn']   # { bit: signal }
            # Sort by bit number to get correct detector order
            ordered = [sig for _, sig in sorted(bits.items())]
            self._vsn_signals[scn] = ordered
            self._total_count += len(ordered)
            log.info("SCOOT: SCN %s has %d VSn detectors", scn, len(ordered))

        # Accumulated sample counts { scn: [count_per_detector] }
        self._sample_counts = {
            scn: [0] * len(sigs)
            for scn, sigs in self._vsn_signals.items()
        }
        self._tick_count = 0   # ticks since last pack

        # Packed results { scn: bytes }
        # Initialise to correct byte length for detector count (zero occupancy)
        self._packed = {
            scn: bytes((len(sigs) + 1) // 2)
            for scn, sigs in self._vsn_signals.items()
        }
        self._lock   = threading.Lock()

    @property
    def detector_count(self):
        """Total VSn detector count across all SCNs."""
        return self._total_count

    def get_packed(self, scn):
        """
        Return packed nibble bytes for a SCN.
        Called by SNMP GET handler for utcType2ScootVSn.
        """
        with self._lock:
            return self._packed.get(scn, b'\x00')

    def start(self):
        if not self._vsn_signals:
            log.info("SCOOT: no VSn signals configured — sampler idle")
            return
        threading.Thread(target=self._sample_loop, daemon=True).start()
        log.info("SCOOT: sampler started — %d detectors total", self._total_count)

    # ── Sample loop ───────────────────────────────────────────────────────────

    def _sample_loop(self):
        """Sample VSn signals every 100ms. Pack every ScootSampleReportInterval."""
        while True:
            time.sleep(SAMPLE_INTERVAL_MS / 1000.0)

            # Read and accumulate
            with self._lock:
                for scn, signals in self._vsn_signals.items():
                    for i, sig in enumerate(signals):
                        if self.io.read(sig):
                            self._sample_counts[scn][i] += 1
                self._tick_count += 1

            # Check if report interval elapsed
            report_ticks = int(
                self.live.get('ScootSampleReportInterval', 4) * 1000
                / SAMPLE_INTERVAL_MS
            )

            if self._tick_count >= report_ticks:
                self._pack_and_reset()

    def _pack_and_reset(self):
        """Pack accumulated counts into nibble bytes and reset counters."""
        with self._lock:
            total_ticks = self._tick_count

            for scn, counts in self._sample_counts.items():
                packed = self._pack_nibbles(counts, total_ticks)
                self._packed[scn] = packed
                log.debug("SCOOT: %s packed=%s", scn, packed.hex())
                # Notify RBE on every interval
                if self.on_sample:
                    self.on_sample(scn, packed)

            # Reset counters
            for scn in self._sample_counts:
                self._sample_counts[scn] = [0] * len(self._sample_counts[scn])
            self._tick_count = 0

    def _pack_nibbles(self, counts, total_ticks):
        """
        Convert occupancy counts to packed nibble bytes.

        counts      : [int] — occupied ticks per detector
        total_ticks : int   — total ticks in interval

        Returns exactly ceil(len(counts)/2) bytes:
          Byte 0 = D2 nibble (high) | D1 nibble (low)
          Byte 1 = D4 nibble (high) | D3 nibble (low)
          ...
        Zero bytes are included — length is always fixed for detector count.
        """
        # Always fixed length regardless of content
        n_bytes = (len(counts) + 1) // 2

        if total_ticks == 0 or not counts:
            return bytes(n_bytes)

        # Convert counts to nibbles (0-15)
        nibbles = [min(15, int(c / total_ticks * 15)) for c in counts]

        # Pad to even length
        while len(nibbles) % 2:
            nibbles.append(0)

        # Pack: low nibble = odd detector (D1,D3...), high nibble = even (D2,D4...)
        result = bytearray()
        for i in range(0, len(nibbles), 2):
            result.append((nibbles[i + 1] << 4) | nibbles[i])

        # Ensure correct length (handles rounding edge cases)
        while len(result) < n_bytes:
            result.append(0)

        return bytes(result[:n_bytes])


if __name__ == '__main__':
    logging.basicConfig(
        level=logging.DEBUG,
        format='%(asctime)s [%(name)s] %(levelname)s  %(message)s',
        datefmt='%H:%M:%S'
    )

    from io_bus import IOBus
    from config import load_config

    cfg     = load_config('/opt/ug405/platform.cfg')
    io      = IOBus()
    for name, owner in cfg['signals'].items():
        io.register(name, owner)

    live = {'ScootSampleReportInterval': 5}

    sampler = ScootSampler(io, cfg['ug405'], live)
    sampler.start()

    print(f"Total SCOOT detectors: {sampler.detector_count}")

    # Simulate detector activity
    import random
    scns = cfg['ug405']['scns']
    for scn in scns:
        if 'VSn' in cfg['ug405']['reply'][scn]:
            sigs = list(cfg['ug405']['reply'][scn]['VSn'].values())
            print(f"Simulating {len(sigs)} detectors for {scn}")
            # Toggle detectors randomly
            def toggle(sigs=sigs):
                while True:
                    for sig in sigs:
                        io.write(sig, random.randint(0,1), source='xkop_driver')
                    time.sleep(0.1)
            threading.Thread(target=toggle, daemon=True).start()

    # Print packed values every interval
    try:
        while True:
            time.sleep(live['ScootSampleReportInterval'])
            for scn in scns:
                packed = sampler.get_packed(scn)
                if packed:
                    print(f"{scn} VSn packed: {packed.hex()} "
                          f"({len(packed)*2} detectors)")
    except KeyboardInterrupt:
        print("Stopped")