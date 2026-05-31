/*
    Name:           PCClock.c
    Author (Date):  ihenderson, 23/11/05
    Platform:       PC
    Purpose:        PCClock provides functionality for setting,
                    reading, and updating the current time in
                    PCMOVA.  The clock is read and set in the
                    normal way (using Com_read_rtc) by the MOVA
                    kernel, which includes "obclock.h" to access
                    this functionality as normal.  The clock is 
                    also initialised, set and updated by PCKernel
                    (on startup) and PCIOScan (when initialise
                    and update messages are received from 
                    the PCMOVA Controller).

                    Note that the clock does not carry out daylight
                    saving changes - this is not considered desirable
                    when running MOVA in simulation.

                    Synchronisation Note:
                    Access to the clock is synchronised as it can be 
                    get/set by the MOVA kernel TRLUserIF/term threads 
                    as well as the main thread. 

    Last revision:  $Revision:: 6                           $
    Last changed:   $Date:: 19/05/06 13:28                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: pcclock.c $
 * 
 * *****************  Version 6  *****************
 * User: Ihenderson   Date: 19/05/06   Time: 13:28
 * Updated in $/PCMOVA
    
    *****************  Version 5  *****************
    User: Ihenderson   Date: 12/04/06   Time: 16:07
    Updated in $/PCMOVA
    
    *****************  Version 4  *****************
    User: Abooth       Date: 12/01/06   Time: 11:05
    Updated in $/PCMOVA
    
    *****************  Version 3  *****************
    User: Ihenderson   Date: 16/12/05   Time: 10:14
    Updated in $/PCMOVA
    
    *****************  Version 2  *****************
    User: Ihenderson   Date: 5/12/05    Time: 11:01
    Updated in $/PCMOVA
    
    *****************  Version 1  *****************
    User: Ihenderson   Date: 28/11/05   Time: 11:49
    Created in $/PCMOVA
*/

/*
    TODO:
*/

#include "stdafx.h"

#include <stdarg.h>
#include <time.h>
#include "pcclock.h"        /* PCMOVA interface to PCClock */
#include "obclock.h"        /* Kernel interface to PCClock */


/* Module Defines */
#define g_MODULE_NAME   ("PCClock")




/* Global Variables */
static MVBool       g_isInitialised = MV_FALSE;
static DateTime     g_dateTime;


/* Synchronise access to the clock - can be get/set by 
the MOVA kernel TRLUserIF/term threads as well as the main thread. */
static CRITICAL_SECTION    g_criticalSection;

static MVStatus PCClock_StoreDateTime( struct tm *, MVInt );
static void PCClock_RetrieveDateTime( struct tm * );


/*
    Name:           PCClock_Initialise
    Author (Date):  Ian Henderson (23/11/05)
    Platform:       PC
    Purpose:        Initialises the PCClock by setting it to the current
                    system time.
    Inputs:         
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Remarks:        
*/
MVStatus PCClock_Initialise( void )
{
    MVStatus status = MV_SUCCESS;

    TRACE_ASSERT( !g_isInitialised, "Clock already initialised." );

    /* Initialise the critical section used for synchronising clock access */
    InitializeCriticalSection( &g_criticalSection );

    TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Initialising clock..." );
       
    /* Get current system time in seconds */
    time( &( g_dateTime.secondsSince1970 ) );
    g_dateTime.millisecs = 0;
   
    TRACE_WRITE2_IF( LEVEL_INFO, g_MODULE_NAME, 
        "Initialising date and time to: (+%dms) %s", 
        g_dateTime.millisecs,
        asctime( localtime( &( g_dateTime.secondsSince1970 ) ) ) );

    g_isInitialised = MV_TRUE;

    TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Clock initialised." );

    return ( status );
    
} /* PCClock_Initialise */


/*
    Name:           PCClock_IsInitialised
    Author (Date):  Ian Henderson (23/11/05)
    Platform:       PC
    Purpose:        Whether the PCClock is initialised
    Inputs:         
    Returns:        True if initialised; False otherwise
    Remarks:        
*/
MVBool  PCClock_IsInitialised( void )
{
    return ( g_isInitialised );
    
} /* PCClock_IsInitialised */


