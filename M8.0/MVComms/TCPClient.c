/*
    Name:           TCPClient.c
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Provides TCP client connection functionality
                    which is used in PCMOVA for communicating 
                    between PCMOVA and a Controller.  
                    This module is generic though - 
                    it could be used by other programs
                    e.g. MOVA Comm and MOVA Sim.

*/

/*
	TODO:  Thread-safety! ReadBytes and WriteBytes use critical sections,
    but no other functions at present.

    Use InitializeCriticalSectionAndSpinCount to pre-allocate memory
    for critical sections? - means that there's no risk of raising (low) memory 
    exceptions when calling EnterCriticalSection.
*/

/* Include standard Windows headers */
#if defined (WIN32)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#else

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <sys/time.h>
#include <arpa/inet.h>
#undef _FEATURES_H
#define _XOPEN_SOURCE 600
#include <unistd.h>

typedef int SOCKET;
#define SOCKET_ERROR (-1)
#define INVALID_SOCKET (-1)
#define SD_RECEIVE 0
#define SD_SEND 1
#define SD_BOTH 2
#define closesocket close
#endif

#include <string.h>

#include "MVTypes.h"
#include "MVDebug.h"
#include "MVTrace.h"
#include "MVMath.h"

#include "TCPClient.h"
#include "TCPCommon.h"
#include "CommsErr.h"

#define	g_MODULE_NAME	    ("TCPClient")

/* Number of seconds to wait to send unsent data when closesocket() is called.
Used for blocking sockets only - not recommended for non-blocking sockets. 
Not using lingering at the moment. */
#define g_LINGER_TIMEOUT            (0)

/* Number of seconds to wait for a connection to be established.
Used for non-blocking sockets only - blocking sockets take care of this for us. */
#define g_CONNECT_TIMEOUT           (2)

/* Number of seconds to wait for a socket to become writeable.
Used for non-blocking sockets only - blocking sockets take care of this for us. */
#define g_WRITE_TIMEOUT             (2)

/* Number of bytes to try to read per recv() on shutdown. */
#define g_READ_DISCONNECT_BUF_SIZE  (128)


typedef struct _IPTimeouts
{
    MVInt  receiveConstant;
    MVInt  sendConstant;

} IPTimeouts;


typedef struct _IPConnection
{
    SOCKET		        socket;
    MVBool		        isConnected;
    MVBool		        isBlocking;
	IPEndPoint	        ipEndPoint;
    IPTimeouts          timeOuts;
    CRITICAL_SECTION    criticalSection;

} IPConnection;


static IPConnection	    g_connections[ CONNECTION_ID_COUNT ];
static MVInt		    g_connectionCount;
static MVBool		    g_isInitialised = MV_FALSE;
static MVInt            g_refCount;

static MVStatus     TCPClient_Configure(            const IPConnection * );
static MVStatus     TCPClient_WaitForConnection(    const IPConnection * );


/* This function is accessed outside this module by TCPServer */
MVStatus			TCPClient_Add(				SOCKET, ConnectionID,
                                                const IPEndPoint *, MVBool );

#if defined (_TRACE)
    static void		TCPClient_OutputSocketInfo_TRACE( SOCKET );
#else
    #define         TCPClient_OutputSocketInfo_TRACE( sock )
#endif


/*
    Name:           TCPClient_Initialise
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Initialises Windows Sockets
    Inputs:         void
    Returns:        Status (success/failure)
                    Call //PCError_GetLastError for 
                    further error information.
    Assumptions:    
    Effects:        
*/
MVStatus TCPClient_Initialise( const char * configFileName, 
                              const char * traceFileName )
{
    MVStatus	    status = MV_SUCCESS;
	MVInt		    result;
	WSADATA         wsaData;
    ConnectionID    connID;            	

    if ( !g_isInitialised )
    {               
        for ( connID = 0; connID != MATH_ARRAY_LEN( g_connections ); ++connID )
        {
            /* Initialise the critical section for each connection (once only) */    
            InitializeCriticalSection( &( g_connections[ connID ].criticalSection ) );
        }

        TRACE_INITIALISE( configFileName, traceFileName );
	    CommsError_Initialise();

        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Initialising communications..." );

	    /* Initialise Winsock (ask for version 2.2) */
	    result = WSAStartup( MAKEWORD(2,2), &wsaData );
	    if ( result != NO_ERROR )
	    {
		    CommsError_SetLast( COMMS_ERROR_INIT, g_MODULE_NAME );
		    status = MV_FAILURE;
	    }

        if ( status == MV_SUCCESS )
        {
            /* All connections are disconnected to begin with */
            for ( connID = 0; connID != MATH_ARRAY_LEN( g_connections ); ++connID )
            {
                IPConnection * p_conn = &( g_connections[ connID ] );
                
                p_conn->isConnected = MV_FALSE;
                p_conn->isBlocking = MV_TRUE;
                p_conn->socket = INVALID_SOCKET;
                p_conn->ipEndPoint.ipAddress[0] = '\0';
                p_conn->ipEndPoint.port[0] = '\0';

                /* By default, don't use timeouts */
                p_conn->timeOuts.receiveConstant = 0;
                p_conn->timeOuts.sendConstant = 0;
            }

            g_connectionCount = 0;        

			g_isInitialised = MV_TRUE;
			g_refCount = 1;

            TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Communications initialised." );
		}

    } /* if ( !g_isInitialised ) */

    else
    {
        g_refCount++;
    }

    return ( status );

} /* TCPClient_Initialise */



