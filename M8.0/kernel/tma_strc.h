/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         tma_strc.h
*
*  TITLE:        MOVA TMA Logs: Data Structures
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	31-May-2011		7.0.0		AA		PK		CRN003			New development and undergoing first full system testing
*	12-Dec-2011		7.0.1		AB		PK		CRN004			SatFlow and CANDET improvements
*	22-Feb-2012		7.0.3		..		AK		CRN008			Include time.h for time_t
*	06-Jul-2012		7.0.3		AC		AK		CRN012			Memory leak in TMA module and statistics lost on dataset change
*	29-Sep-2012		7.0.4		AD		AK		CRN013			Use thread safe time functions
*	17-Dec-2012		7.0.5		..		PA		CRN019			Improve alerts interface
*	21-Jan-2013		7.0.5		..		AK		CRN022			Buffer overflow in TMA output
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

#if !defined (TMA_STRUCT_H)
#define TMA_STRUCT_H

#include "defines.h"
#include "time.h"

/* Windows lacks the POSIX localtime_r() function */
#ifdef _WIN32
#define localtime_r(x, y) localtime_s((y), (x))
#endif

/* General Notes:
 a) Max values are only to be used for summary logs (step before printing log).
*/
#define endSatMAXReasons	9		/*reasons to end saturation; at the moment there are 9 reported  reasons  */
#define detMAXHist			100		/*faulty detector history maximum length */
#define sumLogMAX			MAX_TMA_LOGS		/*Max sum logs allowed to be recorded, a week of 15 min period */ 

/*Be sure that the longest message will fit in these buffers*/
#define MAX_OUTPUT_LEN		512		/* Buffer length for TMA output buffers */

#define TMP_BUF_LEN			128		/* Buffer length for TMA temp buffers */
									/* printLiLaM5() required max 116 bytes for 30 links and 30 lanes */


/* To record detector's status*/
typedef enum 
{
	NoFault = 0, 
	Active = 1, 
	Inactive = 2
} susDetec; 


 /*  Lane information needed for flow, occupancy and oversaturated logs, 
 this information is not stage based, it is period based and it is cleared when the period is finished*/
typedef struct 
{
	/*Flow over X detectors*/
	int _xFlow;

	/*Flow over IN detectors*/
	int _inFlow;

	susDetec _xSuspect,_inSuspect;

	/*Occupancy log*/
	float _xInOcc;

	/*oversaturated lanes */
	int _noOveSatCycles;

} strcLane;


/* Link information, is used for end of saturation logs and is staged based information*/
typedef struct 
{
	/* endsat log*/
	int _endSat;

} strcLink;


/* Stage information, used for traffic logs*/
typedef struct
{
	/* traffic stages*/
	float _timeTotTrafStg; 

	time_t _timeStarted, _timeEnded;

	int _trafStgRun;

} strcStage;


/* Pedestrian information, used for ped logs*/
typedef struct
{
	/*Maintains sum total of Time for All red ped stage run in a period*/	 
	float _timeTotPedStgP;

	/*Maintains sum of square(time) for All red ped stage run in a period*/
	float _timeSqrPedStgP;

	/*Count to indicate number of pedestrian stages run during a period*/
	int _pedStgRunP;

	/*Flag to indicate if any of ped detectors was ON during previous period */
	int _pedPending;

	/*Flag to indicate that first ped detector used for pedestrian logs is ON*/
	int _recStarted;

	/*Flag to indicate that last ped detector used for pedestrian logs is OFF*/
	int _redEnded;

	/*Records the duration of ped stages in the case when ped logs are been recorded and period changes*/
	int	_pedLastStgTime;

	/*Time when first ped detector was ON*/
	time_t _timeDemanded;

	/*Time when last ped detector was OFF*/
	time_t _timeServiced;

} strcPed;


/* Faulty Detector Information, is used for history of faulty detectors */
typedef struct
{
	/*Detector number*/
	int _ID;

	/*Records the type/reason a detector is marked as faulty*/
	int _FaultType;

	/* for repeated failures, when "a" detector is reported more than once as faulty, this entry shouldn't be used */
	int _usedEntry;  

	/*Records Time when detector was first marked as faulty*/
	time_t _timeFaulted;

	/*Records Time when detector returned to normal working state*/
	time_t _timeReturnedWork;

} strcFaultyDetector;


/*
  Information kept for the only instance of a period.
  When the period is finished, this information is minimised and processed, so if printed, no more processing is needed to produce logs.
*/
typedef struct
{
	/*Counter to record number of time end of saturation occurred during a period */
	int _ensatCount; 	

	/*keeps the amount of time the detector is on during a period.*/
	float _detOnTime[mxdets]; 

	/* keeps detector flow count for a period	*/
	int _detFlow[mxdets]; 

	/* indicates when the detector has been suspected at any time during the current period */
	int _detFaulty[mxdets]; 

	/*when the end sat routine has finished we need to know which links has "last" lost the right of way and ended sat*/
	int _ensatLinkSt1[mxlink]; 
	int _ensatLinkSt2[mxlink]; 

	/* lanes but not stage based  */
	strcLane _Lanes[mxlane]; 

	/*links but not stage based  */
	int _LinksEndSat[mxlink][endSatMAXReasons];  

	/*stages*/
	strcStage _llStage[mxstag]; 

	/*Keeps the ped detector on time and off time*/
	strcPed _Ped;

} strcPeriod;


