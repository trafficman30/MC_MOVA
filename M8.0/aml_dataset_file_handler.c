/*******************************************************************************
*
*                      MOVA KERNEL
*
*  FILE:         aml_dataset_file_handler.c
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
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef PCMOVA
#include <winsock2.h>
#endif

#include "aml_dataset_file_handler.h"
#include "aml_handler.h"
#include "aml_generic_files_handler.h"

#include "generalfunc.h" /*Safe_Strncpy and GetWidnowsTempFolder*/
#include "CommsErr.h"
#include "ui_dataaccess.h"


#define PACKET_SIZE (64)

extern MVStatus GetMsgFromMT_ReadBytes(MVByte * p_bytes, MVInt count);
extern void SendStringToLH(const char *sOP_String, int nCharsInString);

static MVBool	is_dataset_file_initialised = MV_FALSE;
static int		dataset_file_size;
static char		dataset_file_path_and_name[FILENAME_MAX];

static FILE * file_ptr;

static unsigned int calc_crc32(unsigned char * file_data, long size);

/*Sets to TRUE if the dataset that will be sent to the controller is compatible with the active one*/
static MVBool	is_dataset_compatible = MV_TRUE;

MVBool aml_dataset_is_compatible()
{
	return is_dataset_compatible;
}

void aml_dataset_set_compatibility(MVBool var)
{
	is_dataset_compatible = var;
}

void aml_dataset_get_file_name(char * file_name)
{
	if (is_dataset_file_initialised)
	{
		sprintf(file_name, "%s", dataset_file_path_and_name);
	}
	else
	{
		sprintf(file_name, "%s", "");
	}
}

void aml_dataset_set_file_name(char * ds_file_path_and_name)
{
	sprintf(dataset_file_path_and_name, "%s", ds_file_path_and_name);
}

MVStatus aml_dataset_file_init(char * file_name, MVInt32 file_size)
{
	char dataset_location[256];

#if  PCMOVA
	
	if (GetWindowsTempFolder(dataset_location) == MV_FAILURE)
		return MV_FAILURE;

	sprintf(dataset_file_path_and_name, "%sTr_%s", dataset_location, file_name);

#else
	/* SIGNAL_COMPANY: Signal companies need to define here the location of the dataset file when transferred to the controller.*/
	/* SIGNAL_COMPANY: Signal companies need to adjust here also the dataset file location based on OS.*/
	UNUSED(dataset_location);
#endif
	
	file_ptr = fopen(dataset_file_path_and_name, "w");

	if (file_ptr == 0x00)
		return MV_FAILURE;
		
	is_dataset_file_initialised = MV_TRUE;
	dataset_file_size = file_size;

	return MV_SUCCESS;
}


MVStatus aml_dataset_file_write(char first_char)
{
	char buffer[PACKET_SIZE];
	int num_of_packets = (int)ceil((dataset_file_size / (float)PACKET_SIZE));
	int current_packet_length;
	int total_length = dataset_file_size-1;
	int i;

	if (is_dataset_file_initialised == MV_FALSE)
		return MV_FAILURE;

	if (file_ptr == 0x00)
		return MV_FAILURE;
	
	/* Write the already ready first char*/
	fputc(first_char, file_ptr);

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

		if (GetMsgFromMT_ReadBytes((MVByte *)buffer, (MVInt)current_packet_length) == MV_SUCCESS)
		{			
			fwrite(buffer, 1, current_packet_length, file_ptr);
		}
		else
		{
			fclose(file_ptr);
			aml_disable_dataset_receiving();

			return MV_FAILURE;
		}
	}

	fclose(file_ptr);
	aml_disable_dataset_receiving();

	return MV_SUCCESS;
}


MVBool aml_dataset_file_check_integrity(MVUInt32 file_crc32_in)
{
	FILE *fileptr;
	char *buffer;
	long file_length = 0;
	MVUInt32 calculated_crc32;

	
	if (strlen(dataset_file_path_and_name) == 0)
	{
		UI_GetLatestDatasetFileInfo_XDS(dataset_file_path_and_name, (MVInt *)&file_length);

		if (strlen(dataset_file_path_and_name) == 0)
			return MV_FALSE;
	}		

	/*Open the dataset file and calculate its size*/
	fileptr = fopen(dataset_file_path_and_name, "r");

	if (fileptr == NULL)
		return MV_FALSE;

	if (fseek(fileptr, 0, SEEK_END) != 0)
	{
		fclose(fileptr);
		return MV_FALSE;
	}

	file_length = ftell(fileptr);             
	rewind(fileptr);

	/*SIGNAL_COMPANY: Note the dynamic memory allocation for the dataset file buffer here.*/

	/* Enough memory for file + 0x00*/
	buffer = (char *)MV_MALLOC((file_length + 1) * sizeof(char));

	if (buffer == NULL)
	{
		fclose(fileptr);
		return MV_FALSE;
	}

	buffer[file_length] = 0x00;

	fread(buffer, file_length, 1, fileptr);
	fclose(fileptr);

	calculated_crc32 = calc_crc32((unsigned char *)buffer, file_length);

	UI_SetDatasetFileCRC32_XDS(calculated_crc32);

	MV_FREE(buffer);

	if (calculated_crc32 == file_crc32_in)
		return MV_TRUE;
	else
		return MV_FALSE;
}


static unsigned int calc_crc32(unsigned char * file_data, long size)
{
	int i, j;
	MVUInt32 byte, crc, mask;

	crc = 0xFFFFFFFF;

	for (i = 0; i<size; i++)
	{
		byte = file_data[i];

		/*ignore considering the CR or LF*/
		if (byte == 10 || byte == 13)
			continue;

		crc = crc ^ byte;
		for (j = 7; j >= 0; j--)
		{
			mask = -1 * (crc & 1);
			crc = (crc >> 1) ^ (0xEDB88320 & mask);
		}
	}

	return ~crc;
}