/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         sf.c
*
*  TITLE:        MOVA Saturation flow calculator
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	10-Feb-2002		6.0.0		..		IHR		......			Initial implementation for testing with MOVA
*	18-Aug-2008		6.0.3		..		PK		......			n16XDetToStoplineDis to use dx value used in mova setup and not use a fixed value.
*	31-May-2011		7.0.0		AA		PK		CRN003			Inserted data collection points (function calls) to satflow stats and satdepend var modules.
*	12-Dec-2011		7.0.1		AB		PK		CRN004			SatFlow and CANDET improvements
*	21-Jan-2013		7.0.5		..		PA		CRN025			Reduce memory requirements of satflow module
*	25-Jan-2013		7.0.5		..		AK		CRN023			Fix compiler warnings
*	31-Jan-2013		7.0.5		AC		AK		M7.0.5			Issue M7.0.5
*	15-Apr-2013		7.0.6		..		AK		CRN027			Improvements to SF feedback
*	22-May-2013		7.0.6		AD		AK		M7.0.6			Issue M7.0.6
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

/* Project includes */

#include "sf.h"
#include "sfmath.h"
#include "sfprv.h"
#include "sfstats.h"
#include "sftst.h"
#include "sftstprv.h"
#include "sftypes.h"
#if defined (TRL_INTEGRATION_TEST)
#include "generalfunc.h"
#endif

#include "dataset_interface.h"
#include "dynamic_data.h"
#include "mova_constants.h"
#include "junction_handler.h"
#include "links_handler.h"
#include "lanes_handler.h"
#include "detectors_handler.h"
#include "program_controller.h"

#include "core_interface.h"

/*************************
*	Defines
**************************/

/* Number of lanes per link that are referenced in the Ccomshr.llanes array */
#define gn16LANES_PER_LINK_NUM_MAX							((int16)3)

/* Default times that a detector must be blocked for (tenths of secs) 
   for the lane to be considered 'exit blocked' */
#define gn16X_DET_EXIT_BLOCKED_THRESHOLD_DEFAULT			((int16)60)
#define gn16STOP_OUT_DET_EXIT_BLOCKED_THRESHOLD_DEFAULT		((int16)60) /* For both OUT and STOPLINE detectors */
#define gn16EXIT_BLOCKED_THRESHOLD_INVALID					((int16)-1)

/* Default numbers of vehicles to stop-line/X-detectors at the start of green 
   before saturation flow rate can be reached */
#define gn16STOPLINE_STLOST_VEHICLES_NUM_DEFAULT			((int16)3)
#define gn16XDET_STLOST_VEHICLES_NUM_DEFAULT				((int16)3)
#define gn16STLOST_VEHICLES_NUM_MIN							((int16)0)
#define gn16STLOST_VEHICLES_NUM_MAX							((int16)10)

/* Durations are in tenths of seconds */
#define gn16LEAVING_AMBER_DURATION							((int16)30)
#define gn16EARLY_RED_DURATION								((int16)50)

/* Time (tenths of secs) that a vehicle must be standing over a detector 
   for to count as queueing */
#define	gn16STANDING_QUEUE_DURATION							((int16)40)

/* Metres of lane space assumed to be occupied by one car (and its gap) */
#define	gn16LANE_METRES_PER_CAR								((int16)6)

/* Invalid detector state change number */
#define gn16DET_STATE_CHANGE_NUM_INVALID					((int16)-1)

/* Invalid number of vehicles */
#define gn16VEHICLE_COUNT_INVALID							((int16)-1)

/* Invalid time */
#define gn16TIME_INVALID									((int16)-1)

/* Invalid STLOST time */
#define gn16STLOST_TIME_INVALID								((int16)-99)

/* Invalid flow rate */
#define grFLOW_RATE_INVALID									((REAL)-1.0)

/* Invalid detector occupancy */
#define grDET_OCCUPANCY_INVALID								((REAL)-1.0)

/* Invalid detector, lane and link numbers */
#define gn16DET_NUM_INVALID									((int16)-1)
#define gn16LANE_NUM_INVALID								((int16)-1)
#define gn16LINK_NUM_INVALID								((int16)-1)

/* Default exponential smoothing factors for combining 
   successive sat flow and STLOST estimates, and detector occupancies */
#define grSAT_FLOW_SMOOTHING_FACTOR_DEFAULT					((REAL)0.2)
#define grSTLOST_SMOOTHING_FACTOR_DEFAULT					((REAL)0.2)
#define	grDET_OCCUPANCY_SMOOTHING_FACTOR_DEFAULT			((REAL)0.05)
#define grFLOW_RATE_SMOOTHING_FACTOR_DEFAULT				(((REAL)1.0) / ((REAL)3.0))
#define gn16SMOOTHED_FLOW_DEFAULT							((int16)18)

/* Default factor for scaling MOVA critical gap values to find optimal critical 
   gaps for saturation flow estimation */
#define grCRITICAL_GAP_FACTOR_DEFAULT						((REAL)0.5)

/* Bounds for critical gap scaling factor */
#define grCRITICAL_GAP_SCALING_FACTOR_MIN					((REAL)0.0)
#define grCRITICAL_GAP_SCALING_FACTOR_MAX					((REAL)1.0)

/* Duration of gap between STLOST vehicles that is large enough to prevent a 
   (short queue) estimate from being carried out (tenths of seconds) */
/* TODO: Check with Mark/Keith about the length of this gap */
#define gn16STLOST_VEHICLE_SIGNIFICANT_GAP_TIME				((int16)40)

/* Number of vehicles at saturation flow considered to constitute a 'long' queue */
#define gn16LONG_QUEUE_VEHICLES_NUM_MIN						((int16)8)

/* Factor determining the bias towards the long queue sat. flow estimate for cases 
   where a valid short and long queue estimate have been obtained. 
   The smaller this value, the stronger the bias towards the long queue estimate. */
#define grLONG_QUEUE_WEIGHTING_FACTOR						((REAL)44.0)

/* Initial value (and min/max) for the factor used to scale mean (car) detector 
   occupancy to give an estimate of occupancy time for long/slow vehicles */
#define grHEAVIES_DET_OCC_FACTOR_DEFAULT					((REAL)2.0)
#define grHEAVIES_DET_OCC_FACTOR_MIN						((REAL)1.0)
#define grHEAVIES_DET_OCC_FACTOR_MAX						((REAL)100.0)

/* Initial value for the mean time a car takes to cross a detector (tenths of secs) */
#define grDET_MEAN_CAR_OCCUPANCY_TIME_DEFAULT				((REAL)7.0)

/* The default maximum permitted proportional error between the 
   flow rate calculated for a STOPLINE detector and the 
   flow rate calculated by MOVA for the X-detector in a lane */
#define grFLOW_RATE_PROPORTIONAL_ERROR_MAX_DEFAULT			((REAL)0.03)

/* Number of tenths of seconds in 6 minutes (hour/10) */
#define grTENTHS_OF_SECONDS_TO_SIX_MINUTES_FACTOR			((REAL)3600.0)

/* Number of tenths of seconds in 1 hour */
#define grTENTHS_OF_SECONDS_TO_HOURS_FACTOR					((REAL)36000.0)

/* Number of tenths of seconds in 1 minute */
#define grTENTHS_OF_SECONDS_TO_MINUTES_FACTOR				((REAL)600.0)

/* Number of tenths of seconds in 1 second */
#define grTENTHS_OF_SECONDS_TO_SECONDS_FACTOR				((REAL)10.0)

/* Number of tenths of seconds in 0.5 seconds */
#define gn16HALF_SECONDS_TO_TENTHS_OF_SECONDS_FACTOR		((int16)5)

/* Number of times that a lane passes through 'start of green' 
   that indicate that the warmup period has ended (at least 2) */
#define gn16WARM_UP_PERIOD_FINISHED_COUNT					((int16)2)

/* Time added to the saturation gap by MOVA to form the critical
   gap it uses for control purposes (tenths of seconds) */
#define gn16CONTROL_CRITICAL_GAP_INCREMENT					((int16)20)

/*Delay between simulation(Movasim/Vissim) updates and detector scan registering the changes*/
#define gn16SIMULATION_DELAY								((int16) 1)

/*Identifiers for Satflow\Stlost data*/
#define gn16SATFLOW											((int16) 0)
#define gn16STLOST											((int16) 1)

/* Data Required to find a bug in the SF routines */
/*#define SF_PROBLEM_FINDER */


/* This is the max number of detector transitions for one calculation period */
/* It is based on having approx 50 state changes for around 40 detectors */

#define gnMAX_TRANSITIONS									((uint16) 2000)


/* Used to identify that an invalid request has been made for a particular */
/* transition */
#define gnINVALID_DET_NO										((uint8) -1)

/* Used to identify that a slot in the container is not used */
#define gnINVALID_SLOT_NO										((uint16) -1)


/* Values that entries in the link greens matrix (Ccomshr.lgreen) can take */
enum
{	
	STAGE_LINK_SIGNAL_RED,
	STAGE_LINK_SIGNAL_ONLY_GREEN,
	STAGE_LINK_SIGNAL_GREEN,
	STAGE_LINK_SIGNAL_UNOPPOSED_GREEN,
	STAGE_LINK_SIGNAL_OPPOSED_GREEN
};

/* Values that the green marker (Ccomshr.marker) can take */
enum
{
	MARKER_STATE_START_INTERGREEN,
	MARKER_STATE_CONTINUED_INTERGREEN,
	MARKER_STATE_ABSOLUTE_MINIMUM_GREEN,
	MARKER_STATE_VARIABLE_MINIMUM_GREEN,
	MARKER_STATE_SATURATION_CHECK_GREEN,
	MARKER_STATE_DELAY_OPTIMISATION_GREEN,
	MARKER_STATE_DEMANDING_STAGE_CHANGE,
	MARKER_STATE_MAXIMUM_GREEN_EXPIRED,
	MARKER_STATE_WAITING_STAGE_CHANGE
};


/*************************
*	Global variables 
**************************/

static LOGIC	glModuleInitialised = FALSE;
SatData			gSatData;
int	*			ShowSatFlow;
#if defined (TRL_INTEGRATION_TEST)
static char SATTextBuf[MAX_OUTPUT_LEN];
#endif

/* Data required for test output */
#if defined (TEST_OUTPUT)

	static LaneOutput	gLaneOutput_[ gn16LANES_NUM_MAX ];

#endif /* defined (TEST_OUTPUT) */

extern TIMESTAMP_TYPE Com_read_rtc(int, ... );

#ifdef IS_MOVACOMM_ENABLED
extern int GetOutputDestination( void );
#endif

/* Data Required to find a bug in the SF routines */
#if defined (SF_PROBLEM_FINDER)

	int SF_Problem_Data [1][30];

#endif


/*********************************************************/
/* Rework sat flow container */

/* Container for all transitions for all detectors */
/* This is more efficient than having an array for each detector */
static DetectorTransition arrDetTransitionStore[gnMAX_TRANSITIONS];

/*Contains the index (into arrDetTransitionStore) of the first transition  */
/*for the appropriate detector i.e. arrFirstTransitionDet[2] contains the index */
/*for the first transition for detector number three*/
/*This saves searching through the whole array for a particular transition*/
static uint16 arrFirstTransitionDet[gn16DETS_NUM_MAX];

/*Contains the index (into arrDetTransitionStore) of the last transition  */
/*for the appropriate detector i.e. arrLastTransitionDet[2] contains the index */
/*for the last transition for detector number three*/
/*This is used to add transitions */
static uint16 arrLastTransitionDet[gn16DETS_NUM_MAX];

/* Cache the last read transition. */
/* If we are reading a list of transitions for one detector this ensures that */
/* we do not start at the beginning of the list */
static uint16 cachedLastDetTransition;

/* Cache the last read transition count. */
/* The position that the cached transition is in the list*/
static uint16 cachedGetDetTransitionCount;


/* Cache the last allocated position. */
/* Use the last allocated position when searching for next free element */
static uint16 cachedLastIndexPos;

/* end Rework sat flow container */
/*********************************************************/


/*************************
*	Private functions
**************************/

/* Main functions */
static void			MovaSatFlow_ResetAllVariables_(					void );
static void			MovaSatFlow_EstimateSatFlow_(					Lane * const pLane );
																	
/* Auxiliary functions */											
static void			MovaSatFlow_FlowRateDataUpdate_(				Lane * const pLane );
static void			MovaSatFlow_CalcSmoothedFlowRate_(				Lane * const pLane );
static void			MovaSatFlow_CheckSatFlowDetConsistency_(		Lane * const pLane );
static int16		MovaSatFlow_GetSatFlowDetector_(				const Lane * const pLane );
static LOGIC		MovaSatFlow_IsINDetectorValid_(					const Lane * const pLane );
static void			MovaSatFlow_DetectorReset_(						const Lane * const pLane );
static void			MovaSatFlow_DetectorUpdate_(					const Lane * const pLane );
static int16		MovaSatFlow_LinkFromLane_(						const Lane * const pLane );
static LOGIC		MovaSatFlow_IsLaneGreen_(						const Lane * const pLane );
static LOGIC		MovaSatFlow_IsLaneGreenOpposed_(				const Lane * const pLane );
static LOGIC		MovaSatFlow_IsExitBlocked_(						const Lane * const pLane );
static LOGIC		MovaSatFlow_IsDetectorBlocked_(					const Detector * const pDetector,
																	const int16 n16TimeStart, 
																	const int16 n16TimeEnd );
static DET_TYPE		MovaSatFlow_FindDetectorType_(					const int16 n16DetNo );
																	
static void			MovaSatFlow_InLateRed_(							Lane * const pLane );
static void			MovaSatFlow_InStartOfGreen_(					Lane * const pLane );
static void			MovaSatFlow_InMinimumGreen_(					Lane * const pLane );
static void			MovaSatFlow_InExtendedGreen_(					Lane * const pLane );
static void			MovaSatFlow_InLeavingAmber_(					Lane * const pLane );
static void			MovaSatFlow_InEarlyRed_(						Lane * const pLane );
static LOGIC		MovaSatFlow_IsXDetOnFromGreenStart_(			const Detector * const pDetector, 
																	const Lane * const pLane );
																	
static int16		MovaSatFlow_GetXDetQueueSizeMax_(				const int16 n16LaneNo );
																	
																	
static LOGIC		MovaSatFlow_IsSatFlowAfterVarMinGreen_(			const Detector * const pDetector, 
																	const Lane * const pLane,
																	const int16 n16FirstSatVehDetStateChangeNum,
																	const int16 n16LastSatVehDetStateChangeNum );
static int16		MovaSatFlow_GetFirstSatVehDetStateChangeNum_(	const Detector * const pDetector );
static int16		MovaSatFlow_GetLastSatVehDetStateChangeNum_(	const Detector * const pDetector,
																	const Lane * const pLane,
																	const int16 n16FirstSatVehDetStateChangeNum );
static LOGIC		MovaSatFlow_IsLaneFullySat_(					const Detector * const pDetector, 
																	const Lane * const pLane,
																	const int16 n16LastSatVehDetStateChangeNum );
static LOGIC		MovaSatFlow_IsCriticalGapBeforeVehicle_(		const Detector * const pDetector,
																	const Lane * const pLane,
																	const int16 n16ArrivingVehicleDetStateChangeNum );
static LOGIC		MovaSatFlow_IsSignificantGapBeforeFirstSatVehicle_(	const Detector * const pDetector,
																		const int16 n16FirstSatVehDetStateChangeNum );
static int16		MovaSatFlow_CalcMeasurementCritGap_(			const int16 n16MeanVehicleSatGapTime, 
																	const REAL rCritGapScalingFactor );
static void			MovaSatFlow_EstimateSatFlowFromStoplineDet_(	Detector * const pDetector, 
																	Lane * const pLane, 
																	const int16 n16FirstSatVehDetStateChangeNum, 
																	const int16 n16LastSatVehDetStateChangeNum );
static void			MovaSatFlow_EstimateSatFlowFromXDet_(			Detector * const pDetector, 
																	Lane * const pLane, 
																	const int16 n16FirstSatVehDetStateChangeNum, 
																	const int16 n16LastSatVehDetStateChangeNum, 
																	const LOGIC lIsLaneFullySat,
																	const LOGIC lIsGreenStartQueue );
static void			MovaSatFlow_CombineEstimates_(					Lane * const pLane, 
																	Detector * const pDetector,
																	const REAL rShortQueueSatFlowProportion,
																	const REAL rShortQueueSatFlowRate,
																	const REAL rLongQueueSatFlowRate,
																	const REAL rSTLOSTTime );

static void			MovaSatFlow_UpdateMeanDetOccupancy_(			const Lane * const pLane,
																	Detector * const pDetector );
static int16		MovaSatFlow_GetFirstArrivalAfterTime_(			const Detector * const pDetector,
																	const int16 n16Time );
static int16		MovaSatFlow_GetLastDepartureBeforeTime_(		const Detector * const pDetector,
																	const int16 n16Time );
static int16		MovaSatFlow_GetFirstVehicleArrival_(			const Detector * const pDetector );
static int16		MovaSatFlow_GetLastVehicleDeparture_(			const Detector * const pDetector );
static LOGIC		MovaSatFlow_IsDetectorSuspect_(					const int16 n16DetNo );


void 				MovaSatFlow_Output_Message_( 									const int16 lane );


/*********************************************************/
/* Rework sat flow container */

static void MovaSatFlowStats_AddDetectorTransition( const TransitionItem transition);

static BOOL MovaSatFlowStats_FindDetectorTransition( const uint16 transNo,
													 const uint16 detNo,
													 TransitionItem* foundTrans );

static int16 MovaSatFlowStats_GetTransitionTime( const uint16 transNo,
												 const uint16 detNo );

static int16 MovaSatFlowStats_GetTransitionState( const uint16 transNo,
												  const uint16 detNo );

static void MovaSatFlowStats_InitialiseTransitionData( void );

static void MovaSatFlowStats_DeleteDetectorTransitions( const uint16 detNo );

static uint16 MovaSatFlowStats_GetNextFreeSlot( void );

static int16 MovaSatFlowStats_GetLastTransitionState(const uint16 detNo);

/* end Rework sat flow container */
/*********************************************************/

/*************************************************************************
 *
 * Function: MovaSatFlow_LinkFromLane_
 *
 * Description: Given a lane, finds the link that lane is in
 *
 * Author: ihenderson
 *
 * Date: 13/11/02
 *
 * Params: Lane *pLane - lane to look up
 *
 * Return: int16 - number of link containing lane, or gn16LINK_NUM_INVALID
 *					if not found
 *
 ************************************************************************/
static int16 MovaSatFlow_LinkFromLane_( const Lane * const pLane )
{
	int16	n16LinkNo;
	uint8	n16LaneNo;
	int16	n16LinkLoop;
	uint8	n16LaneLoop;

	NULL_POINTER_CHECK( SF, pLane );

	/* Assume failure */
	n16LinkNo = gn16LINK_NUM_INVALID;

	/* For each link */
	for ( n16LinkLoop = 0; n16LinkLoop < gSatData.n16LinksNum; ++n16LinkLoop )
	{			
		/* For each lane that's part of the current link */
		for ( n16LaneLoop = 0; n16LaneLoop < DI_get_link_lanes_count((uint8)(n16LinkLoop+1)); ++n16LaneLoop )
		{
			/* Get the 0-based lane number */
			n16LaneNo = DI_get_lane_num((uint8)(n16LinkLoop + 1), n16LaneLoop) - 1;
			
			/* If this lane matches the lane we're searching for */
			if ( n16LaneNo == pLane->n16LaneNo )
			{
				/* Record link number */
				n16LinkNo = n16LinkLoop;

				/* Prepare to quit loops */
				n16LaneLoop = DI_get_link_lanes_count((uint8)(n16LinkLoop+1));
				n16LinkLoop = gSatData.n16LinksNum;

			} /* if ( n16LaneNo == pLane->n16LaneNo ) */

		} /* for ( n16LaneLoop = 0; n16LaneLoop < Tcomshr->llatot[ n16LinkLoop ]; ++n16LaneLoop ) */

	} /* for ( n16LinkLoop = 0; n16LinkLoop < gSatData.n16LinksNum; ++n16LinkLoop ) */

	return n16LinkNo;

} /* static int16 MovaSatFlow_LinkFromLane_( const Lane * const pLane ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_IsLaneGreen_
 *
 * Description: Determines whether the lane is at green
 *
 * Author: ihenderson
 *
 * Date: 13/11/02
 *
 * Params: Lane *pLane - lane
 *
 * Return: lIsLaneGreen - TRUE - lane is green, FALSE - lane isn't green
 *
 ************************************************************************/
static LOGIC MovaSatFlow_IsLaneGreen_( const Lane * const pLane )
{
	LOGIC	lIsLaneGreen;
	int16	n16LinkNo;

	NULL_POINTER_CHECK( SF, pLane );

	/* Get link lane belongs to */
	n16LinkNo = pLane->n16LinkNo;

	/* If link is at green (check the link green flag) */
	if ( get_link_green_flag((uint8)n16LinkNo) == LINK_FLAG_GREEN )
	{
		lIsLaneGreen = TRUE;
	}
	else
	{
		lIsLaneGreen = FALSE;
	}

	return ( lIsLaneGreen );

} /* static LOGIC MovaSatFlow_IsLaneGreen_( const Lane * const pLane ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_IsLaneGreenOpposed_
 *
 * Description: Determines whether a lane in the current stage has
 *				opposed green (and is therefore invalid for processing)
 *				This would only apply to right-turn links.
 *
 * Author: ihenderson

 *
 * Date: 14/11/02
 *
 * Params: Lane *pLane - lane
 *
 * Return: LOGIC lIsLinkGreenOpposed - whether the link the lane is 
 *									   in is opposed or not
 *
 ************************************************************************/
static LOGIC MovaSatFlow_IsLaneGreenOpposed_( const Lane * const pLane )
{
	int16	n16LinkNo;
	int16	n16StageNo;
	LOGIC	lIsLinkGreenOpposed;

	NULL_POINTER_CHECK(SF, pLane );

	/* Get 0-based link number */
	n16LinkNo = pLane->n16LinkNo;

	/* Get the 0-based current stage number */
	n16StageNo = (int16)get_current_stage()-1;
	KDEBUG_EVAL_EXPRESSION( SF, ( n16StageNo < gSatData.n16StagesNum ) && ( n16StageNo >= 0 ) );

	/* Decide whether the lane signal is valid for saturation flow calculation 
	   - don't want to compute saturation flow estimates for opposed greens */
	switch ( DI_get_link_green_status((uint8)n16StageNo+1, (uint8)n16LinkNo+1))
	{
		/*TODO: change those constant to the new ones in "mova_constants.h"*/
		case STAGE_LINK_SIGNAL_ONLY_GREEN:
		case STAGE_LINK_SIGNAL_GREEN:
		case STAGE_LINK_SIGNAL_UNOPPOSED_GREEN:

			lIsLinkGreenOpposed = FALSE;
			break;

		case STAGE_LINK_SIGNAL_RED:
		case STAGE_LINK_SIGNAL_OPPOSED_GREEN:

			lIsLinkGreenOpposed = TRUE;
			break;

		default:
			
			KDEBUG_WRITE1( DEBUG_LEVEL_ERROR, SF, "MovaSatFlow_IsLaneGreenOpposed_ bad enum = %d",DI_get_link_green_status((uint8)n16StageNo+1, (uint8)n16LinkNo+1) );
			
			lIsLinkGreenOpposed = FALSE;
			break;

	} /* switch ( Tcomshr->lgreen[ n16StageNo ][ n16LinkNo ] ) */

	return ( lIsLinkGreenOpposed );

} /* static LOGIC MovaSatFlow_IsLaneGreenOpposed_( const Lane * const pLane ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_IsExitBlocked_
 *
 * Description: Determines whether the given lane was blocked
 *				using the appropriate detector. (If it was blocked
 *				for a sufficient time, saturation flow estimation 
 *				will not be carried out.)
 *
 * Author: ihenderson
 *
 * Date: 14/11/02
 *
 * Params: Lane *pLane - lane
 *
 * Return: LOGIC lIsExitBlocked - TRUE - lane exit was blocked; 
 *							      FALSE - exit wasn't blocked.
 *
 ************************************************************************/
static LOGIC MovaSatFlow_IsExitBlocked_( const Lane * const pLane )
{
	int16		n16DetNo;
	int16		n16TimeStart, n16TimeEnd;
	LOGIC		lIsExitBlocked;
	Detector	*pDetector;

	NULL_POINTER_CHECK( SF, pLane );

	/* Get the detector being used for sat flow estimation in this lane */
	n16DetNo = pLane->n16SatFlowDetNo;
	pDetector = &( gSatData.Detector_[ n16DetNo ] );

	/* Set start and end times for exit blocking check according to detector type */
	switch ( pDetector->n16DetType )
	{
		case DET_TYPE_OUT_DET:
		case DET_TYPE_STOP_DET:
			
			/* Check from a little after the start of green so we don't identify
			   the lane as exit-blocked if the first vehicle is slow off the mark 
			   (checked use of STLOST here with Keith Wood (3/12/02) - I was 
			   concerned that STLOST wouldn't be enough time here, and wasn't
			   sure about using it as an actual time when it's really a 
			   theoretical time used in the modelling of saturation flow (ihenderson)) */
			n16TimeStart = pLane->n16ControlSTLOSTTime;
			n16TimeEnd = pLane->n16LeavingAmberStartTime;
			break;

		case DET_TYPE_X_DET:
						
			/* Changed start time to variable minimum green after 
			   consultation with Keith Wood (3/12/02) (ihenderson) */
			n16TimeStart = pLane->n16VarMinGreenTime;
			/*n16TimeStart = pLane->n16MaxVarMinGreenTime;*/
			
			/* Checked use of red start time with Keith Wood (3/12/02) 
			   - we only estimate saturation flow up to leaving amber, 
			   so I wondered whether we should be looking for evidence of 
			   exit-blocking after this (ihenderson) */
			n16TimeEnd = pLane->n16RedStartTime;
			break;

		default:
			
			/* Should always be one of the detector types above */
			KDEBUG_WRITE1( DEBUG_LEVEL_ERROR, SF, " MovaSatFlow_IsExitBlocked_ bad enum = %d", pDetector->n16DetType);
			n16TimeStart = gn16TIME_INVALID;
			n16TimeEnd = gn16TIME_INVALID;
			break;

	} /* switch ( pDetector->n16DetType ) */

	/* See if this detector was blocked between the given times */
	lIsExitBlocked = MovaSatFlow_IsDetectorBlocked_( 
		pDetector, n16TimeStart, n16TimeEnd );

	return ( lIsExitBlocked );

} /* static LOGIC MovaSatFlow_IsExitBlocked_( const Lane * const pLane ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_IsDetectorBlocked_
 *
 * Description: Determines whether the given detector was blocked
 *				between the given times.
 *
 * Author: ihenderson
 *
 * Date: 15/11/02
 *
 * Params: Detector* pDetector - detector to check for blocking
 *		   int16 n16TimeStart - check for exit blocking from this time
 *		   int16 n16TimeEnd - check for exit blocking up to this time
 *
 * Return: LOGIC lIsDetectorBlocked - 
 *			TRUE - detector was blocked; FALSE - detector wasn't blocked.
 *
 ************************************************************************/
