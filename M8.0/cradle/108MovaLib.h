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
//!  \file    108MovaLib.h
//!	\brief   Header for callable MOVA functions
//!	\author  Andrew Leigh
//!	\date    27-May-2018
//

//! \defgroup MovaLib  Chameleon MOVA M8 Library
//!  callable MovaLib functions for M8 cradle
//  
//! \{
//

//! \} 


#ifndef __108MOVALIB_H
#define __108MOVALIB_H

static char MovaLib_h_rcsid[] = "$Id: ";

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
#include "peek.h"
#include "pcerror.h"
#include "errhand.h"
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/fsuid.h>

/*
 * --------------------------------------------------------------------------- *
 *                     SYMBOLIC CONSTANTS                                      *
 * --------------------------------------------------------------------------- *
*/

// MOVA's gbltypes.h redefines TRUE, lets put it back to normal for chameleon
// libraries
#ifdef TRUE
#undef TRUE
#define TRUE   ((BOOLEAN)1)
#endif

#ifdef FALSE
#undef FALSE
#define FALSE  ((BOOLEAN)0)
#endif

/* --------------------------------------------------------------------------- *
 *                     MACRO DEFINITIONS                                       *
 * --------------------------------------------------------------------------- *
*/

/*
 * --------------------------------------------------------------------------- *
 *                     TYPE DEFINITIONS                                        *
 * --------------------------------------------------------------------------- *
 */

typedef struct movaNonVolStream_t
{
    Ccomshr Tcom;
    Cerrlog Terr;
    Cdinout Tdout;
    struct _nonVol Dataset;
    struct _LastMessage Message;
} movaNonVolStream_t ;

typedef struct movaNonVol_t
{
    movaNonVolStream_t stream[MAX_MOVA_IO_ENTRIES] ;
    BOOLEAN StreamDataLoaded[MAX_MOVA_IO_ENTRIES] ;
} movaNonVol_t ;

typedef struct _MovaLibMemory
{
    int instance;

    Ccomshr *Tcom;
    int      Tcomfd;
    BOOLEAN  TcomNew;

    Cerrlog *Terr;
    int      Terrfd;
    BOOLEAN  TerrNew;

    Cdinout *Tdout;
    int      Tdoutfd;
    BOOLEAN  TdoutNew;

    struct _nonVol *Dataset;
    int      Datasetfd;
    BOOLEAN  DatasetNew;

    struct _LastMessage *Message;
    int      Messagefd;
    BOOLEAN  MessageNew;
} MovaLibMemory_t;


typedef enum mova_restart_t
{
    RT_WARM = 0,
    RT_COLD = 1
} mova_restart_t;


/* --------------------------------------------------------------------------- *
 *                     GLOBAL DATA REFERENCES                                  *
 * --------------------------------------------------------------------------- *
*/

static const char *idsMessage[ERROR_IDS_NUM] =
{
    "Invalid Parameter",  // addition to show out-of-range error for chameleon
    "MOVA Back On Line",
    "Faulty Detector",
    "Stage Not Ended",
    "Intergreen Not Ended",
    "Invalid Stage Demanded",
    "Wrong Stage Confirmed",
    "Stage Stuck On",            /* Deprecated */
    "Watchdog Routine",
    "Controller Ready Bit Off",
    "Controller Ready Bit On",
    "Program Error",
    "Checksum Error",
    "Reloading Error",
    "Multiple Stage Confirms",
    "MOVA Restarted",
    "Divide Error",
    "Empty Repository Area",
    "Repository Checksum",
    "Default Loaded"
};

static const char *errorString[PCMOVA_ERROR_COUNT] =
{
    "No Error",
    "Assertion Failure",
    "Already Init",
    "Param Invalid",
    "Dataset Open",
    "Dataset Read",
    "Dataset Format",
    "Dataset Close",
    "Task Init",
    "Task Deinit",
    "Terminate",
    "Handshake",
    "Initialise",
    "Update",
    "Controller Send",
    "Controller Receive",
    "User Send",
    "Msg Checksum",
    "Msg Length",
    "Msg Start",
    "Msg Id",
    "Msg Type",
    "Msg Data",
    "Time",
    "Controller Init",
    "Controller Deinit",
    "User Init",
    "User Deinit",
    "Not Init",
    "Dataset Version",
    "Dataset Checksum",
    "Kernel Fatal",
    "Kernel User Fatal",
    "Task Suspend",
    "Task Resume",
    "Task Resume"
};

// This array must stay in sync with
// mova_status_t in 000Common.h
static const char *MovaStatusString[MOVAST_MAX] =
{
    "Not Started",
    "Debug Start Loop",
    "No MOVA Licence",
    "No MOVA Configuration",
    "No Dataset",
    "Initial",
    "Off",
    "Multiple Stage Confirms",
    "Warmup",
    "On Control",
    "Shutdown",
    "Controller Not Ready",
    "Manual Force"
};



