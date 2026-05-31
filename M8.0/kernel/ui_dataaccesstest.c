/**
 * @file
 *
 * The test harness for the MOVA UI API. 
 * Interface functions from file ui_dataaccess.h are tested.
 * Output is written to file UIDataFile.txt.
 *
 *
 * @author Peter Anderson
 * @version 1.0
 * @date   2015-03-24; Peter Anderson; Initial
 * @copyright  Copyright TRL Ltd 2015. Based on and adapted from Crown Copyright
 *  material under licence. Rights to and ownership of the software remain
 *  unaffected by changes to the software, whether by addition, removal
 *  modification. Agreement has been reached between The MOVA Licensees
 *  and TRL Ltd whereby The Licensees are authorised to use and
 *  modify the MOVA code. TRL Ltd cannot be held responsible for improper
 *  use.
 *
 */
#include "ui_dataaccess.h"
#include <stdio.h>
#include <windows.h>
#include "tma_preprint.h"
#include "gbltypes.h"
#include "generalfunc.h"
#include "errhand.h"
#include "sfstats.h"
#include "kdebug.h"

#define WRITE_SIZE 1
#define WRITE_ITERATIONS 5180

CRITICAL_SECTION UI_DataFileCriticalSection;
FILE *	UI_DataFile = NULL;
CRITICAL_SECTION UI_DataReadFileCriticalSection;
FILE *	UI_ReadDataFile = NULL;

MVInt DatasetTestset[WRITE_SIZE * WRITE_ITERATIONS];

char HeaderLine1[30], HeaderLine2[80], HeaderLine3[80], HeaderLine4[80], HeaderLine5[80], working[80];

/* Create file to contain UI data */
void UI_TestInitialise(void)
{
	/* Open file for writing */
	UI_DataFile = fopen( "UIDataFile.txt", "w" );

	if ( UI_DataFile != NULL )
	{
		InitializeCriticalSection( &UI_DataFileCriticalSection );
	}
}
static void UI_print_link_module(UI_DataSetLinkSiteType SiteType)
{
	const char *MNAMES[UI_DATA_SET_SITE_TYPE_LINK_MAX] = {"LTYPE", "LMIN", "LLATOT", "ESLMAX", "LOSTIM", "STOPEN", "WAITCH",
	 "HOLD_TM", "CANDET", "BONTIM", "BONCUT", "MIXOUT", "SHORTL", "NMAINL"};

	fprintf(UI_DataFile, "%s\n", MNAMES[(int)SiteType]);
}

static void UI_print_linkIndexed_module(UI_DataSetIndexedLinkSiteType SiteType)
{
	const char *MNAMES[UI_DATA_SET_SITE_TYPE_INDEXED_LINK_MAX] = {"LGREEN","SDCODE","LLANES","EP_DETECTS", "EP_EXT"};

	fprintf(UI_DataFile, "%s\n", MNAMES[(int)SiteType]);
}

static void UI_print_lane_module(UI_DataSetLaneSiteType SiteType)
{
	const char *MNAMES[UI_DATA_SET_SITE_TYPE_LANE_MAX] = {"IN_DET", "X_DET", "OUT_DET", "IN SINK", "COMB_X", "X SINK", "ALT-UP X", "ALT-DOWN X", 
		"STOP LINE", "ASSOC_DET_1", "ASSOC_DET_2", "CSPEED", "SATINC", "STLOST should be +10", "LANEWF", "COMTIM", "USESF", "USEST",
		"BONBC", "EXITAL", "MAXMIN", "CRUSIN", "CRUSX", "MOVEQIN", "MOVEQX", "QMINON", "XOSAT", "OSATTM", "OSATCC", "GAMBER", "SATGAP", "CRITG", 
		"OSATCX", "OSATAL"};

	fprintf(UI_DataFile, "%s\n", MNAMES[(int)SiteType]);
}

static void UI_print_stage_module(UI_DataSetStageSiteType SiteType)
{
	const char *MNAMES[UI_DATA_SET_SITE_TYPE_STAGE_MAX] = {"MIN_GREEN", "MAX_GREEN", "LOW_MAX", "PED_MAX_1", "PED_MAX_2", "OCC_ALERT_ON", 
		"OCC_ALERT_OFF", "PFCAIL", "EMXMAX","PRXMAX"};

	fprintf(UI_DataFile, "%s\n", MNAMES[(int)SiteType]);
}

static void UI_print_stage_indexed_module(UI_DataSetIndexedStageSiteType SiteType)
{
	const char *MNAMES[UI_DATA_SET_SITE_TYPE_STAGE_MAX] = {"STAGE_CHANGE"};

	fprintf(UI_DataFile, "%s\n", MNAMES[(int)SiteType]);
}
static void UI_print_StageBasedMultipleData(UI_DataSetIndexedStageSiteType SiteType, int RepoArea, int Index, UI_DataSetDetails DS_Details, int* AreaToCheck)
{

	MVInt ElementsRead = 250, Stage;
	MVInt DataSetSiteData[250];

	UI_print_stage_indexed_module(SiteType);
	ElementsRead = UI_Datahand_GetDataSetIndexedStageSiteData(RepoArea, SiteType, Index, DataSetSiteData);

	if (ElementsRead != DS_Details.NoOfStages)
	{
		fprintf(UI_DataFile, "error elements = %d no of stages = %d\n", ElementsRead, DS_Details.NoOfStages);
	}

	for (Stage = 0; Stage < DS_Details.NoOfStages; Stage++)
	{
		fprintf(UI_DataFile, "%d ", DataSetSiteData[Stage]);
	}
	fprintf(UI_DataFile, "\n");

	//just to check they are the same
	for (Stage = 0; Stage < DS_Details.NoOfStages; Stage++)
	{
		fprintf(UI_DataFile, "%d ", AreaToCheck[Stage]);
	}
	fprintf(UI_DataFile, "\n");
}


static void UI_print_StageBasedData(UI_DataSetStageSiteType SiteType, int RepoArea, UI_DataSetDetails DS_Details, int* AreaToCheck)
{
	MVInt ElementsRead = 250, Stage;
	MVInt DataSetSiteData[250];

	UI_print_stage_module(SiteType);
	ElementsRead = UI_Datahand_GetDataSetStageSiteData(RepoArea, SiteType, DataSetSiteData);

	if (ElementsRead != DS_Details.NoOfStages)
	{
		fprintf(UI_DataFile, "error elements = %d no of stages = %d\n", ElementsRead, DS_Details.NoOfStages);
	}

	for (Stage = 0; Stage < DS_Details.NoOfStages; Stage++)
	{
		fprintf(UI_DataFile, "%d ", DataSetSiteData[Stage]);
	}
	fprintf(UI_DataFile, "\n");

	//just to check they are the same
	for (Stage = 0; Stage < DS_Details.NoOfStages; Stage++)
	{
		fprintf(UI_DataFile, "%d ", AreaToCheck[Stage]);
	}
	fprintf(UI_DataFile, "\n");
}

static void UI_print_StageBasedDataChar(UI_DataSetStageSiteType SiteType, int RepoArea, UI_DataSetDetails DS_Details, signed char* AreaToCheck)
{
	MVInt ElementsRead = 250, Stage;
	MVInt DataSetSiteData[250];

	UI_print_stage_module(SiteType);
	ElementsRead = UI_Datahand_GetDataSetStageSiteData(RepoArea, SiteType, DataSetSiteData);

	if (ElementsRead != DS_Details.NoOfStages)
	{
		fprintf(UI_DataFile, "error elements = %d no of stages = %d\n", ElementsRead, DS_Details.NoOfStages);
	}

	for (Stage = 0; Stage < DS_Details.NoOfStages; Stage++)
	{
		fprintf(UI_DataFile, "%d ", DataSetSiteData[Stage]);
	}
	fprintf(UI_DataFile, "\n");

	//just to check they are the same
	for (Stage = 0; Stage < DS_Details.NoOfStages; Stage++)
	{
		fprintf(UI_DataFile, "%d ", AreaToCheck[Stage]);
	}
	fprintf(UI_DataFile, "\n");
}




