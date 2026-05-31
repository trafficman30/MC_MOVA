/*
    Name:           Messages.c
    Author (Date):  ihenderson, 15/10/05
    Platform:       PC
    Purpose:        Implementation of functionality for packing and 
                    unpacking messages for sending between PCMOVA and 
                    the controller.
                    
                    N.B. The message handling code and the structure of
                    the messages themselves is mostly based on that
                    used by Siemens for their enhanced ST800 serial
                    link to MOVA (also used by MOVA Sim for communicating
                    with Siemens Gemini units).

    Last revision:  $Revision:: 3                           $
    Last changed:   $Date:: 12/04/06 16:07                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: Messages.c $
    
    *****************  Version 3  *****************
    User: Ihenderson   Date: 12/04/06   Time: 16:07
    Updated in $/PCMOVA
    
    *****************  Version 2  *****************
    User: Ihenderson   Date: 5/12/05    Time: 11:00
    Updated in $/PCMOVA
    
    *****************  Version 1  *****************
    User: Ihenderson   Date: 28/11/05   Time: 11:49
    Created in $/PCMOVA
*/

/*
    TODO: 
*/

#include <memory.h> // For memcpy
#include <string.h> // For strncpy

#include "stdafx.h"

#include "MsgTypes.h"


#define    g_MODULE_NAME    ("Messages")


static void         Message_SetChecksum( MVByte * );
static MVUInt16     Message_GetChecksum( const MVByte * );
static MVUInt16     Message_ComputeChecksum( const MVByte * );
static MVBool       Message_IsChecksumOK( MsgType * );
static MVInt        Message_GetDataLength( const MVByte * );
static MessageType  Message_ConvertIDToType( MVInt );
static MVBool       Message_IsStart( MVByte );
static MVInt        Message_Pack( MVInt, MVByte, MVUInt16, MVUInt16, 
                        const void *, MVByte * );


/*
    Name:           Message_Pack
    Author (Date):  ihenderson, 17/10/05
    Platform:       PC
    Purpose:        Packs the given data into the given buffer
                    using the correct message format.
    Inputs:         Message type
                    count (used in multi-part messages to indicate order)
                    length (of the data portion - excluding headers etc.)
                    return address (where appropriate, identifies source of message)
                    the data to be packed
                    a buffer to pack the data into
    Returns:        The number of bytes in the packed message,
                    or -1 if an error occurred.
                    Call PCError_GetLast... for 
                    further error information.
    Assumptions:    
    Effects:        
*/
static MVInt Message_Pack(  MVInt msgTypeID,
                            MVByte count, 
                            MVUInt16 dataLen, 
                            MVUInt16 retaddr, 
                            const void * p_data,
                            MVByte * p_byteBuf )
{
    MsgHeader * p_message;
    MVInt       msgBytesLen;
    
    TRACE_ASSERT_VALID_ADDRESS( p_byteBuf, "No data to pack message into." );
    
    /* Pointer to the message composition buffer */
    p_message = (MsgHeader *)p_byteBuf;

    p_message->SOM =            g_START_OF_MESSAGE_CHAR;
    p_message->type =           (MVByte)msgTypeID;
    p_message->retaddr_LSB =    (MVByte)( retaddr );
    p_message->retaddr_MSB =    (MVByte)( retaddr >> 8 );
    p_message->count =          count;

    /* If there is any data in this message, copy it to the composition buffer */
    if ( dataLen > 0 )
    {
        TRACE_ASSERT_VALID_ADDRESS( p_data, "No data to pack in message." );

        memcpy( &( p_message->data[0] ), p_data, dataLen );
    }

    dataLen += g_HEADER_LEN;
    p_message->length_LSB = (MVByte)( dataLen );
    p_message->length_MSB = (MVByte)( dataLen >> 8 );

    msgBytesLen = (MVInt)( g_LENGTH_LEN + dataLen + g_CHECKSUM_LEN );

    Message_SetChecksum( (MVByte *)p_message );

    return ( msgBytesLen );

} /* Message_Pack */


