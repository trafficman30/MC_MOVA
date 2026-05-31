#if !defined (__PCCLOCK_H)
#define __PCCLOCK_H

/*
    Name:           PCClock.h
    Author (Date):  ihenderson, 23/11/05
    Platform:       PC
    Purpose:        PCClock provides functionality for setting,
                    reading, and updating the current time in
                    MOVA.  The PCClock.h header provides an
                    interface for the PCMOVA modules only; 
                    the MOVA kernel itself must continue to 
                    use the obclock.h header, which provides
                    an appropriate interface to PCClock.c for
                    the MOVA kernel.

    Last revision:  $Revision:: 3                           $
    Last changed:   $Date:: 12/04/06 16:07                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: pcclock.h $
    
    *****************  Version 3  *****************
    User: Ihenderson   Date: 12/04/06   Time: 16:07
    Updated in $/PCMOVA
    
    *****************  Version 2  *****************
    User: Ihenderson   Date: 5/12/05    Time: 11:01
    Updated in $/PCMOVA
    
    *****************  Version 1  *****************
    User: Ihenderson   Date: 28/11/05   Time: 11:49
    Created in $/PCMOVA
*/

#if defined (__cplusplus)
    extern "C" {
#endif

#include "MVTypes.h"

/* Maximum length of date/time string set by PCClock_GetDateTimeStr */
#define xg_DATE_TIME_STR_LEN_MAX    (26)

/* Date/time error string that can be used if PCClock_GetDateTimeStr failed 
(no more than 26 chars) */
#define xg_DATE_TIME_STR_ERROR      ("<could not get date/time>")

typedef struct _DateTime
{
    time_t      secondsSince1970;
    MVInt       millisecs;

} DateTime;

MVStatus    PCClock_Initialise(     void );
MVBool      PCClock_IsInitialised(  void );
MVStatus    PCClock_DeInitialise(   void );
MVStatus    PCClock_SetDateTime(    MVUInt32, MVUInt32 );
void        PCClock_Tick(           MVInt );
MVSize      PCClock_GetDateTimeStr( char *, MVSize );


#if defined (__cplusplus)
    }
#endif

#endif /* __PCCLOCK_H */