static void UI_print_LinkBasedDataSourceChar(UI_DataSetIndexedLinkSiteType SiteType, int RepoArea, int Elements, int Index, signed char* AreaToCheck)
{
	MVInt ElementsRead = 250, Link;
	MVInt DataSetSiteData[250];

	UI_print_linkIndexed_module(SiteType);
	ElementsRead = UI_Datahand_GetDataSetIndexedLinkSiteData(RepoArea, SiteType, Index, DataSetSiteData);

	if (ElementsRead != Elements)
	{
		fprintf(UI_DataFile, "error elements = %d no of stage elements = %d\n", ElementsRead, Elements);
	}

	for (Link = 0; Link < Elements; Link++)
	{
		fprintf(UI_DataFile, "%d ", DataSetSiteData[Link]);
	}
	fprintf(UI_DataFile, "\n");

	//just to check they are the same
	for (Link = 0; Link < Elements; Link++)
	{
		fprintf(UI_DataFile, "%d ", AreaToCheck[Link]);
	}
	fprintf(UI_DataFile, "\n");
}


static void UI_print_LinkIndexedBasedDataSourceInt(UI_DataSetIndexedLinkSiteType SiteType, int RepoArea, int TotalElements, int Index, int* AreaToCheck)
{
	MVInt ElementsRead = 250, Link;
	MVInt DataSetSiteData[250];

	UI_print_linkIndexed_module(SiteType);
	ElementsRead = UI_Datahand_GetDataSetIndexedLinkSiteData(RepoArea, SiteType, Index, DataSetSiteData);

	if (ElementsRead != TotalElements)
	{
		//fprintf(UI_DataFile, "error elements = %d no of links = %d\n", ElementsRead, DS_Details.NoOfLinks);
		fprintf(UI_DataFile, "error elements = %d no of links = %d\n", ElementsRead, TotalElements);
	}

	for (Link = 0; Link < TotalElements; Link++)
	{
		fprintf(UI_DataFile, "%d ", DataSetSiteData[Link]);
	}
	fprintf(UI_DataFile, "\n");

	//just to check they are the same
	for (Link = 0; Link < TotalElements; Link++)
	{
		fprintf(UI_DataFile, "%d ", AreaToCheck[Link]);
	}
	fprintf(UI_DataFile, "\n");
}

static void UI_print_LinkBasedDataSourceInt(UI_DataSetLinkSiteType SiteType, int RepoArea, int TotalElements, int* AreaToCheck)
{
	MVInt ElementsRead = 250, Link;
	MVInt DataSetSiteData[250];

	UI_print_link_module(SiteType);
	ElementsRead = UI_Datahand_GetDataSetLinkSiteData(RepoArea, SiteType, DataSetSiteData);

	if (ElementsRead != TotalElements)
	{
		//fprintf(UI_DataFile, "error elements = %d no of links = %d\n", ElementsRead, DS_Details.NoOfLinks);
		fprintf(UI_DataFile, "error elements = %d no of links = %d\n", ElementsRead, TotalElements);
	}

	for (Link = 0; Link < TotalElements; Link++)
	{
		fprintf(UI_DataFile, "%d ", DataSetSiteData[Link]);
	}
	fprintf(UI_DataFile, "\n");

	//just to check they are the same
	for (Link = 0; Link < TotalElements; Link++)
	{
		fprintf(UI_DataFile, "%d ", AreaToCheck[Link]);
	}
	fprintf(UI_DataFile, "\n");
}

static void UI_print_LaneBasedDataSourceDet(UI_DataSetLaneSiteType SiteType, int RepoArea, int TotalElements)
{
	MVInt ElementsRead = 250, Lane;
	MVInt DataSetLaneSiteData[250];

	UI_print_lane_module(SiteType);
	ElementsRead = UI_Datahand_GetDataSetLaneSiteData(RepoArea, SiteType, DataSetLaneSiteData);

	if (ElementsRead != TotalElements)
	{
		fprintf(UI_DataFile, "error elements = %d no of lanes = %d\n", ElementsRead, TotalElements);
	}

	for (Lane = 0; Lane < TotalElements; Lane++)
	{
		fprintf(UI_DataFile, "%d ", DataSetLaneSiteData[Lane]);
	}
	fprintf(UI_DataFile, "\n");

	//just to check they are the same
	for (Lane = 0; Lane < TotalElements; Lane++)
	{
		if (SiteType > UI_DATA_SET_SITE_TYPE_LANE_STOP_LINE_DET)
		{
			fprintf(UI_DataFile, "%d ", Tcomshr->assocd[Lane][SiteType - UI_DATA_SET_SITE_TYPE_LANE_STOP_LINE_DET - 1]);
		}
		else
		{
			fprintf(UI_DataFile, "%d ", Tcomshr->det[Lane][SiteType]);
		}
	}
	fprintf(UI_DataFile, "\n");
}


static void UI_print_LaneBasedDataSourceInt(UI_DataSetLaneSiteType SiteType, int RepoArea, int TotalElements, int* AreaToCheck)
{
	MVInt ElementsRead = 250, Lane;
	MVInt DataSetLaneSiteData[250];

	UI_print_lane_module(SiteType);
	ElementsRead = UI_Datahand_GetDataSetLaneSiteData(RepoArea, SiteType, DataSetLaneSiteData);

	if (ElementsRead != TotalElements)
	{
		fprintf(UI_DataFile, "error elements = %d no of lanes = %d\n", ElementsRead, TotalElements);
	}

	for (Lane = 0; Lane < TotalElements; Lane++)
	{
		fprintf(UI_DataFile, "%d ", DataSetLaneSiteData[Lane]);
	}
	fprintf(UI_DataFile, "\n");

	//just to check they are the same
	for (Lane = 0; Lane < TotalElements; Lane++)
	{
		fprintf(UI_DataFile, "%d ", AreaToCheck[Lane]);
	}
	fprintf(UI_DataFile, "\n");
}


/* Functions to test data access functions only by printing to file */

static void UI_print_DailyFlowLogs(int noLanes, int Day)
{
	static char buf[MAX_OUTPUT_LEN];
	static char tmp[TMP_BUF_LEN];
	MVByte TimeInstance, laneId;
	const int NoOfHrs = 48;

	memset(buf, 0, MAX_OUTPUT_LEN);

	sprintf(buf, "Day = %d \r\n", Day);
	fprintf(UI_DataFile, "%s ", buf);
	memset(buf, 0, MAX_OUTPUT_LEN);

	for (TimeInstance = 0; TimeInstance < NoOfHrs; TimeInstance++)
	{
		sprintf(tmp, "Time Instance  %d ", TimeInstance);
		strcat(buf, tmp);
		memset(tmp, 0, TMP_BUF_LEN);

		for (laneId = 0; laneId < noLanes; laneId++)
		{
			sprintf(tmp, " %d ", UI_GetAverageFlow((MVByte)Day, TimeInstance, (MVByte)laneId));
			strcat(buf, tmp);
		}
		strcat(buf, "\r\n");
		memset(buf, 0, MAX_OUTPUT_LEN);
	}
	fprintf(UI_DataFile, "%s", buf);
}



/* Functions to test data access functions only by printing to file */
static void UI_print_FlowLogs(int PeriodInstance, int noLanes)
{
	struct tm ltime;
	MVByte laneId;
	time_t time;
	static char buf[MAX_OUTPUT_LEN];
	static char tmp[TMP_BUF_LEN];
	MVInt inFlow, xFlow;
	MVByte inSuspect, xSuspect;

	// Just get the time
	UI_GetLaneFlowLogDetails(0, (MVInt)PeriodInstance, &time, &xFlow, &inFlow, &xSuspect, &inSuspect);

	/* printing time*/
	localtime_r(&time, &ltime);
	strftime (buf, MAX_OUTPUT_LEN, "%A, %d/%m/%y to %H:%M:%S hrs.", &ltime);

	fprintf(UI_DataFile, "%s ", buf);

	memset(buf, 0, MAX_OUTPUT_LEN);

	sprintf(buf, "Period Instance = %d", PeriodInstance);
	fprintf(UI_DataFile, "%s ", buf);
	memset(buf, 0, MAX_OUTPUT_LEN);


	for (laneId = 0; laneId < noLanes; laneId++)
	{
		UI_GetLaneFlowLogDetails(laneId, (MVInt)PeriodInstance, &time, &xFlow, &inFlow, &xSuspect, &inSuspect);
		sprintf(tmp, ",%d,%d,%d,%d", inFlow, inSuspect, xFlow, xSuspect);
		strcat(buf, tmp);
	}
	strcat(buf, "\r\n");
	fprintf(UI_DataFile, "%s", buf);
}


