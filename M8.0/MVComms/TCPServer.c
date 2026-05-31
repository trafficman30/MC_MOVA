/*
    Name:           TCPServer.c
    Author (Date):  ihenderson, 14/11/05
    Platform:       PC
    Purpose:        Provides TCP server connection functionality
                    which is used in PCMOVA for communicating 
                    between PCMOVA and MOVA Comm. (Only one
					connection to MOVA is allowed at a time.)
                    This module is generic though - 
                    it could be used by other programs.

    Last revision:  $Revision:: 6                           $
    Last changed:   $Date:: 12/04/06 16:07                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: TCPServer.c $
    
    *****************  Version 6  *****************
    User: Ihenderson   Date: 12/04/06   Time: 16:07
    Updated in $/PCMOVA/MVComms
    
    *****************  Version 5  *****************
    User: Ihenderson   Date: 3/04/06    Time: 10:34
    Updated in $/PCMOVA/MVComms
    
    *****************  Version 4  *****************
    User: Ihenderson   Date: 16/12/05   Time: 11:07
    Updated in $/PCMOVA/MVComms
    
    *****************  Version 3  *****************
    User: Ihenderson   Date: 5/12/05    Time: 11:13
    Updated in $/PCMOVA/MVComms
    
    *****************  Version 2  *****************
    User: Ihenderson   Date: 28/11/05   Time: 16:59
    Updated in $/PCMOVA/MVComms
    
    *****************  Version 1  *****************
    User: Ihenderson   Date: 28/11/05   Time: 11:52
    Created in $/PCMOVA/MVComms
*/

/*
    TODO: Add Init, DeInit, IsInit?
*/


/* Include standard Windows headers */
#define WIN32_LEAN_AND_MEAN

/* Supporting Win 2000 and later 
(TryEnterCriticalSection is not available in Windows 95/98) */
//#define _WIN32_WINNT	0x0500
//#define WINVER			0x0500

#include <winsock2.h>
#include <ws2tcpip.h>
//#include <windows.h>
#include <process.h>
#include <stdio.h> /* For sprintf */
#include <string.h> /* For strncpy */

#include "MVTypes.h"
#include "MVDebug.h"
#include "MVTrace.h"
#include "MVMath.h"

#include "TCPServer.h"
#include "TCPClient.h"
#include "TCPCommon.h"
#include "CommsErr.h"

#define	g_MODULE_NAME	            ("TCPServer")

#define g_LOOP_BACK_ADDRESS         ("127.0.0.1")

#define g_PORT_MIN			        (IPPORT_RESERVED + 1)
#define g_PORT_MAX			        (MV_UINT16_MAX)

/* Time to wait between checking for new connections (ms) */
#define g_NEW_CONNECTION_WAIT       (500)

/* Timeout to use when waiting for server thread to terminate - should be plenty (ms) */
#define g_TERMINATE_TIMEOUT         (100)

typedef struct _IPServer
{
    SOCKET				socket;
    MVBool				isStarted;
    MVBool				isBlocking;
    ConnectionID		connID;
	IPEndPoint			endPoint;    
    
    /* For wait-for-connection thread */
    HANDLE				hThread;
    DWORD				dwThreadID;

	//CRITICAL_SECTION    criticalSection;

} IPServer;


static IPServer     g_server;
static HANDLE       g_hTerminateEvent;
static MVBool       g_isTerminating;

static DWORD WINAPI	TCPServer_WaitForConnection(LPVOID );
static MVStatus		TCPServer_Add(				SOCKET,
                                                ConnectionID, MVBool );

/* This function is implemented in TCPClient */
extern MVStatus     TCPClient_Add(				SOCKET, ConnectionID,
                                                const IPEndPoint *, MVBool );

