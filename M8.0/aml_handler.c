/*******************************************************************************
*
*                      MOVA KERNEL
*
*  FILE:         aml_handler.c
*
*  TITLE:        Mova Kernel: AML
*
*
*  (c) Copyright TRL Ltd 2016. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/

#include <string.h>
#include <stdlib.h>


#include "nucleus.h" /* MOVA Kernel interface to PCTasks */
#include "pcuserio.h"
#include "pctasks.h"

#include "aml_handler.h"
#include "aml_messages_decoder_encoder.h"
#include "aml_dataset_file_handler.h"


#define AML_MSG_MAX_SIZE 1024

extern char GetMOVACharFromHandset(void);
extern MVStatus GetMsgFromMT_ReadBytes(MVByte * p_bytes, MVInt count);
extern void SendStringToLH( const char *sOP_String, int nCharsInString);

static void aml_handle_comm_errors();

static char				msg_buffer[AML_MSG_MAX_SIZE];
static  int				msg_i = 0;
static MVBool 			is_in_msg_size = MV_FALSE;

static MVBool			is_receiving_dataset = MV_FALSE;


void aml_handler(UNSIGNED argc, void * argv)
{
	UNUSED(argc);
	UNUSED(argv);

	char c;
	int msg_size;


	while(!IS_MOVACOMM_ENABLED)
	{

		if(PCUserIO_IsConnected() == MV_TRUE)
		{
			c = GetMOVACharFromHandset();

			/*Ignoring any empty chars*/
			if (c == 0x00)
			{
#if defined (PC_WRAPPER)
				PCTasks_Sleep(1, MOVA_TASK_TERM);
#else
				/*SIGNAL_COMPANY: The following Sleep needs to be adjusted in the view of M8 comm approach.*/
				NU_Sleep(1);
#endif
				continue;
			}
			else if(c == 0x02) //Start of the message (STX) - To read the size field
			{
				msg_i = 0;
				is_in_msg_size = MV_TRUE;
			}

			else if(c == 0x03) //End of the message (ETX) - End of reading the size field
			{				
				is_in_msg_size = MV_FALSE;

				/*Remove the STX and ETX*/
				msg_buffer[0] = 48;  /*The ASCII of '0'*/
				msg_buffer[msg_i] = 0x00;

				/*msg_buffer now holds the size only, and it should never exceed 9 digits (as def in MT3).*/
				if (strlen(msg_buffer) > 9)
				{
					aml_handle_comm_errors();
					continue;
				}

				msg_size = atoi(msg_buffer);

				/*First, check that the message size is ok*/
				if (msg_size > AML_MSG_MAX_SIZE-1 || msg_size <= 0)
				{
					aml_handle_comm_errors();
					continue;
				}

				if (GetMsgFromMT_ReadBytes((MVByte *)msg_buffer, msg_size) == MV_FAILURE)
					continue;

				msg_buffer[msg_size] = 0x00;

				if (aml_decode_message_json(msg_buffer) == MV_FAILURE)
				{
					aml_handle_comm_errors();
				}
			}
			else if (is_receiving_dataset == MV_TRUE) 			/*When AML receives the dataset (XML) file*/
			{
				aml_dataset_file_write(c);

				/*Sends the message "DatasetFileReceived" after writing the file locally. The message must be sent in all
				cases, and then the next message (ReqCheckTransferedFileIntegrity) will do the file checking.*/
				aml_encode_message_json("DatasetFileReceived", 0);
				continue;
			}

			if(is_in_msg_size == MV_TRUE && msg_i < sizeof(msg_buffer))
			{
				msg_buffer[msg_i] = c; 
				msg_i++;
			}
		}
		else
		{
#if defined (PC_WRAPPER)
			PCTasks_Sleep(50, MOVA_TASK_AML);
#else
			NU_Sleep(50);
#endif
		}

		/*Exit the loop if the threads are terminated.*/
		if (PCTasks_IsTerminating())
		{
			PCUserIO_Disconnect();
			break;
		}

	} /*While loop*/
}

void aml_send_message_to_user(char * msg)
{
	if(PCUserIO_IsConnected() == MV_TRUE)
	{
		SendStringToLH(msg, strlen(msg));
	}
}

MVStatus aml_enable_dataset_receiving(char * file_name, int file_size)
{
	is_receiving_dataset = MV_TRUE;

	return aml_dataset_file_init(file_name, file_size);
}

void aml_disable_dataset_receiving()
{
	is_receiving_dataset = MV_FALSE;
}

static void aml_handle_comm_errors()
{
	aml_encode_message_json("FailedToHandleReqMessage", 0);
	PCUserIO_Disconnect();
}