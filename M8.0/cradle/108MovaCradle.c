//
//  Project     Chameleon
//
//  Copyright 	Dynniq UK Ltd 2018
//
//  All rights reserved
//
//  The information contained herein is the property of Peek Traffic Limited
//  and is supplied without any liability for errors or omissions. No part
//  may be reproduced or copied except as authorised by contract or other
//  written permission. The copyright and foregoing restriction on
//  reproduction and use extend to all media in which the information may be
//  embodied.
//
//!  \file    108MovaCradle.c
//!	\brief   Abstraction layer and support functions for MOVA M8 kernel
//!	\author  Andrew Leigh
//!	\date    27-May-2018
//

static char MovaCradle_c_rcsid[] = "$Id: ";

/* --------------------------------------------------------------------------- *
 *                     INCLUDE FILES                                           *
 * --------------------------------------------------------------------------- *
*/
#include "108MovaCradle.h"
#include "108MovaLib.h"
#include "108MovaFaults.h"

void UploadData(int nChosen, int nWx);
void TryForces(int LU);

/*
 * --------------------------------------------------------------------------- *
 *                     SYMBOLIC CONSTANTS                                      *
 * --------------------------------------------------------------------------- *
*/
//#define TRACE_MOVA_SOCKET
/* --------------------------------------------------------------------------- *
 *                     MACRO DEFINITIONS                                       *
 * --------------------------------------------------------------------------- *
*/

/*
 * --------------------------------------------------------------------------- *
 *                     TYPE DEFINITIONS                                        *
 * --------------------------------------------------------------------------- *
 */


/* --------------------------------------------------------------------------- *
 *                     GLOBAL DATA REFERENCES                                  *
 * --------------------------------------------------------------------------- *
*/


// PCMOVA tracing support
static TraceLevel   g_traceLevel;
static MVBool       g_isInitialised = MV_FALSE;
static CommsType    g_commsType = COMMS_TYPE_SERIAL;


MODULE GModule;          // Module number of task
int    GInstance;        // Instance number of task
int    GMySocket;        // Socket number of task
//int    GMyMovaMsgSocket; // Socket for communicating with MovaLib
equip_id_text_t gMovaEqId;
equip_id_t gMovaEqIdNum;
struct mova_if_t *gMovaIf;      // Mova Interface (used for OMU communication)
unsigned char lLineDropped; // non zero if the modem line has dropped

char __thread *gCHAR_HANDSETptr = NULL;
char __thread *gCHAR_MODEMptr = NULL;

// pthreads

pthread_t Mova_pthead;
pthread_t TrlUI_pthead;
pthread_t Term_pthread;
pthread_t Output_pthread;
pthread_t sig_thread;
pthread_t Dataset_thread;
pthread_t Aml_thread;
pthread_attr_t  Mova_attr;
pthread_attr_t  TrlUI_attr;
pthread_attr_t  Term_attr;
pthread_attr_t  Output_attr;
pthread_attr_t  Signal_attr;
pthread_attr_t  Dataset_attr;
pthread_attr_t  Aml_attr;

// condition variables

pthread_cond_t      SuspendOutput;
pthread_condattr_t  SuspOut_attr;
pthread_cond_t	    tick;
pthread_condattr_t  tick_attr;
pthread_cond_t	    longtick;
pthread_condattr_t  longtick_attr;
pthread_cond_t      GetOutput;
pthread_condattr_t  GetOutput_attr;
pthread_cond_t      DatasetActive;
pthread_condattr_t  DatasetActive_attr;

// mutexes

pthread_mutex_t     SuspOut_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutexattr_t SuspOut_mutex_attr;
pthread_mutex_t	    tick_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutexattr_t tick_mutex_attr;
pthread_mutex_t	    longtick_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutexattr_t longtick_mutex_attr;
pthread_mutex_t     GetOutput_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutexattr_t GetOutput_mutex_attr;
pthread_mutex_t     Ringbuf_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutexattr_t Ringbuf_mutex_attr;
//pthread_mutex_t     Ringbuf_mutex = PTHREAD_MUTEX_INITIALIZER;
//pthread_mutexattr_t Ringbuf_mutex_attr;
pthread_mutex_t     Dataset_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutexattr_t Dataset_mutex_attr;
pthread_mutex_t     TrlUi_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutexattr_t TrlUi_mutex_attr;
pthread_mutex_t     Aml_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutexattr_t Aml_mutex_attr;



// keys

static pthread_key_t key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;

/*
 * --------------------------------------------------------------------------- *
 *                   LOCAL FUNCTIONS                                           *
 *                   Abstraction Layer for MOVA Kernel                         *
 * --------------------------------------------------------------------------- *
 */

#ifndef INCLUDE_PCDATSET

//!  \fn     PCDatSet_GetDefault()
//!  \brief  Load the data set
//!	\param	movaData MOVA_CONST_DATA_STRUCT * MOVA data set structure
//!  \retval void none
//
void PCDatSet_GetDefault(MOVA_CONST_DATA_STRUCT *movaData )
{
    // Load the data set
    CMN_TRACE_ENTRY;
    CMN_TRACE_EXIT;
}   // PCDatSet_GetDefault()
#endif // INCLUDE_PCDATSET



//!  \fn     Trace_Assert()
//!  \brief  if val MV_FALSE in MOVA code, display reason string
//!	\param	val bool
//!	\param	expression const char *
//!	\param	message const char *
//!	\param	filename const char * 
//!	\param	line int
//!  \retval void none
//
void Trace_Assert(MVBool val, const char *expression,  const char *file,
                  MVInt32 line,const char *message )
{
    // assumption: val MV_FALSE implies send message
    // TBC check PCMOVA code when receive correct header

    CMN_TRACE_ENTRY;

    if (val == MV_FALSE)
    {
        CmnTrace(GTraceFileId,"%s: Stream:%s, %s %s %s %d", __FUNCTION__, gMovaEqId, expression, message, file, line);
    }
    CMN_TRACE_EXIT;
}   // Trace_Assert()



//!  \fn     Trace_AssertNull()
//!  \brief  val =0 implies display reason string then exit
//!	\param	val bool
//!	\param	expression const char *
//!	\param	message const char *
//!	\param	filename const char * 
//!	\param	line int
//!  \retval void none
//
void Trace_AssertNull(const void *val, const char *expression,  const char *file,
                      MVInt32 line,const char *message )
{
    // assumption: *val =0 implies send message
    // TBC check PCMOVA code when receive correct header

    CMN_TRACE_ENTRY;

    if (val == NULL)
    {
        CmnTrace(GTraceFileId,"%s: Stream:%s, %s %s %s %d", __FUNCTION__, gMovaEqId, expression, message, file, line);
    }
    CMN_TRACE_EXIT;
}   // Trace_AssertNull()



//!  \fn     Trace_RaiseAssert()
//!  \brief  exit with message or abort using Assert()
//!	\param	message const char *
//!  \retval void none
//
static void Trace_RaiseAssert( const char * message )
{
    CMN_TRACE_ENTRY;

#if defined (_DEBUG)
    /* Exit immediately - assert() calls abort()
            (should therefore only be used in debug builds) */
    assert( MV_FALSE );
#else
    /* Exit gracefully - using exit(), allowing cleanup. */
#if defined (WIN32)
    Trace_RecommendExit_WIN32( message );
#elif defined (_CONSOLE)
    Trace_RecommendExit_CONSOLE( message );
#else
    // restart of chameleon implied with DC_ERROR
    DiagLog(DC_WARNING, "%s: MOVA:%s, Trace_RecommendExit: %s",__FUNCTION__, gMovaEqId, message);
    Trace_Exit();
#endif
#endif
    CMN_TRACE_EXIT;

} /* Trace_RaiseAssert */



#define g_EXIT_MSG         ("Program exited following assertion failure.")

//!  \fn     Trace_Exit()
//!  \brief  send message and exit
//!	
//!  \retval  void none
//
static void Trace_Exit( void )
{
    CMN_TRACE_ENTRY;

    CmnTrace(GTraceFileId,"%s: MOVA:%s, %s", __FUNCTION__, gMovaEqId, g_EXIT_MSG);

    /* exit() calls any cleanup functions registered using atexit() */
    exit( EXIT_FAILURE );

    CMN_TRACE_EXIT;

} /* Trace_Exit */




//!  \fn     Trace_WriteLinelf()
//!  \brief  trace message to chameleon common trace
//!	\param	level TraceLevel
//!	\param	prefix const char *
//!	\param	message const char *
//!	\param	va_args const char * 
//!  \retval void none
//
void Trace_WriteLineIf( TraceLevel level, const char * prefix,
                        const char * message, ... )
{
    CMN_TRACE_ENTRY;

    if ( level <= g_traceLevel )
    {
        char traceBuffer[ 1024 ];
        char formatBuffer[ 1024 ];

        va_list  argList;
        va_start( argList, message );

        if ( prefix != NULL )
        {
            sprintf( formatBuffer, "%s: %s\r\n", prefix, message );
        }
        else
        {
            sprintf( formatBuffer, "%s\r\n", message );
        }
        vsprintf( traceBuffer, formatBuffer, argList );

        CmnTrace(GTraceFileId,"%s: MOVA:%s, %s", __FUNCTION__, gMovaEqId, traceBuffer);

        va_end( argList );
    }
    CMN_TRACE_EXIT;

} /* Trace_WriteLineIf */



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

//!  \fn     PCUserIO_IsConnected()
//!  \brief  This is a cut-down version of pcuserio.c's version.
//!		return whether the chameleon's engineer's terminal socket
//!      is connected
//!	
//!  \retval  MVBool MV_FALSE, MV_TRUE
//!
MVBool PCUserIO_IsConnected( void )
{
    MVBool isConnected;
    mova_io_t *movaIo;

    CMN_TRACE_ENTRY;

    switch ( g_commsType )
    {
        case COMMS_TYPE_SERIAL:
        {
            // check chameleon's engineer's terminal socket
            movaIo = &GIOshmData->mova_io[GInstance];
            if ((movaIo->movaIf).LComms == MOVAIF_COMMS_TO_OMU)
            {
                isConnected = MV_FALSE;
            }
            else
            {
                isConnected = MV_TRUE;
            }
            break;
        }

        case COMMS_TYPE_TCP:
        {
            //isConnected = TCPClient_IsConnected( CONNECTION_ID_USER );
            isConnected = MV_FALSE;
            break;
        }

        default:
        {
            isConnected = MV_FALSE;
            TRACE_ASSERT( MV_FALSE, "Communications type is invalid." );
            break;
        }
    }

    CMN_TRACE_EXIT;
    return ( isConnected );

} /* PCUserIO_IsConnected */



