"""
pci_mova.licence.licence
Licence management — hardware binding, HMAC-JWT signing, tier enforcement.

Licence file format:
  A compact JWT string (header.payload.signature), stored as plain text in
  pci_mova/licence/mova.licence.

  header:    {"alg": "HS256"}
  payload:   {"fp": <hw fingerprint>, "str": <max_streams>, "own": <owner>,
               "prv": <provider>, "ref": <licence_ref>,
               "iat": <unix issued>, "exp": <unix expiry or null>}
  signature: HMAC-SHA256(header.payload, LICENCE_SECRET)

Forgery protection:
  Editing any byte of the payload invalidates the HMAC signature.
  Without the secret key the signature cannot be recomputed.

Hardware fingerprint:
  SHA256(/etc/machine-id + ":" + first non-loopback MAC).
  Stable across reboots and most hardware changes short of replacing
  both the board and NIC simultaneously.

Generating a licence (issuing side):
  python3 -m pci_mova.licence.generate --streams 4 --owner "Site A" --fp <fingerprint>

Displaying fingerprint (deployment side):
  python3 -m pci_mova.licence.generate --show
"""

import base64
import hashlib
import hmac
import json
import logging
import os
import socket
import time
import uuid
from dataclasses import dataclass
from typing import Optional

from pci_mova.config import LICENCE_DIR, MAX_STREAMS

logger = logging.getLogger(__name__)

LICENCE_FILE = os.path.join(LICENCE_DIR, "mova.licence")

# ── Signing secret ─────────────────────────────────────────────────────────────
# Override with MOVA_LICENCE_SECRET env var for production.
# Change this value before any commercial deployment — once changed, all
# previously-issued licences become invalid (they must be re-issued).
_DEFAULT_SECRET = "pci-mova-dev-key-CHANGE-BEFORE-DEPLOYMENT-2026"
LICENCE_SECRET  = os.environ.get("MOVA_LICENCE_SECRET", _DEFAULT_SECRET).encode()


# ── Licence data ───────────────────────────────────────────────────────────────

@dataclass
class LicenceData:
    provider:    str
    owner:       str
    max_streams: int
    fingerprint: str
    issued_at:   float
    expires_at:  Optional[float] = None
    licence_ref: str = ""


@dataclass
class LicenceResult:
    valid:       bool
    max_streams: int
    reason:      str
    data:        Optional[LicenceData] = None

    def allows_stream(self, stream_id: int) -> bool:
        return self.valid and stream_id < self.max_streams


# ── JWT helpers (no external dependencies) ────────────────────────────────────

def _b64url_encode(data: bytes) -> str:
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode()


def _b64url_decode(s: str) -> bytes:
    pad = 4 - len(s) % 4
    if pad != 4:
        s += "=" * pad
    return base64.urlsafe_b64decode(s)


def _sign(header_b64: str, payload_b64: str) -> str:
    msg = f"{header_b64}.{payload_b64}".encode()
    sig = hmac.new(LICENCE_SECRET, msg, hashlib.sha256).digest()
    return _b64url_encode(sig)


def encode_jwt(payload: dict) -> str:
    header  = _b64url_encode(json.dumps({"alg": "HS256"}, separators=(",", ":")).encode())
    body    = _b64url_encode(json.dumps(payload, separators=(",", ":")).encode())
    sig     = _sign(header, body)
    return f"{header}.{body}.{sig}"


def decode_jwt(token: str) -> dict:
    """Decode and verify a JWT token. Raises ValueError if invalid."""
    parts = token.strip().split(".")
    if len(parts) != 3:
        raise ValueError("Not a valid JWT — expected 3 segments")
    header_b64, payload_b64, sig_b64 = parts

    expected = _sign(header_b64, payload_b64)
    if not hmac.compare_digest(expected, sig_b64):
        raise ValueError("Licence signature invalid — file may have been tampered with")

    try:
        return json.loads(_b64url_decode(payload_b64))
    except Exception as exc:
        raise ValueError(f"Licence payload decode failed: {exc}")


