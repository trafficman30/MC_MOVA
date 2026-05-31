
// Pages display
//******************
function toggler(divId) {
    // Hide all first then bring back one want
    $("#indexcontainer").addClass('hidden');
    $("#onlinedatacontainer").addClass('hidden');
    $("#errorcontainer").addClass('hidden');
    $("#activedatasetcontainer").addClass('hidden');
    $("#tmalogscontainer").addClass('hidden');
    $("#satflowlogscontainer").addClass('hidden');
    $('#messagescontainer').addClass('hidden');
    // $("#" + divId).toggle();
    $("#" + divId).removeClass('hidden');

    setTitle(divId);
}

// Need to set title on page load
function setVisibleContainerTitle()
{ 
    if (!$("#onlinedatacontainer").is(":hidden")) { setTitle('onlinedatacontainer'); }
    if (!$("#errorcontainer").is(":hidden")) { setTitle('errorcontainer'); }
    if (!$("#activedatasetcontainer").is(":hidden")) { setTitle('activedatasetcontainer'); }
    if (!$("#tmalogscontainer").is(":hidden")) { setTitle('tmalogscontainer'); }
    if (!$("#satflowlogscontainer").is(":hidden")) { setTitle('satflowlogscontainer'); }
    if (!$("#messagescontainer").is(":hidden")) { setTitle('messagescontainer'); }
}
function setTitle(divId)
{
    // Set title
    var title = document.getElementById("pageText");
    title.innerHTML = "&nbsp&nbsp";
    switch (divId) {
        case ("indexcontainer"):
            title.innerHTML += "";
            break;
        case ("onlinedatacontainer"):
            title.innerHTML += "Online Data";
            break;
        case ("errorcontainer"):
            title.innerHTML += "Errors";
            break;
        case ("activedatasetcontainer"):
            title.innerHTML += "Active Data Set";
            break;
        case ("tmalogscontainer"):
            title.innerHTML += "TMA Logs";
            break;
        case ("satflowlogscontainer"):
            title.innerHTML += "SatFlow Logs";
            break;
        case ("messagescontainer"):
            title.innerHTML += "Messages";
            break;
        default:
            title.innerHTML += "";
            break;
    }
}

function onLoad()
{
    sortResizing();
}

// Handle click events
//********************
$(function () {
    $(document).click(function () {
        $('.collapse').collapse('hide'); // collapse the menu when click elsewhere
    });
});
$(function () {
    $("#satflowDownload").click(function () {
        downloadSatFlowCSVFile();
    });
});
$(function () {
    $("#satflowRefresh").click(function () {
        $("#satFlowLogsDiv").empty();  // removes the child elements from div
        sendReqLanesCount(); // this in turn gets the Satflows and STLOST data into tables
    });
});
$(function () {
    $("#errorRefresh").click(function () {
        $("#errorTable").empty();
        sendReqErrorCount();
    });
});
$(function () {
    $("#mesgRefresh").click(function () {
        processMessages();
    });
});


// General Appearance
//******************
function resizeNavBarElements() {
    var logo = document.getElementById("logo");
    var smallLogo = document.getElementById("smallLogo");

    var title = document.getElementById("pageText");

    if (window.outerWidth <= 450) { // Seems to cause nav button to go below if set as 400
        $(title).removeClass("largeTitle").removeClass("mediumTitle").addClass("smallTitle");
        $(smallLogo).removeClass("hidden");
        $(logo).addClass("hidden");
    }
    else {
        if ((window.outerWidth > 450) && (window.outerWidth <= 800)) {
            $(title).removeClass("largeTitle").removeClass("smallTitle").addClass("mediumTitle");
        } else { $(title).removeClass("smallTitle").removeClass("mediumTitle").addClass("largeTitle"); }
        $(smallLogo).addClass("hidden");
        $(logo).removeClass("hidden");
    }
}

function resizeSwitches()
{
    document.getElementById("MovaEnabledText").innerHTML = String.format("{0,-14}", "MOVA enabled");
    document.getElementById("OnControlText").innerHTML = String.format("{0,-20}", "On-control");
    document.getElementById("ErrorText").innerHTML = String.format("{0,-20}", "Error count");
    document.getElementById("WarmUpText").innerHTML = String.format("{0,-20}", "Warmup");
    document.getElementById("AutoScrollText").innerHTML = String.format("{0,-13}", "Auto-scroll");
    document.getElementById("SFMessageText").innerHTML = String.format("{0,-11}", "SF Message");

    if (isBreakpoint('xs')) { mode = "xs"; }
    else if (isBreakpoint('sm')) { mode = "sm"; }
    else if (isBreakpoint('md')) { mode = "md"; }
    else { mode = "lg"; }

    if (mode === "lg" || mode === "md") {
        $('#WarmUpText').removeClass("spaceTextsm").addClass("spaceTextlg");
        $('#ErrorText').removeClass("spaceTextsm").addClass("spaceTextlg");
        $('#MovaEnabledText').removeClass("spaceTextsm").addClass("spaceTextlg");
        $('#OnControlText').removeClass("spaceTextsm").addClass("spaceTextlg");
        $('#AutoScrollText').removeClass("spaceTextsm").addClass("spaceTextlg");
        $('#SFMessageText').removeClass("spaceTextsm").addClass("spaceTextlg");
        $('#MessagesBoxDiv').removeClass("messagesBoxSmall").addClass("messagesBoxBig");
    }
    if (mode === "sm" || mode === "xs") {
        $('#WarmUpText').removeClass("spaceTextlg").addClass("spaceTextsm");
        $('#ErrorText').removeClass("spaceTextlg").addClass("spaceTextsm");
        $('#MovaEnabledText').removeClass("spaceTextlg").addClass("spaceTextsm");
        $('#OnControlText').removeClass("spaceTextlg").addClass("spaceTextsm");
        $('#AutoScrollText').removeClass("spaceTextlg").addClass("spaceTextsm");
        $('#SFMessageText').removeClass("spaceTextlg").addClass("spaceTextsm");
        $('#MessagesBoxDiv').removeClass("messagesBoxBig").addClass("messagesBoxSmall");
    }
    document.getElementById("AutoScrollSwitch").checked = JSON.parse(sessionStorage.getItem("AutoScrollSwitch"));
    document.getElementById("SFMessageSwitch").checked = JSON.parse(sessionStorage.getItem("SFMessageState")); // Block/unblock the SF message (MsgID = M7.1) 
}