//!  \fn     PCTasks_SuspendOutput(void)
//!  \brief  Suspend the output task until next second run time
//!
//!  \retval  void none
//!
void PCTasks_SuspendOutput(void)
{
    // Suspend the output task until next second run time
    CMN_TRACE_ENTRY;

    pthread_mutex_lock(&SuspOut_mutex);
    pthread_cond_wait(&SuspendOutput, &SuspOut_mutex);
    pthread_mutex_unlock(&SuspOut_mutex);

    CMN_TRACE_EXIT;
}   // PCTasks_SuspendOutput()



//!  \fn     PCTasks_Resume()
//!  \brief  Resume  the output task until next second run time
//! This differs from PCMOVA as all suspended tasks are resumed
//! PCMOVA currently resumes all tasks in turn from within pctasks.c and the
//! MOVA kernel does a similar job for Siemens.  For simplisity resume all
//! tasks at once.
//!
//!  \param  enTask not used.  Just there for pcmova compatibility
//!  \retval  void none
//!
void PCTasks_Resume(psMOVA_TASK enTask)
{
    // Resume the output task
    CMN_TRACE_ENTRY;

    pthread_mutex_lock(&SuspOut_mutex);
    pthread_cond_broadcast(&SuspendOutput);
    pthread_mutex_unlock(&SuspOut_mutex);

    CMN_TRACE_EXIT;
}   // PCTasks_Resume()



//!  \fn     PCError_ExitKernel()
//!  \brief  Fatal error in kernel.
//!	\param	reason PCError index PCMova error code
//!  \retval  
//!
void PCError_ExitKernel( PCError index )
{
    CMN_TRACE_ENTRY;

    // errorString is a const array of zero terminated strings
    // so pass directly to DiagLog

    // restart of MOVA process is implied with DC_ERROR
    DiagLog(DC_ERROR, "%s: MOVA:%s, Exit Kernel: %s",__FUNCTION__, gMovaEqId, errorString[index]);

    // here pcmova would exit
    exit(EXIT_FAILURE);

    CMN_TRACE_EXIT;
}   // PCError_ExitKernel()



//!  \fn     PCError_SetLastKernel()
//!  \brief  store kernel error message
//!
//!	\param	nErrorID MVUInt16
//!	\param	errorString char * pointer to error message
//!	\param	errorLen MVUInt16 length 
//!  \retval  void none
//!
void PCError_SetLastKernel( MVUInt16 nErrorID, MVInt nErrorData,
                            const char *errorString, MVSize errorLen)

{
    HINSTANCE *instance;
    int status;

    CMN_TRACE_ENTRY;

    // find error message from nErrorID
    instance = GetModuleHandle(gMovaEqId);
    instance->kernelError = (unsigned int)nErrorID;
    instance->kernelData = nErrorData;

    // make sure we are passed a zero terminated string
    if (errorLen >= MAX_LOG_TEXT_LEN)
    {
        errorLen = MAX_LOG_TEXT_LEN-1;
    }

    if (errorString)
    {
        strncpy(instance->kernelErrorMessage, errorString, errorLen);
        instance->kernelErrorMessage[errorLen] = '\0';  // force terminate string
    }
    else
    {
        strcpy(instance->kernelErrorMessage, "Error string not supplied");
    }

    if ( !MATH_IS_IN_RANGE( nErrorID, 0, ERROR_IDS_NUM - 1 ) )
    {
        nErrorID = 0; // force out-of-range display
        SetLastError( ERROR_INVALID_PARAMETER );
    }
    strcpy(instance->idsMessage, idsMessage[nErrorID]);

    DiagLog(DC_WARNING,   "%s: MOVA:%s, KernErr %d=%s, Data %d:",
            __FUNCTION__, gMovaEqId, nErrorID, instance->idsMessage, nErrorData);
    CmnTrace(GTraceFileId,"%s: MOVA:%s, KernErr %d=%s, Data %d:",
             __FUNCTION__, gMovaEqId, nErrorID, instance->idsMessage, nErrorData);

    // now raise a chameleon Fault
    RaiseMovaErrorAsHistoricalFault(nErrorID, nErrorData);

    CMN_TRACE_EXIT;
}   // PCError_SetLastKernel()




//!  \fn     PCError_SetLast()
//!  \brief  store error message against the thread and log error detail
//!	\param	error PCError
//!	\param	detailMessage char* pointer to detailed message
//!	\param	source const char* source of error
//!  \retval void none
//!
void PCError_SetLast(PCError error, const char * detailMessage, const char * source )
{
    // AML don't differentiate between windows error and PCError.
    // AML TBC revist this assumption again later

    char bufferDet[MAX_LOG_TEXT_LEN+1];
    char bufferSrc[MAX_LOG_TEXT_LEN+1];

    CMN_TRACE_ENTRY;

    SetLastError((int)error);

    // now log the message and source

    // make sure we are passed a zero terminated string
    if (detailMessage)
    {
        strncpy(bufferDet, detailMessage, MAX_LOG_TEXT_LEN);
        bufferDet[MAX_LOG_TEXT_LEN] = '\0';
    }
    else
    {
        strcpy(bufferDet, "PCError_SetLast: Error string not supplied");
    }


    if (source)
    {
        strncpy(bufferSrc, source, MAX_LOG_TEXT_LEN);
        bufferDet[MAX_LOG_TEXT_LEN] = '\0';
    }
    else
    {
        strcpy(bufferSrc, "PCError_SetLast: Error source not supplied");
    }


    DiagLog(DC_WARNING,   "%s: MOVA:%s, MOVA_err %d, %s, %s",__FUNCTION__, gMovaEqId, error, bufferDet, bufferSrc);
    CmnTrace(GTraceFileId,"%s: MOVA:%s, MOVA_err %d, %s, %s",__FUNCTION__, gMovaEqId, error, bufferDet, bufferSrc);

    // this appears to be just an error message logging routine, so no need
    // to restart kernel?

    CMN_TRACE_EXIT;
}   // PCError_SetLastKernel()



//!  \fn     PCError_GetLast(void)
//!  \brief  retreive last error message logged with PCErrorSetLast()
//!
//!  \retval  PCError errorcode
//!
PCError PCError_GetLast( void )
{
    // AML don't differentiate between windows error and PCError.
    // AML TBC revist this assumtion again later
    PCError err = (PCError)GetLastError();
    return err;
} // PCError_GetLast()



//!  \fn     SOFT_ERROR(reason)
//!  \brief  Report a soft error
//!	\param	reason	error number
//!  \retval  PCError errorcode
//!
void SOFT_ERROR(int reason)
{
    // report a soft error reason
    CMN_TRACE_ENTRY;

    CmnTrace(GTraceFileId,"%s: MOVA:%s, Soft Error: %d",__FUNCTION__, gMovaEqId, reason);
    DiagLog(DC_WARNING,   "%s: MOVA:%s, Soft Error: %d",__FUNCTION__, gMovaEqId, reason);
    CMN_TRACE_EXIT;
}   // SOFT_ERROR()



//!  \fn     SendStringToModem()
//!  \brief  send string sOP_String to the modem with len nCharsInString
//!		The modem is the destination is MovaLib
//!	\note	Synchronisation:
//!    This function calls LockRingbuf() and sets the writerActive semaphore to
//!    serialise.
//!	\param	sOP_String unsigned char * string to send to the modem
//!	\param	nCharsInString int length of string
//!  \retval void none 
//!
void SendStringToModem(unsigned char *sOP_String, int nCharsInString)
{
    // send string sOP_String to the MOD_MOVALIB with len nCharsInString

    CMN_TRACE_ENTRY;

    if (MovaCradleWritePipe(GInstance, sOP_String, nCharsInString) == FALSE)
    {
        diag_printf("MovaCradleWritePipe() MOVA %s failed to write to pipe", gMovaEqId);
    }

    CMN_TRACE_EXIT;
}   // SendStringToModem()



//!  \fn     PCTasks_Sleep()
//!  \brief  Sleep the current task. This differs from pcmova as TaskIdent is ignored
//!
//!	\param	timeOutmsec int msec timeout
//!	\param	TaskIdent	psMOVA_TASK task ident  msec timeout 
//!  \retval	void none 
//!
void PCTasks_Sleep( unsigned long timeOutmsec, psMOVA_TASK TaskIdent )
{
    // Sleep the current thread.  This differs to PCMOVA as calls to this function
    // provide a TaskIdent which is not used.  Actually PCMOVA callers only make
    // a request to suspend their own thread.
    int lockStatus;

    CmnTrace(GTraceFileId, "%s: MOVA:%s, Entry. Timeout %d. TaskIdent %d.",__FUNCTION__, gMovaEqId,timeOutmsec,TaskIdent);

    if (usleep(timeOutmsec*1000) != 0)
    {
        CmnTrace(GTraceFileId, "%s: MOVA:%s, usleep(%d) failed.",__FUNCTION__, gMovaEqId,timeOutmsec*1000);
    }


    if (TaskIdent == MOVA_TASK_TRLUSERIF)
    {
        // wait for mutex then release; this is used to block the User Interface
        // this must be an error checking mutex as this routine may be passed the
        //same ident from different threads to prevent a deadlock!
        lockStatus = pthread_mutex_lock(&TrlUi_mutex);
        if (lockStatus == 0)
        {   // success, now unlock mutex
            pthread_mutex_unlock(&TrlUi_mutex);
        }
        else
        {
            CmnTrace(GTraceFileId, "%s: MOVA:%s, pthread_mutex_lock() lockStatus %d. TaskIdent %d.",__FUNCTION__, gMovaEqId, lockStatus, TaskIdent);
        }
    }

    CMN_TRACE_EXIT;
} // PCTasks_Sleep()



