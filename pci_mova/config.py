"""
pci_mova — Standalone MOVA Runtime
Configuration and constants
"""

import os

# ─── Stream limits ────────────────────────────────────────────────────────────
MAX_STREAMS        = 8          # Physical ceiling (controller supports 8)
DEFAULT_STREAMS    = 4          # Default licensed tier
TICK_RATE_HZ       = 10         # 100 ms tick (matches kernel rate)
TICK_SEC           = 1.0 / TICK_RATE_HZ
SLOW_TICK_DIVISOR  = 10         # Every 10 ticks = 1 Hz slow tasks

# ─── MOVA Kernel constants ────────────────────────────────────────────────────
MOVA_VERSION       = "M8.0"     # Dataset version string — must match .mxds header
MAX_DETECTORS      = 64
MAX_CONFIRMS       = 32
MAX_FORCE          = 10
MAX_DIN            = 100
MAX_DOUT           = 24
MAX_IO             = 64         # Tcomshr->io[] size (mxdets=NumberOfDetectors=64)

# ─── Watchdog timeouts ────────────────────────────────────────────────────────
MAX_WARMUP_SECONDS      = 120   # Seconds before warmup fault raised
MAX_MANUAL_FORCE_SECONDS = 60   # Seconds before force auto-cleared

# ─── MOVA Tools protocol ──────────────────────────────────────────────────────
MOVA_TOOLS_BASE_PORT = 6000     # Stream 0 listens here; stream N on 6000+N
MOVA_TOOLS_TIMEOUT   = 120      # Seconds idle before pipe watchdog disconnects
MAX_MSG_LEN          = 2000

# ─── Web API ──────────────────────────────────────────────────────────────────
API_HOST = os.getenv("MOVA_HOST", "0.0.0.0")
API_PORT = int(os.getenv("MOVA_PORT", "8080"))

# ─── Paths ────────────────────────────────────────────────────────────────────
BASE_DIR     = os.path.dirname(os.path.abspath(__file__))
DATASET_DIR  = os.getenv("MOVA_DATASET_DIR",  os.path.join(BASE_DIR, "datasets"))
LICENCE_DIR  = os.getenv("MOVA_LICENCE_DIR",  os.path.join(BASE_DIR, "licence"))
LOG_DIR      = os.getenv("MOVA_LOG_DIR",       os.path.join(BASE_DIR, "logs"))

# Ensure directories exist at import time
for _d in (DATASET_DIR, LICENCE_DIR, LOG_DIR):
    os.makedirs(_d, exist_ok=True)
