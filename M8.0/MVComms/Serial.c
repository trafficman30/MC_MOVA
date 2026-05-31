/*
    Name:           Serial.c
    Author (Date):  ihenderson, 15/11/05
    Platform:       PC
    Purpose:        Provides serial connection functionality
                    for connecting between PCMOVA and MOVA Comm.
                    This module is generic though - 
                    it could be used by other programs
                    e.g. MOVA Sim.

*/


/* 
	TODO: Thread-safety!  ReadBytes and WriteBytes use critical sections,
    but no other functions at present.

    Use InitializeCriticalSectionAndSpinCount to pre-allocate memory
    for critical sections? - means that there's no risk of raising (low) memory 
    exceptions when calling EnterCriticalSection.
	
    NOTE: PeekNamedPipe() can be used for peeking with serial coms if needed.
*/


/* Include standard Windows headers */
#if defined (WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#endif

#include <stdio.h>

#include "MVTypes.h"
#include "MVDebug.h"
#include "MVTrace.h"
#include "MVMath.h"

#include "Serial.h"
#include "CommsErr.h"


#define	g_MODULE_NAME	("Serial")

#define g_PARITY_CHECK_DISABLED         (0)
#define g_PARITY_CHECK_ENABLED          (1)

#define g_READ_TIMEOUT_BYTE_INTERVAL    (0)


typedef struct _SerialEndPoint
{
	MVByte     comPort;
    MVUInt32   baudRate;
    MVByte     byteSize;
    MVByte     parity;
    MVByte     stopBits;

} SerialEndPoint;


typedef struct _Timeouts
{
    MVUInt32 readConstant;
    MVUInt32 writeConstant;
    MVUInt32 readPerByte;
    MVUInt32 writePerByte;

} Timeouts;


typedef struct _Connection
{
    HANDLE				comFile;
    MVBool				isOpen;
	SerialEndPoint		endPoint;
    Timeouts			timeOuts;
	CRITICAL_SECTION    criticalSection;

} Connection;


static Connection   g_connections[ CONNECTION_ID_COUNT ];
static MVInt        g_connectionCount;
static MVBool       g_isInitialised = MV_FALSE;
static MVInt        g_refCount;


static MVStatus     Serial_ConfigurePort(   const Connection *, MVUInt );
static MVStatus     Serial_ApplyTimeouts(   const Connection *, MVUInt32 );

/*
    Name:           Serial_Initialise
    Author (Date):  ihenderson, 15/11/05
    Platform:       PC
    Purpose:        Initialises serial module
    Inputs:         void
    Returns:        Status (success/failure)
                    Call TODO for 
                    further error information.
    Assumptions:    
    Effects:        
*/
MVStatus Serial_Initialise( const char * configFileName, 
                           const char * traceFileName )
{
    MVStatus	    status = MV_SUCCESS;
    ConnectionID    connID;

    if ( !g_isInitialised )
    {
		for ( connID = 0; connID != MATH_ARRAY_LEN( g_connections ); ++connID )
        {
            /* Initialise the critical section for each connection (once only) */    
            InitializeCriticalSection( &( g_connections[ connID ].criticalSection ) );
        }

        TRACE_INITIALISE( configFileName, traceFileName );
	    CommsError_Initialise();

        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
            "Initialising communications..." );

        /* All connections are closed to begin with */
        for ( connID = 0; connID != MATH_ARRAY_LEN( g_connections ); ++connID )
        {
            Connection * p_conn = &( g_connections[ connID ] );
            
            p_conn->isOpen = MV_FALSE;
            p_conn->comFile = INVALID_HANDLE_VALUE;
            p_conn->endPoint.baudRate = 0;
            p_conn->endPoint.byteSize = 0;
            p_conn->endPoint.comPort = 0;
            p_conn->endPoint.parity = 0;
            p_conn->endPoint.stopBits = 0;

            /* By default, don't use timeouts */
            p_conn->timeOuts.readConstant = 0;
            p_conn->timeOuts.readPerByte = 0;
            p_conn->timeOuts.writeConstant = 0;
            p_conn->timeOuts.writePerByte = 0;
        }

        g_connectionCount = 0;

		g_isInitialised = MV_TRUE;
		g_refCount = 1;

        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
            "Communications initialised." );

    } /* if ( !g_isInitialised ) */

    else
    {
        g_refCount++;
    }

    return ( status );

} /* Serial_Initialise */



