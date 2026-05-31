/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         links_ep_handler.c
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
#include "mova_constants.h"

#include "links_ep_handler.h"
#include "dataset_interface.h"
#include "dynamic_data.h"
#include "links_handler.h"
#include "junction_handler.h"
#include "detectors_handler.h"
#include "stages_handler.h"

#include "timers_manager.h"
#include "stages_ep_handler.h"

#include "tma_alerts.h"

typedef enum { RED, GREEN } COLOUR;

static struct Links	*	p_links;

/*static functions*/
static void check_ep_cancel_det(uint8 linkIdx);

void set_links_pointer_for_ep_handler(struct Links * linksRef)
{
	p_links = linksRef;
}


/*!
*	\brief	Updates the link demand marker (in case of e/p demand) and all the related markers.
*
*	\param[in]	linkIdx	The e/p link index.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void update_ep_link_demand(uint8 linkIdx)
{
	uint8 det_num;
	mv_bool ep_demand; /*epdem*/

	uint8 link_num = linkIdx + 1;

	/* If the link is now green do not modify demand here.
		Hold/extension is cancelled in block P9. */
	if (! is_link_effectively_red(get_current_stage()-1, linkIdx))
	{
		p_links->is_to_cancel_ep_demand_in_red[linkIdx] = mv_false;
		p_links->is_ep_demanded_in_red[linkIdx] = mv_false;
		return;
	}

	if (p_links->is_to_cancel_ep_demand_in_red[linkIdx])
	{
		/* check if there is an e/p det on now */
		ep_demand = is_ep_det_active(linkIdx, p_links->is_ep_demanded_in_red[linkIdx]);

		if(ep_demand)
		{
			/*clear the IG (RED) demand flag as it is no longer intergreen */
			p_links->is_ep_demanded_in_red[linkIdx] = mv_false;
		}

		/* check if the candet is still on */
		det_num = DI_get_ep_cancel_det_num(link_num) % 100;

		if (!is_det_suspected(det_num-1) && is_det_on(det_num))
		{
			/* candet is on so cancel demand */
			if (p_links->demand_marker[linkIdx] > LINK_DEMAND_NO)
			{
				p_links->demand_marker[linkIdx] = LINK_DEMAND_NO;
				clear_ep_stage_demand(linkIdx);
			}
		}
		else
		{
			/* candet is now off (or faulty) */
			p_links->is_to_cancel_ep_demand_in_red[linkIdx] = mv_false;

			/* However, the candet may have both gone ON and then OFF again during intergreen.
			   If no current e/p demand, cancel any existing demand. */
			if (!ep_demand && p_links->demand_marker[linkIdx] > LINK_DEMAND_NO)
			{
				p_links->demand_marker[linkIdx] = LINK_DEMAND_NO;
				clear_ep_stage_demand(linkIdx);
			}
		}
		return;
	}

	if (p_links->demand_marker[linkIdx] > LINK_DEMAND_NO)
	{
		/* Carry forward existing emerg/priority-vehicle demand */
		ep_demand = mv_true;
	}
	else
	{
		/* Otherwise check both e/p detectors on the link */
		ep_demand = is_ep_det_active(linkIdx, p_links->is_ep_demanded_in_red[linkIdx]);

		if(ep_demand)
			p_links->is_ep_demanded_in_red[linkIdx] = mv_false;
	}

	if (ep_demand)
	{
		/* Set link e/p demand */
		p_links->demand_marker[linkIdx] = LINK_DEMAND_EMERGENCY;

		if (DI_is_priority_link(link_num))
		{
			p_links->demand_marker[linkIdx] = LINK_DEMAND_PRIORITY;

#ifdef M8_IMPORVED_BUS_PRIORITY
			set_is_priority_demand_present(mv_true);  
#endif
		}
	}
}




