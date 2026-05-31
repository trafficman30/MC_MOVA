/*******************************************************************************
*
*                      UI DATA ACCESS
*
*  FILE:         ui_dataaccess.c
*
*  TITLE:        UI Data Access
*
*  (c) Copyright TRL Ltd 2014. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal or
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/
#include "ui_dataaccess.h"
#include "obclock.h"
#include "generalfunc.h"
#include "tma_alerts.h"
#include "tma_preprint.h"
#include "errhand.h"
#include "gbltypes.h"
#include "sfstats.h"
#include "kdebug.h"
#include "string.h"
#include "datahand.h"
#include "write.h"
#include "core_interface.h"
#include "delay_and_stops_optimiser.h"
#include "timers_manager.h"
#include "licence.h"
#include "dataset_interface.h"
#include "mova_op.h"
#include "dataset_handler.h"
#include "xml_dataset.h"

// SLM FIXME only here for MOVA_IF, is this still needed?
#ifndef PCMOVA
#include "proj_def.h"
#endif

extern void GetForceBits(char *Control);
extern void SetForceBits(int index, char val);
extern char GetTakeOverStatus(void);
extern char GetHurryInhibitStatus(void);
extern void GetOutputChannels(char *OutChan);
extern void SetDetectorStatus(char* Status, int NumDetectors);
extern void GetRawDetectorsStatus(char * RawDetsStatus);
extern void GetDetectorsStatus(char * DetsStatus);
extern void SetConfirmStatus(char* Status, int NumConfirm);
extern void GetConfirmationBits(char * ConfBits);
extern char Valid_Licence(void);

/*
* Internal Data
*/


char tempDetsin[mxdets];

MVInt ErrorIDsLocal[xgnERRORS_NUM_MAX];
MVInt ErrorValuesLocal[xgnERRORS_NUM_MAX];
int TelLocal[16];

#ifndef M8_ENABLED
/*Max size of dataset duplicated from datahand.c*/
#define gnHEADER_LINES_NUM          (5)
#define LASTLINE                    (5184)
#define gnDATA_AREA_SIZE            (LASTLINE - gnHEADER_LINES_NUM)
MVInt Holding[gnDATA_AREA_SIZE];
#endif

static MVInt OldForcedStageNum = 0;

/*
* Internal functions
*/

time_t UI_GetMovaTime( void )
{
	return(Com_read_rtc_timet());
}

time_t UI_SetMovaTime( time_t* CurrentTime )
{
	TIMESTAMP_TYPE tTimeDate;
	struct tm* timeinfo;
	struct tm localtimeinfo;

	timeinfo = localtime (CurrentTime);

	if (timeinfo)
	{
		tTimeDate.date = timeinfo->tm_mday;
		/*tm month 0 -> 11 */
		tTimeDate.month = timeinfo->tm_mon + 1;
		/*MOVA time is since 2000 not 1900 therefore subtract 100*/
		tTimeDate.year = timeinfo->tm_year - 100;
		tTimeDate.hours = timeinfo->tm_hour;
		tTimeDate.minutes = timeinfo->tm_min;
		tTimeDate.seconds = timeinfo->tm_sec;

		tTimeDate = Com_read_rtc( SET_RTC, tTimeDate);
	}
	else
	{ /* If bad input just return current time */
		tTimeDate = Com_read_rtc(READ_RTC);
	}

	localtimeinfo.tm_mday = tTimeDate.date;
	localtimeinfo.tm_mon = tTimeDate.month - 1;
	/*MOVA time is since 2000 not 1900 therefore add 100*/
	localtimeinfo.tm_year = tTimeDate.year + 100;
	localtimeinfo.tm_hour = tTimeDate.hours;
	localtimeinfo.tm_min = tTimeDate.minutes;
	localtimeinfo.tm_sec = tTimeDate.seconds;
	localtimeinfo.tm_isdst = -1;

	*CurrentTime = mktime(&localtimeinfo);

	return(*CurrentTime);

}

char* UI_GetMovaVersion(void)
{
	return (GetMovaVersionNumber());
}

void UI_GetAlertMonitoring(MVBool *BlockingStatus, MVBool *OverSaturationStatus, MVBool *OccupancyStatus)
{
	BOOL BlockStatus, OverSatStatus, OccStatus;

	GetAlertMonitoring(&BlockStatus, &OverSatStatus, &OccStatus);
	*BlockingStatus = (MVBool)BlockStatus;
	*OverSaturationStatus = (MVBool)OverSatStatus;
	*OccupancyStatus = (MVBool)OccStatus;

}

void UI_SetAlertMonitoring(MVBool BlockingStatus, MVBool OccupancyStatus, MVBool OverSaturationStatus)
{
	BOOL BlockStatus, OccStatus, OverSatStatus;

	BlockStatus = (BOOL)BlockingStatus;
	OccStatus = (BOOL)OccupancyStatus;
	OverSatStatus = (BOOL)OverSaturationStatus;
	SetAlertMonitoring(BlockStatus, OccStatus, OverSatStatus);
}


void UI_GetPeriodDetails(MVInt16* FirstPeriodInstance, MVInt16* LastPeriodInstance, MVInt16* MaxLogs)
{
	int FirstPeriod, LastPeriod, MaxLog;

	GetPeriodDetails(&FirstPeriod, &LastPeriod, &MaxLog);
	*FirstPeriodInstance = (MVInt16)FirstPeriod;
	*LastPeriodInstance = (MVInt16)LastPeriod;
	*MaxLogs = (MVInt16)MaxLog;
}


void UI_GetFlagDetails(MVByte* ErrorCount, MVByte* PhoneHome, MVBool* OnControl, 
	MVBool* ControllerReady, MVByte* StageDemanded, MVBool* VAMova, MVBool* PhoneConnected,
	MVBool* MultiStageConfirm, MVInt* PhoneCount, MVInt* WarmUpCount)
{
	*ErrorCount = (MVByte)Tcomshr->io[ERROR_COUNT];
	*PhoneHome = (MVByte)Tcomshr->io[PHONE_HOME];
	*OnControl = (MVBool)Tcomshr->io[ON_CONTROL];
	*ControllerReady = (MVBool)Tcomshr->io[CONTROLLER_READY];
	*StageDemanded = (MVBool)Tcomshr->io[DEMAND_STAGE];
	*VAMova = (MVBool)Tcomshr->io[MOVA_ON];
	*PhoneConnected = (MVBool)Tcomshr->io[PHONE_LINE_CONNECTED];
	*MultiStageConfirm = (MVBool)Tcomshr->io[MULTI_STAGE_CONF];
	*PhoneCount = (MVBool)Tcomshr->io[INC_WHILE_PHONE_IN_USE];
	*WarmUpCount = CI_get_warmup_counter();
}