MVStatus Serial_DeInitialise( void )
{
    MVStatus status = MV_SUCCESS;

    if ( g_isInitialised )
    {        
		ConnectionID connID;

        if ( g_refCount == 1 )
        {
            TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
                "Deinitialising communications..." );            

            /* Terminate any existing connections */
            if ( g_connectionCount > 0 )
            {                
                for ( connID = 0; 
                    connID != MATH_ARRAY_LEN( g_connections ); 
                    ++connID )
                {
					status = Serial_ClosePort( connID );
                }
            }

            for ( connID = 0; connID != MATH_ARRAY_LEN( g_connections ); ++connID )
            {
                /* Delete the critical section for each connection (once only) */
                DeleteCriticalSection( &( g_connections[ connID ].criticalSection ) );
            }

            /* BUGFIX: 10077/10055.  Ensure module is marked as deinitialised 
            even if an error occurred - ReadBytes etc. cannot be called now
            because the critical section has been destroyed.  Doing so will
            cause an access violation in NTDll.dll. */
		    g_isInitialised = MV_FALSE;
            TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
                "Communications deinitialised." );

        } /* if ( g_refCount == 0 ) */

		g_refCount--;

    } /* if ( g_isInitialised ) */

    return ( status );

} /* Serial_DeInitialise */


MVBool Serial_IsInitialised( void )
{
    return ( g_isInitialised );
}


MVStatus Serial_SetTimeouts( ConnectionID connID, 
                          MVUInt32 readConstant,
                          MVUInt32 writeConstant,
                          MVUInt32 readPerByte,
                          MVUInt32 writePerByte )
{
    MVStatus       status = MV_SUCCESS;
    Connection *    p_conn;

    /* Ensure that SetTimeouts fails gracefully
    if parameters or module state are incorrect. */
    if ( !CommsError_CheckInitialised( "Serial_SetTimeouts", g_isInitialised ) 
        || !CommsError_CheckConnectionID( "Serial_SetTimeouts", connID ) ) 
    {
        return MV_FAILURE;
    }
	
    p_conn = &( g_connections[ connID ] );

	TRACE_WRITE4_IF( LEVEL_INFO, g_MODULE_NAME, 
		"Setting timeouts: read %d (ms), write %d (ms), read byte %d (ms), write byte %d (ms).",
		readConstant, writeConstant, readPerByte, writePerByte );

    p_conn->timeOuts.readConstant = readConstant;
    p_conn->timeOuts.writeConstant = writeConstant;
    p_conn->timeOuts.readPerByte = readPerByte;
    p_conn->timeOuts.writePerByte = writePerByte;

    return ( status );
}