static void UI_print_OccLogs(int PeriodInstance, int noStages)
{
	struct tm ltime;
	int stageId;
	time_t time;
	static char buf[MAX_OUTPUT_LEN];
	static char tmp[TMP_BUF_LEN];
	//int inFlow, inSuspect, xFlow, xSuspect;
	float xInOcc;
	float xFlow;
	float totalFlow;
	float totalOcc;
	int xInSuspect;

	// Just get the time
	GetOccupancyLogDetails(0, PeriodInstance, &time, &xInOcc, &xFlow, &xInSuspect);

	/* printing time*/
	localtime_r(&time, &ltime);
	strftime (buf, MAX_OUTPUT_LEN, "%A, %d/%m/%y to %H:%M:%S hrs.", &ltime);

	fprintf(UI_DataFile, "%s ", buf);

	memset(buf, 0, MAX_OUTPUT_LEN);

	sprintf(buf, "Period Instance = %d", PeriodInstance);
	fprintf(UI_DataFile, "%s ", buf);
	memset(buf, 0, MAX_OUTPUT_LEN);


	for (stageId = 0; stageId < noStages; stageId++)
	{
		GetOccupancyLogDetails(stageId, PeriodInstance, &time, &xInOcc, &xFlow, &xInSuspect);
		sprintf(tmp, " %5.2f, %5.0f, %d, ", xInOcc, xFlow, xInSuspect );
		strcat(buf, tmp);
	}

	GetOccupancyLogTotalDetails(PeriodInstance, &totalOcc, &totalFlow);
	sprintf(tmp, " %5.2f, %5.0f", totalOcc, totalFlow);
	strcat(buf, tmp);

	strcat(buf, "\r\n");
	fprintf(UI_DataFile, "%s", buf);
}



static void UI_print_EndSatLogs(int PeriodInstance, int noStages)
{
	struct tm ltime;
	int stageId;
	time_t time;
	static char buf[MAX_OUTPUT_LEN];
	static char tmp[TMP_BUF_LEN];
	unsigned char stageNo;
	unsigned char linkNo;
	unsigned char endSatReason;

	// Just get the time
	GetEndSatLogDetails(0, PeriodInstance, &time, &stageNo, &linkNo, &endSatReason);

	/* printing time*/
	localtime_r(&time, &ltime);
	strftime (buf, MAX_OUTPUT_LEN, "%A, %d/%m/%y to %H:%M:%S hrs.", &ltime);

	fprintf(UI_DataFile, "%s ", buf);

	memset(buf, 0, MAX_OUTPUT_LEN);

	sprintf(buf, "Period Instance = %d", PeriodInstance);
	fprintf(UI_DataFile, "%s ", buf);
	memset(buf, 0, MAX_OUTPUT_LEN);


	for (stageId = 0; stageId < noStages; stageId++)
	{
		GetEndSatLogDetails(stageId, PeriodInstance, &time, &stageNo, &linkNo, &endSatReason);
		sprintf(tmp, ", S%d, %d, %d", stageNo, linkNo, endSatReason);
		strcat(buf, tmp);
	}

	strcat(buf, "\r\n");
	fprintf(UI_DataFile, "%s", buf);
}



static void UI_print_OverSatLogs(int PeriodInstance, int noLanes)
{
	struct tm ltime;
	int laneId;
	time_t time;
	static char buf[MAX_OUTPUT_LEN];
	static char tmp[TMP_BUF_LEN];
	int overSatCy;

	// Just get the time
	GetOverSatLogDetails(0, PeriodInstance, &time, &overSatCy);

	/* printing time*/
	localtime_r(&time, &ltime);
	strftime (buf, MAX_OUTPUT_LEN, "%A, %d/%m/%y to %H:%M:%S hrs.", &ltime);

	fprintf(UI_DataFile, "%s ", buf);

	memset(buf, 0, MAX_OUTPUT_LEN);

	sprintf(buf, "Period Instance = %d", PeriodInstance);
	fprintf(UI_DataFile, "%s ", buf);
	memset(buf, 0, MAX_OUTPUT_LEN);


	for (laneId = 0; laneId < noLanes; laneId++)
	{
		GetOverSatLogDetails(laneId, PeriodInstance, &time, &overSatCy);
		sprintf(tmp, ",%d", overSatCy);
		strcat(buf, tmp);
	}

	strcat(buf, "\r\n");
	fprintf(UI_DataFile, "%s", buf);
}


static void UI_print_PedSevLogLine(int PeriodInstance, int NoOfPedStages, int pedStg[])
{
	struct tm ltime;
	int pedId;
	time_t time;
	static char buf[MAX_OUTPUT_LEN];
	static char tmp[TMP_BUF_LEN];
	float MeanP;
	float SDevP;
	float totalP;
	float totalSquareP;
	unsigned char NoServices;

	// Just get the time
	GetPedServiceLogDetails(PeriodInstance, &time, &MeanP, &SDevP, &totalP, &totalSquareP, &NoServices);

	/* printing time */
	localtime_r(&time, &ltime);
	strftime (buf, MAX_OUTPUT_LEN, "%A, %d/%m/%y to %H:%M:%S hrs.", &ltime);

	fprintf(UI_DataFile, "%s ", buf);

	memset(buf, 0, MAX_OUTPUT_LEN);

	for (pedId = 0; pedId < NoOfPedStages; pedId++)
	{
		if (pedStg[pedId] > 0)
		{
			GetPedServiceLogDetails(PeriodInstance, &time, &MeanP, &SDevP, &totalP, &totalSquareP, &NoServices);
			sprintf(tmp, ",%3.0f, %3.0f, %d, %3.0f, %4.0f", MeanP, SDevP, NoServices, totalP, totalSquareP);
			strcat(buf, tmp);
		}
	}

	strcat(buf, "\r\n");
	fprintf(UI_DataFile, "%s", buf);
}


static void UI_print_TravelServiceLogs(int PeriodInstance, int noStages)
{
	struct tm ltime;
	time_t time;
	MVInt NoServices, Mean;
	MVByte lastDataSet, stgID;
	static char buf[MAX_OUTPUT_LEN];
	static char tmp[TMP_BUF_LEN];


	/*time*/
	UI_GetTrafficServiceLogDetails(0, PeriodInstance, &time, &NoServices, &Mean, &lastDataSet);
	localtime_r(&time, &ltime);
	strftime (buf, MAX_OUTPUT_LEN, "%A, %d/%m/%y to %H:%M:%S hrs.,", &ltime);

	fprintf(UI_DataFile, "%s ", buf);
	memset(buf, 0, MAX_OUTPUT_LEN);

	/*headers*/
	for (stgID = 0; stgID < noStages; stgID++)
	{
		UI_GetTrafficServiceLogDetails(stgID, PeriodInstance, &time, &NoServices, &Mean, &lastDataSet);
		sprintf(tmp, "%d, %d,", NoServices, Mean);
		strcat(buf, tmp);
	}

	sprintf(tmp, "F = %d\r\n", lastDataSet);
	strcat(buf, tmp);
	fprintf(UI_DataFile, "%s", buf);
}


static void UI_print_FaultyDetectorLogs(int DetectorNo)
{
	struct tm ltime;
	int ID;
	int FaultType;
	int usedEntry;
	time_t TimeFaulted;
	time_t TimeReturned;
	static char buf[MAX_OUTPUT_LEN];
	static char tmp[TMP_BUF_LEN];

	GetSuspectDetectorHistoryLogDetails( DetectorNo, &ID, &FaultType, &usedEntry, &TimeFaulted, &TimeReturned);

	sprintf(buf, "Det no. %d, fault type %d on ", ID+1, FaultType);
	localtime_r(&TimeFaulted, &ltime);
	strftime (tmp, TMP_BUF_LEN, "%A %d/%m/%y at %H:%M:%Shrs ", &ltime);
	strcat(buf, tmp);

	if (usedEntry == 1) /*record completed, detector is working again*/
	{
		strcat(buf, " Returned to working on ");
		localtime_r(&TimeReturned, &ltime);
		strftime (tmp, TMP_BUF_LEN, "%A %d/%m/%y at %H:%M:%Shrs ", &ltime);
		strcat(buf, tmp);
	}

	fprintf(UI_DataFile, "%s \r\n", buf);

}