void UI_SetFlagDetails(MVByte ErrorCount, MVByte PhoneHome, MVBool OnControl, 
	MVBool VAMova)
{
	Errhand_SetErrorCount((int)ErrorCount);
	Tcomshr->io[PHONE_HOME] = PhoneHome;
	Tcomshr->io[ON_CONTROL] = OnControl;
	Tcomshr->io[MOVA_ON] = VAMova;
}

void UI_ResetErrorCount()
{
	extern unsigned char soft_error_count;
	extern unsigned char IC_WATCHDOG_FAIL_COUNT;

	Tcomshr->io[ERROR_COUNT] = 0;

	/* also clear the soft error and watchdog fail counters so the unit */
	/* does not hard error unless another set of errors occur. */
	soft_error_count = 0;
	IC_WATCHDOG_FAIL_COUNT = 0;
}


void UI_GetLaneFlowLogDetails(MVByte LaneNo, MVInt PeriodInstance, time_t* time, MVInt* xFlow, MVInt* inFlow, MVByte* xSuspect, MVByte* inSuspect)
{
	int LaneNoLocal, PeriodInstanceLocal, xFlowLocal, inFlowLocal, xSuspectLocal, inSuspectLocal;

	LaneNoLocal = (int)LaneNo;
	PeriodInstanceLocal = (int)PeriodInstance;

	GetLaneFlowLogDetails(LaneNoLocal, PeriodInstanceLocal, time, &xFlowLocal, &inFlowLocal, &xSuspectLocal, &inSuspectLocal);

	*xFlow = (MVInt)xFlowLocal;
	*inFlow = (MVInt)inFlowLocal;
	*xSuspect= (MVByte)xSuspectLocal;
	*inSuspect= (MVByte)inSuspectLocal;
}

void UI_GetOverSatLogDetails(MVByte LaneID, MVInt PeriodInstance, time_t* time, MVInt* overSatCy)
{
	int overSatCyLocal;
	GetOverSatLogDetails( (int)LaneID, (int)PeriodInstance, time, &overSatCyLocal);
	*overSatCy = overSatCyLocal;
}

void UI_GetTrafficServiceLogDetails(MVByte StageID, MVInt PeriodInstance, time_t* time, MVInt* NoOfServices, MVInt* Mean, MVByte* lastDataSet)
{
	int NoOfServicesLocal, lastDataSetLocal;
	float MeanLocal;
	GetTrafficServiceLogDetails((int)StageID, (int)PeriodInstance, time, &NoOfServicesLocal, &MeanLocal, &lastDataSetLocal);
	*NoOfServices = (MVInt)NoOfServicesLocal;
	*Mean = (MVInt)(MeanLocal + 0.5);
	*lastDataSet = (MVByte)lastDataSetLocal;
}

void UI_GetOccupancyLogDetails(MVByte StageNo, MVInt PeriodInstance, time_t* time, MVInt* xInOcc, MVInt* xFlow, MVBool* xInSuspect)
{
	float xInOccLocal, xFlowLocal;
	int xInSuspectLocal;

	GetOccupancyLogDetails((int)StageNo, (int)PeriodInstance, time, &xInOccLocal, &xFlowLocal, &xInSuspectLocal);
	*xInSuspect = (MVBool)xInSuspectLocal;
	*xFlow = (MVInt)(xFlowLocal * 100);
	*xInOcc = (MVInt)(xInOccLocal * 100);
}

void UI_GetEndSatLogDetails(MVByte StageNo, MVInt PeriodInstance, time_t* time,
	MVByte* StageNoEndSat, MVByte* linkNo, MVByte* endSatReason)
{
	unsigned char StageNum, LinkNum, EndSatReason;
	GetEndSatLogDetails(StageNo, PeriodInstance, time, &StageNum, &LinkNum, &EndSatReason);
	*StageNoEndSat = (MVByte)StageNum;
	*linkNo = (MVByte)LinkNum;
	*endSatReason = (MVByte)EndSatReason;
}

void UI_GetAlertStatus(MVBool *BlockingAlert, MVBool *OverSaturationAlert, MVBool *OccupancyAlert)
{
	BOOL BlockAlert, OverSatAlert, OccAlert;
	GetAlertStatus(&BlockAlert, &OverSatAlert, &OccAlert);
	*BlockingAlert = (MVBool)BlockAlert;
	*OverSaturationAlert = (MVBool)OverSatAlert;
	*OccupancyAlert = (MVBool)OccAlert;
}

MVByte UI_GetNumLanes(void)
{
	return ((MVByte)GetNoLanes());
}

MVByte UI_GetNumStages(void)
{
	return((MVByte)GetNoStages());
}

MVByte UI_GetNumDetectors(void)
{
	return((MVByte)GetNoDetectors());

}

void UI_GetPedDetectorConfiguration(MVByte *NumPedStage, MVByte *PedStage1, MVByte *PedStage2)
{
	int PedStageCount = 0;

	if (Tcomshr->redped[0] > 0)
	{
		PedStageCount++;
	}
	if (Tcomshr->redped[1] > 0)
	{
		PedStageCount++;
	}

	*NumPedStage = (MVByte)PedStageCount;
	*PedStage1 = (MVByte)Tcomshr->redped[0];
	*PedStage2 = (MVByte)Tcomshr->redped[1];
}

MVByte UI_GetNumLinks(void)
{
	return((MVByte)GetNoLinks());
}

void UI_ClearErrorLog(void)
{
	Errhand_ResetErrorCount();
}

