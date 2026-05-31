// Create an enum for looking up dictionaries and titles for tables
function tableNames() {
      var values = {
          MOVASITEDETAILS: "Mova site data details",
          SITEDIMS: "Site dimensions and parameters",
          STAGES: "Stage maximums and alerts",
          DETECTORS: "Lane detector data",
          GENERALLANEDATA: "General lane data",
          CONSTANTS: "Constant Junction data",
          LINKS: "Link data",
          LANESBONUS: "Lane Bonus data",
          LINKSBONUS: "Link Bonus data",
          LANES: "Derived Lane data",
          SATTIMES: "Saturation flow time periods",
          ERRORS: "ERROR VALUES",
          LGREEN: "LGREEN",
          SDCODE: "SDCODE",
          LPHASE: "LPHASE",
          CHANGE: "Stage Change matrix",
          DATASETTRIGGERING: "Dataset Triggering",
          LINKPFACIL: "Link PFACIL codes",
          TIMEDEPCHANGES: "Time dependent site data changes",
          PHONENO: "Phone number",
          SATFLOWSUMMARY: "Saturation flow",
          STLOSTSUMMARY: "Start-up lost-time (STLOST)",
          SATFLOWDETAILS: "Saturation flow (Verbose)",
          STLOSTDETAILS: "STLOST (Verbose)"
        }
 
        return values;
}

function SatFlowSTLost() {
    var values = {
        SATFLOW: "satFlow",
        STLOST: "STLOST"
    }
    return values;
}

