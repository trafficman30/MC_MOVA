
/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         tma_preprint.c
*
*  TITLE:        TMA Logs display preperation and execution.
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	31-May-2011		7.0.0		AA		PK		CRN003			Initial implementation. Module for preparing TMA logs and print them
*	31-Aug-2011		7.0.1		..		PK		CRN004			SatFlow and CANDET improvements
*	12-Dec-2011		7.0.1		AB		AK		CRN006			Correct compiler warnings
*	22-Feb-2012		7.0.3		..		AK		CRN008			Fix compiler warnings
*	06-Jul-2012		7.0.3		AC		AK		CRN012			Memory leak in TMA module and statistics lost on dataset change
*	29-Sep-2012		7.0.4		AD		AK		CRN013			Use thread safe time functions
*	21-Jan-2013		7.0.5		..		AK		CRN022			Buffer overflow in TMA output
*	31-Jan-2013		7.0.5		AE		AK		M7.0.5			Issue M7.0.5
*	26-Apr-2013		7.0.6		..		PA		CRN028			Fix compiler warnings
*	22-May-2013		7.0.6		AF		AK		M7.0.6			Issue M7.0.6
*
*  (c) Copyright TRL Ltd 2013. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal or
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/


/*
Modules listed below implements requirements described in "MOVA TMA output and alerts  – Detailed Requirement Specification" by Mark Crabtree (TRL).

Overall the TMA requirment implementation is implemented in three parts
a) general functions
b) log generation
c) alert verification.

This module implements Log generation functions which follow the following partern:
1.- Collection of the data is done in the kernel, in the appropiate section
2.- prepare_XXX_Log converts information stored in TMA structure (tmaData) into summary data
3.- print_XXXLog is called when the user wants to display log information,
print_XXXLog calls print_xxxTitle to print title, and print_xxxLogLine to print each entry
of the number of logs. The pointers are not altered, the information will stay there, it will be
only overwritten.

After the period is finished, the log is prepared/processed and the information
goes from the only period to a simple base summary logs, then the current period is cleared.
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "kdebug.h"
#include "obclock.h"
#include "defines.h"
#include "datahand.h"

#include "tma_strc.h"
#include "tma_mem.h"
#include "tma_preprint.h"
#include "tma_alerts.h"
#include "generalfunc.h"
#include "dataset_handler.h"

#include "../aml_generic_files_handler.h"


static char	glModuleInitialised = FALSE;

static char buf[MAX_OUTPUT_LEN];
static char tmp[TMP_BUF_LEN];

static float TMA_PreP_StdDev(int Count, float Mean, float SumSqr);

static void prepare_Flow_Log(strcTMA *const pTma);
static void print_FlowLogTitle(int outDevice, int noLanes);
static void print_FlowLogLine(int outDevice, const summa_FlowLog *const pFlowLog, int noLanes);

static void prepare_Occupancy_Log(strcTMA *const pTma);
static void print_OccLogTitle(int outDevice, int noStages);
static void print_OccLogLine(int outDevice, const summa_OccuLog *const pOccLog, int noStages);

static void prepare_EndSat_Log(strcTMA *const pTma);
static void print_EndSatLogTitle(int outDevice, int noStages);
static void print_EndSatLogLine(int outDevice, const summa_EndSatLog *const pEndSatLog, int noStages);

static void prepare_Oversatured_Log(strcTMA *const pTma);
static void print_OverSatLogTitle(int outDevice, int noLanes);
static void print_OverSatLogLine(int outDevice, const summa_OversatLog *const pOverSatLog, int noLanes);

static void prepare_TraStg_Log(strcTMA *const pTma, int dsLast);
static void print_TraSevLogTitle(int outDevice, int noStages);
static void print_TraSevLogLine(int outDevice, const summa_TrafSerLog *const pTraSerLog, int noStages);

static void prepare_PedStg_Log(strcTMA *const pTma);
static void print_PedSevLogTitle(int outDevice, int allRedStg);
static void print_PedSevLogLine(int outDevice, const summa_PedSerLog *const pPedSerLog, int allRedStg, int pedStg[]);


extern strcTMA tmaData;
extern strcTMA *const pTmaData;


/*************************************************************************
 *
 * Function: TMA_Initialise_
 *
 * Description: Initalise TMA module which is responsbile for calling a function which 
 *				allocates dynamic memory
 *
 * Author: PKale
 *
 * Date: 08-Feb-2011
 *
 * Params: void
 *
 * Return: void
 *
 ************************************************************************/
void TMA_PreP_Init_(void )
{
	KDEBUG_WRITE0( DEBUG_LEVEL_VERBOSE, TMAPREPRINT, "TMA Pre Print : initialising." );
	
	if (glModuleInitialised == TRUE)
	{
		KDEBUG_WRITE0( DEBUG_LEVEL_ERROR, TMAPREPRINT, "TMA already initialised " );
	}
	glModuleInitialised = TRUE;
}


/*************************************************************************
 *
 * Function: writeTMAString
 *
 * Description: Prints string to different devices
 *
 * Author: PAnderson
 *
 * Date: 09-Jul-2013
 *
 * Params: MsgBuf:   The string to be printed.
		 : DeviceID: Where it is to go.
 *
 * Return: void
 *
 ************************************************************************/
void writeTMAString( int DeviceID, char *MsgBuf )
{
	if (DeviceID == WRITE_TO_TMA_LOG_FILE)
	{
		aml_write_tma_log_data(MsgBuf);
	}
	
	/*NOTE: The following code will never get executed in M8 because there is no MOVA Comm.*/

#ifdef TRL_INTEGRATION_TEST
	if (DeviceID == SYS_LOG_PRINTING)
	{
		TrimLogString( MsgBuf );
		TMALogWrite( buf );
	}
	else
#endif
	{
		writeString( DeviceID, buf );
	}

}


/* LOG GENERATION */
/*************************************************************************
 *
 * Function: prepare_Flow_Log
 *
 * Description: Function which process the Flow logs information collected during the latest
 *				TMA period.
 *				
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	strcTMA *pTma:- Pointer to TMA data structure 
 *
 * Return: void
 *
 ************************************************************************/
static void prepare_Flow_Log(strcTMA *const pTma)
{
	int laneID;
	int detID;

	/*moving flow detector infomation to its correspondant lane*/
	for (detID = 0; detID < GetNoDetectors(); detID++)
	{
		laneID = findXdetLane(detID+1, 1, GetNoLanes(), GetDetArray());
		if (laneID >= 0) /*the X detector only, position 1,Tcomshr->det[noLane][noDet]*/
		{
			pTma->_currPeriod._Lanes[laneID]._xFlow = pTma->_currPeriod._detFlow[detID];
			pTma->_currPeriod._Lanes[laneID]._xSuspect = (susDetec)pTma->_currPeriod._detFaulty[detID];
		}

		laneID = findXdetLane(detID+1, 0, GetNoLanes(), GetDetArray());
		if (laneID >= 0) /*the IN detector only, position 0, Tcomshr->det[noLane][noDet]*/
		{
			pTma->_currPeriod._Lanes[laneID]._inFlow = pTma->_currPeriod._detFlow[detID];
			pTma->_currPeriod._Lanes[laneID]._inSuspect = (susDetec)pTma->_currPeriod._detFaulty[detID];
		}
	}

	for (laneID = 0; laneID < GetNoLanes(); laneID++) /*all max lanes for MOVA*/
	{
		/* per lane store each printable line*/
		pTma->_sumFlowLog[pTma->peUserTo]._inFlow[laneID] = pTma->_currPeriod._Lanes[laneID]._inFlow;
		pTma->_sumFlowLog[pTma->peUserTo]._xFlow[laneID] = pTma->_currPeriod._Lanes[laneID]._xFlow;		
		pTma->_sumFlowLog[pTma->peUserTo]._xSuspect[laneID] = pTma->_currPeriod._Lanes[laneID]._xSuspect;
		pTma->_sumFlowLog[pTma->peUserTo]._inSuspect[laneID] = pTma->_currPeriod._Lanes[laneID]._inSuspect;
	}

	pTma->_sumFlowLog[pTma->peUserTo]._timeStgEnd_FL = pTma->nextPerStarts;
}