MVInt UI_GetErrorCount(void)
{ /*The error count in Errhand is 1 based */
	return ((MVInt)(Errhand_GetErrorCount() - 1));
}


void UI_GetError(MVInt ErrorIndex, MVByte* ErrorID, time_t* time, MVInt* add1, MVInt* add2, MVInt* add3)
{ 
	int ErrorIDLocal, add1Local, add2Local, add3Local;
	Errhand_GetMOVAError((MVInt)ErrorIndex, &ErrorIDLocal, time, &add1Local, &add2Local, &add3Local);
	*ErrorID = (MVByte)ErrorIDLocal;
	*add1 = (MVByte)add1Local;
	*add2 = (MVByte)add2Local;
	*add3 = (MVByte)add3Local;
}

void UI_GetMessage(MVInt MessageIndex, MVInt* MessageDetails )
{
	GetMessageDetails(MessageIndex, MessageDetails);
}

MVInt UI_GetMessageCount(void)
{
	return ((MVInt)(CI_get_current_msg_num()));
}

void UI_SetMessageIsReadFlag(uint8 msg_index)
{
	CI_get_msgs_data_ptr()->msgs_data[msg_index][MSG_MAX_LENGTH - 1] = 1;
}

MVBool UI_IsMessageRead(uint8 msg_index)
{
	return CI_get_msgs_data_ptr()->msgs_data[msg_index][MSG_MAX_LENGTH - 1] == 1;	
}


MVInt UI_GetAverageFlow(MVByte Day, MVByte TimeIncrement, MVByte Lane)
{
	return GetAverageFlow((int)Day, (int)TimeIncrement, (int)Lane);
}

void UI_GetSuspectDetectorIndex(MVInt* FirstFaultyDetector, MVInt* LastFaultyDetector, MVInt* MaxDetectors)
{
	int FirstFaultyDetLocal, LastFaultyDetLocal;
	GetSuspectDetectorIndex(&FirstFaultyDetLocal, &LastFaultyDetLocal);
	*FirstFaultyDetector = (MVInt)FirstFaultyDetLocal;
	*LastFaultyDetector = (MVInt)LastFaultyDetLocal;
	*MaxDetectors = (MVInt)detMAXHist;
}

void UI_GetSuspectDetectorLogDetails(MVInt FaultInstance, MVByte* DetectorID, MVByte* FaultType, 
									MVBool* ReturnToWorking, time_t* TimeFaulty, time_t* TimeReturnedWork )
{
	int DetectorIDLocal, FaultTypeLocal, ReturnToWorkingLocal;
	GetSuspectDetectorHistoryLogDetails( (int)FaultInstance, &DetectorIDLocal, &FaultTypeLocal, &ReturnToWorkingLocal, TimeFaulty, TimeReturnedWork);
	*DetectorID = (MVByte)DetectorIDLocal;
	*FaultType = (MVByte)FaultTypeLocal;
	*ReturnToWorking = (MVBool)ReturnToWorkingLocal;

}

void UI_GetSatFlowDetails(MVByte LaneNo, MVInt PeriodInstance, MVInt* ESV, MVInt* Satinc, 
	MVInt* LastMean, MVInt* ConfLevel, MVBool* StatsValid, MVInt* TotalCount, MVInt* Mean, MVInt* Smoothed15, MVInt* SumSquared)
{
	PeriodData PeriodData;
	
	PeriodData = MovaSatFlowStats_GetSatVerbose_Data( (int)LaneNo, (int)PeriodInstance);
	*ESV = (MVInt)PeriodData.rSmoothed50;
	*Satinc = (MVInt)((PeriodData.rSatinc + 0.05 ) * 10);
	*LastMean = (MVInt)PeriodData.rMeanSDisp;
	*ConfLevel = (MVInt)PeriodData.rConfLevel;
	*StatsValid = (MVBool)PeriodData.lRecAvgMetCriteria;
	*TotalCount = (MVInt)PeriodData.n16Count;
	*Mean = (MVInt)PeriodData.rMeanS;
	*Smoothed15 = (MVInt)PeriodData.r15thPercentile;
	*SumSquared = (MVInt)PeriodData.rSumSqrS;

}

void UI_GetST_LostDetails( MVByte LaneNo, MVInt PeriodInstance, MVInt* ESV, 
	MVInt* LastMean, MVInt* ConfLevel, MVBool* StatsValid, 
	MVInt* TotalCount, MVInt* Mean, MVInt* SumSquared)
{

	PeriodData PeriodData;
	
	PeriodData = MovaSatFlowStats_GetSTLostVerbose_Data( (int)LaneNo, (int)PeriodInstance );
	*ESV = (MVInt)PeriodData.rSmoothed50;
	*LastMean = (MVInt)PeriodData.rMeanSDisp;
	*ConfLevel = (MVInt)PeriodData.rConfLevel;
	*StatsValid = (MVBool)PeriodData.lRecAvgMetCriteria;
	*TotalCount = (MVInt)PeriodData.n16Count;
	*Mean = (MVInt)PeriodData.rMeanS;
	*SumSquared = (MVInt)PeriodData.rSumSqrS;
}

void UI_GetPedestrianSeviceLogDetails(MVInt PeriodInstance, time_t* Time, 
	MVInt* Mean, MVInt* StandDev, MVInt* NoOfServices, MVInt* Sum, MVInt* SumSquared )
{
	float MeanF, StandDevF, SumF, SumSquaredF; 
	unsigned char NoOfServicesUC;
	GetPedServiceLogDetails( PeriodInstance, Time, &MeanF, &StandDevF, &SumF, &SumSquaredF, &NoOfServicesUC);
	*Mean = (MVInt)(MeanF + 0.5);
	*StandDev = (MVInt)(StandDevF + 0.5);
	*Sum = (MVInt)(SumF + 0.5);
	*SumSquared = (MVInt)(SumSquaredF + 0.5);
	*NoOfServices = (MVInt)(NoOfServicesUC + 0.5);
}

void UI_GetSatFlowTimingPeriod(MVInt PeriodInstance, MVInt* PeriodStart, MVInt* PeriodEnd)
{
	MVInt LocalPeriodStart, LocalPeriodEnd;

	DSH_get_satflow_timing_period(PeriodInstance, &LocalPeriodStart, &LocalPeriodEnd);
	*PeriodStart = (MVInt)LocalPeriodStart;
	*PeriodEnd = (MVInt)LocalPeriodEnd;
}

