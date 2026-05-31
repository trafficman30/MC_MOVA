"""
pci_mova.dataset.parser
MovaDataset — parses MOVA Tools M8 .mxds XML dataset files.

The .mxds format is the MOVA Tools M8 XML design file:
  namespace: http://www.trl.co.uk/schemas/MOVATOOLSFILE
  root tag:  MOVATOOLSDATA

All junction configuration is stored as structured XML — stages, links,
lanes, detectors, confirms, interstage matrix, constants.
"""

import os
import re
import logging
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from typing import Optional

logger = logging.getLogger(__name__)

NS           = "http://www.trl.co.uk/schemas/MOVATOOLSFILE"
MOVA_VERSION = "M8.0"


class DatasetError(Exception):
    """Raised when a dataset file fails validation."""


def _find(el, tag):
    r = el.find(f"{{{NS}}}{tag}")
    return r if r is not None else el.find(tag)

def _findall(el, tag):
    r = el.findall(f"{{{NS}}}{tag}")
    return r if r else el.findall(tag)

def _text(el, tag, default=""):
    if el is None:
        return default
    c = _find(el, tag)
    return c.text.strip() if c is not None and c.text else default

def _int(el, tag, default=0):
    try:
        return int(_text(el, tag, str(default)))
    except (ValueError, TypeError):
        return default

def _float(el, tag, default=0.0):
    try:
        return float(_text(el, tag, str(default)))
    except (ValueError, TypeError):
        return default


@dataclass
class DatasetHeader:
    filename:      str
    version:       str
    title:         str
    date:          str
    mds_name:      str
    tool_version:  str
    stream_id_str: str = ""    # original <ID> value from XML (e.g. "J14861", "403BC1")
    stages:        int = 0
    stagon:        int = 0    # 0=Open contact (active-low), 1=Closed contact (active-high)

    def to_dict(self):
        return {
            "filename":      self.filename,
            "version":       self.version,
            "title":         self.title,
            "date":          self.date,
            "mds_name":      self.mds_name,
            "tool_version":  self.tool_version,
            "stream_id_str": self.stream_id_str,
            "stages":        self.stages,
            "stagon":        self.stagon,
        }


@dataclass
class LaneDef:
    id:           int
    lane_type:    str
    short_length: float
    x_det:        int
    in_det:       int
    cruise_speed: float
    sat_flow:     int
    headway:      float


@dataclass
class LinkDef:
    id:           int
    phase:        str
    link_type:    str
    demand_ch:    int
    latched:      bool
    is_short:     bool
    n_main_lanes: int
    lanes:        list = field(default_factory=list)
    lgreen:       int = 0
    ltype:        int = 0
    lphase:       int = 0


@dataclass
class StageDef:
    id:            int
    min_green:     float
    max_green:     float
    is_ped:        bool
    phases:        list = field(default_factory=list)
    running_links: dict = field(default_factory=dict)


@dataclass
class DetectorDef:
    id:       int
    det_type: str
    lane_id:  Optional[int]
    link_id:  Optional[int]
    distance: float


@dataclass
class ConfirmDef:
    id:    int
    stage: int
    phase: str


@dataclass
class Interstage:
    from_stage: int
    to_stage:   int
    value:      int
    banned:     bool


@dataclass
class JunctionConstants:
    stages:    int
    links:     int
    lanes:     int
    detectors: int
    scan:      float
    min_ext:   int
    max_ext:   float
    stagon:    int = 0   # 0=Open contact (active-low), 1=Closed contact (active-high)


@dataclass
class MovaDataset:
    header:      DatasetHeader
    constants:   JunctionConstants
    stages:      list = field(default_factory=list)
    links:       list = field(default_factory=list)
    detectors:   list = field(default_factory=list)
    confirms:    list = field(default_factory=list)
    interstages: list = field(default_factory=list)
    source_path: str = ""

    @property
    def is_placeholder(self):
        return len(self.links) == 0

    def stage_by_id(self, stage_id):
        return next((s for s in self.stages if s.id == stage_id), None)

    def link_by_id(self, link_id):
        return next((l for l in self.links if l.id == link_id), None)

    def interstage_time(self, from_stage, to_stage):
        ig = next(
            (i for i in self.interstages
             if i.from_stage == from_stage and i.to_stage == to_stage),
            None
        )
        return ig.value if ig else 0


