"use strict";
function MessagesDictionary(msgIn) {
    //// Javascript enums equivalent

    var MsgIDs = {
        M1_1: "M1.1",
        M1_2: "M1.2",
        M2_0: "M2",
        M3_1: "M3.1",
        M3_2: "M3.2",
        M4_0: "M4",
        M5_1: "M5.1",
        M5_2: "M5.2",
        M5_3: "M5.3",
        M5_4: "M5.4",
        M6_1: "M6.1",
        M6_2: "M6.2",
        M7_1: "M7.1"
    };
    return GetMessageText(msgIn);

    function GetMessageText(msgIn) {
        var msgID = msgIn.MsgID;

        switch (msgID) {
            case MsgIDs.M1_1:
                return String.format("S {0,1} {1} SMF {2}{3,4} SAT {4}{5}  LAM {6,2} {7,4} CUT {8,2}",
                      msgIn.StageNum,
                      msgIn.Time,
                      toGroups(msgIn.SmoothedFlow, 5, "{0,2} "),
                      msgIn.TotalSmoothedFlow,
                      toGroups(msgIn.OversatCounter, 6, "{0,1}"),
                      msgIn.MaxLaneOversatCounter,
                      msgIn.Lambda,
                      msgIn.TotalLambda,
                      msgIn.CutMaxTimesMarker
                      );
                break;
            case MsgIDs.M1_2:
                return String.format("SMCYC {0,3} DMX {1} SUS {2}",
                     msgIn.SmoothedCycleTime,
                     toGroups(msgIn.StagesDriftMax, 5, "{0,2} "),
                     formatDets(msgIn.DetsSusStatus)
                     );
                break;
            case MsgIDs.M2_0:
                return String.format("{0,3} SMIN {1,2} LMIN {2} RCX {3}",
                                   msgIn.TimeInGreen,
                                   msgIn.VarMinGreenTime,
                                   toGroups(msgIn.LinksVarMinGreenTime, 6, "{0,1} "),
                                   toGroups(msgIn.RedCountX, 5, "{0,1} ")
                                   );
                break;
            case MsgIDs.M3_1:
                if (msgIn.TimeInGreen == 65)
                {
                    var stop = 1;
                }
                return String.format("{0,3} NX {1,1}  ESLI {2} {3}",
                        msgIn.TimeInGreen,
                        msgIn.NextStageNum,
                        toGroups(msgIn.LinksEndsat, 6, "{0,1}"),
                        toLAGroups(msgIn.RelLanesNum, msgIn.RelLanesOldGap, msgIn.RelLanesCurrentGap, msgIn.RelLanesWasteTime)
                        );
                break;
            case MsgIDs.M3_2:
                return String.format("{0,3} NX {1,1}  hold {2,1}  ext {3,1}  xtimr {4,1}    lxtm {5}",
                              msgIn.TimeInGreen,
                              msgIn.NextStageNum,
                              msgIn.EPHoldMarker,
                              msgIn.EPExtMarker,
                              msgIn.EPExtMaxTimer,
                              toGroups(msgIn.LinksExtTimer, 6, "{0,1} ")
                              );
                break;
            case MsgIDs.M4_0:
                return String.format("{0,3} NX {1,1} BDR{2,3}{3,3}{4,4} {5}",
                     msgIn.TimeIntoGreen,
                     msgIn.NextStageNum,
                     msgIn.Benefit,
                     msgIn.Disbenefit,
                     msgIn.FutureRed,
                     toLKGroups(msgIn.LinksLosingRoW, msgIn.LinksNetBenefitFlowRate, msgIn.LinksActualFlowRate, msgIn.LinksFutureRed, msgIn.LinksExtraGreen)
                     );
                break;
            case MsgIDs.M5_1:
                return String.format("{0,3} NX {1,1} {2} OPT BDR {3,2} {4,2} {5,2} CF {6,2}  DEM {7}  RCX {8}",
                                    msgIn.TimeIntoGreen,
                                    msgIn.NextStageNum,
                                    msgIn.DemandTime,
                                    msgIn.Benefit,
                                    msgIn.Disbenefit,
                                    msgIn.FutureRed,
                                    msgIn.CycleExpansionFactor,
                                    toGroups(msgIn.LinksDemandMarker, 6, "{0,1}"),
                                    toGroups(msgIn.LanesRedCount, 5, "{0,1} ")
                                    );
                break;
            case MsgIDs.M5_2:
                if (parseInt(msgIn.PedDemandMarker) > 0) {
                    return String.format("{0,3} NX {1,1} {2} MAX CHANGE, PED MAX= {3,2} DEM {4}  RCX {5}",
                                msgIn.TimeIntoGreen,
                                msgIn.NextStageNum,
                                msgIn.DemandTime,
                                msgIn.MaxChange,
                                toGroups(msgIn.LinksDemandMarker, 6, "{0,1}"),
                                toGroups(msgIn.LanesRedCount, 5, "{0,1} ")
                                );
                }
                else {
                    return String.format("{0,3} NX {1,1} {2} MAX CHANGE, DMX+MXP= {3,2} DEM {4}  RCX {5}",
                                msgIn.TimeIntoGreen,
                                msgIn.NextStageNum,
                                msgIn.DemandTime,
                                msgIn.MaxChange,
                                toGroups(msgIn.LinksDemandMarker, 6, "{0,1}"),
                                toGroups(msgIn.LanesRedCount, 5, "{0,1} ")
                                );
                }
                break;
            case MsgIDs.M5_3:
                return String.format("{0,3} NX {1,1} {2} OSAT+ES, PCS {3} DEM {4}  RCX {5}",
                                        msgIn.TimeIntoGreen,
                                        msgIn.NextStageNum,
                                        msgIn.DemandTime,
                                        toGroups(msgIn.MaxPercentChange, 1, "{0,1}"),
                                        toGroups(msgIn.LinksDemandMarker, 6, "{0,1}"),
                                        toGroups(msgIn.LanesRedCount, 5, "{0,1} ")
                                        );
                break;
            case MsgIDs.M5_4:
                return String.format("{0,3} NX {1,1} {2}  EM/PR TRUNC. ENX={3,2}   PNX={4,2}  DEM {5}  RCX {6}",
                                        msgIn.TimeIntoGreen,
                                        msgIn.NextStageNum,
                                        msgIn.DemandTime,
                                        msgIn.NextEmgStageToDemand,
                                        msgIn.NextPriStageToDemand,
                                        toGroups(msgIn.LinksDemandMarker, 6, "{0,1}"),
                                        toGroups(msgIn.LanesRedCount, 5, "{0,1} ")
                                        );
                break;
            case MsgIDs.M6_1:
                return String.format("   IG:{0,1}  SDEM {1} BON {2} RCIN {3}",
                                     msgIn.StartIG_Time,
                                     toGroups(msgIn.StagesDemandMarker, 5, "{0,1}"),
                                     toGroups(msgIn.LinksBonusGreen, 5, "{0,1} "),
                                     toGroups(msgIn.RedCountIN, 5, "{0,1} ")
                                     );
                break;
            case MsgIDs.M6_2:
                return String.format("   IG:{0,1}  EM/PR CHANGE. SDEM {1} TRUN {2} SKIP {3}",
                                     msgIn.MsgTimeInSecs,
                                     toGroups(msgIn.EPStageDemandMarker, 5, "{0,1}"),
                                     toGroups(msgIn.EPLinksTruncIndicator, 6, "{0,1} "),
                                     toGroups(msgIn.EPLinksSkipIndicator, 6, "{0,1} ")
                                     );
                break;
            case MsgIDs.M7_1:
                if ( JSON.parse(sessionStorage.getItem("SFMessageState"))) {
                    return String.format("SF: L{0,2} ExBl:{1} InQ:{2} XQ:{3} Min:{4} Full:{5} LQCnt:{6} LQSat:{7} SQSat:{8} RawSat:{9} RawSTL:{10} Flow:{11} Occ:{12}",
                                       msgIn.LaneID,
                                       toYN(msgIn.IsExitBlocked),
                                       toYN(msgIn.IN_DetStandingQ),
                                       toYN(msgIn.Q_OverX_Det),
                                       toYN(msgIn.IsSatFlowAfterVarMinGreen),
                                       toYN(msgIn.FullySat),
                                       msgIn.LongQ_SatCount,
                                       msgIn.LongQ_SatFlow,
                                       msgIn.ShortQ_SatFlow,
                                       msgIn.RawSatFlow,
                                       msgIn.RawSTLOST,
                                       msgIn.RawFlowRate,
                                       msgIn.SmoothedOcc
                                       );
                }
                else { return "";}
                break;
            default:
                return "Unrecognised message.";
                break;
        }
    }
}

