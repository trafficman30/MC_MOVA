/*******************************************************************************
*
*                      MOVA KERNEL ALGORITHM
*
*  FILE:         estimate_right_turners_count_during_opposed_stage.c
*
*  TITLE:        Mova Kernel: Estimate Right Turners Count Algorithm
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	19-Mar-2014		8.0.0		AA		IA		......			Newly created
*
*  (c) Copyright TRL Ltd 2014. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/

#include <math.h>

#ifdef M8_RT_OPTIMISATION

#include "estimate_right_turners_count_during_opposed_stage.h"

#include "core_interface.h"
#include "dataset_interface.h"
#include "timers_manager.h"



/* This algorithm will used the following dataset values:
		- nl : DI_get_opposing_lane_num(uint8 laneNum, uint8 oppLaneOrder);
		- So : will use DI_get_satinc_time(uint8 laneNum) to calc it.
		- Ns : DI_get_right_turn_storage_capacity(int laneNum);
		- r  : DI_get_right_turn_radius(int laneNum);
		- f  : DI_get_right_turn_veh_proportion(int laneNum);
		- S0 : will use DI_get_satinc_time(uint8 laneNum) to calc it.		
*/


/** The number of vehs that managed to turn tight during the IG.*/
static uint8	veh_count_turned_during_intergreen = 0;

/** The number of vehs that managed to turn tight during the opposing stage.*/
static uint8	veh_count_turned_during_opposing_stage = 0;