/*!
*	\brief	Checks if the e/p hold time expired or not.
*
*	\param[in]	linkIdx				The e/p link index.
*	\param[in]  currentGreenTime	The value of the current green time.
*
*	\return		TRUE if e/p hold time expired or FALSE if not expirted yet.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
#ifndef M8_REMOVE_REV_DEMAND
mv_bool is_link_ep_hold_time_expired(uint8 linkIdx, int32 currentGreenTime)
{
	/* Otherwise, check for e/p demands and var_min_green */

	if (p_links->demand_marker[linkIdx] == LINK_DEMAND_EMERGENCY ||
		p_links->rev_demand_marker[linkIdx] == REV_DEMAND_EMERGENCY_HOLD_PRESENT ||
		p_links->rev_demand_marker[linkIdx] == REV_DEMAND_EMERGENCY_EXT_PRESENT)
	{
		/* There is emergency demand for the link */
		/* Check if it is cancelled or hold time has expired */
		if (currentGreenTime < DI_get_ep_hold_time(linkIdx + 1) && p_links->ep_cancel_marker[linkIdx] == EP_CANCEL_CLEAR)
		{
			p_links->rev_demand_marker[linkIdx] = REV_DEMAND_EMERGENCY_HOLD_PRESENT;
			set_ep_hold_marker(EMERGENCY_HOLD_PRESENT);

			return mv_false; /*not expired yet*/
		}
	}

	if (p_links->demand_marker[linkIdx] == LINK_DEMAND_PRIORITY ||
		p_links->rev_demand_marker[linkIdx] == REV_DEMAND_PRIORITY_HOLD_PRESENT ||
		p_links->rev_demand_marker[linkIdx] == REV_DEMAND_PRIORITY_EXT_PRESENT)
	{
		/* There is priority demand for the link */
		/* Check if it is cancelled or hold time has expired */
		if (currentGreenTime < DI_get_ep_hold_time(linkIdx + 1) && p_links->ep_cancel_marker[linkIdx] == EP_CANCEL_CLEAR)
		{
			p_links->rev_demand_marker[linkIdx] = REV_DEMAND_PRIORITY_HOLD_PRESENT;
			set_ep_hold_marker(PRIORITY_HOLD_PRESENT);

			return mv_false; /*not expired yet*/
		}
	}

	return mv_true;
}
#else
mv_bool is_link_ep_hold_time_expired(uint8 linkIdx, int32 currentGreenTime)
{
	/* Otherwise, check for e/p demands and var_min_green */

	if (p_links->demand_marker[linkIdx] == LINK_DEMAND_EMERGENCY)
	{
		/* There is emergency demand for the link */
		/* Check if it is cancelled or hold time has expired */
		if (currentGreenTime < DI_get_ep_hold_time(linkIdx + 1) && p_links->ep_cancel_marker[linkIdx] == EP_CANCEL_CLEAR)
		{
			set_ep_hold_marker(EMERGENCY_HOLD_PRESENT);
			return mv_false; /*not expired yet*/
		}
	}

	if (p_links->demand_marker[linkIdx] == LINK_DEMAND_PRIORITY)
	{
		/* There is priority demand for the link */
		/* Check if it is cancelled or hold time has expired */
		if (currentGreenTime < DI_get_ep_hold_time(linkIdx + 1) && p_links->ep_cancel_marker[linkIdx] == EP_CANCEL_CLEAR)
		{
			set_ep_hold_marker(PRIORITY_HOLD_PRESENT);
			return mv_false; /*not expired yet*/
		}
	}
	return mv_true;
}
#endif