/*
    Name:           TCPServer_Create
    Author (Date):  ihenderson, 14/11/05
    Platform:       PC
    Purpose:        Creates a server and starts
					listening for connection 
					requests.
    Inputs:         Connection ID to assign to 
					any TCP clients when they connect.
    Returns:        Status (success/failure)
    Assumptions:    Assumes Winsock has already been 
					started (see TCPClient).
    Effects:        
*/
MVStatus TCPServer_Create( ConnectionID connID, 
                          const char * ipEndPoint, MVBool isBlocking )
{
    MVStatus status = MV_SUCCESS;
    
	ADDRINFO        Hints, *AddrInfo, *AI;
    MVInt           result;
    WSADATA         wsaData;
    
    TRACE_ASSERT( connID >= 0 && connID < CONNECTION_ID_COUNT,
		"Connection ID invalid." );
    TRACE_ASSERT( !g_server.isStarted, "Server is already started." );

	/* Initialise Winsock (ask for version 2.2) */
	result = WSAStartup( MAKEWORD(2,2), &wsaData );
	if ( result != NO_ERROR )
	{
		CommsError_SetLast( COMMS_ERROR_INIT, g_MODULE_NAME );
		return ( MV_FAILURE );
	}

	/* Initialise the critical section for each connection (once only) */    
    //InitializeCriticalSection( &( g_server.criticalSection ) );
	
	/* START CRITICAL SECTION */
    //EnterCriticalSection( &( g_server.criticalSection ) );

    TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Starting server..." );
  
    memset( &Hints, 0, sizeof( Hints ) );
    Hints.ai_family = PF_UNSPEC;
	Hints.ai_socktype = SOCK_STREAM;
	Hints.ai_protocol = IPPROTO_TCP;
    Hints.ai_flags = AI_NUMERICHOST | AI_PASSIVE;

    /* If a specific end point was requested, use that */
    if ( ipEndPoint != NULL )
    {
        IPEndPoint endPointToTry;

        TCPCommon_ReadEndPoint( ipEndPoint, &( endPointToTry ) );
        result = getaddrinfo( endPointToTry.ipAddress, 
            endPointToTry.port, &Hints, &AddrInfo );
    }
    /* Else no end point requested, try loopback address and 
    an automatically assigned port number */
    else
    {        
        result = getaddrinfo( g_LOOP_BACK_ADDRESS, 
            NULL, &Hints, &AddrInfo);
    }
    
    if ( result == NO_ERROR )
    {
        status = MV_FAILURE;

        /* For each address getaddrinfo returned, we create a new socket,
        bind that address to it, and create a queue to listen on.
        Finish once a socket has been set up successfully. */
        for ( AI = AddrInfo; AI != NULL; AI = AI->ai_next )
        {
		    SOCKET	socketNew;

            /* Only supporting PF_INET and PF_INET6. */
            if ( ( AI->ai_family != PF_INET ) && ( AI->ai_family != PF_INET6 ) )
            {
                continue;
            }

            /* Open a socket with the correct address family for this address. */
            socketNew = socket( AI->ai_family, AI->ai_socktype, AI->ai_protocol );
            if ( socketNew == INVALID_SOCKET )
            {
                CommsError_SetLastWin( COMMS_ERROR_SERVER_START, 
                    WSAGetLastError(), g_MODULE_NAME );
                continue;
            }

            /* bind() associates a local address and port combination
            with the socket just created. This is most useful when
            the application is a server that has a well-known port
            that clients know about in advance. */
            result = bind( socketNew, AI->ai_addr, (int)(AI->ai_addrlen) );
            if ( result == SOCKET_ERROR )
            {
                CommsError_SetLastWin( COMMS_ERROR_SERVER_START, 
                    WSAGetLastError(), g_MODULE_NAME );
                continue;
            }

            /* Listen for incoming connections.
            Only allow a backlog of one pending connection */
            result = listen( socketNew, 1 );
            if ( result == SOCKET_ERROR )
            {
                CommsError_SetLastWin( COMMS_ERROR_SERVER_START, 
                    WSAGetLastError(), g_MODULE_NAME );
                continue;
            }

		    /* Socket was set up successfully - register it and quit */
		    status = TCPServer_Add( socketNew, connID, isBlocking );
		    break;

        } /* For each address */

        freeaddrinfo( AddrInfo );

        /* If a server couldn't be set up, set an error */
        if ( status == MV_FAILURE )
        {
            CommsError_SetLast( COMMS_ERROR_SERVER_START, g_MODULE_NAME );
        }
    }
    else
    {
        CommsError_SetLastWin( COMMS_ERROR_SERVER_START, 
            WSAGetLastError(), g_MODULE_NAME );
        status = MV_FAILURE;
    }


    /* Create event for terminating the server thread */
    if ( status == MV_SUCCESS )
    {	    
		g_isTerminating = MV_FALSE;

	    g_hTerminateEvent = CreateEvent( 
		    NULL,	// Default security attributes
		    TRUE,	// Manual reset
		    FALSE,	// Not signalled to start with
		    NULL ); // No name

		if ( g_hTerminateEvent == NULL )
		{
			CommsError_SetLastWin( COMMS_ERROR_SERVER_START, 
				GetLastError(), g_MODULE_NAME );
			status = MV_FAILURE;
		}
	}


	/* Start wait for connection thread */
    if ( status == MV_SUCCESS )
	{
        g_server.hThread = (HANDLE)_beginthreadex( 
            NULL,                        /* Default security attributes (by default all access) */
            0,					         /* initial stack size default for this application     */
            (_beginthreadex_proc_type)TCPServer_WaitForConnection, /* thread function           */
            &g_isTerminating,            /* thread argument                                     */
            0,                           /* creation option (not suspended - start immediately) */
            &((unsigned int)(g_server.dwThreadID))     /* thread identifier                                   */
        );

        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
            "Server started successfully." );
    }
    
	/* END CRITICAL SECTION */
    //LeaveCriticalSection( &( g_server.criticalSection ) );

    return ( status );

} /* TCPServer_Create */


