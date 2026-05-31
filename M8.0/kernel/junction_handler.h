/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         junction_handler.h
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

#if !defined (JUNCTION_HANDLER_H)
#define JUNCTION_HANDLER_H

#include "mova_types.h"

void init_junction_handler();
void init_junction_dynamic_data();

void handle_junction_scan_initial_updates(mv_bool isInStartOfIG);
void handle_junction_bonus_situation();
void handle_junction_oversat_updates();
void handle_junction_end_of_stage_updates();
void handle_junction_intergreen_updates();
void handle_junction_green_stage_updates();
mv_bool handle_junction_next_stage_updates(mv_bool * is_var_min_green_Expired);
mv_bool handle_junction_endsat_checking(mv_bool isInCheckingEndsatOnwards);
void update_lane_oversat_counter_max(uint8 laneIdx);
void reset_max_lane_oversat_counter();
void handle_junction_demands_updates();
mv_bool is_junction_max_green_expired(mv_bool isInMinGreen);
void handle_junction_average_flow_update();
void handle_junction_stage_changing();
void handle_junction_ep_ext_updates();
uint8 handle_junction_ep_timers_and_markers_updates(mv_bool isEP_MaxGreenExpired);

#ifdef M8_RE_ESTABLISHMENT_OF_SATFLOW
mv_bool handle_junction_platoon_checking();
#endif

void handle_junction_NTT_code();

#ifdef M8_IMPROVED_LINKING
void handle_junction_linked_mova_operation();
#endif

/*Setters*/
void set_junction_cut_max_times_marker(int8 markerVal);
void set_is_oversat_changed(mv_bool val);
void set_is_oversat_increased(mv_bool val);
void set_junction_smoothed_cycle_time(int32 val);
void set_junction_total_smoothed_flow(int32 val);
void set_ped_demand_present(mv_bool val);
void set_is_to_truncate_current_stage(mv_bool val);
void set_ep_hold_marker(int8 val);

#ifdef M8_IMPORVED_BUS_PRIORITY
void set_is_priority_demand_present(mv_bool val);
#endif

/*Incrementers/Decrementers*/
void inc_junction_total_smoothed_flow(int32 incBy);
void inc_junction_oversat_counter(int32 incrementValue);
void dec_junction_oversat_counter(int32 decrementValue);
void inc_warmup_counter();

/*Getters*/
int32 get_junction_oversat_counter();
int8 get_junction_cut_max_times_marker();
uint8 get_current_stage();
uint8 get_previous_stage();
uint8 get_last_stage();
int32 get_junction_smoothed_cycle_time();
int32 get_junction_total_lambda();
uint8 get_next_stage();
int32 get_var_min_green_time();
int32 get_max_lane_oversat_counter();
uint8 get_next_emeregency_stage_to_demand();
uint8 get_next_priority_stage_to_demand();
uint8 get_demanded_stage();

#ifdef 	M8_IMPROVED_LINKING
int32 get_forward_link_delay();
#endif


mv_bool is_in_warmup();
mv_bool is_in_initialisation();
mv_bool is_in_abs_min_green();

mv_bool is_ep_hold_or_ext_present();

#endif /*JUNCTION_HANDLER_H*/