/*
   To generate alerts. This structure keeps the information for the Oversaturated, Blocking and Occupancy alert.
   This is not a period or stage based information.
   Occupancy alert is checked every 15 minutes.
   Alerts are based on conditions specified in Mova Setup.
*/
typedef struct
{
	/* Minimum occupancy limit * 10 (Stage based) */
	int _occMinLmt[mxstag]; 

	/* Maximum occupancy limit * 10 (Stage based) */
	/* i.e. stored value of 99 is 9.9 */
	int _occMaxLmt[mxstag]; 

	/*Occupancy status (Stage based)*/
	/* 0 indicates the occupancy threshold has not been exceeded*/
	/* 1 indicates occupancy threshold has been exceeded. */
	/* See description of OCCUAL in AG45 for more information */
	BOOL _occStatus[mxstag]; 

	/*keeps the amount of time the detector is on during a period.*/
	double _detOnTime[mxdets];

	/* Oversaturation limit (Lane based) */
	int _osatLmt[mxlane]; 

	/* Exit blocking limit (Link based) */
	int _blockLmt[mxlane];  /* Note PK malloc() code used link not lane: Check design spec.  Probably above comment incorrect */

	/* Exit blocking status of each lane */
	BOOL _ebStatus[mxlane];

	/*Flag to indicate if lane is oversaturated*/
	int _flagOverSat;

	/*Flag to indicate if link is blocked*/
	int _flagBlocking;

	/*Flag to indicate if stage occupancy (ON - if over max limit , OFF - if under min limit)*/
	int _flagOccupancy;

	/*are the flags checking active*/
	int _actOvFl, _actBlocFl, _actOccFl; 

	/*keeps detector flow count for a period	*/
	int _detFlow[mxdets];

	/*for oversat cycles in the alerts do not mix with period oversat cycles counter*/
	int _noOveSatCy[mxlane];

	/*exit blocking alert*/
	int _noExitBlCy[mxlane];

} strcAlert;


/*
The summ_XXX structures contain information matching the Requirement Document.
It gets filled every time the period finished and behave as circular queue.
*/
typedef struct
{
	int _xFlow[mxlane];
	int _inFlow[mxlane];
	susDetec _xSuspect[mxlane];
	susDetec _inSuspect[mxlane];

	/*End time for Flow log for this period*/
	time_t _timeStgEnd_FL;

} summa_FlowLog;


typedef struct
{
	float _xInOcc[mxstag];
	float _xFlow[mxstag];
	susDetec _xInSuspect[mxlane];
	float totalOcc, totalFlow;

	/*End time for Occupancy log for this period*/
	time_t _timeStgEnd_OC;

} summa_OccuLog;


typedef struct
{
	int _overSatCy[mxlane];

	/*End time for Oversat log for this period*/
	time_t _timeStgEnd_OS;

} summa_OversatLog;


typedef struct
{
	float _MeanP, _SDevP, _totalP, _totalSquareP;
	unsigned char _NoServicesP;

	/*End time for Pedestrian service log for this period*/
	time_t _timeStgEnd_PS;

} summa_PedSerLog;


typedef struct
{
	float _Mean[mxstag];
	int _NoServices[mxstag];
	/*Mova Dataset Number (Data repository 1/2/3)*/
	int _lastDataSet;

	/*End time for Traffic service log for this period*/
	time_t _timeStgEnd_TS;

} summa_TrafSerLog;


typedef struct
{
	/*Stage most associated with end-sat decision type during a period*/
	unsigned char _stageNo[mxstag];

	/*Link to go end-sat in a stage*/
	unsigned char _linkNo[mxstag];

	/*End sat decision type*/
	unsigned char _endSatReason[mxstag];

	/*End time for End of Saturation logs for this period*/
	time_t _timeStgEnd_ES;

} summa_EndSatLog;

/*
strcTMA is the main structure and it contains all temporary information, extracted from different places of
Mova Kernel. All but summa_XXX structures are non static.

*/
typedef struct
{
	/*Index number pointing to the first TMA period to download (from summary logs)*/
	int peUserFrom;

	/*Index number pointing to the last TMA period to download (from summary logs)*/
	int peUserTo; 

	/* TMA period duration in seconds*/		
	int periodSec; 

	/* the present period started at this time*/
	time_t currPerStarted; 

	/*the next period will start at this time*/
	time_t nextPerStarts;

	/*keeps the history of the faulty detectors (time is reported)*/
	/*this is not a period based information*/
	strcFaultyDetector detFaultHist[detMAXHist];

	/*Index number() pointing to the first faulty detector*/
	int pDetHisFrom;

	/*Index number pointing to the last faulty detector*/
	int pDetHisTo;

	/* Summary logs, max of 672 logs
	   summa_XXX  information is converted to string when user wants to download logs*/
	strcPeriod _currPeriod;
	summa_FlowLog _sumFlowLog[sumLogMAX];
	summa_OccuLog _sumOccuLog[sumLogMAX];	
	summa_EndSatLog _sumEndSatLog[sumLogMAX] ;
	summa_OversatLog _sumOverSatLog[sumLogMAX];
	summa_PedSerLog _sumPedSerLog[sumLogMAX];
	summa_TrafSerLog _sumTraSerLog[sumLogMAX];
	strcAlert sAlert; 

} strcTMA;


extern strcTMA tmaData;
extern strcTMA *const pTmaData;

#endif /*TMA_STRUCT_H*/