function sortResizing() {

    generateStatusDisplay();
    resizeNavBarElements();
    resizeSwitches();
}


function isBreakpoint(alias) {
    return $('.device-' + alias).is(':visible');
}


//The Bootstrap 3 grid system has four tiers of classes: xs (phones), sm (tablets), md (desktops), and lg (larger desktops)
//(xs – extra small device [<768px], sm – small device [>=768px], md – medium device [>=992px] and lg – large device [>=1200px]).
//function getBootstrapDeviceSize() {
//    return $('#users-device-size').find('div:visible').first().attr('id');
//}

// Incrementing Mova time in javascript
function automateMovaTime() {
    // Calculate the difference in time from system time and store to be used later
    var movaDateTime = new Date(sessionStorage.getItem("movaTime"));
    var currentDateTime = new Date(sessionStorage.getItem("startTime"));
    var timeDiff = (currentDateTime.getTime() - movaDateTime.getTime());
    var threshold = 30; // seconds
    if (Math.floor(Math.abs(timeDiff) / 1e3) > threshold) { // colour text red if offset is big
        $("#movaTime").addClass('bigTimeOffset').removeClass('smallTimeOffset');
    }
    else {
        $("#movaTime").addClass('smallTimeOffset').removeClass('bigTimeOffset');
    }
    var runningMovaTime = new Date();
    runningMovaTime -= timeDiff; // offset the original difference from system time
    runningMovaTime = new Date(runningMovaTime);
    document.getElementById('movaTime').innerHTML = runningMovaTime.toUTCString();
    var t = setTimeout(automateMovaTime, 500);
}

// Online Data Page
//**********************************
//Refresh Online Data Screen every 1s
window.setInterval(event, 1000);
function event() {
    if ($('#onlinedatacontainer').is(":visible")) {
        // ONLINE DATA DISPLAY
        sendReqDetectorsStatus();
        sendReqConfirmBits();
        sendReqForceBits();
        sendReqOperationFlags();
    }
}

// Detectors and Status Flag display
//************************************
function setBits(type, statusArray) {
    var bitBox;
    // Reset only this type before setting new state — eliminates flash
    for (var r = 1; r < statusArray.length + 1; r++) {
        $(document.getElementById(type + r.toString())).removeClass('active');
    }
    for (num = 1; num < statusArray.length + 1; num++) {
        if (statusArray[num - 1] == true) {
            bitBox = document.getElementById(type + num.toString());
            $(bitBox).addClass('active');
        }
    }
}

function resetSquares() {
    for (i = 1; i < 65; i++) { $(document.getElementById("Channel" + i.toString())).removeClass('active'); }
    for (i = 1; i < 32; i++) { $(document.getElementById("ConfirmBit" + i.toString())).removeClass('active'); }
    for (i = 1; i < 13; i++) { $(document.getElementById("ForceBit" + i.toString())).removeClass('active'); }
}

function generateStatusDisplay() {
    var i, channels = [], confirmBits = [], forceBits = [];

    for (i = 1; i < 65; i++) { channels.push(i.toString()); }
    for (i = 1; i < 32; i++) { confirmBits.push(i.toString()); }
    for (i = 1; i < 13; i++) { forceBits.push(i.toString()); }
    forceBits[10] = "HI"; // javascript array 0 referenced
    forceBits[11] = "TO";

    statusDisplay("Channel", 4, channels);
    statusDisplay("ConfirmBit", 4, confirmBits);
    statusDisplay("ForceBit", 4, forceBits);
}

function statusDisplay(id, squareGroupingNum, squareText) {
    var totalNumSquares = squareText.length;
    var numGroupsAcross = Math.ceil(totalNumSquares, squareGroupingNum);
    var rowDiv, fullRow, squaresNextInRow;
    var startDiv = document.getElementById(id);
    var count = 1;
    var mode;

    if (isBreakpoint('xs')) { numGroupsAcross = Math.min(numGroupsAcross, 3); mode = "xs"; }
    else if (isBreakpoint('sm')) { numGroupsAcross = Math.min(numGroupsAcross, 3); mode = "sm"; }
    else if (isBreakpoint('md')) { numGroupsAcross = Math.min(numGroupsAcross, 5); mode = "md"; }
    else { numGroupsAcross = Math.min(numGroupsAcross, 7); mode = "lg"; } 

   // setOnlineControlSpacing(mode);

    fullRow = numGroupsAcross * squareGroupingNum;

    while (startDiv.hasChildNodes()) {
        startDiv.removeChild(startDiv.lastChild);
    }

    while (count < totalNumSquares + 1) {

        rowDiv = document.createElement('div');
        startDiv.appendChild(rowDiv)

        if (totalNumSquares % fullRow == 0) {
            squaresNextInRow = totalNumSquares // only this row to create
        }
        else if ((count - 1 + fullRow) < totalNumSquares) {
            squaresNextInRow = fullRow; //a full row to create
        }
        else {  //a part row to create
            squaresNextInRow = totalNumSquares % fullRow;
        }
        var firstInRow = false;
        for (i = 1; i < squaresNextInRow + 1; i++) {
            if (i === 1)
            { firstInRow = true }
            else
            { firstInRow = false }
            appendSquare(id, rowDiv, count++, squareText, mode, firstInRow);
        }
    }
}



function appendSquare(id, divElement, num, squareText, mode, firstInRow) {

    var squareDiv = document.createElement('div');
    var contentDiv = document.createElement('div');

    var spanElement = document.createElement('span'); //Adding a span element allows the font to be resized if wanted
    var className;
    divElement.appendChild(squareDiv);

    if (firstInRow)
    { squareDiv.className = "square Bmarg" + mode; }
    else {
        if (num % 4 === 0)
        { squareDiv.className = "square Emarg" + mode; }
        else
        { squareDiv.className = "square Rmarg" + mode; }
    }
    squareDiv.id = id + num;
    contentDiv.className = "content" + mode;
    contentDiv.innerHTML = squareText[num - 1];
    squareDiv.appendChild(contentDiv);
}

