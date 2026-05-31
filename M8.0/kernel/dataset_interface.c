/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         dataset_interface.c
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


#include "dataset_interface.h"
#include "mova_constants.h"

#include "gbltypes.h"
#include "dataset_handler.h"

extern void checks(Ccomshr const * const, int32 * const, int32 * const );

uint8 DI_get_lanes_count()
{
	return (uint8)Tcomshr->nlanes;
}

uint8 DI_get_link_lanes_count(uint8 linkNum) /*llatot[links]*/
{
	return (uint8)Tcomshr->llatot[linkNum-1];
}

uint8 DI_get_dets_count()
{
	return (uint8)Tcomshr->ndets;
}

uint8 DI_get_stages_count() /*STAGES*/
{
	return (uint8)Tcomshr->stages;
}

uint8 DI_get_links_count() /*NLINKS*/
{
	return (uint8)Tcomshr->nlinks;
}

uint8 DI_get_lane_num(uint8 linkNum, uint8 laneInLinkIdx) /*llanes[links][lanes]*/
{
	return (uint8)Tcomshr->llanes[linkNum-1][laneInLinkIdx];
}

uint8 DI_get_main_stage_num() /*mainst*/
{
	return (uint8)Tcomshr->mainst;
}

int32 DI_get_min_percent_sat(uint8 linkNum) /*minpcs*/
{
	return Tcomshr->minpcs[linkNum-1];
}


uint8 DI_get_link_main_lanes_count(uint8 linkNum) /*nmainl*/
{
	return (uint8)Tcomshr->nmainl[linkNum-1];
}

int8 DI_get_short_link_marker(uint8 linkNum) /*shortl*/
{
	return (int8)Tcomshr->shortl[linkNum-1];
}

int32 DI_get_short_lane_length(uint8 laneNum) /*dshort*/
{
	return Tcomshr->dshort[laneNum-1];
}


uint8 DI_get_det_num(uint8 laneNum, uint8 detType) /*det[laneNum-1][detType]*/
{
	return Tcomshr->det[laneNum-1][detType];
}

uint8 DI_get_in_det_num(uint8 laneNum)	/*det[][0]*/
{
	return Tcomshr->det[laneNum-1][IN_DET];
}

uint8 DI_get_x_det_num(uint8 laneNum)			/*det[][1]*/
{
	return Tcomshr->det[laneNum-1][X_DET];
}

uint8 DI_get_out_det_num(uint8 laneNum)		/*det[][2]*/
{
	return Tcomshr->det[laneNum-1][OUT_DET];
}

uint8 DI_get_in_sink_det_num(uint8 laneNum)	/*det[][3]*/
{
	return Tcomshr->det[laneNum-1][IN_SINK_DET];
}

uint8 DI_get_in_sink_2_det_num(uint8 laneNum)	/*det[][9]*/
{
	return Tcomshr->det[laneNum-1][IN_SINK2_DET];
}

uint8 DI_get_comb_x_det_num(uint8 laneNum)	/*det[][4]*/
{
	return Tcomshr->det[laneNum-1][COMB_X_DET];
}


uint8	DI_get_x_sink_det_num(uint8 laneNum)	/*det[][5]*/
{
	return Tcomshr->det[laneNum-1][X_SINK_DET];
}

uint8	DI_get_x_sink_2_det_num(uint8 laneNum)	/*det[][10]*/
{
	return Tcomshr->det[laneNum-1][X_SINK2_DET];
}


uint8	DI_get_alt_up_det_num(uint8 laneNum)	/*det[][6]*/
{
	return Tcomshr->det[laneNum-1][ALT_UP_DET];
}


uint8	DI_get_alt_down_det_num(uint8 laneNum)	/*det[][7]*/
{
	return Tcomshr->det[laneNum-1][ALT_DOWN_DET];
}

uint8	DI_get_stop_line_det_num(uint8 laneNum) /*det[][8]*/
{
	return Tcomshr->det[laneNum-1][STOP_LINE_DET];
}