//!  \fn     Valid_Licence()
//!  \brief  Validates the MOVA license
//!
//!  \retval  int TRUE if a valid MOVA license
//!
int Valid_Licence(void)
{
    // returns true if valid MOVA licence
    CMN_TRACE_ENTRY;
    CMN_TRACE_EXIT;
    return TRUE;
}   // Valid_Licence()



//!  \fn     check_facility_licence(LicenceType)
//!  \brief  Returns true for a valid Licence Type
//!	\param	LicenceType
//!  \retval int TRUE if a valid MOVA licence
//!
int check_facility_licence(int LicenceType)
{
    // Returns true valid License Type
    CMN_TRACE_ENTRY;
    CMN_TRACE_EXIT;
    return TRUE;
}   // check_facility_licence()



//!  \fn     GetMOVACharFromHandset(void)
//!  \brief  Get a character from the engineers terminal and pass it to Mova
//!		via an intermediate buffer
//!	\note	Globals: gCHAR_HANDSETptr - pointer to next char in buffer 
//!  \retval  char character returned from the MOVA kernel
//!
char GetMOVACharFromHandset(void)
{
    char retChar = 0;
    static MESSAGE_ID MessageId;

    static __thread char buffer[MAX_MOVA_BUFFER];
    static __thread int bufferSize = sizeof(buffer);
    static __thread char *segPtr;

    unsigned int segLeft = 0;
    unsigned int rtnbufferLen = 0;
    char *bufferPtr = buffer;

    CMN_TRACE_ENTRY;


    if (gCHAR_HANDSETptr == NULL)
    {
        // read any data received into buffer
        segPtr    = bufferPtr;
        segLeft   = bufferSize;
        MessageId = TERM_STRING_ID;

        while (MessageId == TERM_STRING_ID)
        {
            // read socket and set pointer to next char in buffer
            MessageId = CheckForSocketInput(SKT_WAIT_IMMEDIATE, segPtr, segLeft, &rtnbufferLen);
            if (MessageId == TERM_STRING_ID)
            {
                segPtr  += rtnbufferLen;
                segLeft -= rtnbufferLen;
                diag_printf("recvd TERM_STRING_ID B:%x, RL:%x, S:%x",buffer, rtnbufferLen, segPtr);
            }
        }

        if (segPtr > bufferPtr)
        {
            // data received
            gCHAR_HANDSETptr = buffer;
        }
        else
        {
            // we have read no data so return
            gCHAR_HANDSETptr = NULL;
            retChar = 0;
            return retChar;
        }
    }

    // gCHAR_HANDSETptr now points to data
    if (gCHAR_HANDSETptr < segPtr)
    {
        // return next char in buffer
        retChar = *gCHAR_HANDSETptr++;
    }
    else
    {
        // force socket read next iteration
        retChar = *gCHAR_HANDSETptr;
        gCHAR_HANDSETptr = NULL;
    }
    diag_printf("sending %c to MOVA",retChar);

    CMN_TRACE_EXIT;
    return retChar;
}   // GetMOVACharFromHandset()



//!  \fn     CheckForMovaLibRequest(void)
//!  \brief  Check for socket input and discard anything from ETerm
//!
//!  \retval  void none
//!
void MovaCradleCheckForRequest(void)
{
    MESSAGE_ID MessageId;
    char buffer[MAX_MOVA_CMD_LEN];
    int bufferSize = sizeof(buffer);
    unsigned int rtnbufferLen = 0;

    CMN_TRACE_ENTRY;

    // read socket and set pointer to next char in buffer
    MessageId = CheckForSocketInput(SKT_WAIT_IMMEDIATE, buffer, bufferSize, &rtnbufferLen);
    switch(MessageId)
    {
        case TERM_STRING_ID:
            // discard
            diag_printf("recvd TERM_STRING_ID B:%x, RL:%x",buffer, rtnbufferLen);
            break;

        case MOVA_REQUEST_ID:
            diag_printf("recvd MOVA_REQUEST_ID");
            break;

        default:
            diag_printf("received unknown request id");
            break;
    }

    CMN_TRACE_EXIT;
}   // CheckForMovaLibRequest(void)



//!  \fn     CollectModemCharNE()
//!  \brief  Get a character from the ring buffer and pass it to Mova via an
//!		 intermediate buffer
//!	\note	Globals: gCHAR_HANDSETptr - pointer to next char in buffer 
//!
//!  \retval  char character returned from the MOVA kernel
//!
char CollectModemCharNE(void)
{
    static __thread char buffer[xgnDBZ_MESSAGE_STR_LEN+1];
    static __thread int bufferSize = sizeof(buffer);
    static __thread unsigned int rtnbufferLen = 0;
    unsigned char retChar = '\0';
    char *bufferPtr = buffer;

    CMN_TRACE_ENTRY;

    if (gCHAR_MODEMptr == NULL)
    {
#ifdef ERASE_BUFFER
        bzero(buffer, bufferSize);
        rtnbufferLen = 0;
#endif // ERASE_BUFFER
        if (MovaCradleReadPipe(GInstance, buffer, bufferSize, &rtnbufferLen) != FALSE)
        {
            diag_printf("recvd from MovaCradleReadPipe() B:%x, RL:%x",buffer, rtnbufferLen);
            gCHAR_MODEMptr = &buffer[0];
            retChar = *gCHAR_MODEMptr++;
        }
        else
        {
            // we have read no data
            gCHAR_MODEMptr = NULL;
            retChar = '\0';
        }
    }
    else
    {
        // gCHAR_HANDSETptr now points to data
        if (gCHAR_MODEMptr < &buffer[rtnbufferLen-1])
        {
            // return next char in buffer
            retChar = *gCHAR_MODEMptr++;
        }
        else
        {
            // force read pipe for next iteration
            retChar = *gCHAR_MODEMptr;
            gCHAR_MODEMptr = NULL;
        }
    }
    diag_printf("%s: Stream:%s, sending %c to MOVA", __FUNCTION__, gMovaEqId, retChar);

    CMN_TRACE_EXIT;
    return retChar;
} // CollectModemCharNE(void)



//!  \fn     SendStringToLH()
//!  \brief  Send a string to local handset
//!
//!	\param	sOP_String char* string to send to the modem
//!	\param	nCharsInString int length of string
//!  \retval void none 
//!
void SendStringToLH(unsigned char *sOP_String, int nCharsInString)
{
    // Send a string to local hand set str is string and len is num of chars to send

    unsigned char *section_start;

    CmnTrace(GTraceFileId, "%s: MOVA:%s, ENTRY  sOP_String len %d",
             __FUNCTION__, gMovaEqId, nCharsInString);

    for(section_start = sOP_String; nCharsInString > MAX_SOCKET_WRITE;
        section_start = section_start+MAX_SOCKET_WRITE)
    {
        sendSections(MOD_ETERM, section_start, MAX_SOCKET_WRITE);
        nCharsInString = nCharsInString - MAX_SOCKET_WRITE;
    }
    if (nCharsInString > 0)
    {
        // send the last section
        sendSections(MOD_ETERM, section_start, nCharsInString);
    }

    CMN_TRACE_EXIT;
}   // SendStringToLH()



/  \fn     sendSections()
//!  \brief  Send a string to local handset in pieces
//!
//!	\param	sOP_String unsigned char*  string to send to the modem
//!	\param	section_len length of string
//!  \retval BOOLEAN TRUE = success, FALSE = failure
//!
static BOOLEAN  sendSections(MODULE destModule, unsigned char * section_start, const int section_len)
{
    int				msgLen = 0;
    my_msg_term_string_t	TermString;
    struct sockaddr		To;
    BOOLEAN                     status = FALSE;
    mova_io_t *movaIo;

    CmnTrace(GTraceFileId, "%s: MOVA:%s, ENTRY  len %d", __FUNCTION__, gMovaEqId, section_len);

    if (SktSocketFileExists(destModule, 0) != FALSE)
    {

        SktAddress(&To, destModule, 0);

        // Create and send message to Engineers Terminal socket
        TermString.address = 0;

        // Add 1 to include the NUL terminator
        strncpy(TermString.text, section_start, section_len);
        TermString.text[section_len] = '\0';
        msgLen = sizeof(int) + section_len + 1;

        diag_printf("calling SktSendMessageBlock(...,%s, MODULE %d)",TermString.text, destModule);
        status = SktSendMessageBlock(__FUNCTION__,
                            TERM_STRING_ID,
                            GMySocket,
                            msgLen,
                            (char *)&TermString,
                            &To);
        if (status == FALSE)
        {
            movaIo = &GIOshmData->mova_io[GInstance];

            if (destModule == MOD_ETERM)
            {
                (movaIo->movaIf).LComms = MOVAIF_COMMS_TO_OMU;
            }
            else if (destModule == MOD_MOVALIB)
            {
                (movaIo->movaIf).RComms = MOVAIF_COMMS_TO_OMU;
            }

        }  // End if session is active
    }
    CMN_TRACE_EXIT;

    return status;
} // sendSections()