static LOGIC MovaSatFlow_IsDetectorBlocked_(const Detector * const pDetector,
											const int16 n16TimeStart, const int16 n16TimeEnd )
{
	LOGIC	lIsDetectorBlocked;
	int16	n16DetChangeLoop;
	int16	n16BlockedTime;
	int16	n16DetectorOnTime;
	int16	n16DetectorOffTime;

	/* Assume detector wasn't blocked */
	lIsDetectorBlocked = FALSE;

	/* For each change of state */
	for ( n16DetChangeLoop = 0; 
		  n16DetChangeLoop < (pDetector->n16DetChangeCount - 1); 
		  ++n16DetChangeLoop )
	{
		/* If the detector was on (vehicle is over it) */
		if ( MovaSatFlowStats_GetTransitionState(n16DetChangeLoop, pDetector->n16DetNo) == DET_STATE_ON )
		{
			/* Get the times it went on and off */
			n16DetectorOnTime = MovaSatFlowStats_GetTransitionTime( n16DetChangeLoop, pDetector->n16DetNo );
			n16DetectorOffTime =  MovaSatFlowStats_GetTransitionTime( (n16DetChangeLoop + 1), pDetector->n16DetNo );

			/* If the times are both within the valid range for exit blocking */
			if ( Math_IsWithinRange( n16DetectorOnTime, n16TimeStart, n16TimeEnd ) &&
				 Math_IsWithinRange( n16DetectorOffTime, n16TimeStart, n16TimeEnd ) )
			{
				/* Calculate blocked time */
				n16BlockedTime = n16DetectorOffTime - n16DetectorOnTime;

				/* If the blocked time is big enough for exit blocking */
				if ( n16BlockedTime > pDetector->n16DetExitBlockedThreshold )
				{
					lIsDetectorBlocked = TRUE;
					
					/* Prepare to quit loop */
					n16DetChangeLoop = pDetector->n16DetChangeCount - 1;

				} /* If the blocked time is big enough for exit blocking */

			} /* If the times are both within the valid range for exit blocking */

		} /* If the detector is on (vehicle is over it) */
		
	} /* for ( n16DetChangeLoop = 0; n16DetChangeLoop < n16DetChangesNum; ++n16DetChangeLoop ) */

	return ( lIsDetectorBlocked );

} /* static LOGIC MovaSatFlow_IsDetectorBlocked_( const int16 n16DetChangesNum, ... ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetXDetQueueSizeMax_
 *
 * Description: Calculates the maximum number of whole cars 
 *				that can fit between the stop-line and the X-detector.
 *
 * Author: ihenderson
 *
 * Date: 20/11/02
 *
 * Params: int16 n16LaneNo - number of lane to process
 *
 * Return: int16 n16CarsNumMax - maximum number of cars
 *
 ************************************************************************/
static int16 MovaSatFlow_GetXDetQueueSizeMax_( const int16 n16LaneNo )
{
	int16	n16CarsNumMax;
	int16	n16XDetToStoplineDist;

	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );

	/* Get distance (metres) from X-det to stop-line */
	/* 
		TODO: dx is not a member of Ccomshr yet - it's part of the user setup data
		but is currently discarded by MOVA once times from X-det to stopline
		and other data that MOVA needs to operate is calculated. In the next version
		of MOVA (to which this module will be appended) this setup data *will* be in Ccomshr.
	*/

	n16XDetToStoplineDist = (int16)DI_get_x_det_distance((uint8)n16LaneNo+1) ;/* PK: Changed from 16XDetToStoplineDist = 45 to 16XDetToStoplineDist = Tcomshr->dx[ n16LaneNo ];*/
	KDEBUG_EVAL_EXPRESSION( SF, n16XDetToStoplineDist > 0 );		/* using a fixed DX of 45m might not seriously affect behaviour of the satflow algorithm, but it might have a significant effect at some junctions where DX is very low/high - e.g. the Bracknell Puffin, which has
														a DX of about 20m on one lane. */

	/* Calculate the number of whole cars that fit within this distance */
	n16CarsNumMax = n16XDetToStoplineDist / gn16LANE_METRES_PER_CAR;

	return ( n16CarsNumMax );

} /* static int16 MovaSatFlow_GetXDetQueueSizeMax_( const int16 n16LaneNo ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_InLateRed_
 *
 * Description: Checks to see whether the lane has changed from red.
 *
 * Author: ihenderson
 *
 * Date: 19/11/02
 *
 * Params: Lane * pLane - lane being processed
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlow_InLateRed_( Lane * const pLane )
{
	LOGIC		lIsLaneGreen;
	LOGIC		lIsGreenOpposed;

	NULL_POINTER_CHECK( SF, pLane );

	/* See if the lane has changed to green */
	lIsLaneGreen = MovaSatFlow_IsLaneGreen_( pLane );

	/* If the lane is green */	
    /* When flows are low, intergreen  Tcomshr->lgrefl[ n16LinkNo ]  is not updated properly,
       amber is being reported as green*/
            
	if ( lIsLaneGreen == TRUE && input_data.current_stage != 0 && (get_current_stage()-1)>=0)
	{
		/* Check that the lane has unopposed green for the current stage */
		lIsGreenOpposed = MovaSatFlow_IsLaneGreenOpposed_( pLane );

		if ( lIsGreenOpposed == FALSE )
		{				
			/* Lane has just changed to unopposed green */
			MovaSatFlow_InStartOfGreen_( pLane );
		}

	} /* If the lane is green */

} /* static void MovaSatFlow_InLateRed_( Lane * const pLane ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_InStartOfGreen_
 *
 * Description: Records times and whether there's a queue stretching
 *				to the X-detector at start of green.
 *
 * Author: ihenderson
 *
 * Date: 19/11/02
 *
 * Params: Lane *pLane - lane being processed
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlow_InStartOfGreen_( Lane * const pLane )
{
	NULL_POINTER_CHECK( SF, pLane );

	/* Reset local variable output data for this lane */
	MovaSatFlowTest_TestOutputReset_( pLane->n16LaneNo );

	MovaSatFlow_Output_Record_( gSatData.Detector_[ pLane->n16SatFlowDetNo ].n16DetType, 
		gLaneOutput_[ pLane->n16LaneNo ].n16SatFlowDetType, pLane->n16LaneNo );

	/* Reset lane timer */
	pLane->n16StageTime = 0;

	/* Reset detector data */
	MovaSatFlow_DetectorReset_( pLane );

	/* Store start times */
	pLane->n16GreenEffStartTime = pLane->n16StageTime + pLane->n16ControlSTLOSTTime;
	pLane->n16GreenStartTime = pLane->n16StageTime;

	KDEBUG_WRITE1( DEBUG_LEVEL_INFO, SF, "Lane %d: start of green detected.", 
		pLane->n16LaneNo + 1 );
	KDEBUG_WRITE2( DEBUG_LEVEL_INFO, SF, "Green start time %d, effective start time %d.", 
		pLane->n16GreenStartTime, pLane->n16GreenEffStartTime );

	/* If this lane has a valid (exclusive) IN-detector that isn't faulty and 
	   the number of vehicles that passed the IN detector for the lane
	   since start of effective red implies a queue beyond the X-detector 
	   (N.B. MOVA subtracts vehicles lost at sink dets from redcin for us) */
	if ( ( pLane->lIsINDetValid == TRUE ) &&
		 ( !MovaSatFlow_IsDetectorSuspect_( pLane->n16Dets_[ DET_TYPE_IN_DET ] ) ) && 
		 ( get_lane_red_count_in_det((uint8)pLane->n16LaneNo) > pLane->n16XDetQueueSizeMax ) )
	{
		/* Set IN queue flag */
		pLane->lINQueue = TRUE;
	}
	else
	{
		/* Reset IN queue flag */
		pLane->lINQueue = FALSE;
	}

	MovaSatFlow_Output_Record_( (int16)get_lane_red_count_in_det((uint8)pLane->n16LaneNo), 
		gLaneOutput_[ pLane->n16LaneNo ].n16RedCountIN, pLane->n16LaneNo );
	MovaSatFlow_Output_Record_( pLane->lINQueue, 
		gLaneOutput_[ pLane->n16LaneNo ].lINDetStandingQueue, pLane->n16LaneNo );

	/* Record the queue estimated between X-det and stop-line at start of green */
	pLane->n16XDetQueueSizeCurrent = (int16)get_lane_red_count_x_det((uint8)pLane->n16LaneNo);

	/* Lane is now in minimum green state */
	pLane->n16CurrentState = LANE_STATE_MINIMUM_GREEN;

	/* If start of green hasn't been entered enough times 
	   for warmup to have finished */
	if ( pLane->n16StartOfGreenCount < gn16WARM_UP_PERIOD_FINISHED_COUNT )
	{
		/* Increment the green warmup counter */
		pLane->n16StartOfGreenCount++;
	}

	MovaSatFlow_Output_Record_( pLane->n16StartOfGreenCount == 
		gn16WARM_UP_PERIOD_FINISHED_COUNT ? FALSE : TRUE,
		gLaneOutput_[ pLane->n16LaneNo ].lInWarmUpPeriod, pLane->n16LaneNo );

} /* static void MovaSatFlow_InStartOfGreen_( Lane * const pLane ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_InMinimumGreen_
 *
 * Description: Checks for end of variable minimum green,
 *				recording variable minimum green time if ended.
 *
 * Author: ihenderson
 *
 * Date: 20/11/02
 *
 * Params: Lane *pLane - lane being processed
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlow_InMinimumGreen_( Lane * const pLane )
{
	int16		n16LinkNo;
	
	NULL_POINTER_CHECK( SF, pLane );

	/* Update lane timer */
	KDEBUG_EVAL_EXPRESSION( SF, pLane->n16StageTime < INT16_MAX ); /* TIMER OVERRUN */
	pLane->n16StageTime++;

	/* Update the detector data */
	MovaSatFlow_DetectorUpdate_( pLane );

	/* Get the link this lane is part of */
	n16LinkNo = pLane->n16LinkNo;

	/* If the stage variable minimum green has been set (the maximum 
	   of all the relevant link variable minimum greens - links that started
	   green this stage). We could also check here that mingre != 0 - 
	   it should be 0 if it hasn't been set yet - gets reset to 0 at the 
	   beginning of leaving amber. */
	if (get_prog_marker() == MARKER_STATE_VARIABLE_MINIMUM_GREEN )
	{		
		/* We are now in the next green stage (checking for end of saturation flow) */
		pLane->n16CurrentState = LANE_STATE_EXTENDED_GREEN;

		/* Store variable minimum green time 
		  (time from start of green that var min green expires) */
		pLane->n16VarMinGreenTime = (int16)get_link_var_min_green_time((uint8)n16LinkNo);

		KDEBUG_WRITE1( DEBUG_LEVEL_INFO, SF, "Lane %d: stage abs min green expired; in var min green", 
			pLane->n16LaneNo + 1 );

		MovaSatFlow_Output_Record_( (int16)get_lane_red_count_x_det((uint8)pLane->n16LaneNo), 
			gLaneOutput_[ pLane->n16LaneNo ].n16RedCountX, pLane->n16LaneNo );
		MovaSatFlow_Output_Record_( pLane->n16VarMinGreenTime, 
			gLaneOutput_[ pLane->n16LaneNo ].n16VarMinGreen, pLane->n16LaneNo );

	} /* if ( Tcomshr->marker == MARKER_STATE_VARIABLE_MINIMUM_GREEN ) */

} /* static void MovaSatFlow_InMinimumGreen_( Lane * const pLane ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_InExtendedGreen_
 *
 * Description: Checks for end of green, recording time for the 
 *				start of leaving amber if ended.
 *
 * Author: ihenderson
 *
 * Date: 20/11/02
 *
 * Params: Lane *pLane - lane being processed
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlow_InExtendedGreen_( Lane * const pLane )
{
	LOGIC		lIsLaneGreen;
	
	NULL_POINTER_CHECK( SF, pLane );

	/* Update lane timer */
	KDEBUG_EVAL_EXPRESSION( SF, pLane->n16StageTime < INT16_MAX ); /* TIMER OVERRUN */
	pLane->n16StageTime++;

	/* Update the detector data */
	MovaSatFlow_DetectorUpdate_( pLane );

	/* See if the lane is still green */
	lIsLaneGreen = MovaSatFlow_IsLaneGreen_( pLane );

	/* If the lane is no longer green */
	if ( lIsLaneGreen == FALSE )
	{		
		/* Now in leaving amber */
		pLane->n16CurrentState = LANE_STATE_LEAVING_AMBER;

		/* Record leaving amber start time */
		pLane->n16LeavingAmberStartTime = pLane->n16StageTime;

		MovaSatFlow_Output_Record_( pLane->n16LeavingAmberStartTime, 
			gLaneOutput_[ pLane->n16LaneNo ].n16LeavingAmber, pLane->n16LaneNo );

	} /* if ( lIsLaneGreen == FALSE ) */

} /* static void MovaSatFlow_InExtendedGreen_( Lane * const pLane ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_InLeavingAmber_
 *
 * Description: Checks for end of leaving amber (start of red)
 *				recording red start time if it has ended.
 *
 * Author: ihenderson
 *
 * Date: 20/11/02
 *
 * Params: Lane *pLane - lane being processed
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlow_InLeavingAmber_( Lane * const pLane )
{

	NULL_POINTER_CHECK( SF, pLane );

	/* Update lane timer */
	KDEBUG_EVAL_EXPRESSION( SF, pLane->n16StageTime < INT16_MAX ); /* TIMER OVERRUN */
	pLane->n16StageTime++;

	/* Update the detector data */
	MovaSatFlow_DetectorUpdate_( pLane );

	/* If the leaving amber is complete */
	if ( pLane->n16StageTime >= pLane->n16LeavingAmberStartTime + gn16LEAVING_AMBER_DURATION )
	{		
		/* Lane is now about to move to red */
		pLane->n16CurrentState = LANE_STATE_EARLY_RED;

		/* Record red start time */
		pLane->n16RedStartTime = pLane->n16StageTime;

		MovaSatFlow_Output_Record_( pLane->n16RedStartTime, 
			gLaneOutput_[ pLane->n16LaneNo ].n16StartRed, pLane->n16LaneNo );

	} /* If the leaving amber is complete */

} /* static void MovaSatFlow_InLeavingAmber_( Lane * const pLane ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_InEarlyRed_
 *
 * Description: Checks for the end of early red, and if ended
 *				calls MovaSatFlow_EstimateSatFlow_ (if no exit-blocking)
 *				and updates the smoothed flow rate.
 *
 * Author: ihenderson
 *
 * Date: 20/11/02
 *
 * Params: Lane *pLane - lane being processed
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlow_InEarlyRed_( Lane * const pLane )
{
	LOGIC		lIsLaneExitBlocked;
	
	NULL_POINTER_CHECK( SF, pLane );

	/* Update lane timer */
	KDEBUG_EVAL_EXPRESSION( SF, pLane->n16StageTime < INT16_MAX ); /* TIMER OVERRUN */
	pLane->n16StageTime++;

	/* Update the detector data */
	MovaSatFlow_DetectorUpdate_( pLane );

	/* If the lane has been red for more than 5 seconds */
	if ( pLane->n16StageTime > pLane->n16RedStartTime + gn16EARLY_RED_DURATION )
	{
		KDEBUG_WRITE1( DEBUG_LEVEL_INFO, SF, "Lane %d: more than 5 seconds in red", 
			pLane->n16LaneNo + 1 );
		
		/* Lane is now in 'late' red */
		pLane->n16CurrentState = LANE_STATE_LATE_RED;

		MovaSatFlow_Output_Record_( pLane->lIsSatFlowDetFaulty, 
			gLaneOutput_[ pLane->n16LaneNo ].lIsSatFlowDetFaulty, pLane->n16LaneNo );

		/* If the sat flow detector was OK over the last cycle and 
		   the warm up period has finished */
		if ( ( pLane->lIsSatFlowDetFaulty == FALSE ) && 
			 ( pLane->n16StartOfGreenCount == gn16WARM_UP_PERIOD_FINISHED_COUNT ) )
		{
			/* Calculate the smoothed traffic flow rate for this lane */
			MovaSatFlow_CalcSmoothedFlowRate_( pLane );
		
			/* Determine whether the lane was exit-blocked at any point since green */
			lIsLaneExitBlocked = MovaSatFlow_IsExitBlocked_( pLane );
		
			MovaSatFlow_Output_Record_( lIsLaneExitBlocked, 
				gLaneOutput_[ pLane->n16LaneNo ].lIsExitBlocked, pLane->n16LaneNo );
		
			/* If lane wasn't exit-blocked */ 
			if ( lIsLaneExitBlocked == FALSE )
			{
				KDEBUG_WRITE1( DEBUG_LEVEL_INFO, SF, "Lane %d: exit and detector OK; attempting sat flow estimation", 
					pLane->n16LaneNo + 1 );
		
				/* Gathered all the information we need from start of green, so
				   estimate saturation flow */
				MovaSatFlow_EstimateSatFlow_( pLane );
		
			} /* If lane wasn't exit-blocked */
		
		} /* If the sat flow detector was OK over the last cycle and 
		     the warm up period has finished */
		
		/* Else detector was faulty for some or all of the last cycle
		   or the warmup period isn't over */
		else
		{
			KDEBUG_WRITE1( DEBUG_LEVEL_INFO, SF, "Lane %d: still in warm-up period or sat flow det faulty.", 
				pLane->n16LaneNo + 1 );
		
			/* Reset faulty detector flag, so we can see whether it's 
			   been repaired at the start of the next estimation period */
			pLane->lIsSatFlowDetFaulty = FALSE;
			
			/* Reset any flow rate data collected this cycle - we don't 
			   want data taken from (partially) faulty or warmup cycles */
			pLane->n16FlowRateDetStateCurrent = DET_STATE_INVALID;
			
		} /* Else detector was faulty for some or all of the last cycle
		     or the warmup period isn't over */

		/* Always save out the current smoothed estimates, 
		   regardless of whether they were updated or not 
		   (convert flow rates to veh/hour, and STLOST times to secs) */

		MovaSatFlow_Output_Record_( pLane->rSmoothedSatFlowRate == 
			grFLOW_RATE_INVALID ? pLane->rSmoothedSatFlowRate : 
			pLane->rSmoothedSatFlowRate * grTENTHS_OF_SECONDS_TO_HOURS_FACTOR,
			gLaneOutput_[ pLane->n16LaneNo ].rSmoothedSatFlow, 
			pLane->n16LaneNo );
		
		MovaSatFlow_Output_Record_( pLane->rSmoothedSTLOSTTime,
			gLaneOutput_[ pLane->n16LaneNo ].rSmoothedSTLOST, 
			pLane->n16LaneNo );
		
		MovaSatFlow_Output_Record_( gSatData.Detector_[ pLane->n16SatFlowDetNo ].rDetMeanCarOccupancyTime, 
			gLaneOutput_[ pLane->n16LaneNo ].rSmoothedOcc, pLane->n16LaneNo );

		/* x10 to convert from vehicles per 6 minutes to vehicles per hour */
		MovaSatFlow_Output_Record_( pLane->rSmoothedFlowRate * (REAL)10.0, 
			gLaneOutput_[ pLane->n16LaneNo ].rSmoothedFlowRate, pLane->n16LaneNo );
			
		/* Write MOVA Message for this lane as necessary */
		MovaSatFlow_Output_Message_( pLane->n16LaneNo );		

	} /* If the lane has been red for more than 5 seconds */

} /* static void MovaSatFlow_InEarlyRed_( Lane * const pLane ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_CalcSmoothedFlowRate_
 *
 * Description: Calculates the current flow rate for the given lane, 
 *				and updates the exponentially smoothed mean flow rate. 
 *				Should be called 5 seconds into red before an attempt 
 *				is made to estimate saturation flow.
 *
 * Author: ihenderson
 *
 * Date: 10/12/02
 *
 * Params: Lane *pLane - lane being processed
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlow_CalcSmoothedFlowRate_( Lane * const pLane )
{	
	REAL	rCurrentFlowRate;
	
	NULL_POINTER_CHECK( SF, pLane );
	
	/* Calculate the current flow rate */
	KDEBUG_EVAL_EXPRESSION( SF, pLane->n16FlowRateUpdateTime > 0 ); /* DIVIDE BY ZERO */
	rCurrentFlowRate = ( (REAL)( pLane->n16FlowRateVehiclesNum ) / 
		(REAL)( pLane->n16FlowRateUpdateTime ) ) * 
		grTENTHS_OF_SECONDS_TO_SIX_MINUTES_FACTOR;

	/* x10 to convert from vehicles per 6 minutes to vehicles per hour */
	MovaSatFlow_Output_Record_( rCurrentFlowRate * (REAL)10.0, 
		gLaneOutput_[ pLane->n16LaneNo ].rRawFlowRate, pLane->n16LaneNo );

	/* Update the exponentially smoothed mean flow rate */
	pLane->rSmoothedFlowRate = (REAL)( 
		( rCurrentFlowRate * grFLOW_RATE_SMOOTHING_FACTOR_DEFAULT )
		+ ( ( 1.0 - grFLOW_RATE_SMOOTHING_FACTOR_DEFAULT ) * pLane->rSmoothedFlowRate ) );

	/* Reset the flow rate update time and number of vehicles */
	pLane->n16FlowRateUpdateTime = 0;
	pLane->n16FlowRateVehiclesNum = 0;

} /* static void MovaSatFlow_CalcSmoothedFlowRate_( Lane * const pLane ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_EstimateSatFlow_
 *
 * Description: Controls saturation flow estimation by calling 
 *				estimation function for appropriate detector 
 *				depending on the results of a few pre-conditions.
 *
 * Author: ihenderson
 *
 * Date: 25/11/02
 *
 * Params: Lane *pLane - lane to estimate sat flow on
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlow_EstimateSatFlow_( Lane * const pLane )
{
	int16		n16DetNo;
	Detector	*pDetector;
	LOGIC		lIsLaneFullySat;
	LOGIC		lIsVehiclesAtSatFlowAfterVarMinGreen;
	LOGIC		lXStandingQueue;
	LOGIC		lIsGreenStartQueue;
	int16		n16FirstSatVehDetStateChangeNum;
	int16		n16LastSatVehDetStateChangeNum;
	
	NULL_POINTER_CHECK( SF, pLane );

	/* Get detector being used for saturation flow estimation in this lane */
	n16DetNo = pLane->n16SatFlowDetNo;
	pDetector = &( gSatData.Detector_[ n16DetNo ] );

	/* Find detector state change corresponding to the 
	   departure of the first vehicle at saturation flow */
	n16FirstSatVehDetStateChangeNum = 
		MovaSatFlow_GetFirstSatVehDetStateChangeNum_( pDetector );

	MovaSatFlow_Output_Record_( n16FirstSatVehDetStateChangeNum, 
		gLaneOutput_[ pLane->n16LaneNo ].n16FirstSatVehicle, pLane->n16LaneNo );

	/* If a first vehicle at saturation flow was found */
	if ( n16FirstSatVehDetStateChangeNum != gn16DET_STATE_CHANGE_NUM_INVALID )
	{
		/* Find detector state change corresponding to the 
		   departure of the last vehicle at saturation flow */
		n16LastSatVehDetStateChangeNum = 
			MovaSatFlow_GetLastSatVehDetStateChangeNum_( pDetector, pLane,
				n16FirstSatVehDetStateChangeNum );

		MovaSatFlow_Output_Record_( n16LastSatVehDetStateChangeNum, 
			gLaneOutput_[ pLane->n16LaneNo ].n16LastSatVehicle, pLane->n16LaneNo );

		/* If a last vehicle at saturation flow was found */
		if ( n16LastSatVehDetStateChangeNum != gn16DET_STATE_CHANGE_NUM_INVALID )
		{		
			/* Decide how to estimate sat flow based on detector type */
			switch ( pDetector->n16DetType )
			{
				case DET_TYPE_OUT_DET:
				case DET_TYPE_STOP_DET:
			
					/* Estimate saturation flow and STLOST */
					MovaSatFlow_EstimateSatFlowFromStoplineDet_( pDetector, pLane, 
						n16FirstSatVehDetStateChangeNum, n16LastSatVehDetStateChangeNum );
			
					break;
			
				case DET_TYPE_X_DET:
					
					/* Check for 4 seconds of continuous queue over the X-det from start of green */
					lXStandingQueue = MovaSatFlow_IsXDetOnFromGreenStart_( pDetector, pLane );
			
					MovaSatFlow_Output_Record_( lXStandingQueue, 
						gLaneOutput_[ pLane->n16LaneNo ].lQueueOverXDet, pLane->n16LaneNo );

					/* Check to see if saturation flow continued for at least three vehicles
					   after the end of the variable minimum green (the time MOVA 
					   calculates as sufficient to clear any vehicles queued between the 
					   stop-line and x-detector at the end of absolute minimum green) */
					lIsVehiclesAtSatFlowAfterVarMinGreen = 
						MovaSatFlow_IsSatFlowAfterVarMinGreen_( pDetector, pLane, 
							n16FirstSatVehDetStateChangeNum, n16LastSatVehDetStateChangeNum );
			
					MovaSatFlow_Output_Record_( lIsVehiclesAtSatFlowAfterVarMinGreen, 
						gLaneOutput_[ pLane->n16LaneNo ].lIsSatFlowAfterVarMinGreen, pLane->n16LaneNo );
			
					/* 
						If there was a queue at the start of green, estimate saturation flow. 
						Requires one of these two conditions:
				
						1.	Continuous occupancy of the X-detector at start of green (over certain time)
						2.	Sufficient vehicles crossed the IN-detector in the previous red to produce
							a queue upstream of the X-detector (in case a gap in the queue coincided
							with the X-detector, for which case 1. would be FALSE)
					*/
					if ( ( lXStandingQueue == TRUE ) ||
						 ( pLane->lINQueue == TRUE ) )
					{
						/* The short queue method relies on a queue at start of green to work */
						lIsGreenStartQueue = TRUE;

						/* Determine whether saturation flow continued for the duration of green */
						lIsLaneFullySat = MovaSatFlow_IsLaneFullySat_( pDetector, pLane,
							n16LastSatVehDetStateChangeNum );
		
						MovaSatFlow_Output_Record_( lIsLaneFullySat, 
							gLaneOutput_[ pLane->n16LaneNo ].lFullySat, pLane->n16LaneNo );
					
						/* Estimate saturation flow (both long and short queue methods) */
						MovaSatFlow_EstimateSatFlowFromXDet_( pDetector, pLane, 
							n16FirstSatVehDetStateChangeNum, n16LastSatVehDetStateChangeNum, 
							lIsLaneFullySat, lIsGreenStartQueue );
			
					} /* If queue at start of green */
			
					/* Else no queue at start of green */
					else
					{
						lIsGreenStartQueue = FALSE;
			
						/* If sat. flow continued until at least three vehicles after the 
						   end of the variable minimum green time */
						if ( lIsVehiclesAtSatFlowAfterVarMinGreen == TRUE )
						{
							/* Determine whether saturation flow continued for the duration of green */
							lIsLaneFullySat = MovaSatFlow_IsLaneFullySat_( pDetector, pLane,
								n16LastSatVehDetStateChangeNum );
		
							MovaSatFlow_Output_Record_( lIsLaneFullySat, 
								gLaneOutput_[ pLane->n16LaneNo ].lFullySat, pLane->n16LaneNo );							
							
							/* Estimate saturation flow (long queue method only) */
							MovaSatFlow_EstimateSatFlowFromXDet_( pDetector, pLane, 
								n16FirstSatVehDetStateChangeNum, n16LastSatVehDetStateChangeNum, 
								lIsLaneFullySat, lIsGreenStartQueue );
						}
						else
						{
							/* Update the average time per vehicle 
						       the X-detector was on for during the green */
							MovaSatFlow_UpdateMeanDetOccupancy_( pLane, pDetector );
						}
			
					} /* Else no queue at start of green */
			
					break;
			
				default:
					
					/* Should not be estimating saturation flow for detectors other than above */
					KDEBUG_WRITE1( DEBUG_LEVEL_ERROR, SF, "MovaSatFlow_EstimateSatFlow_ bad enum = %d", pDetector->n16DetType);
					break;
			
			} /* switch ( pDetector->n16DetType ) */

		} /* If a last vehicle at saturation flow was found */

	} /* If a first vehicle at saturation flow was found */

} /* static void MovaSatFlow_EstimateSatFlow_( Lane * const pLane ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_EstimateSatFlowFromStoplineDet_
 *
 * Description: Estimates saturation flow using the given data
 *				using the long queue method. The long queue estimate
 *				is just the number of vehicles crossing the 
 *				the stop-line detector at saturation flow divided by
 *				the time they took (N.B. the first vehicle at saturation
 *				flow, n16FirstSatVehDetStateChangeNum, takes account of 
 *				STLOST - see MovaSatFlow_GetFirstSatVehDetStateChangeNum_).
 *				Also estimates STLOST time.
 *
 * Author: ihenderson
 *
 * Date: 26/11/02
 *
 * Params:	Detector *pDetector - detector to estimate sat flow at
 *			Lane *pLane - lane to estimate sat flow on
 *			int16 n16FirstSatVehDetStateChangeNum - first vehicle
 *				leaving at saturation flow detector state change
 *			int16 n16LastSatVehDetStateChangeNum - last vehicle
 *				leaving at saturation flow detector state change
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlow_EstimateSatFlowFromStoplineDet_( Detector * const pDetector, 
														 Lane * const pLane, 
														 const int16 n16FirstSatVehDetStateChangeNum, 
														 const int16 n16LastSatVehDetStateChangeNum )
{
	int16		n16SatFlowVehiclesNum;
	int16		n16SatFlowEndTime;
	int16		n16SatFlowStartTime;
	int16		n16SatFlowTime;
	REAL		rStartupTime;
	REAL		rSTLOSTVehiclesSatFlowTime;
	REAL		rSTLOSTTime;
	REAL		rLongQueueSatFlowRate;
	REAL		rShortQueueSatFlowRate;
	REAL		rShortQueueSatFlowProportion;
	
	NULL_POINTER_CHECK( SF, pDetector );
	NULL_POINTER_CHECK( SF, pLane );
	KDEBUG_EVAL_EXPRESSION( SF, ( pDetector->n16DetType == DET_TYPE_STOP_DET ) || 
								( pDetector->n16DetType == DET_TYPE_OUT_DET ) );

	MovaSatFlow_Output_Record_( FALSE, 
			gLaneOutput_[ pLane->n16LaneNo ].lShortQueueMethodUsed, pLane->n16LaneNo );

	/* Determine number of vehicles at sat. flow
	   (the first vehicle itself is not counted, as the gap preceding it
	   belongs to the STLOST vehicle in front of it) 
	   N.B. Should always be an even number of detector state changes */
	n16SatFlowVehiclesNum = 
		( n16LastSatVehDetStateChangeNum - n16FirstSatVehDetStateChangeNum ) / 2;

	MovaSatFlow_Output_Record_( n16SatFlowVehiclesNum, 
		gLaneOutput_[ pLane->n16LaneNo ].n16LongQueueSatCount, pLane->n16LaneNo );

	/* If there were a sufficiently large number of vehicles at saturation flow */