static MVStatus TCPServer_Add( SOCKET socketNew, 
                              ConnectionID connID, MVBool isBlocking )
{
	MVStatus status = MV_SUCCESS;
	struct sockaddr_storage addressInfo;
	MVInt	addressLen;
    MVInt   result;

    TRACE_ASSERT( connID >= 0 && connID < CONNECTION_ID_COUNT,
		"Connection ID invalid." );
    TRACE_ASSERT( !g_server.isStarted, "Server is already started." );
    TRACE_ASSERT( g_server.socket != INVALID_SOCKET, "Socket is invalid." );

	g_server.socket = socketNew;
	g_server.isStarted = MV_TRUE;
    g_server.isBlocking = isBlocking;
    g_server.connID = connID;

	/* Determine where the socket is connected */
	addressLen = sizeof( addressInfo );
    result = getsockname( socketNew, (LPSOCKADDR)&addressInfo, &addressLen );
	
    if ( result == NO_ERROR )
	{
        /* Get port */
        u_short portNum = ntohs( SS_PORT( &addressInfo ) );
        TRACE_ASSERT( MATH_IS_IN_RANGE( portNum, g_PORT_MIN, g_PORT_MAX ),
            "Port number is outside the valid range." );

        sprintf( g_server.endPoint.port, "%d", portNum );

#ifdef PCMOVA
		printf("PCMOVA is listening on port: %d\n", portNum);		
#endif

        /* Get IP address */
        result = getnameinfo( (LPSOCKADDR)&addressInfo, addressLen,
            g_server.endPoint.ipAddress, sizeof( g_server.endPoint.ipAddress ),
			NULL, 0, NI_NUMERICHOST );
    }

    if ( result == NO_ERROR )
    {
		TRACE_WRITE4_IF( LEVEL_INFO, g_MODULE_NAME, 
			"Socket info: address %s, port %s, protocol %s, protocol family %s.",
			g_server.endPoint.ipAddress, g_server.endPoint.port, "TCP",
			( addressInfo.ss_family == PF_INET ) ? "PF_INET" : "PF_INET6" );
	}

	else
	{
		CommsError_SetLastWin( COMMS_ERROR_END_POINT_GET, 
            WSAGetLastError(), g_MODULE_NAME );
        status = MV_FAILURE;
	}		

	return ( status );

} /* TCPServer_Add */


