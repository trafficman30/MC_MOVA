/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         delay_and_stops_optimiser.c
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

#include "mova_constants.h"
#include "delay_and_stops_optimiser.h"
#include "dataset_interface.h"
#include "stages_handler.h"

#include "core_interface.h"

#include "algorithms_manager.h"

#include "kdebug.h"

#include <memory.h>

#include <stdlib.h>

/*------------------------- Static variables ---------------------------------*/

static struct DelayAndStopsOptiAlgInput * pData = &delayAndStopsOptiAlgInput;

/** Referes to the benfit of continuing the GREEN.
	Previous name: Tcomshr->benfit */
static int32 benfit;

/** Referes to the disbenfit of continuing the GREEN.
	Previous name: Tcomshr->disben */
static int32 disbenfit; 

/** Benefiting flow rate for links losing right-of-way (priority) if change made to SNEXT. (veh/h/10) (per link).
	Previous name: Tcomshr->ben */
static int32 ben_flow_rate[MAX_LINKS];

/** Red time for benefiting links in predicted future cycle.(0.1-sec)
	Previous name: Tcomshr->red */
static int32 future_red_time_for_ben_links[MAX_LINKS];

/** Number of vehicles counted in part of register model of approach (veh).
	Previous name: Tcomshr->regtot */
static int32 register_total[MAX_LANES];

/** Net  benefiting flow rate (veh/h/10)
	Previous name: Tcomshr->netbfr */
static int32 net_ben_flow_rate[MAX_LANES];

/** Red  time (0.1 sec) predicted for stage currently green if change now made.
	Previous name: Tcomshr->r */
static int32 predicted_red_time;

/** Amount  predicted  cycle  expands  due  to  green extension now. Stored as percentage..
	Previous name: cdelta*/
static int32 cycle_delta;

static uint8 next_stage;
static uint8 current_stage;
static uint8 previous_stage;
static int32 intergreen_time;
static int32 benex[MAX_LINKS];
static int32 lqueue[MAX_LINKS];
static int32 lrfact[MAX_LINKS];
static int32 green_time[MAX_LINKS];
static int32 red_time[MAX_LINKS];
/*--------------- End of static variables -------------------*/


/*---------------  Static functions declarations ---------------  */
static void init_algorithm_data();
static void init_model_for_one_cycle_predection();
static void find_net_benefitting_flow_rate();
static mv_bool estimate_future_red_and_flow_rate();
static mv_bool check_benefit_vs_disbenefit();
static void set_link_queue(uint8 linkIdx);
static void set_link_green(uint8 linkIdx);
static uint8 get_next_demanded_stage(uint8 nextStage);
static void save_red_time();
static void update_green_and_red_time(int32 greenStage);
static mv_bool check_valid_stage_change(uint8 currentStage, uint8 * nextStage);
static int32 calculate_disben_for_ped_or_ep_link(uint8 linkIdx, int32 stageGreenTime);
static mv_bool calculate_disben_for_traffic_link(uint8 linkIdx, int32 * greenStage, int32 * redExpFactor);
static int32 get_outmost_pri_det_cruise_time(uint8 laneIndex);
/*---------------  End of static functions declarations --------------- */


/*!
*	\brief		Implements the Delay & Stops optimisation algorithm.
*
*	\return		TRUE if Benfit < Disbenfit (therefore change current stage) or FALSE if Benfit > Disbenfit  (therefore do not change current stage).
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
mv_bool ALG_delay_and_stops_optimiser()
{
	KDEBUG_WRITE0( DEBUG_LEVEL_VERBOSE, ALG, "Initialising the 'delay_and_stops_optimiser' algorithm");

	init_algorithm_data();

	/*1. Calculate the flow rate (veh/hour) for the vehicels that will loose its right-of-way (GREEN)
		  if a change happened to SNEXT (REF for them).*/
	find_net_benefitting_flow_rate(); /*D1*/

	init_model_for_one_cycle_predection(); /*D2*/

	if(estimate_future_red_and_flow_rate()) /*D3*/
	{
		if(check_benefit_vs_disbenefit()) /*D4*/
		{
			return mv_true;
		}
	}
	else
	{
		return mv_true;
	}

	return mv_false;
}

