/**
 * @file
 *
 * The MOVA UI API that details interface functions that may be used to 
 * implement GUI clients.
 *
 * @author Peter Anderson
 * @version 1.0
 * @date   2013-11-20; Peter Anderson; Initial
 * @copyright  Copyright TRL Ltd 2013. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
 *
 */
#ifndef _ALERTS_H_
#define _ALERTS_H_

#include "defines.h"
#include "time.h"
#include "MVTypes.h"

 /** @defgroup Alerts Alerts Management
 *  This category is associated to setting the alert trigger status and
 *  reading the current status of the alerts.
 *  @{
 */

/** @} */

/** @defgroup ControllerIO Controller Input Output
 *  This category is associated to input and output controller data such as 
 *  confirmation and detector bits.
 *  @{
 */

/** @} */

/** @defgroup DataSet Dataset Management
 *  This category is associated to downloading and obtaining data related to
 *  the available datasets
 *  
 *  @{
 */

/** @} */

/** @defgroup ErrorLog Error Log Management
 *  This category is associated to displaying the error log.
 *  
 *  @{
 */

/** @} */

/** @defgroup MessageLog Message Log Management
 *  This category is associated to displaying the message log.
 *  
 *  @{
 */

/** @} */

/** @defgroup Misc Miscellanous Utilities Management
 *  This category is associated to general utilities such as current time, 
 *  number of links, number of lanes, etc
 *  @{
 */

/** @} */

 /** @defgroup SaturationFlow Saturation Flow Management
 *  This category is associated to the saturation flow details
 *  
 *  @{
 */

/** @} */

/** @defgroup TMA Time Management Act Management
 *  This category is associated to the Traffic Management act and as such 
 *  deals with functions such as lane flow and pedestrian service.
 *  @{
 */

/** @} */

/**
* Constants used by interface functions
*/

/* Maximum length of dataset detail string */
#define MAX_DATASET_DETAIL 40

/*
* Types used by interface functions
*/

/*
 *
 * Data associated to a particular repository area.
 *
*/
typedef struct UI_DataSetDetails_struct
{
	MVBool DataValid;
	char Fname[ MAX_DATASET_DETAIL ]; 
	char Version[ MAX_DATASET_DETAIL ]; 
	char Time[ MAX_DATASET_DETAIL ]; 
	char Date[ MAX_DATASET_DETAIL ]; 
	char Title[ MAX_DATASET_DETAIL ];
	MVByte NoOfDetectors;
	MVByte Scan; /**< Cycle Scan time in 1/10 seconds */
	MVInt MinExt;
	MVInt MaxExt; /**< Maximum extension for delay/stops optimisation */
	MVByte AddGap;
	MVByte SubGap;
	MVByte NoOfStages;
	MVByte NoOfLinks;
	MVByte NoOfLanes;
	MVInt ChecksumComplete;
	MVInt ChecksumFixedData;
} UI_DataSetDetails;

/*
 *
 * The result of an attempt to load from a repository
 * Do not change these enumerations without changing the  corresponding type
 * in the datahand.h
*/
typedef enum
{
	UI_NO_LOADING_ERRORS,
	UI_PLAN_NUMBER_OUT_OF_RANGE,
	UI_REQUESTED_REPO_HAS_NO_DATA,
	UI_ACTUAL_REQUESTED_PLANS_DIFFER,
	UI_TOD_POINTS_TO_EMPTY_PLAN,
	UI_MOVA_ON_CONTROL_AND_INCOMPAT_PLAN_REQ
} UI_LoadDataSetResult;

/*
 * The type associated to listing the site Data by stage.
 * For more information on particular enumerations please see the corresponding
 * description in Application Guide 45; Guide to MOVA Data Set-Up and Use
 * Do not change these enumerations without changing the  corresponding type
 * in the datahand.h
 * Do not change the order of these enums as there is an internal dependency
*/
typedef enum
{
	UI_DATA_SET_SITE_TYPE_STAGE_MIN_GREEN,
	UI_DATA_SET_SITE_TYPE_STAGE_MAX_GREEN,
	UI_DATA_SET_SITE_TYPE_STAGE_LOW_MAX_GREEN,
	UI_DATA_SET_SITE_TYPE_STAGE_PEDMAX_1,
	UI_DATA_SET_SITE_TYPE_STAGE_PEDMAX_2,
	UI_DATA_SET_SITE_TYPE_STAGE_OCC_ALERT_ON,
	UI_DATA_SET_SITE_TYPE_STAGE_OCC_ALERT_OFF,
	UI_DATA_SET_SITE_TYPE_STAGE_PFACIL,
	UI_DATA_SET_SITE_TYPE_STAGE_EMXMAX,
	UI_DATA_SET_SITE_TYPE_STAGE_PRXMAX,
	UI_DATA_SET_SITE_TYPE_STAGE_MAX
} UI_DataSetStageSiteType;

/*
 * The type associated to listing the site Data by indexed stage
 * For more information on the stage change matrix please see the corresponding
 * description in Application Guide 45; Guide to MOVA Data Set-Up and Use
 * Do not change these enumerations without changing the  corresponding type
 * in the datahand.h
 * Do not change the order of these enums as there is an internal dependency
*/
typedef enum
{
	UI_DATA_SET_SITE_TYPE_INDEXED_STAGE_CHANGE,
	UI_DATA_SET_SITE_TYPE_INDEXED_STAGE_MAX
} UI_DataSetIndexedStageSiteType;

/*
 *
 * The type associated to listing the site Data by link.
 * For more information on particular enumerations please see the corresponding
 * description in Application Guide 45; Guide to MOVA Data Set-Up and Use
 * Do not change these enumerations without changing the  corresponding type
 * in the datahand.h
*/
typedef enum
{
	UI_DATA_SET_SITE_TYPE_LINK_LTYPE,
	UI_DATA_SET_SITE_TYPE_LINK_LMIN,
	UI_DATA_SET_SITE_TYPE_LINK_LLATOT,
	UI_DATA_SET_SITE_TYPE_LINK_ESLMAX,
	UI_DATA_SET_SITE_TYPE_LINK_LOSTIM,
	UI_DATA_SET_SITE_TYPE_LINK_STOPEN,
	UI_DATA_SET_SITE_TYPE_LINK_WAITCH,
	UI_DATA_SET_SITE_TYPE_LINK_HOLDTM,
	UI_DATA_SET_SITE_TYPE_LINK_CANDET,
	UI_DATA_SET_SITE_TYPE_LINK_BONTIM,
	UI_DATA_SET_SITE_TYPE_LINK_BONCUT,
	UI_DATA_SET_SITE_TYPE_LINK_MIXOUT,
	UI_DATA_SET_SITE_TYPE_LINK_SHORTL,
	UI_DATA_SET_SITE_TYPE_LINK_NMAINL,
	UI_DATA_SET_SITE_TYPE_LINK_MAX
} UI_DataSetLinkSiteType;


/*
 *
 * The type associated to listing the site Data by indexed link.
 * For more information on particular enumerations please see the corresponding
 * description in Application Guide 45; Guide to MOVA Data Set-Up and Use
 * Do not change these enumerations without changing the  corresponding type
 * in the datahand.h
*/
typedef enum
{
	UI_DATA_SET_SITE_TYPE_INDEXED_LINK_LGREEN,
	UI_DATA_SET_SITE_TYPE_INDEXED_LINK_SDCODE,
	UI_DATA_SET_SITE_TYPE_INDEXED_LINK_LANES,
	UI_DATA_SET_SITE_TYPE_INDEXED_LINK_EP_DET,
	UI_DATA_SET_SITE_TYPE_INDEXED_LINK_EP_EXT,
	UI_DATA_SET_SITE_TYPE_INDEXED_LINK_MAX
} UI_DataSetIndexedLinkSiteType;

