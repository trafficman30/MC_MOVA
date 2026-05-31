// Send JSON message to webserver
//********************************
function sendJSONMessage(xmlhttp, msg) {
    xmlhttp.open("POST", "/json-handler", true);
    xmlhttp.setRequestHeader("Content-Type", "application/json;charset=UTF-8");
    xmlhttp.send(JSON.stringify(msg));
}


// Fetch data from webserver
//**************************
function sendReqMOVATime() {
    var movaTime;
    var xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function () {
        if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
            // Mova time is to be collected at page load but then incremented through javascript thereafter
            movaTime = JSON.parse(xmlhttp.responseText).Params.DateTime;
            sessionStorage.setItem("movaTime", movaTime);
            sessionStorage.setItem("startTime", (new Date()));
            automateMovaTime();
        }
    };
    sendJSONMessage(xmlhttp, { StreamID: "1", MessageType: "ReqMOVATime" });
}

function sendReqKernelVersion() {
    var movaVersion;
    var xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function () {
        if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
            movaVersion = JSON.parse(xmlhttp.responseText).Params.Version;
            document.getElementById("movaVersion").innerHTML = movaVersion;
        }
    };
    sendJSONMessage(xmlhttp, { StreamID: "1", MessageType: "ReqKernelVersion" });
}

function sendReqDetectorsStatus() {
    var xmlhttp = new XMLHttpRequest();
    var detectorStatus = new Array();
    xmlhttp.onreadystatechange = function () {
        if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
            detectorStatus = JSON.parse(xmlhttp.responseText).Params.Status;
            document.getElementById("dets_status").innerHTML = detectorStatus;
            setBits("Channel", detectorStatus);
        }
        if (xmlhttp.responseText == "\{\"ConnectionLost\"\}") {
            $("#warningMessage").removeClass('hidden');
        }
    };
    sendJSONMessage(xmlhttp, { StreamID: "1", MessageType: "ReqDetectorsStatus" });
}

function sendReqConfirmBits() {
    var xmlhttp = new XMLHttpRequest();
    var confirmBits = new Array();
    xmlhttp.onreadystatechange = function () {
        if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
            confirmBits = JSON.parse(xmlhttp.responseText).Params.ConfirmBits;
            setBits("ConfirmBit", confirmBits);
            document.getElementById("confirm_bits").innerHTML = confirmBits;
        }
    };
    sendJSONMessage(xmlhttp, { StreamID: "1", MessageType: "ReqConfirmBits" });
}

function sendReqForceBits() {
    var xmlhttp = new XMLHttpRequest();
    var forceBits = new Array();
    xmlhttp.onreadystatechange = function () {
        if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
            forceBits = JSON.parse(xmlhttp.responseText).Params.ForceBits;
            forceBits[10] = JSON.parse(xmlhttp.responseText).Params.HurryInhibit;
            forceBits[11] = JSON.parse(xmlhttp.responseText).Params.TakeOverBit;
            setBits("ForceBit", forceBits);

            document.getElementById("force_bits").innerHTML = forceBits;
            document.getElementById("hurry_inhibit").innerHTML = forceBits[10];
            document.getElementById("take_over_bit").innerHTML = forceBits[11];
        }
    };
    sendJSONMessage(xmlhttp, { StreamID: "1", MessageType: "ReqForceBits" });
}

function sendReqOperationFlags() {
    var xmlhttp = new XMLHttpRequest();
    var mova; var control; var warmup;
    xmlhttp.onreadystatechange = function () {
        if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
            mova = JSON.parse(xmlhttp.responseText).Params.IsMOVAEnabled;
            control = JSON.parse(xmlhttp.responseText).Params.IsOnControl;
            warmup = JSON.parse(xmlhttp.responseText).Params.Warmup;

            document.getElementById("is_mova_enabled").innerHTML = mova;
            document.getElementById("is_on_control").innerHTML = control;
            document.getElementById("warm_up").innerHTML = warmup;

            setUIControls(mova, control, warmup);
        }
    };
    sendJSONMessage(xmlhttp, { StreamID: "1", MessageType: "ReqOperationFlags" });
}


function sendReqErrorCount() {
    var xmlhttp = new XMLHttpRequest();
    var cnt;
    xmlhttp.onreadystatechange = function () {
        if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
            cnt = JSON.parse(xmlhttp.responseText).Params.Count
            for (i = 1; i < cnt + 1; i++) {
                sendReqErrorDetail(i);
            }
            document.getElementById("error_count").innerHTML = cnt;
            document.getElementById("errorCountBox").innerHTML = cnt.toString();
        }
    };
    sendJSONMessage(xmlhttp, { StreamID: "1", MessageType: "ReqErrorCount" });
}

