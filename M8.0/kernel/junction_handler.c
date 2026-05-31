/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         junction_handler.c
*
*  TITLE:        Mova Kernel: Junction Handler
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

#include "dataset_interface.h"
#include "dynamic_data.h"

#include "junction_handler.h"
#include "links_handler.h"
#include "links_endsat_handler.h"
#include "links_ep_handler.h"
#include "lanes_handler.h"
#include "detectors_handler.h"
#include "stages_handler.h"
#include "stages_ep_handler.h"
#include "stages_codes_handler.h"

#include "algorithms_manager.h"
#include "messages_manager.h"
#include "timers_manager.h"
#include "core_interface.h"

#include "tma_alerts.h"

#include "kdebug.h"

#include <stdio.h>
#include <stdlib.h> /* PCMOVA, embedded too? For abs() */

STATIC struct Junction	junction;

static void recalc_junction_total_lambda();
static mv_bool update_junction_next_stage();
static void update_ped_marker_and_timer(mv_bool isInStartofIG);
static void update_var_min_green_time(uint8 linkIdx);

#ifdef 	M8_RT_OPTIMISATION
static void reset_gaps_data();
#endif
void init_junction_handler()
{
	CI_set_junction_pointer(&junction);
	ALG_set_junction_pointer(&junction);
	MSG_set_junction_pointer(&junction);
}

void init_junction_dynamic_data()
{
	/* set junction dynamic data initialisation values*/
	junction.warmup_counter = -1;
	junction.last_stage = MAX_STAGES;
	junction.next_stage = MAX_STAGES;
	junction.total_smoothed_flow = 75;
	junction.smoothed_cycle_time = 990;
	junction.total_lambda = 75;

#ifdef M8_IMPROVED_LINKING
	junction.is_to_do_backward_linking = mv_true;
	
	junction.is_to_send_backward_hold_signal = mv_false;
	junction.is_to_send_backward_release_signal = mv_false;

	junction.forward_link_delay = 0; 
#endif

}


/*!
*	\brief	Handles all the required updates that to be performed at the begining of each scan.
*
*	\param[in]	isInStartOfIG	A flag that determines if this function has been 
*								called during the start of an IG.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
void handle_junction_scan_initial_updates(mv_bool isInStartOfIG)
{
	uint8 m; /*lanes loop variable */
	
	/*1: Update detectors data*/
	update_dets_data();

	/*2: Update current/previous stage values*/
	junction.previous_stage = (uint8)junction.current_stage;
	junction.current_stage = (uint8)input_data.current_stage;

	/*3: Update pedestrian data*/
	update_ped_marker_and_timer(isInStartOfIG);

	/*4: Update ep links data */
	update_ep_links_data();

	/*5: Update timers */
	update_timers();

	/*6: Update RED count in for all the lanes */
	for (m = 0; m < DI_get_lanes_count() ; m++)
	{
		update_lane_shift_register_and_red_counts(m);
		
		ALG_set_algorithm_params(ALG_ESTIMATE_QUEUES_SIZE);
		ALG_estimate_queue_beyond_in_det(m+1, get_lanes_extra_veh_beyond_in_det());

		correct_lane_red_count(m);
	}
}



/*!
*	\brief	Updates the oversat flags for all the junction's lanes and then calls the 
*			Calculate Reduced Lost Time algorithm.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
void handle_junction_oversat_updates()
{
	uint8 l; /* links loop variable */
	uint8 k; /* lanes loop variable */

	junction.is_oversat_changed = mv_false;
	junction.is_oversat_increased = mv_false;		

	reset_stages_oversat_max_marker();

	for (l = 0; l < DI_get_links_count() ; l++) 
	{
		/* skip ped and e/p links as they can't be o'sat */
		if ( !DI_is_traffic_link(l+1) )
			continue;

		for (k = 0; k < DI_get_link_lanes_count(l+1); k++)
		{
			update_lane_oversat_counters(l, DI_get_lane_num(l+1, k) - 1);
		}
	}

	if(junction.is_oversat_changed)					
	{
		ALG_set_algorithm_params(ALG_CALC_REDUCED_LOST_TIME);
		ALG_calculate_reduced_lost_time(get_links_reduced_lost_time());
	}
}


