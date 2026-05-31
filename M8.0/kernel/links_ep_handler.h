/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         links_ep_handler.h
*
*  TITLE:        Mova Kernel: Links EP Handler
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

#if !defined (LINKS_EP_HANDLER_H)
#define LINKS_EP_HANDLER_H

#include "mova_types.h"
#include "dynamic_data.h"

void set_links_pointer_for_ep_handler(struct Links * linksRef);

void update_ep_links_data();
void update_ep_link_demand(uint8 linkIdx);
void update_ep_link_ext_timer(uint8 linkIdx);
void update_links_ep_indicators();
void reset_ep_links_cancel_marker();
void reset_links_ep_indicators();
void update_links_ep_trunc_indicator();
mv_bool is_link_ep_hold_time_expired(uint8 linkIdx, int32 currentGreenTime);
void process_NTT_code_for_ep_links();

int32 get_ep_link_demand_delay(uint8 linkIdx);

#ifdef M8_IMPORVED_BUS_PRIORITY
uint8 get_demanded_priority_stage_num_by_a_link(uint8 linkIdx);
void  set_demanded_priority_stage_num_by_a_link(uint8 linkIdx, uint8 stageNum);
void reset_demanded_priority_stage_num(uint8 stageNum);
#endif

#endif /*LINKS_EP_HANDLER_H*/