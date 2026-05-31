/*
    Name:           PCUserIO.c
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Manages PCMOVA communications with a terminal
                    program, e.g. MOVA Comm.
                    PCUserIO is analogous to PCContIO, but handles
                    user input/output rather than controller I/O.
                    It provides PC implementations of SendStringToLH and 
                    GetMOVACharFromHandset, which process user I/O
                    according to the CommsType (TCP/Serial); these
                    functions are currently included in the MOVA kernel
                    via proj_def.h.

                    The underlying TCP or Serial I/O is carried out using 
                    the TCPClient and Serial modules in the MVComms 
                    communications DLL.

                    Synchronisation Note:
                    SendStringToLH and GetMOVACharFromHandset are called
                    from MOVA kernel user IO threads (term/TRLUserIF/output).
                    These functions are re-entrant - the Serial or TCP
                    functions they call deal with synchronisation issues.

*/

/*
    TODO: (non-urgent) Serial errors - need to call the ClearError() function
*/


#include "stdafx.h"

#include "PCUserIO.h"

#include "Serial.h"
#include "TCPServer.h"
#include "TCPClient.h"
#include "CommsErr.h"
#include "kdebug.h"

#define g_MODULE_NAME       ("PCUserIO")


static MVBool       g_isInitialised = MV_FALSE;
static CommsType    g_commsType = COMMS_TYPE_INVALID;


static void PCUserIO_SetLastError( PCError, MVBool );

extern void SendStringToLH(const char *sOP_String, int nCharsInString);

/*
    Name:           PCUserIO_Initialise
    Author (Date):  ihenderson, 12/11/05
    Platform:       PC
    Purpose:        Starts listening for user connections using
                    the given communications type.
    Inputs:         Communications type (e.g. Serial, TCP)
                    End point to use for comms (may be NULL)
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Remarks:        TCP connections for user I/O are non-blocking - 
                    the MOVA Kernel I/O tasks assume this by the way
                    they currently operate - e.g. calling GetHandsetChar(), 
                    doing some stuff, then sleeping.  It might be possible 
                    to change this in future - blocking sockets would 
                    probably be preferable.
*/
MVStatus PCUserIO_Initialise( CommsType userComms, const char * userEndPoint )
{
    MVStatus    status = MV_SUCCESS;
    
    TRACE_ASSERT( !g_isInitialised, "PC User IO module already initialised." );
    TRACE_ASSERT( MATH_IS_IN_RANGE( userComms, COMMS_TYPE_FIRST, COMMS_TYPE_LAST ),
        "Communications type is invalid." );
    
    TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Initialising user IO..." );
    
    g_commsType = userComms;

    /* Start listening for a user connection according to 
    the requested comms type */
    switch ( userComms )
    {
        case COMMS_TYPE_TCP:
        {
            /* Initialise the TCPClient module, passing it PCMOVA tracing
            information (file name, and trace level via the config file). */
            char traceFileName[16];
            status = TCPClient_Initialise( xg_APP_CONFIG_FILE_NAME,
                TRACE_GET_FILE_NAME( traceFileName, sizeof( traceFileName ) ) );

            if ( status == MV_SUCCESS )
            {   
                /* Create a server for listening for user connection requests
                (PCMOVA sockets for user comms are non-blocking) */
                status = TCPServer_Create( 
                    CONNECTION_ID_USER, userEndPoint, MV_FALSE );
            }

            break;
        }
        
        case COMMS_TYPE_SERIAL:
        {
            char traceFileName[16];

            TRACE_WRITE0( g_MODULE_NAME, 
                "Warning: Serial comms to user not fully supported." );

            /* Initialise the Serial module, passing it PCMOVA tracing
            information (file name, and trace level via the config file). */
            status = Serial_Initialise( xg_APP_CONFIG_FILE_NAME,
                TRACE_GET_FILE_NAME( traceFileName, sizeof( traceFileName ) ) );

            if ( status == MV_SUCCESS )
            {
                // TODO: Timeouts?  These are the user timeouts used in the prototype
                status = Serial_SetTimeouts( CONNECTION_ID_USER, 10, 1000, 0, 0 );
            }

            if ( status == MV_SUCCESS )
            {   
                // TODO: Fixed COM port settings - should read from the command line
                status = Serial_OpenPort( CONNECTION_ID_USER,
                    1,      // Comm port 
                    57600,  // Baud rate
                    7,      // Data bits
                    EVENPARITY,
                    ONESTOPBIT );
            }

            break;
        }
        default:
        {
            g_commsType = COMMS_TYPE_INVALID;
            status = MV_FAILURE;
            TRACE_ASSERT( MV_FALSE, "Communications type is invalid." );
            break;
        }   

    } /* switch ( g_commsType ) */                 

    if ( status == MV_SUCCESS )
    {
        g_isInitialised = MV_TRUE;
        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
            "User IO initialised." );
    }
    else
    {
        PCUserIO_SetLastError( PCMOVA_ERROR_USER_INIT, MV_TRUE );
    }

    return ( status );

} /* PCUserIO_Initialise */