/*!
*	\brief	Updates all the junction's entities if have bonus situation.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
void handle_junction_bonus_situation()
{
	mv_bool is_link_bonus_changed = mv_false;

	ALG_set_algorithm_params(ALG_CALC_BONUS_GREEN_TIME);
	is_link_bonus_changed = ALG_calculate_bonus_green_time(	get_links_total_bonus_green_time(),
															get_links_bonus_stage(),
															get_links_endsat_marker());


	/* Apply if a change happened in the oversaturation (set in P5) or stage bonus (set in P7)*/
	if(junction.is_oversat_changed || is_link_bonus_changed)
	{
		/* CHECK IF JUNCTION NO LONGER OVERSAT. ELSE, RESET LOW DRIFT MAX'S ETC */
		if (get_junction_oversat_counter() <= 1)
		{
			junction.cut_max_times_marker = CUTMAX_DISABLE_SHORT_CYCLE_CONTROL;
			return;
		}

		set_stages_bonus_time_and_link();
	}

	/* Check if already short-cycling. Else, see if needed */
	if (junction.cut_max_times_marker <= CUTMAX_DISABLE_SHORT_CYCLE_CONTROL)
	{
		/* Skip if no changes in o'sat or bonuses to require short cycling */
		if(junction.is_oversat_changed || junction.is_oversat_increased || is_link_bonus_changed)
		{
			ALG_set_algorithm_params(ALG_CALC_SHORT_CYCLE_CONTROL_PARAMS);
			if(ALG_calculate_short_cycle_control_params( &junction.low_total_green,
															get_stages_low_drift_max()))
			{
				if (junction.low_total_green <= DI_get_total_green_time())
				{
					junction.cut_max_times_marker = DI_get_stages_count();
				}
				else // ignore short cycle if exceeds user-specified limit 
				{
					/*ZERO CUTMAX TO END SHORT-CYCLING WHEN JUNCTION NO LONGER O'SAT*/
					junction.cut_max_times_marker = CUTMAX_DISABLE_SHORT_CYCLE_CONTROL;
				}
			}
		}
	}
}

/* the function */
/*!
*	\brief	Mainly resets/initializes several data elements of the junction and stages when the
*			end of a stage is reached.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
void handle_junction_end_of_stage_updates()
{
	int32 ped_max_1;
	int32 ped_max_2;

	
	if(!is_in_initialisation()) /* finished initializing data but may be still in the warmup */
	{
		update_skipped_stages_data_at_endstage();
		
		/* Skip lambda update if already done or in warmup */
		if(timers.stage_cycle_timer[get_previous_stage()-1] > 0 && !is_in_warmup())
		{
			ped_max_1 = DI_get_ped_max_1( get_previous_stage() );
			ped_max_2 = DI_get_ped_max_2( get_previous_stage() );

#ifdef M8_UPDATE_LAMBDA_IN_PEDMAX_CYCLES
			if( junction.ped_demand_marker <= 0 
				||	(ped_max_2 == 100 && get_junction_oversat_counter() >= 2)
				||	timers.signal_state_timer < calc_stage_revised_max_green_time(ped_max_2, ped_max_1,  get_previous_stage(), mv_true))
#endif
			{
#ifdef M8_LAMBDA_IN_PFACIL_EQ_4
				if(!junction.is_to_truncate_current_stage)
#endif
				{
					update_stage_lambdas( get_previous_stage()-1 ); 
				}
			}
		}

#ifdef M8_RT_OPTIMISATION
		if(junction.net_gaps_count > 0 && get_next_stage() > 0)
			update_opposed_link_red_x_counts(junction.net_gaps_count, junction.net_gaps);
#endif

	}
	
	TMA_check_alert_exiting(pTmaData, junction.previous_stage-1, get_lanes_endsat_marker());

	timers.stage_cycle_timer[get_previous_stage()-1] = 0;

	recalc_junction_total_lambda();

	reset_stages_endsat_marker();

	if(!is_in_warmup()) /*warming up finished*/
	{
		calc_stages_drift_max();

		update_links_ep_indicators();
		reset_ep_links_cancel_marker();		
	}

	reset_previous_stage_demand_markers();

	junction.last_stage = junction.previous_stage;
	junction.var_min_green_time = 0;

#ifdef 	M8_RT_OPTIMISATION
	reset_gaps_data();
#endif

	/*Reset timers*/
	timers.signal_state_timer = DI_get_scan_period();
	timers.wait_stage_change_timer = 0;
	timers.stage_max_green_timer = 0;		
	timers.ep_ext_max_timer = 0;
}

