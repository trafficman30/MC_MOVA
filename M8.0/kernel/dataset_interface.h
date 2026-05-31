
/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         dataset_interface.h
*
*  TITLE:        Mova Kernel: Dataset Interface
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

#if !defined (DATASET_INTERFACE_H)
#define DATASET_INTERFACE_H

#include "mova_types.h"

uint8 DI_get_lanes_count(); /*NLANES*/
uint8 DI_get_link_lanes_count(uint8 linkNum); /*llatot[links]*/
uint8 DI_get_dets_count();
uint8 DI_get_stages_count(); /*STAGES*/
uint8 DI_get_links_count(); /*NLINKS*/
uint8 DI_get_link_main_lanes_count(uint8 linkNum); /*nmainl*/

uint8 DI_get_lane_num(uint8 linkNum, uint8 laneInLinkIdx); /*llanes[links][lanes]*/

uint8 DI_get_main_stage_num(); /*mainst*/
int32 DI_get_min_percent_sat(uint8 linkNum); /*shortl*/
int8 DI_get_short_link_marker(uint8 linkNum); /*shortl*/
int32 DI_get_short_lane_length(uint8 laneNum); /*dshort*/

uint8 DI_get_det_num(uint8 laneNum, uint8 detType); /*det[laneNum-1][detType]*/
uint8 DI_get_in_det_num(uint8 laneNum);		/*det[][0]*/
uint8 DI_get_x_det_num(uint8 laneNum);			/*det[][1]*/
uint8 DI_get_out_det_num(uint8 laneNum);		/*det[][2]*/
uint8 DI_get_in_sink_det_num(uint8 laneNum);	/*det[][3]*/
uint8 DI_get_comb_x_det_num(uint8 laneNum);	/*det[][4]*/
uint8 DI_get_x_sink_det_num(uint8 laneNum);	/*det[][5]*/
uint8 DI_get_alt_up_det_num(uint8 laneNum);	/*det[][6]*/
uint8 DI_get_alt_down_det_num(uint8 laneNum);	/*det[][7]*/
uint8 DI_get_stop_line_det_num(uint8 laneNum);	/*det[][8]*/
uint8 DI_get_in_sink_2_det_num(uint8 laneNum);	/*det[][9]*/
uint8 DI_get_x_sink_2_det_num(uint8 laneNum);	/*det[][10]*/

uint8 DI_get_assoc_det_num(uint8 laneNum, uint8 assocDetNum);
uint8 DI_get_ped_det_num(uint8 linkNum);
mv_bool  DI_is_latched_ped_link(uint8 linkNum);

uint16 DI_get_x_det_distance(uint8 laneNum); /*dx*/
uint16 DI_get_in_det_distance(uint8 laneNum); /*din*/

uint8 DI_get_wait_channel_num(uint8 linkNum); /*WAITCH*/

int32 DI_get_time_to_use_comb_x(uint8 laneNum); /*comtim[lanes]*/

uint8 DI_get_oversat_det(uint8 laneNum); /*xosat*/
int32 DI_get_oversat_time(uint8 laneNum); /*osattm*/
int32 DI_get_oversat_critical_veh_count(uint8 laneNum); /*osatcc*/
int32 DI_get_oversat_critical_veh_count_compact(uint8 laneNum); /*osatcx*/
int32 DI_get_cruise_speed(uint8 laneNum); /*cpeed*/

int32 DI_get_lane_weighting_factor(uint8 laneNum); /*lanewf*/

int32	DI_get_det_min_on_time_for_queue(uint8 laneNum); /*QMINON[lanes]*/
int32 DI_get_moving_q_over_in_det(uint8 laneNum); /*MOVQIN[lanes] <time>*/
int32 DI_get_moving_q_over_x_det(uint8 laneNum); /*MOVEQX[lanes] <time>*/

int32 DI_get_x_det_cruise_time(uint8 laneNum); /*CRUSX[lanes]*/
int32 DI_get_in_det_cruise_time(uint8 laneNum); /*CRUSIN[lanes]*/

int32 DI_get_gamber_time(uint8 laneNum); /*GAMBER[lanes]*/
int32 DI_get_satinc_time(uint8 laneNum); /*SATINC[lanes]*/
int32 DI_get_startup_lost_time(uint8 laneNum); /*STLOST[lanes]*/

int32 DI_get_min_veh_count_between_x_and_out_det(uint8 linkNum); /*mixout[links]*/
int32 DI_get_call_time(uint8 linkNum); /*mxcall[links]*/
int32 DI_get_cancel_time(uint8 linkNum); /*mxcncl[links]*/

uint8 DI_get_link_green_status(uint8 stageNum, uint8 linkNum); /*LGREEN[stages][links]*/
int32 DI_get_stage_change_allowance(uint8 currentStageNum, uint8 nextStageNum); /* CHANGE[stages][stages]*/
int16 DI_get_sdcode(uint8 stageNum, uint8 linkNum); /*SDCODE*/