def _best_title(cs, file_title: str = "") -> str:
    """
    Pick the best human-readable label for a ControllerStream.
    Priority: <Description> → <Name> → <ID> string → file-level title.
    Skips the literal string '(untitled)' at every level.
    """
    _skip = {"(untitled)", ""}
    for tag in ("Description", "Name", "ID"):
        val = _text(cs, tag)
        if val and val.lower().strip("()") not in ("untitled", ""):
            return val
    return file_title or ""


def load(path: str) -> MovaDataset:
    """Load and validate a .mxds dataset file."""
    if not os.path.exists(path):
        raise DatasetError(f"Dataset file not found: {path}")

    logger.info("Loading dataset: %s", path)

    try:
        tree = ET.parse(path)
    except ET.ParseError as exc:
        raise DatasetError(f"XML parse error in {os.path.basename(path)}: {exc}")

    root = tree.getroot()
    root_tag = root.tag.split("}")[-1] if "}" in root.tag else root.tag
    if root_tag != "MOVATOOLSDATA":
        raise DatasetError(f"Not a MOVA Tools file — root tag: {root_tag}")

    tool_version = root.get("Version", "")

    fd    = _find(root, "FileDescription")
    title = _text(fd, "Title")
    date  = _text(fd, "Date")

    mova_net = _find(root, "MOVANetwork")
    if mova_net is None:
        raise DatasetError("No MOVANetwork element found")

    cs = _find(mova_net, "ControllerStream")
    if cs is None:
        raise DatasetError("No ControllerStream element found")

    mova_ver = _text(cs, "MOVAVersion")
    if not mova_ver.startswith(MOVA_VERSION):
        raise DatasetError(f"Unsupported MOVA version: expected {MOVA_VERSION}, got '{mova_ver}'")

    consts_el = _find(cs, "Constants")
    stage_confirm_on = _text(cs, "StageConfirmON", "Open").strip()
    constants = JunctionConstants(
        stages    = _int(consts_el, "STAGES"),
        links     = _int(consts_el, "NLINKS"),
        lanes     = _int(consts_el, "NLANES"),
        detectors = _int(consts_el, "NDETS"),
        scan      = _float(consts_el, "SCAN", 0.5),
        min_ext   = _int(consts_el, "MINEXT", 3),
        max_ext   = _float(consts_el, "MAXEXT", 7.0),
        stagon    = 1 if stage_confirm_on.lower() == "closed" else 0,
    )

    header = DatasetHeader(
        filename      = os.path.basename(path),
        version       = mova_ver,
        title         = _best_title(cs, title),
        date          = date,
        mds_name      = _text(cs, "MDSFileName"),
        tool_version  = tool_version,
        stream_id_str = _text(cs, "ID"),
        stages        = constants.stages,
        stagon        = constants.stagon,
    )

    dataset = MovaDataset(
        header      = header,
        constants   = constants,
        stages      = _parse_stages(cs),
        links       = _parse_links(cs),
        detectors   = _parse_detectors(cs),
        confirms    = _parse_confirms(cs),
        interstages = _parse_interstages(cs),
        source_path = path,
    )

    logger.info(
        "Dataset loaded OK: %s  v=%s  title=%s  stages=%d  links=%d  dets=%d",
        header.filename, mova_ver, title,
        len(dataset.stages), len(dataset.links), len(dataset.detectors),
    )
    return dataset