/*
 * The type associated to listing the site Data by lane
 * For more information on particular enumerations please see the corresponding
 * description in Application Guide 45; Guide to MOVA Data Set-Up and Use
 * Do not change these enumerations without changing the  corresponding type
 * in the datahand.h
 * Do not change the order of these enums as there is an internal dependency
*/
typedef enum
{
	UI_DATA_SET_SITE_TYPE_LANE_IN_DET,
	UI_DATA_SET_SITE_TYPE_LANE_X_DET,
	UI_DATA_SET_SITE_TYPE_LANE_OUT_DET,
	UI_DATA_SET_SITE_TYPE_LANE_IN_SINK_DET,
	UI_DATA_SET_SITE_TYPE_LANE_COMB_X_DET,
	UI_DATA_SET_SITE_TYPE_LANE_X_SINK_DET,
	UI_DATA_SET_SITE_TYPE_LANE_ALT_UP_X_DET,
	UI_DATA_SET_SITE_TYPE_LANE_ALT_DOWN_X_DET,
	UI_DATA_SET_SITE_TYPE_LANE_STOP_LINE_DET,
	UI_DATA_SET_SITE_TYPE_LANE_ASSOC_DET_1_DET,
	UI_DATA_SET_SITE_TYPE_LANE_ASSOC_DET_2_DET,
	UI_DATA_SET_SITE_TYPE_LANE_CSPEED,
	UI_DATA_SET_SITE_TYPE_LANE_SATINC,
	UI_DATA_SET_SITE_TYPE_LANE_STLOST,
	UI_DATA_SET_SITE_TYPE_LANE_LANEWF,
	UI_DATA_SET_SITE_TYPE_LANE_COMTIM,
	UI_DATA_SET_SITE_TYPE_LANE_USESF,
	UI_DATA_SET_SITE_TYPE_LANE_USEST,
	UI_DATA_SET_SITE_TYPE_LANE_BONBC,
	UI_DATA_SET_SITE_TYPE_LANE_EXITAL,
	UI_DATA_SET_SITE_TYPE_LANE_MAXMIN,
	UI_DATA_SET_SITE_TYPE_LANE_CRUSIN,
	UI_DATA_SET_SITE_TYPE_LANE_CRUSX,
	UI_DATA_SET_SITE_TYPE_LANE_MOVQIN,
	UI_DATA_SET_SITE_TYPE_LANE_MOVEQX,
	UI_DATA_SET_SITE_TYPE_LANE_QMINON,
	UI_DATA_SET_SITE_TYPE_LANE_XOSAT,
	UI_DATA_SET_SITE_TYPE_LANE_OSATTM,
	UI_DATA_SET_SITE_TYPE_LANE_OSATCC,
	UI_DATA_SET_SITE_TYPE_LANE_GAMBER,
	UI_DATA_SET_SITE_TYPE_LANE_SATGAP,
	UI_DATA_SET_SITE_TYPE_LANE_CRITG,
	UI_DATA_SET_SITE_TYPE_LANE_OSATCX,
	UI_DATA_SET_SITE_TYPE_LANE_OSATAL,
	UI_DATA_SET_SITE_TYPE_LANE_MAX
} UI_DataSetLaneSiteType;

/*
 *
 * This type returns the error status when receiving site data.
 * Do not change these enumerations without changing the  corresponding type
 * in the datahand.h
*/
typedef enum
{
	UI_RX_DATA_CHECKSUM_FAILED,
	UI_RX_DATA_OFFSET_EXCEEDED,
	UI_RX_DATA_BASIC_CHECKS_FAILED,
	UI_RX_DATA_STRICT_CHECKS_FAILED,
	UI_RX_DATA_DATASET_COMPATIBILITY_FAILURE,
	UI_RX_DATA_DATASET_NOT_AVAILABLE,
	UI_RX_DATA_INVOKED_WRONG_STATE,
	UI_RX_DATA_NO_ERROR,
	UI_RX_DATA_MAX
} UI_RxSiteDataStatusType;

/*
 *
 * Data associated to the active dataset.
 *
*/
typedef struct UI_ActiveDataDetails_struct
{
	MVByte CurrentlyActiveDataSet;
	MVBool DataTriggerStatus;
	MVBool TimeOfDayActive;
	MVInt PlanChangePending;
} UI_ActiveDataDetails;


/*
 *
 * Faults generated when performing checks after loading a dataset into the
 * temp area but before loading it into a Repository slot.
 * This maps on to a type internally do not change this without changing the 
 * corresponding type.
*/
typedef enum
{
	UI_DATA_SET_STRICT_FAIL_NONE = 0,
	UI_DATA_SET_STRICT_FAIL_NO_ACTIVE_DATASET = 1,
	UI_DATA_SET_STRICT_FAIL_BASIC = 2,
	UI_DATA_SET_STRICT_FAIL_LGREEN = 4,
	UI_DATA_SET_STRICT_FAIL_CHANGE_MATRIX_MUST_REMAIN_MINUS_ONE = 8,
	UI_DATA_SET_STRICT_FAIL_CHANGE_MATRIX_CANNOT_CHANGE_MINUS_ONE = 16,
	UI_DATA_SET_STRICT_FAIL_SD_CODE_MUST_REMAIN_ZERO = 32,
	UI_DATA_SET_STRICT_FAIL_SD_CODE_MUST_REMAIN_NON_ZERO = 64,
	UI_DATA_SET_STRICT_FAIL_SHORT_LANE_CHANGED = 128,
	UI_DATA_SET_STRICT_FAIL_LANES_ON_LINK_CHANGED = 256,
	UI_DATA_SET_STRICT_FAIL_MAIN_LANE_ON_LINK_CHANGED = 512,
	UI_DATA_SET_STRICT_FAIL_LANES_NOS_LINK_CHANGED = 1024,
	UI_DATA_SET_STRICT_FAIL_MIXOUT_CHANGED = 2048,
	UI_DATA_SET_STRICT_FAIL_MORE_THAN_TWO_DET_CHANGED = 4096,
	UI_DATA_SET_STRICT_FAIL_MORE_THAN_10_CHANGES = 8192
} UI_RxSiteStrictCheckFail;

/**
 * @ingroup    Alerts
 *
 * Sets the Blocking, OverSaturation and Occupancy statuses to be monitored.
 *
 * @param [in] BlockingStatus Set/clear the monitoring status for the blocking 
 * monitor
 * @param [in] OverSaturationStatus Set/clear the monitoring status for the over
 * saturation monitor
 * @param [in] OccupancyStatus Set/clear the monitoring status for the occupancy
 * monitor
 *
 * @retval Void
 *
*/

void UI_SetAlertMonitoring(MVBool BlockingStatus, MVBool OccupancyStatus,
	MVBool OverSaturationStatus);

/**
 * @ingroup    Alerts
 *
 * Determine whether the Blocking, OverSaturation and Occupancy statuses are 
 * being monitored.
 *
 * @param [out] BlockingStatus  Whether the blocking trigger is being monitored.
 * @param [out] OverSaturationStatus  Whether the over saturation trigger is 
 * being monitored.
 * @param [out] OccupancyStatus  Whether the occupancy trigger is being 
 * monitored.
 *
 * @retval Void
 *
*/

void UI_GetAlertMonitoring(MVBool *BlockingStatus, MVBool *OverSaturationStatus, 
	MVBool *OccupancyStatus);

