/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         getentry.c
*
*  TITLE:        MOVA Kernel: Generic fetch\set functionality. 
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	29-Sep-2004		5.0.0		..		MC		SIE_00			First undergoing system test
*	04-Mar-2005		5.0.2		..		MC		SIE_01			Modifed Sin6plus() so that there isn't a 'sleep' after every single char is read
*	11-May-2006		5.0.3		..		MC		SIE_02			Stdlib.h and Stdarg.h included for PCMOVA compatibility.
*	31-May-2011		7.0.0		AA		PK		CRN003			Several Changes for MOVA 7 release	
*	15-Mar-2012		7.0.3		AB		AK		CRN009			Changes to conditional compilation flags
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
  
/*****************************
 *   Include files
 *****************************/

#include <nucleus.h>
#include <stdio.h>
#include <stdlib.h> /* IRH MOD: 22/02/05: PCMOVA, embedded too? For atoi() */
#include <stdarg.h> /* IRH MOD: 22/02/05: PCMOVA, embedded too? For var arg macros */
#include <string.h>
#include <ctype.h>
#include <time.h> /* IRH MOD: M5.0.0: 27/07/04: Must be included before proj_def.h */
#include <proj_def.h>
#include <nu_omu.h>
#include "generalfunc.h"

/*#include "time.h"*/
#include "gbltypes.h"
#include "obclock.h"
#include "write.h"

/* IRH MOD: M5.0.0: 27/07/04: Ensure this module's data isn't in the OMU data area 
 * as it's a MOVA module */
#include <diff_bss.h>

#include "getentry.h"


/*****************************
 *   Constants
 *****************************/

/* Maximum number of characters entered by the user 
 * that are stored in Terrlog->buffer */
#define gnINPUT_CHARS_NUM_MAX       (19)
#define gnPASSWORD_CHARS_NUM_MAX    (6)

/* Maximum length of a string that can reliably be converted 
 * to a 16-bit integer */
#define gnINTEGER_STRING_LEN_MAX    (4)

/* Amount of time input routines should wait between
 * successive polls for input (millisecs) */
#define gu32INPUT_SLEEP_TIME_DEFAULT  (50)

/* Default timeout when waiting for user input 
 * (0 == no timeout used) */
#define gu32TIMEOUT_DEFAULT         (0u)

/* Indexes into the Terrlog->buffer[] array */
enum ERRLOG_BUFFER_INDEX
{
    ERRLOG_BUFFER_INVALID = -1,
    
    ERRLOG_BUFFER_PORT,
    ERRLOG_BUFFER_INPUT_START,
    
    ERRLOG_BUFFER_INPUT_END = gnINPUT_CHARS_NUM_MAX,
    
    ERRLOG_BUFFER_DATASET_DOWNLOAD_PASSWORD_START,    
    ERRLOG_BUFFER_DATASET_DOWNLOAD_PASSWORD_END = 
        ERRLOG_BUFFER_DATASET_DOWNLOAD_PASSWORD_START 
            + gnPASSWORD_CHARS_NUM_MAX - 1,
    
    ERRLOG_BUFFER_REMOTE_CONNECTION_PASSWORD_START,    
    ERRLOG_BUFFER_REMOTE_CONNECTION_PASSWORD_END = 
        ERRLOG_BUFFER_REMOTE_CONNECTION_PASSWORD_START 
            + gnPASSWORD_CHARS_NUM_MAX - 1,
                      
    ERRLOG_BUFFER_SIZE
};


/*****************************
 *   External globals
 *****************************/

extern unsigned char lLH_CONNECTED; /* something plugged into handset? Set in Monitr() */
extern unsigned char lLineDropped;  /* telephone line dropped during communication? 
                                       Set in Answer() */

/*****************************
 *   Private globals
 *****************************/

/* Timeout to use when waiting for user input */
static uint32   gu32TimeOut = gu32TIMEOUT_DEFAULT;

/* Time to wait between polls for user input */
static uint32   gu32InputSleepTime = gu32INPUT_SLEEP_TIME_DEFAULT;


/*****************************
 *   External functions
 *****************************/

/* IRH MOD: M5.0.0: 20/07/04: Added (defined in Comms.c) */
extern char CollectModemCharNE( void );
extern char GetHandsetCharNE( void);

/*****************************
 *   Private functions
 *****************************/

static BOOL Get_CharacterOrNewLine( int nWx, char * const pchChar );
static BOOL Send_String( int nWx, const char * szStr, int nStrLen );

#define IS_NEW_LINE( ch )   ( ( (ch) == '\n' ) || ( (ch) == '\r' ) )