#if defined (SF_PROBLEM_FINDER)
	SF_Problem_Data[ 0 ][ pLane->n16LaneNo ] = n16SatFlowVehiclesNum;
#endif
	
	if ( n16SatFlowVehiclesNum >= gn16LONG_QUEUE_VEHICLES_NUM_MIN )
	{
		/* Get the times saturation flow began and ended */
		n16SatFlowEndTime = MovaSatFlowStats_GetTransitionTime( n16LastSatVehDetStateChangeNum, pDetector->n16DetNo );
		n16SatFlowStartTime = MovaSatFlowStats_GetTransitionTime( n16FirstSatVehDetStateChangeNum, pDetector->n16DetNo );

		/* Calculate time at saturation flow */
		n16SatFlowTime = n16SatFlowEndTime - n16SatFlowStartTime;

		/* Calculate saturation flow for this lane */
		KDEBUG_EVAL_EXPRESSION( SF, n16SatFlowTime > 0 ); /* DIVIDE BY ZERO */
		rLongQueueSatFlowRate = (REAL)n16SatFlowVehiclesNum / (REAL)n16SatFlowTime;

		
		/* Calculate the startup time (time between green and the arrival at the 
		   detector of the first vehicle at saturation flow - the time for
		   approximately STLOST number of vehicles and their following gaps) */
		rStartupTime = (REAL)(MovaSatFlowStats_GetTransitionTime( (n16FirstSatVehDetStateChangeNum -1), pDetector->n16DetNo ) - pLane->n16GreenStartTime);

		/* Calculate the theoretical time it would take for STLOST vehicles num  
		   to cross through the detector assuming they were travelling at sat. flow */
		KDEBUG_EVAL_EXPRESSION( SF, pLane->rSmoothedSatFlowRate > 0.0 ); /* DIVIDE BY ZERO */
		
		rSTLOSTVehiclesSatFlowTime =  gSatData.n16StoplineSTLOSTVehiclesNum / pLane->rSmoothedSatFlowRate;

		/* Calculate STLOST time (the difference between the actual time taken, 
		   and the theoretical time taken if the vehicles were travelling at sat. flow) */
		rSTLOSTTime =  (rStartupTime - gn16SIMULATION_DELAY) - rSTLOSTVehiclesSatFlowTime + (1/pLane->rSmoothedSatFlowRate);
						
		/* ihenderson - removed 04/02/03 - *very* occasionally we will get a negative STLOST */
		/*KDEBUG_EVAL_EXPRESSION( SF, n16STLOSTTime >= 0 );*/

		/* Set the proportion of the short queue estimate to be used when combining
		   short and long queue estimates to zero, because there is no short queue
		   estimate for stop-line detectors */
		rShortQueueSatFlowProportion = 0.0;
		rShortQueueSatFlowRate = 0.0;
		
		MovaSatFlowStats_RecordData_( pLane, rSTLOSTTime, gn16STLOST );		

		/* Combine the sat flow and STLOST time estimates and store them */
		MovaSatFlow_CombineEstimates_( pLane, pDetector, rShortQueueSatFlowProportion,
			rShortQueueSatFlowRate, rLongQueueSatFlowRate, rSTLOSTTime );

		MovaSatFlow_Output_Record_( TRUE, 
			gLaneOutput_[ pLane->n16LaneNo ].lLongQueueMethodUsed, pLane->n16LaneNo );

		MovaSatFlow_Output_Record_( n16SatFlowTime, 
			gLaneOutput_[ pLane->n16LaneNo ].n16SatFlowTime, pLane->n16LaneNo );

	} /* If there were a sufficiently large number of vehicles at saturation flow */

	/* Else low number of vehicles at saturation flow */
	else
	{
		/* Just update the average time per vehicle the stop-line 
		   detector was on for during the green */
		MovaSatFlow_UpdateMeanDetOccupancy_( pLane, pDetector );

		MovaSatFlow_Output_Record_( FALSE, 
			gLaneOutput_[ pLane->n16LaneNo ].lLongQueueMethodUsed, pLane->n16LaneNo );
	}

} /* static void MovaSatFlow_EstimateSatFlowFromStoplineDet_( Detector * const pDetector, ... ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_EstimateSatFlowFromXDet_
 *
 * Description: Estimates saturation flow using the given data
 *				using methods appropriate to X-detectors.
 *				Creates two estimates if possible - long and
 *				short queue estimates. The long queue estimate
 *				is just the number of vehicles crossing the 
 *				the X-detector at saturation flow divided by
 *				the time they took; the short queue estimate
 *				(designed as a backup for cases where there 
 *				aren't enough vehicles for the more accurate
 *				long queue method) uses the X-detector data
 *				to estimate saturation flow at the *stop-line*
 *				Doesn't estimate STLOST.
 *
 * Author: ihenderson
 *
 * Date: 26/11/02
 *
 * Params:	Detector *pDetector - detector to estimate sat flow at
 *			Lane *pLane - lane to estimate sat flow on
 *			int16 n16FirstSatVehDetStateChangeNum - first vehicle
 *				leaving at saturation flow
 *			int16 n16LastSatVehDetStateChangeNum - last vehicle
 *				leaving at saturation flow
 *			LOGIC lIsLaneFullySat - whether lane was fully saturated
 *				for the entire green
 *			LOGIC lIsGreenStartQueue - whether there was a queue at
 *				start of green.
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlow_EstimateSatFlowFromXDet_( Detector * const pDetector, 
												  Lane * const pLane, 
												  const int16 n16FirstSatVehDetStateChangeNum, 
												  const int16 n16LastSatVehDetStateChangeNum, 
												  const LOGIC lIsLaneFullySat,
												  const LOGIC lIsGreenStartQueue )
{
	int16		n16LongQueueSatFlowVehiclesNum;
	int16		n16LongQueueSatFlowEndTime;
	int16		n16LongQueueSatFlowStartTime;
	int16		n16LongQueueSatFlowTime = 0;
	int16		n16ShortQueueSatFlowVehiclesNum;
	int16		n16ShortQueueSatFlowEndTime;
	int16		n16ShortQueueSatFlowStartTime;
	int16		n16ShortQueueSatFlowTime = 0;
	REAL		rSTLOSTTime;
	int16		n16ValidEstimatesNum;
	REAL		rLongQueueSatFlowRate;
	REAL		rShortQueueSatFlowRate;
	REAL		rShortQueueSatFlowProportion;
	
	NULL_POINTER_CHECK( SF, pLane );	
	NULL_POINTER_CHECK( SF, pDetector );
	KDEBUG_EVAL_EXPRESSION( SF, pDetector->n16DetType == DET_TYPE_X_DET );

	rSTLOSTTime = 0.0; /* STLOST time irrelevant for X-detectors */

	/* Determine number of vehicles at sat. flow 
	   (the first vehicle itself is not counted, as the gap preceding it
	   belongs to the 'STLOST' vehicle in front of it) 
	   N.B. Should always be an even number of detector state changes */
	n16LongQueueSatFlowVehiclesNum = 
		( n16LastSatVehDetStateChangeNum - n16FirstSatVehDetStateChangeNum ) / 2;

	/* Calculate the proportion of the short queue estimate to use
	   (the long queue estimate is likely to be more reliable, so 
	   this calculation weights it towards that) */
	rShortQueueSatFlowProportion = (REAL)( Math_Max( 0.0, 
		( 0.5 - ( (REAL)( n16LongQueueSatFlowVehiclesNum - 
		gn16LONG_QUEUE_VEHICLES_NUM_MIN )
		/ grLONG_QUEUE_WEIGHTING_FACTOR ) ) ) );

	/* If lane was saturated until into leaving amber */ 
	if ( lIsLaneFullySat == TRUE )
	{
		/* Reduce number of short queue vehicles by 1, 
		   so that effectively the last saturated vehicle was the one
		   immediately *before* leaving amber, instead of the one immediately *after*.
		   We need to be more conservative, as we're using the X-detector to estimate
		   saturation flow at the stop-line for the short queue method, and 
		   it's more likely that this vehicle will get across the stopline before red 
		   than the one after start of leaving amber at the X-det. */
		n16ShortQueueSatFlowVehiclesNum = n16LongQueueSatFlowVehiclesNum - 1;
	}
	else
	{
		n16ShortQueueSatFlowVehiclesNum = n16LongQueueSatFlowVehiclesNum;
	}

	n16ValidEstimatesNum = 0;

	MovaSatFlow_Output_Record_( n16ShortQueueSatFlowVehiclesNum, 
		gLaneOutput_[ pLane->n16LaneNo ].n16ShortQueueSatCount, pLane->n16LaneNo );

	/* If there are enough vehicles for short queue method sat flow estimation 
	   AND there's evidence of a queue to the X-detector at start of green
	   AND there aren't any large gaps amongst the STLOST vehicles (before the 
	   first vehicle at saturation flow) */
	if ( 
		 ( n16ShortQueueSatFlowVehiclesNum > 1 ) &&
		 ( lIsGreenStartQueue == TRUE) && 
		 ( !MovaSatFlow_IsSignificantGapBeforeFirstSatVehicle_( pDetector,
				n16FirstSatVehDetStateChangeNum ) )
	   )
	{
		/* Get the start and end saturation times */
		
		/* If lane was saturated until into leaving amber */ 
		if ( lIsLaneFullySat == TRUE )
		{
			/* Ignore last vehicle - may not cross stop-line (see comment above) */
			n16ShortQueueSatFlowEndTime = MovaSatFlowStats_GetTransitionTime(( n16LastSatVehDetStateChangeNum - 2 ), pDetector->n16DetNo);
		}
		else
		{
			n16ShortQueueSatFlowEndTime = MovaSatFlowStats_GetTransitionTime( n16LastSatVehDetStateChangeNum, pDetector->n16DetNo);
		}

		/* Add the theoretical time we're assuming it will take for the last 
		   vehicle to reach the stop-line from the X-detector */
		n16ShortQueueSatFlowEndTime += pLane->n16XDetToStoplineCruiseTime;

		/* Get the short queue start time (we approximate the flow/time graph
		   by assuming that vehicles don't move *at all* until start of green
		   + STLOST, and then move *immediately* at saturation flow rate - 
		   i.e. infinite acceleration after STLOST) */
		n16ShortQueueSatFlowStartTime = pLane->n16GreenEffStartTime;

		/* Add the number of vehicles estimated to be between the X-detector
		   and stop-line at start of green to the short queue vehicles total 
		   (as above, these are modelled as stationary from start of green 
		   to STLOST time, and then as moving immediately at saturation flow rate) */
		n16ShortQueueSatFlowVehiclesNum += pLane->n16XDetQueueSizeCurrent;

		/* Add the number of STLOST vehicles at the X-detector 
		   (between the queue to the X-det and the first sat flow vehicle) */
		n16ShortQueueSatFlowVehiclesNum += gSatData.n16XDetSTLOSTVehiclesNum;

		/* Add the first vehicle at saturation flow (not counted so far). 
		   N.B. The gap ahead of this vehicle 'belongs' to the STLOST vehicle
		   in front of it, so actually we'll be very slightly 
		   overestimating saturation flow for the short queue method. 
		   We have 'n' vehicles but 'n-1' gaps! */
		n16ShortQueueSatFlowVehiclesNum += 1;

		/* Calculate time at saturation flow */
		n16ShortQueueSatFlowTime = n16ShortQueueSatFlowEndTime - n16ShortQueueSatFlowStartTime;

		/* Add time for the missing gap (see above) */
		n16ShortQueueSatFlowTime += (int16)DI_get_satflow_mean_gap((uint8)pLane->n16LaneNo+1);

		/* Calculate the short queue method saturation flow estimate */
		KDEBUG_EVAL_EXPRESSION( SF, n16ShortQueueSatFlowTime > 0 ); /* DIVIDE BY ZERO */
		rShortQueueSatFlowRate = (REAL)n16ShortQueueSatFlowVehiclesNum / 
			(REAL)n16ShortQueueSatFlowTime;

		n16ValidEstimatesNum++;

		MovaSatFlow_Output_Record_( TRUE, 
			gLaneOutput_[ pLane->n16LaneNo ].lShortQueueMethodUsed, pLane->n16LaneNo );

	} /* If there are enough vehicles for short queue method sat flow estimation */

	/* Else not enough vehicles for short queue method */
	else
	{
		rShortQueueSatFlowRate = 0.0;
		
		/* Ensure only the long queue estimate is used in the combined estimate */
		rShortQueueSatFlowProportion = 0.0; 

		MovaSatFlow_Output_Record_( FALSE, 
			gLaneOutput_[ pLane->n16LaneNo ].lShortQueueMethodUsed, pLane->n16LaneNo );

	} /* Else not enough vehicles for short queue method */

	MovaSatFlow_Output_Record_( n16LongQueueSatFlowVehiclesNum, 
		gLaneOutput_[ pLane->n16LaneNo ].n16LongQueueSatCount, pLane->n16LaneNo );

	/* If there were enough vehicles at saturation flow for long queue method */
	if ( n16LongQueueSatFlowVehiclesNum >= gn16LONG_QUEUE_VEHICLES_NUM_MIN )
	{
		/* Get the times saturation flow began and ended */
		n16LongQueueSatFlowStartTime = MovaSatFlowStats_GetTransitionTime( n16FirstSatVehDetStateChangeNum, pDetector->n16DetNo );
		n16LongQueueSatFlowEndTime = MovaSatFlowStats_GetTransitionTime( n16LastSatVehDetStateChangeNum, pDetector->n16DetNo );

		/* Calculate time at saturation flow */
		n16LongQueueSatFlowTime = n16LongQueueSatFlowEndTime - n16LongQueueSatFlowStartTime;

		/* Calculate the long queue method saturation flow estimate */
		KDEBUG_EVAL_EXPRESSION( SF, n16LongQueueSatFlowTime > 0 ); /* DIVIDE BY ZERO */
		rLongQueueSatFlowRate = (REAL)n16LongQueueSatFlowVehiclesNum / 
			(REAL)n16LongQueueSatFlowTime;

		n16ValidEstimatesNum++;

		MovaSatFlow_Output_Record_( TRUE, 
			gLaneOutput_[ pLane->n16LaneNo ].lLongQueueMethodUsed, pLane->n16LaneNo );

	} /* If there were enough vehicles at saturation flow for long queue method */
		
	/* Else not enough vehicles at sat flow for the long queue estimation method */
	else
	{
		rLongQueueSatFlowRate = 0.0;
		
		/* Ensure only the short queue estimate is used in the combined estimate */
		rShortQueueSatFlowProportion = 1.0;

		MovaSatFlow_Output_Record_( FALSE, 
			gLaneOutput_[ pLane->n16LaneNo ].lLongQueueMethodUsed, pLane->n16LaneNo );

	} /* Else not enough vehicles at sat flow for the long queue estimation method */

	/* If we made at least one valid estimate  */
#if defined (SF_PROBLEM_FINDER)
	SF_Problem_Data[ 1 ][ pLane->n16LaneNo ] = n16ValidEstimatesNum;
#endif

	if ( n16ValidEstimatesNum > 0 )
	{				
		/* Combine the sat flow estimates and store them */
		MovaSatFlow_CombineEstimates_( pLane, pDetector, rShortQueueSatFlowProportion,
			rShortQueueSatFlowRate, rLongQueueSatFlowRate, rSTLOSTTime );

		/* Record time over which estimate was made (weighted the same as for sat. flow estimates) */
		MovaSatFlow_Output_Record_( (int16)( ( rShortQueueSatFlowProportion * (REAL)n16ShortQueueSatFlowTime ) +
			( ( 1.0 - rShortQueueSatFlowProportion ) * (REAL)n16LongQueueSatFlowTime ) ), 
			gLaneOutput_[ pLane->n16LaneNo ].n16SatFlowTime, pLane->n16LaneNo );
		
		/* If there's a STOP detector */
		if (pLane->n16Dets_[ DET_TYPE_STOP_DET ] != gn16DET_NUM_INVALID)
		{
			/* Change the saturation flow estimation detector to the Stopline-detector. 
			Ensures that the stopline detectors is constantly checked of consistency.
			Previously, once stopline detectors was found inconsistent, it was ignored forever*/
			pLane->n16SatFlowDetNo = pLane->n16Dets_[ DET_TYPE_STOP_DET ];
			KDEBUG_EVAL_EXPRESSION( SF, pLane->n16SatFlowDetNo != gn16DET_NUM_INVALID );
			
			/* Reset the current X-detector state to invalid */
			pLane->n16FlowRateDetStateCurrent = DET_STATE_INVALID;
			
			/* Initialise the smoothed flow rate to the original MOVA X-detector smoothed flow rate */
			pLane->rSmoothedFlowRate = (REAL)get_lane_smoothed_flow((uint8)pLane->n16LaneNo);

		}
		
	}
	/* Else no valid estimates */
	else
	{
		/* Just update the average time per vehicle the stop-line 
		   detector was on for during the green */
		MovaSatFlow_UpdateMeanDetOccupancy_( pLane, pDetector );
	}

} /* static void MovaSatFlow_EstimateSatFlowFromXDet_( Detector * const pDetector, ... ) */




/*************************************************************************
 *
 * Function: MovaSatFlow_CombineEstimates_
 *
 * Description: Combines short and long queue estimates into one
 *				estimate of saturation flow for this lane, and
 *				merges it with the previous estimate. Also
 *				saves the estimates.
 *
 * Author: ihenderson
 *
 * Date: 26/11/02
 *
 * Params:	Detector *pDetector - detector to estimate sat flow at
 *			Lane *pLane - lane to estimate sat flow on
 *			REAL rShortQueueSatFlowProportion - proportion of short
 *				queue sat. flow estimate to use
 *			REAL rShortQueueSatFlowRate - short queue sat. flow estimate
 *			REAL rLongQueueSatFlowRate - long queue sat. flow estimate
 *			int16 n16STLOSTTime - Startup time lost
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlow_CombineEstimates_( Lane * const pLane, 
										   Detector * const pDetector,
										   const REAL rShortQueueSatFlowProportion,
										   const REAL rShortQueueSatFlowRate,
										   const REAL rLongQueueSatFlowRate,
										   const REAL rSTLOSTTime )
{
	REAL		rCombinedSatFlowRate;

	NULL_POINTER_CHECK( SF, pLane );	
	NULL_POINTER_CHECK( SF, pDetector );

	MovaSatFlow_Output_Record_( rShortQueueSatFlowRate * grTENTHS_OF_SECONDS_TO_HOURS_FACTOR,
		gLaneOutput_[ pLane->n16LaneNo ].rShortQueueSatFlow, pLane->n16LaneNo );
	MovaSatFlow_Output_Record_( rLongQueueSatFlowRate * grTENTHS_OF_SECONDS_TO_HOURS_FACTOR,
		gLaneOutput_[ pLane->n16LaneNo ].rLongQueueSatFlow, pLane->n16LaneNo );

	/* Calculate the combined sat. flow estimate from weighted proportions of
	   the long and short queue sat. flow estimates */
	rCombinedSatFlowRate = (REAL)( ( rShortQueueSatFlowProportion * rShortQueueSatFlowRate )
		+ ( ( 1.0 - rShortQueueSatFlowProportion ) * rLongQueueSatFlowRate ) );

	MovaSatFlow_Output_Record_( rCombinedSatFlowRate * grTENTHS_OF_SECONDS_TO_HOURS_FACTOR, 
		gLaneOutput_[ pLane->n16LaneNo ].rRawSatFlow, pLane->n16LaneNo );
	MovaSatFlow_Output_Record_( rSTLOSTTime, 
		gLaneOutput_[ pLane->n16LaneNo ].rRawSTLOST, pLane->n16LaneNo );

	/* Update the running smoothed saturation flow estimate for this lane */
	pLane->rSmoothedSatFlowRate = 
		(REAL)( ( rCombinedSatFlowRate * gSatData.rSatFlowSmoothingFactor ) +
		( ( 1.0 - gSatData.rSatFlowSmoothingFactor ) * pLane->rSmoothedSatFlowRate ) );

	/* Update the running smoothed STLOST time estimate for this lane */
	pLane->rSmoothedSTLOSTTime = 
		(REAL)( ( rSTLOSTTime * gSatData.rSTLOSTSmoothingFactor ) +
		( ( 1.0 - gSatData.rSTLOSTSmoothingFactor ) * pLane->rSmoothedSTLOSTTime ) );

	
	MovaSatFlowStats_RecordData_( pLane, rCombinedSatFlowRate, gn16SATFLOW );

	 /* Check the detector's appropriateness for continued saturation 
                flow estimation and change it if necessary */
     MovaSatFlow_CheckSatFlowDetConsistency_( pLane );

	/* Update the average time per vehicle the stop-line 
	   detector was on for during the green */
	MovaSatFlow_UpdateMeanDetOccupancy_( pLane, pDetector );

} /* static void MovaSatFlow_CombineEstimates_( Lane * const pLane, ... ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_CheckSatFlowDetConsistency_
 *
 * Description: Checks the validity of the detector used for 
 *				saturation flow estimation in the given lane by
 *				comparing the current flow rate with the flow
 *				rate calculated by MOVA from the X-detector in
 *				this lane.
 *				Only does this for STOPLINE detectors currently.
 *
 * Author: ihenderson
 *
 * Date: 10/12/02
 *
 * Params:	Lane *pLane - lane to check detector in
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlow_CheckSatFlowDetConsistency_( Lane * const pLane )
{
	int16		n16DetNo;
	Detector	*pDetector;
	REAL		rFlowRateProportion;
	LOGIC		lWasDetectorValid;
	
	NULL_POINTER_CHECK( SF, pLane );

	/* Get the detector used for saturation flow estimation in this lane */
	n16DetNo = pLane->n16SatFlowDetNo;
	pDetector = &( gSatData.Detector_[ n16DetNo ] );

	/* Assume the detector was valid for saturation flow estimation */
	lWasDetectorValid = TRUE;

	/* If the sat flow detector is a STOPLINE detector */
	if ( pDetector->n16DetType == DET_TYPE_STOP_DET )
	{
		/* Calculate the proportion of the MOVA X-detector flow rate
		   that this STOPLINE detector's flow rate represents */
		rFlowRateProportion = (REAL)(get_lane_smoothed_flow((uint8)pLane->n16LaneNo) ) /
			pLane->rSmoothedFlowRate;

		/* If the flow rate isn't within the valid range */
		if ( !Math_IsWithinRange( rFlowRateProportion, 
			(1.0 - gSatData.rFlowRateProportionalErrorMax), 
			(1.0 + gSatData.rFlowRateProportionalErrorMax) ) )
		{
			/* Detector wasn't valid for saturation flow estimation */
			lWasDetectorValid = FALSE;
			
			/* Change the saturation flow estimation detector to the X-detector */
			pLane->n16SatFlowDetNo = pLane->n16Dets_[ DET_TYPE_X_DET ];
			KDEBUG_EVAL_EXPRESSION( SF, pLane->n16SatFlowDetNo != gn16DET_NUM_INVALID );

			/* Reset the current X-detector state to invalid */
			pLane->n16FlowRateDetStateCurrent = DET_STATE_INVALID;

		} /* If the flow rate isn't within the valid range */

	} /* If the sat flow detector is a STOPLINE detector */

} /* static void MovaSatFlow_CheckSatFlowDetConsistency_( Lane * const pLane ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_IsSatFlowAfterVarMinGreen_
 *
 * Description: Determines whether at least three vehicles crossed 
 *				the X-detector after the variable minimum green and before
 *				saturation flow ended (a condition for sat
 *				flow estimation with an X-detector)
 *
 *				N.B. This function changed 3/12/02 to use the variable
 *				minimum green instead of the maximum variable minimum green
 *				after consultation with Keith Wood (ihenderson).
 *				
 * Author: ihenderson
 *
 * Date: 25/11/02
 *
 * Params:	Detector *pDetector - detector to check
 *			Lane *pLane - lane to check
 *			int16 n16FirstSatVehDetStateChangeNum - change number corres.
 *				to first vehicle at saturation flow
 *			int16 n16LastSatVehDetStateChangeNum - change number corres.
 *				to last vehicle at saturation flow
 *
 * Return:	LOGIC lIsSatFlowAfterVarMinGreen - whether there was sat flow
 *				after the variable minimum green
 *
 ************************************************************************/