def load_all(path: str) -> dict:
    """
    Load all ControllerStreams from a .mxds file.
    Returns {stream_id: MovaDataset} — typically 1-4 entries.
    Each stream maps to one MOVA instance.
    """
    if not os.path.exists(path):
        raise DatasetError(f"Dataset file not found: {path}")

    try:
        tree = ET.parse(path)
    except ET.ParseError as exc:
        raise DatasetError(f"XML parse error in {os.path.basename(path)}: {exc}")

    root = tree.getroot()
    root_tag = root.tag.split("}")[-1] if "}" in root.tag else root.tag
    if root_tag != "MOVATOOLSDATA":
        raise DatasetError(f"Not a MOVA Tools file — root tag: {root_tag}")

    tool_version = root.get("Version", "")
    fd    = _find(root, "FileDescription")
    title = _text(fd, "Title")
    date  = _text(fd, "Date")

    mova_net = _find(root, "MOVANetwork")
    if mova_net is None:
        raise DatasetError("No MOVANetwork element found")

    streams = _findall(mova_net, "ControllerStream")
    if not streams:
        raise DatasetError("No ControllerStream elements found")

    result = {}
    for cs in streams:
        id_str   = _text(cs, "ID")          # e.g. "J14861", "403BC1", "1"
        mova_ver = _text(cs, "MOVAVersion")
        if not mova_ver.startswith(MOVA_VERSION):
            raise DatasetError(
                f"Stream {id_str!r}: unsupported version '{mova_ver}'"
            )

        consts_el = _find(cs, "Constants")
        stage_confirm_on = _text(cs, "StageConfirmON", "Open").strip()
        constants = JunctionConstants(
            stages    = _int(consts_el, "STAGES"),
            links     = _int(consts_el, "NLINKS"),
            lanes     = _int(consts_el, "NLANES"),
            detectors = _int(consts_el, "NDETS"),
            scan      = _float(consts_el, "SCAN", 0.5),
            min_ext   = _int(consts_el, "MINEXT", 3),
            max_ext   = _float(consts_el, "MAXEXT", 7.0),
            stagon    = 1 if stage_confirm_on.lower() == "closed" else 0,
        )

        header = DatasetHeader(
            filename      = os.path.basename(path),
            version       = mova_ver,
            title         = _best_title(cs, title),
            date          = date,
            mds_name      = _text(cs, "MDSFileName"),
            tool_version  = tool_version,
            stream_id_str = id_str,
            stages        = constants.stages,
            stagon        = constants.stagon,
        )

        dataset = MovaDataset(
            header      = header,
            constants   = constants,
            stages      = _parse_stages(cs),
            links       = _parse_links(cs),
            detectors   = _parse_detectors(cs),
            confirms    = _parse_confirms(cs),
            interstages = _parse_interstages(cs),
            source_path = path,
        )
        result[id_str] = dataset
        logger.debug(
            "Stream %s loaded: %s  stages=%d  links=%d",
            id_str, header.title, len(dataset.stages), len(dataset.links)
        )

    return result


def _parse_stages(cs):
    stages = []
    lib = _find(cs, "StageLibrary")
    if lib is None:
        return stages
    for s_el in _findall(lib, "LibraryStage"):
        phases_str = _text(s_el, "PhasesInStage", "")
        phases     = [p.strip() for p in phases_str.split(",") if p.strip()]
        running    = {}
        rl_el = _find(s_el, "RunningLinks")
        if rl_el is not None:
            for rl in _findall(rl_el, "LinkInStage"):
                if _text(rl, "LinkRunsInStage", "false").lower() == "true":
                    lid = _int(rl, "LinkID")
                    running[lid] = {
                        "sdcode":  _int(rl, "SDCODE"),
                        "lgreen":  _int(rl, "LGREEN"),
                        "pfacilm": _int(rl, "PFACILM"),
                    }
        stages.append(StageDef(
            id            = _int(s_el, "ID"),
            min_green     = _float(s_el, "UserStageMinimum"),
            max_green     = _float(s_el, "UserStageMaximum", 60.0),
            is_ped        = _text(s_el, "AllRedPedestrianStage", "false").lower() == "true",
            phases        = phases,
            running_links = running,
        ))
    return stages


def _parse_links(cs):
    links = []
    links_el = _find(cs, "Links")
    if links_el is None:
        return links
    for l_el in _findall(links_el, "Link"):
        consts_el = _find(l_el, "Constants")
        links.append(LinkDef(
            id           = _int(l_el, "ID"),
            phase        = _text(l_el, "Phase"),
            link_type    = _text(l_el, "LinkType", "Traffic"),
            demand_ch    = _int(l_el, "DemandChannel"),
            latched      = _text(l_el, "Latched", "false").lower() == "true",
            is_short     = _text(consts_el, "IsShortLink", "false").lower() == "true",
            n_main_lanes = _int(consts_el, "NumberOfMainLanes", 1),
            lanes        = _parse_lanes(l_el),
            lphase       = _int(l_el, "LPHASE_RAW"),
            ltype        = _int(l_el, "LTYPE_RAW"),
        ))
    return links


def _parse_lanes(l_el):
    lanes = []
    lanes_el = _find(l_el, "Lanes")
    if lanes_el is None:
        return lanes
    for lane_el in _findall(lanes_el, "Lane"):
        dets_el    = _find(lane_el, "Detectors")
        in_present = _text(dets_el, "INDetectorPresent", "false").lower() == "true"
        lanes.append(LaneDef(
            id           = _int(lane_el, "ID"),
            lane_type    = _text(lane_el, "LaneType", "Normal"),
            short_length = _float(lane_el, "ShortLaneLength"),
            x_det        = _int(dets_el, "XDetector"),
            in_det       = _int(dets_el, "INDetector") if in_present else 0,
            cruise_speed = _float(lane_el, "CruiseSpeed", 36.0),
            sat_flow     = _int(lane_el, "SaturationFlow", 1800),
            headway      = _float(lane_el, "Headway", 2.0),
        ))
    return lanes


