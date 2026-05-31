
#include "mova_constants.h"

struct MovaDataset
{
	int lanes_count;
	int link_lanes_count[MAX_LINKS];
	int dets_count;
	int stages_count;
	int links_count;
	int link_main_lanes_count[MAX_LINKS];

	int lane_num[MAX_LINKS][MAX_LANES_ON_LINK];

	int link_type[MAX_LINKS];

	int wait_channel_num[MAX_LINKS];

	int main_stage_num;
	int short_link_marker[MAX_LINKS];
	int min_percent_sat[MAX_LINKS];
	int short_lane_length[MAX_LANES];

	signed char	det_on[MAX_DETS];

	int det_num[MAX_LANES][MAX_DETS_TYPES];
	int	assoc_det_num[MAX_LANES][MAX_ASSOC_DET];

	int time_to_use_comb_x[MAX_LANES];

	int oversat_det[MAX_LANES];
	int oversat_time[MAX_LANES];
	int oversat_critical_veh_count[MAX_LANES];

	int lane_weighting_factor[MAX_LANES];

	int	det_min_on_time_for_queue[MAX_LANES];
	int moving_q_over_in_det[MAX_LANES];
	int moving_q_over_x_det[MAX_LANES];

	int x_det_cruise_time[MAX_LANES];
	int in_det_cruise_time[MAX_LANES];

	int gamber_time[MAX_LANES];
	int satinc_time[MAX_LANES];
	int startup_lost_time[MAX_LANES];

	int min_veh_count_between_x_and_out_det[MAX_LINKS];
	
	int link_green_status[MAX_STAGES][MAX_LINKS];
	int stage_change_allowance[MAX_STAGES][MAX_STAGES];
	int sdcode[MAX_STAGES][MAX_LINKS];

	int lost_time[MAX_LINKS];

	int min_green_time[MAX_STAGES];
	int max_green_time[MAX_STAGES];
	int low_max_green_time[MAX_STAGES];
	int link_min_green_time[MAX_LINKS];
	int link_endsat_max_green[MAX_LINKS];
	int max_min_green_time[MAX_LANES];

	int pedmax[2][MAX_STAGES];

	int lane_critical_gap[MAX_LANES];
	int lane_satflow_gap[MAX_LANES];

	int total_green_time;

	int bonus_green_recalc_time[MAX_LINKS];
	int bonus_enabling_marker[MAX_LINKS];
	int	bonus_base_count[MAX_LANES];
	
	int stop_penalty[MAX_LINKS];

	int min_extension;
	int max_extension;

	int	scan_period;
	int reduced_critical_gap;
	int additional_critical_gap;

	int satflow_mean_gap[MAX_LANES];

	/*E/P data*/
	int priority_facility[MAX_STAGES]; /*PFACIL[stages]*/
	int emergency_ext_max[MAX_STAGES]; /*EMXMAX[stages]*/
	int priority_ext_max[MAX_STAGES]; /*PRXMAX[stages]*/
	
	int ep_det_num[MAX_LINKS][MAX_EP_DETS]; /*EPDET[links][2]*/
	int ep_cancel_det_num[MAX_LINKS]; /*CANDET[links]*/

	int ep_ext_time[MAX_LINKS][MAX_EP_DETS]; /*EPEXT[links][2]*/
	int ep_hold_time[MAX_LINKS]; /*HOLDTM[links]*/

};

struct MovaDataset movaDataset;