/*
    Name:           PCUserIO_DeInitialise
    Author (Date):  ihenderson, 12/11/05
    Platform:       PC
    Purpose:        Deinitialises user communications
                    and frees any associated resources.
    Inputs:         
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Remarks:
*/
MVStatus PCUserIO_DeInitialise( void )
{
    MVStatus    status = MV_SUCCESS;

    if ( g_isInitialised )
    {
        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
            "Deinitialising user IO..." );        

        switch ( g_commsType )
        {
            case COMMS_TYPE_TCP:
            {
                /* Stop listening for new connections */
                status = TCPServer_Destroy();
                DEBUG_ASSERT( status != MV_FAILURE );

                /* Disconnect any client currently connected (MOVA Comm) */
                status = TCPClient_Disconnect( CONNECTION_ID_USER );
                DEBUG_ASSERT( status != MV_FAILURE );

                status = TCPClient_DeInitialise();
                DEBUG_ASSERT( status != MV_FAILURE );

                break;
            }
                
            case COMMS_TYPE_SERIAL:
            {   
                /* Close the serial port */
                status = Serial_ClosePort( CONNECTION_ID_USER );
                DEBUG_ASSERT( status != MV_FAILURE );

                status = Serial_DeInitialise();
                DEBUG_ASSERT( status != MV_FAILURE );

                break;
            }
            default:
            {
                status = MV_FAILURE;
                DEBUG_ASSERT( MV_FALSE );
                break;
            }   

        } /* switch ( g_commsType ) */     

        if ( status == MV_SUCCESS )
        {
            g_isInitialised = MV_FALSE;
            TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
                "User IO deinitialised." );
        }
        else
        {
            PCUserIO_SetLastError( PCMOVA_ERROR_USER_DEINIT, MV_TRUE );
        }

    } /* If initialised */

    return ( status );

} /* PCUserIO_DeInitialise */


/*
    Name:           PCUserIO_GetEndPoint
    Author (Date):  ihenderson, 12/11/05
    Platform:       PC
    Purpose:        Returns the end point used for user
                    communications.
    Inputs:         String to fill with the user endpoint
                    Should be xg_END_POINT_LEN_MAX characters.
    Returns:        The number of characters in the end point
                    or 0 if the end point couldn't be retrieved.
    Remarks:
*/
MVSize PCUserIO_GetEndPoint( char * endPointStr, MVSize endPointLenMax )
{
    MVSize endPointLen = 0;

    TRACE_ASSERT( g_isInitialised, "User IO not initialised." );
    TRACE_ASSERT_VALID_ADDRESS( endPointStr, "No end point to set." );
    
    switch ( g_commsType )
    {
        case COMMS_TYPE_SERIAL:
        {
            // TODO: Serial - user end point support
            TRACE_ASSERT( MV_FALSE, 
                "Serial user comms endpoint currently unavailable." );
            break;
        }

        case COMMS_TYPE_TCP:
        {
            endPointLen = TCPServer_GetEndPoint( endPointStr, endPointLenMax );
            break;
        }

        default:
        {
            TRACE_ASSERT( MV_FALSE, "Communications type is invalid." );
            break;
        }
    }
    
    return ( endPointLen );

} /* PCUserIO_GetEndPoint */


/*
    Name:           PCUserIO_IsConnected
    Author (Date):  ihenderson, 12/11/05
    Platform:       PC
    Purpose:        Returns whether a user connection currently 
                    exists/is open.
    Inputs:         
    Returns:        True if connection exists; False otherwise..
    Remarks:        A TCP connection is only active if MOVA Comm or 
                    another terminal emulator is connected.
                    The Serial port is open regardless of whether
                    a terminal emulator is connected.
*/
MVBool PCUserIO_IsConnected( void )
{
    MVBool isConnected;

    switch ( g_commsType )
    {
        case COMMS_TYPE_SERIAL:
        {
            isConnected = Serial_IsOpen( CONNECTION_ID_USER );
            break;
        }

        case COMMS_TYPE_TCP:
        {
            isConnected = TCPClient_IsConnected( CONNECTION_ID_USER );
            break;
        }

        default:
        {
            isConnected = MV_FALSE;
            TRACE_ASSERT( MV_FALSE, "Communications type is invalid." );
            break;
        }
    }

    return ( isConnected );

} /* PCUserIO_IsConnected */



  /*
  Name:           PCUserIO_Disconnect
  Author (Date):  iabdelhalim, 20/10/2016
  Platform:       PC
  Purpose:        Disconnect the connection
  */
