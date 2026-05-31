/*
    Name:           PCContIO.c
    Author (Date):  ihenderson, 14/10/05
    Platform:       PC
    Purpose:        Manages PCMOVA communications with the Controller
                    Initially, this takes place via TCP, but the
                    module has been designed to allow support
                    for Serial comms (or SNMP, etc.) to be added
                    at a later date.  The TCP or Serial I/O is carried out 
                    using the TCPClient and Serial modules in the MVComms 
                    communications DLL.

                    Synchronisation Note:
                    Send buffers local - Event messages can be sent by 
                    the MOVA kernel threads, e.g. TRLUserIF and term, 
                    as well as by the main thread. The buffer isn't too 
                    big for Windows, and putting the buffer on the stack 
                    makes these function re-entrant, saveing us from 
                    having to put in synchronisation (e.g. critical sections). 
                    (May want to do something similar with the receive buffer 
                    for consistency/safety in case other threads try to 
                    receive messages in future.)

    Last revision:  $Revision:: 4                           $
    Last changed:   $Date:: 12/04/06 16:07                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: pccontio.c $
    
    *****************  Version 4  *****************
    User: Ihenderson   Date: 12/04/06   Time: 16:07
    Updated in $/PCMOVA
    
    *****************  Version 3  *****************
    User: Ihenderson   Date: 16/12/05   Time: 10:15
    Updated in $/PCMOVA
    
    *****************  Version 2  *****************
    User: Ihenderson   Date: 5/12/05    Time: 11:02
    Updated in $/PCMOVA
    
    *****************  Version 1  *****************
    User: Ihenderson   Date: 28/11/05   Time: 11:49
    Created in $/PCMOVA
*/

/*
     TODO:
*/

#include "stdafx.h"

#include "TCPClient.h"
#include "Serial.h"
#include "CommsErr.h"
#include "Messages.h"
#include "pccontio.h"

#define g_MODULE_NAME       ("PCControllerIO")


static MVBool       g_isInitialised = MV_FALSE;
static CommsType    g_commsType = COMMS_TYPE_INVALID;

// Buffers for storing messages
// Send buffers now local - Event messages can be sent by MOVA kernel 
// threads, e.g. TRLUserIF and term, as well as by the main thread.
// The buffer isn't too big for Windows, and putting the buffer on the
// stack saves us having to put in explicit synchronisation (e.g. critical sections).
//static MVByte       g_sentMsg[ xg_MSG_LEN_MAX ];
static MVByte       g_recvMsg[ xg_MSG_LEN_MAX ];


static MVStatus     PCContIO_WriteBytes(        const MVByte *, MVInt );
static MVStatus     PCContIO_ReadBytes(         MVByte *, MVInt );
static void         PCContIO_SetLastError(      PCError );