uint8	DI_get_assoc_det_num(uint8 laneNum, uint8 assocDetNum)
{
	return Tcomshr->assocd[laneNum-1][assocDetNum-1];
}

/*Note: do not use this function without checking that this is a ped link first*/
uint8 DI_get_ped_det_num(uint8 linkNum)
{
	return Tcomshr->ltype[linkNum-1] % 100;
}

mv_bool DI_is_latched_ped_link(uint8 linkNum)
{
	return Tcomshr->ltype[linkNum-1] < 100;
}

uint16 DI_get_x_det_distance(uint8 laneNum)
{
	return (uint16)Tcomshr->dx[laneNum-1];
}

uint16 DI_get_in_det_distance(uint8 laneNum)
{
	return (uint16)Tcomshr->din[laneNum-1];
}


uint8 DI_get_wait_channel_num(uint8 linkNum) /*waitch*/
{
	return (uint8)Tcomshr->waitch[linkNum-1];
}

int32 DI_get_time_to_use_comb_x(uint8 laneNum)
{
	return Tcomshr->comtim[laneNum-1];
}

uint8 DI_get_oversat_det(uint8 laneNum) /*xosat*/
{
	return (uint8)Tcomshr->xosat[laneNum-1];
}

int32 DI_get_oversat_time(uint8 laneNum) /*osattm*/
{
	return Tcomshr->osattm[laneNum-1];
}

int32 DI_get_oversat_critical_veh_count(uint8 laneNum) /*osatcc*/
{
	return Tcomshr->osatcc[laneNum-1];
}

int32 DI_get_oversat_critical_veh_count_compact(uint8 laneNum) /*osatcx*/
{
	return Tcomshr->osatcx[laneNum-1];
}

int32 DI_get_cruise_speed(uint8 laneNum) /*cpeed*/
{
	return Tcomshr->cspeed[laneNum-1];
}

int32 DI_get_lane_weighting_factor(uint8 laneNum) /*lanewf*/
{
	return Tcomshr->lanewf[laneNum-1];
}


int32	DI_get_det_min_on_time_for_queue(uint8 laneNum) /*QMINON[lanes]*/
{
	return Tcomshr->qminon[laneNum-1];
}

int32 DI_get_moving_q_over_in_det(uint8 laneNum) /*MOVQIN[lanes]*/
{
	return Tcomshr->movqin[laneNum-1];
}

int32 DI_get_moving_q_over_x_det(uint8 laneNum) /*MOVEQX*/
{
	return Tcomshr->moveqx[laneNum-1];
}

int32 DI_get_x_det_cruise_time(uint8 laneNum) /*CRUSX[lanes]*/
{
	return Tcomshr->crusx[laneNum-1];
}

int32 DI_get_in_det_cruise_time(uint8 laneNum) /*CRUSIN[lanes]*/
{
	return Tcomshr->crusin[laneNum-1];
}

int32 DI_get_gamber_time(uint8 laneNum) /*GAMBER[lanes]*/
{
	return Tcomshr->gamber[laneNum-1];
}

int32 DI_get_satinc_time(uint8 laneNum) /*SATINC[lanes]*/
{
	return Tcomshr->satinc[laneNum-1];
}

int32 DI_get_startup_lost_time(uint8 laneNum) /*STLOST[lanes]*/
{
	return Tcomshr->stlost[laneNum-1];
}

int32 DI_get_min_veh_count_between_x_and_out_det(uint8 linkNum)
{
	return Tcomshr->mixout[linkNum-1];
}

int32 DI_get_call_time(uint8 linkNum) /*mxcall[links]*/
{
	return Tcomshr->mxcall[linkNum - 1];
}

int32 DI_get_cancel_time(uint8 linkNum) /*mxcncl[links]*/
{
	return Tcomshr->mxcncl[linkNum - 1];
}


