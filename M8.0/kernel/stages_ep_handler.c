/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         stages_ep_handler.c
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

#include "stages_ep_handler.h"
#include "dynamic_data.h"
#include "dataset_interface.h"
#include "links_handler.h"
#include "links_ep_handler.h"
#include "junction_handler.h"

#include "timers_manager.h"
#include "stages_codes_handler.h"

static struct Stages * p_stages;

void set_stages_pointer_for_ep_handler(struct Stages * stagesRef)
{
	p_stages = stagesRef;
}



/*!
*	\brief	Clears all the priority or emergency demand markers for a specific green link.
*
*	\param[in]	linkIdx		The link index.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void clear_ep_stage_demand(uint8 linkIdx)
{
	uint8 stage_num;
	uint8 link_num = linkIdx+1;

	/* Ideally ought to check SDCODES to find which stage was demanded, 
	but the quick solution is to just go through the stages in order 
	from CUSIG+1 and if this e/p link is green in that stage, 
	clear any e/p demand for the stage. */

	/* Find the next stage in which the e/p link gets a full green */
	stage_num = get_current_stage();

	for (;;)
	{
		stage_num++;
		
		if (stage_num > DI_get_stages_count())
			stage_num -= DI_get_stages_count();

		if (stage_num == get_current_stage())
			break;

		if (!is_link_effectively_red(stage_num-1, linkIdx))
		{
			/* if link is green in 'stage_num' and there was an e/p	demand set, clear it */

			if (DI_is_emeregency_link(link_num) && p_stages->emergency_demand_marker[stage_num-1] == EMERGENCY_DEMAND_LATCHED)
			{
				p_stages->emergency_demand_marker[stage_num-1] = EMERGENCY_DEMAND_NONE;
				break;
			}
			else if (DI_is_priority_link(link_num) && p_stages->priority_demand_marker[stage_num-1] == PRIORITY_DEMAND_LATCHED)
			{
				p_stages->priority_demand_marker[stage_num-1] = PRIORITY_DEMAND_NONE;

#ifdef M8_IMPORVED_BUS_PRIORITY
				reset_demanded_priority_stage_num(stage_num);
#endif
				break;
			}
		}
	}
}



/*!
*	\brief	Resets emergency/priority stage-ext from largest of link-ext
*			timers on links now green and going red/opposed-green next stage.
*			It also sets reversionary demands in case emergency action/max's cut ext's
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void reset_ep_stage_ext_timers()
{
	uint8 link_num;
	uint8 link_idx;

	timers.emergency_stage_ext_timer = 0;
	timers.priority_stage_ext_timer = 0;

	for (link_num=1; link_num<=DI_get_links_count(); link_num++)
	{
		link_idx = link_num-1;

		/* Skip non-emergency/priority links, or links now red */
		if (!DI_is_ep_link(link_num) || get_link_green_flag(link_idx) == LINK_FLAG_NOT_GREEN)
			continue;

		/* Skip emerg/priority links now green & staying green next stage */
		if (!is_link_effectively_red( get_next_stage()-1, link_idx))
			continue;

#ifndef M8_REMOVE_REV_DEMAND
		/* Clear temp revers demand set by previous emerg/priority ext */
		if (get_link_rev_demand_marker(link_idx) < REV_DEMAND_NONE)
			set_rev_demand_marker(link_idx, REV_DEMAND_NONE);

		/* Set e/p reversionary demand, unless DO 810 loop set=1,4,5 */
		if (get_link_rev_demand_marker(link_idx) == REV_DEMAND_NONE)
			set_rev_demand_marker(link_idx, (int8)DI_get_link_type(link_num));
#endif
		
		/* Skip links with no emergency/priority extension */
		if (timers.ep_link_ext_timer[link_idx] == 0)
			continue;

		if (DI_is_priority_link(link_num))
		{
			/* Reset priority-stage ext'n timer if rel. link ext is longer */
			if (timers.ep_link_ext_timer[link_idx] > timers.priority_stage_ext_timer)
				timers.priority_stage_ext_timer = timers.ep_link_ext_timer[link_idx];
		}
		else
		{
			/* Reset emergency stage ext'n timer if link ext is longer */
			if (timers.ep_link_ext_timer[link_idx] > timers.emergency_stage_ext_timer)
				 timers.emergency_stage_ext_timer = timers.ep_link_ext_timer[link_idx];
		}
	}
}





/*----------------------------------------------------------------*/
/*------------------------- Getters ------------------------------*/
/*----------------------------------------------------------------*/
uint8 get_emergency_next_stage()
{
	uint8 k;
	uint8 i;

	for (i=1; i<DI_get_stages_count(); i++)
	{
		k = get_current_stage()+ i;

		if (k > DI_get_stages_count())
			k -= DI_get_stages_count();

		if (p_stages->emergency_demand_marker[k-1] == EMERGENCY_DEMAND_NONE)
			continue;

		while (k != get_current_stage())
		{
			if (DI_get_stage_change_allowance(get_current_stage(), k) != -1)
			{
				/* if stage demanded and direct change is allowed then implement */
				return k;
			}

			/* if can't change directly to stage, find the closest intermediate */
			k--;
			if (k <= 0)
				k = DI_get_stages_count();
		}

		/* If the while loop completes there are no allowed moves between
		   CUSIG and ESTDEM. Highly unlikely so just ignore the demand */
		continue;
	}

	/* No valid emergency stage demand found */
	return 0;
}

uint8 get_priority_next_stage()
{
	uint8 priority_stage_num;
	uint8 i;

	for (i=1; i<DI_get_stages_count(); i++)
	{
		priority_stage_num = get_current_stage()+i;

		if (priority_stage_num > DI_get_stages_count())
			priority_stage_num -= DI_get_stages_count();

		if (p_stages->priority_demand_marker[priority_stage_num-1] == PRIORITY_DEMAND_NONE)
			continue;

		/* Priority change to stage K demanded */
		/* Check if truncation of current stage CUSIG is inhibited (can't skip CUSIG) */

		/* First check if any indicators to be cleared during green */
		reset_links_ep_indicators(); /*if applicable*/

		/* Now look to truncate CUSIG.  Start with the assumption it is possible */

		priority_stage_num = process_priority_facility_code( get_current_stage()-1, priority_stage_num-1);



		/* change to stage 'priority_stage_num' (or closest intermediate stage if direct change banned) */
		while (priority_stage_num != get_current_stage())
		{
			if (DI_get_stage_change_allowance(get_current_stage(), priority_stage_num) != -1)
			{
				/* if stage demanded and direct change is allowed then implement */
				return priority_stage_num;
			}

			/* if can't change directly to stage, find the closest intermediate */
			priority_stage_num--;
			if (priority_stage_num <= 0)
				priority_stage_num = DI_get_stages_count();
		}
	}


	/* No valid priority stage demand found */
	return 0;
}

