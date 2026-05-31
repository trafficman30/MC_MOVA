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
//!  \file    108MovaCradle.h
//!
//!	\brief   Header for abstraction layer and support functions for MOVA module
//!
//!	\author  Andrew Leigh
//!
//!	\date    27-May-2018
//
//! \defgroup MovaCradle  Chameleon MOVA M8 Abstraction Layer
//!  callable MovaCradle functions for M8 
//! \{
//! \} 

static char MovaCradle_h_rcsid[] = "$Id: ";

/* --------------------------------------------------------------------------- *
 *                     INCLUDE FILES                                           *
 * --------------------------------------------------------------------------- *
*/
#include "000Common.h"
#include "501DiagLib.h"
#include "502FaultLib.h"
#include "503SktLib.h"
#include "504CmnLib.h"
#include "505EventLib.h"
#include "108MovaLib.h"


#include "gbltypes.h"
#include "peek.h"

#include "MVTypes.h"
#include "MVTrace.h"
#include "MVMath.h"
//#include "pcerror.h"
#include "pcdatset.h"
//#include "errhand.h"
#include "WinError.h"

#include <pthread.h>
#include <stdarg.h>
#include <sys/mman.h>
#ifndef INCLUDED_Write
#include <write.h>
#endif // INCLUDED_Write


/*
 * --------------------------------------------------------------------------- *
 *                     SYMBOLIC CONSTANTS                                      *
 * --------------------------------------------------------------------------- *
*/
#ifndef __108MOVACRADLE_H
#define __108MOVACRADLE_H

#define INCLUDE_PCDATSET
#define notY2K

#define MAX_MOVA_BUFFER        0x10000
// above is the MOVA download size rounded up to next boundary
#define MAX_MOVA_CMD_LEN        2000
#define MAX_MOVALIB_WATCHDOG    120 // refresh cycles from web browser in 0.5 sec cycles
                                    // this number is chosen as 2*MOVA message timeout
#define RTC_OK 0

#define PCMOVA_DATASET	CONF_DIR"mova.mds"
// number of 1/10s ticks until Output needs to be triggered
#define OUTPUT_TRIGGER_TICKS 10

// PCMOVA tracing support
#define g_AUTOFLUSH         (MV_TRUE)
#define g_ASSERT_MSG        ("Programming error detected (assertion failure)")
#define g_ASSERT_NULL_MSG       ("Programming error detected (assertion failure - invalid address)")
#define g_PROGRAM_ERROR_TITLE ("Programming error. Exit (recommended)?")
#define g_EXIT_MSG         ("Program exited following assertion failure.")
#define g_TRACE_FILE_NAME_DEF       ("trace.log")
#define g_TRACE_FILE_NAME_LEN_MAX   (31)

#define g_TRACE_LEVEL_DEF           (LEVEL_ERROR)

#define g_TRACE_LEVEL_ERROR_NAME    ("TRACE_ERROR")
#define g_TRACE_LEVEL_WARNING_NAME  ("TRACE_WARNING")
#define g_TRACE_LEVEL_INFO_NAME     ("TRACE_INFO")
#define g_TRACE_LEVEL_VERBOSE_NAME  ("TRACE_VERBOSE")
#define g_TRACE_LEVEL_OFF_NAME      ("TRACE_OFF")



/* --------------------------------------------------------------------------- *
 *                     MACRO DEFINITIONS                                       *
 * --------------------------------------------------------------------------- *
*/

/*
 * --------------------------------------------------------------------------- *
 *                     TYPE DEFINITIONS                                        *
 * --------------------------------------------------------------------------- *
 */

typedef struct my_msg_term_string_t
{
    int                 address;
    char                text[MAX_MOVA_CMD_LEN+1];
}  my_msg_term_string_t;

// PCMOVA tracing support

typedef enum
{
    LEVEL_OFF,
    LEVEL_ERROR,
    LEVEL_WARNING,
    LEVEL_INFO,
    LEVEL_VERBOSE

} TraceLevel;

/* --------------------------------------------------------------------------- *
 *                     EXTERNAL GLOBAL DATA REFERENCES                         *
 * --------------------------------------------------------------------------- *
*/
extern MODULE GModule;          // Module number of task
extern int    GInstance;        // Instance number of task
extern int    GMySocket;        // Socket number of task
//extern equip_id_text_t gMovaEqId;
//extern equip_id_t gEqIdNum;

// pthreads

extern pthread_t Mova_pthead;
extern pthread_t TrlUI_pthead;
extern pthread_t Term_pthread;
extern pthread_t Output_pthread;
extern pthread_t sig_thread;
extern pthread_t Dataset_thread;
extern pthread_t Aml_thread;
extern pthread_attr_t  TrlUI_attr;
extern pthread_attr_t  Term_attr;
extern pthread_attr_t  Output_attr;
extern pthread_attr_t  Signal_attr;
extern pthread_attr_t  Dataset_attr;
extern pthread_attr_t  Aml_attr;

// condition variables

extern pthread_cond_t      SuspendOutput;
extern pthread_condattr_t  SuspOut_attr;
extern pthread_cond_t	    tick;
extern pthread_condattr_t  tick_attr;
extern pthread_cond_t	    longtick;
extern pthread_condattr_t  longtick_attr;
extern pthread_cond_t      GetOutput;
extern pthread_condattr_t  GetOutput_attr;
extern pthread_cond_t      DatasetActive;
extern pthread_condattr_t  DatasetActive_attr;

// mutexes

