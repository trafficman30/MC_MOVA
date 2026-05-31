/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         tma_alerts.c
*
*  TITLE:        MOVA TMA Alert logs
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	31-May-2011		7.0.0		AA		PK		CRN003			New development and undergoing first full system testing
*	31-Aug-2011		7.0.1		..		PK		CRN004			SatFlow and CANDET improvements
*	30-Oct-2011		7.0.1		..		AK		CRN005			Further CANDET improvements
*	12-Dec-2011		7.0.1		AB		AK		CRN007			Implement alerts API
*	06-Jul-2012		7.0.3		AC		AK		CRN012			Memory leak in TMA module and statistics lost on dataset change
*	30-Nov-2012		7.0.5		..		AK		CRN018			Occupancy alert not resetting correctly
*	17-Dec-2012		7.0.5		..		PA		CRN019			Improve alerts interface
*	21-Jan-2013		7.0.5		..		AK		CRN022			Buffer overflow in TMA output
*	31-Jan-2013		7.0.5		AD		AK		M7.0.5			Issue M7.0.5
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "write.h"
#include "obclock.h"
#include "defines.h"

#include "tma_strc.h"
#include "tma_alerts.h"
#include "alerts.h"

#include "tma_mem.h"

#include "generalfunc.h"

static char buf[MAX_OUTPUT_LEN];


/*************************************************************************
 *
 * Function: checkAlertOcc
 *
 * Description: Function which determines the state of Occupancy alert Flag for a given lane
 *				by assessing current occupancy and comparing with thresholds
 *
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params: strcTMA *pTma:- Pointer to TMA data structure 
 *
 * Return: void
 *
 ************************************************************************/

void checkAlertOcc(strcTMA *const pTma)
{
	int detLoop, laneLoop, stgLoop, laneXdet, laneIndet = 0;
	double flowTemp[mxlane], detOnTemp[mxlane], flow[mxlane], detOn[mxlane];	
	int lanesRunStg[mxlane];		
	int temOcc;
	BOOL overallOcc = FALSE;
	
	/*count occupancy and flow per lane*/
	for (laneLoop = 0; laneLoop < GetNoLanes(); laneLoop++)
	{
		flow[laneLoop] = flowTemp[laneLoop] = 0;
		detOn[laneLoop] = detOnTemp[laneLoop] = 0;
	}

	for (detLoop = 0; detLoop < GetNoDetectors(); detLoop++)
	{
		laneXdet = findXdetLane(detLoop+1, 1, GetNoLanes(), GetDetArray());

		if (laneXdet >= 0)
		{
			flow[laneXdet] += pTma->sAlert._detFlow[detLoop];
			detOn[laneXdet] += pTma->sAlert._detOnTime[detLoop] / 10;
		}

		laneIndet = findXdetLane(detLoop+1, 0, GetNoLanes(), GetDetArray());

		if (laneIndet >= 0)
		{
			flow[laneIndet] += pTma->sAlert._detFlow[detLoop];
			detOn[laneIndet] += pTma->sAlert._detOnTime[detLoop] / 10;
		}
	}

	for (stgLoop = 0; stgLoop < GetNoStages(); stgLoop++)
	{
		detOnTemp[stgLoop] = 0;
		flowTemp[stgLoop] = 0;
		lanesRunStage(stgLoop, &lanesRunStg[0], GetLlaneArray(), GetLgreenArray());

		for (laneLoop = 0; laneLoop < GetNoLanes() && lanesRunStg[laneLoop] >= 0; laneLoop++)
		{
			/* summing occupancy for all lanes forming the stage stgLoop */
			detOnTemp[stgLoop] += detOn[lanesRunStg[laneLoop]];
			/* for occupancy calculation only*/
			flowTemp[stgLoop] += flow[lanesRunStg[laneLoop]];
		}
	}

	for (stgLoop = 0; stgLoop < GetNoStages(); stgLoop++)
	{
		/*If max/min occual is either 0 or 9.9 occupancy alert is always off*/
		if ((pTma->sAlert._occMaxLmt[stgLoop] > 0 && pTma->sAlert._occMaxLmt[stgLoop] < 99)
			&& (pTma->sAlert._occMinLmt[stgLoop] > 0 && pTma->sAlert._occMinLmt[stgLoop] < 99))
		{
			/*Avoid divide by zero error*/
			if (flowTemp[stgLoop] > 0.0)
			{
				temOcc = (int)(10.0 * detOnTemp[stgLoop] / flowTemp[stgLoop]);
			}
			else
			{
				temOcc = 0;
			}

			if ((temOcc > pTma->sAlert._occMaxLmt[stgLoop]) && !(pTma->sAlert._occStatus[stgLoop]))
			{	/* For this stage we have gone over occupied */
				pTma->sAlert._occStatus[stgLoop] = 1;
			}
			else if ((temOcc < pTma->sAlert._occMinLmt[stgLoop]) && (pTma->sAlert._occStatus[stgLoop]))
			{	/* For this stage we have cleared over occupied */
				pTma->sAlert._occStatus[stgLoop] = 0;
			}

			/*Calculate overall occupancy*/
			overallOcc = overallOcc || pTma->sAlert._occStatus[stgLoop];
		}
	}

	/*Only trigger occupancy alert if active */
	if (pTma->sAlert._actOccFl == 1)
	{
		if ((pTma->sAlert._flagOccupancy == 0) && (overallOcc))
		{ /*Alert is not active and at least one stage has gone over occupied*/
			set_alert_occupancy(Tcomshr, pTma);
		}
		else if ((pTma->sAlert._flagOccupancy == 1) && (!overallOcc))
		{ /* Alert is active and no stage is over occupied*/
			clear_alert_occupancy(Tcomshr, pTma);
		}
	}
	pTma->sAlert._flagOccupancy = overallOcc;

	/*clearing occupancy flag for the next 15 minutes*/
	for (detLoop = 0; detLoop < GetNoDetectors(); detLoop++)
	{
		pTma->sAlert._detFlow[detLoop] = 0;
		pTma->sAlert._detOnTime[detLoop] = 0;
	}
}