/* func WaitKeyAt ======================================================== */
int WaitKeyAt(int nWx, int lPrompt)
{
   int lResult;
   char cCL;
   static const char *const sPrompt = 
      {"(' Press Enter to continue, X to Exit > > >',\\ )"};
   BOOL boIsCharValid;

   if ( lPrompt ) {WRITE(nWx,IOSTAT,NULL,2,FMT,sPrompt,1,0);}
   cCL = 0/*NULL*/; /* IRH MOD: M5.0.0: 26/07/04: NULL is for pointers - (void *)0 */
   
    /* Assume the user cancelled */
    lResult = FALSE;

    /* wait until keyboard input found or disconnected */
    do
    {  
		/* IRH MOD: M5.0.0: 26/07/04: Get_Character replaces Get_Char */
      boIsCharValid = Get_CharacterOrNewLine( nWx, &cCL );      
      cCL = (char)tolower( cCL );
      
      /* If we received a character */
      if ( boIsCharValid )
      {
          /* if CR or LF return TRUE */
          if ( IS_NEW_LINE( cCL ) ) lResult = TRUE;
          
          /* if Ctrl-C or 'x', return FALSE */
          else if ( cCL == '\x3' || cCL == 'x' ) lResult = FALSE;
                    
          /* otherwise continue to look for valid char */
          else cCL = 0;          
      }
      
      /* Else we weren't able to receive a valid character 
       * (maybe the connection was lost, or we timed-out
       * if a timeout was set) */         
   }
   while ( boIsCharValid && cCL == 0 );
      
   return lResult;
}


/*************************************************************
 *
 * Function      : Set_InputTimeout
 *
 * Author (Date) : Ian Henderson (26/07/2004)
 *
 * Description   : Sets the time-out used when trying to get
 *                 input from the user (either connected 
 *                 remotely or locally)
 *
 * Parameters    : nTimeOutSecs [in] - new timeout to use (in seconds)
 *
 * Return value  : void
 *
 * Side effects  : None
 *
 *************************************************************/
void Set_InputTimeout( int nTimeOutSecs )
{
    if ( nTimeOutSecs >= 0 )
    {    
        gu32TimeOut = (uint32)( nTimeOutSecs ) * 1000u;
    }
    
} /* void Set_InputTimeout( int nTimeOutSecs ) */


/*************************************************************
 *
 * Function      : Reset_InputTimeout
 *
 * Author (Date) : Ian Henderson (26/07/2004)
 *
 * Description   : Resets the time-out used when trying to get
 *                 input from the user (either connected 
 *                 remotely or locally) to the default.
 *
 * Parameters    : void
 *
 * Return value  : void
 *
 * Side effects  : None
 *
 *************************************************************/
void Reset_InputTimeout( void )
{
    gu32TimeOut = gu32TIMEOUT_DEFAULT;
    
} /* void Reset_InputTimeout( void ) */


/*************************************************************
 *
 * Function      : Get_InputTimeout
 *
 * Author (Date) : Ian Henderson (26/07/2004)
 *
 * Description   : Gets the time-out used when trying to get
 *                 input from the user (either connected 
 *                 remotely or locally)
 *
 * Parameters    : None
 *
 * Return value  : int - number of seconds being used for timeout
 *
 * Side effects  : None
 *
 *************************************************************/
int Get_InputTimeout( void )
{
    int nTimeOutSecs;
    
    nTimeOutSecs = (int)( gu32TimeOut / 1000u );
    
    return ( nTimeOutSecs );
    
} /* int Get_InputTimeout( void ) */


/*************************************************************
 *
 * Function      : Set_InputSleepTime
 *
 * Author (Date) : Ian Henderson (26/07/2004)
 *
 * Description   : Sets the time to wait between polls for input
 *                 (either connected locally or remotely)
 *
 * Parameters    : u32InputSleepTime [in] - new time to wait
 *                  (in milliseconds)
 *
 * Return value  : void
 *
 * Side effects  : None
 *
 *************************************************************/
void Set_InputSleepTime( uint32 u32InputSleepTime )
{
    gu32InputSleepTime = u32InputSleepTime;
    
} /* void Set_InputSleepTime( uint32 u32InputSleepTime ) */


/*************************************************************
 *
 * Function      : Reset_InputSleepTime
 *
 * Author (Date) : Ian Henderson (26/07/2004)
 *
 * Description   : Resets the time to wait between polls for input
 *                 (either connected locally or remotely)
 *
 * Parameters    : void
 *
 * Return value  : void
 *
 * Side effects  : None
 *
 *************************************************************/
