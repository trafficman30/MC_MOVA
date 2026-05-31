/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         sfstats.c
*
*  TITLE:        MOVA Saturation Flow: Perform Stats and feedback satflow at real-time 
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	31-May-2011		7.0.0		AA		PK		CRN003			Collects real-time Satflow measurements at defined intervals
*																Processes SF and determines if output is ready to be feedback.
*	12-Dec-2011		7.0.1		AB		PK		CRN004			SatFlow and CANDET improvements
*	29-Sep-2012		7.0.4		..		AK		CRN014			Fixed data checksum issue after SatFlow stats feedback 
*	29-Sep-2012		7.0.4		AC		AK		CRN016			Incorrect loop termination criteria in MovaSatFlowStats_ResetAllVariables_ 
*	25-Jan-2013		7.0.5		..		AK		CRN023			Fix compiler warnings
*	31-Jan-2013		7.0.5		AD		AK		M7.0.5			Issue M7.0.5
*	15-Apr-2013		7.0.6		..		AK		CRN027			Improvements to SF feedback
*	30-Apr-2013		7.0.6		..		AK		CRN029			Correction SF output formatting
*	03-May-2013		7.0.6		..		PA		CRN031			Don't reset SF data on warm restart
*	10-May-2013		7.0.6		..		IA		CRN034			Change n1615thPercentile from int16 to REAL to avoid loss of precision
*	22-May-2013		7.0.6		AE		AK		M7.0.6			Issue M7.0.6
*
*  (c) Copyright TRL Ltd 2013. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/

/*************************
*	Includes
**************************/

/* Standard libraries */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

/* MOVA includes */
#include "kdebug.h"
#include "gbltypes.h"
#include "obclock.h"
#include "getentry.h" /* For Send_FormatString() */
#include "errhand.h"  /* For MOVA error handler */

/* Project includes */
#include "sfmath.h"
#include "sfprv.h"
#include "sfstats.h"
#include "sfdepen.h"
#include "datahand.h"

#include "dataset_interface.h"
#include "mova_constants.h"

#include "dataset_handler.h"


/*************************
*	Defines
**************************/

/*Maximum number of raw SF\STLOST periods */
#define	gn16SF_STLOST_PERIOD_MAX							((int16)6)

/*Maximum number of values used for Student T table for calculating confidence value*/
#define	gn16SF_CONFIDENCE_INTERVAL_MAX						((int16)50)

/* Number used to represent zero sat flow value */
#define gn16ZERO											((int16)0)

/* SFTIMS start and end times in minutes since midnight */
#define SFTIMS_PERIOD_START									((int)0)
#define SFTIMS_PERIOD_END									((int)1)

#define gn16FEEDBACK_NONE									((int16)0)
#define gn1FEEDBACK_PERCENTILE_15							((int16)15)
#define gn1FEEDBACK_PERCENTILE_50							((int16)50)

/*Maximum and Minimum Outline removal value for saturation flow and stlost*/
#define grSF_OUTLINERANGE_MAX								((REAL)3000.0)
#define grSF_OUTLINERANGE_MIN								((REAL)1000.0)
#define gn16SF_MET_CRITERIA_OUTLINE_REMO_RANGE				((int16)500)
#define grSTLOST_OUTLINERANGE_MAX							((REAL)3.0)
#define grSTLOST_OUTLINERANGE_MIN							((REAL)-3.0)

/*Maximum and Minimum confidence value for saturation flow and stlost*/
#define grSF_CONFIDENCE_MAX									((REAL)100.0)
#define grSF_CONFIDENCE_MIN									((REAL)-100.0)
#define grSTLOST_CONFIDENCE_MAX								((REAL)0.1)
#define grSTLOST_CONFIDENCE_MIN								((REAL)-0.1)

/* Number of tenths of seconds in 6 minutes (hour/10) */
#define grTENTHS_OF_SECONDS_TO_SIX_MINUTES_FACTOR			((REAL)3600.0)

/* Number of tenths of seconds in 1 hour */
#define grTENTHS_OF_SECONDS_TO_HOURS_FACTOR					((REAL)36000.0)

/* Ten Seconds factor */
#define grTEN_SECONDS										((REAL)10.0)

#define gn16SATFLOW											((int16) 0)
#define gn16STLOST											((int16) 1)
#define gn16SATFLOW_REJECTED								((int16) 2)
#define gn16STLOST_REJECTED									((int16) 3)

/*Standard multiplier factor for 15th percentile calculation*/
#define	gr15thPercentile_Factor								((REAL)1.306)
#define grExpSmoothCal_25Perc_Factor						((REAL)0.25)
#define grExpSmoothCal_75Perc_Factor						((REAL)0.75)



/*************************
*	Global variables 
**************************/

static LOGIC	glModuleInitialised = FALSE;
static int16	gn16SelectInterval = gn16PERIOD_INVALID;

int ShowSatFlw = 0;
SatData *gSatDataSts;

static TIMESTAMP_TYPE tTimeDate;

/*Array for students T table distribution for 99% confidence level*/
static const REAL arrConfInter[gn16SF_CONFIDENCE_INTERVAL_MAX] = 
{

	63.657f ,   9.925f   ,   5.841f   ,   4.604f   , 
	 4.032f ,   3.707f   ,   3.499f   ,   3.355f   ,
	 3.250f ,   3.169f   ,   3.106f   ,   3.055f   ,
     3.012f ,   2.977f   ,   2.947f   ,   2.921f   ,
	 2.898f ,   2.878f   ,   2.861f   ,   2.845f   ,
	 2.831f ,   2.819f   ,   2.807f   ,   2.797f   ,
     2.787f ,   2.779f   ,   2.771f   ,   2.763f  ,
     2.756f ,   2.750f   ,   2.744f   ,   2.738f   ,
     2.733f ,   2.728f   ,   2.724f   ,   2.719f   ,
     2.715f ,   2.712f   ,   2.708f   ,   2.704f   ,
     2.701f ,   2.698f   ,   2.695f   ,   2.692f   ,
     2.690f ,   2.687f   ,   2.685f   ,   2.682f   ,
     2.680f ,   2.678f  ,

};


static char sClrScr[] = { " \033[2J" };

static BOOL DatasetHasChanged = FALSE;

static BOOL SatFlowColdStart = FALSE;

/*************************
*	External functions
**************************/

#ifdef IS_MOVACOMM_ENABLED
extern int GetOutputDestination( void );
#endif

extern void checks(Ccomshr const * const, long * const, long * const );

/*************************
*	Private functions
**************************/
static void update_std_dev_data_(REAL x, PeriodData * pPData);
static int get_SF_period(void);
static void restore_original_values();
static BOOL get_measured_SF(int periodNo, uint8 laneNo);
static void MovaSatFlowStats_UpdateSFPeriod_( void );
static void MovaSatFlowStats_ChangeDataset_(void);


static void		MovaSatFlowStats_ResetAllVariables_(void);
static void		MovaSatFlowStats_Analysis_(const int16 n16period);
static void		MovaSatFlowStats_Analyse_Struct_(PeriodData * const pPeriodData);
static LOGIC	MovaSatFlowStats_CheckRange_(const PeriodData * const pPeriodData, const REAL RawValue);
static void		MovaSatFlowStats_CalcStats_(PeriodData *pPeriodData);
static void		MovaSatFlowStats_Conf_Level_(PeriodData * pPeriodData);
static void		MovaSatFlowStats_Conf_Interval_(PeriodData * const pPeriodData);
static void		MovaSatFlowStats_IncESVRecordCount_(PeriodData * const pPeriodData);
static void		MovaSatFlowStats_CalcESV_(PeriodData * const pPeriodData);
static void		MovaSatFlowStats_CalcSATINC_(PeriodData * const pPeriodData);
static void		MovaSatFlowStats_15thPercentile_(PeriodData * const pPeriodData);
static void		MovaSatFlowStats_ClearRecord_Period_(const int16 n16Period);
static void		MovaSatFlowStats_SetOutputHeader_(const int16 n16Period);
static void		MovaSatFlowStats_Feedback_Messages_(const int16 period);
static void		MovaSatFlowStats_GetESV_(char * const szBuffer, const PeriodData * const pPeriodData);
static void		MovaSatFlowStats_GetSATINC_(char * const szBuffer, const PeriodData * const pPeriodData);
static void		MovaSatFlowStats_GetLastValue_(char * const szBuffer, const PeriodData * const pPeriodData);
static void		MovaSatFlowStats_Get99Perc_(char * const szBuffer, const PeriodData * const pPeriodData);
static void		MovaSatFlowStats_GetStatsOk_(char * const szBuffer, const PeriodData * const pPeriodData); 
static void		MovaSatFlowStats_GetTotal_(char * const szBuffer, const PeriodData * const pPeriodData);
static void		MovaSatFlowStats_GetMean_(char * const szBuffer, const PeriodData * const pPeriodData);
static void		MovaSatFlowStats_Get15Perc_(char * const szBuffer, const PeriodData * const pPeriodData);
static void		MovaSatFlowStats_GetSumofSquares_(char * const szBuffer, const PeriodData * const pPeriodData);




/*************************************************************************
 *
 * Function: MovaSatFlow_PerformColdStart_
 *
 * Description: Next time module initialises perform cold start
 *
 * Author: panderson
 *
 * Date: 02/05/13
 *
 * Params: void
 *
 * Return: void
 *
 ************************************************************************/

void MovaSatFlowStats_PerformColdStart( void )
{
	SatFlowColdStart = TRUE;
}


/*************************************************************************
 *
 * Function: MovaSatFlowStats_Initialise_
 *
 * Description: First function to be called in this module.
 *				Initialises module and calls functions to initialise dependent modules.
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010	
 *
 * Params: SatData* gSatData - Structure which contains Satflow data
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlowStats_Initialise_(SatData* gSatData)
{
	if (glModuleInitialised)
	{
		KDEBUG_WRITE0( DEBUG_LEVEL_ERROR, SFSTATS, "MovaSatFlow Stats: Already initialised!" );
	}
	KDEBUG_WRITE0( DEBUG_LEVEL_INFO, SFSTATS, "MovaSatFlow Stats: initialising" );

	gSatDataSts = gSatData;

	MovaSatFlowDependantVar_Initialise_();

	/*MovaSatFlowStats_Initialise_ is he first function to be called in this module
	It calls MovaSatFlowStats_ResetAllVariables_ which prepares the module for calculations.
	glModuleInitialised is set to true to indicate initialisation of module.
	Checked by other functions in this module to confirm initialisation*/
	glModuleInitialised = TRUE;

	/* Reset all SatData variables */
	MovaSatFlowStats_ResetAllVariables_();
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_ReInitialise_
 *
 * Description: ReInitialises module and calls functions to re-initialise dependent modules
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010	
 *
 * Params: void
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlowStats_ReInitialise_(void)
{

	KDEBUG_WRITE0( DEBUG_LEVEL_INFO, SFSTATS, "MovaSatFlow Stats: Re-initialising" );
	MovaSatFlowDependantVar_ReInitialise_();

	/* Reset all SatData variables */
	MovaSatFlowStats_ResetAllVariables_();
}