void PCUserIO_Disconnect(void)
{	
	switch (g_commsType)
	{
	
		case COMMS_TYPE_SERIAL:
		{
			Serial_ClosePort(CONNECTION_ID_USER);
			break;
		}
		case COMMS_TYPE_TCP:
		{
			TCPClient_Disconnect(CONNECTION_ID_USER);
			break;
		}

		default:
		{
			TRACE_ASSERT(MV_FALSE, "Communications type is invalid.");
			break;
		}
	}

} /* PCUserIO_Disconnect */


/*
    Name:           GetMOVACharFromHandset
    Author (Date):  ihenderson, 12/11/05
    Platform:       PC
    Purpose:        Reads a character from the terminal user
                    communications program (e.g. MOVA Comm).
                    Called by the MOVA kernel functions
                    GetHandsetChar and GetHandsetCharNE in Comms.c
    Inputs:         
    Returns:        Character read from the terminal comms
                    program, or 0 (null character) if 
                    a character could not be read.
    Assumptions:    That a connection to the user comms
                    program has been established.
    Effects:        
*/
char GetMOVACharFromHandset( void  )
{
    MVByte     byte = 0;
    MVStatus   status;
    char       character;

    //DEBUG_WRITE0( "Reading from local handset." );

    switch ( g_commsType )
    {
        case COMMS_TYPE_SERIAL:
        {
            status = Serial_ReadByte( CONNECTION_ID_USER, &byte );
                        
            // TODO: (non-urgent) if the error wasn't EOF, we'll end up 
            // terminating MOVA.  Would it be better to try closing and
            // reopening the port?
            // Also, we should probably close + reopen the serial port 
            // if we've not received a response for a certain period of time, 
            // as with embedded MOVA.
            if ( status == MV_FAILURE )
            {
                // If the read failed because we were at the EOF (nothing to read)
                // stay connected.
                if ( CommsError_GetLast() == COMMS_ERROR_RECV_END )
                {
                    // No error 
                }
                else
                {
                    PCUserIO_SetLastError( PCMOVA_ERROR_USER_RECEIVE, MV_TRUE );
                }
            }
            
            break;
        }

        case COMMS_TYPE_TCP:
        {
            status = TCPClient_ReadByte( CONNECTION_ID_USER, &byte );
            
            if ( status == MV_FAILURE )
            {
                CommsError commsErrLast = CommsError_GetLast();

                // With user terminal comms, if there was nothing to receive
                // immediately (the socket is non-blocking), no error
                if ( commsErrLast == COMMS_ERROR_RECV_BLOCK )
                {
                    // No error
                }
                else
                {
                    // If the read failed because the connection
                    // was closed gracefully, or is already closed,
                    if ( commsErrLast == COMMS_ERROR_RECV_END
                        || commsErrLast == COMMS_ERROR_RECV_CLOSED )
                    {
                        // Ignore the error - this will happen because
                        // the MOVA kernel UI threads term and TRLUserIF
                        // may attempt to read/write a number of times
                        // before the MOVA kernel flag lLH_CONNECTED gets 
                        // set to FALSE
                    }
                    else
                    {
                        PCUserIO_SetLastError( PCMOVA_ERROR_USER_RECEIVE, MV_TRUE );
                    }

                    /* Disconnect so that new clients can connect */
                    TCPClient_Disconnect( CONNECTION_ID_USER );
                }
            }
            break;
        }

        default:
        {
            status = MV_FAILURE;
            TRACE_ASSERT( MV_FALSE, "Communications type is invalid." );
            break;
        }
    }

    if ( status == MV_SUCCESS )
    {
        character = (char)byte;

#ifdef IS_MOVACOMM_ENABLED 
		character = (char)( toupper( character ) );
#endif
    }
    else
    {
        character = 0;
    }

    return ( character );

} /* GetMOVACharFromHandset*/


