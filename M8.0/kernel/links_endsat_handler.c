/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         links_endsat_handler.c
*
*  TITLE:        Mova Kernel: Links Endsat Handler
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
#include "mova_types.h"
#include "dynamic_data.h"
#include "timers_manager.h"
#include "links_handler.h"
#include "links_endsat_handler.h"
#include "lanes_handler.h"
#include "lanes_endsat_handler.h"
#include "capacity_maximisation.h"
#include "dataset_interface.h"
#include "detectors_handler.h"

#include "kdebug.h"

static struct Links * p_links;
static struct CapacityMaxAlgInput	capacityMaxAlgInput;

static void update_suspected_link_endsat_marker(uint8 linkIdx);
static void update_oversat_link_endsat_marker(uint8 linkIdx);
static void update_normal_link_endsat_marker(uint8 linkIdx);

void set_links_pointer_for_endsat_handler(struct Links * linksRef)
{
	p_links = linksRef;
}



/*!
*	\brief	Updates the endsat marker for a specific link based on several checks.
*
*	\param[in]	linkIdx	The index of the link to update its endsat marker.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void update_link_endsat_marker(uint8 linkIdx)
{
	/* Check if the link GREEN time exceeded the threshold ESLMAX*/
	if(timers.link_green_timer[linkIdx] >= DI_get_link_endsat_max_green(linkIdx+1))
	{			
		p_links->endsat_marker[linkIdx] = LINK_ENDSAT_MAX_GREEN_EXPIRED;
		return;
	}

	/* Now check the status of the link */

	/*1- Check if the link has too many suspected detectors */
	if( p_links->det_fault_marker[linkIdx] > DI_get_link_main_lanes_count(linkIdx+1) )
	{
		update_suspected_link_endsat_marker(linkIdx); /*C3*/
		return;
	}

	/*2- Check if the link is oversaturated */
	if(is_oversat_link(linkIdx) && p_links->det_fault_marker[linkIdx] == LINK_DET_FAULT_NO)
	{
		update_oversat_link_endsat_marker(linkIdx); /*C4*/
	}
	else /* 3- Getting here, means that this is a normal link*/
	{
		update_normal_link_endsat_marker(linkIdx);
	}
}




/*!
*	\brief	Updates the endsat marker for a specific link that contains faulty detectors.
*
*	\param[in]	linkIdx	The index of the link to update its endsat marker.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static void update_suspected_link_endsat_marker(uint8 linkIdx)
{
	int32 green_time_threshold;

	green_time_threshold = calc_lanes_endsat_green_time_threshold(linkIdx);

	/*REQUIRED GREEN IS green_time_threshold IN 0.1 SECONDS, EXTRA GIVEN TO OVERSAT LINK */
	if(is_oversat_link(linkIdx))
		green_time_threshold += 75;
	
	/* Set LIENSA (Link Endsat Flag), just if we have sufficient GREEN */
	if(timers.link_green_timer[linkIdx] >= green_time_threshold)
	{
		p_links->endsat_marker[linkIdx] = LINK_ENDSAT_MIN_GREEN_EXPIRED_FOR_FAULTY_DET;
	}
}