//!  \fn     com_read_clock()
//!  \brief  reads the date/time
//!
//!	\param	omu_time TIME_TYPE* MOVA date/time structure
//!  \retval unsigned char sts; RTC_OK => success
//!
unsigned char com_read_clock(TIME_TYPE *omu_time)
{
    unsigned char sts = !RTC_OK;
    time_t Clock;
    struct tm TimeNow;
    struct timeval tv;
    uid_t  userid;

    // AML This function appears to be called several times
    // per second.  For now switch off function tracing

    Clock = time(NULL);
    localtime_r(&Clock, &TimeNow);

    omu_time->seconds = (utiny)TimeNow.tm_sec;
    omu_time->minutes = (utiny)TimeNow.tm_min;
    omu_time->hours   = (utiny)TimeNow.tm_hour;
    omu_time->date    = (utiny)TimeNow.tm_mday;
    omu_time->month   = (utiny)TimeNow.tm_mon+1;

    // MOVA kernel is not currently y2k compliant
#ifdef notY2K
    omu_time->year    = (utiny)TimeNow.tm_year+1900-2000;
#else
    omu_time->year    = (utiny)TimeNow.tm_year+1900;
#endif /* notY2K*/


    /* Day Of Week:  S S M T W T F S S */
    /* MOVA Numbers: 6 7 1 2 3 4 5 6 7 */
    /* OMU Numbers:  0 1 2 3 4 5 6 0 1 */
    /* tm_wday:      6 0 1 2 3 4 5 6 0 */

    omu_time->day     = (utiny)(TimeNow.tm_wday == 6 ? 0 : TimeNow.tm_wday+1);
    sts = RTC_OK;

    //CMN_TRACE_EXIT;
    return sts;
} // com_read_clock()



//!  \fn     com_set_clock()
//!  \brief  Changes the date/time
//!
//!	\param	omu_time TIME_TYPE* MOVA date/time structure
//!  \retval unsigned char sts; RTC_OK => success
//!
unsigned char com_set_clock(TIME_TYPE const *omu_time)
{
    unsigned char sts = !RTC_OK;
    time_t Clock;
    struct tm TimeNow;
    struct timeval tv;
    uid_t  userid;

    CmnTrace(GTraceFileId, "%s: Stream:%s, ENTRY", __FUNCTION__, gMovaEqId  );

    userid = getuid();
    if (seteuid(0) !=0)
    {
        DiagLog( DC_WARNING, "%s: Stream:%s, seteuid() failed: Unable to set permissions", __FUNCTION__, gMovaEqId );
    }
    else
    {   // permissions ok so now set TimeNow

		//Fill time zone
		Clock = time(NULL);
		localtime_r(&Clock, &TimeNow);

        TimeNow.tm_sec  = omu_time->seconds;
        TimeNow.tm_min  = omu_time->minutes;
        TimeNow.tm_hour = omu_time->hours;
        TimeNow.tm_mday = omu_time->date;
        TimeNow.tm_mon  = omu_time->month-1;

        // MOVA kernel is not currently y2k compliant
#ifdef notY2K
        TimeNow.tm_year = omu_time->year+2000-1900;
#else
        TimeNow.tm_year = omu_time->year-1900;
#endif /* notY2K*/

        CmnTrace( GTraceFileId, "%s: Stream:%s, New Date/Time %04d/%02d/%02d %02d:%02d:%02d",
                  __FUNCTION__, gMovaEqId, TimeNow.tm_year+1900 , TimeNow.tm_mon+1,
                  TimeNow.tm_mday, TimeNow.tm_hour, TimeNow.tm_min, TimeNow.tm_sec);

        // convert time to correct format for setting system clock
        Clock = mktime( &TimeNow );
        if (Clock==-1)
        {
            CmnTrace( GTraceFileId, "%s: Stream:%s, mktime failed", __FUNCTION__, gMovaEqId );
            DiagLog( DC_WARNING,    "%s: Stream:%s, mktime failed", __FUNCTION__, gMovaEqId );
        }
        else
        {
            // set system time . NOTE this exe must have root premission
            tv.tv_sec=Clock;
            tv.tv_usec=0;

            if (settimeofday(&tv,NULL) !=0)
            {
                CmnTrace( GTraceFileId, "%s: Stream:%s, settimeofday() failed %s", __FUNCTION__, gMovaEqId, strerror(errno) );
                DiagLog( DC_WARNING,    "%s: Stream:%s, settimeofday() failed %s", __FUNCTION__, gMovaEqId, strerror(errno) );
            }
            else
            {
                sts = RTC_OK;

                // tell MOVA the clock has changed
                if(Tcomshr->mark1 == 1234)
                {
                    Tcomshr->mark1 = RELOAD_TIMES;
                }
            }
        }
    } // set root
    if (seteuid(userid) !=0)
    {
        DiagLog( DC_WARNING, "%s: Stream:%s, seteuid() failed: Unable to return permissions", __FUNCTION__, gMovaEqId );
    }
    CMN_TRACE_EXIT;
    return sts;
} // com_set_clock()



//!  \fn     diag_printf()
//!  \brief  MOVA kernel debugging.  Gemini style debug converted to Chameleon CmnTrace()
//!  \param  va_arg printf params
//!  \note   globals gMovaEqId
//!  \retval  void none
//!
inline void diag_printf(char *string, ...)
{
    va_list ap;
    int *chp;
    char errorMessage[MAX_LOG_TEXT_LEN];

    // CMN_TRACE_ENTRY  // save trace file space

    va_start( ap, string );
    vsnprintf( errorMessage,MAX_LOG_TEXT_LEN, string, ap );
    va_end(ap);

    // make sure we are passed a zero terminated string
    errorMessage[MAX_LOG_TEXT_LEN] = '\0';  // force terminate string

    CmnTrace(GTraceFileId,"%s: Stream:%s, %s",  __FUNCTION__, gMovaEqId, errorMessage);

    // CMN_TRACE_EXIT  // save trace file space
}   // diag_printf()


//! windows api abstraction

//!  \fn     GetModuleHandle()
//!  \brief  Return a pointer to the HINSTANCE datastructure.  This contains thread
//!		 specific data using pthread_key_ routines. 
//!	\param	string const char* equipment id 
//!  \retval  HINSTANCE*
//!
HINSTANCE *GetModuleHandle(const char *eqid)
{
    void *ptr = NULL;

    CMN_TRACE_ENTRY;

    pthread_once(&key_once, CreateKey);
    ptr = pthread_getspecific(key);

    CMN_TRACE_EXIT;

    return (HINSTANCE *) ptr;
} // GetModuleHandle()



//!  \fn    LoadString()
//!  \brief  Open a string resource for an instance
//! 		in this case an the resource is the error strings from pcerror.h
//!
//!	\param	string const char* filename
//!	\param	uID unsigned int error code
//!	\param	lpBuffer char* destination for char string
//!	\param	nBufferMax int buffer length
//!  \retval  int >0 success, 0 = failure (out-of-range)
//!
int LoadString(HINSTANCE *hInstance, const unsigned int uID, char *lpBuffer, int nBufferMax)
{
    unsigned int index = uID;

    CMN_TRACE_ENTRY;

    if (index < 0 || index >= PCMOVA_ERROR_COUNT)
    {
        index = 0;
    }
    strncpy(lpBuffer,errorString[index], nBufferMax);
    strncpy(hInstance->errorstring, errorString[index], sizeof(hInstance->errorstring));

    CMN_TRACE_EXIT;
    return index;
} // LoadString()



//!  \fn     GetLastError()
//!		Get the last error returned stored with SetLastError()
//!  \note	In this case an instance relates to thread specific data
//!	\param	
//!  \retval error int 
//!
int GetLastError()
{
    int errorcode;
    HINSTANCE *instance;

    CMN_TRACE_ENTRY;

    instance = GetModuleHandle("PCMOVA.EXE");   // AML to fix this statement
    errorcode = instance->error;

    CMN_TRACE_EXIT;
    return errorcode;
} // GetLastError()



//!  \fn     SetLastError()
//!		Store an error against the thread, obtainable with GetLastError()
//!  \note	In this case an instance relates to thread specific data
//!	\param	errorcode error code to store
//!  \retval void none 
//!
void SetLastError(int errorcode)
{
    HINSTANCE *instance;

    CMN_TRACE_ENTRY;
    instance = GetModuleHandle("PCMOVA.EXE");   // AML to fix this statement
    instance->error = errorcode;

    CMN_TRACE_EXIT;
} // SetLastError()


/* --------------------------------------------------------------------------- *
 *                   SUPPORT FUNCTIONS                                         *
 * --------------------------------------------------------------------------- *
*/


//!  \fn     CheckForSocketInput()
//!  \brief  reads a text buffer from the socket
//!
//!	\param	iDelay const int Delay in seconds
//!	\param	buffer char* buffer containing data
//!	\param	bufferLen const int size of buffer
//!	\param	rtnLen int length of CR NULL terminated string
//!  \retval  MESSAGE_ID or NULL if no message
//!
static MESSAGE_ID CheckForSocketInput(const int iDelay, char *buffer, const int bufferLen, int *rtnLen)
{
    MESSAGE_ID MessageId = NO_MESSAGE;
    struct sockaddr From;
    struct sockaddr To;
    MSG *Msg = NULL;
    int textLen = 0;
    static __thread unsigned int MovaLibMessagesWatchdog = 0;
    BOOLEAN pipeConnected = FALSE;


    CmnTrace(GTraceFileId, "Stream:%s, %s", gMovaEqId, __FUNCTION__);

    /* Wait for next message */
    MessageId = SktReadMessage ("CheckForSocketInput:",GMySocket, &Msg, &From, iDelay);

    pipeConnected = MovaLibPipeConnected();
    if (pipeConnected == TRUE)
    {
        MovaLibMessagesWatchdog++;
    }

    CmnTrace(GTraceFileId, "Stream:%s, MessageId %d, watchdog %d", gMovaEqId, MessageId, MovaLibMessagesWatchdog);

    if (MessageId != NO_MESSAGE)
    {
        textLen = Msg->MsgHdr.Length - sizeof(int);
        switch (MessageId)
        {
            case TERM_STRING_ID:
                processETermRequest((msg_term_string_t *)&(Msg->Body), textLen, buffer, bufferLen, rtnLen);
                MovaLibMessagesWatchdog = 0;
                break;

            case MOVA_REQUEST_ID:
                processMovaLibRequest((msg_mova_request_t *)&(Msg->Body));
                MovaLibMessagesWatchdog = 0;
                break;

            case TEST_MSG1_ID:	// (TEST CODE)
                break;

            case TEST_FILE_ID:	// (TEST CODE)
                break;
        }
    }
    SktFreeMessage (&Msg);

    if (pipeConnected == TRUE && MovaLibMessagesWatchdog >= MAX_MOVALIB_WATCHDOG)
    {
        CmnTrace(GTraceFileId, "Stream:%s, Watchdog %d. Pipe idle. Disconnecting pipe and reset watchdog.", gMovaEqId, MovaLibMessagesWatchdog);
        MovaCradleDisconnectPipe(GInstance);
        MovaLibMessagesWatchdog = 0;
    }

    CMN_TRACE_EXIT;
    return MessageId;
}   // CheckForSocketInput()



