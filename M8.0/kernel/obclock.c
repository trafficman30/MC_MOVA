/*
Module name . . . . . . . . . . . . . . . obclock.c
Module version  . . . . . . . . . . . . . SIE_03
Last modification date  . . . . . . . . . 29/09/04
MOVA version at which this 
         module was last modified . . . . M5.0.0
Authority . . . . . . . . . . . . . . . . TRL Ltd

          Last change:  IRH   29 Sep 04    2:20 pm

------------ Note on version numbering ------------ 

Module version 

    Current version number for this module. Module versions
    are independent of the overall MOVA version number.

MOVA version at which this module was last modified

    The MOVA version at which the last modification to this 
    module was made.  This may be older than the currently 
    compatible MOVA version if no changes to this module
    were made as part of the current version.
*/
/*
______________________________________________________________________

(c) Copyright TRL Ltd 2012. Rights to and ownership of the software
remain unaffected by changes to the software, whether by addition,
removal or modification. Agreement has been reached between Siemens
Traffic Controls Ltd and TRL Ltd whereby Siemens are authorised to use
and modify the MOVA code. TRL Ltd cannot be held responsible for
improper use. 
______________________________________________________________________

;*                         MODIFICATION RECORD
;*                         -------------------
;*
;* SOURCE
;* IDENT.   DATE        NAME               REASON FOR CHANGE
;* -----    ----        ----               -----------------

   SIE_00   26/1/98     MRC                First undergoing system test
   SIE_01   4/3/98      MRC                Initial date now 1/1/97 (was 1/1/96)
   
Changes for combined OMU and MOVA:-
   SIE_02   12/1/00     PC               - Modified Com_read_rtc() so it just
                                           calls the standard OMU routines
                                           com_read_clock() and com_set_clock().
                                         - Moved day_from_date() from MOVADATA.C
                                           to OBCLOCK.C
                                         - Modified day_from_date() so that its
                                           parameter is the whole TIMESTAMP_TYPE
                                           from which it can easily extract the
                                           day of week which it contains.

Changes for MOVA 5.0 (includes Compact MOVA):-
   SIE_03  29/09/04     IRH     Undef TRUE/FALSE if already defined.
*/

#include <nucleus.h>
#include <proj_def.h>
#include <stdarg.h>
#include "obclock.h"
#include <diff_bss.h>

/* Module Defines */

/* IRH MOD: M5.0.0: 30/09/04: Undef TRUE/FALSE if already defined */
#ifdef TRUE 
    #undef TRUE 
#endif 

#ifdef FALSE
    #undef FALSE
#endif 

#define RTC_OK 0
#define TRUE 0xFF
#define FALSE 0

/* Global Variables */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *       Function :  Com_read_clock()

 *
 *       Description : Reads the real-time clock and puts the timestamp
 *                     into the TIMESTAMP parameter
 *
 *       Input parameters  : new_timestamp_ptr
 *       Output parameters : sts
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Prototype */
TIMESTAMP_TYPE Com_read_rtc(int lMode, ... );