void UI_GetForceBits(MVBool* ForceBits)
{
	MVInt index;
	char control[NumberOfForce];
	GetForceBits(control);

	for (index =0; index < NumberOfForce; index++)
	{
		*ForceBits++ = (MVBool)control[index];
	}
}

MVBool UI_GetTakeOverStatus(void)
{ 
	return ((MVBool)GetTakeOverStatus());
}

MVBool UI_GetHurryInhibitStatus(void)
{
	return ((MVBool)GetHurryInhibitStatus());
}


void UI_GetConfirmationBits(MVBool* ConfBits)
{
	int index;
	char conf_bits[NumberOfConfirms];
	GetConfirmationBits(conf_bits);

	for (index =0; index < NumberOfConfirms; index++)
	{
		*ConfBits++ = (MVBool)conf_bits[index];
	}
}


void UI_GetOutputChannels(MVBool* OutChan)
{
	char control[NumberofOutputChans];
	MVInt index;

	GetOutputChannels(control);

	for (index = 0; index < NumberofOutputChans; index++)
	{
		*OutChan++ = (MVBool)control[index];
	}
}


void UI_SetDetectorStatus(MVBool* DetectorStatus, MVByte NoOfDetectors)
{
	int index;

	for (index=0; index < NoOfDetectors; index++)
	{
		tempDetsin[index] = (char)DetectorStatus[index];
	}
	SetDetectorStatus(tempDetsin, NoOfDetectors);
}

void UI_GetRawDetectorsStatus(MVBool* RawDetectorsStatus)
{
	int index;
	char raw_dets_status[MAX_DETS];
	GetRawDetectorsStatus(raw_dets_status);

	for (index = 0; index < MAX_DETS; index++)
	{
		RawDetectorsStatus[index] = (MVBool)raw_dets_status[index];
	}
}

void UI_GetDetectorsStatus(MVBool* DetectorsStatus)
{
	int index;
	char dets_status[MAX_DETS];
	GetDetectorsStatus(dets_status);

	for (index = 0; index < MAX_DETS; index++)
	{
		DetectorsStatus[index] = (MVBool)dets_status[index];
	}
}

void UI_GetDetectorsSusStatus(MVInt* DetectorsSusStatus)
{
	uint8 index;

	for (index = 0; index < MAX_DETS; index++)
	{
		DetectorsSusStatus[index] = (MVInt)CI_get_detector_suspicion_status(index);
	}
}

void UI_SetConfirmStatus(MVBool* ConfirmStatus, MVByte NoOfConfirm)
{
	char tempConf[mxconf];
	int index;

	for (index=0; index < NoOfConfirm; index++)
	{
		tempConf[index] = (char)ConfirmStatus[index];
	}
	SetConfirmStatus(tempConf, NoOfConfirm);
}

#ifndef M8_ENABLED
UI_DataSetDetails UI_Datahand_GetDataSetDetails(MVByte DataSetNum)
{
	UI_DataSetDetails DetailsOut;
	DataSetDetails DataHandlerDetails;

	DataHandlerDetails  = Datahand_GetDataSetDetails(DataSetNum);

	Safe_Strncpy( DetailsOut.Time, DataHandlerDetails.Time,
		xgnTIME_STR_LEN );
	Safe_Strncpy( DetailsOut.Version, DataHandlerDetails.Version,
		xgnVERSION_STR_LEN );
	Safe_Strncpy( DetailsOut.Date, DataHandlerDetails.Date,
		xgnDATE_STR_LEN );
	Safe_Strncpy( DetailsOut.Title, DataHandlerDetails.Title,
		xgnTITLE_STR_LEN );
	Safe_Strncpy( DetailsOut.Fname, DataHandlerDetails.Fname,
		xgnFILE_NAME_STR_LEN );
	DetailsOut.NoOfDetectors = (MVByte)DataHandlerDetails.NoOfDetectors;
	DetailsOut.Scan = (MVByte)DataHandlerDetails.Scan;
	DetailsOut.MaxExt= (MVInt)DataHandlerDetails.MaxExt;
	DetailsOut.MinExt = (MVInt)DataHandlerDetails.MinExt;
	DetailsOut.AddGap = (MVByte)DataHandlerDetails.AddGap;
	DetailsOut.SubGap = (MVByte)DataHandlerDetails.SubGap;
	DetailsOut.NoOfStages = (MVByte)DataHandlerDetails.NoOfStages;
	DetailsOut.NoOfLanes = (MVByte)DataHandlerDetails.NoOfLanes;
	DetailsOut.NoOfLinks = (MVByte)DataHandlerDetails.NoOfLinks;
	DetailsOut.ChecksumComplete = (MVInt)DataHandlerDetails.ChecksumComplete;
	DetailsOut.ChecksumFixedData = (MVInt)DataHandlerDetails.ChecksumFixedData;
	DetailsOut.DataValid = (MVBool)DataHandlerDetails.DataValid;

	return (DetailsOut);
}
#endif

#ifndef M8_ENABLED
UI_ActiveDataDetails UI_Datahand_GetActiveDataDetails(void)
{
	UI_ActiveDataDetails UI_ActiveData;
	ActiveDataDetails ActiveData;

	ActiveData = Datahand_ActiveDataDetail();
	UI_ActiveData.CurrentlyActiveDataSet = (MVByte)ActiveData.CurrentlyActiveDataSet;
	UI_ActiveData.DataTriggerStatus = (MVBool)ActiveData.DataTriggerStatus;
	UI_ActiveData.TimeOfDayActive = (MVBool)ActiveData.TimeOfDayActive;
	UI_ActiveData.PlanChangePending = (MVInt)ActiveData.PlanChangePending;

	return (UI_ActiveData);
}
#endif