/*
    Name:           PCContIO_Initialise
    Author (Date):  ihenderson, 14/10/05
    Platform:       PC
    Purpose:        Connects to a controller using the given connection
                    type and end point information.
                    Starts listening for messages.
    Inputs:         Controller end point as a string - the contents of
                    which depend on the connection type
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for 
                    further error information.
    Assumptions:    
    Effects:        
*/
MVStatus PCContIO_Initialise( CommsType controllerComms, 
                             const char * controllerEndPoint )
{
    MVStatus    status = MV_SUCCESS;
    
    TRACE_ASSERT( !g_isInitialised, "PC Controller IO module already initialised." );
    TRACE_ASSERT_VALID_ADDRESS( controllerEndPoint, "No controller end point." );
    TRACE_ASSERT( MATH_IS_IN_RANGE( controllerComms, COMMS_TYPE_FIRST, COMMS_TYPE_LAST ),
        "Communications type is invalid." );

    TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
        "Initialising controller IO..." );

    // Select the functions appropriate to the connection type (could use function pointers
    // if we assume that TCPClient, SerialClient etc. are going to expose the same functions
    // in the same format.  Otherwise we'll need a switch every time Send, Receive etc. is called
    g_commsType = controllerComms;       

    switch ( g_commsType )
    {
        case COMMS_TYPE_TCP:
        {
            char traceFileName[16];
            status = TCPClient_Initialise( xg_APP_CONFIG_FILE_NAME,
                TRACE_GET_FILE_NAME( traceFileName, sizeof( traceFileName ) ) );

            //if ( status == MV_SUCCESS )
            //{
            //    // Set the timeouts (by default both send and
            //    // receive timeouts are infinite.  We need to wait
            //    // for receives, but we shouldn't for sends?
            //    // Generally, Controller should only have timeouts,
            //    // as PCMOVA is the "slave", but send timeouts
            //    // might be appropriate for events?  Not sure...
            //    // If the Controller has them, that seems enough to me - 
            //    // if PCMOVA waits too long to do an event message
            //    // send, the Controller receive timeout (waiting for a 
            //    // reply from the last kernel update) will expire anyway.
            //    status = TCPClient_SetTimeouts( 
            //        CONNECTION_ID_DATA, 100, 100 );
            //}

            if ( status == MV_SUCCESS )
            {
                /* Create a blocking TCP client - we need to wait while 
                sending/receiving so that we keep in step with the controller. */
                status = TCPClient_Connect(
                    CONNECTION_ID_DATA, controllerEndPoint, MV_TRUE );
            }

            break;
        }
        
        case COMMS_TYPE_SERIAL:
        {
            char traceFileName[16];

            TRACE_ASSERT( MV_FALSE, 
                "Error: Serial comms to controller not yet supported." );
                        
            status = Serial_Initialise( xg_APP_CONFIG_FILE_NAME,
                TRACE_GET_FILE_NAME( traceFileName, sizeof( traceFileName ) ) );

            if ( status == MV_SUCCESS )
            {
                // TODO: Timeouts?  No read or write timeouts for controller as above
                status = Serial_SetTimeouts( 
                    CONNECTION_ID_DATA, 0, 0, 0, 0 );
            }

            if ( status == MV_SUCCESS )
            {   
                // TODO: End point string should come from the controller via the command line
                status = Serial_OpenPort( CONNECTION_ID_DATA,
                    1,      // Comm port 
                    57600,  // Baud rate
                    7,      // Data bits
                    EVENPARITY,
                    ONESTOPBIT );
            }
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
            "Controller IO initialised." );
    }
    else
    {
        PCContIO_SetLastError( PCMOVA_ERROR_CONTROLLER_INIT );
    }

    return ( status );

} /* PCContIO_Initialise */


/*
    Name:           PCContIO_SendHandshake
    Author (Date):  ihenderson, 23/11/05
    Platform:       PC
    Purpose:        Creates and sends a handshake request message to 
                    the controller.
    Inputs:         Junction ID to send with the handshake message.
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Assumptions:    
    Effects:        
*/
MVStatus PCContIO_SendHandshake( MVInt junctionID )
{
    MVStatus    status = MV_SUCCESS;
    MVInt       bytesNum;
    MVByte        sentMsg[ xg_MSG_LEN_MAX ];

    TRACE_ASSERT( g_isInitialised, "Controller IO not initialised." );
    TRACE_ASSERT( MATH_IS_IN_RANGE( junctionID, xg_JUNCTION_ID_MIN, xg_JUNCTION_ID_MAX ),
        "Junction ID out of range." );

    bytesNum = Message_PackHandshake( junctionID, &( sentMsg[0] ) );

    // Now send the bytes that make up the message
    status = PCContIO_WriteBytes( &( sentMsg[0] ), bytesNum );

    return ( status );
    
} /* PCContIO_SendHandshake */


/*
    Name:           PCContIO_SendTerminate
    Author (Date):  ihenderson, 23/11/05
    Platform:       PC
    Purpose:        Creates and sends a terminate reply message to 
                    the controller.
    Inputs:         
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Assumptions:    
    Effects:        
*/
MVStatus PCContIO_SendTerminate( void )
{
    MVStatus    status = MV_SUCCESS;
    MVInt       bytesNum;
    MVByte        sentMsg[ xg_MSG_LEN_MAX ];

    TRACE_ASSERT( g_isInitialised, "Controller IO not initialised." );

    bytesNum = Message_PackTerminate( &( sentMsg[0] ) );

    // Now send the bytes that make up the message
    status = PCContIO_WriteBytes( &( sentMsg[0] ), bytesNum );

    return ( status );
    
} /* PCContIO_SendTerminate */