int32 DI_get_lost_time(uint8 linkNum); /*LOSTIM*/

int32 DI_get_stage_min_green_time(uint8 stageNum); /*MIN*/
int32 DI_get_stage_max_green_time(uint8 stageNum); /*MAX*/
int32 DI_get_stage_low_max_green_time(uint8 stageNum); /*LOWMAX*/

int32 DI_get_link_min_green_time(uint8 linkNum); /*LMIN[links]*/

#ifdef M8_LINK_MAXIMUM
int32 DI_get_link_max_green_time(uint8 linkNum); /*LNKMAX[links]*/
#endif

int32 DI_get_link_endsat_max_green(uint8 linkNum); /*ESLMAX*/

int32 DI_get_max_min_green_time(uint8 laneNum); /*MAXMIN[lanes]*/
int32 DI_get_ped_max_1(uint8 stageNum); /*PEDMAX1[stages]*/
int32 DI_get_ped_max_2(uint8 stageNum); /*PEDMAX2[stages]*/

int32 DI_get_total_green_time(); /*TOTALG*/

int32 DI_get_lane_critical_gap(uint8 laneNum); /*critg[lanes]*/

#ifdef M8_PREMATURE_ENDSAT
int32 DI_get_lane_lengthened_critical_gap(uint8 laneNum); /*pcritg[lanes]*/
#endif

#ifdef M8_RE_ESTABLISHMENT_OF_SATFLOW
int32 DI_get_lane_portion_of_critical_gap(uint8 laneNum); /*rcritg[lanes]*/
#endif


int32 DI_get_lane_satflow_gap(uint8 laneNum); /*satgap[lanes]*/

int32 DI_get_bonus_green_recalc_time(uint8 linkNum); /*bontim*/
int32	DI_get_bonus_base_count(uint8 laneNum);	/*BONBC*/
int8 DI_get_bonus_enabling_marker(uint8 linkNum); /*BONCUT*/


int32 DI_get_stop_penalty(uint8 linkNum); /*STOPEN[links]*/

int32 DI_get_min_extension(); /*MINEXT for the delay_and_stops optimisation*/
int32 DI_get_max_extension(); /*MAXEXT for the delay_and_stops optimisation*/

int32 DI_get_link_type(uint8 linkNum); /*ltype*/
mv_bool DI_is_traffic_link(uint8 linkNum); /*ltype == 0*/
mv_bool DI_is_ped_link(uint8 linkNum); /*ltype > 0*/
mv_bool DI_is_ep_link(uint8 linkNum);
mv_bool DI_is_emeregency_link(uint8 linkNum);
mv_bool DI_is_priority_link(uint8 linkNum);

int32 DI_get_scan_period(); /*scan*/
int32 DI_get_reduced_critical_gap(); /*subgap*/
int32 DI_get_additional_critical_gap(); /*addgap*/

int32 DI_get_satflow_mean_gap(uint8 laneNum); /*satgap*/

mv_bool DI_is_long_lane(uint8 laneIdx);


/*E/P functions*/
#ifndef M8_IMPORVED_BUS_PRIORITY
int16 DI_get_priority_facility(uint8 stageNum); /*PFACIL[stages]*/
#endif

int32 DI_get_emergency_ext_max(uint8 stageNum); /*EMXMAX[stages]*/
int32 DI_get_priority_ext_max(uint8 stageNum); /*PRXMAX[stages]*/

uint8 DI_get_ep_det_num(uint8 linkNum, uint8 epDetNum); /*EPDET[links][2]*/
uint8 DI_get_ep_cancel_det_num(uint8 linkNum); /*CANDET[links]*/

int32 DI_get_ep_ext_time(uint8 linkNum, uint8 epDetNum); /*EPEXT[links][2]*/
int32 DI_get_ep_hold_time(uint8 linkNum); /*HOLDTM[links]*/

#ifdef M8_IMPORVED_BUS_PRIORITY
int32 DI_get_priority_facility_matrix(uint8 stageNum, uint8 linkNum); /*PFACILM[stages][links]*/
uint8 DI_get_associated_priority_link_num_for_a_lane(uint8 assoc, uint8 laneNum); /*ASCPRL*/
uint32 DI_get_ep_det_cruise_time(uint8 epLinkNum, uint8 epDetNum); /*EPCRT*/
int32 DI_get_bus_weight(uint8 epLinkNum); /*BUSWGT*/
int32 DI_get_bus_max_1(uint8 stageNum); /*BUSMAX1*/
int32 DI_get_bus_max_2(uint8 stageNum); /*BUSMAX2*/
#endif

#ifdef M8_IMPROVED_LINKING 
int32 DI_get_upstream_junction_intergreen(); /*UPSMIG*/
int32 DI_get_link_distance_from_to_stoplines(); /*LKDIST*/
int32 DI_get_assumed_acceleration_to_reach_cruise_speed(); /*a*/
int32 DI_get_vehicle_length(uint8 laneNum); /*VLEN*/
#endif