/*!
*	\brief	Updates the junction's links data when the intergreen is reached.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
void handle_junction_intergreen_updates()
{
	update_links_at_intergreen();
}


/*!
*	\brief	Updates the junction's lanes oversat max counter.
*
*	\param[in]	laneIdx	The index of the lane to get its oversat counter.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
void update_lane_oversat_counter_max(uint8 laneIdx)
{
	if( get_lane_oversat_counter(laneIdx) > junction.max_lane_oversat_counter )
	{
		 junction.max_lane_oversat_counter = get_lane_oversat_counter(laneIdx);
	}
}


/*!
*	\brief	Updates the junction and its links variable min green time.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
void handle_junction_green_stage_updates()
{
	uint8 l; /* links loop variable*/

	junction.var_min_green_time =  DI_get_stage_min_green_time( junction.current_stage );
	
	for (l=0; l < DI_get_links_count(); l++)
	{
		if(reset_link_timers_and_markers_at_stage_green(l))
		{
			update_link_var_min_green(l);

			update_var_min_green_time(l);
		}
	} 
}

/*!
*	\brief	Updates the junction var_min_green based on the updated link var_min_green.
*
*	\param[in]	linkIdx	The index of the link to be updated.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
static void update_var_min_green_time(uint8 linkIdx)
{
	int32 var_min_green;

	/* If only green stage for this link is current stage
	* set stage min-green as max of mins for single-green links*/

	if(DI_get_link_green_status( get_current_stage(), linkIdx+1) == LINK_STATUS_GREEN_IN_ONE_STAGE)
	{
		var_min_green = get_link_var_min_green_time(linkIdx) - (timers.link_green_timer[linkIdx] - timers.signal_state_timer);
		
		if (var_min_green > junction.var_min_green_time)
			junction.var_min_green_time = var_min_green;
	}
}


/*!
*	\brief	Updates all the junction's links types demands (including e/p and ped links).
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
void handle_junction_demands_updates()
{
	uint8 l; /* links loop variable */
	uint8 s; /* stages loop variable */

	/*Demands initialisation*/
	junction.is_ped_demand_present = mv_false;

#ifdef M8_IMPORVED_BUS_PRIORITY
	junction.is_priority_demand_present = mv_false;
#endif

	/* First, update the links demand */
	for (l = 0; l < DI_get_links_count() ; l++) 
	{

		/* 1: If normal traffic link */
		if (DI_is_traffic_link(l+1))
		{
			update_traffic_link_demand(l);
			update_special_link_demand(l);
		}
		/* 2: If e/p link */
		else if (DI_is_ep_link(l+1))
		{
			update_ep_link_demand(l);
		}
		else
		/* 3: If pedestrian link */
		{
			update_ped_link_demand(l);
		}
	}


	/* Second, update the stage demand*/
	for (s=0; s < DI_get_stages_count(); s++)
	{
		if(update_stage_demand(s) == 4)
		{
			/* 6MRR - skip next 3 stages as already done */
			s+= 3;
		}
	}

}


/*!
*	\brief	Handles the updates related to the next stage. this include the endsat marker.
*
*	\param[out]	isVarMinGreenEXpired	A flag to determine if the var-min-green has expired or not.
*
*	\return		FALSE if in absolute-min-green and thus the max green expiry checking is not required.
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
mv_bool handle_junction_next_stage_updates(mv_bool * isVarMinGreenEXpired)
{
	if( update_junction_next_stage() )
	{
		/*Getting here means that the next stage will be different */

		/* Increment stage timer for comparison with max-green */
		timers.stage_max_green_timer += DI_get_scan_period();

		if(!is_in_abs_min_green())
		{
			junction.ep_hold_marker = NO_EP_HOLD_PRESENT;

			if(get_stage_endsat_marker(get_next_stage()-1) == STAGE_ENDSAT_REACHED_FOR_ALL_REL_LINKS ||
				get_stage_endsat_marker(get_next_stage()-1) == STAGE_ENDSAT_VAR_MIN_GREEN_OR_EP_HOLD_EXPIRED)
			{
				*isVarMinGreenEXpired = mv_true;
				return mv_true;
			}
			else
			{
				return update_next_stage_endsat_marker_part1(isVarMinGreenEXpired);
			}
		}
	}

	return mv_false;
}