//!  \fn     processETermRequest()
//!  \brief  Process text sent from ETerm
//!
//!	\param	msg_mova_request_t* message
//!	\param	buffer char* buffer containing data
//!	\param	bufferLen const int size of buffer
//!	\param	rtnLen int length of CR NULL terminated string
//!  \retval void none
//!
static void processETermRequest(msg_term_string_t *TermStringMsg, const int textLen, char *buffer, const int bufferLen, int *rtnLen)
{
    struct sockaddr From;
    struct sockaddr To;
    msg_mova_request_t MovaLibReply;
    int slot = 0;
    int i = 0;
    int j = 0;
    static const MODULE ModuleMovaLib = MOD_MOVALIB;
    static const MODULE ModuleMovaMsg = MOD_MOVA;

    CMN_TRACE_ENTRY;

    // assumption TermStringMsg->text contains a zero terminated string
    i = 0;
    j = 0;
    while(i < textLen && j < bufferLen-3)   // each itteration can add 2 chars
    {
        if (TermStringMsg->text[i] != '\r' && TermStringMsg->text[i] != '\n')
        {
                        // copy the byte
            buffer[j++] = TermStringMsg->text[i++];
        }
        else
        {
            // we have hit a record terminator
#ifdef CRLF
            // insert cr/lf pair
            buffer[j++] = '\r';
            buffer[j++] = '\n';
#else
            // insert a cr
            buffer[j++] = '\r';
#endif // CRLF
            while (TermStringMsg->text[i] == '\r' || TermStringMsg->text[i] == '\n')
            {   // walk past record terminator characters
                i++;
            }
        }
    }

    // no need to terminate buffer as zero is copied from source
    *rtnLen   = j-1;

    if (i < textLen)
    {
        CmnTrace(GTraceFileId, "%s: Stream:%s, SktReadMessage truncation. recvd %d, buffer %d",
                __FUNCTION__, gMovaEqId, textLen, bufferLen-1);
    }

#ifdef TRACE_MOVA_SOCKET
    if ( g_dload_fd > 0 )
    {
        rlen = write(g_dload_fd, buffer, *rtnLen);
        if (rlen > 0)
        {
            fsync(g_dload_fd);
        }
    }
#endif // TRACE_MOVA_SOCKET

    CmnTrace(GTraceFileId, "%s: Stream:%s, Mova-text %s", __FUNCTION__, gMovaEqId, buffer);
    CMN_TRACE_EXIT;
}   // processETerm()



//!  \fn     processMovaLibRequest()
//!  \brief  Process a MOVA request and respond
//!  \param  MovaRequest *msg_mova_request_t
//!  \retval  BOOLEAN TRUE => SUCCESS
//!
static void processMovaLibRequest(msg_mova_request_t *MovaRequest)
{
    struct sockaddr From;
    struct sockaddr To;
    msg_mova_request_t MovaLibReply;
    int textLen = 0;
    int slot = 0;
    static const MODULE ModuleMovaLib = MOD_MOVALIB;
    static const MODULE ModuleMovaMsg = MOD_MOVA;
    mova_io_t *movaIo;
    static BOOLEAN outputIsRedirectedToModem = FALSE;       // flag to make sure the first request for MOVA Messages
                                                // is sent to the kernel, otherwise a refresh is requested
    movaIo = &GIOshmData->mova_io[GInstance];

    CMN_TRACE_ENTRY;

    slot = MovaRequest->slot;

    if (MovaCradleThisPipeIsConnected() == FALSE)
    {
        // we cannot be redirecting output to an invalid Pipe
        outputIsRedirectedToModem = FALSE;
    }

    if (MovaCradleValidateRequest("CONNECT_PIPE", MovaRequest) != FALSE)  // mova redefines true
    {
        CmnTrace(GTraceFileId, "%s: Stream:%s, MOVA_REQUEST: CONNECT_PIPE", __FUNCTION__, gMovaEqId);
        MovaCradleConnectPipe(GInstance);
        MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_SUCCESS, slot);
        MovaCradleSendReply(&MovaLibReply);
    }
    else if (MovaCradleValidateRequest("DISCONNECT_PIPE", MovaRequest) != FALSE)  // mova redefines true
    {
        CmnTrace(GTraceFileId, "%s: Stream:%s, MOVA_REQUEST: DISCONNECT_PIPE", __FUNCTION__, gMovaEqId);
        MovaCradleDisconnectPipe(GInstance);
        MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_SUCCESS, slot);
        MovaCradleSendReply(&MovaLibReply);
    }
    else if (MovaCradleValidateRequest("MESSAGE_DISP", MovaRequest) != FALSE)  // mova redefines true
    {
        CmnTrace(GTraceFileId, "%s: Stream:%s, MOVA_REQUEST: MESSAGE_DISP", __FUNCTION__, gMovaEqId);

        // io flag 20 controls the output thread displaying 50 messages
        // io flag 28 needs to be set to 23 to keep messages displayed

        // ::::::::::::::::::::::: VIEW MOVA MESSAGES :::::::::::::::::::::::::::
        // ---------- MOVA messages are output by the OUTPUT program for X minutes
        // ---------- controlled by setting IO(21)=50+X for TERM, 40+X for PHONe

        // request messages are redirected to the modem only once and subsequent
        // requests should only refresh the display
        if (outputIsRedirectedToModem == FALSE)
        {
            Tcomshr->io[20] = 40;   // send to Modem
            outputIsRedirectedToModem = TRUE;
        }

        Tcomshr->io[28] = 23;   // refresh display

        // The following command disables term, but as term is used for the
        // communication between MovaCradle and MovaLib it should not be
        // disabled as it would break the WebServer.
        // Check added to term so it still processes MovaLib comms

        lMessOn = TRUE;          // tell term thread that messages output is On.

        MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_SUCCESS, slot);
        MovaCradleSendReply(&MovaLibReply);
    }
    else if (MovaCradleValidateRequest("MESSAGE_CTRL", MovaRequest) != FALSE)  // mova redefines true
    {
        CmnTrace(GTraceFileId, "%s: Stream:%s, MOVA_REQUEST: MESSAGE_CTRL", __FUNCTION__, gMovaEqId);

        if (MovaCradleSetLower(&Tcomshr->io[20], slot) != FALSE)
        {
            MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_SUCCESS, slot);
        }
        else
        {
            MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_FAILURE, slot);
        }
        MovaCradleSendReply(&MovaLibReply);
    }
    else if (MovaCradleValidateRequest("REFRESH_CTRL", MovaRequest) != FALSE)  // mova redefines true
    {
        CmnTrace(GTraceFileId, "%s: Stream:%s, MOVA_REQUEST: REFRESH_CTRL", __FUNCTION__, gMovaEqId);

        // io flag 28 needs to be set to 23, 26, or 32 to keep messages displayed
        // where:
        // 23 => view MOVA messages
        // 22 => display MOVA messages
        // 27 => print last 50 MOVA messages
        // 26 => view error messages
        // 32 => view assessment messages
        // 0  => stop displaying messages
        Tcomshr->io[28] = slot;
        if (slot == 0)
        {
            lMessOn = FALSE;    // tell term thread that message output Off.
            outputIsRedirectedToModem = FALSE;
        }
        MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_SUCCESS, slot);
        MovaCradleSendReply(&MovaLibReply);
    }
    else if (MovaCradleValidateRequest("FLOW_DISP", MovaRequest) != FALSE)  // mova redefines true
    {
        CmnTrace(GTraceFileId, "%s: Stream:%s, MOVA_REQUEST: FLOW_DISP", __FUNCTION__, gMovaEqId);
        // io flag 22 controls the output thread displaying flows
        Tcomshr->io[22] |= 30;
        MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_SUCCESS, slot);
        MovaCradleSendReply(&MovaLibReply);
    }
    else if (MovaCradleValidateRequest("FLOW_CTRL", MovaRequest) != FALSE)  // mova redefines true
    {
        CmnTrace(GTraceFileId, "%s: Stream:%s, MOVA_REQUEST: FLOW_CTRL", __FUNCTION__, gMovaEqId);
        // io flag 22 controls the output thread displaying flows
        if (MovaCradleSetLower(&Tcomshr->io[22], MovaRequest->slot) != FALSE)
        {
            MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_SUCCESS, slot);
        }
        else
        {
            MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_FAILURE, slot);
        }
        MovaCradleSendReply(&MovaLibReply);
    }
    else if (MovaCradleValidateRequest("ASSESS_DISP", MovaRequest) != FALSE)  // mova redefines true
    {
        CmnTrace(GTraceFileId, "%s: Stream:%s, MOVA_REQUEST: ASSESS_DISP", __FUNCTION__, gMovaEqId);

        // io flag 25 controls the output thread displaying assessment log
        Tcomshr->io[25] |= 30;
        Tcomshr->io[28] = 32;
        MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_SUCCESS, slot);
        MovaCradleSendReply(&MovaLibReply);
    }
    else if (MovaCradleValidateRequest("ASSESS_CTRL", MovaRequest) != FALSE)  // mova redefines true
    {
        CmnTrace(GTraceFileId, "%s: Stream:%s, MOVA_REQUEST: ASSESS_CTRL", __FUNCTION__, gMovaEqId);

        // io flag 25 controls the output thread displaying assessment log
        if (MovaCradleSetLower(&Tcomshr->io[25], MovaRequest->slot) != FALSE)
        {
            MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_SUCCESS, slot);
        }
        else
        {
            MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_FAILURE, slot);
        }
        MovaCradleSendReply(&MovaLibReply);
    }
    else if (MovaCradleValidateRequest("ERROR_DISP", MovaRequest) != FALSE)  // mova redefines true
    {
        CmnTrace(GTraceFileId, "%s: Stream:%s, MOVA_REQUEST: ERROR_DISP", __FUNCTION__, gMovaEqId);

        // io flag 26 controls the output thread displaying the error log
        Tcomshr->io[26] |= 30;
        Tcomshr->io[28] = 26;
        MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_SUCCESS, slot);
        MovaCradleSendReply(&MovaLibReply);
    }
    else if (MovaCradleValidateRequest("ERROR_CTRL", MovaRequest) != FALSE)  // mova redefines true
    {
        CmnTrace(GTraceFileId, "%s: Stream:%s, MOVA_REQUEST: ERROR_CTRL", __FUNCTION__, gMovaEqId);

        // io flag 26 controls the output thread displaying the error log
        if (MovaCradleSetLower(&Tcomshr->io[26], MovaRequest->slot) != FALSE)
        {
            MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_SUCCESS, slot);
        }
        else
        {
            MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_FAILURE, slot);
        }
        MovaCradleSendReply(&MovaLibReply);
    }
    else if (MovaCradleValidateRequest("START", MovaRequest) != FALSE)  // mova redefines true
    {
        CmnTrace(GTraceFileId, "%s: Stream:%s, MOVA_REQUEST: START", __FUNCTION__, gMovaEqId);

        // io flag 27 controls MOVA On

        Tcomshr->io[27] = 1;

        MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_SUCCESS, slot);
        MovaCradleSendReply(&MovaLibReply);
    }
    else if (MovaCradleValidateRequest("STOP", MovaRequest) != FALSE)  // mova redefines true
    {
        CmnTrace(GTraceFileId, "%s: Stream:%s, MOVA_REQUEST: STOP", __FUNCTION__, gMovaEqId);

        // io flag 27 controls MOVA On

        Tcomshr->io[27] = 0;
        // Switching MOVA off is is more complex as the force bits need to be
        // switched off.  Rely on guard code in 100Ioscan.c (switch_mova_off())
        // to reset forces
        Switch_MOVA_Off(NO_SAV_MESSAGES, NO_REBOOT);

        MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_SUCCESS, slot);
        MovaCradleSendReply(&MovaLibReply);
    }
    else if (MovaCradleValidateRequest("CLEAR", MovaRequest) != FALSE)  // mova redefines true
    {
        CmnTrace(GTraceFileId, "%s: Stream:%s, MOVA_REQUEST: CLEAR", __FUNCTION__, gMovaEqId);

        // io flag 16 controls MOVA Error counter
        // MOVA Userbig.c also clears local error variables which are not accessable
        // here.
        Tcomshr->io[16] = MovaRequest->slot;
        MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_SUCCESS, slot);
        MovaCradleSendReply(&MovaLibReply);
    }
    // AML test code needs validating
    else if (MovaCradleValidateRequest("UPLOAD", MovaRequest) != FALSE)  // mova redefines true
    {
        CmnTrace(GTraceFileId, "%s: Stream:%s, MOVA_REQUEST: UPLOAD", __FUNCTION__, gMovaEqId);
        if ((nonVol->plan_)[slot-1] != FALSE)   // (MOVA redefines TRUE)
        {
                            // We have a valid dataset so upload it.
            UploadData(slot-1, MODEM_PORT);
            MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_SUCCESS, slot);
        }
        else
        {
            MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_FAILURE, slot);
        }
        MovaCradleSendReply(&MovaLibReply);
    }
    else if (MovaCradleValidateRequest("DATASET", MovaRequest) != FALSE)  // mova redefines true
    {
        CmnTrace(GTraceFileId, "%s: Stream:%s, MOVA_REQUEST: DATASET", __FUNCTION__, gMovaEqId);
        signalDatasetThread();
        MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_SUCCESS, slot);
        MovaCradleSendReply(&MovaLibReply);
    }
    else if (MovaCradleValidateRequest("FORCE", MovaRequest) != FALSE)  // mova redefines true
    {
        CmnTrace(GTraceFileId, "%s: Stream:%s, MOVA_REQUEST: FORCE", __FUNCTION__, gMovaEqId);
        nForceReq = slot;
        TryForces(MODEM_PORT);

        if (slot > 0)
        {
            MovaCradleSetStatus(MOVAST_MANUAL_FORCE);
        }
        else
        {
            MovaCradleSetStatus(movaIo->prevStatus);
        }
        MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_SUCCESS, slot);
        MovaCradleSendReply(&MovaLibReply);
    }
    else
    {   // unknown command, return failure

        CmnTrace(GTraceFileId, "%s: Stream:%s, MOVA_REQUEST: %s unknown", __FUNCTION__, gMovaEqId, MovaRequest->request);

        MovaCradleBuildReply(&MovaLibReply, MovaRequest, MM_FAILURE, slot);
        MovaCradleSendReply(&MovaLibReply);
    }
    CMN_TRACE_EXIT;
}   // processMovaLibRequest()