void LoadDataSetToRepo(int RepoNo1, char* FileName, int LoadOnly, int LoadAndMakeActive, int MakeActiveOnly)
{
	UI_RxSiteDataStatusType Status;
	MVInt *startHere = DatasetTestset;
	UI_RxSiteStrictCheckFail StrictErrors = UI_DATA_SET_STRICT_FAIL_NONE;
	int index, OffsetNo = 0, int1;
	MVInt PlansRef[4];
	MVBool OnControl;

	UI_ReadDataFile = fopen( FileName, "r" );
	if ( UI_ReadDataFile != NULL )
	{
		InitializeCriticalSection( &UI_DataReadFileCriticalSection );

		fprintf(UI_DataFile, "<RX SITE DATA >\n");
	
		if (LoadOnly || LoadAndMakeActive)
		{
			MVInt FoundToD;

			EnterCriticalSection( &UI_DataReadFileCriticalSection );
			rewind(UI_ReadDataFile);
			memset(HeaderLine1, 0x00, 30);
			// Read header lines
			fgets(working, 80, UI_ReadDataFile);
			index = strlen(working);
			/*Remove carriage returns*/
			memcpy(HeaderLine1, working, (index - 1));
			memset(working, 0x00, 30);
			fgets(working, 80, UI_ReadDataFile);
			index = strlen(working);
			/*Remove carriage returns*/
			memcpy(HeaderLine2, working, (index - 1));
			fgets(HeaderLine3, 80, UI_ReadDataFile);
			fgets(HeaderLine4, 80, UI_ReadDataFile);
			fgets(HeaderLine5, 80, UI_ReadDataFile);

			// read data 
			for (index=0; index < (WRITE_SIZE * WRITE_ITERATIONS); index++)
			{
				int match;

				match = fscanf(UI_ReadDataFile, "%d", &int1);

				if (match == 0)
				{
					fprintf(UI_DataFile, "Rx Data bad match  \n");
				}
				DatasetTestset[index] = int1;
			}
			LeaveCriticalSection( &UI_DataReadFileCriticalSection );

			UI_Datahand_RxSiteDataInitialise();
			Status = UI_Datahand_RxSiteDataReceiveHeader(HeaderLine1, HeaderLine2, HeaderLine3, HeaderLine4, HeaderLine5);
			fprintf(UI_DataFile, "Rx Header status  = %d\n", Status);

			for (index = 0; index < WRITE_ITERATIONS; index++)
			{
				Status = UI_Datahand_RXSiteDataReceiveData(startHere, WRITE_SIZE, OffsetNo);
				startHere = startHere + WRITE_SIZE;
				OffsetNo = OffsetNo + WRITE_SIZE;
			}

			Status = UI_Datahand_RXSiteDataCalculateChecksumStatus();
			fprintf(UI_DataFile, "Rx data checksum status = %d\n", Status);

			Status = UI_Datahand_RXSiteDataPerformBasicChecks();
			fprintf(UI_DataFile, "Rx data perform basic checks status = %d\n", Status);

			Status = UI_Datahand_RXSiteDataPerformStrictChecks(&StrictErrors);
			fprintf(UI_DataFile, "Rx data perform strict checks status = %d Errors = %d \n", Status, StrictErrors);

			Status = UI_Datahand_RXSiteDataGetTimeOfDayData(&FoundToD, PlansRef);
			fprintf(UI_DataFile, "Rx data perform ToD status = %d found = %d \n", Status, FoundToD);
			fprintf(UI_DataFile, "Rx data perform ToD plans ref plan[1] = %d  plan[2] = %d plan[3] = %d plan[4] = %d \n",
					 PlansRef[0], PlansRef[1], PlansRef[2], PlansRef[3] );

			Status = UI_Datahand_RXSiteDataTxRepo(RepoNo1);
			fprintf(UI_DataFile, "Tx data to repo should pass = %d\n", Status);

			Status = UI_Datahand_RXSiteDataPreLoadRepoCompCheck(RepoNo1, &OnControl);
			fprintf(UI_DataFile, "Preload  checks status = %d On Control   = %d\n", Status, OnControl);

		}
		if (LoadAndMakeActive || MakeActiveOnly)
		{

			Status = UI_Datahand_RXSiteDataLoadToDData(RepoNo1, &index);
			fprintf(UI_DataFile, "ToD load status = %d Plan to load   = %d\n", Status, index);

			Status = UI_Datahand_RXSiteDataLoadActiveArea(RepoNo1, MV_FALSE, 2, &index);
			fprintf(UI_DataFile, "Load active area status = %d RepoNo = %d Plan to load = %d \n", Status, RepoNo1, index);
		}

		fclose(UI_ReadDataFile);
	}
	else
	{
		fprintf(UI_DataFile, "Bad input file name \n");
	}
	fprintf(UI_DataFile, "<END RX SITE DATA >\n");
}

