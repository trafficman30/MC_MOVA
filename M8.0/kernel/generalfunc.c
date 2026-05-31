/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         generalfunc.c
*
*  TITLE:        MOVA General Functions
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	31-May-2011		7.0.0		AA		PK		CRN003			New development and undergoing first full system testing
*	12-Dec-2011		7.0.1		AB		PK		CRN004			SatFlow and CANDET improvements
*	06-Jul-2012		7.0.3		AC		AK		CRN012			Memory leak in TMA module and statistics lost on dataset change
*
*  (c) Copyright TRL Ltd 2012. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal or
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/


#if defined (TRL_INTEGRATION_TEST)
	#include <windows.h>
	#include "ui_dataaccesstest.h"
#endif

#include <stdio.h>

#if PCMOVA
	#include <Windows.h>
#endif

#include <string.h>

#include "gbltypes.h"
#include "obclock.h"
#include "write.h"
#include "generalfunc.h"
#include "datahand.h"
#include "kdebug.h"
#include "core_interface.h"
#include "dataset_handler.h"

#include "xml_dataset.h"


#define INVALID		-1

extern unsigned char lLH_CONNECTED; /* something plugged into handset? Set in Monitr() */
extern unsigned char lLineDropped; /* telephone line dropped during communication? Set in Answer() */


static char MOVAVersionNo[] = "M8.0.0.435" ;

#if defined (TRL_INTEGRATION_TEST)
CRITICAL_SECTION SysLogCriticalSection;
FILE *	Msgs_logFile = NULL;
CRITICAL_SECTION SatLogCriticalSection;
FILE *	Sat_logFile = NULL;
CRITICAL_SECTION ErrorLogCriticalSection;
FILE *	Error_logFile = NULL;
CRITICAL_SECTION TMALogCriticalSection;
FILE *	TMA_logFile = NULL;
#endif

/*************************************************************************
 *
 * Function: lanesRunStage
 *
 * Description: Function to find the lanes running in a given stage (srStage) and returns the lanes number in *lanesRun.
 *
 * Author:	PKale
 *
 * Date:	08-Feb-2011
 *
 * Params:	int srStage							- Stage Number
 *			int *lanesRun						- Array to indicate if a lane is green during the stage.
 *			int llanes[mxlink][3]				- Links per lane:a matrix giving the numbers allocated to the lane(s)
 *												  which make up each link
 *			signed char lgreen[mxstag][mxlink]	- Link Green: each link L what type of signal the link receives during each stage S
 *
 * Return: void
 *
 ************************************************************************/

void lanesRunStage(int srStage, int *lanesRun, const MovaLlaneArray_t *llanes, const MovaLgreenArray_t *lgreen)
{
	int linkLoop,j,k;

	for (j = 0; j < mxlane; j++)
		lanesRun[j] = -1; /*making sure a -1 is set for all stages*/

	k=0;

	for (j=0; j<GetNoLinks(); j++)
	{
		if(lgreen[srStage][j] >0)
		{
			for (linkLoop=0; linkLoop<3; linkLoop++)
			{
				if (llanes[j][linkLoop]>0)
				{
					lanesRun[k] = llanes[j][linkLoop]-1;
					k++;
				}
			}
		}
	}

}/*end of void lanesRunStage(int srStage, int *lanesRun, int llanes[mxlink][3], signed char lgreen[mxstag][mxlink])*/

/*************************************************************************
 *
 * Function: lanesRunStage
 *
 * Description: Find the links that run in a given stage (srStage) and returns those links in *linksRun.
 *
 * Author: PKale
 *
 * Date: 08-Feb-2011
 *
 * Params:	int srStage							- Stage Number
 *			int *linksRun						- Array to indicate if a link is green during the stage.
 *			signed char lgreen[mxstag][mxlink]	- Link Green: each link L what type of signal the link receives during each stage S
 *
 * Return: void
 *
 ************************************************************************/
void linksRunStage(int srStage, int *linksRun,   const MovaLgreenArray_t *lgreen)
{
	int j,k;

	for (j=0; j<GetNoLinks() ; j++)
		linksRun[j]=-1; /*making sure a -1 is set for all stages*/

	k=0;

	for (j=0; j<GetNoLinks() ; j++)
	{
		if( lgreen[srStage][j] >0)
		{
			linksRun[k] = j;
			k++;
		}
	}

}/*end of void linksRunStage(int srStage, int *linksRun,  const MovaLgreenArray_t *lgreen)*/


