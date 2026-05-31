/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         getentry.h
*
*  TITLE:        MOVA Kernel: Generic fetch\set functionality. 
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	29-Sep-2004		5.0.0		..		MC		SIE_00			First undergoing system test
*	31-May-2011		7.0.0		AA		PK		CRN03			Updated header inline with new Software Quality Plan
*
*  (c) Copyright TRL Ltd 2010. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/
  
#ifndef INCLUDED_GetEntry
#define INCLUDED_GetEntry

/*************************
*   Defines
**************************/

/* Port identifiers (for 'nWx' IO function argument) */
#define xgnPORT_MODEM               (2)
#define xgnPORT_LOCAL_HANDSET       (3)

/* IRH MOD: M5.0.0: 04/10/04: MOVA date/time struct containing
 * the date and time data that the MOVA kernel needs
 * (non-manufacturer specific) 
 * Not likely to change, so I haven't bothered hiding the
 * struct definition in GetEntry.c with accessor functions
 * to get at the members. */
typedef struct MovaDateTimeStruct
{   
   int   nDayOfWeek;  
   int   nDayOfMonth;
   int   nMonth;
   int   nYear;
   
   int   nHours;
   int   nMinutes;
   int   nSeconds;
   
} MovaDateTime;


/*****************************
 *   Public functions
 *****************************/

/* IRH TODO: Manufacturer-specific functions belong elsewhere
 * preferably together in 'MOVA interface' module */

/* Date/time functions */

void            Wait_for_millisecs_MANUFAC(     int iNo_of_millisecs);
/* IRH MOD: M5.0.0: 04/10/04: New date/time function */
MovaDateTime *  Get_DateTime_MANUFAC(           MovaDateTime * pDateTimeCurr );
long            Seconds_after_midnight(         void);

/* Data send/receive functions */

int         WaitKeyAt(              int nWx, int lPrompt);
int         Sin6plus(               int nWx, char idata[], int nSizeLimit );

/* IRH MOD: M5.0.0: 26/07/04: New user IO functions */

void        Set_InputTimeout(       int nTimeOutSecs );
void        Reset_InputTimeout(     void );
int         Get_InputTimeout(       void );

void        Set_InputSleepTime(     uint32 u32InputSleepTime );
void        Reset_InputSleepTime(   void );
uint32      Get_InputSleepTime(     void );

BOOL        Get_Character(          int nWx, char * const pchChar );
BOOL        Get_String(             int nWx, char * const szBuffer );
BOOL        Get_RestrictedString(   int nWx, char * const szRestrictedBuffer, 
                                    int nCharsNumMin, int nCharsNumMax );
BOOL        Get_PositiveInteger(    int nWx, int * const pnNumber );  
BOOL        Get_Confirmation(       int nWx );
BOOL        Get_Line(               int nWx, char * const szBuffer, 
                                    int nBufferLen );

BOOL        Send_FormatString(      int nWx, char * szFormatStr, ... );
int         Get_ActivePort(         void );
BOOL        Is_Connected(           int nWx );

/*****************************
 *   Macros
 *****************************/
#define Get_ExactLengthString( nWx, szRestrictedBuffer, nCharsNum )     \
        Get_RestrictedString(  nWx, szRestrictedBuffer, nCharsNum, nCharsNum )


#endif /* INCLUDED_GetEntry */