/*************************************************************************
 *
 * Function: TMA_check_alert_overSat

 *
 * Description: Function which determines the state of Oversaturation alert Flag for a given lane
 *				by assessing current saturation levels and comparing with thresholds
 *
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params: strcTMA *pTma:- Pointer to TMA data structure 
 *
 * Return: void
 *
 ************************************************************************/
void TMA_check_alert_overSat(strcTMA *const pTma)
{
	int laneLoop;
	int temF = 0;
	int prevSatFlag = pTma->sAlert._flagOverSat;

	for (laneLoop = 0; laneLoop < GetNoLanes(); laneLoop++)
	{
		/*If over sat limit is zero for a lane, do not consider this lane*/
		if (pTma->sAlert._osatLmt[laneLoop] > 0)
		{
			if (pTma->sAlert._noOveSatCy[laneLoop] > pTma->sAlert._osatLmt[laneLoop])
			{ /* lane has exceeded the limit set status*/
				temF = 1;
				break;  /* No need to continue checking once we have found an oversaturated lane */
			}
		}
	}

	if (temF > 0)
	{
		pTma->sAlert._flagOverSat = 1;
	}
	else
	{
		pTma->sAlert._flagOverSat = 0;
	}


	/* If saturation status has changed and triggering is enabled, send appropriate alert */
	if ((pTma->sAlert._flagOverSat != prevSatFlag) && (pTma->sAlert._actOvFl))
	{
		if (pTma->sAlert._flagOverSat)
		{
			set_alert_oversat(Tcomshr, pTma);
		}
		else
		{
			clear_alert_oversat(Tcomshr, pTma);
		}
	}
}



/*************************************************************************
 *
 * Function: GetAlertMonitoring
 *
 * Description: Function to return whether alerts are being monitored or not.
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	int	BlockingStatus	Is BlockingStatus being monitored
 * Params:	int	OverSaturationStatus Is OverSaturation Status being monitored
 * Params:	int	OccupancyStatus Is Occupancy Status being monitored
 *
 * Return: void
 *
 ************************************************************************/
void GetAlertMonitoring(BOOL *BlockingStatus, BOOL *OverSaturationStatus, BOOL *OccupancyStatus)
{
	*BlockingStatus = tmaData.sAlert._actBlocFl;
	*OverSaturationStatus = tmaData.sAlert._actOvFl; 
	*OccupancyStatus = tmaData.sAlert._actOccFl;
	return;

}