/*************************************************************************
 *
 * Function: print_FlowLogTitle
 *
 * Description: Function to print Flow logs header/title
 *
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	int outDevice:-		Destination for user communication
 *			int noLanes:-		Maximum number of lanes
 *			
 * Return: void
 *
 ************************************************************************/
static void print_FlowLogTitle(int outDevice, int noLanes)
{
	int laneId;

	/*titles*/
	strcpy(buf, "Counts across IN-detector and X-detectors (flow) by lane\r\n");
	strcat(buf, "Lane number,, ");
	writeTMAString(outDevice, buf);

	memset(buf, 0, MAX_OUTPUT_LEN);

	for (laneId = 0; laneId < noLanes; laneId++)
	{
		sprintf(tmp, "%d,,,,", laneId+1);
		strcat(buf, tmp);
	}

	strcat(buf, "\r\nDetector type, Date/time,");
	writeTMAString(outDevice, buf);

	memset(buf, 0, MAX_OUTPUT_LEN);
	strcpy(tmp, "IN,S?,X,S?,");

	for (laneId = 0; laneId < noLanes; laneId++)
	{
		strcat(buf, tmp);
	}

	strcat(buf, "\r\n");
	writeTMAString(outDevice, buf);
}



/*************************************************************************
 *
 * Function: print_FlowLogLine
 *
 * Description: Function to print one Flow logs 
 *				for the TMA period under consideration(i.e a line of Flow log)
 *				
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	int outDevice:-					Destination for user communication
 *			summa_FlowLog pFlowLog:-		Data Structure holding Flow logs information for the current period
 *			int noLanes:-					Maximum number of Lanes 
 *
 * Return: void
 *
 ************************************************************************/
static void print_FlowLogLine(int outDevice, const summa_FlowLog *const pFlowLog, int noLanes)
{	
	struct tm ltime;
	int laneId;

	/* printing time*/
	localtime_r(&pFlowLog->_timeStgEnd_FL, &ltime);
	strftime (buf, MAX_OUTPUT_LEN, "%A, %d/%m/%y to %H:%M:%S hrs.", &ltime);
	writeTMAString(outDevice, buf);

	memset(buf, 0, MAX_OUTPUT_LEN);

	/* printing headers*/
	for (laneId = 0; laneId < noLanes; laneId++)
	{
		sprintf(tmp, ",%d,%c,%d,%c", pFlowLog->_inFlow[laneId], conSuspectToCh(pFlowLog->_inSuspect[laneId]), pFlowLog->_xFlow[laneId], conSuspectToCh(pFlowLog->_xSuspect[laneId]));
		strcat(buf, tmp);
	}

	strcat(buf, "\r\n");
	writeTMAString(outDevice, buf);
}

/*************************************************************************
 *
 * Function: GetLaneFlowLogDetails
 *
 * Description: Get the lane flow details
 *				for the TMA period under consideration
 *				
 * Author: PAnderson
 *
 * Date: 18/09/2013
 *
 * Params:	LaneNo				The lane in question
 *			PeriodInstance		The Period in question
 *			time 				The resulting time
 			xFlow 				The flow for X detector
 			inFlow				The flow for the in detector
 			xSuspect 			The suspect count for the x detector
 			inSuspect 			The suspect count for the in detector
 * Return: void
 *
 ************************************************************************/
void GetLaneFlowLogDetails(int LaneNo, int PeriodInstance, time_t* time, int* xFlow, int* inFlow, int* xSuspect, int* inSuspect)
{
	*time =  pTmaData->_sumFlowLog[PeriodInstance]._timeStgEnd_FL;
	*xFlow = pTmaData->_sumFlowLog[PeriodInstance]._xFlow[LaneNo];
	*inFlow = pTmaData->_sumFlowLog[PeriodInstance]._inFlow[LaneNo];
	*xSuspect = pTmaData->_sumFlowLog[PeriodInstance]._xSuspect[LaneNo];
	*inSuspect = pTmaData->_sumFlowLog[PeriodInstance]._inSuspect[LaneNo]; 

}

/*************************************************************************
 *
 * Function: GetPeriodDetails
 *
 * Description: Get the period details
 *				for the TMA period under consideration
 *				
 * Author: PAnderson
 *
 * Date: 18/09/2013
 *
 * Params:	FirstPeriodInstance		The first period in question
 *			LastPeriodInstance		The last Period in question
 * Return: void
 *
 ************************************************************************/

void GetPeriodDetails(int* FirstPeriodInstance, int* LastPeriodInstance, int* MaxLogs)
{
	*FirstPeriodInstance = pTmaData->peUserFrom;
	*LastPeriodInstance = pTmaData->peUserTo;
	*MaxLogs = sumLogMAX;
}

/*************************************************************************
 *
 * Function: print_FlowLog
 *
 * Description: Functions which determines the start Flow log/end Flow log
 *				within circular queue of logs and calls a function to print each log within the logs boundary.
 *
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	strcTMA *pTma:- Pointer to TMA data structure 
 *			int outDevice:-		Destination for user communication
 *			int noStages:-		Maximum number of Stages 
 *
 * Return: void
 *
 ************************************************************************/
void print_FlowLog(strcTMA *const pTma, int outDevice, int noLanes)
{
	int logsLoop=0;
	/*the number of logs to be printed*/
	int noLogsToP = 0;
	int posQ = pTma->peUserFrom;

	if (pTma->peUserTo != pTma->peUserFrom)
	{
		print_FlowLogTitle(outDevice,noLanes);

		if (pTma->peUserTo > pTma->peUserFrom) /*array is not full */
		{
			noLogsToP = pTma->peUserTo;
		}
		else /*the array has been full at some point in the past*/
		{
			noLogsToP = sumLogMAX;
			if (posQ > 0)
				posQ--;
			else
				posQ = sumLogMAX - 1;
		}

		while (logsLoop < noLogsToP)
		{
			print_FlowLogLine(outDevice, &pTma->_sumFlowLog[posQ], noLanes);
			posQ = (posQ + 1) % (sumLogMAX);
			logsLoop++;
		}
	}
}



/*************************************************************************
 *
 * Function: prepare_Occupancy_Log
 *
 * Description: Function which process the Occupancy logs information collected during the latest
 *				TMA period.
 *				
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	strcTMA *pTma:- Pointer to TMA data structure 
 *
 * Return: void
 *
 ************************************************************************/
