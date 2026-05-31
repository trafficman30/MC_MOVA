#if !defined (__TCPSERVER_H)
#define __TCPSERVER_H

/*
    Name:           TCPServer.h
    Author (Date):  ihenderson, 14/11/05
    Platform:       PC
    Purpose:        Provides TCP server connection functionality
                    which is used in PCMOVA for communicating 
                    between PCMOVA and MOVA Comm. (Only one
					connection to MOVA is allowed at a time.)
                    This module is generic though - 
                    it could be used by other programs.

*/


#if defined (__cplusplus) || defined (_FORTRAN)
extern "C" 
{
#endif

#include "MVTypes.h"
#include "cmstypes.h"
#include "MVComms.h"

MVCOMMS_API MVStatus    TCPServer_Create( ConnectionID, const char *, MVBool );
MVCOMMS_API MVStatus    TCPServer_Destroy( void );
MVCOMMS_API MVSize      TCPServer_GetEndPoint( char *, MVSize );

#if defined (__cplusplus) || defined (_FORTRAN)
}
#endif

#endif /* __TCPSERVER_H */
