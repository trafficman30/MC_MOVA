/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         errhand.c
*
*  TITLE:        Mova Kernel: Graceful Error handling 
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	26-Jan-1998		4.0.0		..		MC		SIE_00			First undergoing system test
*	28-Apr-1998		4.0.0		..		PC		SIE_01			Added 'Last50MessStored', see Siemens fault report ref. MOVA-3.
*	03-Mar-1998		4.0.0		..		PC		SIE_02			Overlay on 'Xcomshr', etc., replaced by explicit use of structures.
*	29-Sep-2004		5.0.3		..		IH		SIE_03			New error handing functionalities
*	31-May-2011		7.0.0		AA		PK		CRN003			Several Changes for MOVA 7 release
*	12-Dec-2011		7.0.1		AB		AK		CRN006			Correct compiler warnings
*	15-Mar-2012		7.0.3		AC		AK		CRN009			Changes to conditional compilation flags
*
*  (c) Copyright TRL Ltd 2012. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/

/* Include files */
#include <nucleus.h>
#include <stdarg.h> /* IRH MOD: M5.0.0: 31/08/04: For variable arg macros */
#include <stdio.h>  /* IRH MOD: M5.0.0: 31/08/04: For vsprintf */
#include <string.h> /* IRH MOD: M5.0.0: 07/10/04: For memset */
#include <proj_def.h>
#include <error.h>
#include "gbltypes.h"
#include "mova_op.h"
#include "errhand.h"
#include "getentry.h"
#include "datahand.h"
#include "obclock.h"
#include <diff_bss.h>
#include "kdebug.h"
#include "generalfunc.h"
#include "mova_constants.h"

#include "core_interface.h"

/* IRH MOD: 30/11/05: PCMOVA: For PCError_SetLastKernel and PCError_ExitKernel */
#if defined (PC_WRAPPER)
	#include "../PCError.h"
#endif


/* Field combinations for each column in the error store */
#define ID_AND_DAY 0
#define MONTH_AND_YEAR 1
#define HOURS_AND_MINUTES 2
#define DATA 3

int Last50MessStored;
int nLast50Mess_[MSG_MAX_LENGTH][MAX_MSGS];
static char buf[MAX_OUTPUT_LEN];

int ErrorStore[4][MAX_NO_OF_ERRORS_STORED];
int ErrorCount;

// Put this new code in here
//static PrivateCerrlog IntXerrlog;
//static PrivateCerrlog *const IntTerrlog  = &IntXerrlog;

/* Function prototypes */
void DivideError ( int nDivisor, int nDivErrLocation, ... );
void Switch_MOVA_Off ( int lSaveLast50Mess, int lReBootRequest );

/*====================================================================*/
/* 
   Check for divide error and update error log if found with time
   date and nDivErrLocation, an indicator of the location at which
   the error occurred 
   
   100 = MOVA, 200 = BLKA, 300 = BLKB, 400 = BLKC, 500 = BLKD
   600 = BLKP, 700 = DETSCN
   
*/

/*  IRH MOD: M5.0.0: 31/08/04: Added a variable length argument list 
 *  with more detailed data on what was going on when the error occurred */

void DivideError (int nDivisor, int nDivErrLocation, ... )
{
   if ( nDivisor == 0 )
   {
       va_list  argList;
       char *   szFormat;

       memset( Terrlog->dbzstr, 0, xgnDBZ_MESSAGE_STR_LEN ); 

       va_start( argList, nDivErrLocation );

       /* IRH MOD: M5.0.0: 31/08/04: Store a message in the error log
        * for this divide-by-zero error */
       szFormat = va_arg( argList, char * );
       if ( szFormat )
       {
           vsprintf( Terrlog->dbzstr, szFormat, argList );
       }
       va_end( argList );

       /* IRH MOD: M5.0.0: 27/08/04: Call error logging function */
       Errhand_LogMOVAError( ERROR_ID_DIVIDE_ERROR, nDivErrLocation );

#if 0       
       /* Get time and date & set up compressed message log */
       tTimeDate = Com_read_rtc(READ_RTC);
       ErrorStore[ID_AND_DAY][Terrlog->nerr -1] = 26000 + (int)tTimeDate.date;
       ErrorStore[MONTH_AND_YEAR][Terrlog->nerr -1] = (int)tTimeDate.month * 100 + (int)tTimeDate.year;
       ErrorStore[HOURS_AND_MINUTES][Terrlog->nerr -1] = (int)tTimeDate.hours * 100 + (int)tTimeDate.minutes;
       ErrorStore[DATA][Terrlog->nerr -1] = nDivErrLocation;
       
       /* Increment nerr, maintain 1-base for other tasks */
             Terrlog->nerr++;
       if ( Terrlog->nerr > 100 ) Terrlog->nerr = 1;
#endif

       /* IRH MOD: M5.0.0: 27/08/04: If the error count isn't enough to put MOVA off-control */
       if ( Tcomshr->io[16] < 20 )
       {          
            /* Error count */
            /* IRH MOD: M5.0.0: 27/08/04: Now using function to get user-specified value */
            Tcomshr->io[16] += (signed char)Errhand_GetMOVAErrorValue( ERROR_ID_DIVIDE_ERROR );
            /*Tcomshr->io[16] += 5;*/
       }

       /* Switch MOVA off-control now re start MOVA unless testing. Go back to inspect values */
#ifndef TEST_MOVA       
       Switch_MOVA_Off(SAV_MESSAGES, RESTART_MOVA);
#endif
   }
}

