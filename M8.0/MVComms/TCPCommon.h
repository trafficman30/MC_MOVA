#if !defined (__TCPCOMMON_H)
#define __TCPCOMMON_H

/*
    Name:           TCPCommon.h
    Author (Date):  ihenderson, 25/11/05
    Platform:       PC
    Purpose:        Common TCP functionality used by
                    TCPClient and TCPServer modules.

*/

#if defined (__cplusplus) || defined (FORTRAN)
extern "C" 
{
#endif

#include "MVTypes.h"

#define xg_IP_ADDRESS_LEN_MAX	    (15)
#define xg_PORT_LEN_MAX			    (5)
#define xg_TCP_END_POINT_LEN_MAX    (xg_IP_ADDRESS_LEN_MAX + xg_PORT_LEN_MAX + 1)
/* + 1 for colon, e.g. 255.255.255.255:65535 */

typedef struct _IPEndPoint
{
	char		ipAddress[ xg_IP_ADDRESS_LEN_MAX + 1 ];
	char		port[ xg_PORT_LEN_MAX + 1 ];

} IPEndPoint;


MVStatus TCPCommon_ReadEndPoint( const char *, IPEndPoint * );


#if defined (__cplusplus) || defined (FORTRAN)
}
#endif

#endif /* __TCPCOMMON_H */