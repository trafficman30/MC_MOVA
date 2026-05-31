/*
    Name:           PCError.c
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        PCMOVA error handling - used throughout the PCMOVA Kernel
                    application.

                    Note: Error reporting functions should never assert in 
                    release builds - it's better for them to fail gracefully,
                    perhaps printing something to the trace log.
                    If an error reporting function fails it shouldn't 
                    stop the program.

                    Synchronisation Note:
                    Last error codes and detail error message have been 
                    allocated thread local storage (TLS) - these error
                    codes should be on a per-thread basis.
                    Error functions are thread-safe (re-entrant).

    Last revision:  $Revision:: 7                           $
    Last changed:   $Date:: 19/05/06 13:28                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: pcerror.c $
 * 
 * *****************  Version 7  *****************
 * User: Ihenderson   Date: 19/05/06   Time: 13:28
 * Updated in $/PCMOVA
    
    *****************  Version 6  *****************
    User: Ihenderson   Date: 12/04/06   Time: 16:07
    Updated in $/PCMOVA
    
    *****************  Version 5  *****************
    User: Ihenderson   Date: 20/03/06   Time: 16:27
    Updated in $/PCMOVA
    
    *****************  Version 4  *****************
    User: Ihenderson   Date: 9/03/06    Time: 14:05
    Updated in $/PCMOVA
    
    *****************  Version 3  *****************
    User: Ihenderson   Date: 16/12/05   Time: 10:17
    Updated in $/PCMOVA
    
    *****************  Version 2  *****************
    User: Ihenderson   Date: 5/12/05    Time: 11:03
    Updated in $/PCMOVA
    
    *****************  Version 1  *****************
    User: Ihenderson   Date: 28/11/05   Time: 11:49
    Created in $/PCMOVA
*/

/*
    TODO:

    - (non-urgent) Would like to store more data about an error, 
    e.g. the actual and expected checksums for a dataset checksum error.
*/

#include <string.h>

#include "stdafx.h"

#include "PCError.h"
#include "pccontio.h"   /* For PCContIO_SendEvent */
#include "pcclock.h"    /* For PCClock_GetDateTimeStr */
#include "resource.h"

#include "errhand.h" /* For MOVA kernel error IDs */

#define g_MODULE_NAME           ("PCError")

/* Any PCMOVA errors reported to the controller start at this offset */
#define g_PCMOVA_ERROR_OFFSET   (10000)

#define g_DETAIL_MSG_LEN_MAX    (127)

#define ThreadVar               __declspec( thread )

/* Handle to this instance of the PCMOVA Kernel, 
used for getting error resource strings. */
static HINSTANCE            g_hInstance;

static ThreadVar PCError    g_lastError;
static ThreadVar WinError   g_lastErrorWin;
static ThreadVar char       g_lastDetailMessage[ g_DETAIL_MSG_LEN_MAX + 1 ];

static MVBool               g_isInitialised = MV_FALSE;