/*
Print and trim to specific devices
*/
static void printErrorMessage(char *MsgBuf, int DeviceID)
{

#ifdef TRL_INTEGRATION_TEST
	if (DeviceID == SYS_LOG_PRINTING)
	{
		TrimLogString(MsgBuf);
		strcat(buf, "\n");
		ErrorLogWrite( buf );
	}
	else
#endif
	{
		strcat(buf, "\r\n");
		writeString(DeviceID, buf);
	}

}

/*************************************************************
 *
 * Function      : Errhand_LogMOVAError
 *
 * Author (Date) : Ian Henderson (26/08/2004)
 *
 * Description   : Records data for a particular error message
 *                 in the compressed message log.                 
 * 
 *                 Note 1: Some messages MOVA sends to the error log
 *                 don't necessarily mean an error has occurred
 *                 e.g. MOVA BACK ON-LINE, CONTROLLER-READY BIT ON/OFF
 * 
 *                 Note 2: If MOVA should be set off-control or rebooted
 *                 after an error, that should be done separately by calling
 *                 Switch_MOVA_Off()
 * 
 *                 Note 3: If the error count (Tcomshr->io[16]) needs
 *                 incrementing, that should also be done separately,
 *                 using Errhand_GetErrorValue() to get the increment
 *                 for a given error ID.
 *
 * Parameters    : nErrorID [in] - ID of error, associated with a string 
 *                  description when the error log is displayed (see Output.c)
 *                 nErrorData [in] - Extra data that helps describe the error (e.g.
 *                  where it occurred, what stage was running at the time).
 *                  May be 0 if no extra information available.
 *
 * Return value  : void
 *
 * Side effects  : As above - updates the error log, and the number
 *                 of errors in the log (Terrlog->nerr)
 *
 *************************************************************/
void Errhand_LogMOVAError( int nErrorID, int nErrorData )
{
    MovaDateTime    tMovaDateTime;
 
    /* Get timestamp from real-time clock */
    Get_DateTime_MANUFAC( &tMovaDateTime );
   
    /* Record error ID, data and timestamp in compressed message log */
    ErrorStore[ID_AND_DAY][ErrorCount-1] = ( ( 10 + nErrorID ) * 1000 ) + tMovaDateTime.nDayOfMonth;
    ErrorStore[MONTH_AND_YEAR][ErrorCount-1] = tMovaDateTime.nMonth * 100 + tMovaDateTime.nYear;
    ErrorStore[HOURS_AND_MINUTES][ErrorCount-1] = tMovaDateTime.nHours * 100 + tMovaDateTime.nMinutes;
    ErrorStore[DATA][ErrorCount-1] = nErrorData;   

#if defined (TRL_INTEGRATION_TEST)
	KDEBUG_WRITE2( DEBUG_LEVEL_INFO, ERRHAND, "Error ID = %d index = %d", nErrorID, ErrorCount-1 );
	Errhand_PrintMOVAError( ErrorCount, SYS_LOG_PRINTING );
#endif

    /* Update the number of errors (1-based) */
    ErrorCount++;
    if ( ErrorCount > MAX_NO_OF_ERRORS_STORED )
    {
        ErrorCount = 1;
    } 

	/* IRH MOD: 30/11/05: PCMOVA: Send the error to the controller */
#if defined (PC_WRAPPER)
	PCError_SetLastKernel( (MVUInt16)nErrorID, nErrorData, 
		( nErrorID == ERROR_ID_DIVIDE_ERROR ? Terrlog->dbzstr : NULL ),
		( nErrorID == ERROR_ID_DIVIDE_ERROR ? strlen( Terrlog->dbzstr ) : 0 ) );
#endif

#if defined (MOVA_DEBUG)
    {
        BOOL            boIsUploading;
        int             nPort;
        
        /* IRH MOD: M5.0.0: 14/09/04: Output the error immediately if 
         * attached locally/remotely.  One or two errors might prevent us 
         * accessing the error log to see what happened, e.g. checksum
         * errors on loading data (the user can't get as far as the MOVA menu
         * to view the error log) */    
        nPort = Get_ActivePort( );
        boIsUploading = Datahand_IsUploadingSiteData( );
        
        /* If the user is connected and not uploading site data */
        if ( nPort && !boIsUploading )
        {
            /* Send the error number */
            Send_FormatString( nPort, 
                "MOVA Error %d logged with data %d\r\n",
                nErrorID, 
                nErrorData );
        }
    }    
#endif /* defined (MOVA_DEBUG) */


} /* void Errhand_LogMOVAError( ... ) */

static int Errhand_GetErrorID(int ErrorRow)
{
	return ((ErrorStore[ID_AND_DAY][ErrorRow - 1]/1000)-10);
}

static int Errhand_GetDay(int ErrorRow)
{
	int ErrorOffset = (Errhand_GetErrorID(ErrorRow) + 10) * 1000;
	return (ErrorStore[ID_AND_DAY][ErrorRow - 1] - ErrorOffset);
}

static void Errhand_GetTimeDate(int ErrorRow, int* dy, int* mm, int* hr, int* yy, int* mn)
{
	*dy = Errhand_GetDay(ErrorRow);
    *mm = ErrorStore[MONTH_AND_YEAR][ErrorRow - 1]/100;
    *hr = ErrorStore[HOURS_AND_MINUTES][ErrorRow - 1]/100; 
    *yy = ErrorStore[MONTH_AND_YEAR][ErrorRow - 1]-*mm*100;
    *mn = ErrorStore[HOURS_AND_MINUTES][ErrorRow - 1]-*hr*100;
}