MVStatus TCPClient_DeInitialise( void )
{
    MVStatus status = MV_SUCCESS;

    if ( g_isInitialised )
    {               
        if ( g_refCount == 1 )
        {
            MVInt result;
            ConnectionID connID;

            TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
                "Deinitialising communications..." );			

            /* Terminate any existing connections */
            if ( g_connectionCount > 0 )
            {                
                for ( connID = 0; 
                    connID != MATH_ARRAY_LEN( g_connections ); 
                    ++connID )
                {
					status = TCPClient_Disconnect( connID );
                }
            }

            result = WSACleanup();
            if ( result == SOCKET_ERROR )
            {
                CommsError_SetLastWin( COMMS_ERROR_DEINIT, 
                    WSAGetLastError(), g_MODULE_NAME );
                status = MV_FAILURE;
            }

            for ( connID = 0; connID != MATH_ARRAY_LEN( g_connections ); ++connID )
            {
                /* Delete the critical section for each connection (once only) */
                DeleteCriticalSection( &( g_connections[ connID ].criticalSection ) );
            }                        

            /* BUGFIX: 10077/10055.  Ensure module is marked as deinitialised 
            even if an error occurred - ReadBytes etc. cannot be called now
            because the critical section has been destroyed.  Doing so will
            cause an access violation in NTDll.dll. */
            g_isInitialised = MV_FALSE;
            TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
                "Communications deinitialised." );

        } /* if ( g_refCount == 0 ) */
			
		g_refCount--;

    } /* if ( g_isInitialised ) */

    return ( status );

} /* TCPClient_DeInitialise */


MVBool TCPClient_IsInitialised( void )
{
    return ( g_isInitialised );
}


MVStatus TCPClient_SetTimeouts( ConnectionID connID, 
                               MVInt receiveTimeout, MVInt sendTimeout )
{
    IPConnection *    p_conn;
    
    /* Ensure that SetTimeouts fails gracefully
    if parameters or module state are incorrect. */
    if ( !CommsError_CheckInitialised( "TCPClient_SetTimeouts", g_isInitialised ) 
        || !CommsError_CheckConnectionID( "TCPClient_SetTimeouts", connID ) ) 
    {
        return MV_FAILURE;
    }

    p_conn = &( g_connections[ connID ] );
	
	TRACE_WRITE2_IF( LEVEL_INFO, g_MODULE_NAME, 
		"Setting timeouts: receive %d (ms), send %d (ms).", 
        receiveTimeout, sendTimeout );    

    p_conn->timeOuts.receiveConstant = receiveTimeout;
    p_conn->timeOuts.sendConstant = sendTimeout;

    return ( MV_SUCCESS );
}


