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
//!  \file    108MovaLib.c
//!	\brief   Callable MOVA library functions
//!	\author  Andrew Leigh
//!	\date    27-May-2018
//

static char MovaLib_c_rcsid[] = "$Id: ";

/* --------------------------------------------------------------------------- *
 *                     INCLUDE FILES                                           *
 * --------------------------------------------------------------------------- *
*/
#include "108MovaLib.h"

/*
 * --------------------------------------------------------------------------- *
 *                     SYMBOLIC CONSTANTS                                      *
 * --------------------------------------------------------------------------- *
*/

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
static int GMyMovaLibSocket = 0; // Socket number of task
static movaNonVol_t* GMovaNonVolShm = NULL;

/*
 * --------------------------------------------------------------------------- *
 *                   CALLABLE FUNCTIONS                                        *
 *                   Abstraction Layer for MOVA Kernel                         *
 * --------------------------------------------------------------------------- *
 */


//!  \fn     MovaLibGetMovaShMem()
//!  \brief  Get the MOVA shared memory for non volatile storage
//!	\note	Globals: GMovaNonVolShm The shared memory segment
//!
//!	\param	Instance int The instance number for this MOVA stream
//!  \retval void none
//!
void MovaLibGetMovaShMem(int Instance)
{
	key_t semKey;
	unsigned int mem_size;
	void *shmBuf; // Pointer to the share memory buffer
	int cnt;
	int	RTCshm;
	FILE *movaFile;
	int readNum;
	BOOLEAN memExists = FALSE;

	CMN_TRACE_ENTRY;

	if(MovaLibCheckLicence(0) == FALSE)
	{
		CmnTrace(GTraceFileId, "%s: EXIT: No MOVA licences", __FUNCTION__);
		return;
	}

	if(GMovaNonVolShm != NULL)
	{
		CmnTrace(GTraceFileId, "%s: EXIT: Shared memory already mapped", __FUNCTION__);
		return;
	}

	// Generate key unique to system by identifying the Chameleon instance
	semKey = ftok(KEY_FILE_PATH, SHMEM_KEY_MOVA_ID);
	if(semKey == -1)
	{
		CmnTrace(GTraceFileId, "%s: ftok Failed: %s", __FUNCTION__, CmnStrerror(errno));
		DiagLog(DC_ERROR, "MOVA SetUpShm: ftok: %s", CmnStrerror(errno));
	}

	mem_size = sizeof(movaNonVol_t);

	RTCshm = shmget(semKey, mem_size, IPC_CREAT | IPC_EXCL | 0666);

	if((RTCshm == -1) && (errno == EEXIST))
	{
		// Get an existing segment
		RTCshm = shmget(semKey, 0, 0666);
		memExists = TRUE;
	}

	// Test that the function has returned successfully
	if(RTCshm == -1)
	{
		CmnTrace(GTraceFileId, "%s: shmget Failed: %s", __FUNCTION__, CmnStrerror(errno));
		DiagLog(DC_ERROR, "MOVA SetUpShm: shmget: %s", CmnStrerror(errno));
	}

	// Get the memory address of the shared memory area
	shmBuf = shmat(RTCshm, NULL, 0);

	// Test that the function has returned successfully
	if(shmBuf == (void*)-1)
	{
		CmnTrace(GTraceFileId, "%s: shmat Failed: %s", __FUNCTION__, CmnStrerror(errno));
		DiagLog(DC_ERROR, "SetUpShm: shmat: %s", CmnStrerror(errno));
	}

	if(memExists == FALSE)
	{
		movaFile = fopen(CONST_MOVASHM, "r");

		if(movaFile != NULL)
		{
			readNum = fread(shmBuf, mem_size, 1, movaFile);
			if(readNum != 1)
			{
				DiagLog(DC_WARNING, "Failed to read MOVA mem file %s", CONST_MOVASHM);
			}
			else
			{
				memExists = TRUE;
			}

			fclose(movaFile);
		}

		if(memExists == FALSE)
		{
			// Clear all data in allocated memory
			memset(shmBuf, 0, mem_size);
		}
	}

	GMovaNonVolShm = (movaNonVol_t *)shmBuf;

	if(GIOshmData != NULL)
	{
		GIOshmData->movaShmSize = mem_size;
	}

	CMN_TRACE_EXIT;
}



//!  \fn     MovaLibGetMovaShMemPtr()
//!  \brief  Get the MOVA shared memory for non volatile storage
//!	\note	Globals: GMovaNonVolShm The shared memory segment
//!
//!  \retval movaNonVol_t* Pointer to the shared memory segment
//!
movaNonVol_t* MovaLibGetMovaShMemPtr()
{
	if(GMovaNonVolShm == NULL)
	{
		CmnTrace(GTraceFileId, "%s: shm not linked", __FUNCTION__);
		DiagLog(DC_ERROR, "MOVA %s: shm not linked", __FUNCTION__);
	}

	return GMovaNonVolShm;
}



//!  \fn     MovaLibMapMemToFile()
//!  \brief  Get the MOVA shared memory for non volatile storage
//!	\note	Globals: gMovaEqId
//!
//!	\param	MovaMem MovaLibMemory_t* struct containing pointers to mapped memory
//!			 and returns
//!  \retval BOOLEAN TRUE == a mapped file is created/replaced
//!          FALSE == files mapped already exist
//!
BOOLEAN MovaLibMapMemToFile(MovaLibMemory_t *MovaMem)
{
	int shm_fd = -1;
	char streamName[MAX_EQ_ID_LEN + 1];
	uid_t olduid = -1;
	gid_t oldgid = -1;
	MovaMem->TcomNew = FALSE;
	MovaMem->TerrNew = FALSE;
	MovaMem->TdoutNew = FALSE;
	MovaMem->DatasetNew = FALSE;
	MovaMem->MessageNew = FALSE;
	BOOLEAN fileReplaced = FALSE;
	movaNonVol_t* MovaNonVol;

	CMN_TRACE_ENTRY;

	if(getuid() == 0 || geteuid() == 0)
	{
		// if root
		olduid = setfsuid(IOUT_USER_ID);
		oldgid = setfsgid(IOUT_GROUP_ID);
	}

	// ensure shared memory is mapped and get the pointer

	MovaLibGetMovaShMem(MovaMem->instance);
	MovaNonVol = MovaLibGetMovaShMemPtr();

	MovaMem->Tcom = &MovaNonVol->stream[MovaMem->instance].Tcom;
	MovaMem->Terr = &MovaNonVol->stream[MovaMem->instance].Terr;
	MovaMem->Tdout = &MovaNonVol->stream[MovaMem->instance].Tdout;
	MovaMem->Dataset = &MovaNonVol->stream[MovaMem->instance].Dataset;
	MovaMem->Message = &MovaNonVol->stream[MovaMem->instance].Message;

	if(olduid != -1)
	{
		setfsuid(olduid);
	}

	if(oldgid != -1)
	{
		setfsgid(oldgid);
	}

	CMN_TRACE_EXIT;

	return fileReplaced;
} // MovaLibMapMemToFile()



//!  \fn     MovaLibUnmapMem()
//!  \brief  Un-maps MOVA structures from backing file
//!
//!	\param	MovaMem MovaLibMemory_t* struct containing pointers to mapped memory
//!			 and returns
//!  \retval void none
//!
void MovaLibUnmapMem(MovaLibMemory_t *MovaMem)
{
/*	uid_t olduid = -1;
	gid_t oldgid = -1;

	CMN_TRACE_ENTRY;

	if(getuid() == 0 || geteuid() == 0)
	{
		// if root
		olduid = setfsuid(IOUT_USER_ID);
		oldgid = setfsgid(IOUT_GROUP_ID);
	}

	CloseMemFile((void **)&MovaMem->Tcom, sizeof(Ccomshr), &MovaMem->Tcomfd);
	CloseMemFile((void **)&MovaMem->Terr, sizeof(Cerrlog), &MovaMem->Terrfd);
	CloseMemFile((void **)&MovaMem->Tdout, sizeof(Cdinout), &MovaMem->Tdoutfd);
	CloseMemFile((void **)&MovaMem->Dataset, sizeof(struct _nonVol), &MovaMem->Datasetfd);
	CloseMemFile((void **)&MovaMem->Message, sizeof(struct _LastMessage), &MovaMem->Messagefd);

	if(olduid != -1)
	{
		setfsuid(olduid);
	}

	if(oldgid != -1)
	{
		setfsgid(oldgid);
	}

	CMN_TRACE_EXIT */
} // MovaLibUnmapMem()