void UI_TestWriteData( void )
{

	/*Alert data */
	MVBool BlockingAlert, BlockingAlertStatus;
	MVBool OverSaturationAlert, OverSaturationAlertStatus;
	MVBool OccupancyAlert, OccupancyAlertStatus;
	int ErrorID;
	int index = 0, laneNo;
	int FirstDetector, LastDetector, PeriodNo, DetectorNo, PeriodExit;
	MVInt16 FirstPeriodInstance, LastPeriodInstance, MaxPeriod;
	time_t datetime;
	struct tm timeinfo;
	MVBool Force[13];


	int CurrentError = 1;

	/* START CRITICAL SECTION */
	EnterCriticalSection( &UI_DataFileCriticalSection );


	if ( UI_DataFile != NULL )
	{
		UI_DataSetDetails DS_Details;
		UI_ActiveDataDetails DS_ActiveData ;


		int result = fseek ( UI_DataFile , 0 , SEEK_SET );

		for (index = 0; index < 5; index++)
		{
			DS_Details = UI_Datahand_GetDataSetDetails((MVByte)index);
			fprintf(UI_DataFile, "<DATA SET DETAILS >\n");
			fprintf(UI_DataFile, "Dataset index = %d\n", index);
			fprintf(UI_DataFile, "Dataset data valid = %d\n", DS_Details.DataValid);
			if (DS_Details.DataValid)
			{
				fprintf(UI_DataFile, "Dataset details date = %s\n", DS_Details.Date);
				fprintf(UI_DataFile, "Dataset details time = %s\n", DS_Details.Time);
				fprintf(UI_DataFile, "Dataset details fname = %s\n", DS_Details.Fname);
				fprintf(UI_DataFile, "Dataset details title = %s\n", DS_Details.Title);
				fprintf(UI_DataFile, "Dataset details version = %s\n", DS_Details.Version);
				fprintf(UI_DataFile, "Dataset details no of detectors = %d\n", DS_Details.NoOfDetectors);
				fprintf(UI_DataFile, "Dataset details scan = %d\n", DS_Details.Scan);
				fprintf(UI_DataFile, "Dataset details max ext = %d\n", DS_Details.MaxExt);
				fprintf(UI_DataFile, "Dataset details min ext = %d\n", DS_Details.MinExt);
				fprintf(UI_DataFile, "Dataset details add gap = %d\n", DS_Details.AddGap);
				fprintf(UI_DataFile, "Dataset details sub gap = %d\n", DS_Details.SubGap);
				fprintf(UI_DataFile, "Dataset details no of stages = %d\n", DS_Details.NoOfStages);
				fprintf(UI_DataFile, "Dataset details no of links = %d\n", DS_Details.NoOfLinks);
				fprintf(UI_DataFile, "Dataset details no of lanes = %d\n", DS_Details.NoOfLanes);
				fprintf(UI_DataFile, "Dataset details checksum complete = %d\n", DS_Details.ChecksumComplete);
				fprintf(UI_DataFile, "Dataset details checksum complete = %d\n", DS_Details.ChecksumFixedData);

			}
			fprintf(UI_DataFile, "<END>\n");

			if (DS_Details.DataValid)
			{
				MVInt BytesRead = 250, RepoArea = index, StartAddress = 0;
				MVInt DataSetData[250];

				fprintf(UI_DataFile, "<DATA SET DATA >\n");

				for (StartAddress = 0; BytesRead == 250; (StartAddress += 250))
				{
					int innerindex;
					BytesRead = UI_Datahand_GetDataSetData(RepoArea, StartAddress, DataSetData);
					//fprintf(UI_DataFile, "Dataset data StartAddress = %d Bytes Read = %d\n", StartAddress, BytesRead);
					for (innerindex = 0; innerindex < BytesRead; innerindex++)
					{
						// just take this out to help output file have less stuff
						// fprintf(UI_DataFile, "%d\n", DataSetData[innerindex]);
					}

				}
				fprintf(UI_DataFile, "<DATA SET DATA END>\n");
			}

			if (DS_Details.DataValid)
			{
				MVInt RepoArea = index, Link, Stage, PeriodInstance, PeriodStart, PeriodEnd, Elements;
				MVBool DataSetTrig1, DataSetTrig2, DataSetTrig3, DataSetTrig4;
				MVInt ErrorIDs[xgnERRORS_NUM_MAX], ErrorValues[xgnERRORS_NUM_MAX];

				fprintf(UI_DataFile, "<DATA SET SITE DATA RepoArea = %d >\n", RepoArea);

				for (Stage = 0; Stage < DS_Details.NoOfStages; Stage++)
				{
					UI_print_LinkBasedDataSourceChar(UI_DATA_SET_SITE_TYPE_INDEXED_LINK_LGREEN, RepoArea, DS_Details.NoOfLinks, Stage, Tcomshr->lgreen[ Stage ]);
					UI_print_LinkIndexedBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_INDEXED_LINK_SDCODE, RepoArea, DS_Details.NoOfLinks, Stage, Tcomshr->sdcode[ Stage ]);
					// Rename this later?
					UI_print_StageBasedMultipleData(UI_DATA_SET_SITE_TYPE_INDEXED_STAGE_CHANGE, RepoArea, Stage, DS_Details, Tcomshr->change[Stage]);
				}

				for ( Link = 0; Link < DS_Details.NoOfLinks; Link++)
				{
					UI_print_LinkIndexedBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_INDEXED_LINK_LANES, RepoArea, mxlnOnlink, Link, Tcomshr->llanes[Link]);
					UI_print_LinkIndexedBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_INDEXED_LINK_EP_DET, RepoArea, 2, Link, Tcomshr->epdet[Link]);
					UI_print_LinkIndexedBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_INDEXED_LINK_EP_EXT, RepoArea, 2, Link, Tcomshr->epext[Link]);
				}

				UI_print_StageBasedData(UI_DATA_SET_SITE_TYPE_STAGE_MAX_GREEN, RepoArea, DS_Details, Tcomshr->max);
				UI_print_StageBasedData(UI_DATA_SET_SITE_TYPE_STAGE_MIN_GREEN, RepoArea, DS_Details, Tcomshr->min);
				UI_print_StageBasedData(UI_DATA_SET_SITE_TYPE_STAGE_LOW_MAX_GREEN, RepoArea, DS_Details, Tcomshr->lowmax);
				UI_print_StageBasedData(UI_DATA_SET_SITE_TYPE_STAGE_PEDMAX_1, RepoArea, DS_Details, Tcomshr->pedmax[0]);
				UI_print_StageBasedData(UI_DATA_SET_SITE_TYPE_STAGE_PEDMAX_2, RepoArea, DS_Details, Tcomshr->pedmax[1]);
				UI_print_StageBasedData(UI_DATA_SET_SITE_TYPE_STAGE_OCC_ALERT_ON, RepoArea, DS_Details, Tcomshr->occual[0]);
				UI_print_StageBasedData(UI_DATA_SET_SITE_TYPE_STAGE_OCC_ALERT_OFF, RepoArea, DS_Details, Tcomshr->occual[1]);
				UI_print_StageBasedDataChar(UI_DATA_SET_SITE_TYPE_STAGE_PFACIL, RepoArea, DS_Details, Tcomshr->pfacil);
				UI_print_StageBasedData(UI_DATA_SET_SITE_TYPE_STAGE_EMXMAX, RepoArea, DS_Details, Tcomshr->emxmax);
				UI_print_StageBasedData(UI_DATA_SET_SITE_TYPE_STAGE_PRXMAX, RepoArea, DS_Details, Tcomshr->prxmax);

				UI_print_LinkBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LINK_LTYPE, RepoArea, DS_Details.NoOfLinks, Tcomshr->ltype);
				UI_print_LinkBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LINK_LMIN, RepoArea, DS_Details.NoOfLinks, Tcomshr->lmin);
				UI_print_LinkBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LINK_LLATOT, RepoArea, DS_Details.NoOfLinks, Tcomshr->llatot);
				UI_print_LinkBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LINK_ESLMAX, RepoArea, DS_Details.NoOfLinks, Tcomshr->eslmax);
				UI_print_LinkBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LINK_LOSTIM, RepoArea, DS_Details.NoOfLinks, Tcomshr->lostim);
				UI_print_LinkBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LINK_STOPEN, RepoArea, DS_Details.NoOfLinks, Tcomshr->stopen);
				UI_print_LinkBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LINK_WAITCH, RepoArea, DS_Details.NoOfLinks, Tcomshr->waitch);
				UI_print_LinkBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LINK_HOLDTM, RepoArea, DS_Details.NoOfLinks, Tcomshr->holdtm);
				UI_print_LinkBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LINK_CANDET, RepoArea, DS_Details.NoOfLinks, Tcomshr->candet);

				UI_print_LaneBasedDataSourceDet(UI_DATA_SET_SITE_TYPE_LANE_IN_DET, RepoArea, DS_Details.NoOfLanes);
				UI_print_LaneBasedDataSourceDet(UI_DATA_SET_SITE_TYPE_LANE_X_DET, RepoArea, DS_Details.NoOfLanes);
				UI_print_LaneBasedDataSourceDet(UI_DATA_SET_SITE_TYPE_LANE_OUT_DET, RepoArea, DS_Details.NoOfLanes);
				UI_print_LaneBasedDataSourceDet(UI_DATA_SET_SITE_TYPE_LANE_IN_SINK_DET, RepoArea, DS_Details.NoOfLanes);
				UI_print_LaneBasedDataSourceDet(UI_DATA_SET_SITE_TYPE_LANE_COMB_X_DET, RepoArea, DS_Details.NoOfLanes);
				UI_print_LaneBasedDataSourceDet(UI_DATA_SET_SITE_TYPE_LANE_X_SINK_DET, RepoArea, DS_Details.NoOfLanes);
				UI_print_LaneBasedDataSourceDet(UI_DATA_SET_SITE_TYPE_LANE_ALT_UP_X_DET, RepoArea, DS_Details.NoOfLanes);
				UI_print_LaneBasedDataSourceDet(UI_DATA_SET_SITE_TYPE_LANE_ALT_DOWN_X_DET, RepoArea, DS_Details.NoOfLanes);
				UI_print_LaneBasedDataSourceDet(UI_DATA_SET_SITE_TYPE_LANE_STOP_LINE_DET, RepoArea, DS_Details.NoOfLanes);
				UI_print_LaneBasedDataSourceDet(UI_DATA_SET_SITE_TYPE_LANE_ASSOC_DET_1_DET, RepoArea, DS_Details.NoOfLanes);
				UI_print_LaneBasedDataSourceDet(UI_DATA_SET_SITE_TYPE_LANE_ASSOC_DET_2_DET, RepoArea, DS_Details.NoOfLanes);

				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_CSPEED, RepoArea, DS_Details.NoOfLanes, Tcomshr->cspeed);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_SATINC, RepoArea, DS_Details.NoOfLanes, Tcomshr->satinc);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_STLOST, RepoArea, DS_Details.NoOfLanes, Tcomshr->stlost);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_LANEWF, RepoArea, DS_Details.NoOfLanes, Tcomshr->lanewf);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_COMTIM, RepoArea, DS_Details.NoOfLanes, Tcomshr->comtim);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_USESF, RepoArea, DS_Details.NoOfLanes, Tcomshr->usesf);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_USEST, RepoArea, DS_Details.NoOfLanes, Tcomshr->usest);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_BONBC, RepoArea, DS_Details.NoOfLanes, Tcomshr->bonbc);

				UI_print_LinkBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LINK_BONTIM, RepoArea, DS_Details.NoOfLinks, Tcomshr->bontim);
				UI_print_LinkBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LINK_BONCUT, RepoArea, DS_Details.NoOfLinks, Tcomshr->boncut);
				UI_print_LinkBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LINK_MIXOUT, RepoArea, DS_Details.NoOfLinks, Tcomshr->mixout);
				UI_print_LinkBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LINK_SHORTL, RepoArea, DS_Details.NoOfLinks, Tcomshr->shortl);
				UI_print_LinkBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LINK_NMAINL, RepoArea, DS_Details.NoOfLinks, Tcomshr->nmainl);

				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_EXITAL, RepoArea, DS_Details.NoOfLanes, Tcomshr->exital);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_MAXMIN, RepoArea, DS_Details.NoOfLanes, Tcomshr->maxmin);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_CRUSIN, RepoArea, DS_Details.NoOfLanes, Tcomshr->crusin);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_CRUSX, RepoArea, DS_Details.NoOfLanes, Tcomshr->crusx);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_MOVQIN, RepoArea, DS_Details.NoOfLanes, Tcomshr->movqin);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_MOVEQX, RepoArea, DS_Details.NoOfLanes, Tcomshr->moveqx);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_QMINON, RepoArea, DS_Details.NoOfLanes, Tcomshr->qminon);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_XOSAT, RepoArea, DS_Details.NoOfLanes, Tcomshr->xosat);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_OSATTM, RepoArea, DS_Details.NoOfLanes, Tcomshr->osattm);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_OSATCC, RepoArea, DS_Details.NoOfLanes, Tcomshr->osatcc);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_GAMBER, RepoArea, DS_Details.NoOfLanes, Tcomshr->gamber);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_SATGAP, RepoArea, DS_Details.NoOfLanes, Tcomshr->satgap);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_CRITG, RepoArea, DS_Details.NoOfLanes, Tcomshr->critg);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_OSATCX, RepoArea, DS_Details.NoOfLanes, Tcomshr->osatcx);
				UI_print_LaneBasedDataSourceInt(UI_DATA_SET_SITE_TYPE_LANE_OSATAL, RepoArea, DS_Details.NoOfLanes, Tcomshr->osatal);
				fprintf(UI_DataFile, "<DATA SET SITE DATA END>\n");

				for (PeriodInstance=1; PeriodInstance < 7; PeriodInstance++)
				{
					UI_GetSatFlowTimingPeriod(PeriodInstance, &PeriodStart, &PeriodEnd);
					fprintf(UI_DataFile, "Period  Instance = %d Start = %d end = %d\n", PeriodInstance, PeriodStart, PeriodEnd);
					fprintf(UI_DataFile, "Tcomshr Instance = %d start = %d end = %d\n", 
						PeriodInstance, Tcomshr->sftims[PeriodInstance-1][0], Tcomshr->sftims[PeriodInstance-1][1]);
				}

					UI_Datahand_GetDataSetTriggerStatus((MVByte)RepoArea, &DataSetTrig1,
					&DataSetTrig2, &DataSetTrig3, &DataSetTrig4);
					fprintf(UI_DataFile, "Read Dataset trig 1 = %d, 2= %d, 3= %d 4= %d \n", 
						DataSetTrig1, DataSetTrig2, DataSetTrig3, DataSetTrig4);
					fprintf(UI_DataFile, "Tcomshr Dataset trig 1 = %d, 2= %d, 3= %d 4= %d \n", 
						Tcomshr->dstrig[0], Tcomshr->dstrig[1], Tcomshr->dstrig[2], Tcomshr->dstrig[3]);

					Elements = UI_Datahand_GetDataErrVal(RepoArea, ErrorIDs, ErrorValues);

					fprintf(UI_DataFile, " Error IDs \n" );

					for (index=0; index < xgnERRORS_NUM_MAX; index++)
					{
						fprintf(UI_DataFile, "  %d  ", ErrorIDs[index] );
					}
					fprintf(UI_DataFile, "\n" );

					for (index=0; index < xgnERRORS_NUM_MAX; index++)
					{
						fprintf(UI_DataFile, " %d", ErrorValues[index] );
					}
					fprintf(UI_DataFile, "\n");

					Elements = UI_Datahand_GetDataTelNo(RepoArea, ErrorIDs);
					fprintf(UI_DataFile, " Tel values \n" );

					for (index=0; index < 16; index++)
					{
						fprintf(UI_DataFile, "  %d  ", ErrorIDs[index] );
					}
					fprintf(UI_DataFile, "\n Tel Tcomshr values \n" );

					for (index=0; index < 16; index++)
					{
						fprintf(UI_DataFile, "  %d  ", Tcomshr->tel[index] );
					}
					fprintf(UI_DataFile, "\n" );

					fprintf(UI_DataFile, " TMA prd = %d Tcomshr prd = %d \n", UI_GetTMAprd(), Tcomshr->tmaprd);

			}
		}
		fprintf(UI_DataFile, "<ACIVE DATA DETAILS >\n");
		DS_ActiveData = UI_Datahand_GetActiveDataDetails();
		fprintf(UI_DataFile, "Active data set = %d\n", DS_ActiveData.CurrentlyActiveDataSet);
		fprintf(UI_DataFile, "Active data triggering set = %d\n", DS_ActiveData.DataTriggerStatus);
		fprintf(UI_DataFile, "Active ToD flag = %d\n", DS_ActiveData.TimeOfDayActive);
		fprintf(UI_DataFile, "Active Plan change pending = %d\n", DS_ActiveData.PlanChangePending);
		fprintf(UI_DataFile, "<END>\n");

		//void LoadDataSetToRepo(int RepoNo1, char* FileName, int LoadOnly, int LoadAndMakeActive, int MakeActiveOnly)
		LoadDataSetToRepo(2, "ThurnbyM7_02.MDS", 1, 0, 0);
		LoadDataSetToRepo(2, "ThurnbyM7_02.MDS", 0, 0, 1);
		LoadDataSetToRepo(3, "ThurnbyM7_03.MDS", 1, 0, 0);
		LoadDataSetToRepo(4, "ThurnbyM7_04.MDS", 1, 0, 0);
		//LoadDataSetToRepo(2, "ThurnbyM7_04.MDS", 0, 1, 0);
		//LoadDataSetToRepo(3, "ThurnbyM7_04.MDS", 1, 0, 0 );

			/*Handle alerts*/
		UI_GetAlertStatus(&BlockingAlert, &OverSaturationAlert, &OccupancyAlert);
		fprintf(UI_DataFile, "<ALERTSTATUS>\n");
		fprintf(UI_DataFile, "%d\n", BlockingAlert);
		fprintf(UI_DataFile, "%d\n", OverSaturationAlert);
		fprintf(UI_DataFile, "%d\n", OccupancyAlert);
		fprintf(UI_DataFile, "<END>\n");

			/*Handle alerts*/
		UI_GetAlertStatus(&BlockingAlert, &OverSaturationAlert, &OccupancyAlert);
		fprintf(UI_DataFile, "<ALERTSTATUS>\n");
		fprintf(UI_DataFile, "%d\n", BlockingAlert);
		fprintf(UI_DataFile, "%d\n", OverSaturationAlert);
		fprintf(UI_DataFile, "%d\n", OccupancyAlert);
		fprintf(UI_DataFile, "<END>\n");


		fprintf(UI_DataFile, "<FORCE_BITS>\n");
		/*Handle force bits */
		UI_GetForceBits(Force);
		for ( index=0; index<10; index++)
		{
			fprintf(UI_DataFile, "%d\n", Force[index]);
		}
		fprintf(UI_DataFile, "<END>\n");

		fprintf(UI_DataFile, "<TAKE OVER BIT>\n");
		/*Handle force bits */
		Force[0] = UI_GetTakeOverStatus();
		fprintf(UI_DataFile, "%d\n", Force[0]);
		fprintf(UI_DataFile, "<END>\n");

		{
			static int count;
			MVByte NoDet = UI_GetNumDetectors();
			MVBool dets[64];
			static int DetectorFailed = 0;
			static int ConfFailed = 0;
			static int ConfRan = 0;
			static int DetRan = 0;

			if (count == 300)
			{ // only reset detectors for test every so often

				DetRan = 1;

				// SET DETECTOR BITS
				for (index=0; index < NoDet; index++)
				{
					dets[index] = MV_FALSE;
				}
				UI_SetDetectorStatus(dets, NoDet);
				// SET ALL DETECTORS TO ZERO 

				for (index=0; index < NoDet; index++)
				{
					if (detsin[index] != 0)
					{
						DetectorFailed = 1;
					}
				}

				for (index=0; index < NoDet; index++)
				{
					dets[index] = MV_TRUE;
				}
				UI_SetDetectorStatus(dets, NoDet);
				
				// SET ALL DETECTORS TO ONE 

				for (index=0; index < NoDet; index++)
				{
					if (detsin[index] != 1)
					{
						DetectorFailed = 1;
					}
				}
				count++;
			}
			else if (count == 700)
			{	// test conf channels at another point
				MVByte NoConf;
				int n, l, total = 0;
				MVBool conf[32];

				ConfRan = 1;

				for (l = 0; l != Tcomshr->nlinks; ++l )
				{
					/* 'N' is no of confirmation channel for phase controlling link L */
					n = Tcomshr->lphase[l];

					if( n > total )
					{
						total = n;
					}
				}

				// confirmation bits = number of stages + phase confirms 
				NoConf = (MVByte) (total + GetNoStages());

				// SET CONFIRM BITS> 
				for (index=0; index < NoConf; index++)
				{
					conf[index] = MV_FALSE;
				}
				UI_SetConfirmStatus(conf, NoConf);
				// SET ALL CONFIRM BITS TO ZERO

				for (index=0; index < NoConf; index++)
				{
					if (confin[index] != 0)
					{
						ConfFailed = 1;
					}
				}

				for (index=0; index < NoConf; index++)
				{
					conf[index] = MV_TRUE;
				}
				UI_SetConfirmStatus(conf, NoConf);
				// SET ALL CONFIRM BITS TO ONE 

				for (index=0; index < NoConf; index++)
				{
					if (confin[index] != 1)
					{
						ConfFailed = 1;
					}
				}
				count=0;
			}
			else
			{
				count++;
			}

			fprintf(UI_DataFile, " Confirm tests ran = %d Confirm result Failed = %d, Detector ran = %d Detector result Failed = %d\n",  ConfRan, ConfFailed, DetRan, DetectorFailed);
		}

		UI_GetAlertMonitoring(&BlockingAlertStatus, &OverSaturationAlertStatus, &OccupancyAlertStatus);
		fprintf(UI_DataFile, "<ALERTTRIGGER>\n");
		fprintf(UI_DataFile, "%d\n", BlockingAlertStatus);
		fprintf(UI_DataFile, "%d\n", OverSaturationAlertStatus);
		fprintf(UI_DataFile, "%d\n", OccupancyAlertStatus);
		fprintf(UI_DataFile, "<END>\n");

		fprintf(UI_DataFile, "<ERRORS>\n");
		while ( !(Errhand_ExceededMaxErrors(CurrentError)) )
		{
			if (Errhand_IsErrorValid(CurrentError))
			{
				int add1, add2, add3;
				Errhand_GetMOVAError(CurrentError, &ErrorID, &datetime, &add1, &add2, &add3);
				localtime_r(&datetime, &timeinfo);
				fprintf(UI_DataFile, "Error ID = %d: AT %d/%d/%d %d:%d \n", ErrorID, timeinfo.tm_mday, timeinfo.tm_mon + 1, 
					( timeinfo.tm_year - 100), timeinfo.tm_hour, timeinfo.tm_min);
			}
			CurrentError++;
		}
		fprintf(UI_DataFile, "<END>\n");

		fprintf(UI_DataFile, "<VersionNumber>\n");
		fprintf(UI_DataFile, "%s\n", UI_GetMovaVersion());
		fprintf(UI_DataFile, "<END>\n");

		datetime = UI_GetMovaTime();
		localtime_r(&datetime, &timeinfo);



		fprintf(UI_DataFile, "<MOVATIME>\n");
		fprintf(UI_DataFile, "%d/%d/%d %d:%d:%d\n", timeinfo.tm_mday, 
			timeinfo.tm_mon + 1, timeinfo.tm_year, timeinfo.tm_hour, 
			timeinfo.tm_min, timeinfo.tm_sec);

		fprintf(UI_DataFile, "<END>\n");

		{
			MVByte ErrorCount, PhoneHome, StageDemanded, WarmUpCount;
			MVBool OnControl, ControllerReady, VAMova, PhoneConnected,
				MultiStageConfirm;
			MVInt PhoneCount;

			fprintf(UI_DataFile, "<FLAGS>\n");

			UI_GetFlagDetails(&ErrorCount, &PhoneHome, &OnControl, 
				&ControllerReady, &StageDemanded, &VAMova, &PhoneConnected,
				&MultiStageConfirm, &PhoneCount, &WarmUpCount);

			fprintf(UI_DataFile, "ErrorCount= %d PhoneHome = %d, OnControl = %d, \
				\n ControllerReady = %d, StageDemanded = %d, VAMova = %d, \
				\n PhoneConnected = %d, MultiStageConfirm = %d, PhoneCount = %d, \
				\n WarmUpCount = %d \n", ErrorCount, PhoneHome, OnControl, 
				ControllerReady, StageDemanded, VAMova, PhoneConnected,
				MultiStageConfirm, PhoneCount, WarmUpCount);

			fprintf(UI_DataFile, "<END>\n");
		}

		fprintf(UI_DataFile, "<LANE FLOW DETAILS>\n");
		index = 0;
		UI_GetPeriodDetails(&FirstPeriodInstance, &LastPeriodInstance, &MaxPeriod);

		if (LastPeriodInstance != FirstPeriodInstance)
		{
			if (LastPeriodInstance > FirstPeriodInstance )
			{
				//Circular buffer so never full up
				PeriodNo = FirstPeriodInstance;
				PeriodExit = LastPeriodInstance;
			}
			else
			{
				PeriodExit = 672;
				if (FirstPeriodInstance > 0)
				{
					PeriodNo = FirstPeriodInstance--;
				}
				else
				{
					PeriodNo = 672 - 1;
				}
			}

			while (index < PeriodExit)
			{
				UI_print_FlowLogs( PeriodNo, GetNoLanes());
				PeriodNo = (PeriodNo + 1) % 672;
				index++;
			}
		}

		fprintf(UI_DataFile, "<END>\n");


		fprintf(UI_DataFile, "<DAILY LANE FLOW DETAILS>\n");
		index = 0;

		for (PeriodNo = 1; PeriodNo < 8; PeriodNo++)
		{
			UI_print_DailyFlowLogs(GetNoLanes(), PeriodNo);
		}

		fprintf(UI_DataFile, "<END>\n");

		fprintf(UI_DataFile, "<LANE OCCUPANCY DETAILS>\n");
		index = 0;
		UI_GetPeriodDetails(&FirstPeriodInstance, &LastPeriodInstance, &MaxPeriod);

		if (LastPeriodInstance != FirstPeriodInstance)
		{
			if (LastPeriodInstance > FirstPeriodInstance )
			{
				//Circular buffer so never full up
				PeriodNo = FirstPeriodInstance;
				PeriodExit = LastPeriodInstance;
			}
			else
			{
				PeriodExit = 672;
				if (FirstPeriodInstance > 0)
				{
					PeriodNo = FirstPeriodInstance--;
				}
				else
				{
					PeriodNo = 672 - 1;
				}
			}

			while (index < PeriodExit)
			{
				UI_print_OccLogs( PeriodNo, GetNoStages());
				PeriodNo = (PeriodNo + 1) % 672;
				index++;
			}
		}

		fprintf(UI_DataFile, "<END>\n");

		fprintf(UI_DataFile, "<stage count and length DETAILS>\n");
		index = 0;
		UI_GetPeriodDetails(&FirstPeriodInstance, &LastPeriodInstance, &MaxPeriod);

		if (LastPeriodInstance != FirstPeriodInstance)
		{
			if (LastPeriodInstance > FirstPeriodInstance )
			{
				//Circular buffer so never full up
				PeriodNo = FirstPeriodInstance;
				PeriodExit = LastPeriodInstance;
			}
			else
			{
				PeriodExit = 672;
				if (FirstPeriodInstance > 0)
				{
					PeriodNo = FirstPeriodInstance--;
				}
				else
				{
					PeriodNo = 672 - 1;
				}
			}

			while (index < PeriodExit)
			{
				UI_print_TravelServiceLogs( PeriodNo, GetNoStages());
				PeriodNo = (PeriodNo + 1) % 672;
				index++;
			}
		}
		fprintf(UI_DataFile, "<END>\n");


		fprintf(UI_DataFile, "<Ped stages log >\n");
		index = 0;
		UI_GetPeriodDetails(&FirstPeriodInstance, &LastPeriodInstance, &MaxPeriod);

		if (LastPeriodInstance != FirstPeriodInstance && (Tcomshr->redped[0] > 0 || &Tcomshr->redped[1] > 0))
		{
			if (LastPeriodInstance > FirstPeriodInstance)
			{
				//Circular buffer so never full up
				PeriodNo = FirstPeriodInstance;
				PeriodExit = LastPeriodInstance;
			}
			else /*the array has been full at some point in the past*/
			{
				PeriodExit = 672;
				if (FirstPeriodInstance > 0)
					PeriodNo = FirstPeriodInstance--;
				else
					PeriodNo = 672 - 1;
			}

			while (index < PeriodExit)
			{
				UI_print_PedSevLogLine(PeriodNo, xgnREDPED_MAX, &Tcomshr->redped[0]);
				PeriodNo = (PeriodNo + 1) % 672;
				index++;
			}
		}
		fprintf(UI_DataFile, "<END>\n");

		fprintf(UI_DataFile, "<END SAT LOG>\n");
		index = 0;
		UI_GetPeriodDetails(&FirstPeriodInstance, &LastPeriodInstance, &MaxPeriod);

		if (LastPeriodInstance != FirstPeriodInstance)
		{
			if (LastPeriodInstance > FirstPeriodInstance )
			{
				//Circular buffer so never full up
				PeriodNo = FirstPeriodInstance;
				PeriodExit = LastPeriodInstance;
			}
			else
			{
				PeriodExit = 672;
				if (FirstPeriodInstance > 0)
				{
					PeriodNo = FirstPeriodInstance--;
				}
				else
				{
					PeriodNo = 672 - 1;
				}
			}

			while (index < PeriodExit)
			{
				UI_print_EndSatLogs( PeriodNo, GetNoStages());
				PeriodNo = (PeriodNo + 1) % 672;
				index++;
			}
		}
		fprintf(UI_DataFile, "<END>\n");

		fprintf(UI_DataFile, "<OVER SAT LOG>\n");
		index = 0;
		UI_GetPeriodDetails(&FirstPeriodInstance, &LastPeriodInstance, &MaxPeriod);

		if (LastPeriodInstance != FirstPeriodInstance)
		{
			if (LastPeriodInstance > FirstPeriodInstance )
			{
				//Circular buffer so never full up
				PeriodNo = FirstPeriodInstance;
				PeriodExit = LastPeriodInstance;
			}
			else
			{
				PeriodExit = 672;
				if (FirstPeriodInstance > 0)
				{
					PeriodNo = FirstPeriodInstance--;
				}
				else
				{
					PeriodNo = 672 - 1;
				}
			}

			while (index < PeriodExit)
			{
				UI_print_OverSatLogs( PeriodNo, GetNoLanes());
				PeriodNo = (PeriodNo + 1) % 672;
				index++;
			}
		}
		fprintf(UI_DataFile, "<END>\n");


		fprintf(UI_DataFile, "<FAULTY DETECTOR LOG>\n");
		index = 0;
		PeriodExit = 0;
		GetSuspectDetectorIndex(&FirstDetector, &LastDetector);

		if (LastDetector > 0)
		{
			if (LastDetector > FirstDetector )
			{
				//Circular buffer so never full up
				PeriodExit = LastDetector - FirstDetector;
			}
			else
			{
				PeriodExit = 100 - FirstDetector + LastDetector;
			}
		}

		DetectorNo = FirstDetector;

		while (index < PeriodExit)
		{
			UI_print_FaultyDetectorLogs( DetectorNo);
			DetectorNo++;
			DetectorNo = (DetectorNo) % (100 - 1);
			index++;
		}

		fprintf(UI_DataFile, "<END>\n");


		fprintf(UI_DataFile, "<SAT FLOW LOG>\n");
		index = 0;

		for (PeriodNo = 1; PeriodNo <= 6; PeriodNo++)
		{
			fprintf(UI_DataFile, "Period No = %d  ", PeriodNo);

			for (laneNo = 0; laneNo < GetNoLanes(); laneNo++)
			{
				float esv = MovaSatFlowStats_GetSatESV_Data(laneNo, PeriodNo);
				if (esv != 0)
				{
					fprintf(UI_DataFile, "%4d ", (int) esv);
				}
				else
				{
					fprintf(UI_DataFile, "0000 ");
				}
			}
			fprintf(UI_DataFile, "\n");
		}

		fprintf(UI_DataFile, "<END>\n");


		fprintf(UI_DataFile, "<ST LOST LOG>\n");
		index = 0;

		for (PeriodNo = 1; PeriodNo <= 6; PeriodNo++)
		{
			fprintf(UI_DataFile, "Period No = %d  ", PeriodNo);

			for (laneNo = 0; laneNo < GetNoLanes(); laneNo++)
			{
				float esv = MovaSatFlowStats_GetSTLostESV_Data(laneNo, PeriodNo);

				fprintf(UI_DataFile, "%4.1f ", (float) esv);

			}
			fprintf(UI_DataFile, "\n");
		}

		fprintf(UI_DataFile, "<END>\n");


		fprintf(UI_DataFile, "<SAT FLOW VERBOSE LOG>\n");
		index = 0;

		for (laneNo = 0; laneNo < GetNoLanes(); laneNo++)
		{
			fprintf(UI_DataFile, "Lane No = %d  \n", laneNo);

			for (PeriodNo = 1; PeriodNo <= 6; PeriodNo++)
			{
				PeriodData SatData = MovaSatFlowStats_GetSatVerbose_Data(laneNo, PeriodNo);
				int LimitCount = SatData.n16Count;
				fprintf(UI_DataFile, "Period No = %d  ", PeriodNo);
				if (LimitCount > 999)
				{
					LimitCount = 999;
				}

				/* Display results for selected time interval*/
				fprintf(UI_DataFile,
						"ESV = %d  Satinc = %.1f last value = %d 99Conf = %3d statsvalid =  %d Count = %3d Mean = %4d smoothed15 = %4d sum squared = %1.1E  \r\n",(int) SatData.rSmoothed50, SatData.rSatinc,
						(int) SatData.rMeanSDisp,(int)SatData.rConfLevel, (int)SatData.lRecAvgMetCriteria,
						(int) LimitCount,(int)SatData.rMeanS, (int) SatData.rSmoothed15, SatData.rSumSqrS );
			}
			fprintf(UI_DataFile, "\n");
		}

		fprintf(UI_DataFile, "<END>\n");


		fprintf(UI_DataFile, "<ST VERBOSE LOG>\n");
		index = 0;

		for (laneNo = 0; laneNo < GetNoLanes(); laneNo++)
		{
			fprintf(UI_DataFile, "Lane No = %d  \n", laneNo);

			for (PeriodNo = 1; PeriodNo <= 6; PeriodNo++)
			{
				PeriodData SatData = MovaSatFlowStats_GetSTLostVerbose_Data(laneNo, PeriodNo);
				int LimitCount = SatData.n16Count;
				fprintf(UI_DataFile, "Period No = %d  ", PeriodNo);

				if (LimitCount > 999)
				{
					LimitCount = 999;
				}

				/* Display results for selected time interval*/
				fprintf(UI_DataFile,
						"%d   %d  %3d  %d  %3d  %4d   %1.1E  \r\n",(int) SatData.rSmoothed50,
						(int) SatData.rMeanSDisp,(int)SatData.rConfLevel, (int)SatData.lRecAvgMetCriteria,
						(int) LimitCount,(int)SatData.rMeanS, SatData.rSumSqrS );

			}
			fprintf(UI_DataFile, "\n");
		}

		fprintf(UI_DataFile, "<END>\n");

		/* Flush stream immediately */
		result = fflush( UI_DataFile );
	}

	/* END CRITICAL SECTION */
	LeaveCriticalSection( &UI_DataFileCriticalSection );
}


void TestUI_Interface(void)
{

}

