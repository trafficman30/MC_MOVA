#if !defined (__PCIOSCAN_H)
#define __PCIOSCAN_H

/*
    Name:           PCIOScan.h
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Provides input and output "ports" which PCMOVA
                    uses to store inputs (detectors and confirms)
                    and outputs (forces).
                    The MOVA kernel sends and retrieves data from
                    the ports using the InputScan and OutputScan functions 
                    (see IOScan.c for the embedded version equivalents).

    Last revision:  $Revision:: 4                           $
    Last changed:   $Date:: 12/04/06 16:07                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: pcioscan.h $
    
    *****************  Version 4  *****************
    User: Ihenderson   Date: 12/04/06   Time: 16:07
    Updated in $/PCMOVA
    
    *****************  Version 3  *****************
    User: Ihenderson   Date: 16/12/05   Time: 10:19
    Updated in $/PCMOVA
    
    *****************  Version 2  *****************
    User: Ihenderson   Date: 5/12/05    Time: 11:04
    Updated in $/PCMOVA
*/

#if defined (__cplusplus)
    extern "C" {
#endif

#include "MVTypes.h"


MVStatus    PCIOScan_Initialise(            void );
MVBool      PCIOScan_IsInitialised(         void );
MVStatus    PCIOScan_DeInitialise(          void );

void        PCIOScan_SetInputs(             const IOPort *, 
                                            const IOPort *, MVBool );
void        PCIOScan_GetOutputs(            IOPort *, MVBool *, IOPort * p_isHoldSignalOn,  IOPort * p_isReleaseSignalOn );

MVBool      PCIOScan_EnableMOVAControl(     MVBool );
MVBool      PCIOScan_IsMOVAControlEnabled(  void );


#if defined (__cplusplus)
    }
#endif

#endif /* __PCIOSCAN_H */