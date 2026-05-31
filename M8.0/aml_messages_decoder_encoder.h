/*******************************************************************************
*
*                      MOVA KERNEL
*
*  FILE:         aml_messages_decoder_encoder_handler.h
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

#if !defined (AML_MESSAGES_DECODER_HANDLER_H)
#define AML_MESSAGES_DECODER_HANDLER_H

#include "MVTypes.h"
#include <stdarg.h>

MVStatus aml_decode_message_json(char * decoded_message);
MVStatus aml_encode_message_json(char * msg_type, int param_count, ...);

#endif /*AML_MESSAGES_DECODER_HANDLER_H*/
