/*
    Name:           CliComms.c
    Author (Date):  ihenderson, 14/11/05
    Platform:       PC
    Purpose:        Wrapper for MOVA Comm communications
					to MOVA, suitable for calling from
					Fortran code.

					N.B. MOVA Comm is currently single-threaded - 
					this may change in future, as there are 
					a number of issues. Ideally the communications
					should be done in a separate thread to the 
					Windows GUI.  If MOVA Comm becomes 
					multithreaded, we will need to make sure CliComms
					is thread-safe.

*/

/*
    TODO:
    
    May want to provide multiple byte Read/Write functions
    - easy to do, may improve performance if this is an issue.

    Should provide a GetLastError() function to retrieve Windows
    error values/error strings?

	Ideally, should check for errors in parameters + return failure 
	- this module is a "barricade".  ATM, doesn't check 
	parameters - Serial and TCP modules will assert if they 
	get bad params.
	Probably not necessary though as module 
	should only be used within TRL (for MOVA Comm/MOVA Sim).
*/

/*   
  SLINK command line for linking in this module:

  SLINK MVComms.dll ... e.g. - TestClientComms.obj -OUT:TestClientComms
*/


//#define BUG_10073_TEST
#if defined (BUG_10073_TEST)
    #include <time.h>   /* For getting system time */
    #include <stdio.h>  /* For sprintf */
    #include <string.h> /* For strlen */
#endif


#include "MVDebug.h"
#include "MVTrace.h"

#include "TCPClient.h"
#include "Serial.h"
#include "CommsErr.h"

#include "clicomms.h"

/* Trace defines */
#define	g_MODULE_NAME		("ClientComms")

// Name of the config file
// TODO: could make more use of this - currently just used for changing trace level
#define g_CONFIG_FILE_NAME  ("MOVAComm.exe.config")

#define g_TRACE_FILE_NAME   ("tracecomms.log")

typedef enum
{
        COMMS_TYPE_INVALID = -1,
    
        COMMS_TYPE_FIRST,
    COMMS_TYPE_TCP = COMMS_TYPE_FIRST,
    COMMS_TYPE_SERIAL,
        COMMS_TYPE_LAST = COMMS_TYPE_SERIAL,

        COMMS_TYPE_COUNT

} CommsType;


static CommsType	g_commsType;
static MVBool		g_isConnected = MV_FALSE;
static MVBool		g_isErrorState = MV_FALSE;


static MVStatus (*ClientComms_WriteByteAsByte)( ConnectionID, MVByte );
static MVStatus (*ClientComms_ReadByteAsByte)( ConnectionID, MVByte * );


MVInt32 ClientComms_OpenTCP( const char * ipEndPoint, MVInt32 n32TimeOut )
{
	MVStatus	status = MV_FAILURE;	   

	if ( !g_isConnected )
	{
		UNUSED(n32TimeOut);
        TRACE_INITIALISE( g_CONFIG_FILE_NAME, g_TRACE_FILE_NAME );
        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Opening TCP communications..." );

        //TRACE_ASSERT( MV_FALSE, "Testing assertion in release builds for MOVA Comm." );

        status = TCPClient_Initialise( NULL, NULL );
        
        /* Time-outs not used for MOVA Comm TCP -   
        Blocking sockets can have minimum timeouts of 
        500ms - too slow for MOVA Comm, which isn't multithreaded. */
        //if ( status == MV_SUCCESS )
        //{
        //    status = TCPClient_SetTimeouts( CONNECTION_ID_USER, 
        //        (MVInt)n32TimeOut, (MVInt)n32TimeOut );
        //}

        /* Create a non-blocking socket for MOVA Comm. */
        if ( status == MV_SUCCESS )
        {            
            status = TCPClient_Connect( CONNECTION_ID_USER, 
                ipEndPoint, MV_FALSE );
		}			

		if ( status == MV_SUCCESS )
		{
		    ClientComms_WriteByteAsByte = &TCPClient_WriteByte;
			ClientComms_ReadByteAsByte = &TCPClient_ReadByte;

			g_isConnected = MV_TRUE;
            g_commsType = COMMS_TYPE_TCP;

            // Reset the comms error flag
            g_isErrorState = MV_FALSE;

			TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "TCP communications opened." );
		}
	}

	return ( status == MV_SUCCESS ? 1 : 0 );

} /* ClientComms_OpenTCP */


MVInt32    ClientComms_OpenSerial(	MVInt32 n32CommPortNum, 
								  MVInt32 n32BaudRate, 
								  MVInt32 n32ByteSize, 
								  MVInt32 n32Parity,
								  MVInt32 n32StopBits,
								  MVInt32 n32TimeOut )
{
	MVStatus	status = MV_FAILURE;

	if ( !g_isConnected )
	{
        TRACE_INITIALISE( g_CONFIG_FILE_NAME, g_TRACE_FILE_NAME );
        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Opening serial communications..." );

        status = Serial_Initialise( NULL, NULL );	

        if ( status == MV_SUCCESS )
        {
            status = Serial_SetTimeouts( CONNECTION_ID_USER, 
                (MVUInt32)n32TimeOut, (MVUInt32)n32TimeOut,
                (MVUInt32)n32TimeOut, (MVUInt32)n32TimeOut );
        }

        if ( status == MV_SUCCESS )
        {	
            status = Serial_OpenPort( CONNECTION_ID_USER,
                (MVByte)n32CommPortNum, (MVUInt32)n32BaudRate,
                (MVByte)n32ByteSize, (MVByte)n32Parity, (MVByte)n32StopBits );
        }

		if ( status == MV_SUCCESS )
		{
		    ClientComms_WriteByteAsByte = &Serial_WriteByte;
		    ClientComms_ReadByteAsByte = &Serial_ReadByte;

			g_isConnected = MV_TRUE;
            g_commsType = COMMS_TYPE_SERIAL;

			TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Serial communications opened." );
		}
    }

    return ( status == MV_SUCCESS ? 1 : 0 );

} /* ClientComms_OpenSerial */			