/*
    Name:           PCContIO_ReceiveInitialise
    Author (Date):  ihenderson, 23/11/05
    Platform:       PC
    Purpose:        Receives an initialise request message from
                    the controller.
    Inputs:         The date and time to retrieve from the initialise message.
    Returns:        
    Assumptions:    
    Effects:        
*/
void PCContIO_ReceiveInitialise( MVUInt32 * p_date, MVUInt32 * p_time )
{
    TRACE_ASSERT( g_isInitialised, "Controller IO not initialised." );
    TRACE_ASSERT( g_recvMsg[ 0 ], "No message to unpack." );    

    Message_UnpackInitialise( p_date, p_time, &( g_recvMsg[0] ) );

} /* PCContIO_ReceiveInitialise */


/*
    Name:           PCContIO_SendInitialise
    Author (Date):  ihenderson, 23/11/05
    Platform:       PC
    Purpose:        Creates and sends an initialise reply message to 
                    the controller.
    Inputs:         The end point that PCMOVA has set up for user 
                    communications (e.g. using MOVA Comm).
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Assumptions:    
    Effects:        
*/
MVStatus PCContIO_SendInitialise( const char * userEndPoint, 
                                 MVSize userEndPointLen )
{
    MVStatus    status = MV_SUCCESS;
    MVInt       bytesNum;
    MVByte        sentMsg[ xg_MSG_LEN_MAX ];

    TRACE_ASSERT( g_isInitialised, "Controller IO not initialised." );
    TRACE_ASSERT_VALID_ADDRESS( userEndPoint, "No user end point." );

    bytesNum = Message_PackInitialise( userEndPoint, 
        userEndPointLen, &( sentMsg[0] ) );

    // Now send the bytes that make up the message
    status = PCContIO_WriteBytes( &( sentMsg[0] ), bytesNum );

    return ( status );

} /* PCContIO_SendInitialise */


/*
    Name:           PCContIO_ReceiveUpdate
    Author (Date):  ihenderson, 23/11/05
    Platform:       PC
    Purpose:        Receives an update request message from
                    the controller.
    Inputs:         The detectors and confirms to retrieve from 
                    the initialise message.
    Returns:        
    Assumptions:    
    Effects:        
*/
void PCContIO_ReceiveUpdate( IOPort * p_detectors, IOPort * p_confirms,
                            MVBool * p_isControllerReady )
{
    TRACE_ASSERT( g_isInitialised, "Controller IO not initialised." );
    TRACE_ASSERT( g_recvMsg[ 0 ], "No message to unpack." );

    Message_UnpackUpdate( p_detectors, p_confirms, 
        p_isControllerReady, &( g_recvMsg[0] ) );

} /* PCContIO_ReceiveUpdate */


/*
    Name:           PCContIO_SendUpdate
    Author (Date):  ihenderson, 24/11/05
    Platform:       PC
    Purpose:        Creates and sends an update reply message to 
                    the controller.
    Inputs:         The forces to send in the update reply message.
                    Whether MOVA wants to take over control of the signals
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Assumptions:    
    Effects:        
*/
MVStatus PCContIO_SendUpdate( const IOPort * p_forces, MVBool isTakeOver, IOPort isHoldSignalOn, IOPort isReleaseSignalOn)
{
    MVStatus    status = MV_SUCCESS;
    MVInt       bytesNum;
    MVByte        sentMsg[ xg_MSG_LEN_MAX ];

    TRACE_ASSERT( g_isInitialised, "Controller IO not initialised." );
    TRACE_ASSERT_VALID_ADDRESS( p_forces, "No forces to send." );

    bytesNum = Message_PackUpdate( p_forces, isTakeOver, isHoldSignalOn, isReleaseSignalOn,  &( sentMsg[0] ) );

    // Now send the bytes that make up the message
    status = PCContIO_WriteBytes( &( sentMsg[0] ), bytesNum );

    return ( status );

} /* PCContIO_SendUpdate */


