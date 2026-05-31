#if !defined (__PCTYPES_H)
#define __PCTYPES_H

/*
    Name:           pctypes.h
    Author (Date):  ihenderson, 15/11/05
    Platform:       PC
    Purpose:        Types and defines specific to the PCMOVA modules
                    (used generally throughout them).

    Last revision:  $Revision:: 3                           $
    Last changed:   $Date:: 12/04/06 16:07                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: pctypes.h $    
    
    *****************  Version 3  *****************
    User: Ihenderson   Date: 12/04/06   Time: 16:07
    Updated in $/PCMOVA
    
    *****************  Version 2  *****************
    User: Ihenderson   Date: 5/12/05    Time: 11:07
    Updated in $/PCMOVA
    
    *****************  Version 1  *****************
    User: Ihenderson   Date: 28/11/05   Time: 11:49
    Created in $/PCMOVA
*/

#if defined (__cplusplus)
    extern "C" {
#endif

#include "MVTypes.h"


/* Type for Controller IO ports */
typedef MVUInt8 IOPort;

/* IO Port size in bits */
#define xg_PORT_SIZE            (8 * sizeof( IOPort ))

/* Number of ports for the different IO "channels" */
#define xg_DETECTOR_PORTS_MAX   (8)
#define xg_CONFIRM_PORTS_MAX    (4)
#define xg_FORCE_PORTS_MAX      (2)


#define xg_JUNCTION_ID_MIN      (0)
#define xg_JUNCTION_ID_MAX      (255)


/* Types of communications supported by PCMOVA */
typedef enum
{
        COMMS_TYPE_INVALID = -1,
    
        COMMS_TYPE_FIRST,
    COMMS_TYPE_TCP = COMMS_TYPE_FIRST,
    COMMS_TYPE_SERIAL,
        COMMS_TYPE_LAST = COMMS_TYPE_SERIAL,

        COMMS_TYPE_COUNT

} CommsType;


/* Types of message sent/received between PCMOVA and the controller */
typedef enum
{
        MSG_TYPE_INVALID = -1,

        MSG_TYPE_FIRST,
    MSG_TYPE_PING = MSG_TYPE_FIRST,
    MSG_TYPE_UPDATE,
    MSG_TYPE_HANDSHAKE,
    MSG_TYPE_INITIALISE,
    MSG_TYPE_EVENT,
    MSG_TYPE_TERMINATE,
        MSG_TYPE_LAST = MSG_TYPE_TERMINATE,

        MSG_TYPE_COUNT

} MessageType;


/* Event messages can be of several types */
typedef enum
{
        EVENT_TYPE_INVALID = -1,

        EVENT_TYPE_FIRST,
    EVENT_TYPE_ERROR = EVENT_TYPE_FIRST,
    EVENT_TYPE_WARNING,
    EVENT_TYPE_INFO,
        EVENT_TYPE_LAST = EVENT_TYPE_INFO,

        EVENT_TYPE_COUNT    

} EventType;


#if defined (__cplusplus)
    }
#endif

#endif /* __PCTYPES_H */