/*!
*	\brief	Updates all the e/p links e/p flags and status.
*
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void update_ep_links_data()
{
	uint8 i; /*ep dets loop variable*/

	uint8 link_num;
	uint8 det_num;
	uint8 stage_num; /*stg*/

	mv_bool cancel_det_status; /*cdetStat*/
	mv_bool ep_demand; /*epdem*/

	COLOUR current_colour = RED; /*colour*/

	/* Record any CANDET coming on */
	for (link_num=1; link_num <= DI_get_links_count(); link_num++)
	{
		cancel_det_status = mv_false;

		det_num = DI_get_ep_cancel_det_num(link_num);

		if (det_num > 0)
		{
			if (is_det_on(det_num%100))
			{
				current_colour = RED;

				/* Is the link currently green or red */
				if (get_current_stage() == 0)
					stage_num = get_demanded_stage();
				else
					stage_num = get_current_stage();

				/* assume link is red if there has been an error getting the 'stage' */
				if (stage_num == 0)
					current_colour = RED;
				else
				{
					if (!is_link_effectively_red(stage_num-1, link_num-1) )
						current_colour = GREEN;
					else
						current_colour = RED;
				}

				/* cancel the demand based on CANDET type */
				/* 'is_to_cancel_ep_demand_in_red' is only used to cancel demand (i.e. before the stage runs)
				   so only set when link is RED and CANDET is > 100 */
				if (det_num > 100 && current_colour == RED)
				{
					cancel_det_status = mv_true;
					p_links->is_to_cancel_ep_demand_in_red[link_num-1] = mv_true;
				}
			}
			else
			{
				/* CANDET is off - clear any suspect status */
				det_num = det_num % 100;
				if (get_det_suspicion_status(det_num) == DET_FAULT_STATUS_SUSPECTED_DUE_TO_LONG_ON)
				{ 
					set_det_suspicion_status(det_num, DET_FAULT_STATUS_OK);

					TMA_det_faulty_changed(det_num-1, DET_FAULT_STATUS_OK);
				}

			}
		}

		/* Also check the e/p dets here and clear long ON faults if appropriate */
		for (i=1; i<=MAX_EP_DETS; i++)
		{
			det_num = DI_get_ep_det_num(link_num,i);
			if (det_num > 0)
			{
				if (!is_det_on(det_num) && get_det_suspicion_status(det_num) == DET_FAULT_STATUS_SUSPECTED_DUE_TO_LONG_ON)
				{ 
					set_det_suspicion_status(det_num, DET_FAULT_STATUS_OK);

					TMA_det_faulty_changed(det_num-1, DET_FAULT_STATUS_OK);
				}
			}
		}

		/* Also need to record any EPDET activity during IG which could be missed
		   by the time Block B runs during the next stage green */
		if (get_current_stage() == 0)
		{
			ep_demand = mv_false;

			for (i=1; i<=MAX_EP_DETS; i++)
			{
				det_num = DI_get_ep_det_num(link_num,i);

				if (det_num > 0)
				{
					if (is_det_on(det_num) && !is_det_suspected(det_num-1))
					{
						ep_demand = mv_true;
						break;
					}
				}
			}

			if (cancel_det_status)
			{
				ep_demand = mv_false;
				p_links->is_ep_demanded_in_red[link_num-1] = mv_false;
			}

			if (ep_demand)
				p_links->is_ep_demanded_in_red[link_num-1] = mv_true;;
		}
	}
}



/*!
*	\brief	Updates the e/p links extension timer.
*
*	\param[in]	linkIdx		The e/p link index.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void update_ep_link_ext_timer(uint8 linkIdx)
{
	uint8 i; /*ep det loop variable*/

	
	uint8 link_num = linkIdx+1;
	uint8 det_num;
	int32 ext_time;
		
	/* Decrement existing emerg/priority-link extension timer */
	if (timers.ep_link_ext_timer[linkIdx] > 0)
		timers.ep_link_ext_timer[linkIdx] -= 5;
	
	check_ep_cancel_det(linkIdx);

	/* Set-up link extensions on all emerg/pr links */
	/* Check both possible detectors on link */
	for (i=1; i<=MAX_EP_DETS; i++) 
	{
		det_num = DI_get_ep_det_num(link_num, i);

		if (det_num == 0)
			continue;

		if (get_det_on_time(det_num) < 3000)
		{
			if (!is_det_on(det_num))
				continue;

			if (p_links->ep_cancel_marker[linkIdx] == EP_CANCEL_CLEAR)
			{
				/* Set new priority extension if longer than existing one. */
				ext_time = DI_get_ep_ext_time(link_num, i);

				if (ext_time > timers.ep_link_ext_timer[linkIdx])
					timers.ep_link_ext_timer[linkIdx] = ext_time;
			}
		}
		else
		{
			/* flag 'long-on' (>300s) det suspect */
			set_det_suspicion_status(det_num, DET_FAULT_STATUS_SUSPECTED_DUE_TO_LONG_ON);

			TMA_det_faulty_changed(det_num-1, DET_FAULT_STATUS_SUSPECTED_DUE_TO_LONG_ON);
		}
	}
}