//!  \fn     MovaLibInstanceToStream()
//!  \brief  Get the MOVA shared memory for non volatile storage
//!	\note	Globals: GMovaNonVolShm The shared memory segment
//!
//!	\param	Instance const int The instance number for this MOVA stream
//!	\param	streamName char* Name of Stream
//!	\param	streamDataSet char* MOVA default dataset for this stream, only used by MOVA
//!		           if its active dataset is non-valid
//!	\param	MovaOn  BOOLEAN* Default state of MOVA ON flag
//!	\param	MovaIf mova_if_t** address of Mova Interface
//!  \retval equip_id_t >0 success, -1 = failure
//!
equip_id_t MovaLibInstanceToStream(const int instance, char *streamName, char *streamDataSet, BOOLEAN *MovaOn, mova_if_t **MovaIf)
{
	mova_io_t *movaIo;
	equipment_t *equipment;
	equip_id_t EqIdNum = -1;

	CMN_TRACE_ENTRY;

	if(GIOshmData != NULL && instance < GIOshmData->numMovaIOEntries)
	{
		movaIo = &GIOshmData->mova_io[instance];
		if(streamDataSet != NULL)
		{
			strncpy(streamDataSet, movaIo->dataset, MAX_MOVA_IO_DATASET_LEN);
		}
		if(MovaOn != NULL)
		{
			if(movaIo->movaOn != 0)
			{
				*MovaOn = TRUE;
			}
			else
			{
				*MovaOn = FALSE;
			}
		}

		// stream (equipment id name)
		equipment = GIOshmData->equipment;
		EqIdNum = movaIo->eqId;
		if(streamName != NULL)
		{
			strncpy(streamName, equipment[movaIo->eqId].idText, MAX_EQ_ID_LEN);
		}

		if(MovaIf != NULL)
		{
			*MovaIf = &movaIo->movaIf;
		}
	}

	CMN_TRACE_EXIT;

	return EqIdNum;
} // MovaLibInstanceToStream()



//!  \fn     MovalibEtConnected()
//!  \brief  determines if any MOVA instance is connected to
//!              MOVA
//!  \retval  BOOLEAN TRUE = connected, FALSE = not connected
//!
BOOLEAN MovaLibEtConnected(void)
{
	mova_io_t *movaIo;
	equipment_t *equipment;
	equip_id_t EqIdNum = 0;
	BOOLEAN EtConnected = FALSE;
	mova_if_t *MovaIf;
	int instance;

	CMN_TRACE_ENTRY;

	if(GIOshmData != NULL)
	{
		for(instance = 0;instance < GIOshmData->numMovaIOEntries;instance++)
		{
			movaIo = &GIOshmData->mova_io[instance];
			MovaIf = &movaIo->movaIf;
			if(MovaIf->LComms == MOVAIF_COMMS_TO_MOVA)
			{
				EtConnected = TRUE;
				break;
			}
		}
	}

	CMN_TRACE_EXIT;

	return EtConnected;
} // MovaLibEtConnected()

//!  \fn     MovaLibPipeConnected()
//!  \brief  determines if any MOVA instance is connected to
//!              MOVA modem port.  Used here to pipe data to
//!              the caller.
//!
//!  \retval  BOOLEAN TRUE = connected, FALSE = not connected
//!
BOOLEAN MovaLibPipeConnected(void)
{
	mova_io_t *movaIo;
	equipment_t *equipment;
	equip_id_t EqIdNum = 0;
	BOOLEAN PipeConnected = FALSE;
	mova_if_t *MovaIf;
	int instance;

	CMN_TRACE_ENTRY;

	if(GIOshmData != NULL)
	{
		for(instance = 0;instance < GIOshmData->numMovaIOEntries;instance++)
		{
			movaIo = &GIOshmData->mova_io[instance];
			MovaIf = &movaIo->movaIf;
			if(MovaIf->RComms == MOVAIF_COMMS_TO_MOVA)
			{
				PipeConnected = TRUE;
				break;
			}
		}
	}

	CMN_TRACE_EXIT;

	return PipeConnected;
} // MovaLibPipeConnected()



//!  \fn     MovaLibPipeConnected()
//!  \brief  determines if any MOVA instance is connected to
//!              MOVA modem port.  Used here to pipe data to
//!              the caller.
//!
//!	\param	Instance const int Instance number of MOVA stream
//!  \retval  BOOLEAN TRUE = connected, FALSE = not connected
//!
BOOLEAN MovaLibDatasetThreadActive(const int instance)
{
	mova_io_t *movaIo;
	BOOLEAN DatasetActive = FALSE;

	CMN_TRACE_ENTRY;

	if(GIOshmData != NULL)
	{
		movaIo = &GIOshmData->mova_io[instance];
		if(movaIo->DatasetThreadActive != FALSE)
		{
			DatasetActive = TRUE;
		}
	}

	CMN_TRACE_EXIT;

	return DatasetActive;
} // MovaLibDatasetThreadActive()



//!  \fn     MovaLibUploadData()
//!  \brief  Trigger to ask MOVA to send data to modem
//!
//!	\param	Instance const int Instance number of MOVA stream
//!	\param	slot const int registry number of the requested dataset
//!  \retval  BOOLEAN TRUE = connected, FALSE = not connected
//!
BOOLEAN MovaLibUploadData(const int instance, const int slot)
{
	static int sequence = 1;
	BOOLEAN rtn = FALSE;
	equip_id_text_t movaEqId;
	equip_id_t movaEqIdNum;
	struct mova_if_t *movaIf; // Mova Interface (used for OMU communication)
	msg_mova_request_t req;
	struct sockaddr To;
	static const MODULE ModuleMovaLib = MOD_MOVALIB;
	static const MODULE ModuleMovaMsg = MOD_MOVA;
	BOOLEAN MovaCommsEnabledHere = FALSE;
	int MyMovaLibSocket = 0;

	CMN_TRACE_ENTRY;

	MyMovaLibSocket = SktCreateSocket("MovaLibUploadData", ModuleMovaLib, 0);
	movaEqIdNum = MovaLibInstanceToStream(instance, movaEqId, 0, 0, &movaIf);
	if(movaEqIdNum >= 0 )
	{
		if(movaIf->LComms == MOVAIF_COMMS_TO_OMU)
		{
			movaIf->LComms = MOVAIF_COMMS_TO_MOVA;
			MovaCommsEnabledHere = TRUE;
		}

		strncpy(req.request, "UPLOAD", MAX_MOVALIB_MSG_LEN);
		req.slot = slot;
		req.seq = sequence++;
		req.type = MM_REQUEST;
		req.status = MM_SUCCESS;

		// Create and send to Mova Message socket
		SktAddress(&To, ModuleMovaMsg, instance);
		SktSendMessageBlock(__FUNCTION__, MOVA_REQUEST_ID, MyMovaLibSocket, sizeof(req), (char *)&req, &To);
		// AML default wait tick is 5 seconds
		// may need to revisit this timer or use a blocking call
		rtn = WaitForMovaCradleReply(instance, MyMovaLibSocket, &req, &MyMovaLibSocket);
		SktCloseSocket("MovaLibUploadData", MyMovaLibSocket);
	}

	if(MovaCommsEnabledHere == TRUE)
	{
		movaIf->LComms = MOVAIF_COMMS_TO_OMU;
	}

	CMN_TRACE_EXIT;

	return rtn;
} // MovaLibUploadData()



//!  \fn     MovaLibForce()
//!  \brief  Request MOVA to set force bits
//!
//!	\param	Instance const int Instance number of MOVA stream
//!	\param	slot const int registry number of the requested dataset
//!  \retval  BOOLEAN TRUE = connected, FALSE = not connected
//!
BOOLEAN MovaLibForce(const int instance, const int slot)
{
	static int sequence = 1;
	BOOLEAN rtn = FALSE;
	equip_id_text_t movaEqId;
	equip_id_t movaEqIdNum;
	struct mova_if_t *movaIf; // Mova Interface (used for OMU communication)
	msg_mova_request_t req;
	struct sockaddr To;
	static const MODULE ModuleMovaLib = MOD_MOVALIB;
	static const MODULE ModuleMovaMsg = MOD_MOVA;
	BOOLEAN MovaCommsEnabledHere = FALSE;
	int MyMovaLibSocket = 0;

	CMN_TRACE_ENTRY;

	MyMovaLibSocket = SktCreateSocket("MovaLibForce", ModuleMovaLib, 0);
	movaEqIdNum = MovaLibInstanceToStream(instance, movaEqId, 0, 0, &movaIf);
	if(movaEqIdNum >= 0 )
	{
		if(movaIf->LComms == MOVAIF_COMMS_TO_OMU)
		{
			movaIf->LComms = MOVAIF_COMMS_TO_MOVA;
			MovaCommsEnabledHere = TRUE;
		}

		strncpy(req.request, "FORCE", MAX_MOVALIB_MSG_LEN);
		req.slot = slot;
		req.seq = sequence++;
		req.type = MM_REQUEST;
		req.status = MM_SUCCESS;

		// Create and send to Mova Message socket
		SktAddress(&To, ModuleMovaMsg, instance);
		SktSendMessageBlock(__FUNCTION__, MOVA_REQUEST_ID, MyMovaLibSocket, sizeof(req), (char *)&req, &To);
		// AML default wait tick is 5 seconds
		// may need to revisit this timer or use a blocking call
		rtn = WaitForMovaCradleReply(instance, MyMovaLibSocket, &req, &MyMovaLibSocket);
		SktCloseSocket("MovaLibForce", MyMovaLibSocket);
	}

	if(MovaCommsEnabledHere == TRUE)
	{
		movaIf->LComms = MOVAIF_COMMS_TO_OMU;
	}

	CMN_TRACE_EXIT;

	return rtn;
} // MovaLibForce()