int* MovaSatFlowStats_GetSatFlowFlagPtr_(void)
{
	return(&ShowSatFlw);
}



/*************************************************************************
 *
 * Function:	get_SF_period
 *
 * Description:	Checks the current day and time and determines which SF
 *				period is currently active (if any)
 *
 * Author:		akirkham
 *
 * Date:		11/04/2013
 *
 * Params:		none
 *
 * Return:		The period number (1-6) or 0 if outside of any SF period
 *
 ************************************************************************/
static int get_SF_period(void)
{
	int tnow;
	uint8 periodNo;

	/* Obtain current date and time */
	tTimeDate = Com_read_rtc(READ_RTC);
	tnow = (int)((tTimeDate.hours * 60) + tTimeDate.minutes);

	/* Day Of Week:  S S M T W T F S S */
	/* MOVA Numbers: 6 7 1 2 3 4 5 6 7 */
	if (tTimeDate.day < 6)
	{
		/* weekday */
		for (periodNo = gn16PERIOD_1; periodNo <= gn16PERIOD_4; periodNo++)
		{
			if (tnow >= DI_get_satflow_start_time(periodNo) && tnow < DI_get_satflow_end_time(periodNo))
				return (int)periodNo;
		}
	}
	else if (tTimeDate.day == 6)
	{
		/* Saturday */
		periodNo = gn16PERIOD_5;
		if (tnow >= DI_get_satflow_start_time(periodNo) && tnow < DI_get_satflow_end_time(periodNo))
			return periodNo;
	}
	else if (tTimeDate.day == 7)
	{
		/* Sunday */
		periodNo = gn16PERIOD_6;
		if (tnow >= DI_get_satflow_start_time(periodNo)  && tnow < DI_get_satflow_end_time(periodNo))
			return periodNo;
	}

	return gn16PERIOD_INVALID;
}



/*************************************************************************
 *
 * Function:	MovaSatFlowStats_LoadSFPeriodData_
 *
 * Description:	Feeds back measured SF data into the active dataset and 
 *				recalculates any dependant variables.
 *
 * Author:		akirkham
 *
 * Date:		11/04/2013
 *
 * Params:		int periodNo	the SF period from which data is to be used
 *
 * Return:		none
 *
 ************************************************************************/
void MovaSatFlowStats_LoadSFPeriodData_(int periodNo)
{
	uint8 laneNo;
	uint8 linkNo;
	uint8 i;
	BOOL changed;
	BOOL recalc_chksum = FALSE;

	/* check the periodNo is sensible */
	if (periodNo < gn16PERIOD_1 || periodNo > gn16PERIOD_6)
		return;

	/* BONTIM is link based so update lanes in link order */
	for (linkNo = 1; linkNo <= DI_get_links_count(); linkNo++)
	{
		changed = FALSE;

		for (i = 0; i < DI_get_link_lanes_count(linkNo); i++)
		{
			laneNo = DI_get_lane_num(linkNo,i);

			if (get_measured_SF(periodNo, laneNo))
			{
				/* SATINC / STLOST have changed so update dependent variables */
				MovaSatFlowDependantVar_UpdateDependentLaneVariables_(laneNo);
				changed = TRUE;
			}
		}

		if (changed == TRUE)
		{
			/* update dependent link variables AFTER we have done all the constituent lanes */
			MovaSatFlowDependantVar_UpdateDependentLinkVariables_((uint8)linkNo);

			recalc_chksum = TRUE;
		}
	}

#ifndef M8_ENABLED 
	if (recalc_chksum == TRUE)
		DI_calculate_chksum();
#endif
}


/*************************************************************************
 *
 * Function:	MovaSatFlowStats_NewDatasetLoaded_
 *
 * Description:	Indicate a new dataset has loaded so it may be handled
 *
 * Author:		panderson
 *
 * Date:		20/04/2013
 *
 * Params:		none
 *
 * Return:		none
 *
 ************************************************************************/
void MovaSatFlowStats_NewDatasetLoaded_(void)
{
	DatasetHasChanged = TRUE;
}



/*************************************************************************
 *
 * Function:	MovaSatFlowStats_UpdateSatFlowStats_
 *
 * Description:	Clients call this function to determine if a change of 
 *				period of dataset has occurred
 *
 * Author:		panderson
 *
 * Date:		20/04/2013
 *
 * Params:		none
 *
 * Return:		none
 *
 ************************************************************************/
void MovaSatFlowStats_UpdateSatFlowStats_(void)
{
	if (DatasetHasChanged)
	{
		MovaSatFlowStats_ChangeDataset_();
		DatasetHasChanged = FALSE;
	}
	else
	{ /*Determine if period data has changed */
		MovaSatFlowStats_UpdateSFPeriod_();
	}
}


/*************************************************************************
 *
 * Function: MovaSatFlowStats_GetSatVerbose_Data
 *
 * Description: Get verbose sat data for the period
 *              
 * Author: PAnderson
 *
 * Date: 18/09/2013
 *
 * Params:
			Lane 			The lane in question
			PeriodNo 		The Period in question
 * Return: 
 			PeriodData 		The oversat period data
 *
 ************************************************************************/

PeriodData MovaSatFlowStats_GetSatVerbose_Data( int Lane, int PeriodNo )
{
	static PeriodData SatVerbose;
	SatVerbose = gSatDataSts->Lane_[Lane].LaneSatFlowSTlostPeriodData[PeriodNo-1].Satflow;
	/* Use the rejected values for the three elements below */
	SatVerbose.rSumSqrS = gSatDataSts->Lane_[Lane].LaneSatFlowSTlostPeriodData[PeriodNo-1].SatflowRejected.rSumSqrS;
	SatVerbose.rMeanS = gSatDataSts->Lane_[Lane].LaneSatFlowSTlostPeriodData[PeriodNo-1].SatflowRejected.rMeanS;
	SatVerbose.n16Count = gSatDataSts->Lane_[Lane].LaneSatFlowSTlostPeriodData[PeriodNo-1].SatflowRejected.n16Count;
	return SatVerbose;
}


/*************************************************************************
 *
 * Function: MovaSatFlowStats_GetStLostVerbose_Data
 *
 * Description: Get verbose sat data for the period
 *              
 * Author: PAnderson
 *
 * Date: 18/09/2013
 *
 * Params:
			Lane 			The lane in question
			PeriodNo 		The Period in question
 * Return: 
 			PeriodData 		The stlost period data
 *
 ************************************************************************/
PeriodData MovaSatFlowStats_GetSTLostVerbose_Data( int Lane, int PeriodNo )
{
	static PeriodData STLostVerbose;
	STLostVerbose = gSatDataSts->Lane_[Lane].LaneSatFlowSTlostPeriodData[PeriodNo-1].Stlost;
	/*Use rejected values below */
	STLostVerbose.rSumSqrS = gSatDataSts->Lane_[Lane].LaneSatFlowSTlostPeriodData[PeriodNo-1].StlostRejected.rSumSqrS;
	STLostVerbose.rMeanS = gSatDataSts->Lane_[Lane].LaneSatFlowSTlostPeriodData[PeriodNo-1].StlostRejected.rMeanS;
	STLostVerbose.n16Count = gSatDataSts->Lane_[Lane].LaneSatFlowSTlostPeriodData[PeriodNo-1].StlostRejected.n16Count;
	return STLostVerbose;
}

/*************************************************************************
 *
 * Function:	MovaSatFlowStats_ChangeDataset_
 *
 * Description:	When a dataset change has occurred either 
 *				A) Load up new dataset if in same period.
				B) Perform normal processing if not in period or in new one.
 *
 * Author:		akirkham
 *
 * Date:		15/04/2013
 *
 * Params:		none
 *
 * Return:		none
 *
 ************************************************************************/
static void MovaSatFlowStats_ChangeDataset_(void)
{
	int periodNo;

	periodNo = get_SF_period();

	/* update repository pointer since we know the dataset has changed */
	MovaSatFlowDependantVar_ReInitialise_();

	if (periodNo == gn16SelectInterval && gn16SelectInterval > 0)
	{
		/* Staying within the same period but changed dataset.
		   Apply SF data to new dataset */
		MovaSatFlowStats_LoadSFPeriodData_(periodNo);
		MovaSatFlowStats_Feedback_Messages_((int16)periodNo);
		gn16SelectInterval = (int16)periodNo;
	}
	else
	{
		/* if periodNo != gn16SelectInterval OR they are both zero
		   then we can just call the usual function to do the updating */
		MovaSatFlowStats_UpdateSFPeriod_();
	}
}



/*************************************************************************
 *
 * Function:	get_measured_SF
 *
 * Description:	Gets the measured values for STLOST and SATINC for the 
				specified SF period and lane and updates the value in
				Tcomshr as appropriate.
 *
 * Author:		akirkham
 *
 * Date:		11/04/2013
 *
 * Params:		int periodNo	the SF period from which data is to be used
				int laneNo		the lane number to operate on
 *
 * Return:		TRUE	if any data values have actually been modified
 *				FALSE	otherwise
 *
 ************************************************************************/