static LOGIC MovaSatFlow_IsSatFlowAfterVarMinGreen_( const Detector * const pDetector, 
													 const Lane * const pLane,
													 const int16 n16FirstSatVehDetStateChangeNum,
													 const int16 n16LastSatVehDetStateChangeNum )
{
	int16		n16DetStateChangeLoop;
	int16		n16ThirdFromLastVehicleDetStateChangeNum;
	LOGIC		lIsSatFlowAfterVarMinGreen;
	
	NULL_POINTER_CHECK( SF, pLane );	
	NULL_POINTER_CHECK( SF, pDetector );
	KDEBUG_EVAL_EXPRESSION( SF, pDetector->n16DetType == DET_TYPE_X_DET );

	/* Get the detector change corresponding to departure of third from last vehicle */
	n16ThirdFromLastVehicleDetStateChangeNum = n16LastSatVehDetStateChangeNum - 4;

	/* Assume failure */
	lIsSatFlowAfterVarMinGreen = FALSE;

	/* For each vehicle leaving during saturation flow */
	for ( n16DetStateChangeLoop = n16FirstSatVehDetStateChangeNum;
		  n16DetStateChangeLoop <= n16ThirdFromLastVehicleDetStateChangeNum;
		  n16DetStateChangeLoop += 2 )
	{
		/* If this detector state change occurred after the variable minimum-green */
		if ( MovaSatFlowStats_GetTransitionTime( n16DetStateChangeLoop, pDetector->n16DetNo ) > 
			( pLane->n16VarMinGreenTime + pLane->n16GreenStartTime ) )
		{
			/*	Change occurred after var min green, and there are at least three vehicles
				after var min green at saturation flow */
			lIsSatFlowAfterVarMinGreen = TRUE;

			/* Quit loop */
			n16DetStateChangeLoop = n16ThirdFromLastVehicleDetStateChangeNum + 1;

		} /* If this detector state change occurred after the max minimum-green */

	} /* For each vehicle leaving during saturation flow */

	return ( lIsSatFlowAfterVarMinGreen );

} /* static LOGIC MovaSatFlow_IsSatFlowAfterVarMinGreen_( const Detector * const pDetector, ... ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetFirstSatVehDetStateChangeNum_
 *
 * Description: Determines the detector state change corresponding
 *				to the vehicle leaving the detector at the start of sat. flow
 *				(an OFF state change). For a stopline detector, this is after
 *				STLOST vehicles (2, 3, 4...) have left the detector.
 *				For an X-detector, this is the second vehicle to leave the 
 *				detector. 
 *				If there isn't such a detector state change, returns -1
 *
 * Author: ihenderson
 *
 * Date: 25/11/02
 *
 * Params:	Detector *pDetector - detector to check
 *
 * Return: int16 n16FirstSatVehDetStateChangeNum - det state change of
 *				first vehicle leaving at sat flow
 *
 ************************************************************************/
static int16 MovaSatFlow_GetFirstSatVehDetStateChangeNum_( const Detector * const pDetector )
{
	int16		n16FirstSatVehDetStateChangeNum;
	int16		n16FirstVehDetStateChangeNum;
	
	NULL_POINTER_CHECK( SF, pDetector );

	/* Determine the detector state transition corresponding to the first vehicle leaving */
	if ( MovaSatFlowStats_GetTransitionState(0, pDetector->n16DetNo) == DET_STATE_OFF )
	{
		n16FirstVehDetStateChangeNum = 0;
	}
	else
	{
		n16FirstVehDetStateChangeNum = 1;
	}

	/* Calculate the hypothetical first saturation vehicle detector state transition */
	switch ( pDetector->n16DetType )
	{
		case DET_TYPE_STOP_DET:
		case DET_TYPE_OUT_DET:
			
			/* First vehicle at sat flow is the first to cross the detector
			   after the STLOST vehicles */
			n16FirstSatVehDetStateChangeNum = n16FirstVehDetStateChangeNum + 
				( 2 * gSatData.n16StoplineSTLOSTVehiclesNum );
			break;

		case DET_TYPE_X_DET:

			/* First vehicle at sat flow is the second to cross the detector */
			n16FirstSatVehDetStateChangeNum = n16FirstVehDetStateChangeNum + 
				( 2 * gSatData.n16XDetSTLOSTVehiclesNum );
			break;

		default:
			/* Shouldn't be calling this function if it's not one of the
			   detector types above */
			KDEBUG_WRITE1( DEBUG_LEVEL_ERROR, SF, " MovaSatFlow_GetFirstSatVehDetStateChangeNum_ bad enum = %d", pDetector->n16DetType );		
			n16FirstSatVehDetStateChangeNum = gn16DET_STATE_CHANGE_NUM_INVALID;
			break;

	} /* switch ( pDetector->n16DetType ) */


	/* If the hypothetical first vehicle at sat flow transition 
	   is not within valid bounds */
	if ( n16FirstSatVehDetStateChangeNum >= pDetector->n16DetChangeCount )
	{
		/* Set to invalid */
		n16FirstSatVehDetStateChangeNum = gn16DET_STATE_CHANGE_NUM_INVALID;
	}

	return ( n16FirstSatVehDetStateChangeNum );

} /* static int16 MovaSatFlow_GetFirstSatVehDetStateChangeNum_( const Detector * const pDetector ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetLastSatVehDetStateChangeNum_
 *
 * Description: Determines the detector state change corresponding
 *				to the last vehicle leaving the detector during
 *				saturation flow (before a critical gap, or end of green) 
 *				- an OFF state change.
 *
 * Author: ihenderson
 *
 * Date: 25/11/02
 *
 * Params:	Detector *pDetector - detector to check
 *			Lane *pLane - lane detector is in
 *			int16 n16FirstSatVehDetStateChangeNum - det state change
 *				corres. to departure of the first vehicle that
 *				we're calculating saturation flow from.
 *
 * Return: int16 n16LastSatVehDetStateChangeNum - det state change
 *				corres. to last vehicle at saturation flow during green
 *
 ************************************************************************/
static int16 MovaSatFlow_GetLastSatVehDetStateChangeNum_( const Detector * const pDetector,
														  const Lane * const pLane,
														  const int16 n16FirstSatVehDetStateChangeNum )
{
	int16		n16LastSatVehDetStateChangeNum;
	int16		n16LastDetStateChangeNum;
	int16		n16DetStateChangeLoop;
	LOGIC		lIsCriticalGap;
	
	NULL_POINTER_CHECK( SF, pDetector );
	NULL_POINTER_CHECK( SF, pLane );

	/* Get the last recorded detector state change number */
	n16LastDetStateChangeNum = pDetector->n16DetChangeCount - 1;

	/* Invalidate the last vehicle det state change num */
	n16LastSatVehDetStateChangeNum = gn16DET_STATE_CHANGE_NUM_INVALID;

	/* 
		Iterate through state changes searching for a critical gap or 
		vehicle arriving in leaving amber
	*/
	
	/* For each detector change to ON */
	for ( n16DetStateChangeLoop = n16FirstSatVehDetStateChangeNum + 1;
		  n16DetStateChangeLoop < pDetector->n16DetChangeCount;
		  n16DetStateChangeLoop += 2 )
	{
		/* See if there's a critical gap before this state change */
		lIsCriticalGap = MovaSatFlow_IsCriticalGapBeforeVehicle_( pDetector, 
			pLane, n16DetStateChangeLoop );

		/* If critical gap found */
		if ( lIsCriticalGap == TRUE )
		{
			/* Set last vehicle state change to the vehicle that left 
			   before the critical gap */
			n16LastSatVehDetStateChangeNum = n16DetStateChangeLoop - 1;

			/* Quit loop */
			n16DetStateChangeLoop = pDetector->n16DetChangeCount;

		} /* If critical gap found */

		/* Else critical gap not found before current vehicle */
		else
		{
			/* If this state change occurred during leaving amber */
			if ( MovaSatFlowStats_GetTransitionTime( n16DetStateChangeLoop, pDetector->n16DetNo )  >=
				 pLane->n16LeavingAmberStartTime )
			{
				/* If this isn't the last detector state change */
				if ( n16DetStateChangeLoop < n16LastDetStateChangeNum )
				{					
					/* Set the last vehicle at saturation flow to this one 
					   ( + 1 for when it left the detector ) */
					/* Note: this vehicle left in amber */
					n16LastSatVehDetStateChangeNum = n16DetStateChangeLoop + 1;
				}
				
				/* Else this change was the last one recorded */
				else
				{
					/* Set the last vehicle at saturation flow to the previous one */
					/* Note: this vehicle left in green */
					n16LastSatVehDetStateChangeNum = n16DetStateChangeLoop - 1;
				}

				/* Quit loop */
				n16DetStateChangeLoop = pDetector->n16DetChangeCount;

			} /* If this state change occurred during leaving amber */

		} /* Else critical gap not found before current vehicle */

	} /* For each detector change to ON */

	/* If we didn't find a last vehicle (i.e. no critical gap found and no 
	   vehicle arrived at the detector from the start of leaving amber) */
	if ( n16LastSatVehDetStateChangeNum == gn16DET_STATE_CHANGE_NUM_INVALID )
	{
		/* Set the last vehicle to the last vehicle leaving the detector */
		
		/* If the last state was a vehicle leaving */
		if ( MovaSatFlowStats_GetTransitionState(n16LastDetStateChangeNum, pDetector->n16DetNo) == DET_STATE_OFF )
		{
			/* Set the last vehicle to this vehicle */
			n16LastSatVehDetStateChangeNum = n16LastDetStateChangeNum;
		}
		
		/* Else the last state was a vehicle arriving */
		else
		{
			/* Set the last vehicle to the previous vehicle */
			n16LastSatVehDetStateChangeNum = n16LastDetStateChangeNum - 1;
		}

	} /* If we didn't find a last vehicle */

	return ( n16LastSatVehDetStateChangeNum );

} /* static int16 MovaSatFlow_GetLastSatVehDetStateChangeNum_( const Detector * const pDetector, ... ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_IsLaneFullySat_
 *
 * Description: Was traffic passing through this lane (at the given
 *				detector) at saturation flow throughout green?
 *
 * Author: ihenderson
 *
 * Date: 25/11/02
 *
 * Params:	Detector *pDetector - detector to check
 *			Lane *pLane - lane detector is in
 *			int16 n16LastSatVehDetStateChangeNum - det state change
 *				corres. to departure of the last vehicle that
 *				we're calculating saturation flow from.
 *
 * Return: LOGIC lIsLaneFullySat - whether the lane was at full saturation
 *				flow throughout green.
 *
 ************************************************************************/
static LOGIC MovaSatFlow_IsLaneFullySat_( const Detector * const pDetector, 
										  const Lane * const pLane,
										  const int16 n16LastSatVehDetStateChangeNum )
{
	LOGIC		lIsLaneFullySat;
	
	NULL_POINTER_CHECK( SF, pDetector );
	NULL_POINTER_CHECK( SF, pLane );

	/* If the last vehicle left during leaving amber */
	if ( MovaSatFlowStats_GetTransitionTime( n16LastSatVehDetStateChangeNum, pDetector->n16DetNo) >=
		 pLane->n16LeavingAmberStartTime )
	{
		lIsLaneFullySat = TRUE;
	}

	/* Else the last vehicle left during green */
	else
	{
		lIsLaneFullySat = FALSE;
	}

	return ( lIsLaneFullySat );

} /* static LOGIC MovaSatFlow_IsLaneFullySat_( const Detector * const pDetector, ... ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_IsCriticalGapBeforeVehicle_
 *
 * Description: Determines whether there was a critical gap
 *				before the given vehicle arrived. Such a gap
 *				is used to indicate the end of saturation flow.
 *
 * Author: ihenderson
 *
 * Date: 25/11/02
 *
 * Params:	Detector *pDetector - detector to check
 *			Lane *pLane - lane detector is in
 *			int16 n16ArrivingVehicleDetStateChangeNum - det state change
 *				corres. to the arrival of the vehicle before which we
 *				want to check for a critical gap.
 *
 * Return: LOGIC lIsCriticalGap - whether a critical gap was found or not
 *
 ************************************************************************/
static LOGIC MovaSatFlow_IsCriticalGapBeforeVehicle_( const Detector * const pDetector,
													  const Lane * const pLane,
													  const int16 n16ArrivingVehicleDetStateChangeNum )
{
	LOGIC		lIsCriticalGap;
	int16		n16GapTime;
	int16		n16LastDetStateChangeNum;
	int16		n16VehicleDetArrivalTime;
	int16		n16VehicleDetDepartureTime;
	REAL		rLongSlowVehicleDetOccupancyTime;
	REAL		rDetOccupancyTime;
	
	NULL_POINTER_CHECK( SF, pDetector );
	NULL_POINTER_CHECK( SF, pLane );

	/* Calculate the gap preceding this vehicle, from when the previous vehicle left */
	n16GapTime = MovaSatFlowStats_GetTransitionTime( n16ArrivingVehicleDetStateChangeNum, pDetector->n16DetNo ) 
		 - MovaSatFlowStats_GetTransitionTime( ( n16ArrivingVehicleDetStateChangeNum - 1), pDetector->n16DetNo );

	/* Get the number of the last recorded state change */
	n16LastDetStateChangeNum = pDetector->n16DetChangeCount - 1;

	/* If the gap is equal to or larger than the sat flow measurement critical gap */
	if ( n16GapTime >= pLane->n16MeasurementCriticalGapTime )
	{
		/* If this is the last recorded state change */
		if ( n16ArrivingVehicleDetStateChangeNum == n16LastDetStateChangeNum )
		{
			/* Label this as a critical gap */
			lIsCriticalGap = TRUE;
		}

		/* Else try to determine whether this is a long vehicle - if it is,
		   we use the (usually) more generous MOVA control critical gap */
		else
		{
			/* Get the times the vehicle arrived at and left the detector */
			n16VehicleDetArrivalTime = MovaSatFlowStats_GetTransitionTime( n16ArrivingVehicleDetStateChangeNum, pDetector->n16DetNo ); 
			n16VehicleDetDepartureTime = MovaSatFlowStats_GetTransitionTime( (n16ArrivingVehicleDetStateChangeNum + 1), pDetector->n16DetNo );


			/* Get the detector occupancy time for this vehicle */
			rDetOccupancyTime = (REAL)( n16VehicleDetDepartureTime - 
				n16VehicleDetArrivalTime );

			/* Estimate a minimum time for detector occupancy for long/slow vehicles */
			rLongSlowVehicleDetOccupancyTime = pDetector->rDetMeanCarOccupancyTime * 
				gSatData.rLongSlowVehicleDetOccupanyFactor;

			/* If the vehicle took less than the estimated detector occupancy time for 
			   long or slow vehicles */
			if ( rDetOccupancyTime < rLongSlowVehicleDetOccupancyTime )
			{
				/* The gap was not a result of the vehicle being slow moving, 
				   (probably a car) so label this as a critical gap */
				lIsCriticalGap = TRUE;	
			}

			/* Else vehicle took an abnormal time to cross the detector */
			else
			{
				/* If the gap is equal to or larger than the MOVA control critical gap 
				   (this is designed to take into account larger/slower vehicles) */
				if ( n16GapTime >= pLane->n16ControlCriticalGapTime )
				{
					lIsCriticalGap = TRUE;
				}

				/* Else gap is shorter than MOVA control critical gap */
				else
				{

					lIsCriticalGap = FALSE;
				}

			} /* Else vehicle took an abnormal time to cross the detector */

		} /* Else try to determine whether this is a long vehicle */

	} /* If the gap is equal to or larger than the sat flow measurement critical gap */

	/* Else gap is shorter than measurement critical gap */
	else
	{
		/* No critical gap found */
		lIsCriticalGap = FALSE;
	}

	return ( lIsCriticalGap );

} /* static LOGIC MovaSatFlow_IsCriticalGapBeforeVehicle_( const Detector * const pDetector ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_IsSignificantGapBeforeFirstSatVehicle_
 *
 * Description: Determines whether there was a significant gap 
 *				between any vehicle from the first departing the
 *				detector to the first at saturation flow at that
 *				detector. This is check is necessary for the
 *				X-detector short queue method.
 *
 * Author: ihenderson
 *
 * Date: 05/02/03
 *
 * Params:	Detector *pDetector - detector to check
 *			int16 n16FirstSatVehDetStateChangeNum - det state change
 *				corres. to the arrival of the first vehicle at 
 *				saturation flow at the given detector.
 *
 * Return: LOGIC lIsSignificantGap - whether a significant gap was found or not
 *
 ************************************************************************/
static LOGIC MovaSatFlow_IsSignificantGapBeforeFirstSatVehicle_( const Detector * const pDetector,
																 const int16 n16FirstSatVehDetStateChangeNum )
{
	LOGIC	lIsSignificantGap;
	int16	n16FirstVehArrival;
	int16	n16VehArrivalLoop;
	int16	n16GapTime;

	/* Assume failure */
	lIsSignificantGap = FALSE;

	/* Determine the detector state change of the first vehicle to arrive 
	   at the detector during green */
	n16FirstVehArrival = MovaSatFlow_GetFirstVehicleArrival_( pDetector );
	KDEBUG_EVAL_EXPRESSION( SF, n16FirstVehArrival != gn16DET_STATE_CHANGE_NUM_INVALID );

	/* If this was the first vehicle arrival recorded 
	   (i.e. no vehicle over X-det at start of green) */
	if ( n16FirstVehArrival == 0 )
	{
		/* We can't check the size of the gap before the first arrival
		   if it is also the first detector state change recorded. In this
		   case, we don't need to check the gap in advance of it anyway 
		   because we know this vehicle must have been queueing behind the 
		   X-detector before it started moving - it must have crossed the 
		   IN-detector before the start of green (INQueue must be TRUE) */
		n16FirstVehArrival = 2;
	}

	/* For each vehicle arrival from the very first to the first at sat flow */
	for ( n16VehArrivalLoop = n16FirstVehArrival; 
		  n16VehArrivalLoop < n16FirstSatVehDetStateChangeNum;
		  n16VehArrivalLoop += 2 );
	{
		/* Calculate the size of the gap preceding the arrival */
		n16GapTime =  MovaSatFlowStats_GetTransitionTime(n16VehArrivalLoop, pDetector->n16DetNo)  - 
			MovaSatFlowStats_GetTransitionTime((n16VehArrivalLoop - 1), pDetector->n16DetNo);
		
		/* If this gap is significant */
		if ( n16GapTime >= gn16STLOST_VEHICLE_SIGNIFICANT_GAP_TIME )
		{
			/* Found a significant gap (therefore short queue method inappropriate) */
			lIsSignificantGap = TRUE;
			
			/* Quit loop */
			n16VehArrivalLoop = n16FirstSatVehDetStateChangeNum;

		} /* If this gap is significant */

	} /* For each vehicle arrival from the very first to the first at sat flow */

	return ( lIsSignificantGap );

} /* static LOGIC MovaSatFlow_IsSignificantGapBeforeFirstSatVehicle_( const Detector * const pDetector, ... ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_UpdateMeanDetOccupancy_
 *
 * Description: Updates the running mean time that vehicles are taking
 *				to cross the given detector in the given lane,
 *				combining each successive detector occupancy value 
 *				using exponential smoothing. Vehicles are only counted
 *				from the end of variable minimum green (X-dets) or the
 *				end of STLOST time (stop-line dets) to start of 
 *				leaving amber.
 *
 * Author: ihenderson
 *
 * Date: 27/11/02
 *
 * Params:	Detector *pDetector - detector to calculate occupancy for
 *			Lane *pLane - lane detector is in
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlow_UpdateMeanDetOccupancy_( const Lane * const pLane,
												 Detector * const pDetector )
{
	int16		n16StartTime;
	int16		n16EndTime;
	int16		n16StartDetTransitionNum;
	int16		n16EndDetTransitionNum;
	int16		n16DetTransitionLoop;
	REAL		rDetOccupancyTime;
	REAL		rLongSlowVehicleDetOccupancyTime;
	REAL		rNewDetMeanCarOccupancyTime;
	
	NULL_POINTER_CHECK( SF, pDetector );
	NULL_POINTER_CHECK( SF, pLane );

	/* Set start and end times to measure detector occupancy */
	switch ( pDetector->n16DetType )
	{
		case DET_TYPE_OUT_DET:
		case DET_TYPE_STOP_DET:
			
			/* Traffic is assumed to be moving freely over stop-line detectors from 
			   the end of control STLOST to leaving amber start time. We don't
			   want to be looking from the start of green because of startup time lost.
			   Note: STLOST isn't a 'real' time, but we use it here as if it was
			   - queried with Keith Wood (3/12/02) as I was concerned that traffic
			   wouldn't be moving freely after STLOST time, as in reality 
			   vehicles don't start moving at exactly saturation flow rate after 
			   STLOST time as in the model (ihenderson). */
			n16StartTime = pLane->n16ControlSTLOSTTime;
			n16EndTime = pLane->n16LeavingAmberStartTime;
			break;

		case DET_TYPE_X_DET:
			
			/* Traffic is assumed to be moving freely over X detectors from 
			   the end of variable minimum green (the time calculated by MOVA
			   to clear the queue between the X-detector and stop-line)
			   to leaving amber start time */
			n16StartTime = pLane->n16VarMinGreenTime;
			n16EndTime = pLane->n16LeavingAmberStartTime;
			
			/* No - potential problem of vehicles in front stopping 
			   early in leaving amber and slowing others down 
			   - changed after consultation with Keith Wood (ihenderson) */
			/*n16EndTime = pLane->n16RedStartTime;*/ 
			break;

		default:
			/* Should only call this function for the detector types above */
			KDEBUG_WRITE1( DEBUG_LEVEL_ERROR, SF, " MovaSatFlow_UpdateMeanDetOccupancy_ bad enum = %d",pDetector->n16DetType );		
			n16StartTime = gn16TIME_INVALID;
			n16EndTime = gn16TIME_INVALID;
			break;

	} /* switch ( pDetector->n16DetType ) */

	/* Get the vehicle arrival detector transition first after the start time
	   and the vehicle departure transition last before the end time */
	n16StartDetTransitionNum = MovaSatFlow_GetFirstArrivalAfterTime_( 
		pDetector, n16StartTime );
	n16EndDetTransitionNum = MovaSatFlow_GetLastDepartureBeforeTime_( 
		pDetector, n16EndTime );

	/* If an arrival and departure occurred between the given times */
	if ( ( n16StartDetTransitionNum != gn16DET_STATE_CHANGE_NUM_INVALID ) &&
		 ( n16EndDetTransitionNum != gn16DET_STATE_CHANGE_NUM_INVALID ) )
	{
		/* For each vehicle arrival transition up to the last within the time frame */
		for ( n16DetTransitionLoop = n16StartDetTransitionNum;	/* An ON-transition */
			  n16DetTransitionLoop < n16EndDetTransitionNum;	/* An OFF-transition */
			  n16DetTransitionLoop += 2 )
		{
			/* Get the occupancy time between the vehicle's departure and arrival */
			rDetOccupancyTime = (REAL)(MovaSatFlowStats_GetTransitionTime((n16DetTransitionLoop + 1), pDetector->n16DetNo) 
				 - MovaSatFlowStats_GetTransitionTime((n16DetTransitionLoop), pDetector->n16DetNo));
	
			/* Calculate the detector occupancy time for long/slow vehicles */
			rLongSlowVehicleDetOccupancyTime = pDetector->rDetMeanCarOccupancyTime * 
				gSatData.rLongSlowVehicleDetOccupanyFactor;
	
			/* If the vehicle crossed the detector fast enough not to be a long/slow vehicle */
			if ( rDetOccupancyTime < rLongSlowVehicleDetOccupancyTime )
			{
				/* Calculate the new mean car occupancy time using exponential smoothing */
				rNewDetMeanCarOccupancyTime = (REAL)( ( gSatData.rDetOccupancySmoothingFactor *
					rDetOccupancyTime ) + ( ( 1.0 - gSatData.rDetOccupancySmoothingFactor ) *
					pDetector->rDetMeanCarOccupancyTime	) );
				
				/* Clamp the new mean to a minimum of half a second (in case there are large
				   numbers of free flowing vehicles at a high speed site) */
				pDetector->rDetMeanCarOccupancyTime = (REAL)( Math_Max( 5.0, 
					rNewDetMeanCarOccupancyTime ) );
	
			} /* If the vehicle crossed the detector fast enough not to be a long/slow vehicle */
	
		} /* For each vehicle arrival transition up to the last within the time frame */

	} /* If an arrival and departure occurred between the given times */

} /* static void MovaSatFlow_UpdateMeanDetOccupancy_( const Lane * const pLane, ... ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetFirstArrivalAfterTime_
 *
 * Description: Finds and returns the first detector ON transition
 *				(representing the arrival of a vehicle) to occur
 *				at or after the given time at the given detector.
 *
 * Author: ihenderson
 *
 * Date: 27/11/02
 *
 * Params:	Detector *pDetector - detector to check
 *			int16 n16Time - time to search to
 *
 * Return: void
 *
 ************************************************************************/