void Reset_InputSleepTime( void )
{
    gu32InputSleepTime = gu32INPUT_SLEEP_TIME_DEFAULT;
    
} /* void Reset_InputSleepTime( void ) */


/*************************************************************
 *
 * Function      : Get_InputSleepTime
 *
 * Author (Date) : Ian Henderson (26/07/2004)
 *
 * Description   : Gets the time to wait between polls for input
 *                 (either connected locally or remotely)
 *
 * Parameters    : None
 *
 * Return value  : int - number of milliseconds wait time
 *
 * Side effects  : None
 *
 *************************************************************/
uint32 Get_InputSleepTime( void )
{
    return ( gu32InputSleepTime );
    
} /* uint32 Get_InputSleepTime( void ) */


/*************************************************************
 *
 * Function      : Get_Character
 *
 * Author (Date) : Ian Henderson (26/07/2004)
 *
 * Description   : Returns a character entered by 
 *                 the user at the terminal (either remote or local).
 *
 * Parameters    : nWx [in] - identifies port that we expect string from
 *                  3 = local handset port
 *                  2 = modem port
 *                 pchChar [out] - address to receive character.
 *                  Set to received character if function succeeds,
 *                  otherwise is left unchanged.
 *
 * Return value  : BOOL - indicates whether the character was
 *                 received successfully (within the time-out, 
 *                 if applicable) and is therefore valid.
 *
 * Side effects  : See Get_String()
 *
 *************************************************************/
BOOL Get_Character( int nWx, char * const pchChar )
{
    char    szBuffer[ 2 ];
    BOOL    boIsCharValid, boIsStringValid;

    boIsCharValid = FALSE;

    /* Get an input string of 1 character length */
    boIsStringValid = Get_RestrictedString( nWx, szBuffer, 1, 1 );
    
    /* If the user entered just one character, get it */
    if ( boIsStringValid )
    {
        *pchChar = szBuffer[ 0 ];
        
        boIsCharValid = TRUE;
    }
    
    return ( boIsCharValid );
 
} /* BOOL Get_Character( int nWx, char * const szBuffer ) */


/*************************************************************
 *
 * Function      : Get_CharacterOrNewLine
 *
 * Author (Date) : Ian Henderson (26/07/2004)
 *
 * Description   : Returns a character entered by 
 *                 the user at the terminal or a newline 
 *                 character if the user just pressed
 *                 Enter (either remote or local).
 *
 * Parameters    : nWx [in] - identifies port that we expect string from
 *                  3 = local handset port
 *                  2 = modem port
 *                 pchChar [out] - address to receive character
 *                  Set to received character if function succeeds,
 *                  otherwise is left unchanged. 
 *
 * Return value  : BOOL - indicates whether the character was
 *                 received successfully (within the time-out, 
 *                 if applicable) and is therefore valid.
 *
 * Side effects  : See Get_String()
 *
 *************************************************************/
static BOOL Get_CharacterOrNewLine( int nWx, char * const pchChar )
{
    char    szBuffer[ 2 ];
    BOOL    boIsCharValid, boIsStringValid;

    boIsCharValid = FALSE;

    /* Get an input string of 0 or 1 character length */
    boIsStringValid = Get_RestrictedString( nWx, szBuffer, 0, 1 );
    
    /* If the user entered 0 or 1 chars */
    if ( boIsStringValid )
    {
        /* If the user didn't enter anything 
         * (i.e. just pressed enter) */
        if ( strlen( szBuffer ) == 0 )
        {
            *pchChar = '\n';
        }
        else
        {        
            *pchChar = szBuffer[ 0 ];
        }
        
        boIsCharValid = TRUE;
    }
    
    return ( boIsCharValid );
 
} /* static BOOL Get_CharacterOrNewLine( int nWx, ... ) */


/*************************************************************
 *
 * Function      : Get_Confirmation
 *
 * Author (Date) : Ian Henderson (26/07/2004)
 *
 * Description   : Returns the answer to a 'yes/no' type 
 *                 question. Keeps retrying
 *                 until the user answers 'properly'
 *                 (with 'y' or 'n').
 *
 * Parameters    : nWx [in] - identifies port that we expect string from
 *                  3 = local handset port
 *                  2 = modem port
 *
 * Return value  : BOOL - TRUE indicates the user answered in the
 *                 affirmative; FALSE indicates the opposite.
 *
 * Side effects  : See Get_String()
 *
 *************************************************************/