static void prepare_Occupancy_Log(strcTMA *const pTma)
{
	int laneLoop,stgLoop,detLoop;
	int susFlag,laneXdet, laneIndet;
	susDetec sus_Value;	
	int lanesRunStg[mxlane];
	
	float totalOccFlow,sumXflowStg = 0;
	float flow[mxlane],flowTemp[mxlane],detOn[mxlane],detOnTemp[mxlane];

	/*count occupancy and flow per lane*/
	for (laneLoop = 0; laneLoop < GetNoLanes(); laneLoop++)
	{
		flow[laneLoop] = flowTemp[laneLoop] = 0;
		detOn[laneLoop] = detOnTemp[laneLoop] = 0;
	}

	/*
	Note that Tcomshr->tton time is stored in 10th of second, similar to detOnTime
	Summing total of detOnTime is now stored in a second and not in 10th of sec.
	*/
	for (detLoop = 0; detLoop < GetNoDetectors(); detLoop++)
	{
		laneXdet = findXdetLane(detLoop+1, 1, GetNoLanes(), GetDetArray());

		if (laneXdet >= 0)
		{
			flow[laneXdet] += pTma->_currPeriod._detFlow[detLoop];
			detOn[laneXdet] += pTma->_currPeriod._detOnTime[detLoop] / 10;
			pTma->_currPeriod._Lanes[laneXdet]._xSuspect = (susDetec)pTma->_currPeriod._detFaulty[detLoop];
		}

		laneIndet=findXdetLane(detLoop+1, 0, GetNoLanes(), GetDetArray());

		if (laneIndet >= 0)
		{
			flow[laneIndet] += pTma->_currPeriod._detFlow[detLoop];
			detOn[laneIndet] += pTma->_currPeriod._detOnTime[detLoop] / 10;
			pTma->_currPeriod._Lanes[laneIndet]._xSuspect = (susDetec)pTma->_currPeriod._detFaulty[detLoop];
		}
	}


	/*
	After counting flow and detector on, sum lanes conforming stages
	flow is counting over the two detectors...
	*/
	for (stgLoop = 0; stgLoop < GetNoStages(); stgLoop++)
	{
		sus_Value = NoFault;
		susFlag = 0;
		pTma->_sumOccuLog[pTma->peUserTo]._xFlow[stgLoop] = 0;
		detOnTemp[stgLoop] = 0;
		flowTemp[stgLoop] = 0;
		sumXflowStg = 0;

		lanesRunStage(stgLoop, &lanesRunStg[0], GetLlaneArray(), GetLgreenArray());

		for (laneLoop = 0; laneLoop < GetNoLanes() && lanesRunStg[laneLoop] >= 0; laneLoop++)
		{
			/* summing occupancy for all lanes forming the stage stgLoop */
			detOnTemp[stgLoop] += detOn[lanesRunStg[laneLoop]];
			/* for occupancy calculation only*/
			flowTemp[stgLoop] += flow[lanesRunStg[laneLoop]];
			/* only using X flow for lanes running in "s" stage*/
			sumXflowStg += pTma->_currPeriod._Lanes[lanesRunStg[laneLoop]]._xFlow;

			/*suspect Detectors wil be modified when detector log is done*/
			if (susFlag == 0 && pTma->_currPeriod._Lanes[lanesRunStg[laneLoop]]._xSuspect != NoFault)
			{
				susFlag = 1;
				sus_Value = pTma->_currPeriod._Lanes[lanesRunStg[laneLoop]]._xSuspect;
			}

			if (susFlag == 0 && pTma->_currPeriod._Lanes[lanesRunStg[laneLoop]]._inSuspect != NoFault)
			{
				susFlag=1;
				sus_Value = pTma->_currPeriod._Lanes[lanesRunStg[laneLoop]]._inSuspect;
			}

		}

		pTma->_sumOccuLog[pTma->peUserTo]._xInSuspect[stgLoop] = sus_Value;
		pTma->_sumOccuLog[pTma->peUserTo]._xFlow[stgLoop] = sumXflowStg;
	}


	for (stgLoop = 0; stgLoop < GetNoStages(); stgLoop++)
	{
		if (flowTemp[stgLoop] > 0)
		{
			pTma->_sumOccuLog[pTma->peUserTo]._xInOcc[stgLoop] = detOnTemp[stgLoop] / flowTemp[stgLoop];
		}
	}

	/*totals*/
	pTma->_sumOccuLog[pTma->peUserTo].totalFlow = 0;
	pTma->_sumOccuLog[pTma->peUserTo].totalOcc = 0;
	totalOccFlow = 0;

	for (laneLoop = 0; laneLoop < GetNoLanes(); laneLoop++)
	{
		/*to keep period lane information up to date, so next logs can use flows and occupancy values*/
		pTma->_currPeriod._Lanes[laneLoop]._xInOcc = detOn[laneLoop] / flow[laneLoop];
		totalOccFlow += flow[laneLoop];
		pTma->_sumOccuLog[pTma->peUserTo].totalOcc += detOn[laneLoop];	/*just summing time on*/
		pTma->_sumOccuLog[pTma->peUserTo].totalFlow += pTma->_currPeriod._Lanes[laneLoop]._xFlow;
	}

	if (totalOccFlow > 0)
		pTma->_sumOccuLog[pTma->peUserTo].totalOcc = pTma->_sumOccuLog[pTma->peUserTo].totalOcc / totalOccFlow; /*calculating total occupancy*/
	else
		pTma->_sumOccuLog[pTma->peUserTo].totalOcc = 0;

	pTma->_sumOccuLog[pTma->peUserTo]._timeStgEnd_OC = pTma->nextPerStarts;

}


/*************************************************************************
 *
 * Function: GetOccupancyLogDetails
 *
 * Description: Get the stage occupancy details
 *				for the TMA period under consideration
 *				
 * Author: PAnderson
 *
 * Date: 18/09/2013
 *
 * Params:	StageID				The stage in question
 *			PeriodInstance		The Period in question
 *			time 				The resulting time
 			xInOcc 				The average occupancy for X and In detectors in the stage
 			xFlow				The total x-flow for the stage
 			xInSuspect 			Indicate suspect detector at any time during period
 * Return: void
 *
 ************************************************************************/

void GetOccupancyLogDetails(int StageID, int PeriodInstance, time_t* time, float* xInOcc, float* xFlow, int* xInSuspect)
{
	*time =  pTmaData->_sumOccuLog[PeriodInstance]._timeStgEnd_OC;
	*xInOcc = pTmaData->_sumOccuLog[PeriodInstance]._xInOcc[StageID];
	*xFlow = pTmaData->_sumOccuLog[PeriodInstance]._xFlow[StageID];
	*xInSuspect = pTmaData->_sumOccuLog[PeriodInstance]._xInSuspect[StageID];
}



/*************************************************************************
 *
 * Function: GetOccupancyLogTotalDetails
 *
 * Description: Get the stage occupancy details
 *				for the TMA period under consideration
 *				
 * Author: PAnderson
 *
 * Date: 18/09/2013
 *
 * Params:
 *			PeriodInstance		The Period in question
 			totalOcc 			The total occupancy for the junction
 			totalFlow 			The total flow for the junction
 * Return: void
 *
 ************************************************************************/
void GetOccupancyLogTotalDetails(int PeriodInstance, float* totalOcc, float* totalFlow)
{
	*totalOcc = pTmaData->_sumOccuLog[PeriodInstance].totalOcc;
	*totalFlow = pTmaData->_sumOccuLog[PeriodInstance].totalFlow;
}

/*************************************************************************
 *
 * Function: print_OccLogTitle
 *
 * Description: Function to print Occupancy logs header/title
 *
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	int outDevice:-		Destination for user communication
 *			int noStages:-		Maximum number of stages
 *			
 * Return: void
 *
 ************************************************************************/
