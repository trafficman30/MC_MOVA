/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         core_interface.c
*
*  TITLE:        Mova Kernel: Core Interface
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

#include "gbltypes.h"
#include "mova_constants.h"
#include "timers_manager.h"
#include "core_interface.h"
#include "junction_handler.h"
#include "stages_handler.h"
#include "links_handler.h"
#include "lanes_handler.h"
#include "detectors_handler.h"
#include "program_controller.h"
#include "messages_manager.h"
#include "errhand.h"

#include <memory.h>


static ProgramStruct	*	p_prog;
static JunctionStruct	*	p_junction;
static StagesStruct		*	p_stages;
static LinksStruct		*	p_links;
static LanesStruct		*	p_lanes;
static DetectorsStruct	*	p_dets;
static MessagesStruct	*	p_msgs;


void CI_init_core_module()
{
	init_program_controller();
	init_junction_handler();
	init_stages_handler();
	init_links_handler();
	init_lanes_handler();
	init_dets_handler();
	init_messages_manager();
}


void CI_init_core_dynmaic_data()
{
	init_program_dynamic_data();
	init_junction_dynamic_data();
	init_stages_dynamic_data();
	init_links_dynamic_data();
	init_lanes_dynamic_data();
	init_timers_value();
}


mv_bool CI_start_scan()
{
	return program_controller();
}

/*!
*	\brief	Initialises all the input_data that are required to be updated
*			before each scan.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void CI_set_each_scan_input_data(int currentStage,
							int * linksGreenFlagRef,						
							int8 * detsOnFlagRef,
							int * detsPreviousGapRef,
							int * detsOnTimeRef,
							int * detsOffTimeRef,
							int * detsVehCountRef)
{
	/* we do referencing here instead of copying to have only one copy of the data in the memory */
	input_data.current_stage = currentStage;

	input_data.links_green_flag = linksGreenFlagRef;

	input_data.dets_on_flag = detsOnFlagRef;
	input_data.dets_previous_gap = detsPreviousGapRef;
	input_data.dets_on_time = detsOnTimeRef;
	input_data.dets_off_time = detsOffTimeRef;
	input_data.dets_veh_count = detsVehCountRef;
}

ProgramStruct *	 const CI_get_program_data_ptr()
{
	return p_prog;
}

JunctionStruct *  const CI_get_junction_data_ptr()
{
	return p_junction;
}

StagesStruct *	 const CI_get_stages_data_ptr()
{
	return p_stages;
}

LinksStruct *	 const CI_get_links_data_ptr()
{
	return p_links;
}

LanesStruct *	 const CI_get_lanes_data_ptr()
{
	return p_lanes;
}

DetectorsStruct * const CI_get_dets_data_ptr()
{
	return p_dets;
}

MessagesStruct *	 const CI_get_msgs_data_ptr()
{
	return p_msgs;
}

TimersStruct * const CI_get_timers_data_ptr()
{
	return &timers;
}


uint32 CI_get_program_data_size()
{
	return sizeof(struct Program);
}

uint32 CI_get_junction_data_size()
{
	return sizeof(struct Junction);
}

uint32 CI_get_stages_data_size()
{
	return sizeof(struct Stages);
}

uint32 CI_get_links_data_size()
{
	return sizeof(struct Links);
}

uint32 CI_get_lanes_data_size()
{
	return sizeof(struct Lanes);
}

uint32 CI_get_dets_data_size()
{
	return sizeof(struct Detectors);
}

uint32 CI_get_msgs_data_size()
{
	return sizeof(struct Messages);
}

uint32 CI_get_timers_data_size()
{
	return sizeof(struct Timers);
}