function setUIControls(mova, control, warmup) {
    // Need controls to be UI disabled
    var movaOnOffSwitch = document.getElementById("MovaEnabledSwitch");
    if (mova)
    { movaOnOffSwitch.checked = true; }
    else
    { movaOnOffSwitch.checked = false; }
    var OnControlOnOffSwitch = document.getElementById("OnControlSwitch");
    if (control)
    { OnControlOnOffSwitch.checked = true; }
    else
    { OnControlOnOffSwitch.checked = false; }

    var warmupBox = document.getElementById("warmUpBox")
    warmupBox.innerHTML = warmup.toString();
}

// Error Logs
//******************

function addErrorToTable(response) {
    var err = JSON.parse(response).Params;

    var table = document.getElementById("errorTable");
    var row = table.insertRow(-1);
    var cell1 = row.insertCell(0);
    var cell2 = row.insertCell(1);
    cell1.innerHTML = new Date(err.DateTime).toUTCString();
    cell2.innerHTML = MOVAErrorsDictionary(err);
}


// Sat Flow and STLOST Data
//*************************

function downloadSatFlowCSVFile() {
    var csvData = htmlTableToCSV(document.getElementById("tableSatFlowSummary").innerHTML);
    var fileid = "satFlowTable";
    var lanenum = 1;
    while (document.getElementById(fileid + lanenum.toString())) {
        csvData = csvData + htmlTableToCSV(document.getElementById(fileid + lanenum.toString()).innerHTML); //tableType + 'Table' + laneNo;
        lanenum++;
    }
    csvData = csvData + htmlTableToCSV(document.getElementById("tableSTLOSTSummary").innerHTML);
    fileid = 'STLOSTTable';
    lanenum = 1;
    while (document.getElementById(fileid + lanenum.toString())) {
        csvData = csvData + htmlTableToCSV(document.getElementById(fileid + lanenum.toString()).innerHTML);
        lanenum++;
    }

    // the only way could find to enable IE to do this:       
    var blob = new Blob([csvData], { type: "text/csv;charset=utf-8;" });
    if (navigator.msSaveBlob) { // IE 10+
        navigator.msSaveBlob(blob, "SatFlowDownload.csv")
    } else {
        var link = document.createElement("a");
        link.className = "hidden";
        if (link.download !== undefined) { // feature detection
            // Browsers that support HTML5 download attribute
            var url = URL.createObjectURL(blob);
            link.setAttribute("href", url);
            link.setAttribute("download", "SatFlowDownload.csv");
            document.body.appendChild(link);
            link.click();
            document.body.removeChild(link);
        }
    }

}

function htmlTableToCSV(table) {
    return csvData = table.replace(/<thead>/g, '')
    .replace(/<\/thead>/g, '')
    .replace(/<tbody>/g, '')
    .replace(/<\/tbody>/g, '')
    .replace(/<tr>/g, '')
    .replace(/<\/tr>/g, '\r\n')
    .replace(/<th>/g, '')
    .replace(/<\/th>/g, ',')

     .replace(/<th colspan="16">/g, '')
    .replace(/<\/th>/g, ',')
    .replace(/<th colspan="11">/g, '')
    .replace(/<\/th>/g, ',')
    .replace(/<th colspan="9">/g, '')
    .replace(/<\/th>/g, ',')
    .replace(/<th colspan="8">/g, '')
    .replace(/<\/th>/g, ',')
    .replace(/<th colspan="6">/g, '')
    .replace(/<\/th>/g, ',')
    .replace(/<th colspan="2">/g, '')
    .replace(/<\/th>/g, ',')
    .replace(/<th colspan="1">/g, '')
    .replace(/<\/th>/g, ',')

    .replace(/<td>/g, '')
    .replace(/<\/td>/g, ',')

    .replace(/\t/g, '')
    .replace(/\n/g, '');
}

function addSatFlowAndSTLOST(laneCount) {

    var tableSatFlowSummary = document.createElement('table');
    tableSatFlowSummary.id = "tableSatFlowSummary";
    var tableSatFlowDetails = new Array(laneCount);
     fillSatFlowSTLOSTTables(laneCount, SatFlowSTLost().SATFLOW, tableSatFlowSummary, tableSatFlowDetails);


    var tableSTLOSTSummary = document.createElement('table');
    tableSTLOSTSummary.id = "tableSTLOSTSummary";
    var tableSTLOSTDetails = new Array(laneCount);
    fillSatFlowSTLOSTTables(laneCount, SatFlowSTLost().STLOST, tableSTLOSTSummary, tableSTLOSTDetails);
    // Attach tables to the div's in the order required to show
    var div = document.getElementById('satFlowLogsDiv');
    // Add Sat Flows
    div.appendChild(tableSatFlowSummary);
    for (lane = 1; lane < laneCount + 1; lane++) {
        div.appendChild(document.createElement('p'));
        div.appendChild(tableSatFlowDetails[lane - 1]);
    }
    div.appendChild(document.createElement('p'));
    // Add STLOST
    div.appendChild(tableSTLOSTSummary);
    for (lane = 1; lane < laneCount + 1; lane++) {
        div.appendChild(document.createElement('p'));
        div.appendChild(tableSTLOSTDetails[lane - 1]);
    }
}