/*
Printing out ERROR_ID_MOVA_BACK_ON_LINE error
old format:
"(' MOVA BACK ON-LINE AT ',i02,'/',i02,'/',i02,' ',i02,':',i02,' ERR=',i2)";
*/
static void Errhand_PrintERROR_ID_MOVA_BACK_ON_LINE(int Row, int DeviceID)
{
	int dy, mm, hr, yy, mn;

	Errhand_GetTimeDate(Row, &dy, &mm, &hr, &yy, &mn);
	sprintf(buf, " MOVA BACK ON-LINE AT %02d/%02d/%02d %02d:%02d ERR=%2d", dy, mm, yy, hr, mn, ErrorStore[DATA][Row - 1] ); 
	printErrorMessage(buf, DeviceID);
};

/*
Printing out ERROR_ID_FAULTY_DETECTOR error
old format:
"('FAULTY DETECTOR NO',i3,' AT ',i02,'/',i02,'/',i02,' ',i02,':',i02)"
*/
static void Errhand_PrintERROR_ID_FAULTY_DETECTOR(int Row, int DeviceID)
{
	int dy, mm, hr, yy, mn;
    int i = ErrorStore[DATA][Row - 1]/100;

	Errhand_GetTimeDate(Row, &dy, &mm, &hr, &yy, &mn);
	sprintf(buf, "FAULTY DETECTOR NO%3d AT %02d/%02d/%02d %02d:%02d", i, dy, mm, yy, hr, mn);
	printErrorMessage(buf, DeviceID);
}

/*
Printing out ERROR_ID_STAGE_NOT_ENDED error
old format:
(' STAGE NOT ENDED, S',i2,'-',i2,' OFF-LINE ',i02,'/',i02,'/',i02,' ',i02,':',i02,' ERR=',i2)
*/
static void Errhand_PrintERROR_ID_STAGE_NOT_ENDED(int Row, int DeviceID)
{
    int s = ErrorStore[DATA][Row - 1]/1000;
    int d = ErrorStore[DATA][Row - 1]/100-s*10;
    int err = ErrorStore[DATA][Row - 1]-(d*100+s*1000);
	int dy, mm, hr, yy, mn;

	Errhand_GetTimeDate(Row, &dy, &mm, &hr, &yy, &mn);
	sprintf(buf, " STAGE NOT ENDED, S%2d-%2d OFF-LINE %02d/%02d/%02d %02d:%02d ERR=%2d", s, d, dy, mm, yy, hr, mn, err);
	printErrorMessage(buf, DeviceID);
}

/*
Printing out ERROR_ID_INTERGREEN_NOT_ENDED error
old format:
"(' 30-SEC IG FAILS TO END. OFF-LINE AT ',i02,'/',i02,'/',i02,' ',i02,':',i02,' ERR=',i2)"
*/
static void Errhand_PrintERROR_ID_INTERGREEN_NOT_ENDED(int Row, int DeviceID)
{
	int dy, mm, hr, yy, mn;
	int err_data = ErrorStore[DATA][Row - 1];

	Errhand_GetTimeDate(Row, &dy, &mm, &hr, &yy, &mn);
	sprintf(buf, " 30-SEC IG FAILS TO END. OFF-LINE AT %02d/%02d/%02d %02d:%02d ERR=%2d", dy, mm, yy, hr, mn, err_data);
	printErrorMessage(buf, DeviceID);
}

/*
Printing out ERROR_ID_INVALID_STAGE_DEMANDED error
old format:
"(' CHANGE S',i2,' S',i2,' NOT ALLOWED AT ',i02,'/',i02,'/',i02,' ',i02,':',i02,' ERR=',i2)"
*/
static void Errhand_PrintERROR_ID_INVALID_STAGE_DEMANDED(int Row, int DeviceID)
{
	int dy, mm, hr, yy, mn;
    int s = ErrorStore[DATA][Row - 1]/1000;
    int d = ErrorStore[DATA][Row - 1]/100-s*10;
	int err_data = ErrorStore[DATA][Row - 1]-(s*1000+d*100);

	Errhand_GetTimeDate(Row, &dy, &mm, &hr, &yy, &mn);
	sprintf(buf, "CHANGE S%2d S%2d NOT ALLOWED AT  %02d/%02d/%02d %02d:%02d ERR=%2d", s, d, dy, mm, yy, hr, mn, err_data);
	printErrorMessage(buf, DeviceID);
}

/*
Printing out ERROR_ID_WRONG_STAGE_CONFIRMED error
old format:
"(' WRONG STAGE SNO=',i2,' DEM=',i2,' AT ',i02,'/',i02,'/',i02,' ',i02,':',i02,' ERR=',i2)"
*/
static void Errhand_PrintERROR_ID_WRONG_STAGE_CONFIRMED(int Row, int DeviceID)
{
	int dy, mm, hr, yy, mn;
	int s = ErrorStore[DATA][Row - 1]/1000;
	int d = ErrorStore[DATA][Row - 1]/100-s*10;
	int err_data = ErrorStore[DATA][Row - 1]-(s*1000+d*100);

	Errhand_GetTimeDate(Row, &dy, &mm, &hr, &yy, &mn);
	sprintf(buf, " WRONG STAGE SNO=%2d DEM=%2d AT  %02d/%02d/%02d %02d:%02d ERR=%2d", s, d, dy, mm, yy, hr, mn, err_data);
	printErrorMessage(buf, DeviceID);
}