/*
    Name:           PCClock_DeInitialise
    Author (Date):  Ian Henderson (23/11/05)
    Platform:       PC
    Purpose:        Deinitialises the PCClock.
    Inputs:         
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Remarks:        
*/
MVStatus PCClock_DeInitialise( void )
{   
    MVStatus status = MV_SUCCESS;

    if ( g_isInitialised )
    {
        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
            "Deinitialising clock..." );

        /* Delete the critical section used for synchronising clock access */
        DeleteCriticalSection( &g_criticalSection );

        g_isInitialised = MV_FALSE;
            
        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
            "Clock deinitialised." );
    }

    return ( status );
    
} /* PCClock_DeInitialise */


/*
    Name:           PCClock_SetDateTime
    Author (Date):  Ian Henderson (23/11/05)
    Platform:       PC
    Purpose:        Sets the PCClock to the given date and time
                    as sent from the PCMOVAController.
    Inputs:         Date as a 32-bit integer in the format 0xDDMMYYYY
                    Time as a 32-bit integer in the format 0xHHMMSSUU
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Remarks:        
*/
MVStatus PCClock_SetDateTime( MVUInt32 date, MVUInt32 time )
{
    #define DAY_OF_MONTH( date )    ( (MVInt)( ( date >> 24 ) & 0xFF ) )
    #define MONTH( date )           ( (MVInt)( ( date >> 16 ) & 0xFF ) )
    #define YEAR_MC( date )         ( (MVInt)( date & 0xFF ) )
    #define YEAR_XI( date )         ( (MVInt)( ( date >> 8 ) & 0xFF ) )
    #define YEAR( date )            ( YEAR_MC( date ) * 100 + YEAR_XI( date ) )
    #define HOUR( time )            ( (MVInt)( ( time >> 24 ) & 0xFF ) )
    #define MINUTES( time )         ( (MVInt)( ( time >> 16 ) & 0xFF ) )
    #define SECONDS( time )         ( (MVInt)( ( time >> 8 ) & 0xFF ) )
    #define HUNDREDTHS( time )      ( (MVInt)( time & 0xFF ) )

    MVStatus    status = MV_SUCCESS;
    struct tm   dateTime;

    TRACE_ASSERT( g_isInitialised, "Clock not initialised." );
    
    TRACE_WRITE7_IF( LEVEL_INFO, g_MODULE_NAME, 
        "Controller setting MOVA clock to: %02d/%02d/%04d %02d:%02d:%02d.%02d.",
        DAY_OF_MONTH( date ), MONTH( date ), YEAR( date ),
        HOUR( time ), MINUTES( time ), SECONDS( time ), HUNDREDTHS( time ) );

    dateTime.tm_mday =  DAY_OF_MONTH( date );   // Day of month (1–31)
    dateTime.tm_mon =   MONTH( date ) - 1;      // Month (0–11; January = 0)
    dateTime.tm_year =  YEAR( date ) - 1900;    // Year (current year minus 1900)

    dateTime.tm_hour =  HOUR( time );           // Hours since midnight (0–23)
    dateTime.tm_min =   MINUTES( time );        // Minutes after hour (0–59)
    dateTime.tm_sec =   SECONDS( time );        // Seconds after minute (0–59)    

    status = PCClock_StoreDateTime( &dateTime, HUNDREDTHS( time ) );
    if ( status != MV_SUCCESS )
    {
        TRACE_WRITE2_IF( LEVEL_ERROR, g_MODULE_NAME, 
            "Error setting date and time (date = 0x%08X, time = 0x%08X).", 
            date, time );
    }                

    return ( status );
    
} /* PCClock_SetDateTime */


