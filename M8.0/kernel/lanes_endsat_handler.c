/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         lanes_endsat_handler.c
*
*  TITLE:        Mova Kernel: Lanes Endsat Handler
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
#include "timers_manager.h"
#include "dataset_interface.h"
#include "links_handler.h"
#include "links_endsat_handler.h"
#include "lanes_endsat_handler.h"
#include "detectors_handler.h"
#include "junction_handler.h"
#include "core_interface.h"


static struct Lanes * p_lanes;

static mv_bool do_second_part_of_endsat_checking(uint8 linkIdx, uint8 laneIdx);
static mv_bool finalize_x_det_checking(uint8 linkIdx, uint8 laneIdx);
static mv_bool use_in_det_to_check_endsat(uint8 linkIdx, uint8 laneIdx);
static int32   calc_corrected_lane_critical_gap(uint8 laneNum);
static mv_bool use_x_det_to_check_endsat(uint8 linkIdx, uint8 laneIdx);

void set_lanes_pointer_for_endsat_handler(struct Lanes * lanesRef)
{
	p_lanes = lanesRef;
}


/*!
*	\brief	Updates the lane endsat marker
*
*	\param[in]	linkIdx	The link index that includes the lane.
*	\param[in]  laneIdx	The lane index in which its endsat marker to be updates.
*
*	\return		FALSE if no need to update/inspect the reset of the links, TRUE otherwise
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
mv_bool update_lane_endsat_marker(uint8 linkIdx, uint8 laneIdx)
{
	/* SKIP IRRELEVANT OR DUD-DET LANE (=8/9) DO OTHER CHECK IF PROV-ES */
    if(p_lanes->endsat_marker[laneIdx] >= LANE_ENDSAT_IRRELEVANT_LANE)
		return mv_true;

    if(p_lanes->endsat_marker[laneIdx] == LANE_ENDSAT_PART_1_FROM_2PART_CHECKING_IS_DONE) 
		return do_second_part_of_endsat_checking(linkIdx, laneIdx);

	if(p_lanes->endsat_marker[laneIdx] > LANE_ENDSAT_NOT_YET)	
	{
		/* LANE ALREADY END-SAT TO CHECK IF LINK E.S. */
		return update_normal_link_endsat_marker_after_lanes_checking(linkIdx, laneIdx);
	}

	/* Compact MOVA:  do not assume a long lane has an IN-detector */
	if( !DI_is_long_lane(laneIdx) && DI_get_short_link_marker(linkIdx+1) <= 0)
	{
		
		if(timers.link_green_timer[linkIdx] < DI_get_time_to_use_comb_x(laneIdx+1) &&
			p_lanes->det_fault_marker[laneIdx] == LANE_DET_FAULT_NO_PROBLEM)
		{
			/* CHECK SHORT LANE ON LONG LINK BEFORE COMTIM*/
			return use_x_det_to_check_endsat(linkIdx, laneIdx);
		}

		
		/* ELSE, FLAG IRRELEVANT SHORT LANE AND SKIP TO NEXT LANE */
		p_lanes->endsat_marker[laneIdx] = LANE_ENDSAT_IRRELEVANT_LANE;
		return mv_true;
	}
	
	
	if(p_lanes->det_fault_marker[laneIdx] <= LANE_DET_FAULT_NO_PROBLEM)
	{
		return use_x_det_to_check_endsat(linkIdx, laneIdx);
	}

	
	if(p_lanes->det_fault_marker[laneIdx] == LANE_DET_FAULT_YES_BUT_CAN_DO_ENDSAT_CHECK) 
	{
		/* TO DO VARIMIN TYPE GREEN CHECK, SINCE ONLY IN-DET OK */
		return use_in_det_to_check_endsat(linkIdx, laneIdx);
	}
	else
	{
		/* ELSE, FLAG ENDSAT FOR LANE WITH ALL DETS ARE SUSPECTED*/
		p_lanes->endsat_marker[laneIdx] = LANE_ENDSAT_TOO_MANY_SUSPECTED_DETECTORS;
		return mv_true;
	}
}