static int16 MovaSatFlow_GetFirstArrivalAfterTime_( const Detector * const pDetector,
												   const int16 n16Time )
{
	int16		n16FirstVehArrivalDetTransition;
	int16		n16LastVehDepartureDetTransition;
	int16		n16DetTransitionLoop;
	int16		n16FirstVehArrivalAfterTimeDetTransition;
	
	NULL_POINTER_CHECK( SF, pDetector );
	KDEBUG_EVAL_EXPRESSION( SF, n16Time >= 0 );

	/* Assume failure */
	n16FirstVehArrivalAfterTimeDetTransition = gn16DET_STATE_CHANGE_NUM_INVALID;

	/* Get the first recorded vehicle arrival (since start of green) 
	   and the last recorded departure */
	n16FirstVehArrivalDetTransition = MovaSatFlow_GetFirstVehicleArrival_( pDetector );
	n16LastVehDepartureDetTransition = MovaSatFlow_GetLastVehicleDeparture_( pDetector );

	/* If first and last vehicles state transitions were found */
	if ( ( n16FirstVehArrivalDetTransition != gn16DET_STATE_CHANGE_NUM_INVALID ) &&
		 ( n16LastVehDepartureDetTransition != gn16DET_STATE_CHANGE_NUM_INVALID ) )
	{
		/* Search linearly through the arrivals from the first to the last with 
		   an associated departure to find the first occurring from the given time */
		for ( n16DetTransitionLoop = n16FirstVehArrivalDetTransition;
			  n16DetTransitionLoop < n16LastVehDepartureDetTransition;
			  n16DetTransitionLoop += 2 )
		{
			/* If this transition occurred from the given time */
			if ( MovaSatFlowStats_GetTransitionTime(n16DetTransitionLoop, pDetector->n16DetNo)  >= n16Time )
			{
				/* Record transition */
				n16FirstVehArrivalAfterTimeDetTransition = n16DetTransitionLoop;
	
				/* Quit loop */
				n16DetTransitionLoop = n16LastVehDepartureDetTransition;
	
			} /* If this transition occurred from the given time */
	
		} /* For each arrival transition from the first to the last with an 
			 associated departure transition */
	
	} /* If first and last vehicles state transitions were found */

	return ( n16FirstVehArrivalAfterTimeDetTransition );

} /* static int16 MovaSatFlow_GetFirstArrivalAfterTime_( const Detector * const pDetector, ... ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetLastDepartureBeforeTime_
 *
 * Description: Finds and returns the last detector OFF transition
 *				(representing the departure of a vehicle) to occur
 *				at or before the given time at the given detector.
 *
 * Author: ihenderson
 *
 * Date: 27/11/02
 *
 * Params:	Detector *pDetector - detector to check
 *			int16 n16Time - time to search to
 *
 * Return: void
 *
 ************************************************************************/
static int16 MovaSatFlow_GetLastDepartureBeforeTime_( const Detector * const pDetector,
													 const int16 n16Time )
{
	int16		n16FirstVehArrivalDetTransition;
	int16		n16LastVehDepartureDetTransition;
	int16		n16DetTransitionLoop;
	int16		n16LastVehDepartureBeforeTimeDetTransition;
	
	NULL_POINTER_CHECK( SF, pDetector );
	KDEBUG_EVAL_EXPRESSION( SF, n16Time >= 0 );

	/* Assume failure */
	n16LastVehDepartureBeforeTimeDetTransition = gn16DET_STATE_CHANGE_NUM_INVALID;

	/* Get the first recorded vehicle arrival (since start of green) 
	   and the last recorded departure */
	n16FirstVehArrivalDetTransition = MovaSatFlow_GetFirstVehicleArrival_( pDetector );
	n16LastVehDepartureDetTransition = MovaSatFlow_GetLastVehicleDeparture_( pDetector );

	/* If first and last vehicles state transitions were found */
	if ( ( n16FirstVehArrivalDetTransition != gn16DET_STATE_CHANGE_NUM_INVALID ) &&
		 ( n16LastVehDepartureDetTransition != gn16DET_STATE_CHANGE_NUM_INVALID ) )
	{
		/* Search linearly through the departures from the last to the first with 
		   an associated arrival to find the first occurring from the given time */
		for ( n16DetTransitionLoop = n16LastVehDepartureDetTransition;
			  n16DetTransitionLoop > n16FirstVehArrivalDetTransition;
			  n16DetTransitionLoop -= 2 )
		{
			/* If this transition occurred up to the given time */
//			if ( pDetector->n16DetStateChangeTime_[ n16DetTransitionLoop ] <= n16Time )
			if ( MovaSatFlowStats_GetTransitionTime(n16DetTransitionLoop, pDetector->n16DetNo)  <= n16Time )
			{
				/* Record transition */
				n16LastVehDepartureBeforeTimeDetTransition = n16DetTransitionLoop;
	
				/* Quit loop */
				n16DetTransitionLoop = n16FirstVehArrivalDetTransition;
	
			} /* If this transition occurred up to the given time */
	
		} /* For each departure transition from the last to the first with an 
			 associated arrival transition */

	} /* If first and last vehicles state transitions were found */

	return ( n16LastVehDepartureBeforeTimeDetTransition );

} /* static int16 MovaSatFlow_GetLastDepartureBeforeTime_( const Detector * const pDetector, ... ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetFirstVehicleArrival_
 *
 * Description: Gets the first recorded vehicle arrival 
 *				detector state transition, or -1 if there wasn't one.
 *
 * Author: ihenderson
 *
 * Date: 27/11/02
 *
 * Params:	Detector *pDetector - detector to get transition from 
 *
 * Return: int16 n16FirstArrivalDetTransitionNum
 *
 ************************************************************************/
static int16 MovaSatFlow_GetFirstVehicleArrival_( const Detector * const pDetector )
{
	int16	n16FirstArrivalDetTransitionNum;

	NULL_POINTER_CHECK( SF, pDetector );

	/* Assume failure */
	n16FirstArrivalDetTransitionNum = gn16DET_STATE_CHANGE_NUM_INVALID;

	/* If there was a first state */
	if ( pDetector->n16DetChangeCount > 0 )
	{	
		/* If state was ON transition */
		if ( MovaSatFlowStats_GetTransitionState(0, pDetector->n16DetNo)  == DET_STATE_ON )
		{
			n16FirstArrivalDetTransitionNum = 0;
		}
		
		/* Else try second state */
		else
		{
			/* If there was a second state */
			if ( pDetector->n16DetChangeCount > 1 )
			{
				n16FirstArrivalDetTransitionNum = 1;
			}

		} /* Else try second state */

	} /* If there was a first state */

	return ( n16FirstArrivalDetTransitionNum );

} /* static int16 MovaSatFlow_GetFirstVehicleArrival_( const Detector * const pDetector ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetLastVehicleDeparture_
 *
 * Description: Gets the last recorded vehicle departure 
 *				detector state transition, or -1 if there wasn't one.
 *
 * Author: ihenderson
 *
 * Date: 27/11/02
 *
 * Params:	Detector *pDetector - detector to get transition from 
 *
 * Return: int16 n16LastDepartureDetTransitionNum
 *
 ************************************************************************/
static int16 MovaSatFlow_GetLastVehicleDeparture_( const Detector * const pDetector )
{
	int16	n16LastDepartureDetTransitionNum;
	int16	n16LastDetTransitionNum;

	NULL_POINTER_CHECK( SF, pDetector );

	/* Assume failure */
	n16LastDepartureDetTransitionNum = gn16DET_STATE_CHANGE_NUM_INVALID;

	/* Get the last detector state */
	n16LastDetTransitionNum = pDetector->n16DetChangeCount - 1;

	/* If there was a last state */
	if ( n16LastDetTransitionNum >= 0 )
	{
		/* If state was OFF transition */
		if ( MovaSatFlowStats_GetLastTransitionState(pDetector->n16DetNo) == DET_STATE_OFF )
		{
			n16LastDepartureDetTransitionNum = n16LastDetTransitionNum;
		}

		/* Else try second from last transition */
		else
		{			
			/* If there was a second from last state */
			if ( n16LastDetTransitionNum >= 1 )
			{
				n16LastDepartureDetTransitionNum = n16LastDetTransitionNum - 1;
			}

		} /* Else try second from last transition */

	} /* If there was a last state */

	return ( n16LastDepartureDetTransitionNum );

} /* static int16 MovaSatFlow_GetLastVehicleDeparture_( const Detector * const pDetector ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_IsXDetOnFromGreenStart_
 *
 * Description: Determines whether the given X-detector was on 
 *				from start of green to a set time, enough to 
 *				imply a queue over the detector. Note: this 
 *				function would work equally well for checking
 *				for queues at start of green at other types of det.
 *
 * Author: ihenderson
 *
 * Date: 28/11/02
 *
 * Params:	Detector *pDetector - (X)detector to check
 *			Lane *pLane - lane the detector is in
 *
 * Return: LOGIC lIsXStandingQueue - whether there's a queue or not
 *
 ************************************************************************/
static LOGIC MovaSatFlow_IsXDetOnFromGreenStart_( const Detector * const pDetector, 
												  const Lane * const pLane )
{
	LOGIC		lIsXStandingQueue;
	int16		n16QueueTime;

	NULL_POINTER_CHECK( SF, pDetector );
	NULL_POINTER_CHECK( SF, pLane );

	KDEBUG_EVAL_EXPRESSION( SF, MovaSatFlowStats_GetTransitionTime(0, pDetector->n16DetNo) >= pLane->n16GreenStartTime );

	/* Was the first transition from start of green a vehicle leaving? */
	if ( MovaSatFlowStats_GetTransitionState(0, pDetector->n16DetNo) == DET_STATE_OFF )
	{
		/* Calculate the time the detector must be on until to indicate
		   a standing queue at this detector */
		n16QueueTime = pLane->n16GreenStartTime + gn16STANDING_QUEUE_DURATION;
		
		/* If the detector was on for sufficiently long before it changed to OFF */
		if ( MovaSatFlowStats_GetTransitionTime(0, pDetector->n16DetNo) >= n16QueueTime )
		{
			/* There was a standing queue over the detector at start of green */
			lIsXStandingQueue = TRUE;
		}

		/* Else the detector didn't stay on for long enough */
		else
		{
			lIsXStandingQueue = FALSE;
		}

	} /* if ( pDetector->n16DetStateNew_[ 0 ] == DET_STATE_OFF ) */

	/* Else first transition was to ON */
	else
	{
		/* No vehicle over detector at start of green */
		lIsXStandingQueue = FALSE;
	}

	return ( lIsXStandingQueue );

} /* static LOGIC MovaSatFlow_IsXDetOnFromGreenStart_( const Detector * const pDetector, ... ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_FindDetectorType_
 *
 * Description: Given a detector number, look up its type.
 *				Disregards detector types that aren't used
 *				by the module.
 *
 * Author: ihenderson
 *
 * Date: 21/11/02
 *
 * Params: int16 n16DetNo - identification number of the detector
 *
 * Return: DET_TYPE	n16DetType - detector type
 *
 ************************************************************************/
static DET_TYPE MovaSatFlow_FindDetectorType_( const int16 n16DetNo )
{
	DET_TYPE	n16DetType;
	int16		n16LaneLoop;
	Lane		*pLane;
	
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16DetNo, 0, gSatData.n16DetsNum - 1 ) );

	/* Assume failure */
	n16DetType = DET_TYPE_INVALID;

	/* For each lane */
	for ( n16LaneLoop = 0; n16LaneLoop < gSatData.n16LanesNum; ++n16LaneLoop )
	{		
		pLane = &( gSatData.Lane_[ n16LaneLoop ] );
		
		/* See if the given detector matches one of the detectors 
		   of the types we're interested in, in this lane (OUT, STOP, X, IN) */

		if ( pLane->n16Dets_[ DET_TYPE_OUT_DET ] == n16DetNo )
		{
			n16DetType = DET_TYPE_OUT_DET;

			/* Prepare to quit loop */
			n16LaneLoop = gSatData.n16LanesNum;
		}
		else
		{
			if ( pLane->n16Dets_[ DET_TYPE_STOP_DET ] == n16DetNo )
			{
				n16DetType = DET_TYPE_STOP_DET;

				/* Prepare to quit loop */
				n16LaneLoop = gSatData.n16LanesNum;
			}
			else
			{
				if ( pLane->n16Dets_[ DET_TYPE_X_DET ] == n16DetNo )
				{
					n16DetType = DET_TYPE_X_DET;

					/* Prepare to quit loop */
					n16LaneLoop = gSatData.n16LanesNum;
				}
				else
				{
					if ( pLane->n16Dets_[ DET_TYPE_IN_DET ] == n16DetNo )
					{
						n16DetType = DET_TYPE_IN_DET;

						/* Prepare to quit loop */
						n16LaneLoop = gSatData.n16LanesNum;
					}

				} /* Else not OUT, STOP or X det */

			} /* Else not OUT or STOP det */

		} /* Else not OUT det */

	} /* For each lane */

	return ( n16DetType );

} /* static DET_TYPE MovaSatFlow_FindDetectorType_( const int16 n16DetNo ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_CalcMeasurementCritGap_
 *
 * Description: Calculates the critical gap to be used for saturation
 *				flow estimation purposes. This is calculated by adding a scaled
 *				proportion of the critical gap increment to the mean
 *				saturation gap (time from rear of one car to front of next,
 *				both travelling at saturation flow).
 *
 * Author: ihenderson
 *
 * Date: 26/11/02
 * Modified: 05/02/03 so that the critical gap increment is scaled, 
 *			 instead of satinc.
 *
 * Params: int16 n16MeanVehicleSatGapTime - average time from rear of 
 *				one car to the front of the next travelling at 
 *				saturation flow (tenths of secs)
 *				(Ccomshr.satgap in MOVA)
 *		   REAL rCritGapScalingFactor - scaling factor for critical gap increment
 *
 * Return: int16 n16MeasurementCriticalGapTime - time to be used as the
 *				critical gap for sat. flow estimation purposes (tenths of secs)
 *
 ************************************************************************/
static int16 MovaSatFlow_CalcMeasurementCritGap_( const int16 n16MeanVehicleSatGapTime, 
												  const REAL rCritGapScalingFactor )
{
	int16	n16MeasurementCriticalGapTime;
	int16	n16CriticalGapIncrementScaled;

	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( rCritGapScalingFactor, 
		grCRITICAL_GAP_SCALING_FACTOR_MIN, grCRITICAL_GAP_SCALING_FACTOR_MAX ) );

	n16CriticalGapIncrementScaled = (int16)( rCritGapScalingFactor * 
		( (REAL)gn16CONTROL_CRITICAL_GAP_INCREMENT ) );

	n16MeasurementCriticalGapTime = n16MeanVehicleSatGapTime + 
		n16CriticalGapIncrementScaled;

	return ( n16MeasurementCriticalGapTime );

} /* static int16 MovaSatFlow_CalcMeasurementCritGap_( const int16 n16MeanVehicleSatGapTime, ... ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_DetectorReset_
 *
 * Description: Resets the current detector state and 
 *				number of recorded states/times. 
 *				Should be called at the start of green.
 *
 * Author: ihenderson
 *
 * Date: 29/11/02
 *
 * Params: Lane *pLane - lane
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlow_DetectorReset_( const Lane * const pLane )
{
	Detector	*pDetector;
	int16		n16DetNo;

	NULL_POINTER_CHECK( SF, pLane );

	/* Get the detector used for sat flow estimation in this lane */
	n16DetNo = pLane->n16SatFlowDetNo;
	pDetector = &( gSatData.Detector_[ n16DetNo ] );

	/* Delete detector transitions */
	MovaSatFlowStats_DeleteDetectorTransitions(pDetector->n16DetNo);

	/* Reset number of detector state changes */
	pDetector->n16DetChangeCount = 0;
	
	/* Set current state of detector to MOVA state */
	pDetector->n16DetStateCurrent = (DET_STATE)(input_data.dets_on_flag[ n16DetNo ] );
	KDEBUG_EVAL_EXPRESSION( SF, ( pDetector->n16DetStateCurrent == DET_STATE_OFF ) ||
				  ( pDetector->n16DetStateCurrent == DET_STATE_ON ) );

} /* static void MovaSatFlow_DetectorReset_( const Lane * const pLane ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_DetectorUpdate_
 *
 * Description: Updates the MovaSatFlow module detector states and 
 *				transition times. Should be called during green and 
 *				five seconds into red.
 *
 * Author: ihenderson
 *
 * Date: 29/11/02
 *
 * Params: Lane *pLane - lane
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlow_DetectorUpdate_( const Lane * const pLane )
{
	Detector	*pDetector;
	int16		n16DetNo;
	int16		n16DetChangeNumNew;
	DET_STATE	n16CurrentDetState;
	DET_STATE	n16LastDetState;

	NULL_POINTER_CHECK( SF, pLane );

	/* Get the detector this lane uses for sat flow estimation */
	n16DetNo = pLane->n16SatFlowDetNo;
	pDetector = &( gSatData.Detector_[ n16DetNo ] );

	/* Get the last recorded det state and current MOVA det state */
	n16LastDetState = pDetector->n16DetStateCurrent;
	n16CurrentDetState = (DET_STATE)( input_data.dets_on_flag[ n16DetNo ] );
	KDEBUG_EVAL_EXPRESSION( SF, ( n16LastDetState == DET_STATE_ON ) || 
				  ( n16LastDetState == DET_STATE_OFF ) );
	KDEBUG_EVAL_EXPRESSION( SF, ( n16CurrentDetState == DET_STATE_ON ) || 
				  ( n16CurrentDetState == DET_STATE_OFF ) );

	/* If the MOVA detector state has changed */
	if ( n16LastDetState != n16CurrentDetState )
	{	
		n16DetChangeNumNew = pDetector->n16DetChangeCount;
		KDEBUG_EVAL_EXPRESSION( SF, n16DetChangeNumNew < gnMAX_TRANSITIONS );

		if ( n16DetChangeNumNew < gnMAX_TRANSITIONS-1 )
		{
			TransitionItem newTransition;

			newTransition.n16DetNo = (uint8) n16DetNo;
			newTransition.n16DetStateChangeTime_ = pLane->n16StageTime;
			newTransition.n16DetStateNew_ = n16CurrentDetState;

			MovaSatFlowStats_AddDetectorTransition(newTransition);

			/* Update number of transitions */
			pDetector->n16DetChangeCount++; 

		} /* If there is still space in the arrays */

		/* Update current state */
		pDetector->n16DetStateCurrent = n16CurrentDetState;

	} /* if ( n16LastDetState != n16CurrentDetState ) */

} /* static void MovaSatFlow_DetectorUpdate_( const Lane * const pLane ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_ResetAllVariables_
 *
 * Description: Resets all SatData struct variables on (re)initialisation
 *				of the module. Some of these are just zeroed, some (e.g.
 *				concerning the physical layout of the junction) are set
 *				from the MOVA data, some are set to specified defaults.
 *				TODO: ideally, split this into separate ResetAllLaneVars etc. 
 *				functions.
 *
 * Author: ihenderson
 *
 * Date: 18/11/02
 *
 * Params: void
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlow_ResetAllVariables_( void )
{
	TIMESTAMP_TYPE					tTimeDate;
	uint8							n16LaneLoop;
	uint8							n16DetLoop;
	uint8							n16DetTypeLoop;
	Lane							*pLane;
	Detector						*pDetector;

	/* Get total numbers of parts of the junction */
	gSatData.n16DetsNum = (int16)DI_get_dets_count();
	gSatData.n16LanesNum = (int16)DI_get_lanes_count(); 
	gSatData.n16LinksNum = (int16)DI_get_links_count();
	gSatData.n16StagesNum = (int16)DI_get_stages_count();

	/* Set the exponential smoothing factors for combining successive sat flow 
	   estimates, STLOST estimates, and detector occupancies to a default value */
	gSatData.rSatFlowSmoothingFactor = grSAT_FLOW_SMOOTHING_FACTOR_DEFAULT;
	gSatData.rSTLOSTSmoothingFactor = grSTLOST_SMOOTHING_FACTOR_DEFAULT;
	gSatData.rDetOccupancySmoothingFactor = grDET_OCCUPANCY_SMOOTHING_FACTOR_DEFAULT;

	/* Set the critical gap scaling factor to a default */
	gSatData.rCriticalGapFactor = grCRITICAL_GAP_FACTOR_DEFAULT;

	/* Set the factor used to scale mean (car) detector occupancy
	   to give an estimate of occupancy time for long/slow vehicles to a default */
	gSatData.rLongSlowVehicleDetOccupanyFactor = 
		grHEAVIES_DET_OCC_FACTOR_DEFAULT;

	/* Set the number of vehicles to be disregarded for saturation flow estimation
	   purposes due to start up time lost at stop-line and X detectors */
	gSatData.n16StoplineSTLOSTVehiclesNum = gn16STOPLINE_STLOST_VEHICLES_NUM_DEFAULT;
	gSatData.n16XDetSTLOSTVehiclesNum = gn16XDET_STLOST_VEHICLES_NUM_DEFAULT;

	/* Set the maximum allowed percentage difference between STOPLINE and 
	   X-detector flow rates */
	gSatData.rFlowRateProportionalErrorMax = grFLOW_RATE_PROPORTIONAL_ERROR_MAX_DEFAULT;

	/* For each lane */
	for ( n16LaneLoop = 0; n16LaneLoop < gSatData.n16LanesNum; ++n16LaneLoop )
	{
		pLane = &( gSatData.Lane_[ n16LaneLoop ] );
		
		/* Record this lane's number (0-based) */
		pLane->n16LaneNo = n16LaneLoop;

		/* No queue from traffic counted crossing IN detector during red */
		pLane->lINQueue = FALSE;

		/* Reset the number of times start of green has been detected */
		pLane->n16StartOfGreenCount = 0;
		
		/* Reset lane times */
		tTimeDate = Com_read_rtc( READ_RTC);
		pLane->n16StageTime = 0;

		pLane->n16GreenEffStartTime = 0;
		pLane->n16GreenStartTime = 0;

		/* Actual variable minimum green time */
		pLane->n16VarMinGreenTime = 0;
		
		/* Theoretical maximum variable minimum green time */
		pLane->n16MaxVarMinGreenTime = (int16)DI_get_max_min_green_time(n16LaneLoop+1);

		pLane->n16LeavingAmberStartTime = 0;
		pLane->n16RedStartTime = 0;

		/* Set lane state to red */
		pLane->n16CurrentState = LANE_STATE_LATE_RED;

		/* Calculate the max no. of whole cars that can fit from X-det to stop-line */
		pLane->n16XDetQueueSizeMax = MovaSatFlow_GetXDetQueueSizeMax_( (int16)n16LaneLoop );

		/* Number of vehicles between X-det and stop-line at start of green */
		pLane->n16XDetQueueSizeCurrent = 0;

		/* For each detector type */
		for ( n16DetTypeLoop = DET_TYPE_FIRST; n16DetTypeLoop < DET_TYPE_NUM; ++n16DetTypeLoop )
		{			
			/* Get the number of this detector (converting from 1-based to 0-based 
			   detector numbers) - this means if a detector type doesn't exist in 
			   this lane, it will have number -1 */
			pLane->n16Dets_[ n16DetTypeLoop ] = (int16)DI_get_det_num(n16LaneLoop+1, n16DetTypeLoop)-1;

		} /* for ( n16DetTypeLoop = DET_TYPE_FIRST; n16DetTypeLoop < DET_TYPE_NUM; ++n16DetTypeLoop ) */
		
		/* Get the MOVA control and measurement critical gap times for this lane 
		   (assuming critg and satinc are in tenths of seconds) */
		pLane->n16ControlCriticalGapTime = (int16)DI_get_lane_critical_gap(n16LaneLoop+1);
		pLane->n16MeasurementCriticalGapTime = MovaSatFlow_CalcMeasurementCritGap_( 
			(int16)DI_get_satflow_mean_gap(n16LaneLoop+1), gSatData.rCriticalGapFactor );

		/* Initialise control STLOST time to MOVA value */
		pLane->n16ControlSTLOSTTime = (int16)DI_get_startup_lost_time(n16LaneLoop+1) * 
			gn16HALF_SECONDS_TO_TENTHS_OF_SECONDS_FACTOR;

		/* Initialise the smoothed sat flow rate to the fixed saturation flow rate
		   being used by MOVA (satinc is the headway time between vehicles travelling
		   at saturation flow - determined from observation of the junction). */
		KDEBUG_EVAL_EXPRESSION( SF, DI_get_satinc_time(n16LaneLoop+1) > 0 ); /* DIVIDE BY ZERO */
		pLane->rSmoothedSatFlowRate = (REAL)( 1.0 / (REAL)( DI_get_satinc_time(n16LaneLoop+1) ) );
		
		/* Initialise the smoothed STLOST time to the fixed STLOST time
		   being used by MOVA for control purposes */
		pLane->rSmoothedSTLOSTTime = (REAL)( pLane->n16ControlSTLOSTTime );

		/* Initialise the smoothed flow rate to the MOVA X-detector smoothed flow rate */
		pLane->rSmoothedFlowRate = (REAL)(get_lane_smoothed_flow(n16LaneLoop) );

		/* Reset the time since last flow rate update */
		pLane->n16FlowRateUpdateTime = 0;

		/* Reset the number of vehicles detected since last flow rate update */
		pLane->n16FlowRateVehiclesNum = 0;

		/* Initialise the current sat flow estimation detector state to invalid */
		pLane->n16FlowRateDetStateCurrent = DET_STATE_INVALID;

		/* Get the cruise time for this lane from X-detector to stop-line */
		pLane->n16XDetToStoplineCruiseTime = (int16)DI_get_x_det_cruise_time(n16LaneLoop+1) *
			gn16HALF_SECONDS_TO_TENTHS_OF_SECONDS_FACTOR;

		/* No fault detected on the sat flow detector */
		pLane->lIsSatFlowDetFaulty = FALSE;

	} /* for ( n16LaneLoop = 0; n16LaneLoop < gSatData.n16LanesNum; ++n16LaneLoop ) */


	/* For each detector */
	for ( n16DetLoop = 0; n16DetLoop < gSatData.n16DetsNum; ++n16DetLoop )
	{
		pDetector = &( gSatData.Detector_[ n16DetLoop ] );

		/* Find the type of this detector */
		pDetector->n16DetType = MovaSatFlow_FindDetectorType_( n16DetLoop );

		/* If the detector was found in the lane/detector assignment array */
		if ( pDetector->n16DetType != DET_TYPE_INVALID )
		{
			/* Record this detector's number (0-based) */
			pDetector->n16DetNo = n16DetLoop;
		}
		else
		{
			/* This detector isn't an OUT, STOP, X or IN detector, 
			   or isn't being used by MOVA */
			pDetector->n16DetNo = gn16DET_NUM_INVALID;
		}

		/* Set the exit-blocked threshold and 
		   number of STLOST vehicles according to detector type */
		switch ( pDetector->n16DetType )
		{
			case DET_TYPE_OUT_DET:
			case DET_TYPE_STOP_DET:
				
				pDetector->n16DetExitBlockedThreshold = 
					gn16STOP_OUT_DET_EXIT_BLOCKED_THRESHOLD_DEFAULT;
				break;

			case DET_TYPE_X_DET:

				pDetector->n16DetExitBlockedThreshold = 
					gn16X_DET_EXIT_BLOCKED_THRESHOLD_DEFAULT;
				break;

			default:

				/* We're not interested in other detectors as far as 
				   exit-blocked threshold is concerned */
				pDetector->n16DetExitBlockedThreshold = 
					gn16EXIT_BLOCKED_THRESHOLD_INVALID;
				break;

		} /* switch ( pDetector->n16DetType ) */
		
		pDetector->n16DetChangeCount = 0;

		/* Initialise the detector state to invalid */
		pDetector->n16DetStateCurrent = DET_STATE_INVALID;

		/* Set the mean time taken for a vehicle to cross the detector to an initial value */
		pDetector->rDetMeanCarOccupancyTime = grDET_MEAN_CAR_OCCUPANCY_TIME_DEFAULT;

	} /* for ( n16DetLoop = 0; n16DetLoop < gSatData.n16DetsNum; ++n16DetLoop ) */

	/*Initialise the detector transition holding area */
	MovaSatFlowStats_InitialiseTransitionData();

	
	/* For each lane */
	for ( n16LaneLoop = 0; n16LaneLoop < gSatData.n16LanesNum; ++n16LaneLoop )
	{
		pLane = &( gSatData.Lane_[ n16LaneLoop ] );

		/* Find this lane's link number (0-based) */
		pLane->n16LinkNo = MovaSatFlow_LinkFromLane_( pLane );
		KDEBUG_EVAL_EXPRESSION( SF, pLane->n16LinkNo != gn16LINK_NUM_INVALID );
	
		/* Determine the detector to be used for saturation flow estimation 
		   in this lane */
		pLane->n16SatFlowDetNo = MovaSatFlow_GetSatFlowDetector_( pLane );
		KDEBUG_EVAL_EXPRESSION( SF, pLane->n16SatFlowDetNo != gn16DET_NUM_INVALID );

		/* Determine whether there is a valid IN-detector in this lane */
		pLane->lIsINDetValid = MovaSatFlow_IsINDetectorValid_( pLane );
	
	} /* For each lane */

	/* Initialise test output data */
	MovaSatFlowTest_TestOutputInitialise_( );

} /* static void MovaSatFlow_ResetAllVariables_( void ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetSatFlowDetector_
 *
 * Description: Determines the detector to use for saturation flow
 *				estimation in the given lane. For STOP detectors,
 *				checks that the detector is exclusive to this lane
 *				(according to lane-detector assignment array).
 *				N.B. This is not sufficient to ensure that a
 *				STOP detector is suitable for sat flow estimation.
 *
 * Author: ihenderson
 *
 * Date: 02/12/02
 *
 * Params: Lane *pLane - lane
 *
 * Return: int16 n16SatFlowDetNo - number of the detector to use
 *
 ************************************************************************/
