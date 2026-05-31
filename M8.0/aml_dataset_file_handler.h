/*******************************************************************************
*
*                      MOVA KERNEL
*
*  FILE:         aml_dataset_file_handler.h
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

#if !defined (AML_DATASET_FILE_HANDLER_H)
#define AML_DATASET_FILE_HANDLER_H

#include "MVTypes.h"


MVStatus aml_dataset_file_init(char * file_name, MVInt32 file_size);
MVStatus aml_dataset_file_write(char first_char);
MVBool aml_dataset_file_check_integrity(MVUInt32 file_crc32);
void aml_dataset_get_file_name(char * file_name);
void aml_dataset_set_file_name(char * ds_file_path_and_name);
MVBool aml_dataset_is_compatible();
void aml_dataset_set_compatibility(MVBool var);

#endif /*AML_DATASET_FILE_HANDLER_H*/