function fillSatFlowSTLOSTTables(laneCnt, tableType, tableSummary, tableDetails) {

    // table type is either satFlow or SATLOST
    var trSummary = new Array();
    for (periodNo = 1; periodNo < 7; periodNo++) {
        trSummary[periodNo - 1] = document.createElement('tr');
    }
    var trHeader0;
    var trHeader1;
    var trDetails;
    for (laneNo = 1; laneNo < laneCnt + 1; laneNo++) {
        tableDetails[laneNo - 1] = document.createElement('table');
        tableDetails[laneNo - 1].className = 'satFlowLogsTable';
        tableDetails[laneNo - 1].id = tableType + 'Table' + laneNo;
        trHeader0 = document.createElement('tr');
        trHeader1 = document.createElement('tr');
        
     if (tableType == SatFlowSTLost().SATFLOW) { fillTopHeaders(trHeader0, trHeader1, tableNames().SATFLOWDETAILS, laneNo.toString(), laneCnt); }
     if (tableType == SatFlowSTLost().STLOST) { fillTopHeaders(trHeader0, trHeader1, tableNames().STLOSTDETAILS, laneNo.toString(), laneCnt);}
        tableDetails[laneNo - 1].appendChild(trHeader0);
        tableDetails[laneNo - 1].appendChild(trHeader1);

        for (periodNo = 1; periodNo < 7; periodNo++) {
            trDetails = document.createElement('tr');
            fillSideHeaders(trDetails, periodNo);

            // Now populate data for both summary and details tables:
            if (tableType == SatFlowSTLost().SATFLOW) {
                sendReqSatFlowDetails(trSummary[periodNo - 1], trDetails, laneNo, periodNo);
            } else if (tableType == SatFlowSTLost().STLOST) {
                sendReqSTLOSTDetails(trSummary[periodNo - 1], trDetails, laneNo, periodNo);
            }
            else { alert('table type not an option') }
            tableDetails[laneNo - 1].appendChild(trDetails);       // Adding a row to details table
        }
    }

    // Complete the summary table
    tableSummary.className = 'satFlowLogsTable';

    trHeader0 = document.createElement('tr');
    trHeader1 = document.createElement('tr');

    if (tableType == SatFlowSTLost().SATFLOW) {fillTopHeaders(trHeader0, trHeader1, tableNames().SATFLOWSUMMARY, '', laneCnt); }
    if (tableType == SatFlowSTLost().STLOST) {fillTopHeaders(trHeader0, trHeader1, tableNames().STLOSTSUMMARY, '', laneCnt); }

    tableSummary.appendChild(trHeader0);
    tableSummary.appendChild(trHeader1);

    // Attach the data to the table 
    for (periodNo = 1; periodNo < 7; periodNo++) {
        fillSideHeaders(trSummary[periodNo - 1], periodNo);
        tableSummary.appendChild(trSummary[periodNo - 1]); // Adding a data row to summary table
    }

}

function setSatFlowData(trSummary, trDetails, data) {
    // Add to summary table
    addCellData(trSummary, data.ESV);
    // Add to details table
    addCellData(trDetails, data.ESV);
    addCellData(trDetails, data.SATINC);
    addCellData(trDetails, data.LastVaueMeasured);
    addCellData(trDetails, data.ConfLevel);
    addCellData(trDetails, data.IsStatsOk);
    addCellData(trDetails, data.TotalCount);
    addCellData(trDetails, data.Mean);
    addCellData(trDetails, data.Smoothed15);
    addCellData(trDetails, data.SumSquared);
}

function setSTLOSTData(trSummary, trDetails, data) {
    // Add to summary table
    addCellData(trSummary, data.ESV);
    // Add to details table
    addCellData(trDetails, data.ESV);
    addCellData(trDetails, data.LastVaueMeasured);
    addCellData(trDetails, data.ConfLevel);
    addCellData(trDetails, data.IsStatsOk);
    addCellData(trDetails, data.TotalCount);
    addCellData(trDetails, data.Mean);
    addCellData(trDetails, data.SumSquared);
}
function fillSideHeaders(trDetails, periodNo) {
    var day = ["Weekday", "Weekday", "Weekday", "Weekday", "Saturday", "Sunday"];
    var satFlowTimes = JSON.parse(sessionStorage.getItem("SatFlowPeriods"));
    var th = document.createElement('th');
    th.innerHTML = day[periodNo - 1];
    trDetails.appendChild(th);
    var th1 = document.createElement('th');
    th1.innerHTML = satFlowTimes[0][periodNo - 1] + ":" + satFlowTimes[1][periodNo - 1];
    trDetails.appendChild(th1);
}

function fillTopHeaders(trHeader0, trHeader1, tableType, laneNum, laneCnt) {
    var mainHeader;
    var headers;
    var headerLanes = new Array(laneCnt);
    for (lane = 1; lane < laneCnt + 1; lane++) {
        headerLanes[lane - 1] = lane.toString();
    }
    var th0 = document.createElement('th');
    switch (tableType) {
        case (tableNames().SATFLOWSUMMARY):
            th0.innerHTML = tableNames().SATFLOWSUMMARY;
            headers = headerLanes;
            break;
        case (tableNames().STLOSTSUMMARY):
            th0.innerHTML = tableNames().STLOSTSUMMARY;
            headers = headerLanes;
            break;
        case (tableNames().SATFLOWDETAILS):
            th0.innerHTML = tableNames().SATFLOWDETAILS;
            headers = ["SF ESV", "SATINC", "Last Value Measured", "99% Conf int", "Stats Ok?", "Total So Far", "Mean SF So Far", "15th So Far", "Sum Sqs"];
            break;
        case (tableNames().STLOSTDETAILS) :
            th0.innerHTML = tableNames().STLOSTDETAILS;
            headers = ["STLOST_ESV", "Last Value Measured", "99% Conf int", "Stats Ok?", "Total So Far", "Mean STLOST So Far", "Sum Sqs"];
            break;
        default:
            break;
    }
  
    th0.colSpan = headers.length + 2;
    trHeader0.appendChild(th0);

    var th1 = document.createElement('th');
    th1.innerHTML = 'Lane ' + laneNum;
    th1.colSpan = 2;
    trHeader1.appendChild(th1);
    var th2;
    for (i = 0; i < headers.length; i++) {
        th2 = document.createElement('th');
        th2.innerHTML = headers[i];
        th2.colSpan = 1;
        trHeader1.appendChild(th2);
    }

}


