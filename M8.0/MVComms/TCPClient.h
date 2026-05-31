#if !defined (__TCPCLIENT_H)
#define __TCPCLIENT_H

/*
    Name:           TCPClient.h
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Provides TCP connection functionality
                    which is used in PCMOVA for communicating 
                    between PCMOVA and a Controller.  
                    This module is generic though - 
                    it could be used by other programs
                    e.g. MOVA Comm and MOVA Sim.

*/


#if defined (__cplusplus) || defined (FORTRAN)
extern "C" 
{
#endif

#include "MVTypes.h"
#include "cmstypes.h"
#include "MVComms.h"


MVCOMMS_API MVStatus    TCPClient_Initialise(       const char *, const char * );
MVCOMMS_API MVStatus    TCPClient_DeInitialise(     void );
MVCOMMS_API MVBool      TCPClient_IsInitialised(    void );

MVCOMMS_API MVStatus    TCPClient_SetTimeouts(      ConnectionID, MVInt, MVInt );
MVCOMMS_API MVStatus    TCPClient_Connect(          ConnectionID, const char *, MVBool );
MVCOMMS_API MVBool      TCPClient_IsConnected(      ConnectionID );
MVCOMMS_API MVStatus    TCPClient_Disconnect(       ConnectionID );
MVCOMMS_API MVInt       TCPClient_GetConnectionCount(  void );

MVCOMMS_API MVStatus	TCPClient_ReadByte(         ConnectionID, MVByte * );
MVCOMMS_API MVStatus	TCPClient_WriteByte(        ConnectionID, MVByte );
MVCOMMS_API MVStatus	TCPClient_ReadBytes(        ConnectionID, MVByte *, MVInt, MVInt * );
MVCOMMS_API MVStatus	TCPClient_WriteBytes(       ConnectionID, const MVByte *, MVInt, MVInt * );


#if defined (__cplusplus) || defined (FORTRAN)
}
#endif

#endif /* __TCPCLIENT_H */