/*
    Name:           TCPClient_Connect
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Creates a connection to the given end point
    Inputs:         IP end point (address and port number) as a string
                    Connection ID to give to the connection
                    Whether the connection is blocking or not - 
                    whether reads/writes return immediately.
    Returns:        Status (success/failure)
                    Call //PCError_GetLastError for 
                    further error information.
    Assumptions:    
    Effects:        
*/
MVStatus TCPClient_Connect( ConnectionID connID, 
                           const char * ipEndPointStr, MVBool isBlocking )
{
	MVStatus		status = MV_SUCCESS;
	IPConnection *	p_conn;

    /* Ensure that Connect fails gracefully
    if parameters or module state are incorrect. */
    if ( !CommsError_CheckInitialised( "TCPClient_Connect", g_isInitialised ) 
        || !CommsError_CheckConnectionID( "TCPClient_Connect", connID )
        || !CommsError_CheckConnectionClosed( "TCPClient_Connect", g_connections[ connID ].isConnected )
        || !CommsError_CheckEndPoint( "TCPClient_Connect", ipEndPointStr ) ) 
    {
        return MV_FAILURE;
    }

    /* Get the connection data for a new connection with the given ID */
    p_conn = &( g_connections[ connID ] );

	TRACE_WRITE1_IF( LEVEL_INFO, g_MODULE_NAME,
		"Opening connection with IP end point: %s.", ipEndPointStr );
    
    p_conn->isBlocking = isBlocking;
	status = TCPCommon_ReadEndPoint( ipEndPointStr, &( p_conn->ipEndPoint ) );

	if ( status == MV_SUCCESS )
	{
		MVBool isConnected = MV_FALSE;
		MVInt result;
		ADDRINFO Hints, *AddrInfo, *AI;
		memset(&Hints, 0, sizeof(Hints));
		
		Hints.ai_family = PF_UNSPEC;
		Hints.ai_socktype = SOCK_STREAM;
		Hints.ai_protocol = IPPROTO_TCP;

		result = getaddrinfo( p_conn->ipEndPoint.ipAddress,
			p_conn->ipEndPoint.port, &Hints, &AddrInfo );
		if ( result != 0 )
        {
            CommsError_SetLastWin( COMMS_ERROR_OPEN, 
                WSAGetLastError(), g_MODULE_NAME );
        }

        // Valid addresses were found
        else
		{
			AI = AddrInfo;
			isConnected = MV_FALSE;
			while ( !isConnected && AI != NULL )
			{
				// Open a socket with the correct address family for this address.
				p_conn->socket = socket( AI->ai_family, 
                    AI->ai_socktype, AI->ai_protocol );
				if ( p_conn->socket == INVALID_SOCKET )
				{
                    CommsError_SetLastWin( COMMS_ERROR_OPEN, 
                        WSAGetLastError(), g_MODULE_NAME );

                    status = MV_FAILURE;
				}
                
                // A socket was opened
                else
				{
                    // Configure the socket (e.g. apply timeouts etc.)
                    status = TCPClient_Configure( p_conn );
                    if ( status == MV_SUCCESS )
                    {
					    // IRH NOTE: Explicit bind()'ing is discouraged for client apps.
					    // Explicit binding only for server apps
					    result = connect( p_conn->socket, AI->ai_addr, (int)(AI->ai_addrlen) );
                        if ( result == SOCKET_ERROR )
                        {
                            MVInt lastError = WSAGetLastError();
                            
                            /* If this is a non-blocking socket, connect() cannot complete
                            immediately - wait for it to complete, otherwise read/write 
                            operations will fail */
                            if ( !( p_conn->isBlocking )
                                && lastError == WSAEWOULDBLOCK )
                            {
                                status = TCPClient_WaitForConnection( p_conn );
                            }

                            /* Otherwise a genuine error occurred - report it */
                            else
                            {
                                CommsError_SetLastWin( COMMS_ERROR_OPEN, 
                                    lastError, g_MODULE_NAME );

                                status = MV_FAILURE;
                            }
                        }
                        
                    } // A socket was configured

				} // A socket was opened

                if ( status == MV_SUCCESS )
                {
                    /* The socket was opened and configured correctly 
                    and a connection has been established */
                    isConnected	= MV_TRUE;                            
                }

                /* Else an error occurred configuring or connecting the socket */
                else
                {	
					result = closesocket( p_conn->socket );
                    if ( result == SOCKET_ERROR )
                    {
                        TRACE_WRITE1_IF( LEVEL_ERROR, g_MODULE_NAME,
                            "Failed to close socket following initialisation error. Windows error code: %d.", 
                            WSAGetLastError() );
                    }
				}

				AI = AI->ai_next;

			} // while ( !isConnected && AI != NULL )

			freeaddrinfo( AddrInfo );

		} // Valid addresses were found

		if ( isConnected )
		{		
            p_conn->isConnected = MV_TRUE;
            g_connectionCount++;

            TCPClient_OutputSocketInfo_TRACE( p_conn->socket );
			TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
				"Connection opened successfully." );
		}
		else
		{
            CommsError_SetLast( COMMS_ERROR_OPEN, g_MODULE_NAME );
			status = MV_FAILURE;
		}

	} /* If the IP end point was extracted ok */    

	return ( status );

} /* TCPClient_Connect */