uint8 DI_get_link_green_status(uint8 stageNum, uint8 linkNum) /*LGREEN[stages][links]*/
{
	return Tcomshr->lgreen[stageNum-1][linkNum-1];
}

int32 DI_get_stage_change_allowance(uint8 currentStageNum, uint8 nextStageNum) /* CHANGE[stages][stages]*/
{
	return Tcomshr->change[currentStageNum-1][nextStageNum-1];
}

int16 DI_get_sdcode(uint8 stageNum, uint8 linkNum) /*SDCODE*/
{
	return (int16)Tcomshr->sdcode[stageNum-1][linkNum-1];
}

int32 DI_get_lost_time(uint8 linkNum) /*LOSTIM*/
{
	return Tcomshr->lostim[linkNum-1];
}

int32 DI_get_stage_min_green_time(uint8 stageNum) /*MIN*/
{
	return Tcomshr->min[stageNum-1];
}

int32 DI_get_stage_max_green_time(uint8 stageNum) /*MAX*/
{
	return Tcomshr->max[stageNum-1];
}

int32 DI_get_stage_low_max_green_time(uint8 stageNum) /*LOWMAX*/
{
	return Tcomshr->lowmax[stageNum-1];
}

int32 DI_get_link_min_green_time(uint8 linkNum) /*LMIN[links]*/
{
	return Tcomshr->lmin[linkNum-1];
}

#ifdef M8_LINK_MAXIMUM
int32 DI_get_link_max_green_time(uint8 linkNum) /*LNKMAX[links]*/
{
	return Tcomshr->lnkmax[linkNum - 1];
}
#endif


int32 DI_get_link_endsat_max_green(uint8 linkNum) /*ESLMAX[links]*/
{
	return Tcomshr->eslmax[linkNum-1];
}

int32 DI_get_max_min_green_time(uint8 laneNum) /*MAXMIN[lanes]*/
{
	return Tcomshr->maxmin[laneNum-1];
}

int32 DI_get_ped_max_1(uint8 stageNum) /*PEDMAX1*/
{
	return Tcomshr->pedmax[0][stageNum-1];
}

int32 DI_get_ped_max_2(uint8 stageNum) /*PEDMAX2*/
{
	return Tcomshr->pedmax[1][stageNum-1];
}

int32 DI_get_total_green_time() /*TOTALG*/
{
	return Tcomshr->totalg;
}

int32 DI_get_lane_critical_gap(uint8 laneNum) /*critg[lanes]*/
{
	return Tcomshr->critg[laneNum-1];
}

#ifdef M8_PREMATURE_ENDSAT
int32 DI_get_lane_lengthened_critical_gap(uint8 laneNum) /*pcritg[lanes]*/
{
	return Tcomshr->pcritg[laneNum-1];
}
#endif

#ifdef M8_RE_ESTABLISHMENT_OF_SATFLOW
int32 DI_get_lane_portion_of_critical_gap(uint8 laneNum) /*rcritg[lanes]*/
{
	return Tcomshr->rcritg[laneNum-1];
}
#endif

int32 DI_get_lane_satflow_gap(uint8 laneNum) /*satgap[lanes]*/
{
	return Tcomshr->satgap[laneNum-1];
}

int32 DI_get_bonus_green_recalc_time(uint8 linkNum) /*BONTIM*/
{
	return Tcomshr->bontim[linkNum-1];
}

int32	DI_get_bonus_base_count(uint8 laneNum)	/*BONBC*/
{
	return Tcomshr->bonbc[laneNum-1];
}

int8 DI_get_bonus_enabling_marker(uint8 linkNum) /*BONCUT*/
{
	return (int8)Tcomshr->boncut[linkNum-1];
}

int32 DI_get_stop_penalty(uint8 linkNum) /*STOPEN[links]*/
{
	return Tcomshr->stopen[linkNum-1];
}