//!  \fn     MovaLibConnectPipe()
//!  \brief  Ask MOVA to connect to modem.  This is
//!              piped via a ring buffer to MovaLib.
//!
//!	\param	Instance const int Instance number of MOVA stream
//!  \retval  BOOLEAN TRUE = connected, FALSE = not connected
//!
BOOLEAN MovaLibConnectPipe(const int instance)
{
	static int sequence = 1;
	BOOLEAN rtn = FALSE;
	equip_id_text_t movaEqId;
	equip_id_t movaEqIdNum;
	struct mova_if_t *movaIf; // Mova Interface (used for OMU communication)
	msg_mova_request_t req;
	struct sockaddr To;
	static const MODULE ModuleMovaLib = MOD_MOVALIB;
	static const MODULE ModuleMovaMsg = MOD_MOVA;
	int MyMovaLibSocket = 0;

	CMN_TRACE_ENTRY;

	MyMovaLibSocket = SktCreateSocket("MovaLibConnectPipe", ModuleMovaLib, 0);
	movaEqIdNum = MovaLibInstanceToStream(instance, movaEqId, 0, 0, &movaIf);
	if(movaEqIdNum >= 0)
	{
		movaIf->LComms = MOVAIF_COMMS_TO_MOVA;

		strncpy(req.request, "CONNECT_PIPE", MAX_MOVALIB_MSG_LEN);
		req.slot = 0;
		req.seq = sequence++;
		req.type = MM_REQUEST;
		req.status = MM_SUCCESS;

		// Create and send to Mova Message socket
		SktAddress(&To, ModuleMovaMsg, instance);
		SktSendMessageBlock(__FUNCTION__, MOVA_REQUEST_ID, MyMovaLibSocket, sizeof(req), (char *)&req, &To);
		rtn = WaitForMovaCradleReply(instance, MyMovaLibSocket, &req, &MyMovaLibSocket);
		SktCloseSocket("MovaLibConnectPipe", MyMovaLibSocket);
	}

	CMN_TRACE_EXIT;

	return rtn;
} // MovaLibConnectPipe()



//!  \fn     MovaDisconnectPipe()
//!  \brief  Ask MOVA to disconnect to modem.  This is
//!              piped via a ring buffer to MovaLib.
//!
//!	\param	Instance const int Instance number of MOVA stream
//!  \retval  BOOLEAN TRUE = connected, FALSE = not connected
//!
BOOLEAN MovaLibDisconnectPipe(const int instance)
{
	static int sequence = 1;
	BOOLEAN rtn = FALSE;
	equip_id_text_t movaEqId;
	equip_id_t movaEqIdNum;
	struct mova_if_t *movaIf; // Mova Interface (used for OMU communication)
	msg_mova_request_t req;
	struct sockaddr To;
	static const MODULE ModuleMovaLib = MOD_MOVALIB;
	static const MODULE ModuleMovaMsg = MOD_MOVA;
	int MyMovaLibSocket = 0;

	CMN_TRACE_ENTRY;

	MyMovaLibSocket = SktCreateSocket("MovaLibDisconnectPipe", ModuleMovaLib, 0);
	movaEqIdNum = MovaLibInstanceToStream(instance, movaEqId, 0, 0, &movaIf);
	if(movaEqIdNum >= 0)
	{
		// only send message if slot is valid
		strncpy(req.request, "DISCONNECT_PIPE", MAX_MOVALIB_MSG_LEN);
		req.slot = 0;
		req.seq = sequence++;
		req.type = MM_REQUEST;
		req.status = MM_SUCCESS;

		// Create and send to Mova Message socket
		SktAddress(&To, ModuleMovaMsg, instance);
		SktSendMessageBlock(__FUNCTION__, MOVA_REQUEST_ID, MyMovaLibSocket, sizeof(req), (char *)&req, &To);

		rtn = WaitForMovaCradleReply(instance, MyMovaLibSocket, &req, &MyMovaLibSocket);
		movaIf->LComms = MOVAIF_COMMS_TO_OMU;
		SktCloseSocket("MovaLibDisonnectPipe", MyMovaLibSocket);
	}

	CMN_TRACE_EXIT;

	return rtn;
} // MovaLibDisonnectPipe()



//!  \fn     MovaLibReadPipe()
//!  \brief  This routine reads from the ring buffer associated with a MOVA stream
//!	\note	Synchronization: This function sets the readerActive semaphore to serialise.
//!
//!	\param	instance unsigned char* instance number of MOVA
//!	\param	buffer unsigned char* pointer to buffer to read into
//!	\param	bufferLen const int length of read buffer
//!	\param	returnBufferLen int* returned length of read
//!  \retval BOOLEAN TRUE => successful read, FALSE=>failure 
//!
BOOLEAN MovaLibReadPipe(const int instance, unsigned char *buffer, const int bufferLen, int *returnBufferLen)
{
	mova_pipe_t *movaPipe;
	mova_ringbuf_t *rbp;
	BOOLEAN status = FALSE;

	// CMN_TRACE_ENTRY;

	status = MovaLibReadPipeFrom(&GIOshmData->mova_io[instance].xmtPipe, buffer, bufferLen, returnBufferLen);

	// CMN_TRACE_EXIT;

	return status;
} // MovaLibReadPipe()



//!  \fn     MovaLibWriteCommand()
//!  \brief  This routine reads from the ring buffer associated with a MOVA stream
//!	\note	Synchronization: This function uses ERRLOG_BUFFER_PORT as a semaphore.
//!		 TBC consider mutex.  Globals: Terrlog, GInstance
//!
//!	\param	instance unsigned char* instance number of MOVA
//!	\param	port const int 2 = modem, 3 = handset
//!	\param	buffer unsigned char* pointer to buffer to read into
//!	\param	bufferLen const int length of string
//!	\param	returnBufferLen int* returned length of read
//!  \retval BOOLEAN TRUE => successful read, FALSE=>failure 
//!
BOOLEAN MovaLibWriteCommand(const int instance, const int port, unsigned char *buffer, const int bufferLen)
{
	BOOLEAN status = FALSE;
	MovaLibMemory_t MovaMem;
	static const int timeOut = 500 * 1000;
	int timeCount = 0;
	int maxBuffLen;
	Cerrlog *Terrlog;

	CMN_TRACE_ENTRY;

	bzero(&MovaMem, sizeof(MovaMem));
	MovaMem.instance = instance;

	MovaLibMapMemToFile(&MovaMem);
	Terrlog = MovaMem.Terr;

	while(Terrlog->buffer[ERRLOG_BUFFER_PORT] != 0 && timeCount < 10)
	{
		usleep(timeOut);
		timeCount++;
	}

	if(Terrlog->buffer[ERRLOG_BUFFER_PORT] == 0)
	{
		strncpy(&Terrlog->buffer[ERRLOG_BUFFER_INPUT_START], buffer, min(bufferLen, ERRLOG_BUFFER_SIZE));
		Terrlog->buffer[ERRLOG_BUFFER_SIZE-1] = '\0'; // force terminate string
		Terrlog->buffer[ERRLOG_BUFFER_PORT] = port;
		status = TRUE;
	}

	MovaLibUnmapMem(&MovaMem);

	CMN_TRACE_EXIT;

	return status;
} // MovaWriteCommand()



//!  \fn     MovaLibOutputCtrl()
//!  \brief  Asks MOVA to constol the displays from the output thread
//!	\note	Valid commands are:
//!          "MESSAGE", "FLOW", "ASSESSMENT", "RECORD_ASSESSMENT"
//!          "ERROR", "ABANDON", "RECORD_FLOW"
//!
//!	\param	instance unsigned char* instance number of MOVA
//!	\param	cmd const See Note.
//!	\param	on const intswitch to control output 1=on, 0=off
//!  \retval BOOLEAN TRUE => successful read, FALSE=>failure 
//!
BOOLEAN MovaLibOutputCtrl(const int instance, const char *cmd, const int slot)
{
	static int sequence = 1;
	BOOLEAN rtn = FALSE;
	equip_id_text_t movaEqId;
	equip_id_t movaEqIdNum;
	struct mova_if_t *movaIf; // Mova Interface (used for OMU communication)
	msg_mova_request_t req;
	struct sockaddr To;
	static const MODULE ModuleMovaLib = MOD_MOVALIB;
	static const MODULE ModuleMovaMsg = MOD_MOVA;
	BOOLEAN MovaCommsEnabledHere = FALSE;
	int MyMovaLibSocket = 0;

	CMN_TRACE_ENTRY;

	MyMovaLibSocket = SktCreateSocket("MovaLibOutputCtrl", ModuleMovaLib, 0);
	movaEqIdNum = MovaLibInstanceToStream(instance, movaEqId, 0, 0, &movaIf);
	if(movaEqIdNum >= 0 && cmd != NULL)
	{
		if(movaIf->LComms == MOVAIF_COMMS_TO_OMU)
		{
			movaIf->LComms = MOVAIF_COMMS_TO_MOVA;
			MovaCommsEnabledHere = TRUE;
		}

		strncpy(req.request, cmd, MAX_MOVALIB_MSG_LEN);
		req.slot = slot;
		req.slot2 = 0;
		req.seq = sequence++;
		req.type = MM_REQUEST;
		req.status = MM_SUCCESS;

		// Create and send to Mova Message socket
		SktAddress(&To, ModuleMovaMsg, instance);
		SktSendMessageBlock(__FUNCTION__, MOVA_REQUEST_ID, MyMovaLibSocket, sizeof(req), (char *)&req, &To);
		rtn = WaitForMovaCradleReply(instance, MyMovaLibSocket, &req, &MyMovaLibSocket);
		SktCloseSocket("MovaLibOutputCtrl", MyMovaLibSocket);
	}

	if(MovaCommsEnabledHere == TRUE)
	{
		movaIf->LComms = MOVAIF_COMMS_TO_OMU;
	}

	CMN_TRACE_EXIT;

	return rtn;
} // MovaLibOutputCtrl()



