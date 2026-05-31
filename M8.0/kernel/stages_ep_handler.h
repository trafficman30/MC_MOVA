/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         stages_ep_handler.h
*
*  TITLE:        Mova Kernel: Stages EP Handler
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

#if !defined (STAGES_EP_HANDLER_H)
#define STAGES_EP_HANDLER_H

#include "mova_types.h"
#include "dynamic_data.h"

void set_stages_pointer_for_ep_handler(struct Stages * stagesRef);

void clear_ep_stage_demand(uint8 linkIdx);
void reset_ep_stage_ext_timers();

uint8 get_emergency_next_stage();
uint8 get_priority_next_stage();

#endif /*STAGES_EP_HANDLER_H*/