/**
 * @ingroup    Alerts
 *
 * Obtain the curent status of the Blocking, OverSaturation and Occupancy alerts
 * 
 * @param [out] BlockingAlert  The current status of the blocking alert.
 * @param [out] OverSaturationAlert The current status of the over saturation 
 * alert.
 * @param [out] OccupancyAlert The current status of the occupancy alert.
 *
 * @retval Void
 *
*/

void UI_GetAlertStatus(MVBool *BlockingAlert, MVBool *OverSaturationAlert, 
	MVBool *OccupancyAlert);

/**
 * @ingroup    Misc
 *
 * Get the current time from MOVA.
 *
 * @retval Time The current time from MOVA .
 *
*/
time_t UI_GetMovaTime( void );

/**
 * @ingroup    Misc
 *
 * Set the current time in MOVA.
 *
 * @param [in] CurrentTime  The time to set MOVA to.
 *
 * @retval time The time returned after the set operation. If an invalid time 
 * is input this function returns the current time.
 *
*/
time_t UI_SetMovaTime( time_t* CurrentTime );
/**
 *
 * @ingroup    Misc
 *
 * Get the current version from MOVA.
 *
 * @retval Version Null terminated char array 10 bytes big.
 *
*/

char* UI_GetMovaVersion(void);

/**
 * @ingroup    Misc
 *
 * The periodic data is stored in a circular buffer. This function returns the
 * first period the last period and maximum number of periods.
 *
 * @param [out] FirstPeriodInstance The current entry in the circular buffer for
 * the first entry
 * @param [out] LastPeriodInstance The current entry in the circular buffer for 
 * the last entry
 * @param [out] MaxLogs The size of the circular buffer.
 *
 * @retval Void
 *
*/

void UI_GetPeriodDetails(MVInt16* FirstPeriodInstance, MVInt16* LastPeriodInstance, 
	MVInt16* MaxLogs);

/**
 * @ingroup    Misc
 *
 * Obtain data stored in various system flags.
 * 
 *
 * @param [out] ErrorCount The current error total.
 * @param [out] PhoneHome The current status of the home phone flag 
 * @param [out] OnControl Whether MOVA is on control
 * @param [out] ControllerReady Whether the controller is ready
 * @param [out] StageDemanded The stage demanded by MOVA
 * @param [out] VAMova Whether the junction will run VA or MOVA
 * @param [out] PhoneConnected The phone connection status
 * @param [out] MultiStageConfirm Whether multi stage confirms have been 
 * detected
 * @param [out] PhoneCount Counter used when phone connected
 * @param [out] WarmUpCount The warm up counter incremented at the start-
 * green for each stage 
 *
 * @note See the appropriate section of AG45 which describes the range for each
 * flag
 *
 * @retval Void
 *
*/

void UI_GetFlagDetails(MVByte* ErrorCount, MVByte* PhoneHome, MVBool* OnControl, 
	MVBool* ControllerReady, MVByte* StageDemanded, MVBool* VAMova, MVBool* PhoneConnected,
	MVBool* MultiStageConfirm, MVInt* PhoneCount, MVInt* WarmUpCount);

/**
 * @ingroup    Misc
 *
 * Set various system flags.
 * 
 * @param [in] ErrorCount The current error total.
 * @param [in] PhoneHome The current status of the home phone flag 
 * @param [in] OnControl Whether MOVA is on control
 * @param [in] VAMova Whether the junction will run VA or MOVA
 *
 * @note See the appropriate section of AG45 which describes the range for each
 * flag
 *
 * @retval Void
 *
*/

void UI_SetFlagDetails(MVByte ErrorCount, MVByte PhoneHome, MVBool OnControl, 
	MVBool VAMova);

/**
 * @ingroup    TMA
 *
 * Obtain the flow information for a lane during a particular period
 * 
 * @param [in] LaneNo The lane number in question
 * @param [in] PeriodInstance The period in question
 * @param [out] time The time at which the period occurred
 * @param [out] xFlow The flow for the X detector during the period
 * @param [out] inFlow The flow for the IN detector during the period
 * @param [out] xSuspect If the X detector has failed during the period \n
 * Possible values:\n
 * 0 : No Fault \n
 * 1 : Active Fault \n
 * 2 : InActive Fault
 * @param [out] inSuspect If the IN detector has failed during the period \n
 * Possible values:\n
 * 0 : No Fault \n
 * 1 : Active Fault \n
 * 2 : InActive Fault
 * @retval Void
 *
*/

void UI_GetLaneFlowLogDetails( MVByte LaneNo, MVInt PeriodInstance, time_t* time, 
	MVInt* xFlow, MVInt* inFlow, MVByte* xSuspect, MVByte* inSuspect);

/**
 * @ingroup    TMA
 *
 * The faulty detector data is stored in a circular buffer. This function returns
 * the first detector fault, last detector fault and maximum number of faults.
 *
 * @param [out] FirstFaultyDetector The current entry in the circular buffer for
 *  the first entry
 * @param [out] LastFaultyDetector The current entry in the circular buffer for 
 * the last entry
 * @param [out] MaxDetectors The size of the circular buffer.
 *
 * @retval Void
 *
*/

void UI_GetSuspectDetectorIndex( MVInt* FirstFaultyDetector, 
	MVInt* LastFaultyDetector, MVInt* MaxDetectors);

/**
 * @ingroup    TMA
 *
 * Obtain the information for a particular faulty detector.
 * 
 * @param [in] FaultInstance The fault in question
 * @param [out] DetectorID The ID of the faulty detector
 * @param [out] FaultType The type of fault \n
 * Possible values:\n
 * 0 : No Fault \n
 * 1 : Long Off \n
 * 2 : Long On
 * @param [out] ReturnedToWorking Detector was faulty and has returned to 
 * working.
 * @param [out] TimeFaulty The time fault occurred first
 * @param [out] TimeReturnedWork The time when detector returned to working 
 * state.
 *
 * @retval Void
 *
*/

void UI_GetSuspectDetectorLogDetails( MVInt FaultInstance, MVByte* DetectorID, 
	MVByte* FaultType, MVBool* ReturnedToWorking, time_t* TimeFaulty, 
	time_t* TimeReturnedWork );

/**
 * @ingroup    TMA
 *
 * Obtain pedestrian service log details.
 * 
 * @param [in] PeriodInstance The period in question
 * @param [out] Time The period time
 * @param [out] Mean The mean demand to service time in seconds
 * @param [out] StandDev The standard deviation of the demand to service time
 * @param [out] NoOfServices The number of pedestrian stages/phases which 
 * indicate level of service
 * @param [out] Sum The total demand to service time for this period
 * @param [out] SumSquared The sum of all the squares of the demand to service 
 * times for this period

 *
 * @retval Void
 *
*/

void UI_GetPedestrianSeviceLogDetails( MVInt PeriodInstance, time_t* Time, 
	MVInt* Mean, MVInt* StandDev, MVInt* NoOfServices, MVInt* Sum, MVInt* SumSquared );


/**
 * @ingroup    TMA
 *
 * Obtain the oversaturation  information for a lane during a particular period
 * 
 * @param [in] LaneNo The lane in question
 * @param [in] PeriodInstance The period in question
 * @param [out] time The time associated to this period
 * @param [out] overSatCy The number of cycles that over saturation has occurred
 * within the period.
 *
 * @retval Void
 *
*/

void UI_GetOverSatLogDetails( MVByte LaneNo, MVInt PeriodInstance, time_t* time, 
	MVInt* overSatCy);

