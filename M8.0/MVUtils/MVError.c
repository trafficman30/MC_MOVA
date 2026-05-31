/*
    Name:           MVError.c
    Author (Date):  ihenderson, 29/11/05
    Platform:       PC
    Purpose:        Common error handling functionality

                    Note: Error reporting functions should never assert in 
                    release builds - it's better for them to fail gracefully,
                    perhaps printing something to the trace log.
                    If an error reporting function fails it shouldn't 
                    stop the program.
*/


/* Include standard Windows headers */
#if defined (WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <string.h>
#endif

#include "MVError.h"
#include "MVTrace.h"
#include "MVDebug.h"

#define	g_MODULE_NAME   ("MVError")

#define	g_RESOURCE_STRING_ERROR_MSG \
	("An error occurred but an error message could not be found.")


MVSize MVError_DecodeWin( WinError errorWin, 
                         char * msgBuf, MVSize msgBufLenMax )
{
    MVSize messageLen;

#if defined (WIN32)
	DEBUG_ASSERT_VALID_ADDRESS( msgBuf );
	DEBUG_ASSERT( msgBufLenMax > 0 );	
    DEBUG_ASSERT( errorWin != xg_WIN_ERROR_INVALID );

    messageLen = FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | 
        FORMAT_MESSAGE_IGNORE_INSERTS |
        FORMAT_MESSAGE_MAX_WIDTH_MASK,
        NULL, 
        errorWin, 
        MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
        msgBuf, 
        (DWORD)msgBufLenMax, 
        NULL );
#else
	strncpy(msgBuf, strerror(errorWin), msgBufLenMax - 1);
	msgBuf[msgBufLenMax - 1] = 0;
	messageLen = strlen(msgBuf);
#endif

    return ( messageLen );

} /* MVError_DecodeWin */


/*
    Name:           MVError_BuildDefaultMessage
    Author (Date):  ihenderson, 30/11/05
    Platform:       PC
    Purpose:        Copies a default message to the given
					buffer.  Intended for use when a string
					couldn't be retrieved from a resource file.
    Inputs:         Message buffer
					Size of message buffer
					Optional Windows error code
					(e.g. as returned by GetLastError() 
					following failure to load resource string
					using LoadString()).
    Returns:        The number of characters copied to the buffer.
    Assumptions:    
    Effects:        
*/
MVSize MVError_BuildDefaultMessage( char * msgBuf, MVSize msgBufLenMax, 
								 WinError resError )
{
	MVSize resErrorMsgLen;

	DEBUG_ASSERT_VALID_ADDRESS( msgBuf );
	DEBUG_ASSERT( msgBufLenMax > 0 );
    if ( msgBuf == NULL || msgBufLenMax <= 0 )
    {
        TRACE_WRITE0_IF( LEVEL_ERROR, g_MODULE_NAME,
            "MVError_BuildDefaultMessage passed an invalid buffer." );
        return 0;
    }

	resErrorMsgLen = strlen( g_RESOURCE_STRING_ERROR_MSG );

	if ( msgBufLenMax > resErrorMsgLen + 2 ) /* + 2 - null char and end line */
	{
		strncpy( msgBuf, g_RESOURCE_STRING_ERROR_MSG, resErrorMsgLen );
		msgBuf[ resErrorMsgLen ] = '\0';
    	
		if ( resError != xg_WIN_ERROR_INVALID )
		{
			msgBuf[ resErrorMsgLen ] = '\n';
			resErrorMsgLen++;
			MVError_DecodeWin( resError, 
				msgBuf + resErrorMsgLen, msgBufLenMax - resErrorMsgLen );
		}
	}
	else
	{
		strncpy( msgBuf, g_RESOURCE_STRING_ERROR_MSG, msgBufLenMax );
        msgBuf[ msgBufLenMax - 1 ] = '\0';
	}

	return ( strlen( msgBuf ) );

} /* MVError_BuildDefaultMessage */