/*!
*	\brief	Performs the second part of the endsat checking.
*
*	\param[in]	linkIdx	The link index that includes the lane.
*	\param[in]  laneIdx	The lane index in which its endsat to be checked.
*
*	\return		FALSE if no need to update/inspect the reset of the links, TRUE otherwise.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static mv_bool do_second_part_of_endsat_checking(uint8 linkIdx, uint8 laneIdx)
{
	uint8 det_num;

	/* NOTE PART 1 OF 2-PART CHECK DONE (PROV ES)
	NOW DO PART 2 OF 2-PART CHECK UNLESS ALREADY DONE */

	if(p_lanes->endsat_marker[laneIdx] > LANE_ENDSAT_PART_2_FROM_2PART_CHECKING_IS_DONE)
	{
		/* Getting here means that the 2nd Part is not done yet, so the following part will
		   do the check */
		det_num = DI_get_in_det_num(laneIdx+1);

		/* Compact MOVA: Ignore -ve IN dets */
		if(det_num > 0 && !is_det_suspected(det_num-1))
		{
			/* JUMP IF CAN'T DO PART 2 CHECK.ELSE, CHECK CRITICAL GAPS */
			if(get_det_off_time(det_num) <= calc_corrected_lane_critical_gap(laneIdx+1) &&
				get_det_old_gap(det_num-1) <= calc_corrected_lane_critical_gap(laneIdx+1))
			{
				if(get_det_on_time(det_num) < DI_get_det_min_on_time_for_queue(laneIdx+1)||
					timers.link_green_timer[linkIdx] < DI_get_moving_q_over_in_det(laneIdx+1))
				{
					return mv_true;
				}
			}
		}

		/* FINALLY JUMP IF PART 2 CHECK FAILS. ELSE, NOTE PROV ES*/
		p_lanes->endsat_marker[laneIdx] -= 2; 
	}

	/* JUMP IF BOTH PARTS NOT COMPLETE. ELSE, FLAG ES=4*/
	if(p_lanes->endsat_marker[laneIdx]  >= LANE_ENDSAT_PART_2_FROM_2PART_CHECKING_IS_DONE)
		return mv_true;

	p_lanes->endsat_marker[laneIdx] = LANE_ENDSAT_PART_1_AND_2_ARE_DONE;

	return update_normal_link_endsat_marker_after_lanes_checking(linkIdx, laneIdx);
}


/*!
*	\brief	Uses X-DET to check endsat which is the normal endsat checking based on the detected gaps.
*
*	\param[in]	linkIdx	The link index that includes the lane.
*	\param[in]  laneIdx	The lane index in which its endsat to be checked.
*
*	\return		FALSE if no need to update/inspect the reset of the links, TRUE otherwise.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
STATIC mv_bool use_x_det_to_check_endsat(uint8 linkIdx, uint8 laneIdx)
{
	uint8 det_num; /*d*/
	uint8 comb_x_det_num; /*c*/
	uint8 lanes_count; /*aa*/
	
	uint8 lane_num = laneIdx+1;

	/*calculation parameters*/
	int32 e;

	det_num = DI_get_x_det_num(lane_num); /*X_DET*/

    if(is_det_suspected(det_num-1))
		det_num = DI_get_alt_down_det_num(lane_num);

    comb_x_det_num = DI_get_comb_x_det_num(lane_num);

	/* USE COMBX-DET IF EXISTS AND LINK HAS BEEN GREEN LONG ENOUGH */
    if(	comb_x_det_num > 0 && 
		!is_det_suspected(comb_x_det_num-1) &&
		timers.link_green_timer[linkIdx] >= DI_get_time_to_use_comb_x(lane_num))	
	{
		det_num = comb_x_det_num ;
	}

	/* X-DET OR ALT OK. CHECK FOR QUEUE OVER DET */
	if(	timers.link_green_timer[linkIdx] >= DI_get_moving_q_over_x_det(lane_num) &&
		is_queue_over_det(linkIdx,det_num-1))
	{
		/* FLAG LINK ENDSAT BY Q-BACK AND SKIP TO NEXT LINK */
		p_lanes->endsat_marker[laneIdx] = LANE_ENDSAT_QUEUE_OVER_X_DET;
		return mv_false;
	}

	if(get_det_on_time(det_num) >= DI_get_scan_period())
	{
		/* SKIP GAP-CHECKS IF DET 'ON' NO LESS THAN SCAN INTERVAL */
		return mv_true; /*goto S140;*/
	}

	/* Now do the gap checking using the X_DET (or eqv) */
	e = 0;
	if(get_max_lane_oversat_counter() > 1 && !is_oversat_link(linkIdx))
	{
		/* REDUCE CRITICAL GAPS IF SIGNIFICANT OVERSAT BUT NOT THIS LINK */
		e = -DI_get_reduced_critical_gap();
	}
	
	lanes_count = DI_get_link_main_lanes_count(linkIdx+1);

	if(timers.link_green_timer[linkIdx] < DI_get_time_to_use_comb_x(lane_num))
		lanes_count = DI_get_link_lanes_count(linkIdx+1);

    if(lanes_count > 1)
	{
		/* increase critical gaps if two main lanes on link */
		e += DI_get_additional_critical_gap();
	}
	
	if(get_det_off_time(det_num) >= calc_corrected_lane_critical_gap(lane_num)+e )
	{
		return finalize_x_det_checking(linkIdx, laneIdx);  //goto S80;
	}

	if(timers.link_green_timer[linkIdx] >= get_link_var_min_green_time(linkIdx)+40)
	{
		/* jump : don't use OLDGAP for checks just after min-green*/
		if( get_det_off_time(det_num) < 3 && 
			get_det_old_gap(det_num-1) >= calc_corrected_lane_critical_gap(lane_num)+e)
		{
			return finalize_x_det_checking(linkIdx, laneIdx); //goto S80;
		}
		
		/*jump to flag endsat if gap ended since last scan >= critical*/
		if(lanes_count <= 2)
		{
			/* SKIP DOUBLE-GAP CHECK ON 3-MAIN-LANE-LINKS. THEN, WITH ADDGAP,
				PROBABILITY OF GAP-OUT SIMILAR FOR 1,2,3 LANE LINKS	*/
			if(get_det_off_time(det_num) + get_det_old_gap(det_num-1) >=
				DI_get_satflow_mean_gap(lane_num) + calc_corrected_lane_critical_gap(lane_num) + e)
			{
				return finalize_x_det_checking(linkIdx, laneIdx); //goto S80;
			}
		}
	}

	/*CHECK IF 2-PART ES CHECK NEEDED. ELSE, FINISH*/
	if(p_lanes->det_fault_marker[laneIdx] < LANE_DET_FAULT_NO_PROBLEM) /*Equivalent to: LANE_DET_FAULT_NEED_2_PART_ENDSAT_CHECK*/
		return do_second_part_of_endsat_checking(linkIdx, laneIdx);
			
	return mv_true;
}


