#if !defined (__SERIAL_H)
#define __SERIAL_H

/*
    Name:           Serial.h
    Author (Date):  ihenderson, 15/11/05
    Platform:       PC
    Purpose:        Provides serial connection functionality
                    for connecting between PCMOVA and MOVA Comm.
                    This module is generic though - 
                    it could be used by other programs
                    e.g. MOVA Sim.

*/

/* TODO: comment in if linking with Fortran code. */
/*#define FORTRAN*/

#if defined (__cplusplus) || defined (FORTRAN)
extern "C" 
{
#endif

#include "MVTypes.h"
#include "cmstypes.h"
#include "MVComms.h"


MVCOMMS_API MVStatus    Serial_Initialise(      const char *, const char * );
MVCOMMS_API MVStatus    Serial_DeInitialise(    void );
MVCOMMS_API MVBool      Serial_IsInitialised(   void );

MVCOMMS_API MVStatus    Serial_SetTimeouts(     ConnectionID, MVUInt32, MVUInt32,
                                                MVUInt32, MVUInt32 );
MVCOMMS_API MVStatus    Serial_OpenPort(        ConnectionID, MVByte, MVUInt32, MVByte, 
                                                MVByte, MVByte );
MVCOMMS_API MVBool      Serial_IsOpen(          ConnectionID );
MVCOMMS_API MVStatus    Serial_ClosePort(       ConnectionID );
MVCOMMS_API MVInt       Serial_GetPortCount(    void );

MVCOMMS_API MVStatus    Serial_ReadByte(        ConnectionID, MVByte * );
MVCOMMS_API MVStatus    Serial_WriteByte(       ConnectionID, MVByte );
MVCOMMS_API MVStatus    Serial_ReadBytes(       ConnectionID, MVByte *, MVInt, MVInt * );
MVCOMMS_API MVStatus    Serial_WriteBytes(      ConnectionID, const MVByte *, MVInt, MVInt * );

MVCOMMS_API MVStatus    Serial_ClearError(      ConnectionID );
//MVCOMMS_API MVInt	    Serial_GetLastError(    void );


#if defined (__cplusplus) || defined (FORTRAN)
}
#endif

#endif /* __SERIAL_H */