/*
Printing out ERROR_ID_STAGE_STUCK_ON error
old format:
"(' STAGE ',i2,' STUCK FOR',i2,' MIN ',i02,'/',i02,'/',i02,' ',i02,':',i02,' ERR=',i2)"

*/
static void Errhand_PrintERROR_ID_STAGE_STUCK_ON(int Row, int DeviceID)
{
	int iold = ErrorStore[DATA][Row - 1]/1000;
	int t = ErrorStore[DATA][Row - 1]/100-iold*10;
	int dy, mm, hr, yy, mn;
	int err_data = ErrorStore[DATA][Row - 1]- (iold*1000+t*100);

	Errhand_GetTimeDate(Row, &dy, &mm, &hr, &yy, &mn);
	sprintf(buf, " STAGE %2d STUCK FOR %2d MIN '%02d/%02d/%02d %02d:%02d ERR=%2d", iold, t, dy, mm, yy, hr, mn, err_data);
	printErrorMessage(buf, DeviceID);
}

/*
Printing out ERROR_ID_WATCHDOG_ROUTINE error
old format:
"('MOVA WATCHDOG AT ',i02,'/',i02,'/',i02,' ',i02,':',i02,' ERR=',i3,' IO(19)=',i3)"
*/
static void Errhand_PrintERROR_ID_WATCHDOG_ROUTINE(int Row, int DeviceID)
{
	int dy, mm, hr, yy, mn;
	int err_data = ErrorStore[DATA][Row - 1];

	Errhand_GetTimeDate(Row, &dy, &mm, &hr, &yy, &mn);
	sprintf(buf, "MOVA WATCHDOG AT %02d/%02d/%02d %02d:%02d ERR=%3d IO(18)=%3d", dy, mm, yy, hr, mn, err_data, Tcomshr->io[18]);
	printErrorMessage(buf, DeviceID);
}

/*
Printing out ERROR_ID_CONTROLLER_READY_BIT_OFF error
old format:
"('Controller-ready bit OFF at ',i02,'/',i02,'/',i02,' ',i02,':',i02,' ERR=',i3)"
*/
static void Errhand_PrintERROR_ID_CONTROLLER_READY_BIT_OFF(int Row, int DeviceID)
{
	int dy, mm, hr, yy, mn;

	Errhand_GetTimeDate(Row, &dy, &mm, &hr, &yy, &mn);
	sprintf(buf, "Controller-ready bit OFF at %02d/%02d/%02d %02d:%02d ERR=%3d", dy, mm, yy, hr, mn, ErrorStore[DATA][Row - 1]);
	printErrorMessage(buf, DeviceID);
}

/*
Printing out ERROR_ID_CONTROLLER_READY_BIT_ON error
old format:
//"(' Controller-ready bit ON at ',i02,'/',i02,'/',i02,' ',i02,':',i02,' ERR=',i3)"
*/
static void Errhand_PrintERROR_ID_CONTROLLER_READY_BIT_ON(int Row, int DeviceID)
{
	int dy, mm, hr, yy, mn;
	int err_data = ErrorStore[DATA][Row - 1];

	Errhand_GetTimeDate(Row, &dy, &mm, &hr, &yy, &mn);
	sprintf(buf, " Controller-ready bit ON at %02d/%02d/%02d %02d:%02d ERR=%3d", dy, mm, yy, hr, mn, err_data);
	printErrorMessage(buf, DeviceID);
}

/*
Printing out ERROR_ID_PROGRAM_ERROR error
old format:
//		"(' PROGRAM ERROR AT ',i02,'/',i02,'/',i02,' ',i02,':',i02,' CODE=',i3,' RVL=',i4)"
*/
static void Errhand_PrintERROR_ID_PROGRAM_ERROR(int Row, int DeviceID)
{
	int dy, mm, hr, yy, mn;
    int d = ErrorStore[DATA][Row - 1]/100;
    int s = ErrorStore[DATA][Row - 1]-100*d;/* D=CODE,S=RVL*/

	Errhand_GetTimeDate(Row, &dy, &mm, &hr, &yy, &mn);
	sprintf(buf, " PROGRAM ERROR AT %02d/%02d/%02d %02d:%02d CODE=%3d RVL=%4d", dy, mm, yy, hr, mn, d, s);
	printErrorMessage(buf, DeviceID);
}


/*
Printing out ERROR_ID_CHECKSUM_ERROR error
old format:
//"(' CHECKSUM ERROR ON RE-LOADING SITE DATA AT ',i02,'/',i02,'/',i02,' ',i02,':',i02)"
*/
static void Errhand_PrintERROR_ID_CHECKSUM_ERROR(int Row, int DeviceID)
{
	int dy, mm, hr, yy, mn;

	Errhand_GetTimeDate(Row, &dy, &mm, &hr, &yy, &mn);
	sprintf(buf, " CHECKSUM ERROR ON RE-LOADING SITE DATA AT %02d/%02d/%02d %02d:%02d ", dy, mm, yy, hr, mn);
	printErrorMessage(buf, DeviceID);
}

/*
Printing out ERROR_ID_RELOADING_ERROR error
old format:
//"(' DATA #',i2,'; ERROR ON RE-LOADING AT ',i02,'/',i02,'/',i02,' ',i02,':',i02)"
*/
static void Errhand_PrintERROR_ID_RELOADING_ERROR(int Row, int DeviceID)
{
	int dy, mm, hr, yy, mn;
	int err = ErrorStore[DATA][Row - 1];

	Errhand_GetTimeDate(Row, &dy, &mm, &hr, &yy, &mn);
	sprintf(buf, " DATA #%2d; ERROR ON RE-LOADING AT %02d/%02d/%02d %02d:%02d ", err, dy, mm, yy, hr, mn);
	printErrorMessage(buf, DeviceID);
}