#ifndef M8_ENABLED
void UI_Datahand_GetDataSetTriggerStatus(MVByte RepoNo, MVBool* DataSetTrig1, 
	MVBool* DataSetTrig2, MVBool* DataSetTrig3, MVBool* DataSetTrig4)
{
	int LocalDataSetTrig1, LocalDataSetTrig2, LocalDataSetTrig3, 
		LocalDataSetTrig4;

	Datahand_GetPlanTriggeringStatus(RepoNo, &LocalDataSetTrig1, 
		&LocalDataSetTrig2, &LocalDataSetTrig3, &LocalDataSetTrig4);

	*DataSetTrig1 = (MVBool)LocalDataSetTrig1;
	*DataSetTrig2 = (MVBool)LocalDataSetTrig2;
	*DataSetTrig3 = (MVBool)LocalDataSetTrig3;
	*DataSetTrig4 = (MVBool)LocalDataSetTrig4;

}
#endif

#ifndef M8_ENABLED
MVInt UI_Datahand_GetDataErrVal(MVInt RepoNo, MVInt* ErrorIDs, MVInt* ErrorValues)
{
	MVInt returnValue, index;

	returnValue = Datahand_GetDataErrVal((int)RepoNo, ErrorIDsLocal, ErrorValuesLocal);

	for (index = 0; index < xgnERRORS_NUM_MAX; index++)
	{
		*ErrorIDs++ = ErrorIDsLocal[index];
		*ErrorValues++ = ErrorValuesLocal[index];
	}
	return (index);
}
#endif

#ifndef M8_ENABLED
MVInt UI_Datahand_GetDataTelNo(MVInt RepoNo, MVInt* Tel)
{
	MVInt returnValue, index;

	returnValue = Datahand_GetDataTel((int)RepoNo, TelLocal);

	for (index = 0; index < 16; index++)
	{
		*Tel++ = TelLocal[index];
	}
	return (index);
}
#endif

#ifndef M8_ENABLED
void UI_Datahand_DeleteTimeOfDay(void)
{
	Datahand_DeleteTimeOfDay();
}
#endif

#ifndef M8_ENABLED
void UI_Datahand_SetDataSetTriggering(MVBool TriggeringStatus)
{
	Datahand_SetDataSetTriggering((int)TriggeringStatus);
}
#endif

#ifndef M8_ENABLED
UI_LoadDataSetResult UI_Datahand_LoadDatafromRepository(MVByte RepositoryNo, 
	MVByte RebootIfNotComp, MVByte StillLoadIfNotComp, MVInt* ActualPlan)
{
	LoadDataSetResult UI_LoadDataResult;
	int UI_ActualPlan;

	UI_LoadDataResult = LoadDataWithNoTextOutput((int)RepositoryNo, 
		(int)RebootIfNotComp, (int)StillLoadIfNotComp, &UI_ActualPlan);
	*ActualPlan = (MVInt)UI_ActualPlan;

return ((UI_LoadDataSetResult)UI_LoadDataResult);
}
#endif

#ifndef M8_ENABLED
MVInt UI_Datahand_GetDataSetData(MVInt RepoArea, MVInt StartAddress, 
	MVInt* DataSetData)
{
	MVInt DataLength;
	
	DataLength = Datahand_GetDataSetData((int)RepoArea, (int)StartAddress, DataSetData);

	return (DataLength);
}
#endif

#ifndef M8_ENABLED
MVInt UI_Datahand_GetDataSetLinkSiteData(MVInt RepoArea, 
	UI_DataSetLinkSiteType SiteType, MVInt* DataSetSiteData)
{
	MVInt BytesRead;

	BytesRead = Datahand_GetDataSetLinkSiteData((MVInt)RepoArea, 
		(DataSetLinkSiteType)SiteType, DataSetSiteData);

	return(BytesRead);
}
#endif

#ifndef M8_ENABLED
MVInt UI_Datahand_GetDataSetIndexedLinkSiteData(MVInt RepoArea, 
	UI_DataSetIndexedLinkSiteType SiteType,  MVInt Index, MVInt* DataSetSiteData)
{
	MVInt BytesRead;

	BytesRead = Datahand_GetDataSetIndexedLinkSiteData((MVInt)RepoArea, 
		(DataSetIndexedLinkSiteType)SiteType,  (MVInt)Index, DataSetSiteData);

	return(BytesRead);
}
#endif

#ifndef M8_ENABLED
MVInt UI_Datahand_GetDataSetLaneSiteData(MVInt RepoArea,
	UI_DataSetLaneSiteType SiteType,  MVInt* DataSetLaneSiteData)
{
	MVInt BytesRead;

	BytesRead = Datahand_GetDataSetLaneSiteData((MVInt)RepoArea, 
		(DataSetLaneSiteType)SiteType, DataSetLaneSiteData);

	return(BytesRead);
}
#endif

#ifndef M8_ENABLED
MVInt UI_Datahand_GetDataSetStageSiteData(MVInt RepoArea, 
	UI_DataSetStageSiteType SiteType, MVInt* DataSetStageSiteData)
{
	MVInt BytesRead;

	BytesRead = Datahand_GetDataSetStageSiteData((MVInt)RepoArea, 
		(DataSetStageSiteType)SiteType,  DataSetStageSiteData);

	return(BytesRead);
}
#endif

#ifndef M8_ENABLED
MVInt UI_Datahand_GetDataSetIndexedStageSiteData(MVInt RepoArea, 
	UI_DataSetIndexedStageSiteType SiteType,  MVInt Index, MVInt* DataSetStageSiteData)
{
	MVInt BytesRead;

	BytesRead = Datahand_GetDataSetIndexedStageSiteData((MVInt)RepoArea, 
		(DataSetStageSiteType)SiteType,  (MVInt)Index, DataSetStageSiteData);

	return(BytesRead);
}
#endif

#ifndef M8_ENABLED
MVInt UI_GetTMAprd(void)
{
	return ((MVInt)GetTMAprd());
}
#endif

#ifndef M8_ENABLED
void UI_Datahand_RxSiteDataInitialise(void)
{
	Datahand_RxSiteDataInitialise();
}
#endif

#ifndef M8_ENABLED
UI_RxSiteDataStatusType UI_Datahand_RxSiteDataReceiveHeader(char* Filename, 
	char* Version, char* Time, char* Date, char* Title)
{
	RxSiteDataStatus ReturnedStatus;
	ReturnedStatus = Datahand_RxSiteDataReceiveHeader(Filename, Version, Time, Date, Title);
	return ((UI_RxSiteDataStatusType)(ReturnedStatus));
}
#endif