MVStatus Serial_OpenPort( ConnectionID connID, 
                          MVByte comPort,
                          MVUInt32 baudRate, 
                          MVByte byteSize, 
                          MVByte parity, 
                          MVByte stopBits )
{
    MVStatus		status = MV_FAILURE;
    char            comPortFileName[ 16 ];
    Connection *    p_conn;

    /* Ensure that OpenPort fails gracefully
    if parameters or module state are incorrect. */
    if ( !CommsError_CheckInitialised( "Serial_OpenPort", g_isInitialised ) 
        || !CommsError_CheckConnectionID( "Serial_OpenPort", connID )
        || !CommsError_CheckConnectionClosed( "Serial_OpenPort", g_connections[ connID ].isOpen ) ) 
    {
        return MV_FAILURE;
    }

    /* Get the connection data for a new connection with the given ID */
	p_conn = &( g_connections[ connID ] );

	TRACE_WRITE5_IF( LEVEL_INFO, g_MODULE_NAME,
		"Opening port with settings: port %d, baud rate %d, byte size %d, parity %d, stop bits %d.",
		comPort, baudRate, byteSize, parity, stopBits );

    p_conn->endPoint.comPort = comPort;
    p_conn->endPoint.baudRate = baudRate;
    p_conn->endPoint.byteSize = byteSize;
    p_conn->endPoint.parity = parity;
    p_conn->endPoint.stopBits = stopBits;    

    sprintf( comPortFileName, "%s%d", "//./com", comPort );

    p_conn->comFile = CreateFile( comPortFileName,
        GENERIC_READ | GENERIC_WRITE,
        0,
        0,
        OPEN_EXISTING,
        0,
        0 );

    if( p_conn->comFile != INVALID_HANDLE_VALUE )
    {
        /*MessageBox(NULL,"Opened Communication Port.","Com Port Info",MB_OK+MB_ICONINFORMATION );*/        
        status = Serial_ConfigurePort( p_conn, g_PARITY_CHECK_DISABLED );
    }

    if( status == MV_SUCCESS )
    {
        /*MessageBox(NULL,"Configured Communication Port.","Com Port Info",MB_OK+MB_ICONINFORMATION );*/
        /*boResult = Serial_ApplyTimeouts(0,n32TimeOut,n32TimeOut,n32TimeOut,n32TimeOut);     // All zeroes means no timeouts - changed to get Movasim working*/
        /*************************
            * IRH MOD: PCMOVA: 19/04/05: Changed timeouts to get Read operations to return immediately
            * if there is no character waiting in the buffer.  Otherwise, in Term.c  (EngScreen()) we do
            
            // get key press if any
            if ( LU == LOCAL_HANDSET )
            {
            cKey = GetHandsetChar();
            }

            which won't return until the Read timeout has expired.  This is in the same thread as UpdateScreen()
            therefore the screen won't get refreshed until this read has failed.  This isn't good.  In the 
            embedded version, GetHandsetChar must return immediately, so it should here too - user comms
            are assumed to be non-blocking by the term() and TRLUserIF() routines in MOVA.

            static BOOL Serial_ApplyTimeouts(
            DWORD ReadIntervalTimeout, DWORD ReadTotalTimeoutMultiplier, 
            DWORD ReadTotalTimeoutConstant, DWORD WriteTotalTimeoutMultiplier, DWORD WriteTotalTimeoutConstant)
            
        0,0,10,0,1000 -> PCMOVA
        0,10,10,10,10 -> MOVA Comm
        *************************/
        status = Serial_ApplyTimeouts( p_conn, g_READ_TIMEOUT_BYTE_INTERVAL );
    }

    if ( status == MV_SUCCESS )
    {
        p_conn->isOpen = MV_TRUE;
        g_connectionCount++;

        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME,
            "Port opened successfully." );
    }

	/* An error occurred during port initialisation */
    else
    {
		CommsError_SetLastWin( COMMS_ERROR_OPEN, GetLastError(), g_MODULE_NAME );
		
        if ( p_conn->comFile != INVALID_HANDLE_VALUE )
        {
			MVBool success = CloseHandle( p_conn->comFile );

			if ( success == MV_FALSE )
			{
                TRACE_WRITE1_IF( LEVEL_ERROR, g_MODULE_NAME,
                    "Failed to close port following initialisation error. Windows error code: %d.",
                    GetLastError() );
			}
        }

    } /* An error occurred during port initialisation */

    return ( status );

} /* Serial_OpenPort */


static MVStatus Serial_ConfigurePort( const Connection * p_conn, 
                               MVUInt parityChecking )
{
    DCB     deviceControlSettings;
    MVBool	success;

    TRACE_ASSERT( g_isInitialised, "Module not initialised." );
    TRACE_ASSERT_VALID_ADDRESS( p_conn, "Invalid connection settings." );

    success = GetCommState( p_conn->comFile, &deviceControlSettings );

    if( success != MV_FALSE )
    {
        deviceControlSettings.BaudRate = p_conn->endPoint.baudRate;
        deviceControlSettings.fBinary = MV_TRUE;
        deviceControlSettings.fParity = parityChecking;
        deviceControlSettings.fOutxCtsFlow = MV_FALSE;
        deviceControlSettings.fOutxDsrFlow = MV_FALSE;
        
        // Changed to get MOVASim working
        deviceControlSettings.fDtrControl = DTR_CONTROL_HANDSHAKE;
        deviceControlSettings.fDsrSensitivity = MV_FALSE;
        deviceControlSettings.fTXContinueOnXoff = 1;
        deviceControlSettings.fOutX = MV_FALSE;
        deviceControlSettings.fInX = MV_FALSE;
        deviceControlSettings.fErrorChar = MV_FALSE;
        deviceControlSettings.fNull = MV_FALSE;
        
        // Changed from RTS_CONTROL_DISABLE
        deviceControlSettings.fRtsControl = RTS_CONTROL_HANDSHAKE;
        deviceControlSettings.fAbortOnError = MV_TRUE;
        deviceControlSettings.ByteSize = p_conn->endPoint.byteSize;
        deviceControlSettings.Parity = p_conn->endPoint.parity;
        deviceControlSettings.StopBits = p_conn->endPoint.stopBits;
    
        success = SetCommState( p_conn->comFile, &deviceControlSettings );
    }

    return ( success == MV_FALSE ? MV_FAILURE : MV_SUCCESS );
}