static int16 MovaSatFlow_GetSatFlowDetector_( const Lane * const pLane )
{
	int16	n16SatFlowDetNo;
	int16	n16LaneLoop;
	Lane	*pOtherLane;

	NULL_POINTER_CHECK( SF, pLane );

	/* If there's an OUT detector */
	if ( pLane->n16Dets_[ DET_TYPE_OUT_DET ] != gn16DET_NUM_INVALID )
	{
		/* Get the OUT detector number */
		n16SatFlowDetNo = pLane->n16Dets_[ DET_TYPE_OUT_DET ];
	}

	/* Else no OUT detector */
	else
	{
		/* If there's a STOP detector */
		if ( pLane->n16Dets_[ DET_TYPE_STOP_DET ] != gn16DET_NUM_INVALID )
		{
			/* Get the STOP detector number */
			n16SatFlowDetNo = pLane->n16Dets_[ DET_TYPE_STOP_DET ];
			
			/* Check that this stop detector is exclusive according to
			   lane-detector assignments */
			
			/* For each lane */
			for ( n16LaneLoop = 0; n16LaneLoop < gSatData.n16LanesNum; ++n16LaneLoop )
			{
				/* If this is a different lane */
				if ( n16LaneLoop != pLane->n16LaneNo )
				{
					pOtherLane = &( gSatData.Lane_[ n16LaneLoop ] );
					
					/* If the stop detector is the same */
					if ( pOtherLane->n16Dets_[ DET_TYPE_STOP_DET ] == n16SatFlowDetNo )
					{
						/* Stop detector not exclusive, so use X-detector */
						n16SatFlowDetNo = pLane->n16Dets_[ DET_TYPE_X_DET ];

						/* Quit loop */
						n16LaneLoop = gSatData.n16LanesNum;
					}

				} /* If this is a different lane */

			} /* For each lane */
			
		} /* If there's a STOP detector */

		/* Else no OUT or STOP detector */
		else
		{
			/* Get the X detector number */
			n16SatFlowDetNo = pLane->n16Dets_[ DET_TYPE_X_DET ];
		}

	} /* Else no OUT detector */

	return ( n16SatFlowDetNo );

} /* static int16 MovaSatFlow_GetSatFlowDetector_( const Lane * const pLane ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_IsINDetectorValid_
 *
 * Description: Determines whether an IN-detector exists for the given
 *				lane, and if so whether it is exclusive to that lane
 *				(i.e. it isn't shared by a short lane and associated 
 *				long lane).
 *
 * Author: ihenderson
 *
 * Date: 03/01/03
 *
 * Params: Lane *pLane - lane
 *
 * Return: LOGIC lIsINDetValid - whether there is a valid IN-detector
 *
 ************************************************************************/
static LOGIC MovaSatFlow_IsINDetectorValid_( const Lane * const pLane )
{
	LOGIC	lIsINDetValid;
	int16	n16LaneLoop;
	int16	n16INDetNo;
	Lane	*pOtherLane;

	NULL_POINTER_CHECK( SF, pLane );

	n16INDetNo = pLane->n16Dets_[ DET_TYPE_IN_DET ];

	/* If an IN-detector exists for this lane */
	if ( n16INDetNo != gn16DET_NUM_INVALID )
	{
		/* Assume it is exclusive to this lane */
		lIsINDetValid = TRUE;
		
		/* Search through other lanes to see if it is shared */
		for ( n16LaneLoop = 0; n16LaneLoop < gSatData.n16LanesNum; ++n16LaneLoop )
		{
			/* If this is a different lane */
			if ( n16LaneLoop != pLane->n16LaneNo )
			{
				pOtherLane = &( gSatData.Lane_[ n16LaneLoop ] );
					
				/* If the IN detector is the same */
				if ( pOtherLane->n16Dets_[ DET_TYPE_IN_DET ] == n16INDetNo )
				{
					/* IN detector not exclusive */
					lIsINDetValid = FALSE;

					/* Quit loop */
					n16LaneLoop = gSatData.n16LanesNum;
				}

			} /* If this is a different lane */

		} /* For each lane */

	} /* If an IN-detector exists for this lane */

	/* Else no IN-detector in this lane */
	else
	{
		lIsINDetValid = FALSE;
	}

	return ( lIsINDetValid );

} /* static LOGIC MovaSatFlow_IsINDetectorValid_( const Lane * const pLane ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_FlowRateDataUpdate_
 *
 * Description: Updates the data (vehicle count and time) required
 *				to calculate the vehicle flow rate in this lane
 *				at the saturation flow estimation detector.
 *
 * Author: ihenderson
 *
 * Date: 10/12/02
 *
 * Params: Lane *pLane - lane to update flow rate data for
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlow_FlowRateDataUpdate_( Lane * const pLane )
{
	DET_STATE			n16CurrentDetState;
	DET_STATE			n16LastDetState;
	int16				n16DetNo;

	NULL_POINTER_CHECK( SF, pLane );

	/* Get the detector this lane uses for sat flow estimation */
	n16DetNo = pLane->n16SatFlowDetNo;

	/* Get the last recorded det state and current MOVA det state */
	n16LastDetState = pLane->n16FlowRateDetStateCurrent;
	n16CurrentDetState = (DET_STATE)( input_data.dets_on_flag[ n16DetNo ] );
	KDEBUG_EVAL_EXPRESSION( SF, ( n16CurrentDetState == DET_STATE_ON ) || 
				  ( n16CurrentDetState == DET_STATE_OFF ) );

	/* If there is no recorded det state */
	if ( n16LastDetState == DET_STATE_INVALID )
	{
		/* Reset the number of vehicles and update time */
		pLane->n16FlowRateUpdateTime = 0;
		pLane->n16FlowRateVehiclesNum = 0;

		/* Just record the current detector state */
		pLane->n16FlowRateDetStateCurrent = n16CurrentDetState;

	} /* If there is no recorded det state */

	/* Else there is a previous detector state */
	else
	{
		/* When flows are low, less than a car per hour  n16FlowRateUpdateTime could potentially be bigger than INT16_MAX, overflow. */
        if (pLane->n16FlowRateUpdateTime < INT16_MAX-1 )
            {
            /* Increment lane flow rate update timer */
            KDEBUG_EVAL_EXPRESSION( SF, pLane->n16FlowRateUpdateTime < INT16_MAX-1 ); /* TIMER OVERRUN */
            pLane->n16FlowRateUpdateTime++;
            }
		
		/* If the detector state has changed */
		if ( n16LastDetState != n16CurrentDetState )
		{
			/* If a vehicle has just left the detector */
			if ( n16CurrentDetState == DET_STATE_OFF )
			{
				/* Increment number of vehicles to leave detector */
				pLane->n16FlowRateVehiclesNum++;
			}

			/* Record the current detector state */
			pLane->n16FlowRateDetStateCurrent = n16CurrentDetState;
	
		} /* If the detector state has changed */

	} /* Else there is a previous detector state */

} /* static void MovaSatFlow_FlowRateDataUpdate_( Lane * const pLane ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_IsDetectorSuspect_
 *
 * Description: Determines whether the given detector is suspect
 *				according to MOVA.
 *
 * Author: ihenderson
 *
 * Date: 11/12/02
 *
 * Params: int16 n16DetNo - number of detector to check
 *
 * Return: void
 *
 ************************************************************************/
static LOGIC MovaSatFlow_IsDetectorSuspect_( const int16 n16DetNo )
{
	LOGIC	lIsDetectorSuspect;

	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16DetNo, 0, gSatData.n16DetsNum - 1 ) );

	/* Check with MOVA to see if this detector is suspect 
	   (off or on for an inordinate amount of time) */
	if ( is_det_suspected((uint8)n16DetNo))
	{
		lIsDetectorSuspect = TRUE;
	}
	else
	{
		lIsDetectorSuspect = FALSE;
	}

	return ( lIsDetectorSuspect );

} /* static LOGIC MovaSatFlow_IsDetectorSuspect_( const int16 n16DetNo ) */


/*************************
*	Public functions
**************************/


/*************************************************************************
 *
 * Function: MovaSatFlow_Initialise_
 *
 * Description: Initialises module, setting up the data in the gSatData to
 *				default values
 *
 * Author: ihenderson
 *
 * Date: 13/11/02
 *
 * Params: void
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlow_Initialise_( void )
{	
	if (glModuleInitialised)
	{
		KDEBUG_WRITE0( DEBUG_LEVEL_ERROR, SF, "MovaSatFlow already initialised!" );
	}

	KDEBUG_WRITE0( DEBUG_LEVEL_INFO, SF, "MovaSatFlow: initialising." );

	glModuleInitialised = TRUE;	

	MovaSatFlowStats_Initialise_( &gSatData);

	ShowSatFlow = MovaSatFlowStats_GetSatFlowFlagPtr_();
	/* Reset all SatData variables */
	MovaSatFlow_ResetAllVariables_( );

	
} /* void MovaSatFlow_Initialise_( void ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_ReInitialise_
 *
 * Description: Carries out any tasks necessary for reinitialisation of the 
 *				module. May be useful to set gSatData etc. back to defaults.
 *
 * Author: ihenderson
 *
 * Date: 13/11/02
 *
 * Params: void
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlow_ReInitialise_( void )
{
	if (!glModuleInitialised)
	{
		KDEBUG_WRITE0( DEBUG_LEVEL_ERROR, SF, "MovaSatFlow not initialised." );
	}

	KDEBUG_WRITE0( DEBUG_LEVEL_INFO, SF, "MovaSatFlow: reinitialising." );

	MovaSatFlowStats_ReInitialise_();

	ShowSatFlow = MovaSatFlowStats_GetSatFlowFlagPtr_();

	/* Reset all SatData variables */
	MovaSatFlow_ResetAllVariables_( );	

} /* void MovaSatFlow_ReInitialise_( void ) */

/*************************************************************************
 *
 * Function: MovaSatFlow_Update_
 *
 * Description: Stores all the data required for saturation flow
 *				estimation, including detector states and signal
 *				change times. Initiates saturation flow estimation 
 *				when necessary. Uses a state system to determine what 
 *				data to collect when and when to carry out saturation 
 *				flow estimation.
 *				Should be called every tenth of a second.
 *
 * Author: ihenderson
 *
 * Date: 14/11/02
 *
 * Params: void
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlow_Update_( void )
{
	int16		n16LaneLoop;
	Lane		*pLane;

	/* For each lane */
	for ( n16LaneLoop = 0; n16LaneLoop < gSatData.n16LanesNum; ++n16LaneLoop )
	{		
		pLane = &( gSatData.Lane_[ n16LaneLoop ] );

		/* Update whether the sat flow detector is currently working OK
		   (OR-ing ensures sat flow won't be estimated or flow rate updated 
		   at the end of cycles where the detector was faulty for all or even 
		   part of the cycle) */
		pLane->lIsSatFlowDetFaulty |= 
			MovaSatFlow_IsDetectorSuspect_( pLane->n16SatFlowDetNo );

		/* Update flow rate data (vehicle count and time) */
		MovaSatFlow_FlowRateDataUpdate_( pLane );

		/* Examine the state the lane is in and act accordingly */
		switch ( pLane->n16CurrentState )
		{
			case LANE_STATE_LATE_RED:
				
				MovaSatFlow_InLateRed_( pLane );
				break;

			case LANE_STATE_MINIMUM_GREEN:
				
				MovaSatFlow_InMinimumGreen_( pLane );
				break;

			case LANE_STATE_EXTENDED_GREEN:

				MovaSatFlow_InExtendedGreen_( pLane );
				break;
			
			case LANE_STATE_LEAVING_AMBER:

				MovaSatFlow_InLeavingAmber_( pLane );
				break;

			case LANE_STATE_EARLY_RED:

				MovaSatFlow_InEarlyRed_( pLane );

				break;

			default:

				/* Should never reach an unrecognised state */
				KDEBUG_WRITE1( DEBUG_LEVEL_ERROR, SF, " MovaSatFlow_Update_ bad enum = %d", pLane->n16CurrentState);
				break;

		} /* switch ( pLane->n16CurrentState ) */


	} /* for ( n16LaneLoop = 0; n16LaneLoop < gSatData.n16LanesNum; ++n16LaneLoop ) */


} /* static void MovaSatFlow_Update_( void ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetLane_
 *
 * Description: Gets a pointer to the lane of the given number.
 *
 * Author: ihenderson
 *
 * Date: 02/12/02
 *
 * Params: int16 n16LaneNo - lane number
 *
 * Return: Lane * - pointer to requested lane
 *
 ************************************************************************/
Lane * MovaSatFlow_GetLane_( const int16 n16LaneNo )
{

	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );

	return ( &( gSatData.Lane_[ n16LaneNo ] ) );

} /* Lane * MovaSatFlow_GetLane_( const int16 n16LaneNo ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetDetector_
 *
 * Description: Gets a pointer to the detector of the given number.
 *
 * Author: ihenderson
 *
 * Date: 21/11/02
 *
 * Params: int16 n16DetNo - detector number
 *
 * Return: Detector * - pointer to requested detector
 *
 ************************************************************************/
Detector * MovaSatFlow_GetDetector_( const int16 n16DetNo )
{

	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16DetNo, 0, gSatData.n16DetsNum - 1 ) );

	return ( &( gSatData.Detector_[ n16DetNo ] ) );

} /* Detector * MovaSatFlow_GetDetector_( const int16 n16DetNo ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetDetectorInLane_
 *
 * Description: Gets a pointer to the detector of the given type
 *				in the given lane (or NULL if not in this lane).
 *
 * Author: ihenderson
 *
 * Date: 23/12/02
 *
 * Params: Lane *pLane - lane to get the detector from.
 *		   DET_TYPE n16DetTypeNo - type of the detector to look for 
 *
 * Return: Detector * - pointer to requested detector, or NULL if invalid
 *
 ************************************************************************/
Detector * MovaSatFlow_GetDetectorInLane_( const Lane * const pLane,
										   const DET_TYPE n16DetTypeNo )
{
	Detector	*pDetector;
	int16		n16DetNo;

	NULL_POINTER_CHECK( SF, pLane );
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16DetTypeNo, DET_TYPE_FIRST, DET_TYPE_LAST ) );

	/* Get the detector number for this detector type */
	n16DetNo = pLane->n16Dets_[ n16DetTypeNo ];

	/* If a detector of this type exists */
	if ( n16DetNo != gn16DET_NUM_INVALID )
	{
		pDetector = &( gSatData.Detector_[ n16DetNo ] );
	}

	/* Else detector of this type doesn't exist */
	else
	{
		pDetector = NULL;
	}

	return ( pDetector );

} /* Detector * MovaSatFlow_GetDetectorInLane_( const Lane * const pLane, ... ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetSaturationFlowDetector_
 *
 * Description: Gets a pointer to the Detector used for saturation flow
 *				in the given Lane.
 *
 * Author: ihenderson
 *
 * Date: 16/12/02
 *
 * Params: Lane *pLane - Lane to get detector from
 *
 * Return: Detector * - pointer to sat flow detector
 *
 ************************************************************************/
Detector * MovaSatFlow_GetSaturationFlowDetector_( const Lane * const pLane )
{

	NULL_POINTER_CHECK( SF, pLane );

	return ( &( gSatData.Detector_[ pLane->n16SatFlowDetNo ] ) );

} /* Detector * MovaSatFlow_GetSaturationFlowDetector_( const Lane * const pLane ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_SetSaturationFlowDetector_
 *
 * Description: Attempts to set the detector used for saturation
 *				flow estimation in the given lane.
 *
 * Author: ihenderson
 *
 * Date: 23/12/02
 *
 * Params: Lane *pLane - lane to try to change detector in
 *		   Detector *pDetector - Detector to change sat flow det to
 *
 * Return: LOGIC lIsDetValid - whether detector is valid for this lane
 *
 ************************************************************************/
LOGIC MovaSatFlow_SetSaturationFlowDetector_( Lane * const pLane, 
											  const Detector * const pDetector )
{
	int16	n16DetNo;
	LOGIC	lIsDetValid;

	NULL_POINTER_CHECK( SF, pLane );
	NULL_POINTER_CHECK( SF, pDetector );

	/* Get the detector number for this type of detector in the lane */
	n16DetNo = pLane->n16Dets_[ pDetector->n16DetType ];

	/* If the detector number is the same as the detector number 
	   of the detector we're attempting to change to */
	if ( n16DetNo == pDetector->n16DetNo )
	{
		lIsDetValid = TRUE;
		
		/* Set the saturation flow detector for this lane */
		pLane->n16SatFlowDetNo = n16DetNo;

		MovaSatFlow_Output_Record_( gSatData.Detector_[ pLane->n16SatFlowDetNo ].n16DetType, 
			gLaneOutput_[ pLane->n16LaneNo ].n16SatFlowDetType, pLane->n16LaneNo );
	}

	/* Else the detector isn't in this lane */
	else
	{
		lIsDetValid = FALSE;
	}

	return ( lIsDetValid );

} /* LOGIC MovaSatFlow_SetSaturationFlowDetector_( Lane * const pLane, ... ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetLanesNum_
 *
 * Description: Gets the number of lanes.
 *
 * Author: ihenderson
 *
 * Date: 13/12/02
 *
 * Params: void
 *
 * Return: int16
 *
 ************************************************************************/
int16 MovaSatFlow_GetLanesNum_( void )
{

	return ( gSatData.n16LanesNum );

} /* int16 MovaSatFlow_GetLanesNum_( void ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetDetectorsNum_
 *
 * Description: Gets the number of detectors.
 *
 * Author: ihenderson
 *
 * Date: 21/11/02
 *
 * Params: void
 *
 * Return: int16
 *
 ************************************************************************/
int16 MovaSatFlow_GetDetectorsNum_( void )
{

	return ( gSatData.n16DetsNum );

} /* int16 MovaSatFlow_GetDetectorsNum_( void ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetCurrentSatFlow_
 *
 * Description: Gets the current smoothed saturation flow estimate
 *				for the given lane. 
 *
 * Author: ihenderson
 *
 * Date: 02/12/02
 *
 * Params: Lane *pLane - Lane to get sat flow estimate from
 *
 * Return: REAL - smoothed saturation flow rate (vehicles/hour)
 *
 ************************************************************************/
REAL MovaSatFlow_GetCurrentSatFlow_( const Lane * const pLane )
{

	NULL_POINTER_CHECK( SF, pLane );

	return ( pLane->rSmoothedSatFlowRate * grTENTHS_OF_SECONDS_TO_HOURS_FACTOR );
	
} /* REAL MovaSatFlow_GetCurrentSatFlow_( const Lane * const pLane ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetDetectorType_
 *
 * Description: Gets the type of the given detector.
 *
 * Author: ihenderson
 *
 * Date: 21/11/02
 *
 * Params: Detector *pDetector - Detector to get the type of
 *
 * Return: DET_TYPE
 *
 ************************************************************************/
DET_TYPE MovaSatFlow_GetDetectorType_( const Detector * const pDetector )
{

	NULL_POINTER_CHECK( SF, pDetector );

	return ( pDetector->n16DetType );

} /* DET_TYPE MovaSatFlow_GetDetectorType_( const Detector * const pDetector ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetDetectorExitBlockedThreshold_
 *
 * Description: Gets the exit blocked threshold for the specified
 *				detector
 *
 * Author: ihenderson
 *
 * Date: 13/12/02
 *
 * Params: Detector *pDetector - Detector to get the threshold from
 *
 * Return: int16 - exit-blocked threshold (tenths of seconds)
 *
 ************************************************************************/
int16 MovaSatFlow_GetDetectorExitBlockedThreshold_( const Detector * const pDetector )
{

	NULL_POINTER_CHECK( SF, pDetector );

	return ( pDetector->n16DetExitBlockedThreshold );

} /* void MovaSatFlow_GetDetectorExitBlockedThreshold_( const Detector * const pDetector ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_SetDetectorExitBlockedThreshold_
 *
 * Description: Sets the exit blocked threshold for the specified
 *				detector
 *
 * Author: ihenderson
 *
 * Date: 15/11/02
 *
 * Params: int16 n16ThresholdNew - new exit blocking threshold (tenths of secs)
 *		   Detector *pDetector - Detector to set the threshold for
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlow_SetDetectorExitBlockedThreshold_( const int16 n16ThresholdNew,
												   Detector * const pDetector )
{

	KDEBUG_EVAL_EXPRESSION( SF, n16ThresholdNew >= 0 );
	NULL_POINTER_CHECK( SF, pDetector );

	pDetector->n16DetExitBlockedThreshold = n16ThresholdNew;

} /* void MovaSatFlow_SetDetectorExitBlockedThreshold_( const int16 n16ThresholdNew, ... ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetSatFlowSmoothingFactor_
 *
 * Description: Gets the smoothing factor used to combine successive
 *				saturation flow estimates.
 *
 * Author: ihenderson
 *
 * Date: 13/12/02
 *
 * Params: void
 *
 * Return: REAL - smoothing factor
 *
 ************************************************************************/
REAL MovaSatFlow_GetSatFlowSmoothingFactor_( void )
{

	return ( gSatData.rSatFlowSmoothingFactor );

} /* REAL MovaSatFlow_GetSatFlowSmoothingFactor_( void ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_SetSatFlowSmoothingFactor_
 *
 * Description: Sets the smoothing factor used to combine successive
 *				saturation flow (and STLOST) estimates.
 *
 * Author: ihenderson
 *
 * Date: 13/12/02
 *
 * Params: REAL rSmoothingFactor - new smoothing factor
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlow_SetSatFlowSmoothingFactor_( const REAL rSmoothingFactor )
{

	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( rSmoothingFactor, 0.0, 1.0 ) );

	gSatData.rSatFlowSmoothingFactor = rSmoothingFactor;
	
	/* Currently, set the STLOST smoothing factor to the same as the 
	   one for sat flow */
	gSatData.rSTLOSTSmoothingFactor = rSmoothingFactor;

} /* void MovaSatFlow_SetSatFlowSmoothingFactor_( const REAL rSmoothingFactor ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetDetOccupancySmoothingFactor_
 *
 * Description: Gets the detector occupancy smoothing factor used to 
 *				combine successive detector occupancy times.
 *
 * Author: ihenderson
 *
 * Date: 13/12/02
 *
 * Params: void
 *
 * Return: REAL - smoothing factor
 *
 ************************************************************************/