// Active Data Screen
//*******************
function addActiveData(response) {

    var data = JSON.parse(response).Params;
    // Attach tables to the div's in the order required to show
    // try to create a multi table
    var IsEmergency = JSON.parse(sessionStorage.getItem("isEmergency"));
    setActiveTable(data, tableNames().MOVASITEDETAILS);
    setActiveTable(data, tableNames().SITEDIMS);
    setActiveTable(data, tableNames().LGREEN, tableNames().SDCODE, tableNames().LPHASE);
    setActiveTable(data, tableNames().TIMEDEPCHANGES);
    setActiveTable(data, tableNames().STAGES);
    setActiveTable(data, tableNames().CHANGE);
    setActiveTable(data, tableNames().LINKS);
    if (IsEmergency) setActiveTable(data, tableNames().LINKPFACIL); // active item
    setActiveTable(data, tableNames().DETECTORS);
    setActiveTable(data, tableNames().GENERALLANEDATA);
    setActiveTable(data, tableNames().LANESBONUS, tableNames().LINKSBONUS);
    setActiveTable(data, tableNames().LANES);
    setActiveTable(data, tableNames().CONSTANTS);
    setActiveTable(data, tableNames().SATTIMES);
    setActiveTable(data, tableNames().DATASETTRIGGERING);
    setActiveTable(data, tableNames().ERRORS);
    setActiveTable(data, tableNames().PHONENO);
}