/*Sat flow recalculations functions*/
int32 DI_get_satflow_start_time(uint8 periodNum);
int32 DI_get_satflow_end_time(uint8 periodNum);
int32 DI_is_to_use_calc_satinc_time(uint8 laneNum);
int32 DI_is_to_use_calc_startup_lost_time(uint8 laneNum);
mv_bool DI_is_to_show_mova_messages();

/* Right Turners estimation */
uint8 DI_get_opposing_lane_num(uint8 laneNum, uint8 oppLaneOrder); /*RTOPPO[lanes]*/
uint8 DI_get_opposing_lanes_count(uint8 laneNum);
uint8 DI_get_right_turn_storage_capacity(int laneNum); /*RTSTOR[lanes]*/
uint8 DI_get_right_turn_radius(int laneNum); /*RTRAD[lanes]*/
uint8 DI_get_right_turn_veh_proportion(int laneNum); /*RTPROP[lanes]*/


/* Setters of the data that can be calculated in the SF*/
void DI_set_max_min_green_time(uint8 laneNum, int32 val); /*maxmin[lanes]*/
void DI_set_moving_q_over_x_det(uint8 laneNum, int32 val); /*moveqx[lanes] <time>*/
void DI_set_oversat_critical_veh_count(uint8 laneNum, int32 val); /*osatcc*/
void DI_set_satflow_mean_gap(uint8 laneNum, int32 val); /*satgap*/
void DI_set_oversat_time(uint8 laneNum, int32 val); /*osattm*/
void DI_set_lane_critical_gap(uint8 laneNum, int32 val); /*critg[lanes]*/
void DI_set_time_to_use_comb_x(uint8 laneNum, int32 val); /*comtim[lanes]*/
void DI_set_bonus_green_recalc_time(uint8 linkNum, int32 val); /*bontim*/
void DI_set_bonus_base_count(uint8 laneNum, int32 val);	/*BONBC*/
void DI_set_satinc_time(uint8 laneNum, int32 val);	/*satinc*/
void DI_set_startup_lost_time(uint8 laneNum, int32 val); /*stlost[lanes]*/

#ifndef M8_ENABLED 
void DI_calculate_chksum();
#endif



#ifdef UNIT_TESTING

void DI_set_short_lane_lenght(uint8 laneNum, int32 val); /*dshort*/
void DI_set_short_link_marker(uint8 linkNum, int8 val); /*shortl*/
void DI_set_det_num(uint8 laneNum, uint8 detType, uint8 val); /*det[laneNum-1][detType]*/
void DI_set_oversat_det(uint8 laneNum, uint8  val); 
void DI_set_in_det_cruise_time(uint8 laneNum, int32 val); /*CRUSIN[lanes]*/
void DI_set_stages_count(uint8 val); /*STAGES*/
void DI_set_link_green_status(uint8 stageNum, uint8 linkNum, uint8  val); /*LGREEN*/
void DI_set_lane_weighting_factor(uint8 laneNum, int32 val); /*lanewf*/
void DI_set_total_green_time(int32 val); /*TOTALG*/
void DI_set_ped_max_1(uint8 stageNum, int32 val); /*PEDMAX1[stages]*/
void DI_set_ped_max_2(uint8 stageNum, int32 val); /*PEDMAX2[stages]*/
void DI_set_links_count(uint8 val); /*NLINKS*/
void DI_set_link_lanes_count(uint8 linkNum, uint8 val); /*llatot[links]*/
void DI_set_lane_num(uint8 linkNum, uint8 laneInLinkIdx, uint8  val); /*llanes[links][lanes]*/
void DI_set_stage_low_max_green_time(uint8 stageNum, int32 val); /*LOWMAX*/
void DI_set_stage_max_green_time(uint8 stageNum, int32 val); /*MAX*/
void DI_set_det_min_on_time_for_queue(uint8 laneNum, int32	val); /*QMINON[lanes]*/
void DI_set_scan_period(int32 val); /*scan*/
void DI_set_moving_q_over_in_det(uint8 laneNum, int32 val); /*MOVQIN[lanes]*/
void DI_set_lane_lengthened_critical_gap(uint8 laneNum, int32  val); /*PCRITG[lanes]*/
void DI_set_opposed_lane_num(uint8 laneNum, uint8 oppLaneOrder, uint8 val);
void DI_set_right_turn_storage_capacity(int laneNum, uint8 val);
void DI_set_right_turn_radius(int laneNum, uint8 val);
void DI_set_right_turn_veh_proportion(int laneNum, uint8 val);
void DI_set_lane_portion_of_critical_gap(uint8 laneNum, int32 val); /*pcritg[lanes]*/
void DI_set_oversat_critical_veh_count_compact(uint8 laneNum, int32 val); /*osatcx*/

#endif







#endif /*DATASET_INTERFACE_H*/