/* Resource string IDs indexed by PCMOVA error number
(see enum in PCError.h) */
static const MVInt g_errorStringIDs[] =
{
    IDS_PCMOVA_ERROR_NO_ERROR,
    IDS_PCMOVA_ERROR_ASSERTION_FAILURE,    
    IDS_PCMOVA_ERROR_ALREADY_INIT,
    IDS_PCMOVA_ERROR_OUT_OF_MEMORY,
    IDS_PCMOVA_ERROR_PARAM_INVALID,
    IDS_PCMOVA_ERROR_DATASET_OPEN,
    IDS_PCMOVA_ERROR_DATASET_READ,
    IDS_PCMOVA_ERROR_DATASET_FORMAT,
    IDS_PCMOVA_ERROR_DATASET_CLOSE,
    IDS_PCMOVA_ERROR_TASK_INIT,
    IDS_PCMOVA_ERROR_TASK_DEINIT,
    IDS_PCMOVA_ERROR_TERMINATE,
    IDS_PCMOVA_ERROR_HANDSHAKE,
    IDS_PCMOVA_ERROR_INITIALISE,
    IDS_PCMOVA_ERROR_UPDATE,    
    IDS_PCMOVA_ERROR_CONTROLLER_SEND,
    IDS_PCMOVA_ERROR_CONTROLLER_RECEIVE,        
    IDS_PCMOVA_ERROR_USER_SEND,
    IDS_PCMOVA_ERROR_USER_RECEIVE,    
    IDS_PCMOVA_ERROR_MSG_CHECKSUM,
    IDS_PCMOVA_ERROR_MSG_LENGTH,
    IDS_PCMOVA_ERROR_MSG_START,
    IDS_PCMOVA_ERROR_MSG_ID,
    IDS_PCMOVA_ERROR_MSG_TYPE,
    IDS_PCMOVA_ERROR_MSG_DATA,
    IDS_PCMOVA_ERROR_TIME,
    IDS_PCMOVA_ERROR_CONTROLLER_INIT,
    IDS_PCMOVA_ERROR_CONTROLLER_DEINIT,
    IDS_PCMOVA_ERROR_USER_INIT,
    IDS_PCMOVA_ERROR_USER_DEINIT,
    IDS_PCMOVA_ERROR_NOT_INIT,
    IDS_PCMOVA_ERROR_DATASET_VERSION,
    IDS_PCMOVA_ERROR_DATASET_CHECKSUM,
    IDS_PCMOVA_ERROR_KERNEL_FATAL,
    IDS_PCMOVA_ERROR_KERNEL_USER_FATAL,
    IDS_PCMOVA_ERROR_TASK_SUSPEND,
    IDS_PCMOVA_ERROR_TASK_RESUME
};


/* Resource string IDs indexed by MOVA kernel error number
(see enum in Errhand.h) */
static const MVInt g_kernelErrorStringIDs[] =
{
    IDS_KERNEL_ERROR_NO_ERROR,
    IDS_KERNEL_ERROR_MOVA_BACK_ON_LINE,
    IDS_KERNEL_ERROR_FAULTY_DETECTOR,
    IDS_KERNEL_ERROR_STAGE_NOT_ENDED,
    IDS_KERNEL_ERROR_INTERGREEN_NOT_ENDED,
    IDS_KERNEL_ERROR_INVALID_STAGE_DEMANDED,
    IDS_KERNEL_ERROR_WRONG_STAGE_CONFIRMED,
    IDS_KERNEL_ERROR_STAGE_STUCK_ON,
    IDS_KERNEL_ERROR_WATCHDOG_ROUTINE,
    IDS_KERNEL_ERROR_CONTROLLER_READY_BIT_OFF,
    IDS_KERNEL_ERROR_CONTROLLER_READY_BIT_ON,
    IDS_KERNEL_ERROR_PROGRAM_ERROR,
    IDS_KERNEL_ERROR_CHECKSUM_ERROR,
    IDS_KERNEL_ERROR_RELOADING_ERROR,
    IDS_KERNEL_ERROR_MULTIPLE_STAGE_CONFIRMS,
    IDS_KERNEL_ERROR_MOVA_RESTARTED,
    IDS_KERNEL_ERROR_DIVIDE_ERROR,
    IDS_KERNEL_ERROR_EMPTY_REPOSITORY_AREA,
    IDS_KERNEL_ERROR_REPOSITORY_CHECKSUM,
    IDS_KERNEL_ERROR_DEFAULT_LOADED
};


static MVSize PCError_BuildKernelMessage( char *, const char *, MVUInt16, MVInt );

#if defined (_TRACE)
    static void PCError_Output_TRACE( PCError, WinError, const char *, const char * );
#else
    #define    PCError_Output_TRACE( error, errorWin, detailMessage, source )
#endif


/*
    Name:           PCError_Initialise
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Initialises the PCMOVA error handling module
    Inputs:         
    Returns:        
    Remarks:
*/
void PCError_Initialise( void )
{
    if ( !g_isInitialised )
    {
        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Initialising error logger..." );

        g_hInstance = GetModuleHandle( "PCMOVA.EXE" );

        g_lastError = PCMOVA_ERROR_NO_ERROR;
        g_lastErrorWin = xg_WIN_ERROR_INVALID;

        g_lastDetailMessage[0] = '\0';

        g_isInitialised = MV_TRUE;

        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Error logger initialised." );
    }

} /* PCError_Initialise */