BOOL Get_Confirmation( int nWx )
{
    char    chAnswer;
    BOOL    boIsCharValid, boIsAnswerValid;

    boIsAnswerValid = FALSE;
    
    /* Assume the user doesn't want to go ahead 
     * (the safest option) */
    chAnswer = 'n';

    /* Repeat until we receive a definite Yes or No 
     * or we fail to receive a character 
     * (e.g. connection lost, or timed out if set) */
    do
    {     
        /* Get a character */
        boIsCharValid = Get_Character( nWx, &chAnswer );
        
        /* If the character was got successfully */
        if ( boIsCharValid )
        {
            chAnswer = (char)tolower( chAnswer );
                    
            if (    ( chAnswer == 'y' )
               ||   ( chAnswer == 'n' ) )
            {
                boIsAnswerValid = TRUE;
            }
            
        } /* If the character was got successfully */                 
    }
    while ( boIsCharValid && !boIsAnswerValid );
    
    return ( chAnswer == 'y' ? TRUE : FALSE );
 
} /* BOOL Get_Confirmation( int nWx ) */


/*************************************************************
 *
 * Function      : Get_String
 *
 * Author (Date) : Ian Henderson (23/07/2004)
 *
 * Description   : Returns a null-terminated string of up to 
 *                 gnINPUT_CHARS_NUM_MAX characters entered by 
 *                 the user at the terminal (either remote or local).
 *
 * Parameters    : nWx [in] - identifies port that we expect string from
 *                  3 = local handset port
 *                  2 = modem port
 *                 szBuffer [out] - buffer to received null-terminated
 *                  string. Must be at least gnINPUT_CHARS_NUM_MAX + 1
 *                  in length (+ 1 for null-terminator).
 *                  Set to received string if function succeeds,
 *                  otherwise is left unchanged.
 *
 * Return value  : BOOL - indicates whether the string was
 *                 received successfully (within the time-out, 
 *                 if applicable) and is therefore valid.
 *
 * Side effects  : Zeroes the input data in the Terrlog->buffer
 *                 Also resets the variable Terrlog->buffer[0],
 *                 which signifies where the data came from (leaves 
 *                 the password strings stored at the end of this
 *                 buffer alone)
 *                 Only does this if the string is received 
 *                 successfully
 *
 *************************************************************/
BOOL Get_String( int nWx, char * const szBuffer )
{
    char *  pchPort;
    char *  pchInput;
    BOOL    boIsStringValid;
    uint32  u32TimeElapsed;
    /*int     nStrLen;*/

   static const char *const sMess = 
       {"(' Other port in use - please try later',\\ )"};
    
    boIsStringValid = FALSE;
    
    pchPort = &( Terrlog->buffer[ ERRLOG_BUFFER_PORT ] );
    pchInput = &( Terrlog->buffer[ ERRLOG_BUFFER_INPUT_START ] );
    
    /* Clear the port number for input of new data */
    *pchPort = 0; 
 
    /* Initialise timeout in milliseconds */
    u32TimeElapsed = 0u;

    /* While nothing has been received within the valid time */
    while ( !( *pchPort ) && ( u32TimeElapsed <= gu32TimeOut ) )
    {        
        /* If the connection has been lost */
        if ( ( ( nWx == xgnPORT_MODEM ) && lLineDropped ) || 
             ( ( nWx == xgnPORT_LOCAL_HANDSET ) && !lLH_CONNECTED ) )
        { 
            /* Don't wait any longer */
            break;    
        }
     
        /* If a wait time is specified, wait for the user to enter something 
         * N.B. It seems we must wait at least some time, otherwise
         * we get a crash after a few seconds with the message:
         * "ERROR: [QSYSCTL] failed to post to Q"
         * I'm not sure exactly why this occurs - 
         * I think it's a thread synchronisation issue.
         * One thread appears to be posting to a 'message box'
         * (used for passing data between threads) quicker 
         * than other thread(s) are reading from it, with
         * the result that it is filling up. */
        if ( gu32InputSleepTime > 0u )
        {
            Wait_for_millisecs_MANUFAC( gu32InputSleepTime );                       
        }
        
        /* If a time-out is set, update the timer 
         * N.B. Time out needs a non-zero sleep time (above)
         * to work - not good. */
        if ( gu32TimeOut > 0u )
        {
            u32TimeElapsed += gu32InputSleepTime;
        }        

        /* If something has been received (via LH or modem) */
        if ( ( ( *pchPort ) == xgnPORT_LOCAL_HANDSET ) || 
             ( ( *pchPort ) == xgnPORT_MODEM ) )
        {
            /* If we didn't receive the characters from the expected port*/
            if ( nWx != *pchPort )
            {
                /* Someone trying to use other port */
                WRITE( *pchPort, IOSTAT,NULL,2,FMT,sMess,1,0);
            }
            
            /* Else received characters from the correct port */
            else
            {
                /* Copy the characters to the output buffer (+1 for null terminator) */
                Safe_Strncpy( szBuffer, pchInput, gnINPUT_CHARS_NUM_MAX + 1 );
               
                boIsStringValid = TRUE;
            }            
            
        } /* If something has been received (via LH or modem) */     
        
    } /* While nothing has been received within the valid time */
 
    /* If we successfully received the string */
    if ( boIsStringValid )
    {
        /* Clear port number and input data */
        *pchPort = 0;
        memset( pchInput, 0, gnINPUT_CHARS_NUM_MAX );
    }
    /* Otherwise, assume the input data is for another part of the program 
     * so leave it alone */
 
    return ( boIsStringValid );
    
} /* BOOL Get_String( int nWx, ... ) */