static MVStatus TCPClient_Configure( const IPConnection * p_conn )
{
    MVInt				result = NO_ERROR;
    MVStatus            status = MV_SUCCESS;

    TRACE_ASSERT( g_isInitialised, "Module not initialised." );
    TRACE_ASSERT_VALID_ADDRESS( p_conn, "Connection is invalid." );

    /* Set client to non-blocking if requested (e.g. for MOVA user I/O) */
    if ( !( p_conn->isBlocking ) )
    {
        MVUInt32 nonBlocking = 1;
        
        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
            "Configuration: setting socket to non-blocking mode." );
              
        result = ioctlsocket(
            p_conn->socket,
            FIONBIO,
            &( nonBlocking ) );

        if ( result == SOCKET_ERROR )
        {
            CommsError_SetLastWin( COMMS_ERROR_CONFIGURE, 
                WSAGetLastError(), g_MODULE_NAME );

            status = MV_FAILURE;
        }

    } /* Non-blocking socket */


    /* Else this is a blocking socket (e.g. for MOVA controller I/O)
    Set a linger timeout and apply any read/write timeouts */
    else
    {
	    const IPTimeouts *	p_timeouts = &( p_conn->timeOuts );

        TRACE_WRITE3_IF( LEVEL_INFO, g_MODULE_NAME, 
            "Configuration: applying timeouts: linger %d (s), receive %d (ms), send %d (ms).",
		    g_LINGER_TIMEOUT, p_timeouts->receiveConstant, p_timeouts->sendConstant );

		// NOTE - This if statement causes a compiler warning because g_LINGER_TIMEOUT is
		// defined above as zero so this chunk of code is never called. Given that we may
		// need this code at some point in the future we will leave as is for now.
        if ( g_LINGER_TIMEOUT > 0 )
        {
            struct linger   lingerOptions;
            
            lingerOptions.l_onoff = 1;                  // Enable lingering
            lingerOptions.l_linger = g_LINGER_TIMEOUT;  // Linger timeout (s)

            /* Enable lingering so that if there is unsent data,
            the data gets sent when closesocket() is called and 
            closesocket() blocks until that happens */
            result = setsockopt( 
                p_conn->socket, 
                SOL_SOCKET, 
                SO_LINGER, 
                (char *)&lingerOptions, 
                sizeof( lingerOptions ) );
        }
        
        if ( result != SOCKET_ERROR 
            && p_conn->timeOuts.receiveConstant > 0 )
        {
            result = setsockopt( 
                p_conn->socket, 
                SOL_SOCKET, 
                SO_RCVTIMEO, 
                (char *)&( p_timeouts->receiveConstant ), 
                sizeof( p_timeouts->receiveConstant ) );
        }

        if ( result != SOCKET_ERROR
            && p_conn->timeOuts.sendConstant > 0 )
        {
            result = setsockopt( 
                p_conn->socket, 
                SOL_SOCKET, 
                SO_SNDTIMEO, 
                (char *)&( p_timeouts->sendConstant ), 
                sizeof( p_timeouts->sendConstant ) );
        }

        if ( result == SOCKET_ERROR )
        {        
            CommsError_SetLastWin( COMMS_ERROR_CONFIGURE, 
                WSAGetLastError(), g_MODULE_NAME );

            status = MV_FAILURE;
        }

    } /* Blocking socket */

    return ( status );

} /* TCPClient_Configure */


MVBool TCPClient_IsConnected( ConnectionID connID )
{
    return ( g_connections[ connID ].isConnected );

} /* TCPClient_IsConnected */


MVInt TCPClient_GetConnectionCount( void )
{
    return ( g_connectionCount );

} /* TCPClient_GetConnectionCount */