var setActiveTable = function () {

    var data = arguments[0];
    var dict;

    var siteDimsDict = TableDictionary(data, tableNames().SITEDIMS);
    var numLinks = siteDimsDict.NLINKS;
    var numLanes = siteDimsDict.NLANES;
    var numStages = siteDimsDict.STAGES;

    var div = document.getElementById('activeDatasetDiv');

    var table = document.createElement('table');
    table.className = "activeDataSetTable1";

    for (tableNumber = 1; tableNumber < arguments.length; tableNumber++) {
        tableType = arguments[tableNumber];
        dict = TableDictionary(data, tableType);
        var tr0 = document.createElement('tr');
        var th0 = document.createElement('th'); //  if colspan is made extra large in firefox will give right border correctly but not for chrome so must specify if greater than default 1.
        th0.innerHTML = tableType;       // Title Header
        var tr1;
        // Heading across the top
         switch (tableType) {
            case (tableNames().LGREEN):
            case (tableNames().SDCODE):
                tr1 = document.createElement('tr');
                tr1.appendChild(createCrossHeader('Stage', 'Link'));
                tr1.className = 'altRow subHeadInRow';
                for (i = 0; i < numLinks; i++) { addHeader(tr1, (i + 1).toString()); }
                th0.colSpan = numLinks + 2;
                tr0.appendChild(th0);
                table.appendChild(tr0);
                table.appendChild(tr1);
                break;
             case (tableNames().STAGES):
                tr1 = document.createElement('tr');
                tr1.appendChild(createRegularHeader('Stage'));
                for (i = 0; i < numStages; i++) { addHeader(tr1, (i + 1).toString()); }
                th0.colSpan = numStages + 2;
                tr0.appendChild(th0);
                table.appendChild(tr0);
                table.appendChild(tr1);
                break;
             case (tableNames().LPHASE):
                tr1 = document.createElement('tr');
                tr1.appendChild(createRegularHeader('Link'));
                tr1.className = 'altRow subHeadInRow';
                for (i = 0; i < numLinks; i++) { addHeader(tr1, (i +1).toString()) ; }
                th0.colSpan = numLinks +2;
                tr0.appendChild(th0);
                table.appendChild(tr0);
                table.appendChild(tr1);
                break;
            case (tableNames().LINKS):
                tr1 = document.createElement('tr');
                tr1.appendChild(createRegularHeader('Link'));
                for (i = 0; i < numLinks; i++) { addHeader(tr1, (i + 1).toString()); }
                th0.colSpan = numLinks + 2;
                tr0.appendChild(th0);
                table.appendChild(tr0);
                table.appendChild(tr1);
                break;
            case (tableNames().CHANGE):
                tr1 = document.createElement('tr');
                tr1.appendChild(createCrossHeader('From Stage', 'To Stage'));
                for (i = 0; i < numStages; i++) { addHeader(tr1, (i + 1).toString()); }
                th0.colSpan = numStages + 2;
                tr0.appendChild(th0);
                table.appendChild(tr0);
                table.appendChild(tr1);
                break;
            case (tableNames().DETECTORS):
                tr1 = document.createElement('tr');
                tr1.appendChild(createCrossHeader('Dets', 'Lane'));
                for (i = 0; i < numLanes; i++) { addHeader(tr1, (i + 1).toString()); }
                th0.colSpan = numLanes + 2;
                tr0.appendChild(th0);
                table.appendChild(tr0);
                table.appendChild(tr1);
                break;
             case (tableNames().LANESBONUS):
                tr1 = document.createElement('tr');
                tr1.appendChild(createRegularHeader('Lane'));
                tr1.className = 'altRow subHeadInRow';
                for (i = 0; i < numLanes; i++) { addHeader(tr1, (i + 1).toString()); }
                if (numLinks > numLanes)
                    addHeadSpacerCell(tr1, numLinks - numLanes);
                th0.colSpan = Math.max(numLanes, numLinks) + 2;
                tr0.appendChild(th0);
                table.appendChild(tr0);
                table.appendChild(tr1);
                break;
             case (tableNames().GENERALLANEDATA):
                 tr1 = document.createElement('tr');
                 tr1.appendChild(createRegularHeader('Lane'));
                 for (i = 0; i < numLanes; i++) { addHeader(tr1, (i + 1).toString()); }
                 th0.colSpan = numLanes+2;
                 tr0.appendChild(th0);
                 table.appendChild(tr0);
                 table.appendChild(tr1);
                 break;
            case (tableNames().LINKSBONUS):
                tr1 = document.createElement('tr');
                tr1.appendChild(createRegularHeader('Link')) ;
                tr1.className = 'altRow subHeadInRow';
                for (i = 0; i < numLinks; i++) { addHeader(tr1, (i + 1).toString()); }
                if (numLanes > numLinks)
                    addHeadSpacerCell(tr1, numLanes - numLinks);
                th0.colSpan = Math.max(numLanes, numLinks) + 2;
                tr0.appendChild(th0);
                table.appendChild(tr0);
                table.appendChild(tr1);
                break;
            case (tableNames().LANES):
                tr1 = document.createElement('tr');
                tr1.appendChild(createRegularHeader('Lane'));
                for (i = 0; i < numLanes; i++) { addHeader(tr1, (i + 1).toString()); }
                th0.colSpan = numLanes + 2;
                tr0.appendChild(th0);
                table.appendChild(tr0);
                table.appendChild(tr1);
                break;
            case (tableNames().DATASETTRIGGERING):
                tr1 = document.createElement('tr');
                tr1.appendChild(createRegularHeader('')); //blank
                var arrayOfHeaders = new Array(dict.DATATRIGGER.length);
                for (i = 0; i < dict.DATATRIGGER.length; i++) { arrayOfHeaders[i] = "Dataset " + (i + 1).toString(); }
                for (i = 0; i < arrayOfHeaders.length; i++) { addHeader(tr1, arrayOfHeaders[i]); }
                th0.colSpan = arrayOfHeaders.length + 2;
                tr0.appendChild(th0);
                table.appendChild(tr0);
                table.appendChild(tr1);
                break;
            case (tableNames().CONSTANTS):
                tr1 = document.createElement('tr');
                tr1.appendChild(createRegularHeader('Constants'));
                for (key in dict) { addHeader(tr1, key); }
                th0.colSpan = Object.keys(dict).length + 2;
                tr0.appendChild(th0);
                table.appendChild(tr0);
                table.appendChild(tr1);
                break;
            case (tableNames().SATTIMES):
                tr1 = document.createElement('tr');
                tr1.appendChild(createRegularHeader('')); //blank
                var arrayOfHeaders = ["Weekday 1", "Weekday 2", "Weekday 3", "Weekday 4", "Saturday", "Sunday"]; // may want to change this on tidy up
                for (i = 0; i < arrayOfHeaders.length; i++) { addHeader(tr1, arrayOfHeaders[i]); }
                th0.colSpan = arrayOfHeaders.length + 2;
                tr0.appendChild(th0);
                table.appendChild(tr0);
                table.appendChild(tr1);
                break;
             case (tableNames().LINKPFACIL):
                tr1 = document.createElement('tr');
                tr1.appendChild(createCrossHeader('Link', 'Stage'));
                for (i = 0; i < numStages; i++) { addHeader(tr1, (i + 1).toString()); }
                th0.colSpan = numStages + 2;
                tr0.appendChild(th0);
                table.appendChild(tr0);
                table.appendChild(tr1);
                break;
            case (tableNames().SITEDIMS):
                tr1 = document.createElement('tr');
                for (key in dict) { addHeader(tr1, key); }
                th0.colSpan = Object.keys(dict).length;
                tr0.appendChild(th0);
                table.appendChild(tr0);
                table.appendChild(tr1);
                break;
            case (tableNames().ERRORS):
                th0.colSpan = dict["ERROR NUMBER"].length +2;
                tr0.appendChild(th0);
                table.appendChild(tr0);
                break;
             case (tableNames().MOVASITEDETAILS):
                 th0.colSpan = 2;
                 tr0.appendChild(th0);
                 table.appendChild(tr0);
                 break;
             case (tableNames().TIMEDEPCHANGES):
             case (tableNames().PHONENO):
                tr0.appendChild(th0);
                table.appendChild(tr0);
                break;
            default:
                break;
        }

        // Heading down the side and Data
        switch (tableType) {
            case (tableNames().LGREEN):
                for (i = 0; i < numStages; i++) {
                    var tr2 = document.createElement('tr');
                    tr2.className = "altRow";
                    var th = document.createElement('th');
                    th.colSpan = 2;
                    th.innerHTML = (i + 1).toString();
                    th.className = 'subHeader';
                    tr2.appendChild(th); // Side Header
                    for (j = 0; j < numLinks; j++) {
                        var td = document.createElement('td');
                        td.colspan = 1;
                        td.innerHTML = dict.LGREEN[i][j];
                        tr2.appendChild(td); // Data
                    }
                    table.appendChild(tr2);
                }
                break;
            case (tableNames().LPHASE):
                for (i = 0; i < Object.keys(dict).length; i++) {
                    var tr2 = document.createElement('tr');
                    tr2.className = "altRow";
                    var th = document.createElement('th');
                    th.colSpan = 2;
                    th.innerHTML = ''; //blank
                    th.className = 'subHeader';
                    tr2.appendChild(th); // Side Header
                    for (j = 0; j < numLinks; j++) {
                        addCellData(tr2, dict.LPHASE[j])
                    }
                    table.appendChild(tr2);
                }
                break;
            case (tableNames().LINKS):
                for (key in dict) {
                    var tr2 = document.createElement('tr');
                    var th = document.createElement('th');
                    th.colSpan = 2;
                    th.className = 'subHeader';
                    th.innerHTML = key;
                    tr2.appendChild(th); // Side Header
                    for (j = 0; j < numLinks; j++) {
                        addCellData(tr2, dict[key][j]);
                    }
                    table.appendChild(tr2);
                }
                break;
            case (tableNames().LANESBONUS):
                for (key in dict) {
                    var tr2 = document.createElement('tr');
                    tr2.className = "altRow";
                    var th = document.createElement('th');
                    th.colSpan = 2;
                    th.innerHTML = key;
                    th.className = 'subHeader';
                    tr2.appendChild(th); // Side Header
                    for (j = 0; j < numLanes; j++) {
                        addCellData(tr2, dict[key][j]);
                    }
                    if (numLinks > numLanes)
                        addHeadSpacerCell(tr2, numLinks - numLanes);
                    table.appendChild(tr2);
                }
                break;
            case (tableNames().LINKSBONUS):
                for (key in dict) {
                    var tr2 = document.createElement('tr');
                    tr2.className = "altRow";
                    var th = document.createElement('th');
                    th.colSpan = 2;
                    th.innerHTML = key;
                    th.className = 'subHeader';
                    tr2.appendChild(th); // Side Header
                      for (j = 0; j < numLinks; j++) {
                        addCellData(tr2, dict[key][j])
                    }
                    if (numLanes > numLinks)
                        addHeadSpacerCell(tr2, numLanes - numLinks);
                    table.appendChild(tr2);
                }
                break;
            case (tableNames().SDCODE):
                for (i = 0; i < numStages; i++) {
                    var tr2 = document.createElement('tr');
                    tr2.className = "altRow";
                    var th = document.createElement('th');
                    th.colSpan = 2;
                    th.innerHTML = (i + 1).toString();
                    th.className = 'subHeader';
                    tr2.appendChild(th); // Side Header
                    for (j = 0; j < numLinks; j++) {
                        addCellData(tr2, dict.SDCODE[i][j]);
                    }
                    table.appendChild(tr2);
                }
                break;
            case (tableNames().TIMEDEPCHANGES):
                for (i = 0; i < dict.TIMEDEP.length; i++) {
                    var tr2 = document.createElement('tr');
                    addCellData(tr2, dict.TIMEDEP[i]);
                    table.appendChild(tr2);
                }
                break;
            case (tableNames().MOVASITEDETAILS):
                for (key in dict) {
                    var tr2 = document.createElement('tr');
                    var th = document.createElement('th');
                    th.className = 'subHeader';
                    th.innerHTML = key;
                    tr2.appendChild(th);
                    addCellData(tr2, dict[key])
                    if (key == "Title")
                        tr2.id = "TitleRow";
                    table.appendChild(tr2);
                }
                break;
            case (tableNames().CONSTANTS):
                var tr2 = document.createElement('tr');
                tr2.appendChild(createRegularHeader(''));
                for (key in dict) {
                    addCellData(tr2, dict[key]); // need to tidy code as this doesn't seem best approach
                }
                table.appendChild(tr2);
                break;
            case (tableNames().SITEDIMS):
                var tr2 = document.createElement('tr');
                for (key in dict) {
                    addCellData(tr2, dict[key]); // need to tidy code as this doesn't seem best approach
                }
                table.appendChild(tr2);
                break;
            case (tableNames().STAGES): // Stage maximums and alerts
                for (key in dict) {
                    var tr2 = document.createElement('tr');
                    var th = document.createElement('th');
                    th.colSpan = 2;
                    th.className = 'subHeader';
                    th.innerHTML = key;
                    tr2.appendChild(th); // Side Header
                    if (key == "MIN")
                        tr2.id = "MINRow";
                    for (j = 0; j < numStages; j++) {
                        addCellData(tr2, dict[key][j])
                    }
                    table.appendChild(tr2);
                }
                break;
            case (tableNames().CHANGE):
                for (i = 0; i < numStages; i++) {
                    var tr2 = document.createElement('tr');
                    var th = document.createElement('th');
                    th.colSpan = 2;
                    th.className = 'subHeader';
                    th.innerHTML = (i + 1).toString();
                    tr2.appendChild(th); // Side Header
                    for (j = 0; j < numStages; j++) {
                        addCellData(tr2, dict.CHANGE[i][j])
                    }
                    table.appendChild(tr2);
                }
                break;
            case (tableNames().DETECTORS):
                var detector;
                for (key in dict) {
                    var tr2 = document.createElement('tr');
                    var th = document.createElement('th');
                    th.colSpan = 2;
                    th.className = 'subHeader';
                    detector = dict[key];
                    th.innerHTML = key;
                    tr2.appendChild(th); // Side Header
                    for (j = 0; j < numLanes; j++) {
                        addCellData(tr2, detector[j])
                    }
                    table.appendChild(tr2);
                }
                break;
            case (tableNames().GENERALLANEDATA):
                for (key in dict) {
                    var tr2 = document.createElement('tr');
                    var th = document.createElement('th');
                    th.colSpan = 2;
                    th.className = 'subHeader';
                    th.innerHTML = key;
                    tr2.appendChild(th); // Side Header
                    for (j = 0; j < numLanes; j++) {
                        addCellData(tr2, dict[key][j])
                    }
                    table.appendChild(tr2);
                }
                break;
            case (tableNames().LANES):
                for (key in dict) {
                    var tr2 = document.createElement('tr');
                    var th = document.createElement('th');
                    th.colSpan = 2;
                    th.className = 'subHeader';
                    th.innerHTML = key;
                    tr2.appendChild(th); // Side Header
                    for (j = 0; j < numLanes; j++) {
                        addCellData(tr2, dict[key][j])
                    }
                    table.appendChild(tr2);
                }
                break;

            case (tableNames().SATTIMES):
                for (key in dict) {
                    var tr2 = document.createElement('tr');
                    var th = document.createElement('th');
                    th.colSpan = 2;
                    th.className = 'subHeader';
                    th.innerHTML = key;
                    tr2.appendChild(th); // Side Header
                    for (j = 0; j < dict[key].length; j++) {
                        addCellData(tr2, dict[key][j]);
                    }
                    table.appendChild(tr2);
                }
                break;
            case (tableNames().DATASETTRIGGERING):
                var tr2 = document.createElement('tr');
                var th = document.createElement('th');
                th.colSpan = 2;
                th.className = 'subHeader';
                th.innerHTML = "DETECTOR";
                tr2.appendChild(th); // Side Header
                for (j = 0; j < dict.DATATRIGGER.length; j++) {
                    addCellData(tr2, dict.DATATRIGGER[j])
                }
                table.appendChild(tr2);
                break;
            case (tableNames().ERRORS):
                for (key in dict) {
                    var tr2 = document.createElement('tr');
                    var th = document.createElement('th');
                    th.colSpan = 2;
                    th.className = 'subHeader';
                    th.innerHTML = key;
                    tr2.appendChild(th); // Side Header
                    for (j = 0; j < dict["ERROR NUMBER"].length; j++) {
                        addCellData(tr2, dict[key][j])
                    }
                    table.appendChild(tr2);
                }
                break;
            case (tableNames().PHONENO):
                var tr2 = document.createElement('tr');
                addCellData(tr2, dict.NO);
                table.appendChild(tr2);
                break;
            case (tableNames().LINKPFACIL):
                for (i = 0; i < numLinks; i++) {
                    var tr2 = document.createElement('tr');
                    var th = document.createElement('th');
                    th.colSpan = 2;
                    th.className = 'subHeader';
                    th.innerHTML = (i + 1).toString();
                    tr2.appendChild(th); // Side Header
                    for (j = 0; j < numStages; j++) {
                        addCellData(tr2, dict.LINKPFACIL[j][i]);
                    }
                    table.appendChild(tr2);
                }
                break;
            default:
                break;

        }

        // is this a multi-table, sort css classes via jquery
        if (arguments.length > 2) {
            table.id = 'multi';
            for (i = 1; i < arguments.length; i++) {
               table.id = table.id + arguments[i].replace(/\s/g, ''); // more than one multi table so giving unique ids
            }

        }

    } // end loop needed for multi table
    div.appendChild(table);
    if (arguments.length > 2) styleMultiTable(table.id);
 
    div.appendChild(document.createElement('p'));
}