MVStatus GetMsgFromMT_ReadBytes( MVByte * p_bytes, MVInt count )
{
    MVStatus status;
    MVInt   bytesRead;

    switch ( g_commsType )
    {
        case COMMS_TYPE_SERIAL:
        {
            status = Serial_ReadBytes( CONNECTION_ID_USER, 
                p_bytes, count, &bytesRead );
            break;
        }

        case COMMS_TYPE_TCP:
        {
            status = TCPClient_ReadBytes( CONNECTION_ID_USER, 
                p_bytes, count, &bytesRead );
            break;
        }

        default:
        {
            status = MV_FAILURE;
            TRACE_ASSERT( MV_FALSE, "Communications type is invalid." );
            break;
        }
    }

    if ( status == MV_FAILURE ) 
    {
        //PCContIO_SetLastError( PCMOVA_ERROR_CONTROLLER_RECEIVE );
    }

    return ( status );

} /* GetMsgFromWPF_ReadBytes */


/*
    Name:           SendStringToLH
    Author (Date):  ihenderson, 12/11/05
    Platform:       PC
    Purpose:        Sends a string of characters to the terminal
                    user communications program (e.g. MOVA Comm).
                    Called by various MOVA kernel functions
                    e.g. SendString in GetEntry.c, and WRITE in Write.c.
    Inputs:         The string to write
                    The number of characters to write from the string 
    Returns:        
    Assumptions:    That a connection to the user comms
                    program has been established.
    Effects:        
*/
void SendStringToLH( const char *sOP_String, int nCharsInString )
{
    MVStatus status;
    MVInt   bytesSent;

    switch ( g_commsType )
    {
        case COMMS_TYPE_SERIAL:
        {
            status = Serial_WriteBytes( CONNECTION_ID_USER, (MVByte *)sOP_String, 
                nCharsInString, &bytesSent );

            // If the write failed, stay connected (TODO: (non-urgent) Is this ok?)
            if ( status == MV_FAILURE )
            {
                PCUserIO_SetLastError( PCMOVA_ERROR_USER_SEND, MV_TRUE );
            }

            break;
        }

        case COMMS_TYPE_TCP:
        {
            status = TCPClient_WriteBytes( CONNECTION_ID_USER, (MVByte *)sOP_String, 
                nCharsInString, &bytesSent );

            // If the write failed, disconnect so that a new client can connect
            if ( status == MV_FAILURE )
            {
                CommsError commsErrLast = CommsError_GetLast();

                // If the write failed because the connection is already closed
                // or was shut down gracefully by the client app (MOVA Comm, Hyperterm)
                if ( commsErrLast == COMMS_ERROR_SEND_CLOSED
                    || commsErrLast == COMMS_ERROR_SEND_END )
                {
                    // Ignore the error - this will happen because
                    // the MOVA kernel UI threads term and TRLUserIF
                    // may attempt to read/write a number of times
                    // before the MOVA kernel flag lLH_CONNECTED gets 
                    // set to FALSE
                }
                else
                {
                    PCUserIO_SetLastError( PCMOVA_ERROR_USER_SEND, MV_TRUE );
                }

                TCPClient_Disconnect( CONNECTION_ID_USER );
            }
            break;
        }

        default:
        {
            status = MV_FAILURE;
            TRACE_ASSERT( MV_FALSE, "Communications type is invalid." );
            break;
        }
    }

} /* SendStringToLH */

/*
    Name:           SendStringToModem
    Author (Date):  ihenderson, 12/11/05
    Platform:       PC
    Purpose:        Does nothing - no user comms modem support
                    specified for PCMOVA.
    Inputs:         The string to write
                    The number of characters to write from the string 
    Returns:        
    Remarks:
*/
void SendStringToModem( const char *sOP_String, int nCharsInString )
{    
    /* Do nothing - PCMOVA does not support remote connections. */
	UNUSED(sOP_String);
	UNUSED(nCharsInString);

} /* SendStringToModem */


/*
    Name:           PCUserIO_SetLastError
    Author (Date):  ihenderson, 12/11/05
    Platform:       PC
    Purpose:        Sets the PCMOVA last error value and error message 
                    for user communications errors.
                    The error message is obtained from the communications
                    DLL (MVComms).
    Inputs:         The PCMOVA error code to use
                    Whether the error should be recorded in the trace log.
    Returns:        
    Remarks:        This function is re-entrant as different threads 
                    may call it at the same time.
*/
static void PCUserIO_SetLastError( PCError pcmovaError, MVBool isTrace )
{
    char commsErrorMsg[ xg_ERROR_MSG_LEN_MAX + 1 ];

    /* Get the message accompanying the last comms err */
    CommsError_BuildMessage( 
        CommsError_GetLast(), CommsError_GetLastWin(), 
        commsErrorMsg, sizeof( commsErrorMsg ) );

    /* Set the last error with the comms error message as the detail message */
    PCError_SetLast( pcmovaError, commsErrorMsg, 
        ( isTrace ? g_MODULE_NAME : NULL ) );

} /* PCUserIO_SetLastError */
