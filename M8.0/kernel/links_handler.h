/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         links_handler.h
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

#if !defined (LINKS_HANDLER_H)
#define LINKS_HANDLER_H

#include "mova_types.h"

/*Initialisers*/
void init_links_handler();
void init_links_dynamic_data();

mv_bool is_link_effectively_red(uint8 stageIdx, uint8 linkIdx);
void update_links_at_intergreen();
void update_link_var_min_green(uint8 linkIdx);

void update_traffic_link_demand(uint8 liknIdx);
void update_special_link_demand(uint8 liknIdx);
void update_ped_link_demand(uint8 linkIdx);
int32 calc_link_current_green_time(uint8 stageIdx, uint8 linkIdx);
mv_bool update_normal_link_endsat_marker_after_lanes_checking(uint8 linkIdx, uint8 laneIdx);
void update_links_benefiting_marker();
int32 calc_links_merit_value();
void update_link_green_flag(uint8 linkIdx);
#ifdef M8_RT_OPTIMISATION
void update_opposed_link_red_x_counts(uint8 netGapsCount, int32 * netGaps);
#endif

#ifdef M8_IMPROVED_LINKING
uint8 decide_forward_or_backward_linking(uint8 linkIdx, int32 * timeOffset);
#endif


/* Getters */
int32 get_link_total_bonus_green_time(uint8 linkIdx);
uint8 get_link_bonus_stage(uint8 linkIdx);
int8 get_link_green_flag(uint8 linkIdx);
int8 get_link_endsat_marker(uint8 linkIdx);
int32 get_link_var_min_green_time(uint8 linkIdx);
int8 get_link_demand_marker(uint8 linkIdx);								
int8 get_link_benefiting_marker(uint8 linkIdx);
int32 get_link_ep_skip_indicator(uint8 linkIdx);
int32 get_link_ep_trunc_indicator(uint8 linkIdx);
int32 get_link_smoothed_flow(uint8 linkIdx);
int32 get_link_y_value(uint8 linkIdx);
mv_bool is_oversat_link(uint8 linkIdx);

#if defined (TRL_HOST_DEBUG)
int32 * get_links_bonus_green_time();
#endif 

int32 * get_links_total_bonus_green_time();
uint8 * get_links_bonus_stage();
int8 * get_links_endsat_marker();
int32 * get_links_reduced_lost_time();

/* Resetters */
mv_bool reset_link_timers_and_markers_at_stage_green(uint8 linkIdx);

/* Incs/Decs*/
void inc_link_oversat_weighting_factor_counter(uint8 linkIdx, int32 incrementValue);
void dec_link_oversat_weighting_factor_counter(uint8 linkIdx, int32 decrementValue);
void inc_link_det_fault_marker(uint8 linkIdx, int32 incVal);

#ifndef M8_REMOVE_REV_DEMAND
int8 get_link_rev_demand_marker(uint8 linkIdx);
void set_rev_demand_marker(uint8 linkIdx, int8 val);
#endif

#endif /*LINKS_HANDLER_H*/