MVStatus TCPClient_Disconnect( ConnectionID connID )
{
    MVStatus        status = MV_SUCCESS;
    IPConnection *  p_conn;
	MVInt			result = NO_ERROR;
    MVInt			bytesRecv;
    MVByte          recvBuf[ g_READ_DISCONNECT_BUF_SIZE ];

    /* Don't assert during cleanup in release builds */
    DEBUG_ASSERT( g_isInitialised );
    DEBUG_ASSERT( MATH_IS_IN_RANGE( connID, 0, MATH_ARRAY_LEN( g_connections ) - 1 ) );
    if ( !MATH_IS_IN_RANGE( connID, 0, MATH_ARRAY_LEN( g_connections ) - 1 ) )
    {
        TRACE_WRITE1_IF( LEVEL_ERROR, g_MODULE_NAME,
            "TCPClient_Disconnect failed: connection ID %d.", connID );
		CommsError_SetLast( COMMS_ERROR_PARAM_INVALID, g_MODULE_NAME ); 
        return ( MV_FAILURE );
    }
    if ( !g_isInitialised )
    {
        TRACE_WRITE1_IF( LEVEL_ERROR, g_MODULE_NAME,
            "TCPClient_Disconnect failed: initialised %d.", g_isInitialised );
        CommsError_SetLast( COMMS_ERROR_INVALID_OPERATION, g_MODULE_NAME ); 
        return ( MV_FAILURE );
    }

    p_conn = &( g_connections[ connID ] );

    if ( p_conn->isConnected )
    {    
        TRACE_WRITE2_IF( LEVEL_INFO, g_MODULE_NAME, 
            "Closing connection to %s:%s.",
            p_conn->ipEndPoint.ipAddress, p_conn->ipEndPoint.port );
       
        /* Mark the connection as disconnected */
        p_conn->isConnected = MV_FALSE;
        g_connectionCount--;

        /* Call shutdown before closesocket to close the socket gracefully. */
        result = shutdown( p_conn->socket, SD_SEND );
        if ( result == SOCKET_ERROR )
        {      
            CommsError_SetLastWin( COMMS_ERROR_CLOSE, 
                WSAGetLastError(), g_MODULE_NAME );
            status = MV_FAILURE;
        }
    
        /* If the socket is blocking, set to non-blocking before receiving
           any remaining bytes.  We don't want to hang around waiting to 
           read bytes on disconnection. */
        if ( p_conn->isBlocking )
        {                
            MVUInt32 nonBlocking = 1;

            result = ioctlsocket( p_conn->socket, FIONBIO, &( nonBlocking ) );
            if ( result == SOCKET_ERROR )
            {
                CommsError_SetLastWin( COMMS_ERROR_CLOSE, 
                    WSAGetLastError(), g_MODULE_NAME );
                status = MV_FAILURE;
            }
        }

        if ( result == NO_ERROR )
        {
		    /* Read bytes while the server side is notified and initiates its own shutdown
            until no bytes are received or an error occurs. */			
		    /* See: "Graceful Shutdown, Linger Options, and Socket Closure" 
		    in the Windows Platform SDK */
		    do
            {   
                bytesRecv = recv( p_conn->socket, 
                    &( (char)recvBuf[0] ), sizeof( recvBuf ), 0 );
		    }
		    while ( bytesRecv != SOCKET_ERROR && bytesRecv != 0 );
        }
        else
        {
            CommsError_SetLastWin( COMMS_ERROR_CLOSE, 
                WSAGetLastError(), g_MODULE_NAME );
            status = MV_FAILURE;
        }

        /* Close the socket, freeing any resources associated with it,
        If lingering is enabled (blocking sockets only), any unsent data 
        should be sent up to the linger timeout. */
        result = closesocket( p_conn->socket );
        if ( result == SOCKET_ERROR )
        {      
            CommsError_SetLastWin( COMMS_ERROR_CLOSE, 
                WSAGetLastError(), g_MODULE_NAME );
            status = MV_FAILURE;
        }

        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Connection closed." );
    }

    return ( status );

} /* TCPClient_Disconnect */


MVStatus TCPClient_Add( SOCKET newSocket, ConnectionID connID, 
                       const IPEndPoint * p_endPoint, MVBool isBlocking  )
{
    MVStatus		status = MV_SUCCESS;
	IPConnection *	p_conn;

    TRACE_ASSERT( g_isInitialised, "Module not initialised." );
    TRACE_ASSERT_VALID_ADDRESS( p_endPoint, "No IP end point." );
    TRACE_ASSERT( connID >= 0 && connID < MATH_ARRAY_LEN( g_connections ),
		"Connection ID invalid." );

    /* Get the connection data for a new connection with the given ID */
    p_conn = &( g_connections[ connID ] );

    TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Adding new connection..." );
	TCPClient_OutputSocketInfo_TRACE( newSocket );

    p_conn->socket = newSocket;	
    p_conn->isBlocking = isBlocking;

    /* Copy the end point strings */
    strncpy( p_conn->ipEndPoint.ipAddress, p_endPoint->ipAddress, 
        xg_IP_ADDRESS_LEN_MAX );
    strncpy( p_conn->ipEndPoint.port, p_endPoint->port, 
        xg_PORT_LEN_MAX );

	g_connectionCount++;

    status = TCPClient_Configure( p_conn );
    if ( status == MV_SUCCESS )
    {
        p_conn->isConnected = MV_TRUE;
	    TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "New connection added." );
    }

    return ( status );

} /* TCPClient_Add */


