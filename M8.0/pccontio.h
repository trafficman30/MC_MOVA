#if !defined (__PCCONTIO_H)
#define __PCCONTIO_H

/*
    Name:           PCContIO.h
    Author (Date):  ihenderson, 14/10/05
    Platform:       PC
    Purpose:        Functions for sending and receiving messages
                    to and from the Controller.

    Last revision:  $Revision:: 3                           $
    Last changed:   $Date:: 12/04/06 16:07                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: pccontio.h $
    
    *****************  Version 3  *****************
    User: Ihenderson   Date: 12/04/06   Time: 16:07
    Updated in $/PCMOVA
    
    *****************  Version 2  *****************
    User: Ihenderson   Date: 5/12/05    Time: 11:02
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


MVStatus    PCContIO_Initialise(        CommsType, const char * );
MVBool      PCContIO_IsInitialised(     void );
MVStatus    PCContIO_DeInitialise(      void );

MVStatus    PCContIO_SendHandshake(     MVInt );
void        PCContIO_ReceiveHandshake(  MVInt * );

MVStatus    PCContIO_SendTerminate(     void );

void        PCContIO_ReceiveInitialise( MVUInt32 *, MVUInt32 * );
MVStatus    PCContIO_SendInitialise(    const char *, MVSize );

void        PCContIO_ReceiveUpdate(     IOPort *, IOPort *, MVBool * );
MVStatus    PCContIO_SendUpdate(const IOPort * p_forces, MVBool isTakeOver, IOPort isHoldSignalOn, IOPort isReleaseSignalOn);

MVStatus    PCContIO_SendEvent(         EventType, MVUInt16, const char *, MVSize );

MessageType PCContIO_Peek(              void );

CommsType   PCContIO_CommsTypeGet(      void );


#if defined (__cplusplus)
    }
#endif

#endif /* __PCCONTIO_H */