def _parse_detectors(cs):
    dets = []
    dets_el = _find(cs, "Detectors")
    if dets_el is None:
        return dets
    for d_el in _findall(dets_el, "Detector"):
        link_raw = _text(d_el, "Link", "")
        lane_raw = _text(d_el, "Lane", "")
        dets.append(DetectorDef(
            id       = _int(d_el, "ID"),
            det_type = _text(d_el, "DetectorType", "X"),
            lane_id  = int(lane_raw) if lane_raw.strip().isdigit() else None,
            link_id  = int(link_raw) if link_raw.strip().isdigit() else None,
            distance = _float(d_el, "DistanceFromStopline"),
        ))
    return dets


def _parse_confirms(cs):
    confirms = []
    conf_el = _find(cs, "Confirms")
    if conf_el is None:
        return confirms
    for c_el in _findall(conf_el, "Confirm"):
        confirms.append(ConfirmDef(
            id    = _int(c_el, "ID"),
            stage = _int(c_el, "Stage"),
            phase = _text(c_el, "Phase"),
        ))
    return confirms


def _parse_interstages(cs):
    interstages = []
    matrix_el = _find(cs, "InterstageMatrix")
    if matrix_el is None:
        return interstages
    for el in _findall(matrix_el, "ISelement"):
        interstages.append(Interstage(
            from_stage = _int(el, "FromStage"),
            to_stage   = _int(el, "ToStage"),
            value      = _int(el, "Value"),
            banned     = _text(el, "Banned", "Allow").lower() != "allow",
        ))
    return interstages


# ── Full detail parser (for dataset viewer popup) ─────────────────────────────

