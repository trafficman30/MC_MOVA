/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         sfprv.h
*
*  TITLE:        MOVA Satutation flow calculator - Private Data Data Structures used by Satflow module
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	10-Feb-2003		6.0.0		..		IH		......			Initial implementation for testing with MOVA
*	31-May-2011		7.0.0		AA		PK		CRN003			New Header file inline with MOVA Kernel SQP
*	22-Feb-2012		7.0.3		AB		AK		CRN008			Include defines.h for int16
*	21-Jan-2013		7.0.5		..		PA		CRN025			Reduce memory requirements of satflow module
*	31-Jan-2013		7.0.5		AC		AK		M7.0.5			Issue M7.0.5
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

#if !defined (MOVASATFLOW_PRIVATE_H)
#define MOVASATFLOW_PRIVATE_H


/*************************
*	Includes
**************************/

#include "defines.h"
#include "sftypes.h"
#include "sfstatsprv.h"

/* Ensures that we don't include redefinitions for structs Lane and Detector
 * if SF.h is also included.  N.B. SF.h must be included after SFPrv.h. */
#define __SF_PRIVATE

/*************************
*	Defines
**************************/

/* Maximum number of detector state changes that can be recorded
   during a green */
/* Modify, array size changed from 100 to 1000 */
#define gn16DET_STATE_CHANGES_NUM_MAX     ((int16)1000)


/* Maximum numbers of parts of the junction (from Gbltypes.h) */
#define gn16LANES_NUM_MAX				((int16)mxlane)
#define gn16DETS_NUM_MAX				((int16)mxdets)
#define gn16LINKS_NUM_MAX				((int16)mxlink)
#define gn16STAGES_NUM_MAX				((int16)mxstag)


/* 
	States a detector can be in
	
	Don't typedef as enum's are ints (= size of the system word)
	but we always want them as 16-bit (?).
*/
enum
{
	DET_STATE_INVALID = -1,
	
	DET_STATE_OFF,
	DET_STATE_ON,
	DET_STATE_FAULTY,

	DET_STATE_NUM
};
typedef int16	DET_STATE;


/*
	Mutually exclusive states we're interested in 
	that a lane can be in over time
*/
enum
{
	LANE_STATE_INVALID = -1,

	LANE_STATE_LATE_RED,
	LANE_STATE_MINIMUM_GREEN,
	LANE_STATE_EXTENDED_GREEN,
	LANE_STATE_LEAVING_AMBER,
	LANE_STATE_EARLY_RED,

	LANE_STATE_NUM
};
typedef int16	LANE_STATE;


/*************************
*	Structs
**************************/