/*
    Name:           Message_PackHandshake
    Author (Date):  ihenderson, 14/10/05
    Platform:       PC
    Purpose:        Packs the given data into a Handshake
                    message and returns it as an array of bytes.
    Inputs:         Junction ID
                    The bytes to fill with the packed message
    Returns:        The number of bytes in the packed message,
                    or -1 if an error occurred.
                    Call PCError_GetLast... for 
                    further error information.
    Assumptions:    
    Effects:        
*/
MVInt Message_PackHandshake( MVInt junctionID, MVByte * p_byteBuf )
{
    MVInt byteBufLen;
    MsgHandshakeRequest handshakeRequest;
    
    TRACE_ASSERT( MATH_IS_IN_RANGE( junctionID, xg_JUNCTION_ID_MIN, xg_JUNCTION_ID_MAX ),
        "Junction ID out of range." );
    TRACE_ASSERT_VALID_ADDRESS( p_byteBuf, "No data to pack message into." );

    handshakeRequest.junctionID = (MVByte)( junctionID );

    byteBufLen = Message_Pack( g_MSG_TYPE_HANDSHAKE_ID, 0, 
        sizeof( MsgHandshakeRequest ), 0, &( handshakeRequest ), p_byteBuf );

    return ( byteBufLen );

} /* Message_PackHandshake */


/*
    Name:           Message_UnpackHandshake
    Author (Date):  ihenderson, 14/10/05
    Platform:       PC
    Purpose:        Unpacks the handshaking data encapsulated
                    within the given message.
    Inputs:         Junction ID to be read from the message
                    The message bytes
    Returns:        
    Assumptions:    That the given message is a handshake message
    Effects:        
*/
void Message_UnpackHandshake( MVInt * p_junctionID, const MVByte * p_byteBuf )
{
    MsgHeader * msg = (MsgHeader *)p_byteBuf;
    MsgHandshakeReply * p_handshake;

    TRACE_ASSERT_VALID_ADDRESS( p_junctionID, "No junction ID to set." );
    TRACE_ASSERT_VALID_ADDRESS( p_byteBuf, "No bytes to unpack." );

    // Get the message data
    p_handshake = (MsgHandshakeReply *) &( msg->data[ 0 ] );

    (*p_junctionID) = (MVInt)( p_handshake->junctionID );

} /* Message_UnpackHandshake */


/*
    Name:           Message_PackTerminate
    Author (Date):  ihenderson, 04/11/05
    Platform:       PC
    Purpose:        Packs a Terminate reply message and returns 
                    it as an array of bytes. Terminate
                    messages contain no data - just metadata.
    Inputs:         The bytes to fill with the packed message
    Returns:        The number of bytes in the packed message,
                    or -1 if an error occurred.
                    Call PCError_GetLast... for 
                    further error information.
    Assumptions:    
    Effects:        
*/
MVInt Message_PackTerminate( MVByte * p_byteBuf )
{
    MVInt byteBufLen;
    
    TRACE_ASSERT_VALID_ADDRESS( p_byteBuf, "No data to pack message into." );

    byteBufLen = Message_Pack( g_MSG_TYPE_TERMINATE_ID, 0, 
        0, 0, NULL, p_byteBuf );

    return ( byteBufLen );

} /* Message_PackTerminate */


/*
    Name:           Message_UnpackInitialise
    Author (Date):  ihenderson, 23/11/05
    Platform:       PC
    Purpose:        Unpacks an Initialise reply message and returns 
                    the date and time data contained within it.
    Inputs:         The date and time to set
                    The bytes that make up the message
    Returns:        
    Assumptions:    
    Effects:        
*/
void Message_UnpackInitialise( MVUInt32 * p_date, 
                              MVUInt32 * p_time, const MVByte * p_byteBuf )
{
    MsgHeader * msg = (MsgHeader *)p_byteBuf;
    MsgInitialiseRequest * p_initialise;

    TRACE_ASSERT_VALID_ADDRESS( p_date, "No date to set." );
    TRACE_ASSERT_VALID_ADDRESS( p_time, "No time to set." );
    TRACE_ASSERT_VALID_ADDRESS( p_byteBuf, "No bytes to unpack." );

    // Get the message data
    p_initialise = (MsgInitialiseRequest *) &( msg->data[ 0 ] );

    (*p_date) = (MVUInt32)( p_initialise->date );
    (*p_time) = (MVUInt32)( p_initialise->time );

    // TEST CODE: 01/03/2008, 12:10:59.30
    //(*p_date) = 0x010307D8;
    //(*p_time) = 0x0C0A3B1E;

} /* Message_UnpackInitialise */


