#if !defined (__PCTASKS_H)
#define __PCTASKS_H

/*
    Name:           PCTasks.c
    Author (Date):  ihenderson, 24/11/05
    Platform:       PC
    Purpose:        PCTasks provides functionality for running
                    and controlling the MOVA Kernel tasks (threads).
                    Note that remote connections and their associated 
                    threads (answer/phone) are not (yet) supported in PCMOVA.
                    
    Last revision:  $Revision:: 3                           $
    Last changed:   $Date:: 12/04/06 16:07                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: pctasks.h $
    
    *****************  Version 3  *****************
    User: Ihenderson   Date: 12/04/06   Time: 16:07
    Updated in $/PCMOVA
*/

#if defined (__cplusplus)
    extern "C" {
#endif

#include "MVTypes.h"


MVStatus    PCTasks_Initialise(     void );
MVStatus    PCTasks_DeInitialise(   void );
MVBool    PCTasks_IsInitialised(  void );
MVStatus    PCTasks_Start(          void );
MVStatus    PCTasks_Update(         void );

MVBool PCTasks_IsTerminating(void);

#if defined (__cplusplus)
    }
#endif

#endif /* __PCTASKS_H */