/*************************************************************************
 *
 * Function:	writeString
 *
 * Description: Sends to the proper terminal outputs, caters for only formatted strings.
 *
 * Author:		PKale
 *
 * Date:		08-Feb-2011
 *
 * Params:		int terminal		-	Output destination (Terminal screen/telephone line)
 *				const char *op_str	-	pointer to string to be printed.
 *
 * Return:		void
 *
 ************************************************************************/
void writeString(int terminal, const char *op_str)
{
	int j= (int)strlen(op_str);
	if ( terminal == LOCAL_HANDSET && lLH_CONNECTED )
		SendStringToLH(op_str, j);
	else if ( terminal == MODEM_PORT && !lLineDropped )
		SendStringToModem(op_str, j);

}/*end of void writeString(int terminal, const char *op_str)*/

/*************************************************************************
 *
 * Function:	clearScreen
 *
 * Description: Function to clear screen
 *
 * Author:		PKale
 *
 * Date:		08-Feb-2011
 *
 * Params:		int terminal		-	Output destination (Terminal screen/telephone line)
 *				const char *op_str	-	pointer to string to be printed.
 *
 * Return:		int result			-	1-TRUE if screen cleared successfully, 0-FALSE otherwise
 *
 ************************************************************************/
int clearScreen(int outDevice)
{
	static const char sSP_Esc[] = { " \x1B"};
	static const char sClrScr[] = { "[2J \x1B[1;1H" };
	static const char sNormal[] = { "[0m"};
	static const char *const F4a = {"(4a,\\)"};
	int result;
	if (WRITE(outDevice,IOSTAT,NULL,2,FMT,F4a,1,
		STRG,sSP_Esc,2,STRG,sClrScr,10,
		STRG,sSP_Esc,2,STRG,sNormal,3,0))
	{
		result =0;
	}
	else
	{
		result=1;
	}

	return result;

}/*end of int clearScreen(int outDevice)*/

/*************************************************************************
 *
 * Function:	conSuspectToCh
 *
 * Description: Converts integer to char for displaying
 *
 * Author:		PKale
 *
 * Date:		08-Feb-2011
 *
 * Params:		susDetec susDetA	-	Suspect detector state as integer
 *
 * Return:		char chA			-	Character representing the suspect detector state
 *
 ************************************************************************/
char conSuspectToCh(susDetec susDetA)
{
	char chA;

	switch (susDetA)
	{
	case 0:
		chA=' ';
		break;
	case 1:
		chA='A';
		break;
	case 2:
		chA='I';
		break;
	default:
		chA='x';
	}
	return chA;

}/*end of char conSuspectToCh(susDetec susDetA)*/

/*************************************************************************
 *
 * Function:	findXdetLane
 *
 * Description: Checks if a giving detector number (detNo) is an X detector.
 *
 * Author:		PKale
 *
 * Date:		08-Feb-2011
 *
 * Params:		int detNo	-	Detector number
 *				int XorIN	-	index in "det" array
 *				int noLanes	-	Number of lanes in the dataset
 *				MovaDetArray_t *det	- Array holding detector information (from Tcomshr)
 *
 * Return:		int laneNoFound	-	If the detector is found, the lane number (laneNo) is returned.
 *
 ************************************************************************/
int findXdetLane(int detNo, int XorIN, int noLanes, const MovaDetArray_t *det)
{
	int j;
	int laneNoFound;

	laneNoFound=-1;

	/*Careful with index, here detector is 1 based and tcomshr is 0-based*/
	for(j=0; j< noLanes; j++)
	{
		if (det[j][XorIN]==(signed char)detNo)
			laneNoFound=j;
	}

	return laneNoFound;

}/*end of int findXdetLane(int detNo, int XorIN, int noLanes, const MovaDetArray_t *det)*/