typedef struct _Lane
{
	int16		n16Dets_[ DET_TYPE_NUM ]; 
	/* Detector 'IDs' for given detector types in this lane. 
	   Initialised from Ccomshr.det[mxlane][9].
	   Zero indicates that a detector type doesn't exist in this lane.
	*/

	int16		n16SatFlowDetNo;
	/* Detector number (from the Dets_ array) of the detector in this lane
	   being used for saturation flow estimation */
	
	int16		n16StageTime;
	/* Time (tenths of secs) since beginning of stage for this lane */

	int16		n16GreenStartTime;
	/* Actual green start time (tenths of secs) */

	int16		n16GreenEffStartTime;
	/* Effective green start time (after stlost) (tenths of secs) */

	int16		n16VarMinGreenTime;
	/* The variable minimum green time for this lane (same for all lanes within a link) 
	   (tenths of secs) */

	int16		n16MaxVarMinGreenTime;
	/* The maximum variable minimum green time for this lane (same for all lanes within a link) 
	   (tenths of secs) */

	int16		n16LeavingAmberStartTime;
	/* Time (from when lane turned green) that leaving amber occurred (tenths of secs) */

	int16		n16RedStartTime;
	/* Time (from when lane turned green) that red occurred (tenths of secs) */	

	int16		n16XDetQueueSizeMax;
	/* Theoretical maximum number of whole cars that can fit 
	   between the X-det and the stop-line */

	int16		n16XDetQueueSizeCurrent;
	/* The actual (estimated) number of vehicles between the 
	   X-detector and stop-line, recorded at start of green (REDCX) */

	int16		n16MeasurementCriticalGapTime;
	/* Time (tenths of secs) between two vehicles that is used to determine the 
	   the end of saturation flow for sat. flow estimation purposes */

	int16		n16ControlCriticalGapTime;
	/* Time (tenths of secs) between two vehicles that is used to determine the 
	   the end of saturation flow for MOVA junction control purposes (Ccomshr.critg) */

	int16		n16ControlSTLOSTTime;
	/* The STLOST time used by MOVA for control purposes (Ccomshr.stlost) */

	REAL		rSmoothedSatFlowRate;
	/* The current saturation flow rate estimate for this lane, as a smoothed 
	   combination of all the sat. flow rates so far */

	REAL		rSmoothedSTLOSTTime;
	/* The current lost startup (STLOST) time estimate for this lane (tenths of secs), 
	   as a smoothed combination of all the STLOST times so far (this is only 
	   used for lanes with stop-line detectors for which STLOST is calculated) */

	REAL		rSmoothedFlowRate;
	/* The current flow rate for this lane, as a smoothed combination of all the 
	   flow rates so far */

	int16		n16XDetToStoplineCruiseTime;
	/* Cruise time from X-detector to stop-line - used in short queue method 
	   sat. flow estimation (tenths of secs) */

	LANE_STATE	n16CurrentState;
	/* Current state that this lane is in */

	LOGIC		lINQueue;
	/* Flag - whether there is enough vehicles in the lane at start of green
	   to more than fill the distance between the stop-line and X-detector */
	
	int16		n16FlowRateUpdateTime;
	/* The time since the last update of the traffic flow rate (tenths of secs) */

	int16		n16FlowRateVehiclesNum;
	/* The number of vehicles to leave the saturation flow detector 
	   since the last update of the traffic flow rate */

	int16		n16FlowRateDetStateCurrent;
	/* The current state of the detector being used for saturation flow 
	   estimation in this lane */

	LaneSatFlowSTlostPeriod		LaneSatFlowSTlostPeriodData[6];
	/*Period structure containing Lane SatFlow and STLOST data for each period*/
	
	int16		n16LaneNo;
	/* Lane number identifying this lane (0-based) */

	int16		n16LinkNo;
	/* Link number identifying the link this lane is part of (0-based) */

	LOGIC		lIsSatFlowDetFaulty;
	/* Flag recording whether the sat flow detector being used for this lane is faulty 
	   (in which case sat flow estimates will not be updated until the detector is fixed) */

	LOGIC		lIsINDetValid;
	/* Flag recording whether there's a valid IN-detector associated with this lane 
	   - some lanes may only carry small amounts of traffic and will not have an 
	   IN-detector (they're mainly used for checking for over saturation, which won't
	   occur in this circumstance), or an IN-detector may be shared, e.g. by a short
	   lane with associated long lane. An IN-detector is only 'valid' for use by this
	   module if it exists in a lane and is exclusive to that lane. */

	int16		n16StartOfGreenCount;
	/* Number indicating whether a lane has gone through sufficient warmup
	   time for saturation flow estimation to be valid */

} Lane;