/*!
*	\brief	Determines the next stage to be activated.
*
*	\return		FALSE if no need to continue the rest of the CONTINUED_INTERGREEN handling
*				in the program_controller, TRUE otherwise.
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
static mv_bool update_junction_next_stage()
{
	uint8 next_stage;

	/*Check the emergency stage first*/
	next_stage = get_emergency_next_stage();

	if (next_stage > 0)
	{
		junction.next_emeregency_stage_to_demand = next_stage;
		junction.next_priority_stage_to_demand = 0;
	}
	else
	{
		/* No emergency demand, check priority demand next */
		junction.next_emeregency_stage_to_demand = 0;

		next_stage = get_priority_next_stage();

		if (next_stage > 0)
		{
			junction.next_priority_stage_to_demand = next_stage;
		}
		else
		{
			/* No priority demand either*/
			junction.next_priority_stage_to_demand = 0;

			/* Get normal demand */
			next_stage = decide_next_stage();

			/* if no demands for any stage just return (MARKER = 2,3 as on entry) */
			if (next_stage == 0)
				return mv_false; /*Should exit the whole block!*/
		}
	}

	junction.previous_next_stage = junction.next_stage;
	junction.next_stage = next_stage;

	return mv_true;
}


/*!
*	\brief	Performs all the needed calculations/checking to determine if the stage max green has expired 
*			or not.
*
*	\param[in]	isInMinGreen	A flag to determin if the program is in absolute or variable min green.
*
*	\return		TRUE if the stage max green is reached (expired).
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
mv_bool is_junction_max_green_expired(mv_bool isInMinGreen)
{
	int32 max; /* holds MAX or LOWMAX green time*/ /*b*/
	int32 drift; /* holds DRIFMX or LODFMX */ /*d*/
	int32 total_green; /* holds TOTALG or LOTOTG */ /*g*/
	
	int32 temp_max; /* holds MAX or LOWMAX green time*/ /*b*/

	junction.extra_green_for_drift_max = 0;

	if (junction.cut_max_times_marker == CUTMAX_DISABLE_SHORT_CYCLE_CONTROL)
	{
		max = DI_get_stage_max_green_time( get_current_stage() );
		drift = get_stage_drift_max( get_current_stage()-1 );
		total_green = DI_get_total_green_time();
	}
	else
	{
		max = DI_get_stage_low_max_green_time( get_current_stage() );
		drift = get_stage_low_drift_max( get_current_stage()-1 );
		total_green = junction.low_total_green;
	}

	if (junction.ped_demand_marker != 0 && DI_get_ped_max_2(get_current_stage()) != 999) 
	{		
		max = calc_stage_revised_max_green_time( DI_get_ped_max_2(get_current_stage()), DI_get_ped_max_1(get_current_stage()), get_current_stage(), mv_true);
		junction.ped_max_green_time = max; /* this is just for a message */

		drift = max;
	}

#ifdef M8_IMPORVED_BUS_PRIORITY
	if (junction.is_priority_demand_present &&
		DI_get_bus_max_2(get_current_stage()) != 999) 
	{		
		temp_max = calc_stage_revised_max_green_time( DI_get_bus_max_2(get_current_stage()), DI_get_bus_max_1(get_current_stage()), get_current_stage(), mv_false);

		if(temp_max != SKIP_MAX_GREEN_CALC)
		{
			max = temp_max;
			drift = temp_max;
		}
	}
#endif
		

	if(timers.stage_max_green_timer >= max)
	{
		/* max green reached */
		return mv_true;
	}
	else
	{
		/* Check drift-max */
		if (DI_get_stage_min_green_time(get_current_stage()) + 30  > timers.signal_state_timer
			&& isInMinGreen)
		{
			/* don't allow drift-max to cut stage if within 3 sec of absolute stage min */
		}
		else if (timers.stage_max_green_timer < drift)
		{
			/* drift-max > stage max */
		}
		else
		{
			junction.extra_green_for_drift_max = calc_stages_extra_green_for_drift_max( total_green );

			max = drift + junction.extra_green_for_drift_max;

			if (timers.stage_max_green_timer >= max)
			{
				return mv_true;
			}
		}
	}

	return mv_false;

}

static void recalc_junction_total_lambda()
{
	uint8 s; /* stage loop variable */

	junction.total_lambda = 0;

	for (s=0; s<DI_get_stages_count(); s++)
	{
		junction.total_lambda += get_stage_lambda(s);
	}

	/*KDEBUG_WRITE1(DEBUG_LEVEL_VERBOSE, CORE, "Updated state for all the links ENDSAT marker: %s", KDebugToString(DI_get_links_count(), get_links_endsat_marker() ));*/

}