//!  \fn     MovaLibStartInstance()
//!  \brief  Starts/Stops a MOVA instance
//!	\param	instance instance of Mova to send command
//!  \retval  BOOLEAN TRUE=> success, FALSE=> failure
//!
BOOLEAN MovaLibStartInstance(const int instance)
{
	static int sequence = 1;
	BOOLEAN rtn = FALSE;
	equip_id_text_t movaEqId;
	equip_id_t movaEqIdNum;
	struct mova_if_t *movaIf; // Mova Interface (used for OMU communication)
	msg_mova_request_t req;
	struct sockaddr To;
	static const MODULE ModuleMovaLib = MOD_MOVALIB;
	static const MODULE ModuleMovaMsg = MOD_MOVA;
	BOOLEAN MovaCommsEnabledHere = FALSE;
	int MyMovaLibSocket = 0;

	CMN_TRACE_ENTRY;

	MyMovaLibSocket = SktCreateSocket("MovaLibStartInstance", ModuleMovaLib, 0);
	movaEqIdNum = MovaLibInstanceToStream(instance, movaEqId, 0, 0, &movaIf);
	if(movaEqIdNum >= 0 )
	{
		if(movaIf->LComms == MOVAIF_COMMS_TO_OMU)
		{
			movaIf->LComms = MOVAIF_COMMS_TO_MOVA;
			MovaCommsEnabledHere = TRUE;
		}

		strncpy(req.request, "START", MAX_MOVALIB_MSG_LEN);
		req.slot = 1;
		req.seq = sequence++;
		req.type = MM_REQUEST;
		req.status = MM_SUCCESS;

		// Create and send to Mova Message socket
		SktAddress(&To, ModuleMovaMsg, instance);
		SktSendMessageBlock(__FUNCTION__, MOVA_REQUEST_ID, MyMovaLibSocket, sizeof(req), (char *)&req, &To);
		rtn = WaitForMovaCradleReply(instance, MyMovaLibSocket, &req, &MyMovaLibSocket);
		SktCloseSocket("MovaLibStartInstance", MyMovaLibSocket);
	}

	if(MovaCommsEnabledHere == TRUE)
	{
		movaIf->LComms = MOVAIF_COMMS_TO_OMU;
	}

	CMN_TRACE_EXIT;

	return rtn;
} // MovaLibStartInstance()



//!  \fn     MovaLibStopInstance()
//!  \brief  Starts/Stops a MOVA instance
//!	\param	instance instance of Mova to send command
//!  \retval  BOOLEAN TRUE=> success, FALSE=> failure
//!
BOOLEAN MovaLibStopInstance(const int instance)
{
	static int sequence = 1;
	BOOLEAN rtn = FALSE;
	equip_id_text_t movaEqId;
	equip_id_t movaEqIdNum;
	struct mova_if_t *movaIf; // Mova Interface (used for OMU communication)
	msg_mova_request_t req;
	struct sockaddr To;
	static const MODULE ModuleMovaLib = MOD_MOVALIB;
	static const MODULE ModuleMovaMsg = MOD_MOVA;
	BOOLEAN MovaCommsEnabledHere = FALSE;
	int MyMovaLibSocket = 0;

	CMN_TRACE_ENTRY;

	MyMovaLibSocket = SktCreateSocket("MovaLibStopInstance", ModuleMovaLib, 0);
	movaEqIdNum = MovaLibInstanceToStream(instance, movaEqId, 0, 0, &movaIf);
	if(movaEqIdNum >= 0 )
	{
		if(movaIf->LComms == MOVAIF_COMMS_TO_OMU)
		{
			movaIf->LComms = MOVAIF_COMMS_TO_MOVA;
			MovaCommsEnabledHere = TRUE;
		}

		strncpy(req.request, "STOP", MAX_MOVALIB_MSG_LEN);
		req.slot = 0;
		req.seq = sequence++;
		req.type = MM_REQUEST;
		req.status = MM_SUCCESS;

		// Create and send to Mova Message socket
		SktAddress(&To, ModuleMovaMsg, instance);
		SktSendMessageBlock(__FUNCTION__, MOVA_REQUEST_ID, MyMovaLibSocket, sizeof(req), (char *)&req, &To);
		rtn = WaitForMovaCradleReply(instance, MyMovaLibSocket, &req, &MyMovaLibSocket);
		SktCloseSocket("MovaLibStopInstance", MyMovaLibSocket);
	}

	if(MovaCommsEnabledHere == TRUE)
	{
		movaIf->LComms = MOVAIF_COMMS_TO_OMU;
	}

	CMN_TRACE_EXIT;

	return rtn;
} // MovaLibStopInstance()



//!  \fn     MovaLibRestartInstance()
//!  \brief  Asks a MOVA instance to unmap its non-volatile memory
//!  	and restart the instance
//!	\note	Environment: root priv is required before calling this function
//!
//!	\param	instance instance of Mova to send command
//!	\param	cold TRUE => cold restart, FALSE => warm restart
//!  \retval  BOOLEAN TRUE=> success, FALSE=> failure
//!
BOOLEAN MovaLibRestartInstance(const int instance, const mova_restart_t restartType)
{
	static int sequence = 1;
	BOOLEAN rtn = FALSE;
	equip_id_text_t movaEqId;
	equip_id_t movaEqIdNum;
	char rmShmStr[MAX_PATHNAME_LEN];
	int i;
	static const int timeOut = 500 * 1000;

	CMN_TRACE_ENTRY;

	movaEqIdNum = MovaLibInstanceToStream(instance, movaEqId, 0, 0, 0);
	if(movaEqIdNum >= 0 )
	{
		for(i = 0;i < GIOshmData->numProcesses;i++)
		{
			if(GIOshmData->processData[i].mod == MOD_MOVA && GIOshmData->processData[i].instance == instance)
			{
				// found the MOVA instance
				rtn = TRUE;

				CmnTrace(GTraceFileId, "%s: Found pid %d, mod %d, instance %d", __FUNCTION__, GIOshmData->processData[i].pid, GIOshmData->processData[i].mod, GIOshmData->processData[i].instance);

				if(restartType == RT_COLD)
				{
					// cold restart required
					// 'ask' MOVA instance to unmap and close backing store
					if(kill(GIOshmData->processData[i].pid, SIGRTMIN) != 0)
					{
						CmnTrace(GTraceFileId, "%s: kill SIGRTMIN failed %s", __FUNCTION__, strerror(errno));
						rtn = FALSE;
					}

					// build up command
					strcpy(rmShmStr, CONST_REMOVE_MOVASHMBEGIN);
					strcat(rmShmStr, movaEqId);
					strcat(rmShmStr, CONST_REMOVE_MOVASHMEND);

					// remove MOVA backing store *.shm files
					usleep(timeOut);
					CmnSystem(rmShmStr);
					usleep(timeOut);

					// MOVA instance shutdown without signal handling so kill
					// it with a sure kill
					if(kill(GIOshmData->processData[i].pid, SIGKILL) != 0)
					{
						CmnTrace(GTraceFileId, "%s: kill SIGHUP failed %s", __FUNCTION__, strerror(errno));
						rtn = FALSE;
					}
				}
				else
				{
					// restartType == RT_WARM
					// now restart the instance of MOVA
					if(kill(GIOshmData->processData[i].pid, SIGTERM) != 0)
					{
						CmnTrace(GTraceFileId, "%s: kill SIGHUP failed %s", __FUNCTION__, strerror(errno));
						rtn = FALSE;
					}
				}
				break;
			} // if found
		} // foreach process

		// did it work?
		if(rtn == TRUE)
		{
			CmnTrace(GTraceFileId, "%s(%s, %d): Request sent to pid %d, mod %d, instance %d", __FUNCTION__, movaEqId, restartType, GIOshmData->processData[i].pid, GIOshmData->processData[i].mod, GIOshmData->processData[i].instance);
		}
		else
		{
			CmnTrace(GTraceFileId, "%s(%s, %d): Request sent failed", __FUNCTION__, movaEqId, restartType);
		}
	} // if valid MOVA request

	CMN_TRACE_EXIT;

	return rtn;
} // MovaLibRestartInstance()



