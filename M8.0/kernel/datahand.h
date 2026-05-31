/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         datahand.c
*
*  TITLE:        MOVA Kernel: Interface to handle mova data. 
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	29-Sep-2004		5.0.0		..		MC/IH	SIE_00			First undergoing system test
*	31-May-2011		7.0.0		AA		PK		CRN03			Updated header inline with new Software Quality Plan
*	15-Apr-2013		7.0.6		..		AK		CRN027			Improvements to SF feedback
*	22-May-2013		7.0.6		AB		AK		M7.0.6			Issue M7.0.6
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

#ifndef INCLUDED_Datahand
#define INCLUDED_Datahand

#include "repository.h"

/*****************************
 *   Constants
 *****************************/

/* The range of valid plan numbers */
#define     xgnPLAN_NUM_MIN  (1)
#define     xgnPLAN_NUM_MAX  (4)

/* IRH MOD: M5.0.0: 21/07/04: Number of data areas */
#define gnDATA_AREAS_NUM     (4)


/*****************************
 *   Types
 *****************************/

typedef enum
{
    NO_LOADING_ERRORS,
	PLAN_NUMBER_OUT_OF_RANGE,
	REQUESTED_REPO_HAS_NO_DATA,
	ACTUAL_REQUESTED_PLANS_DIFFER,
	TOD_POINTS_TO_EMPTY_PLAN,
	MOVA_ON_CONTROL_AND_INCOMPAT_PLAN_REQ
} LoadDataSetResult;


typedef enum
{
	DATA_SET_SITE_TYPE_STAGE_MIN_GREEN,
	DATA_SET_SITE_TYPE_STAGE_MAX_GREEN,
	DATA_SET_SITE_TYPE_STAGE_LOW_MAX_GREEN,
	DATA_SET_SITE_TYPE_STAGE_PEDMAX_1,
	DATA_SET_SITE_TYPE_STAGE_PEDMAX_2,
	DATA_SET_SITE_TYPE_STAGE_OCC_ALERT_ON,
	DATA_SET_SITE_TYPE_STAGE_OCC_ALERT_OFF,
	DATA_SET_SITE_TYPE_STAGE_PFACIL,
	DATA_SET_SITE_TYPE_STAGE_EMXMAX,
	DATA_SET_SITE_TYPE_STAGE_PRXMAX,
	DATA_SET_SITE_TYPE_STAGE_MAX
} DataSetStageSiteType;


typedef enum
{
	DATA_SET_SITE_TYPE_INDEXED_STAGE_CHANGE,
	DATA_SET_SITE_TYPE_INDEXED_STAGE_MAX
} DataSetIndexedStageSiteType;

typedef enum
{
	DATA_SET_SITE_TYPE_LINK_LTYPE,
	DATA_SET_SITE_TYPE_LINK_LMIN,
	DATA_SET_SITE_TYPE_LINK_LLATOT,
	DATA_SET_SITE_TYPE_LINK_ESLMAX,
	DATA_SET_SITE_TYPE_LINK_LOSTIM,
	DATA_SET_SITE_TYPE_LINK_STOPEN,
	DATA_SET_SITE_TYPE_LINK_WAITCH,
	DATA_SET_SITE_TYPE_LINK_HOLDTM,
	DATA_SET_SITE_TYPE_LINK_CANDET,
	DATA_SET_SITE_TYPE_LINK_BONTIM,
	DATA_SET_SITE_TYPE_LINK_BONCUT,
	DATA_SET_SITE_TYPE_LINK_MIXOUT,
	DATA_SET_SITE_TYPE_LINK_SHORTL,
	DATA_SET_SITE_TYPE_LINK_NMAINL,
	DATA_SET_SITE_TYPE_LINK_MAX
} DataSetLinkSiteType;

typedef enum
{
	DATA_SET_SITE_TYPE_INDEXED_LINK_LGREEN,
	DATA_SET_SITE_TYPE_INDEXED_LINK_SDCODE,
	DATA_SET_SITE_TYPE_INDEXED_LINK_LANES,
	DATA_SET_SITE_TYPE_INDEXED_LINK_EP_DET,
	DATA_SET_SITE_TYPE_INDEXED_LINK_EP_EXT,
	DATA_SET_SITE_TYPE_INDEXED_LINK_MAX
} DataSetIndexedLinkSiteType;