static void print_OccLogTitle(int outDevice, int noStages)
{
	int stgID;

	/*titles*/
	strcpy(buf, "Table of average occupancy and X-flows\r\n");
	strcat(buf, "Stage number/occupancy/count/suspect detector?\r\n");
	strcat(buf, ",,");

	for (stgID = 0; stgID < noStages; stgID++)
	{
		sprintf(tmp, "Stg%d,, S?,", stgID+1);
		strcat(buf, tmp);
	}

	strcat(buf, ",");

	strcat(buf, "\r\nDay,Date/time,");
	writeTMAString(outDevice, buf);

	memset(buf, 0, MAX_OUTPUT_LEN);
	strcpy(tmp, "occ, xflow,,");

	for (stgID = 0; stgID < noStages; stgID++)
	{
		strcat(buf, tmp);
	}

	strcat(buf, " Total Occ, Total Xflow \r\n");
	writeTMAString(outDevice, buf);

}



/*************************************************************************
 *
 * Function: print_OccLogLine
 *
 * Description: Function to print one Occupany logs 
 *				for the TMA period under consideration(i.e a line of Occupancy log)
 *				
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	int outDevice:-					Destination for user communication
 *			summa_OccuLog pOccLog:-			Data Structure holding Occupany logs information for the current period
 *			int noStages:-					Maximum number of stages 
 *
 * Return: void
 *
 ************************************************************************/
static void print_OccLogLine(int outDevice, const summa_OccuLog *const pOccLog, int noStages)
{
	struct tm ltime;
	int stgID;

	/*time*/
	localtime_r(&pOccLog->_timeStgEnd_OC, &ltime);
	strftime (buf, MAX_OUTPUT_LEN, "%A, %d/%m/%y to %H:%M:%S hrs.,", &ltime);

	/*values*/
	for (stgID = 0; stgID < noStages; stgID++) /*all max lanes for MOVA*/
	{
		sprintf(tmp, "%5.2f, %5.0f, %c, ", pOccLog->_xInOcc[stgID], pOccLog->_xFlow[stgID], conSuspectToCh(pOccLog->_xInSuspect[stgID]));
		strcat(buf, tmp);
	}

	sprintf(tmp, " %5.2f, %5.0f \r\n", pOccLog->totalOcc, pOccLog->totalFlow);

	strcat(buf, tmp);
	writeTMAString(outDevice, buf);

}



/*************************************************************************
 *
 * Function: print_OccupancyLog
 *
 * Description: Functions which determines the start Occupany log/end Occupancy log
 *				within circular queue of logs and calls a function to print each log within the logs boundary.
 *
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	strcTMA *pTma:- Pointer to TMA data structure 
 *			int outDevice:-		Destination for user communication
 *			int noStages:-		Maximum number of Stages 
 *
 * Return: void
 *
 ************************************************************************/
void print_OccupancyLog(strcTMA *const pTma, int outDevice, int noStages)
{
	int logsLoop = 0;
	/*the number of logs to be printed*/
	int noLogsToP = 0;
	int posQ = pTma->peUserFrom;

	if (pTma->peUserTo != pTma->peUserFrom)
	{
		print_OccLogTitle(outDevice,noStages);

		if (pTma->peUserTo > pTma->peUserFrom) /*array is not full */
		{
			noLogsToP= pTma->peUserTo;
		}
		else /*the array has been full at some point in the past*/
		{
			noLogsToP = sumLogMAX;
			if (posQ > 0)
				posQ--;
			else
				posQ = sumLogMAX - 1;

		}

		while (logsLoop < noLogsToP)
		{
			print_OccLogLine(outDevice, &pTma->_sumOccuLog[posQ], noStages);
			posQ = (posQ + 1) % (sumLogMAX);
			logsLoop++;
		}
	}
}



/*************************************************************************
 *
 * Function: prepare_EndSat_Log
 *
 * Description: Function which process the End of Saturation logs information collected during the latest
 *				TMA period.
 *				
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	strcTMA *pTma:- Pointer to TMA data structure 
 *
 * Return: void
 *
 ************************************************************************/
static void prepare_EndSat_Log(strcTMA *const pTma)
{
	int linkLoop,endsatLoop,stgLoop,maxLink, endedReason;
	int maxEndSat;	
	int linksRunStg[mxlink];

	for (stgLoop = 0; stgLoop < GetNoStages(); stgLoop++)
	{
		maxLink = maxEndSat = endedReason = 0;
		pTma->_sumEndSatLog[pTma->peUserTo]._stageNo[stgLoop] = (unsigned char)(stgLoop+1);
		
		linksRunStage(stgLoop, &linksRunStg[0], GetLgreenArray());
		/*checking for the maximum value of end of saturation repetition
		for all links in stgLoop stage*/

		for (linkLoop = 0; linkLoop < GetNoLinks() && linksRunStg[linkLoop] != -1; linkLoop++)
		{
			for (endsatLoop = 0; endsatLoop < endSatMAXReasons; endsatLoop++)
			{
				if (pTma->_currPeriod._LinksEndSat[linksRunStg[linkLoop]][endsatLoop] > maxEndSat)
				{
					maxEndSat = pTma->_currPeriod._LinksEndSat[linksRunStg[linkLoop]][endsatLoop];
					maxLink = linksRunStg[linkLoop] + 1;
					endedReason = endsatLoop + 1;
				}
			}
		}

		pTma->_sumEndSatLog[pTma->peUserTo]._endSatReason[stgLoop] = (unsigned char)endedReason;
		pTma->_sumEndSatLog[pTma->peUserTo]._linkNo[stgLoop] = (unsigned char)maxLink;
	}

	pTma->_sumEndSatLog[pTma->peUserTo]._timeStgEnd_ES = pTma->nextPerStarts;
}



/*************************************************************************
 *
 * Function: print_EndSatLogTitle
 *
 * Description: Function to print End of Saturation logs header/title
 *
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	int outDevice:-		Destination for user communication
 *			int noStages:-		Maximum number of stages
 *			
 * Return: void
 *
 ************************************************************************/
static void print_EndSatLogTitle(int outDevice, int noStages)
{
	int stgID;

	/*titles*/
	strcpy(buf, "Table shows number of times end-sat 'Decision Type' occurs by stage\r\n");
	strcat(buf, "and single link that was most frequently associated with that end-sat decision type \r\n");
	strcat(buf, "Day,Date/time,");

	for (stgID = 0; stgID < noStages; stgID++)
	{
		strcat(buf, "Stg, lnk, type,");
	}

	strcat(buf, "\r\n");
	writeTMAString(outDevice, buf);

}


/*************************************************************************
 *
 * Function: GetEndSatLogDetails
 *
 * Description: Get the end sat details
 *				for the TMA period under consideration
 *				
 * Author: PAnderson
 *
 * Date: 18/09/2013
 *
 * Params:
 *			PeriodInstance		The Period in question
 			StageID				The stage in question
 			time 				The time in question
 			stageNo 			The stage number most associated with end sat
 			linkNo 				Link to go end sat in stage 
 			endSatReason 		End sat decision
 * Return: void
 *
 ************************************************************************/
void GetEndSatLogDetails(int StageID, int PeriodInstance, time_t* time, unsigned char* stageNo, unsigned char* linkNo, unsigned char* endSatReason)
{
	*time =  pTmaData->_sumEndSatLog[PeriodInstance]._timeStgEnd_ES;
	*stageNo = pTmaData->_sumEndSatLog[PeriodInstance]._stageNo[StageID];
	*linkNo = pTmaData->_sumEndSatLog[PeriodInstance]._linkNo[StageID];
	*endSatReason = pTmaData->_sumEndSatLog[PeriodInstance]._endSatReason[StageID];
}