//!  \fn     MovaLibClearErrorFlag(const int instance)
//!  \brief  Clears Error flag for MOVA instance
//!	\param	instance instance of Mova to send command
//!  \retval  BOOLEAN TRUE=> success, FALSE=> failure
//!
BOOLEAN MovaLibClearErrorFlag(const int instance)
{
	static int sequence = 1;
	BOOLEAN rtn = FALSE;
	equip_id_text_t movaEqId;
	equip_id_t movaEqIdNum;
	struct mova_if_t *movaIf; // Mova Interface (used for OMU communication)
	msg_mova_request_t req;
	struct sockaddr To;
	static const MODULE ModuleMovaLib = MOD_MOVALIB;
	static const MODULE ModuleMovaMsg = MOD_MOVA;
	BOOLEAN MovaCommsEnabledHere = FALSE;
	int MyMovaLibSocket = 0;

	CMN_TRACE_ENTRY;

	MyMovaLibSocket = SktCreateSocket("MovaLibClearErrorFlag", ModuleMovaLib, 0);
	movaEqIdNum = MovaLibInstanceToStream(instance, movaEqId, 0, 0, &movaIf);
	if(movaEqIdNum >= 0 )
	{
		if(movaIf->LComms == MOVAIF_COMMS_TO_OMU)
		{
			movaIf->LComms = MOVAIF_COMMS_TO_MOVA;
			MovaCommsEnabledHere = TRUE;
		}

		strncpy(req.request, "CLEAR", MAX_MOVALIB_MSG_LEN);
		req.slot = 0;
		req.seq = sequence++;
		req.type = MM_REQUEST;
		req.status = MM_SUCCESS;

		// Create and send to Mova Message socket
		SktAddress(&To, ModuleMovaMsg, instance);
		SktSendMessageBlock(__FUNCTION__, MOVA_REQUEST_ID, MyMovaLibSocket, sizeof(req), (char *)&req, &To);
		rtn = WaitForMovaCradleReply(instance, MyMovaLibSocket, &req, &MyMovaLibSocket);
		SktCloseSocket("MovaLibClearErrorFlag", MyMovaLibSocket);
	}

	if(MovaCommsEnabledHere == TRUE)
	{
		movaIf->LComms = MOVAIF_COMMS_TO_OMU;
	}

	CMN_TRACE_EXIT;

	return rtn;
} // MovaLibClearErrorFlag()



//!  \fn     MovaLibDataset(const int instance)
//!  \brief  Triggers dataset thread for the specified MOVA stream
//!
//!	\param	instance instance of Mova to send command
//!  \retval  BOOLEAN TRUE=> success, FALSE=> failure
//!
BOOLEAN MovaLibDataset(const int instance)
{
	static int sequence = 1;
	BOOLEAN rtn = FALSE;
	equip_id_text_t movaEqId;
	equip_id_t movaEqIdNum;
	struct mova_if_t *movaIf; // Mova Interface (used for OMU communication)
	msg_mova_request_t req;
	struct sockaddr To;
	static const MODULE ModuleMovaLib = MOD_MOVALIB;
	static const MODULE ModuleMovaMsg = MOD_MOVA;
	BOOLEAN MovaCommsEnabledHere = FALSE;
	int MyMovaLibSocket = 0;

	CMN_TRACE_ENTRY;

	MyMovaLibSocket = SktCreateSocket("MovaLibDataset", ModuleMovaLib, 0);
	movaEqIdNum = MovaLibInstanceToStream(instance, movaEqId, 0, 0, &movaIf);
	if(movaEqIdNum >= 0 )
	{
		if(movaIf->LComms == MOVAIF_COMMS_TO_OMU)
		{
			movaIf->LComms = MOVAIF_COMMS_TO_MOVA;
			MovaCommsEnabledHere = TRUE;
		}

		strncpy(req.request, "DATASET", MAX_MOVALIB_MSG_LEN);
		req.slot = 1;
		req.seq = sequence++;
		req.type = MM_REQUEST;
		req.status = MM_SUCCESS;

		// Create and send to Mova Message socket
		SktAddress(&To, ModuleMovaMsg, instance);
		SktSendMessageBlock(__FUNCTION__, MOVA_REQUEST_ID, MyMovaLibSocket, sizeof(req), (char *)&req, &To);
		rtn = WaitForMovaCradleReply(instance, MyMovaLibSocket, &req, &MyMovaLibSocket);
		SktCloseSocket("MovaLibDataset", MyMovaLibSocket);
	}

	if(MovaCommsEnabledHere == TRUE)
	{
		movaIf->LComms = MOVAIF_COMMS_TO_OMU;
	}

	CMN_TRACE_EXIT;

	return rtn;
} // MovaLibDataset()

/*!
 *  \brief	MovaLibCheckLicence
 *
 *	Check for a loaded MOVA licence
 *
 *  \param	instance
 *
 *  \retval	BOOLEAN TRUE = valid licence, FALSE = not-valid
 *
 */
BOOLEAN MovaLibCheckLicence(int instance)
{
	mova_licence_error_t isValid = MOVALICE_INVALID;
	licence_t *MovaLicePtr;

	CMN_TRACE_ENTRY;

	if(GIOshmData != NULL)
	{
		// check for permanent licence first
		MovaLicePtr = CmnCheckForLicence(GIOshmData->licence, "MOVA", 6, 0, PERMANENT);
		isValid = ValidLicence(MovaLicePtr, instance);

		//! Allow MOVA to run when this host has been swapped out
		if(isValid == MOVALICE_INVALID_FINGERPRINT)
		{
			isValid = MOVALICE_OK;
		}

		if(isValid == MOVALICE_INVALID)
		{
			// check for temporary licence
			MovaLicePtr = CmnCheckForLicence(GIOshmData->licence, "MOVA", 6, 0, TEMPORARY);
			isValid = ValidLicence(MovaLicePtr, instance);
		}
	} // shared mem?

	CMN_TRACE_EXIT;

	return (isValid == MOVALICE_OK);
} // MovaLibCheckLicence()

/*!
 *  \brief	ValidLicence
 *
 *	Check for a valid licence
 *
 *  \param	licence_t* pointer to licence type
 *
 *  \param	instance of target product (starts from 0)
 *
 *  \retval	mova_licence_error_t status of licence 0 for success > 0 for error
 *
 */
static mova_licence_error_t ValidLicence(licence_t *LicePtr, const int instance)
{
	mova_licence_error_t isValid = MOVALICE_INVALID;
	BOOLEAN validFingerprint = FALSE;
	BOOLEAN validDate = FALSE;
	static const char notThisHost[] = "Stream:%d, MOVA licence is not issued to this host";
	static const char outOfDate[] = "Stream:%d, MOVA licence out of date";
	const char *errorMessage = notThisHost;
	static BOOLEAN licMsgSent = FALSE;

	if(LicePtr != FALSE && instance < LicePtr->body.streams)
	{
		validFingerprint = ValidFingerprint(LicePtr);
		validDate = ValidDate(LicePtr);

		if(!validDate)
		{
			isValid = MOVALICE_INVALID_DATE;
			errorMessage = outOfDate;
		}
		else if(!validFingerprint)
		{
			isValid = MOVALICE_INVALID_FINGERPRINT;
			errorMessage = notThisHost;
		}
		else
		{
			isValid = MOVALICE_OK;
		}

		// record in syslog
		if(!LicePtr->stats.validityCheck)
		{
			if(isValid != MOVALICE_OK)
			{
				if(licMsgSent == FALSE)
				{
					// warn of mismatch
					DiagLog(DC_WARNING, errorMessage, instance);
					licMsgSent = TRUE;
				}
			}
		}

		LicePtr->stats.validityCheck = TRUE;
	}

	return isValid;
}

/*!
 *  \fn	ValidFingerprint
 *
 *	\brief Check for a valid licence fingerprint
 *
 *  \param	licence_t* pointer to licence type
 *
 *  \retval	BOOLEAN TRUE = valid match, FALSE = not-valid
 *
 */
static BOOLEAN ValidFingerprint(const licence_t *LicePtr)
{
	BOOLEAN matched = TRUE;
	int j;
	const int fingerprintSize = sizeof(LicePtr->body.ownerFingerprint);
	unsigned char mac[fingerprintSize];
	bzero(&mac, sizeof(mac));
	CmnGetMacAddr(mac);

	for(j = 0;j < fingerprintSize;j++)
	{
		if(LicePtr->body.ownerFingerprint[j] != mac[j])
		{
			matched = FALSE;
			break;
		}
	}

	return matched;
}

/*!
 *  \fn	ValidDate
 *
 *	\brief Check for a valid licence date
 *
 *  \param	licence_t* pointer to licence type
 *
 *  \retval	BOOLEAN TRUE = valid match, FALSE = not-valid
 *
 */
static BOOLEAN ValidDate(const licence_t *LicePtr)
{
	BOOLEAN matched = FALSE;
	time_t binTim = 0;
	time_t deltaTim = 0;

	if(LicePtr->body.type == TEMPORARY)
	{
		time(&binTim);
		deltaTim = binTim - LicePtr->body.dateofIssue;
		if(deltaTim < SECONDS_IN_A_DAY * 60)
		{
			matched = TRUE;
		}
	}
	else
	{
		// date is only relevant to temporary licences
		matched = TRUE;
	}

	return matched;
}