/*************************************************************
 *
 * Function      : Get_RestrictedString
 *
 * Author (Date) : Ian Henderson (26/07/2004)
 *
 * Description   : Returns a null-terminated string entered by 
 *                 the user at the terminal (either remote or local).
 *                 String entered by user must be within
 *                 a specified length range (between 0 and 
 *                 gnINPUT_CHARS_NUM_MAX).
 *
 * Parameters    : nWx [in] - identifies port that we expect string from
 *                  3 = local handset port
 *                  2 = modem port
 *                 nCharsNumMin [in] - minimum number of characters that 
 *                  the user may enter.
 *                 nCharsNumMax [in] - maximum number of characters that
 *                  the user may enter (can equal nCharsNumMin
 *                  if an exact number is required)
 *                 szRestrictedBuffer [out] - buffer to received null-terminated
 *                  string. Must be at least nCharsNumMax + 1
 *                  in length to accommodate null-terminator
 *                  Set to received string if function succeeds,
 *                  otherwise is left unchanged. 
 *
 * Return value  : BOOL - indicates whether the string was
 *                 received successfully and is therefore valid.
 *
 * Side effects  : See Get_String
 *
 *************************************************************/
BOOL Get_RestrictedString(  int nWx, char * const szRestrictedBuffer, 
                            int nCharsNumMin, int nCharsNumMax )
{
    char    szBuffer[ gnINPUT_CHARS_NUM_MAX + 1 ]; 
    BOOL    boIsStringValid, boIsRestrictedStringValid;
    int     nStrLen;

    boIsRestrictedStringValid = FALSE;

    /* Get an input string */
    boIsStringValid = Get_String( nWx, szBuffer );
    
    if ( boIsStringValid )
    {    
        /* Get its length as an integer */
        nStrLen = (int)( strlen( szBuffer ) );
    
        /* If the string is within the valid range */
        if (    ( nStrLen >= nCharsNumMin )
           &&   ( nStrLen <= nCharsNumMax ) )
        {
            /* Copy it to the output buffer (+1 for null-terminator) */
            Safe_Strncpy( szRestrictedBuffer, szBuffer, nStrLen + 1 ); 
            
            boIsRestrictedStringValid = TRUE;
        }
    }
 
    return ( boIsRestrictedStringValid );
    
} /* BOOL Get_RestrictedString(  int nWx, ... ) */


/*************************************************************
 *
 * Function      : Get_PositiveInteger
 *
 * Author (Date) : Ian Henderson (26/07/2004)
 *
 * Description   : Returns a positive integer entered by the user at
 *                 the terminal (either remote or local).
 *                 NOTE 1: the integer can only be in the range
 *                 0 <= n <= 9999 to prevent overflow
 *                 when converting from string to int using atoi()
 *                 NOTE 2: the first character entered by the user 
 *                 must be a digit (0-9).
 *
 * Parameters    : nWx [in] - identifies port that we expect number from
 *                  3 = local handset port
 *                  2 = modem port
 *                 pnNumber [out] - address to receive integer.
 *
 * Return value  : BOOL - indicates whether the number was
 *                 received successfully and is therefore valid.
 *
 * Side effects  : See Get_String()
 *
 *************************************************************/
