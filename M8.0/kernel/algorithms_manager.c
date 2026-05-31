/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         algorithms_manager.c
*
*  TITLE:        Mova Kernel: Algorithms Manager
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

#include "algorithms_manager.h"
#include "mova_constants.h"
#include "timers_manager.h"

#include "detectors_handler.h"

static struct Junction * p_junction;
static struct Stages * p_stages;
static struct Links * p_links;
static struct Lanes * p_lanes;
static struct Detectors * p_dets;



void ALG_set_junction_pointer(struct Junction * junRef)
{
	p_junction = junRef;
}

void ALG_set_stages_pointer(struct Stages * stagesRef)
{
	p_stages = stagesRef;
}

void ALG_set_links_pointer(struct Links * linksRef)
{
	p_links = linksRef;
}

void ALG_set_lanes_pointer(struct Lanes * lanesRef)
{
	p_lanes = lanesRef;
}

void ALG_set_dets_pointer(struct Detectors * detsRef)
{
	p_dets = detsRef;
}


/*!
*	\brief	Sets the parameters required for each algorithm.
*
*	\param[in]	algID	The ID of the algorithm to set its parameters. This can be one of the values
*						defined in mova_constants.h (ALG_...).
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void ALG_set_algorithm_params(uint8 algID)
{
	switch(algID)
	{

	case ALG_CALC_REDUCED_LOST_TIME:
		calcReducedLostTimeAlgInput.lambda = p_stages->lambda;
		calcReducedLostTimeAlgInput.stage_oversat_weighting_factor = p_stages->oversat_weighting_factor;
		calcReducedLostTimeAlgInput.links_oversat_weighting_factor = p_links->oversat_weighting_factor;
		break;

	case ALG_CALC_BONUS_GREEN_TIME:
		calcBonusGreenTimeAlgInput.current_stage = p_junction->current_stage;
		calcBonusGreenTimeAlgInput.last_stage = p_junction->last_stage;
		calcBonusGreenTimeAlgInput.previous_stage = p_junction->previous_stage;

		calcBonusGreenTimeAlgInput.links_green_flag = p_links->green_flag;
		calcBonusGreenTimeAlgInput.links_endsat_marker = p_links->endsat_marker;
		
		calcBonusGreenTimeAlgInput.red_count_x_det = p_lanes->red_count_x_det;
		calcBonusGreenTimeAlgInput.exit_red_count = p_lanes->exit_red_count;
		calcBonusGreenTimeAlgInput.sink_red_count = p_lanes->sink_red_count;
		calcBonusGreenTimeAlgInput.det_suspicion_status = p_dets->suspect_status;
		calcBonusGreenTimeAlgInput.link_oversat_weighting_factor = p_links->oversat_weighting_factor;

		calcBonusGreenTimeAlgInput.bonus_green_time = p_links->bonus_green_time;
		calcBonusGreenTimeAlgInput.sink_bonus_green_time = p_links->sink_bonus_green_time;
		break;


	case ALG_CALC_SHORT_CYCLE_CONTROL_PARAMS:
		calcShortCycleCtrlParamAlgInput.smoothed_cycle_time =  p_junction->smoothed_cycle_time;
		calcShortCycleCtrlParamAlgInput.alt_lambda = p_stages->alt_lambda;
		calcShortCycleCtrlParamAlgInput.stage_bonus_time = p_stages->bonus_time;
		calcShortCycleCtrlParamAlgInput.stage_bonus_link_num = p_stages->bonus_link_num;
		calcShortCycleCtrlParamAlgInput.links_oversat_weighting_factor = p_links->oversat_weighting_factor;
		break;

	case ALG_CAPACITY_MAXIMISATION:
		/*This is a specical case algorithm becasue it uses a local data that are not in any of the entities structs
		  (e.g., Lanes, Links, ...). For that reason, we fill the struct durectly in the Links Endsat Handler */
		break;

	case ALG_DELAY_AND_STOPS_OPTIMISER:
		delayAndStopsOptiAlgInput.shift_register = p_lanes->shift_register;
		delayAndStopsOptiAlgInput.extra_green_time = p_links->extra_green_time;
		delayAndStopsOptiAlgInput.total_bonus_green = p_links->total_bonus_green_time;
		delayAndStopsOptiAlgInput.y_value = p_links->y_value;
		delayAndStopsOptiAlgInput.ben_link_marker = p_links->benefiting_marker;
		delayAndStopsOptiAlgInput.link_smoothed_flow = p_links->smoothed_flow;

		delayAndStopsOptiAlgInput.red_count_x_det = p_lanes->red_count_x_det;
		delayAndStopsOptiAlgInput.red_count_in_det = p_lanes->red_count_in_det;

#ifdef M8_IMPORVED_BUS_PRIORITY
		delayAndStopsOptiAlgInput.bus_weighted_red_count = p_lanes->bus_weighted_red_count;
#endif

		delayAndStopsOptiAlgInput.det_suspicion_status = p_dets->suspect_status;
		delayAndStopsOptiAlgInput.extra_veh_beyond_in_det = p_lanes->extra_veh_beyond_in_det;

		delayAndStopsOptiAlgInput.current_stage = p_junction->current_stage;
		delayAndStopsOptiAlgInput.next_stage = p_junction->next_stage;

		delayAndStopsOptiAlgInput.stage_demand_marker = p_stages->demand_marker;
		delayAndStopsOptiAlgInput.link_demand_marker = p_links->demand_marker;

		delayAndStopsOptiAlgInput.stage_emergency_demand_marker = p_stages->emergency_demand_marker;
		delayAndStopsOptiAlgInput.stage_priority_demand_marker = p_stages->priority_demand_marker;
		
		delayAndStopsOptiAlgInput.ped_wait_timer = timers.ped_wait_timer;

		delayAndStopsOptiAlgInput.link_green_timer = timers.link_green_timer;
		delayAndStopsOptiAlgInput.cycle_time_timer = timers.cycle_time_timer;
		break;

	case ALG_ESTIMATE_QUEUES_SIZE:
		estimateQueueSizeAlgInput.det_suspicion_status = p_dets->suspect_status;
		estimateQueueSizeAlgInput.det_on_time = p_dets->time_on;
		estimateQueueSizeAlgInput.smoothed_flow = p_lanes->smoothed_flow;
		estimateQueueSizeAlgInput.exit_red_count = p_lanes->exit_red_count;
		estimateQueueSizeAlgInput.sink_red_count = p_lanes->sink_red_count;
		estimateQueueSizeAlgInput.red_count_for_in_det = p_lanes->red_count_in_det;
		break;

	default:
		/*Should never reach this line*/
		break;	
	}
}

mv_bool ALG_is_det_suspected(uint8 detIdx)
{
	return is_det_suspected(detIdx);
}