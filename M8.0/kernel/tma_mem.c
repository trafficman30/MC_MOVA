/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         tma_mem.c
*
*  TITLE:        MOVA TMA Logs: Memory Management
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	31-May-2011		7.0.0		AA		PK		CRN003			New development and undergoing first full system testing
*	12-Dec-2011		7.0.1		AB		PK		CRN004			SatFlow and CANDET improvements
*	06-Jul-2012		7.0.3		AC		AK		CRN012			Memory leak in TMA module and statistics lost on dataset change
*	29-Sep-2012		7.0.4		AD		AK		CRN013			Use thread safe time functions
*	17-Dec-2012		7.0.5		..		PA		CRN019			Improve alerts interface
*	31-Jan-2013		7.0.5		AE		AK		M7.0.5			Issue M7.0.5
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

#include "generalfunc.h"
#include "obclock.h"
#include "tma_mem.h"
#include "tma_preprint.h"
#include "kdebug.h"


#define INVALID -1

static char	glModuleInitialised = FALSE;



/*************************************************************************
 *
 * Function: TMA_Init_
 *
 * Description: Initialise TMA module which is in-turn responsible for initialising related TMA modules
 *				and calling a function which allocates dynamic memory for TMA structures
 *
 * Author:	PKale
 *
 * Date: 31-May-2011
 *
 * Params: void
 *
 * Return: void
 *
 ************************************************************************/
void TMA_Init_( void )
{	
	time_t init_time;
	int i;

	KDEBUG_WRITE0( DEBUG_LEVEL_VERBOSE, TMAMEM, "TMA Memory : initialising." );

	glModuleInitialised = TRUE;

	TMA_PreP_Init_();
	
	tmaData.pDetHisFrom = tmaData.pDetHisTo = 0;
	for (i = 0; i < detMAXHist; i++)
	{
		tmaData.detFaultHist[i]._ID = -1;
		tmaData.detFaultHist[i]._FaultType = 0;
		tmaData.detFaultHist[i]._usedEntry = -1;
		tmaData.detFaultHist[i]._timeFaulted = 0;
		tmaData.detFaultHist[i]._timeReturnedWork = 0;
	}

	/* Initialise non-zero values - mostly times which get set to time now */
	init_time = Com_read_rtc_timet();
	for (i = 0; i < sumLogMAX; i++)
	{
		tmaData._sumFlowLog[i]._timeStgEnd_FL = init_time;
		tmaData._sumOccuLog[i]._timeStgEnd_OC = init_time;
		tmaData._sumEndSatLog[i]._timeStgEnd_ES = init_time;
		tmaData._sumOverSatLog[i]._timeStgEnd_OS = init_time;
		tmaData._sumTraSerLog[i]._timeStgEnd_TS = init_time;
		tmaData._sumPedSerLog[i]._timeStgEnd_PS = init_time;
	}

	tmaData.peUserFrom = 0;		/* first period to be stored */
	tmaData.peUserTo = 0;		/* last period */
	tmaData.pDetHisFrom = 0;	/* for faulty detector index */
	tmaData.pDetHisTo = 0;		/* for faulty detector index */	

	TMA_SetFstPeriodTime();
	tmaData.currPerStarted = Com_read_rtc_timet();

	/* calculate the end time for this period as the next 'regular' period boundary */
	/* e.g. if tnow=00:10 and tmaprd=60min, next period should start at 01:00 not 01:10 */
	tmaData.nextPerStarts = GetNextPeriodBoundary();

	/* check the period length isn't ridiculously short (i.e. we started up within 2 min of a period boundary) */
	if ((int)(tmaData.nextPerStarts - tmaData.currPerStarted) < 120)
		tmaData.nextPerStarts += tmaData.periodSec;
	

	/*set alerts*/
	TMA_setAlertsHis();
	/*clear THE period */
	TMA_setPeriod();
	
}/*end of - void MovaSatFlowDependantVar_Initialise_( void) */



/*************************************************************************
 *
 * Function: TMA_setAlertsHis
 *
 * Description: Initialise alerts structures, and limits.
 *				This function is called once no matter how many datasets plans assigned for the site  
 *
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params:	void
 *
 * Return: void
 *
 ************************************************************************/
void TMA_setAlertsHis( void )
{
	int i;

	tmaData.sAlert._flagBlocking = tmaData.sAlert._flagOccupancy = tmaData.sAlert._flagOverSat = 0;
	tmaData.sAlert._actBlocFl = tmaData.sAlert._actOccFl = tmaData.sAlert._actOvFl = 1; /*All alerts active*/

	for (i = 0; i < mxstag; i++)
	{
		tmaData.sAlert._occMaxLmt[i] = GetOcualArray()[0][i];
		tmaData.sAlert._occMinLmt[i] = GetOcualArray()[1][i];
		tmaData.sAlert._occStatus[i] = FALSE; 
	}

	for (i = 0; i < mxlane; i++)
	{
		tmaData.sAlert._osatLmt[i] = GetOsatAlArray()[i];
		tmaData.sAlert._noOveSatCy[i] = 0;
		tmaData.sAlert._noExitBlCy[i] = 0;
		tmaData.sAlert._blockLmt[i] = GetExitAlArray()[i];
		tmaData.sAlert._ebStatus[i] = FALSE; 
	}

	for (i = 0; i < mxdets; i++)
	{
		tmaData.sAlert._detFlow[i] = 0;
		tmaData.sAlert._detOnTime[i] = 0;
	}

}/*end of - void TMA_setAlertsHis(void )*/

