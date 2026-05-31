#if !defined (__MESSAGES_H)
#define __MESSAGES_H

/*
    Name:           Messages.h
    Author (Date):  ihenderson, 15/10/05
    Platform:       PC
    Purpose:        Public interface to functionality for packing and 
                    unpacking messages for sending between PCMOVA and 
                    the controller.

    Last revision:  $Revision:: 3                           $
    Last changed:   $Date:: 12/04/06 16:07                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: Messages.h $
    
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

#if defined (__cplusplus)
    extern "C" {
#endif

#include "MVTypes.h"
#include "pctypes.h"


// Max message length 5k including headers etc.
#define xg_MSG_LEN_MAX      (5120)

// How many bytes to read to find out the length of the next message
#define xg_MSG_LENGTH_LEN   (3)

MVInt       Message_PackHandshake(      MVInt, MVByte * );
void        Message_UnpackHandshake(    MVInt *, const MVByte * );

MVInt       Message_PackTerminate(      MVByte * );

void        Message_UnpackInitialise(   MVUInt32 *, MVUInt32 *, const MVByte * );
MVInt       Message_PackInitialise(     const char *, MVSize, MVByte * );

void        Message_UnpackUpdate(       IOPort *, IOPort *, 
                                        MVBool *, const MVByte * );
MVInt       Message_PackUpdate(			const IOPort * p_forces, MVBool isTakeOver, IOPort isHoldSignalOn, IOPort isReleaseSignalOn,  MVByte * p_byteBuf );

MVInt       Message_PackEvent(          EventType, MVUInt16, const char *, 
                                        MVSize, MVByte * );

MVStatus    Message_GetLength(          const MVByte *, MVInt * );
MessageType Message_GetType(            const MVByte * );

#if defined (__cplusplus)
    }
#endif

#endif /* __MESSAGES_H */