/*
    Name:           Message_PackInitialise
    Author (Date):  ihenderson, 23/11/05
    Platform:       PC
    Purpose:        Packs an Initialise reply message and returns 
                    it as an array of bytes.
    Inputs:         The bytes to fill with the packed message
                    The user end point data to pack
                    The length of the user end point data
    Returns:        The number of bytes in the packed message,
                    or -1 if an error occurred.
                    Call PCError_GetLast... for 
                    further error information.
    Assumptions:    
    Effects:        
*/
MVInt Message_PackInitialise( const char * userEndPoint, 
                             MVSize userEndPointLen, MVByte * p_byteBuf )
{
    MVInt byteBufLen;
    
    TRACE_ASSERT_VALID_ADDRESS( p_byteBuf, "No bytes to pack." );
    TRACE_ASSERT_VALID_ADDRESS( userEndPoint, "No user end point." );

    byteBufLen = Message_Pack( g_MSG_TYPE_INITIALISE_ID, 0,  (MVUInt16)userEndPointLen, 0, userEndPoint, p_byteBuf );

    return ( byteBufLen );

} /* Message_PackInitialise */


/*
    Name:           Message_UnpackUpdate
    Author (Date):  ihenderson, 23/11/05
    Platform:       PC
    Purpose:        Unpacks an Update reply message and returns 
                    the detector and stage/phase confirm data 
                    contained within it.
    Inputs:         The detectors and confirms to set
                    The bytes that make up the message
    Returns:        
    Assumptions:    
*/
void Message_UnpackUpdate( IOPort * p_detectors, IOPort * p_confirms, 
                          MVBool * p_isControllerReady, const MVByte * p_byteBuf )
{
    MsgHeader * msg = (MsgHeader *)p_byteBuf;
    MsgUpdateRequest * p_update;
    MVInt portIndex;

    TRACE_ASSERT_VALID_ADDRESS( p_detectors, "No detectors to set." );
    TRACE_ASSERT_VALID_ADDRESS( p_confirms, "No confirms to set." );
    TRACE_ASSERT_VALID_ADDRESS( p_byteBuf, "No bytes to unpack." );

    // Get the message data
    p_update = (MsgUpdateRequest *) &( msg->data[ 0 ] );

    (*p_isControllerReady) = 
        ( p_update->controllerReady == 0 ? MV_FALSE : MV_TRUE );
    
    for ( portIndex = 0; portIndex != xg_DETECTOR_PORTS_MAX; ++portIndex )
    {
        p_detectors[ portIndex ] = p_update->detectors[ portIndex ];
    }

    // TEST CODE:
    /* Lowest bit in first port = detector 1; highest bit in last port = detector 64 */
    //p_detectors[ 0 ] = 192; //0b11000000;
    //p_detectors[ 1 ] = 80;  //0b01010000;
    //p_detectors[ 2 ] = 24;  //0b00011000;
    //p_detectors[ 3 ] = 66;  //0b01000010;
    //p_detectors[ 4 ] = 0;   //0b00000000;
    //p_detectors[ 5 ] = 255; //0b11111111;
    //p_detectors[ 6 ] = 28;  //0b00011100;
    //p_detectors[ 7 ] = 60;  //0b00111100;

    for ( portIndex = 0; portIndex != xg_CONFIRM_PORTS_MAX; ++portIndex )
    {
        p_confirms[ portIndex ] = p_update->confirms[ portIndex ];
    }

    // TEST CODE:
    /* Lowest bit in first port = confirm 1; highest bit in last port = confirm 32 */
    //p_confirms[ 0 ] = 223;  //0b11011111;
    //p_confirms[ 1 ] = 127;  //0b01111111;
    //p_confirms[ 2 ] = 253;  //0b11111101;
    //p_confirms[ 3 ] = 247;  //0b11110111;

} /* Message_UnpackUpdate */


