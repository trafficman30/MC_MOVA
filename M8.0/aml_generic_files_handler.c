/*******************************************************************************
*
*                      MOVA KERNEL
*
*  FILE:         aml_generic_files_handler.c
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

#include <stdio.h>
#include <string.h>

#include "aml_generic_files_handler.h"
#include "generalfunc.h"

extern void SendStringToLH(const char *sOP_String, int nCharsInString);


static FILE *	file_ptr;
static char		tma_logs_file_name[] = "tma_logs.csv";
static MVInt	tma_file_size;

char			tma_log_file_location_and_name[256];

MVStatus aml_init_tma_logs_file(MVBool is_init_to_web_downloading)
{

#if  PCMOVA

	if (is_init_to_web_downloading == MV_FALSE)
	{
		if (GetWindowsTempFolder(tma_log_file_location_and_name) == MV_FAILURE)
			return MV_FAILURE;
	}
	else
	{
		/* The web root of MOVA Tools web directory*/
		sprintf(tma_log_file_location_and_name, "C:\\Data\\MOVA_WebUI\\");
	}
#else
	/* SIGNAL_COMPANY: Signal companies need to define here the location of TMA log when it is prepared either to be read by 
						MOVA Tools (first branch) or to be downloaded through the Web UI (second branch).*/
#endif
		
	strcat(tma_log_file_location_and_name, tma_logs_file_name);
	
	/*First we need to create the TMA logs file for writing.*/
	file_ptr = fopen(tma_log_file_location_and_name, "w");
		
	if (file_ptr == 0x00)
		return MV_FAILURE;

	tma_file_size = 0;

	return MV_SUCCESS;
}

void aml_write_tma_log_data(char * log_data)
{
	tma_file_size += (MVInt)fwrite(log_data, 1, strlen(log_data), file_ptr);
}

void aml_get_tma_log_file_uri(char * tma_file_link)
{
#ifdef  PCMOVA
	sprintf(tma_file_link, tma_logs_file_name);
#else
	/* SIGNAL_COMPANY: This segment should handle the situation if the TMA log file is saved NOT in the web root directory (in a sub folder inside it).*/
#endif
}

void aml_get_tma_log_file_path_and_name(char * file_path_and_name)
{
	Safe_Strncpy(file_path_and_name, tma_log_file_location_and_name, strlen(tma_log_file_location_and_name)+1);
}


void aml_close_tma_log_file()
{
	fclose(file_ptr);
}

MVInt aml_get_tma_file_size()
{
	return tma_file_size;
}

MVStatus aml_file_read_and_send(char * file_path_and_name, MVInt file_size_in)
{
	MVStatus ret = MV_SUCCESS;
	char * file_to_send_buffer;

	

#ifdef SEND_MULTI_PACKETS
	MVInt i;
	MVInt num_of_packets = (int)ceil((file_size_in / (float)PACKET_SIZE));
	MVInt current_packet_length;
	MVInt total_length = file_size_in - 1;


	file_ptr = fopen(file_path_and_name, "r");

	if (file_ptr == 0x00)
		return MV_FAILURE;

	/*Loop over all the packets and write them into the XML file.*/
	for (i = 0; i < num_of_packets; i++)
	{
		if (total_length> PACKET_SIZE)
		{
			current_packet_length = PACKET_SIZE;
			total_length = total_length - current_packet_length;
		}
		else
		{
			current_packet_length = total_length;
		}

		//if (fgets(buffer, current_packet_length, file_ptr) != 0x00)
		if (fread(buffer, 1, current_packet_length, file_ptr) == current_packet_length)
		{
			SendStringToLH(buffer, current_packet_length);
		}
		else
		{
			return MV_FAILURE;
		}
	}

	fclose(file_ptr);

	return MV_SUCCESS;
#else

	/*SIGNAL_COMPANY: Note the dynamic memory allocation for the dataset file buffer here.*/

	/*1. Allocate memory for the file to send*/
	file_to_send_buffer = (char*)MV_MALLOC(file_size_in);

	if (file_to_send_buffer == 0x00)
		return MV_FAILURE;

	/*2. Open the file to read*/
	file_ptr = fopen(file_path_and_name, "r");

	if (file_ptr != 0x00)
	{
		/*3. Read the file into memory and send it.*/
		if (fread(file_to_send_buffer, 1, file_size_in, file_ptr) == (size_t)file_size_in)
		{
			SendStringToLH(file_to_send_buffer, file_size_in);
		}
		else
		{
			ret = MV_FAILURE;
		}
	}
	else
	{
		ret = MV_FAILURE;
	}


	/*Clean up*/
	MV_FREE((char*)file_to_send_buffer);

	if (file_ptr != 0x00)
		fclose(file_ptr);

	return ret;

#endif
}