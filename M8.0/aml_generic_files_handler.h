/*******************************************************************************
*
*                      MOVA KERNEL
*
*  FILE:         aml_generic_files_handler.h
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

#if !defined (AML_GENERIC_FILES_HANDLER_h)
#define AML_GENERIC_FILES_HANDLER_H

#include "MVTypes.h"

MVStatus		aml_init_tma_logs_file(MVBool is_init_to_web_downloading);
void			aml_write_tma_log_data(char * log_data);
void			aml_get_tma_log_file_uri(char * tma_file_link);
void			aml_close_tma_log_file();
MVStatus		aml_file_read_and_send(char * file_path_and_name, MVInt file_size);
void			aml_get_tma_log_file_path_and_name(char * file_path_and_name);
MVInt			aml_get_tma_file_size();

#endif /*AML_GENERIC_FILES_HANDLER_H*/