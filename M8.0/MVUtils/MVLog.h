#if !defined (__MVLOG_H)
#define __MVLOG_H

/*
    Name:           MVLog.h
    Author (Date):  ihenderson, 13/10/05
    Platform:       PC
    Purpose:        Basic log to file functionality

    Last revision:  $Revision:: 1                           $
    Last changed:   $Date:: 28/11/05 11:53                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: MVLog.h $
    
    *****************  Version 1  *****************
    User: Ihenderson   Date: 28/11/05   Time: 11:53
    Created in $/PCMOVA/MVUtils
*/


/*
	TODO:
*/

#if defined (__cplusplus)
    extern "C" {
#endif


#include "MVTypes.h"


void		Log_Initialise(		    const char *, MVBool );
void		Log_DeInitialise(	    void );
MVBool		Log_IsInitialised(	    void );
void		Log_Write(			    const char * );


#if defined (__cplusplus)
    }
#endif

#endif /* __MVLOG_H */