/*!
*	\brief	Finalize the endsat checking using the X-DET.
*
*	\param[in]	linkIdx	The link index that includes the lane.
*	\param[in]  laneIdx	The lane index in which its endsat to be checked.
*
*	\return		FALSE if no need to update/inspect the reset of the links, TRUE otherwise.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static mv_bool finalize_x_det_checking(uint8 linkIdx, uint8 laneIdx)
{	
	if(p_lanes->det_fault_marker[laneIdx] < LANE_DET_FAULT_NO_PROBLEM)  /*Equivalent to: LANE_DET_FAULT_NEED_2_PART_ENDSAT_CHECK*/
	{
		p_lanes->endsat_marker[laneIdx] -= 1;
		return do_second_part_of_endsat_checking(linkIdx, laneIdx);	//goto S90;
	}
	else
	{
		 p_lanes->endsat_marker[laneIdx] = LANE_ENDSAT_NORMAL;
		 return update_normal_link_endsat_marker_after_lanes_checking(linkIdx, laneIdx);
	}
}



/*!
*	\brief	Uses IN-DET to perform the endsat checking.
*
*	\param[in]	linkIdx	The link index that includes the lane.
*	\param[in]  laneIdx	The lane index in which its endsat to be checked.
*
*	\return		FALSE if no need to update/inspect the reset of the links, TRUE otherwise.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static mv_bool use_in_det_to_check_endsat(uint8 linkIdx, uint8 laneIdx)
{
	int32 red_count_in_det; /*n*/
	uint8 det_num; /*d*/
	uint8 lane_num = laneIdx + 1;

	/* calculations parameters */
	int32 b;
	int32 g;

	det_num = DI_get_alt_up_det_num(lane_num);
	red_count_in_det = p_lanes->red_count_in_det[laneIdx];

	/*ALLOW FOR ANOTHER VEHICLE IF LANE IS OVERSAT*/
	
    if(p_lanes->oversat_counter[laneIdx] > 0)
		red_count_in_det++;
	
	b = 0;

	if(timers.link_green_timer[linkIdx] >=  DI_get_moving_q_over_in_det(lane_num))
	{
		/* ALLOW FOR FREE-FLOWING VEHICLES AFTER MOVQIN TIME (IN 0.1 SEC) */
		b = 3 * DI_get_in_det_cruise_time(lane_num);
	}

	g = DI_get_startup_lost_time(lane_num) + red_count_in_det * DI_get_satinc_time(lane_num) - b;

	if(p_lanes->exit_red_count[laneIdx] + p_lanes->sink_red_count[laneIdx] <= 0)
	{
		/* JUMP IF LANE NEEDS 2-PART ES CHECK. ELSE, CHECK NORMAL LANE */
		if(g >= timers.link_green_timer[linkIdx])
			return mv_true;
		
        p_lanes->endsat_marker[laneIdx] = LANE_ENDSAT_NO_MORE_GREEN_NEEDED;
				
		return update_normal_link_endsat_marker_after_lanes_checking(linkIdx, laneIdx);
	}

	if(g <= timers.link_green_timer[linkIdx])
		p_lanes->endsat_marker[laneIdx]--;

	/* NOW CHECK LANE NEEDING 2-PART CHECK*/
	return do_second_part_of_endsat_checking(linkIdx, laneIdx);/*goto S100;*/
}


