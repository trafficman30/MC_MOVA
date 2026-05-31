/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         links_handler.c
*
*  TITLE:        Mova Kernel: Links Handler
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
#include "dataset_interface.h"

#include "junction_handler.h"
#include "stages_handler.h"
#include "links_handler.h"
#include "links_endsat_handler.h"
#include "links_ep_handler.h"
#include "lanes_handler.h"
#include "detectors_handler.h"
#include "dynamic_data.h"

#include "timers_manager.h"
#include "algorithms_manager.h"
#include "messages_manager.h"
#include "core_interface.h"
#include "kdebug.h"

#include "estimate_right_turners_count_during_opposed_stage.h"

#include <stdlib.h> /* PCMOVA, embedded too? For abs() */
#include <string.h>


STATIC struct Links		links;

/*Static function */
static void update_traffic_link_counts(uint8 linkIdx);
static void update_ped_ep_link_counts(uint8 linkIdx);




void init_links_handler()
{
	set_links_pointer_for_endsat_handler(&links);
	set_links_pointer_for_ep_handler(&links);

	CI_set_links_pointer(&links);
	ALG_set_links_pointer(&links);
	MSG_set_links_pointer(&links);
}


/*!
*	\brief	Initialises some variables in the Links struct. This part used to be
*			in Mova Setup (Mova Tools) and the initial values written in the MDS file.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void init_links_dynamic_data()
{
	uint8 l; /* links loop variable*/
	uint8 s; /* stages loop variable*/

	for(l=0; l<DI_get_links_count(); l++)
	{
		if(DI_is_ep_link(l+1))
		{
			links.demand_marker[l] = LINK_DEMAND_NO;
		}
		else
		{
			links.demand_marker[l] = LINK_DEMAND_LATCHED;
		}

		links.y_value[l] = 0;

		for(s=0; s<DI_get_stages_count(); s++)
		{
			if(DI_get_link_green_status(s+1, l+1) > 0)
				links.y_value[l] += get_stage_lambda(s);
		}

		links.smoothed_flow[l] = DI_get_link_lanes_count(l+1) * 18;

		links.extra_green_time[l] = 5;
		links.previous_endsat_marker[l] = LINK_ENDSAT_NORMAL;

#ifdef M8_IMPORVED_BUS_PRIORITY
		links.demanded_priority_stage_num[l] = 0;
#endif
	}

	memset(links.is_to_cancel_ep_demand_in_red, mv_false, sizeof(links.is_to_cancel_ep_demand_in_red));
	memset(links.is_ep_demanded_in_red, mv_false, sizeof(links.is_ep_demanded_in_red));
}



/*!
*	\brief	Performs a group of checks to choose the link that will be updated in either:
*			update_vehicle_link_counts(...) or update_ped_ep_link_counts(...).
*
*	\param[in]	xx	The ....
*	\param[in]  yy	The ....
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void update_links_at_intergreen()
{
	uint8 linkIdx; /* links loop variable */

	set_junction_total_smoothed_flow(0);

	for (linkIdx = 0; linkIdx < DI_get_links_count(); linkIdx++) 
	{
		/* Sum smoothed flows for all links */
		inc_junction_total_smoothed_flow( links.smoothed_flow[linkIdx] );

		/* link red (or effective red) in stage just ended */
		if( is_link_effectively_red(get_last_stage()-1, linkIdx))
			continue;
			
		if (links.green_flag[linkIdx] == LINK_FLAG_NOT_GREEN)
		{
			/* link previously green but not now -- reset now */
		}
		else
		{
			/* link now green but expected to change soon */
			/* reset it later */
			if (DI_get_link_green_status(get_next_stage(), linkIdx+1) == LINK_STATUS_RED)
				continue;

			/* now left with links green from lastag to snext */

			/* skip resets unless SNEXT < LASTAG and cycle is restarting */
			if (get_next_stage() > get_last_stage())
				continue;

			/* or if link has not been green for > cycle time */
			if (timers.link_green_timer[linkIdx] <= get_junction_smoothed_cycle_time())
				continue;
		}

		/* Check if the link already partially reset */
		if (timers.cycle_time_timer[linkIdx] >= timers.signal_state_timer)
		{
			if (DI_is_traffic_link(linkIdx+1))
			{
				/* normal traffic links */
				update_traffic_link_counts(linkIdx);
			}
			else
			{
				/* pedestrian/emergency/priority link -- omit lane checks */
				/* save cumulative detector counts */
				update_ped_ep_link_counts(linkIdx);
			}

			timers.cycle_time_timer[linkIdx] = 0;
		}

		/* skip timer reset if link is still green */
		if(links.green_flag[linkIdx] > 0)
			continue;

		/* skip if timers already reset on a previous scan */
		if(timers.link_green_timer[linkIdx] < timers.signal_state_timer)
			continue;

		/* Zero timers at start of amber */
		timers.link_green_timer[linkIdx] = 0;
		timers.link_waste_green_timer[linkIdx] = 0;

		links.previous_endsat_marker[linkIdx] = links.endsat_marker[linkIdx];
		links.endsat_marker[linkIdx] = LINK_ENDSAT_NOT_YET;
		links.max_percent_sat[linkIdx] = 0;
		links.var_min_green[linkIdx] = 0;
		links.demand_marker[linkIdx] = LINK_DEMAND_NO;
	}

	/* Reset demand delays for emergency/priority links with 4TT and 5TT SDCodes */
	memset(links.ep_demand_delay, 0, sizeof(links.ep_demand_delay));
}