def parse_full_detail(path: str, stream_id_str: str = "") -> dict:
    """
    Parse the full dataset for display in the Dataset Viewer popup.
    Returns a rich dict covering controller info, phases, stages, links,
    detectors, confirms, interstage matrix, and data plan timetables.
    Does NOT load the dataset into the kernel — read-only.
    """
    if not os.path.exists(path):
        raise DatasetError(f"Dataset file not found: {path}")

    try:
        tree = ET.parse(path)
    except ET.ParseError as exc:
        raise DatasetError(f"XML parse error: {exc}")

    root = tree.getroot()

    # Locate the right ControllerStream
    mova_net = _find(root, "MOVANetwork")
    if mova_net is None:
        raise DatasetError("No MOVANetwork element found")

    css = _findall(mova_net, "ControllerStream")
    cs  = None
    if stream_id_str:
        for c in css:
            if _text(c, "ID") == stream_id_str:
                cs = c
                break
    if cs is None and css:
        cs = css[0]
    if cs is None:
        raise DatasetError("No ControllerStream found")

    # ── Controller info ────────────────────────────────────────────────────
    info = {
        "id":            _text(cs, "ID"),
        "name":          _text(cs, "Name"),
        "description":   _text(cs, "Description"),
        "mova_version":  _text(cs, "MOVAVersion"),
        "main_stage":    _int(cs,  "MainStage"),
        "total_green":   _int(cs,  "TotalGreenLimit"),
        "detector_on":   _text(cs, "DetectorON"),
        "stage_conf_on": _text(cs, "StageConfirmON"),
        "phase_conf_on": _text(cs, "PhaseConfirmON"),
        "stage_dem_on":  _text(cs, "StageDemandON"),
        "mds_filename":  _text(cs, "MDSFileName"),
    }
    consts_el = _find(cs, "Constants")
    if consts_el is not None:
        info["scan"]    = _float(consts_el, "SCAN", 0.5)
        info["stages"]  = _int(consts_el,   "STAGES")
        info["links"]   = _int(consts_el,   "NLINKS")
        info["lanes"]   = _int(consts_el,   "NLANES")
        info["dets"]    = _int(consts_el,   "NDETS")
        info["min_ext"] = _int(consts_el,   "MINEXT")
        info["max_ext"] = _float(consts_el, "MAXEXT")

    # ── Phases ─────────────────────────────────────────────────────────────
    phases = []
    phases_el = _find(cs, "Phases")
    if phases_el is not None:
        for ph in _findall(phases_el, "Phase"):
            phases.append({
                "id":          _text(ph, "ID"),
                "type":        _text(ph, "PhaseType"),
                "min_green":   _float(ph, "MinimumGreen"),
                "assoc":       _text(ph, "AssociatedPhase"),
                "interstage":  _text(ph, "InterstageBehaviour"),
                "confirm_ch":  _text(ph, "PhaseConfirmChannel"),
            })

    # ── Stages ─────────────────────────────────────────────────────────────
    stages = []
    lib = _find(cs, "StageLibrary")
    if lib is not None:
        for st in _findall(lib, "LibraryStage"):
            rl_el = _find(st, "RunningLinks")
            running = []
            if rl_el is not None:
                for rl in _findall(rl_el, "LinkInStage"):
                    if _text(rl, "LinkRunsInStage", "false").lower() == "true":
                        running.append(_int(rl, "LinkID"))
            stages.append({
                "id":        _int(st,   "ID"),
                "phases":    _text(st,  "PhasesInStage"),
                "min_green": _float(st, "UserStageMinimum"),
                "max_green": _float(st, "UserStageMaximum"),
                "is_ped":    _text(st,  "AllRedPedestrianStage", "false").lower() == "true",
                "links":     running,
            })

    # ── Links ──────────────────────────────────────────────────────────────
    links = []
    links_el = _find(cs, "Links")
    if links_el is not None:
        for lk in _findall(links_el, "Link"):
            c2 = _find(lk, "Constants")
            bonus_el = _find(lk, "Bonus")
            ep_el    = _find(lk, "EmergencyAndPriority")
            lanes_el = _find(lk, "Lanes")
            n_lanes  = len(_findall(lanes_el, "Lane")) if lanes_el is not None else 0
            links.append({
                "id":         _int(lk,  "ID"),
                "name":       _text(lk, "Name"),
                "phase":      _text(lk, "Phase"),
                "type":       _text(lk, "LinkType"),
                "dem_ch":     _int(lk,  "DemandChannel"),
                "latched":    _text(lk, "Latched", "false").lower() == "true",
                "n_lanes":    n_lanes,
                "minmin":     _float(c2, "MINMIN")  if c2 else 0,
                "maxmin":     _float(c2, "MAXMIN")  if c2 else 0,
                "bontim":     _float(bonus_el, "BONTIM") if bonus_el else 0,
                "bonmax":     _float(bonus_el, "BONMAX") if bonus_el else 0,
                "is_ep":      ep_el is not None and _text(ep_el, "IsEPLink", "false").lower() == "true",
            })

    # ── Detectors ──────────────────────────────────────────────────────────
    detectors = []
    dets_el = _find(cs, "Detectors")
    if dets_el is not None:
        for d in _findall(dets_el, "Detector"):
            detectors.append({
                "id":       _int(d,   "ID"),
                "type":     _text(d,  "DetectorType"),
                "link":     _text(d,  "Link"),
                "lane":     _text(d,  "Lane"),
                "distance": _float(d, "DistanceFromStopline"),
            })

    # ── Confirms ───────────────────────────────────────────────────────────
    confirms = []
    conf_el = _find(cs, "Confirms")
    if conf_el is not None:
        for c3 in _findall(conf_el, "Confirm"):
            confirms.append({
                "id":    _int(c3,  "ID"),
                "stage": _int(c3,  "Stage"),
                "phase": _text(c3, "Phase"),
            })

    # ── Interstage matrix ──────────────────────────────────────────────────
    matrix = []
    matrix_el = _find(cs, "InterstageMatrix")
    if matrix_el is not None:
        for el in _findall(matrix_el, "ISelement"):
            if _text(el, "Banned", "Allow").lower() == "allow":
                matrix.append({
                    "from": _int(el, "FromStage"),
                    "to":   _int(el, "ToStage"),
                    "val":  _int(el, "Value"),
                })

    # ── Data plan timetables (root AnalysisSets) ───────────────────────────
    plans = []
    asets_el = _find(root, "AnalysisSets")
    if asets_el is not None:
        for i, aset in enumerate(_findall(asets_el, "AnalysisSet")):
            trig_present = _text(aset, "TriggerDetectorPresent", "false").lower() == "true"
            trig_det     = _int(aset, "TriggerDetector") if trig_present else None
            schedule = []
            rts_el = _find(aset, "RunningTimes")
            if rts_el is not None:
                for rt in _findall(rts_el, "RunningTime"):
                    days = [d for d in
                            ["Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"]
                            if _text(rt, d, "false").lower() == "true"]
                    schedule.append({
                        "name":         _text(rt, "Name"),
                        "start":        _text(rt, "StartTime"),
                        "end":          _text(rt, "EndTime"),
                        "reversionary": _text(rt, "Reversionary", "false").lower() == "true",
                        "days":         days,
                    })
            plans.append({
                "plan_num":       i + 1,
                "name":           _text(aset, "Name"),
                "description":    _text(aset, "Description"),
                "trigger_det":    trig_det,
                "schedule":       schedule,
                "reversionary":   any(s["reversionary"] for s in schedule),
            })

    return {
        "info":       info,
        "phases":     phases,
        "stages":     stages,
        "links":      links,
        "detectors":  detectors,
        "confirms":   confirms,
        "matrix":     matrix,
        "plans":      plans,
        "filename":   os.path.basename(path),
    }