/*
    Name:           PCError_SetLast
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Sets the code for the last error that occurred
    Inputs:         The PCMOVA error code 
                    (has a corresponding error message string 
                    in the PCMOVA resource file), 
                    An optional detail message
                    An optional source string (currently 
                    only used for _TRACE)
    Returns:        
    Remarks:
*/
void PCError_SetLast( PCError error, const char * detailMessage, 
                     const char * source )
{
    PCError_SetLastWin( error, xg_WIN_ERROR_INVALID, detailMessage, source );

} /* PCError_SetLast */


/*
    Name:           PCError_SetLastWin
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Sets the code for the last error that occurred.
                    Can be used to store a Windows error code.
    Inputs:         The PCMOVA error code 
                    (has a corresponding error message string 
                    in the PCMOVA resource file), 
                    A Windows error code
                    An optional detail message
                    An optional source string (currently 
                    only used for _TRACE)
    Returns:        
    Remarks:        The Windows error code will be xg_WIN_ERROR_INVALID
                    if the error didn't come from a Windows function.
*/
void PCError_SetLastWin( PCError error, WinError errorWin, 
                        const char * detailMessage, 
                        const char * source )
{
    DEBUG_ASSERT( MATH_IS_IN_RANGE( error, 
        PCMOVA_ERROR_FIRST, PCMOVA_ERROR_LAST ) );

    g_lastError = error;
    g_lastErrorWin = errorWin;
    
    /* Store detail message, truncated if necessary */
    if ( detailMessage != NULL )
    {
        MVSize detailMessageLen = strlen( detailMessage );
        if ( detailMessageLen > g_DETAIL_MSG_LEN_MAX )
        {
            strncpy( g_lastDetailMessage, detailMessage, g_DETAIL_MSG_LEN_MAX );
            g_lastDetailMessage[ g_DETAIL_MSG_LEN_MAX ] = '\0';
        }
        else
        {
            strncpy( g_lastDetailMessage, detailMessage, detailMessageLen );
            g_lastDetailMessage[ detailMessageLen ] = '\0';
        }
    }

    /* No detail message */
    else
    {
        g_lastDetailMessage[0] = '\0';
    }

    if ( source != NULL )
    {
        PCError_Output_TRACE( g_lastError, g_lastErrorWin, detailMessage, source );
    }

} /* PCError_SetLastWin */


/*
    Name:           PCError_Output_TRACE
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Outputs the PCMOVA error to the trace log
                    and asserts in debug builds only.
    Inputs:         The PCMOVA error code 
                    A Windows error code
                    A detail message
                    A source string 
    Returns:        
    Remarks:        The Windows error code will be xg_WIN_ERROR_INVALID
                    if the error didn't come from a Windows function.
*/
#if defined (_TRACE)
static void PCError_Output_TRACE( PCError error, WinError errorWin, 
                                 const char * detailMessage, 
                                 const char * source )
{
	UNUSED(error);
	UNUSED(errorWin);

    char errorMsg[ xg_ERROR_MSG_LEN_MAX + 1 ];

    PCError_BuildMessage( g_lastError, g_lastErrorWin, 
        errorMsg, sizeof( errorMsg ) );

    if ( detailMessage == NULL )
    {
        TRACE_WRITE3_IF( LEVEL_ERROR, source, 
            "%s\nError codes: pcmova %d, windows %d.",
            errorMsg, g_lastError, g_lastErrorWin );
    }
    else
    {
        TRACE_WRITE4_IF( LEVEL_ERROR, source, 
            "%s\nError codes: pcmova %d, windows %d.\nDetail message: %s",
            errorMsg, g_lastError, g_lastErrorWin, detailMessage );
    }

    /* Stop the program, but only in debug builds */
    DEBUG_ASSERT( MV_FALSE );

} /* PCError_Output_TRACE */
#endif /* (_TRACE) */


