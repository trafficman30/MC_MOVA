#if !defined (__CLICOMMS_H)
#define __CLICOMMS_H

/*
    Name:           CliComms.h
    Author (Date):  ihenderson, 14/11/05
    Platform:       PC
    Purpose:        Wrapper for MOVA Comm communications
					to MOVA, suitable for calling from
					Fortran code.

*/


#if defined (__cplusplus) || defined (_FORTRAN)
extern "C" 
{
#endif

#include "MVTypes.h"

/*  See CliComms.c for function descriptions 
    We use longs as Fortran doesn't have an 
    unsigned byte type (for Read/WriteByte) */

MVCOMMS_API MVInt32	ClientComms_OpenSerial(	        MVInt32 n32CommPortNum, 
                                                    MVInt32 n32BaudRate, 
                                                    MVInt32 n32ByteSize, 
                                                    MVInt32 n32Parity,
                                                    MVInt32 n32StopBits,
                                                    MVInt32 n32TimeOut );

MVCOMMS_API MVInt32	ClientComms_OpenTCP(	        const char * ipEndPoint, 
                                                    MVInt32 n32TimeOut );

MVCOMMS_API MVInt32 ClientComms_WriteByte(          MVInt32 );
MVCOMMS_API MVInt32 ClientComms_ReadByte(           MVInt32 * );

MVCOMMS_API MVInt32 ClientComms_ClearError(         void );

MVCOMMS_API MVInt32 ClientComms_GetErrorMessage(    char * message, 
                                                    MVInt32 messageLenMax );
MVCOMMS_API MVInt32 ClientComms_Close(		        void );


#if defined (__cplusplus) || defined (_FORTRAN)
}
#endif

#endif /* __CLICOMMS_H */