/*************************************************************************
 *
 * Function: SetAlertMonitoring
 *
 * Description: Function to set alerts status.
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	int	BlockingStatus	Is BlockingStatus being monitored
 * Params:	int	OverSaturationStatus Is OverSaturation Status being monitored
 * Params:	int	OccupancyStatus Is Occupancy Status being monitored
 *
 * Return: void
 *
 ************************************************************************/
void SetAlertMonitoring(BOOL BlockingStatus, BOOL OccupancyStatus, BOOL OverSaturationStatus)
{
	tmaData.sAlert._actBlocFl = BlockingStatus;
	tmaData.sAlert._actOvFl = OverSaturationStatus; 
	tmaData.sAlert._actOccFl = OccupancyStatus;
	return;

}

/*************************************************************************
 *
 * Function: GetAlertStatus
 *
 * Description: Function to return whether alerts are set or not.
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	int	BlockingStatus	Is Blocking Alert set.
 * Params:	int	OverSaturationStatus Is OverSaturation Alert set
 * Params:	int	OccupancyStatus Is Occupancy Alert set
 *
 * Return: void
 *
 ************************************************************************/
void GetAlertStatus(BOOL *BlockingAlert, BOOL *OverSaturationAlert, BOOL *OccupancyAlert)
{
	*BlockingAlert = tmaData.sAlert._flagBlocking;
	*OverSaturationAlert = tmaData.sAlert._flagOverSat; 
	*OccupancyAlert = tmaData.sAlert._flagOccupancy;
	return;

}

/*************************************************************************
 *
 * Function: displayAlerts
 *
 * Description: Function to display the latest status of alert flags
 *
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	int outDevice:-			Destination for user communication
 *			strcAlert *stAlert:-	Pointer to Alert logs/data structure 
 *
 * Return: void
 *
 ************************************************************************/
void displayAlerts(int outDevice, const strcAlert *const stAlert)
{
	/*Oversat flag*/
	if (stAlert->_actOvFl == 1)
	{
		strcpy(buf, "\x1B[4;38H Enabled\x1B[K ");
	}
	else
	{
		strcpy(buf, "\x1B[4;38H Disabled ");
	}

	if (stAlert->_flagOverSat == 1)
	{
		strcat(buf, "\x1B[5;38H On\x1B[K ");
	}
	else
	{
		strcat(buf, "\x1B[5;38H Off ");
	}


	/*Blocking flag*/
	if (stAlert->_actBlocFl == 1)
	{
		strcat(buf, "\x1B[7;38H Enabled\x1B[K ");
	}
	else
	{
		strcat(buf, "\x1B[7;38H Disabled ");
	}

	if (stAlert->_flagBlocking == 1)
	{
		strcat(buf, "\x1B[8;38H On\x1B[K ");
	}
	else
	{
		strcat(buf, "\x1B[8;38H Off ");
	}

	/*Occupancy flag*/
	if (stAlert->_actOccFl == 1)
	{
		strcat(buf, "\x1B[10;38H Enabled\x1B[K ");
	}
	else
	{
		strcat(buf, "\x1B[10;38H Disabled ");
	}

	if (stAlert->_flagOccupancy == 1)
	{
		strcat(buf, "\x1B[11;38H On\x1B[K ");
	}
	else
	{
		strcat(buf, "\x1B[11;38H Off ");
	}

	/* Clear any text user has entered at cursor */
	strcat(buf, "\x1B[26;40H\x1B[K ");

	writeString(outDevice, buf);
}



/*************************************************************************
 *
 * Function: changeFlagAlert
 *
 * Description: Function to Enable/Disable Alert reporting feature
 *
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	int			outDevice:-			Destination for user communication
 *			strcAlert	*stAlert:-			Pointer to Alert logs/data structure 
 *
 * Return: void
 *
 ************************************************************************/
