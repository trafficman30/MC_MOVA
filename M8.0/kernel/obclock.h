/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         obclock.h
*
*  TITLE:        MOVA Kernel: Data structures for MOVA time
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	26-Jan-1988		4.0.0		..		MC		SIE_00			First undergoing system test
*	14-Jan-2000		4.0.0		..		PC		SIE_01			Added 'day' to TIMESTAMP_TYPE.
*	07-Mar-2005		5.1.0		..		RD		SIE_02			Added Cruise Speed data collection structure
*	31-May-2011		7.0.0		AA		MC		CRN03			Added function to return time as time_t for new M7 logs
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
#include <time.h>

#define READ_RTC   1
#define SET_RTC    2

typedef struct {
   /* dd/mm/yy and hh/mm/ss were always in this structure */
   unsigned long date;
   unsigned long month;
   unsigned long year;
   unsigned long hours;
   unsigned long minutes;
   unsigned long seconds;
   /* added day of week because it can be read from the RTC rather than
   |  computed using day_from_date() */
   int day;
} TIMESTAMP_TYPE;

extern TIMESTAMP_TYPE Com_read_rtc(int lMode, ... );

/* use this routine to calculate the day of week from the date */
/* extern int day_from_date(TIMESTAMP_TYPE *tTimeDate); */
/* can now pick out the day of week immediately */
#define day_from_date(tTimeDate) tTimeDate.day

typedef struct {
   /* This structure will hold all the relevant information *
    * For calculating the cruise speeds time-stamps         */
   unsigned long Hours;
   unsigned long Minutes;
   unsigned long Seconds;
   unsigned long Duration;
   unsigned long Date;
   unsigned long Month;
   unsigned long Year;
   int State;
   int ActualLane;
   int EffectiveLane;
   int Detector;
   int DetType;
} CruiseSpeed;


/* Function to be called from the MOVA kernel to read the current time as the 
   number of seconds since 00:00:00 on 1 Jan 1970 (i.e. the standard time_t).
   Note: this should always return the same time as Com_read_rtc(), just in
   a different format. */
extern time_t Com_read_rtc_timet();