function toYN(flag_str) {
    if (flag_str) {
        return "Y";
    }
    else if (!flag_str) {
        return "N";
    }
    else {
        return "?";
    }
}

function toGroups(data, groupSize, groupFormat) {
    var resultStr = "";
    for (i = 0; i < data.length; i++)
    {
        resultStr += String.format(groupFormat, data[i])
        if ((i+1) % groupSize == 0) {
            resultStr += "  ";
        }
    }
    return resultStr;
}

function formatDets(data) {
    var resultStr = "";
    for (i = 0; i < data.length; i++) {
        resultStr += data[i].toString();
        if ((i+1) % 6 == 0) {
            resultStr += " ";
        }
    }
    return resultStr
}

//4LA 16 12 0  4LA 16 12 0  4LA 16 12 0
function toLAGroups(lanesNum, lanesOldGap, lanesCurrentGap, lanesWasteTime) {
    var resultStr = "";
    for (i = 0; i < lanesNum.length; i++) {
        resultStr += String.format("{0,1}LA {1,1} {2,1} {3,1}  ",
                                        lanesNum[i],
                                        lanesOldGap[i],
                                        lanesCurrentGap[i],
                                        lanesWasteTime[i]
                                        );
    }
    return resultStr;
}

function toLKGroups(linksNum ,linksNetBenefitFlowRate,linksActualFlowRate, linksFutureRed, linksExtraGreenStr)
{
    var result_str = "";
    for (i = 0 ; i < linksNum.length; i++)
    {
        result_str += String.format("{0,2}LK{1,4}{2,4}{3,3}{4,3} ",
                                        linksNum[i],
                                        linksNetBenefitFlowRate[i],
                                        linksActualFlowRate[i],
                                        linksFutureRed[i],
                                        linksExtraGreenStr[i]);
    }
    return result_str;
}