/*
    Name:           PCClock_StoreDateTime
    Author (Date):  Ian Henderson (23/11/05)
    Platform:       PC
    Purpose:        Copies the date and time to the internal
                    time data used by the PCClock.
    Inputs:         Date/time
                    Hundredths of seconds.   
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Remarks:        Access to the PCClock is synchronised - it's possible
                    for Com_read_rtc() and PCClock_SetDateTime() to call
                    this function at the same time (Com_read_rtc() from
                    the TRLUserIF thread, and PCClock_SetDateTime() from
                    the main PCMOVA thread.).
*/
static MVStatus PCClock_StoreDateTime( struct tm * p_dateTime, 
                                      MVInt hundredths )
{
    MVStatus    status = MV_SUCCESS;
    time_t      dateTimeSecs;
    MVInt       dateTimeMillisecs;

    TRACE_ASSERT( g_isInitialised, "Clock not initialised." );
        
    // Daylight saving time not in effect
    // The PCMOVA kernel doesn't support BST - considered confusing
    // when running MOVA in simulation.
    p_dateTime->tm_isdst = 0;
    
    // Convert the time to time in seconds since midnight, January 1, 1970
    // N.B. Can only convert times up to 3:14:07 January 19, 2038
    dateTimeSecs = mktime( p_dateTime );
    dateTimeMillisecs = hundredths * 10;

    if ( dateTimeSecs == -1 || dateTimeMillisecs >= 1000 )
    {
        PCError_SetLast( PCMOVA_ERROR_TIME, NULL, g_MODULE_NAME );
        status = MV_FAILURE;
    }
    else
    {
        /* START CRITICAL SECTION */
        EnterCriticalSection( &g_criticalSection );

        g_dateTime.secondsSince1970 = dateTimeSecs;
        g_dateTime.millisecs = dateTimeMillisecs;

        /* END CRITICAL SECTION */
        LeaveCriticalSection( &g_criticalSection );

        TRACE_WRITE2_IF( LEVEL_INFO, g_MODULE_NAME, 
            "MOVA clock set to: (+%dms) %s", 
            g_dateTime.millisecs, asctime( p_dateTime ) );
    }

    return ( status );
    
} /* PCClock_StoreDateTime */


/*
    Name:           PCClock_RetrieveDateTime
    Author (Date):  Ian Henderson (23/11/05)
    Platform:       PC
    Purpose:        Gets the date and time as an easy-to-interrogate struct.
    Inputs:         Struct to fill with date and time
    Returns:        
    Remarks:        
*/
static void PCClock_RetrieveDateTime( struct tm * p_dateTime )
{
    struct tm * p_dateTimeNow;

    TRACE_ASSERT( g_isInitialised, "Clock not initialised." );

    p_dateTimeNow = gmtime( &( g_dateTime.secondsSince1970 ) );

    (*p_dateTime) = (*p_dateTimeNow);
    
} /* PCClock_RetrieveDateTime */


/*
    Name:           PCClock_Tick
    Author (Date):  Ian Henderson (23/11/05)
    Platform:       PC
    Purpose:        Advances the PCClock by the given number of
                    milliseconds.  After 3:14:07 January 19, 2038
                    the clock will wrap around to 00:00:00, January 1, 1970.
    Inputs:         Struct to fill with date and time
    Returns:        
    Remarks:        Access to the PCClock is synchronised - it's possible
                    for the main PCMOVA thread to advance the clock
                    at the same time as it's being set by the user
                    using Com_read_rtc().
*/
void PCClock_Tick( MVInt milliseconds )
{
    /* Get number of seconds and milliseconds */
    MVInt   millisecsNew = milliseconds % 1000;
    MVInt   secsNew = milliseconds / 1000;

    TRACE_ASSERT( g_isInitialised, "Clock not initialised." );    

    /* START CRITICAL SECTION */
    EnterCriticalSection( &g_criticalSection );

    /* Update the current time */
    g_dateTime.secondsSince1970 += secsNew;
    g_dateTime.millisecs += millisecsNew;

    if ( g_dateTime.millisecs >= 1000 )
    {
        g_dateTime.millisecs -= 1000;
        g_dateTime.secondsSince1970++;
    }

    /* END CRITICAL SECTION */
    LeaveCriticalSection( &g_criticalSection );

} /* PCClock_Tick */