/*!
*	\brief	Updates all the detectors count belong to the lanes of a specific link.
*			It also updates some important links variables: smoothed_flow, y_value
*			and extra_green_time.
*
*	\param[in]	xx	The ....
*	\param[in]  yy	The ....
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static void update_traffic_link_counts(uint8 linkIdx)
{
	uint8 k; /*lanes in link loop variable*/
	uint8 lane_idx; /*m-1*/

	int32 spare_green;
	int32 det_count; /*c*/
	uint8 lanes_count; /*j*/

	/* resetting traffic links params, that will be updated/set below */
	links.det_fault_marker[linkIdx] = LINK_DET_FAULT_NO;
	links.smoothed_flow[linkIdx] = 0;
	links.extra_green_time[linkIdx] = 0;
	links.y_value[linkIdx] = 24;

	
	lanes_count = DI_get_link_lanes_count(linkIdx+1);

	for (k = 0; k < lanes_count; k++) 
	{
		/* for all lanes on link, reset red counts & zero endsat markers */
		lane_idx = DI_get_lane_num(linkIdx+1, k) - 1;

		/* before resetting REDCX, save spare green for all lanes */
		spare_green = timers.link_green_timer[linkIdx] - get_lane_red_count_x_det(lane_idx) * DI_get_satinc_time(lane_idx+1);
		if (spare_green > 0)
			links.extra_green_time[linkIdx] += spare_green;

		reset_lane_counts_and_markers(lane_idx);
		
		det_count = update_lane_dets_status(linkIdx, lane_idx);

		calc_lane_smoothed_flow(linkIdx, lane_idx, det_count);

		/* Updating some other parameters related to just calculate the smoothed flow */
		links.y_value[linkIdx] += DI_get_satinc_time(lane_idx+1) * get_lane_smoothed_flow(lane_idx);
		links.smoothed_flow[linkIdx] += get_lane_smoothed_flow(lane_idx);

		update_lane_dets_cum_counts(lane_idx);
	}

	/* Finally correct the calculated Y_VALUE and EXTRA_GREEN_TIME*/

	/* convert sum(3600*Y) for lanes to Y% per link */
	p_divide_error((int)(36* lanes_count ), 210);
	links.y_value[linkIdx] = links.y_value[linkIdx]/(36*lanes_count);

	if (links.y_value[linkIdx] > 90)
		links.y_value[linkIdx] = 90;

	if (links.y_value[linkIdx] < 1)
		links.y_value[linkIdx] = 1;

	/* convert spare green sum to mean freeflow green (0.1sec) */

	p_divide_error((int32)(lanes_count), 211);
	p_divide_error((int32)(100-links.y_value[linkIdx]), 212,"link number was %d", linkIdx+1);

	links.extra_green_time[linkIdx] = links.extra_green_time[linkIdx]/lanes_count*10/(100-links.y_value[linkIdx])*10;

	if(links.extra_green_time[linkIdx] > timers.link_green_timer[linkIdx]) 
		links.extra_green_time[linkIdx] = timers.link_green_timer[linkIdx];
}