/*************************************************************************
 *
 * Function: print_EndSatLogLine
 *
 * Description: Function to print one End of Saturation logs 
 *				for the TMA period under consideration(i.e a line of End of Saturation log)
 *				
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	int outDevice:-					Destination for user communication
 *			summa_EndSatLog pEndSatLog:-	Data Structure holding End of Saturation logs information for the current period
 *			int noStages:-					Maximum number of stages 
 *
 * Return: void
 *
 ************************************************************************/
static void print_EndSatLogLine(int outDevice, const summa_EndSatLog *const pEndSatLog, int noStages)
{
	struct tm ltime;
	int stgID;

	/*time*/
	localtime_r(&pEndSatLog->_timeStgEnd_ES, &ltime);
	strftime (buf, MAX_OUTPUT_LEN, "%A, %d/%m/%y to %H:%M:%S hrs. ", &ltime);

	/*values*/
	for (stgID = 0; stgID < noStages; stgID++) /*all max lanes for MOVA*/
	{
		sprintf(tmp, ", S%d, %d, %d", pEndSatLog->_stageNo[stgID], pEndSatLog->_linkNo[stgID], pEndSatLog->_endSatReason[stgID]);
		strcat(buf, tmp);
	}

	strcat(buf, "\r\n");
	writeTMAString(outDevice, buf);
}



/*************************************************************************
 *
 * Function: print_EndSatLog
 *
 * Description: Functions which determines the start End of Saturation log/end end of saturation log
 *				within circular queue of logs and calls a function to print each log within the logs boundary.
 *
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	strcTMA *pTma:- Pointer to TMA data structure 
 *			int outDevice:-		Destination for user communication
 *			int noStages:-		Maximum number of Stages 
 *			int noLinks:-		Maximum number of Links 
 *			int noLanes:-		Maximum number of Lanes 
 *
 * Return: void
 *
 ************************************************************************/
void print_EndSatLog(strcTMA *const pTma, int outDevice, int noStages)
{
	int logsLoop = 0;
	/*the number of logs to be printed*/
	int noLogsToP = 0;
	int posQ = pTma->peUserFrom;

	if (pTma->peUserTo != pTma->peUserFrom)
	{
		print_EndSatLogTitle(outDevice, noStages);
		if (pTma->peUserTo > pTma->peUserFrom) /*array is not full */
		{
			noLogsToP = pTma->peUserTo;
		}
		else /*the array has been full at some point in the past*/
		{
			noLogsToP = sumLogMAX;
			if (posQ > 0)
				posQ--;
			else
				posQ = sumLogMAX - 1;
		}

		while (logsLoop < noLogsToP)
		{
			print_EndSatLogLine(outDevice, &pTma->_sumEndSatLog[posQ], noStages);
			posQ = (posQ + 1) % (sumLogMAX);
			logsLoop++;
		}
	}
}



/*************************************************************************
 *
 * Function: GetSuspectDetectorHistoryLogDetails
 *
 * Description: Faulty detector history
 *              
 * Author: PAnderson
 *
 * Date: 18/09/2013
 *
 * Params:
 * 			FaultInstance		The fault in question
			ID 					The ID of the detector
			FaultType 			The fault type
			usedEntry 			for repeated failures, when "a" detector is reported more than once as faulty
			TimeFaulted 		Time fault occured first
			timeReturnedWork 	Time when detector returned to working state

 * Return: void
 *
 ************************************************************************/

void GetSuspectDetectorHistoryLogDetails( int FaultInstance, int* ID, int* FaultType, int* usedEntry, time_t* TimeFaulted, time_t* TimeReturned)
{
	*ID =  pTmaData->detFaultHist[FaultInstance]._ID;
	*FaultType =  pTmaData->detFaultHist[FaultInstance]._FaultType;
	*usedEntry =  pTmaData->detFaultHist[FaultInstance]._usedEntry;
	*TimeFaulted =  pTmaData->detFaultHist[FaultInstance]._timeFaulted;
	*TimeReturned =  pTmaData->detFaultHist[FaultInstance]._timeReturnedWork;
}


/*************************************************************************
 *
 * Function: GetSuspectDetectorIndex
 *
 * Description: Return nos of fault detectors in list
 *              
 * Author: PAnderson
 *
 * Date: 18/09/2013
 *
 * Params:
			FirstDetector The first faulty detector
			LastDetector The last faulty detector

 * Return: void
 *
 ************************************************************************/
void GetSuspectDetectorIndex(int* FirstDetector, int* LastDetector)
{
	*FirstDetector = pTmaData->pDetHisFrom;
	*LastDetector = pTmaData->pDetHisTo;
}

/*************************************************************************
 *
 * Function: print_FaultyDetecLog
 *
 * Description: Functions which determines the start Faulty detector log/end faulty detector log
 *				within circular queue of logs and calls a function to print each log within the logs boundary.
 *
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	strcTMA *pTma:- Pointer to TMA data structure 
 *			int outDevice:-		Destination for user communication
 *			int noStages:-		Maximum number of Lanes 
 *
 * Return: void
 *
 ************************************************************************/
void print_FaultyDetecLog(strcTMA *const pTma, int outDevice)
{
	struct tm ltime;
	int logsLoop = 0;

	/*the number of logs to be printed*/
	int noLogsToP = 0;
	int posQ = pTma->pDetHisFrom;

	if (pTma->pDetHisTo > 0) /*the array is not empty*/
	{
		if (pTma->pDetHisTo > pTma->pDetHisFrom) /*array is not full*/
		{
			noLogsToP = pTma->pDetHisTo - pTma->pDetHisFrom;
		}
		else /*the array has been full at some point in the past*/
		{
			noLogsToP = (detMAXHist - pTma->pDetHisFrom) + pTma->pDetHisTo;
		}
	}

	/*title*/
	strcpy(buf, "Suspect Detector History\r\n");
	writeTMAString(outDevice, buf);

	/*history*/
	while (logsLoop < noLogsToP)
	{
		sprintf(buf, "Det no. %d, fault type %d on ", pTma->detFaultHist[posQ]._ID+1, pTma->detFaultHist[posQ]._FaultType);
		localtime_r(&pTma->detFaultHist[posQ]._timeFaulted, &ltime);
		strftime (tmp, TMP_BUF_LEN, "%A %d/%m/%y at %H:%M:%Shrs ", &ltime);
		strcat(buf, tmp);

		if (pTma->detFaultHist[posQ]._usedEntry == 1) /*record completed, detector is working again*/
		{
			strcat(buf, " Returned to working on ");
			localtime_r(&pTma->detFaultHist[posQ]._timeReturnedWork, &ltime);
			strftime (tmp, TMP_BUF_LEN, "%A %d/%m/%y at %H:%M:%Shrs ", &ltime);
			strcat(buf, tmp);
		}

		strcat(buf, "\r\n");
		writeTMAString(outDevice, buf);

		/*next pos*/
		posQ++;
		posQ = (posQ) % (detMAXHist - 1);
		logsLoop++;
	}
}



/*************************************************************************
 *
 * Function: prepare_Oversatured_Log
 *
 * Description: Function which process the OverSaturation logs information collected during the latest
 *				TMA period.
 *				
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	strcTMA *pTma:- Pointer to TMA data structure 
 *
 * Return: void
 *
 ************************************************************************/