#ifndef M8_ENABLED
UI_RxSiteDataStatusType UI_Datahand_RXSiteDataReceiveData(MVInt* DataSetData, MVInt DataSize, 
	MVInt DataOffset)
{
	RxSiteDataStatus Status;
	int* Holding_p = Holding;
	int index;

	for (index =0; index < DataSize; index++)
	{
		*Holding_p++ = *DataSetData++;
	}
	Status = Datahand_RXSiteDataReceiveData(Holding, (int)DataSize, (int)DataOffset);

	return ((UI_RxSiteDataStatusType)Status);
}
#endif

#ifndef M8_ENABLED
UI_RxSiteDataStatusType UI_Datahand_RXSiteDataCalculateChecksumStatus(void)
{
	RxSiteDataStatus Status;

	Status = Datahand_RXSiteDataCalculateChecksumStatus();

	return ((UI_RxSiteDataStatusType)Status);
}
#endif

#ifndef M8_ENABLED
UI_RxSiteDataStatusType UI_Datahand_RXSiteDataPerformBasicChecks(void)
{
	RxSiteDataStatus Status;

	Status = Datahand_RXSiteDataPerformBasicChecks();

	return ((UI_RxSiteDataStatusType)Status);
}
#endif

#ifndef M8_ENABLED
UI_RxSiteDataStatusType UI_Datahand_RXSiteDataPerformStrictChecks(UI_RxSiteStrictCheckFail* ErrorCodes)
{
	RxSiteDataStatus Status;
	int ErrorCode;

	Status = Datahand_RXSiteDataPerformStrictChecks(&ErrorCode);
	*ErrorCodes = (UI_RxSiteStrictCheckFail)ErrorCode;

	return ((UI_RxSiteDataStatusType)Status);
}
#endif

#ifndef M8_ENABLED
UI_RxSiteDataStatusType UI_Datahand_RXSiteDataGetTimeOfDayData(MVInt* FoundTimeOfDay, MVInt* PlansRef)
{
	RxSiteDataStatus Status;
	int FoundToD, index;
	int PlanRef[gnDATA_AREAS_NUM];

	Status = Datahand_RXSiteDataGetTimeOfDayData(&FoundToD, PlanRef);
	*FoundTimeOfDay = (MVInt)FoundToD;

	if (FoundTimeOfDay)
	{
		for (index=0; index < gnDATA_AREAS_NUM; index++)
		{
			PlansRef[index] = (MVInt)PlanRef[index];
		}
	}
	else
	{
		memset (PlansRef, 0x00, gnDATA_AREAS_NUM * sizeof(MVInt));
	}

	return((UI_RxSiteDataStatusType)Status);

}
#endif

#ifndef M8_ENABLED
MVBool UI_Datahand_RXSiteDataIsRepoRefByActiveArea( MVInt RepoArea )
{
	BOOL LocalStatus;
	
	LocalStatus = Datahand_IsReferencedByActiveTimeOfDayData( RepoArea );
	return ((MVBool)LocalStatus);
}
#endif

#ifndef M8_ENABLED
UI_RxSiteDataStatusType UI_Datahand_RXSiteDataResetActiveToD( void )
{
	RxSiteDataStatus Status;

	Status = Datahand_RXSiteDataResetTimeOfDayData();
	return((UI_RxSiteDataStatusType)Status);
}
#endif


#ifndef M8_ENABLED
UI_RxSiteDataStatusType UI_Datahand_RXSiteDataTxRepo( MVInt RepoNo )
{
	RxSiteDataStatus Status;

	Status = Datahand_RXSiteDataTxRepo((int)RepoNo);
	return((UI_RxSiteDataStatusType)Status);
}
#endif


#ifndef M8_ENABLED
UI_RxSiteDataStatusType UI_Datahand_RXSiteDataLoadToDData( MVInt RepoNo, MVInt* FoundNo )
{
	RxSiteDataStatus Status;
	int Found;

	Status = Datahand_RXSiteDataLoadToDData((int)RepoNo, &Found);
	*FoundNo = Found;

	return((UI_RxSiteDataStatusType)Status);
}
#endif


#ifndef M8_ENABLED
UI_RxSiteDataStatusType UI_Datahand_RXSiteDataRebootInitialRepoLoad(MVInt RepoNo, MVInt IODevice)
{
	RxSiteDataStatus Status;

	Status = Datahand_RXSiteDataRebootInitialRepoLoad((int)RepoNo, (int)IODevice);
	return((UI_RxSiteDataStatusType)Status);
}
#endif


#ifndef M8_ENABLED
UI_RxSiteDataStatusType UI_Datahand_RXSiteDataPreLoadRepoCompCheck(MVInt RepoNo, MVBool* OnControl)
{
	RxSiteDataStatus Status;
	BOOL OnCon;

	Status = Datahand_RXSiteDataPreLoadRepoCompCheck(RepoNo, &OnCon);

	if (OnCon == 0)
	{
		*OnControl = MV_FALSE;
	}
	else
	{ /* TRUE may be defined as 0xFF . */
		*OnControl = MV_TRUE;
	}
	return((UI_RxSiteDataStatusType)Status);
}
#endif


#ifndef M8_ENABLED
UI_RxSiteDataStatusType UI_Datahand_RXSiteDataLoadActiveArea(MVInt RepoNo, 
	MVBool ReBootRequired, MVInt IODevice, MVInt* SelectedPlan)
{
	RxSiteDataStatus Status;
	int SelectedPln;

	Status = Datahand_RXSiteDataLoadActiveArea((int)RepoNo, (int)ReBootRequired,
		(int)IODevice, &SelectedPln);

	*SelectedPlan = SelectedPln;
	return((UI_RxSiteDataStatusType)Status);

}
#endif


