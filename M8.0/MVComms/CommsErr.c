/*
    Name:           CommsErr.c
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Provides functionality for handling communications
					errors in the Serial and TCP modules.
					The last error only is stored, including:
					- A unique identifier for the error
					- A message string for the error
					  This comes from a resource file, and 
					  is suitable for display to the user.
					  Where a Windows error code occurred,
					  the Windows error message is appended.

                    Note: Error reporting functions should never assert in 
                    release builds - it's better for them to fail gracefully,
                    perhaps printing something to the trace log.
                    If an error reporting function fails it shouldn't 
                    stop the program.

*/
/*
	TODO:
*/

/* Include standard Windows headers */
#if defined (WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define HINSTANCE unsigned long*
#endif

#include <stdio.h> /* For sprintf() */

#include "MVTypes.h"
#include "MVTrace.h"
#include "MVDebug.h"
#include "MVMath.h"

#include "cmstypes.h"
#include "CommsErr.h"

#include "resource.h"

#define	g_MODULE_NAME			("CommsErr")

#define g_ERROR_MSG_LEN_MAX		(1023)

#define ThreadVar				__declspec( thread )

#define	g_RESOURCE_STRING_ERROR_MSG \
	("An error occurred but an error message could not be found.")

static HINSTANCE			g_hInstance;
static ThreadVar CommsError	g_lastError;
static ThreadVar WinError	g_lastErrorWin;

static MVBool		g_isInitialised = MV_FALSE;

static const MVInt g_errorStringIDs[] =
{
	IDS_COMMS_ERROR_NO_ERROR,
	IDS_COMMS_ERROR_OPEN,
	IDS_COMMS_ERROR_SEND,
	IDS_COMMS_ERROR_SEND_CLOSED,
	IDS_COMMS_ERROR_RECV,
	IDS_COMMS_ERROR_RECV_CLOSED,
	IDS_COMMS_ERROR_CLOSE,
	IDS_COMMS_ERROR_CLEAR_ERROR,
	IDS_COMMS_ERROR_END_POINT_INVALID,
	IDS_COMMS_ERROR_CONFIGURE,
	IDS_COMMS_ERROR_INIT,
	IDS_COMMS_ERROR_DEINIT,
	IDS_COMMS_ERROR_RECV_TIMEOUT,
	IDS_COMMS_ERROR_SEND_TIMEOUT,
	IDS_COMMS_ERROR_END_POINT_GET,
	IDS_COMMS_ERROR_SERVER_START,
	IDS_COMMS_ERROR_SERVER_LISTEN,
	IDS_COMMS_ERROR_SERVER_STOP,
	IDS_COMMS_ERROR_RECV_END,
	IDS_COMMS_ERROR_RECV_BLOCK,
	IDS_COMMS_ERROR_RECV_COUNT,
	IDS_COMMS_ERROR_CONNECT_TIMEOUT,
	IDS_COMMS_ERROR_SEND_END,
	IDS_COMMS_ERROR_PARAM_INVALID,
	IDS_COMMS_ERROR_CONNECTION_BUSY,
	IDS_COMMS_ERROR_INVALID_OPERATION
};


#if defined (_TRACE)
    static void CommsError_Output_TRACE( CommsError, WinError, const char * );
#else
	#define	CommsError_Output_TRACE( error, errorWin, source )
#endif

void CommsError_Initialise( void )
{
	if ( !g_isInitialised )
	{
        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Initialising error logger..." );

		g_hInstance = GetModuleHandle( "MVComms.DLL" );

		g_lastError = COMMS_ERROR_NO_ERROR;
		g_lastErrorWin = xg_WIN_ERROR_INVALID;

		g_isInitialised = MV_TRUE;

        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Error logger initialised." );
	}

} /* CommsError_Initialise */


void CommsError_SetLast( CommsError error, const char * source )
{
	CommsError_SetLastWin( error, xg_WIN_ERROR_INVALID, source );

} /* CommsError_SetLast */


void CommsError_SetLastWin( CommsError error, 
						   WinError errorWin, const char * source )
{
	DEBUG_ASSERT( MATH_IS_IN_RANGE( error, 
        COMMS_ERROR_FIRST, COMMS_ERROR_LAST ) );

	g_lastError = error;
	g_lastErrorWin = errorWin;

    if ( source != NULL )
    {
	    CommsError_Output_TRACE( g_lastError, g_lastErrorWin, source );
    }

} /* CommsError_SetLastWin */


#if defined (_TRACE)
static void CommsError_Output_TRACE( CommsError error, 
                                    WinError errorWin, const char * source )
{
	UNUSED(error);
	UNUSED(errorWin);

	char errorMsg[ g_ERROR_MSG_LEN_MAX + 1 ];

	CommsError_BuildMessage( g_lastError, g_lastErrorWin, 
        errorMsg, sizeof( errorMsg ) );

	TRACE_WRITE3_IF( LEVEL_ERROR, source, 
        "%s\nError codes: comms %d, windows %d.",
		errorMsg, g_lastError, g_lastErrorWin );

	/* Stop the program, but only in debug builds */
	DEBUG_ASSERT( MV_FALSE );

} /* CommsError_Output_TRACE */
#endif /* (_TRACE) */


CommsError CommsError_GetLast( void )
{
	return ( g_lastError );
}


WinError CommsError_GetLastWin( void )
{
	return ( g_lastErrorWin );
}