//!  \fn     MovaLibCreateLicence()
//!  \brief  Create a MOVA licence 
//!	\param	streams int number of streams (instances) of MOVA allowed to be configured
//!	\param	type lice_type_t type of licence, PERMANENT, UPGRADE, TEMPORARY
//!	\param	provider char* Optional name of licence provider
//!	\param	owner char* Optional name of licencee
//!  \retval  BOOLEAN TRUE = valid licence, FALSE = not-valid
//!
BOOLEAN MovaLibCreateLicence(const int streams, lice_type_t type, const char *provider, const char *owner, const unsigned char ownerFingerprint[6])
{
	BOOLEAN retStat = FALSE;
	time_t binTim;
	licence_t NewLicence;
	unsigned char mac[6];

	CMN_TRACE_ENTRY;

	bzero(&NewLicence, sizeof(NewLicence));
	time(&binTim);
	bzero(mac, sizeof(mac));
	CmnGetMacAddr(mac);

	//
	// datastructure version
	//
	NewLicence.body.dsVer = LICENCE_DSVER;

	//
	// supplier and owner
	//
	NewLicence.body.dateofIssue = binTim;
	if(provider == NULL)
	{
		strncpy(NewLicence.body.provider, LICENCE_STR_PROVIDER, LICENCE_STRLEN);
	}
	else
	{
		strncpy(NewLicence.body.provider, provider, LICENCE_STRLEN);
	}
	NewLicence.body.provider[LICENCE_STRLEN-1] = '\0';
	bcopy(mac, NewLicence.body.providerFingerprint, sizeof(mac));

	if(owner == NULL)
	{
		strncpy(NewLicence.body.owner, LICENCE_STR_OWNER, LICENCE_STRLEN);
	}
	else
	{
		strncpy(NewLicence.body.owner, owner, LICENCE_STRLEN);
	}
	NewLicence.body.owner[LICENCE_STRLEN-1] = '\0';

	if(ownerFingerprint != 0)
	{
		bcopy(ownerFingerprint, NewLicence.body.ownerFingerprint, sizeof(const unsigned char) * 6);
	}
	else
	{
		// record mac address
		bcopy(mac, NewLicence.body.ownerFingerprint, sizeof(mac));
	}

	//
	// product data
	//
	strncpy(NewLicence.body.product, LICENCE_MOVASTR_PRODUCT, LICENCE_STRLEN);
	NewLicence.body.product[LICENCE_STRLEN - 1] = '\0';
	NewLicence.body.majorVer = LICENCE_MOVA_MAJOR;
	NewLicence.body.minorVer = LICENCE_MOVA_MINOR;

	// upgrade/permanent
	NewLicence.body.type = type;

	if(streams > 0 && streams <= MAX_MOVA_IO_ENTRIES)
	{
		NewLicence.body.streams = streams;
		retStat = CmnSaveLicence(&NewLicence);
	}
	else
	{
		retStat = FALSE;
	}

	CMN_TRACE_EXIT;

	return retStat;
} // MovaLibCreateLicence()



//!  \fn     BuildFilePath()
//!  \brief  Converts a directory and a filename to a full path.
//!
//!	\param	psFullPath The destination where the path is copied to
//!	\param	psDirectory The directory where the file is located
//!	\param	psFilename The name of the file
//!
//!  \retval char* Pointer to the full path
//!
char *BuildFilePath(char *psFullPath, const char *psDirectory, const char *psFilename)
{
	(void)snprintf(psFullPath, MAX_PATHNAME_LEN, "%s/%s", psDirectory, psFilename);

	return psFullPath;
}



//!  \fn    MovalibGetVersion()
//!  \brief Returns the version string of MOVA
//!	\param	VersionString char* buffer to return the version string
//!	\param	VersionLen int The size of the Version string supplied
//!  \retval  BOOLEAN TRUE = version supplied , FALSE = not supplied
//!
BOOLEAN MovaLibGetVersion(char *VersionString, int VersionLen)
{
	BOOLEAN rtn;
	mova_io_t *movaIo;

	movaIo = &GIOshmData->mova_io[0]; // just copy from stream 1
	if(VersionString == NULL)
	{
		rtn = FALSE;
	}
	else
	{
		strncpy(VersionString, movaIo->MovaVer, min(CONST_LINE_LENGTH, VersionLen - 1));
		VersionString[VersionLen - 1] = '\0'; // terminate just in case
		if(strlen(VersionString) == 0)
		{
			// there is no version text in this string
			rtn = FALSE;
		}
		else
		{
			// success
			rtn = TRUE;
		}
	}

	return rtn;
} // MovaLibGetVersion()



//!  \fn     MovaLibGetStatus()
//!  \brief  gets mova status for current MOVA instance
//!
//!	\param	instance const int instance number of MOVA stream
//!  \retval  mova_status_t status of MOVA stream
//!
mova_status_t MovaLibGetStatus(const int instance)
{
	mova_io_t *movaIo;

	// CMN_TRACE_ENTRY // Waste of tracefile space

	movaIo = &GIOshmData->mova_io[instance];

	// CMN_TRACE_EXIT;

	return movaIo->currStatus;
}   // MovaLibGetStatus()



//!  \fn     MovaLibGetStatus()
//!  \brief  Returns a string contining the MOVA status
//!
//!	\param	instance const int instance number of MOVA stream
//!	\param	StatusString char* buffer to return the version string
//!	\param	StatusLen The size of the Status  string supplied
//!  \retval  BOOLEAN TRUE = status  supplied , FALSE = not supplied
//!
BOOLEAN MovaLibDisplayStatus(const int instance, char *StatusString, int StatusLen)
{
	BOOLEAN rtn;
	mova_status_t Mst;

	if(StatusString == NULL)
	{
		rtn = FALSE;
	}
	else
	{
		Mst = MovaLibGetStatus(instance);
		strncpy(StatusString, MovaStatusString[Mst], StatusLen - 1);
		StatusString[StatusLen - 1] = '\0'; // terminate just in case
		rtn = TRUE;
	}

	return rtn;
} // MovaLibDisplayStatus()



//!  \fn     MovaLibFormatRing()
//!  \brief  Format the MOVA ring buffer, used for communication between MovaLib and
//!		 MovaCradle.  This must be called before the Mova instance is started.
//!
//!	\param	instance const int instance number of MOVA stream
//!  \retval  BOOLEAN TRUE = format success, ALSE = not-valid 
//!
BOOLEAN MovaLibFormatRing(const int instance)
{
	BOOLEAN retStat = FALSE;

	CMN_TRACE_ENTRY;

	if(instance >= 0 && instance < MAX_MOVA_IO_ENTRIES && GIOshmData != NULL)
	{
		// params valid
		MovaLibFormatPipe(&GIOshmData->mova_io[instance].rcvPipe);
		MovaLibFormatPipe(&GIOshmData->mova_io[instance].xmtPipe);
		retStat = TRUE;
	} // valid params

	CMN_TRACE_EXIT;

	return retStat;
} // MovaLibFormatRing()

/*
 * --------------------------------------------------------------------------- *
 *                   INFRASTRUCTURE FUNCTIONS                                  *
 *                                                                             *
 * --------------------------------------------------------------------------- *
 */



//!  \fn     MovaLibFormatPipe()
//!  \brief  Format the MOVA ring buffer, used for communication between MovaLib and
//!		 MovaCradle.  This must be called before the Mova instance is started.
//!
//!	\param	movaIo mova_pipe_t* address of pipe
//!  \retval void none
//!
void MovaLibFormatPipe(mova_pipe_t *movaIo)
{
	int i;

	mova_ringbuf_t *rbp;

	movaIo->head = 0; // head of ring buffer
	movaIo->writer = 0; // current position of writer
	movaIo->reader = 0; // current position of reader
	movaIo->readerActive = MOVARB_LOCK_DEALLOC;
	movaIo->writerActive = MOVARB_LOCK_DEALLOC;

	for(i = 0;i < MOVA_RECORDED_MESSAGES;i++)
	{
		rbp = &movaIo->ring[i];
		rbp->hdr.hlink = 0;
		rbp->hdr.lock = MOVARB_LOCK_DEALLOC; // nothing is locking these records

		// check for end conditions
		if(i == 0)
		{
			// at head of ring
			rbp->hdr.flink = i + 1;
			rbp->hdr.blink = MOVA_RECORDED_MESSAGES - 1;
		}
		else if(i == MOVA_RECORDED_MESSAGES - 1)
		{
			// at tail of ring
			rbp->hdr.flink = 0;
			rbp->hdr.blink = i - 1;
		}
		else
		{
			// walking the ring
			rbp->hdr.flink = i + 1;
			rbp->hdr.blink = i - 1;
		}

		rbp->dataLen = 0; // zero data records
		bzero(rbp->data, sizeof(rbp->data));
	}
} // MovaLibFormatPipe()