/*!
*	\brief	Checks the CANDET to see if e/p extension should be cancelled or not.
*
*	\param[in]	linkIdx		The e/p link index.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static void check_ep_cancel_det(uint8 linkIdx)
{
	uint8 det_num; /*detNo*/
	uint8 link_num = linkIdx+1;
	uint8 stage_num;
	COLOUR current_colour;

	det_num = DI_get_ep_cancel_det_num(link_num);
	
	if (det_num > 0)
	{
#ifdef M8_ENABLED /* MVAK-2908 */
		if (get_det_on_time(det_num%100) < 3000)
#else
		if (get_det_on_time(det_num % 100) < 1000)
#endif
		{
			if (is_det_on(det_num%100))
			{
				/* Is the link currently green or red */

				if (get_current_stage() == INTERGREEN_STAGE)
					stage_num = get_demanded_stage();
				else
					stage_num = get_current_stage();

				/* assume link is red there has been an error getting the 'stage' */
				if (stage_num == 0)
					current_colour = RED;
				else
				{
					if (DI_get_link_green_status(stage_num, link_num) != LINK_STATUS_RED && 
						DI_get_link_green_status(stage_num, link_num) != LINK_STATUS_GREEN_BUT_OPPOSED)
						current_colour = GREEN;
					else
						current_colour = RED;
				}

				/* cancel the extension based on CANDET type */
				/* Can only cancel extensions when link is green */
				/* CANDET=1nn => only cancel during red so it can't cancel extensions */
				if (current_colour == GREEN && (det_num < 100 || det_num > 200))
				{
					p_links->ep_cancel_marker[linkIdx] = EP_CANCEL_EXT;
					timers.ep_link_ext_timer[linkIdx] = 0;
				}
			}
			else
			{
				/* CANDET is an inhibit, so as soon as it goes off clear the cancel flag */
				p_links->ep_cancel_marker[linkIdx] = EP_CANCEL_CLEAR;
			}
		}
		else
		{
			/* flag 'long-on' (>300s) so the cancel det suspect */
			set_det_suspicion_status(det_num%100, DET_FAULT_STATUS_SUSPECTED_DUE_TO_LONG_ON);

			TMA_det_faulty_changed((det_num%100)-1, DET_FAULT_STATUS_SUSPECTED_DUE_TO_LONG_ON);
		}
	}
}



/*!
*	\brief	Updates all the links e/p indicators.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void update_links_ep_indicators()
{
	uint8 s; /*stage loop variable*/
	uint8 link_idx; /*links loop variable*/

	uint8 skipped_stage_num; /*n*/

	/* Now deal with truncation and skip indicators for links
	affected by emergency or priority action */

	/* decrement/reset indicators for links previously truncated or skipped
		or that will be skipped by emergency or priority action */
	for (s=0; s<DI_get_stages_count(); s++) 
	{
		skipped_stage_num = get_previous_stage() + (s+1);
		if (skipped_stage_num > DI_get_stages_count())
			skipped_stage_num -= DI_get_stages_count();

		/* First, reduce existing indicators for each stage passed */
		for (link_idx=0; link_idx<DI_get_links_count(); link_idx++) 
		{
			if ( p_links->ep_trunc_indicator[link_idx] > 0)
				p_links->ep_trunc_indicator[link_idx]--;

			if(p_links->ep_skip_indicator[link_idx] > 0)
				p_links->ep_skip_indicator[link_idx]--;
		}

		/* break if have reached next stage to which changing */
		if (skipped_stage_num == get_next_stage())
			break;

		/* continue if no emergency/priority change to cause any skipping */
		if (get_next_emeregency_stage_to_demand() == 0 && get_next_priority_stage_to_demand() == 0)
			continue;

		/* continue if no demand for skipped stage */
		if (get_stage_demand_marker(skipped_stage_num-1) <= STAGE_DEMAND_NO_DEMAND)
			continue;

		/* set skip ind's for traffic and ped links*/
		for (link_idx=0; link_idx<DI_get_links_count(); link_idx++)
		{
			/* Skip emerg/priority links, or links red in skipped stage N */
			if (DI_is_ep_link(link_idx+1) || DI_get_link_green_status(skipped_stage_num, link_idx+1) == LINK_STATUS_RED)
				continue;

			/* Skip setting of ind's for links not demanded */
#ifndef M8_REMOVE_REV_DEMAND
			if (p_links->demand_marker[link_idx] == LINK_DEMAND_NO && p_links->rev_demand_marker[link_idx] == 0)
				continue;
#else
			if (p_links->demand_marker[link_idx] == LINK_DEMAND_NO)
				continue;
#endif		

			/* set skip ind's for both ped and traffic links */
			p_links->ep_skip_indicator[link_idx] = DI_get_stages_count() + DI_get_stages_count();
		}
	}

	/* clear ind's if last green was enough */
	/* Note: if gets enough green in future, ind's will be cleared in B4 */
	for (link_idx=0; link_idx<DI_get_links_count(); link_idx++) 
	{
		/* Skip emerg/priority links, or links red in prev. stage */
		if (DI_is_ep_link(link_idx+1) || DI_get_link_green_status(get_previous_stage(), link_idx+1) == LINK_STATUS_RED)
			continue;

		if (DI_is_traffic_link(link_idx+1))
		{
			/* skip traffic link if still sat'd or had opposed green last stage */
			if (DI_get_link_green_status(get_previous_stage(), link_idx+1) == LINK_STATUS_GREEN_BUT_OPPOSED || p_links->endsat_marker[link_idx] == LINK_ENDSAT_NOT_YET)
				continue;
		}

		/* clear ind's for all ped links and traffic links previously end-sat in unopposed green */
		p_links->ep_skip_indicator[link_idx] = 0;
		p_links->ep_trunc_indicator[link_idx] = 0;
	}
}


