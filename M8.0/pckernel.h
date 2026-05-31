#if !defined (__PCKERNEL_H)
#define __PCKERNEL_H

/*
    Name:           PCKernel
    Author (Date):  ihenderson, 01/10/05
    Platform:       PC
    Purpose:        PCKernel provides top-level functions for
                    initialising, running, and terminating PCMOVA.
                    These are called from PCMOVA.cpp.

    Last revision:  $Revision:: 2                           $
    Last changed:   $Date:: 12/04/06 16:07                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: pckernel.h $
    
    *****************  Version 2  *****************
    User: Ihenderson   Date: 12/04/06   Time: 16:07
    Updated in $/PCMOVA
*/

#if defined (__cplusplus)
    extern "C" {
#endif

#include "MVTypes.h"
#include "pctypes.h"


MVStatus    PCKernel_Initialise(    CommsType, const char *, 
                                    CommsType, const char *, 
                                    MVInt, const char *, const char *);
MVBool      PCKernel_IsInitialised( void );
MVStatus    PCKernel_DeInitialise(  void );

MVStatus    PCKernel_Run(           void );


#if defined (__cplusplus)
    }
#endif

#endif /* __PCKERNEL_H */