# ── Timetable-only parser (for Python-side TOD plan switching) ────────────────

_DAY_IDX = {
    "Monday": 0, "Tuesday": 1, "Wednesday": 2, "Thursday": 3,
    "Friday": 4, "Saturday": 5, "Sunday": 6,
}

def parse_timetable(path: str) -> list:
    """
    Parse data plan timetable from root AnalysisSets.
    Returns a list (plan index = plan_num-1) of dicts:
      { plan_num, reversionary, trigger_det,
        schedule: [{days:[0-6], start_min, end_min}] }
    Returns [] if no timetable data found.
    """
    if not os.path.exists(path):
        return []
    try:
        root = ET.parse(path).getroot()
    except ET.ParseError:
        return []

    asets_el = _find(root, "AnalysisSets")
    if asets_el is None:
        return []

    def _to_min(t):
        try:
            parts = t.split(":")
            return int(parts[0]) * 60 + int(parts[1])
        except Exception:
            return 0

    plans = []
    for i, aset in enumerate(_findall(asets_el, "AnalysisSet")):
        trig_present = _text(aset, "TriggerDetectorPresent", "false").lower() == "true"
        trig_det     = _int(aset, "TriggerDetector") if trig_present else None

        schedule = []
        rts_el = _find(aset, "RunningTimes")
        if rts_el is not None:
            for rt in _findall(rts_el, "RunningTime"):
                days = [v for k, v in _DAY_IDX.items()
                        if _text(rt, k, "false").lower() == "true"]
                rev = _text(rt, "Reversionary", "false").lower() == "true"
                schedule.append({
                    "days":      days,
                    "start_min": _to_min(_text(rt, "StartTime", "00:00:00")),
                    "end_min":   _to_min(_text(rt, "EndTime",   "00:00:00")),
                    "reversionary": rev,
                })

        plans.append({
            "plan_num":     i + 1,
            "trigger_det":  trig_det,
            "reversionary": any(s["reversionary"] for s in schedule),
            "schedule":     schedule,
        })

    return plans


def validate_sc_rules(path: str) -> None:
    """
    Scan the .mxds XML for Special Conditioning rule IDs and raise DatasetError
    if any ControllerStream has non-sequential IDs (gaps cause a kernel NULL-ptr crash).
    """
    try:
        with open(path, encoding="utf-8", errors="ignore") as f:
            xml = f.read()
    except OSError:
        return  # can't read — let the kernel surface its own error

    for cs_match in re.finditer(
        r'<ControllerStream>.*?<ID>(.*?)</ID>.*?<SpecialConditioning>(.*?)</SpecialConditioning>',
        xml, re.DOTALL,
    ):
        stream_id = cs_match.group(1).strip()
        sc_block  = cs_match.group(2)
        rule_ids  = [int(m) for m in re.findall(r'<Rule>\s*<ID>(\d+)</ID>', sc_block)]

        if not rule_ids:
            continue

        expected = list(range(1, len(rule_ids) + 1))
        if sorted(rule_ids) != expected:
            missing = sorted(set(expected) - set(rule_ids))
            logger.error(
                "Dataset rejected — ControllerStream %s has SC rule ID gaps: "
                "found %s, missing %s. Renumber in MOVA Tools.",
                stream_id, sorted(rule_ids), missing,
            )
            raise DatasetError(
                f"Dataset rejected: ControllerStream '{stream_id}' has "
                f"non-sequential Special Conditioning rule IDs "
                f"(found: {sorted(rule_ids)}, missing: {missing}). "
                f"Renumber rules sequentially in MOVA Tools before loading."
            )