//!  \fn     MovaCradleValidateRequest()
//!  \brief  Check the MovaRequest received from MovaLib against the requesttype param
//!  \param  RequestType char request type, Const msg_mova_request_t *request
//!  \param  MovaRequest Const char request type, Const msg_mova_request_t *request
//!  \retval  BOOLEAN TRUE => SUCCESS
//!
inline static BOOLEAN MovaCradleValidateRequest(const char *RequestType,
                                                 const msg_mova_request_t *MovaRequest)
{
    int rtn;

    CMN_TRACE_ENTRY;
    if (strcasecmp(RequestType, MovaRequest->request) == 0)
    {
        // if request valid return success
        rtn = TRUE;
    }
    else
    {
        rtn = FALSE;
    }
    CMN_TRACE_EXIT;
    return rtn;
} // MovaCradleValidateRequest()



//!  \fn     MovaCradleSendReply()
//!  \brief  Build a reply packet to a MovaLibRequest from a blank request
//!  \param  MovaLibReply *Reply In/Out
//!  \param  MovaRequest const *Request, In
//!  \param  replyStatus mova_msg_t   In
//!  \param  slot int status In
//!  \retval  BOOLEAN TRUE => SUCCESS
//!
inline static BOOLEAN MovaCradleBuildReply(msg_mova_request_t       *MovaLibReply,
                                            const msg_mova_request_t *MovaRequest,
                                            mova_msg_t replyStatus,
                                            int slot)
{
    int rtn;

    CMN_TRACE_ENTRY;
    if (MovaRequest == NULL || MovaLibReply == NULL)
    {   // parameters are incorrect
        rtn = FALSE;
    }
    else
    {   // ok, copy the request and set return status
        strncpy(MovaLibReply->request, MovaRequest->request, MAX_MOVALIB_MSG_LEN);
        MovaLibReply->request[MAX_MOVALIB_MSG_LEN-1] = '\0';        // terminate just-in-case

        MovaLibReply->seq     = MovaRequest->seq;
        MovaLibReply->type    = MM_REPLY;
        MovaLibReply->status  = replyStatus;
        MovaLibReply->slot    = slot;

        rtn = TRUE;
    }
    CMN_TRACE_EXIT;
    return rtn;
} // MovaCradleBuildReply()



//!  \fn     MovaCradleSendReply(void)
//!  \brief  Send a reply packet to MovaLib
//!  \param  MovaLibReply (msg_mova_request_t *) in/out
//!  \retval  BOOLEAN TRUE => SUCCESS
//!
static int MovaCradleSendReply(const msg_mova_request_t *MovaLibReply)
{
    static const MODULE ModuleMovaLib = MOD_MOVALIB;
    static const MODULE ModuleMovaMsg = MOD_MOVA;
    struct sockaddr To;
    int rtn;

    CMN_TRACE_ENTRY;
    if (MovaLibReply == NULL)
    {   // parameters are incorrect
        rtn = FALSE;
    }
    else
    {   // ok, copy the request and set return status
        if (GMySocket == 0)
        {
            GMySocket = SktCreateSocket("Initialise:Creating own socket", ModuleMovaMsg, GInstance);
        }
        SktAddress(&To, ModuleMovaLib, 0);
        SktSendMessageBlock(__FUNCTION__, MOVA_REQUEST_ID, GMySocket,
                            sizeof(*MovaLibReply), (char *)MovaLibReply, &To);

        rtn = TRUE;
    }
    CMN_TRACE_EXIT;
    return rtn;
} // MovaCradleSendReply()



//!  \fn     CreateKey(void)
//!  \brief  Associate a datastructure with a pthread key.  This function must
//!     only be called once
//!  \retval  void none
//!
static void CreateKey()
{
    HINSTANCE *ptr = NULL;

    CMN_TRACE_ENTRY;

    pthread_key_create(&key, DeleteKey);
    ptr = pthread_getspecific(key);
    if (ptr == NULL)
    {   // allocate and associate datastructure

        ptr = (HINSTANCE *) malloc(sizeof(HINSTANCE));
        if (ptr != NULL)
        {
            ptr->instance = GInstance;
            strcpy(ptr->stream, gMovaEqId);

            pthread_setspecific(key, ptr);
        }
        else
        {   // malloc error
        }
    }  // create datastructure

    CMN_TRACE_EXIT;
} // CreateKey()



//!  \fn     DeleteKeyContents(void)
//!  \brief  Deallocate datastructure associated with pthread key
//!  \retval  void none
//!
void DeleteKeyContents()
{
    void *ptr = NULL;

    CMN_TRACE_ENTRY;

    ptr = pthread_getspecific(key);
    if (ptr != NULL)
    {
        free(ptr);
    }

    CMN_TRACE_EXIT;
} // DeleteKeyContents()



//!  \fn     DeleteKey(void)
//!  \brief  Cleanup pthreads key.  Called from thread exit handler
//!  \retval  void none
//!
void DeleteKey()
{
    CMN_TRACE_ENTRY;

    DeleteKeyContents();
    pthread_key_delete(key);

    CMN_TRACE_EXIT;
} // DeleteKey()