MVStatus	TCPClient_WriteBytes(  ConnectionID connID, const MVByte * p_bytes, 
                                 MVInt bytesCount, MVInt * p_bytesWritten )
{
	MVStatus	    status = MV_SUCCESS;
    MVInt           result;
    MVInt		    bytesSent;
	MVInt		    bytesOffset = 0;
    IPConnection *  p_conn;

    /* Ensure that WriteBytes fails gracefully
    if parameters or module state are incorrect. */
    if ( !CommsError_CheckInitialised( "TCPClient_WriteBytes", g_isInitialised ) 
        || !CommsError_CheckConnectionID( "TCPClient_WriteBytes", connID )
        || !CommsError_CheckBytes( "TCPClient_WriteBytes", p_bytes ) 
        || !CommsError_CheckBytesCount( "TCPClient_WriteBytes", bytesCount ) ) 
    {
        return MV_FAILURE;
    }

    p_conn = &( g_connections[ connID ] );   

    /* START CRITICAL SECTION */
    EnterCriticalSection( &( p_conn->criticalSection ) );

    if ( p_conn->isConnected )
    {
	    // TODO: should check that messages aren't too long for protocol 
        // - use getsockopt() for this

        while ( bytesOffset != bytesCount )
        {
            /* Non-blocking socket */
            if ( !( p_conn->isBlocking ) )
            {
                /* Check socket for writability before calling send
                This ensures that socket is in a suitable state for 
                writing, e.g. that the write buffer isn't full-up */
                struct timeval  writeTimeout;
                fd_set  socketSet;                
                FD_ZERO( &socketSet );
                FD_SET( p_conn->socket, &socketSet );

                writeTimeout.tv_sec = g_WRITE_TIMEOUT;
                writeTimeout.tv_usec = 0;

                /* Call the select function, which blocks until 
                the socket becomes writeable or times out */
                result = select(
                    0,                           
                    NULL,
                    &socketSet,     // Check for writability
                    NULL,
                    &writeTimeout   // Block until the socket becomes writeable or times out 
                );

                if ( result == SOCKET_ERROR )
                {
                    CommsError_SetLastWin( COMMS_ERROR_SEND, 
                        WSAGetLastError(), g_MODULE_NAME );

                    status = MV_FAILURE;
                    break;
                }                
                
                /* The select timed out - assume connection has failed */
                if ( result == 0 )
                {
                    CommsError_SetLast( COMMS_ERROR_SEND_TIMEOUT, 
                        g_MODULE_NAME );

                    status = MV_FAILURE;
                    break;
                }

            } /* Non-blocking socket */

            /* Now try to send the bytes */
            bytesSent = send( p_conn->socket, 
				(char *)p_bytes + bytesOffset, 
				bytesCount - bytesOffset, 0 );
            if ( bytesSent == SOCKET_ERROR )
            {
                MVInt lastError = WSAGetLastError();

                switch ( lastError )
                {
                    /* Blocking sockets with time outs */
                    case WSAETIMEDOUT:
                    {
                        TRACE_WRITE1_IF( LEVEL_INFO, g_MODULE_NAME,
                            "Could not write to connection (timed-out). Windows error code: %d.", 
                            lastError );

				        CommsError_SetLastWin( COMMS_ERROR_SEND_TIMEOUT, 
                            lastError, NULL );
                        break;
                    }

                    /* All sockets */
                    case WSAECONNABORTED:
                    {
                        TRACE_WRITE1_IF( LEVEL_INFO, g_MODULE_NAME,
                            "Could not write to connection (aborted). Windows error code: %d.", 
                            lastError );

				        CommsError_SetLastWin( COMMS_ERROR_SEND_END, 
                            lastError, NULL );
                        break;
                    }

                    default:
                    {
                        TRACE_WRITE1_IF( LEVEL_ERROR, g_MODULE_NAME,
                            "Could not write to connection. Windows error code: %d.", 
                            lastError );

                        CommsError_SetLastWin( COMMS_ERROR_SEND, 
                            lastError, g_MODULE_NAME );
                        break;
                    }
                }

                status = MV_FAILURE;
                break;
            }
            else
            {
                bytesOffset += bytesSent;
            }

        } // while ( bytesOffset != bytesCount )

    } // If a connection exists

    else
    {
		TRACE_WRITE0_IF( LEVEL_VERBOSE, g_MODULE_NAME, 
			"Could not write to connection: connection closed." );
        
        CommsError_SetLast( COMMS_ERROR_SEND_CLOSED, NULL );
        status = MV_FAILURE;
    }

    (*p_bytesWritten) = bytesOffset;

    /* END CRITICAL SECTION */
    LeaveCriticalSection( &( p_conn->criticalSection ) );

	return ( status );

} /* TCPClient_WriteBytes */