typedef enum mova_licence_error_t
{
	MOVALICE_OK = 0,
	MOVALICE_INVALID,
	MOVALICE_INVALID_FINGERPRINT,
	MOVALICE_INVALID_DATE
} mova_licence_error_t;



/* --------------------------------------------------------------------------- *
 *                     FUNCTION REFERENCE                                      *
 * --------------------------------------------------------------------------- *
*/
//! \addtogroup MovaLib
//! \{

// extern void UploadData(int nChosen, int nWx);

/*
 * Trigger to send dataset over modem
 */
extern BOOLEAN MovaLibUploadData(const int instance, const int slot);

/*
 * Non-Volatile memory mapping prototypes
 */
extern BOOLEAN MovaLibMapMemToFile(MovaLibMemory_t *MovaMem);
extern void MovaLibUnmapMem(MovaLibMemory_t *MovaMem);
/*
 * General callable MOVA requests
 */
extern equip_id_t MovaLibInstanceToStream(const int instance, char *streamName, char *streamDataSet,
                                   BOOLEAN *MovaOn, mova_if_t **MovaIf);
extern BOOLEAN MovaLibEtConnected(void);
extern BOOLEAN MovaLibPipeConnected(void);
extern BOOLEAN MovaLibConnectPipe(   const int instance);
extern BOOLEAN MovaLibDisconnectPipe(const int instance);
extern BOOLEAN MovaLibReadPipe(const int instance, unsigned char *buffer, const int bufferLen, int *returnBufferLen);
extern BOOLEAN MovaLibWritePipe(const int instance, unsigned char *buffer, const int bufferLen);
extern BOOLEAN MovaLibWritePipeTo(mova_pipe_t *movaIo, unsigned char *buffer, int bufferLen);
extern BOOLEAN MovaLibReadPipeFrom(mova_pipe_t *movaPipe, unsigned char *buffer, const int bufferLen, int *returnBufferLen);

extern BOOLEAN MovaLibOutputCtrl(const int instance, const char *cmd, const int on);
extern BOOLEAN MovaLibStartInstance(const int instance);
extern BOOLEAN MovaLibStopInstance(const int instance);
extern BOOLEAN MovaLibRestartInstance(const int instance, const mova_restart_t restartType);

extern BOOLEAN MovaLibClearErrorFlag(const int instance);
extern BOOLEAN MovaLibDataset(const int instance);

extern BOOLEAN MovaLibCheckLicence(int instance);
extern BOOLEAN MovaLibCreateLicence(const int streams, lice_type_t type, const char *provider,
                                    const char *owner, const unsigned char ownerFingerprint[6]);
extern char *BuildFilePath (char *psFullPath, const char *psDirectory, const char *psFilename);
extern mova_status_t MovaLibGetStatus(const int instance);
extern BOOLEAN MovaLibDisplayStatus(const int instance, char *StatusString, int StatusLen);
extern BOOLEAN MovaLibFormatRing(const int instance);
extern BOOLEAN MovaLibFormatRcvPipe(const int instance);
extern BOOLEAN MovaLibForce(const int instance, const int slot);
extern BOOLEAN MovaLibWriteCommand(const int instance, const int port, unsigned char *buffer, const int bufferLen);
extern BOOLEAN MovaLibDatasetThreadActive(const int instance);
extern BOOLEAN MovaLibGetVersion(char *VersionString, int VersionLen);
extern void MovaLibFormatPipe(mova_pipe_t *movaIo);
extern void MovaLibGetMovaShMem(int Instance) ;
extern movaNonVol_t* MovaLibGetMovaShMemPtr() ;

//! \} // end of MovaLib

/*
 * internal
 */
static struct stat StatMemFile(char const *fname, int len, BOOLEAN *rep);
static void *CreateMemFile(char const *fname, int *fd, int const len, BOOLEAN *repl);
static void  CloseMemFile(void **ds, int dsLen, int *fd);
inline static BOOLEAN  readPacket(mova_ringbuf_t *rbp, unsigned char *buffer, const int bufferLen, int *returnBufferLen);
static BOOLEAN WaitForMovaCradleReply(const int instance, const int movaLibSocket, const msg_mova_request_t *req,
                                        int *MyMovaLibSocket);
static BOOLEAN  writePacketSections(mova_ringbuf_t *rbp, unsigned char * section_start, int section_len);
static mova_licence_error_t ValidLicence(licence_t *LicePtr, const int instance);
static BOOLEAN ValidFingerprint(const licence_t *LicePtr);
static BOOLEAN ValidDate(const licence_t *LicePtr);
#endif /* __100MOVALIB_H */