int32 DI_get_min_extension() /*MINEXT for the delay_and_stops optimisation*/
{
	return Tcomshr->minext;
}

int32 DI_get_max_extension() /*MAXEXT for the delay_and_stops optimisation*/
{
	return Tcomshr->maxext;
}

/* Links types functions */
int32 DI_get_link_type(uint8 linkNum)
{
	return  Tcomshr->ltype[linkNum-1];
}

mv_bool DI_is_traffic_link(uint8 linkNum) /*ltype == 0*/
{
	return Tcomshr->ltype[linkNum-1] == 0;
}

mv_bool DI_is_ped_link(uint8 linkNum) /*ltype > 0*/
{
	return Tcomshr->ltype[linkNum-1] > 0;
}

mv_bool DI_is_ep_link(uint8 linkNum)
{
	/* Emergency link type = -4,  Priority link type = -5 */
	return Tcomshr->ltype[linkNum-1] < 0;
}

mv_bool DI_is_emeregency_link(uint8 linkNum)
{
	return Tcomshr->ltype[linkNum-1] == -4;
}

mv_bool DI_is_priority_link(uint8 linkNum)
{
	return Tcomshr->ltype[linkNum-1] == -5;
}


#ifdef M8_IMPORVED_BUS_PRIORITY

int32 DI_get_priority_facility_matrix(uint8 stageNum, uint8 linkNum) /*PFACILM[stages][links]*/
{
	return Tcomshr->pfacilm[stageNum-1][linkNum-1];
}

uint8 DI_get_associated_priority_link_num_for_a_lane(uint8 assoc, uint8 laneNum)
{
	return (uint8)Tcomshr->ascprl[assoc][laneNum -1];
}

uint32 DI_get_ep_det_cruise_time(uint8 epLinkNum, uint8 epDetNum)
{
	/*NOTE: epDetNum starts with 1, and it is not the ID*/
	return Tcomshr->epcrt[epDetNum-1][epLinkNum-1];
}

int32 DI_get_bus_weight(uint8 epLinkNum)
{
	return Tcomshr->buswgt[epLinkNum-1];
}

int32 DI_get_bus_max_1(uint8 stageNum) /*BUSMAX1*/
{
	return Tcomshr->busmax[0][stageNum-1];
}

int32 DI_get_bus_max_2(uint8 stageNum) /*BUSMAX2*/
{
	return Tcomshr->busmax[1][stageNum-1];
}

#endif /*M8_IMPORVED_BUS_PRIORITY*/

#ifdef M8_IMPROVED_LINKING
int32 DI_get_upstream_junction_intergreen() /*UPSMIG (10th of a second)*/
{
	return 0;
}

int32 DI_get_link_distance_from_to_stoplines() /*LKDIST (meters)*/
{
	//return 70;
}

int32 DI_get_assumed_acceleration_to_reach_cruise_speed() /*a (in m/s^2)*/
{
	return 10;
}

int32 DI_get_vehicle_length(uint8 laneNum) /*VLEN (meters)*/
{
	//return Tcomshr->vlen[laneNum-1];
	return 6;
}

#endif

/* End of links types functions*/


int32 DI_get_scan_period() /*scan*/
{
	return Tcomshr->scan;
}

int32 DI_get_reduced_critical_gap() /*subgap*/
{
	return Tcomshr->subgap;
}

int32 DI_get_additional_critical_gap() /*addgap*/
{
	return Tcomshr->addgap;
}

int32 DI_get_satflow_mean_gap(uint8 laneNum) /*satgap*/
{
	return Tcomshr->satgap[laneNum-1];
}

mv_bool DI_is_long_lane(uint8 laneIdx)
{
	return Tcomshr->dshort[laneIdx] == COMPACT_MOVA_LANE || Tcomshr->det[laneIdx][IN_DET] >0;
}

/*E/P functions*/
#ifndef M8_IMPORVED_BUS_PRIORITY
int16 DI_get_priority_facility(uint8 stageNum) /*PFACIL[stages]*/
{
	return Tcomshr->pfacil[stageNum-1];
}
#endif