/*************************************************************************
 *
 * Function: detGetActivePlan
 *
 * Description: There are three plans that can be running:
 *				a) if there are dstrig detectors
 *				b) if there are ToD plans
 *				c) if the user has loaded an specific plan using L option from DS menu
 *
 *				Option C takes priority above the other two, this priority is determine by global flag
 *				flagDstring, can be set on user request. If the flag is on, dstrig detectors take priority over
 *				ToD plans. Tcomshr->mark2 is modified accordingly.
 *
 * Author: PKale
 *
 * Date: 02-Aug-2011
 *
 * Params:	int *mark2Plan		-	Indicates current Mova dataset number
 *			int dstrig[]		-	Array store detector number used for triggering mova datasets
 *			signed char deton[]	-	Detector On array: Holds the current state of the detectors
 *			int userOverwrites	-	Switch to enable disable triggers
 *
 * Return: void
 *
 ************************************************************************/

void detGetActivePlan(int *mark2Plan, int dstrig[], signed char deton[], int userOverwrites)
{
	static int _detTriggeredPlan = 0;

	int detLoop, temP, found, mark2Tem, cellPos, skip15MinChecking;
	int dayPlan;
	TIMESTAMP_TYPE tTimDat;
	int activePlan, timeOfDayPlan, previousPlanNum;

	temP=4;
	found=0;
	mark2Tem=0;
	skip15MinChecking = 0;

	for (detLoop=0; detLoop<4; detLoop++)
	{
		if (deton[dstrig[detLoop]-1]==1 && dstrig[detLoop]>0 )
		{
			/*If there is more than one DSTRIG on, choose the one with the smallest data plan number.*/
			if  (detLoop<temP)
			{
				temP=detLoop;
				found=1;
			}
		}
	}

	if (found && userOverwrites)  /*triggers on and user hasn't disable any triggers*/
	{
		dayPlan = temP+1;
		_detTriggeredPlan = dayPlan;

		/* If there is a valid and new Time of Day plan */
		if ( dayPlan > 0   )
		{
			mark2Tem= dayPlan;
		}
	}
	else
	{
		mark2Tem=0;

		if (_detTriggeredPlan > 0)
		{
			skip15MinChecking = 1;
			_detTriggeredPlan = 0;
		}
	}

	activePlan = DSH_get_active_data_plan_number();

	if (mark2Tem >0 ) /*dataset by detector activation*/
	{
		if  (mark2Tem != activePlan ) /*be sure the plan is not running already*/
			*mark2Plan = mark2Tem;/*changes by detectors */
	}
	else
	{		
		tTimDat = Com_read_rtc(READ_RTC);

		cellPos = ((tTimDat.day + 1) % 7) * 96 + tTimDat.hours * 4 + tTimDat.minutes / 15 + 1;
		timeOfDayPlan = DSH_get_tod_plan(cellPos - 1);

		if (skip15MinChecking == 0)
		{
			/*Check on the ToD every 15 mins*/
			if (tTimDat.minutes % 15 == 0)
			{
				if (timeOfDayPlan > 0 && timeOfDayPlan != activePlan)
				{
					previousPlanNum = 0;
					/*Get the plan number of the next cell*/
					if (cellPos - 2 > 0)
						previousPlanNum = DSH_get_tod_plan(cellPos - 2);

					/*Only apply the plan change if their previous plan is different than the ToD one.
					This has been done to keep the active plan if the user manually changed it.
					i.e., only override the active one if there is a change in the plan (between the previous
					cell and the current one.*/
					if (previousPlanNum != timeOfDayPlan)
						*mark2Plan = timeOfDayPlan; /*changes by ToD  or user loading requirement*/
				}
			}
		}
		else /*skip15MinChecking == 1*/
		{
			/*Will come here only when an activation of a DP was due to DETRIG and then it come off, so we need to reactivate 
			the DP that was running before the triggering.*/
			if (activePlan != timeOfDayPlan)
				*mark2Plan = timeOfDayPlan; /*changes by ToD  or user loading requirement*/
		}
	}

}/*end of void detGetActivePlan(int *mark2Plan, int dstrig[], signed char deton[], int userOverwrites)*/

int GetNoLanes (void)
{
	return (Tcomshr->nlanes );
}

int GetNoLinks (void)
{
	return (Tcomshr->nlinks );
}

int GetNoStages (void)
{
	return (Tcomshr->stages );
}

int GetNoDetectors (void)
{
	return (Tcomshr->ndets );
}

const MovaDetArray_t * GetDetArray(void)
{
	return(Tcomshr->det);

}

