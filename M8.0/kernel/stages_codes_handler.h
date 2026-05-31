/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         stages_codes_handler.h
*
*  TITLE:        Mova Kernel: Stages Codes Handler
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	24-Oct-2013		7.5.0		AA		IA		......			Newly created
*
*  (c) Copyright TRL Ltd 2013. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/

#if !defined (STAGES_CODES_HANDLER_H)
#define STAGES_CODES_HANDLER_H

void set_stages_pointer_for_codes_handler(struct Stages * stagesRef);

int8 process_sdcode(uint8 stageNo, uint8 linkNo);
int8 process_priority_facility_code(uint8 currentStageIdx, uint8 priorityStageIdx);

#endif /*STAGES_CODES_HANDLER_H*/