/*!
*	\brief	Update the counts of the ep and ped links.
*
*	\param[in]	linkIdx		The ep or the ped link index.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static void update_ped_ep_link_counts(uint8 linkIdx)
{
	uint8 i; /* e/p dets loop variable*/

	uint8 det_num;
	uint8 link_num = linkIdx+1;
	
	if (DI_is_ped_link(link_num))
	{
		/* Then is ped or e/p link which may have LTYPE = D or D+100 or D+200 */
		/* (d adjustments added for PB681 issue 14 - MOVA-85) */
		det_num = DI_get_ped_det_num(link_num);

		update_pre_cum_det_count_with_old(det_num-1);
		return;
	}

#if defined (CMOVA_EP)
	for (i=1; i<=MAX_EP_DETS; i++)
	{
		det_num = DI_get_ep_det_num(link_num, i);

		if(det_num > 0) 
			update_pre_cum_det_count_with_old(det_num-1);
	}

	det_num = DI_get_ep_cancel_det_num(link_num)%100;

	if (det_num > 0)
		update_pre_cum_det_count_with_old(det_num-1);
#endif
}



/*!
*	\brief	Resets the following variables for a specific link: Timers::link_green_timer, 
*			Timers::link_waste_green_timer, Links::endsat_marker, Links::max_percent_sat,
*			and Lanes::endsat_marker.
*
*	\param[in]  linkIdx		The index of the link.
*
*	\return		FALSE if to skip that link (i.e., not to continue the function after its calling).
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
mv_bool reset_link_timers_and_markers_at_stage_green(uint8 linkIdx)
{
	/* Skip link if it is red now */
	if (links.green_flag[linkIdx] == LINK_FLAG_NOT_GREEN)
		return mv_false;

	if (links.var_min_green[linkIdx] != 0)
	{
		if (DI_get_link_green_status( get_current_stage(), linkIdx + 1) == LINK_STATUS_GREEN_AND_UNOPPOSED && 
			DI_get_link_green_status( get_last_stage(), linkIdx + 1) != LINK_STATUS_GREEN_AND_UNOPPOSED)
		{
			/* Overlap link (opposed sometime) reset min green now */
			if ( !DI_is_ped_link(linkIdx+1) )
			{
				timers.link_green_timer[linkIdx] = timers.signal_state_timer;

				/* To skip e/p links which have no lanes*/ 
				if( DI_is_traffic_link(linkIdx+1) )
				{
					timers.link_waste_green_timer[linkIdx] = 0;

					links.max_percent_sat[linkIdx] = 0;
					links.endsat_marker[linkIdx] = LINK_ENDSAT_NOT_YET;	
				}

				reset_lanes_endsat_marker(linkIdx);
			}
		}
		else
		{
			return mv_false;
		}
	}

	return mv_true;
}



/*!
*	\brief	Updates the link's var_min_green and then call the function
*			calc_lanes_var_min_green(...) to calculate its lanes var_min_green. 
*
*	\param[in]	linkIdx		The link index to calculate its var_min_green.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void update_link_var_min_green(uint8 linkIdx)
{		
	/* Initially, set the link var-min-green with its absolute-min-green value from the dataset */
	links.var_min_green[linkIdx] = DI_get_link_min_green_time(linkIdx+1);

	/* variable-min-green to be set to the traffic links only */
	if( DI_is_traffic_link(linkIdx+1) )
	{
		calc_lanes_var_min_green(linkIdx, &links.var_min_green[linkIdx]);
	}
}


/*!
*	\brief	Updates the lanes demand for a specific traffic link.
*
*	\param[in]	linkIdx		The index of the traffic link.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void update_traffic_link_demand(uint8 linkIdx)
{
	uint8 k; /*lanes loop variable*/

	uint8 lane_idx;
	int8 ret_val;

	/* If link already demanded no need to check further */
	if(links.demand_marker[linkIdx] > LINK_DEMAND_NO)
	{
		return;
	}

	for (k = 0; k < DI_get_link_lanes_count(linkIdx+1); k++) 
	{
		lane_idx = DI_get_lane_num(linkIdx+1, k) - 1;

		ret_val = decide_lane_demand(linkIdx, lane_idx);

		if(ret_val == SKIP_TO_NEXT_LANE)
		{
			continue;
		}
		else /* a change of the demand is required */
		{
			links.demand_marker[linkIdx] = ret_val;

			KDEBUG_WRITE2(DEBUG_LEVEL_VERBOSE, CORE, "Setting link demand marker=%d for link number:%d", ret_val, linkIdx+1 );

			return;
		}

	}
}