/**
 * @ingroup    SaturationFlow
 *
 * Obtain the saturation flow information for a lane during a particular period
 * 
 * @param [in] LaneNo The lane in question
 * @param [in] PeriodInstance The period in question
 * @param [out] ESV The exponentially smoothed saturation flow
 * @param [out] Satinc The Saturation flow headway increment. Measured in 1/10s.
 * @param [out] LastMean The most recently measured average saturation flow value
 * @param [out] ConfLevel The 99% confidence level for the most recently 
 * measured sat flow
 * @param [out] StatsValid Whether the most recently measured value met the 
 * statisical criterion
 * @param [out] TotalCount If StatsValid not met the number of measurements of 
 * sat flow made during this period
 * @param [out] Mean Mean value during this period and equivalent periods when 
 * StatsValid not satisfied
 * @param [out] Smoothed15 Mean value during this period and equivalent periods 
 * when StatsValid was satisfied
 * @param [out] SumSquared Sum of squares of SF during this period and 
 * equivalent periods when StatsValid not satisfied
 *
 * @retval Void
 *
*/

void UI_GetSatFlowDetails( MVByte LaneNo, MVInt PeriodInstance, MVInt* ESV, 
	MVInt* Satinc, MVInt* LastMean, MVInt* ConfLevel, MVBool* StatsValid, 
	MVInt* TotalCount, MVInt* Mean, MVInt* Smoothed15, MVInt* SumSquared);

/**
 * @ingroup    SaturationFlow
 *
 * Obtain the start up lost time at the beginning of green (STLOST), for a lane
 * during a particular period
 * 
 * @param [in] LaneNo The lane in question
 * @param [in] PeriodInstance The period in question
 * @param [out] ESV The exponentially smoothed STLOST 
 * @param [out] LastMean The most recently measured average STLOST time 
 * value
 * @param [out] ConfLevel The 99% confidence level for the most recently 
 * measured value
 * @param [out] StatsValid Whether the most recently measured value met the 
 * statisical criterion
 * @param [out] TotalCount If StatsValid not met the number of measurements of 
 * STLOST made during this period
 * @param [out] Mean Mean value during this period and equivalent periods when 
 * StatsValid not satisfied
 * @param [out] SumSquared Sum of squares of STLOST during this period and 
 * equivalent periods when StatsValid not satisfied
 *
 * @retval Void
 *
*/

void UI_GetST_LostDetails( MVByte LaneNo, MVInt PeriodInstance, MVInt* ESV, 
	MVInt* LastMean, MVInt* ConfLevel, MVBool* StatsValid, 
	MVInt* TotalCount, MVInt* Mean, MVInt* SumSquared);

/**
 * @ingroup    SaturationFlow
 *
 * Obtain the start and end times for each saturation flow timing period.
 * A maximum of six timing periods can be defined.
 * 
 * @param [in] RepoArea The repo area 1 to 4 or 0 for active area.
 * @param [in] PeriodInstance The period in question.
 * @param [out] PeriodStart The start time of the period in minutes since 
 * midnight
 * @param [out] PeriodEnd The end time of the period in minutes since midnight
 *
 * @retval Void
 *
*/

void UI_GetSatFlowTimingPeriod(MVInt PeriodInstance, MVInt* PeriodStart, 
	MVInt* PeriodEnd);

/**
 * @ingroup    TMA
 *
 * Obtain the traffic service log  information for a lane during a particular 
 * period
 * 
 * @param [in] StageNo The stage in question
 * @param [in] PeriodInstance The period in question
 * @param [out] time The time associated to this period
 * @param [out] NoOfServices The number of times the stage has ran
 * @param [out] Mean The average length of the stage in seconds
 * @param [out] lastDataSet The number of the dataset that was running at the 
 * end of the period
 *
 * @retval Void
 *
*/

void UI_GetTrafficServiceLogDetails(MVByte StageNo, MVInt PeriodInstance, 
	time_t* time, MVInt* NoOfServices, MVInt* Mean, MVByte* lastDataSet);

/**
 * @ingroup    TMA
 *
 * Obtain the Occupancy log  information for a stage during a particular period
 * 
 * @param [in] StageNo The stage in question
 * @param [in] PeriodInstance The period in question
 * @param [out] time The time associated to this period
 * @param [out] xInOcc The average occupancy for X and In detectors in the stage in 1/100s
 * @param [out] xFlow The total x-flow for the stage in 1/100s
 * @param [out] xInSuspect Indicate suspect detector during the period \n
 * Possible values: \n
 * 0 No Fault \n
 * 1 Active Fault \n
 * 2 InActive Fault \n
 * 
 * @retval Void
 *
*/

void UI_GetOccupancyLogDetails(MVByte StageNo, MVInt PeriodInstance, time_t* time, 
	MVInt* xInOcc, MVInt* xFlow, MVBool* xInSuspect);

/**
 * @ingroup    TMA
 *
 * Obtain the TMA period.
 * 
 * @retval Period TMA Period in minutes
 *
*/
MVInt UI_GetTMAprd(void);

/**
 * @ingroup    TMA
 *
 * Obtain the End Sat log  information for a stage during a particular period
 * 
 * @param [in] StageNo The stage in question
 * @param [in] PeriodInstance The period in question
 * @param [out] time The time associated to this period
 * @param [out] StageNoEndSat The stage number most associated with end sat
 * @param [out] linkNo Link to go end sat in stage 
 * @param [out] endSatReason End sat decision
 *
 * @retval Void
 *
*/
void UI_GetEndSatLogDetails(MVByte StageNo, MVInt PeriodInstance, time_t* time, 
	MVByte* StageNoEndSat, MVByte* linkNo, MVByte* endSatReason);

/**
 * @ingroup    Misc
 *
 * Get the number of lanes for the junction
 * 
 * @retval NumLanes Number of Lanes.
 *
*/

MVByte UI_GetNumLanes(void);

/**
 * @ingroup    Misc
 *
 * Get the number of stages for the junction
 * 
 * @retval NumStages Number of Stages.
 *
*/

MVByte UI_GetNumStages(void);


/**
 * @ingroup    Misc
 *
 * Get the number of detectors for the junction
 * 
 * @retval NumDetectors Number of detectors.
 *
*/

MVByte UI_GetNumDetectors(void);


/**
 * @ingroup    Misc
 *
 * Gets the number of pedestrian stages and their numbers
 * 
 * @param [out] NumPedStage The number of pedestrian stages
 * @param [out] PedStage1 The stage number of the first pedestrian stage
 * @param [out] PedStage2 The stage number of the second pedestrian stage
 *
 * @retval Void
 *
*/

void UI_GetPedDetectorConfiguration(MVByte *NumPedStage, MVByte *PedStage1, 
	MVByte *PedStage2);

/**
 * @ingroup    Misc
 *
 * Get the number of links for the junction
 * 
 * @retval NumLinks Number of Links.
 *
*/

MVByte UI_GetNumLinks(void);

/**
 * @ingroup    ErrorLog
 *
 * Clear the error log.
 * 
 * @retval Void
 *
*/

void UI_ClearErrorLog(void);

/**
 * @ingroup    ErrorLog
 *
 * Get the total number of errors.
 * 
 * @retval NoOfErrors The total number of errors
 *
*/

MVInt UI_GetErrorCount(void);

/**
 * @ingroup    ErrorLog
 *
 * Get the details of a particular error.
 *
 * @param [in] ErrorIndex The Error in question
 * @param [out] ErrorID The ID of the error
 * @param [out] time The time the error occurred.
 * @param [out] add1 The first additional data field
 * @param [out] add2 The second additional data field
 * @param [out] add3 The third additional data field
 * 
 * @note See the Fault Details Section of this document for further information.
 *
 * @retval Void
 *
*/

void UI_GetError(MVInt ErrorIndex, MVByte* ErrorID, time_t* time, MVInt* add1, 
	MVInt* add2, MVInt* add3);