const MovaLlaneArray_t * GetLlaneArray(void)
{
	return(Tcomshr->llanes);
}

const MovaLgreenArray_t * GetLgreenArray(void)
{
	return(Tcomshr->lgreen);
}


const int GetTMAprd(void)
{
	return(Tcomshr->tmaprd);
}


const int *GetOsatAlArray(void)
{
	return(Tcomshr->osatal);
}



const int *GetExitAlArray(void)
{
	return(Tcomshr->exital);

}/*end of - const int *GetExitAlArray(void)*/


const MovaOccalArray_t *GetOcualArray(void)
{
	return(Tcomshr->occual);

}/*end of - const MovaOccalArray_t *GetOcualArray(void)*/


/* Data Access Functions */

/*************************************************************************
 *
 * Function: GetVersionNumber
 *
 * Description: Gets a string that contains the version number
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params: digits - the string to convert
 *			noValidDigits - max size of string
 			MatchExact - the string should have exactly noValidDigits
 *
 * Return: number converted
 *
 ************************************************************************/
char* GetMovaVersionNumber(void)
{
	return (MOVAVersionNo);
}

 int32 GetAverageFlow(int32 Day, int32 HalfHourIncrement, int32 Lane)
 {
	/*return Tcomshr->aveflo[Day - 1][HalfHourIncrement -1][Lane];*/
	 return CI_get_lane_average_flow((uint8)(Day - 1), (uint8)(HalfHourIncrement -1), (uint8)(Lane -1));
 }

void GetMessageDetails(MVInt MessageIndex, int *MessageDetails)
{
	uint8 i;
	int* MessageDetails_ptr = MessageDetails;

	for(i = 0; i < MSG_MAX_LENGTH; i++)
	{
		/**MessageDetails_ptr = Tcomshr->imess[i][MessageIndex];*/
		*MessageDetails_ptr = CI_get_msg_data(i, (uint8)MessageIndex);
		MessageDetails_ptr++;
	}
}

/* End of data access functions */


#if defined (TRL_INTEGRATION_TEST)

static void SysLogInitialise(char * controller_number)
{
	char log_filename[20];

	strcpy(log_filename, "SysLogFile_C");
	strcat(log_filename, controller_number);
	strcat(log_filename, ".txt");

	/* Open file for writing */
	Msgs_logFile = fopen( log_filename, "w" );
	
	if ( Msgs_logFile != NULL )
	{
		InitializeCriticalSection( &SysLogCriticalSection );
	}
}


void SysLogWrite( const char * message )
{
	/* START CRITICAL SECTION */
	EnterCriticalSection( &SysLogCriticalSection );
		
	if ( Msgs_logFile != NULL )
	{
		int result = fputs( message, Msgs_logFile );
			
		/* Flush stream immediately */
		result = fflush( Msgs_logFile );
	}
		
	/* END CRITICAL SECTION */
	LeaveCriticalSection( &SysLogCriticalSection );
}

static void ErrorLogInitialise( char * controller_number )
{
	char log_filename[20];

	strcpy(log_filename, "ErrorLogFile_C");
	strcat(log_filename, controller_number);
	strcat(log_filename, ".txt");

	/* Open file for writing */
	Error_logFile = fopen( log_filename, "w" );

	if ( Error_logFile != NULL )
	{
		InitializeCriticalSection( &ErrorLogCriticalSection );
	}
}

void ErrorLogWrite( const char * message )
{
	/* START CRITICAL SECTION */
	EnterCriticalSection( &ErrorLogCriticalSection );
		
	if ( Error_logFile != NULL )
	{
		int result = fputs( message, Error_logFile );
			
		/* Flush stream immediately */
		result = fflush( Error_logFile );
	}
		
	/* END CRITICAL SECTION */
	LeaveCriticalSection( &ErrorLogCriticalSection );
}

static void SatLogInitialise( char * controller_number )
{
	char log_filename[20];

	strcpy(log_filename, "SatLogFile_C");
	strcat(log_filename, controller_number);
	strcat(log_filename, ".txt");

	/* Open file for writing */
	Sat_logFile = fopen( log_filename, "w" );

	if ( Sat_logFile != NULL )
	{
		InitializeCriticalSection( &SatLogCriticalSection );
	}
}