/*!
*	\brief	Updates all the links e/p trunc indicators.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void update_links_ep_trunc_indicator()
{
	uint8 link_num;

	for (link_num=1; link_num<=DI_get_links_count(); link_num++)
	{
		/* Skip irrelevant non-traffic links, or those not green in CUSIG */
		if (!DI_is_traffic_link(link_num) ||  get_link_green_flag(link_num-1) == LINK_FLAG_NOT_GREEN)
			continue;

		/* set priority truncation flag = STAGES*2 on links still sat */
		/* For links already flagged end-sat, trunc'n is not a problem */
		if (p_links->endsat_marker[link_num-1] == LINK_ENDSAT_NOT_YET)
			p_links->ep_trunc_indicator[link_num-1] = DI_get_stages_count() * 2; 
    }
}

/*!
*	\brief	Process the NTT code for all the EP links.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-06-2015 */
void process_NTT_code_for_ep_links()
{
	uint8 i; /*e/p dets loop count*/

	uint8 link_idx;
	uint8 stage_num;
	uint8 det_num;
	int sdcode = 0;
	int delay_time;
	
	for (link_idx = 0; link_idx < DI_get_links_count(); link_idx++)
	{
		/* skip non em/pr links */
		if (!DI_is_ep_link(link_idx + 1))
			continue;
		 
		/* Does the link have a 4TT or 5TT code, and if so what is TT? */
		/* Start with CUSIG and process stages in order on the off chance
		   the link demands more than one stage */

		delay_time = 0;
		for (stage_num = 1; stage_num <= DI_get_stages_count(); stage_num++)
		{
			sdcode = DI_get_sdcode(stage_num, link_idx+1);

			/* only interested in 4TT or 5TT codes */
			if (sdcode < 400 || sdcode > 599)
				continue;

			delay_time = sdcode % 100;

			sdcode -= delay_time; /* store whether it was a 400 or 500 for later */

			break; /* Note the value of stage_num is also held for later use */
		}

#ifdef M8_IMPROVED_LINKING
		/*CHECK: check this condition with MC*/
		if (delay_time == 99) /*99 here is a magic number that means: (O - UPSMIG) should be used for the delay*/
		{
			if(get_forward_link_delay() > 0)
				delay_time = get_forward_link_delay();
			else
				delay_time = 0;
		}
		else
		{
			delay_time = delay_time * 10;
		}
#endif


		/* Not a 4TT or 5TT SDCODE on the link so skip it */
		if (delay_time == 0)
			continue;

		if (p_links->ep_demand_delay[link_idx] > 0)
		{
			/* Timer already started.  Increment for this scan */
			p_links->ep_demand_delay[link_idx] += DI_get_scan_period();

			/* Now check whether to insert a link demand */
			if (p_links->ep_demand_delay[link_idx] >= delay_time)
			{
				if (sdcode == 400 || get_stage_demand_marker(stage_num-1) > STAGE_DEMAND_NO_DEMAND)
				{
					p_links->demand_marker[link_idx] =  LINK_DEMAND_EMERGENCY;

					if (DI_is_priority_link(link_idx+1))
						p_links->demand_marker[link_idx] =  LINK_DEMAND_PRIORITY;
				}
			}
		}
		else
		{
			/* Check both e/p detectors for this link for demand */
			for (i = 1; i <= MAX_EP_DETS; i++)
			{
				det_num = DI_get_ep_det_num(link_idx+1, i);

				if (det_num > 0 && !is_det_suspected(det_num-1))
				{
					if (is_det_on(det_num))
					{
						/* initialise the delayed demand timer */
						p_links->ep_demand_delay[link_idx] = DI_get_scan_period();

						/* do NOT actually demand the link at this time - do that when the timer expires */
						break;
					}
				}
			}
		}
	}
}