void changeFlagAlert(int outDevice, int flagType, strcAlert *const stAlert)
{
	int state = 0;

	switch (flagType)
	{
	case 0: /*Oversat flag*/

		if (stAlert->_actOvFl == 1)
		{
			stAlert->_actOvFl = 0;
		}
		else
		{
			stAlert->_actOvFl = 1;
		}

		break;

	case 1: /*Blocking Alert*/

		if (stAlert->_actBlocFl == 1)
		{
			stAlert->_actBlocFl = 0;
		}
		else
		{
			stAlert->_actBlocFl = 1;
		}

		break;


	case 2: /*Occupancy Alert*/

		if (stAlert->_actOccFl == 1)
		{
			stAlert->_actOccFl = 0;
		}
		else
		{
			stAlert->_actOccFl = 1;
		}

		break;

	case 3: /*All on or all Off*/

		/* The alerts won't necessarily all be enabled or disabled, could be a mix.
		   If any are disabled, action is to enable all, otherwise disable all */
		if (stAlert->_actOvFl == 1 &&
			stAlert->_actBlocFl == 1 &&
			stAlert->_actOccFl == 1)
			state = 0;
		else
			state = 1;

		stAlert->_actOvFl = stAlert->_actBlocFl = stAlert->_actOccFl = state;

		break;

	default:
		return;
	}

	displayAlerts(outDevice, stAlert);
}



/*************************************************************************
 *
 * Function: TMA_check_alert_exiting
 *
 * Description: Function which determines the state of Exit alert Flag for a given lane
 *				by assessing if current lane exit is blocked for cycles greater then user defined threshold
 *
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	strcTMA *pTma:- Pointer to TMA data structure 
 *			int stgRun:- Stage No under consideration
 *			int laensa[]:- Lane end of saturation
 *
 * Return: void
 *
 ************************************************************************/

void TMA_check_alert_exiting(strcTMA *const pTma,  int stgRun, const int8 laensa[])
{
	int laneLoop;
	int temF = 0;
	int lanesRunStg[mxlane];
	int prevFlagBlocking = pTma->sAlert._flagBlocking;
	
	lanesRunStage(stgRun, &lanesRunStg[0], GetLlaneArray(), GetLgreenArray());

	for (laneLoop = 0; laneLoop < GetNoLanes() && lanesRunStg[laneLoop] >= 0; laneLoop++)
	{
		if (laensa[lanesRunStg[laneLoop]] == 3)
		{
			pTma->sAlert._noExitBlCy[lanesRunStg[laneLoop]]++;
		}
		else
		{
			pTma->sAlert._noExitBlCy[lanesRunStg[laneLoop]] = 0;
		}
	}

	for (laneLoop = 0; laneLoop < GetNoLanes() && lanesRunStg[laneLoop] >= 0; laneLoop++)
	{
		/*If exit blocking limit is zero for a lane, do not consider this lane*/
		if (pTma->sAlert._blockLmt[lanesRunStg[laneLoop]] > 0)					
		{
			if (pTma->sAlert._noExitBlCy[lanesRunStg[laneLoop]] > pTma->sAlert._blockLmt[lanesRunStg[laneLoop]])
			{ /* lane has exceeded it's limit set status. */
				pTma->sAlert._ebStatus[laneLoop] = 1;
				temF = 1;
			}
			else
			{
				pTma->sAlert._ebStatus[laneLoop] = 0;
			}
		}
	}

	if (temF > 0)
	{
		pTma->sAlert._flagBlocking = 1;
	}
	else
	{
		pTma->sAlert._flagBlocking = 0;
	}


	/* If exiting block status changed and triggering enabled, send alert update */
	if ((pTma->sAlert._actBlocFl) && (pTma->sAlert._flagBlocking != prevFlagBlocking))
	{
		if (pTma->sAlert._flagBlocking)
		{
			set_alert_exitblocked(Tcomshr, pTma);
		}
		else
		{
			clear_alert_exitblocked(Tcomshr, pTma);
		}
	}
}



/*************************************************************************
 *
 * Function: isReportedFaulty
 *
 * Description: Checks if a detector (det) have been previously reported faulty.

* Author: PKale
 *
 * Date: 08-Feb-2011
 *
 * Params: int det	-	Detector number
 *
 * Return: int state
  *				Values to returned:	
 *				the detector is not reported faulty (-1),
 *				the detector is reported faulty but has not returned to work (position in the array for the detector) and
 *				the detector is reported faulty and has returned to work (0).
 *
 *
 ************************************************************************/
int isReportedFaulty(int detID)
{
	int detLoop;
	int state = -1;
	
	for (detLoop = 0; detLoop < detMAXHist && state == -1; detLoop++)
	{
		/* the detector is in the list but its record has not been completed, i,e there is not return to work time*/
		if (pTmaData->detFaultHist[detLoop]._ID == detID) /*detector in record*/
		{
			if (pTmaData->detFaultHist[detLoop]._usedEntry <= 0) /*record not completed*/
			{
				state = detLoop;
			}
		}
	}

	return state;
}