static void init_algorithm_data()
{
	uint8 l; /* links loop variable */
	uint8 m; /* lanes loop variable */

	for (l = 0; l < DI_get_links_count(); l++)
	{
		lrfact[l] = 100;
		ben_flow_rate[l] = 0;

		future_red_time_for_ben_links[l] = 0;

		if (pData->ben_link_marker[l] == 1) /*i.e., this link benefits from extra GREEN*/
			pData->extra_green_time[l] = 0;
	}

	for (m = 1; m <= DI_get_lanes_count() ; m++)
	{
		register_total[m-1] = 0;
	}

	benfit = 0;
	disbenfit = 0;
}

static void find_net_benefitting_flow_rate()
{
	int32 e; /* extensions loop variable */
	uint8 l; /* links loop variable */
	uint8 i; /* lanes loop variable */
	int32 k; /* summation loop variable */

	int32 buffer;		/* this lane buffer (UNITS VEH/H/10 AS FOR SMFLOWS) */ /*c*/
	int32 max_buffer = 0;
	int32 sum_buffer; /* SUMS BFR FOR E AND ALL LINKS BENEFITTING FROM GREEN EXT*/ /*b*/
	
	/* initialise 'stopline_count' to count from stopline end of register */
	int32 stopline_count = 1; /*aa*/

	uint8 lanes_count; /*j*/
	uint8 lane_idx;
	uint8 lane_num;

	/*calculations parameters*/
	int32 a;
	int32 g;
	int32 f;	/*A TEMPORARY POINTER IN CASE GAMBER > 0 ON FIRST PASS*/

	int32 pri_det_cruise_time; /*the cruise time of the priority det*/

	/*START LOOP FOR VARIOUS EXTENSIONS (E) OF GREEN */
	for (e = DI_get_min_extension(); e <= DI_get_max_extension(); e += 2)
	{
		sum_buffer = 0;

		for (l = 0; l < DI_get_links_count(); l++)
		{
			/* SKIP IF PED LINK OR NON-BENEFITTING LINK. ELSE IS RELEVANT LINK */
			if (pData->ben_link_marker[l] != 1)
				continue;

			benex[l] = 0;
			lanes_count = DI_get_link_lanes_count(l+1);

			for (i = 0; i < lanes_count; i++)
			{
				lane_num = DI_get_lane_num(l+1, i);
				lane_idx = lane_num - 1;

				g = e;
				
				/* CONSTRAIN 'G' SO AS NOT TO COUNT BEYOND REMOTE DETECTOR*/
				/* Compact MOVA: Ignore -ve IN dets */
				if (DI_get_in_det_num(lane_num) <= 0)
				{
					if (e > DI_get_x_det_cruise_time(lane_num))
						g = DI_get_x_det_cruise_time(lane_num) + 1;
				}
				else
				{
					if (e > DI_get_in_det_cruise_time(lane_num))
						g = DI_get_in_det_cruise_time(lane_num) + 1;
				}

#ifdef M8_IMPORVED_BUS_PRIORITY
				pri_det_cruise_time = get_outmost_pri_det_cruise_time(lane_idx);

				if (pri_det_cruise_time != 0 && e > pri_det_cruise_time)
				{
					if (pri_det_cruise_time > g)
						g = pri_det_cruise_time + 1;
				}
#endif

			    /* JUMP IF NO IN-DET.  G IS UPPER LIMIT ON EXT*/

				f = stopline_count;

				/* IGNORE VEH'S NEARER THAN GAMBER+1 (0.5-SEC UNITS) TO STOP-LINE */
				if (f <=DI_get_gamber_time(lane_num))
					f = DI_get_gamber_time(lane_num) + 1;
				

				/* JUMP TO 295 AVOIDS ZERO DIVIDE AT 290,
				IF GAMBER = MINEXT = E = 6, THEN F > G
				SKIP IF 'F' BEYOND LENGTH OF REGISTER; ELSE, SUM CONTENTS.*/
				
				if (f <= g)
				{
					for (k = f; k <= g; k++)
					{
						/*Safety checking*/
						if (k > REG_SIZE)
							continue;

						/* sum up vehicles in lane register */
						register_total[lane_idx] += pData->shift_register[lane_idx][k-1];
					}
            
					p_divide_error(e-DI_get_gamber_time(lane_num), 501,
							"e was %d, gamber was %d, m was %d", e, DI_get_gamber_time(lane_num), lane_num);


					buffer = register_total[lane_idx]*720/(e-DI_get_gamber_time(lane_num));

					/* LIMIT BFR TO MAX FEASIBLE FOR ONE LANE */
					if (buffer > 218)
						buffer = 218;

					sum_buffer += buffer;

					/* LINK BFR FOR THIS EXTENSION SAVED (VEH/H/10) */
					benex[l] += buffer;
				}

				if (stopline_count <= 1)
				{
					/* SUM SPARE GREEN FOR LINK ON FIRST PASS (0.1-SEC UNITS) */
					a = pData->link_green_timer[l] - pData->red_count_x_det[lane_idx] * DI_get_satinc_time(lane_num);
					if (a > 0)
						pData->extra_green_time[l] += a;
				}
			}/* end of lanes for loop */

			/* CONVERT TOTAL SPARE TO MEAN SPARE GREEN/LANE ON FIRST PASS*/
			if (stopline_count == 1)
			{
				p_divide_error(lanes_count, 502, "Lanes count cannot be zero");
				pData->extra_green_time[l] = pData->extra_green_time[l]/lanes_count;
			}
		} /* end of links for loop*/

		if (sum_buffer > max_buffer)
		{
			/* SAVE LINK BFR'S (COMPONENTS OF max_buffer) */
			max_buffer = sum_buffer;

			for (l = 0; l < DI_get_links_count(); l++)
			{
				ben_flow_rate[l] = benex[l];
			}
		}

		/* UPDATE stopline_count TO POINT TO NEXT SLOT IN SHREGA TO BE COUNTED */
		stopline_count = e+1;

	} /* end of extensions for loop*/
}