/*----------------------------------------------------------------*/
/*------------------------- Restters -----------------------------*/
/*----------------------------------------------------------------*/

void reset_ep_links_cancel_marker()
{
	uint8 link_idx;
	uint8 previous_stage_num;

	/* Clear Tcomshr->cancel for e/p links that were green in previous stage
	   or any stages being skipped this change */
	for (link_idx=0; link_idx<DI_get_links_count(); link_idx++)
	{
		if (DI_is_ep_link(link_idx))
		{
			previous_stage_num = get_previous_stage();
			do
			{
				if(! is_link_effectively_red(previous_stage_num-1, link_idx))
				{
					p_links->ep_cancel_marker[link_idx] = EP_CANCEL_CLEAR;
					p_links->is_to_cancel_ep_demand_in_red[link_idx] = mv_false;
				}

				previous_stage_num++;

				if (previous_stage_num > DI_get_stages_count())
					previous_stage_num -= DI_get_stages_count();
			}
			while (previous_stage_num != get_demanded_stage());
		}
	}
}

void reset_links_ep_indicators()
{
	uint8 link_num;
		
	for (link_num=1; link_num <= DI_get_links_count(); link_num++)
	{
		if (DI_is_ep_link(link_num) || get_link_green_flag(link_num-1) == LINK_FLAG_NOT_GREEN)
		{
			/* e/p link or link now red */
			continue;
		}

		/* Ped links always get enough green.  Check vehicle links */
		if (DI_is_traffic_link(link_num))
		{
			/* check if opposed green */
			if(DI_get_link_green_status( get_current_stage(), link_num) != LINK_STATUS_GREEN_BUT_OPPOSED)
			{
				/* don't clear ind's if link still sat */
				if(get_link_endsat_marker(link_num-1) == LINK_ENDSAT_NOT_YET)
					continue;
			}
			else if (get_link_demand_marker(link_num-1) == LINK_DEMAND_UNLATCHED ||
						get_link_demand_marker(link_num-1) == LINK_DEMAND_CALL_CANCEL)
				continue;
		}

		p_links->ep_trunc_indicator[link_num-1] = 0;
		p_links->ep_skip_indicator[link_num-1] = 0;
	}
}

#ifdef M8_IMPORVED_BUS_PRIORITY
/*!
*	\brief	Resets the stage number for the priority link that demanded it.
*
*	\return		void
*
*	\author		Islam Abdelhalim
*	\date		20-09-2016 */

void reset_demanded_priority_stage_num(uint8 stageNum)
{
	uint8 l; /*links loop counter*/

	for (l = 0; l < DI_get_links_count(); l++)
	{
		if (p_links->demanded_priority_stage_num[l] == stageNum)
		{
			p_links->demanded_priority_stage_num[l] = 0;
		}
	}
}
#endif




int32 get_ep_link_demand_delay(uint8 linkIdx)
{
	return p_links->ep_demand_delay[linkIdx];
}

#ifdef M8_IMPORVED_BUS_PRIORITY
uint8 get_demanded_priority_stage_num_by_a_link(uint8 linkIdx)
{
	return p_links->demanded_priority_stage_num[linkIdx];
}
#endif


/*----------------------------------------------------------------*/
/*-------------------------  Setters -----------------------------*/
/*----------------------------------------------------------------*/

#ifdef M8_IMPORVED_BUS_PRIORITY
void set_demanded_priority_stage_num_by_a_link(uint8 linkIdx, uint8 stageNum)
{
	p_links->demanded_priority_stage_num[linkIdx] = stageNum;
}
#endif