/*
    Name:           Message_PackUpdate
    Author (Date):  ihenderson, 24/11/05
    Platform:       PC
    Purpose:        Packs an Update reply message and returns 
                    it as an array of bytes.
    Inputs:         The bytes to fill with the packed message
                    The (stage) force data to pack
                    Whether MOVA wants to take over control of the signals
    Returns:        The number of bytes in the packed message,
                    or -1 if an error occurred.
                    Call PCError_GetLast... for 
                    further error information.
    Assumptions:    
    Effects:        
*/
MVInt Message_PackUpdate( const IOPort * p_forces, MVBool isTakeOver, IOPort isHoldSignalOn, IOPort isReleaseSignalOn,  MVByte * p_byteBuf )
{
    MVInt byteBufLen;
    MsgUpdateReply updateReply;
    MVInt portIndex;
    
    TRACE_ASSERT_VALID_ADDRESS( p_byteBuf, "No bytes to pack." );
    TRACE_ASSERT_VALID_ADDRESS( p_forces, "No forces to pack." );

    updateReply.takeOver = isTakeOver;

    for ( portIndex = 0; portIndex != xg_FORCE_PORTS_MAX; ++portIndex )
    {
        updateReply.forces[ portIndex ] = p_forces[ portIndex ];
    }

	updateReply.isHoldSignalOn = isHoldSignalOn;
	updateReply.isReleaseSignalOn = isReleaseSignalOn; 
	
    byteBufLen = Message_Pack( g_MSG_TYPE_UPDATE_ID, 0, 
        sizeof( MsgUpdateReply ), 0, &( updateReply ), p_byteBuf );

    return ( byteBufLen );

} /* Message_PackUpdate */


/*
    Name:           Message_PackEvent
    Author (Date):  ihenderson, 30/11/05
    Platform:       PC
    Purpose:        Packs an Event message and returns 
                    it as an array of bytes.
    Inputs:         The bytes to fill with the packed message.
                    The type of the event message (e.g. error/info).
                    A unique ID identifying the event.
                    A message string describing the event.
                    The length of the message string.
    Returns:        The number of bytes in the packed message,
                    or -1 if an error occurred.
                    Call PCError_GetLast... for 
                    further error information.
    Assumptions:    
    Effects:        
*/
MVInt Message_PackEvent( EventType eventType, MVUInt16 eventID, 
                        const char * eventDesc, MVSize eventDescLen,
                        MVByte * p_byteBuf )
{
    MVInt       byteBufLen;
    MsgEvent    event;
    MVSize      msgLen;
    
    TRACE_ASSERT_VALID_ADDRESS( p_byteBuf, "No bytes to pack." );
    TRACE_ASSERT_VALID_ADDRESS( eventDesc, "No event description string." );
    TRACE_ASSERT( MATH_IS_IN_RANGE( eventType, EVENT_TYPE_FIRST, EVENT_TYPE_LAST ),
        "Event type invalid." );
    
    DEBUG_ASSERT( eventDescLen <= g_EVENT_DESC_LEN_MAX );
    
    /* Set the type, ID and description of the message */
    event.type = eventType;
    event.id_LSB = (MVByte)( eventID );
    event.id_MSB = (MVByte)( eventID >> 8 );

    /* Truncate error description if it is too long
       (shouldn't bother the user with this relatively trivial error) */
    if ( eventDescLen > g_EVENT_DESC_LEN_MAX )
    {        
        strncpy( event.description, eventDesc, g_EVENT_DESC_LEN_MAX );
        msgLen = sizeof( MsgEvent );
    }
    else
    {
        strncpy( event.description, eventDesc, eventDescLen );
        msgLen = sizeof( MsgEvent ) - g_EVENT_DESC_LEN_MAX + eventDescLen;
    }

    byteBufLen = Message_Pack( g_MSG_TYPE_EVENT_ID, 0, (MVUInt16)msgLen, 0, &( event ), p_byteBuf );

    return ( byteBufLen );

} /* Message_PackEvent */


/*
    Name:           Message_SetChecksum
    Author (Date):  ihenderson, 17/10/05
    Platform:       PC
    Purpose:        Computes and adds the checksum to the given message.
    Inputs:         The message buffer to add the checksum to
    Returns:        
    Remarks:
*/
static void Message_SetChecksum( MVByte * msgBuf )
{
    MsgType *   msg = (MsgType *)msgBuf;
    MVUInt16    checksum;
    MVUInt16    checksumOffset;

    /* calculate the checksum */
    checksum = Message_ComputeChecksum( (MVByte *)msg );

    /* the checksum goes at the end of the message */
    checksumOffset = (MVUInt16)( Message_GetDataLength( msgBuf ) );

    /* write the checksum at the end of the message (LSB, MSB) */
    msg->data[ checksumOffset ] = (MVByte)( checksum );
    msg->data[ checksumOffset + 1 ] = (MVByte)( checksum >> 8 );

} /* Message_SetChecksum */