/*!
*	\brief	Updates the endsat marker for an oversaturated link using the Capacity Max algorithm.
*
*	\param[in]	linkIdx	The index of the link to update its endsat marker.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static void update_oversat_link_endsat_marker(uint8 linkIdx)
{
	uint8 k; /* lanes loop variable*/

	/* Calculations parameters*/
	int32 satflow_headway = 0; /*c*/
	int32 excess_gap = 0; /*e*/
	int32 register_veh = 0; /*f*/
	int32 register_time = 0; /*h*/
	int32 flow_eff_factor = 0; /*a*/

	int32 on_time; /*a*/
	int32 off_time; /*g*/
	
	/*Compact MOVA: For determining the long lane to use for the upstream arrival efficiency check at S395 */
	int32 nLinkMOVQIN = 0;
	uint8 nLinkMOVQINLaneNum = 0; 

	uint8 lane_num;
	uint8 det_num;

	/* USED TO MARK LINKS WITH SUPERSATURATION ON SOME LANE(S) */
	mv_bool is_supersat_lane = mv_false;

	/* the main purpose of the following loop is to prepare the data to be passed to the 
	Capacity Maximisation algorithm, however at the same time it adjusts the 'link_waste_green_timer'*/
	for (k = 0; k<DI_get_link_lanes_count(linkIdx+1); k++) 
	{
		lane_num = DI_get_lane_num(linkIdx+1, k);

        if(get_lane_oversat_counter(lane_num-1) > 1)
			is_supersat_lane = mv_true;
              
		/* Compact MOVA: set MOVQIN lane number for the link to the maximum of the long lane MOVQINs.
             NOTE: We choose the max MOVQIN of long lanes so we don't 
             risk ending sat flow as a result of upstream inefficiency until 
             *all* long lanes on the link have had a chance to clear back to IN-detectors. */
		if (DI_get_in_det_num(lane_num)>0 && DI_get_moving_q_over_in_det(lane_num) > nLinkMOVQIN )
        {
            nLinkMOVQIN = DI_get_moving_q_over_in_det(lane_num);
            nLinkMOVQINLaneNum = lane_num;
        }

        /* Compact MOVA: do not assume IN-detector means long lane */
        if(!DI_is_long_lane(lane_num-1) && DI_get_short_link_marker(linkIdx+1) == 0)
			continue;

		det_num = (uint8)get_det_num_for_capacity_max_calc(linkIdx, lane_num-1);

		if(det_num == NO_SUITABLE_DET)
		{
			/* Skip if the X-DET is suspected and no ALT-DWN detector*/
			update_suspected_link_endsat_marker(linkIdx);
			return;
		}

		off_time = get_det_off_time(det_num);;
        on_time  = get_det_on_time(det_num);

		/* Check if the detector was ON for longer than for slow lorry */
        if(on_time > 25)
		{
            if(timers.link_green_timer[linkIdx] >= DI_get_moving_q_over_x_det(lane_num))
			{
				/* ADD EXCESSIVE 'ON' TIME TO WASTE-TIME. THIS SAVES E.S.=3 TEST */
            	timers.link_waste_green_timer[linkIdx] += 5;
				
			}
			
			calc_lane_capacity_max_params(linkIdx, lane_num-1, det_num, &excess_gap, &satflow_headway, &register_veh, &register_time, off_time);
			continue;
		}

    
		/* Check if the detector is fully OFF, else it is ON or OFF time = 0.1sec*/
		if(on_time <= 0)
		{
			if(off_time > DI_get_scan_period()+1)
			{
				timers.link_waste_green_timer[linkIdx] += (DI_get_scan_period() - on_time);

				calc_lane_capacity_max_params(linkIdx, lane_num-1, det_num, &excess_gap, &satflow_headway, &register_veh, &register_time, off_time);
				continue;
			}
		
			/* ADJUST WASTE-TIMER FOR MEAN GAP AT START GAP. JUMP IF DONE */
			timers.link_waste_green_timer[linkIdx] = timers.link_waste_green_timer[linkIdx]
													 - DI_get_lane_satflow_gap(lane_num) + off_time;
			
			calc_lane_capacity_max_params(linkIdx, lane_num-1, det_num, &excess_gap, &satflow_headway, &register_veh, &register_time, off_time);
			continue;
		}

        /* CHECK IF 'ON' ENOUGH FOR NO WASTE SINCE LAST SCAN */
		if(on_time >= DI_get_scan_period())
		{
			calc_lane_capacity_max_params(linkIdx, lane_num-1, det_num, &excess_gap, &satflow_headway, &register_veh, &register_time, off_time);
			continue;
		}

		/* CHECK TO ALLOW FOR SCAN-JUMPING OF SHORT GAP */
        if(on_time + get_det_old_gap(det_num-1) - (DI_get_scan_period()+1) > 0)
		{
			 timers.link_waste_green_timer[linkIdx] += (DI_get_scan_period() - on_time);
		}
		else
		{
			timers.link_waste_green_timer[linkIdx] = timers.link_waste_green_timer[linkIdx]
														- DI_get_lane_satflow_gap(lane_num)
														+ get_det_old_gap(det_num-1);
		}

		calc_lane_capacity_max_params(linkIdx, lane_num-1, det_num, &excess_gap, &satflow_headway, &register_veh, &register_time, off_time);
	}



	capacityMaxAlgInput.flow_eff_factor = flow_eff_factor;
	capacityMaxAlgInput.satflow_headway = satflow_headway;
	capacityMaxAlgInput.excess_gap = excess_gap;
	capacityMaxAlgInput.is_supersat_lane = is_supersat_lane;
	capacityMaxAlgInput.nLinkMOVQINLaneNum = nLinkMOVQINLaneNum;
	capacityMaxAlgInput.register_veh = register_veh;
	capacityMaxAlgInput.register_time = register_time;
	capacityMaxAlgInput.reduced_lost_time = p_links->reduced_lost_time;
	capacityMaxAlgInput.smoothed_bonus_green_time = p_links->bonus_green_time;
	capacityMaxAlgInput.max_percent_sat = p_links->max_percent_sat;
	capacityMaxAlgInput.link_green_timer = timers.link_green_timer;
	capacityMaxAlgInput.link_waste_green_timer = timers.link_waste_green_timer;

	p_links->endsat_marker[linkIdx] =  ALG_capacity_maximisation(linkIdx, p_links->endsat_marker[linkIdx] ,&capacityMaxAlgInput);

}