int32 DI_get_emergency_ext_max(uint8 stageNum) /*EMXMAX[stages]*/
{
	return Tcomshr->emxmax[stageNum-1];
}

int32 DI_get_priority_ext_max(uint8 stageNum) /*PRXMAX[stages]*/
{
	return Tcomshr->prxmax[stageNum-1];
}

uint8 DI_get_ep_det_num(uint8 linkNum, uint8 epDetNum) /*EPDET[links][2]*/
{
	return (uint8)Tcomshr->epdet[linkNum-1][epDetNum-1];
}

uint8 DI_get_ep_cancel_det_num(uint8 linkNum) /*CANDET[links]*/
{
	return (uint8)Tcomshr->candet[linkNum-1];
}

int32 DI_get_ep_ext_time(uint8 linkNum, uint8 epDetNum) /*EPEXT[links][2]*/
{
	return Tcomshr->epext[linkNum-1][epDetNum-1];
}

int32 DI_get_ep_hold_time(uint8 linkNum) /*HOLDTM[links]*/
{
	return Tcomshr->holdtm[linkNum-1];
}


/*Sat flow recalculations functions*/
int32 DI_get_satflow_start_time(uint8 periodNum)
{
	return Tcomshr->sftims[periodNum-1][0];
}

int32 DI_get_satflow_end_time(uint8 periodNum)
{
	return Tcomshr->sftims[periodNum-1][1];
}

int32 DI_is_to_use_calc_satinc_time(uint8 laneNum)
{
	return Tcomshr->usesf[laneNum-1];
}

int32 DI_is_to_use_calc_startup_lost_time(uint8 laneNum)
{
	return Tcomshr->usest[laneNum-1];
}

mv_bool DI_is_to_show_mova_messages()
{
	return (mv_bool)(Tcomshr->io[USER_OUTPUT_FLAG] == 23);
}

/* Right Turners estimation */
uint8 DI_get_opposing_lane_num(uint8 laneNum, uint8 oppLaneOrder)
{
	return (uint8)Tcomshr->rtoppo[laneNum-1][oppLaneOrder];
}

uint8 DI_get_opposing_lanes_count(uint8 laneNum)
{
	uint8 i;

	for(i=0; i<MAX_OPP_LANES; i++)
	{
		if(Tcomshr->rtoppo[laneNum-1][i] == 0)
			return i;
	}

	return MAX_OPP_LANES;
}

uint8 DI_get_right_turn_storage_capacity(int laneNum)
{
	return (uint8)Tcomshr->rtstor[laneNum-1];
}

uint8 DI_get_right_turn_radius(int laneNum)
{
	return (uint8)Tcomshr->rtrad[laneNum-1];
}

uint8 DI_get_right_turn_veh_proportion(int laneNum)
{
	return (uint8)Tcomshr->rtprop[laneNum-1];
}


/* Setters of the data that can be calculated in the SF*/
void DI_set_max_min_green_time(uint8 laneNum, int32 val) /*maxmin[lanes]*/
{
	Tcomshr->maxmin[laneNum-1] = val;

	g_data_plans[DSH_get_active_data_plan_number() - 1].maxmin[laneNum - 1] = val;
}

void DI_set_moving_q_over_x_det(uint8 laneNum, int32 val) /*moveqx[lanes] <time>*/
{
	Tcomshr->moveqx[laneNum-1] = val;

	g_data_plans[DSH_get_active_data_plan_number() - 1].moveqx[laneNum - 1] = val;
}

void DI_set_oversat_critical_veh_count(uint8 laneNum, int32 val) /*osatcc*/
{
	Tcomshr->osatcc[laneNum-1] = val;

	g_data_plans[DSH_get_active_data_plan_number() - 1].osatcc[laneNum - 1] = val;
}