/*
    Name:           PCError_GetLast
    Author (Date):  ihenderson, 28/09/05
    Platform:       PC
    Purpose:        Returns the PCMOVA error code for the 
                    last error that occurred
    Inputs:         
    Returns:        The PCMOVA error code
    Assumptions:    
    Effects:        
*/
PCError PCError_GetLast( void )
{
    return ( g_lastError );
    
} /* PCError_GetLast */


/*
    Name:           PCError_GetLastWin
    Author (Date):  ihenderson, 28/09/05
    Platform:       PC
    Purpose:        Returns a Windows error code for the 
                    last PCMOVA error that occurred (may be 
                    xg_WIN_ERROR_INVALID if the last PCMOVA error
                    didn't come from a Windows function)
    Inputs:         
    Returns:        The Windows error code
    Remarks:
*/
WinError PCError_GetLastWin( void )
{
    return ( g_lastErrorWin );
    
} /* PCError_GetLastWin */


/*
    Name:           PCError_BuildLastMessage
    Author (Date):  ihenderson, 29/11/05
    Platform:       PC
    Purpose:        Returns a string description for the last PCMOVA error
                    to occur.  Unlike PCError_BuildMessage, 
                    PCError_BuildLastMessage includes any detail message
                    and doesn't require error codes.
    Inputs:         Buffer for error message
                    Length of buffer
    Returns:        Error string length
    Remarks:        
*/
MVSize PCError_BuildLastMessage( char * msgBuf, MVSize msgBufLenMax )
{
    DEBUG_ASSERT_VALID_ADDRESS( msgBuf );
    DEBUG_ASSERT( msgBufLenMax > 0 );
    if ( msgBuf == NULL || msgBufLenMax <= 0 )
    {
        TRACE_WRITE0_IF( LEVEL_ERROR, g_MODULE_NAME,
            "PCError_BuildLastMessage passed an invalid buffer." );
        return 0;
    }

    PCError_BuildMessage( g_lastError, g_lastErrorWin, msgBuf, msgBufLenMax );

    // If there's a detail message, add it
    if ( g_lastDetailMessage[0] != '\0' )
    {
        MVSize msgLen = strlen( msgBuf );
        MVSize detailMsgLen = strlen( g_lastDetailMessage );

        // Add new line character before detail message
        if ( msgLen + 1 < msgBufLenMax )
        {
            strncat( msgBuf, "\n", 1 );
            msgLen++;
        }
            
        // Add detail message, truncated if necessary
        if ( msgLen + detailMsgLen < msgBufLenMax )
        {
            strncat( msgBuf, g_lastDetailMessage, detailMsgLen );
            msgBuf[ msgLen + detailMsgLen ] = '\0';
        }
        else
        {
            strncat( msgBuf, g_lastDetailMessage, msgBufLenMax - msgLen );
            msgBuf[ msgBufLenMax - 1 ] = '\0';
        }
    }

    return ( strlen( msgBuf ) );

} /* PCError_BuildLastMessage */


/*
    Name:           PCError_BuildMessage
    Author (Date):  ihenderson, 11/10/05
    Platform:       PC
    Purpose:        Returns a string description for the given PCMOVA
                    and Windows error codes
    Inputs:         PCMOVA and Windows Error codes
                    Buffer for error message
                    Length of buffer
    Returns:        Error string length    
    Remarks:        Uses the MVError module in MVUtils.lib to
                    decode the Windows error code.
*/
MVSize PCError_BuildMessage( PCError error, WinError errorWin,
                            char * msgBuf, MVSize msgBufLenMax )
{
    MVSize messageLen;

    DEBUG_ASSERT_VALID_ADDRESS( msgBuf );
    DEBUG_ASSERT( msgBufLenMax > 0 );
    if ( msgBuf == NULL || msgBufLenMax <= 0 )
    {
        TRACE_WRITE0_IF( LEVEL_ERROR, g_MODULE_NAME,
            "PCError_BuildMessage passed an invalid buffer." );
        return 0;
    }
    
    DEBUG_ASSERT( MATH_IS_IN_RANGE( error, 
        PCMOVA_ERROR_FIRST, PCMOVA_ERROR_LAST ) );

    /* If the error ID is valid, get the message from the resource file. */
    if ( MATH_IS_IN_RANGE( error, PCMOVA_ERROR_FIRST, PCMOVA_ERROR_LAST ) )
    {        
        messageLen = LoadString( g_hInstance, 
            g_errorStringIDs[ error ], msgBuf, (int)(msgBufLenMax) );
    }
    else
    {
        messageLen = 0;
        SetLastError( ERROR_INVALID_PARAMETER );
    }

    /* If the message could not be found, put in a default error message */
    if ( messageLen == 0 )
    {
        MVError_BuildDefaultMessage( msgBuf, msgBufLenMax, GetLastError() );
    }

    /* Add any Windows error message */
    if ( errorWin != xg_WIN_ERROR_INVALID
        && msgBufLenMax > messageLen + 2 ) /* + 2 - null and new line */
    {    
        strncat( msgBuf, "\n", 1 );
        messageLen++;

        MVError_DecodeWin( errorWin,
            msgBuf + messageLen, msgBufLenMax - messageLen );
    }

    return ( strlen( msgBuf ) );

} /* PCError_BuildMessage */