/**
 * @ingroup    Misc
 *
 * Get the Average Flow details.
 *
 * @param [in] Day The day in question Monday = 1, Sunday = 7
 * @param [in] TimeIncrement The time increment currently in half hours, 
 * index starts at 1
 * @param [in] Lane The Lane in question
 *
 * @retval AverageFlow The average flow
 *
*/

MVInt UI_GetAverageFlow(MVByte Day, MVByte TimeIncrement, MVByte Lane);

/**
 * @ingroup    ControllerIO
 *
 * Get the force bits
 *
 * @param [out] ForceBits The maximum of ten force bits.
 * 
 * @retval Void
 *
*/

void UI_GetForceBits(MVBool* ForceBits);


/**
 * @ingroup    ControllerIO
 *
 * Get the confirmation bits
 *
 * @param [out] ConfBits The maximum of 32 force bits.
 * 
 * @retval Void
 *
*/
void UI_GetConfirmationBits(MVBool* ConfBits);

/**
 * @ingroup    ControllerIO
 *
 * Get the takeover status
 *
 * @retval int takeover status TRUE/FALSE
 *
*/

MVBool UI_GetTakeOverStatus(void);


/**
* @ingroup    ControllerIO
*
* Get the hurry inhibit (HI) status
*
* @retval int hurry inhibit status TRUE/FALSE
*
*/
MVBool UI_GetHurryInhibitStatus(void);


/**
* @ingroup    ControllerIO
*
* Get the output channels
*
* @param [out] OutChan The number of output channels defined by NumberofOutputChans.
*
* @retval Void
*
*/
void UI_GetOutputChannels(MVBool* OutChan);


/**
 * @ingroup    MessageLog
 *
 * Get a message from the message log.
 *
 * @param [in] MessageIndex The message ID in question.
 * @param [out] MessageDetails The message details
 * 
 * @note See AG45 for more information on explicit message details
 * @note Default Message size is 800 bytes 
 * @note The client needs to allocate memory for the MessageDetails element.
 *
 * @retval Void
 *
*/

void UI_GetMessage(MVInt MessageIndex, MVInt* MessageDetails );

/**
* @ingroup    MessageLog
*
* Sets the flag that indicates that the message has been read by the comm app.
*
* @param [in] msg_index		The message index.
*
* @retval Void
*
*/
void UI_SetMessageIsReadFlag(uint8 msg_index);


/**
* @ingroup    MessageLog
*
* Query if the message has been read by the comm app.
*
* @param [in] msg_index		The message index.
*
* @retval True if the message was read.
*
*/
MVBool UI_IsMessageRead(uint8 msg_index);

/**
 * @ingroup    MessageLog
 *
 * Get the current number of messages.
 *
 * @note At present this indexes a circular buffer of 50 messages.
 * 
 * @retval NumMessages The current number of messages

*/

MVInt UI_GetMessageCount(void);

/**
 * @ingroup    ControllerIO
 *
 * Sets the status for each detector.
 *
 * @param [in] DetectorStatus The array containing the status for each detector.
 * @param [in] NoOfDetectors The number of detectors.
 *
 * @note If NoOfDetectors exceeds the maximum no detectors are set.
 * 
 * @retval Void

*/

void UI_SetDetectorStatus(MVBool* DetectorStatus, MVByte NoOfDetectors);



/**
* @ingroup    ControllerIO
*
* Gets the raw status for all the detectors (rawdetsin array).
*
* @param [out] RawDetectorsStatus The array containing the raw status for each detector.
*
* @retval Void

*/
void UI_GetRawDetectorsStatus(MVBool* RawDetectorsStatus);


/**
* @ingroup    ControllerIO
*
* Gets the status for all the detectors (detsin array).
*
* @param [out] DetectorsStatus The array containing the status for each detector.
*
* @retval Void

*/
void UI_GetDetectorsStatus(MVBool* DetectorsStatus);


/**
* @ingroup    ControllerIO
*
* Gets the sus status for all the detectors.
*
* @param [out] DetectorsSusStatus The array containing the sus status for each detector.
*
* @retval Void

*/
void UI_GetDetectorsSusStatus(MVInt* DetectorsSusStatus);

/**
 * @ingroup    ControllerIO
 *
 * Sets the status for each stage/phase confirmation .
 *
 * @param [in] ConfirmStatus The array containing the status for each stage/phase confirmation.
 * @param [in] NoOfConfirm The number of confirms.
 *
 * @note If NoOfConfirm exceeds the maximum no confirms are set.
 * 
 * @retval Void

*/

void UI_SetConfirmStatus(MVBool* ConfirmStatus, MVByte NoOfConfirm);

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Gets the information related to a particular dataset stored in the
 * repository
 *
 * @param [in] DataSetNum The number of the dataset, range 1 to 4.
 *
 * @note Returns type UI_DataSetDetails elements detailed below
 *
 * @retval DataValid Whether this repository area is valid
 * @retval Fname The filename that this was loaded from
 * @retval Version The compatible MOVA version number
 * @retval Time The time this file was created
 * @retval Date The date this file was created
 * @retval Title The dataset title
 * @retval NoOfDetectors The number of Detectors
 * @retval Scan The scan time in 1/10s.
 * @retval MinExt The minext time
 * @retval MaxExt The maxext time
 * @retval AddGap The addgap time
 * @retval SubGap The subgap time
 * @retval NoOfStages The number of stages 
 * @retval NoOfLinks The number of links
 * @retval NoOfLanes The number of lanes
 * @retval ChecksumComplete The calculated checksum for the complete data set
 * @retval ChecksumFixedData The calculated checksum for the fixed data set
*/


UI_DataSetDetails UI_Datahand_GetDataSetDetails(MVByte DataSetNum);
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Gets information related to the currently active data.
 *
 * @note Returns type UI_ActiveDataDetails elements detailed below
 *
 * @retval CurrentlyActiveDataSet The repository number of the active dataset
 * @retval DataTriggerStatus Is triggering enabled
 * @retval TimeOfDayActive Is Time of Day active
 * @retval PlanChangePending Is a plan change pending, if so ToD is new plan
 *
*/

UI_ActiveDataDetails UI_Datahand_GetActiveDataDetails(void);
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Gets information relating to whether dataset triggering has been enabled.
 *
 * @param [in] RepoNo The number of the repository, 0 for active 1 to 4 for Repo.
 * @param [out] DataSetTrig1 The detector has been assigned to detector channel 1.
 * @param [out] DataSetTrig2 The detector has been assigned to detector channel 2.
 * @param [out] DataSetTrig3 The detector has been assigned to detector channel 3.
 * @param [out] DataSetTrig4 The detector has been assigned to detector channel 4.
 *
 * @retval void
 *
*/
void UI_Datahand_GetDataSetTriggerStatus(MVByte RepoNo, MVBool* DataSetTrig1, 
	MVBool* DataSetTrig2, MVBool* DataSetTrig3, MVBool* DataSetTrig4);
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Deletes the Time of Day data
 *
 * @retval void
*/

void UI_Datahand_DeleteTimeOfDay(void);
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Enable/disable dataset triggering.
 *
 * @param [in] TriggeringStatus Whether to set or clear data set triggering.
 *
*/

void UI_Datahand_SetDataSetTriggering(MVBool TriggeringStatus);
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Get the predefined error weight values.
 *
 * @param [in] RepoNo The number of the repository, 0 for active 1 to 4 for Repo.
 * @param [out] ErrorIDs  An array of Error IDs that have a predefined weight
 * @param [out] ErrorValues An array containing the associated weight for that ID.
 *
 * @note  For more information on ErrVal please see the corresponding
 * description in Application Guide 45. Currently this has 30 elements.
 *
 * @retval The number of elements that have been read.
 *