MVSize CommsError_BuildMessage( CommsError error, WinError errorWin,
                               char * msgBuf, MVSize msgBufLenMax )
{
	MVSize messageLen;

	DEBUG_ASSERT_VALID_ADDRESS( msgBuf );
	DEBUG_ASSERT( msgBufLenMax > 0 );
    if ( msgBuf == NULL || msgBufLenMax <= 0 )
    {
        TRACE_WRITE0_IF( LEVEL_ERROR, g_MODULE_NAME,
            "CommsError_BuildMessage passed an invalid buffer." );
        return 0;
    }

	DEBUG_ASSERT( MATH_IS_IN_RANGE( error, 
        COMMS_ERROR_FIRST, COMMS_ERROR_LAST ) );

    /* If the error ID is valid, get the message from the resource file. */
    if ( MATH_IS_IN_RANGE( error, COMMS_ERROR_FIRST, COMMS_ERROR_LAST ) )
    {	    
	    messageLen = LoadString( g_hInstance, g_errorStringIDs[ error ], msgBuf, (int)msgBufLenMax );
    }
    else
    {
        messageLen = 0;
        SetLastError( ERROR_INVALID_PARAMETER );
    }

	/* If the message could not be found, put in a default error message */
	if ( messageLen == 0 )
	{
		MVError_BuildDefaultMessage( msgBuf, msgBufLenMax, GetLastError() );
	}

	/* Add any Windows error message */
	if ( errorWin != xg_WIN_ERROR_INVALID
		&& msgBufLenMax > messageLen + 2 ) /* + 2 - null and new line */
	{	
		strncat( msgBuf, "\n", 1 );
		messageLen++;

		MVError_DecodeWin( errorWin,
			msgBuf + messageLen, msgBufLenMax - messageLen );
	}

	return ( strlen( msgBuf ) );

} /* CommsError_BuildMessage */


/* Functions that can be used to carry out checks when publically
accessible functions are called.  As MVComms.dll is used
by other programs, the error checking should be more rigorous */

MVBool CommsError_CheckInitialised( 
    const char * functionName, MVBool isInit )
{    
    if ( !isInit )
    {
        char assertMsg[256];

        CommsError_SetLast( COMMS_ERROR_INVALID_OPERATION, functionName ); 

        sprintf( assertMsg, "%s failed: module not initialised.", functionName );        
        TRACE_ASSERT( isInit, assertMsg );
        
        return ( MV_FALSE );
    }

    return ( MV_TRUE );

} /* CommsError_CheckInitialised */


MVBool CommsError_CheckConnectionID( 
    const char * functionName, ConnectionID connID )
{    
    if ( !MATH_IS_IN_RANGE( connID, 0, CONNECTION_ID_COUNT - 1 ) )
    {
        char assertMsg[256];

        CommsError_SetLast( COMMS_ERROR_PARAM_INVALID, functionName ); 

        sprintf( assertMsg, "%s failed: the connection ID was invalid.", functionName );
        TRACE_ASSERT( MATH_IS_IN_RANGE( connID, 0, CONNECTION_ID_COUNT - 1 ),
		    assertMsg );
        
        return ( MV_FALSE );
    }

    return ( MV_TRUE );

} /* CommsError_CheckConnectionID */


MVBool CommsError_CheckBytes( 
    const char * functionName, const MVByte * p_bytes )
{    
    if ( p_bytes == NULL )
    {
        char assertMsg[256];

        CommsError_SetLast( COMMS_ERROR_PARAM_INVALID, functionName ); 

        sprintf( assertMsg, "%s failed: invalid byte buffer.", functionName );
        TRACE_ASSERT_VALID_ADDRESS( p_bytes, assertMsg );
        
        return ( MV_FALSE );
    }

    return ( MV_TRUE );

} /* CommsError_CheckBytes */


MVBool CommsError_CheckBytesCount( 
    const char * functionName, MVInt bytesCount )
{    
    if ( bytesCount <= 0 )
    {
        char assertMsg[256];

        CommsError_SetLast( COMMS_ERROR_PARAM_INVALID, functionName ); 

        sprintf( assertMsg, "%s failed: invalid number of bytes.", functionName );
        TRACE_ASSERT( bytesCount > 0, assertMsg );
        
        return ( MV_FALSE );
    }

    return ( MV_TRUE );

} /* CommsError_CheckBytesCount */


MVBool CommsError_CheckEndPoint( 
    const char * functionName, const char * endPoint )
{    
    if ( endPoint == NULL )
    {
        char assertMsg[256];

        CommsError_SetLast( COMMS_ERROR_PARAM_INVALID, functionName ); 

        sprintf( assertMsg, "%s failed: invalid end point.", functionName );
        TRACE_ASSERT_VALID_ADDRESS( endPoint, assertMsg );
        
        return ( MV_FALSE );
    }

    return ( MV_TRUE );

} /* CommsError_CheckEndPoint */


MVBool CommsError_CheckConnectionClosed( 
    const char * functionName, MVBool isConnected )
{    
    if ( isConnected )
    {
        char assertMsg[256];

        CommsError_SetLast( COMMS_ERROR_INVALID_OPERATION, functionName ); 

        sprintf( assertMsg, "%s failed: connection already open.", functionName );
        TRACE_ASSERT( !isConnected, assertMsg );
        
        return ( MV_FALSE );
    }

    return ( MV_TRUE );

} /* CommsError_CheckConnectionClosed */