function TableDictionary(data,tableType)
{
    var IsEmergency = JSON.parse(sessionStorage.getItem("isEmergency"));
    var dict = {};
    switch (tableType) {
        case (tableNames().MOVASITEDETAILS):
            dict["Filename"] = data.FileName;
            dict["Title"] = data.Title;
            dict["Version"] = data.Version;
            dict["Created"] = data.Created_Date + data.Created_Time;
            dict["TMA period"] = data.TMA_Period;
            break;
        case (tableNames().SITEDIMS):
            dict["STAGES"] = data.STAGES;
            dict["NLANES"] = data.NLANES;
            dict["NLINKS"] = data.NLINKS;
            dict["MAINST"] = data.MAINST;
            dict["REVTIM"] = data.REVTIM;
            dict["TOTALG"] = data.TOTALG;
            dict["DETON"] = data.DETON;
            dict["STAGON"] = data.STAGON;
            dict["PHASON"] = data.PHASON;
            dict["STGDEM"] = data.STGDEM;
            break;
        case (tableNames().STAGES):
            dict["MIN"] = data.MIN;
            dict["MAX"] = data.MAX;
            dict["LOWMAX"] = data.LOWMAX;
            if (IsEmergency) dict["EMXMAX"] = data.EMXMAX;
            if (IsEmergency) dict["PRXMAX"] = data.PRXMAX;
            dict["PEDMAX1"] = data.PEDMAX1;
            dict["PEDMAX2"] = data.PEDMAX2;
            if (IsEmergency) dict["BUSMAX1"] = data.BUSMAX1;
            if (IsEmergency) dict["BUSMAX2"] = data.BUSMAX2;
            dict["OCCUALon"] = data.OCCUAL_On;
            dict["OCCUALoff"] = data.OCCUAL_Off;
            break;
        case (tableNames().DETECTORS):
            dict["X"] = data.X;
            dict["DX"] = data.DX;
            dict["IN"] = data.IN;
            dict["DIN"] = data.DIN;
            dict["OUT"] = data.OUT;
            dict["IN-SINK"] = data.IN_SINK;
            dict["IN-SINK2"] = data.IN_SINK2;
            dict["COMB-X"] = data.COMB_X;
            dict["X-SINK"] = data.X_SINK;
            dict["X-SINK2"] = data.X_SINK2;
            dict["STOP-LINE"] = data.STOP_LINE;
            dict["ALT-UP X"] = data.ALT_UP;
            dict["ALT-DOWN X"] = data.ALT_DOWN;
            dict["ASSOC DET 1"] = data.ASOCC_DET_1;
            dict["ASSOC DET 2"] = data.ASOCC_DET_2;
            break;
        case (tableNames().GENERALLANEDATA):
            dict["DSHORT"] = data.DSHORT;
            dict["CSPEED"] = data.CSPEED;
            dict["SATINC"] = data.SATINC;
            dict["STLOST"] = data.STLOST;
            dict["COMTIM"] = data.COMTIM;
            dict["LANEWF"] = data.LANEWF;
            dict["USESF"]  = data.USESF;
            dict["USEST"]  = data.USEST;
            break;
        case (tableNames().CONSTANTS):
            dict["NDETS"]  = data.NDETS;
            dict["SCAN"]   = data.SCAN;
            dict["MINEXT"] = data.MINEXT;
            dict["MAXEXT"] = data.MAXEXT;
            dict["ADDGAP"] = data.ADDGAP;
            dict["SUBGAP"] = data.SUBGAP;
            break;
        case (tableNames().LINKS):
            dict["LTYPE"] = data.LTYPE;
            dict["LMIN"] = data.LMIN;
            dict["LLATOT"] = data.LLATOT;
            dict["LLANES1"] = ExtractCol(data.LLANES, 1);
            dict["LLANES2"] = ExtractCol(data.LLANES, 2);
            dict["LLANES3"] = ExtractCol(data.LLANES, 3);
            dict["ESLMAX"] = data.ESLMAX;
            dict["LOSTIM"] = data.LOSTIM;
            dict["STOPEN"] = data.STOPEN;
            dict["WAITCH"] = data.WAITCH;
            dict["MIXOUT"] = data.MIXOUT;
            if (IsEmergency) dict["MXCALL"] = data.MXCALL;
            if (IsEmergency) dict["MXCNCL"] = data.MXCNCL;
            dict["SHORTL"] = data.SHORTL;
            dict["NMAINL"] = data.NMAINL;
            if (IsEmergency) dict["EPDET1"] = ExtractCol(data.EPDET, 1);
            if (IsEmergency) dict["EPEXT1"] = ExtractCol(data.EPEXT, 1);
            if (IsEmergency) dict["Cruise time 1"] = data.EPCRT1;              
            if (IsEmergency) dict["EPDET2"] = ExtractCol(data.EPDET, 2);
            if (IsEmergency) dict["EPEXT2"] = ExtractCol(data.EPEXT, 2);
            if (IsEmergency) dict["Cruise time 2"] = data.EPCRT2;             
            if (IsEmergency) dict["HOLDTM"] = data.HOLDTM;
            if (IsEmergency) dict["CANDET"] = data.CANDET;   
            if (IsEmergency) dict["BUSWGT"] = data.BUSWGT; 
            break;
        case (tableNames().LANESBONUS):
            dict["Bonus BONBC"] = data.BONBC;
            break;
        case (tableNames().LINKSBONUS):
            dict["BONTIM"] = data.BONTIM;
            dict["BONCUT"] = data.BONCUT;
            break;
        case (tableNames().LANES):
            dict["MAXMIN"] = data.MAXMIN;
            dict["CRUSIN"] = data.CRUSIN;
            dict["CRUSX"] = data.CRUSX;
            dict["MOVQIN"] = data.MOVQIN;
            dict["MOVEQX"] = data.MOVEQX;
            dict["QMINON"] = data.QMINON;
            dict["XOSAT"] = data.XOSAT;
            dict["OSATTM"] = data.OSATTM;
            dict["OSATCC"] = data.OSATCC;
            dict["GAMBER"] = data.GAMBER;
            dict["SATGAP"] = data.SATGAP;
            dict["CRITG"] = data.CRITG;
            dict["PCRITG"] = data.PCRITG;
            dict["RCRITG"] = data.RCRITG;
            dict["OSATCX"] = data.OSATCX;
            dict["OSATAL"] = data.OSATAL;
            dict["EXITAL"] = data.EXITAL;
            break;
        case (tableNames().SATTIMES):
            var satFlowTimes = processSatTimesArray(data.SatFlowTimePeriods);
            dict["Start"] = satFlowTimes[0];
            dict["Stop"] = satFlowTimes[1];
            sessionStorage.setItem("SatFlowPeriods", JSON.stringify(satFlowTimes));
            break;
        case (tableNames().ERRORS):
            dict["ERROR NUMBER"] = data.ErrorValues[0];
            dict["ERROR VALUE"] = data.ErrorValues[1];
            break;
        case (tableNames().LGREEN):
            dict["LGREEN"] = data.LGREEN;
            break;
        case (tableNames().SDCODE):
            dict["SDCODE"] = data.SDCODE;
            break;
        case (tableNames().LPHASE):
            dict["LPHASE"] = data.LPHASE;
            break;
        case (tableNames().CHANGE):
            dict["CHANGE"] = data.CHANGE;
            break;
        case (tableNames().DATASETTRIGGERING):
            dict["DATATRIGGER"] = data.DatasetTriggering;
            break;
        case (tableNames().LINKPFACIL):
            dict["LINKPFACIL"] = data.PFACILM;
            break;
        case (tableNames().TIMEDEPCHANGES):
            dict["TIMEDEP"] = processTimeDependentData(data.DATTIM, data.DATNUM);
            break;
        case (tableNames().PHONENO):
            dict["NO"] = data.TEL;
            break;
        default:
            break;
    }

    return dict;
}


function ExtractCol(dataVar,colno)
{//Extract column of data
    var dataCol = new Array(dataVar.length);
    for (n = 0; n < dataVar.length; n++) {
        dataCol[n] = dataVar[n][colno - 1];
    }
    return dataCol;
}