static MVStatus Serial_ApplyTimeouts( const Connection * p_conn, 
                                      MVUInt32 readIntervalTimeout )
{
    MVBool				success;
    COMMTIMEOUTS		timeOuts;
	const Timeouts *	p_connTimeOuts;

    TRACE_ASSERT( g_isInitialised, "Module not initialised." );
    TRACE_ASSERT_VALID_ADDRESS( p_conn, "Invalid connection settings." );

	p_connTimeOuts = &( p_conn->timeOuts );

    TRACE_WRITE5_IF( LEVEL_INFO, g_MODULE_NAME, 
        "Applying timeouts (ms): read interval %d, read %d, write %d, read byte %d, write byte %d.",
		readIntervalTimeout, p_connTimeOuts->readConstant, p_connTimeOuts->writeConstant, 
		p_connTimeOuts->readPerByte, p_connTimeOuts->writePerByte );
	
    success = GetCommTimeouts( p_conn->comFile, &timeOuts );
    if ( success != MV_FALSE )
    {
        timeOuts.ReadIntervalTimeout = readIntervalTimeout;
        timeOuts.ReadTotalTimeoutConstant = p_connTimeOuts->readConstant;
        timeOuts.ReadTotalTimeoutMultiplier = p_connTimeOuts->readPerByte;
        timeOuts.WriteTotalTimeoutConstant = p_connTimeOuts->writeConstant;
        timeOuts.WriteTotalTimeoutMultiplier = p_connTimeOuts->writePerByte;

		success = SetCommTimeouts( p_conn->comFile, &timeOuts);
    }

    return ( success == MV_FALSE ? MV_FAILURE : MV_SUCCESS );
}


MVStatus Serial_WriteByte( ConnectionID connID, MVByte byte )
{
    MVInt bytesWritten;

    return ( Serial_WriteBytes( connID, &byte, 1, &bytesWritten ) );
}


MVStatus Serial_ReadByte( ConnectionID connID, MVByte * p_byte )
{
    MVInt bytesRead;
	
    return ( Serial_ReadBytes( connID, p_byte, 1, &bytesRead ) );
}


MVStatus Serial_WriteBytes( ConnectionID connID, const MVByte * p_bytes, 
                           MVInt count, MVInt * p_bytesWritten )
{
    MVStatus	status = MV_SUCCESS;
    MVBool     success;
    MVUInt32   bytesWritten = 0;    
    Connection *    p_conn;

    /* Ensure that WriteBytes fails gracefully
    if parameters or module state are incorrect. */
    if ( !CommsError_CheckInitialised( "Serial_WriteBytes", g_isInitialised ) 
        || !CommsError_CheckConnectionID( "Serial_WriteBytes", connID )
        || !CommsError_CheckBytes( "Serial_WriteBytes", p_bytes ) 
        || !CommsError_CheckBytesCount( "Serial_WriteBytes", count ) ) 
    {
        return MV_FAILURE;
    }
    
    p_conn = &( g_connections[ connID ] );

	/* START CRITICAL SECTION */
    EnterCriticalSection( &( p_conn->criticalSection ) );

    if ( p_conn->isOpen )
    {
        success = WriteFile( p_conn->comFile, p_bytes, count, &bytesWritten, NULL );

        if ( success == MV_FALSE )
        {             
			CommsError_SetLastWin( COMMS_ERROR_SEND, 
				GetLastError(), g_MODULE_NAME );
            status = MV_FAILURE;
        }

		else 
		{
			if ( bytesWritten != (MVUInt32)count )
			{
				TRACE_WRITE2( g_MODULE_NAME, 
					"Send error: bytes written %d, bytes expected %d.", 
					bytesWritten, count );
				
				CommsError_SetLast( COMMS_ERROR_SEND, g_MODULE_NAME );
                status = MV_FAILURE;
			}
		}

    } /* If connection is open */

    else
    {
		TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
			"Could not write to port: port closed." );
        
        CommsError_SetLast( COMMS_ERROR_SEND_CLOSED, NULL );
        status = MV_FAILURE;
    }

    (*p_bytesWritten) = bytesWritten;

	/* END CRITICAL SECTION */
    LeaveCriticalSection( &( p_conn->criticalSection ) );

    return ( status );
}