/*!
*	\brief	Calculates the threshold that will be used in the update of the endsat marker.
*
*	\param[in]	linkIdx	The link index.
*
*	\return		The threshold to be used in the endsat marker update.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
int32 calc_lanes_endsat_green_time_threshold(uint8 linkIdx)
{
	uint8 i; /* lanes loop variable */

	uint8 lane_num;
	uint8 link_num = linkIdx + 1;
	
	/* Calculations parameters*/
	int32	stlost_acc = 0; /*d = 0;*/
	int32	satinc_acc = 0; /*e = 0;*/
	int32	smflow_acc = 1; /*initialised by 1 to avoid div by zero */ /*f = 1;*/
		
	for(i=0; i<DI_get_link_lanes_count(link_num); i++) 
	{
		lane_num = DI_get_lane_num(link_num, i);

        /* Compact MOVA: do not assume absence of IN-detector means this isn't a long lane */
		if( !DI_is_long_lane(lane_num-1) && DI_get_short_link_marker(link_num) == 0)
			continue;

		stlost_acc += DI_get_startup_lost_time(lane_num);
        satinc_acc += DI_get_satinc_time(lane_num);
		smflow_acc += p_lanes->smoothed_flow[lane_num-1];
	}
          
    /* Further info added for divide error */
    p_divide_error( smflow_acc, 401,
				    "smflow[0] was %d, smflow[1] was %d, smflow[2] was %d, "
					"smflow[3] was %d, smflow[4] was %d, smflow[5] was %d",
					p_lanes->smoothed_flow[0], p_lanes->smoothed_flow[1], p_lanes->smoothed_flow[2],
					p_lanes->smoothed_flow[3], p_lanes->smoothed_flow[4],p_lanes->smoothed_flow[5] );

	/* Further info added for divide error */
    p_divide_error((int32)(3600/smflow_acc), 402,
					"smflow[0] was %d, smflow[1] was %d, smflow[2] was %d, "
					"smflow[3] was %d, smflow[4] was %d, smflow[5] was %d",
					p_lanes->smoothed_flow[0], p_lanes->smoothed_flow[1], p_lanes->smoothed_flow[2],
					p_lanes->smoothed_flow[3], p_lanes->smoothed_flow[4], p_lanes->smoothed_flow[5] );

	/* CONVERT SMFLOWS VEH/H/10 TO VEH THIS CYCLE. */
	smflow_acc = timers.cycle_time_timer[linkIdx]/(3600/smflow_acc);

	/* Further info added for divide error */
    p_divide_error(DI_get_link_main_lanes_count(link_num), 403, "link number was %d", (link_num));
	return (stlost_acc + smflow_acc * satinc_acc/DI_get_link_main_lanes_count(link_num))/DI_get_link_main_lanes_count(link_num);
}