*/
MVInt UI_Datahand_GetDataErrVal(MVInt RepoNo, MVInt* ErrorIDs, MVInt* ErrorValues);
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Get the predefined telephone values.
 *
 * @param [in] RepoNo The number of the repository, 0 for active 1 to 4 for Repo.
 * @param [out] Tel  The array of telephone number values.
 *
 * @note  For more information on TelNo please see the corresponding
 * description in Application Guide 45. Currently this has 16 elements.
 *
 * @retval NoOfElements The number of elements that have been read.
 *
*/
MVInt UI_Datahand_GetDataTelNo(MVInt RepoNo, MVInt* Tel);
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Gets information related to the currently active data.
 *
 * @param [in] RepositoryNo The number of the repository to load from, range 1 to 4.
 * @param [in] RebootIfNotComp Attempt a reboot if plan is not compatible
 * @param [in] StillLoadIfNotComp Still Load if plan is not compatible
 * @param [out] ActualPlan The selected plan due to be loaded at the start of stage 1.
 *
 * @note Returns type UI_LoadDataSetResult with values detailed below
 *
 * @retval NO_LOADING_ERRORS The repo has been checked and will be loaded the 
 * next time stage 1 runs
 * @retval PLAN_NUMBER_OUT_OF_RANGE Repo number not in range
 * @retval REQUESTED_REPO_HAS_NO_DATA No data at requested repo
 * @retval ACTUAL_REQUESTED_PLANS_DIFFER Requested and Actual plans differ
 * @retval TOD_POINTS_TO_EMPTY_PLAN Time of Day plan points to empty repo
 * @retval MOVA_ON_CONTROL_AND_INCOMPAT_PLAN_REQ Incompatible repo and MOVA
 * on control therefore cannot load
 *
*/

UI_LoadDataSetResult UI_Datahand_LoadDatafromRepository(MVByte RepositoryNo, 
	MVByte RebootIfNotComp, MVByte StillLoadIfNotComp, MVInt* ActualPlan);
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Gets the data contained within a certain repository. Note this does not 
 * include the header or checksum  this data is returned by function 
 * UI_Datahand_GetDataSetDetails. The amount of data read is specified by a
 * constant in the datahandler, by default it is a maximum of 100 or whatever
 * is available.
 *
 * @param [in] RepoArea The number of the repository, range 1 to 4.
 * @param [in] StartAddress The address to read the data from in the selected dataset.
 * @param [out] DataSetData The data set as a stream of elements.
 *
 * @retval NoOfElements The number of elements written to parameter DataSetData
 *
*/


MVInt UI_Datahand_GetDataSetData(MVInt RepoArea, MVInt StartAddress, MVInt* DataSetData);
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Get link data associated to enumeration SiteType. Each link has an associated
 * element and as such this function will return n elements where n is the 
 * number of links. The link data is stored in a table with more than one 
 * dimension. The second dimension is referenced by the Index parameter.
 *
 * @param [in] RepoArea The number of the repository, range 0 for active or 1 to 4.
 * @param [in] SiteType Data element requested see Application Guide 45 for 
 * detailed description of each enumeration
 * @param [in] Index The table index for this row of link data.
 * @param [out] DataSetSiteData The data returned.
 *
 * @retval NoOfElements The number of elements returned
 *
*/
MVInt UI_Datahand_GetDataSetIndexedLinkSiteData(MVInt RepoArea, UI_DataSetIndexedLinkSiteType SiteType,  MVInt Index, MVInt* DataSetSiteData);
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Get link data associated to enumeration SiteType. Each link has an associated
 * element and as such this function will return n elements where n is the 
 * number of links.
 *
 * @param [in] RepoArea The number of the repository, range 0 for active or 1 to 4.
 * @param [in] SiteType Data element requested see Application Guide 45 for 
 * detailed description of each enumeration 
 * @param [out] DataSetSiteData The data returned.
 *
 * @retval NoOfElements The number of elements returned
 *
*/

MVInt UI_Datahand_GetDataSetLinkSiteData(MVInt RepoArea, UI_DataSetLinkSiteType SiteType, MVInt* DataSetSiteData);
#endif


#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Get lane data associated to enumeration SiteType. Each lane has an associated
 * element and as such this function will return n elements where n is the 
 * number of lanes.
 *
 * @param [in] RepoArea The number of the repository, range 0 for active or 1 to 4.
 * @param [in] SiteType Data element requested see Application Guide 45 for 
 * detailed description of each enumeration
 * @param [out] DataSetLaneSiteData The lane data returned.
 *
 * @retval NoOfElements The number of elements returned
 *
*/


MVInt UI_Datahand_GetDataSetLaneSiteData(MVInt RepoArea, UI_DataSetLaneSiteType SiteType, MVInt* DataSetLaneSiteData);
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Get stage data associated to enumeration SiteType. Each stage has an associated
 * element and as such this function will return n elements where n is the 
 * number of stages.
 *
 * @param [in] RepoArea The number of the repository, range 0 for active or 1 to 4.
 * @param [in] SiteType Data element requested see Application Guide 45 for 
 * detailed description of each enumeration
 * @param [out] DataSetStageSiteData The stage data returned.
 *
 * @retval NoOfElements The number of elements returned
 *
*/


MVInt UI_Datahand_GetDataSetStageSiteData(MVInt RepoArea, UI_DataSetStageSiteType SiteType, MVInt* DataSetStageSiteData);
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Get stage data associated to enumeration SiteType. Each stage has an associated
 * element and as such this function will return n elements where n is the 
 * number of stages. The stage data is stored in a table with more than one 
 * dimension. The second dimension is referenced by the Index parameter.
 *
 * @param [in] RepoArea The number of the repository, range 0 for active or 1 to 4.
 * @param [in] SiteType Data element requested see Application Guide 45 for 
 * detailed description of each enumeration
 * @param [in] Index The table index for this row of stage data.
 * @param [out] DataSetStageSiteData The stage data returned.
 *
 * @retval NoOfElements The number of elements returned
 *
*/


MVInt UI_Datahand_GetDataSetIndexedStageSiteData(MVInt RepoArea, 
	UI_DataSetIndexedStageSiteType SiteType,  MVInt Index, MVInt* DataSetStageSiteData);
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Initialisation at the start of the dataset transfer process
 *
 * @retval void None
 *
*/
void UI_Datahand_RxSiteDataInitialise(void);
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Receive the header data associated to a dataset. These parameters are 
 * strings with the associated lengths below. This function should be called 
 * prior to processing the dataset data and after initialisation.
 *
 * @param [in] Filename The filename associated to this dataset. Length = 30.
 * @param [in] Version The MOVA dataset version number. Length = 8.
 * @param [in] Time The time of creation of this dataset. Length = 6
 * @param [in] Date The date of creation of this dataset.  Length = 9.
 * @param [in] Title The title of this dataset.  Length = 37.
 *
 * @retval Status The returned error status
 *
*/

UI_RxSiteDataStatusType UI_Datahand_RxSiteDataReceiveHeader(char* Filename, 
	char* Version, char* Time, char* Date, char* Title);
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Receive a new dataset. This function transfers the dataset to a temp area
 * where it can be checked before transferring it to a particular repository.
 * This function expects the header data to have been received before it is
 * invoked.
 *
 * @param [in] DataSize The number of elements in the DataSetData parameter.
 * @param [in] DataOffset The offset relative to the start of the dataset that
 * is being written to. This value is zero based.
 * @param [out] DataSetData The data to be written.
 *
 * @note An error will be returned if an offset is supplied that exceeds the 
 * size of the dataset file 
 *
 * @retval Status The returned error status
 *