TIMESTAMP_TYPE Com_read_rtc(int lMode, ... )
{
   va_list ap;
   unsigned char sts = !RTC_OK;
   TIMESTAMP_TYPE time, new_time, *new_timestamp_ptr; /* MOVA time fields */
   TIME_TYPE omu_time; /* OMU system time fields */
   extern char lIsClockSet;
   
   va_start(ap, lMode);
   
   new_timestamp_ptr = &new_time; /* new_timestamp_ptr points to new_time */

      /* on first pass */
      if ( !lIsClockSet ) /* RT option in USER or cold start */
      {
#if 0
          /* on the original MOVA unit, it used to initialise the clock to
          |  12:12:12 1/1/97 if this flag was not set */
          new_timestamp_ptr->seconds = 12;
          new_timestamp_ptr->minutes = 12;
          new_timestamp_ptr->hours   = 12;
          new_timestamp_ptr->date    = 1;
          new_timestamp_ptr->month   = 1;
          new_timestamp_ptr->year    = 97;
          lMode = SET_RTC;
#else
          /* On a combined MOVA/OMU, the clock was initialised to 00:00:00 1/1/90
          |  by NUC_IF if the first time power-up code is wrong and may have been
          |  set-up by the OMU applications. Therefore just set the flag to say
          |  the clock has been set-up. */
          lIsClockSet = TRUE;
#endif
      }
          
      /* the mode asks to set the clock */
      if ( lMode == SET_RTC )
      {
            if ( !lIsClockSet )
                lIsClockSet = TRUE;
            else
                new_time = va_arg(ap, TIMESTAMP_TYPE );

            /* read all the parts of the current time first so that anything not
            |  set is left unchanged */
            sts = com_read_clock(&omu_time);
            if ( sts == RTC_OK )
            {
                /* change those values given */
                omu_time.seconds = (utiny)new_timestamp_ptr->seconds;
                omu_time.minutes = (utiny)new_timestamp_ptr->minutes;
                omu_time.hours   = (utiny)new_timestamp_ptr->hours;
                omu_time.date    = (utiny)new_timestamp_ptr->date;
                omu_time.month   = (utiny)new_timestamp_ptr->month;
                omu_time.year    = (utiny)new_timestamp_ptr->year;
                /* Day Of Week:  S S M T W T F S S */
                /* MOVA Numbers: 6 7 1 2 3 4 5 6 7 */
                /* OMU Numbers:  0 1 2 3 4 5 6 0 1 */
                omu_time.day     = (utiny)(new_timestamp_ptr->day >= 6 ?
                                           new_timestamp_ptr->day - 6 :
                                           new_timestamp_ptr->day + 1 );
                /* attempt to set the clock */
                sts = com_set_clock(&omu_time);
            }
      }

    /* regardless of what was asked, always read the clock so the structure
    |  returned is correct */   
    /* Read the time using the OMU function */
    sts = com_read_clock(&omu_time);
    time.seconds = omu_time.seconds;
    time.minutes = omu_time.minutes;
    time.hours = omu_time.hours;
    time.date  = omu_time.date;
    time.month = omu_time.month;
    time.year  = omu_time.year;
    /* Day Of Week:  S S M T W T F S S */
    /* OMU Numbers:  0 1 2 3 4 5 6 0 1 */
    /* MOVA Numbers: 6 7 1 2 3 4 5 6 7 */
    time.day   = ( omu_time.day <= 1 ? omu_time.day+6 : omu_time.day-1 );

   if ( sts != RTC_OK )
   {
      time.seconds = 0;
      time.minutes = 0;
      time.hours   = 0;
      time.date    = 0;
      time.month   = 0;
      time.year    = 0;
   }
   va_end(ap); /* To clean stack after variable length parameter list */
   return (time);
}


/*******************************************************************************
* day_from_date()                                                              *
* Calculates the day of week (1-7) from the given date.                        *
* Based around TRL's original code which was 'in-line' where required rather   *
* than in a separate function.                                                 *
*******************************************************************************/
/* old function before OMU/MOVA combined with RTC chip holding the day */
/* now a implemented as a macro which is defined in obclock.h */
#if 0
int day_from_date(long dd, long mm, long yy)
{
    /* 'cal' is used in working out the day from the date */
    /*                     J  F  M  A  M  J  J  A  S  O  N  D  */
    /*                    31 28 31 30 31 30 31 31 30 31 30 31  */
    /*                     3  0  3  2  3  2  3  3  2  3  2  3  */
    const char cal[12] = { 0, 3, 3, 6, 8,11,13,16,19,21,24,26 };
    /* TRL original code offset cal[] by two days...                          */
    /*    char cal[12] = { 2, 5, 5, 1, 3, 6, 1, 4, 0, 2, 5, 0 };              */
    /* and then calculated the day of week as follows...                      */
    /* dy = dd+(long)cal[mm-1];                                               */
    /* if ( yy < 97 ) yy += 100;                                              */
    /* if ( yy % 4L == 0 && mm >= 3) dy++;                                    */
    /* dy = (yy-52L)*365L+(yy-1L)/4L+dy;                                      */
    /* dy = (dy % 7L) +1L;                                                    */

    int day;

    /* calculate the days this year (less any multiples of 7) */
    day = dd + cal[mm-1];
    /* adjust two digit year for 2000 */
    if ( yy < 97 ) yy += 100;
    /* add one day for each year (as 1 year = 52 weeks + 1 day) and correctly
    |  offset the number of days. TRL original code added (y-52)*365 which
    |  effectively added 1 day per year and subtracted 3 (52 = 49+3), but since
    |  TRL's cal[] added 2, we only actually subtract 1. */
    day = day + yy - 1;
    /* add one day for every leap year which has already passed */
    day = day + ( yy - 1) / 4;
    /* if this year was also a leap year, then add one extra day */
    if ( (yy % 4) == 0 && ( mm >= 3 ) ) day++;
    /* now remove the whole number of weeks (to give days 0-6) */
    day = day % 7;
    /* and convert to give the range 1 to 7 */
    day = day + 1;
    /* return the result */
    return(day);
}
#endif


/*******************************************************************************
* Com_read_rtc_timet()                                                         *
* Returns the current RTC time as a time_t rather than TIMESTAMP_TYPE          *
*                                                                              *
* This is an example implementation only and should be optimised by			   *
* signal companies															   *
*******************************************************************************/
time_t Com_read_rtc_timet()
{
	return time(NULL);
}