# ── Hardware fingerprinting ────────────────────────────────────────────────────

def get_machine_id() -> str:
    try:
        with open("/etc/machine-id") as fh:
            return fh.read().strip()
    except OSError:
        return ""


def get_primary_mac() -> str:
    try:
        return f"{uuid.getnode():012x}"
    except Exception:
        return "000000000000"


def build_fingerprint() -> str:
    raw = f"{get_machine_id()}:{get_primary_mac()}".encode()
    return hashlib.sha256(raw).hexdigest()

# Public alias used by API and generate tool
hardware_fingerprint = build_fingerprint


# ── Licence I/O ────────────────────────────────────────────────────────────────

def save_licence(data: LicenceData, path: str = LICENCE_FILE) -> str:
    """Sign and save a LicenceData as a JWT licence file. Returns the token."""
    payload = {
        "fp":  data.fingerprint,
        "str": data.max_streams,
        "own": data.owner,
        "prv": data.provider,
        "ref": data.licence_ref,
        "iat": data.issued_at,
        "exp": data.expires_at,
    }
    token = encode_jwt(payload)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as fh:
        fh.write(token + "\n")
    logger.info("Licence saved to %s", path)
    return token


def load_licence(path: str = LICENCE_FILE) -> Optional[LicenceData]:
    if not os.path.exists(path):
        return None
    try:
        with open(path) as fh:
            token = fh.read().strip()
        payload = decode_jwt(token)
        return LicenceData(
            fingerprint = payload["fp"],
            max_streams = int(payload["str"]),
            owner       = payload.get("own", ""),
            provider    = payload.get("prv", ""),
            licence_ref = payload.get("ref", ""),
            issued_at   = float(payload.get("iat", 0)),
            expires_at  = payload.get("exp"),
        )
    except ValueError as exc:
        logger.error("Licence rejected: %s", exc)
        return None
    except Exception as exc:
        logger.error("Failed to load licence from %s: %s", path, exc)
        return None


# ── Validation ─────────────────────────────────────────────────────────────────

def validate(path: str = LICENCE_FILE) -> LicenceResult:
    data = load_licence(path)

    if data is None:
        logger.warning("No valid licence at %s — UNLICENSED", path)
        return LicenceResult(valid=False, max_streams=0, reason="No licence file or signature invalid")

    if data.expires_at is not None and time.time() > data.expires_at:
        return LicenceResult(valid=False, max_streams=0,
                             reason="Licence has expired", data=data)

    local_fp = build_fingerprint()
    if data.fingerprint != local_fp:
        logger.warning("Licence fingerprint mismatch — licence: %s...  hardware: %s...",
                       data.fingerprint[:16], local_fp[:16])
        return LicenceResult(valid=False, max_streams=0,
                             reason="Licence is not valid for this hardware", data=data)

    max_streams = min(data.max_streams, MAX_STREAMS)
    logger.debug("Licence valid — owner: %s  streams: %d  ref: %s",
                 data.owner, max_streams, data.licence_ref)
    return LicenceResult(valid=True, max_streams=max_streams, reason="OK", data=data)


# ── Activation helper ──────────────────────────────────────────────────────────

def create_licence(
    provider:    str,
    owner:       str,
    max_streams: int,
    licence_ref: str = "",
    expires_at:  Optional[float] = None,
    fingerprint: Optional[str] = None,
    path:        str = LICENCE_FILE,
) -> LicenceData:
    fp   = fingerprint or build_fingerprint()
    data = LicenceData(
        provider    = provider,
        owner       = owner,
        max_streams = max_streams,
        fingerprint = fp,
        issued_at   = time.time(),
        expires_at  = expires_at,
        licence_ref = licence_ref,
    )
    save_licence(data, path)
    return data


def get_hardware_info() -> dict:
    return {
        "machine_id":  get_machine_id(),
        "mac":         get_primary_mac(),
        "fingerprint": build_fingerprint(),
        "hostname":    socket.gethostname(),
    }