*/

UI_RxSiteDataStatusType UI_Datahand_RXSiteDataReceiveData(MVInt* DataSetData, MVInt DataSize, 
	MVInt DataOffset);
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Calculate and compare the checksum of the recently transmitted dataset.
 * This should be calculated after the dataset is transmitted to the unit and 
 * before it is transferred into the selected repository.
 *
 * @retval Status Whether the checksum comparison has failed or passed.
 *
*/

UI_RxSiteDataStatusType UI_Datahand_RXSiteDataCalculateChecksumStatus(void);
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Run basic checks on the dataset that has been transferred to MOVA before 
 * transferring to the appropriate repo.
 *
 *
 * @retval Status Whether the basic checks have passed or not.
 *
*/

UI_RxSiteDataStatusType UI_Datahand_RXSiteDataPerformBasicChecks(void);
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Run strict checks on the dataset that has been transferred to MOVA before 
 * transferring to the appropriate repo.
 *
 * @param [out] ErrorCodes The errors that have been detected. They are 
 * described by type UI_RxSiteStrictCheckFail
 *
 * @retval Status Whether the strict checks have passed or not.
 *
*/

UI_RxSiteDataStatusType UI_Datahand_RXSiteDataPerformStrictChecks(UI_RxSiteStrictCheckFail* ErrorCodes);
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Determine whether a dataset has any time of day data and if so what plan
 * number it references. 
 * This function should be invoked after a dataset has been transferred to the
 * temp area and the checksum has been clarified.
 * Before loading a dataset into the active area all repositories referenced in
 * the Time of Day data should have been loaded. This function will aid the 
 * client in this check.
 * In general the target repository area should reference its own Time of Day
 * data. If this is not the case the plan will not run if its own Time of Day
 * data is active. This function will aid the client in this check.
 * 
 *
 * @param [out] FoundTimeOfDay  Has Time of Day data been found
 * @param [out] PlansRef An array of four elements. Each representing if a 
 * plan number has been referenced. Note plans are one based not zero.
 *
 * @retval Status Whether the time of day check was successful or not.
 *
*/
UI_RxSiteDataStatusType UI_Datahand_RXSiteDataGetTimeOfDayData(MVInt* FoundTimeOfDay, MVInt* PlansRef);
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Checks whether a Repository Number is mentioned in the active time of day
 * data. 
 * If the active Time of Day data is not compatible with the dataset to be 
 * loaded into the repository then it can be deleted. 
 *
 *
 * @param [in] RepoArea The repository to check 
 *
 * @retval Status Whether the Repo No is referenced in the active data
 *
*/
MVBool UI_Datahand_RXSiteDataIsRepoRefByActiveArea( MVInt RepoArea );
#endif


#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Deletes the Time of Day data in the active area. This is typically used 
 * when a dataset is to be loaded into an area that the active Time of Day 
 * references.
 *
 * @retval Status Whether the Active ToD data has been deleted.
 *
*/
UI_RxSiteDataStatusType UI_Datahand_RXSiteDataResetActiveToD( void );
#endif


#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * This transfers the dataset from the temp area to a particular repository
 * This requires a dataset to have been downloaded, the checksum to be good
 * and at least the basic checks to have ran.
 * After a successful transfer has been completed the dataset in the temp area
 * is erased.
 *
 * @param [in] RepoNo The repository to transfer to.
 *
 * @retval Status Whether the dataset was transferred successfully.
 *
*/
UI_RxSiteDataStatusType UI_Datahand_RXSiteDataTxRepo( MVInt RepoNo );
#endif


#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Loads the Time of Day data. After loading the data it determines which 
 * plan should currently be running. 
 * This function should be invoked after the dataset has been transferred from
 * the temp area to a repository.
 *
 * @param [in] RepoNo The repository area used.
 * @param [out] FoundNo The plan to be loaded. This may be RepoNo or a plan
 * that was referenced in the Time of Day data that should be currently running.
 *
 * @retval Status Whether the Time of Day data was transferred successfully.
 *
*/
UI_RxSiteDataStatusType UI_Datahand_RXSiteDataLoadToDData( MVInt RepoNo, MVInt* FoundNo );
#endif


#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Reboots the hardware after a dataset has been loaded into a repo area. This
 * is required when we initially load a dataset up when all repos are empty.
 * This is manufacturer specific and we would expect this to change for 
 * different hardware configurations. We would not expect to return from this 
 * function unless a failure is detected.
 *
 * @param [in] RepoNo The repository area to be loaded into the active area.
 * @param [in] IODevice IO channel to use after reboot is performed.
 *
 * @retval Status The failure that has prevented a reboot being performed.
 *
*/
UI_RxSiteDataStatusType UI_Datahand_RXSiteDataRebootInitialRepoLoad( MVInt RepoNo, MVInt IODevice );
#endif

#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Before loading a dataset from a repo area to the active area check that it
 * is compatible with
 * a) Plans referenced in it's Time of Day data
 * b) The currently active dataset.
 * If compatibility checks fail and MOVA is on control then it is not recommended
 * to load up the dataset to the active area.
 * This functionality is required to be ran before a dataset is loaded into the
 * active area unless we are loading a dataset for the first time to the unit.
 *
 * @param [in] RepoNo The repository area to be checked
 * @param [out] OnControl Whether MOVA is on control.
 *
 * @retval Status The result of the compatibility checks.
 *
*/
UI_RxSiteDataStatusType UI_Datahand_RXSiteDataPreLoadRepoCompCheck(int RepoNo, MVBool* OnControl);
#endif


#ifndef M8_ENABLED
/**
 * @ingroup    DataSet
 *
 * Loads the Time of Day data from the repository area provided. 
 * Loads the active area with the dataset from either the repository area 
 * provided or from one specified in the Time of Day data.
 * If MOVA is on control and an incompatible dataset is provided we return a 
 * dataset incompatible error.
 * If MOVA is not on control and the dataset to load is incompatible with the 
 * current active one we recommend a reboot takes place. The parameter 
 * ReBootRequired should be set to enable this. After a reboot has taken place
 * the IO output channel is defined by parameter IODevice.
 * If the Time of Day references a plan to be loaded that is not available in
 * the designated repository an error is returned.
 * If all checks pass the selected dataset will load at the start of stage one.
 *
 * @param [in] RepoNo The repository area to be loaded to the active area
 * @param [in] ReBootRequired If the plan to be loaded is incompatible with the 
 * current active one it is recommended to reboot MOVA. 
 * @param [in] IODevice IO channel to use after reboot is performed.
 * @param [out] SelectedPlan The plan that has been selected to load.
 *
 * @retval Status Whether the load is going to take place or has failed.
 *
*/
UI_RxSiteDataStatusType UI_Datahand_RXSiteDataLoadActiveArea(MVInt RepoNo, 
	MVBool ReBootRequired, MVInt IODevice, MVInt* SelectedPlan);
#endif