/* This function */
/*!
*	\brief	Sets the special-demand for the RT links that has an OUT-DET. 
*			This includes the call-cancel demand checking.
*
*	\param[in]	linkIdx		The special link index.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void update_special_link_demand(uint8 linkIdx)
{
	uint8 k; /*lanes loop variable*/

	uint8 lane_idx;
	int32 min_q;  /* minimum Q for special demand from X-OUT count */
	int32 total_q;

	int8 ret_val;

	total_q = 0;

	/* Skip if no requirement for special demands for this link */
	min_q = DI_get_min_veh_count_between_x_and_out_det(linkIdx+1);

	if (min_q == 0)
		return;

	if (min_q < 0)
		min_q = -min_q;
	
	for (k=0; k < DI_get_link_lanes_count(linkIdx+1); k++) 
	{
		lane_idx = DI_get_lane_num(linkIdx+1, k) - 1;

		ret_val = decide_lane_special_demand(linkIdx, lane_idx, min_q, &total_q);
		if(ret_val == SKIP_TO_NEXT_LANE)
		{
			continue;
		}
		else
		{
			links.demand_marker[linkIdx] = ret_val;
			return;
		}
	}

	/* if no vehicles present or link has no OUT-DET */
	if (total_q == 0)
	{
		return;
	}

	/* if queue count insufficient for special demand, set normal demand */
	if (total_q < min_q)
	{
		links.demand_marker[linkIdx] = LINK_DEMAND_LATCHED;
		return;
	}

	/* Set special demand type 2 for reasons other than call/cancel */
	links.demand_marker[linkIdx] = LINK_DEMAND_UNLATCHED;
}



/*!
*	\brief	Updates the ped link demand marker and the total bonus green time.
*			Do not call this function without checking that the input parameter
*			(linkIdx) represents a ped link.
*
*	\param[in]	linkIdx		The ped link index.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void update_ped_link_demand(uint8 linkIdx)
{
    uint8 det_num; /*detNo*/
	int32 count_diff; /*c*/
	uint8 ped_wait_channel; /*nPedWaitChannel*/
    mv_bool ped_wait_signal; /*nPedWaitSignal*/

	uint8 link_num = linkIdx+1;

    /* Initialise pedestrian queue to zero */
	links.total_bonus_green_time[linkIdx] = 0;
    
    if (DI_is_latched_ped_link(link_num))
    {
		/* Traditional latched pedestrian detector (LTYPE=detNo) */
		/* Set latched pedestrian demand */

		det_num = DI_get_ped_det_num(link_num);

		count_diff = get_det_counts_diff_pre(det_num);

		if (count_diff != 0 || is_det_on(det_num))
		{
			/* set demand & set Q to nominal 1 pedestrian */
			/* Ped Q scaled by 10. O/P via equiv to TOTBON is /10 */
			links.demand_marker[linkIdx] = LINK_DEMAND_LATCHED;
			links.total_bonus_green_time[linkIdx] = 10;
		}
    }
    else  
    {
		/* Else LTYPE > 100 (Puffin-type ped link - unlatched) */

		/*  No longer using a fixed wait light 
		* channel - may be different wait lights for 
		* different pedestrian detectors. Use a signal from
		* a user-specified wait light channel (Tcomshr->waitch)
		* that can vary for different pedestrian links. */
		ped_wait_channel = DI_get_wait_channel_num(link_num);
          
		/* If the user has specified a wait light channel */
		if (ped_wait_channel > 0)
		{
			/* Get whether it's on or off */
			ped_wait_signal = is_det_on(ped_wait_channel);
		}
		else
		{
			/* Else no wait channel is specified */
			/* Assume wait light is on (signal may already
			* have been combined (ANDed) with the 
			* pedestrian detector (d) in the controller) */
			ped_wait_signal = mv_true;
		}

		/* Clear unlatched pedestrian demand and reset as necessary */
		links.demand_marker[linkIdx] = LINK_DEMAND_NO;

		det_num = DI_get_ped_det_num(link_num);

#ifndef M8_ENABLED

		/* Clear suspect detector flag prior to possible setting */
		set_det_suspicion_status(det_num, DET_FAULT_STATUS_OK);

		/* Detector channel has been constantly 'on' for > 4-minutes
		so flag suspect detector, purely for information */

		/*Wrong implementation in M7 */
		if (get_det_on_flag(det_num) > 2400)
			set_det_suspicion_status(det_num, DET_FAULT_STATUS_SUSPECTED_PED_LINK);
#endif
		if (is_det_on(det_num) && ped_wait_signal)
		{
			/* use detector output only if wait light is on */
			/* set demand and set queue to nominal one ped */
			links.demand_marker[linkIdx] = LINK_DEMAND_UNLATCHED;
			links.total_bonus_green_time[linkIdx] = 10;

			if (DI_get_ped_max_2(get_current_stage()) != 999)
			{
				set_ped_demand_present(mv_true);
			}
		}
    } /* Else LTYPE > 100 (Puffin-type ped link) */