REAL MovaSatFlow_GetDetOccupancySmoothingFactor_( void )
{

	return ( gSatData.rDetOccupancySmoothingFactor );

} /* REAL MovaSatFlow_GetDetOccupancySmoothingFactor_( void ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_SetDetOccupancySmoothingFactor_
 *
 * Description: Sets the smoothing factor used to combine successive
 *				detector occupancy times.
 *
 * Author: ihenderson
 *
 * Date: 13/12/02
 *
 * Params: REAL rSmoothingFactor - new smoothing factor
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlow_SetDetOccupancySmoothingFactor_( const REAL rSmoothingFactor )
{

	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( rSmoothingFactor, 0.0, 1.0 ) );

	gSatData.rDetOccupancySmoothingFactor = rSmoothingFactor;

} /* void MovaSatFlow_SetDetOccupancySmoothingFactor_( const REAL rSmoothingFactor ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetCriticalGapFactor_
 *
 * Description: Gets the factor used to scale the MOVA critical gap
 *				to find a critical gap suitable for sat flow estimation.
 *
 * Author: ihenderson
 *
 * Date: 13/12/02
 *
 * Params: void
 *
 * Return: REAL - gap factor
 *
 ************************************************************************/
REAL MovaSatFlow_GetCriticalGapFactor_( void )
{

	return ( gSatData.rCriticalGapFactor );

} /* REAL MovaSatFlow_GetCriticalGapFactor_( void ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_SetCriticalGapFactor_
 *
 * Description: Sets the factor used to scale the MOVA critical gap
 *				to find a critical gap suitable for sat flow estimation.
 *				Recalculates this 'measurement' critical gap.
 *
 * Author: ihenderson
 *
 * Date: 13/12/02
 *
 * Params: REAL rSmoothingFactor - new gap factor
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlow_SetCriticalGapFactor_( const REAL rGapFactor )
{
	uint8	n16LaneLoop;
	Lane	*pLane;

	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( rGapFactor, 0.0, 1.0 ) );

	gSatData.rCriticalGapFactor = rGapFactor;

	/* Bug fix 3, Mova saturation flow estimation test report. 
	   Recalculate the measurement critical gap for each lane 
	   after the critical gap factor is changed. */
	for ( n16LaneLoop = 0; n16LaneLoop < gSatData.n16LanesNum; ++n16LaneLoop )
	{
		pLane = &( gSatData.Lane_[ n16LaneLoop ] );

		pLane->n16MeasurementCriticalGapTime = MovaSatFlow_CalcMeasurementCritGap_( 
			(int16)DI_get_satflow_mean_gap(n16LaneLoop+1), gSatData.rCriticalGapFactor );

		KDEBUG_WRITE2( DEBUG_LEVEL_INFO, SF, "Lane %d: measurement critical gap set to %d.", 
			n16LaneLoop + 1, pLane->n16MeasurementCriticalGapTime );

	} /* For each lane */

} /* void MovaSatFlow_SetCriticalGapFactor_( const REAL rGapFactor ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetLongSlowVehicleDetOccupancyFactor_
 *
 * Description: Gets the factor used to scale the mean (car) 
 *				detector occupancy to give an estimate of occupancy
 *				for long/slow vehicles.
 *
 * Author: ihenderson
 *
 * Date: 13/12/02
 *
 * Params: void
 *
 * Return: REAL - occupancy factor
 *
 ************************************************************************/
REAL MovaSatFlow_GetLongSlowVehicleDetOccupancyFactor_( void )
{

	return ( gSatData.rLongSlowVehicleDetOccupanyFactor );

} /* REAL MovaSatFlow_GetLongSlowVehicleDetOccupancyFactor_( void ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_SetLongSlowVehicleDetOccupancyFactor_
 *
 * Description: Sets the factor used to scale the mean (car) 
 *				detector occupancy to give an estimate of occupancy
 *				for long/slow vehicles.
 *
 * Author: ihenderson
 *
 * Date: 13/12/02
 *
 * Params: REAL rOccupancyFactor - new occupancy factor
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlow_SetLongSlowVehicleDetOccupancyFactor_( const REAL rOccupancyFactor )
{

	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( rOccupancyFactor, 
		grHEAVIES_DET_OCC_FACTOR_MIN, 
		grHEAVIES_DET_OCC_FACTOR_MAX ) );

	gSatData.rLongSlowVehicleDetOccupanyFactor = rOccupancyFactor;

} /* void MovaSatFlow_SetLongSlowVehicleDetOccupancyFactor_( const REAL rOccupancyFactor ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetFlowRateProportionalErrorMax_
 *
 * Description: Gets the maximum allowed percentage error between
 *				flow rate calculated from a STOPLINE detector and
 *				flow rate calculated by MOVA from the X-detector.
 *
 * Author: ihenderson
 *
 * Date: 13/12/02
 *
 * Params: void
 *
 * Return: REAL - flow rate error 
 *
 ************************************************************************/
REAL MovaSatFlow_GetFlowRateProportionalErrorMax_( void )
{

	return ( gSatData.rFlowRateProportionalErrorMax );

} /* REAL MovaSatFlow_GetFlowRateProportionalErrorMax_( void ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_SetFlowRateProportionalErrorMax_
 *
 * Description: Sets the maximum allowed percentage error between
 *				flow rate calculated from a STOPLINE detector and
 *				flow rate calculated by MOVA from the X-detector.
 *
 * Author: ihenderson
 *
 * Date: 13/12/02
 *
 * Params: REAL rFlowRateError - new flow rate error
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlow_SetFlowRateProportionalErrorMax_( const REAL rFlowRateError )
{

	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( rFlowRateError, 0.0, 1.0 ) );

	gSatData.rFlowRateProportionalErrorMax = rFlowRateError;

} /* void MovaSatFlow_SetFlowRateProportionalErrorMax_( const REAL rFlowRateError ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetStoplineSTLOSTVehiclesNum_
 *
 * Description: Gets the number of vehicles queueing at the stop-line
 *				at start of green that are considered to be unable
 *				to cross the stop-line at saturation flow rate.
 *
 * Author: ihenderson
 *
 * Date: 13/12/02
 *
 * Params: void
 *
 * Return: int16 - STLOST vehicles num
 *
 ************************************************************************/
int16 MovaSatFlow_GetStoplineSTLOSTVehiclesNum_( void )
{

	return ( gSatData.n16StoplineSTLOSTVehiclesNum );

} /* int16 MovaSatFlow_GetStoplineSTLOSTVehiclesNum_( void ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_SetStoplineSTLOSTVehiclesNum_
 *
 * Description: Sets the number of vehicles queueing at the stop-line
 *				at start of green that are considered to be unable
 *				to cross the stop-line at saturation flow rate.
 *
 * Author: ihenderson
 *
 * Date: 13/12/02
 *
 * Params: int16 n16StoplineSTLOSTVehiclesNum - STLOST vehicles num
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlow_SetStoplineSTLOSTVehiclesNum_( const int16 n16StoplineSTLOSTVehiclesNum )
{

	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16StoplineSTLOSTVehiclesNum, 
		gn16STLOST_VEHICLES_NUM_MIN, 
		gn16STLOST_VEHICLES_NUM_MAX ) );

	gSatData.n16StoplineSTLOSTVehiclesNum = n16StoplineSTLOSTVehiclesNum;

} /* void MovaSatFlow_SetStoplineSTLOSTVehiclesNum_( const int16 n16StoplineSTLOSTVehiclesNum ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_GetXDetSTLOSTVehiclesNum_
 *
 * Description: Gets the number of vehicles queueing at the X-detector
 *				at start of green that are considered to be unable
 *				to cross the X-detector at saturation flow rate.
 *
 * Author: ihenderson
 *
 * Date: 13/12/02
 *
 * Params: void
 *
 * Return: int16 - STLOST vehicles num
 *
 ************************************************************************/
int16 MovaSatFlow_GetXDetSTLOSTVehiclesNum_( void )
{

	return ( gSatData.n16XDetSTLOSTVehiclesNum );

} /* int16 MovaSatFlow_GetXDetSTLOSTVehiclesNum_( void ) */


/*************************************************************************
 *
 * Function: MovaSatFlow_SetXDetSTLOSTVehiclesNum_
 *
 * Description: Sets the number of vehicles queueing at the X-detector
 *				at start of green that are considered to be unable
 *				to cross the X-detector at saturation flow rate.
 *
 * Author: ihenderson
 *
 * Date: 13/12/02
 *
 * Params: int16 n16XDetSTLOSTVehiclesNum - STLOST vehicles num
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlow_SetXDetSTLOSTVehiclesNum_( const int16 n16XDetSTLOSTVehiclesNum )
{

	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16XDetSTLOSTVehiclesNum, 
		gn16STLOST_VEHICLES_NUM_MIN, 
		gn16STLOST_VEHICLES_NUM_MAX ) );

	gSatData.n16XDetSTLOSTVehiclesNum = n16XDetSTLOSTVehiclesNum;

} /* void MovaSatFlow_SetXDetSTLOSTVehiclesNum_( const int16 n16XDetSTLOSTVehiclesNum ) */

/*************************************************************************
 *
 * Function: MovaSatFlow_Output_Message_Buffer
 *
 * Description: Outputs the satflow messages into the message buffer
 *
 * Author: Panderson
 *
 * Date: 18/02/2014
 *
 * Params: int16 lane - Lane number to output
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlow_Output_Message_Buffer( const int16 LaneNo )
{
   uint8 i = 0;
   uint8 current_msg_idx = CI_get_current_msg_num() - 1; 
   
   CI_set_msg_data(i++,current_msg_idx, 7);
   CI_set_msg_data(i++,current_msg_idx,1);
   /* Make lane 1 based */
   CI_set_msg_data(i++,current_msg_idx,LaneNo + 1);
   CI_set_msg_data(i++,current_msg_idx,gLaneOutput_[ LaneNo ].lIsExitBlocked);
   CI_set_msg_data(i++,current_msg_idx,gLaneOutput_[ LaneNo ].lINDetStandingQueue);
   CI_set_msg_data(i++,current_msg_idx,gLaneOutput_[ LaneNo ].lQueueOverXDet);
   CI_set_msg_data(i++,current_msg_idx,gLaneOutput_[ LaneNo ].lIsSatFlowAfterVarMinGreen);
   CI_set_msg_data(i++,current_msg_idx,gLaneOutput_[ LaneNo ].lFullySat);
   CI_set_msg_data(i++,current_msg_idx,gLaneOutput_[ LaneNo ].n16LongQueueSatCount);
   /* Real so convert to 1/100s and round up */
   CI_set_msg_data(i++,current_msg_idx,(int32)(gLaneOutput_[ LaneNo ].rLongQueueSatFlow));
   /* Real so convert to 1/100s and round up*/
   CI_set_msg_data(i++,current_msg_idx, (int32)(gLaneOutput_[ LaneNo ].rShortQueueSatFlow));
   /* Real so convert to 1/100s and round up */
   CI_set_msg_data(i++,current_msg_idx, (int32)(gLaneOutput_[ LaneNo ].rRawSatFlow));
   /* Real so convert to 1/100s and round up */
   CI_set_msg_data(i++,current_msg_idx, (int32)(gLaneOutput_[ LaneNo ].rRawSTLOST));
   /* Real so convert to 1/100s and round up */
    CI_set_msg_data(i++,current_msg_idx, (int32)(gLaneOutput_[ LaneNo ].rRawFlowRate));
   /* Real so convert to 1/100s and round up */
    CI_set_msg_data(i++,current_msg_idx, (int32)(gLaneOutput_[ LaneNo ].rSmoothedOcc));

	CI_inc_current_msg_num();
	    
    if (CI_get_current_msg_num() > MAX_MSGS)
    {
        CI_reset_current_msg_number(); 
    }
}

/*************************************************************************
 *
 * Function: MovaSatFlow_Output_Message_
 *
 * Description: Outputs the satflow messages
 *
 * Author: rdrury
 *
 * Date: 04/10/05
 *
 * Params: int16 lane - Lane number to output
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlow_Output_Message_( const int16 lane )
{
#if defined(TRL_INTEGRATION_TEST) || defined(IS_MOVA_COMM_ENABLED)
	static char sfExBl		[ 16 ];
	static char sfInQ		[ 16 ];
	static char sfXQ		[ 16 ];
	static char sfMin		[ 16 ];
	static char sfFull		[ 16 ];
	static char sfLQCnt		[ 16 ];
	static char sfLQSat		[ 16 ];
	static char sfSQSat		[ 16 ];
	static char sfRawSat	[ 16 ];
	static char sfRawSTL	[ 16 ];
	static char sfFlow		[ 16 ];
	static char sfOcc		[ 16 ];
	static char sfCurrTime  [ 16 ];		/*Delete after testing??*/
	static char sfCurrDate  [ 16 ];		/*Delete after testing??*/
#endif

#if defined (SF_PROBLEM_FINDER)
	Lane       *pLane;
#endif	
	
	if( DI_is_to_show_mova_messages() && *ShowSatFlow == 1)
	{       
        /* IRH MOD: 22/05/06: Get the destination for sat flow
        messages. GetOutputDestination() can be found in Userbig.c */

#ifdef IS_MOVA_COMM_ENABLED
        int nOutputDest = GetOutputDestination();

		MovaSatFlowTest_GetIsExitBlocked_(sfExBl, lane);
		MovaSatFlowTest_GetINDetStandingQueue_(sfInQ, lane);
		MovaSatFlowTest_GetQueueOverXDet_(sfXQ, lane);
		MovaSatFlowTest_GetIsSatFlowAfterVarMinGreen_(sfMin, lane);
		MovaSatFlowTest_GetFullySat_(sfFull, lane);
		MovaSatFlowTest_GetLQSatCount_(sfLQCnt, lane);
		MovaSatFlowTest_GetLQSatFlow_(sfLQSat, lane);
		MovaSatFlowTest_GetSQSatFlow_(sfSQSat, lane);
		MovaSatFlowTest_GetRawSatFlow_(sfRawSat, lane);
		MovaSatFlowTest_GetRawSTLOST_(sfRawSTL, lane);
		MovaSatFlowTest_GetRawFlowRate_(sfFlow, lane);
		MovaSatFlowTest_GetSmoothedOcc_(sfOcc, lane);

		/*PSK MOD: 15/04/09: Added extra function to get time and date
		Later followed by condition to print only those messages which
		contains valid Sat Flow value. Remove after testing???*/
		MovaSatFlowTest_GetCurrentTime_(sfCurrTime);
		MovaSatFlowTest_GetCurrentDate_(sfCurrDate);
		
		if (gLaneOutput_[ lane ].rRawSatFlow != grFLOW_RATE_INVALID)
		{

			/* IRH MOD: 19/05/06: Use Send_FormatString() - diag_printf is just for 
			debugging on the Gemini unit.  Send_FormatString works on PC as well, 
			and has its own buffer (OutputString buffer no longer required). */
			Send_FormatString( nOutputDest, 
				"SF: Date: %s Time: %s L%02d ExBl:%s InQ:%s XQ:%s Min:%s Full:%s LQCnt:%s LQSat:%s SQSat:%s RawSat:%s RawSTL:%s Flow:%s Occ:%s\r\n",
				sfCurrDate, sfCurrTime, (lane) + 1, sfExBl, sfInQ, sfXQ, sfMin, sfFull, sfLQCnt, sfLQSat, sfSQSat, sfRawSat, sfRawSTL, sfFlow, sfOcc );
			/*
			sprintf( OutputString, ("SF: L%02d ExBl:%s InQ:%s XQ:%s Min:%s Full:%s LQCnt:%s LQSat:%s SQSat:%s RawSat:%s RawSTL:%s Flow:%s Occ:%s\n"),
			(lane) + 1, sfExBl, sfInQ, sfXQ, sfMin, sfFull, sfLQCnt, sfLQSat, sfSQSat, sfRawSat, sfRawSTL, sfFlow, sfOcc);
	 		diag_printf( OutputString, stdout );
			*/     
		}
#endif
	}

	if (gLaneOutput_[ lane ].rRawSatFlow != grFLOW_RATE_INVALID)
	{
		/* Always output sat flow message into buffer if valid */
		MovaSatFlow_Output_Message_Buffer( lane );
	}

#if defined (TRL_INTEGRATION_TEST)
	{

		MovaSatFlowTest_GetIsExitBlocked_(sfExBl, lane);
		MovaSatFlowTest_GetINDetStandingQueue_(sfInQ, lane);
		MovaSatFlowTest_GetQueueOverXDet_(sfXQ, lane);
		MovaSatFlowTest_GetIsSatFlowAfterVarMinGreen_(sfMin, lane);
		MovaSatFlowTest_GetFullySat_(sfFull, lane);
		MovaSatFlowTest_GetLQSatCount_(sfLQCnt, lane);
		MovaSatFlowTest_GetLQSatFlow_(sfLQSat, lane);
		MovaSatFlowTest_GetSQSatFlow_(sfSQSat, lane);
		MovaSatFlowTest_GetRawSatFlow_(sfRawSat, lane);
		MovaSatFlowTest_GetRawSTLOST_(sfRawSTL, lane);
		MovaSatFlowTest_GetRawFlowRate_(sfFlow, lane);
		MovaSatFlowTest_GetSmoothedOcc_(sfOcc, lane);
		MovaSatFlowTest_GetCurrentTime_(sfCurrTime);
		MovaSatFlowTest_GetCurrentDate_(sfCurrDate);

		if (gLaneOutput_[ lane ].rRawSatFlow != grFLOW_RATE_INVALID)
		{
			sprintf(SATTextBuf, "SF: Date: %s Time: %s L%02d ExBl:%s InQ:%s XQ:%s Min:%s Full:%s LQCnt:%s LQSat:%s SQSat:%s RawSat:%s RawSTL:%s Flow:%s Occ:%s\r\n",
					sfCurrDate, sfCurrTime, (lane) + 1, sfExBl, sfInQ, sfXQ, sfMin, sfFull, sfLQCnt, sfLQSat, sfSQSat, sfRawSat, sfRawSTL, sfFlow, sfOcc);
			SatLogWrite( SATTextBuf);
		}

	}
#endif

}



/*********************************************************/
/* Rework sat flow container */

/*************************************************************************
 *
 * Function: MovaSatFlowStats_AddDetectorTransition
 *
 * Description: Add transition to store
 *
 * Author: PAnderson
 *
 * Date: 04-Jan-2013	
 *
 * Params:	transition	Transition to add
			currentCount	No of transitions that have occurred for this det
 *
 * Return:	None
 *
 ************************************************************************/
static void MovaSatFlowStats_AddDetectorTransition( const TransitionItem transition )
{
	uint16 nextFree = MovaSatFlowStats_GetNextFreeSlot();

	if (transition.n16DetNo >= gn16DETS_NUM_MAX)
	{
		KDEBUG_WRITE0( DEBUG_LEVEL_INFO, SF, " MovaSatFlowStats_AddDetectorTransition detector number not valid ");
		return;
		
	}

	if (nextFree == gnINVALID_SLOT_NO)
	{
		KDEBUG_WRITE0( DEBUG_LEVEL_INFO, SF, " MovaSatFlowStats_AddDetectorTransition no free space in container ");
		return;
	}

	if (arrFirstTransitionDet[transition.n16DetNo] == gnINVALID_SLOT_NO)
	{ /*First transition for this detector */

		arrDetTransitionStore[nextFree].transitionItem.n16DetNo = transition.n16DetNo;
		arrDetTransitionStore[nextFree].transitionItem.n16DetStateChangeTime_ = transition.n16DetStateChangeTime_;
		arrDetTransitionStore[nextFree].transitionItem.n16DetStateNew_ = transition.n16DetStateNew_;
		arrDetTransitionStore[nextFree].NextDet = gnINVALID_SLOT_NO;
		arrDetTransitionStore[nextFree].PrevDet = gnINVALID_SLOT_NO;
		arrFirstTransitionDet[transition.n16DetNo] = nextFree;
		arrLastTransitionDet[transition.n16DetNo] =  nextFree;
	}
	else
	{ /* Link to previous transition for this detector */

		uint16 currentLast = arrLastTransitionDet[transition.n16DetNo];
		
		arrDetTransitionStore[currentLast].NextDet = nextFree;
		arrDetTransitionStore[nextFree].transitionItem.n16DetNo = transition.n16DetNo;
		arrDetTransitionStore[nextFree].transitionItem.n16DetStateChangeTime_ = transition.n16DetStateChangeTime_;
		arrDetTransitionStore[nextFree].transitionItem.n16DetStateNew_ = transition.n16DetStateNew_;
		arrDetTransitionStore[nextFree].NextDet = gnINVALID_SLOT_NO;
		arrDetTransitionStore[nextFree].PrevDet = currentLast;
		arrLastTransitionDet[transition.n16DetNo] = nextFree;
	
	}
}


/*************************************************************************
 *
 * Function: MovaSatFlowStats_FindDetectorTransition
 *
 * Description: Find a transition for a particular detector
 *
 * Author: PAnderson
 *
 * Date: 04-Jan-2013	
 *
 * Params:	transNo			Transition to find
			detNo			detector number
			_DetectorTrans	The detector transition in question
 *
 * Return:	1 if found 0 if not.
 *
 ************************************************************************/
static BOOL MovaSatFlowStats_FindDetectorTransition(  const uint16 transNo,
													  const uint16 detNo,
													  TransitionItem* foundTrans )

{
	uint16 lastTransition = arrFirstTransitionDet[detNo];
	int16 currentTransition = 0;
	uint16 realTransNo = transNo;
	/*go down by default */
	int8 goingUp = 1; 

	if ((cachedLastDetTransition != gnINVALID_SLOT_NO) && 
		(detNo == arrDetTransitionStore[cachedLastDetTransition].transitionItem.n16DetNo))
	{ /* Try to use cached version if  */
	  /* 1. Detector number is the same */
	  /* 2. Valid cache is present  */

		if (transNo >= cachedGetDetTransitionCount)
		{ /* Transition is greater or same as cache */
			goingUp = 1;
			lastTransition = cachedLastDetTransition;
			realTransNo = transNo - cachedGetDetTransitionCount;
		}
		else
		{ /* Should we go down or start from the beginning */
			uint16 diff = cachedGetDetTransitionCount - transNo;

			if (diff > transNo)
			{ /* Nearer the beginning */
				goingUp = 1;
			}
			else
			{ /*Use cache and go down*/
				goingUp = 0;
				lastTransition = cachedLastDetTransition;
				realTransNo = cachedGetDetTransitionCount - transNo;
			}
		}
	}

	if (goingUp)
	{
		for (currentTransition = 0; currentTransition < realTransNo; currentTransition++ )
		{ /*Walk up the list to find the transition for this detector */
			lastTransition = arrDetTransitionStore[lastTransition].NextDet;

			if (lastTransition == gnINVALID_SLOT_NO)
			{ /* This indicates that we have exceeded the number of transitions as we do not have a link to the next one */
				KDEBUG_WRITE0( DEBUG_LEVEL_ERROR, SF, "MovaSatFlowStats_FindDetectorTransition Error exceeded walking up transition count  ");
				KDEBUG_WRITE4( DEBUG_LEVEL_ERROR, SF, "detNo = %d current = %d, req trans = %d, realTransNo = %d  ", detNo, currentTransition, transNo, realTransNo);
				return (0);
			}
		}
	}
	else
	{
		for (currentTransition = cachedGetDetTransitionCount; currentTransition > transNo; currentTransition-- )
		{ /*Walk down the list to find the transition for this detector */
			lastTransition = arrDetTransitionStore[lastTransition].PrevDet;

			if (lastTransition == gnINVALID_SLOT_NO)
			{ /* This indicates that we have exceeded the number of transitions as we do not have a link to the next one */
				KDEBUG_WRITE0( DEBUG_LEVEL_ERROR, SF, "MovaSatFlowStats_FindDetectorTransition Error exceeded walking down transition count  ");
				KDEBUG_WRITE4( DEBUG_LEVEL_ERROR, SF, "detNo = %d current = %d, req trans = %d, realTransNo = %d  ", detNo, currentTransition, transNo, realTransNo);
				return (0);
			}
		}

	}

	if (transNo != 0 )
	{ 	/*Update cache value unless getting first element */
		cachedGetDetTransitionCount = transNo;
		cachedLastDetTransition = lastTransition;
	}

	foundTrans->n16DetStateChangeTime_ = arrDetTransitionStore[lastTransition].transitionItem.n16DetStateChangeTime_;
	foundTrans->n16DetStateNew_ = arrDetTransitionStore[lastTransition].transitionItem.n16DetStateNew_;
	foundTrans->n16DetNo = arrDetTransitionStore[lastTransition].transitionItem.n16DetNo;

	return (1);
}


/*************************************************************************
 *
 * Function: MovaSatFlowStats_GetTransitionTime
 *
 * Description: Find a time for a transition for a particular detector
 *
 * Author: PAnderson
 *
 * Date: 04-Jan-2013	
 *
 * Params:	transNo			Transition containing time
			detNo			detector number
 *
 * Return:	The detector transition time in question
 *
 ************************************************************************/
static int16 MovaSatFlowStats_GetTransitionTime( const uint16 transNo,
												 const uint16 detNo)
{
	TransitionItem thisDetectorTransition;
	BOOL foundDet = 0;

	if (arrFirstTransitionDet[detNo] ==  gnINVALID_SLOT_NO)
	{ /*Detector does not have any transitions*/ 
		return (gn16TIME_INVALID);
	}

	foundDet =  MovaSatFlowStats_FindDetectorTransition(transNo,
														 detNo,
														 &thisDetectorTransition);

	if (!foundDet)
	{ /* Did not find particular transition */
		return(gn16TIME_INVALID);
	}

	return (thisDetectorTransition.n16DetStateChangeTime_);
}


/*************************************************************************
 *
 * Function: MovaSatFlowStats_GetLastTransitionTime
 *
 * Description: Find a time for the last transition for a particular detector
 *
 * Author: PAnderson
 *
 * Date: 04-Jan-2013	
 *
 * Params:	detNo			detector number
 *
 * Return:	The last detector transition time 
 *
 ************************************************************************/
// SLM FIXME this function is not used anywhere
static int16 MovaSatFlowStats_GetLastTransitionTime(const uint16 detNo)
{

	if ((arrFirstTransitionDet[detNo] ==  gnINVALID_SLOT_NO) ||
		(arrLastTransitionDet[detNo]  ==  gnINVALID_SLOT_NO) ||
		(arrDetTransitionStore[arrLastTransitionDet[detNo]].transitionItem.n16DetStateChangeTime_ == gn16TIME_INVALID) ||
		(arrDetTransitionStore[arrLastTransitionDet[detNo]].transitionItem.n16DetNo != detNo) ||
		(arrDetTransitionStore[arrLastTransitionDet[detNo]].transitionItem.n16DetStateNew_ == DET_STATE_INVALID))
	{ /*Detector does not have valid last transition*/ 
		return (gn16TIME_INVALID);
	}

	return( arrDetTransitionStore[arrLastTransitionDet[detNo]].transitionItem.n16DetStateChangeTime_ );
}