//!  \fn     MovaLibReadPipeFrom()
//!  \brief  This routine reads from the ring buffer
//!	\note	Synchronization: This function sets the readerActive semaphore to serialise.
//!
//!	\param	movaPipe mova_pipe_t* string to send to the modem
//!	\param	buffer unsigned char* pointer to buffer to read into
//!	\param	bufferLen const int length of read buffer
//!	\param	returnBufferLen int* returned length of read
//!  \retval BOOLEAN TRUE => successful read, FALSE=>failure 
//!
BOOLEAN MovaLibReadPipeFrom(mova_pipe_t *movaPipe, unsigned char *buffer, const int bufferLen, int *returnBufferLen)
{
	mova_ringbuf_t *rbp;
	BOOLEAN status = FALSE;

	CMN_TRACE_ENTRY;

	if(movaPipe->readerActive == MOVARB_LOCK_DEALLOC)
	{
		// take out mutex
		movaPipe->readerActive = MOVARB_LOCK_ALLOC;

		// find current position in ring
		rbp = &movaPipe->ring[movaPipe->reader];

		// algorithm:
		// reads the next packet from the ring buffer and unlocks it

		status = readPacket(rbp, buffer, bufferLen, returnBufferLen);
		if(status != FALSE)
		{
			// unlock and walk ring
			movaPipe->reader = rbp->hdr.flink;
		}

		// return mutex
		movaPipe->readerActive = MOVARB_LOCK_DEALLOC;
	}
	else if(movaPipe->readerActive == MOVARB_LOCK_ALLOC)
	{
		// reader active. Trace error and quietly exit
		CmnTrace(GTraceFileId, "%s: MOVA:%s, reader mutex already locked. Will not read ring.", __FUNCTION__, gMovaEqId);
	}
	else
	{
		CmnTrace(GTraceFileId, "%s: MOVA:%s, reader mutex %x not valid. Will not read ring.", __FUNCTION__, gMovaEqId, movaPipe->readerActive);
	}

	CMN_TRACE_EXIT;

	return status;
}   // MovaLibReadPipeFrom()



//!  \fn     StatMemFile()
//!  \brief  checks file exists and created one of the correct size if it doesn't exist
//!
//!	\param	fname file pathname
//!	\param	len lengh of datastructure to be mapped
//!  \retval struct stat
//!
static struct stat StatMemFile(char const *fname, int len, BOOLEAN *replaced)
{
	struct stat sbuf;
	int status = 0;
	void *zero;
	char errbuff[MAX_MOVA_IO_DATASET_LEN + 1]; // AML TBC replace length definition
	int fd = 0;
	int rtn = 0;

	CMN_TRACE_ENTRY;

	bzero(&sbuf, sizeof(sbuf));
	status = stat(fname, &sbuf);
	if(status == -1)
	{
		if(errno != ENOENT)
		{
			strerror_r(errno, errbuff, sizeof(errbuff));
			DiagLog(DC_WARNING, "MOVA:%s, Failed stat(%s) Error: %s", gMovaEqId, fname, errbuff);
			unlink(fname);
			DiagLog(DC_ERROR, "restarting");
		}
		else
		{
			// no such file, so create it with the correct size
			fd = creat(fname, IOUT_GROUP_PERMISSIONS);
			if(fd == -1)
			{
				strerror_r(errno, errbuff, sizeof(errbuff));
				DiagLog(DC_WARNING, "MOVA:%s, Failed creat(%s) Error: %s", gMovaEqId, fname, errbuff);
				close(fd);
				unlink(fname);
				DiagLog(DC_ERROR, "restarting");
			}
			else
			{
				zero = malloc(len);
				if(zero == NULL)
				{
					DiagLog(DC_WARNING, "MOVA:%s, Failed malloc(%d)", gMovaEqId, len);
					close(fd);
					DiagLog(DC_ERROR, "restarting");
				}

				// clean datastructure
				bzero(zero, len);
				rtn = write(fd, zero, len);
				if(rtn != len)
				{
					strerror_r(errno, errbuff, sizeof(errbuff));
					DiagLog(DC_WARNING, "MOVA:%s, Failed write(%s) Error: %s", gMovaEqId, fname, errbuff);
					close(fd);
					unlink(fname);
					DiagLog(DC_ERROR, "restarting");
				}
				close(fd);
				free(zero);

				status = stat(fname, &sbuf);
				if(status == -1)
				{
					strerror_r(errno, errbuff, sizeof(errbuff));
					DiagLog(DC_WARNING, "MOVA:%s, Failed stat(%s) Error: %s", gMovaEqId, fname, errbuff);
					unlink(fname);
					DiagLog(DC_ERROR, "restarting");
				}

				if(replaced != NULL)
				{
					*replaced = TRUE;
				}
			}
		} // if no such file
	} // if stat() error

	CMN_TRACE_EXIT;

	return sbuf;
} // StatMemFile()



