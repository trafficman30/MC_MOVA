/*******************************************************************************
*
*                      MOVA KERNEL ALGORITHM
*
*  FILE:         delay_and_stops_optimisation.h
*
*  TITLE:        Mova Kernel: CDelay  Stops Optimisation Algorithm
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

#if !defined (DELAY_AND_STOP_OPTIMISATION_H)
#define DELAY_AND_STOP_OPTIMISATION_H

#include "mova_constants.h"
#include "mova_types.h"

struct DelayAndStopsOptiAlgInput
{
	int32		(*shift_register)[REG_SIZE];	/* SHREGA[lanes][21] */

	int32 *	extra_green_time;	/*EXTRAG[links]*/
	int32 *	total_bonus_green;	/*TOTBON[links]*/
	int32 *	y_value;			/*YVALUE[links]*/
	
	/* Links */
	int8 *	ben_link_marker; /*BENLIN[links]*/ /* benefiting link marker */
	int32 *	link_smoothed_flow; /*LSMFLOW[links]*/

	/* Detectors */
	int32 *	red_count_x_det;	/*redcx[lanes]*/
	int32 *	red_count_in_det;	/*redcin[lanes]*/
	int8 *	det_suspicion_status; /*SUSDET[dets]*/
	int32 *	extra_veh_beyond_in_det; /*inxtra[lanes]*/

#ifdef M8_IMPORVED_BUS_PRIORITY
	int32 * bus_weighted_red_count; /*bus_weighted_red_count[lanes]*/
#endif

	/* Stages*/
	uint8		current_stage; /*CUSIG*/
	uint8		next_stage; /*SNEXT*/
		
	/* Markers*/
	int8	*	stage_demand_marker; /*STADEM[stages]*/
	int8	*	link_demand_marker; /*LINDEM[links]*/

	int8	*	stage_emergency_demand_marker; /*ESTDEM[stages]*/
	int8	*	stage_priority_demand_marker; /*PSTDEM[stages]*/

	/* Pedestrians specific*/
	int32		ped_wait_timer; /* nPedWaitTime*/

	/* Timers*/
	int32 * link_green_timer; /*lingre[links]*/
	int32 * cycle_time_timer; /*cycltm[links]*/
};

struct DelayAndStopsOptiAlgInput delayAndStopsOptiAlgInput;

mv_bool ALG_delay_and_stops_optimiser();

void ALG_ds_get_internel_data_for_message( int32 * benfitRef,
										int32 * disbenfitRef,
										int32 * predictedRedTimeRef,
										int32 * cycleDeltaRef,
										int32 * benFlowRateRef, /*[links]*/
										int32 * futureRedRef, /*[links]*/
										int32 * netBenFlowRateRef /*[links]*/	);

/* Getters*/
int32 get_lane_register_total(uint8 laneIdx);

#endif /*DELAY_AND_STOP_OPTIMISATION_H*/