/**
* @ingroup    ControllerIO
*
* Gets some parameters about a specific lane. 
*
* @param [in] LaneID The ID of the lane in question.

* @param [out] RedCountIN	 The red count over the IN det (RCIN).
* @param [out] RedCountX	 The red count over the X det (RCX).
* @param [out] SFSmoothed	 The saturation flow smoothed for the lane (SMFLOW).
* @param [out] SFLastCycle	 Last cycle's SF.
* @param [out] ShiftRegister The lane's shift register (SHREGA).
* @param [out] OversatCounter The lane's oversat counter (LAOSAT).
* @param [out] Endsat		 The lane's endsat marker (LAENSA).
* @param [out] ExtraVehBeyondINDET	Number of vehicles beyond the IN-DET in the lane (INXTRA).
* @param [out] OversatInitCount		The number of vehs between the IN-DET (or X-DET if XOSAT=1) and the stop line as start amber early red arrivals (OSATIC).
*
*/
void UI_GetLaneCommData(MVInt8 LaneID, MVInt32 * RedCountIN, MVInt32 * RedCountX, MVInt32 * SFSmoothed, float * SFLastCycle, 
						MVInt32 * ShiftRegister, MVInt32 * OversatCounter, MVInt8 * Endsat,
						MVInt32 * ExtraVehBeyondINDET, MVInt32 * OversatInitCount);



/**
* @ingroup    ControllerIO
*
* Gets some parameters about a specific link.
*
* @param [in] LinkID		The ID of the linkin question.
*
* @param [out] BonusGreenTime		The bonus green time of the link.
* @param [out] SmoothedFlow			The smoothed flow value.
* @param [out] Endsat				The endsat marker value.
* @param [out] DemandType			The demand type marker.
* @param [out] NetBenFlow			The new benifit flow.
* @param [out] ActualFlow			The actual flow.
* @param [out] FutureRedTime		The future red time.
* @param [out] ExtraGreenTime		The extra green time.
* @param [out] EPHoldMarker			The EP hold marker.
* @param [out] EPExtMarker			The EP extention marker.
* @param [out] EPExtTimer			The EP extention timer.
*
*/
void UI_GetLinkCommData(MVInt8 LinkID, MVInt32 * BonusGreenTime, MVInt32 * SmoothedFlow, MVInt8 * Endsat, MVInt8 * DemandType,
	MVInt32 * NetBenFlow, MVInt32 * ActualFlow, MVInt32 * FutureRedTime, MVInt32 * ExtraGreenTime,
	MVInt8 * EPHoldMarker, MVInt8 * EPExtMarker, MVInt32 * EPExtTimer);



/**
* @ingroup    ControllerIO
*
* Gets the current value of the ped demand marker.
*
* @retval the ped demand marker.
*/
MVInt8 UI_GetPedDemandMarker();


/**
* @ingroup    ControllerIO
*
* Sets the value of operation flags.
*
* @param [in] FlagID		The ID of the flag to set (e.g., ON_CONTROL)
* @param [in] NewVlaue		The new value of the flag.
*
* @param [out] FailureReason	The failure reason marker (in case of cannot set the op flag).
*
* @retval flag indicates if the change has been done successfully.
*/
MVBool UI_SetOpFlag(MVInt8 FlagID, MVBool NewVlaue, MVInt8 * FailureReason);


/**
* @ingroup    ControllerIO
*
* Resets the error count.
*
*/
void UI_ResetErrorCount();


/**
* @ingroup    ControllerIO
*
* Force specific stage to run.
*
* @param [in] StageNum		The stage number.
*
* @param [out] FailureReason	The failure reason marker (in case of cannot set the op flag).
*
* @retval flag indicates if the change has been done successfully.
*/

MVBool UI_ForceStage(MVInt StageNum, MVInt8 * FailureReason);


/**
* @ingroup    ControllerIO
*
* Cancels the previously forced stage.
*
*/
void UI_CancelForcedStage();


/**
* @ingroup    DataSet
*
* Activate certain data plan based on the user request.
*
* @param [in] plan_num		data plan number to activate.
*/
MVStatus UI_ActivateDataPlan(MVInt8 plan_num);


/**
* @ingroup    DataSet
*
* Gets the currently active data plan number.
*
* @retval the active data plan number.
*/
MVInt UI_GetActiveDataPlanNumber();

/**
* @ingroup    DataSet
*
* Resets the ToD table based on the user request.
*
*/
void UI_ResetTimeOfDayTable();

/**
* @ingroup    DataSet
*
* Query if the ToD has been removed by the user from the active data plan.
*
* @retval a flag to indicate the query result.
*/
MVBool UI_IsToDRemovedByUser();

/**
* @ingroup    DataSet
*
* Gets the dataset triggering flag status.
*
* @retval the value of the flag.
*/
MVBool UI_GetDatasetTriggering();

/**
* @ingroup    DataSet
*
* Sets the dataset triggering flag.
*
*/
void UI_SetDatasetTriggering(MVBool triggering_status);


/**
* @ingroup    DataSet
*
* Gets the text of a JSON message that holds all the site info based on the active data plan.
*
* @param [out] buff		a buffer that holds the formated message.
*
*/
void UI_GetJsonStringOfDataset(char * buff);

/**
* @ingroup    ControllerIO
*
* Gets the number of lanes count in the running controller.
*
* @retval the number of lanes.
*/
MVUInt8 UI_GetLanesCount();

/**
* @ingroup    ControllerIO
*
* Gets the number of links count in the running controller.
*
* @retval the number of links.
*/
MVUInt8 UI_GetLinksCount();


/**
* @ingroup    ControllerIO
*
* Gets the number of stages count in the running controller.
*
* @retval the number of stages.
*/
MVUInt8 UI_GetStagesCount();

/**
* @ingroup    ControllerIO
*
* Gets the number of detectors count in the running controller.
*
* @retval the number of detectors.
*/
MVUInt8 UI_GetDetsCount();


/**
* @ingroup    ControllerIO
*
* Check if MOVA is on-control.
*
* @retval the flag value.
*/
MVBool UI_IsMOVAOnControl();


/**
* @ingroup    ControllerIO
*
* Get a pointer to the REDPED field. 
*
* @retval the pointer to that field.
*/
MVInt * UI_GetRedPedPtr();

/**
* @ingroup    DataSet
*
* Loads the dataset file, update its content from g_data_area and then write it back
*
* @retval The failure/success of the update operation.
*
*/
MVStatus UI_UpdateDatasetFile_XDS();

/**
* @ingroup    DataSet
*
* Gets info about the dataset file.
*
* @param [out] file_name		the data set file name.
* @param [out] file_size		the data set file size.
* @param [out] controller_id	the running controller ID that has been read from the dataset file.
*
* @retval The failure/success of the operation.
*
*/
MVStatus UI_GetDatasetFileInfo_XDS(char * file_name, MVInt * file_size, MVInt8 * controller_id);



/**
* @ingroup    DataSet
*
* Gets the latest stored file info.
*
* @param [out] file_path_and_name		the dataset file name with the full path.
* @param [out] file_size				the dataset file size.
*
*/
void UI_GetLatestDatasetFileInfo_XDS(char * file_path_and_name, MVInt * file_size);

/**
* @ingroup    DataSet
*
* Gets the running controller ID that has been read from the dataset file.
*
* @retval The ID of the controller.
*
*/
MVInt8 UI_GetRunningControllerID_XDS();

/**
* @ingroup    DataSet
*
* Sets the CRC32 value of the dataset (XML) file.
*/
void UI_SetDatasetFileCRC32_XDS(MVUInt32 crc32);

/**
* @ingroup    DataSet
*
* Loads the dataset from the XML file into memory
*
* @param [in] dataset_file_name		the data set file name.
* @param [in] dp1_index				the index of the first data plan.
* @param [in] dp2_index				the index of the second data plan.
* @param [in] dp3_index				the index of the third data plan.
* @param [in] dp4_index				the index of the fourth data plan.
*
* @retval The failure/success of the operation.
*
*/
MVStatus UI_LoadDatasetFromXMLFileIntoMemeory(char * dataset_file_name, MVInt8 dp1_index, MVInt8 dp2_index, MVInt8 dp3_index, MVInt8 dp4_index);

#endif
