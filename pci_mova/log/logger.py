"""
pci_mova.log.logger
StreamLogger — buffered JSONL historian for one MOVA stream.

Three record types written per stream per day:
  {"t":"msg",  "ts":..., "type":N, "sub":N, "seq":N, "raw":[...], "desc":"..."}
  {"t":"err",  "ts":..., "error_id":N, "day":N, "month":N, "year":N, "hours":N, "minutes":N, "data":N}
  {"t":"hb",   "ts":..., "cs":N, "ds":N, "ns":N, "ec":N, "wc":N, "plan":N, "on_ctrl":N, "mova_on":N}

Write strategy:
  Messages and errors are buffered in RAM.  The buffer is flushed to disk every
  FLUSH_TICKS ticks (30 s at 10 Hz) and on explicit flush() / close() calls.
  A heartbeat ("hb") record is written every HB_TICKS ticks (10 s).

  This gives ~2880 disk writes/day regardless of message volume — safe for both
  server SSD and flash-based embedded hardware.

Daily rotation happens at flush time when the date changes — no midnight thread needed.
"""

import json
import logging
import os
import time
from datetime import date

logger = logging.getLogger(__name__)

FLUSH_TICKS = 300   # 30 s at 10 Hz — flush interval
HB_TICKS    = 100   # 10 s at 10 Hz — heartbeat interval
TMA_TICKS   = 600   # 60 s at 10 Hz — TMA count sample interval


class StreamLogger:
    """
    Buffered JSONL historian for a single MOVA stream.
    All methods are called from the stream tick thread — no locking needed.
    """

    def __init__(self, stream_id: int, log_dir: str) -> None:
        self._id       = stream_id
        self._dir      = log_dir
        self._buf: list = []
        self._file     = None
        self._cur_date: str = ""
        self._flush_ctr: int = 0
        self._hb_ctr:   int = 0
        self._tma_ctr:  int = 0

    # ── Public interface ──────────────────────────────────────────────────────

    def log_session(self, no_links: int, no_lanes: int, no_stages: int, no_dets: int,
                    detector_meta: list = None) -> None:
        """Write a session-start record when a dataset is loaded."""
        self._buf.append({
            "t": "session", "ts": time.time(),
            "nl": no_links, "nla": no_lanes, "ns": no_stages, "nd": no_dets,
            "dm": detector_meta or [],   # per-detector {id, type, link, lane, dist}
        })

    def log_msg(self, msg: dict) -> None:
        """Buffer one decoded kernel decision message."""
        self._buf.append({
            "t":    "msg",
            "ts":   msg.get("ts", time.time()),
            "seq":  msg.get("seq"),
            "type": msg.get("type"),
            "sub":  msg.get("sub"),
            "raw":  msg.get("raw", []),
            "desc": msg.get("desc", ""),
        })

    def log_error(self, err: dict) -> None:
        """Buffer one kernel IDS error entry."""
        self._buf.append({"t": "err", "ts": time.time(), **err})

    def sample_tma(self, tma_info: dict) -> None:
        """
        Buffer one per-detector kernel TMA count snapshot every TMA_TICKS ticks (60 s).
        tma_info is the dict from MovaKernel.tma_det_counts(): det_flow[], period_start, period_sec.
        Counts are cumulative within the current TMAPRD period; pstart identifies the period
        so callers can compute per-minute deltas (same pstart = subtract, new pstart = reset).
        """
        self._tma_ctr += 1
        if self._tma_ctr < TMA_TICKS:
            return
        self._tma_ctr = 0
        if not tma_info:
            return
        self._buf.append({
            "t":      "tma",
            "ts":     time.time(),
            "pstart": tma_info.get("period_start", 0),
            "psec":   tma_info.get("period_sec",   0),
            "dc":     tma_info.get("det_flow",     []),
        })

    def tick(self, cs, ds, ns, ec, wc, plan, on_ctrl, mova_on) -> None:
        """
        Called once per kernel tick.  Writes a heartbeat every HB_TICKS ticks
        and flushes the buffer to disk every FLUSH_TICKS ticks.
        """
        self._hb_ctr += 1
        if self._hb_ctr >= HB_TICKS:
            self._buf.append({
                "t": "hb", "ts": time.time(),
                "cs": cs, "ds": ds, "ns": ns,
                "ec": ec, "wc": wc, "plan": plan,
                "on_ctrl": on_ctrl, "mova_on": mova_on,
            })
            self._hb_ctr = 0

        self._flush_ctr += 1
        if self._flush_ctr >= FLUSH_TICKS:
            self._flush()
            self._flush_ctr = 0

    def flush(self) -> None:
        """Force an immediate disk flush — call on stream stop/reset."""
        self._flush()

    def close(self) -> None:
        """Flush and close the current log file."""
        self._flush()
        if self._file:
            self._file.close()
            self._file = None

    # ── Internal ──────────────────────────────────────────────────────────────

    def _open_today(self) -> None:
        today = date.today().isoformat()
        if today == self._cur_date and self._file is not None:
            return
        if self._file is not None:
            self._file.close()
            self._file = None
        os.makedirs(self._dir, exist_ok=True)
        path = os.path.join(self._dir, f"stream_{self._id}_{today}.jsonl")
        self._file     = open(path, "a")
        self._cur_date = today
        logger.debug("Stream %d: log file opened %s", self._id, path)

    def _flush(self) -> None:
        if not self._buf:
            return
        try:
            self._open_today()
            for entry in self._buf:
                self._file.write(json.dumps(entry) + "\n")
            self._file.flush()
        except OSError as exc:
            logger.warning("Stream %d: log flush failed: %s", self._id, exc)
        finally:
            self._buf.clear()

    # ── Utility ───────────────────────────────────────────────────────────────

    def log_path(self, for_date: str = "") -> str:
        """Absolute path to the JSONL file for the given date (defaults to today)."""
        d = for_date or date.today().isoformat()
        return os.path.join(self._dir, f"stream_{self._id}_{d}.jsonl")

    def available_dates(self) -> list:
        """Sorted list of date strings for which log files exist."""
        prefix = f"stream_{self._id}_"
        suffix = ".jsonl"
        try:
            entries = os.listdir(self._dir)
        except OSError:
            return []
        dates = []
        for name in entries:
            if name.startswith(prefix) and name.endswith(suffix):
                d = name[len(prefix):-len(suffix)]
                if len(d) == 10:   # YYYY-MM-DD
                    dates.append(d)
        return sorted(dates)