void UI_GetLaneCommData(MVInt8 LaneID, MVInt32 * RedCountIN, MVInt32 * RedCountX, MVInt32 * SFSmoothed, float * SFLastCycle,
						MVInt32 * ShiftRegister, MVInt32 * OversatCounter, MVInt8 * Endsat,
						MVInt32 * ExtraVehBeyondINDET, MVInt32 * OversatInitCount)
{
	MVUInt8 i;

	if (LaneID - 1 < MAX_LANES)
	{
		*RedCountIN = CI_get_lanes_data_ptr()->red_count_in_det[LaneID - 1];
		*RedCountX = CI_get_lanes_data_ptr()->red_count_x_det[LaneID - 1];
		*SFSmoothed = CI_get_lanes_data_ptr()->smoothed_flow[LaneID - 1];
				
		*SFLastCycle = (float)MovaSatFlowTest_GetSmoothedFlowLastCycle(LaneID - 1);

		if (CI_get_program_marker() == PROG_MARKER_DELAY_AND_STOPS_OPTI)
		{
			for (i = 0; i < REG_SIZE; i++)
				ShiftRegister[i] = CI_get_lanes_data_ptr()->shift_register[LaneID - 1][i];
		}
		else
		{
			/*There is no need to show the shift register if MOVA is not doing optimisation.*/
			for (i = 0; i < REG_SIZE; i++)
				ShiftRegister[i] = 0;
		}
		
		*OversatCounter = CI_get_lanes_data_ptr()->oversat_counter[LaneID - 1];

		*Endsat = CI_get_lanes_data_ptr()->endsat_marker[LaneID - 1];

		*ExtraVehBeyondINDET = CI_get_lanes_data_ptr()->extra_veh_beyond_in_det[LaneID - 1];

		*OversatInitCount = CI_get_lanes_data_ptr()->oversat_init_count[LaneID - 1];
	}

}

void UI_GetLinkCommData(MVInt8 LinkID, MVInt32 * BonusGreenTime, MVInt32 * SmoothedFlow, MVInt8 * Endsat, MVInt8 * DemandType,
					MVInt32 * NetBenFlow, MVInt32 * ActualFlow, MVInt32 * FutureRedTime, MVInt32 * ExtraGreenTime, 
					MVInt8 * EPHoldMarker, MVInt8 * EPExtMarker, MVInt32 * EPExtTimer)
{
	MVInt32 benfit;
	MVInt32 disbenfit;
	MVInt32 predictedRedTime;
	MVInt32 cycleDelta;
	MVInt32 benFlowRates[MAX_LINKS];
	MVInt32 futureReds[MAX_LINKS];
	MVInt32 netBenFlowRates[MAX_LINKS];

	if (LinkID - 1 < MAX_LINKS)
	{
		*BonusGreenTime = (CI_get_links_data_ptr()->bonus_green_time[LinkID - 1] + 5) / 10;
		*SmoothedFlow = CI_get_links_data_ptr()->smoothed_flow[LinkID - 1];
		*Endsat = CI_get_links_data_ptr()->endsat_marker[LinkID - 1];
		*DemandType = CI_get_links_data_ptr()->demand_marker[LinkID - 1];

	
		if (CI_get_program_marker() == PROG_MARKER_DELAY_AND_STOPS_OPTI)
		{
			/*Optimisation vars*/
			ALG_ds_get_internel_data_for_message(&benfit,
				&disbenfit,
				&predictedRedTime,
				&cycleDelta,
				benFlowRates,
				futureReds,
				netBenFlowRates);

			*NetBenFlow = netBenFlowRates[LinkID - 1];
			*ActualFlow = benFlowRates[LinkID - 1];
			*FutureRedTime = (MVInt32)(futureReds[LinkID - 1] / 10);

			*ExtraGreenTime = (MVInt32)(CI_get_links_data_ptr()->extra_green_time[LinkID - 1] / 10);
		}
		else
		{
			*NetBenFlow = -1;
			*ActualFlow = -1;
			*FutureRedTime = -1;
			*ExtraGreenTime = -1;
		}

		/*EP Vars*/
		*EPHoldMarker = CI_get_junction_data_ptr()->ep_hold_marker;
		*EPExtMarker = CI_get_junction_data_ptr()->ep_ext_marker;

		*EPExtTimer = (MVInt32)(CI_get_timers_data_ptr()->ep_link_ext_timer[LinkID - 1] / 10);
	}

}

MVInt8 UI_GetPedDemandMarker()
{
	return CI_get_junction_data_ptr()->ped_demand_marker;
}

/*void UI_SetDataPlanToLoad(MVInt8 data_plan_index)
{
	Tcomshr->mark2 = data_plan_index;
}*/


/* NOTE: This function implements EngScreen starting from line: 695*/
MVBool UI_SetOpFlag(MVInt8 FlagID, MVBool NewVlaue, MVInt8 * FailureReason)
{
	MVInt i;

	if (NewVlaue == MV_TRUE)
	{
		/*Getting here means that the flag is FALSE and the user tries to make it TRUE*/

		if (Tcomshr->io[MOVA_ON] == 2 || Tcomshr->io[ON_CONTROL] == 2)
		{
			*FailureReason = OP_FLAG_FR_TESTING_OF_FORCE_CHANNELS;
			return MV_FALSE;
		}

		if (Tcomshr->io[ERROR_COUNT] > 20)
		{
			*FailureReason = OP_FLAG_FR_TOO_MANY_ERRORS;
			return MV_FALSE;
		}
		
		if (!(Valid_Licence() && check_facility_licence(LIC_MOVA)))
		{
			*FailureReason = OP_FLAG_FR_INVALID_LIC;
			return MV_FALSE;
		}

		/*Getting here means that everything is ready for setting the flag*/

		Tcomshr->io[FlagID] = 1;

		if (FlagID == ON_CONTROL)
			Tcomshr->io[MOVA_ON] = 1;

		*FailureReason = 0;
		return MV_TRUE;		
	}
	else
	{
		/*Setting the flag to FALSE*/

		Tcomshr->io[FlagID] = 0;

		if (FlagID == ON_CONTROL)
		{
			/* legitimate reason for MOVA going off control so
			|  clear the timer - see MOVA_Monitor() */
			
#ifndef PCMOVA
			MOVA_IF.MOVA_Off_Timer = 0;
#endif
		}

		/* need to actually clear the force bits explicitly */
		for (i = 1; i <= mxstag + 2; i++)
		{
			SetForceBits(i - 1, Terrlog->stgoff);
		}
		Switch_MOVA_Off(NO_SAV_MESSAGES, NO_REBOOT);

		*FailureReason = 0;
		return MV_TRUE;
	}
}