#ifdef M8_DET_FAULT_CHECKING_AND_LOGGING
	update_ped_det_sus_status(det_num);
#endif

}


/*!
*	\brief	Calculates the current green time for a specific link.
*
*	\param[in]	stageIdx	The index of the currently running stage. 
*	\param[in]  linkIdx		The link index.
*
*	\return		The calculated green time.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
int32 calc_link_current_green_time(uint8 stageIdx, uint8 linkIdx)
{
	int32 green_time; /*a*/
	
	green_time = timers.link_green_timer[linkIdx];

	/*First, calculate the current green time*/
	if( DI_get_link_green_status(stageIdx+1, linkIdx+1) != LINK_STATUS_RED)
	{
		/* link is GREEN now and also in the stage following.*/
		
		/*Add in the min green length of SNEXT + the IG length */
		green_time = timers.link_green_timer[linkIdx] + DI_get_stage_min_green_time(stageIdx+1)
						+ abs( DI_get_stage_change_allowance(get_current_stage(), stageIdx+1));
	}

	return green_time;
}



/*!
*	\brief	Update the benefiting markers for all the non-red traffic  
*			links in the junction.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void update_links_benefiting_marker()
{
	uint8 l; /* links loop variable */

	int32 a;
	
	/* Skip resetting of benefiting links if no change in snext 
	and if already set once for this stage */

	for (l=0; l<DI_get_links_count(); l++)
	{
		links.benefiting_marker[l] = BENEFIT_RESET;

		/* Skip ped and em/pr links */
		if(!DI_is_traffic_link(l+1))
			continue;

		/* if link red now it is irrelevant to end-sat decision */
		if (DI_get_link_green_status( get_current_stage(), l+1) == LINK_STATUS_RED)
			continue;

		/* Now check the link status in the next stage */
		if( is_link_effectively_red( get_next_stage()-1, l))
		{
			/* link is green now and red / effective red next stage */
			/* therefore link is relevant in determining benefits */
			links.benefiting_marker[l] = BENEFIT_RELEVENT_LINK; 
		}
		else
		{
			a = DI_get_stage_min_green_time(get_next_stage()) + 5;

			/* IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: if link is fixed green 
			   in the next stage AND does not have an extendable green 
			   in a stage immediately following the next stage */

			if (a >= DI_get_stage_max_green_time( get_next_stage()) 
				 && check_green_in_following_stages(l, get_next_stage()-1) == 1)
			{
				/* fixed green next - link is relevant */
				links.benefiting_marker[l] = BENEFIT_RELEVENT_LINK;
			}
			else
			{
				/* link overlaps CUSIG/SNEXT and is not relevant */
				links.benefiting_marker[l] = BENEFIT_IRRELEVENT_LINK;
			}
		}

#ifdef M8_LINK_MAXIMUM
		if (timers.link_green_timer[l] > DI_get_link_max_green_time(l + 1))
		{
			links.benefiting_marker[l] = BENEFIT_RESET;
		}
#endif
	}
}