static BOOL get_measured_SF(int periodNo, uint8 laneNo)
{
	PeriodData *pSatflow;
	PeriodData *pStlost;
	BOOL data_changed = FALSE;
	int16 measured_val;

	pSatflow = &(gSatDataSts->Lane_[laneNo-1].LaneSatFlowSTlostPeriodData[periodNo-1].Satflow);
	pStlost = &(gSatDataSts->Lane_[laneNo-1].LaneSatFlowSTlostPeriodData[periodNo-1].Stlost);

	KDEBUG_WRITE3( DEBUG_LEVEL_INFO, SFSTATS, "Derived Data stlost Lane = %d criteria = %d usest = %d ", laneNo, pStlost->lRecAvgMetCriteria, DI_is_to_use_calc_startup_lost_time(laneNo));
	KDEBUG_WRITE3( DEBUG_LEVEL_INFO, SFSTATS, "Derived Data satinc Lane = %d criteria = %d usesf = %d ", laneNo, pSatflow->lRecAvgMetCriteria, DI_is_to_use_calc_satinc_time(laneNo));

	if (pSatflow->lRecAvgMetCriteria == TRUE)
	{
		switch(DI_is_to_use_calc_satinc_time(laneNo))
		{
			case gn16FEEDBACK_NONE:
				break;

			case gn1FEEDBACK_PERCENTILE_15:
				if (pSatflow->rSmoothed15 > 0)
				{
					measured_val = (int16)(((1.0 / (REAL)(pSatflow->rSmoothed15)) * grTENTHS_OF_SECONDS_TO_SIX_MINUTES_FACTOR) * 10);
					/* only report a change if the new measured value is different to the existing value so we can skip updating dependant variables */
					if (DI_get_satinc_time(laneNo) != measured_val)
					{
						DI_set_satinc_time(laneNo, measured_val);
						KDEBUG_WRITE4( DEBUG_LEVEL_VERBOSE, SFSTATS, " PERCENTILE_15 Lane:%d, Period:%d, Smoothed15:%f => SATINC=%d",laneNo, periodNo,pSatflow->rSmoothed15 , DI_get_satinc_time(laneNo) );
						data_changed = TRUE;
					}
				}
				break;

			case gn1FEEDBACK_PERCENTILE_50:
				if (pSatflow->rSmoothed50 > 0)
				{
					measured_val = (int16)(((1.0 / (REAL)(pSatflow->rSmoothed50)) * grTENTHS_OF_SECONDS_TO_SIX_MINUTES_FACTOR) * 10);
					if (DI_get_satinc_time(laneNo) != measured_val)
					{
						DI_set_satinc_time(laneNo, measured_val);

						KDEBUG_WRITE4( DEBUG_LEVEL_VERBOSE, SFSTATS, "PERCENTILE_50, Lane:%d, Period:%d, rSmoothed50:%f => SATINC=%d",laneNo, periodNo, pSatflow->rSmoothed50, DI_get_satinc_time(laneNo));
						data_changed = TRUE;
					}
				}
				break;

			default:
				KDEBUG_WRITE1( DEBUG_LEVEL_ERROR, SFSTATS, " Usesf has unknown value = %d ",  DI_is_to_use_calc_satinc_time(laneNo));
				break;
		}
	}

	if (pStlost->lRecAvgMetCriteria == TRUE)
	{
		switch( DI_is_to_use_calc_startup_lost_time(laneNo))
		{
			case gn16FEEDBACK_NONE:
				break;

			case gn1FEEDBACK_PERCENTILE_15:
				measured_val = (int16)((pStlost->rSmoothed15) * 10);
				/* only report a change if the new measured value is different to the existing value so we can skip updating dependant variables */
				if (DI_get_startup_lost_time(laneNo) != measured_val)
				{
					DI_set_startup_lost_time(laneNo, measured_val);
					
					KDEBUG_WRITE4( DEBUG_LEVEL_VERBOSE, SFSTATS, "PERCENTILE_15, Lane:%d, Period:%d, Smoothed15:%f => STLOST=%d",laneNo, periodNo, pStlost->rSmoothed15, DI_get_startup_lost_time(laneNo) );
					data_changed = TRUE;
				}
				break;

			case gn1FEEDBACK_PERCENTILE_50:	
				measured_val = (int16)((pStlost->rSmoothed50) * 10);
				if (DI_get_startup_lost_time(laneNo) != measured_val)
				{
					DI_set_startup_lost_time(laneNo, measured_val);

					KDEBUG_WRITE4( DEBUG_LEVEL_VERBOSE, SFSTATS, "PERCENTILE_50, Lane:%d, Period:%d, rSmoothed50:%f => STLOST=%d",laneNo, periodNo, pStlost->rSmoothed50, DI_get_startup_lost_time(laneNo));
					data_changed = TRUE;
				}
				break;

			default:
				KDEBUG_WRITE1( DEBUG_LEVEL_ERROR, SFSTATS, " Usest has unknown value = %d ", DI_is_to_use_calc_startup_lost_time(laneNo));
				break;
		}
	}

	return data_changed;
}



/*************************************************************************
 *
 * Function:	restore_original_values
 *
 * Description:	Restores the original dataset values for SATINC, STLOST, 
				all dependant variables.
 *
 * Author:		akirkham
 *
 * Date:		15/04/2013
 *
 * Params:		none
 *
 * Return:		void
 *
 ************************************************************************/
static void restore_original_values()
{
	/*const Repository_Type *pRepo;*/
	const Ccomshr *pRepo;

	uint8 i;

	pRepo = DSH_get_active_repository();

	if (pRepo != NULL)
	{

		for(i=0; i<MAX_LINKS; i++)
		{
			DI_set_bonus_green_recalc_time(i+1, pRepo->bontim[i]);
		}

		for(i=0; i<MAX_LANES; i++)
		{
			DI_set_max_min_green_time(i+1, pRepo->maxmin[i]);
			DI_set_moving_q_over_x_det(i+1, pRepo->moveqx[i]);
			DI_set_oversat_critical_veh_count(i+1, pRepo->osatcc[i]);
			DI_set_satflow_mean_gap(i+1, pRepo->satgap[i]);
			DI_set_oversat_time(i+1,  pRepo->osattm[i]);
			DI_set_lane_critical_gap(i+1, pRepo->critg[i]);
			DI_set_time_to_use_comb_x(i+1, pRepo->comtim[i]);
			DI_set_bonus_base_count(i+1, pRepo->bonbc[i]);
			DI_set_satinc_time(i+1, pRepo->satinc[i]);
			DI_set_startup_lost_time(i+1, pRepo->stlost[i]);
		}

#ifndef M8_ENABLED 
		/* Repository struct doesn't contain checksums so we have to recalculate these */
		DI_calculate_chksum();
#endif
	}
	else
	{
		KDEBUG_WRITE0( DEBUG_LEVEL_INFO, SFSTATS, " restore_original_values cannot find active Repo");
	}
}




/*************************************************************************
 *
 * Function: MovaSatFlowStats_ResetAllVariables_
 *
 * Description: On module initialisation, resets all Satflow and Stlost variables in SatData struct.
 *				Most of these are just zeroed while some are set to specified defaults.
 *
 * Author: Pkale
 *
 * Date: 26-Nov-2010
 *
 * Params: void
 *
 * Return: void
 *
 ************************************************************************/

static void MovaSatFlowStats_ResetAllVariables_(void)
{
	PeriodData		*pPeriod;
	int16			laneId;
	int16			periodNo;
	int16			datTypeId;

	if (SatFlowColdStart)
	{ /* Do not initialise stats if performing warm start as they are uploaded from memory */
		/* For each lane */
		for (laneId = 0; laneId < DI_get_lanes_count(); laneId++)
		{
			/*For each period within the lane*/
			for (periodNo = gn16PERIOD_1; periodNo <= gn16SF_STLOST_PERIOD_MAX; periodNo++)
			{
				/*For 4 different structure used to store data*/
				for(datTypeId = gn16SATFLOW; datTypeId <= gn16STLOST_REJECTED; datTypeId++)
				{

					switch(datTypeId)
					{
					case gn16SATFLOW:
						pPeriod = &(gSatDataSts->Lane_[laneId].LaneSatFlowSTlostPeriodData[periodNo-1].Satflow);
						break;
					case gn16STLOST:
						pPeriod = &(gSatDataSts->Lane_[laneId].LaneSatFlowSTlostPeriodData[periodNo-1].Stlost);
						break;
					case gn16SATFLOW_REJECTED:
						pPeriod = &(gSatDataSts->Lane_[laneId].LaneSatFlowSTlostPeriodData[periodNo-1].SatflowRejected);
						break;
					case gn16STLOST_REJECTED:
						pPeriod = &(gSatDataSts->Lane_[laneId].LaneSatFlowSTlostPeriodData[periodNo-1].StlostRejected);
						break;
					default:
						pPeriod = NULL;
					}

					if (pPeriod != NULL)
					{
						/*Reset all Variables*/
						pPeriod->n16ID = datTypeId;
						pPeriod->rSmoothed50 = 0.0;
						pPeriod->rSmoothed15 = 0.0;
						pPeriod->rConfInter = 0.0;
						pPeriod->rConfLevel = 0.0;
						pPeriod->lRecAvgMetCriteria= FALSE;
						pPeriod->n16RecordCount = 0;
						pPeriod->rSatinc = 0.0;
						pPeriod->r15thPercentile = 0;
						pPeriod->rMeanS = 0.0;
						pPeriod->rMeanSDisp = 0.0;
						pPeriod->rStdDevS = 0.0;
						pPeriod->rSumSqrS = 0.0;
						pPeriod->rStdErrS = 0.0;
						pPeriod->n16Count = 0;
						pPeriod->rOldMeanS = 0.0;
						pPeriod->rNewMeanS = 0.0;
						pPeriod->rOldS = 0.0;
						pPeriod->rNewS = 0.0;
					}
					else
					{
						pPeriod = NULL;
					}
				}
			}
		}
		/* We only do cold start once! */
		SatFlowColdStart = FALSE;
	}

	restore_original_values();
	gn16SelectInterval = 0;
	DatasetHasChanged = FALSE;
}