/*!
*	\brief	Updates the endsat marker for a normal (not oversaturated or suspected)	link.
*
*	\param[in]	linkIdx	The index of the link to update its endsat marker.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static void update_normal_link_endsat_marker(uint8 linkIdx)
{
	uint8 k; /* lanes loop variable*/
	uint8 lane_idx;

	for (k=0; k<DI_get_link_lanes_count(linkIdx+1); k++) 
	{
		lane_idx = DI_get_lane_num(linkIdx+1, k) - 1;

		if(!update_lane_endsat_marker(linkIdx, lane_idx))
		{
			if(get_lane_endsat_marker(lane_idx) == LANE_ENDSAT_QUEUE_OVER_X_DET)
			{
				p_links->endsat_marker[linkIdx] = LINK_ENDSAT_QUEUE_OVER_X_DET;
			}

			break;
		}
	}
}

/*!
*	\brief	Updates the endsat marker for a normal (not oversaturated or suspected)	link,
*			but after checking its lanes. The function is called from the Lanes Endsat Handler.
*
*	\param[in]	linkIdx		The index of the checked lane.
*	\param[in]	linkIdx		The index of the link to update its endsat marker.
*
*	\return		TRUE if to continue checking the lanes in this link, FALSE otherwise.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
mv_bool update_normal_link_endsat_marker_after_lanes_checking(uint8 linkIdx, uint8 laneIdx)
{
	mv_bool is_continue_lanes_checking; /*continue checking the lanes in this link?*/

	/* KDEBUG_WRITE1( DEBUG_LEVEL_VERBOSE, CORE, "Updated status for all the lanes ENDSAT marker: %s", KDebugToString((int32)DI_get_lanes_count(), get_lanes_endsat_marker())); */

	/* MUST NOT SKIP THIS LINK IF LIDETF>=2, AS O'SAT LANE MAY HAVE LAENSA=9*/
	if( is_oversat_link(linkIdx) && 
		get_lane_oversat_counter(laneIdx) == 0 &&
		p_links->det_fault_marker[linkIdx] < LINK_DET_FAULT_MAJOR)
	{
		is_continue_lanes_checking = mv_true;
	}
	else
	{
		p_links->endsat_marker[linkIdx] = LINK_ENDSAT_NORMAL;
		is_continue_lanes_checking = mv_false;
	}

	return is_continue_lanes_checking;
}


#ifdef M8_RE_ESTABLISHMENT_OF_SATFLOW
/*!
*	\brief	Check if a specific link has a platoon.
*
*	\param[in]	linkIdx The link index.
*
*	\return		TRUE if the link has a platoon of vehs.
*
*	\author		Islam Abdelhalim 
*	\date		27-03-2014 */
mv_bool check_link_has_platoon(uint8 linkIdx)
{
	uint8 l; /*lanes loop variable*/

	uint8 lane_idx;

	/* loop over all the lanes in this link */
	for(l=0; l< DI_get_link_lanes_count(linkIdx+1); l++)
	{
		lane_idx = DI_get_lane_num(linkIdx+1, l) - 1;

		if(lane_idx > -1 && check_lane_has_platoon(lane_idx))
		{
			/* resetting the link's endsat marker as there is a platoon, 
				so the link is not at endsat any more*/
			p_links->endsat_marker[linkIdx] = LINK_ENDSAT_NOT_YET;

			return mv_true;
		}
	}

	return mv_false;
}
#endif