/*
    Name:           PCError_SetLastKernel
    Author (Date):  ihenderson, 30/11/05
    Platform:       PC
    Purpose:        "Sets" the last MOVA kernel error
                    by sending it to the Controller.
                    Kernel errors can be viewed using MOVA Comm,
                    and selecting the "DE" option from the MOVA main
                    menu.  It is also desirable to inform the
                    PCMOVA Controller of such errors (and maybe real
                    controllers too) - unlike other information 
                    available through MOVA Comm, MOVA kernel errors are 
                    serious enough that the controller should be notified 
                    regardless of whether MOVA Comm is running.
                    Errors are sent to the controller as event messages.
    Inputs:         Kernel error ID (from Errhand.h)
                    Optional error data - e.g. the number of a
                    faulty detector or stuck stage.
                    Optional detail message (useful for "assert" type
                    failures in the MOVA kernel, such as divide-by-zero
                    messages - the detail message is intended as a 
                    diagnostic aid for the programmer).
                    Length of detail message (or zero if there isn't one)
    Returns:        
    Remarks:
*/
void PCError_SetLastKernel( MVUInt16 errorID, MVInt errorData, 
                           const char * detailMsg, MVSize detailMsgLen )
{
    MVSize  formatMessageLen;
    MVSize  kernelMessageLen;
    char    formatMessage[ xg_ERROR_MSG_LEN_MAX + 1 ];
    char    kernelMessage[ xg_ERROR_MSG_LEN_MAX + 1 ];

    DEBUG_ASSERT( MATH_IS_IN_RANGE( errorID, 0, ERROR_IDS_NUM - 1 ) );

    /* If the error ID is valid, get the message format string 
    from the resource file. */
    if ( MATH_IS_IN_RANGE( errorID, 0, ERROR_IDS_NUM - 1 ) )
    {        
        formatMessageLen = LoadString( g_hInstance, 
            g_kernelErrorStringIDs[ errorID ], 
            formatMessage, sizeof( formatMessage ) );        
    }
    else
    {
        formatMessageLen = 0;
        SetLastError( ERROR_INVALID_PARAMETER );
    }

    /* If the resource string could not be found, 
    put in a default error message with the error ID */
    if ( formatMessageLen == 0 )
    {
        kernelMessageLen = MVError_BuildDefaultMessage( kernelMessage, 
            sizeof( kernelMessage ), GetLastError() );

        sprintf( kernelMessage + kernelMessageLen, 
            "Error ID was: %d.", errorID );
    }

    /* Otherwise a valid format string was loaded from the resource file */
    else
    {
        kernelMessageLen = PCError_BuildKernelMessage( kernelMessage, 
            formatMessage, errorID, errorData );
    }

    /* Add any detail message, truncated if necessary */
    if ( detailMsg != NULL
        && xg_ERROR_MSG_LEN_MAX > kernelMessageLen + 1 ) /* + 1 - new line */
    {    
        strncat( kernelMessage, "\n", 1 );
        kernelMessageLen++;

        if ( kernelMessageLen + detailMsgLen <= xg_ERROR_MSG_LEN_MAX )
        {
            strncpy( kernelMessage + kernelMessageLen, detailMsg, detailMsgLen );
            kernelMessageLen += detailMsgLen;
        }
        else
        {
            strncpy( kernelMessage + kernelMessageLen, detailMsg, 
                xg_ERROR_MSG_LEN_MAX - kernelMessageLen );
            kernelMessageLen = xg_ERROR_MSG_LEN_MAX;
        }

        kernelMessage[ kernelMessageLen ] = '\0';
    }

    TRACE_WRITE1_IF( LEVEL_ERROR, g_MODULE_NAME, 
        "MOVA Kernel error: %s.", kernelMessage );

    /* Send an error event to the controller with the error ID and message */
    PCContIO_SendEvent( EVENT_TYPE_ERROR, errorID, 
        kernelMessage, kernelMessageLen );

} /* PCError_SetLastKernel */


