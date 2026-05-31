/*
    Name:           TCPCommon.c
    Author (Date):  ihenderson, 25/11/05
    Platform:       PC
    Purpose:        Common TCP functionality used by
                    TCPClient and TCPServer modules.

    Last revision:  $Revision:: 2                           $
    Last changed:   $Date:: 12/04/06 16:07                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: TCPCommon.c $    
    
    *****************  Version 2  *****************
    User: Ihenderson   Date: 12/04/06   Time: 16:07
    Updated in $/PCMOVA/MVComms
    
    *****************  Version 1  *****************
    User: Ihenderson   Date: 28/11/05   Time: 11:52
    Created in $/PCMOVA/MVComms
*/

#include <string.h>
#include "TCPCommon.h"
#include "CommsErr.h"

#include "MVTypes.h"
#include "MVDebug.h"
#include "MVTrace.h"

#define	g_MODULE_NAME	    ("TCPCommon")

/*
    Name:           TCPCommon_ReadEndPoint
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Extracts the IP address and port number
					from the given string.
    Inputs:         IP end point (address and port number) as a string
                    IPEndPoint struct to be initialised
    Returns:        Status (success/failure)
                    Call //PCError_GetLastError for 
                    further error information.
    Assumptions:    
    Effects:        
*/
MVStatus TCPCommon_ReadEndPoint( const char * ipEndPointStr, IPEndPoint * p_endPoint )
{
	MVStatus status = MV_SUCCESS;
	char *		p_colon;
	MVSize		endPointStrLen;

    TRACE_ASSERT_VALID_ADDRESS( ipEndPointStr, "No IP end point string." );
    TRACE_ASSERT_VALID_ADDRESS( p_endPoint, "No IP end point to set." );
	
	p_colon = strchr( ipEndPointStr, ':' );
	endPointStrLen = strlen( ipEndPointStr );

	/* If the port and ip address can be separated and the endpoint isn't too long */
	if ( p_colon != NULL && endPointStrLen <= xg_TCP_END_POINT_LEN_MAX )
	{
		MVPtrDiff	colonPos;
		MVPtrDiff	portStrLen;

		colonPos = p_colon - ipEndPointStr;
		portStrLen = endPointStrLen - colonPos - 1;

		/* If the IP address and port number are the right length */
		if ( colonPos <= xg_IP_ADDRESS_LEN_MAX && portStrLen <= xg_PORT_LEN_MAX )
        {
			/* Extract IP address and port, ensuring they are null-terminated */
			strncpy( p_endPoint->ipAddress, ipEndPointStr, colonPos );
            p_endPoint->ipAddress[ colonPos ] = '\0';

			strncpy( p_endPoint->port, p_colon + 1, portStrLen );
            p_endPoint->port[ portStrLen ] = '\0';
		}
		else
		{
			CommsError_SetLast( COMMS_ERROR_END_POINT_INVALID, g_MODULE_NAME );
			status = MV_FAILURE;
		}
	}
	else
	{
		CommsError_SetLast( COMMS_ERROR_END_POINT_INVALID, g_MODULE_NAME );
		status = MV_FAILURE;
	}

	return ( status );

} /* TCPCommon_ReadEndPoint */