#ifdef M8_IMPORVED_BUS_PRIORITY
static int32 get_outmost_pri_det_cruise_time(uint8 laneIdx)
{
	uint8		a; /*associated pri links counter*/
	uint32		cruise_ep;
	uint32		outmost_cruise_ep = 0;
	uint8		pri_link_num;
	int8		i; /*ep dets loop counter*/
	uint8		pri_det_num;

	for (a = 0; a < MAX_PRIORITY_LINKS_ACCOC_TO_LANE; a++)
	{
		pri_link_num = DI_get_associated_priority_link_num_for_a_lane(a, laneIdx + 1);

		/*Exit the assoc for loop if any of them is zero because they are stored always from the first slot.*/
		if (pri_link_num == 0)
			break;

		for (i = 1; i <= MAX_EP_DETS; i++)
		{
			pri_det_num = DI_get_ep_det_num(pri_link_num, i);

			if (pri_det_num > 0)
			{
				if (ALG_is_det_suspected(pri_det_num - 1))
					continue;

				cruise_ep = DI_get_ep_det_cruise_time(pri_link_num, i);

				outmost_cruise_ep = max(outmost_cruise_ep, cruise_ep);
			}
		}
	}

	return outmost_cruise_ep;
}
#endif

static void init_model_for_one_cycle_predection()
{
	uint8 l; /* links loop variable*/
	uint8 i; /* links lanes loop variable*/

	int32	arrived_vehs;
	uint8	lane_idx;
	uint8	det_num;
	mv_bool	is_link_done;

	next_stage = pData->next_stage;
	current_stage = pData->current_stage;

	intergreen_time = DI_get_stage_change_allowance(current_stage, next_stage);

	/* Replace allowable but negative IG's (<-1) by positive value. */
	if (intergreen_time < -1)
		intergreen_time = -intergreen_time;

	predicted_red_time = intergreen_time;
	cycle_delta = 100;

	for (l = 0; l < DI_get_links_count(); l++)
	{
		/* skip if pedestrian or emerg/pr link*/
		if (!DI_is_traffic_link(l+1))
		{
			set_link_green(l);
			continue;
		}

		if (pData->ben_link_marker[l] == 1)
		{
			//goto S335;

			/* Jump unless traffic link benefitting from extension of CUSIG*/
			lqueue[l] = 0;

			if (DI_get_link_green_status(next_stage,l+1) >= 2) 
				future_red_time_for_ben_links[l] = intergreen_time;
		
			/*SOME BENEFITTING LINKS MAY HAVE ONLY AN IG AS RED.*/

			set_link_green(l);//goto S380;
			continue;
		}

		arrived_vehs = 0;
		is_link_done = mv_false;
		
		/* SUM VEHICLE ARRIVALS IN 'arrived_vehs' FOR ALL LANES ON LINK*/
		for (i = 0; i < DI_get_link_lanes_count(l+1); i++)
		{
			lane_idx = DI_get_lane_num(l+1, i) - 1;
			det_num = DI_get_in_det_num(lane_idx+1); /*IN-DET*/

#ifdef M8_IMPORVED_BUS_PRIORITY
			arrived_vehs += pData->bus_weighted_red_count[lane_idx];
#endif

			/* Compact MOVA: Ignore -ve IN dets */
			if (det_num <= 0)
			{
				det_num = DI_get_x_det_num(lane_idx+1); /* X-DET */
			
				/* CHECK IF X-DET DUD. ELSE, USE X RED COUNTS*/
				if(ALG_is_det_suspected(det_num-1))
				{
					set_link_queue(l);
					is_link_done = mv_true;
					break;
				}

				arrived_vehs += pData->red_count_x_det[lane_idx];
				continue;
			}

			/*SKIP IF NO IN-DET. JUMP IF DUD. ELSE SUM RED COUNTS*/
			if (ALG_is_det_suspected(det_num - 1))
			{
				set_link_queue(l);
				is_link_done = mv_true;
				break;
			}

			arrived_vehs += pData->red_count_in_det[lane_idx] + pData->extra_veh_beyond_in_det[lane_idx];
		}

		if(is_link_done)
			continue;

		/* SCALE LINK QUEUE TO VEHICLES*3 */
		lqueue[l] = arrived_vehs*3;

		set_link_green(l);
	}
}