MVStatus Serial_ReadBytes(   ConnectionID connID, 
                             MVByte * p_bytes, MVInt count, 
                             MVInt * p_bytesRead )
{
    MVStatus	    status = MV_SUCCESS;
    MVBool          success;
    MVUInt32        bytesRead = 0;
    Connection *    p_conn;

    /* Ensure that ReadBytes fails gracefully
    if parameters or module state are incorrect. */
    if ( !CommsError_CheckInitialised( "Serial_ReadBytes", g_isInitialised ) 
        || !CommsError_CheckConnectionID( "Serial_ReadBytes", connID )
        || !CommsError_CheckBytes( "Serial_ReadBytes", p_bytes ) 
        || !CommsError_CheckBytesCount( "Serial_ReadBytes", count ) ) 
    {
        return MV_FAILURE;
    }

    p_conn = &( g_connections[ connID ] );

	/* START CRITICAL SECTION */
    EnterCriticalSection( &( p_conn->criticalSection ) );

    if ( p_conn->isOpen )
    {
        success = ReadFile( p_conn->comFile, p_bytes, count, &bytesRead, NULL );

        if ( success == MV_FALSE )
        { 
            status = MV_FAILURE;
			CommsError_SetLastWin( COMMS_ERROR_RECV, 
				GetLastError(), g_MODULE_NAME );
        }

		else
		{
            /* If no bytes were read, but there was no error,
               we are at the end of file - fail without tracing */
            if ( bytesRead == 0 )
            {                
                CommsError_SetLast( COMMS_ERROR_RECV_END, NULL );
                status = MV_FAILURE;
            }
            else
            {
			    if ( bytesRead != (MVUInt32)count )
			    {
				    TRACE_WRITE2( g_MODULE_NAME, 
					    "Bytes read %d, bytes expected %d.",
					    bytesRead, count );

                    CommsError_SetLast( COMMS_ERROR_RECV_COUNT, g_MODULE_NAME );
				    status = MV_FAILURE;				   
			    }
            }
		}

    } /* If connection is open */

    else
    {
		TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
			"Could not read from port: port closed." );
        
        CommsError_SetLast( COMMS_ERROR_RECV_CLOSED, NULL );
        status = MV_FAILURE;
    }

    (*p_bytesRead) = bytesRead;

	/* END CRITICAL SECTION */
    LeaveCriticalSection( &( p_conn->criticalSection ) );

    return ( status );
}


MVBool Serial_IsOpen( ConnectionID connID )
{
    return ( g_connections[ connID ].isOpen );
}


MVInt Serial_GetPortCount( void )
{
    return ( g_connectionCount );

} /* Serial_GetPortCount */


MVStatus Serial_ClosePort( ConnectionID connID )
{    
    MVStatus       status = MV_SUCCESS;
    Connection *    p_conn;

    DEBUG_ASSERT( g_isInitialised );
    DEBUG_ASSERT( MATH_IS_IN_RANGE( connID, 0, MATH_ARRAY_LEN( g_connections ) - 1 ) );
    if ( !MATH_IS_IN_RANGE( connID, 0, MATH_ARRAY_LEN( g_connections ) - 1 ) 
        || !g_isInitialised )
    {
        TRACE_WRITE2_IF( LEVEL_ERROR, g_MODULE_NAME,
            "Serial_ClosePort failed: connection ID %d; initialised %d.", 
            connID, g_isInitialised );
		CommsError_SetLast( COMMS_ERROR_PARAM_INVALID, g_MODULE_NAME ); 
        return ( MV_FAILURE );
    }

    p_conn = &( g_connections[ connID ] );

    if ( p_conn->isOpen )
    {
        MVBool success;

        TRACE_WRITE1_IF( LEVEL_INFO, g_MODULE_NAME,
            "Closing port COM%d.", p_conn->endPoint.comPort );

        p_conn->isOpen = MV_FALSE;
        g_connectionCount--;

        success = CloseHandle( p_conn->comFile );

        if ( success == MV_FALSE )
        {
            status = MV_FAILURE;

			CommsError_SetLastWin( COMMS_ERROR_CLOSE, 
				GetLastError(), g_MODULE_NAME );
        }        

        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Port closed." );
    }

    return ( status );
}