//!  \fn     waitfortick(void)
//!  \brief  wait for next tick
//!  \retval  void none
//!
inline void waitfortick(void)
{
    // Suspend until the next tick notification from signal handler thread
    // CMN_TRACE_ENTRY;

    pthread_mutex_lock(&tick_mutex);
    pthread_cond_wait(&tick, &tick_mutex);
    pthread_mutex_unlock(&tick_mutex);

    // CMN_TRACE_EXIT;
}   // waitfortick()



//!  \fn     waitforlongtick(void)
//!  \brief  wait for next long tick
//!  \retval  void none
//!
inline void waitforlongtick(void)
{
    // Suspend until the next longtick notification from signal handler thread
    // CMN_TRACE_ENTRY;

    pthread_mutex_lock(&longtick_mutex);
    pthread_cond_wait(&longtick, &longtick_mutex);
    pthread_mutex_unlock(&longtick_mutex);

    // CMN_TRACE_EXIT;
}   // waitforlongtick()



//!  \fn     waitforAmlRequest(void)
//!  \brief  wait for condition variable
//!  \retval  void none
//!
inline void waitforDatasetRequest(void)
{
    // Suspend until the next longtick notification from dload handler thread
    // CMN_TRACE_ENTRY;

    // enable TRLUI thread
    pthread_mutex_unlock(&TrlUi_mutex);

    pthread_mutex_lock(&Dataset_mutex);
    pthread_cond_wait(&DatasetActive, &Dataset_mutex);
    pthread_mutex_unlock(&Dataset_mutex);

    // now block TRLUI thread
    pthread_mutex_lock(&TrlUi_mutex);
    // CMN_TRACE_EXIT;
}   // waitforDatasetrequest()



//!  \fn     waitforAmlRequest(void)
//!  \brief  wait for condition variable
//!  \retval  void none
//!
inline void waitforAmlRequest(void)
{
    // Suspend until condition
    CMN_TRACE_ENTRY;

    pthread_mutex_lock(&Aml_mutex);
    pthread_cond_wait(&AmlActive, &Aml_mutex);
    pthread_mutex_unlock(&Aml_mutex);

    CMN_TRACE_EXIT;
}   // waitforAmlRequest()



//!  \fn     signalDatasetThread(void)
//!  \brief  wait for next tick
//!  \retval  void none
//!
inline void signalDatasetThread(void)
{
    // CMN_TRACE_ENTRY;

    lLineDropped = 0;  // MovaLib 'modem' port up
    pthread_mutex_lock(&Dataset_mutex);
    pthread_cond_signal(&DatasetActive);
    pthread_mutex_unlock(&Dataset_mutex);

    // CMN_TRACE_EXIT;
}   // signalDatasetThread()


//!  \fn     LockIoScan(void)
//!  \brief  Wait for GetOutput mutex
//!  \retval  void none
//!
inline void LockIoScan(void)
{
    CMN_TRACE_ENTRY;

    pthread_mutex_lock(&GetOutput_mutex);
    // Just use a mutex
    // pthread_cond_wait(&GetOutput, &GetOutput_mutex);
    // pthread_mutex_unlock(&GetOutput_mutex);

    CMN_TRACE_EXIT;
}   // LockIoScan()



//!  \fn     UnlockIoScan(void)
//!  \brief  Release GetOutput mutex
//!  \retval  void none
//!
inline void UnlockIoScan(void)
{
    CMN_TRACE_ENTRY;

    // Just use a mutex
    // pthread_mutex_lock(&GetOutput_mutex);
    // pthread_cond_wait(&GetOutput, &GetOutput_mutex);
    pthread_mutex_unlock(&GetOutput_mutex);

    CMN_TRACE_EXIT;
}   // UnlockIoScan()



//!  \fn     LockRingbuf(void)
//!  \brief  This function serialises write operations from the MOVA cradle to
//!     the ring buffer.  It is cradle specific so has no effect for
//!     MovaLibFormatRing and MovaLib's readPacket, thus the ring itself
//!     contains a semaphore
//!
//!  \retval  void none
//!
inline void LockRingbuf(void)
{
    CMN_TRACE_ENTRY;

    pthread_mutex_lock(&Ringbuf_mutex);
    // Just use a mutex
    // pthread_cond_wait(&GetOutput, &GetOutput_mutex);
    // pthread_mutex_unlock(&GetOutput_mutex);

    CMN_TRACE_EXIT;
}   // LockRingbuf()



//!  \fn     UnlockRing(void)
//!  \brief  Release GetOutput mutex
//!
//!  \retval  void none
//!
inline void UnlockRingbuf(void)
{
    CMN_TRACE_ENTRY;

    // Just use a mutex
    // pthread_mutex_lock(&GetOutput_mutex);
    // pthread_cond_wait(&GetOutput, &GetOutput_mutex);
    pthread_mutex_unlock(&Ringbuf_mutex);

    CMN_TRACE_EXIT;
}   // UnlockRingbuf()


//!  \fn     MovaCradleSetStatus(void)
//!  \brief  sets mova status for current MOVA instance
//!  \param  Mstatus MOVA status
//!  \retval  void none
//!
void MovaCradleSetStatus(const mova_status_t Mstatus)
{
    mova_io_t *movaIo;

    CMN_TRACE_ENTRY;

    movaIo = &GIOshmData->mova_io[GInstance];
    if (Mstatus != movaIo->currStatus)
    {
        // we have a change of state
        // record the previous state and raise or clear any faults
        movaIo->prevStatus = movaIo->currStatus;
        movaIo->currStatus = Mstatus;

        // status change state table
        switch (Mstatus)
        {
            case MOVAST_NO_CRB:
                diag_printf("MOVA:%s.  State Change: No CRB", gMovaEqId);
                switch(movaIo->prevStatus)
                {
                    case MOVAST_WARMUP:
                        movaIo->warmupWait = 0; // reset warmup wait count
                        ClearMovaStatusFault(movaIo->prevStatus);
                        break;

                    case MOVAST_MULTI:
                        ClearMovaStatusFault(movaIo->prevStatus);
                        break;

                    case MOVAST_MANUAL_FORCE:
                        movaIo->manualForceWait = 0; // reset force wait count
                        DiagLog(DC_NOTE, "MOVA:%s, State Change: Exit from Manual Force", gMovaEqId);
                        break;

                    default:
                        break;
                }
                DiagLog(DC_NOTE, "MOVA:%s, State Change: MOVA waiting for Controller", gMovaEqId);
                break;

            case MOVAST_MANUAL_FORCE:
                diag_printf("MOVA:%s.  State Change: Manual Force", gMovaEqId);
                switch(movaIo->prevStatus)
                {
                    case MOVAST_WARMUP:
                        movaIo->warmupWait = 0; // reset warmup wait count
                        ClearMovaStatusFault(movaIo->prevStatus);
                        break;

                    case MOVAST_NO_CRB:
                        DiagLog(DC_NOTE, "MOVA:%s, State Change: Exit from MOVA waiting for Controller", gMovaEqId);
                        break;

                    case MOVAST_MULTI:
                        ClearMovaStatusFault(movaIo->prevStatus);
                        break;

                    default:
                        break;
                }
                DiagLog(DC_NOTE, "MOVA:%s, State Change: Manual Force", gMovaEqId);
                movaIo->manualForceWait = 1; // flag to start force wait count
                break;

            case MOVAST_MULTI:
                diag_printf("MOVA:%s.  State Change: Multiple Stage Confirms", gMovaEqId);
                switch(movaIo->prevStatus)
                {
                    case MOVAST_WARMUP:
                        movaIo->warmupWait = 0; // reset warmup wait count
                        ClearMovaStatusFault(movaIo->prevStatus);
                        break;

                    case MOVAST_NO_CRB:
                        DiagLog(DC_NOTE, "MOVA:%s, State Change: Exit from MOVA waiting for Controller", gMovaEqId);
                        break;

                    case MOVAST_MANUAL_FORCE:
                        movaIo->manualForceWait = 0; // reset force wait count
                        DiagLog(DC_NOTE, "MOVA:%s, State Change: Exit from Manual Force", gMovaEqId);
                        break;

                    default:
                        break;
                }
                RaiseMovaStatusFault(movaIo->currStatus);
                break;

            case MOVAST_WARMUP:
                diag_printf("MOVA:%s.  State Change: Warmup", gMovaEqId);
                switch(movaIo->prevStatus)
                {
                    case MOVAST_MULTI:
                        ClearMovaStatusFault(movaIo->prevStatus);
                        break;

                    case MOVAST_NO_CRB:
                        DiagLog(DC_NOTE, "MOVA:%s, State Change: Exit from MOVA waiting for Controller", gMovaEqId);
                        break;

                    case MOVAST_MANUAL_FORCE:
                        movaIo->manualForceWait = 0; // reset force wait count
                        DiagLog(DC_NOTE, "MOVA:%s, State Change: Exit from Manual Force", gMovaEqId);
                        break;

                    default:
                        break;
                }

                movaIo->warmupWait = 1; // flag to start warmup wait count
                break;

            case MOVAST_CONTROL:
                diag_printf("MOVA:%s.  State Change: On Control", gMovaEqId);
                movaIo->warmupWait = 0; // reset warmup wait count
                movaIo->manualForceWait = 0; // reset force wait count
                ClearMovaStatusFault(movaIo->prevStatus);
                break;

            case MOVAST_OFF:
                diag_printf("MOVA:%s.  State Change: Mova Off", gMovaEqId);
                movaIo->warmupWait = 0; // reset warmup wait count
                movaIo->manualForceWait = 0; // reset force wait count
                ClearMovaStatusFault(movaIo->prevStatus);
                break;

            case MOVAST_NO_LICENCE:
                diag_printf("MOVA:%s.  State Change: No MOVA Licence", gMovaEqId);
                break;

            case MOVAST_NO_DATASET:
                diag_printf("MOVA:%s.  State Change: No MOVA Dataset", gMovaEqId);
                break;

            case MOVAST_NOTSTARTED:
                diag_printf("MOVA:%s.  State Change: MOVA Not Started", gMovaEqId);
                break;

            case MOVAST_DEBUGSTART:
                diag_printf("MOVA:%s.  State Change: Debug Start Loop", gMovaEqId);
                break;

            case MOVAST_INIT:
                diag_printf("MOVA:%s.  State Change: MOVA Init", gMovaEqId);
                break;

            case MOVAST_NO_CONFIG:
                diag_printf("MOVA:%s.  State Change: No MOVA Config", gMovaEqId);
                break;

            case MOVAST_SHUT:
                diag_printf("MOVA:%s.  State Change: MOVA Shut", gMovaEqId);
                break;

            case MOVAST_MAX:
                diag_printf("MOVA:%s.  State Change: MOVA MAX", gMovaEqId);
                break;

            default:
                diag_printf("MOVA:%s.  State Change: Unknown State", gMovaEqId);
                break;
        }
    }

    CMN_TRACE_EXIT;
}   // MovaCradleSetStatus()