static void set_link_queue(uint8 linkIdx)
{
	/* IF DUD DETECTORS, SET LQUEUE FROM TIME SINCE END GREEN*/

	p_divide_error(pData->link_smoothed_flow[linkIdx], 504, "link number was %d", (linkIdx+1));
	p_divide_error((int32)(1200/pData->link_smoothed_flow[linkIdx]), 503, "link_smoothed_flow was %d, link number was %d", pData->link_smoothed_flow[linkIdx], (linkIdx+1));

	lqueue[linkIdx] = pData->cycle_time_timer[linkIdx]/(1200/pData->link_smoothed_flow[linkIdx]);

	set_link_green(linkIdx);
}

static void set_link_green(uint8 linkIdx)
{
	green_time[linkIdx] = 0;
	red_time[linkIdx] = 0;
		
	/*TIME FUTURE START-GREENS FROM NOW USING 'T'*/
	if (pData->ben_link_marker[linkIdx] == 2)
	{
		/* LINKS OVERLAPPING GREEN NOW, HAVE PREVIOUS GREEN SET UP */
		green_time[linkIdx] = pData->link_green_timer[linkIdx];
	}
}


/* SUBSECTION D3 :	ESTIMATE FUTURE RED + DISBENEFITTING FLOW RATE
					START MODEL
					UPDATE VARIABLES FOR START OF ANOTHER GREEN STAGE */
static mv_bool estimate_future_red_and_flow_rate()
{
	uint8 s; /* stages loop variable */ /*n*/
	uint8 l; /* links loop variable */

	int32 a;
	int32 stage_green_time = 0; /* initially, it is the absolute green time*/ /*g*/
	int32 red_exp_factor; /* SAVES RED EXPANSION FACTOR FOR CRITICAL LINK ON  STAGE */


	for (s=2; s<=DI_get_stages_count(); s++)
	{
		previous_stage = current_stage;
		current_stage = next_stage;

		/* JUMP TO PAY-OFF WHEN CYCLE COMPLETED */
		if (current_stage == pData->current_stage)
			break;

		next_stage = get_next_demanded_stage(next_stage);

		if( !check_valid_stage_change(current_stage, &next_stage))
			continue;
		
		update_green_and_red_time(stage_green_time);
		
		/* NOW FIND STAGE GREEN stage_green_time FROM RELEVANT LINK GREENS SUBJECT TO MIN/MA */
		stage_green_time = DI_get_stage_min_green_time(current_stage);

		red_exp_factor = 0; /*aa*/

		for (l = 0; l<DI_get_links_count(); l++)
		{
			/* FIND WHICH LINKS POSSIBLY DETERMINE LENGTH OF current_stage */

			/* skip any links red in current_stage */
			if (DI_get_link_green_status(current_stage, l+1) == 0 || DI_get_link_green_status(current_stage, l+1) == 4)
				continue;

			if (DI_get_link_green_status(current_stage, l+1) == 1)
			{
				/* if this is the link's only green then the link is relevant */
			}

			else if (DI_get_link_green_status(next_stage, l+1) == 0 || DI_get_link_green_status(next_stage, l+1) == 4)
			{
				/* if link red next stage then it is relevant */
			}
			else
			{
				/* IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: should check whether 
				   (as stage demands stand) the link will have (extendable) green again immediately 
				   after the fixed green, as we do when determining benefitting and non-benefitting 
				   links in Block C. If it's green again after the fixed green we should jump
				   as the link (probably) won't determine the length of this stage. */
				a = DI_get_stage_min_green_time(next_stage) + 5;

				if (a < DI_get_stage_max_green_time(next_stage) || check_green_in_following_stages(l, next_stage-1) == 0)
					continue;
			}
			
			/* if this is a ped or e/p link */
			if (!DI_is_traffic_link(l+1))
			{
				/*DISBEN: is the disbenefit in predicted cycle due to 1-second extension of current green*/
				stage_green_time = calculate_disben_for_ped_or_ep_link(l, stage_green_time);
			}
			else /* else this is a traffic link */
			{
				if(! calculate_disben_for_traffic_link(l, &stage_green_time, &red_exp_factor))
					return mv_false;

			}
			red_time[l] = 0;
			green_time[l] = 0;
			lqueue[l] = 0;

		}

		/* ALL RELEVANT LINK GREENS DONE. MAX OF stage_green_time's IS STAGE GREEN */

		/* LIMIT stage_green_time TO STAGE MAX */
		if (stage_green_time > DI_get_stage_max_green_time(current_stage))
			stage_green_time = DI_get_stage_max_green_time(current_stage);
				
		intergreen_time = DI_get_stage_change_allowance(current_stage, next_stage);
		predicted_red_time += stage_green_time + intergreen_time;

		/* INCREASE CYCLE EXPANSION FACTOR TO THAT FOR LATEST STAGE */
		if (red_exp_factor > cycle_delta)
			cycle_delta = red_exp_factor;
				
		
		/* NOW SAVE RED TIME ETC. FOR BENEFITTING LINKS */
		save_red_time();
	}

	return mv_true;
}


