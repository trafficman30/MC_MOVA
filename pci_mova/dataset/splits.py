"""
pci_mova.dataset.splits
Resolves MOVA dataset split sizes.

SPLIT sizes are defined in gbltypes.h.  When that file is available on this
system the real values are extracted from it.  Otherwise the known confirmed
values are used and unconfirmed ones are flagged with a warning.

Call get_splits() to obtain the current split configuration.
Call verify_from_header(path) to extract sizes from a gbltypes.h file and
cache them for the lifetime of the process.
"""

import logging
import os
import re
from dataclasses import dataclass
from typing import Optional

logger = logging.getLogger(__name__)

# ── Confirmed values (from source analysis) ───────────────────────────────────
# SPLIT1: 16              — confirmed from gbltypes.h #define SPLIT1 16
# SPLIT2: mxstag*(mxlink+1) = 10*61 = 610 — confirmed from defines.h
_CONFIRMED_SPLITS = {
    "SPLIT1": 16,
    "SPLIT2": 610,
}

# ── Unconfirmed placeholders ──────────────────────────────────────────────────
# These match common M8 dataset sizes seen in practice.
# Must be verified against gbltypes.h SPLIT3..SPLIT7 definitions.
_PLACEHOLDER_SPLITS = {
    "SPLIT3": 1090,   # (mxstag*mxstag)+(mxstag*5)+(mxstag*mxlink)+(mxlink*5)+40
    "SPLIT4": 360,    # mxlink*6
    "SPLIT5": 660,    # mxlink*11
    "SPLIT6": 390,    # mxlane*13
    "SPLIT7": 2943,   # SPLIT7_FIX(2099) + SPLIT7_VAR(840) + SPLIT7_CHK(4)
}

# Runtime override — set by verify_from_header() or environment
_RESOLVED: dict[str, int] = {}
_VERIFIED: bool = False

# Environment variable override path
_GBLTYPES_ENV = "MOVA_GBLTYPES_H"


@dataclass
class SplitSizes:
    SPLIT1: int
    SPLIT2: int
    SPLIT3: int
    SPLIT4: int
    SPLIT5: int
    SPLIT6: int
    SPLIT7: int
    verified: bool   # True if all values extracted from gbltypes.h

    def as_list(self) -> list[int]:
        return [self.SPLIT1, self.SPLIT2, self.SPLIT3,
                self.SPLIT4, self.SPLIT5, self.SPLIT6, self.SPLIT7]


def get_splits() -> SplitSizes:
    """
    Return the current split sizes, attempting to resolve from gbltypes.h
    if not already done.
    """
    global _VERIFIED

    if not _VERIFIED:
        _attempt_auto_resolve()

    merged = {**_CONFIRMED_SPLITS, **_PLACEHOLDER_SPLITS, **_RESOLVED}

    if not _VERIFIED:
        unconfirmed = [k for k in _PLACEHOLDER_SPLITS if k not in _RESOLVED]
        if unconfirmed:
            logger.warning(
                "Dataset split sizes %s are unconfirmed placeholders. "
                "Set MOVA_GBLTYPES_H=/path/to/gbltypes.h or call "
                "pci_mova.dataset.splits.verify_from_header(path) "
                "to resolve from source.",
                ", ".join(unconfirmed)
            )

    return SplitSizes(
        SPLIT1   = merged["SPLIT1"],
        SPLIT2   = merged["SPLIT2"],
        SPLIT3   = merged["SPLIT3"],
        SPLIT4   = merged["SPLIT4"],
        SPLIT5   = merged["SPLIT5"],
        SPLIT6   = merged["SPLIT6"],
        SPLIT7   = merged["SPLIT7"],
        verified = _VERIFIED,
    )


def verify_from_header(path: str) -> SplitSizes:
    """
    Extract SPLIT sizes from a gbltypes.h file and cache for this process.

    Parses #define SPLIT1..SPLIT7 and evaluates any macro expressions
    that reference known defines from defines.h (mxstag, mxlink, etc.).

    Returns the resolved SplitSizes.
    Raises ValueError if required splits cannot be found.
    """
    global _RESOLVED, _VERIFIED

    logger.info("Extracting split sizes from %s", path)
    extracted = _parse_gbltypes(path)
    _RESOLVED.update(extracted)
    _VERIFIED = all(f"SPLIT{i}" in _RESOLVED for i in range(1, 8))

    sizes = get_splits()
    logger.info(
        "Split sizes resolved: %s  verified=%s",
        {f"SPLIT{i}": getattr(sizes, f"SPLIT{i}") for i in range(1, 8)},
        sizes.verified,
    )
    return sizes


def _attempt_auto_resolve() -> None:
    """Try to resolve from MOVA_GBLTYPES_H env var if set."""
    path = os.getenv(_GBLTYPES_ENV)
    if path and os.path.isfile(path):
        try:
            verify_from_header(path)
        except Exception as exc:
            logger.warning("Auto-resolve from %s failed: %s", path, exc)


def _parse_gbltypes(path: str) -> dict[str, int]:
    """
    Parse #define statements from a C header file.
    Handles simple integer defines and basic arithmetic expressions
    referencing other defines (e.g. SPLIT2 = mxstag * (mxlink + 1)).
    """
    with open(path, "r", errors="replace") as fh:
        content = fh.read()

    # Strip C comments
    content = re.sub(r"/\*.*?\*/", " ", content, flags=re.DOTALL)
    content = re.sub(r"//.*$", "", content, flags=re.MULTILINE)

    # Collect all #define name value
    raw: dict[str, str] = {}
    for m in re.finditer(r"#define\s+(\w+)\s+(.+)", content):
        name  = m.group(1).strip()
        value = m.group(2).strip()
        raw[name] = value

    # Known constant substitutions from defines.h
    known = {
        "mxstag":  10,    # NumberOfForce
        "mxlink":  60,    # NumberOfMovaLinks
        "mxlane":  30,
        "mxdets":  64,
        "mxconf":  32,
        "mxlnOnlink": 3,
        "mxdetTypes": 11,
    }
    # Also pull any integer defines from the file itself
    for name, expr in raw.items():
        try:
            known[name] = int(expr)
        except (ValueError, TypeError):
            pass

    # Now evaluate SPLIT1..SPLIT7
    result: dict[str, int] = {}
    for i in range(1, 8):
        key = f"SPLIT{i}"
        if key not in raw:
            continue
        expr = raw[key]
        try:
            value = _eval_expr(expr, known)
            result[key] = value
            known[key]  = value
        except Exception as exc:
            logger.warning("Could not evaluate %s = %r: %s", key, expr, exc)

    return result


def _eval_expr(expr: str, known: dict[str, int]) -> int:
    """
    Safely evaluate a simple C arithmetic expression using known constants.
    Supports: integers, +, -, *, /, (, ), and known identifiers.
    """
    # Replace known identifiers with their values
    def replace_id(m):
        name = m.group(0)
        if name in known:
            return str(known[name])
        return name

    resolved = re.sub(r"[A-Za-z_]\w*", replace_id, expr)

    # Allow only safe characters
    if not re.match(r"^[\d\s\+\-\*\/\(\)]+$", resolved):
        raise ValueError(f"Unsafe expression after substitution: {resolved!r}")

    return int(eval(resolved))   # nosec — expression is sanitised above