/*
    Name:           TCPServer_WaitForConnection
    Author (Date):  ihenderson, 14/11/05
    Platform:       PC
    Purpose:        Tries to accept any incoming connection requests
					creating a new TCP client when an incoming
					connection is accepted.
    Inputs:         None
    Returns:        Status (success/failure)
    Assumptions:   
    Effects:        
*/
static DWORD WINAPI TCPServer_WaitForConnection( LPVOID lpParam )
{
    SOCKET  clientSocket;
    MVInt   FromLen;
    DWORD   status = 0;
	DWORD   waitResult;
    SOCKADDR_STORAGE From;
    MVBool * p_isTerminating;
    
    TRACE_ASSERT( g_server.isStarted, "Server hasn't been started." );
    TRACE_ASSERT( g_server.socket != INVALID_SOCKET, "Socket is invalid." );

    p_isTerminating = (MVBool *)lpParam;

    /* Put the server into an eternal loop, serving requests as they arrive. */
    FromLen = sizeof(From);
    while ( *p_isTerminating == MV_FALSE )
    {              
		/* If there's no client currently connected
        - don't accept new connections while we have an existing client */
        if ( !TCPClient_IsConnected( g_server.connID ) )
        {    
            /* Wait until a connection is available (server socket is blocking)
            accept() will return with INVALID_SOCKET if closesocket is called. */
            clientSocket = accept( g_server.socket, (LPSOCKADDR)&From, &FromLen);
            if ( clientSocket == INVALID_SOCKET )
            {
                MVInt lastErr = WSAGetLastError();                
                if ( lastErr == WSAEINTR )
                {
                    /* Don't report the error if blocking was interrupted
                    (closesocket was called) TODO: is this ok? */
                    CommsError_SetLastWin( COMMS_ERROR_SERVER_LISTEN,
                        WSAGetLastError(), NULL );
                    status = 0;
                }
                else
                {
                    CommsError_SetLastWin( COMMS_ERROR_SERVER_LISTEN,
                        WSAGetLastError(), g_MODULE_NAME );
                    status = 1;
                }                
            }

            else
            {            
                /* Register the accepted socket so we can perform I/O on it            
                - make non-blocking as servers should be able to accept new 
                sockets while I/O is occurring on existing ones. 
                (N.B. Not an issue for PCMOVA, as we're only allowing 
                one client to connect at a time) */
                TCPClient_Add( clientSocket, g_server.connID,
                    &( g_server.endPoint ), g_server.isBlocking );
            }

        } /* If there's no client currently connected */

        /* A client is already connected, so wait a bit before checking again
        - only one client allowed at a time for MOVA. */
        waitResult = WaitForSingleObject( g_hTerminateEvent, g_NEW_CONNECTION_WAIT );
		if ( waitResult == WAIT_FAILED )
		{
			CommsError_SetLastWin( COMMS_ERROR_SERVER_LISTEN,
				GetLastError(), g_MODULE_NAME );
			status = 1;
        
			break;
		}

    } /* Server loop */

    return ( status );

} /* TCPServer_WaitForConnection */