/*
    Name:           PCContIO_SendEvent
    Author (Date):  ihenderson, 30/11/05
    Platform:       PC
    Purpose:        Creates and sends an event message to 
                    the controller.
    Inputs:         The type of the event message (e.g. error/info).
                    A unique ID identifying the event.
                    A message string describing the event.
                    The length of the message string.
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Assumptions:    
    Effects:        
*/
MVStatus PCContIO_SendEvent( EventType eventType, MVUInt16 eventID, 
                            const char * eventDesc, MVSize eventDescLen )
{
    MVStatus    status = MV_SUCCESS;
    MVInt       bytesNum;
    MVByte        sentMsg[ xg_MSG_LEN_MAX ];

    TRACE_ASSERT( g_isInitialised, "Controller IO not initialised." );
    TRACE_ASSERT( MATH_IS_IN_RANGE( eventType, EVENT_TYPE_FIRST, EVENT_TYPE_LAST ),
        "Event type invalid." );
    TRACE_ASSERT_VALID_ADDRESS( eventDesc, "No event message string." );

    bytesNum = Message_PackEvent( eventType, eventID, 
        eventDesc, eventDescLen, &( sentMsg[0] ) );

    // Now send the bytes that make up the message
    status = PCContIO_WriteBytes( &( sentMsg[0] ), bytesNum );

    return ( status );

} /* PCContIO_SendEvent */


/*
    Name:           PCContIO_ReceiveHandshake
    Author (Date):  ihenderson, 17/10/05
    Platform:       PC
    Purpose:        Receives a handshake message from the controller
                    and extracts handshaking data from the message
    Inputs:         The handshaking data to be received
    Returns:        
    Assumptions:    That the next message waiting in the receive 
                    buffer is a handshake message.  To this end,
                    PCContIO_Peek() must be called immediately 
                    beforehand.
    Effects:        
*/
void PCContIO_ReceiveHandshake( MVInt * p_junctionID )
{
    TRACE_ASSERT( g_isInitialised, "Controller IO not initialised." );
    TRACE_ASSERT( g_recvMsg[ 0 ], "No message to unpack." );

    Message_UnpackHandshake( p_junctionID, &( g_recvMsg[0] ) );

} /* PCContIO_ReceiveHandshake */


/*
    Name:           PCContIO_Peek
    Author (Date):  ihenderson, 14/10/05
    Platform:       PC
    Purpose:        Gets the type of the next message waiting
                    to be received.  Stores the message for
                    processing by PCContIO_Receive.
    Inputs:         
    Returns:        Message type, or invalid if there
                    are no messages waiting.
    Assumptions:    
    Effects:        
*/
MessageType PCContIO_Peek( void )
{
    MVStatus    status = MV_SUCCESS;
    MVInt        msgLength = 0;
    MessageType    msgType = MSG_TYPE_INVALID;
    
    TRACE_ASSERT( g_isInitialised, "Controller IO not initialised." );;

    // Get the next message and store it glocally
    // as an array of bytes, returning its type
    status = PCContIO_ReadBytes( &( g_recvMsg[0] ), xg_MSG_LENGTH_LEN );
    if ( status == MV_SUCCESS )
    {
        status = Message_GetLength( &( g_recvMsg[0] ), &msgLength );
        TRACE_ASSERT( msgLength + xg_MSG_LENGTH_LEN <= xg_MSG_LEN_MAX,
            "Message too long for buffer." );
    }

    if ( status == MV_SUCCESS )
    {
        status = PCContIO_ReadBytes( &( g_recvMsg[ xg_MSG_LENGTH_LEN ] ), msgLength );
    }

    if ( status == MV_SUCCESS )
    {
        msgType = Message_GetType( &( g_recvMsg[0] ) );
    }

    return ( msgType );

} /* PCContIO_Peek */


/*
    Name:           PCContIO_IsInitialised
    Author (Date):  ihenderson, 14/10/05
    Platform:       PC
    Purpose:        Used to determine whether the PCMOVA Controller IO
                    module is initialised.
    Inputs:         
    Returns:        True if module initialised; False otherwise.
    Remarks:
*/
MVBool PCContIO_IsInitialised( void )
{
    return ( g_isInitialised );
    
} /* PCContIO_IsInitialised */