/*!
*	\brief	Calculates the checksum of the dynamic data. 
*
*	\return		checksum.
*
*	\author		Islam Abdelhalim 
*	\date		02-01-2014 */
int32 CI_get_dynamic_data_checksum()
{
	uint8 s; /* stages loop variable */
	uint8 l; /* links loop variable */
	uint8 n; /* lanes loop variable */
	uint8 d; /* detectors loop variable */
	uint8 p; /* periods loop variable */
	uint8 r; /* shift register loop variable */ 
	uint8 m; /* messages loop variable */
#ifdef M8_IMPROVED_LINKING
	uint8 g; /* gaps loop variable*/
#endif

	int32 checksum = 0;

	//Program data
	checksum =	p_prog->ep_marker + p_prog->marker + p_prog->previous_marker;

	//Junction data
	checksum += p_junction->current_stage + p_junction->cut_max_times_marker + p_junction->demanded_stage
				+ p_junction->ep_ext_marker + p_junction->ep_hold_marker + p_junction->extra_green_for_drift_max
				+ p_junction->is_oversat_changed + p_junction->is_oversat_increased + p_junction->is_ped_demand_present
				+ p_junction->is_to_truncate_current_stage + p_junction->last_stage + p_junction->low_total_green
				+ p_junction->max_lane_oversat_counter + p_junction->next_emeregency_stage_to_demand
				+ p_junction->next_priority_stage_to_demand + p_junction->next_stage + p_junction->next_stage
				+ p_junction->oversat_counter + p_junction->ped_demand_marker + p_junction->ped_max_green_time
				+ p_junction->previous_next_stage + p_junction->previous_stage + p_junction->smoothed_cycle_time
				+ p_junction->total_lambda + p_junction->total_smoothed_flow + p_junction->var_min_green_time
				+ p_junction->warmup_counter;
	
#ifdef M8_IMPORVED_BUS_PRIORITY
	checksum += p_junction->is_priority_demand_present;
#endif

#ifdef M8_IMPROVED_LINKING
	checksum += p_junction->net_gaps_count + p_junction->is_to_send_backward_hold_signal + p_junction->is_to_send_backward_release_signal
			    + p_junction->forward_link_delay + p_junction->is_to_do_backward_linking;

	for (g = 0; g<MAX_GAPS; g++)
		checksum += p_junction->net_gaps[g];
#endif

	/*Stages data*/
	for(s=0; s<MAX_STAGES; s++)
	{
		checksum +=	  p_stages->alt_lambda[s] + p_stages->bonus_link_num[s] + p_stages->bonus_time[s]
					+ p_stages->demand_marker[s] + p_stages->drift_max[s] + p_stages->emergency_demand_marker[s]
					+ p_stages->endsat_marker[s] + p_stages->lambda[s] + p_stages->low_drift_max[s]
					+ p_stages->merit_value[s] + p_stages->oversat_max_counter[s] + p_stages->oversat_weighting_factor[s]
					+ p_stages->priority_demand_marker[s];
	}

	/*Links data*/
	for(l=0; l<MAX_LINKS; l++)
	{
		checksum +=	  p_links->benefiting_marker[l] + p_links->bonus_green_time[l] + p_links->bonus_stage[l]
					+ p_links->demand_marker[l] + p_links->det_fault_marker[l] + p_links->endsat_marker[l]
					+ p_links->ep_cancel_marker[l] + p_links->ep_demand_delay[l] + p_links->ep_skip_indicator[l]
					+ p_links->ep_trunc_indicator[l] + p_links->extra_green_time[l] + p_links->green_flag[l]
					+ p_links->is_ep_demanded_in_red[l] + p_links->is_to_cancel_ep_demand_in_red[l]
					+ p_links->max_percent_sat[l] + p_links->oversat_weighting_factor[l]
					+ p_links->previous_endsat_marker[l] + p_links->reduced_lost_time[l]
#ifndef M8_REMOVE_REV_DEMAND
					+ p_links->rev_demand_marker[l] + p_links->sink_bonus_green_time[l]
#endif
					+ p_links->smoothed_flow[l] + p_links->total_bonus_green_time[l]
					+ p_links->var_min_green[l] + p_links->y_value[l];


#ifdef M8_IMPORVED_BUS_PRIORITY		
		checksum += p_links->demanded_priority_stage_num[l];
#endif
	}


	/*Lanes data*/
	for(n=0; n<MAX_LANES; n++)
	{
		checksum +=	  p_lanes->assoc_cum_det_count[n][0] + p_lanes->assoc_cum_det_count[n][1]
					+ p_lanes->det_fault_marker[n] + p_lanes->endsat_marker[n] 
					+ p_lanes->exit_red_count[n] +  p_lanes->extra_veh_beyond_in_det[n] + p_lanes->in_sink_cum_det_count[n]
					+ p_lanes->oversat_counter[n] + p_lanes->oversat_init_count[n] + p_lanes->red_count_in_det[n] 
					+ p_lanes->red_count_x_det[n] + p_lanes->sink_red_count[n] + p_lanes->smoothed_flow[n]
					+ p_lanes->total_flow_veh[n] + p_lanes->x_sink_cum_det_count[n] + p_lanes->x_sink_2_cum_det_count[n]
					+ p_lanes->in_sink_2_cum_det_count[n];

#ifdef M8_IMPORVED_BUS_PRIORITY		
		checksum += p_lanes->bus_weighted_red_count[n];
#endif

		for(d=0; d<7; d++)
			for(p=0; p<48; p++)
				checksum += p_lanes->average_flow[d][p][n];
		
		for(r=0; r<REG_SIZE; r++)
			checksum += p_lanes->shift_register[n][r];
	}

	/*Detectors data*/
	for(d=0; d<MAX_DETS; d++)
	{
		checksum +=	  p_dets->cum_det_count[d] + p_dets->old_cum_det_count[d] + p_dets->old_gap[d]
					+ p_dets->on_det_flag[d] + p_dets->pre_cum_det_count[d] + p_dets->suspect_status[d]
					+ p_dets->time_off[d] + p_dets->time_on[d];
	}
	

	/*Timers*/
	checksum += timers.signal_state_timer + timers.wait_stage_change_timer + timers.stage_max_green_timer
		+ timers.ped_wait_timer + timers.ep_ext_max_timer + timers.emergency_stage_ext_timer
		+ timers.priority_stage_ext_timer;
	
#ifdef M8_IMPROVED_LINKING
	checksum += timers.backward_hold_timer;
#endif

	for(s=0; s<MAX_STAGES; s++)
	{
		checksum += timers.stage_cycle_timer[s];
	}

	for(l=0; l<MAX_LINKS; l++)
	{
		checksum +=   timers.cycle_time_timer[l] + timers.link_green_timer[l] + timers.link_waste_green_timer[l]
					+ timers.ep_link_ext_timer[l];
	}

	/*Messages*/
	checksum += p_msgs->current_msg_num;

	for(m=0; m<MAX_MSGS; m++)
		for(d=0; d<MSG_MAX_LENGTH; d++)
			checksum += p_msgs->msgs_data[m][d];

	return checksum;
}