static uint8 get_next_demanded_stage(uint8 nextStage)
{
	/* skip to the next demanded stage */
	for (;;)
	{
		nextStage += 1;
		if (nextStage > DI_get_stages_count())
			nextStage = 1;

		if (pData->stage_demand_marker[nextStage-1] != 0 ||
			pData->stage_emergency_demand_marker[nextStage - 1] != 0 ||
			pData->stage_priority_demand_marker[nextStage - 1] != 0)
			break;
	}

	return nextStage;
}

int32 get_lane_register_total(uint8 laneIdx)
{
	return register_total[laneIdx];
}

/* Check whether it is possible to change from current_stage to next_stage */
/* Work backwards to find an intermediate stage if not. */
/* If no valid change possible, ignore the stage demand */
static mv_bool check_valid_stage_change(uint8 currentStage, uint8 * nextStage)
{
	for (;;)
	{
		if (DI_get_stage_change_allowance(currentStage, *nextStage) >= 0)
			break;

		*nextStage -= 1;
		if (*nextStage <= 0)
			*nextStage = DI_get_stages_count();

		if (*nextStage == currentStage)
			return mv_false; /* No valid change */
	}

	return mv_true;
}

static void update_green_and_red_time(int32 stageGreenTime)
{
	uint8 l; /*links loop variable */

	for (l=0; l<DI_get_links_count(); l++)
	{
		/* link red (or opposed red) last stage */
		if (DI_get_link_green_status(previous_stage, l+1) == 0 || DI_get_link_green_status(previous_stage, l+1) == 4)
		{
			green_time[l] = 0;
			red_time[l] += stageGreenTime + intergreen_time;

			if (DI_get_link_green_status(current_stage, l+1) != 0 && DI_get_link_green_status(current_stage, l+1) != 4)
			{
				/* opposed green starting this stage */
				/* note link red factor for non-benefiting links */
				if (pData->ben_link_marker[l] != 1)
					lrfact[l] = cycle_delta;
			}
		}

		/* link red (or opposed red) this stage, following green last stage */
		else if (DI_get_link_green_status(current_stage, l+1) == 0 || DI_get_link_green_status(current_stage, l+1) == 4)
		{
			green_time[l] = 0;
			red_time[l] = intergreen_time;
		}

		/* link continuing green */
		else
		{
			green_time[l] += stageGreenTime + intergreen_time;
			red_time[l] += stageGreenTime + intergreen_time;
		}

	}
}