MVStatus Serial_ClearError( ConnectionID connID )
{
    MVBool			success;
    MVUInt32		errors;
    COMSTAT			comStatus;
	Connection *    p_conn;

    TRACE_ASSERT( g_isInitialised, "Module not initialised." );
    TRACE_ASSERT( connID >= 0 && connID < MATH_ARRAY_LEN( g_connections ),
		"Connection ID invalid." );
    
	p_conn = &( g_connections[ connID ] );

    success = ClearCommError( p_conn->comFile, &errors, &comStatus );

	if ( success == MV_FALSE )
	{
		CommsError_SetLastWin( COMMS_ERROR_CLEAR_ERROR, 
				GetLastError(), g_MODULE_NAME );
	}

#if defined (_TRACE)
	else
	{
		char errorMsg[ 512 ];

		MVBool    fOOP, fOVERRUN, fPTO, fRXOVER, fRXPARITY, fTXFULL;
		MVBool    fBREAK, fDNS, fFRAME, fIOE, fMODE;

		fDNS = ( errors & CE_DNS ? MV_TRUE : MV_FALSE );
		fIOE = ( errors & CE_IOE ? MV_TRUE : MV_FALSE );
		fOOP = ( errors & CE_OOP ? MV_TRUE : MV_FALSE );
		fPTO = ( errors & CE_PTO ? MV_TRUE : MV_FALSE );
		fMODE = ( errors & CE_MODE ? MV_TRUE : MV_FALSE );
		fBREAK = ( errors & CE_BREAK ? MV_TRUE : MV_FALSE );
		fFRAME = ( errors & CE_FRAME ? MV_TRUE : MV_FALSE );
		fRXOVER = ( errors & CE_RXOVER ? MV_TRUE : MV_FALSE );
		fTXFULL = ( errors & CE_TXFULL ? MV_TRUE : MV_FALSE );
		fOVERRUN = ( errors & CE_OVERRUN ? MV_TRUE : MV_FALSE );
		fRXPARITY = ( errors & CE_RXPARITY ? MV_TRUE : MV_FALSE );

		sprintf( errorMsg, "Errors:\n\tfDNS%d\n\tfIOE %d\n\tfOOP %d\n\tfPTO %d\n\tfMODE %d\n\tfBREAK %d\n\tfFRAME %d\n\tfRXOVER %d\n\tfTXFULL %d\n\tfOVERRUN %d\n\tfRXPARITY %d\n",
			fDNS, fIOE, fOOP, fPTO, fMODE, fBREAK, fFRAME, fRXOVER, fTXFULL, fOVERRUN, fRXPARITY );

		TRACE_WRITE1_IF( LEVEL_ERROR, g_MODULE_NAME, 
			"Comms error(s) cleared: %s.", errorMsg );	

		sprintf( errorMsg, "Status:\n\tfCtsHold %d\n\tfDsrHold %d\n\tfRlsdHold %d\n\tfXoffHold %d\n\tfXoffSent %d\n\tfEof %d\n\tfTxim %d\n\tfReserved %d\n\tcbInQue %d\n\tcbOutQue %d\n",
			comStatus.fCtsHold, comStatus.fDsrHold, comStatus.fRlsdHold, comStatus.fXoffHold, comStatus.fXoffSent, comStatus.fEof, comStatus.fTxim, comStatus.fReserved, comStatus.cbInQue, comStatus.cbOutQue );

		TRACE_WRITE1_IF( LEVEL_ERROR, g_MODULE_NAME, 
			"Comms status after clearing error: %s.", errorMsg );
	}
#endif

    return ( success == MV_FALSE ? MV_FAILURE : MV_SUCCESS );    

} /* Serial_ClearError */