BOOL Get_PositiveInteger( int nWx, int * const pnNumber )
{
    char    szBuffer[ gnINTEGER_STRING_LEN_MAX + 1 ];
    BOOL    boIsNumberValid, boIsStringValid;

    boIsNumberValid = FALSE;

    /* Get an input string of between 1 and 4 characters */
    boIsStringValid = Get_RestrictedString( nWx, szBuffer, 
        1, gnINTEGER_STRING_LEN_MAX );
    
    /* If the string was recieved successfully 
     * and the first character is a digit */
    if ( boIsStringValid && isdigit( szBuffer[ 0 ] ) )
    {
         *pnNumber = atoi( szBuffer );
         
         boIsNumberValid = TRUE;
    }

    return ( boIsNumberValid );

} /* BOOL Get_PositiveInteger( int nWx, int * const pnNumber ) */


/*************************************************************
 *
 * Function      : Is_Connected
 *
 * Author (Date) : Ian Henderson (20/09/2004)
 *
 * Description   : Returns whether the given port is 
 *                 connected.
 *
 * Parameters    : nWx [in] - identifies port that we expect number from
 *                  3 = local handset port
 *                  2 = modem port
 *
 * Return value  : BOOL - indicates whether the port is 
 *                  connected.
 *
 * Side effects  : None
 *
 *************************************************************/
BOOL Is_Connected( int nWx )
{
    if ( ( nWx == xgnPORT_LOCAL_HANDSET && lLH_CONNECTED )  
      || ( nWx == xgnPORT_MODEM && !lLineDropped ) )
    {
        return TRUE; 
    }
    else
    {
        return FALSE; 
    }
    
} /* BOOL Is_Connected( ... ) */


int Sin6plus(int nWx, char idata[], int nSizeLimit)
{
/* Reads MOVA data coming in from port. phone and term disabled
   at this point so we can read directly from the port */
 
   int i;
   char cLH;
   const char chAck = '\x6';
   
   /* wait here unitl something found */
   /* IRH MOD: M5.0.0: 17/08/04: Replacing NULL - macro is for pointers - (void *)0
    * (causes a compiler warning) */
   cLH = '\0';
   
   /* IRH MOD: M5.0.0: 27/07/04: 
    * Sizeof won't work - determines size of array at compile time.
    * Need to pass size of idata as a param */
   memset(idata, 0, /*sizeof(idata)*/ nSizeLimit );   
   
   /* Wait for a valid character */
   /* Filter off any LFs & CRs left over after last call */
   while ( cLH == '\r' || cLH == '\n' || cLH == '\0' )
   {
      /* IRH MOD: M5.0.0: 17/08/04: Use defines for port identifiers */
      if ( nWx == xgnPORT_LOCAL_HANDSET )
      {
         if ( !lLH_CONNECTED ) return (FALSE);
         cLH = GetHandsetCharNE();
      }
      else if ( nWx == xgnPORT_MODEM )
      {
         if ( lLineDropped  ) return (FALSE);
         cLH = CollectModemCharNE();
      }
      else    /* fault in call: nWx != 1 or 2 */
         return (FALSE);

      /* IRH MOD: M5.0.0: 05/09/04: Use variable wait time */
      /* IRH MOD: SIE_01: 04/03/05: BUGFIX! Only wait if we got a null char */
      if ( gu32InputSleepTime > 0u && cLH == '\0' )
      {
        Wait_for_millisecs_MANUFAC( gu32InputSleepTime );         
      }
      
   }
   
   /* 
   cLH is valid character. 
   Continue to read into idata[] until LF or CR found, 
   or if no. valid chars == nSizeLimit (as past in call)  */
   i = 0;   
   while ( cLH != '\r' && cLH != '\n' && i < nSizeLimit )
   {
      /* abort on CONTROL-C or Disconnection */
      /* IRH MOD: M5.0.0: 24/09/04: All control chars apart from 
       * CR/LF and backspace are filtered off by 
       * GetMOVACharFromHandset in MOVA_IF.c.  I'm not sure
       * I want to change this code to allow through CTRL-C's
       * in case it causes problems elsewhere.  Therefore 
       * we're looking for a backspace during a download 
       * to indicate that the user cancelled 
       * (MOVAC updated accordingly).
       * N.B. CTRL-C won't work with MOVA 4 to cancel a download
       * either, so MOVAC can't allow the user to cancel downloads for
       * MOVA 4 units as it won't work (MOVA will just keep waiting for i/p). */
      if (  ( cLH == '\b'/*'\x3'*/ ) 
         || ( nWx == xgnPORT_LOCAL_HANDSET && !lLH_CONNECTED ) 
         || ( nWx == xgnPORT_MODEM && lLineDropped ) )
      {
        return (FALSE);
      }
     
      /* add character to idata */
      idata[i++] = cLH;
      cLH = '\0';

      /* look for another */
      while (cLH == '\0')
      {
         if ( nWx == xgnPORT_LOCAL_HANDSET )
         {
            if ( !lLH_CONNECTED ) return (FALSE);
            cLH = GetHandsetCharNE();
         }
         else if ( nWx == xgnPORT_MODEM )
         {
            if ( lLineDropped  ) return (FALSE);
            cLH = CollectModemCharNE();
         }
         else    /* fault in call: nWx != 1 or 2 */
            return (FALSE);

        /* IRH MOD: M5.0.0: 05/09/04: Use variable wait time */
        /* IRH MOD: SIE_01: 04/03/05: BUGFIX! Only wait if we got a null char */
        if ( gu32InputSleepTime > 0u && cLH == '\0' )
        {
          Wait_for_millisecs_MANUFAC( gu32InputSleepTime );                       
        }
        
      }
      
   }

   /* Let the sender (MOVAC) know we've received the line */
   /* IRH TODO: Ideally, we shouldn't be using this software flow-control
    * but otherwise MOVAC sends us the file faster than we can 
    * process it, and a crash occurs (check that MOVA is setting DTR low
    * when serial buffer full? The serial routines in MOVAC are set to 
    * check DTR - is this happening?).  Need to talk to someone who
    * knows something about serial communication... */
   Send_String( nWx, &chAck, 1 );
      
   return (TRUE);
}