/*----------------------------------------------------------------*/
/*------------------------- Getters ------------------------------*/
/*----------------------------------------------------------------*/
int32 CI_get_warmup_counter()
{
	return p_junction->warmup_counter;
}

uint8 CI_get_current_stage()
{
	return p_junction->current_stage;
}

uint8 CI_get_last_stage()
{
	return p_junction->last_stage;
}
 
uint8 CI_get_next_stage()
{
	return p_junction->next_stage;
}

uint8 CI_get_demanded_stage()
{
	return p_junction->demanded_stage;
}

int8 CI_get_program_marker()
{
	return p_prog->marker;
}

int8 CI_get_previous_marker()
{
	return p_prog->previous_marker;
}

int8 CI_get_ped_demand_marker()
{
	return p_junction->ped_demand_marker;
}

struct Messages * CI_get_msgs()
{
	return p_msgs;
}

int32  CI_get_msg_data(uint8 fieldIdx, uint8 msgIdx)
{
	return p_msgs->msgs_data[msgIdx][fieldIdx];
}

uint8  CI_get_current_msg_num()
{
	return p_msgs->current_msg_num;
}

int32 CI_get_lane_average_flow(uint8 dayIdx, uint8 periodIdx, uint8 laneIdx)
{
	return p_lanes->average_flow[dayIdx][periodIdx][laneIdx]; 
}

#ifdef M8_IMPROVED_LINKING
mv_bool CI_is_to_send_backward_hold_signal()
{
	return p_junction->is_to_send_backward_hold_signal;  
}

mv_bool CI_is_to_send_backward_release_signal()
{
	return p_junction->is_to_send_backward_release_signal;  
}
#endif

int8 CI_get_detector_suspicion_status(uint8 detIdx)
{
	return p_dets->suspect_status[detIdx];
}