static int32 calculate_disben_for_ped_or_ep_link(uint8 linkIdx, int32 stageGreenTime)
{
	int32 a;
	int32 c;
	int32 h;

	float pdelay;
	float rTemp;
	float wf;

	h = DI_get_link_min_green_time(linkIdx+1) - green_time[linkIdx];

	if (stageGreenTime < h)
		stageGreenTime = h;
            
	/* Now deal with pedestrian links only; compute pedestrian green, disbenefit, etc.*/
	if (DI_is_ped_link(linkIdx+1))
	{
		c = pData->cycle_time_timer[linkIdx] + red_time[linkIdx] + h;
					
		/* C is actual 100-per cent cycle for this link (0.1-Sec)*/
		p_divide_error((int32)(pData->cycle_time_timer[linkIdx]/10), 508, "cycle_time_timer was %d, link number was %d", pData->cycle_time_timer[linkIdx], (linkIdx+1));

		a = c * 5 / (pData->cycle_time_timer[linkIdx]/10);
					
		/* Alpha/2 is 50 for peds, =*5//10 above. This 50 is then factored
			***    up by the ratio of: estimated full cycle to current red time;
			***    A is probably 50<= A <200 for PUFFIN ped links.
			***    Ped queue, set in BLOCKB, scaled x10, max 240 for PUFFIN links
			***    Real variables TEMP, WF, PDELAY used to avoid overflow.
		*/
		rTemp  = (float)((float)pData->total_bonus_green[linkIdx] * (float)a * (float)lrfact[linkIdx]);
		pdelay = (float)(rTemp*3.0E-5F);

		/*
			***    PDELAY is extra delay for ped link (scaled to ped*3)
			***    Now, apply any ped-link delay-weighting set in STOPEN.
		*/
		if (DI_get_stop_penalty(linkIdx+1) >= 640)
		{
			/*  THEN, calculate variable delay-weight if STOPEN = 640 or 650.*/
			rTemp = (float)((float)pData->ped_wait_timer/20.0F);
			if(DI_get_stop_penalty(linkIdx+1) == 650)
				rTemp += 1.0F;
						
			/* Increase weighting factor if STOPEN = 650. NB STAMAX in 0.1s */

			wf = (float)(1.0F + rTemp*rTemp);
			rTemp = (float)(pdelay * wf + 0.5F);
			disbenfit += (int32)(rTemp);
		}
		else 
		{
			/* ELSE, is fixed delay-weight, if any, in data; apply to PDELAY*/
			wf = (float)((float)DI_get_stop_penalty(linkIdx+1)/10);
						
			/* Divide STOPEN by 10, as MOVASETUP stores as if 0.1s units*/
			rTemp = (float)(pdelay * wf + 0.5F);
			disbenfit += (int32)(rTemp);
		}
	}

	return stageGreenTime;
}


static void save_red_time()
{
	uint8 l; /* links loop variable */

	for (l=0; l<DI_get_links_count(); l++)
	{
		if (pData->ben_link_marker[l] != 1)
			continue;
			
		/* JUMP UNLESS traffic LINK BENEFITS FROM EXTENSION OF CUSIG*/
		if (DI_get_link_green_status(next_stage, l+1) == 0)
			continue;
			
		/* JUMP IF LINK RED IN next_stage */
		if (DI_get_link_green_status(next_stage, l+1) == 4 && next_stage != pData->current_stage) 
			continue;

		/* JUMP IF OPPOSED GREEN NEXT UNLESS CYCLE IS ENDING */
		/*  IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: Check that link 
			isn't extendable green again *after* the fixed time 
			stage given current stage demands. 
			Also added + 5 correction as is done elsewhere. */
		/* JUMP IF FIXED-TIME GREEN IN next_stage; TREATED AS RED */
		if (DI_get_stage_max_green_time(next_stage) <= (DI_get_stage_min_green_time(next_stage) + 5)
			&& check_green_in_following_stages(l, next_stage-1) == 1)
		{
			continue;
		}

		/* JUMP IF THIS IS SECOND START-GREEN IN CYCLE; ELSE,*/
		if (future_red_time_for_ben_links[l] > 0)
			continue;
				
		/* FOR BENEFITTING LINKS SAVE TIME TILL NEXT EXTENDABLE GREEN */
		future_red_time_for_ben_links[l] = predicted_red_time;

		/*SAVE SHIFT FOR START OF THIS LINK'S NEXT GREEN AFTER RED*/
		lrfact[l] = cycle_delta;		
	}
}