/*************************************************************
 *
 * Function      : Send_FormatString
 *
 * Author (Date) : Ian Henderson (05/09/2004)
 *
 * Description   : Sends the given (printf style) formatted string 
 *
 * Parameters    : nWx [in] - identifies port to send string to
 *                  3 = local handset port
 *                  2 = modem port
 *                 szFormatStr [in] - string to send with format
 *                  specifiers e.g. "Time is: %d"
 *                 ... [in] - variable length list of arguments
 *                  corresponding to the format specifiers in
 *                  szFormatStr
 *
 * Return value  : BOOL - indicates whether the string was
 *                  sent successfully
 *
 * Side effects  : See Send_String()
 *
 *************************************************************/
BOOL Send_FormatString( int nWx, char * szFormatStr, ... )
{
    BOOL        boIsFormatStringSent;
    va_list     argList;
    int         nStrLen;

#if defined (PC_WRAPPER)
    /* IRH MOD: 23/05/06: PCMOVA: Make this function thread safe
    by placing the buffer on the stack.  This should ideally
    be done for the embedded version too - see comments in Write.c. */
    char        szBuffer[512];
#else
    /* Static, as I'm paranoid about running out of stack */
    static char szBuffer[512];
#endif

    va_start( argList, szFormatStr );
    vsprintf( szBuffer, szFormatStr, argList );
    va_end( argList );
    
    nStrLen = (int)strlen( szBuffer );

    boIsFormatStringSent = Send_String( nWx, szBuffer, nStrLen );
     
    return ( boIsFormatStringSent );
 
} /* BOOL Send_FormatString( ... ) */


/*************************************************************
 *
 * Function      : Send_String
 *
 * Author (Date) : Ian Henderson (05/09/2004)
 *
 * Description   : Sends the given string to the local/remote
 *                 connection (as appropriate).
 *
 * Parameters    : nWx [in] - identifies port to send string to
 *                  3 = local handset port
 *                  2 = modem port
 *                 szStr [in] - string to send
 *                 nStrLen [in] - size of string (not including
 *                  null-terminator)
 *
 * Return value  : BOOL - indicates whether the string was
 *                  sent successfully
 *
 * Side effects  : See SendStringToModem() or SendStringToLH()
 *
 *************************************************************/
static BOOL Send_String( int nWx, const char * szStr, int nStrLen )
{
    BOOL    boIsStringSent;
    
    /* Assume failure */
    boIsStringSent = FALSE;     
    
    switch ( nWx )
    {
        case xgnPORT_LOCAL_HANDSET:
        {
            if ( lLH_CONNECTED )
            {
                SendStringToLH( szStr, nStrLen );
    
                /* Assume it went through OK 
                 * (SendStringToLH doesn't return a value) */
                boIsStringSent = TRUE;                          
            }
        }    
        break;    
        
        case xgnPORT_MODEM:
        {
            if ( !lLineDropped )
            {
                SendStringToModem( szStr, nStrLen );
                
                /* Assume it went through OK 
                 * (SendStringToModem doesn't return a value) */
                boIsStringSent = TRUE;                 
            }
        }
        break;
        
        /* Should never get here! */
        default:
        {

        }
        break;        
    }
     
    return ( boIsStringSent );
 
} /* static BOOL Send_String( ... ) */