/*
    Name:           PCClock_GetDateTimeStr
    Author (Date):  Ian Henderson (23/11/05)
    Platform:       PC
    Purpose:        Gets the PCClock date and time as a human-readable 
                    null-terminated string.
    Inputs:         Character buffer to write the date and time to
                    Length of character buffer (must be >26 chars)
    Returns:        The length of the date and time string in the buffer;
                    zero on failure.
    Remarks:        
*/
MVSize PCClock_GetDateTimeStr( char * dateTimeBuf, MVSize dateTimeBufLen )
{   
    struct tm * timeNow;
    MVSize  dateTimeLen = 0;

    /* Date/time strings just for user output, 
    so don't assert in release builds if there's an error. */
    DEBUG_ASSERT( g_isInitialised );    
    if ( !g_isInitialised )
    {
        TRACE_WRITE0_IF( LEVEL_ERROR, g_MODULE_NAME,
            "PCClock_GetDateTimeStr called but module not initialised." );
        return 0;
    }

    DEBUG_ASSERT_VALID_ADDRESS( dateTimeBuf );
    DEBUG_ASSERT( dateTimeBufLen > xg_DATE_TIME_STR_LEN_MAX );
    if ( dateTimeBuf == NULL || dateTimeBufLen <= xg_DATE_TIME_STR_LEN_MAX )
    {
        TRACE_WRITE0_IF( LEVEL_ERROR, g_MODULE_NAME,
            "PCClock_GetDateTimeStr passed an invalid buffer." );
        return 0;
    }

    /* Convert PCClock time to a struct */
    timeNow = localtime( &( g_dateTime.secondsSince1970 ) );

    /* Print local time as a string (exactly 26 chars length) */
    sprintf( dateTimeBuf, "%s", asctime( timeNow ) );

    dateTimeLen = strlen( dateTimeBuf );

    /* Remove the new line from the end of the date/time string */
    dateTimeBuf[ dateTimeLen - 1 ] = '\0';
    dateTimeLen--;

    return ( dateTimeLen );

} /* PCClock_GetDateTimeStr */


/*
    Name:           Com_read_rtc
    Author (Date):  Ian Henderson (23/11/05)
    Platform:       PC
    Purpose:        Gets or sets the PCClock, called from
                    inside the MOVA kernel.
    Inputs:         The "mode" - whether the clock is being 
                    read or set (READ_RTC/SET_RTC).
                    If the mode is SET_RTC, the second parameter
                    is the date/time (TIMESTAMP_TYPE) to set the PCClock to.
    Returns:        The current date/time.  In the event of an error 
                    setting the clock, the time won't be changed.
    Remarks:        PCClock implementation doesn't (need to) use the 
                    lIsClockSet flag.
*/
TIMESTAMP_TYPE Com_read_rtc( int lMode, ... )
{
    va_list          argList;
    TIMESTAMP_TYPE   movaDateTime;
    struct tm        dateTime;

    TRACE_ASSERT( g_isInitialised, "Clock not initialised." );
   
    va_start( argList, lMode );   

    /* the mode asks to set the clock */
    if ( lMode == SET_RTC )
    {
        PCClock_RetrieveDateTime( &dateTime );

        // Get the new time to set
        movaDateTime = va_arg( argList, TIMESTAMP_TYPE );

        TRACE_WRITE7_IF( LEVEL_INFO, g_MODULE_NAME, 
            "Kernel setting MOVA clock to: %02d/%02d/%02d %02d:%02d:%02d.%02d.",
            movaDateTime.date, movaDateTime.month, movaDateTime.year,
            movaDateTime.hours, movaDateTime.minutes, movaDateTime.seconds, 0 );
        
        dateTime.tm_mday =  movaDateTime.date;       // Day of month (1–31)
        dateTime.tm_mon =   movaDateTime.month - 1;  // Month (0–11; January = 0)
        
        // Year (current year minus 1900). MOVA year is year minus 2000.
        dateTime.tm_year =  ( movaDateTime.year + 100 );                          
                                                                                    
        dateTime.tm_hour =  movaDateTime.hours;      // Hours since midnight (0–23)
        dateTime.tm_min =   movaDateTime.minutes;    // Minutes after hour (0–59)
        dateTime.tm_sec =   movaDateTime.seconds;    // Seconds after minute (0–59)
                                                    
        // N.B. If this function fails, MOVA won't know about it - 
        // Com_read_rtc doesn't return/set an error value
        PCClock_StoreDateTime( &dateTime, 0 );
    }

    /*  regardless of what was asked, always read the clock so the structure
        returned is correct */   
    PCClock_RetrieveDateTime( &dateTime );

    /* Day Of Week:     S S M T W T F */
    /* MOVA Numbers:    6 7 1 2 3 4 5 */
    /* PCClock Numbers: 6 0 1 2 3 4 5 */
    movaDateTime.day =     ( dateTime.tm_wday == 0 ? 7 : dateTime.tm_wday );

    movaDateTime.date =    dateTime.tm_mday;       // Day of month (1–31)
    movaDateTime.month =   dateTime.tm_mon + 1;    // Month (1–12; January = 1)

    // Year (current year minus 2000). PCClock year is year minus 1900.
    movaDateTime.year =    dateTime.tm_year - 100;
    
    movaDateTime.hours =   dateTime.tm_hour;       // Hours since midnight (0–23)
    movaDateTime.minutes = dateTime.tm_min;        // Minutes after hour (0–59)
    movaDateTime.seconds = dateTime.tm_sec;        // Seconds after minute (0–59)
    
    va_end( argList ); /* To clean stack after variable length parameter list */

    return ( movaDateTime );
    
} /* Com_read_rtc */