function sendReqErrorDetail(index) {
    var xmlhttp = new XMLHttpRequest();
    var err;
    var cnt;
    xmlhttp.onreadystatechange = function () {
        if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
            err = xmlhttp.responseText;
            document.getElementById("error_detail").innerHTML = err;
            addErrorToTable(xmlhttp.responseText);
        }
    };
    sendJSONMessage(xmlhttp, { StreamID: "1", MessageType: "ReqErrorDetail", Params: { ErrorIndex: index } });
}

function sendReqSiteFullData() {
    var xmlhttp = new XMLHttpRequest();
    var siteFullData;
    xmlhttp.onreadystatechange = function () {
        if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
            siteFullData = xmlhttp.responseText;
            document.getElementById("site_full_data").innerHTML = siteFullData;
            // Check if in an emergency state (if there is a '99' or '999' in the LTYPE array)
            // include instead of indexOf is the standard but not supported by IE yet
            if (JSON.parse(siteFullData).Params.LTYPE.indexOf(99) > -1 || JSON.parse(siteFullData).Params.LTYPE.indexOf(999) > -1)
            { sessionStorage.setItem("isEmergency", true); }
            else
            { sessionStorage.setItem("isEmergency", false); }
            addActiveData(xmlhttp.responseText);
        }
    };
    sendJSONMessage(xmlhttp, { StreamID: "1", MessageType: "ReqSiteFullData" });
}

function sendReqLanesCount() {
    var xmlhttp = new XMLHttpRequest();
    var laneCount;
    xmlhttp.onreadystatechange = function () {
        if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
            laneCount = JSON.parse(xmlhttp.responseText).Params.Count;
            document.getElementById("lanes_count").innerHTML = laneCount;
            addSatFlowAndSTLOST(laneCount);
        }
    };
    sendJSONMessage(xmlhttp, { StreamID: "1", MessageType: "ReqLanesCount" });
}

function sendReqSatFlowDetails(trSummary, trDetails, i, j) {
    var xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function () {
        if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
            document.getElementById("sat_flow_data").innerHTML += xmlhttp.responseText;
            setSatFlowData(trSummary, trDetails, JSON.parse(xmlhttp.responseText).Params)
        }
    };
    sendJSONMessage(xmlhttp, { StreamID: "1", MessageType: "ReqSatFlowDetails", Params: { LaneID: i, PeriodIndex: j } });
}


function sendReqSTLOSTDetails(trSummary, trDetails, i, j) {
    var xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function () {
        if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
            document.getElementById("stlost_data").innerHTML = xmlhttp.responseText;
            setSTLOSTData(trSummary, trDetails, JSON.parse(xmlhttp.responseText).Params)
        }
    };
    sendJSONMessage(xmlhttp, { StreamID: "1", MessageType: "ReqSTLOST_Details", Params: { LaneID: i, PeriodIndex: j } });
}


function sendReqAllTMALogs() {
    var xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function () {
        if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
            document.getElementById("tma_logs_link").href = JSON.parse(xmlhttp.responseText).Params.Link;
        }
    };
    sendJSONMessage(xmlhttp, { StreamID: "1", MessageType: "ReqAllTMA_LogsLink" });
}

function sendReqLastMessageIndexMessage() {
    var xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function () {
        if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
            var index = JSON.parse(xmlhttp.responseText).Params.Index;
            //          var safetyCounter = 0;
            sessionStorage.setItem("safetyCounter", 0);
            sessionStorage.setItem("mesgIndex", index);
            sendReqMessageByIndexMessage();
        }
    };
    sendJSONMessage(xmlhttp, { StreamID: "1", MessageType: "ReqLastMessageIndex" });
}


function sendReqMessageByIndexMessage() {
    var safetyCounter = JSON.parse(sessionStorage.getItem("safetyCounter"));
    var index = JSON.parse(sessionStorage.getItem("mesgIndex"));

    var xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function () {
        if (xmlhttp.readyState == 4 && xmlhttp.status == 200) {
            var safetyCounter = JSON.parse(sessionStorage.getItem("safetyCounter"));
            var index = JSON.parse(sessionStorage.getItem("mesgIndex"));

            if (safetyCounter < 150) {
                if (xmlhttp.responseText != "{\"MessageType\": \"RspMessageByIndex\"\,}") // maybe a better way of checking this not empty
                {
                    displayMessages(index, JSON.parse(xmlhttp.responseText).Params);
                    document.getElementById("messages").innerHTML += xmlhttp.responseText;

                    index += 1;
                    if (index % 50 == 0) {
                        index = 0; // reset index
                    }
                    sessionStorage.setItem("mesgIndex", index);

                    sendReqMessageByIndexMessage(index, safetyCounter);

                    safetyCounter += 1;
                    sessionStorage.setItem("safetyCounter", safetyCounter);
                }
                else {
                    sendReqMessageByIndexMessage(index, safetyCounter);// repeat if empty to attempt again
                }
            }
            else // Reset the Safety Counter  and message box and start again
            {
                processMessages();
            }
        }
    };
    sendJSONMessage(xmlhttp, { StreamID: "1", MessageType: "ReqMessageByIndex", Params: { Index: index } });
}