/*************************************************************
 *
 * Function      : Get_ActivePort
 *
 * Author (Date) : Ian Henderson (14/09/2004)
 *
 * Description   : Determines which port is active (if any)
 *
 * Parameters    : void
 * 
 * Return value  : nPort - identifies active port
 *                  3 = local handset port
 *                  2 = modem port
 *                  0 = no active connection
 *
 * Side effects  : None
 *
 *************************************************************/
int Get_ActivePort( void )
{
    int nPort;
    
    /* If there is a local connection */
    if ( lLH_CONNECTED )
    {
        nPort = xgnPORT_LOCAL_HANDSET;          
    }
    
    /* Else, if there is a remote connection */
    else if ( !lLineDropped )
    {
        nPort = xgnPORT_MODEM;
    }
    
    /* Else there is no connection*/
    else
    {
        nPort = 0; /* No port */ 
    }
    
    return ( nPort );
    
} /* int Get_ActivePort( ... ) */


/*************************************************************
 *
 * Function      : Get_DateTime_MANUFAC
 *
 * Author (Date) : Ian Henderson (04/10/04)
 *
 * Description   : Siemens-specific time/date function that
 *                 fills the given MOVA time/date struct with 
 *                 the current time/date taken from the Siemens
 *                 equipment.
 *
 * Parameters    : pDateTimeCurr - pointer to a MOVA data/time
 *                  struct to be filled with the current 
 *                  date/time info.
 * 
 * Return value  : pDateTimeCurr - return for convenience.
 *
 * Side effects  : None
 *
 *************************************************************/
MovaDateTime * Get_DateTime_MANUFAC( MovaDateTime * pDateTimeCurr )
{
     TIMESTAMP_TYPE tTimeDate;
     extern TIMESTAMP_TYPE Com_read_rtc(int, ... );

     /* Get the current time (Siemens-specific) */
     tTimeDate = Com_read_rtc(READ_RTC);     

     /* Copy the Siemens time/date information to our own
      * (non-manufacturer specific) struct */
     pDateTimeCurr->nDayOfWeek =    tTimeDate.day;     
     pDateTimeCurr->nDayOfMonth =   (int)( tTimeDate.date );
     pDateTimeCurr->nMonth =        (int)( tTimeDate.month );
     pDateTimeCurr->nYear =         (int)( tTimeDate.year );
     pDateTimeCurr->nHours =        (int)( tTimeDate.hours );
     pDateTimeCurr->nMinutes =      (int)( tTimeDate.minutes );
     pDateTimeCurr->nSeconds =      (int)( tTimeDate.seconds );

    return ( pDateTimeCurr );
 
} /* MovaDateTime * Get_DateTime_MANUFAC( ... ) */


/* function to return seconds passed since midnight 1/1/2000 */
long Seconds_after_midnight(void)
{
    long nCurrentSec;
    /* IRH MOD: M5.0.0: 19/07/04: Added local var */
    MovaDateTime tMovaDateTime;    

    Get_DateTime_MANUFAC( &tMovaDateTime );

	/* for the sake of simplicity we're going to assume that all months have 31 days
	   which is less than ideal but far simpler than correctly coding for the number
	   of days in a given month	*/
    nCurrentSec =  tMovaDateTime.nYear * 372 * 24 * 60 * 60; /* calc no of secs in years */
    nCurrentSec += tMovaDateTime.nMonth * 31 * 24 * 60 * 60; /* add no of secs in months  */
    nCurrentSec += tMovaDateTime.nDayOfMonth * 24 * 60 * 60; /* add no of secs in days  */
    nCurrentSec += tMovaDateTime.nHours * 60 * 60;			 /* add no of secs in hours  */
    nCurrentSec += tMovaDateTime.nMinutes * 60;				 /* add no of secs in mins  */
    nCurrentSec += tMovaDateTime.nSeconds;					 /* add remaining secs to total */

    return nCurrentSec;
}    


/*

Function Wait_for_millisecs_MANUFAC(no-of-millisecs)

manufacturer specific routine to wait here for set number of milliseconds

*/

void Wait_for_millisecs_MANUFAC(int iNo_of_millisecs)
{

#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
    PCTasks_Sleep( iNo_of_millisecs, MOVA_TASK_TRLUSERIF );
#else
   int iNo_ticks;
   /* IRH MOD: M5.0.0: 18/08/04: Round to nearest number of ticks */
   iNo_ticks = (int)( ( (float)iNo_of_millisecs / 6.67 ) + 0.5 );
   NU_Sleep(iNo_ticks);
#endif

   return;

}