/*!
*	\brief	Calculates all the traffic and green links merit values.
*
*	\return		the calculated merit value.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
int32 calc_links_merit_value()
{
	uint8 l; /* links loop variable */

	/* calculations parameters */
	int32 h = 0;
	int32 f;
	int32 k;

	for(l=0; l<DI_get_links_count(); l++)
	{
		/* Skip pedestrian & e/p links, or red links. */
		if(!DI_is_traffic_link(l+1) || links.green_flag[l] == LINK_FLAG_NOT_GREEN)
			  continue;
		
		f = links.y_value[l];
		k = DI_get_link_lanes_count(l+1);
			
		/* Check if min-green reset for an overlap link previously opposed */
		if(links.endsat_marker[l] == LINK_ENDSAT_NOT_YET)
		{
			/* check if overlap link was end-sat last cycle*/
			
			if( links.benefiting_marker[l] == BENEFIT_RELEVENT_LINK ||
				links.previous_endsat_marker[l] <= LINK_ENDSAT_NOT_YET)
			{
				f = 100;

				if(timers.link_green_timer[l] >= DI_get_bonus_green_recalc_time(l+1))
					k = DI_get_link_main_lanes_count(l+1);
			}
		}
		
		/* Reduce number of lanes after short lanes finish*/
		h = h+f*k;
    }

	return h;
}

void update_link_green_flag(uint8 linkIdx)
{
	links.green_flag[linkIdx] = (int8)input_data.links_green_flag[linkIdx];
}

#ifdef M8_RT_OPTIMISATION
/*!
*	\brief	Update the opposed link's lane X-DET count. This is the link
*			that contains the RT and was opposed in the previous stage.
*			The function will not be called unless there are recorded gap(s).
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		24-03-2014 */
void update_opposed_link_red_x_counts(uint8 netGapsCount, int32 * netGaps)
{
	uint8 i; /* links loop variable */
	uint8 j; /* lanes loop variable */

	uint8 rt_lane_num;
	int32 get_away_veh_count = 0;


	//TOCHECK: I'm using here the get_next_stage() because this function will be called (most of the time)
	//			during IG. So, I need to double check that, in case of Call/Cancel, there is no way
	//			to switch from the OPPOSED to the UNOPPOSED without running an IG between them. Cos otherwise
	//			(i.e., overlapping stages(not IG) the get_next_stage() will return the wrong stage.


	/* loop till finding the link that was opposed in the previous stage, and now not opposed */
	/* Also make sure that MIXOUT of this link = -99 (Call/Cancel)*/
	for(i=0; i<DI_get_links_count(); i++)
	{
		if(DI_get_link_green_status( get_next_stage(), i+1) == LINK_STATUS_GREEN_AND_UNOPPOSED &&
			DI_get_min_veh_count_between_x_and_out_det(i+1) == USE_CALL_CANCEL)
		{				
			KDEBUG_WRITE1(DEBUG_LEVEL_INFO, CORE, "The net gaps array: %s", KDebugToString(netGapsCount, netGaps ));

			/* loop over the link's lane*/
			/* loop over all the lanes of this link*/

			/* this loop should run only once*/
			for (j=0; j< DI_get_link_lanes_count(i+1); j++) 
			{
				/*getting the lane index of the opposed lane*/
				rt_lane_num = DI_get_lane_num(i+1, j);

				get_away_veh_count = ALG_estimate_right_turners_count_during_opposed_stage( get_previous_stage(),
																							rt_lane_num ,
																							netGaps,
																							netGapsCount);

				KDEBUG_WRITE1(DEBUG_LEVEL_INFO, CORE, "The estimated got away veh count=%d", get_away_veh_count);

				dec_lane_red_count_x_det(rt_lane_num-1, get_away_veh_count);
			}

			break; /* as there is only one link in the junction that meets this condition (if any).*/
		}
			
	}
}
#endif //M8_RT_OPTIMISATION



#ifdef M8_IMPROVED_LINKING