/*
    Name:           PCContIO_DeInitialise
    Author (Date):  ihenderson, 14/10/05
    Platform:       PC
    Purpose:        Deinitialises the PCMOVA Controller IO module,
                    by closing the connection to the controller.
    Inputs:         
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Remarks:
*/
MVStatus PCContIO_DeInitialise( void )
{
    MVStatus    status = MV_SUCCESS;

    if ( g_isInitialised )
    {
        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
            "Deinitialising controller IO..." );        

        switch ( g_commsType )
        {
            case COMMS_TYPE_TCP:
            {
                status = TCPClient_Disconnect( CONNECTION_ID_DATA );
                DEBUG_ASSERT( status != MV_FAILURE );

                status = TCPClient_DeInitialise();
                DEBUG_ASSERT( status != MV_FAILURE );
                
                break;
            }
                
            case COMMS_TYPE_SERIAL:
            {
                status = Serial_ClosePort( CONNECTION_ID_DATA );
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
                "Controller IO deinitialised." );
        }       
        else 
        {
            PCContIO_SetLastError( PCMOVA_ERROR_CONTROLLER_DEINIT );
        }

    } /* If initialised */

    return ( status );
    
} /* PCContIO_DeInitialise */


/*
    Name:           PCContIO_CommsTypeGet
    Author (Date):  ihenderson, 14/10/05
    Platform:       PC
    Purpose:        Gives the type of communications being used for
                    PCMOVA Controller IO (e.g. TCP or Serial)
    Inputs:         
    Returns:        The communications type (e.g. TCP or Serial)
    Remarks:
*/
CommsType PCContIO_CommsTypeGet( void )
{
    return ( g_commsType );
    
} /* PCContIO_CommsTypeGet */


/*
    Name:           PCContIO_WriteBytes
    Author (Date):  ihenderson, 14/10/05
    Platform:       PC
    Purpose:        Sends bytes to the controller using the appropriate
                    method of comms.
    Inputs:         The start of the bytes to send.
                    The number of bytes to send.
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Remarks:
*/
static MVStatus PCContIO_WriteBytes( const MVByte * p_bytes, MVInt count )
{
    MVStatus status;
    MVInt bytesWritten;

    switch ( g_commsType )
    {
        case COMMS_TYPE_SERIAL:
        {
            status = Serial_WriteBytes( CONNECTION_ID_DATA, 
                p_bytes, count, &bytesWritten );
            break;
        }

        case COMMS_TYPE_TCP:
        {
            status = TCPClient_WriteBytes( CONNECTION_ID_DATA, 
                p_bytes, count, &bytesWritten );
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
        PCContIO_SetLastError( PCMOVA_ERROR_CONTROLLER_SEND );
    }

    return ( status );

} /* PCContIO_WriteBytes */


/*
    Name:           PCContIO_ReadBytes
    Author (Date):  ihenderson, 14/10/05
    Platform:       PC
    Purpose:        Reads bytes from the controller using the appropriate
                    method of comms.
    Inputs:         The start of the bytes to read.
                    The number of bytes to read.
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Remarks:
*/
static MVStatus PCContIO_ReadBytes( MVByte * p_bytes, MVInt count )
{
    MVStatus status;
    MVInt   bytesRead;

    switch ( g_commsType )
    {
        case COMMS_TYPE_SERIAL:
        {
            status = Serial_ReadBytes( CONNECTION_ID_DATA, 
                p_bytes, count, &bytesRead );
            break;
        }

        case COMMS_TYPE_TCP:
        {
            status = TCPClient_ReadBytes( CONNECTION_ID_DATA, 
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
        PCContIO_SetLastError( PCMOVA_ERROR_CONTROLLER_RECEIVE );
    }

    return ( status );

} /* PCContIO_ReadBytes */


/*
    Name:           PCContIO_SetLastError
    Author (Date):  ihenderson, 14/10/05
    Platform:       PC
    Purpose:        Sets the PCMOVA last error value and error message 
                    for controller communications errors.
                    The error message is obtained from the communications
                    DLL (MVComms).
    Inputs:         The PCMOVA error code to use
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Remarks:
*/
static void PCContIO_SetLastError( PCError pcmovaError )
{
    char commsErrorMsg[ xg_ERROR_MSG_LEN_MAX + 1 ];

    /* Get the message accompanying the last comms err */
    CommsError_BuildMessage( 
        CommsError_GetLast(), CommsError_GetLastWin(), 
        commsErrorMsg, sizeof( commsErrorMsg ) );

    /* Set the last error with the comms error message as the detail message */
    PCError_SetLast( pcmovaError, commsErrorMsg, g_MODULE_NAME );

} /* PCContIO_SetLastError */