/*----------------------------------------------------------------*/
/*------------------------- Setters ------------------------------*/
/*----------------------------------------------------------------*/
void CI_set_warmup_counter(int32 val)
{
	p_junction->warmup_counter = val;
}

void CI_set_current_stage(uint8 val)
{
	p_junction->current_stage = val;
}

void CI_set_last_stage(uint8 val)
{
	p_junction->last_stage = val;
}
 
void CI_set_next_stage(uint8 val)
{
	p_junction->next_stage = val;
}

void CI_set_demanded_stage(uint8 val)
{
	p_junction->demanded_stage = val;
}

void CI_set_previous_marker(int8 val)
{
	p_prog->previous_marker = val;
}

void CI_set_msg_data(uint8 fieldIdx, uint8 msgIdx, int32 data)
{
	p_msgs->msgs_data[msgIdx][MSG_MAX_LENGTH - 1] = 0;
	p_msgs->msgs_data[msgIdx][fieldIdx] = data;
}

#ifdef M8_RT_OPTIMISATION
void CI_add_opposoing_lanes_net_gap(int32 gap_time)
{
	p_junction->net_gaps_count++;

	if(p_junction->net_gaps_count < MAX_GAPS) /* to ignore the gap if the array is full*/
		p_junction->net_gaps[p_junction->net_gaps_count - 1] = gap_time;
}
#endif

#ifdef M8_DET_FAULT_CHECKING_AND_LOGGING
void CI_set_detector_suspicion_status(uint8 detIdx, int8 status)
{
	p_dets->suspect_status[detIdx] = status;
}
#endif


/*----- setting pointer(s) for external functions ------*/

void CI_set_external_function_pointers(void (*divideError)(int, int, ...))
{
	p_divide_error = divideError;
}



/*------------------ Pointers Setters ----------------------*/
void CI_set_prog_pointer(struct Program * progRef)
{
	p_prog = progRef;
}

void CI_set_junction_pointer(struct Junction * junRef)
{
	p_junction = junRef;
}

void CI_set_stages_pointer(struct Stages * stagesRef)
{
	p_stages = stagesRef;
}

void CI_set_links_pointer(struct Links * linksRef)
{
	p_links = linksRef;
}

void CI_set_lanes_pointer(struct Lanes * lanesRef)
{
	p_lanes = lanesRef;
}

void CI_set_dets_pointer(struct Detectors * detsRef)
{
	p_dets = detsRef;
}

void CI_set_msgs_pointer(struct Messages * msgsRef)
{
	p_msgs = msgsRef;
}



/*----------------------------------------------------------------*/
/*------------------------- Resetters ----------------------------*/
/*----------------------------------------------------------------*/
void CI_reset_current_msg_number()
{
	p_msgs->current_msg_num = 1;
}

void CI_reset_stages_demands()
{
	memset( p_stages->demand_marker, 0, sizeof( p_stages->demand_marker ) );

	memset( p_stages->priority_demand_marker, 0, sizeof( p_stages->priority_demand_marker ) );
	memset( p_stages->emergency_demand_marker, 0, sizeof( p_stages->emergency_demand_marker ) );
}

void CI_reset_lanes_average_flow()
{
	uint8 i; /* lanes loop variable */
	uint8 j; /* periods loop variable */
	uint8 k; /* days loop variable */

	for (i=0; i<MAX_LANES; i++)
	{
		for (j=0; j <48; j++) 
		{
			for (k=0; k<7; k++) 
			{
				p_lanes->average_flow[k][j][i] = 0; /* clear ave flows*/
			}
		}
	}
}


#ifdef 	M8_RT_OPTIMISATION
void CI_reset_opposing_lanes_net_gaps()
{
	uint8 i; /* gaps loop variable */

	for(i=0; i<MAX_GAPS; i++)
	{
		p_junction->net_gaps[i] = 0;
	}

	p_junction->net_gaps_count = 0;
}
#endif



void CI_inc_current_msg_num()
{
	p_msgs->current_msg_num++;
}




void CI_load_dynamic_data()
{
	/*SIGNAL_COMPANY: This function is a place holder in case the signal company needs to load the dynamic data
	as well.*/
}