/*************************************************************************
 *
 * Function: MovaSatFlowStats_UpdateSFPeriod_
 *
 * Description: Function to identify if current time falls within user define time periods 
 *				for recording satflow and stlost values. 
 *				Once time equal to start of a period 
 *				1. gn16SelectInterval is set to identify current period. (gn16SelectInterval - is used in StoreEstimates function
 *				to identify period while recording raw values).  
 *				2. Either 15th\50th percentile or default value of satflow or stlost is used
 *				3. Variables dependent on Sf/Stlost are updated via function calls.
 *				Once time equal to end of the period
 *				1. Perform stats on values recorded during the period
 *				2. Reset all variables in configuration data to user defined values
 *				3. Housekeeping - clear variable used while perform stats. Ready for next period
 *				3. Set current period to invalid - stop recording values in StoreEstimates function
 *
 * Author: pkale
 *
 * Date: 26-Nov-2010	
 *
 * Params:	void	
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlowStats_UpdateSFPeriod_(void)
{
	int periodNo;

	periodNo = get_SF_period();

	if (periodNo != gn16SelectInterval)
	{
		/* Something has changed so we need to do something */

		if (gn16SelectInterval > 0)
		{
			/* We were within a SF period (and are now in a different period or none). */
			/* Do anything necessary for the end of a period */

			/* calculate stats on the values recorded this period*/
			MovaSatFlowStats_Analysis_(gn16SelectInterval);

			/*Reset sf and stlost values with those from configuration data*/
			restore_original_values();

			/* Clear raw values stored for this period*/
			MovaSatFlowStats_ClearRecord_Period_(gn16SelectInterval);
		}

		if (periodNo > 0)
		{
			/* Entering a new period */

			/* Update TComshr with measured SATINC, STLOST and update dependant data */
			MovaSatFlowStats_LoadSFPeriodData_(periodNo);

			MovaSatFlowStats_Feedback_Messages_((int16)periodNo);
		}

		/* update the record of the current period */
		gn16SelectInterval = (int16)periodNo;
	}
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_RecordData_
 *
 * Description: Function to calculate Satflow/ Stlost stats for selected time period
 *
 * Author:	Pkale
 *
 * Date:	26-Nov-2010
 *
 * Params: Lane *pLane - Address of lane under consideration
 *		   REAL rCombinedSatFlowRate - final sat flow estimate for this lane
 *         int16 n16STLOSTTime - startup time lost
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlowStats_RecordData_(Lane * const pLane, REAL rRawData, const int16 n16ID)
{	 
	PeriodData	*pRaw;
	PeriodData	*pRejected;
	REAL		rRaw;

	NULL_POINTER_CHECK( SFSTATS, pLane );

	/* If the required interval has been encountered then start recording values if outside required period do not record */
	if (gn16SelectInterval != gn16PERIOD_INVALID)
	{
		switch(n16ID)
		{
		case gn16SATFLOW:
			pRaw = &(pLane->LaneSatFlowSTlostPeriodData[gn16SelectInterval-1].Satflow);
			pRejected = &(pLane->LaneSatFlowSTlostPeriodData[gn16SelectInterval-1].SatflowRejected);

			/*Satflow is stored in rCombinedSatFlowRate as a factor of grTENTHS_OF_SECONDS_TO_HOURS_FACTOR, so to get raw satlfow back multiple by grTENTHS_OF_SECONDS_TO_HOURS_FACTOR */
			rRaw = rRawData * grTENTHS_OF_SECONDS_TO_HOURS_FACTOR;
			break;

		case gn16STLOST:
			pRaw = &(pLane->LaneSatFlowSTlostPeriodData[gn16SelectInterval-1].Stlost);
			pRejected = &(pLane->LaneSatFlowSTlostPeriodData[gn16SelectInterval-1].StlostRejected);
			
			/*Stlost is calculated as ten times of a second. Divide my 10 to get the raw STLOST*/
			rRaw = rRawData/grTEN_SECONDS;
			break;

		default:
			return;
		}

		/*Check if recorded Sat Flow\STLOST falls within accepted range*/
		if (MovaSatFlowStats_CheckRange_(pRaw, rRaw))
		{
			KDEBUG_WRITE4( DEBUG_LEVEL_VERBOSE, SFSTATS, "Lane:%d, Period:%d, DataTypeID:%d => SF=%f", pLane->n16LaneNo + 1, gn16SelectInterval,	pRaw->n16ID, rRaw);
			update_std_dev_data_(rRaw, pRaw);
			update_std_dev_data_(rRaw, pRejected);
		}
	}
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_CheckRange_
 *
 * Description: Function to check if raw value lies within the Max and Min range specified
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010
 *
 * Params:	PeriodData * pPeriodData - Address of the structure under consideration for calculations
 *			REAL RawValue - Raw Satflow/Stlost value obtained for mova
 *
 * Return: LOGIC
 *
 ************************************************************************/
static LOGIC MovaSatFlowStats_CheckRange_(const PeriodData * const pPeriodData, const REAL RawValue)
{
	LOGIC	lIsWithinRange;
	REAL	rOutlineRemovalMin;
	REAL	rOutlineRemovalMax;

	NULL_POINTER_CHECK( SFSTATS, pPeriodData );

	/*Once the required accuracy is reached for a given period, remove outliners and only accept values
	  more than 500 away from the mean for the period. This will be carried out for SATURATION FLOW ONLY*/
	if (pPeriodData->lRecAvgMetCriteria == TRUE && pPeriodData->n16ID == gn16SATFLOW)
	{
		/*Once required accuracy for a period is met, any saturation flow value more than
		  500 away from the mean for the period is rejected.*/
		rOutlineRemovalMin = pPeriodData->rSmoothed50 - gn16SF_MET_CRITERIA_OUTLINE_REMO_RANGE;
		rOutlineRemovalMax = pPeriodData->rSmoothed50 + gn16SF_MET_CRITERIA_OUTLINE_REMO_RANGE;
	}
	else
	{
		if( (pPeriodData->n16ID == gn16SATFLOW) || (pPeriodData->n16ID == gn16SATFLOW_REJECTED) )
		{
			/*If required accuracy for a period is not met then use the default outline remover for saturation flow*/
			rOutlineRemovalMin = grSF_OUTLINERANGE_MIN;
			rOutlineRemovalMax = grSF_OUTLINERANGE_MAX;
		}
		else
		{
			/*Always use default outline remover for stlost*/
			rOutlineRemovalMin = grSTLOST_OUTLINERANGE_MIN;
			rOutlineRemovalMax = grSTLOST_OUTLINERANGE_MAX;
		}
	}

	/*Is raw value within range?*/
	lIsWithinRange = Math_IsWithinRange(RawValue,rOutlineRemovalMin,rOutlineRemovalMax);

	return(lIsWithinRange);

}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_Analysis_
 *
 * Description: Analyse records collected for satflow and stlost values.
 *				This function will first check if current values are statistically significant
 *				If statistically significant function to calculate parameters for satflow logs is called.
 *				Otherwise current + previous values are used to determine if they are statistically significant.
 *				Once statistically significant clear key parameters recorded for rejected values.
 *				
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010	
 *
 * Params:	void
 *
 * Return:	void
 *
 ************************************************************************/
static void MovaSatFlowStats_Analysis_(const int16 periodNo)
{
	PeriodData						*pRaw;
	PeriodData						*pRejected;
	int16							n16Lane;
	int16							n16StructureLoop;

	for (n16Lane = 1; n16Lane <= DI_get_lanes_count(); n16Lane++)
	{
		/*For 4 different structure used to store data*/
		for (n16StructureLoop = gn16SATFLOW; n16StructureLoop <= gn16STLOST; n16StructureLoop++)
		{
			switch (n16StructureLoop)
			{
			case gn16SATFLOW:
				pRaw = &(gSatDataSts->Lane_[n16Lane-1].LaneSatFlowSTlostPeriodData[periodNo-1].Satflow);
				pRejected = &(gSatDataSts->Lane_[n16Lane-1].LaneSatFlowSTlostPeriodData[periodNo-1].SatflowRejected);
				break;

			case gn16STLOST:
				pRaw = &(gSatDataSts->Lane_[n16Lane-1].LaneSatFlowSTlostPeriodData[periodNo-1].Stlost);
				pRejected = &(gSatDataSts->Lane_[n16Lane-1].LaneSatFlowSTlostPeriodData[periodNo-1].StlostRejected);
				break;

			default:
				continue;
			}

			/*Analyse raw satflow values for current period*/
			MovaSatFlowStats_Analyse_Struct_(pRaw);

			/*Stats performed on raw satflow values dose not meet statistical criteria? Time to check if values gathered 
			  in non statistically significant array meet statistical criteria. */
			if (pRaw->lRecAvgMetCriteria == FALSE)
			{
				/*Analyse Satflow values recorded for current period 
					+ any previous values for the same period which did not meet statistical criteria i.e. rejected*/
				MovaSatFlowStats_Analyse_Struct_(pRejected);

				/*If statistical criteria is met update corresponding variables in raw Satflow data structure 
				  to reflect changes*/
				if (pRejected->lRecAvgMetCriteria == TRUE)
				{
					/*Once confidence level within range, SET Flag to indicate recent average met criteria*/
					pRaw->lRecAvgMetCriteria = TRUE;

					/*As statistical criteria is met using previous not statistical values, average calculated using
					  non statistical values is copied into average values for statistical values. Update is required
					  as ESV and SATINC are dependant on Recent Average*/
					
					/*Seems unnecessary step. But this is required because before cal of satinc and smoothed it checks if mean was non zero*/
					pRaw->rMeanS = pRejected->rMeanSDisp;

					/*Copy of rMeanS is maintained in rMeanSDisp as rMeanS is cleared at end of each period */		
					pRaw->rMeanSDisp = pRaw->rMeanS;

					/*Confidence level using satflow did not met statistical criteria. However, confidence level
					  derived from accumulated non statistical values met statistical criteria and hence copy confidence level results 
					  to confidence level for raw satflow data structure. (Updates values will be displayed in output )*/

					pRaw->rConfLevel= pRejected->rConfLevel;

					/*Similar to Confidence Level description as above*/
					pRaw->r15thPercentile = pRejected->r15thPercentile;

					/*ESV record count keep record of number of time ESV was calculated*/
					MovaSatFlowStats_IncESVRecordCount_(pRaw);
					
					/*Calculate Exponentially Smoothed Value*/
					MovaSatFlowStats_CalcESV_(pRaw);

					/*SATINC calculated only for satflow*/
					if ( (pRaw->n16ID == gn16SATFLOW) || (pRaw->n16ID == gn16SATFLOW_REJECTED) )
					{
						MovaSatFlowStats_CalcSATINC_(pRaw);
					}

					KDEBUG_WRITE10( DEBUG_LEVEL_VERBOSE, SFSTATS, "CALC_OK, DataTypeID:%d, Lane:%d, ValuesCount:%d => Mean=%f, StdDev=%f, StdError=%f, 15thPer=%f, ESV_50=%f, SumSqr=%f, SATINC=%f", pRaw->n16ID, n16Lane, pRaw->n16Count, pRaw->rMeanS, pRaw->rStdDevS, pRaw->rStdErrS, pRaw->rSmoothed15, pRaw->rSmoothed50, pRaw->rSumSqrS, pRaw->rSatinc);

					/*n16Count serves as a flag as well. If n16Count = 0 no analysis are performed in 
					MovaSatFlowStats_Analyse_Struct_ and MovaSatFlowStats_Analyse_Struct_Rejected_*/
					pRaw->n16Count = 0;

				}

				/* pSatflow->rConfLevel is used to output confidence value in the log. At this stage both 
					raw satflow and rejected satflow did not meet statistical criteria. Updated the confidence level of raw satflow
					with rejected satflow*/
				pRaw->rConfLevel= pRejected->rConfLevel;

			}
			else
			{
				/*As statistical value met criteria, previous records for the period are no longer required 
				(pRejected->n16Count - maintains count of all values recorded during previous + current period) */
				pRejected->n16Count = 0;
				pRejected->rMeanS = 0.0;
				pRejected->rSumSqrS = 0.0;
				pRejected->rStdErrS = 0.0;
				pRejected->rStdDevS = 0.0;
				pRejected->rOldMeanS = 0.0;
				pRejected->rNewMeanS = 0.0;
				pRejected->rOldS = 0.0;
				pRejected->rNewS = 0.0;
			}
		}
	}
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_Analyse_Struct_
 *
 * Description: Function determines confidence level for recorded values.
 *				If confidence level falls within specified Max/Min range calls relevant function to 
 *				update/calculate parameters which are displayed in output logs
 *				
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010
 *
 * Params:	PeriodData * pPeriodData - Address of the structure under consideration for calculations
 *
 * Return:	void
 *
 ************************************************************************/
static void MovaSatFlowStats_Analyse_Struct_(PeriodData * const pPeriodData)
{
	LOGIC lIsWithinRange = FALSE;

	NULL_POINTER_CHECK( SFSTATS, pPeriodData );

	/*If only one value recorded during interval, confidence level for one value is infinity.
	  As a result statistical criteria for the period is not met.
	  Hence perform stats only if more than one value recorded during the interval*/
	if (pPeriodData->n16Count > 1)
	{
		MovaSatFlowStats_CalcStats_(pPeriodData );

		/*Calculate confidence level first to determine if further analysis needed for this structure*/
		MovaSatFlowStats_Conf_Level_( pPeriodData );

		/*Acceptable Max and Min confidence level for satflow\satflow rejected different to stlost\stlost rejected */
		if( (pPeriodData->n16ID == gn16SATFLOW) || (pPeriodData->n16ID == gn16SATFLOW_REJECTED) )
		{
			lIsWithinRange = Math_IsWithinRange(pPeriodData->rConfLevel, grSF_CONFIDENCE_MIN, grSF_CONFIDENCE_MAX);
		}
		else if ((pPeriodData->n16ID == gn16STLOST) || (pPeriodData->n16ID == gn16STLOST_REJECTED))
		{
			lIsWithinRange = Math_IsWithinRange(pPeriodData->rConfLevel, grSTLOST_CONFIDENCE_MIN, grSTLOST_CONFIDENCE_MAX);
		}

		/* Confidence level within range*/
		if (lIsWithinRange)
		{
			/*Note: Do not change the order of the statements below. Each line/function call prepares information for the next function call. */
			/*Once confidence level within range SET Flag to indicate recent average met criteria*/
			pPeriodData->lRecAvgMetCriteria = TRUE;
			/*Increment count indicating number of exponentially values recorded*/
			
			/*Calculate 15th percentile which would be used to feed back to mova*/
			MovaSatFlowStats_15thPercentile_(pPeriodData);

			MovaSatFlowStats_IncESVRecordCount_(pPeriodData);

			/*Calculate Exponentially Smoothed Value*/
			MovaSatFlowStats_CalcESV_(pPeriodData);

			/*SATINC calculated only for satflow and rejected satflow*/
			if ((pPeriodData->n16ID == gn16SATFLOW) || (pPeriodData->n16ID == gn16SATFLOW_REJECTED))
			{
				MovaSatFlowStats_CalcSATINC_(pPeriodData);
			}

			/* KDEBUG_WRITE10(DEBUG_LEVEL_VERBOSE, SFSTATS, "CALC_OK, DataTypeID:%d, Lane:%d, ValuesCount:%d => Mean=%f, StdDev=%f, StdError=%f, 15thPer=%f, ESV_50=%f, SumSqr=%f, SATINC=%f",pPeriodData->n16ID,  n16laneNo, pPeriodData->n16Count, pPeriodData->rMeanS, pPeriodData->rStdDevS, pPeriodData->rStdErrS, pPeriodData->rSmoothed15, pPeriodData->rSmoothed50, pPeriodData->rSumSqrS, pPeriodData->rSatinc);*/

			pPeriodData->n16Count = 0;
			pPeriodData->rMeanS = 0.0;
			pPeriodData->rStdDevS = 0.0;
			pPeriodData->rStdErrS = 0.0;
			pPeriodData->rSumSqrS = 0.0;
			pPeriodData->rOldMeanS = 0.0;
			pPeriodData->rNewMeanS = 0.0;
			pPeriodData->rOldS = 0.0;
			pPeriodData->rNewS = 0.0;

		}
		else
		{
			/*Here confidence level outside range, CLEAR Flag to indicate recent average did not meet criteria*/
			pPeriodData->lRecAvgMetCriteria = FALSE;

			/*Do not reset count for rejected satflow\stlost because recorded existing values were not statistically significant,
			new values should be added to previous records.*/
			if ((pPeriodData->n16ID == gn16SATFLOW) || (pPeriodData->n16ID == gn16STLOST))
			{
				/*STATS_WRITE10("SFSTATS: OUT_RNG, DataTypeID:%d, Lane:%d, ValuesCount:%d => Mean=%f, StdDev=%f, StdError=%f, 15thPer=%f, ESV_50=%f, SumSqr=%f, SATINC=%f", pPeriodData->n16ID, n16laneNo, pPeriodData->n16Count, pPeriodData->rMeanS, pPeriodData->rStdDevS, pPeriodData->rStdErrS, pPeriodData->rSmoothed15, pPeriodData->rSmoothed50, pPeriodData->rSumSqrS, pPeriodData->rSatinc);*/
				pPeriodData->n16Count = 0;
			}
		}
	}
	else
	{
		/*Here confidence level outside range, CLEAR Flag to indicate recent average did not meet criteria*/
		pPeriodData->lRecAvgMetCriteria = FALSE;
		/*Update value for confidence interval*/
		pPeriodData->rConfInter = arrConfInter[0];
		pPeriodData->rConfLevel = 0.0;

		/*Do not reset count for rejected satflow\stlost because recorded existing values were not statistically significant,
		new values should be added to previous records.*/
		if ((pPeriodData->n16ID == gn16SATFLOW) || (pPeriodData->n16ID == gn16STLOST))
		{
			pPeriodData->n16Count = 0;
		}
	}
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_CalcStats_
 *
 * Description: Function to calculate stats on the raw\rejected satflow\stlost values collected during
 *				recent period
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010
 *
 * Params:	PeriodData * pPeriodData - Address of the structure under consideration for calculations
 *
 * Return:	void
 *
 ************************************************************************/
static void MovaSatFlowStats_CalcStats_(PeriodData *pPeriodData)
{
	REAL variance;

	NULL_POINTER_CHECK( SFSTATS, pPeriodData);
	
	pPeriodData->rMeanS = (pPeriodData->n16Count > 0) ? pPeriodData->rNewMeanS : 0.0F;

	/*Variables prefixed with Disp are the ones which would be used while displaying the logs.
	Here rMeanS is copied to rMeanSDisp to maintain a copy for display. This is because 
	rMeanS is reset at end of each period*/
	pPeriodData->rMeanSDisp = pPeriodData->rMeanS;

	/*Standard Deviation for just one value is undefined. Also prevents divide by zero*/
	if (pPeriodData->n16Count > 1)
	{
		/*To calculate standard deviation*/
		variance = pPeriodData->rNewS / (pPeriodData->n16Count - 1);
		
		if (variance > 0)
		{
			pPeriodData->rStdDevS = (REAL) sqrt(variance);
		}
		else
		{
			pPeriodData->rStdDevS = 0.0F;
		}

		/* Formula to calculate standard error. */
		pPeriodData->rStdErrS = (REAL)( pPeriodData->rStdDevS / sqrt((REAL)pPeriodData->n16Count));
	}
	else
	{
		/* Standard deviation and standard error are zero. */
		pPeriodData->rStdDevS = 0.0F;
		pPeriodData->rStdErrS = 0.0F;
	}
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_Conf_Level_
 *
 * Description: Function to calculate confidence level
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010
 *
 * Params:	PeriodData * pPeriodData - Address of the structure under consideration for calculations
 *
 * Return:	void
 *
 ************************************************************************/
static void MovaSatFlowStats_Conf_Level_(PeriodData *pPeriodData)
{
	NULL_POINTER_CHECK( SFSTATS, pPeriodData );

	/*Calculate Confidence Interval*/
	MovaSatFlowStats_Conf_Interval_(pPeriodData);

	/*Update Confidence level*/
	pPeriodData->rConfLevel = (REAL)( (pPeriodData->rConfInter) * (pPeriodData->rStdErrS) );

}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_Conf_Interval_
 *
 * Description: Function to Update Confidence interval based on Number of values recorded.
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010
 *
 * Params:	PeriodData * pPeriodData - Address of the structure under consideration for calculations
 *
 * Return:	void
 *
 ************************************************************************/
static void MovaSatFlowStats_Conf_Interval_(PeriodData * const pPeriodData)
{
	NULL_POINTER_CHECK( SFSTATS, pPeriodData );
	
	if(pPeriodData->n16Count > gn16SF_CONFIDENCE_INTERVAL_MAX)
	{
		/*If number of values recoded is greater than Max number of confidence intervals.
		  Set the confidence interval to max confidence interval
		  (NB: After max confidence interval there are minute change in confidence interval for 
		   Number of values recorded. Hence it was decided to set confidence interval to max if 
		   there number of values recorded (EstNum) is greater than max)
		  */
		pPeriodData->rConfInter = arrConfInter[gn16SF_CONFIDENCE_INTERVAL_MAX - 1];
	}
	else
	{
		/*Set confidence interval corresponding to the values recorded(EstNum)*/
		pPeriodData->rConfInter = arrConfInter[pPeriodData->n16Count - 1];
	}
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_IncESVRecordCount_
 *
 * Description: Function to increment Exponentially Smoothed value Record Count.
 *				This count indicates the number of times exponentially smoothed values are encountered.
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010	
 *
 * Params:	PeriodData * pPeriodData - Address of the structure under consideration for calculations
 *
 * Return:	void
 *
 ************************************************************************/
static void MovaSatFlowStats_IncESVRecordCount_(PeriodData * const pPeriodData)
{
	NULL_POINTER_CHECK(SFSTATS, pPeriodData);

	/* Recent average saturation flow equals zero indicates that currently recorded SF values 
		do not meet statistical criteria and hence increment ESV record count only if the recent average is non zero*/
	if (pPeriodData->rMeanS != 0.0)
	{
		pPeriodData->n16RecordCount++;
	}
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_CalcESV_
 *
 * Description: Function to calculate exponentially Smoothed value
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010
 *
 * Params:	PeriodData * pPeriodData - Address of the structure under consideration for calculations
 *
 * Return:	void
 *
 ************************************************************************/
static void MovaSatFlowStats_CalcESV_(PeriodData * const pPeriodData)
{
	NULL_POINTER_CHECK( SFSTATS, pPeriodData );

	if (pPeriodData->n16RecordCount == 1)
	{
		/* For first scan when total number of sf values recorded is 1 then updated smoothed value with recent average)
		 N.B this is the first value for SF hence ESV value updated with this value*/
		pPeriodData->rSmoothed50 = pPeriodData->rMeanS;

		pPeriodData->rSmoothed15 = pPeriodData->r15thPercentile;
	}
	else
	{
		/*Smoothed value = (0.75* Previous Smoothed value + (0.25)* Recent Average)*/
		pPeriodData->rSmoothed50 = (REAL)( ((REAL)(grExpSmoothCal_75Perc_Factor * pPeriodData->rSmoothed50)) 
										+ ((REAL)(grExpSmoothCal_25Perc_Factor * pPeriodData->rMeanS)) ); 

		/*Smoothed value = (0.75* Previous Smoothed value + (0.25)* 15th%-ile)*/
		pPeriodData->rSmoothed15 = (REAL)( ((REAL)(grExpSmoothCal_75Perc_Factor * pPeriodData->rSmoothed15)) 
										+ ((REAL)(grExpSmoothCal_25Perc_Factor * pPeriodData->r15thPercentile)) );
	}
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_CalcSATINC_
 *
 * Description: Function to calculate SATINC
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010
 *
 * Params:	PeriodData * pPeriodData - Address of the structure under consideration for calculations
 *
 * Return:	void
 *
 ************************************************************************/
static void MovaSatFlowStats_CalcSATINC_(PeriodData * const pPeriodData)
{
	NULL_POINTER_CHECK( SFSTATS, pPeriodData );

	/*rSmoothed50 is calculated when recent values collected for the period have met statistical criteria.
	However, if it has not met the criteria then rSmoothed50 would be zero. 
	If rSmoothed50 is zero do not calculate Satinc*/
	if (pPeriodData->rSmoothed50 != 0.0)
	{
		/*Info added for divide error*/
		DivideError((int)pPeriodData->rSmoothed50, 816, "smoothed value was %d", pPeriodData->rSmoothed50);

		/*Formula to calculate satinc*/
		pPeriodData->rSatinc = ((1 / (pPeriodData->rSmoothed50 )) * grTENTHS_OF_SECONDS_TO_SIX_MINUTES_FACTOR);
	}
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_15thPercentile_
 *
 * Description: Function to calculate 15th Percentile from mean and standard deviation
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010
 *
 * Params:	PeriodData * pPeriodData - Address of the structure under consideration for calculations
 *
 * Return:	void
 *
 ************************************************************************/
static void MovaSatFlowStats_15thPercentile_(PeriodData * const pPeriodData)
{
	NULL_POINTER_CHECK( SFSTATS, pPeriodData );

	/* Calculate 15th Percentile of SF Value*/   
	pPeriodData->r15thPercentile = pPeriodData->rMeanS -(gr15thPercentile_Factor * (pPeriodData->rStdDevS) ) ;
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_ClearRecord_Period_
 *
 * Description: Function to clear all raw sat flow values stored 
 *				during latest period.
 *
 * Author: pkale
 *
 * Date: 26-Nov-2010
 *
 * Params:	int16 period - Time slot within the day
 *
 * Return:	void
 *
 ************************************************************************/
static void MovaSatFlowStats_ClearRecord_Period_(const int16 periodNo)
{
	PeriodData	*pRaw;
	PeriodData	*pRejected;
	int16		n16Lane;
	int16		n16StructureLoop;

	for (n16Lane = 0; n16Lane < DI_get_lanes_count(); n16Lane++)
	{
		for (n16StructureLoop = gn16SATFLOW; n16StructureLoop <= gn16STLOST; n16StructureLoop++)
		{
			switch(n16StructureLoop)
			{
			case gn16SATFLOW:
				pRaw = &(gSatDataSts->Lane_[ n16Lane].LaneSatFlowSTlostPeriodData[periodNo-1].Satflow);
				pRejected = &(gSatDataSts->Lane_[ n16Lane].LaneSatFlowSTlostPeriodData[periodNo-1].SatflowRejected);
				break;

			case gn16STLOST:
				pRaw = &(gSatDataSts->Lane_[ n16Lane].LaneSatFlowSTlostPeriodData[periodNo-1].Stlost);
				pRejected = &(gSatDataSts->Lane_[ n16Lane].LaneSatFlowSTlostPeriodData[periodNo-1].StlostRejected);
				break;

			default:
				continue;
			}

			KDEBUG_WRITE10(DEBUG_LEVEL_VERBOSE, SFSTATS, "CLR_REC, DataTypeID:%d, Lane:%d, ValuesCount:%d => Mean=%f, StdDev=%f, StdError=%f, 15thPer=%f, ESV_50=%f, SumSqr=%f, SATINC=%f",n16StructureLoop, n16Lane+1, pRaw->n16Count, pRaw->rMeanS, pRaw->rStdDevS, pRaw->rStdErrS, pRaw->rSmoothed15, pRaw->rSmoothed50, pRaw->rSumSqrS, pRaw->rSatinc);

			/*Reset variables which accumulate information during the period. 
			E.g -rTotalS maintains sum total of raw values during a period. 
			At the end of period clear these variables */
			pRaw->n16Count = 0;
			pRaw->rConfInter = 0.0;
			pRaw->rMeanS = 0.0;
			pRaw->rSumSqrS = 0.0;
			pRaw->rStdErrS = 0.0;
			pRaw->rStdDevS = 0.0;
			pRaw->rOldMeanS = 0.0;
			pRaw->rNewMeanS = 0.0;
			pRaw->rOldS = 0.0;
			pRaw->rNewS = 0.0;
			/*For rejected values, only flag to indicate stats met statistical criteria is reset,
			  rest of variables are reset after performing stats*/
			pRejected->lRecAvgMetCriteria = FALSE;
		}
	}
}


/*************************************************************************
 *
 * Function: MovaSatFlowStats_Output_SFVerbose_
 *
 * Description: Function to Output results of Stats, performed on real-time SF values.
 *				This output will display result in Coma Separated Values
 *				with detail analysis thus giving extra information which would be useful to user.			
 *
 * Author: pkale
 *
 * Date: 26-Nov-2010	
 *
 * Params:	void
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlowStats_Output_SFVerbose_(void)
{
	PeriodData				*pSatflow;
	PeriodData				*pSatflowRejected;
	int16					n16Laneloop;
	int16					n16Period;
	static char sfEsv		[ 16 ];
	static char sfStatin	[ 16 ];
	static char sfLastValue	[ 16 ];
	static char sf99		[ 16 ];
	static char sfStatsok	[ 16 ];
	static char sfTotal		[ 16 ];
	static char sfMean		[ 16 ];
	static char sf15Perc	[ 16 ];
	static char sfSumSqr	[ 16 ];

	/* Determine where to display result (terminal/screen)*/
#ifdef IS_MOVACOMM_ENABLED
	int nOutputDest = GetOutputDestination();
#else
	int nOutputDest = 0;
#endif

/**********************************************Lane XX display SATFLOW****************************************/
	for (n16Laneloop = 0; n16Laneloop < DI_get_lanes_count(); n16Laneloop++)
	{
		/* Header for SatFlow values*/
		Send_FormatString( nOutputDest, 
		"\r\nLane%02d,,SF ESV,SATINC,Last Value,99 Per,Stats ok?,Total,Mean SF,15th SF,Sum Sq,\r\n",(n16Laneloop)+1 );
		Send_FormatString( nOutputDest, 
		"      ,,      ,      ,measured  ,conf int,so far  ,so far,so far,              \r\n" );

		for (n16Period = gn16PERIOD_1; n16Period <= gn16PERIOD_6 ; n16Period ++)
		{
			pSatflow = &(gSatDataSts->Lane_[ n16Laneloop ].LaneSatFlowSTlostPeriodData[n16Period-1].Satflow);
			pSatflowRejected = &(gSatDataSts->Lane_[ n16Laneloop ].LaneSatFlowSTlostPeriodData[n16Period-1].SatflowRejected);

			/* Get stat results required to display output */
			MovaSatFlowStats_GetESV_(sfEsv, pSatflow);
			MovaSatFlowStats_GetSATINC_(sfStatin, pSatflow);
			MovaSatFlowStats_GetLastValue_(sfLastValue, pSatflow);
			MovaSatFlowStats_Get99Perc_(sf99, pSatflow);
			MovaSatFlowStats_GetStatsOk_(sfStatsok, pSatflow);
			MovaSatFlowStats_GetTotal_(sfTotal, pSatflowRejected);
			MovaSatFlowStats_GetMean_(sfMean, pSatflowRejected);
			MovaSatFlowStats_Get15Perc_(sf15Perc, pSatflow);
			MovaSatFlowStats_GetSumofSquares_(sfSumSqr, pSatflowRejected);

			MovaSatFlowStats_SetOutputHeader_(n16Period);

			/* Display results for selected time interval*/
			Send_FormatString( nOutputDest, 
			"%s,%s,%s,%s,%s,%s,%s,%s,%s,\r\n",sfEsv,sfStatin,sfLastValue,sf99,sfStatsok,sfTotal,sfMean,sf15Perc,sfSumSqr );
		}

		Send_FormatString( nOutputDest,"\r\n\r\n" );
	}
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_Output_STVerbose_
 *
 * Description: Function to Output results of Stats, performed on real-time STLOST values.
 *				This output will display result in Coma Separated Values
 *				with detail analysis thus giving extra information which would be useful to user.
 *
 * Author: pkale
 *
 * Date: 26-Nov-2010
 *
 * Params:	void
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlowStats_Output_STVerbose_(void)
{
	PeriodData	*pStlost;
	PeriodData	*pStlostRejected;
	int16		n16Laneloop;
	int16		n16Period;

	static char stlostESV			[ 16 ];
	static char stlostLastValue		[ 16 ];
	static char stlost99			[ 16 ];
	static char stlostStatsok		[ 16 ];
	static char stlostTotal			[ 16 ];
	static char stlostMean			[ 16 ];
	static char stlostSumSqr		[ 16 ];

	/* Determine where to display result (terminal/screen)*/
#ifdef IS_MOVACOMM_ENABLED
	int nOutputDest = GetOutputDestination();
#else
	int nOutputDest = 0;
#endif

	for (n16Laneloop = 0; n16Laneloop < DI_get_lanes_count(); n16Laneloop++)
	{
/**********************************************Lane XX display STLOST****************************************/
		/* Header for Stlost values*/
		Send_FormatString( nOutputDest, 
		"\r\nLane%02d,,STLOST,Last Value,99 Perc,Stats ok?,Total,Mean STLOST,Sum Sq           \r\n",(n16Laneloop)+1 );
		Send_FormatString( nOutputDest, 
		"      ,,ESV   ,measured  ,conf int,so far  ,so far,so far,                     \r\n" );

		for (n16Period = gn16PERIOD_1; n16Period <= gn16PERIOD_6 ; n16Period++)
		{
			pStlost = &(gSatDataSts->Lane_[ n16Laneloop ].LaneSatFlowSTlostPeriodData[n16Period-1].Stlost);
			pStlostRejected = &(gSatDataSts->Lane_[ n16Laneloop ].LaneSatFlowSTlostPeriodData[n16Period-1].StlostRejected);

			/* Get stat results required to display output */
			MovaSatFlowStats_GetESV_(stlostESV, pStlost);
			MovaSatFlowStats_GetLastValue_(stlostLastValue, pStlost);
			MovaSatFlowStats_Get99Perc_(stlost99, pStlost);
			MovaSatFlowStats_GetStatsOk_(stlostStatsok, pStlost);
			MovaSatFlowStats_GetTotal_(stlostTotal, pStlostRejected);
			MovaSatFlowStats_GetMean_(stlostMean, pStlostRejected);
			MovaSatFlowStats_GetSumofSquares_(stlostSumSqr, pStlostRejected);
	
			MovaSatFlowStats_SetOutputHeader_(n16Period);
			
			/* Display results for selected time interval*/
			Send_FormatString( nOutputDest, 
			"%s,%s,%s,%s,%s,%s,%s,\r\n",stlostESV,stlostLastValue,stlost99,stlostStatsok,stlostTotal,stlostMean,stlostSumSqr );
		}
	}

	Send_FormatString( nOutputDest, "\r\n" );
	Send_FormatString( nOutputDest, "\r\n" );
}

/*************************************************************************
 *
 * Function: MovaSatFlowStats_GetSatPeriodTimes
 *
 * Description: Get Sat Period Times
 *              
 * Author: PAnderson
 *
 * Date: 18/09/2013
 *
 * Params:
			PeriodNo 		The Period in question
			PeriodStart 	The start time of the period
			PeriodEnd 		The end time of the period
 * Return: void
 *
 ************************************************************************/
void MovaSatFlowStats_GetSatPeriodTimes( int PeriodNo, int* PeriodStart, int* PeriodEnd )
{
	*PeriodStart =  Tcomshr->sftims[PeriodNo-1][SFTIMS_PERIOD_START];
	*PeriodEnd = Tcomshr->sftims[PeriodNo-1][SFTIMS_PERIOD_END];
}


/*************************************************************************
 *
 * Function: MovaSatFlowStats_Output_ScreenDisplay_
 *
 * Description: Function to output Exponentially Smoothed STLOST values for each lane
 *
 * Author: pkale
 *
 * Date: 26-Nov-2010	
 *
 * Params:	n16DataType - Indicates if satflow or stlost to be displayed
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlowStats_Output_ScreenDisplay_(const int16 n16DataType)
{
	char		buf[128];
	char		tmp[8];
	PeriodData	*pSatflow;
	PeriodData	*pStlost;
	int			tens;
	int			laneNo;
	int			pd;

	/* Determine where to display result (terminal/screen)*/
#ifdef IS_MOVACOMM_ENABLED
	int nOutputDest = GetOutputDestination();
#else
	int nOutputDest = 0;
#endif

	/* Clear screen */
	Send_FormatString( nOutputDest, sClrScr);

	/* Output in blocks of ten lanes */
	for (tens = 0; tens < 3; tens++)  /* max 30 lanes */
	{
		if (n16DataType == gn16SATFLOW)
		{
			sprintf(buf, "\r\n           Lane ");
			for (laneNo = 1; laneNo <= 10; laneNo++)
			{
				sprintf(tmp, "%6d", (10 * tens) + laneNo);
				strcat(buf, tmp);
			}
		}
		else
		{
			sprintf(buf, "\r\n           Lane ");
			for (laneNo = 1; laneNo <= 10; laneNo++)
			{
				sprintf(tmp, "%5d", (10 * tens) + laneNo);
				strcat(buf, tmp);
			}
		}
		Send_FormatString(nOutputDest, buf);


		for (pd = gn16PERIOD_1; pd <= gn16PERIOD_6; pd++)
		{
			/*Displays appropriate headings on the screen */
			MovaSatFlowStats_SetOutputHeader_((int16)pd);

			/* For first 10 lanes*/
			for (laneNo = 1; laneNo <= 10; laneNo++)
			{
				/* Get access to the data*/
				if(n16DataType == gn16SATFLOW)
				{
					pSatflow = &(gSatDataSts->Lane_[(10 * tens) + laneNo-1].LaneSatFlowSTlostPeriodData[pd-1].Satflow);

					/* If value is zero output result in the format required*/
					if (pSatflow->rSmoothed50 == 0)
					{
						Send_FormatString( nOutputDest, " %s ", "0000" );
					}
					else
					{
						/* Output ESV SF value*/
						Send_FormatString( nOutputDest, " %4d ", (int)(pSatflow->rSmoothed50));
					}
				}
				else if (n16DataType == gn16STLOST)
				{
					pStlost = &(gSatDataSts->Lane_[(10 * tens) + laneNo-1].LaneSatFlowSTlostPeriodData[pd-1].Stlost);
					Send_FormatString(nOutputDest, "%4.1f ", (float)pStlost->rSmoothed50);
				}
			}
		}

		/* more lanes? */
		if (DI_get_lanes_count() <= ((tens + 1) * 10))
			break;
		else
			Send_FormatString(nOutputDest, "\r\n\r\n");
	}
}

/*************************************************************************
 *
 * Function: MovaSatFlowStats_GetSatESV_Data
 *
 * Description: Get the exponentially smoothed value for the satflow data over the period
 *              
 * Author: PAnderson
 *
 * Date: 18/09/2013
 *
 * Params:
			Lane 		The lane in question
			PeriodNo 	The period in question
 * Return: void
 			The exponentially smoothed value for the satflow data over the period
 *
 ************************************************************************/
float MovaSatFlowStats_GetSatESV_Data( int Lane, int PeriodNo )
{
	return (gSatDataSts->Lane_[Lane].LaneSatFlowSTlostPeriodData[PeriodNo-1].Satflow.rSmoothed50);
}

/*************************************************************************
 *
 * Function: MovaSatFlowStats_GetSTLostESV_Data
 *
 * Description: Get the exponentially smoothed value for the stlost data over the period
 *              
 * Author: PAnderson
 *
 * Date: 18/09/2013
 *
 * Params:
			Lane 		The lane in question
			PeriodNo 	The period in question
 * Return: void
 			The exponentially smoothed value for the stlost data over the period
 *
 ************************************************************************/
float MovaSatFlowStats_GetSTLostESV_Data( int Lane, int PeriodNo )
{
	return (gSatDataSts->Lane_[Lane].LaneSatFlowSTlostPeriodData[PeriodNo-1].Stlost.rSmoothed50);
}


/*************************************************************************
 *
 * Function: MovaSatFlowStats_GetESV_
 *
 * Description: Function to get Exponentially Smoothed Value
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010
 *
 * Params:	char * szBuffer					- Buffer used to store result and return as a character
 *			const PeriodData *pPeriodData	- Address of the structure under consideration for calculations
 *
 * Return:	None
 *
 ************************************************************************/
static void MovaSatFlowStats_GetESV_(char * const szBuffer, const PeriodData * const pPeriodData)
{
	NULL_POINTER_CHECK( SFSTATS, pPeriodData );
	NULL_POINTER_CHECK( SFSTATS, szBuffer );

	if (pPeriodData->rSmoothed50 == gn16ZERO )
	{
		if(pPeriodData->n16ID == gn16SATFLOW)
		{
			sprintf( szBuffer, gszOUTPUT_VALUE_FOUR_ZERO );
		}
		else
		{
			sprintf( szBuffer, gszOUTPUT_VALUE_THREE_ZERO );
		}
	}
	else
	{
		if(pPeriodData->n16ID == gn16SATFLOW)
		{
			sprintf( szBuffer, "%d", (int) pPeriodData->rSmoothed50);
		}
		else
		{
			sprintf( szBuffer, "%3.1f", pPeriodData->rSmoothed50);
		}
	}
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_GetSATINC_
 *
 * Description: Function to get SATINC
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010
 *
 * Params:	char * szBuffer					- Buffer used to store result and return as a character
 *			const PeriodData *pPeriodData	- Address of the structure under consideration for calculations
 * Return:	None
 *
 ************************************************************************/
static void MovaSatFlowStats_GetSATINC_(char * const szBuffer, const PeriodData * const pPeriodData)
{
	NULL_POINTER_CHECK( SFSTATS, pPeriodData );
	NULL_POINTER_CHECK( SFSTATS, szBuffer );

	if ( pPeriodData->rSatinc == gn16ZERO )
	{
		sprintf( szBuffer, gszOTUPTU_VALUE_ZERO_POINT_ZERO );
	}
	else
	{
		sprintf(szBuffer, "%.1f", pPeriodData->rSatinc);
	}
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_GetLastValue_
 *
 * Description: Function to get mean of Last values measured. 
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010
 *
 * Params:	char * szBuffer					- Buffer used to store result and return as a character
 *			const PeriodData *pPeriodData	- Address of the structure under consideration for calculations
 *
 * Return:	None
 *
 ************************************************************************/
static void MovaSatFlowStats_GetLastValue_(char * const szBuffer, const PeriodData * const pPeriodData)
{
	NULL_POINTER_CHECK( SFSTATS, pPeriodData );
	NULL_POINTER_CHECK( SFSTATS, szBuffer );

	if ( pPeriodData->rMeanSDisp ==  gn16ZERO )
	{
		sprintf( szBuffer, gszOUTPUT_VALUE_FOUR_ZERO );
	}
	else
	{
		if(pPeriodData->n16ID == gn16SATFLOW)
		{
			sprintf( szBuffer, "%d", (int) pPeriodData->rMeanSDisp );
		}
		else
		{
			sprintf( szBuffer, "%3.1f", pPeriodData->rMeanSDisp );
		}
	}
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_Get99Perc_
 *
 * Description: Function to get 99 percentage confidence interval
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010
 *
 * Params:	char * szBuffer					- Buffer used to store result and return as a character
 *			const PeriodData *pPeriodData	- Address of the structure under consideration for calculations
 *
 * Return:	None
 *
 ************************************************************************/
static void MovaSatFlowStats_Get99Perc_( char * const szBuffer, const PeriodData * const pPeriodData)
{
	NULL_POINTER_CHECK( SFSTATS, pPeriodData );
	NULL_POINTER_CHECK( SFSTATS, szBuffer );

	if ( pPeriodData->rConfLevel == gn16ZERO )
	{
		sprintf( szBuffer, gszOUTPUT_VALUE_THREE_ZERO );
	}
	else
	{
		if(pPeriodData->n16ID == gn16SATFLOW)
		{
			sprintf( szBuffer, "%3d", (int)pPeriodData->rConfLevel );
		}
		else
		{
			sprintf( szBuffer, "%3.2f", pPeriodData->rConfLevel );
		}
	}
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_GetStatsOk_
 *
 * Description: Function to get if Satflow/ STLOST met statistical criteria
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010
 *
 * Params:	char * szBuffer					- Buffer used to store result and return as a character
 *			const PeriodData *pPeriodData	- Address of the structure under consideration for calculations
 *
 * Return:	None
 *
 ************************************************************************/
static void MovaSatFlowStats_GetStatsOk_( char * const szBuffer, const PeriodData * const pPeriodData)
{
	NULL_POINTER_CHECK( SFSTATS, pPeriodData );
	NULL_POINTER_CHECK( SFSTATS, szBuffer );

	sprintf( szBuffer, "%s", 
			pPeriodData->lRecAvgMetCriteria  == TRUE ? "Y" :
			pPeriodData->lRecAvgMetCriteria == FALSE ? "N" : gszUNKNOWN_VALUE );
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_GetTotal_
 *
 * Description: Function to get count for the final count of values recorded
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010
 *
 * Params:	char * szBuffer					- Buffer used to store result and return as a character
 *			const PeriodData *pPeriodData	- Address of the structure under consideration for calculations
 *
 * Return:	None
 *
 ************************************************************************/

static void MovaSatFlowStats_GetTotal_( char * const szBuffer, const PeriodData * const pPeriodData)
{
	NULL_POINTER_CHECK( SFSTATS, pPeriodData );
	NULL_POINTER_CHECK( SFSTATS, szBuffer );

	if ( pPeriodData->n16Count == gn16ZERO )
	{
		sprintf( szBuffer, gszOUTPUT_VALUE_THREE_ZERO );
	}
	else
	{
		if (pPeriodData->n16Count > 999)
		{
			sprintf( szBuffer, gszOUTPUT_VALUE_THREE_NINE );
		}
		else
		{
			sprintf( szBuffer, "%3d", (int)pPeriodData->n16Count  );
		}
	}
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_GetMean_
 *
 * Description: Function to get calculated Mean
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010	
 *
 * Params:	char * szBuffer					- Buffer used to store result and return as a character
 *			const PeriodData *pPeriodData	- Address of the structure under consideration for calculations
 *
 * Return:	None
 *
 ************************************************************************/

static void MovaSatFlowStats_GetMean_( char * const szBuffer,const PeriodData * const pPeriodData )
{
	NULL_POINTER_CHECK( SFSTATS, pPeriodData );
	NULL_POINTER_CHECK( SFSTATS, szBuffer );

	if ( pPeriodData->n16Count  == gn16ZERO )
	{
		sprintf( szBuffer, gszOUTPUT_VALUE_FOUR_ZERO );
	}
	else
	{
		if(pPeriodData->n16ID == gn16SATFLOW_REJECTED)
		{
			sprintf( szBuffer, "%4d", (int)pPeriodData->rMeanS);
		}
		else
		{
			sprintf( szBuffer, "%3.1f", pPeriodData->rMeanS);
		}
	}
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_Get15Perc_
 *
 * Description: Function to get calculated 15th Percentile
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010	
 *
 * Params:	char * szBuffer					- Buffer used to store result and return as a character
 *			const PeriodData *pPeriodData	- PeriodData * pPeriodData - Data recorded during selected time period
 *
 * Return:	None
 *
 ************************************************************************/

static void MovaSatFlowStats_Get15Perc_( char * const szBuffer, const PeriodData * const pPeriodData)
{
	NULL_POINTER_CHECK( SFSTATS, pPeriodData );
	NULL_POINTER_CHECK( SFSTATS, szBuffer );

	if ( pPeriodData->rSmoothed15  == gn16ZERO )
	{
		sprintf( szBuffer, gszOUTPUT_VALUE_FOUR_ZERO );
	}
	else
	{
		sprintf( szBuffer, "%4d", (int) pPeriodData->rSmoothed15  );
	}
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_GetSumofSquares_
 *
 * Description: Function to get calculated Sum Of Squares
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010
 *
 * Params:	char * szBuffer					- Buffer used to store result and return as a character
 *			const PeriodData *pPeriodData	- PeriodData * pPeriodData - Address of the structure under consideration for calculations
 *
 * Return:	None
 *
 ************************************************************************/

static void MovaSatFlowStats_GetSumofSquares_( char * const szBuffer, const PeriodData * const pPeriodData )
{
	NULL_POINTER_CHECK( SFSTATS, pPeriodData );
	NULL_POINTER_CHECK( SFSTATS, szBuffer );

	if ( pPeriodData->rSumSqrS == gn16ZERO )
	{
		sprintf( szBuffer, gszOUTPUT_VALUE_FOUR_ZERO );
	}
	else
	{
		if(pPeriodData->n16ID == gn16SATFLOW_REJECTED)
		{
			sprintf( szBuffer, "%1.1E", pPeriodData->rSumSqrS);
		}
		else
		{
			sprintf( szBuffer, "%4.1f", pPeriodData->rSumSqrS);
		}
	}
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_SetOutputHeader_
 *
 * Description: Function to display they day of the week and time period while displaying the logs on the screen
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010
 *
 * Params:	int16 n16Period - Time slot within the day
 *
 * Return:	void
 *
 ************************************************************************/
static void MovaSatFlowStats_SetOutputHeader_(const int16 n16Period)
{
	char day[6];
	/* Determine where to display result (terminal/screen)*/
#ifdef IS_MOVACOMM_ENABLED
	int nOutputDest = GetOutputDestination();
#else
	int nOutputDest = 0;
#endif

	if (n16Period < gn16PERIOD_1 || n16Period > gn16PERIOD_6)
		return;

	sprintf(day, "W.day");
	if (n16Period == gn16PERIOD_5)
		sprintf(day, "Sat  ");
	else if (n16Period == gn16PERIOD_6)
		sprintf(day, "Sun  ");

	Send_FormatString(nOutputDest, 
		"\r\n%s,%2.2d:%2.2d-%2.2d:%2.2d,", day, 
		DI_get_satflow_start_time((uint8)n16Period) / 60,
		DI_get_satflow_start_time((uint8)n16Period) % 60,
		DI_get_satflow_end_time((uint8)n16Period) / 60,
		DI_get_satflow_end_time((uint8)n16Period) % 60);
}



/*************************************************************************
 *
 * Function: MovaSatFlowStats_Feedback_Messages_
 *
 * Description: Outputs the satflow dependent variable messages - Temp to be deleted after testing
 *
 * Author: pkale
 *
 * Date: 26-Nov-2010	
 *
 * Params: int16 lane - Lane number to output
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlowStats_Feedback_Messages_(const int16 periodNo)
{
	uint8 n16LinkNumber;
	uint8 n16LaneLoop;
#ifdef IS_MOVACOMM_ENABLED
	int nOutputDest = GetOutputDestination();
#else
	int nOutputDest = 0;
#endif

	for (n16LaneLoop = 0; n16LaneLoop < DI_get_lanes_count(); n16LaneLoop++)
	{
		if (gSatDataSts->Lane_[n16LaneLoop].LaneSatFlowSTlostPeriodData[periodNo-1].Satflow.lRecAvgMetCriteria == TRUE)
		{
			/*Tcomshr->io[28]= 23 indicates mova messages display selected and ShowSatFlow flag is set when users selects to display sat flow messages */
			if (DI_is_to_show_mova_messages() && ShowSatFlw == 1)
			{
				n16LinkNumber = (uint8)MovaSatFlowDependantVar_GetLinkIdFromLaneId_(n16LaneLoop);

				Send_FormatString( nOutputDest, "\r\nLane: %d Bontim: %.1f Comtim: %d  BonBc: %d "
												"Satinc: %.1f STlost: %.1f "
												"Maxmin: %.1f \r\n"
												"MoveqX: %.1f Osatcc: %d Osattm: %.1f "
												"Critg: %.1f : SatGap %.1f \r\n",
												n16LaneLoop + 1, (DI_get_bonus_green_recalc_time(n16LinkNumber+1)/10.0 ), ( DI_get_time_to_use_comb_x(n16LaneLoop+1)/10 ),DI_get_bonus_base_count(n16LaneLoop+1),
												((DI_get_satinc_time(n16LaneLoop+1))/10.0),(DI_get_startup_lost_time(n16LaneLoop+1)/10.0 ),
												(DI_get_max_min_green_time(n16LaneLoop+1)/10.0 ), (DI_get_moving_q_over_x_det(n16LaneLoop+1)/10.0 ), DI_get_oversat_critical_veh_count(n16LaneLoop+1), (DI_get_oversat_time(n16LaneLoop+1)/10.0 ),
												((float)DI_get_lane_critical_gap(n16LaneLoop+1)/10.0 ), (DI_get_lane_satflow_gap(n16LaneLoop+1)/10.0 ));

			}
		}
	}
}






/*************************************************************************
 *
 * Function: update_std_dev_data_
 *
 * Description: This function updates the Standard Deviation related data, namel: Mean and S.
 *
 * Author: IAbdelhalim
 *
 * Date: 23-April-2013
 *
 * Params: REAL x - the number to be included in calculating the Standard Deviation
 *		   PeriodData * pPData - pointer to the PeriodData that includes the data elements that will be updated.
 *
 * Return: void
 *
 * Note: This function implements the Welford / Knuth way which is detailed in:
 *		  http://www.johndcook.com/standard_deviation.html
 ************************************************************************/
static void update_std_dev_data_(REAL x, PeriodData * pPeriodData)
{
	NULL_POINTER_CHECK( SFSTATS, pPeriodData );
	
	pPeriodData->n16Count++;

	if (pPeriodData->n16Count == 1)
	{
		pPeriodData->rOldMeanS = x;
		pPeriodData->rNewMeanS = x;
		pPeriodData->rOldS = 0.0;
	}
	else
	{
		DivideError((int)(pPeriodData->n16Count-1), 803,"Satflow Count was %d", (pPeriodData->n16Count));

		pPeriodData->rNewMeanS = pPeriodData->rOldMeanS + (x - pPeriodData->rOldMeanS)/pPeriodData->n16Count;

		pPeriodData->rNewS = pPeriodData->rOldS + (x - pPeriodData->rOldMeanS)*(x - pPeriodData->rNewMeanS);

		/*Set up for the next iteration*/
		pPeriodData->rOldMeanS = pPeriodData->rNewMeanS;
		pPeriodData->rOldS = pPeriodData->rNewS;
	}

	/* Although this methods does not require the SumSqr to calculate the Standard Deviation, the SumSqr still
	   needs to be displayed in the messages */
	pPeriodData->rSumSqrS += (x*x);
}
