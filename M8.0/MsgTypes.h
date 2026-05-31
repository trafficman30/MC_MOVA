#if !defined (__MSGTYPES_H)
#define __MSGTYPES_H

/*
    Name:           MsgTypes.h
    Author (Date):  ihenderson, 15/10/05
    Platform:       PC
    Purpose:        Definitions for Messages sent between
                    PCMOVA and the controller                    
                    struct definitions, constants, etc.
                    Should only be included by Messages.c
                    
                    N.B. The message handling code and the structure of
                    the messages themselves is mostly based on that
                    used by Siemens for their enhanced ST800 serial
                    link to MOVA (also used by MOVA Sim for communicating
                    with Siemens Gemini units).

    Last revision:  $Revision:: 3                           $
    Last changed:   $Date:: 12/04/06 16:07                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: MsgTypes.h $
    
    *****************  Version 3  *****************
    User: Ihenderson   Date: 12/04/06   Time: 16:07
    Updated in $/PCMOVA
    
    *****************  Version 2  *****************
    User: Ihenderson   Date: 5/12/05    Time: 11:01
    Updated in $/PCMOVA
    
    *****************  Version 1  *****************
    User: Ihenderson   Date: 28/11/05   Time: 11:49
    Created in $/PCMOVA
*/

#if defined (__cplusplus)
    extern "C" {
#endif

#include "Messages.h"
#include "pctypes.h"

#define g_START_OF_MESSAGE_CHAR    (0xBB)

// Number of bytes in header counted as part of the message length
#define g_HEADER_LEN        (4)

/* Size of the length info at the start (SOM plus 2 byte length) */
#define g_LENGTH_LEN        (xg_MSG_LENGTH_LEN)
#define g_CHECKSUM_LEN      (2)

/* Minimum valid length for message data (just the header) */
#define g_DATA_LEN_MIN      (g_HEADER_LEN)

/* Maximum valid length for message data (public maximum, less the
parts of the message not counted towards the data length) */
#define g_DATA_LEN_MAX      (xg_MSG_LEN_MAX - g_LENGTH_LEN - g_CHECKSUM_LEN)


/* Message IDs

Ping and Update messages correspond to the Ping and Update messages 
used by Siemens in the message protocol for their enhanced ST800 serial 
link, so take the same IDs.

The other messages are PCMOVA-specific, and take IDs from 100 
(unused by Siemens)
*/
#define g_MSG_TYPE_PING_ID          (0)
#define g_MSG_TYPE_UPDATE_ID        (7)
#define g_MSG_TYPE_HANDSHAKE_ID     (100)
#define g_MSG_TYPE_INITIALISE_ID    (101)
#define g_MSG_TYPE_EVENT_ID         (102)
#define g_MSG_TYPE_TERMINATE_ID     (103)


/* General message format - see Message_AddChecksum() */
typedef struct
{
    MVByte  SOM;
    MVByte  length_LSB;
    MVByte  length_MSB;
    MVByte  data[ g_DATA_LEN_MAX + g_CHECKSUM_LEN ];

} MsgType;


/* General message format with complete header */
typedef struct MsgHeader_struct
{
    MVByte  SOM;
    MVByte  length_LSB;
    MVByte  length_MSB;
    MVByte  type;
    MVByte  retaddr_LSB;
    MVByte  retaddr_MSB;
    MVByte  count;
    MVByte  data[ g_DATA_LEN_MAX - g_HEADER_LEN + g_CHECKSUM_LEN ];

} MsgHeader;


typedef struct MsgFooter_struct
{
    MVByte  checksum_LSB;
    MVByte  checksum_MSB;

} MsgFooter;


typedef struct MsgHandshakeRequest_struct
{
    MVByte  junctionID;

} MsgHandshakeRequest;


typedef struct MsgHandshakeReply_struct
{
    MVByte  junctionID;

} MsgHandshakeReply;


typedef struct MsgInitialiseRequest_struct
{
    MVUInt32    date;
    MVUInt32    time;

} MsgInitialiseRequest;


typedef struct MsgUpdateRequest_struct
{
    IOPort  detectors[ xg_DETECTOR_PORTS_MAX ];
    MVByte  controllerReady;    
    IOPort  confirms[ xg_CONFIRM_PORTS_MAX ];

} MsgUpdateRequest;


typedef struct MsgUpdateReply_struct
{
    MVByte  takeOver;
    IOPort  forces[ xg_FORCE_PORTS_MAX ];    

	IOPort  isHoldSignalOn;
	IOPort  isReleaseSignalOn;

} MsgUpdateReply;


/* The maximum number of bytes that can make up an event description */
#define g_EVENT_DESC_LEN_MAX    (1024)

typedef struct MsgEvent_struct
{
    MVByte  type;
    MVByte  id_LSB; /* Split ID into bytes to avoid struct packing */
    MVByte  id_MSB;
    char    description[ g_EVENT_DESC_LEN_MAX ];

} MsgEvent;


#if defined (__cplusplus)
    }
#endif

#endif /* __MSGTYPES_H */