uint8 decide_forward_or_backward_linking(uint8 linkIdx, int32 * timeOffset)
{
	uint8 l; /*lanes loop variable*/

	uint8 lane_idx;

	/*CHECK: Check what we will do when we have multiple lanes links.*/

	for(l=0; l< DI_get_link_lanes_count(linkIdx+1); l++)
	{
		lane_idx = DI_get_lane_num(linkIdx+1, l) - 1;

		*timeOffset = calc_lane_time_offset_for_linked_mova(lane_idx);

		printf("Offset value is: %ld \n", *timeOffset);

		/*Check if we need to do Forward or Backward linking*/
		if(*timeOffset >= DI_get_upstream_junction_intergreen())
		{
			printf("Do FORWARD linking \n");

			/*O >= UPSMIG*/
			return FORWARD_LINK;
		}
		else /*O < UPSMIG*/
		{
			printf("Do BACKWARD linking \n");
			return BACKWARD_LINK;
		}
	}

	/*Should never reach this point.*/
	return INVALID_LINK;
}

#endif

/*----------------------------------------------------------------*/
/*------------------------- Getters ------------------------------*/
/*----------------------------------------------------------------*/
uint8 get_link_bonus_stage(uint8 linkIdx) /*LBONST[links]*/
{
	return	links.bonus_stage[linkIdx];
}

int8 get_link_green_flag(uint8 linkIdx)
{
	return links.green_flag[linkIdx];
}

int32 get_link_ep_skip_indicator(uint8 linkIdx)
{
	return links.ep_skip_indicator[linkIdx];
}

int32 get_link_ep_trunc_indicator(uint8 linkIdx)
{
	return links.ep_trunc_indicator[linkIdx];
}

int32 * get_links_reduced_lost_time()
{
	return links.reduced_lost_time;
}

int8 get_link_endsat_marker(uint8 linkIdx)
{
	return links.endsat_marker[linkIdx];
}

int32 * get_links_total_bonus_green_time()
{
	return links.total_bonus_green_time;
}

uint8 * get_links_bonus_stage()
{
	return links.bonus_stage;
}

int8 * get_links_endsat_marker()
{
	return links.endsat_marker;
}

mv_bool is_oversat_link(uint8 linkIdx) /* linkos > 0 */
{
	return links.oversat_weighting_factor[linkIdx] > 0;
}

int32 get_link_total_bonus_green_time(uint8 linkIdx)
{
	return links.total_bonus_green_time[linkIdx];
}

int32 get_link_var_min_green_time(uint8 linkIdx) /*mingre*/
{
	return links.var_min_green[linkIdx];
}

int8 get_link_demand_marker(uint8 linkIdx) /*lindem*/
{
	return links.demand_marker[linkIdx];
}

int8 get_link_benefiting_marker(uint8 linkIdx) /*benlin*/
{
	return links.benefiting_marker[linkIdx];
}

#ifndef M8_REMOVE_REV_DEMAND
int8 get_link_rev_demand_marker(uint8 linkIdx)
{
	return links.rev_demand_marker[linkIdx];
}
#endif

int32 get_link_smoothed_flow(uint8 linkIdx)
{
	return links.smoothed_flow[linkIdx];
}

int32 get_link_y_value(uint8 linkIdx)
{
	return links.y_value[linkIdx];
}

#if defined (TRL_HOST_DEBUG)
int32 * get_links_bonus_green_time()
{
	return links.bonus_green_time;
}
#endif


/*----------------------------------------------------------------*/
/*------------------ Incrementers/Decrementers -------------------*/
/*----------------------------------------------------------------*/
void inc_link_oversat_weighting_factor_counter(uint8 linkIdx, int32 incrementValue)
{
	links.oversat_weighting_factor[linkIdx] += incrementValue;
}

void dec_link_oversat_weighting_factor_counter(uint8 linkIdx, int32 decrementValue)
{
	links.oversat_weighting_factor[linkIdx] -= decrementValue;
}

mv_bool is_link_effectively_red(uint8 stageIdx, uint8 linkIdx)
{
	return DI_get_link_green_status(stageIdx+1, linkIdx+1) == LINK_STATUS_RED || 
		    DI_get_link_green_status(stageIdx+1, linkIdx+1) == LINK_STATUS_GREEN_BUT_OPPOSED;
}

void inc_link_det_fault_marker(uint8 linkIdx, int32 incVal)
{
	links.det_fault_marker[linkIdx] += incVal;
}


/*----------------------------------------------------------------*/
/*------------------------- Setters ------------------------------*/
/*----------------------------------------------------------------*/

#ifndef M8_REMOVE_REV_DEMAND
void set_rev_demand_marker(uint8 linkIdx, int8 val)
{
	links.rev_demand_marker[linkIdx] = val;
}
#endif