MVStatus TCPClient_ReadBytes( ConnectionID connID, MVByte * p_bytes, 
                             MVInt bytesCount, MVInt * p_bytesRead )
{
	MVStatus	    status = MV_SUCCESS;
	MVInt		    bytesRecv;
    MVInt		    bytesOffset = 0;
    IPConnection *    p_conn;

    /* BUGFIX: 10055/10077.  Ensure that ReadBytes fails gracefully
    if parameters or module state are incorrect. */
    if ( !CommsError_CheckInitialised( "TCPClient_ReadBytes", g_isInitialised ) 
        || !CommsError_CheckConnectionID( "TCPClient_ReadBytes", connID )
        || !CommsError_CheckBytes( "TCPClient_ReadBytes", p_bytes ) 
        || !CommsError_CheckBytesCount( "TCPClient_ReadBytes", bytesCount ) ) 
    {
        return MV_FAILURE;
    }

    p_conn = &( g_connections[ connID ] );
   
    /* START CRITICAL SECTION */
    EnterCriticalSection( &( p_conn->criticalSection ) );

    if ( p_conn->isConnected )
    {
	    // last arg MSG_PEEK for peeking (getting data but leaving it on input queue)
        do
        {
	        bytesRecv = recv( p_conn->socket, 
                (char *)p_bytes + bytesOffset, 
                bytesCount - bytesOffset, 0 );

            if ( bytesRecv == SOCKET_ERROR )
            {
                MVInt lastError = WSAGetLastError();

                switch ( lastError )
                {
                    /* Blocking sockets with time outs */
                    case WSAETIMEDOUT:
                    {
                        /* If no bytes were read because of a timeout 
                        - fail without tracing (e.g. with MOVA Comm, we want
                        to ignore this failure and continue to try to read */
				        CommsError_SetLastWin( COMMS_ERROR_RECV_TIMEOUT, 
                            lastError, NULL );
                        break;
                    }

                    /* Non-blocking sockets */
                    case WSAEWOULDBLOCK:
                    {
                        /* If no bytes were read because there were
                        none waiting - fail without tracing */
				        CommsError_SetLastWin( COMMS_ERROR_RECV_BLOCK, 
                            lastError, NULL );
                        break;
                    }

                    /* All sockets */
                    case WSAECONNABORTED:
                    {
                        TRACE_WRITE1_IF( LEVEL_INFO, g_MODULE_NAME,
                            "Could not read from connection (aborted). Windows error code: %d.", 
                            lastError );

				        CommsError_SetLastWin( COMMS_ERROR_RECV_END, 
                            lastError, NULL );
                        break;
                    }

                    default:
                    {
                        CommsError_SetLastWin( COMMS_ERROR_RECV, 
                            lastError, g_MODULE_NAME );
                        break;
                    }
                }

                status = MV_FAILURE;
                break;		    
	        }
            else
            {
                bytesOffset += bytesRecv;
            }

        } while ( bytesOffset < bytesCount && bytesRecv > 0 );


        // If no error occurred
        if ( bytesRecv != SOCKET_ERROR )
        {
            /* If no bytes were read, but there was no error,
               the connection was closed gracefully - fail without tracing */
            if ( bytesRecv == 0 )
            {
                TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME,
                    "Could not read from connection (closed gracefully)." );

                CommsError_SetLast( COMMS_ERROR_RECV_END, NULL );
                status = MV_FAILURE;
            }
            else
            {
                /* Fail if we didn't received all the bytes and there were
                no errors reported above. */
                if ( bytesOffset != bytesCount )
                {                   
			        TRACE_WRITE2( g_MODULE_NAME, 
                        "Receive error: bytes written %d, bytes expected %d.", 
                        bytesOffset, bytesCount );

                    CommsError_SetLast( COMMS_ERROR_RECV_COUNT, g_MODULE_NAME );
                    status = MV_FAILURE;
                }
            }

        } // If no error occurred

    } // If connected

    // Else not connected
    else
    {
		TRACE_WRITE0_IF( LEVEL_VERBOSE, g_MODULE_NAME, 
			"Could not read from connection: connection closed." );
        
        CommsError_SetLast( COMMS_ERROR_RECV_CLOSED, NULL );
        status = MV_FAILURE;
    }

    (*p_bytesRead) = bytesOffset;

    /* END CRITICAL SECTION */
    LeaveCriticalSection( &( p_conn->criticalSection ) );

	return ( status );

} /* TCPClient_ReadBytes */