/*************************************************************************
 *
 * Function: MovaSatFlowStats_GetLastTransitionState
 *
 * Description: Find a state for the last transition for a particular detector
 *
 * Author: PAnderson
 *
 * Date: 04-Jan-2013	
 *
 * Params:	detNo			detector number
 *
 * Return:	The last detector transition state 
 *
 ************************************************************************/
static int16 MovaSatFlowStats_GetLastTransitionState(const uint16 detNo)
{

	if ((arrFirstTransitionDet[detNo] ==  gnINVALID_SLOT_NO) ||
		(arrLastTransitionDet[detNo]  ==  gnINVALID_SLOT_NO) ||
		(arrDetTransitionStore[arrLastTransitionDet[detNo]].transitionItem.n16DetStateChangeTime_ == gn16TIME_INVALID) ||
		(arrDetTransitionStore[arrLastTransitionDet[detNo]].transitionItem.n16DetNo != detNo) ||
		(arrDetTransitionStore[arrLastTransitionDet[detNo]].transitionItem.n16DetStateNew_ == DET_STATE_INVALID))
	{ /*Detector does not have valid last transition*/ 
		return (DET_STATE_INVALID);
	}

	return( arrDetTransitionStore[arrLastTransitionDet[detNo]].transitionItem.n16DetStateNew_ );
}


/*************************************************************************
 *
 * Function: MovaSatFlowStats_GetTransitionState
 *
 * Description: Find a state for a transition for a particular detector
 *
 * Author: PAnderson
 *
 * Date: 04-Jan-2013	
 *
 * Params:	transNo			Transition containing state
			detNo			detector number
 *
 * Return:	The detector transition state in question
 *
 ************************************************************************/
static int16 MovaSatFlowStats_GetTransitionState( const uint16 transNo,
												  const uint16 detNo)
{
	TransitionItem thisDetectorTransition;
	BOOL foundDet = 0;

	if (arrFirstTransitionDet[detNo] == gnINVALID_SLOT_NO)
	{ /*Detector does not have any transitions*/ 
		return (DET_STATE_INVALID);
	}

	foundDet =  MovaSatFlowStats_FindDetectorTransition(transNo,
														 detNo,
														 &thisDetectorTransition);

	if (!foundDet)
	{ /* Did not find particular transition */
		return(DET_STATE_INVALID);
	}

	return (thisDetectorTransition.n16DetStateNew_);
}


/*************************************************************************
 *
 * Function: MovaSatFlowStats_InitialiseTransitionData
 *
 * Description: Initialise sat container data
 *
 * Author: PAnderson
 *
 * Date: 04-Jan-2013	
 *
 * Params:	transNo		None
 *
 * Return:				None
 *
 ************************************************************************/

void MovaSatFlowStats_InitialiseTransitionData( void )
{
	int16 index = 0;

	for (index=0; index < gn16DETS_NUM_MAX; index++)
	{
		arrFirstTransitionDet[index] = gnINVALID_SLOT_NO;
		arrLastTransitionDet[index] =  gnINVALID_SLOT_NO;
	}

	for (index = 0; index < gnMAX_TRANSITIONS; index++)
	{
		arrDetTransitionStore[index].transitionItem.n16DetNo = gnINVALID_DET_NO;
		arrDetTransitionStore[index].transitionItem.n16DetStateChangeTime_ = gn16TIME_INVALID;
		arrDetTransitionStore[index].transitionItem.n16DetStateNew_ = DET_STATE_INVALID;
		arrDetTransitionStore[index].NextDet = gnINVALID_SLOT_NO;
		arrDetTransitionStore[index].PrevDet = gnINVALID_SLOT_NO;
	}

	cachedLastDetTransition =  gnINVALID_SLOT_NO;
	cachedGetDetTransitionCount = gnINVALID_SLOT_NO;
	cachedLastIndexPos = 0;
}




/*************************************************************************
 *
 * Function: MovaSatFlowStats_GetNextFreeSlot
 *
 * Description: Get Next Free area in store
 *
 * Author: PAnderson
 *
 * Date: 04-Jan-2013	
 *
 * Params: None
 *
 * Return:	Index of next free element or gnINVALID_SLOT_NO if none
 *
 ************************************************************************/
static uint16 MovaSatFlowStats_GetNextFreeSlot( void )
{
	uint16 count = 0;
	uint16 actualIndex = 0;
	uint16 foundIndex = gnINVALID_SLOT_NO;
	uint16 justfordebug = 0;

	for (count = cachedLastIndexPos; count < (cachedLastIndexPos + gnMAX_TRANSITIONS); count++)
	{
		if (count < gnMAX_TRANSITIONS )
		{
			actualIndex = count;
		}
		else
		{ /* Start counting from beginning */
			actualIndex = count - gnMAX_TRANSITIONS;
		}

		if ( arrDetTransitionStore[actualIndex].transitionItem.n16DetNo == gnINVALID_DET_NO)
		{
			foundIndex = actualIndex;
			/* Update last found position to optimise search next time */
			justfordebug = cachedLastIndexPos;
			cachedLastIndexPos = foundIndex + 1;
			break;
		}
	}

	return (foundIndex);
}


/*************************************************************************
 *
 * Function: MovaSatFlowStats_DeleteDetectorTransitions
 *
 * Description: Delete all transitions associated to a detector.
 *
 * Author: PAnderson
 *
 * Date: 04-Jan-2013	
 *
 * Params: detNo The detector in question
 *
 * Return:	None
 *
 ************************************************************************/
static void MovaSatFlowStats_DeleteDetectorTransitions( const uint16 detNo )
{
	uint16 currentTransition = 0;
	int16 index = 0;

	if ((detNo < 0) || (detNo >= gn16DETS_NUM_MAX))
	{
		KDEBUG_WRITE1( DEBUG_LEVEL_ERROR, SF, "MovaSatFlowStats_DeleteDetectorTransitions invalid no = %d ", detNo);
		return;
	}

	if (arrFirstTransitionDet[detNo] == gnINVALID_SLOT_NO)
	{
		return;
	}

	currentTransition = arrFirstTransitionDet[detNo];

	arrFirstTransitionDet[detNo] = gnINVALID_SLOT_NO;
	arrLastTransitionDet[detNo] = gnINVALID_SLOT_NO;

	if ((cachedLastDetTransition != gnINVALID_SLOT_NO) && 
		(detNo == arrDetTransitionStore[cachedLastDetTransition].transitionItem.n16DetNo))
	{ /* Last cached value is for this detector delete it*/
		cachedGetDetTransitionCount = 0 ;
		cachedLastDetTransition = gnINVALID_SLOT_NO;
		//arrDetTransitionStore[cachedLastDetTransition].transitionItem.n16DetNo = gnINVALID_DET_NO;
	}

	for (index = 0; index < gnMAX_TRANSITIONS; index++)
	{ /* Ensure we always exit this loop */
		arrDetTransitionStore[currentTransition].transitionItem.n16DetNo = gnINVALID_DET_NO;
		currentTransition = arrDetTransitionStore[currentTransition].NextDet;

		if ( currentTransition == gnINVALID_SLOT_NO)
		{
			break;
		}
	}
}

/*************************
*	Testing functions
**************************/


#if defined (TEST_OUTPUT)


/*************************************************************************
 *
 * Function: MovaSatFlowTest_TestOutputInitialise_
 *
 * Description: Function for initialising data test output data
 *				that is relevant throughout a cycle, as well as
 *				data that is more 'local'. Should be called after
 *				the main module data is initialised.
 *
 * Author: ihenderson
 *
 * Date: 16/12/02
 *
 * Params: int16 n16LaneNo - lane to initialise output data for
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlowTest_TestOutputInitialise_( void )
{
	int16		n16LaneLoop;
	LaneOutput	*pLaneOutput;
	Lane		*pLane;
	Detector	*pDetector;

	/* For each lane */
	for ( n16LaneLoop = 0; n16LaneLoop < gSatData.n16LanesNum; ++n16LaneLoop )
	{
		pLaneOutput = &( gLaneOutput_[ n16LaneLoop ] );

		/* Get lane and sat flow detector */
		pLane = &( gSatData.Lane_[ n16LaneLoop ] );
		pDetector = &( gSatData.Detector_[ pLane->n16SatFlowDetNo ] );

		/* Set at module initialisation */
		pLaneOutput->n16SatFlowDetType = pDetector->n16DetType;
		
		/* These four seeded with values at module initialisation */
		pLaneOutput->rSmoothedOcc = pDetector->rDetMeanCarOccupancyTime;
		pLaneOutput->rSmoothedSatFlow = pLane->rSmoothedSatFlowRate
			* grTENTHS_OF_SECONDS_TO_HOURS_FACTOR;
		pLaneOutput->rSmoothedSTLOST = pLane->rSmoothedSTLOSTTime;
		pLaneOutput->rSmoothedFlowRate = pLane->rSmoothedFlowRate;

		/* Set the warmup period flag */
		if ( pLane->n16StartOfGreenCount < gn16WARM_UP_PERIOD_FINISHED_COUNT )
		{
			pLaneOutput->lInWarmUpPeriod = TRUE;
		}
		else
		{
			pLaneOutput->lInWarmUpPeriod = FALSE;
		}

		/* Reset the data that needs resetting at the start of every green */
		MovaSatFlowTest_TestOutputReset_( n16LaneLoop );

		/* Set the cycle number to the first (1-based) */
		pLaneOutput->u16CurrentCycleNum = 1;

		/* Set the current estimate number to zero */
		pLaneOutput->u16LastEstimateNum = 0;

	} /* For each lane */

} /* static void MovaSatFlowTest_TestOutputInitialise_( void ) */


/*************************************************************************
 *
 * Function: MovaSatFlowTest_TestOutputReset_
 *
 * Description: Function for resetting test output at the start of green 
 *				that isn't valid throughout the cycle. Should be called
 *				at start of green and at initialisation.
 *
 * Author: ihenderson
 *
 * Date: 16/12/02
 *
 * Params: int16 n16LaneNo - lane to reset output data for
 *
 * Return: void
 *
 ************************************************************************/
static void MovaSatFlowTest_TestOutputReset_( const int16 n16LaneNo )
{
	LaneOutput	*pLaneOutput;

	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );

	pLaneOutput = &( gLaneOutput_[ n16LaneNo ] );

	pLaneOutput->n16RedCountIN = gn16VEHICLE_COUNT_INVALID;
	pLaneOutput->lINDetStandingQueue = INVALID;
	pLaneOutput->n16RedCountX = gn16VEHICLE_COUNT_INVALID;
	pLaneOutput->n16VarMinGreen = gn16TIME_INVALID;
	pLaneOutput->n16LeavingAmber = gn16TIME_INVALID;
	pLaneOutput->n16StartRed = gn16TIME_INVALID;
	pLaneOutput->lIsSatFlowDetFaulty = INVALID;
	pLaneOutput->lIsExitBlocked = INVALID;
	pLaneOutput->n16FirstSatVehicle = gn16DET_STATE_CHANGE_NUM_INVALID;
	pLaneOutput->n16LastSatVehicle = gn16DET_STATE_CHANGE_NUM_INVALID;
	pLaneOutput->lFullySat = INVALID;
	pLaneOutput->lQueueOverXDet = INVALID;
	pLaneOutput->lIsSatFlowAfterVarMinGreen = INVALID;
	pLaneOutput->lLongQueueMethodUsed = INVALID;
	pLaneOutput->lShortQueueMethodUsed = INVALID;
	pLaneOutput->n16LongQueueSatCount = gn16VEHICLE_COUNT_INVALID;
	pLaneOutput->n16ShortQueueSatCount = gn16VEHICLE_COUNT_INVALID;
	pLaneOutput->rLongQueueSatFlow = grFLOW_RATE_INVALID;
	pLaneOutput->rShortQueueSatFlow = grFLOW_RATE_INVALID;
	pLaneOutput->rRawSatFlow = grFLOW_RATE_INVALID;
	pLaneOutput->rRawFlowRate = grFLOW_RATE_INVALID;
	pLaneOutput->n16SatFlowTime = gn16TIME_INVALID;
	pLaneOutput->rRawSTLOST = gn16TIME_INVALID;

} /* static void MovaSatFlowTest_TestOutputReset_( const int16 n16LaneNo ) */


/* Functions for getting values of test output variables as strings */

char * MovaSatFlowTest_GetCurrentCycleNum_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	sprintf( szBuffer, "%u", gLaneOutput_[ n16LaneNo ].u16CurrentCycleNum );

	return ( szBuffer );
}

char *	MovaSatFlowTest_GetLastEstimateNum_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	sprintf( szBuffer, "%u", gLaneOutput_[ n16LaneNo ].u16LastEstimateNum );

	return ( szBuffer );
}

char * MovaSatFlowTest_GetVarMinGreen_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	if ( gLaneOutput_[ n16LaneNo ].n16VarMinGreen == gn16TIME_INVALID )
	{
		sprintf( szBuffer, gszUNKNOWN_VALUE );
	}
	else
	{
		sprintf( szBuffer, "%d", gLaneOutput_[ n16LaneNo ].n16VarMinGreen );
	}
	
	return ( szBuffer );
}

char * MovaSatFlowTest_GetLeavingAmber_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	if ( gLaneOutput_[ n16LaneNo ].n16LeavingAmber == gn16TIME_INVALID )
	{
		sprintf( szBuffer, gszUNKNOWN_VALUE );
	}
	else
	{
		sprintf( szBuffer, "%d", gLaneOutput_[ n16LaneNo ].n16LeavingAmber );
	}
	
	return ( szBuffer );
}

char * MovaSatFlowTest_GetStartRed_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	if ( gLaneOutput_[ n16LaneNo ].n16StartRed == gn16TIME_INVALID )
	{
		sprintf( szBuffer, gszUNKNOWN_VALUE );
	}
	else
	{
		sprintf( szBuffer, "%d", gLaneOutput_[ n16LaneNo ].n16StartRed );
	}
	
	return ( szBuffer );
}

char * MovaSatFlowTest_GetRedCountIN_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	if ( gLaneOutput_[ n16LaneNo ].n16RedCountIN == gn16VEHICLE_COUNT_INVALID )
	{
		sprintf( szBuffer, gszUNKNOWN_VALUE );
	}
	else
	{
		sprintf( szBuffer, "%d", gLaneOutput_[ n16LaneNo ].n16RedCountIN );
	}
	
	return ( szBuffer );
}

char * MovaSatFlowTest_GetRedCountX_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	if ( gLaneOutput_[ n16LaneNo ].n16RedCountX == gn16VEHICLE_COUNT_INVALID )
	{
		sprintf( szBuffer, gszUNKNOWN_VALUE );
	}
	else
	{
		sprintf( szBuffer, "%d", gLaneOutput_[ n16LaneNo ].n16RedCountX );
	}

	return ( szBuffer );
}

char * MovaSatFlowTest_GetLQSatCount_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	if ( gLaneOutput_[ n16LaneNo ].n16LongQueueSatCount == gn16VEHICLE_COUNT_INVALID )
	{
		sprintf( szBuffer, gszUNKNOWN_VALUE );
	}
	else
	{
		sprintf( szBuffer, "%d", gLaneOutput_[ n16LaneNo ].n16LongQueueSatCount );
	}

	return ( szBuffer );
}
	
char * MovaSatFlowTest_GetSQSatCount_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	if ( gLaneOutput_[ n16LaneNo ].n16ShortQueueSatCount == gn16VEHICLE_COUNT_INVALID )
	{
		sprintf( szBuffer, gszUNKNOWN_VALUE );
	}
	else
	{
		sprintf( szBuffer, "%d", gLaneOutput_[ n16LaneNo ].n16ShortQueueSatCount );
	}

	return ( szBuffer );
}

char * MovaSatFlowTest_GetFirstSatVehicle_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	if ( gLaneOutput_[ n16LaneNo ].n16FirstSatVehicle == gn16DET_STATE_CHANGE_NUM_INVALID )
	{
		sprintf( szBuffer, gszUNKNOWN_VALUE );
	}
	else
	{
		sprintf( szBuffer, "%d", gLaneOutput_[ n16LaneNo ].n16FirstSatVehicle );
	}

	return ( szBuffer );
}
	
char * MovaSatFlowTest_GetLastSatVehicle_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	if ( gLaneOutput_[ n16LaneNo ].n16LastSatVehicle == gn16DET_STATE_CHANGE_NUM_INVALID )
	{
		sprintf( szBuffer, gszUNKNOWN_VALUE );
	}
	else
	{
		sprintf( szBuffer, "%d", gLaneOutput_[ n16LaneNo ].n16LastSatVehicle );
	}
	
	return ( szBuffer );
}

char * MovaSatFlowTest_GetSatFlowDetType_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	if ( gLaneOutput_[ n16LaneNo ].n16SatFlowDetType == DET_TYPE_INVALID )
	{
		sprintf( szBuffer, gszUNKNOWN_VALUE );
	}
	else
	{
		sprintf( szBuffer, "%s", 
			gLaneOutput_[ n16LaneNo ].n16SatFlowDetType == DET_TYPE_X_DET ? "X" : 
			gLaneOutput_[ n16LaneNo ].n16SatFlowDetType == DET_TYPE_OUT_DET ? "OUT" : 
			gLaneOutput_[ n16LaneNo ].n16SatFlowDetType == DET_TYPE_STOP_DET ? "STOPLINE" : "UNRECOGNISED" );
	}

	return ( szBuffer );
}

char * MovaSatFlowTest_GetRawSTLOST_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );
	
	sprintf( szBuffer, "%.1f", gLaneOutput_[ n16LaneNo ].rRawSTLOST );	
	
	return ( szBuffer );
}

char * MovaSatFlowTest_GetRawSatFlow_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	if ( gLaneOutput_[ n16LaneNo ].rRawSatFlow == grFLOW_RATE_INVALID )
	{
		sprintf( szBuffer, gszUNKNOWN_VALUE );
	}
	else
	{
		sprintf( szBuffer, "%.2f", gLaneOutput_[ n16LaneNo ].rRawSatFlow );
	}
	
	return ( szBuffer );
}

char * MovaSatFlowTest_GetLQSatFlow_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	if ( gLaneOutput_[ n16LaneNo ].rLongQueueSatFlow == grFLOW_RATE_INVALID )
	{
		sprintf( szBuffer, gszUNKNOWN_VALUE );
	}
	else
	{
		sprintf( szBuffer, "%.2f", gLaneOutput_[ n16LaneNo ].rLongQueueSatFlow );
	}
	
	return ( szBuffer );
}

char * MovaSatFlowTest_GetSQSatFlow_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	if ( gLaneOutput_[ n16LaneNo ].rShortQueueSatFlow == grFLOW_RATE_INVALID )
	{
		sprintf( szBuffer, gszUNKNOWN_VALUE );
	}
	else
	{
		sprintf( szBuffer, "%.2f", gLaneOutput_[ n16LaneNo ].rShortQueueSatFlow );
	}
	
	return ( szBuffer );
}

	
char * MovaSatFlowTest_GetSmoothedSatFlow_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );
	
	if ( gLaneOutput_[ n16LaneNo ].rSmoothedSatFlow == grFLOW_RATE_INVALID )
	{
		sprintf( szBuffer, gszUNKNOWN_VALUE );
	}
	else
	{
		sprintf( szBuffer, "%.2f", gLaneOutput_[ n16LaneNo ].rSmoothedSatFlow );
	}

	return ( szBuffer );
}

char * MovaSatFlowTest_GetSmoothedSTLOST_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	if ( gLaneOutput_[ n16LaneNo ].rSmoothedSTLOST == (REAL)gn16TIME_INVALID )
	{
		sprintf( szBuffer, gszUNKNOWN_VALUE );
	}
	else
	{
		sprintf( szBuffer, "%.2f", gLaneOutput_[ n16LaneNo ].rSmoothedSTLOST );
	}
	
	return ( szBuffer );
}


char * MovaSatFlowTest_GetSmoothedOcc_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	if ( gLaneOutput_[ n16LaneNo ].rSmoothedOcc == grDET_OCCUPANCY_INVALID )
	{
		sprintf( szBuffer, gszUNKNOWN_VALUE );
	}
	else
	{
		sprintf( szBuffer, "%.2f", gLaneOutput_[ n16LaneNo ].rSmoothedOcc );
	}
	
	return ( szBuffer );
}

char * MovaSatFlowTest_GetRawFlowRate_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	if ( gLaneOutput_[ n16LaneNo ].rRawFlowRate == grFLOW_RATE_INVALID )
	{
		sprintf( szBuffer, gszUNKNOWN_VALUE );
	}
	else
	{
		sprintf( szBuffer, "%.2f", gLaneOutput_[ n16LaneNo ].rRawFlowRate );
	}
	
	return ( szBuffer );
}

char * MovaSatFlowTest_GetSmoothedFlowRate_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	if ( gLaneOutput_[ n16LaneNo ].rSmoothedFlowRate == grFLOW_RATE_INVALID )
	{
		sprintf( szBuffer, gszUNKNOWN_VALUE );
	}
	else
	{
		sprintf( szBuffer, "%.2f", gLaneOutput_[ n16LaneNo ].rSmoothedFlowRate );
	}
	
	return ( szBuffer );
}

char *	MovaSatFlowTest_GetSatFlowTime_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	if ( gLaneOutput_[ n16LaneNo ].n16SatFlowTime == gn16TIME_INVALID )
	{
		sprintf( szBuffer, gszUNKNOWN_VALUE );
	}
	else
	{
		sprintf( szBuffer, "%d", gLaneOutput_[ n16LaneNo ].n16SatFlowTime );
	}
	
	return ( szBuffer );
}
char * MovaSatFlowTest_GetFullySat_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	sprintf( szBuffer, "%s", 
		gLaneOutput_[ n16LaneNo ].lFullySat == TRUE ? "Y" :
		gLaneOutput_[ n16LaneNo ].lFullySat == FALSE ? "N" : gszUNKNOWN_VALUE );
	
	return ( szBuffer );
}
	
char * MovaSatFlowTest_GetQueueOverXDet_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	sprintf( szBuffer, "%s", 
		gLaneOutput_[ n16LaneNo ].lQueueOverXDet == TRUE ? "Y" :
		gLaneOutput_[ n16LaneNo ].lQueueOverXDet == FALSE ? "N" : gszUNKNOWN_VALUE );

	return ( szBuffer );
}
	
char * MovaSatFlowTest_GetINDetStandingQueue_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	sprintf( szBuffer, "%s", 
		gLaneOutput_[ n16LaneNo ].lINDetStandingQueue == TRUE ? "Y" :
		gLaneOutput_[ n16LaneNo ].lINDetStandingQueue == FALSE ? "N" : gszUNKNOWN_VALUE );
	
	return ( szBuffer );
}
	
char * MovaSatFlowTest_GetIsSatFlowAfterVarMinGreen_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	sprintf( szBuffer, "%s", 
		gLaneOutput_[ n16LaneNo ].lIsSatFlowAfterVarMinGreen == TRUE ? "Y" :
		gLaneOutput_[ n16LaneNo ].lIsSatFlowAfterVarMinGreen == FALSE ? "N" : gszUNKNOWN_VALUE );

	return ( szBuffer );
}
	
char * MovaSatFlowTest_GetIsExitBlocked_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );
	
	sprintf( szBuffer, "%s", 
		gLaneOutput_[ n16LaneNo ].lIsExitBlocked == TRUE ? "Y" :
		gLaneOutput_[ n16LaneNo ].lIsExitBlocked == FALSE ? "N" : gszUNKNOWN_VALUE );

	return ( szBuffer );
}

char * MovaSatFlowTest_GetSQMethodUsed_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	sprintf( szBuffer, "%s", 
		gLaneOutput_[ n16LaneNo ].lShortQueueMethodUsed == TRUE ? "Y" :
		gLaneOutput_[ n16LaneNo ].lShortQueueMethodUsed == FALSE ? "N" : gszUNKNOWN_VALUE );

	return ( szBuffer );
}

char * MovaSatFlowTest_GetLQMethodUsed_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );
	
	sprintf( szBuffer, "%s", 
		gLaneOutput_[ n16LaneNo ].lLongQueueMethodUsed == TRUE ? "Y" :
		gLaneOutput_[ n16LaneNo ].lLongQueueMethodUsed == FALSE ? "N" : gszUNKNOWN_VALUE );

	return ( szBuffer );
}

char * MovaSatFlowTest_GetIsSatFlowDetFaulty_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	sprintf( szBuffer, "%s", 
		gLaneOutput_[ n16LaneNo ].lIsSatFlowDetFaulty == TRUE ? "Y" :
		gLaneOutput_[ n16LaneNo ].lIsSatFlowDetFaulty == FALSE ? "N" : gszUNKNOWN_VALUE );
	
	return ( szBuffer );
}

char * MovaSatFlowTest_GetInWarmUpPeriod_( char * const szBuffer, const int16 n16LaneNo )
{
	KDEBUG_EVAL_EXPRESSION( SF, Math_IsWithinRange( n16LaneNo, 0, gSatData.n16LanesNum - 1 ) );
	NULL_POINTER_CHECK( SF, szBuffer );

	sprintf( szBuffer, "%s", 
		gLaneOutput_[ n16LaneNo ].lInWarmUpPeriod == TRUE ? "Y" :
		gLaneOutput_[ n16LaneNo ].lInWarmUpPeriod == FALSE ? "N" : gszUNKNOWN_VALUE );

	return ( szBuffer );
}

char * MovaSatFlowTest_GetCurrentTime_( char * const szBuffer )
{
    TIMESTAMP_TYPE tTimeDate;

    NULL_POINTER_CHECK( SF,szBuffer);

    tTimeDate = Com_read_rtc(READ_RTC);

    sprintf(szBuffer, "%.2ld:%.2ld:%.2ld", tTimeDate.hours, tTimeDate.minutes, tTimeDate.seconds);
    return (szBuffer);

}

char * MovaSatFlowTest_GetCurrentDate_( char * const szBuffer )
{
	TIMESTAMP_TYPE tTimeDate;

	NULL_POINTER_CHECK( SF,szBuffer);

	tTimeDate = Com_read_rtc(READ_RTC);

	sprintf(szBuffer, "%.2ld/%.2ld/%.2ld", tTimeDate.date, tTimeDate.month, tTimeDate.year);
	
	return (szBuffer);

}

REAL MovaSatFlowTest_GetSmoothedFlowLastCycle(const int16 laneIdx)
{
	if (gLaneOutput_[laneIdx].rLongQueueSatFlow > 0)
	{
		return gLaneOutput_[laneIdx].rLongQueueSatFlow;
	}
	else if (gLaneOutput_[laneIdx].rShortQueueSatFlow > 0)
	{
		return gLaneOutput_[laneIdx].rShortQueueSatFlow;
	}
	else
	{
		return -1.0;
	}
}

	

	

#endif /* defined (TEST_OUTPUT) */


/*************************
*	EOF MovaSatFlow.c
**************************/