static void prepare_Oversatured_Log(strcTMA *const pTma)
{
	int laneID;

	for (laneID = 0; laneID < GetNoLanes(); laneID++)
	{		
		pTma->_sumOverSatLog[pTma->peUserTo]._overSatCy[laneID] = pTma->_currPeriod._Lanes[laneID]._noOveSatCycles;
	}

	pTma->_sumOverSatLog[pTma->peUserTo]._timeStgEnd_OS = pTma->nextPerStarts;
}



/*************************************************************************
 *
 * Function: GetOverSatLogDetails
 *
 * Description: Faulty detector history
 *              
 * Author: PAnderson
 *
 * Date: 18/09/2013
 *
 * Params:
			LaneID 			The lane in question
			PeriodInstance 	The Period in question
			time 			The end time for oversat for this period
			overSatCy 		The number of cycles that over sat occurs in this period
 * Return: void
 *
 ************************************************************************/

void GetOverSatLogDetails( int LaneID, int PeriodInstance, time_t* time, int* overSatCy)
{
	*time =  pTmaData->_sumOverSatLog[PeriodInstance]._timeStgEnd_OS;
	*overSatCy =  pTmaData->_sumOverSatLog[PeriodInstance]._overSatCy[LaneID];
}


/*************************************************************************
 *
 * Function: print_OverSatLogTitle
 *
 * Description: Function to print Over Saturation logs header/title
 *
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	int outDevice:-		Destination for user communication
 *			int noLanes:-		Maximum number of lanes
 *			
 * Return: void
 *
 ************************************************************************/
static void print_OverSatLogTitle(int outDevice, int noLanes)
{
	int laneID;

	/*titles*/
	strcpy(buf, "Oversaturated cycle lane counts\r\n");
	strcat(buf, "Day,Date/Time, Oversaturated cycle counts\r\n");
	strcat(buf, "Lane number,,");

	for (laneID = 0; laneID < noLanes; laneID++)
	{
		sprintf(tmp, "%d,", laneID + 1);
		strcat(buf, tmp);
	}

	strcat(buf,"\r\n");
	writeTMAString(outDevice, buf);

}



/*************************************************************************
 *
 * Function: print_OverSatLogLine
 *
 * Description: Function to print one Over Saturation logs 
 *				for the TMA period under consideration(i.e a line of Over Saturation log)
 *				
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	int outDevice:-					Destination for user communication
 *			summa_OversatLog pOverSatLog:-	Data Structure holding Over Saturation logs information for the current period
 *			int noLanes:-					Maximum number of lanes 
 *
 * Return: void
 *
 ************************************************************************/
static void print_OverSatLogLine(int outDevice, const summa_OversatLog *const pOverSatLog, int noLanes)
{
	struct tm ltime;
	int laneID;

	/*time*/
	localtime_r(&pOverSatLog->_timeStgEnd_OS, &ltime);
	strftime (buf, MAX_OUTPUT_LEN, "%A, %d/%m/%y to %H:%M:%S hrs. ", &ltime);

	/*values*/
	for (laneID = 0; laneID < noLanes; laneID++) /*all max lanes for MOVA*/
	{
		sprintf(tmp, ",%d", pOverSatLog->_overSatCy[laneID]);
		strcat(buf, tmp);
	}

	strcat(buf, "\r\n");
	writeTMAString(outDevice, buf);
}



/*************************************************************************
 *
 * Function: print_OversatLog
 *
 * Description: Functions which determines the start Over Saturation log/end Over Saturation log
 *				within circular queue of logs and calls a function to print each log within the logs boundary.
 *
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	strcTMA *pTma:- Pointer to TMA data structure 
 *			int outDevice:-		Destination for user communication
 *			int noStages:-		Maximum number of Lanes 
 *
 * Return: void
 *
 ************************************************************************/
 void print_OversatLog(strcTMA *const pTma, int outDevice, int noLanes)
{
	int logsLoop = 0;
	/*the number of logs to be printed*/
	int noLogsToP = 0;
	int posQ = pTma->peUserFrom;

	if (pTma->peUserTo != pTma->peUserFrom)
	{
		print_OverSatLogTitle(outDevice, noLanes);

		if (pTma->peUserTo > pTma->peUserFrom) /*array is not full */
		{
			noLogsToP = pTma->peUserTo;
		}
		else /*the array has been full at some point in the past*/
		{
			noLogsToP= sumLogMAX;

			if (posQ > 0)
				posQ--;
			else
				posQ = sumLogMAX - 1;
		}

		while (logsLoop < noLogsToP)
		{
			print_OverSatLogLine(outDevice, &pTma->_sumOverSatLog[posQ], noLanes);
			posQ = (posQ + 1) % (sumLogMAX);
			logsLoop++;
		}
	}
}



/*************************************************************************
 *
 * Function: prepare_TraStg_Log
 *
 * Description: Function which process the Level of service to vehicles logs information collected during the latest
 *				TMA period.
 *				
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	strcTMA *pTma:- Pointer to TMA data structure 
 *
 * Return: void
 *
 ************************************************************************/
static void prepare_TraStg_Log(strcTMA *const pTma, int dsLast)
{
	int stgID;

	for (stgID = 0; stgID < GetNoStages(); stgID++)
	{
		pTma->_sumTraSerLog[pTma->peUserTo]._NoServices[stgID] = pTma->_currPeriod._llStage[stgID]._trafStgRun;

		if (pTma->_currPeriod._llStage[stgID]._trafStgRun > 0)
			pTma->_sumTraSerLog[pTma->peUserTo]._Mean[stgID] = (pTma->_currPeriod._llStage[stgID]._timeTotTrafStg)/pTma->_currPeriod._llStage[stgID]._trafStgRun;
		else
			pTma->_sumTraSerLog[pTma->peUserTo]._Mean[stgID] = 0;
	}

	pTma->_sumTraSerLog[pTma->peUserTo]._timeStgEnd_TS = pTma->nextPerStarts;
	pTma->_sumTraSerLog[pTma->peUserTo]._lastDataSet = dsLast;
}


/*************************************************************************
 *
 * Function: GetTrafficServiceLogDetails
 *
 * Description: The 
 *              
 * Author: PAnderson
 *
 * Date: 18/09/2013
 *
 * Params:
			StageID 		The stage in question
			PeriodInstance 	The Period in question
			time 			End time for this service log period.
			_NoServices 	Number of times stage has been ran
			Mean 			The average length of stage
			lastDataSet 	Repo Number running at end of period
 * Return: void
 *
 ************************************************************************/


void GetTrafficServiceLogDetails(int StageID, int PeriodInstance, time_t* time, int* _NoServices, float* Mean, int* lastDataSet)
{
	*time =  pTmaData->_sumTraSerLog[PeriodInstance]._timeStgEnd_TS;
	*_NoServices = pTmaData->_sumTraSerLog[PeriodInstance]._NoServices[StageID];
	*Mean = pTmaData->_sumTraSerLog[PeriodInstance]._Mean[StageID];
	*lastDataSet = pTmaData->_sumTraSerLog[PeriodInstance]._lastDataSet;
}


/*************************************************************************
 *
 * Function: print_TraSevLogTitle
 *
 * Description: Function to print Travel service logs headers(title)
 *
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	int outDevice:-		Destination for user communication
 *			int noStages:-		Maximum number of stages
 *			
 * Return: void
 *
 ************************************************************************/

