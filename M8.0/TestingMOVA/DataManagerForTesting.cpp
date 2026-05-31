#include "stdafx.h"
#include "DataManagerForTesting.h"

#include "common_data.h"



DataManagerForTesting::DataManagerForTesting(void)
{
}


DataManagerForTesting::~DataManagerForTesting(void)
{
}


void DataManagerForTesting::ResetingMovaDataSet()
{
	for(uint8 i=1; i<=MAX_DETS_TYPES; i++)
		for(uint8 j=1; j<=MAX_LANES; j++)
		{
			DI_set_det_num(j, i, 0);
		}

	for(uint8 i=1; i<=MAX_LANES; i++)
	{
		DI_set_lane_weighting_factor(i,0);
		DI_set_oversat_det(i, 0);
		DI_set_in_det_cruise_time(i, 0);
		DI_set_oversat_critical_veh_count(i, 0);
		DI_set_short_lane_lenght(i, 0);
		DI_set_lane_lengthened_critical_gap(i, 0);
		DI_set_moving_q_over_in_det(i, 0);
		DI_set_moving_q_over_x_det(i, 0);
		DI_set_det_min_on_time_for_queue(i, 0);
		DI_set_lane_critical_gap(i, 0);
		DI_set_lane_portion_of_critical_gap(i, 0);
		DI_set_oversat_critical_veh_count_compact(i, 0);

		DI_set_satinc_time(i, 0);

		DI_set_opposed_lane_num(i, 0, 0);
		DI_set_opposed_lane_num(i, 1, 0);
		DI_set_opposed_lane_num(i, 1, 0);

		DI_set_right_turn_storage_capacity(i, 0);
		DI_set_right_turn_radius(i, 0);
		DI_set_right_turn_veh_proportion(i, 0);

	}

	for(uint8 i=1; i<MAX_LINKS; i++)
	{
		DI_set_link_lanes_count(i, 0);

		for(uint8 s=1; s<=MAX_STAGES; s++)
			DI_set_link_green_status(s, i, 0);

		for(uint8 l=1; l<MAX_LANES; l++)
			DI_set_lane_num(i, l-1, 0);
	}

	for(uint8 s=1; s<=MAX_STAGES; s++)
	{
		DI_set_stage_max_green_time(s, 0);
	}

	DI_set_stages_count(0);
	DI_set_total_green_time(0);
	DI_set_links_count(0);		

	DI_set_scan_period(0);
}


void DataManagerForTesting::ResetingMovaDynamicData()
{
	//memset(, 0, sizeof());
	memset(timers.cycle_time_timer, 0, sizeof(timers.cycle_time_timer));
	memset(timers.stage_cycle_timer, 0, sizeof(timers.stage_cycle_timer));
	timers.signal_state_timer = 0;
	timers.wait_stage_change_timer = 0;
	timers.stage_max_green_timer = 0;
	timers.ep_ext_max_timer = 0;

	memset(dets.time_on, 0, sizeof(dets.time_on));
	memset(dets.suspicion_status, 0, sizeof(dets.suspicion_status));
	memset(dets.old_gap, 0, sizeof(dets.old_gap));
	memset(dets.time_off, 0, sizeof(dets.time_off));
			

	memset(lanes.oversat_counter, 0, sizeof(lanes.oversat_counter));
	memset(lanes.oversat_init_count, 0, sizeof(lanes.oversat_init_count));
			
	memset(links.oversat_weighting_factor, 0, sizeof(links.oversat_weighting_factor));
	memset(links.green_flag, 0, sizeof(links.green_flag));
	memset(links.endsat_marker, 0, sizeof(links.endsat_marker));

			
	memset(stages.oversat_weighting_factor, 0, sizeof(stages.oversat_weighting_factor));
	memset(stages.oversat_max_counter, 0, sizeof(stages.oversat_max_counter));
	memset(stages.lambda, 0, sizeof(stages.lambda));
	memset(stages.alt_lambda, 0, sizeof(stages.alt_lambda));
	memset(stages.drift_max, 0, sizeof(stages.drift_max));	
	memset(stages.endsat_marker, 0, sizeof(stages.endsat_marker));	


	junction.max_lane_oversat_counter = 0;
	junction.oversat_counter = 0;
	junction.is_oversat_changed = mv_false;
	junction.is_oversat_increased = mv_false;
	junction.warmup_counter = 0;
	junction.previous_stage = 0;
	junction.ped_demand_marker = 0;
	junction.smoothed_cycle_time = 0;
	junction.total_lambda = 0;
	junction.last_stage = 0;
	junction.var_min_green_time = 0;
	junction.is_to_truncate_current_stage = mv_false;
	junction.current_stage = 0;
	junction.next_stage = 0;


	CI_set_external_function_pointers(DivideError);
	set_links_pointer_for_ep_handler(&links);
	set_links_pointer_for_endsat_handler(&links);

	set_lanes_pointer_for_endsat_handler(&lanes);
}