/*!
*	\brief	Updates the stage and links (and thus lanes) endsat markers. It also updates the links benigiting marker.
*
*	\param[in]	isInCheckingEndsatOnwards	Flag to determin if the program_controller is 
*											in Checking Endsat state or any state after it.
*
*	\return		TRUE if prog.marker to be set with PROG_MARKER_DELAY_AND_STOPS_OPTI in the program_controller.
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
mv_bool handle_junction_endsat_checking(mv_bool isInCheckingEndsatOnwards)
{
	uint8 l; /* links loop variable */

	/* FIRST, do the links >> lanes */
    for(l=0; l<DI_get_links_count(); l++) 
	{
		/* Skip if ped or ep link, or if signal red, or link already end-sat */
		if(!DI_is_traffic_link(l+1))
			continue;

		if(get_link_green_flag(l) == LINK_FLAG_NOT_GREEN ||
		   get_link_endsat_marker(l) > LINK_ENDSAT_NOT_YET)
			continue;

		/* SKIP LINK IF VAR-MIN-GREEN NOT YET SET OR EXPIRED */
		if(get_link_var_min_green_time(l) == 0 ||
		   get_link_var_min_green_time(l) > timers.link_green_timer[l])
 			 continue;

		update_link_endsat_marker(l);
    } 

	/*KDEBUG_WRITE1(DEBUG_LEVEL_VERBOSE, CORE, "Updated state for all the links ENDSAT marker: %s", KDebugToString(DI_get_links_count(), get_links_endsat_marker() ));*/


	/* SECOND, do the stage*/
	/* Making sure that the program is not in IG or absolute-min-green OR
	var-min-green*/
	if( isInCheckingEndsatOnwards )
	{
		/*Getting here means that the program is in PROG_MARKER_CHECKING_ENDSAT onwards, so lets see
		if we can mova to PROG_MARKER_DELAY_AND_STOPS_OPTI or not*/
		
		update_links_benefiting_marker(); /*links relevance: RELEVANT or IRRELEVANT*/


		/* Check if the next stage endsat marker already set*/
		if( get_stage_endsat_marker(get_next_stage()-1) > STAGE_ENDSAT_NOT_YET)
		{
			return mv_true; /* to set prog.marker with PROG_MARKER_DELAY_AND_STOPS_OPTI */
		}
		else
		{
			return update_next_stage_endsat_marker_part2();
		}
	}

	return mv_false;
}



void handle_junction_average_flow_update()
{
	update_lanes_average_flow();
}

void handle_junction_stage_changing()
{
	junction.demanded_stage = junction.next_stage;  /* set up required new stage in DEMSTA */
	timers.wait_stage_change_timer = DI_get_scan_period(); /* notes that MOVA waiting for change */
	
#ifdef M8_IMPROVED_LINKING
	junction.is_to_do_backward_linking = mv_true;
#endif

	KDEBUG_WRITE1(DEBUG_LEVEL_VERBOSE, CORE, "The demanded stage is: %d", junction.demanded_stage );
}



/*!
*	\brief	Updates ped marker and timers for the junction.
*
*	\param[in]	isInStartOfIG	A flag that determines if this function has been 
*								called during the start of an IG.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
static void update_ped_marker_and_timer(mv_bool isInStartofIG)
{
	static uint8 l_current_stage = 0;
	static mv_bool check_marker = mv_true;
	static mv_bool count_down = mv_true;
	static int32 timer = 10;

	if (junction.ped_demand_marker > 0 )
	{
		if ((junction.ped_demand_marker == DI_get_stages_count() && !junction.is_ped_demand_present) || 
			(junction.ped_demand_marker < DI_get_stages_count() && get_current_stage() == l_current_stage))  
		{
        /* ped demand no longer present - can stop ped cycle only if still in stage in 
           which demand appeared. Otherwise need to complete cycle
           or
           now back to stage in which demand registered */
			junction.ped_demand_marker = 0;
		}
        else
		{  /* ped demand present, decrement if stage just changed */
			if (isInStartofIG)
			{
				if (check_marker == mv_true)
				{
					check_marker = mv_false;
					junction.ped_demand_marker--;
				}
            }
            else
               check_marker = mv_true;
		}
	}
	else 
	{
		if (junction.is_ped_demand_present)
		{  /* ped demand just come in */
			if (count_down)
			{
                /* decrement nTimer and reset countdown if nTimer 
                   down to 0 (ie 5s elapsed since ped demand first seen) */
				if (timer-- == 0)
					count_down = mv_false;
            }
            else
            { 
				junction.ped_demand_marker = DI_get_stages_count();
				l_current_stage = get_current_stage();
            }
		}
        else 
        {  /* no ped demand */
			count_down = mv_true;  /* set so that next time ped demanded, countdown activated */
			timer = 10;        /* set to count down for 5 seconds */
			junction.ped_demand_marker = 0;           /* reset ped_demand_marker */
		}
    }

	/* Done with the ped_demand_marker, now update the ped_wait_timer */
	if (junction.ped_demand_marker> 0) 
		timers.ped_wait_timer++;
	else
		timers.ped_wait_timer = 0;
}


