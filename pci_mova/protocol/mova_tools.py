"""
pci_mova.protocol.mova_tools
TCP listener implementing the AML wire protocol used by MOVA Tools M8.

Wire framing (from aml_handler.c):
  STX(0x02) + ascii_decimal_size + ETX(0x03) + json_body_of_N_bytes

Messages:
  Request:  {"MessageType": "ReqXxx", ...params...}
  Response: {"MessageType": "RspXxx", ...fields...}

All integer/boolean values are serialised as JSON strings ("T"/"F" for bools,
decimal strings for numbers) — matching the kernel's aml_encode_message_json().
Arrays are comma-separated strings: "T,F,T,F".
"""

import asyncio
import json
import logging
import os
import datetime
from typing import TYPE_CHECKING, Optional

from pci_mova.config import MOVA_TOOLS_BASE_PORT, MOVA_TOOLS_TIMEOUT

if TYPE_CHECKING:
    from pci_mova.core.stream import MovaStream

logger = logging.getLogger(__name__)

STX = b'\x02'
ETX = b'\x03'
MAX_MSG_SIZE = 4096


# ── AML framing ───────────────────────────────────────────────────────────────

async def _read_aml(reader: asyncio.StreamReader, timeout: float) -> bytes | None:
    """Read one AML framed message.  Returns the JSON body or None on EOF/error."""
    try:
        # Scan for STX — discard junk bytes
        while True:
            b = await asyncio.wait_for(reader.read(1), timeout=timeout)
            if not b:
                return None
            if b == STX:
                break

        # Read ASCII size digits until ETX
        size_buf = bytearray()
        while True:
            b = await asyncio.wait_for(reader.read(1), timeout=timeout)
            if not b:
                return None
            if b == ETX:
                break
            size_buf.extend(b)

        size = int(size_buf.decode('ascii'))
        if size <= 0 or size > MAX_MSG_SIZE:
            return None

        body = await asyncio.wait_for(reader.readexactly(size), timeout=timeout)
        return body

    except (asyncio.TimeoutError, asyncio.IncompleteReadError, ValueError, UnicodeDecodeError):
        return None


def _wrap_aml(obj: dict) -> bytes:
    """Serialise dict to JSON and wrap in AML framing."""
    body = json.dumps(obj, separators=(',', ':')).encode('utf-8')
    return STX + str(len(body)).encode('ascii') + ETX + body


def _crc32_mova(data: bytes) -> int:
    """CRC32 matching the kernel's calc_crc32 — skips CR (13) and LF (10) bytes."""
    crc = 0xFFFFFFFF
    for byte in data:
        if byte in (10, 13):
            continue
        crc ^= byte
        for _ in range(8):
            mask = -(crc & 1) & 0xFFFFFFFF
            crc = ((crc >> 1) ^ (0xEDB88320 & mask)) & 0xFFFFFFFF
    return (~crc) & 0xFFFFFFFF


def _p(msg: dict, key: str, default=None):
    """Read a param from an AML message — checks Params sub-object first, then top-level."""
    params = msg.get("Params")
    if isinstance(params, dict) and key in params:
        return params[key]
    return msg.get(key, default)


def _bits(values) -> str:
    """Convert iterable of bool/int to comma-separated T/F string."""
    return ','.join('T' if v else 'F' for v in values)


def _ints(values) -> str:
    """Convert iterable of int to comma-separated string."""
    return ','.join(str(int(v)) for v in values)


# ── Session ───────────────────────────────────────────────────────────────────