/*************************************************************************
 *
 * Function: TMA_det_faulty_changed
 *
 * Description:	Function to store information for faulty detector. It might appear that the faulty detector information has been duplicated
 *				with the history of faulty detectors.
 *				However, for simplicity, an array of detectors going faulty at any point of the period must be separated from history of faulty detectors, that runs
 *				as long as mova is incharged or a single plan is running.
 *
 * Author:	PKale
 *
 * Date:	08-Feb-2011
 *
 * Params:	int detIdx		- Detector Index (i.e. DetNo-1)	
 *			int newState	- Current detector state (1-ON, 0-OFF)
 *
 * Return: void
 *
 ************************************************************************/
void TMA_det_faulty_changed(int detID, int newState)
{
	int index = pTmaData->pDetHisTo;
	int foundDet = isReportedFaulty(detID);
		
	if (pTmaData->_currPeriod._detFaulty[detID] == 0) /*it hasn't been set to faulty previously, change its state*/
		pTmaData->_currPeriod._detFaulty[detID] = newState;

	/*Check that is newly set faulty to record only the beginning of the faulty period*/
	if (newState > 0 && foundDet == -1)
	{
		pTmaData->detFaultHist[index]._FaultType = newState;
		pTmaData->detFaultHist[index]._ID = detID;
		pTmaData->detFaultHist[index]._timeFaulted = Com_read_rtc_timet();
		pTmaData->detFaultHist[index]._usedEntry = 0; /*this entry is not completed*/
		/*getting the next post in this circular array*/
		pTmaData->pDetHisTo++;
		pTmaData->pDetHisTo = (pTmaData->pDetHisTo) % (detMAXHist - 1);
		if (pTmaData->pDetHisTo <= pTmaData->pDetHisFrom) /*the array is full+*/
		{
			pTmaData->pDetHisFrom++;
			pTmaData->pDetHisFrom = (pTmaData->pDetHisFrom) % (detMAXHist - 1);
		}
	}

	/*The detector is cleared (newState =0), check if it still in the history, */
	if (newState == 0 && foundDet >= 0)
	{
		pTmaData->detFaultHist[foundDet]._timeReturnedWork = Com_read_rtc_timet();
		pTmaData->detFaultHist[foundDet]._usedEntry = 1; /*completed entry*/
	}
}


void TMA_reset_oversat_alert_counter(int laneIdx)
{
	pTmaData->sAlert._noOveSatCy[laneIdx] = 0;
}

void TMA_inc_oversat_counter(int laneIdx)
{
	pTmaData->_currPeriod._Lanes[laneIdx]._noOveSatCycles++;
	pTmaData->sAlert._noOveSatCy[laneIdx]++;
}

void TMA_update_endsat_data(int linksCount, const int8 liensa[])
{
	int j;

	for(j=0; j<linksCount; j++)
	{
		pTmaData->_currPeriod._ensatLinkSt1[j] = (int)liensa[j];
	}

	pTmaData->_currPeriod._ensatCount++;
}

void TMA_update_endsat_data_2(int linksCount, const int8 liensa[], int snow)
{
	int i;
	int j;
	

	for (j = 0; j < linksCount; j++)
	{
		pTmaData->_currPeriod._ensatLinkSt2[j] = (int)liensa[j];
	}

	i =- 1;

	if (pTmaData->_currPeriod._ensatCount > 0) /*it has at least 2 ENSAT links messages*/
	{
		for (j = 0; j < linksCount && i == -1; j++)
		{
			if (pTmaData->_currPeriod._ensatLinkSt1[j] != pTmaData->_currPeriod._ensatLinkSt2[j])
				i = j;
		}
	}

	if (snow > 0 && i >= 0) 
	{
		pTmaData->_currPeriod._LinksEndSat[i][pTmaData->_currPeriod._ensatLinkSt2[i]-1]++;
	}

	pTmaData->_currPeriod._ensatCount = 0;
}

void TMA_set_stage_start_time(int demsta)
{
	pTmaData->_currPeriod._llStage[demsta-1]._timeStarted = Com_read_rtc_timet();
}