/*
Printing out ERROR_ID_MULTIPLE_STAGE_CONFIRMS error
old format:
//"(' MULTIPLE STAGE CONFIRMATIONS - MOVA STOPPED AT ',i02,'/',i02,'/',i02,' ',i02,':',i02)"
*/
static void Errhand_PrintERROR_ID_MULTIPLE_STAGE_CONFIRMS(int Row, int DeviceID)
{
	int dy, mm, hr, yy, mn;

	Errhand_GetTimeDate(Row, &dy, &mm, &hr, &yy, &mn);
	sprintf(buf, " MULTIPLE STAGE CONFIRMATIONS - MOVA STOPPED AT %02d/%02d/%02d %02d:%02d ", dy, mm, yy, hr, mn);
	printErrorMessage(buf, DeviceID);
}

/*
Printing out ERROR_ID_MOVA_RESTARTED error
old format:
//"(' MOVA RESTARTED AT ',i02,'/',i02,'/',i02, ' ',i02,':',i02)"
*/
static void Errhand_PrintERROR_ID_MOVA_RESTARTED(int Row, int DeviceID)
{
	int dy, mm, hr, yy, mn;

	Errhand_GetTimeDate(Row, &dy, &mm, &hr, &yy, &mn);
	sprintf(buf, " MOVA RESTARTED AT %02d/%02d/%02d %02d:%02d ", dy, mm, yy, hr, mn);
	printErrorMessage(buf, DeviceID);
}

/*
Printing out ERROR_ID_DIVIDE_ERROR error
old format:
"(' PROGRAM DIVIDE ERROR AT ',i02,'/',i02,'/',i02,' ',i02,':',i02, ' Location =',i4)"
*/
static void Errhand_PrintERROR_ID_DIVIDE_ERROR(int Row, int DeviceID)
{
	int dy, mm, hr, yy, mn;

	Errhand_GetTimeDate(Row, &dy, &mm, &hr, &yy, &mn);
	sprintf(buf, " PROGRAM DIVIDE ERROR AT %02d/%02d/%02d %02d:%02d Location =%4d", dy, mm, yy, hr, mn, ErrorStore[DATA][Row -1]);
	printErrorMessage(buf, DeviceID);
}

/*
Printing out ERROR_ID_EMPTY_REPOSITORY_AREA error
old format:
"(' REPOSITORY AREA #',i2,'; EMPTY ERROR AT ',i02,'/',i02,'/',i02,' ',i02,':',i02)"
*/
static void Errhand_PrintERROR_ID_EMPTY_REPOSITORY_AREA(int Row, int DeviceID)
{
	int dy, mm, hr, yy, mn;

	Errhand_GetTimeDate(Row, &dy, &mm, &hr, &yy, &mn);
	sprintf(buf, " REPOSITORY AREA #%2d; EMPTY ERROR AT %02d/%02d/%02d %02d:%02d ", ErrorStore[DATA][Row - 1], dy, mm, yy, hr, mn );
	printErrorMessage(buf, DeviceID);
}

/*
Printing out ERROR_ID_REPOSITORY_CHECKSUM error
old format:
"(' REPOSITORY AREA #',i2,'; CHECKSUM ERROR AT ',i02,'/',i02,'/',i02,' ',i02,':',i02)"
*/
static void Errhand_PrintERROR_ID_REPOSITORY_CHECKSUM(int Row, int DeviceID)
{
	int dy, mm, hr, yy, mn;

	Errhand_GetTimeDate(Row, &dy, &mm, &hr, &yy, &mn);
	sprintf(buf, " REPOSITORY AREA #%2d; CHECKSUM ERROR AT %02d/%02d/%02d %02d:%02d ", ErrorStore[DATA][Row - 1], dy, mm, yy, hr, mn );
	printErrorMessage(buf, DeviceID);
}

/*
Printing out ERROR_ID_DEFAULT_LOADED error
old format:
"(' REPOSITORY AREA #',i2,'; DEFAULT DATASET LOADED AT ',i02,'/',i02,'/',i02,' ',i02,':',i02)"
*/
static void Errhand_PrintERROR_ID_DEFAULT_LOADED(int Row, int DeviceID)
{
	int dy, mm, hr, yy, mn;

	Errhand_GetTimeDate(Row, &dy, &mm, &hr, &yy, &mn);
	sprintf(buf, " REPOSITORY AREA #%2d; DEFAULT DATASET LOADED AT %02d/%02d/%02d %02d:%02d ", ErrorStore[DATA][Row - 1], dy, mm, yy, hr, mn );
	printErrorMessage(buf, DeviceID);
}