/*!
*	\brief	Calculate the parameters (related to the links) that will be used in the Capacity Max algorithm.
*
*	\param[in]	linkIdx				The index of the link that contains this lane.
*	\param[in]	laneIdx				The index of the lane.
*	\param[in]	detNum				The detector number that will be used to get the gap values.
*	\param[out]	excessGap			The excess gap.
*	\param[out]	satflowHeadway		The satflow headway.
*	\param[out]	registerTime		The register time.
*	\param[out]	registerVeh			The register vehicle count.
*	\param[int]	offTime				The input detector off time.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void calc_lane_capacity_max_params(	uint8 linkIdx,
									uint8 laneIdx /*m-1*/,
									uint8 detNum,
									int32 * excessGap /*e*/,
									int32 * satflowHeadway /*c*/,
									int32 * registerVeh /*f*/,
									int32 * registerTime /*h*/,
									int32 offTime /*g*/)
{
	int32 i; /* loop variable */
	int32 old_gap;
	int32 x_det_cruise_time; /*a*/
	int32 in_det_cruise_time; /*b*/

	uint8 lane_num = laneIdx+1;
	
	/*  ALLOW FOR (RESIDUAL) GAP SINCE LAST SCAN*/
	if(get_det_old_gap(detNum-1) > offTime &&
		get_link_var_min_green_time(linkIdx)+40 <= timers.link_green_timer[linkIdx])
	{
		old_gap = get_det_old_gap(detNum-1);
	}
	else
		old_gap = offTime;

	/*ADD EXCESS GAP FOR LANE (CAN BE <0) TO TOTAL FOR LINK IN 'excess_gap' */
	/* FIND EXCESS GAP FROM LARGER OF PRESENT AND PREVIOUS GAPS */
	*excessGap += (old_gap - calc_corrected_lane_critical_gap(lane_num));

	/* SAVE SAT-FLOW HEADWAY(S) FOR LANE(S) ON LINK*/
	*satflowHeadway += DI_get_satinc_time(lane_num);

	/* NOW, COUNT VEHICLES IN THIS LANE'S REGISTER INTO 'registerVeh'*/

	
	/*Compact MOVA: long links might not have IN-dets in CMOVA - we must check for IN-det too */
	if(DI_get_short_link_marker(linkIdx+1) > 0 || DI_get_in_det_num(lane_num) <= 0)
		return;             

	/* ONLY USE UPPER REG. IN CASE ASSOC-DETS OR SINKS */
	x_det_cruise_time = DI_get_x_det_cruise_time(lane_num) +1;
    in_det_cruise_time = DI_get_in_det_cruise_time(lane_num) +1;
	
	/* NOTE: LONG-ON IN-DET WON'T PUT COUNT IN REGISTER. (SEE P2) */
	for(i=x_det_cruise_time-1; i<in_det_cruise_time; i++) 
	{
		*registerVeh += p_lanes->shift_register[laneIdx][i];
	}
	
	*registerTime += (in_det_cruise_time + 1 - x_det_cruise_time);
}

/*!
*	\brief	Calculates the . For M8.
*
*	\param[in]	laneNum				The index of the lane.
*
*	\return		The calculated lane critical gap.
*
*	\author		Islam Abdelhalim 
*	\date		12-01-2015 */
static int32 calc_corrected_lane_critical_gap(uint8 laneNum)
{
#ifdef M8_PREMATURE_ENDSAT
	if(timers.signal_state_timer > DI_get_moving_q_over_x_det(laneNum))
	{
		return (int32)(DI_get_lane_critical_gap(laneNum) * ((DI_get_lane_lengthened_critical_gap(laneNum)/100.0) +1));
	}
	else
	{
		return DI_get_lane_critical_gap(laneNum);
	}
#else
	return DI_get_lane_critical_gap(laneNum);
#endif	
}

#ifdef M8_RE_ESTABLISHMENT_OF_SATFLOW
/*!
*	\brief	Check if a specific lane has a platoon.
*
*	\param[in]	laneIdx the lanes index.
*
*	\return		TRUE if the lane has a platoon of vehs.
*
*	\author		Islam Abdelhalim 
*	\date		27-03-2014 */
mv_bool check_lane_has_platoon(uint8 laneIdx)
{
	uint8 x_det_num;
	int32 platoon_critical_gap;
	uint8 lane_num = laneIdx + 1;

	x_det_num = DI_get_x_det_num(lane_num);

	/* check if the X-DET is defined */
	if(x_det_num > 0)
	{
		if(is_det_suspected(x_det_num-1))
			return mv_false;

		platoon_critical_gap = (int32)(DI_get_lane_critical_gap(lane_num) * (DI_get_lane_portion_of_critical_gap(lane_num) / 100.0));
				
		if(get_det_off_time(x_det_num) < platoon_critical_gap &&
			get_det_old_gap(x_det_num-1) < platoon_critical_gap)
		{
			/* getting here means that there is a platoon of vehs in this lane*/

			/* resetting the lane's endsat marker as there is a platoon, 
				so the lane is not at endsat any more*/
			p_lanes->endsat_marker[laneIdx] = LINK_ENDSAT_NOT_YET;

			return mv_true;
		}
	}

	return mv_false; /*no platoon detected in this lane*/
}

#endif