typedef enum
{
	DATA_SET_SITE_TYPE_LANE_IN_DET,
	DATA_SET_SITE_TYPE_LANE_X_DET,
	DATA_SET_SITE_TYPE_LANE_OUT_DET,
	DATA_SET_SITE_TYPE_LANE_IN_SINK_DET,
	DATA_SET_SITE_TYPE_LANE_COMB_X_DET,
	DATA_SET_SITE_TYPE_LANE_X_SINK_DET,
	DATA_SET_SITE_TYPE_LANE_ALT_UP_X_DET,
	DATA_SET_SITE_TYPE_LANE_ALT_DOWN_X_DET,
	DATA_SET_SITE_TYPE_LANE_STOP_LINE_DET,
	DATA_SET_SITE_TYPE_LANE_ASSOC_DET_1_DET,
	DATA_SET_SITE_TYPE_LANE_ASSOC_DET_2_DET,
	DATA_SET_SITE_TYPE_LANE_CSPEED,
	DATA_SET_SITE_TYPE_LANE_SATINC,
	DATA_SET_SITE_TYPE_LANE_STLOST,
	DATA_SET_SITE_TYPE_LANE_LANEWF,
	DATA_SET_SITE_TYPE_LANE_COMTIM,
	DATA_SET_SITE_TYPE_LANE_USESF,
	DATA_SET_SITE_TYPE_LANE_USEST,
	DATA_SET_SITE_TYPE_LANE_BONBC,
	DATA_SET_SITE_TYPE_LANE_EXITAL,
	DATA_SET_SITE_TYPE_LANE_MAXMIN,
	DATA_SET_SITE_TYPE_LANE_CRUSIN,
	DATA_SET_SITE_TYPE_LANE_CRUSX,
	DATA_SET_SITE_TYPE_LANE_MOVEQIN,
	DATA_SET_SITE_TYPE_LANE_MOVEQX,
	DATA_SET_SITE_TYPE_LANE_QMINON,
	DATA_SET_SITE_TYPE_LANE_XOSAT,
	DATA_SET_SITE_TYPE_LANE_OSATTM,
	DATA_SET_SITE_TYPE_LANE_OSATCC,
	DATA_SET_SITE_TYPE_LANE_GAMBER,
	DATA_SET_SITE_TYPE_LANE_SATGAP,
	DATA_SET_SITE_TYPE_LANE_CRITG,
	DATA_SET_SITE_TYPE_LANE_OSATCX,
	DATA_SET_SITE_TYPE_LANE_OSATAL,
	DATA_SET_SITE_TYPE_LANE_MAX
} DataSetLaneSiteType;


typedef enum
{
	DATA_SET_STRICT_FAIL_NONE = 0,
	DATA_SET_STRICT_FAIL_NO_ACTIVE_DATASET = 1,
	DATA_SET_STRICT_FAIL_BASIC = 2,
	DATA_SET_STRICT_FAIL_LGREEN = 4,
	DATA_SET_STRICT_FAIL_CHANGE_MATRIX_MUST_REMAIN_MINUS_ONE = 8,
	DATA_SET_STRICT_FAIL_CHANGE_MATRIX_CANNOT_CHANGE_MINUS_ONE = 16,
	DATA_SET_STRICT_FAIL_SD_CODE_MUST_REMAIN_ZERO = 32,
	DATA_SET_STRICT_FAIL_SD_CODE_MUST_REMAIN_NON_ZERO = 64,
	DATA_SET_STRICT_FAIL_SHORT_LANE_CHANGED = 128,
	DATA_SET_STRICT_FAIL_LANES_ON_LINK_CHANGED = 256,
	DATA_SET_STRICT_FAIL_MAIN_LANE_ON_LINK_CHANGED = 512,
	DATA_SET_STRICT_FAIL_LANES_NOS_LINK_CHANGED = 1024,
	DATA_SET_STRICT_FAIL_MIXOUT_CHANGED = 2048,
	DATA_SET_STRICT_FAIL_MORE_THAN_TWO_DET_CHANGED = 4096,
	DATA_SET_STRICT_FAIL_MORE_THAN_10_CHANGES = 8192
} RxSiteStrictCheckFail;

/* structure used to data associated to each dataset entry in the repository*/
typedef struct Details
{
    char Fname[ xgnFILE_NAME_STR_LEN ];
    char Version[ xgnVERSION_STR_LEN ];
    char Time[ xgnTIME_STR_LEN ];
    char Date[ xgnDATE_STR_LEN ];
    char Title[ xgnTITLE_STR_LEN ];
	int NoOfDetectors;
	int Scan;
	int MinExt;
	int MaxExt;
	int AddGap;
	int SubGap;
	int NoOfStages;
	int NoOfLinks;
	int NoOfLanes;
	int ChecksumComplete;
	int ChecksumFixedData;
	int DataValid;
} DataSetDetails;


typedef struct ActiveDataDetails_struct
{
	int TimeOfDayActive;
	int CurrentlyActiveDataSet;
	int DataTriggerStatus;
	int PlanChangePending;
} ActiveDataDetails;

typedef enum RxSiteDataStatus_struct
{
	RX_DATA_CHECKSUM_FAILED,
	RX_DATA_OFFSET_EXCEEDED,
	RX_DATA_BASIC_CHECKS_FAILED,
	RX_DATA_STRICT_CHECKS_FAILED,
	RX_DATA_DATASET_COMPATIBILITY_FAILURE,
	RX_DATA_DATASET_NOT_AVAILABLE,
	RX_DATA_INVOKED_WRONG_STATE,
	RX_DATA_NO_ERROR,
	RX_DATA_MAX
} RxSiteDataStatus;