MVStatus TCPServer_Destroy( void )
{
    MVStatus status = MV_SUCCESS;    
   
    DEBUG_ASSERT( g_server.socket != INVALID_SOCKET );
    DEBUG_ASSERT( MATH_IS_IN_RANGE( g_server.connID, 0, CONNECTION_ID_COUNT - 1 ) );
    if ( !MATH_IS_IN_RANGE( g_server.connID, 0, CONNECTION_ID_COUNT - 1 )
        || g_server.socket == INVALID_SOCKET )
    {
        TRACE_WRITE2_IF( LEVEL_ERROR, g_MODULE_NAME,
            "TCPServer_Destroy failed: connection ID %d; socket %d.", 
            g_server.connID, g_server.socket );
		CommsError_SetLast( COMMS_ERROR_PARAM_INVALID, g_MODULE_NAME ); 
        return ( MV_FAILURE );
    }

	/* START CRITICAL SECTION - if we can't enter, don't continue 
	    Destroy shouldn't block. */
	//if ( TryEnterCriticalSection( &( g_server.criticalSection ) ) == 0 )
	//{
	//	CommsError_SetLast( COMMS_ERROR_CONNECTION_BUSY, g_MODULE_NAME );
	//	return ( MV_FAILURE );
	//}

    if ( g_server.isStarted )
    {
        MVInt   result;
        BOOL    isOK;
        DWORD   waitResult;
       
        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Stopping server..." );        

        /* Close the server socket (causes accept() to return) */
        result = closesocket( g_server.socket );
        if ( result == SOCKET_ERROR )
        {
            CommsError_SetLastWin( COMMS_ERROR_SERVER_STOP, 
                WSAGetLastError(), g_MODULE_NAME );
            status = MV_FAILURE;
        }

        /* Tell the server thread that it should exit and wait for it to do so */
        g_isTerminating = MV_TRUE;

		isOK = SetEvent( g_hTerminateEvent );
		if ( isOK == FALSE )
        {
            CommsError_SetLastWin( COMMS_ERROR_SERVER_STOP, 
                GetLastError(), g_MODULE_NAME );
            status = MV_FAILURE;
        }

		if ( status == MV_SUCCESS )
		{
			waitResult = WaitForSingleObject( 
				g_server.hThread, g_TERMINATE_TIMEOUT ); 

			if ( waitResult == WAIT_FAILED )
			{
				CommsError_SetLastWin( COMMS_ERROR_SERVER_STOP, 
					GetLastError(), g_MODULE_NAME );
				status = MV_FAILURE;
			}

			if ( waitResult == WAIT_TIMEOUT )
			{
				CommsError_SetLastWin( COMMS_ERROR_SERVER_STOP, 
					GetLastError(), g_MODULE_NAME );
				status = MV_FAILURE;
			}

			isOK = ResetEvent( g_hTerminateEvent );
			if ( isOK == FALSE )
			{
				CommsError_SetLastWin( COMMS_ERROR_SERVER_STOP, 
					GetLastError(), g_MODULE_NAME );
				status = MV_FAILURE;
			}
		}

        isOK = CloseHandle( g_hTerminateEvent );
        if ( isOK == FALSE )
        {
            CommsError_SetLastWin( COMMS_ERROR_SERVER_STOP, 
                GetLastError(), g_MODULE_NAME );
            status = MV_FAILURE;
        }

        result = WSACleanup();
        if ( result == SOCKET_ERROR )
        {
            CommsError_SetLastWin( COMMS_ERROR_DEINIT, 
                WSAGetLastError(), g_MODULE_NAME );
            status = MV_FAILURE;
        }

        if ( status == MV_SUCCESS )
        {
			g_server.isStarted = MV_FALSE;
            TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Server stopped." );
        }

    } /* if ( g_server.isStarted ) */

	/* END CRITICAL SECTION */
    //LeaveCriticalSection( &( g_server.criticalSection ) );

	/* Delete the critical section for each connection (once only) */
    //DeleteCriticalSection( &( g_server.criticalSection ) );

    return ( status );

} /* TCPServer_Destroy */


MVSize TCPServer_GetEndPoint( char * endPointStr, MVSize endPointLenMax )
{
    MVSize endPointLen = 0;

    TRACE_ASSERT( g_server.isStarted, "Server hasn't been started." );
    TRACE_ASSERT( g_server.socket != INVALID_SOCKET, "Socket is invalid." );
    TRACE_ASSERT_VALID_ADDRESS( endPointStr, "No end point to set." );

    if ( endPointLenMax >= xg_TCP_END_POINT_LEN_MAX )
    {
        sprintf( endPointStr, "%s:%s", g_server.endPoint.ipAddress,
            g_server.endPoint.port );

        endPointLen = strlen( endPointStr );
    }

    return ( endPointLen );

} /* TCPServer_GetEndPoint */
