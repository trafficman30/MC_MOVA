/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         capacity_maximisation.c
*
*  TITLE:        Mova Kernel: Capacity Maximisation Algorithm
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

#include "mova_constants.h"
#include "capacity_maximisation.h"
#include "dataset_interface.h"
#include "core_interface.h"


static mv_bool flow_eff_factor_test(uint8 linkIdx, int32 * flow_eff_factor);
static mv_bool short_term_eff_factor_test(	uint8   linkIdx, 
										int32 * effFactor,
										int32   excessGap,
										int32	  satflowHeadway,
										mv_bool  isSupersatLane,
										uint8   nLinkMOVQINLaneNum,
										uint8 * laneNum);
static void upper_register_test(uint8 linkIdx,
								uint8 laneNum,
								int32 effFactor,
								int32 registerTime,
								int32 satflowHeadway,
								int32 registerVeh,
								mv_bool isSupersatLane);


static struct CapacityMaxAlgInput * pData;
static int8 link_endsat; /* to be returned at the end of the algorithm running*/


/*!
*	\brief	Implements the Capacity Maximisation algorithm.
*
*	\param[in]	linkIdx					The link index of the link to apply this algorithm on it.
*	\param[in]  currentLinkEndsatMarker	The current link endsat marker value.
*	\param[in]  pDataRef				A pointer to this algorithm stuct that holds all the required parameters to do the calculations.
*
*	\return		The updated link endsat marker value. 
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
int8 ALG_capacity_maximisation(uint8 linkIdx, int8 currentLinkEndsatMarker, 
								struct CapacityMaxAlgInput * pDataRef)
{
	uint8 lane_num = 0;

	pData = pDataRef;

	link_endsat = currentLinkEndsatMarker;

	/*1- The flow-efficiency factor test*/
	if(!flow_eff_factor_test(linkIdx, &pData->flow_eff_factor))
	{
		/*2- the short-term-efficient factor test */
		if(!short_term_eff_factor_test(linkIdx,
										&pData->flow_eff_factor,
										pData->excess_gap,
										pData->satflow_headway,
										pData->is_supersat_lane,
										pData->nLinkMOVQINLaneNum,
										&lane_num))
		{
			/* 3- the upper register test */
			upper_register_test(linkIdx,
								lane_num,
								pData->flow_eff_factor,
								pData->register_time,
								pData->satflow_headway,
								pData->register_veh,
								pData->is_supersat_lane);
		}
	}

	return link_endsat;
}





/*!
*	\brief	Applies the Flow Efficiency Factor test.
*
*	\param[in]	linkIdx				The link index of the link to apply this test on it.
*	\param[out]  flow_eff_factor	The Flow Efficiency Factor.
*
*	\return		TRUE if the endsdat marker was set. 
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
static mv_bool flow_eff_factor_test(uint8 linkIdx, int32 * flow_eff_factor)
{
	int32 green_time; /*g*/
	int32 reduced_lost_time; /*d*/
	int32 tolerance; /*d*/

	int32 cjl;
	int32 b;

	green_time = pData->link_green_timer[linkIdx];
	reduced_lost_time = pData->reduced_lost_time[linkIdx];

	/* EFFECTIVE-GREEN INCLUDES BONUS ONLY AFTER BONUS-CALC-TIME */
	if(green_time >= DI_get_bonus_green_recalc_time(linkIdx+1) && 
		DI_get_bonus_green_recalc_time(linkIdx+1) != 0)
	{
		green_time += pData->smoothed_bonus_green_time[linkIdx];

		p_divide_error(DI_get_lost_time(linkIdx+1), 404, "link number was %d", (linkIdx+1));

		/*  CORRECT REDLT FOR BONUS. RESULTS IN SMALLER VALUE */
		reduced_lost_time = (DI_get_lost_time(linkIdx+1) - pData->smoothed_bonus_green_time[linkIdx])*50/DI_get_lost_time(linkIdx+1)*pData->reduced_lost_time[linkIdx]/50;

		if(reduced_lost_time  < 0) 
			reduced_lost_time = 0;
	}

	p_divide_error(DI_get_link_main_lanes_count(linkIdx+1), 405,  "link number was %d", (linkIdx+1));

	/*  ADJUST WASTE-TIME TO VALUE PER MAIN LANE */
	b = pData->link_waste_green_timer[linkIdx] / DI_get_link_main_lanes_count(linkIdx+1);

	cjl = green_time-b;
	if(cjl > 3276) 
		cjl = 3276; /* ?? fix ?? !*/

	p_divide_error((int)((green_time+reduced_lost_time+5)/10), 406, "green_time was %d, reduced_lost_time was %d", green_time, reduced_lost_time);

	/* ROUNDING ERROR REDUCED BY +5 ABOVE*/
	*flow_eff_factor = cjl*10/((green_time+reduced_lost_time+5)/10);

	if(*flow_eff_factor > 99) 
		*flow_eff_factor = 99;

	/* JUMP UNLESS 'flow_eff_factor' IS DECREASING, WHEN NEED TO CHECK FALL-OFF */
	if(*flow_eff_factor >= pData->max_percent_sat[linkIdx])
	{
		pData->max_percent_sat[linkIdx] = *flow_eff_factor;
	}
	else
	{
		p_divide_error(green_time+reduced_lost_time, 407,"green_time was %d, reduced_lost_time was %d", green_time, reduced_lost_time );

		/* 'tolerance' IS THE PERCENTAGE TOLERANCE ON 'flow_eff_factor' TO NOISE */
		tolerance = 3200/(green_time+reduced_lost_time);

		/* Check if the flow_eff_factor fall-off the tolerance */
		if(pData->max_percent_sat[linkIdx]- (*flow_eff_factor) > tolerance)
		{
			link_endsat = LINK_ENDSAT_LOW_FLOW_EFFICIENCY_FACTOR_WHILE_OVERSAT;
			return mv_true; /*DO NOT CONTINUE TO THE NEXT CHECK*/
		}
	}

	return mv_false;
}