/*
    Name:           PCError_BuildKernelMessage
    Author (Date):  ihenderson, 01/12/05
    Platform:       PC
    Purpose:        Builds an error message for an error
                    that occurred in the MOVA kernel.
                    All messages include date and time,
                    many include extra data describing the error.
    Inputs:         Message buffer to use when building 
                    the message - should be at least
                    xg_ERROR_MSG_LEN_MAX.
                    Kernel error ID (from Errhand.h)
                    Format string for the error message.
                    Optional error data - e.g. the number of a
                    faulty detector or stuck stage (depends on error ID).
    Returns:        The number of characters copied to the message buffer
                    (zero on error).
    Assumptions:    That the message buffer is big enough.
                    Caller should ensure it's at least xg_ERROR_MSG_LEN_MAX.
    Effects:        
*/
static MVSize PCError_BuildKernelMessage( char * message, const char * format,
                                         MVUInt16 errorID, MVInt data )
{
    char    dateTime[ xg_DATE_TIME_STR_LEN_MAX + 1 ];
    MVSize  dateTimeLen;

    DEBUG_ASSERT_VALID_ADDRESS( format );
    DEBUG_ASSERT_VALID_ADDRESS( message );
    if ( format == NULL || message == NULL )
    {
        TRACE_WRITE0_IF( LEVEL_ERROR, g_MODULE_NAME,
            "PCError_BuildKernelMessage passed an invalid string." );
        return 0;
    }

    /* Get the current MOVA date and time (common to all error messages) */
    dateTimeLen = PCClock_GetDateTimeStr( dateTime, sizeof( dateTime ) );
    if ( dateTimeLen == 0 )
    {
        strcpy( dateTime, xg_DATE_TIME_STR_ERROR );
    }

    /* Build the error string according to the error ID */
    switch ( errorID )
    {
        /* Errors with date/time, error count */
        case ERROR_ID_MOVA_BACK_ON_LINE:
        case ERROR_ID_INTERGREEN_NOT_ENDED:
        case ERROR_ID_CONTROLLER_READY_BIT_OFF:
        case ERROR_ID_CONTROLLER_READY_BIT_ON:
        /* Unlike in embedded version, not outputting the watchdog value
        (Tcomshr->io[18]).  This value is copied directly to the error string
        in Output.c, which doesn't look right to me - it should have been stored
        in the error log at the time when the error occurred - its value may 
        have changed by the time output() gets run. */
        case ERROR_ID_WATCHDOG_ROUTINE:
        {
            MVInt errorCount = data;

            sprintf( message, format, dateTime, errorCount );
            break;
        }

        /* Errors with running stage, demanded stage, date/time, error count */
        case ERROR_ID_STAGE_NOT_ENDED:
        case ERROR_ID_INVALID_STAGE_DEMANDED:
        case ERROR_ID_WRONG_STAGE_CONFIRMED:
        {
            /* Code in Output.c to extract data, which we're using below:
            s = Terrlog->ierr[3][nNe-1]/1000;
            d = Terrlog->ierr[3][nNe-1]/100-s*10;
            i = Terrlog->ierr[3][nNe-1]-(d*100+s*1000);
            */

            MVInt stageRunning = data / 1000;
            MVInt stageDemanded = data / 100 - stageRunning * 10;
            MVInt errorCount = data - 
                ( stageDemanded * 100 + stageRunning * 1000 );

            /* Stage 10 should be encoded as stage 0 - currently there's a bug at
            S28 in Genstg() though - see M5_CRN0013 in the change control log. */
            if ( stageDemanded == 0 )
            {
                stageDemanded = 10;
            }

            sprintf( message, format, 
                stageRunning, stageDemanded, dateTime, errorCount );
            break;
        }

        /* Error with detector number, date/time */
        case ERROR_ID_FAULTY_DETECTOR:
        {
            /* Code in Output.c to extract data, which we're using below:
            i = Terrlog->ierr[3][nNe-1]/100;
            */

            MVInt detectorID = data / 100;

            sprintf( message, format, detectorID, dateTime );
            break;
        }
        
        /* Error with running stage, time stuck, date/time, error count */
        /* DEPRECATED - this error message shouldn't be logged by the kernel. */
        case ERROR_ID_STAGE_STUCK_ON: 
        {
            /* Code in Output.c to extract data, which we're using below:
            iold = Terrlog->ierr[3][nNe-1]/1000;
            t = Terrlog->ierr[3][nNe-1]/100-iold*10;
            Terrlog->ierr[3][nNe-1]-(iold*1000+t*100)
            */

            MVInt stageRunning = data / 1000;
            MVInt timeStuck = data / 100 - stageRunning * 10;
            MVInt errorCount = data -
                ( stageRunning * 1000 + timeStuck * 100 );

            sprintf( message, format, 
                stageRunning, timeStuck, dateTime, errorCount );
            break;
        }

        /* Error with date/time, program error code, error handler error code */
        /* Note: this error isn't currently used anywhere in the kernel. 
        It could be used for other assertion-type failures besides DBZ. */
        case ERROR_ID_PROGRAM_ERROR:
        {
            /* Code in Output.c to extract data, which we're using below:
            d = Terrlog->ierr[3][nNe-1]/100;
            s = Terrlog->ierr[3][nNe-1]-100*d; // D=CODE,S=RVL
            */

            MVInt programErrorCode = data / 100;
            MVInt errorHandlerCode = data - 100 * programErrorCode;

            sprintf( message, format, 
                dateTime, programErrorCode, errorHandlerCode );
            break;
        }

        /* Errors with date/time */
        case ERROR_ID_CHECKSUM_ERROR:
        case ERROR_ID_MULTIPLE_STAGE_CONFIRMS:
        case ERROR_ID_MOVA_RESTARTED:
        /* No error, but output an associated message anyway
        (IDS_KERNEL_ERROR_NO_ERROR) - might be useful */
        case 0:
        {
            sprintf( message, format, dateTime );
            break;
        }

        /* Errors with dataset plan number, date/time */
        case ERROR_ID_RELOADING_ERROR:
        case ERROR_ID_EMPTY_REPOSITORY_AREA:
        case ERROR_ID_REPOSITORY_CHECKSUM:
        case ERROR_ID_DEFAULT_LOADED:
        {
            MVInt datasetPlanID = data;

            sprintf( message, format, datasetPlanID, dateTime );
            break;
        }
        
        /* Error with date/time, location code (detail message gives further data) */
        case ERROR_ID_DIVIDE_ERROR:
        {
            MVInt divideErrLocation = data;

            sprintf( message, format, dateTime, divideErrLocation );
            break;
        }

        /* Error ID was unrecognised - print the format message as-is */
        default:
        {
            sprintf( message, format );
            DEBUG_ASSERT( MV_FALSE );
            break;
        }

    } /* switch ( errorID ) */

    return ( strlen( message ) );

} /* PCError_BuildKernelMessage */