/*
    Name:           Message_GetChecksum
    Author (Date):  ihenderson, 17/10/05
    Platform:       PC
    Purpose:        Retrieves the checksum from the given message.
    Inputs:         The message buffer to get the checksum from.
    Returns:        The checksum (modulo 65536)
    Remarks:
*/
static MVUInt16 Message_GetChecksum( const MVByte * msgBuf )
{
    const MsgType * msg = (MsgType *)msgBuf;
    MVUInt16        checksum;
    MVUInt16        checksumOffset;

    /* Get the checksum from the end of the message */
    checksumOffset = (MVUInt16)( Message_GetDataLength( msgBuf ) );
    
    checksum = (MVUInt16)( 
        ( msg->data[ checksumOffset + 1 ] << 8 ) 
        + msg->data[ checksumOffset ] );

    return ( checksum );

} /* Message_GetChecksum */


/*
    Name:           Message_ComputeChecksum
    Author (Date):  ihenderson, 17/10/05
    Platform:       PC
    Purpose:        Computes the checksum for the given message and returns
                    the result.
    Inputs:         The message buffer to calculate the checksum for.
    Returns:        The checksum (modulo 65536)
    Remarks:
*/
static MVUInt16 Message_ComputeChecksum( const MVByte * msgBuf )
{
    const MsgType * msg = (MsgType *)msgBuf;
    MVInt           byteNum, length;
    MVUInt16        checksum = 0;

    /* extract the length from the message */
    length = Message_GetDataLength( msgBuf );

    /* for each byte of the message after the length */
    for ( byteNum = 0; byteNum < length; ++byteNum )
    {
        /* add the contents of the byte to the checksum (modulo 65536) */     
        checksum = (MVUInt16)( checksum + msg->data[ byteNum ] );
    }

    return ( checksum );

} /* Message_ComputeChecksum */


/*
    Name:           Message_IsChecksumOK
    Author (Date):  ihenderson, 17/10/05
    Platform:       PC
    Purpose:        Determines whether the stored and freshly
                    calculated checksums match (indicates
                    whether the message data is clean or corrupt)
    Inputs:         The message to check.
    Returns:        True if the checksums matched; False otherwise
    Remarks:
*/
static MVBool Message_IsChecksumOK( MsgType * msg )
{
    MVUInt16    checksumComputed;
    MVUInt16    checksumRead;

    // Check that the message checksum is ok
    checksumRead = Message_GetChecksum( (MVByte *)msg );
    checksumComputed = Message_ComputeChecksum( (MVByte *)msg );

    return ( checksumComputed == checksumRead ? MV_TRUE : MV_FALSE );

} /* Message_IsChecksumOK */


/*
    Name:           Message_GetDataLength
    Author (Date):  ihenderson, 17/10/05
    Platform:       PC
    Purpose:        Get the length of data in the given message, 
                    excluding the message header and checksum.
    Inputs:         The message to get the data length of
    Returns:        The length of the data contained in the message.
    Remarks:
*/
static MVInt Message_GetDataLength( const MVByte * msgBytes )
{
    MVInt        msgDataLength;
    MsgType *    msg = (MsgType *)msgBytes;

    msgDataLength = (MVInt)( ( msg->length_MSB << 8 ) + msg->length_LSB );

    return ( msgDataLength );

} /* Message_GetDataLength */


/*
    Name:           Message_IsStart
    Author (Date):  ihenderson, 29/11/05
    Platform:       PC
    Purpose:        Determines whether the given byte indicates the 
                    start of a new message.
    Inputs:         The byte to test.
    Returns:        Whether the byte indicates a new message.
    Assumptions:    
    Effects:        
*/
static MVBool Message_IsStart( MVByte msgByte )
{            
    return ( msgByte == g_START_OF_MESSAGE_CHAR ? MV_TRUE : MV_FALSE );

} /* Message_IsStart */