void Errhand_GetMOVAError(int ErrorRow, int* ErrorID, time_t* time, int* add1, int* add2, int*add3 )
{
	struct tm datetime;
	int min, hour, day, month, year;

	*ErrorID = Errhand_GetErrorID(ErrorRow);

	Errhand_GetTimeDate(ErrorRow, &day, &month, &hour, &year, &min);

	datetime.tm_min = min;
	datetime.tm_hour = hour;
	datetime.tm_year = year + 100;
	datetime.tm_mday = day;
	datetime.tm_mon = month -1;
	datetime.tm_sec = 0;
	datetime.tm_isdst = -1;

	*time = mktime(&datetime);

	/* Fill in additional default data to return */
	*add1 = -1;
	*add2 = -1;
	*add3 = -1;

	switch (*ErrorID)
	{
		/* The errors below have no additional data */
	case ERROR_ID_MOVA_BACK_ON_LINE:
		/* Error Count */
		*add1 = ErrorStore[DATA][ErrorRow - 1];
		break;
	case ERROR_ID_FAULTY_DETECTOR:
		/* Detector ID */
		*add1 = ErrorStore[DATA][ErrorRow - 1]/100;
		break;
	case ERROR_ID_STAGE_NOT_ENDED:
		/* Current Stage */
		*add1 = ErrorStore[DATA][ErrorRow - 1]/1000;
		/* Demanded Stage */
		*add2 = ErrorStore[DATA][ErrorRow - 1]/100-(*add1 * 10);
		/* Error Count */
		*add3 = ErrorStore[DATA][ErrorRow - 1]-((*add2*100)+(*add1*1000));
		break;
	case ERROR_ID_INTERGREEN_NOT_ENDED:
		/* Error Count */
		*add1 = ErrorStore[DATA][ErrorRow - 1];
		break;
	case ERROR_ID_INVALID_STAGE_DEMANDED:
		/* Current Stage */
		*add1 = ErrorStore[DATA][ErrorRow - 1]/1000;
		/* Demanded Stage */
		*add2 = ErrorStore[DATA][ErrorRow - 1]/100-*add1*10;
		/* Error Count */
		*add3 = ErrorStore[DATA][ErrorRow - 1]-(*add2*100+*add1*1000);
		break;
	case ERROR_ID_WRONG_STAGE_CONFIRMED:
		/* Current Stage */
		*add1 = ErrorStore[DATA][ErrorRow - 1]/1000;
		/* Demanded Stage */
		*add2 = ErrorStore[DATA][ErrorRow - 1]/100-*add1*10;
		/* Error Count */
		*add3 = ErrorStore[DATA][ErrorRow - 1]-(*add2*100+*add1*1000);
		break;
	case ERROR_ID_STAGE_STUCK_ON:
		/* current stage */
		*add1 = ErrorStore[DATA][ErrorRow - 1]/1000;
		/* time on in mins */
		*add2 = ErrorStore[DATA][ErrorRow - 1]/100-*add1*10;
		/* Error Count */
		*add3 = ErrorStore[DATA][ErrorRow - 1]-(*add2*100+*add1*1000);
		break;
	case ERROR_ID_WATCHDOG_ROUTINE:
		/* Error Count */
		*add1 = ErrorStore[DATA][ErrorRow - 1];
		/* IO[18]*/
		*add2 = Tcomshr->io[18];
		break;
	case ERROR_ID_CONTROLLER_READY_BIT_OFF:
		/*Error Count */
		*add1 = ErrorStore[DATA][ErrorRow - 1];
		break;
	case ERROR_ID_CONTROLLER_READY_BIT_ON:
		/* Error Count */
		*add1 = ErrorStore[DATA][ErrorRow - 1];
		break;
	case ERROR_ID_PROGRAM_ERROR:
		/* Code */
		*add1 = ErrorStore[DATA][ErrorRow - 1]/100;
		/* RVL */
		*add2 = ErrorStore[DATA][ErrorRow - 1]-100*(*add1);
		break;
	case ERROR_ID_CHECKSUM_ERROR:
		/*No Params */
		break;
	case ERROR_ID_RELOADING_ERROR:
		/* Data set number */
		*add1 = ErrorStore[DATA][ErrorRow - 1];
		break;
	case ERROR_ID_MULTIPLE_STAGE_CONFIRMS:
		/* No Params */
		break;
	case ERROR_ID_MOVA_RESTARTED:
		/* No Params */
		break;
	case ERROR_ID_DIVIDE_ERROR:  /* TRL/SIE addition - divide error */
		/* location */
		*add1 = ErrorStore[DATA][ErrorRow -1];
		break;
	case ERROR_ID_EMPTY_REPOSITORY_AREA:
		/* Repo area */
		*add1 = ErrorStore[DATA][ErrorRow - 1];
		break;
	case ERROR_ID_REPOSITORY_CHECKSUM:
		/* Repo area */
		*add1 = ErrorStore[DATA][ErrorRow - 1];
		break;
	case ERROR_ID_DEFAULT_LOADED:
		/* Repo area */
		*add1 = ErrorStore[DATA][ErrorRow - 1];
		break;
	default:
		break;
	}

}