/*
    Name:           PCError_ExitKernel
    Author (Date):  ihenderson, 12/12/05
    Platform:       PC
    Purpose:        Exits the application following
                    a fatal error in the MOVA kernel.
                    Typically this is a divide-by-zero,
                    for which an error message will
                    already have been sent to the Controller
                    using PCError_SetLastKernel.
    Inputs:         A PCMOVA error code used to indicate 
                    the type/location of the kernel fatal error.
    Returns:        THIS FUNCTION DOES NOT RETURN.
    Remarks:
*/
void PCError_ExitKernel( PCError errorID )
{
    char    exitMsg[ xg_ERROR_MSG_LEN_MAX + 1 ];
    MVSize  exitMsgLen;

    exitMsgLen = PCError_BuildMessage( errorID, 
        xg_WIN_ERROR_INVALID, exitMsg, sizeof( exitMsg ) );

    TRACE_WRITE2_IF( LEVEL_ERROR, g_MODULE_NAME, 
        "%s PCMOVA error ID: %d.", exitMsg, errorID );

    /* Send an error event to the controller with the error ID and message */
    PCContIO_SendEvent( EVENT_TYPE_ERROR, (MVUInt16)(g_PCMOVA_ERROR_OFFSET + errorID), 
        exitMsg, exitMsgLen );    

    /* PCKernel_DeInitialise is registered using atexit, 
    so PCMOVA should exit gracefully */
    exit( EXIT_FAILURE );

} /* PCError_ExitKernel */