MVStatus TCPClient_WriteByte( ConnectionID connID, MVByte byte )
{
    MVInt bytesWritten;
    
    return ( TCPClient_WriteBytes( connID, &byte, 1, &bytesWritten ) );

} /* TCPClient_WriteByte */


MVStatus TCPClient_ReadByte( ConnectionID connID, MVByte * p_byte )
{
    MVInt bytesRead;    

	return ( TCPClient_ReadBytes( connID, p_byte, 1, &bytesRead ) );

} /* TCPClient_ReadByte */


static MVStatus TCPClient_WaitForConnection( const IPConnection * p_conn )
{
    MVStatus        status;
    fd_set          socketSet;
    MVInt           result;    
    struct timeval  connectTimeout;

    TRACE_ASSERT( g_isInitialised, "Module not initialised." );
    TRACE_ASSERT_VALID_ADDRESS( p_conn, "Connection is invalid." );

    TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
        "Waiting for non-blocking socket to connect..." );

    FD_ZERO( &socketSet );
    FD_SET( p_conn->socket, &socketSet );

    connectTimeout.tv_sec = g_CONNECT_TIMEOUT;
    connectTimeout.tv_usec = 0;

    /* Call the select function, which blocks until 
    the socket becomes readable and writable */
    result = select(
        0,                           
        &socketSet,     // Check for readability
        &socketSet,     // Check for writability
        NULL,
        &connectTimeout // Block until connected or the timeout is reached
    );

    switch ( result )
    {   
        case SOCKET_ERROR:
        {
            CommsError_SetLastWin( COMMS_ERROR_OPEN, 
                WSAGetLastError(), g_MODULE_NAME );

            status = MV_FAILURE;
            break;
        }
        
        /* The select timed out - assume connection has failed */
        case 0:
        {
            CommsError_SetLast( COMMS_ERROR_CONNECT_TIMEOUT, 
                g_MODULE_NAME );

            status = MV_FAILURE;
            break;
        }
        
        /* Otherwise a connection has been made */
        default:
        {
            TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
                "Non-blocking socket connected." );

            status = MV_SUCCESS;
            break;
        }

    } /* switch ( result ) */

    return ( status );

} /* TCPClient_WaitForConnection */


#if defined (_TRACE)
static void TCPClient_OutputSocketInfo_TRACE( SOCKET sock )
{
	struct sockaddr_storage addressInfo;
	MVInt	addressLen;
	char	addressName[ NI_MAXHOST ];

	/* Determine where the socket is connected */
	addressLen = sizeof(addressInfo);
	if ( getpeername( sock, (LPSOCKADDR)&addressInfo, &addressLen ) == SOCKET_ERROR )
	{
		TRACE_WRITE1_IF( LEVEL_INFO, g_MODULE_NAME, 
            "Failed to retrieve socket information. Windows error code: %d.", 
            WSAGetLastError() );
	}
	else
	{
		if ( getnameinfo( (LPSOCKADDR)&addressInfo, addressLen, addressName,
			sizeof( addressName ), NULL, 0, NI_NUMERICHOST ) != 0 )
		{
			strcpy( addressName, "<unknown>" );
		}
		else
		{
			TRACE_WRITE4_IF( LEVEL_INFO, g_MODULE_NAME, 
				"Socket info: address %s, port %d, protocol %s, protocol family %s.",
				addressName, 
				ntohs( SS_PORT( &addressInfo ) ),
				"TCP",
				( addressInfo.ss_family == PF_INET ) ? "PF_INET" : "PF_INET6" );
		}
	}

} /* TCPClient_OutputSocketInfo_TRACE */
#endif /* _TRACE */