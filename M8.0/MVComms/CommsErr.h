#if !defined (__COMMSERR_H)
#define __COMMSERR_H

#include "MVTypes.h"
#include "MVComms.h"
#include "MVError.h"

#include "cmstypes.h"

typedef enum
{
	COMMS_ERROR_INVALID = -1,

	COMMS_ERROR_FIRST,
	COMMS_ERROR_NO_ERROR = COMMS_ERROR_FIRST,
	COMMS_ERROR_OPEN,
	COMMS_ERROR_SEND,
	COMMS_ERROR_SEND_CLOSED,
	COMMS_ERROR_RECV,
	COMMS_ERROR_RECV_CLOSED,
	COMMS_ERROR_CLOSE,
	COMMS_ERROR_CLEAR_ERROR,
	COMMS_ERROR_END_POINT_INVALID,
	COMMS_ERROR_CONFIGURE,
	COMMS_ERROR_INIT,
	COMMS_ERROR_DEINIT,
	COMMS_ERROR_RECV_TIMEOUT,
	COMMS_ERROR_SEND_TIMEOUT,
	COMMS_ERROR_END_POINT_GET,
	COMMS_ERROR_SERVER_START,
	COMMS_ERROR_SERVER_LISTEN,
	COMMS_ERROR_SERVER_STOP,
	COMMS_ERROR_RECV_END,
	COMMS_ERROR_RECV_BLOCK,
	COMMS_ERROR_RECV_COUNT,
	COMMS_ERROR_CONNECT_TIMEOUT,
	COMMS_ERROR_SEND_END,
	COMMS_ERROR_PARAM_INVALID,
	COMMS_ERROR_CONNECTION_BUSY,
	COMMS_ERROR_INVALID_OPERATION,
	COMMS_ERROR_LAST = COMMS_ERROR_INVALID_OPERATION,
	
	COMMS_ERROR_COUNT

} CommsError;

void						CommsError_Initialise(      void );
void						CommsError_SetLast(         CommsError, const char * );
void						CommsError_SetLastWin(      CommsError, 
												        WinError, const char * );

/* Functions that can be used to carry out checks when publicly
accessible functions are called.  As MVComms.dll is used
by other programs, the error checking should be more rigorous. */

MVBool      CommsError_CheckInitialised(        const char *, MVBool );
MVBool      CommsError_CheckConnectionID(       const char *, ConnectionID );
MVBool      CommsError_CheckBytes(              const char *, const MVByte * );
MVBool      CommsError_CheckBytesCount(         const char *, MVInt );
MVBool      CommsError_CheckEndPoint(           const char *, const char * );
MVBool      CommsError_CheckConnectionClosed(   const char *, MVBool );

MVCOMMS_API CommsError		CommsError_GetLast(         void );
MVCOMMS_API WinError	    CommsError_GetLastWin(      void );
MVCOMMS_API MVSize			CommsError_BuildMessage(    CommsError, WinError, 
												        char *, MVSize );

#endif /* __COMMSERR_H */