static mv_bool calculate_disben_for_traffic_link(uint8 linkIdx, int32 * greenStage, int32 * redExpFactor)
{
	/* calcaultions parameters */
	int32 a;
	int32 b;
	int32 c;
	int32 d;
	int32 e;
	int32 f;
	int32 h;
	int32 k;

	/* NOW FIND TIME TO CLEAR QUEUE ON RELEVANT TRAFFIC LINKS */

	p_divide_error(pData->link_smoothed_flow[linkIdx], 506,"link number was %d", (linkIdx+1) );
	p_divide_error((int32)(1200/pData->link_smoothed_flow[linkIdx]), 505, "link_smoothed_flow (LSMFLO) was %d, link number was %d", pData->link_smoothed_flow[linkIdx], (linkIdx+1));

	a = lqueue[linkIdx] + (red_time[linkIdx] - green_time[linkIdx]) / (1200 / pData->link_smoothed_flow[linkIdx]);
				
	/* FIRST, ADD ON ARRIVALS FOR PERIOD FROM 'NOW' TO START GREEN
		OR, IF DOUBLE GREEN, FOR PERIOD OF LAST RED */
	if (a > 327)
		return mv_false;
				
	/*JUMP TO CHANGE STAGE IF Q>100 VEH. ELSE SCALE Q TO VEH*300 */
	lqueue[linkIdx] = a*100;
	f = 100 - pData->y_value[linkIdx];
          
	p_divide_error(f, 507, "yvalue was %d, link number was %d", pData->y_value[linkIdx], (linkIdx+1));

	d = lqueue[linkIdx]/f;

	if (d > 327) 
		return mv_false;

	/* 'D' IS DELAYED VEHICLES*3. JUMP TO CHANGE IF TOO LARGE */
	p_divide_error(pData->y_value[linkIdx], 509, "link number was %d", (linkIdx+1) );

	/* avoid divide by zero when lsmflo is smaller than yvalue */
	b = f * pData->link_smoothed_flow[linkIdx] / pData->y_value[linkIdx];

	if (b == 0)
		k = lqueue[linkIdx] * 12;
	else
		k = lqueue[linkIdx] / b * 12;
				
	/* CLEAR TIME (K IN 0.1-SEC UNITS).  ENSURE >= MIN GREEN */

	b = DI_get_link_min_green_time(linkIdx+1) - k;

	if (b < 0) 
		b = 0;

	h = k - green_time[linkIdx] + b;
				
	/* NOW REDUCE CLEAR TIME BY GREEN ALREADY HAD. 'H' MAY BE < 0 */
	if (h > *greenStage)
	{
		/*JUMP IF LINK DOES NOT (SO FAR) DETERMINE STAGE GREEN */
		*greenStage = h;
		*redExpFactor = lrfact[linkIdx];
				
		/* RED FACTOR SCALED UP UNLESS GREEN FIXED BY MINIMUM */
		p_divide_error(100 - pData->y_value[linkIdx], 511, "link number was %d", (linkIdx+1) );

		if (b == 0)
			*redExpFactor = *redExpFactor * 100 / (100 - pData->y_value[linkIdx]);

		if (*redExpFactor > 300)
			return mv_false; //goto S650;
	}

	/*JUMP TO CHANGE STAGE IF RED EXPANSION FACTOR IS EXCESSIVE*/
	if (pData->ben_link_marker[linkIdx] == 0)
	{
		/*  JUMP IF NON-DISBENEFITTED LINK. ELSE SUM DISBENEFIT (VEH)
			'D' IS DELAYED VEHICLES*3. 'A' IS (1-ALPHA/2) PER CENT */

		c = pData->cycle_time_timer[linkIdx] + red_time[linkIdx] + h;
		/* 'C' IS ACTUAL 100-PER CENT CYCLE FOR THIS LINK (0.1-SEC)	*/

		p_divide_error((int32)((c+pData->extra_green_time[linkIdx])/10), 512,
			"c was %d, extra_green_time was %d, link number was %d", 
			 c, pData->extra_green_time[linkIdx], (linkIdx+1) );

		a = 100 - c * 5 / ((c + pData->extra_green_time[linkIdx]) / 10);

		/*'E' IS EXTRA DELAY FOR LINK ADDED TO DISBEN (VEH*3) */
		p_divide_error(lrfact[linkIdx], 514, "link number was %d", (linkIdx+1) );

		p_divide_error((int32)(10000/lrfact[linkIdx]), 513,
						"lrfact was %d, link number was %d", lrfact[linkIdx], (linkIdx+1) );
          
		e = d * a / (10000 / lrfact[linkIdx]);

		/* check if there is any kind of link demand */
		if (e == 0 && pData->link_demand_marker[linkIdx] > 0)
			e = 3;
				
		/* ALLOW FOR DEMAND INSERTED BY DUD-DETS. ASSUME 1 VEH*3 */
		/*Tcomshr->ldben[linkIdx] = e;*/ /* this variable is not used anywhere */
		disbenfit += e;
	}

	return mv_true;
}