/*
	Structure containing saturation flow estimation data associated with detectors
	TODO: ideally, remove irrelevant 'Det' prefix on member var. names
*/
typedef struct _Detector
{
	DET_STATE	n16DetStateCurrent;
	/* Current state this detector is in */

	int16		n16DetChangeCount;
	/* Number of times detector state changed from start of green 
	   to 5 secs into red */

	DET_TYPE	n16DetType;
	/* Type of the detector (X, OUT, STOP) */

	int16		n16DetExitBlockedThreshold;
	/* A detector must be blocked for more than this time (tenths of secs) for the lane
	   to be considered 'exit blocked' */

	REAL		rDetMeanCarOccupancyTime;
	/* The average time spent by each car over the detector (det on time - tenths of seconds)
	   during green when the lane exit isn't blocked. Vehicles with a high occupancy time 
	   (x rLongSlowVehicleDetOccupanyFactor this variable's current value) aren't included 
	   in the average, so this time is intended to reflect detector occupancy for cars 
	   only; lorries/long/slow vehicles are assumed to take over 
	   rLongSlowVehicleDetOccupanyFactor times the time of cars to pass over a detector. */

	int16		n16DetNo;
	/* Detector number identifying this detector (0-based) */

} Detector;

/*
	Information for each detector state change.
*/

typedef struct _TransitionItem
{
	int16		n16DetStateChangeTime_;
	/* Times (from when lane turned green) that state change occurred 
	  (tenths of secs) */

	DET_STATE		n16DetStateNew_;
	/* State the detector was in after state change */

	uint8		n16DetNo;
	/* Detector number identifying this detector (0-based) */

} TransitionItem;


/*
	Internal structure for each detector state change.
*/

typedef struct _DetectorTransition
{
	TransitionItem transitionItem;

	uint16 NextDet;
	/* Pointer to next transition with this ID in the list*/

	uint16 PrevDet;
	/* Pointer to previous transition with this ID in the list*/

} DetectorTransition;

/*
	Main structure for saturation flow estimation data
*/
typedef struct _SatData
{
	Lane		Lane_[ gn16LANES_NUM_MAX ];
	/* Per-lane data required for saturation flow estimation */

	Detector	Detector_[ gn16DETS_NUM_MAX ];
	/* Per-detector data required for saturation flow estimation */

	int16		n16DetsNum;
	/* Total number of detectors in the junction */

	int16		n16LanesNum;
	/* Total number of lanes in the junction */

	int16		n16LinksNum;
	/* Total number of links in the junction */

	int16		n16StagesNum;
	/* Total number of stages for the junction */

	REAL		rSatFlowSmoothingFactor;
	/* User-adjustable exponential smoothing factor, controlling how successive
	   saturation flow estimates are combined */

	REAL		rSTLOSTSmoothingFactor;
	/* User-adjustable exponential smoothing factor, controlling how successive
	   STLOST estimates are combined */

	REAL		rDetOccupancySmoothingFactor;
	/* User-adjustable exponential smoothing factor, controlling how successive
	   detector occupancies for vehicles are combined */

	REAL		rCriticalGapFactor;
	/* User-adjustable factor for scaling MOVA critical gap values to find optimal critical 
	   gaps for saturation flow estimation */

	REAL		rLongSlowVehicleDetOccupanyFactor;
	/* User-adjustable factor used to scale mean (car) detector occupancy 
	   to give an estimate of occupancy time for long/slow vehicles */

	REAL		rFlowRateProportionalErrorMax;
	/* User-adjustable percentage error allowed between flow rate calculated
	   from a STOPLINE detector and flow rate calculated by MOVA from the X-detector. */

	int16		n16StoplineSTLOSTVehiclesNum;
	/* Number of vehicles queueing at a stop-line detector at the start of green
	   that are considered to be unable to cross the detector at saturation 
	   flow rate and are therefore ignored for saturation flow estimation 
	   purposes (these vehicles are used to estimate the STLOST time
	   for stop-line detectors though) */

	int16		n16XDetSTLOSTVehiclesNum;
	/* Number of vehicles queueing at an X-detector at the start of green
	   that are considered to be unable to cross the detector at saturation 
	   flow rate and are therefore ignored for saturation flow estimation 
	   purposes */

} SatData;


#endif /* MOVASATFLOW_PRIVATE_H */