class MovaToolsSession:
    """Handles one MOVA Tools connection on the AML protocol."""

    def __init__(self, stream: "MovaStream", reader: asyncio.StreamReader,
                 writer: asyncio.StreamWriter) -> None:
        self._stream = stream
        self._reader = reader
        self._writer = writer
        peer = writer.get_extra_info("peername")
        self._peer = f"{peer[0]}:{peer[1]}" if peer else "unknown"
        logger.info("Stream %d: MOVA Tools connected from %s", stream.stream_id, self._peer)

    async def handle(self) -> None:
        try:
            while True:
                raw = await _read_aml(self._reader, timeout=MOVA_TOOLS_TIMEOUT)
                if raw is None:
                    logger.warning("Stream %d: MOVA Tools idle timeout or EOF — disconnecting %s",
                                   self._stream.stream_id, self._peer)
                    break
                try:
                    msg = json.loads(raw)
                except json.JSONDecodeError:
                    logger.warning("Stream %d: malformed AML body from %s: %r",
                                   self._stream.stream_id, self._peer, raw[:80])
                    continue
                try:
                    await self._dispatch(msg)
                except Exception as exc:
                    logger.exception("Stream %d: dispatch error for %s: %s",
                                     self._stream.stream_id, msg.get("MessageType","?"), exc)

        except (ConnectionResetError, BrokenPipeError, OSError):
            pass
        finally:
            try:
                self._writer.close()
            except Exception:
                pass
            logger.info("Stream %d: MOVA Tools disconnected (%s)",
                        self._stream.stream_id, self._peer)

    async def _send(self, obj: dict) -> None:
        """Send a response. Automatically wraps non-MessageType fields into Params:{}.
        Format: {"MessageType":"Rsp...","Params":{"Field":val,...}}"""
        msg_type = obj.get("MessageType", "")
        params   = {k: v for k, v in obj.items() if k != "MessageType"}
        out = {"MessageType": msg_type}
        if params:
            out["Params"] = params
        try:
            self._writer.write(_wrap_aml(out))
            await self._writer.drain()
        except Exception as exc:
            logger.debug("Stream %d: send failed: %s", self._stream.stream_id, exc)

    async def _dispatch(self, msg: dict) -> None:
        mt = msg.get("MessageType", "")
        logger.debug("Stream %d: AML Req %s", self._stream.stream_id, mt)

        s  = self._stream
        b  = s.buffers
        k  = s.kernel
        d  = s.derived

        match mt:

            case "ReqMOVATime":
                now = datetime.datetime.now().strftime("%Y-%m-%dT%H:%M:%SZ")
                await self._send({"MessageType": "RspMOVATime", "DateTime": now})

            case "ReqOperationFlags":
                io         = b.io
                crb        = "T" if b.crb else "F"
                mova_en    = "T" if (len(io) > 27 and io[27]) else "F"
                on_ctrl    = "T" if (len(io) > 19 and io[19]) else "F"
                multi      = "T" if (len(io) > 30 and io[30]) else "F"
                ec         = str(int(io[16]) if len(io) > 16 else 0)
                wc         = str(b.warmup_counter if b.warmup_counter is not None else -1)
                ds         = str(b.kernel_demanded_stage if hasattr(b, 'kernel_demanded_stage') else 0)
                rsp = {
                    "MessageType":    "RspOperationFlags",
                    "CRB":            crb,
                    "IsMOVAEnabled":  mova_en,
                    "IsOnControl":    on_ctrl,
                    "IsMultiStage":   multi,
                    "ErrorCount":     ec,
                    "Warmup":         wc,
                    "DemandedStageNum": ds,
                }
                if not hasattr(self, '_opflags_logged'):
                    self._opflags_logged = True
                    logger.info("Stream %d: RspOperationFlags sample — %s",
                                s.stream_id, {k:v for k,v in rsp.items() if k!='MessageType'})
                await self._send(rsp)

            case "ReqKernelVersion":
                ver = k.kernel_version if k and k.kernel_version else "M8.0.0.435"
                await self._send({"MessageType": "RspKernelVersion", "Version": ver})

            case "ReqForceBits":
                from pci_mova.core.model.buffers import DOUT_FORCE_START, MAX_FORCE, DOUT_HI
                forces = b.dout[DOUT_FORCE_START:DOUT_FORCE_START + MAX_FORCE]
                hi     = bool(b.dout[DOUT_HI]) if len(b.dout) > DOUT_HI else False
                await self._send({
                    "MessageType": "RspForceBits",
                    "ForceBits":   _bits(forces),
                    "TakeOverBit": "T" if hi else "F",
                    "HurryInhibit": "F",
                })

            case "ReqOutputChannelStatus":
                outs = list(b.special_outputs) if hasattr(b, 'special_outputs') else []
                await self._send({
                    "MessageType": "RspOutputChans",
                    "OutputChans": _bits(outs),
                })

            case "ReqConfirmBits":
                # confin[0..30] = stage confirms; confin[31] = CRB (excluded)
                confs = list(b.confin[:-1]) if hasattr(b, 'confin') else []  # exclude CRB at index 31
                await self._send({
                    "MessageType": "RspConfirmBits",
                    "ConfirmBits": _bits(confs),
                })

            case "ReqRawDetectorsStatus":
                dets = list(b.detsin) if hasattr(b, 'detsin') else []
                await self._send({
                    "MessageType": "RspRawDetectorsStatus",
                    "RawStatus":   _bits(dets),
                })

            case "ReqDetectorsStatus":
                dets = b.kernel_deton if hasattr(b, 'kernel_deton') else (list(b.detsin) if hasattr(b, 'detsin') else [])
                await self._send({
                    "MessageType": "RspDetectorsStatus",
                    "Status":      _bits(dets),
                })

            case "ReqDetectorsSusStatus":
                sus = getattr(d, 'suspect_dets', []) or []
                await self._send({
                    "MessageType": "RspDetectorsSusStatus",
                    "SusStatus":   _ints(sus) if sus else "0",
                })

            case "ReqLastMessageIndex":
                if k and k._lib is not None and not k._simulation:
                    cur = k._lib.MI_get_current_msg_num()
                    # current_msg_num is 1-based next-to-write; last written = (cur - 2) mod 50
                    last = max(0, (cur - 2) % 50)
                else:
                    last = 0
                await self._send({
                    "MessageType": "RspLastMessageIndex",
                    "Index":       str(last),
                })

            case "ReqMessageByIndex":
                index = int(_p(msg, "Index", 0))
                await self._send_message_by_index(index)

            case "ReqErrorCount":
                ec = 0
                if k and k._lib is not None and not k._simulation:
                    try:
                        raw_count = k._lib.MI_get_error_count()
                        ec = max(0, raw_count - 1)  # 1-based, 1 means 0 errors
                    except Exception:
                        pass
                await self._send({
                    "MessageType": "RspErrorCount",
                    "Count":       str(ec),
                })

            case "ReqErrorDetail":
                error_index = int(_p(msg, "ErrorIndex", 1))
                await self._send_error_detail(error_index)

            case "ReqClearErrorLog":
                # No MI_clear_error_log — acknowledge only
                await self._send({"MessageType": "RspClearErrorLog"})

            case "ReqLaneData":
                lane_id = int(_p(msg, "ID", 1))
                await self._send_lane_data(lane_id)

            case "ReqLinkData":
                link_id = int(_p(msg, "ID", 1))
                await self._send_link_data(link_id)

            case "ReqCheckConnectedToRightController":
                # Always accept — MOVA Tools checks its configured controller ID matches
                # the running controller. We accept any connection.
                await self._send({"MessageType": "RspCheckConnectedToRightController", "IsOk": "T"})

            case "ReqLanesCount":
                nla = d.no_lanes if d else 0
                await self._send({"MessageType": "RspLanesCount", "Count": str(nla)})

            case "ReqCurrentActiveDataPlanIndex":
                plan = 0
                if k and k._lib and not k._simulation:
                    try: plan = k._lib.MI_get_active_plan()
                    except Exception: pass
                await self._send({"MessageType": "RspCurrentActiveDataPlanIndex", "PlanIndex": str(plan)})

            case "ReqActivateDataPlan":
                plan_idx = int(_p(msg, "PlanIndex", 1))
                ok = "F"
                if k and k._lib and not k._simulation:
                    try:
                        if k._lib.MI_switch_plan(plan_idx):
                            ok = "T"
                    except Exception:
                        pass
                await self._send({"MessageType": "RspActivateDataPlan", "IsSuccessful": ok})

            case "ReqRemoveCurrentToD":
                # We don't support removing TOD — acknowledge only
                await self._send({"MessageType": "RspRemoveCurrentToD"})

            case "ReqIsToDRemovedByUser":
                await self._send({"MessageType": "RspIsToDRemovedByUser", "IsRemoved": "F"})

            case "ReqDataPlanTriggeringStatus":
                await self._send({"MessageType": "RspDataPlanTriggeringStatus", "IsEnabled": "F"})

            case "ReqDataPlanTriggeringUpdate":
                await self._send({"MessageType": "RspDataPlanTriggeringUpdate"})

            case "ReqMOVAEnabledFlagSetting":
                value = _p(msg, "Value", "F")
                enabled = (str(value).upper() in ("T", "TRUE", "1"))
                b.io[27] = 1 if enabled else 0
                if k and k._lib and not k._simulation:
                    k._lib.MI_set_io_flag(27, 1 if enabled else 0)
                if not enabled and k:
                    k.set_warmup_counter(-1)
                await self._send({
                    "MessageType": "RspMOVAEnabledFlagSetting",
                    "IsOk": "T", "FailureReason": "0",
                })

            case "ReqOnControlFlagSetting":
                # ON_CONTROL is kernel-owned — reject manual set to 1
                value = _p(msg, "Value", "F")
                ok = "T" if str(value).upper() not in ("T", "TRUE", "1") else "F"
                await self._send({
                    "MessageType": "RspOnControlFlagSetting",
                    "IsOk": ok, "FailureReason": "0" if ok == "T" else "1",
                })

            case "ReqResetErrorCount":
                try:
                    was_running = s._running.is_set()
                    s.stop()
                    if k and k._lib and not k._simulation:
                        k.set_io_flag(16, 0)  # ERROR_COUNT = 0
                        k.set_io_flag(27, 0)  # MOVA_ON = 0
                    s.buffers.io[16] = 0
                    s.buffers.io[27] = 0
                    s._prev_ec = 0
                    if was_running and s._dataset is not None:
                        s.buffers.io[27] = 1
                        s.start()
                except Exception as exc:
                    logger.warning("Stream %d: ReqResetErrorCount error: %s", s.stream_id, exc)
                await self._send({"MessageType": "RspResetErrorCount"})

            case "ReqForceStage":
                stage = int(_p(msg, "StageNum", 0))
                ok = "F"
                try:
                    s.force_stage(stage)
                    ok = "T"
                except Exception:
                    pass
                await self._send({"MessageType": "RspForceStage", "IsOk": ok, "FailureReason": "0"})

            case "ReqCancelForcedStage":
                try:
                    s.force_stage(0)
                except Exception:
                    pass
                await self._send({"MessageType": "RspCancelForcedStage", "IsOk": "T"})

            case "ReqRetrieveCurrentDataset":
                ds = s._dataset
                if ds and ds.source_path and os.path.isfile(ds.source_path):
                    fname = os.path.basename(ds.source_path)
                    fsize = os.path.getsize(ds.source_path)
                else:
                    fname, fsize = "", 0
                await self._send({
                    "MessageType":  "RspRetrieveCurrentDataset",
                    "IsSuccessful": "T" if fname else "T",  # always T — F breaks MOVA Tools bypass
                    "FileName":     fname,
                    "FileSize":     str(fsize),
                    "ControllerID": "1",
                })

            case "ReqLoadDatasetIntoMemory":
                ok = "F"
                if hasattr(self, '_transfer_data') and self._transfer_data:
                    try:
                        import tempfile
                        import shutil
                        from pci_mova.config import DATASET_DIR as _DDIR
                        from pci_mova.dataset.parser import load_all
                        from pci_mova.api.routes.dataset import _validate_sc_rules
                        fname = getattr(self, '_transfer_fname', 'transfer.mxds')
                        dest  = os.path.join(_DDIR, os.path.basename(fname))
                        with tempfile.NamedTemporaryFile(delete=False, suffix='.mxds') as tf:
                            tf.write(self._transfer_data)
                            tmp = tf.name
                        _validate_sc_rules(tmp)
                        all_streams = load_all(tmp)
                        shutil.move(tmp, dest)
                        for ds in all_streams.values():
                            ds.source_path = dest
                        # Load first stream into this instance
                        first_ds = next(iter(all_streams.values()))
                        if s.load_dataset(first_ds):
                            ok = "T"
                            logger.info("Stream %d: MOVA Tools loaded dataset '%s'",
                                        s.stream_id, fname)
                        self._transfer_data = None
                    except Exception as exc:
                        logger.warning("Stream %d: dataset load from MOVA Tools failed: %s",
                                       s.stream_id, exc)
                await self._send({"MessageType": "RspLoadDatasetIntoMemory", "IsSuccessful": ok})

            case "ReqCheckTransferedFileIntegrity":
                crc_in = int(_p(msg, "CRC32", 0))
                ok = "T"
                if hasattr(self, '_transfer_data') and self._transfer_data:
                    calculated = _crc32_mova(self._transfer_data)
                    ok = "T" if calculated == crc_in else "F"
                    logger.debug("Stream %d: CRC check — got %08X calc %08X → %s",
                                 s.stream_id, crc_in, calculated, ok)
                await self._send({"MessageType": "RspCheckTransferedFileIntegrity", "IsFileOk": ok})

            case "ReqSendDatasetFile":
                # Respond with empty AML JSON — MOVA Tools reads this as the response,
                # CRC check will fail (no file content), user gets "doesn't match" warning
                # and can bypass to proceed to monitoring mode.
                await self._send({"MessageType": "RspSendDatasetFile"})

            case "ReqSiteFullData" | "ReqSendTMA_LogsFile" \
                 | "ReqAllTMA_LogsLink" | "ReqAllTMA_LogsFile":
                await self._send({"MessageType": mt.replace("Req", "Rsp")})

            case "ReqCheckDatasetCompatibility":
                stages = int(_p(msg, "StagesCount", 0))
                links  = int(_p(msg, "LinksCount", 0))
                lanes  = int(_p(msg, "LanesCount", 0))
                ds_ok = (
                    d.no_stages == stages and
                    d.no_links  == links  and
                    d.no_lanes  == lanes
                ) if d.no_stages else True
                await self._send({
                    "MessageType": "RspCheckDatasetCompatibility",
                    "Compatible":  "T" if ds_ok else "F",
                })

            case "ReqMOVATimeSetting":
                # Acknowledge — we don't change system time
                await self._send({"MessageType": "RspMOVATimeSetting"})

            case "ReqAlertMonitoringFlags":
                await self._send({
                    "MessageType":    "RspAlertMonitoringFlags",
                    "BlockingStatus": "F",
                    "OversatStatus":  "F",
                    "OccupancyStatus":"F",
                })

            case "ReqAlertStatus":
                await self._send({
                    "MessageType": "RspAlertStatus",
                    "AlertStatus": "F",
                })

            case "ReqAlertMonitoringFlagsSetting":
                await self._send({"MessageType": "RspAlertMonitoringFlagsSetting"})

            # Sat flow / average flow — stub empty responses
            case "ReqSatFlowPeriod" | "ReqSatFlowDetails" | "ReqAvgFlow" | "ReqSTLOST_Details":
                await self._send({"MessageType": mt.replace("Req", "Rsp")})

            case "ReqDatasetTransfer":
                fname = _p(msg, "FileName", "dataset.mxds")
                fsize = int(_p(msg, "FileSize", 0))
                await self._send({
                    "MessageType":        "RspDatasetTransfer",
                    "IsReadyForTransfer": "T",
                    "FailureReason":      "0",
                })
                if fsize > 0:
                    try:
                        # Read raw file bytes directly — NOT AML-framed
                        raw = await asyncio.wait_for(
                            self._reader.readexactly(fsize), timeout=60.0)
                        self._transfer_data  = raw
                        self._transfer_fname = fname
                        logger.info("Stream %d: received %d bytes for '%s'",
                                    s.stream_id, len(raw), fname)
                        await self._send({"MessageType": "DatasetFileReceived"})
                    except Exception as exc:
                        logger.warning("Stream %d: dataset receive error: %s", s.stream_id, exc)
                        self._transfer_data = None

            case _:
                logger.debug("Stream %d: unhandled AML MessageType: %s",
                             self._stream.stream_id, mt)
                await self._send({"MessageType": "FailedToHandleReqMessage"})

    async def _send_message_by_index(self, index: int) -> None:
        """Read message at ring-buffer slot `index` and send as RspMessageByIndex."""
        k = self._stream.kernel
        if not k or k._lib is None or k._simulation:
            await self._send({"MessageType": "RspMessageByIndex"})
            return
        try:
            n_fields = 64
            raw = [k._lib.MI_get_msg_field(index, i) for i in range(n_fields)]
        except Exception:
            await self._send({"MessageType": "RspMessageByIndex"})
            return

        msg_type = raw[0]
        msg_sub  = raw[1]

        # Delegate to the C kernel's AML handler by calling the existing MI_* decode path.
        # We reproduce the response fields that MOVA Tools expects for each type.
        d = self._stream.derived

        if msg_type == 1 and msg_sub == 1:
            # Start of green: stage, time, lane flows
            stage  = str(raw[2])
            hh, mm, ss = raw[3], raw[4], raw[5]
            t_str  = f"{hh:02d}:{mm:02d}:{ss:02d}"
            nla    = d.no_lanes or 0
            flows  = ','.join(str(raw[6 + i]) for i in range(nla))
            jflow  = str(raw[6 + nla])
            lao    = str(raw[7 + nla])
            lam    = str(raw[8 + nla])
            cut    = str(raw[9 + nla])
            await self._send({
                "MessageType": "RspMessageByIndex",
                "MsgID": "M1.1",
                "StageNum": stage,
                "DemandTime": t_str,
                "SmoothFlowLane": flows,
                "TotalSmoothedFlow": jflow,
                "LambdaAtStageOver": lao,
                "LambdaAtJunction": lam,
                "CutMaxTimesMarker": cut,
            })

        elif msg_type == 1 and msg_sub == 2:
            ns  = d.no_stages or 0
            cyc = str(raw[2])
            dmx = ','.join(str(raw[3 + i]) for i in range(ns))
            await self._send({
                "MessageType": "RspMessageByIndex",
                "MsgID": "M1.2",
                "CycleTime": cyc,
                "DriftMax": dmx,
            })

        elif msg_type == 2:
            smin = str(raw[3])
            nl   = d.no_links or 0
            lmin = ','.join(str(raw[4 + i]) for i in range(nl))
            await self._send({
                "MessageType": "RspMessageByIndex",
                "MsgID": "M2",
                "SignalStateTimer": str(raw[2]),
                "VarMinGreenTime": smin,
                "LinkVarMinGreen": lmin,
            })

        elif msg_type == 4:
            nl = d.no_links or 0
            links_data = []
            li = 7
            for _ in range(nl):
                if li + 4 >= n_fields:
                    break
                links_data.append(f"{raw[li]},{raw[li+1]},{raw[li+2]},{raw[li+3]},{raw[li+4]}")
                li += 5
            await self._send({
                "MessageType": "RspMessageByIndex",
                "MsgID": "M4",
                "TimeIntoGreen": str(raw[2]),
                "NextStageNum":  str(raw[3]),
                "Benefit":       str(raw[4]),
                "Disbenefit":    str(raw[5]),
                "FutureRed":     str(raw[6]),
                "LinksData":     '|'.join(links_data),
            })

        elif msg_type == 5:
            ns  = d.no_stages or 0
            nl  = d.no_lanes  or 0
            dem = ','.join(str(raw[7 + i]) for i in range(ns))
            rcx = ','.join(str(raw[7 + ns + i]) for i in range(nl))
            rsp = {
                "MessageType":   "RspMessageByIndex",
                "MsgID":         f"M5.{msg_sub}",
                "TimeIntoGreen": str(raw[2]),
                "NextStageNum":  str(raw[3]),
                "DemandTime":    f"{raw[4]:02d}:{raw[5]:02d}:{raw[6]:02d}",
                "LinksDemandMarker": dem,
                "LanesRedCount": rcx,
            }
            if msg_sub == 1:
                rsp.update({"Benefit": str(raw[7+ns+nl]), "Disbenefit": str(raw[8+ns+nl]),
                            "FutureRed": str(raw[9+ns+nl]), "CycleExpansionFactor": str(raw[10+ns+nl])})
            await self._send(rsp)

        elif msg_type == 6:
            ns  = d.no_stages or 0
            nl  = d.no_links  or 0
            dem = ','.join(str(raw[3 + i]) for i in range(ns))
            bon = ','.join(str(raw[3 + ns + i]) for i in range(nl))
            await self._send({
                "MessageType":   "RspMessageByIndex",
                "MsgID":         f"M6.{msg_sub}",
                "Seconds":       str(raw[2]),
                "DemandMarkers": dem,
                "LinkBonus":     bon,
            })

        else:
            await self._send({"MessageType": "RspMessageByIndex", "MsgID": f"M{msg_type}.{msg_sub}"})

    async def _send_error_detail(self, error_index: int) -> None:
        k = self._stream.kernel
        if not k or k._lib is None or k._simulation:
            await self._send({"MessageType": "RspErrorDetail", "ErrorID": "0",
                              "DateTime": "", "Add1": "0", "Add2": "0", "Add3": "0"})
            return
        try:
            # field 0: (10 + errorID) * 1000 + day; field 1: month*100+year; field 2: hh*100+mm; field 3: data
            f0 = k._lib.MI_get_error_store(error_index - 1, 0)
            f1 = k._lib.MI_get_error_store(error_index - 1, 1)
            f2 = k._lib.MI_get_error_store(error_index - 1, 2)
            f3 = k._lib.MI_get_error_store(error_index - 1, 3)
            error_id = (f0 // 1000) - 10
            day      = f0 % 1000
            month    = f1 // 100
            year     = 2000 + (f1 % 100)
            hh       = f2 // 100
            mm       = f2 % 100
            dt_str   = f"{year:04d}-{month:02d}-{day:02d}T{hh:02d}:{mm:02d}:00Z"
            await self._send({
                "MessageType": "RspErrorDetail",
                "ErrorID":  str(error_id),
                "DateTime": dt_str,
                "Add1": str(f3), "Add2": "0", "Add3": "0",
            })
        except Exception:
            await self._send({"MessageType": "RspErrorDetail", "ErrorID": "0",
                              "DateTime": "", "Add1": "0", "Add2": "0", "Add3": "0"})

    async def _send_lane_data(self, lane_id: int) -> None:
        k = self._stream.kernel
        d = self._stream.derived
        nla = d.no_lanes if d else 0
        if k and k._lib and not k._simulation and 1 <= lane_id <= nla:
            ln = lane_id - 1
            sf = d.smoothed_flow[ln] if d.smoothed_flow else 0
            rx = k._lib.MI_get_tma_lane_xflow(ln)
            ri = k._lib.MI_get_tma_lane_inflow(ln)
        else:
            sf = rx = ri = 0
        await self._send({
            "MessageType":     "RspLaneData",
            "RedCountIN":      str(ri),
            "RedCountX":       str(rx),
            "SFSmoothed":      str(sf),
            "SFLastCycle":     "0.00",
            "ShiftRegister":   "F",
            "OversatCounter":  "0",
            "Endsat":          "0",
            "QBeyondINDET":    "0",
            "LeftOverVehs":    "0",
        })

    async def _send_link_data(self, link_id: int) -> None:
        d = self._stream.derived
        nl = d.no_links if d else 0
        if d and 1 <= link_id <= nl:
            li = link_id - 1
            bn  = d.link_bonus[li]   if d.link_bonus   else 0
            smf = d.smoothed_flow[0] if d.smoothed_flow else 0  # junction proxy
            es  = d.endsat_link[li]  if d.endsat_link   else 0
            nb  = d.link_netben[li]  if d.link_netben   else 0
            af  = d.link_ben[li]     if d.link_ben      else 0
            fr  = d.link_fred[li]    if d.link_fred      else 0
            xg  = d.link_xg[li]     if d.link_xg        else 0
            ep_hold = d.ep_hold if d else 0
            ep_ext  = d.ep_ext  if d else 0
        else:
            bn = smf = es = nb = af = fr = xg = ep_hold = ep_ext = 0
        await self._send({
            "MessageType":   "RspLinkData",
            "BonusGreenTime": str(bn),
            "SmoothedFlow":   str(smf),
            "Endsat":         str(es),
            "DemandType":     "0",
            "NetBenFlow":     str(nb),
            "ActualFlow":     str(af),
            "FutureRedTime":  str(fr),
            "ExtraGreenTime": str(xg),
            "EPHoldMarker":   str(ep_hold),
            "EPExtMarker":    str(ep_ext),
            "EPExtTimer":     "0",
        })


# ── Server ────────────────────────────────────────────────────────────────────

class MovaToolsServer:
    """
    Async TCP server — one instance per MOVA stream.
    Listens on MOVA_TOOLS_BASE_PORT + stream_id.
    """

    def __init__(self, stream: "MovaStream") -> None:
        self._stream = stream
        self._port   = MOVA_TOOLS_BASE_PORT + stream.stream_id
        self._server: Optional[asyncio.AbstractServer] = None

    async def start(self) -> None:
        self._server = await asyncio.start_server(
            self._on_connect,
            host="0.0.0.0",
            port=self._port,
            reuse_port=True,
        )
        logger.debug(
            "Stream %d: MOVA Tools server listening on port %d",
            self._stream.stream_id, self._port
        )

    async def stop(self) -> None:
        if self._server:
            self._server.close()
            await self._server.wait_closed()

    async def _on_connect(self, reader: asyncio.StreamReader,
                          writer: asyncio.StreamWriter) -> None:
        session = MovaToolsSession(self._stream, reader, writer)
        await session.handle()