/*!
*	\brief	Updates the e/p extention timer for all the junction's e/p links.
*			It also initiates the nTT code processing.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
void handle_junction_ep_ext_updates()
{
	uint8 l; /*links loop variable*/

	for (l=0; l<DI_get_links_count(); l++) 
	{
		if(DI_is_ep_link(l+1))
			update_ep_link_ext_timer(l);
	}
}


/*!
*	\brief	Updates the junction's e/p timers and markers.
*
*	\param[in]	isEP_MaxGreenExpired	A flag that determines if the e/p max green expired it not.
*
*	\return		A value that tells the program_controller if e/p change is required or not.
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
uint8 handle_junction_ep_timers_and_markers_updates(mv_bool isEP_MaxGreenExpired)
{
	reset_ep_stage_ext_timers();
	
	/* increment e/p stage max timer only if other max not yet reached */
    if (isEP_MaxGreenExpired)
	{
		if(timers.emergency_stage_ext_timer > 0 || timers.priority_stage_ext_timer > 0)
			timers.ep_ext_max_timer += DI_get_scan_period();
	}

	/* Clear emergency/priority extension marker */
    junction.ep_ext_marker = NO_EP_EXT_PRESENT;

	/* if emergency hold exists, return, leaving MARKER=3 or 4 */
	if (junction.ep_hold_marker == EMERGENCY_HOLD_PRESENT)
		return RET_NO_EP_CHANGE_REQUIRED;

	if (timers.emergency_stage_ext_timer != 0)
	{
		if (!isEP_MaxGreenExpired ||
			 timers.ep_ext_max_timer < DI_get_emergency_ext_max( get_current_stage()))
		{
			/* if emergency extn exists and has not reached max, can't override */
			junction.ep_ext_marker = EMERGENCY_EXT_PRESENT;
			return RET_NO_EP_CHANGE_REQUIRED;   /* >>>> leave marker=3,4 >>>>>*/
		}
	}

	if (junction.next_emeregency_stage_to_demand <= 0)
	{
		/* No emergency change needed.  Check priority holds next */
		if (junction.ep_hold_marker == PRIORITY_HOLD_PRESENT)
		{
			/* Return if priority hold prevents any non-emergency change */
			return RET_NO_EP_CHANGE_REQUIRED; /* >>>>> leave marker=3 >>>>>>*/
		}

		if (timers.priority_stage_ext_timer != 0)
		{
			if (!isEP_MaxGreenExpired ||
				timers.ep_ext_max_timer < DI_get_priority_ext_max( get_current_stage()) )
			{
				/* priority extn exists and has not reached max */
				junction.ep_ext_marker = PRIORITY_EXT_PRESENT;
				return RET_NO_EP_CHANGE_REQUIRED;  /* >>>>> leave marker=3,4 >>>>>*/
			}
		}

		if (junction.next_priority_stage_to_demand <= 0 || !junction.is_to_truncate_current_stage)
		{
			return RET_NO_PRIORITY_CHANGE_OR_TRUNC_NEEDED;  /* >>>>> marker=3,4,7 >>>>> */
		}
	}	

	update_links_ep_trunc_indicator();
	
	return RET_EP_CHANGE_REQUIRED;
}

#ifdef M8_RE_ESTABLISHMENT_OF_SATFLOW
/*!
*	\brief	Checks if a platoon exists in the junction.
*
*	\return		TRUE if any GREEN link has a platoon of vehs.
*
*	\author		Islam Abdelhalim 
*	\date		27-03-2014 */
mv_bool handle_junction_platoon_checking()
{
	uint8 l; /* links loop variable */

	
    for(l=0; l<DI_get_links_count(); l++) 
	{
		/* Skip if ped or ep link, or if signal red, or link already end-sat */
		if(!DI_is_traffic_link(l+1))
			continue;

		/* skip if the link is not GREEN now or not in a normal endsat (unsaturated)*/
		if(get_link_green_flag(l) == LINK_FLAG_NOT_GREEN ||
			get_link_endsat_marker(l) != LINK_ENDSAT_NORMAL)
			continue;

		if(check_link_has_platoon(l))
		{
			/*Also we need to check if all the links that will be RED in next stage (relevant links)
			have reached endsat */
			if(get_stage_endsat_marker( get_next_stage()-1 ) ==  STAGE_ENDSAT_REACHED_FOR_ALL_REL_LINKS)
				return mv_true;
		}
    } 

	return mv_false;
}
#endif