#if 0
/* TEMP: TEST: simulate errors occurring during running of the MOVA kernel. */
void PCError_TestKernelMessages( void )
{
    int lrfact = 99999;
    DivideError((int)(10000/lrfact), 513,
        "lrfact was %d, link number was %d", 0, 9 );

    Errhand_LogMOVAError( ERROR_ID_FAULTY_DETECTOR,             11 * 100 ); // Det 11
    
    Errhand_LogMOVAError( ERROR_ID_REPOSITORY_CHECKSUM,         1 );        // Dataset (plan) 1
    Errhand_LogMOVAError( ERROR_ID_DEFAULT_LOADED,              2 );        // Dataset (plan) 2
    Errhand_LogMOVAError( ERROR_ID_RELOADING_ERROR,             3 );        // Dataset (plan) 3
    Errhand_LogMOVAError( ERROR_ID_EMPTY_REPOSITORY_AREA,       4 );        // Dataset (plan) 4

    // Stage 5 running, stage 6 demanded, error count 14
    Errhand_LogMOVAError( ERROR_ID_STAGE_NOT_ENDED,             5*1000 + 6*100 + 14 );
    
    // Stage 7 running, stage 8 demanded, error count 15
    Errhand_LogMOVAError( ERROR_ID_INVALID_STAGE_DEMANDED,      7*1000+8*100 + 15 );    

    // Stage 9 running, stage 10 demanded, error count 16
    Errhand_LogMOVAError( ERROR_ID_WRONG_STAGE_CONFIRMED,       9*1000+10*100 + 16 );    

    Errhand_LogMOVAError( ERROR_ID_WATCHDOG_ROUTINE,            12 );   // Error count 12
    Errhand_LogMOVAError( ERROR_ID_MOVA_BACK_ON_LINE,           13 );   // Error count 13
    Errhand_LogMOVAError( ERROR_ID_INTERGREEN_NOT_ENDED,        17 );   // Error count 17
    Errhand_LogMOVAError( ERROR_ID_CONTROLLER_READY_BIT_OFF,    18 );   // Error count 18
    Errhand_LogMOVAError( ERROR_ID_CONTROLLER_READY_BIT_ON,     19 );   // Error count 19

    // No data for these errors
    Errhand_LogMOVAError( ERROR_ID_CHECKSUM_ERROR,              0 );
    Errhand_LogMOVAError( ERROR_ID_MULTIPLE_STAGE_CONFIRMS,     0 );
    Errhand_LogMOVAError( ERROR_ID_MOVA_RESTARTED,              0 );

    // Program error code 20, error handler return value 21
    Errhand_LogMOVAError( ERROR_ID_PROGRAM_ERROR,               20*100 + 21 );

    // "No error" message
    Errhand_LogMOVAError( 0, 100 );

    // Try to log an invalid error (should cause an assert in 
    // debug builds or trace write in release builds)
    Errhand_LogMOVAError( ERROR_ID_DEFAULT_LOADED + 1, 999 );

} /* PCError_TestKernelMessages */
#endif