#if defined (BUG_10073_TEST)
static MVSize GetDateTimeNow( char * dateTimeBuffer )
{
    struct tm * timeNow;
    time_t      clock;

	/* Get time in seconds */
    time( &clock );

	/* Convert time to a struct */
    timeNow = localtime( &clock );
   
    /* Print local time as a string (exactly 26 chars length) */
	sprintf( dateTimeBuffer, "%s", asctime( timeNow ) );

	return ( strlen( dateTimeBuffer ) );

} /* GetDateTimeNow */
#endif


MVInt32    ClientComms_WriteByte(  MVInt32 byteAsLong )
{
    MVStatus status = MV_FAILURE;


#if defined (BUG_10073_TEST)

    #define LINE_FEED       (10)
    #define CARRIAGE_RETURN (13)

    if ( byteAsLong == CARRIAGE_RETURN || byteAsLong == LINE_FEED )
    {
        char dateTime[ 32 ];
        MVSize dateTimeLen = GetDateTimeNow( dateTime );

        /* Remove the new line from the end of the date/time string */
        dateTime[ dateTimeLen - 1 ] = '\0';

        TRACE_WRITE2( g_MODULE_NAME, "Writing %s at time %s.", 
            ( byteAsLong == CARRIAGE_RETURN ? "CR" : "LF" ), 
            dateTime );
    }
#endif /* BUG_10073_TEST */


    if ( !g_isErrorState )
    {    
        MVByte byte = (MVByte)byteAsLong;

	    status = ClientComms_WriteByteAsByte( 
		    CONNECTION_ID_USER, byte );

    } // If no error

	return ( status == MV_SUCCESS ? 1 : 0 );

} /* ClientComms_WriteByte */


MVInt32    ClientComms_ReadByte(   MVInt32 * p_byteAsLong )
{	
    MVStatus status = MV_FAILURE;

    if ( !g_isErrorState )
    {    
        MVByte byte;

	    status = ClientComms_ReadByteAsByte( 
		    CONNECTION_ID_USER, &byte );

	    if ( status == MV_SUCCESS )
	    {
		    (*p_byteAsLong) = (MVInt32)byte;
        }

        else
        {
            CommsError lastError = CommsError_GetLast();

            // If the serial port is at end of file (no more data ATM)
            if ( g_commsType == COMMS_TYPE_SERIAL 
                && lastError == COMMS_ERROR_RECV_END )
            {
                // No error - return 0 - allows MOVA Comm to retry
                (*p_byteAsLong) = 0;
                status = MV_SUCCESS;
            }

            // If we didn't receive because there was nothing immediately available 
            // (using a non-blocking socket for MOVA Comm)
            if ( g_commsType == COMMS_TYPE_TCP
                && lastError == COMMS_ERROR_RECV_BLOCK )
            {
                // No error - return 0 - allows MOVA Comm to retry
                (*p_byteAsLong) = 0;
                status = MV_SUCCESS;
            }
        }

    } // If no error

    return ( status == MV_SUCCESS ? 1 : 0 );

} /* ClientComms_ReadByte */


MVInt32 ClientComms_ClearError( void )
{
    MVStatus status;

	TRACE_WRITE0_IF( LEVEL_ERROR, g_MODULE_NAME, 
		"Trying to clear a communications error." );

	// If we are using serial comms, attempt to clear the error
    if ( g_commsType == COMMS_TYPE_SERIAL )
    {
        status = Serial_ClearError( CONNECTION_ID_USER );
    }

    // Otherwise (TCP) the error cannot be recovered from
    else
    {
        status = MV_FAILURE;
    }

    // The connection is in an irrecoverable error state
    // Set a flag so that subsequent read/writes fail immediately
    // until the user has disconnected and reconnected.
    if ( status == MV_FAILURE )
    {
        g_isErrorState = MV_TRUE;
    }
    
    return ( status == MV_SUCCESS ? 1 : 0 );

} /* ClientComms_ClearError */


MVInt32 ClientComms_GetErrorMessage( char * message, MVInt32 messageLenMax )
{
    MVSize messageLen = CommsError_BuildMessage( 
		CommsError_GetLast(), 
		CommsError_GetLastWin(), 
		message, 
		messageLenMax );

    return ( (MVInt32)messageLen );

} /* ClientComms_GetErrorMessage */


MVInt32    ClientComms_Close( void )
{
    MVStatus status;

	TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Closing communications..." );

	switch ( g_commsType )
	{
		case COMMS_TYPE_TCP:
		{
			status = TCPClient_DeInitialise( );
			break;
		}
		case COMMS_TYPE_SERIAL:
		{
			status = Serial_DeInitialise( );
			break;
		}
		default:
		{
            status = MV_FAILURE;
			break;
		}
	}

    if ( status == MV_SUCCESS )
    {
        g_isConnected = MV_FALSE;
        g_isErrorState = MV_FALSE;
	    TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Communications closed." );
    }
    
    TRACE_DEINITIALISE( );

    return ( status == MV_SUCCESS ? 1 : 0 );

} /* ClientComms_Close */