/*****************************
 *   Public functions
 *****************************/
#ifndef M8_ENABLED
BOOL    Data_Handler(                   int nWx, char* sPassword);
#endif

BOOL    Datahand_LoadDataset(           int nPlanNum );
void    Movsub(                         int nPlanNum );

/* IRH MOD: M5.0.0: 21/07/04: Various getter functions */
char *  Datahand_GetSiteName(           int nPlanNum, char * const szSiteName );
int     Datahand_GetLinksNum(           int nPlanNum );
int     Datahand_GetLanesNum(           int nPlanNum );
int     Datahand_GetStagesNum(          int nPlanNum );
int     Datahand_GetDetectorsNum(       int nPlanNum );
int     Datahand_GetTimeOfDayPlan(      int nQuarterHourOfWeek );
int     Datahand_GetActivePlan(         void );
BOOL    Datahand_IsUploadingSiteData(   void );
BOOL    Datahand_IsDownloadingSiteData( void );
//const Repository_Type* GetActiveRepositoryPtr(void);

DataSetDetails Datahand_GetDataSetDetails(int nPlanNum);
ActiveDataDetails Datahand_ActiveDataDetail(void);
void Datahand_DeleteTimeOfDay(void);
void Datahand_SetDataSetTriggering(int TriggeringStatus);
LoadDataSetResult LoadDataWithNoTextOutput( int planNo, int RebootIfNotComp, int StillLoadIfNotComp, int* ActualPlan);
int Datahand_GetDataSetData(int RepoArea, int StartAddress, int* DataSetData);
int Datahand_GetDataSetLaneSiteData(int RepoArea, DataSetLaneSiteType SiteType, int* DataSetLaneSiteData);
int Datahand_GetDataSetStageSiteData(int RepoArea, DataSetStageSiteType SiteType, int* DataSetStageSiteData);
int Datahand_GetDataSetIndexedStageSiteData(int RepoArea, DataSetIndexedStageSiteType SiteType, int Index, int* DataSetStageSiteData);
int Datahand_GetDataSetIndexedLinkSiteData(int RepoArea, DataSetIndexedLinkSiteType SiteType, int Index, int* DataSetSiteData);
int Datahand_GetDataSetLinkSiteData(int RepoArea, DataSetLinkSiteType SiteType, int* DataSetSiteData);

void Datahand_GetSatFlowTimingPeriod(int RepoArea, int PeriodInstance, int* PeriodStart, int* PeriodEnd);

void Datahand_GetPlanTriggeringStatus(int RepoNo, int* DataSetTrig1, int* DataSetTrig2, int* DataSetTrig3,int* DataSetTrig4);
int Datahand_GetDataErrVal(int RepoNo, int* ErrorIDs, int* ErrorValues);
int Datahand_GetDataTel(int RepoNo, int* Tel);
int Datahand_GetTMAprd(void);

void Datahand_RxSiteDataInitialise(void);
RxSiteDataStatus Datahand_RxSiteDataReceiveHeader(char* Filename, char* Version, char* Time, char* Date, char* Title);
RxSiteDataStatus Datahand_RXSiteDataReceiveData(int* DataSetData, int DataSize, int DataOffset);
RxSiteDataStatus Datahand_RXSiteDataCalculateChecksumStatus(void);
RxSiteDataStatus Datahand_RXSiteDataPerformBasicChecks(void);
RxSiteDataStatus Datahand_RXSiteDataPerformStrictChecks(int* ErrorCodes);
RxSiteDataStatus Datahand_RXSiteDataGetTimeOfDayData(int* FoundTimeOfDay, int* PlansRef);
BOOL Datahand_IsReferencedByActiveTimeOfDayData( int nRepositoryArea );
RxSiteDataStatus Datahand_RXSiteDataResetTimeOfDayData( void);
RxSiteDataStatus Datahand_RXSiteDataTxRepo(int RepoNo);
RxSiteDataStatus Datahand_RXSiteDataLoadToDData(int RepoNo, int* FoundNo);
RxSiteDataStatus Datahand_RXSiteDataRebootInitialRepoLoad(int RepoNo, int IODevice);
RxSiteDataStatus Datahand_RXSiteDataPreLoadRepoCompCheck(int RepoNo, BOOL* OnControl);
RxSiteDataStatus Datahand_RXSiteDataLoadActiveArea(int RepoNo, int ReBootRequired, int nPortNum, int* SelectedPlan);


//remove after testing
int* GetHoldingArea(void);

//TEMP CODE:
BOOL Datahand_LoadDatasetFromMemory(int nPlanNum);

#endif /* INCLUDED_Datahand */