/*Implement part of the function void TryForces(int LU) in the term.c file.*/
MVBool UI_ForceStage(MVInt StageNum, MVInt8 * FailureReason)
{
	if (StageNum == OldForcedStageNum)
	{
		*FailureReason = FORCE_STAGE_FR_ALREADY_FORCED;
		return MV_FALSE;
	}

	if (StageNum > DI_get_stages_count() || StageNum <= 0)
	{
		*FailureReason = FORCE_STAGE_FR_INVALID_STAGE_NUMBER;
		return MV_FALSE;
	}

	if (Tcomshr->io[MOVA_ON] == 1)
	{
		*FailureReason = FORCE_STAGE_FR_MOVA_IS_ENABLED;
		return MV_FALSE;
	}

	if (Tcomshr->io[MULTI_STAGE_CONF] == 1)
	{
		*FailureReason = FORCE_STAGE_FR_MULTIPLE_CONF_RECEIVED;
		return MV_FALSE;
	}

	/*Getting here means that everything is ready to force this stage.*/

	Tcomshr->io[ON_CONTROL] = 2;
	Tcomshr->io[MOVA_ON] = 2;

	if (OldForcedStageNum == 0)
	{
		/*If this is the first time to force a stage*/
		SetForceBits(StageNum - 1,	1);
		SetForceBits(mxstag,		1);    /* HI */
		SetForceBits(mxstag + 1,	1);  /* TO */

		OutputScan(MOVA_JUST_ON);
	}
	else
	{
		SetForceBits(StageNum - 1,			1);
		SetForceBits(OldForcedStageNum - 1, 0);

		OutputScan(CHANGE_OF_STAGE);
	}

	OldForcedStageNum = StageNum;	
	*FailureReason = 0;

	return MV_TRUE;
}

/*Implement part of the function void TryForces(int LU) in the term.c file.*/
void UI_CancelForcedStage()
{
	MVInt i;

	for (i = 0; i <= mxstag + 1; i++)
		SetForceBits(i, 0);

	Switch_MOVA_Off(NO_SAV_MESSAGES, NO_REBOOT);

	Tcomshr->io[ON_CONTROL] = 0;
	Tcomshr->io[MOVA_ON] = 0;

	OldForcedStageNum = 0;
}

MVStatus UI_ActivateDataPlan(MVInt8 plan_num)
{
	return DSH_activate_dataplan(plan_num);
}

MVInt UI_GetActiveDataPlanNumber()
{
	return DSH_get_active_data_plan_number();
}

void UI_ResetTimeOfDayTable()
{
	DSH_reset_time_of_day_table();
}

MVBool UI_IsToDRemovedByUser()
{
	return  DSH_is_tod_removed_by_user();
}

MVBool UI_GetDatasetTriggering()
{
	return DSH_get_dataset_triggering();
}

void UI_SetDatasetTriggering(MVBool triggering_status)
{
	DSH_set_dataset_triggering(triggering_status);
}

void UI_GetJsonStringOfDataset(char * buff)
{
	DSH_get_json_string_of_dataset(buff);
}

MVUInt8 UI_GetLanesCount()
{
	return DI_get_lanes_count();
}

MVUInt8 UI_GetLinksCount()
{
	return DI_get_links_count();
}

MVUInt8 UI_GetStagesCount()
{
	return DI_get_stages_count();
}


MVUInt8 UI_GetDetsCount()
{
	return DI_get_dets_count();
}

MVBool UI_IsMOVAOnControl()
{
	if (Errhand_IsMOVAOnControl() == 1)
		return MV_TRUE;
	else
		return MV_FALSE;
}

MVInt * UI_GetRedPedPtr()
{
	return &Tcomshr->redped[0];
}

MVStatus UI_UpdateDatasetFile_XDS()
{
	return XML_DataSet_UpdateDatasetFile();
}

MVStatus UI_GetDatasetFileInfo_XDS(char * file_name, MVInt * file_size, MVInt8 * controller_id)
{
	return XML_DataSet_GetDatasetFileInfo(file_name, file_size, controller_id);
}

void UI_GetLatestDatasetFileInfo_XDS(char * file_path_and_name, MVInt * file_size)
{
	XML_DataSet_GetLatestDatasetFileInfo(file_path_and_name, file_size);
}

MVInt8 UI_GetRunningControllerID_XDS()
{
	return XML_GetRunningControllerID();
}

void UI_SetDatasetFileCRC32_XDS(MVUInt32 crc32)
{
	XML_DataSet_SetCRC32(crc32);
}


MVStatus UI_LoadDatasetFromXMLFileIntoMemeory(char * dataset_file_name, MVInt8 dp1_index, MVInt8 dp2_index, MVInt8 dp3_index, MVInt8 dp4_index)
{
	MVStatus is_successful = MV_FAILURE;
	MVInt8 data_plan_num;

	DSH_reset_data_plans_repository();

	is_successful = XML_DataSet_ReadSpecificDataPlansFromFile(dataset_file_name, dp1_index, dp2_index, dp3_index, dp4_index);

	if (is_successful == MV_SUCCESS)
	{

		/*Getting here means that the dataset has been read and loaded into memory (g_data_area[4])*/

		if (DSH_fill_time_of_day_table(1) == MV_SUCCESS)
		{
			/*Getting here means that the first data plan (and thus all of them) contains ToD info.*/
			/*We need to get the plan that should be active now, according to the table*/

			data_plan_num = DSH_get_active_data_plan_num_based_on_tod_table();

			if (data_plan_num == 0)
			{
				/*Getting here means that there is no data plan that should be active now*/
				Tcomshr->mark2 = 1;
			}
			else
			{
				Tcomshr->mark2 = data_plan_num;
			}
		}
		else
		{
			/*To Load the first data plan as there is no ToD info*/
			Tcomshr->mark2 = 1;
		}
	}

	return is_successful;
}