extern pthread_mutex_t     SuspOut_mutex;
extern pthread_mutexattr_t SuspOut_mutex_attr;
extern pthread_mutex_t	    tick_mutex;
extern pthread_mutexattr_t tick_mutex_attr;
extern pthread_mutex_t	    longtick_mutex;
extern pthread_mutexattr_t longtick_mutex_attr;
extern pthread_mutex_t     GetOutput_mutex;
extern pthread_mutexattr_t GetOutput_mutex_attr;
extern pthread_mutex_t     Ringbuf_mutex;
extern pthread_mutexattr_t Ringbuf_mutex_attr;
extern pthread_mutex_t     Dataset_mutex;
extern pthread_mutexattr_t Dataset_mutex_attr;
extern pthread_mutex_t     TrlUi_mutex;
extern pthread_mutexattr_t TrlUi_mutex_attr;
extern pthread_mutex_t     Aml_mutex;
extern pthread_mutexattr_t Aml_mutex_attr;

//extern static const char *idsMessage[ERROR_IDS_NUM];
//extern static const char *errorString[PCMOVA_ERROR_COUNT];
/* --------------------------------------------------------------------------- *
 *                     FUNCTION REFERENCE                                      *
 * --------------------------------------------------------------------------- *
*/

//! \addtogroup MovaCradle
//! \{
/*
 * Abstraction Layer prototypes
 */

// licencing
int Valid_Licence(void);
int check_facility_licence(int LicenceType);

// thread control
void PCTasks_SuspendOutput(void);
void PCTasks_Sleep( unsigned long timeOutmsec, psMOVA_TASK TaskIdent );
void PCTasks_Resume(psMOVA_TASK enTask);

// engineers terminal
void SendStringToModem(unsigned char *sOP_String, int nCharsInString);
char GetMOVACharFromHandset(void);
void SendStringToLH(unsigned char *sOP_String, int nCharsInString);
MVBool PCUserIO_IsConnected( void );
#ifndef INCLUDE_PCDATSET
void PCDatSet_GetDefault(MOVA_CONST_DATA_STRUCT *movaData );
#endif
// chameleon system interface
unsigned char com_read_clock(TIME_TYPE *omu_time);
unsigned char com_set_clock(TIME_TYPE const *omu_time);

// error handling
void PCError_SetLastKernel(MVUInt16, MVInt, const char *, MVSize );
void PCError_SetLast(PCError error, const char * detailMessage, const char * source );
PCError PCError_GetLast( void );
void PCError_ExitKernel( PCError );
void SOFT_ERROR(int reason);

// diagnostic routines
inline void diag_printf(char *string, ...);
void Trace_Assert(MVBool, const char *,  const char *, MVInt32,const char * );
void Trace_AssertNull(const void *, const char *,  const char *, MVInt32, const char * );
static void Trace_Exit( void );
static void Trace_RaiseAssert( const char * message );
void Trace_WriteLineIf( TraceLevel level, const char * prefix, const char * message, ... );

// windows api
HINSTANCE *GetModuleHandle(const char * lpModuleName);
int LoadString(HINSTANCE *hInstance, unsigned int uID, char *lpBuffer, int nBufferMax);
int GetLastError();
void SetLastError(int errorcode);


/*
 * Support Function prototypes
 */
static MESSAGE_ID CheckForSocketInput(const int iDelay, char *buffer, const int bufferLen, int *rtnLen);
static void CreateKey();
void DeleteKey();
void DeleteKeyContents();
inline void waitfortick(void);
inline void waitforlongtick(void);
inline void LockIoScan(void);
inline void UnlockIoScan(void);
inline void LockRingbuf(void);
inline void UnlockRingbuf(void);
inline void waitforDatasetRequest(void);
inline void waitforAmlRequest(void);
inline void signalDatasetThread(void);
static BOOLEAN sendSections(MODULE destModule, unsigned char * section_start, const int section_len);
inline mova_status_t MovaCradleGetStatus(void);
void MovaCradleCheckForRequest(void);
void MovaCradleSetStatus(const mova_status_t Mstatus);
inline static BOOLEAN MovaCradleValidateRequest(const char *RequestType, const msg_mova_request_t *MovaRequest);
inline static BOOLEAN MovaCradleBuildReply(msg_mova_request_t       *MovaLibReply,
                                           const msg_mova_request_t *MovaRequest,
                                           mova_msg_t replyStatus,
                                           int slot);
static int MovaCradleSendReply(const msg_mova_request_t *MovaLibReply);
inline BOOLEAN MovaCradleSetLower(signed char *ioflag, const int setBit);
void MonitorManualForce(const int instance);
inline BOOLEAN MovaCradleWritePipe(const int instance, unsigned char *buffer, const int bufferLen);
inline BOOLEAN MovaCradleReadPipe(const int instance, unsigned char *buffer, const int bufferLen, int *returnBufferLen);
BOOLEAN MovaCradleFormatRcvPipe(const int instance);
char GetMOVACharFromHandset(void);
static void processMovaLibRequest(msg_mova_request_t *MovaRequest);
static void processETermRequest(msg_term_string_t *TermStringMsg, const int textLen, char *buffer, const int bufferLen, int *rtnLen);
static BOOLEAN MovaCradleThisPipeIsConnected();
static BOOLEAN MovaCradleConnectPipe(const int instance);
static BOOLEAN MovaCradleDisconnectPipe(const int instance);

//! \} // end of MovaCradle

#endif /* __100MOVACRADLE_H */