//!  \fn     MovaCradleGetStatus()
//!  \brief  gets mova status for current MOVA instance
//!
//!  \retval  mova_status_t status of MOVA stream
//!
inline mova_status_t MovaCradleGetStatus(void)
{
    mova_status_t   status;
    mova_io_t *movaIo;

    // CMN_TRACE_ENTRY  // Waste of tracefile space

    movaIo = &GIOshmData->mova_io[GInstance];
    return movaIo->currStatus;

    // CMN_TRACE_EXIT;
}   // MovaCradleGetStatus()


//!  \fn     MovaCradleSetLower()
//!  \brief  sets lower 3 bits of parameter using logical AND 
//!	\param	ioflag signed char* ptr to ioflag that needs to be modified
//!	\param	setBit const int The bitmask of the lower 3 bits to set (must be < 8)
//!  \retval BOOLEAN TRUE = bit set
//!
inline BOOLEAN MovaCradleSetLower(signed char *ioflag, const int setBit)
{
    BOOLEAN status = FALSE;
    signed char upper;
    signed char lower;

    // CMN_TRACE_ENTRY  // Waste of tracefile space

    if (setBit < 8)
    {
        upper = *ioflag >> 3;
        lower = *ioflag & 0x7 ;
        *ioflag = (upper << 3) | setBit;
        status = TRUE;
    }
    // CMN_TRACE_EXIT;

    return status;
} // MovaCradleSetLower()



//!  \fn     MonitorManualForce()
//!  \brief  Monitors the number of seconds MovaLibForce() state has been set and
//!		resets if left in state too long
//!
//!	\param	instance const int User command entered at terminal
//!  \retval  void none
//!
void MonitorManualForce(const int instance)
{
    mova_io_t           *movaIo;
    equip_id_text_t     movaEqId;
    equip_id_t          movaEqIdNum;
    static const int    MaxForceSeconds = 60;

    CMN_TRACE_ENTRY;

    movaEqIdNum = MovaLibInstanceToStream(instance, movaEqId, 0, 0, 0);
    if (movaEqIdNum >= 0)
    {
        movaIo = &GIOshmData->mova_io[instance];

        // check if this instance has been in manual force state too long
        if (movaIo->manualForceWait > 0)
        {
            if (movaIo->currStatus != MOVAST_MANUAL_FORCE)
            {   //reset count
                movaIo->manualForceWait = 0;
            }
            else if (movaIo->manualForceWait < MaxForceSeconds)
            {
                movaIo->manualForceWait++;
            }
            else
            {   // too long so clear this state so clear it
                nForceReq = 0;
                TryForces(MODEM_PORT);
                MovaCradleSetStatus(movaIo->prevStatus);
            }
        }
    }
    CMN_TRACE_EXIT;
}   // MonitorManualForce()



//!  \fn     MovaCradleWritePipe()
//!  \brief  This routine writes to  the ring buffer associated with a MOVA stream
//!  \note	Synchronization: This function sets the readerActive semaphore to serialise.
//!
//!	\param	instance const int instance of MOVA
//!	\param	buffer char* buffer containing data
//!	\param	bufferLen const int size of buffer
//!	\param	returnBufferLen int return length of read buffer
//!  \retval	BOOLEAN TRUE => successful read, FALSE=>failure
//!
inline BOOLEAN MovaCradleWritePipe(const int instance, unsigned char *buffer, const int bufferLen)
{
    BOOLEAN         status = FALSE;

    // CMN_TRACE_ENTRY;

    status = MovaLibWritePipeTo(&GIOshmData->mova_io[instance].xmtPipe, buffer, bufferLen);

    // CMN_TRACE_EXIT;

    return status;
}   // MovaCradleWritePipe()



//!  \fn     MovaCradleReadPipe()
//!  \brief  This routine reads from the ring buffer associated with a MOVA stream
//!  \note	Synchronization: This function sets the readerActive semaphore to serialise.
//!
//!	\param	instance const int instance of MOVA
//!	\param	buffer char* buffer containing data
//!	\param	bufferLen const int size of buffer
//!	\param	returnBufferLen int return length of read buffer
//!  \retval	BOOLEAN TRUE => successful read, FALSE=>failure
//!
inline BOOLEAN MovaCradleReadPipe(const int instance, unsigned char *buffer, const int bufferLen, int *returnBufferLen)
{
    mova_pipe_t     *movaPipe;
    mova_ringbuf_t  *rbp;
    BOOLEAN         status = FALSE;

    // CMN_TRACE_ENTRY;

    status = MovaLibReadPipeFrom(&GIOshmData->mova_io[instance].rcvPipe, buffer, bufferLen, returnBufferLen);

    // CMN_TRACE_EXIT;

    return status;
}   // MovaCradleReadPipe()



//!  \fn     MovaCradleFormatRcvPipe()
//!  \brief  Format the MOVA ring buffer, used for communication between MovaLib and
//!		MovaCradle.  This must be called before the Mova issues MovaCradleReadPipe()
//!
//!	\param	instance const int instance of MOVA
//!  \retval	BOOLEAN TRUE => successful read, FALSE=>failure
//!
BOOLEAN MovaCradleFormatRcvPipe(const int instance)
{
    BOOLEAN          retStat = FALSE;

    CMN_TRACE_ENTRY;

    if (instance >= 0 && instance < MAX_MOVA_IO_ENTRIES && GIOshmData != NULL)
    {
        // params valid
        gCHAR_MODEMptr = NULL;
        MovaLibFormatPipe(&GIOshmData->mova_io[instance].rcvPipe);
        retStat = TRUE;
    } // valid params

    CMN_TRACE_EXIT;
    return retStat;
} // MovaCradleFormatRcvPipe()



//!  \fn     MovaCradleConnectPipe()
//!  \brief  Attach the modem port to MOVA as this is used to pipe data to MovaLib
//!
//!	\param	instance const int instance of MOVA
//!  \retval	BOOLEAN TRUE => successful read, FALSE=>failure
//!
static BOOLEAN MovaCradleConnectPipe(const int instance)
{
    BOOLEAN          retStat = FALSE;
    mova_io_t *movaIo;
    mova_if_t *MovaIf;

    CMN_TRACE_ENTRY;

    if (instance >= 0 && instance < MAX_MOVA_IO_ENTRIES && GIOshmData != NULL)
    {
        movaIo = &GIOshmData->mova_io[GInstance];
        MovaIf = &movaIo->movaIf;

        // attach the modem port to MOVA as this is used to
        // pipe data to MovaLib
        Tcomshr->io[31] = 3;    // Modem timeout 60mins
        Tcomshr->io[29] = 0;
        lLineDropped = 0;   // MovaLib 'modem' port up
        MovaIf->RComms = MOVAIF_COMMS_TO_MOVA;

        retStat = TRUE;
    } // valid params

    CMN_TRACE_EXIT;
    return retStat;
} // MovaCradleConnectPipe()



//!  \fn     MovaCradleDisconnectPipe()
//!  \brief  Detach the modem port to MOVA as this is used to pipe data to MovaLib
//!
//!	\param	instance const int instance of MOVA
//!  \retval	BOOLEAN TRUE => successful read, FALSE=>failure
//!
static BOOLEAN MovaCradleDisconnectPipe(const int instance)
{
    BOOLEAN          retStat = FALSE;
    mova_io_t *movaIo;
    mova_if_t *MovaIf;

    CMN_TRACE_ENTRY;

    if (instance >= 0 && instance < MAX_MOVA_IO_ENTRIES && GIOshmData != NULL)
    {
        movaIo = &GIOshmData->mova_io[GInstance];
        MovaIf = &movaIo->movaIf;
        // detach the modem port to MOVA as this is used to
        // pipe data to MovaLib

        //Tcomshr->io[31] = -1;   // no Modem
        Tcomshr->io[29] = 9;
        lLineDropped = 1;   // MovaLib 'modem' port down
        MovaIf->RComms = MOVAIF_COMMS_TO_OMU;
        lMessOn = FALSE;          // tell term thread that messages output is Off.
        retStat = TRUE;
    } // valid params

    CMN_TRACE_EXIT;
    return retStat;
} // MovaCradleDisonnectPipe()



//!  \fn     MovaCradleThisPipeIsConnected()
//!  \brief  determines if this MOVA instance is connected to
//!              MOVA modem port.
//!
//!  \retval	BOOLEAN TRUE => successful read, FALSE=>failure
//!
static BOOLEAN MovaCradleThisPipeIsConnected()
{
    BOOLEAN PipeConnected = FALSE;
    mova_io_t *movaIo;
    mova_if_t *MovaIf;

    CMN_TRACE_ENTRY;

    if (GIOshmData != NULL)
    {
        movaIo = &GIOshmData->mova_io[GInstance];
        MovaIf = &movaIo->movaIf;
        if (MovaIf->RComms == MOVAIF_COMMS_TO_MOVA)
        {
            PipeConnected = TRUE;
        }
    }

    CMN_TRACE_EXIT;
    return PipeConnected;
} // MovaCradleThisPipeIsConnected()