void SatLogWrite( const char * message )
{
	/* START CRITICAL SECTION */
	EnterCriticalSection( &SatLogCriticalSection );
		
	if ( Sat_logFile != NULL )
	{
		int result = fputs( message, Sat_logFile );
			
		/* Flush stream immediately */
		result = fflush( Sat_logFile );
	}
		
	/* END CRITICAL SECTION */
	LeaveCriticalSection( &SatLogCriticalSection );
}


static void TMALogInitialise( char * controller_number )
{
	char log_filename[20];

	strcpy(log_filename, "TMALogFile_C");
	strcat(log_filename, controller_number);
	strcat(log_filename, ".txt");

	/* Open file for writing */
	TMA_logFile = fopen(log_filename, "w" );

	if ( TMA_logFile != NULL )
	{
		InitializeCriticalSection( &TMALogCriticalSection );
	}
}


void TMALogWrite( const char * message )
{
	/* START CRITICAL SECTION */
	EnterCriticalSection( &TMALogCriticalSection );
		
	if ( TMA_logFile != NULL )
	{
		int result = fputs( message, TMA_logFile );
			
		/* Flush stream immediately */
		result = fflush( TMA_logFile );
	}
		
	/* END CRITICAL SECTION */
	LeaveCriticalSection( &TMALogCriticalSection );
}


void InitialiseIntegrationLogs( void )
{
	/* Extracting the controller number from the MDS filename. */
	char controller_id_str[3] = "0";
	MVInt8 controller_id = 0;

	controller_id = XML_GetRunningControllerID();
	sprintf(controller_id_str, "%d", controller_id);

	SysLogInitialise(controller_id_str);
	SatLogInitialise(controller_id_str);
	ErrorLogInitialise(controller_id_str);
	TMALogInitialise(controller_id_str);
}



/*
Trim trailing spaces off of a string
*/
void TrimLogString(char *stringtochange)
{
	char *current = stringtochange;
	int len = strlen(current);

	current = current + len - sizeof(char);
	while (*current == ' ')
	{
		*current = '\0';
		current--;
	}
}

#endif


/* IRH MOD: M5.0.0: 29/4/03: Added */
/*************************************************************************
*
* Function:    Safe_Strncpy
*
* Description: Copies 'n' characters from the one location to another,
*              placing a null-terminator at the end.
*
* Author:      Ian Henderson
*
* Date:        29/04/03
*
* Params:      szStrOut - output string (will be null terminated)
*              szStrIn - input string
*              nCharsNum - number of characters to copy from the input string
*              to the output string (INCLUDING the null-terminator)
*
* Return:      szStrOut - address of output string
*
************************************************************************/
char * Safe_Strncpy(char * const szStrOut,
	const char * const szStrIn,
	const int nCharsNum)
{
	strncpy(szStrOut, szStrIn, nCharsNum - 1);

	/* Ensure a null-terminator is placed at the end */
	szStrOut[nCharsNum - 1] = '\0';

	return (szStrOut);

} /* char * Safe_Strncpy( char * const szStrOut, ... ) */


#if PCMOVA
MVStatus GetWindowsTempFolder(char * folder_path)
{
	if (GetTempPath(MAX_PATH, folder_path) == 0)
	{
		return MV_FAILURE;
	}
	else
		return MV_SUCCESS;
}
#endif

/*************************************************************************
*
* Function:    MV_MALLOC
*
* Description: This is a place holder function if the signal company want to modify
*				the way of allocating the dynamic memeory.
*
* Author:      Islam Abdelhalim
*
* Date:        21/12/2016
*
* Params:      size - memeory size
*
************************************************************************/
void * MV_MALLOC(size_t size)
{
	/*SIGNAL_COMPANY: Modify this function if needed.*/
	return malloc(size);
}

/*************************************************************************
*
* Function:    MV_FREE
*
* Description: This is a place holder function if the signal company want to modify
*				the way of freeing the dynamic memeory.
*
* Author:      Islam Abdelhalim
*
* Date:        21/12/2016
*
* Params:      ptr - pointer to the location where the data needs to be freed.
*
************************************************************************/
void MV_FREE(void * ptr)
{
	/*SIGNAL_COMPANY: Modify this function if needed.*/
	free(ptr);
}