#if 0
// Not needed... mktime() calculates day of year and day of week for us.
static void PCClock_AddDayOfYear( struct tm * p_dateTime )
{
    /*                                 J   F   M   A   M   J   J   A   S   O   N   D  */
    static const MVInt calendar[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };    
    MVInt monthIndex;
    MVInt dayOfYear;

    TRACE_ASSERT( g_isInitialised, "Clock not initialised." );
    TRACE_ASSERT( p_dateTime->tm_year >= 0, "Invalid year since 1900." );
    TRACE_ASSERT( MATH_IS_IN_RANGE( p_dateTime->tm_mon, 0, 11 ),
        "Invalid month of year." );
    TRACE_ASSERT( MATH_IS_IN_RANGE( p_dateTime->tm_mday, 1, 31 ), 
        "Invalid day of month." );

    dayOfYear = p_dateTime->tm_mday;

    for ( monthIndex = 0; monthIndex != p_dateTime->tm_mon; ++monthIndex )
    {
        dayOfYear += calendar[ monthIndex ];
    }

    // If after February, add an extra day if this is a leap year
    if ( p_dateTime->tm_mon > 1 )
    {
        MVInt   yearFull = 1900 + p_dateTime->tm_year;

        // 1. Every year divisible by 4 is a leap year.
        // 2. But every year divisible by 100 is NOT a leap year
        // 3. Unless the year is also divisible by 400, then it is still a leap year.
        MVBool  isYearFourth = ( yearFull % 4 == 0 ? MV_TRUE : MV_FALSE );
        MVBool  isCentury = ( yearFull % 100 == 0 ? MV_TRUE : MV_FALSE );
        MVBool  isCenturyFourth = ( yearFull % 400 == 0 ? MV_TRUE : MV_FALSE );
        
        if ( ( isYearFourth && !isCentury ) || isCenturyFourth )
        {
            dayOfYear++;
        }
    }

    p_dateTime->tm_yday = dayOfYear - 1;
    TRACE_ASSERT( MATH_IS_IN_RANGE( p_dateTime->tm_yday, 0, 365 ), 
        "Invalid day of year." );
}
#endif

/* Additional time function to return time_t format rather than TIMESTAMP_TYPE */
time_t Com_read_rtc_timet()
{
    struct tm		*dateTm;
	time_t			timeSecs;

	dateTm = gmtime( &( g_dateTime.secondsSince1970 ) );

	/*-1 forces mktime to find out if daylight saving is on/off*/
	dateTm->tm_isdst = -1;

	timeSecs = mktime(dateTm);

	return timeSecs;
}