#ifdef M8_IMPROVED_LINKING
void handle_junction_linked_mova_operation()
{
	uint8 l; /* links loop variable */
		

	/*Note: The linking_time_offset is not a local var because it needs to be read in the messages.*/
	junction.linking_time_offset = 0; 
	
	if(get_current_stage() == 0)
	{
		printf("In INTER GREEN .... will not do the calcs of the offset \n");
	}

	//if(get_current_stage() != 0) /*  Skip the calcs if in InterGreen*/
	if(get_current_stage() == 1) /*  Do Forward/Backward linking calcs during Stage 1 only.*/
	{
		/* Check if we need to do Backward or Forward linking?*/
		for(l=0; l<DI_get_links_count(); l++) 
		{
			/* Skip if ped or ep link, or if signal red, or link already end-sat */
			if(!DI_is_traffic_link(l+1))
				continue;

			/*Calculate the time offset only if the relevant link is RED.*/
			//if(DI_get_link_green_status(junction.current_stage, l+1) == LINK_STATUS_RED)
			if(get_link_green_flag(l) == LINK_FLAG_NOT_GREEN)
			{
				if(decide_forward_or_backward_linking(l, &junction.linking_time_offset) == FORWARD_LINK)
				{
					/* Receive Stage Confirm signal from the upstream controller after a delay = O-IPSMIG*/
					junction.forward_link_delay = junction.linking_time_offset - DI_get_upstream_junction_intergreen();

					printf("Setting the forward link delay to: %ld \n", junction.forward_link_delay);

					/*Reset all the Backward link vars*/
					timers.backward_hold_timer =  0; 
					junction.is_to_send_backward_hold_signal = mv_false;
					junction.is_to_send_backward_release_signal = mv_false;
				}
				else /*BACKWARD_LINK*/
				{
					if(junction.is_to_do_backward_linking == mv_true)
					{
						/* Set the HOLD timer to keep the signal for: abs(UPSMIG - offset) + 1 */ 
						junction.backward_hold_timer_limit =  abs(DI_get_upstream_junction_intergreen() - junction.linking_time_offset) + 1; 

						printf("Setting the backward link Hold timer limit to: %ld \n", junction.backward_hold_timer_limit);

						/*This will cause sending the HOLD signal to the upstream controller. */
						junction.is_to_send_backward_hold_signal = mv_true;
						junction.is_to_send_backward_release_signal = mv_false;

						/*Reset the forward link delay to avoid applying it.*/
						junction.forward_link_delay = 0;
					}
					else /*Do not perform a backward linking if the RELEASE signal was raised.*/
					{
						junction.is_to_send_backward_hold_signal = mv_false;
						junction.is_to_send_backward_release_signal = mv_false;
					}
				}

				/*As there will be only one instance in the controller. We exit from the loop here. Otherwise, 
				the' junction.backward_hold_timer_limit' value might be overridden.*/
				break; 
			}
		}
	}
	
	/*CHECK: The following condition is just a hack for testing.*/
	/*if(get_current_stage() == 2)
	{
		junction.is_to_send_backward_hold_signal = mv_false;
		junction.is_to_send_backward_release_signal = mv_false;
	}
	else //Stage 1 and IG
	{*/

		/*The following condition is just to turn the Release signal off after one cycle.*/
		if(junction.is_to_send_backward_hold_signal == mv_false)
		{
			junction.is_to_send_backward_release_signal = mv_false;
		}

		/*Check if the backward HOLD timer has been set.*/
		if(junction.is_to_send_backward_hold_signal == mv_true)
		{
			/*Check if the HOLD timer reached its limit.*/
			if(timers.backward_hold_timer >= junction.backward_hold_timer_limit)
			{
				timers.backward_hold_timer = 0;

				junction.is_to_send_backward_hold_signal = mv_false;
				junction.is_to_send_backward_release_signal = mv_true;

				junction.is_to_do_backward_linking  = mv_false;
			}
			else /* The timer is not expired yet.*/
			{
				/* The following condition postpone the decrementing of the backward Hold timer until the start of the IG.*/
				//if(get_current_stage() == 0 && (get_previous_stage() == 1 || get_previous_stage() == 0) )
				if(get_current_stage() == 0 || timers.backward_hold_timer > 0 )
				{
					/*Increment the HOLD timer.*/
					timers.backward_hold_timer += DI_get_scan_period();
					printf("Current value of the backward Hold timer is: %ld \n", timers.backward_hold_timer);
				}
				else
				{
					printf("Wait till the start of the IG to increment the backward Hold timer (Limit=%ld).\n", junction.backward_hold_timer_limit);
				}

			}
		}
	//}

}
#endif