function styleMultiTable(tableid) {
    // Examples of these are: LGREEN/SDCODE/LPHASE multitable and Lane/Link Bonus multitable
    // Controlling the Multi table look dynamically so that the subtables have the same look as the singular tables, i.e. header title has dark background (can often have but just by coincidence unless forced)
    var altRowNum = 0;
    $('#' + tableid).removeClass('activeDataSetTable1').addClass('multiTable'); //remove alternating background colours to redo and sort out the borders
    $('#' + tableid + ' tr').each(function () {  
        if ($(this).hasClass('altRow')) {
            altRowNum++;
        }
        else {
            altRowNum = 0;
        }
        if ((altRowNum % 2))//  true is odd,false is even
        {
            $(this).addClass('oddColor').removeClass('evenColor');
        }
        else {
            $(this).addClass('evenColor').removeClass('oddColor');
        }
        if ($(this).hasClass('subHeadInRow')) {
            $(this).children().addClass('subHeader'); // only effective if applied to children, all of this table css could be tidier but would take time to sort leave here for now
        }
    });
}

function addCellData(tr, value) {
    var td = document.createElement('td');
    td.innerHTML = value.toString();
    tr.appendChild(td);
}

function addHeadSpacerCell(tr, spanval) {
    var td = document.createElement('td');
    td.colSpan = spanval;
    tr.appendChild(td);
}

