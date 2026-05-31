/*******************************************************************************
*
*                      MOVA KERNEL
*
*  FILE:         aml_handler.h
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

#if !defined (AML_HANDLER_H)
#define AML_HANDLER_H

#include "base_types.h"
#include "pctypes.h"

#define IS_MOVACOMM_ENABLED (0)



void aml_handler(UNSIGNED argc, void * argv);
void aml_send_message_to_user(char * msg);

MVStatus aml_enable_dataset_receiving(char * file_name, int file_size);
void aml_disable_dataset_receiving();

#endif /*AML_HANDLER_H*/