/*SUBSECTION D4 : COMPLETE BENEFIT CALCULATIONS.  DO PAY-OFF .
					NOTE : BEN(VEH/H/10),RED(0.1-SEC),DISBEN(VEH)*/
static mv_bool check_benefit_vs_disbenefit()
{
	uint8 l; /* links loop variable*/

	int32 a;
	int32 b;
	int32 aa;
	int32 f;
	int32 d;

	for (l=0; l<DI_get_links_count(); l++)
	{
		if (pData->ben_link_marker[l] != 1)
			continue;

		a = future_red_time_for_ben_links[l] + 10;

		/* ALLOW FOR ZERO OR HUGE RED CAUSING RANGE PROBLEMS */
		if (future_red_time_for_ben_links[l] > 1300)
			return mv_true;


		p_divide_error((int32)((a+pData->extra_green_time[l]+5)/10), 515, "a was %d, extra_green_time was %d, link number was %d",
							a, pData->extra_green_time[l], (l+1));

		/* 'AA' IS ALPHA/2 PER CENT. 'F' IS (1-Y)/(1-AA), SCALED*30 */
		aa = 5 * a / ((a + pData->extra_green_time[l] + 5) / 10);

		p_divide_error((int32)lrfact[l], 517, "link number was %d", (l+1));
		p_divide_error((int32)(5000/lrfact[l]), 516,	"lrfact was %d, link number was %d", lrfact[l], (l+1) );
      
		/*'D' IS DISBEN FLOW RATE (VEH/H/10) DUE TO SHIFT OF NEXT CYCLE */
		d = pData->link_smoothed_flow[l]*50/(5000/lrfact[l]);

		p_divide_error(100-aa, 518);
		
		f = (100-pData->y_value[l])*30/(100-aa);
		
		p_divide_error(f, 519,
					"yvalue was %d, aa was %d, red was %d, extrag was %d, link number was %d",
					pData->y_value[l], aa, future_red_time_for_ben_links[l], pData->extra_green_time[l], (l+1) );

		/*'B' (VEH/H/10) IS NET BENEFITTING FLOW RATE. CAN BE < 0 */
		b = (ben_flow_rate[l] - d) * 30 / f + ben_flow_rate[l] * aa / 100;

		net_ben_flow_rate[l] = b;

		if (b != 0)
		{
			p_divide_error(b, 521,
					"ben_flow_rate was %d, d was %d, f was %d, aa was %d, link number was %d",
					ben_flow_rate[l], d, f, aa, (l+1) );
      
			p_divide_error((int32)(9000/b), 520,
					"ben_flow_rate was %d, d was %d, f was %d, aa was %d, link number was %d",
					ben_flow_rate[l], d, f, aa, (l+1) );
      
			benfit = benfit + 25 * future_red_time_for_ben_links[l]/(9000/b);
		}

		if (ben_flow_rate[l] == 0)
			continue;
      
		p_divide_error(ben_flow_rate[l], 523, "link number was %d", (l+1) );
		p_divide_error((int32)(18000/ben_flow_rate[l]), 522, "link number was %d", (l+1) );
      
		benfit += 50 * DI_get_stop_penalty(l+1) / (18000/ben_flow_rate[l]);
	}

	/* CONVERT BENEFIT AND DISBENEFIT TO UNITS OF VEHICLES PAY-OFF*/
	benfit = benfit / 10;
	disbenfit = disbenfit / 3;
	
	if (benfit > disbenfit)
	{
		return mv_false;
	}
	else
	{
		return mv_true;
	}
}


void ALG_ds_get_internel_data_for_message(int32 * benfitRef,
										int32 * disbenfitRef,
										int32 * predictedRedTimeRef,
										int32 * cycleDeltaRef,
										int32 * benFlowRateRef, /*[links]*/
										int32 * futureRedRef, /*[links]*/
										int32 * netBenFlowRateRef /*[links]*/	)
{
	*benfitRef = benfit;
	*disbenfitRef = disbenfit;
	*predictedRedTimeRef = predicted_red_time;
	*cycleDeltaRef = cycle_delta;

	memcpy(benFlowRateRef, ben_flow_rate, MAX_LINKS*sizeof(int32));
	memcpy(futureRedRef, future_red_time_for_ben_links, MAX_LINKS*sizeof(int32));
	memcpy(netBenFlowRateRef, net_ben_flow_rate, MAX_LINKS*sizeof(int32));
}