static void print_TraSevLogTitle(int outDevice, int noStages)
{
	int stgID;

	/*titles*/
	strcpy(buf, "Number of times stages ran and average stage length\r\n");
	strcat(buf, "Stage,,");

	for (stgID = 0; stgID < noStages; stgID++)
	{
		sprintf(tmp, "Stg%d,,", stgID + 1);
		strcat(buf, tmp);
	}

	strcat(buf, "Dataset\r\n");
	strcat(buf, ",Date/time,");
	writeTMAString(outDevice, buf);

	memset(buf, 0, MAX_OUTPUT_LEN);

	for (stgID = 0; stgID < noStages; stgID++)
	{
		strcat(buf, "No, len(s),");
	}

	strcat(buf," Number\r\n");
	writeTMAString(outDevice, buf);

}



/*************************************************************************
 *
 * Function: print_TraSevLogLine
 *
 * Description: Function to print one Travel service logs 
 *				for the TMA period under consideration(i.e a line of travel service log)
 *				
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	int outDevice:-					Destination for user communication
 *			summa_TrafSerLog pTraSerLog:-	Data Structure holding Travel service logs information for the current period
 *			int noStages:-					Maximum number of Stages 
 *
 * Return: void
 *
 ************************************************************************/
static void print_TraSevLogLine(int outDevice, const summa_TrafSerLog *const pTraSerLog, int noStages)
{
	struct tm ltime;
	int stgID;

	/*time*/
	localtime_r(&pTraSerLog->_timeStgEnd_TS, &ltime);
	strftime (buf, MAX_OUTPUT_LEN, "%A, %d/%m/%y to %H:%M:%S hrs.,", &ltime);

	/*headers*/
	for (stgID = 0; stgID < noStages; stgID++)
	{
		sprintf(tmp, "%d, %2.0f,", pTraSerLog->_NoServices[stgID], pTraSerLog->_Mean[stgID]);
		strcat(buf, tmp);
	}

	sprintf(tmp, "F = %d\r\n", pTraSerLog->_lastDataSet);
	strcat(buf, tmp);

	writeTMAString(outDevice, buf);

}



/*************************************************************************
 *
 * Function: print_TraSevLog
 *
 * Description: Functions which determines the start travel service log/end travel service log
 *				within circular queue of logs and calls a function to print each log within the logs boundary.
 *
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	strcTMA *pTma:- Pointer to TMA data structure 
 *			int outDevice:-		Destination for user communication
 *			int noStages:-		Maximum number of stages 
 *
 * Return: void
 *
 ************************************************************************/
void print_TraSevLog(strcTMA *const pTma, int outDevice, int noStages)
{
	int logsLoop = 0;
	/*the number of logs to be printed*/
	int noLogsToP = 0;
	int posQ = pTma->peUserFrom;	

	if (pTma->peUserTo != pTma->peUserFrom)
	{
		print_TraSevLogTitle(outDevice, noStages);

		if (pTma->peUserTo > pTma->peUserFrom) /*array is not full */
		{
			noLogsToP = pTma->peUserTo;
		}
		else /*the array has been full at some point in the past*/
		{
			noLogsToP = sumLogMAX;
			if (posQ > 0)
				posQ--;
			else
				posQ = sumLogMAX - 1;
		}

		while (logsLoop < noLogsToP)
		{
			print_TraSevLogLine(outDevice, &pTma->_sumTraSerLog[posQ], noStages);
			posQ = (posQ + 1) % (sumLogMAX);
			logsLoop++;
		}
	}
}



/*************************************************************************
 *
 * Function: prepare_PedStg_Log
 *
 * Description: Function which process the Pedestrain Stage logs information collected during the latest
 *				TMA period.
 *				
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	strcTMA *pTma:- Pointer to TMA data structure 
 *
 * Return: void
 *
 ************************************************************************/
static void prepare_PedStg_Log(strcTMA *const pTma)
{

	pTma->_sumPedSerLog[pTma->peUserTo]._totalP = 0;
	pTma->_sumPedSerLog[pTma->peUserTo]._totalSquareP = 0;

	/*One of the ped detectors were ON in this period. Cannot ignore the time served. So skip
	processing till the last ped det is off. And then process the details*/
	
	pTma->_currPeriod._Ped._pedPending = FALSE;

	if (pTma->_currPeriod._Ped._recStarted == TRUE)
	{
		if (pTma->_currPeriod._Ped._timeDemanded != 0)
		{
			pTma->_currPeriod._Ped._pedPending = TRUE;	
			pTma->_currPeriod._Ped._pedLastStgTime = (int)(Com_read_rtc_timet() - pTma->_currPeriod._Ped._timeDemanded);
		}
	}

	if (pTma->_currPeriod._Ped._pedStgRunP > 1)
	{
		pTma->_sumPedSerLog[pTma->peUserTo]._MeanP = (float)(pTma->_currPeriod._Ped._timeTotPedStgP/ pTma->_currPeriod._Ped._pedStgRunP);	
		pTma->_sumPedSerLog[pTma->peUserTo]._SDevP = TMA_PreP_StdDev(pTma->_currPeriod._Ped._pedStgRunP, pTma->_sumPedSerLog[pTma->peUserTo]._MeanP, pTma->_currPeriod._Ped._timeSqrPedStgP);
		pTma->_sumPedSerLog[pTma->peUserTo]._totalP = pTma->_currPeriod._Ped._timeTotPedStgP;
		pTma->_sumPedSerLog[pTma->peUserTo]._totalSquareP = pTma->_currPeriod._Ped._timeSqrPedStgP;
		pTma->_sumPedSerLog[pTma->peUserTo]._NoServicesP = (unsigned char)pTma->_currPeriod._Ped._pedStgRunP;			
	}
	else
	{
		pTma->_sumPedSerLog[pTma->peUserTo]._MeanP = 0;
		pTma->_sumPedSerLog[pTma->peUserTo]._SDevP = 0;
		pTma->_sumPedSerLog[pTma->peUserTo]._totalP = pTma->_sumPedSerLog[pTma->peUserTo]._totalSquareP = pTma->_sumPedSerLog[pTma->peUserTo]._NoServicesP = 0;
	}

	pTma->_currPeriod._Ped._timeTotPedStgP = 0;
	pTma->_currPeriod._Ped._timeSqrPedStgP = 0;
	pTma->_currPeriod._Ped._pedStgRunP = 0;
	pTma->_sumPedSerLog[pTma->peUserTo]._timeStgEnd_PS = pTma->nextPerStarts;
}



/*************************************************************************
 *
 * Function: print_PedSevLogTitle
 *
 * Description: Function to print level of service to pedestrain logs header/title
 *
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	int outDevice:-		Destination for user communication
 *			int allRedStg:-		Maximum number of All Red pedestrain stage
 *			
 * Return: void
 *
 ************************************************************************/
static void print_PedSevLogTitle(int outDevice, int allRedStg)
{
	int stgID;

	/*titles*/
	strcpy(buf, "Pedestrian level of service\r\n");
	strcat(buf, "average demand to service time(sec)/standard deviation(sec)/ no of pedestrian stages or phases\r\n");
	strcat(buf, "\r\nDay,Date/time,");

	for (stgID = 0; stgID < allRedStg; stgID++)
	{
		strcat(buf, "Mean, SD, No of Ser, Sum., Sum Sq,");
	}

	strcat(buf, "\r\n");
	writeTMAString(outDevice, buf);
}