function createRegularHeader(head) {
    var th1 = document.createElement('th');
    th1.colSpan = 2;
    th1.innerHTML = head;
    return th1;
}

function createCrossHeader(topHead, sideHead) {
    var th1 = document.createElement('th');
    th1.colSpan = 2;
    th1.className = 'crossed';
    var spanStageHead = document.createElement('span');
    var spanLinkHead = document.createElement('span');
    spanStageHead.innerHTML = topHead + '&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp&nbsp';
    spanLinkHead.innerHTML = sideHead;
    th1.appendChild(spanStageHead);
    th1.appendChild(spanLinkHead);
    return th1;
}


function addHeader(tr1, text) {
    var th = document.createElement('th');
    th.colSpan = 1;
    th.innerHTML = text;
    tr1.appendChild(th);
}


function processSatTimesArray(satflowTimesArray) {
    var satflowTimes = twoDArray(2, satflowTimesArray.length); // switch array around for convenience later
    for (t = 0; t < satflowTimesArray.length; t++) {
        satflowTimes[0][t] = checkTime(Math.floor((satflowTimesArray[t][0] / 60).toString())) + ":" + checkTime((satflowTimesArray[t][0] % 60).toString());
        satflowTimes[1][t] = checkTime(Math.floor((satflowTimesArray[t][1] / 60).toString())) + ":" + checkTime((satflowTimesArray[t][1] % 60).toString());
    }
    return satflowTimes;

}

function twoDArray(numRows, numCols) {
    var twoDim = [];
    for (var row = 0; row < numRows; row++) {
        var oneDim = [];
        twoDim[row] = oneDim;
    }
    return twoDim;
}



function processTimeDependentData(dattim, datnum) {
    // Adapted from MOVA CODE:
    var messages = new Array();
    var startTimeHrs;
    var startTimeMins;
    var dayCode;
    var dataSetNum;
    //dattim 52,53,33,34
    //datnum 603,601,702,701
    var i = 0;
    while (dattim[i] != -1) {
        /* de-code stored ToD data */
        startTimeHrs = truncate(dattim[i] / 4); //j                         
        startTimeMins = (dattim[i] - startTimeHrs * 4) * 15; //k   
        dayCode = truncate(datnum[i] / 100); //l                              
        dataSetNum = datnum[i] - dayCode * 100;//m                 

        messages.push(" START TIME =" + checkTime(startTimeHrs) + ":" + checkTime(startTimeMins) + "  DAY CODE =" + dayCode + " DATA SET NUMBER =" + dataSetNum);
        i++;
    }  /* end when no more ToD data */

    if (messages.length < 1)
        messages.push("None");//("No time dependent changes of site data")

    return messages;
}

function checkTime(i) {
    if (i < 10) { i = "0" + i };  // add zero in front of numbers < 10
    return i;
}

//IE 11/Edge does not work with Math.trunc, using bitwise operators for efficiency, bitwise operators convert operands to 32-bit integers
function truncate(n) {
    // converts to int before 'oring' returning n
    return n | 0; 
}

// Message Page
// ****************
function processMessages()
{
    var mesgDiv = document.getElementById('MessagesBoxDiv');
    mesgDiv.innerHTML = ""; // for some reason this needs clearing first
    sendReqLastMessageIndexMessage(); // Initiate the fetching of messages
    switchControl(); // initiallise these controls
}

function displayMessages(inx,msg)//messageDict)
{
    var mesgDiv = document.getElementById('MessagesBoxDiv');
    var message = MessagesDictionary(msg);
    //  mesgDiv.innerHTML += inx + ": " + MessagesDictionary(msg) + "<br>";
    if (!message == "") // empty string returned if M7 blocked
    {
        mesgDiv.innerHTML += message + "<br>";
    }
    // console.log(inx + " : " + MessagesDictionary(msg));
    switchControl();
}


function switchControl() {
    var mesgDiv = document.getElementById('MessagesBoxDiv');

    if (document.getElementById("AutoScrollSwitch").checked) {
        mesgDiv.scrollTop = mesgDiv.scrollHeight;
    }
    else {
        // or no auto scroll
        $(mesgDiv).on('beforeunload', function () {
            $(mesgDiv).scrollTop(0);
        });
    }
    sessionStorage.setItem("AutoScrollSwitch", JSON.stringify(document.getElementById("AutoScrollSwitch").checked));
    sessionStorage.setItem("SFMessageState", JSON.stringify(document.getElementById("SFMessageSwitch").checked));
}