/*!
*	\brief	Implements the algorithm of estimating the number of the veh
*			which will turn right during the opposed stage.
*			This function is an implementation of the RR67 research report
*			fomulas. Seciton 5 includes a summary of the used equations.
*			The var names in this function matches the RR67 report notations.
*
*	\param[in]	stageNum			The stage number of where the RT lane was opposed.
*	\param[in]	laneNum				The number of the lane that contains the RT (opposed).
*	\param[in]	netGapsTime[]		An array that holds the net gaps timings during the opposing.
*	\param[in]	netGapsCount		The size of the netGapsTime array.
*
*	\return		An estimation of the number of vehs that turned right during the opposing stage.
*
*	\author		Islam Abdelhalim 
*	\date		19-03-2014 */
uint8 ALG_estimate_right_turners_count_during_opposed_stage(uint8 stageNum, uint32 laneNum, int32 * netGapsTime, uint8 netGapsCount)
{
	uint8 i; /* gaps loop variable */

	int32 S0; /* Satflow for the lane that contains the RT*/
	int32 So = 0; /* Satflow for the opposing lane(s) */
	uint8 nl; /* Number of lanes on opposing arm */
	uint8 f; /* proportion of turning veh in a lane */ 
	uint8 Ns; /* storage capacity*/
	real32 lambda;	/* proportion of the cycle which is effectively green for the phase under consideration*/

	/* The following vars will be calculated in the function as an interm params. */
	real32 qo;
	real32 Xo;
	real32 t1;
	real32 t2;
	real32 T;
	real32 Sg;
	real32 Sc;
	real32 S2;

	real32 veh_per_gap_ratio;
	uint8  veh_per_gap_count;
	real32 remainder = 0.0;

	//TOCHECK
	const real32 P = 0.9; 
	const real32 FUDGE_FACTOR = 0.2; 


	/* First, prepare some vars that will be used in the equations.*/
	nl = DI_get_opposing_lanes_count(laneNum);
	f  = DI_get_right_turn_veh_proportion(laneNum);
	Ns = DI_get_right_turn_storage_capacity(laneNum);

	p_divide_error((timers.stage_cycle_timer[stageNum-1]), 701, 
					"stage_cycle_timer was zero during calc LAMBDA for stage number %d", timers.stage_cycle_timer[stageNum-1], stageNum);

	lambda = timers.signal_state_timer / (real32)timers.stage_cycle_timer[stageNum-1];

	//TOCHECK: is that approx. is valid (int div) and S0 is int.
	p_divide_error(DI_get_satinc_time(laneNum), 702, "SATINC was zero for lane number %d during calc of S0", laneNum);
	S0 = 36000 / DI_get_satinc_time(laneNum);

	/*Calculating So for all the opposing lanes*/
	for(i=0; i<nl; i++)
	{
		//TOCHECK: is that approx. is valid (int div) and So is int.
		p_divide_error(DI_get_satinc_time(DI_get_opposing_lane_num(laneNum, i)), 703, "SATINC was zero for lane number %d during calc of So", DI_get_opposing_lane_num(laneNum, 0));
		So += (36000 / DI_get_satinc_time( DI_get_opposing_lane_num(laneNum, i)));
	}


	veh_count_turned_during_opposing_stage = 0;

	/* Now, apply RR67 equations using the previous vars. */
	for(i=0; i< netGapsCount; i++)
	{
		/* 1. Calculate the flow on the opposite arm (qo) */
		p_divide_error(netGapsTime[i], 704, "netGapsTime was zero for gap number %d during calc q0", i);

		qo = 36000.0 / (real32)netGapsTime[i];

		/* 2. Calculate the degree of saturation on opposing arm:
			  the ratio of of the flow on the opposing arm to 
			  the sat flow on that arm.	(Xo) */
		p_divide_error(lambda*nl*So, 705, "lambda=%f, nl=%d and So=%d for gap number %d during calc Xo", lambda, nl, So, i);
		
		Xo =qo /(lambda * So);

		/* if X0 is above 81%, then we can assume no turning in gaps and do not need to carry out the calculation*/
		if(Xo > 0.81)
			continue;
			
		/* 3. Calculate t1 */
		t1 = 12*(pow(Xo,2)) / (1+(0.6*(1-f))*Ns);


		/* 4. Calculate t2 */
		t2 = 1 - ( pow(f*Xo,2) );

		/* 5. Calculate the Through Car Unit (TCU) factor of a turning 
			  veh in a lane of mixed turning traffic. Each turning veh is 
			  equivalent to T staright-ahead vehs. (T) */
		p_divide_error(DI_get_right_turn_radius(laneNum), 706, "right_turn_radius was zero for lane number %d", laneNum);

		T = 1 + 1.5/(real32)DI_get_right_turn_radius(laneNum) + t1/t2;


		/*6. Calculate the sat flow in lanes of opposed mixed traffic
			 during the effective green period (pcu/h) (Sg) */
		p_divide_error((1+(T-1)*f), 708, "T was %f and f was %d for gap number %d during calc Sg", T, f, i);

		Sg = (S0-230) / ( 1+ (T-1)*f );


		/* 7. Calculate the sat flow in lanes of opposed mixed 
			  traffic AFTER the effective green period (veh/h) (Sc) */
		p_divide_error((lambda*timers.stage_cycle_timer[stageNum-1]), 709, 
			"LAMBDA was %f and stage_cycle_timer was %d for stage number %d",
			lambda, timers.stage_cycle_timer[stageNum-1], stageNum);

		Sc = P * (1+Ns) * (pow((f*Xo),(real32)0.2)) * 36000.0/ (lambda * timers.stage_cycle_timer[stageNum-1]);


		/* 8. Calculate the sat flow of stram containing opposed RT traffic for this lane (S2) */
		S2 = Sg + Sc;

		/* 9. Calculate the number of veh that could turn during this gap (veh_per_gap)*/
		veh_per_gap_ratio = S2 / 36000.0 * netGapsTime[i];
		
		veh_per_gap_count = (uint8)(veh_per_gap_ratio + remainder);
		veh_count_turned_during_opposing_stage += veh_per_gap_count;

		//TOCHECK: is using '-' is eqv to MOD(x, 1) ?
		remainder = (veh_per_gap_ratio + remainder) - veh_per_gap_count;		
	}

	veh_count_turned_during_intergreen =  (uint8)(remainder + FUDGE_FACTOR);

	veh_count_turned_during_opposing_stage += veh_count_turned_during_intergreen;

	return veh_count_turned_during_opposing_stage;
}

void ALG_rt_get_internel_data_for_message(uint8 * vehCountTurnedDuringIG, uint8 * vehCountTurnedDuringOpposingStage)
{
	*vehCountTurnedDuringIG = veh_count_turned_during_intergreen;
	*vehCountTurnedDuringOpposingStage = veh_count_turned_during_opposing_stage;
}

void ALG_rt_reset_internel_data_for_message()
{
	veh_count_turned_during_intergreen = 0;
	veh_count_turned_during_opposing_stage = 0;
}

#endif