/*!
*	\brief	Applies the Short Term Efficiency Factor test.
*
*	\param[in]	linkIdx				The link index of the link to apply this test on it.
*	\param[out]	effFactor			The Flow Efficiency Factor.
*	\param[in]	excessGap			The excess gap.
*	\param[in]	satflowHeadway		The satflow headway.
*	\param[in]	isSupersatLane		Flag to detrmin if the lane is super saturated.
*	\param[in]	nLinkMOVQINLaneNum	The lane number.
*	\param[out]	laneNum				The output lane number.
*
*	\return		TRUE if the endsdat marker was set. 
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
static mv_bool short_term_eff_factor_test(	uint8   linkIdx, 
										int32 * effFactor,
										int32   excessGap,
										int32   satflowHeadway,
										mv_bool	isSupersatLane,
										uint8	nLinkMOVQINLaneNum,
										uint8 * laneNum)
{

	/* NOW CHECK SHORT-TERM LINK EFFICIENCY WITH TEST EFF. 'A' */

	if(*effFactor < DI_get_min_percent_sat(linkIdx+1))
		*effFactor = DI_get_min_percent_sat(linkIdx+1);

	/* Compact MOVA: Set m to the lane with the greatest MOVQIN in the link may be zero if this is a short link 
       or long link without IN-dets) */
	*laneNum = nLinkMOVQINLaneNum;
          
	/* CORRECT MULTILANE LINKS FOR SOME INDEPENDENCE BETWEEN LANES*/
	excessGap = excessGap +(DI_get_link_main_lanes_count(linkIdx+1)-1)*10;

	if(satflowHeadway + excessGap > 0)
	{
		p_divide_error(satflowHeadway + excessGap, 408, "satflow_headway was %d, excess_gap was %d", satflowHeadway, excessGap);

		excessGap = satflowHeadway *100/( satflowHeadway + excessGap );

		/*JUMP IF EFF HIGH ENOUGH. ELSE, FLAG SHORT-LINK E.S. */
		if(excessGap <= *effFactor)
		{

			/* Compact MOVA: Lane 'm' was not set correctly for S395 - it should be set 
			   to the long lane with the greatest MOVQIN in the link.
			   Usually, MOVQIN will be the same for all long lanes 
			   in a link, but not necessarily. 
			   N.B. m is only set for long lanes with IN-dets. */
	
			/* JUMP TO UPPER REGISTER CHECK IF LONG LINK IS SUPERSATURATED*/
			if( isSupersatLane && (*laneNum) > 0 )
			{
				//upper_register_test();
				return mv_false;
			}
			else
			{
				link_endsat = LINK_ENDSAT_SHORT_TERM_INEFFICIENCY_WHILE_OVERSAT;
				return mv_true;
			}
		}
	}

	/*Compact MOVA: Use correct lane 'lane_num', ensuring there's an IN-det too. */
	if( isSupersatLane || *laneNum == 0 ) 
		return mv_true;
	else
		return mv_false;
}


/*!
*	\brief	Applies the Upper Register test.
*
*	\param[in]	linkIdx				The link index of the link to apply this test on it.
*	\param[in]	linkIdx				The lane number in this link.
*	\param[in]	effFactor			The Flow Efficiency Factor.
*	\param[in]	registerTime		The register time.
*	\param[in]	satflowHeadway		The saturation flow headway.
*	\param[in]	registerVeh			The register vehicle count.
*	\param[in]	isSupersatLane		Flag to detrmin if the lane is super saturated.
*
*	\return		void 
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
static void upper_register_test(uint8 linkIdx,
								uint8 laneNum,
								int32 effFactor,
								int32 registerTime,
								int32 satflowHeadway,
								int32 registerVeh,
								mv_bool  isSupersatLane)
{
	int32 b;
	int32 register_time_veh;

	/* COMPLETE REGISTER CHECK FOR LONG LINKS. CONVERT 'register_time_veh' TO VEH. */	
	if(pData->link_green_timer[linkIdx] < DI_get_moving_q_over_in_det(laneNum)+50)
		return;
	
	/*  ENTER HERE IF LONG LINK FAILS EFF TEST BUT IS SUPERSATURATED
		SKIP CHECK  UNTIL TRAFFIC MOVING WELL ON LONG LINK */

	/* ALLOW  2.0/2.5/3.0-SEC TOLERANCE  FOR 1,2,3 LANES */
	b = registerTime - 3 - DI_get_link_main_lanes_count(linkIdx+1);

	p_divide_error(satflowHeadway*20, 409,
					"satinc[0] was %d, satinc[1] was %d, satinc[2] was %d, "
					"satinc[3] was %d, satinc[4] was %d, satinc[5] was %d",
					DI_get_satinc_time(1), DI_get_satinc_time(2), DI_get_satinc_time(3),
					DI_get_satinc_time(4), DI_get_satinc_time(5), DI_get_satinc_time(6) );

	register_time_veh = DI_get_link_main_lanes_count(linkIdx+1) * b * effFactor/(satflowHeadway*20);

	if(registerVeh < register_time_veh)
	{
		link_endsat = LINK_ENDSAT_UPSTREAM_TOO_SMALL_WHILE_OVERSAT;

		if(isSupersatLane) 
			link_endsat = LINK_ENDSAT_SUPERSAT;
	}
}