/*************************************************************************
 *
 * Function: TMA_SetFstPeriod
 *
 * Description: This function contains logic which sets the initial start time and 
 *				computes the end time for the first TMA period logs. Note: This function
 *				is called only in two cases.
 *				1. The first time TMA logs and period are setup
 *				2. When MOVA time is changed
 *
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params: void
 *
 * Return: void
 *
 ************************************************************************/
void TMA_SetFstPeriodTime( void )
{
	tmaData.periodSec = GetTMAprd() * 60;

	/* Depending on how the dataset is loaded, it may be that this function is called prior to the dataset loading, resulting in TMAprd being 0 */
	if (tmaData.periodSec == 0)
		tmaData.periodSec = 3600;
}

/*************************************************************************
 *
 * Function: TMA_setPeriod
 *
 * Description: Resets counts/intermediate variables used within TMA logs 
 *				thus preparing it for next TMA period.
 *
 * Author: PKale
 *
 * Date: 31-May-2011
 *
 * Params: void
 *
 * Return: void
 *
 ************************************************************************/

void TMA_setPeriod(void)
{
	int i, j;

	/* General period data*/
	tmaData._currPeriod._ensatCount=0;	
	tmaData._currPeriod._Ped._pedStgRunP = 0;	
	tmaData._currPeriod._Ped._recStarted= FALSE;
	tmaData._currPeriod._Ped._redEnded = TRUE;
	tmaData._currPeriod._Ped._timeDemanded = tmaData._currPeriod._Ped._timeServiced = 0;
	tmaData._currPeriod._Ped._timeTotPedStgP = tmaData._currPeriod._Ped._timeSqrPedStgP = 0;


	for (i = 0; i < mxlane; i++)
	{	
		tmaData._currPeriod._Lanes[i]._inFlow=0;
		tmaData._currPeriod._Lanes[i]._xFlow=0;
		tmaData._currPeriod._Lanes[i]._inSuspect=NoFault;
		tmaData._currPeriod._Lanes[i]._xSuspect=NoFault;
		tmaData._currPeriod._Lanes[i]._xInOcc = 0;
		tmaData._currPeriod._Lanes[i]._noOveSatCycles=0;
	}

	for (i = 0; i < mxlink; i++)
		for (j = 0; j < endSatMAXReasons; j++)
			tmaData._currPeriod._LinksEndSat[i][j] = 0;

	/*stage base data*/
	for (i = 0; i < mxstag; i++)
	{
		tmaData._currPeriod._llStage[i]._timeStarted = tmaData._currPeriod._llStage[i]._timeEnded = Com_read_rtc_timet();
		tmaData._currPeriod._llStage[i]._trafStgRun = 0;	
		tmaData._currPeriod._llStage[i]._timeTotTrafStg = 0;
	}

	/*Detector time on */
	for (i = 0; i < mxdets; i++)
	{
		tmaData._currPeriod._detOnTime[i] = 0;
		tmaData._currPeriod._detFlow[i] = 0;
		tmaData._currPeriod._detFaulty[i] = 0;
	}

	/*end of sat links*/
	for (i = 0; i < mxlink; i++)
	{
		tmaData._currPeriod._ensatLinkSt1[i] = 0;
		tmaData._currPeriod._ensatLinkSt2[i] = 0;
	}

}/*end of - void TMA_setPeriod(void)*/


/*************************************************************************
 *
 * Function: TMA_check_clock_change
 *
 * Description: Checks whether any action is required following a change to the 
 *				internal clock.  Starts a new TMA period if necessary.
 *
 * Author: A Kirkham
 *
 * Date: 5-July-2012
 *
 * Params: void
 *
 * Return: void
 *
 ************************************************************************/
void TMA_check_clock_change(void)
{
	time_t time_now;

	time_now = Com_read_rtc_timet();
	if (time_now >= pTmaData->nextPerStarts)
	{
		/* Begin next period! */
		TMA_PreP_UpdateTMALogs(pTmaData);
	}
}


/*************************************************************************
 *
 * Function: GetNextPeriodBoundary
 *
 * Description: Checks whether any action is required following a change to the 
 *				internal clock.  Starts a new TMA period if necessary.
 *
 * Author: A Kirkham
 *
 * Date: 5-July-2012
 *
 * Params: void
 *
 * Return: time_t : The time of the next period change
 *
 ************************************************************************/
time_t GetNextPeriodBoundary()
{
	struct tm tnow;
	time_t time_now;
	long secFromMidnight;
	long periodLen;
	int periodNo;

	time_now = Com_read_rtc_timet();
	localtime_r(&time_now, &tnow);
	periodLen = pTmaData->periodSec;

	secFromMidnight = tnow.tm_hour * 3600 + tnow.tm_min * 60 + tnow.tm_sec;
	periodNo = (int)(secFromMidnight / periodLen) + 1;
	
	return time_now - secFromMidnight + (periodLen * periodNo);
}

/*************************
*	EOF tma_mem.c
**************************/