/*!
*	\brief	Processing the NTT codes.
*
*	\author		Islam Abdelhalim 
*	\date		01-06-2015 */
void handle_junction_NTT_code()
{
	process_NTT_code_for_ep_links();
}

/*----------------------------------------------------------------*/
/*------------------------- Getters ------------------------------*/
/*----------------------------------------------------------------*/
int32 get_junction_oversat_counter()
{
	return junction.oversat_counter;
}

int8 get_junction_cut_max_times_marker()
{
	return junction.cut_max_times_marker;
}

uint8 get_current_stage()
{
	return junction.current_stage;
}

uint8 get_previous_stage()
{
	return junction.previous_stage;
}

uint8 get_last_stage()
{
	return junction.last_stage;
}

int32 get_junction_smoothed_cycle_time()
{
	return junction.smoothed_cycle_time;
}

int32 get_junction_total_lambda()
{
	return junction.total_lambda;
}

uint8 get_next_stage()
{
	return junction.next_stage;
}

int32 get_var_min_green_time()
{
	return junction.var_min_green_time;
}

int32 get_max_lane_oversat_counter() /*laosmx*/
{
	return junction.max_lane_oversat_counter;
}

mv_bool is_in_warmup()
{
	return junction.warmup_counter <= DI_get_stages_count();
}

void inc_warmup_counter()
{
	junction.warmup_counter++; 
}

mv_bool is_in_initialisation()
{
	return junction.warmup_counter <= 0;
}

uint8 get_demanded_stage()
{
	return junction.demanded_stage;
}

uint8 get_next_emeregency_stage_to_demand()
{
	return junction.next_emeregency_stage_to_demand;
}

uint8 get_next_priority_stage_to_demand()
{
	return junction.next_priority_stage_to_demand;
}

#ifdef M8_IMPROVED_LINKING
int32 get_forward_link_delay()
{
	return junction.forward_link_delay; 
}
#endif


mv_bool is_in_abs_min_green()
{
	return junction.var_min_green_time == 0;
}

mv_bool is_ep_hold_or_ext_present()
{
	return	junction.ep_hold_marker > NO_EP_HOLD_PRESENT ||
			junction.ep_ext_marker > NO_EP_EXT_PRESENT;	
}


/*----------------------------------------------------------------*/
/*------------------------- Resetters -----------------------------*/
/*----------------------------------------------------------------*/

void reset_max_lane_oversat_counter()
{
	junction.max_lane_oversat_counter = 0;
}

#ifdef 	M8_RT_OPTIMISATION
static void reset_gaps_data()
{
	uint8 i;

	for(i=0; i<junction.net_gaps_count; i++)
	{
		junction.net_gaps[i] = 0;
	}

	junction.net_gaps_count = 0;
}
#endif

/* Incrementers / Decrementers */
void inc_junction_total_smoothed_flow(int32 incBy)
{
	junction.total_smoothed_flow += incBy;
}

void inc_junction_oversat_counter(int32 incrementValue)
{
	junction.oversat_counter += incrementValue;
}

void dec_junction_oversat_counter(int32 decrementValue)
{
	junction.oversat_counter -= decrementValue;
}




/*----------------------------------------------------------------*/
/*------------------------- Setters ------------------------------*/
/*----------------------------------------------------------------*/

void set_junction_cut_max_times_marker(int8 markerVal)
{
	junction.cut_max_times_marker = markerVal;
}

void set_is_oversat_changed(mv_bool val)
{
	junction.is_oversat_changed = val;
}

void set_is_oversat_increased(mv_bool val)
{
	junction.is_oversat_increased = val;
}

void set_junction_smoothed_cycle_time(int32 val)
{
	junction.smoothed_cycle_time = val;
}

void set_junction_total_smoothed_flow(int32 val)
{
	junction.total_smoothed_flow = val;
}

void set_ped_demand_present(mv_bool val)
{
	junction.is_ped_demand_present = val;
}

void set_is_to_truncate_current_stage(mv_bool val)
{
	junction.is_to_truncate_current_stage = val;
}

void set_ep_hold_marker(int8 val)
{
	junction.ep_hold_marker = val;
}

#ifdef M8_IMPORVED_BUS_PRIORITY
void set_is_priority_demand_present(mv_bool val)
{
	junction.is_priority_demand_present = val;
}
#endif