void Errhand_PrintMOVAError( int ErrorRow, int DeviceID)
{
	int m, mm, hr, yy, mn, errID;

	Errhand_GetTimeDate(ErrorRow, &m, &mm, &hr, &yy, &mn);

	errID = Errhand_GetErrorID(ErrorRow);

	switch(errID)
    {
      /* IRH MOD: M5.0.0: 17/09/04: Now using enumerated constants rather
       * than magic numbers.  Added new repository errors. */     
      case ERROR_ID_MOVA_BACK_ON_LINE:
		  Errhand_PrintERROR_ID_MOVA_BACK_ON_LINE(ErrorRow, DeviceID);
		  break;
      case ERROR_ID_FAULTY_DETECTOR:
		  Errhand_PrintERROR_ID_FAULTY_DETECTOR(ErrorRow, DeviceID);
		  break;
      case ERROR_ID_STAGE_NOT_ENDED:
		  Errhand_PrintERROR_ID_STAGE_NOT_ENDED(ErrorRow, DeviceID);
		  break;
      case ERROR_ID_INTERGREEN_NOT_ENDED:
		  Errhand_PrintERROR_ID_INTERGREEN_NOT_ENDED(ErrorRow, DeviceID);
		  break;
      case ERROR_ID_INVALID_STAGE_DEMANDED:
		  Errhand_PrintERROR_ID_INVALID_STAGE_DEMANDED(ErrorRow, DeviceID);
		  break;
      case ERROR_ID_WRONG_STAGE_CONFIRMED:
		  Errhand_PrintERROR_ID_WRONG_STAGE_CONFIRMED(ErrorRow, DeviceID);
		  break;
      case ERROR_ID_STAGE_STUCK_ON:
		  Errhand_PrintERROR_ID_STAGE_STUCK_ON(ErrorRow, DeviceID);
		  break;
      case ERROR_ID_WATCHDOG_ROUTINE:
		  Errhand_PrintERROR_ID_WATCHDOG_ROUTINE(ErrorRow, DeviceID);
		  break;
      case ERROR_ID_CONTROLLER_READY_BIT_OFF:
		  Errhand_PrintERROR_ID_CONTROLLER_READY_BIT_OFF(ErrorRow, DeviceID);
		  break;
      case ERROR_ID_CONTROLLER_READY_BIT_ON:
		  Errhand_PrintERROR_ID_CONTROLLER_READY_BIT_ON(ErrorRow, DeviceID);
		  break;
      case ERROR_ID_PROGRAM_ERROR:
		  Errhand_PrintERROR_ID_PROGRAM_ERROR(ErrorRow, DeviceID);
		  break;
      case ERROR_ID_CHECKSUM_ERROR:
		  Errhand_PrintERROR_ID_CHECKSUM_ERROR(ErrorRow, DeviceID);
		  break;
      case ERROR_ID_RELOADING_ERROR:
		  Errhand_PrintERROR_ID_RELOADING_ERROR(ErrorRow, DeviceID);
		  break;
      case ERROR_ID_MULTIPLE_STAGE_CONFIRMS:
		  Errhand_PrintERROR_ID_MULTIPLE_STAGE_CONFIRMS(ErrorRow, DeviceID);
		  break;
      case ERROR_ID_MOVA_RESTARTED:
		  Errhand_PrintERROR_ID_MOVA_RESTARTED(ErrorRow, DeviceID);
		  break;
      case ERROR_ID_DIVIDE_ERROR:  /* TRL/SIE addition - divide error */
		  Errhand_PrintERROR_ID_DIVIDE_ERROR(ErrorRow, DeviceID);
		  break;
      /* Repository errors */
      case ERROR_ID_EMPTY_REPOSITORY_AREA:
		  Errhand_PrintERROR_ID_EMPTY_REPOSITORY_AREA(ErrorRow, DeviceID);
		  break;
      case ERROR_ID_REPOSITORY_CHECKSUM:
		  Errhand_PrintERROR_ID_REPOSITORY_CHECKSUM(ErrorRow, DeviceID);
		  break;
      case ERROR_ID_DEFAULT_LOADED:
		  Errhand_PrintERROR_ID_DEFAULT_LOADED(ErrorRow, DeviceID);
		  break;
      default: break;
    }
}



/*************************************************************
 *
 * Function      : Errhand_GetMOVAErrorValue
 *
 * Author (Date) : Ian Henderson (27/08/2004)
 *
 * Description   : Looks up the error value (increment to
 *                 Tcomshr->io[16]) for the given error ID.
 *                 If the error ID cannot be found in the
 *                 Tcomshr->errval table, an error value
 *                 of zero is assumed.
 *
 * Parameters    : nErrorID [in] - ID of error, associated with a string 
 *                  description when the error log is displayed (see Output.c)
 *
 * Return value  : nErrorValue - value (increment) for the error
 *                  or zero if the error ID could not be found
 *
 * Side effects  : None
 *
 *************************************************************/
int Errhand_GetMOVAErrorValue( int nErrorID )
{
    int nErrorValue, nErrorLoop;
    
    /* Default value is zero */
    nErrorValue = 0;
    
    /* For each error in the table of error values */
    for ( nErrorLoop = 0; nErrorLoop < xgnERRORS_NUM; ++nErrorLoop )
    {
        /* If the ID matches... */
        if ( Tcomshr->errval[ ERROR_DATA_TYPE_ID ][ nErrorLoop ] == nErrorID )
        {
            /* Get the corresponding value */
            nErrorValue = Tcomshr->errval[ ERROR_DATA_TYPE_VALUE ][ nErrorLoop ];
             
            nErrorLoop = xgnERRORS_NUM;
        }           
    }
    
    return ( nErrorValue );
 
} /* int Errhand_GetMOVAErrorValue( ... ) */