/*************************************************************************
 *
 * Function: GetPedServiceLogDetails
 *
 * Description: Ped level of service log
 *              
 * Author: PAnderson
 *
 * Date: 18/09/2013
 *
 * Params:
			PeriodInstance 	The Period in question
			time 			End time for Ped service log for this period
			MeanP 			The average time from first demand to service of ped stage
			SDevP 			Standard dev of population above
			totalP 			Total time for above
			totalSquareP 	Total time squared
			NoServicesP 	The no of ped stages to indicate level of ped service

 * Return: void
 *
 ************************************************************************/

void GetPedServiceLogDetails(int PeriodInstance, time_t* time, float* MeanP, float* SDevP, float* totalP,  float* totalSquareP, unsigned char* NoServicesP)
{
	*time =  pTmaData->_sumPedSerLog[PeriodInstance]._timeStgEnd_PS;
	*MeanP = pTmaData->_sumPedSerLog[PeriodInstance]._MeanP;
	*SDevP = pTmaData->_sumPedSerLog[PeriodInstance]._SDevP;
	*totalP = pTmaData->_sumPedSerLog[PeriodInstance]._totalP;
	*totalSquareP = pTmaData->_sumPedSerLog[PeriodInstance]._totalSquareP;
	*NoServicesP = pTmaData->_sumPedSerLog[PeriodInstance]._NoServicesP;

}

/*************************************************************************
 *
 * Function: print_PedSevLogLine
 *
 * Description: Function to print one level of service to pedestrain logs 
 *				for the TMA period under consideration(i.e a line of ped service log)
 *				
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	int outDevice:-					Destination for user communication
 *			summa_PedSerLog pPedSerLog:-	Data Structure holding level of service to pedestrain information for the current period
 *			int allRedStg:-					Maximum number of All Red pedestrain stage
 *			int pedStg[]:-					All Red pedestrains stage indicator. 
 *
 * Return: void
 *
 ************************************************************************/
static void print_PedSevLogLine(int outDevice, const summa_PedSerLog *const pPedSerLog, int allRedStg, int pedStg[])
{
	struct tm ltime;
	int stgID;

	/*time*/
	localtime_r(&pPedSerLog->_timeStgEnd_PS, &ltime);
	strftime (buf, MAX_OUTPUT_LEN, "%A, %d/%m/%y to %H:%M:%S hrs. ", &ltime);

	/*headers */
	for (stgID = 0; stgID < allRedStg; stgID++)
	{
		if (pedStg[stgID] > 0)
		{
			sprintf(tmp, ",%3.0f, %3.0f, %d, %3.0f, %4.0f", pPedSerLog->_MeanP, pPedSerLog->_SDevP, pPedSerLog->_NoServicesP, pPedSerLog->_totalP, pPedSerLog->_totalSquareP);
			strcat(buf, tmp);
		}
	}

	strcat(buf, "\r\n");
	writeTMAString(outDevice, buf);
}



/*************************************************************************
 *
 * Function: print_PedSevLog
 *
 * Description: Functions which determines the start ped service log/end ped service log
 *				within circular queue of logs and calls a function to print each logs within the logs boundary.
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	strcTMA *pTma:- Pointer to TMA data structure 
 *			int outDevice:-		Destination for user communication
 *			int allRedStg:-		Maximum number of All Red pedestrain stage
 *			int pedStg[]:-		All Red pedestrains stage indicator. 
 *
 * Return: void
 *
 ************************************************************************/
void print_PedSevLog(strcTMA *const pTma, int outDevice, int allRedStg, int pedStg[])
{
	int logsLoop = 0;
	/*the number of logs to be printed*/
	int noLogsToP = 0;
	int posQ = pTma->peUserFrom;

	if (pTma->peUserTo != pTma->peUserFrom && (pedStg[0] > 0 || pedStg[1] > 0))
	{
		print_PedSevLogTitle(outDevice, allRedStg);
		if (pTma->peUserTo > pTma->peUserFrom) /*array is not full */
		{
			noLogsToP = pTma->peUserTo;
		}
		else /*the array has been full at some point in the past*/
		{
			noLogsToP = sumLogMAX;
			if (posQ > 0)
				posQ--;
			else
				posQ = sumLogMAX - 1;
		}

		while (logsLoop < noLogsToP)
		{
			print_PedSevLogLine(outDevice, &pTma->_sumPedSerLog[posQ], allRedStg, pedStg);
			posQ = (posQ + 1) % (sumLogMAX);
			logsLoop++;
		}
	}
}



/*************************************************************************
 *
 * Function: TMA_StdDev
 *
 * Description:	Function to calculate std deveation
 *
 * Author: PKale
 *
 * Date: 10/02/11
 *
 * Params:	int Count		-	Number of values recorded
 *			float Mean		-   Avegarge of numbers under consideration for std deveation
 *			float SumSqr	-	Sum of Sqr of numbers under consideration for std deveation
 *
 * Return:	float StdDev	-	If sucessful return Standard Deveation, else return error
 *
 ************************************************************************/

static float TMA_PreP_StdDev(int Count, float Mean, float SumSqr)
{
	float StdDev;

	/*Avoid divide by zero error*/
	if (Count > 1)
	{
		StdDev = (( SumSqr / (Count-1) ) - (( (float)Count/(Count-1) )*(Mean * Mean) ));

		if (StdDev > 1)
		{
			return( (float)sqrt(StdDev));
		}
		else
		{
			return (-1.0);
		}
	}
	else
	{
		return (-1.0);
	}
}



/*************************************************************************
 *
 * Function: TMA_PreP_UpdateTMALogs
 *
 * Description:	Function to process flow/occ/end sat etc log data collected during the period.
 *				Prepare to record data for next period.
 *				
 *
 * Author: PKale
 *
 * Date: 18/02/11
 *
 * Params:	pTma - Data Structure for TMA logs
 *
 * Return:	void
 *
 ************************************************************************/
void TMA_PreP_UpdateTMALogs(strcTMA *const pTma)
{
	time_t time_now;
	struct tm tnow;

	time_now =  Com_read_rtc_timet();
	localtime_r(&time_now, &tnow);

	/*Occupany to be measured every 15 minutes as stated in the requirments document*/
	if (tnow.tm_min % 15 == 0)
		checkAlertOcc(pTma);

	/*is it time for the next period */
	if (time_now >= pTma->nextPerStarts)
	{
		/*starting a new period, get all summaries */
		prepare_Flow_Log(pTma);

		/*flows are ready, so occupancy can be calculated*/
		prepare_Occupancy_Log(pTma);

		/*end sat decisions log*/
		prepare_EndSat_Log(pTma);

		/* oversat cycles log*/
		prepare_Oversatured_Log(pTma);

		/* pedestrian service log*/
		prepare_TraStg_Log(pTma, DSH_get_active_data_plan_number());

		/* stages summary log */
		prepare_PedStg_Log(pTma);

		/*and clean current period*/
		TMA_setPeriod();

		/*update the _currPeriod*/
		pTma->currPerStarted = pTma->nextPerStarts;
		pTma->nextPerStarts = GetNextPeriodBoundary();

		/*update pointers to summaries arrays*/
		pTma->peUserTo = (pTma->peUserTo+1) % (sumLogMAX);
		if (pTma->peUserTo <= pTma->peUserFrom) /*the array summary is full*/
			pTma->peUserFrom = (pTma->peUserFrom+1) % (sumLogMAX);
	}
}

char * TMA_get_log_buffer()
{
	return buf;
}
