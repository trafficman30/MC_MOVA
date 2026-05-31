/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         lanes_handler.h
*
*  TITLE:        Mova Kernel: Lanes Handler
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

#if !defined (LANES_HANDLER_H)
#define LANES_HANDLER_H

#include "mova_types.h"

void init_lanes_handler();
void init_lanes_dynamic_data();

void update_lane_shift_register_and_red_counts(uint8 laneIdx);
void correct_lane_red_count(uint8 laneIdx);
void update_lane_oversat_counters(uint8 linkIdx, uint8 laneIdx);
void set_lane_det_fault_status(uint8 linkIdx, uint8 laneIdx);
int32 update_lane_dets_status(uint8 linkIdx, uint8 laneIdx);
void calc_lane_smoothed_flow(uint8 linkIdx, uint8 laneIdx, int32 detCount);
void update_lane_dets_cum_counts(uint8 laneIdx);
void calc_lanes_var_min_green(uint8 linkIdx, int32 * linkVarMinGreen);
int8 decide_lane_demand(uint8 linkIdx, uint8 laneIdx);
int8 decide_lane_special_demand(uint8 linkIdx, uint8 laneIdx, int32 minQ, int32 * totalQ);
void update_lanes_average_flow();

#ifdef M8_RT_OPTIMISATION
void dec_lane_red_count_x_det(uint8 laneIdx, int32 decBy);
#endif

#ifdef M8_IMPROVED_LINKING
int32 calc_lane_time_offset_for_linked_mova(uint8 laneIdx);
#endif

/* Getters (work)*/
int32 get_lane_current_average_flow(uint8 laneIdx);


/* Getters*/
int32 get_lane_oversat_counter(uint8 laneIdx);
int32 get_lane_red_count_x_det(uint8 laneIdx);
int32 get_lane_red_count_in_det(uint8 laneIdx);
int32 get_lane_smoothed_flow(uint8 laneIdx);
int32 get_in_sink_cum_det_count(uint8 laneIdx);
int32 get_x_sink_cum_det_count(uint8 laneIdx);
int32 get_lane_det_fault_marker(uint8 laneIdx);

#ifdef M8_ADDITIONAL_SINK_DETS
int32 get_in_sink_2_cum_det_count(uint8 laneIdx);
int32 get_x_sink_2_cum_det_count(uint8 laneIdx);
#endif

int32 get_assoc_cum_det_count(uint8 laneIdx, uint8 assocDetIdx);
int8 get_lane_endsat_marker(uint8 laneIdx);
int8 * get_lanes_endsat_marker();
int32 get_lane_oversat_init_count(uint8 laneIdx);
int32 * get_lanes_extra_veh_beyond_in_det();
int32 get_lane_extra_veh_beyond_in_det(uint8 laneIdx);


/* Resetters */
void reset_lane_counts_and_markers(uint8 laneIdx);
void reset_lanes_endsat_marker(uint8 linkIdx);

#endif /*LANES_HANDLER_H*/