void DI_set_satflow_mean_gap(uint8 laneNum, int32 val) /*satgap*/
{
	Tcomshr->satgap[laneNum-1] = val;

	g_data_plans[DSH_get_active_data_plan_number() - 1].satgap[laneNum - 1] = val;
}

void DI_set_oversat_time(uint8 laneNum, int32 val) /*osattm*/
{
	Tcomshr->osattm[laneNum-1] = val;

	g_data_plans[DSH_get_active_data_plan_number() - 1].osattm[laneNum - 1] = val;
}

void DI_set_lane_critical_gap(uint8 laneNum, int32 val) /*critg[lanes]*/
{
	Tcomshr->critg[laneNum-1] = val;

	g_data_plans[DSH_get_active_data_plan_number() - 1].critg[laneNum - 1] = val;
}

void DI_set_time_to_use_comb_x(uint8 laneNum, int32 val) /*comtim[lanes]*/
{
	Tcomshr->comtim[laneNum-1] = val;

	g_data_plans[DSH_get_active_data_plan_number() - 1].comtim[laneNum - 1] = val;
}

void DI_set_bonus_green_recalc_time(uint8 linkNum, int32 val) /*bontim*/
{
	Tcomshr->bontim[linkNum-1] = val;

	g_data_plans[DSH_get_active_data_plan_number() - 1].bontim[linkNum - 1] = val;
}

void DI_set_bonus_base_count(uint8 laneNum, int32 val)	/*BONBC*/
{
	Tcomshr->bonbc[laneNum-1] = val;

	g_data_plans[DSH_get_active_data_plan_number() - 1].bonbc[laneNum - 1] = val;
}

void DI_set_satinc_time(uint8 laneNum, int32 val)	/*satinc*/
{
	Tcomshr->satinc[laneNum-1] = val;

	g_data_plans[DSH_get_active_data_plan_number() - 1].satinc[laneNum - 1] = val;
}

void DI_set_startup_lost_time(uint8 laneNum, int32 val) /*stlost[lanes]*/
{
	Tcomshr->stlost[laneNum-1] = val;

	g_data_plans[DSH_get_active_data_plan_number() - 1].stlost[laneNum - 1] = val;
}




#ifndef M8_ENABLED 
void DI_calculate_chksum(void)
{
	/* Feedback of satflow data can modify fixed data items in TComshr,
	   therefore the checksum should be recalculated */

	long calcFixedChksum = 0;
	long oldFixedChecksum = (Tcomshr->chksm1[0] * 1000L) + Tcomshr->chksm1[1];
	long csum_diff = 0;
	long calcAllChksum = (Tcomshr->chksum[0] * 1000L) + Tcomshr->chksum[1];

	checks(Tcomshr, &calcFixedChksum, NULL);

	/* Copy new chksum into store */
	Tcomshr->chksm1[0] = (int)(calcFixedChksum / 1000);
	Tcomshr->chksm1[1] = (int)(calcFixedChksum % 1000);

	/* Update the all data checksum too */
	/* Can't simply recalculate as all the variable data has changed.
	   Instead, change it by the same amount as the fixed checksum was changed. */
	csum_diff = calcFixedChksum - oldFixedChecksum;
	calcAllChksum += csum_diff;

	Tcomshr->chksum[0] = (int)(calcAllChksum / 1000);
	Tcomshr->chksum[1] = (int)(calcAllChksum % 1000);
}
#endif


#ifdef UNIT_TESTING

void DI_set_short_lane_lenght(uint8 laneNum, int32 val)  /*dshort*/
{
	Tcomshr->dshort[laneNum-1] = val;
}

void DI_set_short_link_marker(uint8 linkNum, int8 val) /*shortl*/
{
	Tcomshr->shortl[linkNum-1] = (int)val;
}

void DI_set_det_num(uint8 laneNum, uint8 detType, uint8 val) /*det[laneNum-1][detType]*/
{
	Tcomshr->det[laneNum-1][detType] = val;
}

