/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         tma_preprint.h
*
*  TITLE:        TMA Logs display preperation and execution.
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	31-May-2011		7.0.0		AA		PK		CRN003			New development and undergoing first full system testing
*	22-Feb-2012		7.0.3		..		AK		CRN008			Fix compiler warnings
*	06-Jul-2012		7.0.3		AB		AK		CRN012			Memory leak in TMA module and statistics lost on dataset change
*	21-Jan-2013		7.0.5		..		AK		CRN022			Buffer overflow in TMA output
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
#if !defined (TMA_PREPRINT_H)
#define TMA_PREPRINT_H

#include "tma_strc.h"


#define WRITE_TO_TMA_LOG_FILE -1

void TMA_PreP_Init_(void);

/* Get Period Numbers */
void GetPeriodDetails(int* FirstPeriodInstance, int* LastPeriodInstance, int* MaxLogs);

/* LOG GENERATION */
/* Flow log*/
void print_FlowLog(strcTMA *const pTma, int outDevice, int noLanes);
void GetLaneFlowLogDetails(int LaneNo, int PeriodInstance, time_t* time, int* xFlow, int* inFlow, int* xSuspect, int* inSuspect);

/* Occupancy log */
void print_OccupancyLog(strcTMA *const pTma, int outDevice, int noStages);
void GetOccupancyLogDetails(int StageID, int PeriodInstance, time_t* time, float* xInOcc, float* xFlow, int* xInSuspect);
void GetOccupancyLogTotalDetails(int PeriodInstance, float* totalOcc, float* totalFlow);

/* End of Saturation log*/
void print_EndSatLog(strcTMA *const pTma, int outDevice, int noStages);
void GetEndSatLogDetails(int StageID, int PeriodInstance, time_t* time, unsigned char* stageNo, unsigned char* linkNo, unsigned char* endSatReason);

/* Faulty Detector History  */
void print_FaultyDetecLog(strcTMA *const pTma, int outDevice);
void GetSuspectDetectorHistoryLogDetails( int FaultInstance, int* ID, int* FaultType, int* usedEntry, time_t* TimeFaulted, time_t* TimeReturned);
void GetSuspectDetectorIndex(int* FirstDetector, int* LastDetector);

/* Oversaturation log  */
void print_OversatLog(strcTMA *const pTma, int outDevice, int noLanes);
void GetOverSatLogDetails(int LaneID, int PeriodInstance, time_t* time, int* overSatCy);


/*Level of service to vehicles  log*/
void print_TraSevLog(strcTMA *const pTma, int outDevice, int noStages);
void GetTrafficServiceLogDetails(int StageID, int PeriodInstance, time_t* time, int* _NoServices, float* Mean, int* lastDataSet);

/*Level of service to pedestrian  log*/
void print_PedSevLog(strcTMA *const pTma, int outDevice, int allRedStg, int pedStg[]);
void GetPedServiceLogDetails(int PeriodInstance, time_t* time, float* MeanP, float* SDevP, float* totalP,  float* totalSquareP, unsigned char* NoServicesP);

void TMA_PreP_UpdateTMALogs(strcTMA *const pTma);

char * TMA_get_log_buffer();

#endif /*TMA_PREPRINT_H*/
