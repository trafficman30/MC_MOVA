#if !defined (__PCERROR_H)
#define __PCERROR_H

/*
    Name:           PCError.h
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        PCMOVA error handling - used throughout
                    the PCMOVA application

    Last revision:  $Revision:: 4                           $
    Last changed:   $Date:: 12/04/06 16:07                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: pcerror.h $
    
    *****************  Version 4  *****************
    User: Ihenderson   Date: 12/04/06   Time: 16:07
    Updated in $/PCMOVA
    
    *****************  Version 3  *****************
    User: Ihenderson   Date: 16/12/05   Time: 10:17
    Updated in $/PCMOVA
    
    *****************  Version 2  *****************
    User: Ihenderson   Date: 5/12/05    Time: 11:03
    Updated in $/PCMOVA
    
    *****************  Version 1  *****************
    User: Ihenderson   Date: 28/11/05   Time: 11:49
    Created in $/PCMOVA
*/

/*
    TODO:
*/

#if defined (__cplusplus)
    extern "C" {
#endif

#include "MVTypes.h"
#include "MVError.h"


#define xg_ERROR_MSG_LEN_MAX     (1023)


typedef enum
{
        PCMOVA_ERROR_INVALID = -1,

        PCMOVA_ERROR_FIRST,
    PCMOVA_ERROR_NO_ERROR = PCMOVA_ERROR_FIRST,
    PCMOVA_ERROR_ASSERTION_FAILURE,    /* Should always be 1 = EXIT_FAILURE */
    PCMOVA_ERROR_ALREADY_INIT,
    PCMOVA_ERROR_OUT_OF_MEMORY,
    PCMOVA_ERROR_PARAM_INVALID,
    PCMOVA_ERROR_DATASET_OPEN,
    PCMOVA_ERROR_DATASET_READ,
    PCMOVA_ERROR_DATASET_FORMAT,
    PCMOVA_ERROR_DATASET_CLOSE,   
    PCMOVA_ERROR_TASK_INIT,
    PCMOVA_ERROR_TASK_DEINIT,
    PCMOVA_ERROR_TERMINATE,
    PCMOVA_ERROR_HANDSHAKE,
    PCMOVA_ERROR_INITIALISE,
    PCMOVA_ERROR_UPDATE,
    PCMOVA_ERROR_CONTROLLER_SEND,
    PCMOVA_ERROR_CONTROLLER_RECEIVE,
    PCMOVA_ERROR_USER_SEND,
    PCMOVA_ERROR_USER_RECEIVE,
    PCMOVA_ERROR_MSG_CHECKSUM,
    PCMOVA_ERROR_MSG_LENGTH,
    PCMOVA_ERROR_MSG_START,
    PCMOVA_ERROR_MSG_ID,
    PCMOVA_ERROR_MSG_TYPE,
    PCMOVA_ERROR_MSG_DATA,
    PCMOVA_ERROR_TIME,
    PCMOVA_ERROR_CONTROLLER_INIT,
    PCMOVA_ERROR_CONTROLLER_DEINIT,
    PCMOVA_ERROR_USER_INIT,
    PCMOVA_ERROR_USER_DEINIT,
    PCMOVA_ERROR_NOT_INIT,
    PCMOVA_ERROR_DATASET_VERSION,
    PCMOVA_ERROR_DATASET_CHECKSUM,
    PCMOVA_ERROR_KERNEL_FATAL,
    PCMOVA_ERROR_KERNEL_USER_FATAL,
    PCMOVA_ERROR_TASK_SUSPEND,
    PCMOVA_ERROR_TASK_RESUME,
        PCMOVA_ERROR_LAST = PCMOVA_ERROR_TASK_RESUME,
    
        PCMOVA_ERROR_COUNT

} PCError;


void        PCError_Initialise(         void );
void        PCError_SetLast(            PCError, const char *, const char * );
void        PCError_SetLastWin(         PCError, WinError, 
                                        const char *, const char * );

// TODO: PCMOVA functions called from kernel should be placed in one header only
void        PCError_SetLastKernel(      MVUInt16, MVInt, const char *, MVSize );
void        PCError_ExitKernel(         PCError );            

PCError     PCError_GetLast(            void );
WinError    PCError_GetLastWin(         void );
MVSize      PCError_BuildLastMessage(   char *, MVSize );
MVSize      PCError_BuildMessage(       PCError, WinError, char *, MVSize );


#if defined (__cplusplus)
    }
#endif

#endif /* __PCERROR_H */