void DI_set_oversat_det(uint8 laneNum, uint8  val)
{
	Tcomshr->xosat[laneNum-1] = val;
}

void DI_set_in_det_cruise_time(uint8 laneNum, int32 val) /*CRUSIN[lanes]*/
{
	Tcomshr->crusin[laneNum-1] = val;
}

void DI_set_stages_count(uint8 val) /*STAGES*/
{
	Tcomshr->stages = (int32)val;
}

void DI_set_link_green_status(uint8 stageNum, uint8 linkNum, uint8  val) /*LGREEN[stages][links]*/
{
	Tcomshr->lgreen[stageNum-1][linkNum-1] = val;
}

void DI_set_lane_weighting_factor(uint8 laneNum, int32 val) /*lanewf*/
{
	Tcomshr->lanewf[laneNum-1] = (int)val;
}

void DI_set_total_green_time(int32 val) /*TOTALG*/
{
	Tcomshr->totalg = val;
}

void DI_set_ped_max_1(uint8 stageNum, int32 val) /*PEDMAX1[stages]*/
{
	Tcomshr->pedmax[0][stageNum-1] = val;
}

void DI_set_ped_max_2(uint8 stageNum, int32 val) /*PEDMAX2[stages]*/
{
	Tcomshr->pedmax[1][stageNum-1] = val;
}

void DI_set_links_count(uint8 val) /*NLINKS*/
{
	Tcomshr->nlinks = val;
}

void DI_set_link_lanes_count(uint8 linkNum, uint8 val) /*llatot[links]*/
{
	Tcomshr->llatot[linkNum-1] = val;
}

void DI_set_lane_num(uint8 linkNum, uint8 laneInLinkIdx, uint8  val) /*llanes[links][lanes]*/
{
	Tcomshr->llanes[linkNum-1][laneInLinkIdx] = val;
}

void DI_set_stage_low_max_green_time(uint8 stageNum, int32 val) /*LOWMAX*/
{
	Tcomshr->lowmax[stageNum-1] = val;
}

void DI_set_stage_max_green_time(uint8 stageNum, int32 val) /*MAX*/
{
	Tcomshr->max[stageNum-1] = val;
}

void DI_set_det_min_on_time_for_queue(uint8 laneNum, int32	val) /*QMINON[lanes]*/
{
	Tcomshr->qminon[laneNum-1] = val;
}

void DI_set_scan_period(int32 val) /*scan*/
{
	Tcomshr->scan = val;
}

void DI_set_moving_q_over_in_det(uint8 laneNum, int32 val) /*MOVQIN[lanes]*/
{
	Tcomshr->movqin[laneNum-1] = val;
}

void DI_set_lane_lengthened_critical_gap(uint8 laneNum, int32  val) /*PCRITG[lanes]*/
{
	Tcomshr->pcritg[laneNum-1] = val;
}

void DI_set_opposed_lane_num(uint8 laneNum, uint8 oppLaneOrder, uint8 val)
{
	Tcomshr->rtoppo[laneNum-1][oppLaneOrder] = val;
}

void DI_set_right_turn_storage_capacity(int laneNum, uint8 val)
{
	Tcomshr->rtstor[laneNum-1] = val;
}

void DI_set_right_turn_radius(int laneNum, uint8 val)
{
	Tcomshr->rtrad[laneNum-1] = val;
}

void DI_set_right_turn_veh_proportion(int laneNum, uint8 val)
{
	Tcomshr->rtprop[laneNum-1] = val;
}

void DI_set_lane_portion_of_critical_gap(uint8 laneNum, int32 val) /*rcritg[lanes]*/
{
	Tcomshr->rcritg[laneNum-1] = val;
}

void DI_set_oversat_critical_veh_count_compact(uint8 laneNum, int32 val) /*osatcx*/
{
	Tcomshr->osatcx[laneNum-1] = val;
}

#endif