/*
    Name:           Message_GetLength
    Author (Date):  ihenderson, 17/10/05
    Platform:       PC
    Purpose:        Get the length of data in the given message, 
                    excluding the message header but *including* 
                    the checksum at the end of the message.
                    This can be used to determine how many bytes exist 
                    (and therefore to read) after the header.
    Inputs:         The message to get the length of.
                    The length of the data contained in the message.
    Returns:        Success if the length of the message could be obtained;
                    Failure otherwise. Call PCError_GetLast() for error code.
    Remarks:        Checks that the message is genuine by checking the start
                    of message character.
*/
MVStatus Message_GetLength( const MVByte * msgBytes, MVInt * length )
{
    MVInt       msgDataLength = 0;
    MVStatus    status = MV_SUCCESS;

    TRACE_ASSERT( msgBytes != NULL, "Message data is null." );

    if ( Message_IsStart( msgBytes[0] ) )
    {
        msgDataLength = Message_GetDataLength( msgBytes );

        if ( msgDataLength < g_DATA_LEN_MIN 
            || msgDataLength > g_DATA_LEN_MAX )
        {
            PCError_SetLast( PCMOVA_ERROR_MSG_LENGTH, NULL, g_MODULE_NAME );
            status = MV_FAILURE;
        }
    }
    else
    {
        PCError_SetLast( PCMOVA_ERROR_MSG_START, NULL, g_MODULE_NAME );
        status = MV_FAILURE;
    }

    /* If the length was retrieved successfully, pass it to the caller */
    if ( status == MV_SUCCESS )
    {
        (*length) = msgDataLength + g_CHECKSUM_LEN;
    }

    return ( status );

} /* Message_GetLength */


/*
    Name:           Message_GetType
    Author (Date):  ihenderson, 17/10/05
    Platform:       PC
    Purpose:        Determines the type of the given message from its
                    message ID.
    Inputs:         The message to get the type of.
    Returns:        The type of the message if successful;
                    MSG_TYPE_INVALID otherwise.
                    Call PCError_GetLast() for error code.
    Remarks:        Checks the checksum of the message before
                    determining the type.
*/
MessageType Message_GetType( const MVByte * msgBytes )
{
    MessageType msgType;
    MsgHeader * msg = (MsgHeader *)msgBytes;
    MVUInt16    checksumRead, checksumComputed;

    TRACE_ASSERT( msgBytes != NULL, "Message data is null." );

    // Check that the message checksum is ok
    checksumRead = Message_GetChecksum( msgBytes );
    checksumComputed = Message_ComputeChecksum( msgBytes );

    if ( checksumComputed == checksumRead )
    {                
        MVInt msgID = (MVInt)( msg->type );
        msgType = Message_ConvertIDToType( msgID );
    }
    else
    {
        PCError_SetLast( PCMOVA_ERROR_MSG_CHECKSUM, NULL, g_MODULE_NAME );
        msgType = MSG_TYPE_INVALID;
    }

    return ( msgType );

} /* Message_GetType */


/*
    Name:           Message_ConvertIDToType
    Author (Date):  ihenderson, 17/10/05
    Platform:       PC
    Purpose:        Converts the given message ID to a message type.
                    Message types are publically available to users
                    of this module; the actually numeric ID of a 
                    message is "private" to the message handling code.
    Inputs:         The message ID to translate
    Returns:        The type of the message if successful;
                    MSG_TYPE_INVALID otherwise.
                    Call PCError_GetLast() for error code.
    Remarks:        
*/
static MessageType Message_ConvertIDToType( MVInt msgID )
{
    MessageType msgType;

    switch ( msgID )
    {
        case g_MSG_TYPE_PING_ID:
        {
            msgType = MSG_TYPE_PING;
            break;
        }
        case g_MSG_TYPE_UPDATE_ID:
        {
            msgType = MSG_TYPE_UPDATE;
            break;
        }
        case g_MSG_TYPE_HANDSHAKE_ID:
        {
            msgType = MSG_TYPE_HANDSHAKE;
            break;
        }
        case g_MSG_TYPE_INITIALISE_ID:
        {
            msgType = MSG_TYPE_INITIALISE;
            break;
        }
        case g_MSG_TYPE_EVENT_ID:
        {
            msgType = MSG_TYPE_EVENT;
            break;
        }
        case g_MSG_TYPE_TERMINATE_ID:
        {
            msgType = MSG_TYPE_TERMINATE;
            break;
        }
        default:
        {
            PCError_SetLast( PCMOVA_ERROR_MSG_ID, NULL, g_MODULE_NAME );
            msgType = MSG_TYPE_INVALID;
            break;
        }

    } /* switch ( msgID ) */

    return ( msgType );

} /* Message_ConvertIDToType */