void Switch_MOVA_Off ( int lSaveLast50Mess, int lSystemReboot )
/*
   Turn MOVA off control, saving last 50 messages and start a soft
   re boot if required
*/
{
   int nIdx, nJdx, nKdx;
   extern char control[NumberOfForce+NumberOfExtra];

   /* Reset on-control flag */
   Tcomshr->io[19] = 0;

   /* Reset all force bits, HI and TO (i.e. everything in 
    * control[] excluding the sync pulse at the end) */
   for ( nIdx = 0; nIdx < ForceArraySize; nIdx++ )
   {
      /* IRH MOD: M5.0.0: 02/09/04: dout array no longer used for outputs
       * - control array is used instead. Also using ForceArraySize
       * instead of mxstag+2 above. */
      /*Tdinout->dout[nIdx] = Terrlog->stgoff;*/
      control[ nIdx ] = Terrlog->stgoff;
   }

#ifdef M8_IMPROVED_LINKING
   control[HOLD_SIGNAL_BIT] = Terrlog->stgoff;
   control[RELEASE_SIGNAL_BIT] = Terrlog->stgoff;
#endif

   /* Scan output channels to all off */
   OutputScan(MOVA_OFF);

   /* IRH MOD: M5.0.0: 31/08/04: prepare MOVA for warmup sequence */
   CI_set_warmup_counter(-1); /*Tcomshr->warmup = -1;*/
   CI_set_current_stage(0);  /*Tcomshr->cusig = 0;*/
   CI_set_next_stage(mxstag);  /*Tcomshr->snext = mxstag;*/
   CI_set_previous_marker(1); /*Tcomshr->premrk = 1;*/
   CI_set_last_stage(mxstag); /*Tcomshr->lastag = mxstag;*/
   
   /* Copy last 50 messages to stored area (if required) */
   if ( lSaveLast50Mess )
   {
      nKdx = CI_get_current_msg_num()-1;
      for ( nIdx=0; nIdx<=56; nIdx++ )
      {
         for ( nJdx=0; nJdx<=49; nJdx++ )
         {
            nLast50Mess_[nIdx][nJdx] = CI_get_msg_data((uint8)nIdx, (uint8)nKdx);
            nKdx++;
            if ( nKdx > 49 ) nKdx = 0;  /* wrap */
         }
      }
      Last50MessStored = TRUE;
   }
      
   /* If reboot required, Set phone home flag to send abort 
      message and try soft reboot 
   */
   if ( lSystemReboot )
   {
      /* IRH MOD: M5.0.0: 06/10/04: Set the phone home flag to 
       * indicate a restart (abort) */
      /*Tcomshr->io[17] = 100;*/
      Errhand_SetMOVAPhoneHome( xgnMOVA_PHONE_HOME_RESTART );

      /* NOW PERFORM REBOOT (call does not return) */
#if defined (PC_WRAPPER)
	  /* IRH MOD: 12/12/05: PCMOVA: A fatal error has occurred - exit PCMOVA */
	  PCError_ExitKernel( PCMOVA_ERROR_KERNEL_FATAL );
#else
      SOFT_ERROR(MOVA_divide_error);
#endif
   }
}


/*************************************************************
 *
 * Function      : Errhand_IsMOVAOnControl
 *
 * Author (Date) : Ian Henderson (21/07/2004)
 *
 * Description   : Returns whether MOVA is on-control
 *                 (wraps the Tcomshr->io[19] flag).
 *                 Perhaps not the best module for this,
 *                 but so many modules change this flag
 *                 it's not clear who 'owns' it.
 *
 * Parameters    : None
 *
 * Return value  : BOOL
 *
 * Side effects  : None
 *
 *************************************************************/
BOOL Errhand_IsMOVAOnControl( void )
{
    return  ( Tcomshr->io[19] == 0 ? FALSE : TRUE );
    
} /* BOOL Errhand_IsMOVAOnControl( ... ) */


/*************************************************************
 *
 * Function      : Errhand_MOVAPhoneHome
 *
 * Author (Date) : Ian Henderson (04/10/04)
 *
 * Description   : Sets a flag that can be used to make 
 *                 MOVA 'phone home' following a fault.
 *
 * Parameters    : nPhoneHomeCode - code to set phone home flag to
 *                  e.g. 99 (xgnMOVA_PHONE_HOME_ERROR) will result in
 *                  MOVA phoning home immediately.
 *                  
 * Return value  : None
 *
 * Side effects  : None
 *
 *************************************************************/
void Errhand_SetMOVAPhoneHome( int nPhoneHomeCode )
{
    Tcomshr->io[17] = (char)nPhoneHomeCode;
    
} /* void Errhand_MOVAPhoneHome( ... ) */

void Errhand_Initialise(void)
{
	int i, j;

	for (i = 1; i <= 4; i++)
	{
		for (j = 1; j <= 100; j++) 
		{
			ErrorStore[i-1][j-1] = 0;/* clear error log*/
		}
	}
}

/*
Determines whether an error row is valid or not.
The coding scheme for the error ID field is below
(ErrorID + 10) *1000 + day. Therefore
Lower limit is 
11000
Upper limit is
((ERROR_IDS_NUM - 1) + 10) * 1000 + max day number

*/
BOOL Errhand_IsErrorValid(int Row)
{
	return ((ErrorStore[ID_AND_DAY][Row - 1] > 11000) && (ErrorStore[ID_AND_DAY][Row - 1] < ((ERROR_IDS_NUM + 9)*1000+32)));
}

void Errhand_ResetErrorCount(void)
{
    ErrorCount = 1;
}


BOOL Errhand_ExceededMaxErrors(int Row)
{
	if (Row > MAX_NO_OF_ERRORS_STORED)
	{
		return(1);
	}
	else
	{
		return(0);
	}
}

int Errhand_GetErrorCount(void)
{
    return(ErrorCount);
}


void Errhand_SetErrorCount(int Count)
{ /* Only used to seed via the UI */
	ErrorCount = Count + 1;
	Tcomshr->io[16] = (char)ErrorCount;
}

int Errhand_IsErrorCountValid(void)
{
	int retVal = 1;

	if ((ErrorCount < 1 ) || (ErrorCount > MAX_NO_OF_ERRORS_STORED))
	{
		retVal = 0;
	}

	return retVal;
}

int Errhand_DecrementErrorCount(void)
{
	int retVal = ErrorCount - 1;

	if (ErrorCount < 1 )
	{
		retVal = 1;
	}
	
	if (ErrorCount > MAX_NO_OF_ERRORS_STORED )
	{
		retVal = MAX_NO_OF_ERRORS_STORED;
	}

	return (retVal);
}