//!  \fn     CreateMemFile()
//!  \brief  creates / maps datastructure to file. File is left open
//!
//!	\param	fname file pathname
//!	\param	fd int* file descriptor of opened file.
//!	\param	len const int length of datastructure to be mapped
//!  \retval void* address of mapped memory
//!
static void *CreateMemFile(char const *fname, int *fd, int const len, BOOLEAN *repl)
{
	int shm_fd = 0;
	void *mmap_rtn;
	struct stat sbuf;
	char errbuff[MAX_MOVA_IO_DATASET_LEN + 1]; // AML TBC replace length definition

	CMN_TRACE_ENTRY;

	bzero(&sbuf, sizeof(sbuf));
	sbuf = StatMemFile(fname, len, repl);
	if(sbuf.st_size < len)
	{
		DiagLog(DC_WARNING, "MOVA:%s, %s too small, replacing it.", gMovaEqId, fname);
		unlink(fname);
		sbuf = StatMemFile(fname, len, repl);
	}

	shm_fd = open(fname, O_CREAT | O_RDWR, IOUT_GROUP_PERMISSIONS);
	if(shm_fd == -1)
	{
		strerror_r(errno, errbuff, sizeof(errbuff));
		DiagLog(DC_WARNING, "MOVA:%s, Failed to open file: %s, Error: %s", gMovaEqId, fname, errbuff);
		DiagLog(DC_ERROR, "restarting");
	}

	mmap_rtn = mmap(0, len, PROT_READ+PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if(mmap_rtn == MAP_FAILED)
	{
		DiagLog(DC_WARNING, "MOVA:%s, Failed to mmap file: %s", gMovaEqId, fname);
		DiagLog(DC_ERROR, "restarting");
	}
	else
	{
		// DiagLog(DC_NOTE, "MOVA:%s, mmap %s address: %x", gMovaEqId, fname, mmap_rtn);
	}
	*fd = shm_fd;

	CMN_TRACE_EXIT;

	return mmap_rtn;
} // CreateMemFile()



//!  \fn     CloseMemFile()
//!  \brief  Unmaps / Closes datastructure to file. File is closed
//!
//!	\param	ds void** pointer to datastructure to unmap
//!	\param	len int length of datastructure to be mapped
//!	\param	fd int* file descriptor of opened file.
//!  \retval void none
//!
static void CloseMemFile(void **ds, int dsLen, int *fd)
{
	int status = 0;
	char errbuff[MAX_MOVA_IO_DATASET_LEN + 1]; // AML TBC replace length definition

	CMN_TRACE_ENTRY;

	if(*ds != NULL)
	{
		status = munmap(*ds, dsLen);
		if(status == -1)
		{
			strerror_r(errno, errbuff, sizeof(errbuff));
			DiagLog(DC_WARNING, "MOVA: %s, Failed to munmap datastructure: %s", gMovaEqId, errbuff);
		}
		else
		{
			// success now close file
			*ds = NULL;
			if(*fd != -1)
			{
				if(close(*fd) != 0)
				{
					strerror_r(errno, errbuff, sizeof(errbuff));
					DiagLog(DC_WARNING, "MOVA: %s, Failed to close backing store: %s", gMovaEqId, errbuff);
				}
				else
				{
					// marked closed
					*fd = -1;
				}
			} // if not closed
		}
	}

	CMN_TRACE_EXIT;
} // CloseMemFile()



//!  \fn     readPacket()
//!  \brief  read a packet from the  MOVA ring buffer
//!	\note	Synchronization: Caller must take out the readerActive semaphore to serialise.
//!
//!	\param	rbp unsigned char* buffer to write into when reading data
//!	\param	buffer unsigned char* buffer to write into when reading data
//!	\param	bufferLen const int length of buffer
//!	\param	returnBufferLen int* length of packet read
//!  \retval BOOLEAN TRUE = success, FALSE = failure  
//!
inline static BOOLEAN  readPacket(mova_ringbuf_t *rbp, unsigned char *buffer, const int bufferLen, int *returnBufferLen)
{
	BOOLEAN status = FALSE;
	int minLen = 0;

	CMN_TRACE_ENTRY;

	if(rbp->hdr.lock == MOVARB_LOCK_ALLOC)
	{
		// read the packet and unlock it
		if(bufferLen < rbp->dataLen)
		{
			minLen = bufferLen -1;
		}
		else
		{
			minLen = rbp->dataLen;
		}

		memcpy(buffer, rbp->data, minLen);
		buffer[minLen] = '\0'; // zero terminate
		*returnBufferLen = minLen;
		rbp->hdr.lock = MOVARB_LOCK_DEALLOC;
		status = TRUE;
	}
	else if(rbp->hdr.lock == MOVARB_LOCK_DEALLOC)
	{
		// packet empty. Return.
		CmnTrace(GTraceFileId, "%s: MOVA:%s, packet not in use.", __FUNCTION__, gMovaEqId);
	}
	else
	{
		// packet empty. Return.
		CmnTrace(GTraceFileId, "%s: MOVA:%s, packet lock %x not valid.", __FUNCTION__, gMovaEqId, rbp->hdr.lock);
	}

	CMN_TRACE_EXIT;

	return status;
} // readPacket()



//!  \fn     MovaLibWritePipe()
//!  \brief  This routine writes to  the ring buffer associated with a MOVA stream
//!	\note	Synchronization: This function sets the readerActive semaphore to serialise.
//!
//!	\param	instance const int The instance number of the MOVA stream
//!	\param	buffer unsigned char* pointer to buffer to read into
//!	\param	bufferLen const int length of read buffer
//!	\param	returnBufferLen int* returned length of read
//!  \retval BOOLEAN TRUE => successful read, FALSE=>failure 
//!
BOOLEAN MovaLibWritePipe(const int instance, unsigned char *buffer, const int bufferLen)
{
	BOOLEAN status = FALSE;

	// CMN_TRACE_ENTRY;

	status = MovaLibWritePipeTo(&GIOshmData->mova_io[instance].rcvPipe, buffer, bufferLen);

	// CMN_TRACE_EXIT;

	return status;
}   // MovaLibWritePipe()



//!  \fn     MovaLibWritePipeTo()
//!  \brief  This routine writes to  the ring buffer associated with a MOVA stream
//!	\note	Synchronization: This function sets the readerActive semaphore to serialise.
//!
//!	\param	movaIo mova_pipe_t* MOVA pipe
//!	\param	buffer unsigned char* pointer to buffer to read into
//!	\param	bufferLen const int length of read buffer
//!  \retval BOOLEAN TRUE => successful read, FALSE=>failure 
//!
BOOLEAN MovaLibWritePipeTo(mova_pipe_t *movaIo, unsigned char *buffer, int bufferLen)
{
	unsigned char *section_start;
	mova_ringbuf_t *rbp;
	BOOLEAN status = TRUE;

	CmnTrace(GTraceFileId, "%s: MOVA:%s, ENTRY String len %d", __FUNCTION__, gMovaEqId, bufferLen);

	//LockRingbuf();

	if(movaIo->writerActive == MOVARB_LOCK_DEALLOC)
	{
		// take out mutex
		movaIo->writerActive = MOVARB_LOCK_ALLOC;

		// find current position in ring
		rbp = &movaIo->ring[movaIo->writer];

		// algorithm:
		// writer can continue to write each packet until it reaches
		// the lock. At that point it will drop packets.

		for(section_start = buffer;bufferLen > xgnDBZ_MESSAGE_STR_LEN;section_start = section_start + xgnDBZ_MESSAGE_STR_LEN)
		{
			status = writePacketSections(rbp, section_start, xgnDBZ_MESSAGE_STR_LEN);
			if(status != FALSE)
			{
				// walk ring and
				// write back where we are
				movaIo->writer = rbp->hdr.flink;
				rbp = &movaIo->ring[movaIo->writer];
				bufferLen = bufferLen - xgnDBZ_MESSAGE_STR_LEN;
			}
			else
			{
				break;
			}
		}

		// check why we left the loop
		if(status != FALSE && bufferLen > 0)
		{
			// write the last section
			status = writePacketSections(rbp, section_start, bufferLen);
			if(status != FALSE)
			{
				// walk ring and
				// write back where we are
				movaIo->writer = rbp->hdr.flink;
				rbp = &movaIo->ring[movaIo->writer];
			}
		}
		// return mutex
		movaIo->writerActive = MOVARB_LOCK_DEALLOC;
	}
	else if(movaIo->writerActive == MOVARB_LOCK_ALLOC)
	{
		// writer active. Trace error and quietly exit
		CmnTrace(GTraceFileId, "%s: MOVA:%s, writer mutex already locked. Dropping packet.", __FUNCTION__, gMovaEqId);
	}
	else
	{
		CmnTrace(GTraceFileId, "%s: MOVA:%s, writer mutex %x not valid. Dropping packet.", __FUNCTION__, gMovaEqId, movaIo->writerActive);
	}

	//UnlockRingbuf();

	CMN_TRACE_EXIT;

	return status;
} // MovaLibWritePipeTo()



//!  \fn     writePacketSections()
//!  \brief  Write a string to a the MOVA ring buffer
//!	\note	Synchronization: Caller must call LockRingbuf() and take out the writerActive
//! 		semaphore to serialise. 
//!
//!	\param	rbp const mova_ringbuf_t* pointer to ring buffer packet
//!	\param	section_start unsigned char* string to send to the modem
//!	\param	section_len const int length of section
//!  \retval BOOLEAN TRUE => successful read, FALSE=>failure 
//!
static BOOLEAN  writePacketSections(mova_ringbuf_t *rbp, unsigned char * section_start, int section_len)
{
	BOOLEAN status = FALSE;

	CmnTrace(GTraceFileId, "%s: MOVA:%s, ENTRY len %d", __FUNCTION__, gMovaEqId, section_len);

	if(rbp->hdr.lock == MOVARB_LOCK_DEALLOC)
	{
		// write new packet
		rbp->hdr.lock = MOVARB_LOCK_ALLOC;
		if(section_len > xgnDBZ_MESSAGE_STR_LEN)
		{
			CmnTrace(GTraceFileId, "MOVA:%s, writePacketSection() Truncating write, section_len=", __FUNCTION__, gMovaEqId, section_len);
			section_len = xgnDBZ_MESSAGE_STR_LEN;
		}

#ifdef ERASE_BUFFER
		bzero(rbp->data, sizeof(rbp->data));
#endif  //

		memcpy(rbp->data, section_start, section_len);
		rbp->dataLen = section_len;
		status = TRUE;
	}
	else if(rbp->hdr.lock == MOVARB_LOCK_ALLOC)
	{
		// packet in use, probably because it has not been emptied by
		// the reader.
		// Trace this error and drop the packet.
		CmnTrace(GTraceFileId, "%s: MOVA:%s, buffer lock ALLOC. Dropping packet.", __FUNCTION__, gMovaEqId);
	}
	else
	{
		// packet in use, probably because it has not been emptied by
		// the reader.
		// Trace this error and drop the packet.
		CmnTrace(GTraceFileId, "%s: MOVA:%s, buffer lock type %x not valid. Dropping packet.", __FUNCTION__, gMovaEqId, rbp->hdr.lock);
	}

	CMN_TRACE_EXIT;

	return status;
} // writePacketSections()



//!  \fn     WaitForMovaCradleReply()
//!  \brief  Write a string to a the MOVA ring buffer
//!
//!	\param	movaLibSocket socket descriptor
//!	\param	req unsigned const msg_mova_request_t* pointer to request datastructure
//!	\param	MyMovaLibSocket int* 
//!  \retval BOOLEAN TRUE = success, FALSE = failure 
//!
static BOOLEAN WaitForMovaCradleReply(const int instance, const int movaLibSocket, const msg_mova_request_t *req, int *MyMovaLibSocket)
{
	BOOLEAN completed = FALSE;
	BOOLEAN rtn = FALSE;
	int Secs = 5; // maximum number of seconds that we should wait for reply
	MESSAGE_ID MessageId = NO_MESSAGE;
	MSG *Msg = NULL;
	msg_mova_request_t *MovaReply;
	msg_term_string_t *TermStringMsg;
	struct sockaddr From;
	struct sockaddr To;
	static const MODULE ModuleMovaLib = MOD_MOVALIB;
	static const MODULE ModuleMovaMsg = MOD_MOVA;
	equip_id_text_t movaEqId;
	equip_id_t movaEqIdNum;
	int repeatCount = 0;

	movaEqIdNum = MovaLibInstanceToStream(instance, movaEqId, 0, 0, 0);
	if(movaEqIdNum >= 0)
	{
		do
		{
			MessageId = SktReadMessage("WaitForMovaCradleReply:", *MyMovaLibSocket, &Msg, &From, Secs);
			if(MessageId != NO_MESSAGE)
			{
				switch(MessageId)
				{
					case MOVA_REQUEST_ID:
						MovaReply = (msg_mova_request_t *)&(Msg->Body);
						CmnTrace(GTraceFileId, "%s: Received MOVA %s mova-request sequence %d", __FUNCTION__, movaEqId, MovaReply->seq);
						if(MovaReply->type == MM_REPLY && MovaReply->seq == req->seq)
						{
							if(MovaReply->status == MM_SUCCESS)
							{
								rtn = TRUE;
							}
						}
						completed = TRUE;
						break;

					case TERM_STRING_ID:
						CmnTrace(GTraceFileId, "%s: Ignoring MOVA %s term-string", __FUNCTION__, movaEqId);
						// fallthrough

					default:
						repeatCount++;
				}
			} // if message received
			else
			{
				repeatCount++;
			}
			SktFreeMessage(&Msg);
		} while(completed == FALSE && repeatCount < 2);
	} // if instance

	CMN_TRACE_EXIT;

	return rtn;
} // WaitForMovaCradleReply()
