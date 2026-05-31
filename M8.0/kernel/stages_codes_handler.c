/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         stages_codes_handler.c
*
*  TITLE:        Mova Kernel: Stages Codes Handler
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


#include <stdlib.h> /* PCMOVA, embedded too? For abs() */

#include "gbltypes.h"

#include "dynamic_data.h"
#include "dataset_interface.h"
#include "mova_constants.h"
#include "mova_types.h"

#include "junction_handler.h"
#include "links_handler.h"
#include "links_ep_handler.h"
#include "lanes_handler.h"
#include "detectors_handler.h"

#include "stages_codes_handler.h"
#include "timers_manager.h"

#include "kdebug.h"

static struct Stages * p_stages;

static void set_conditional_demand(uint8 stageNo, uint8 linkNo);
static void set_latched_demand(uint8 stageNo, uint8 linkNo);
static void process_6mrr_sdcode(uint8 stageNo, uint8 linkNo);
static int16 choose_priority_facility_code(uint8 stageNumToCheck);

void set_stages_pointer_for_codes_handler(struct Stages * stagesRef)
{
	p_stages = stagesRef;
}





/*!
*	\brief	Process SDCODEs for a specific stage and link. 
*
*	\param[in]	stageNo		The stage number.
*	\param[in]  linkNo		The link number.
*
*	\return		The number of stages processed.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
int8 process_sdcode(uint8 stageNo, uint8 linkNo)
{
	/* IRH MOD: M5.0.0: 02/09/04: Delayed demand time for emergency/priority links 
	 * with 4TT and 5TT SDCodes. Defined in Block A. */
	int32 k;
	int32 a,aa,b,e,h,tt;
	uint8 x,y,i,d;

#ifndef M8_ENABLED
	int32 t, mm, yy, dd, dy;//for code 4T
	uint8 l; //for code 4T
#endif
	

	k = DI_get_sdcode(stageNo, linkNo);
	
	/* skip link if it doesn't demand the stage we are considering */
	if (k == 0)
		return 1;

	/* if we are considering the current stage, only consider 6MRR demands */
	if (stageNo == get_current_stage() && k < 6000)
		return 1;

	if (k < 10)
	{
		/* SDCODES 1,2,3,4 */
		switch (k)
		{
			case 1:
				/* For code 1 set latched demand */
				set_latched_demand(stageNo, linkNo);
				return 1;

			case 2:
				/* For code 2 cater for left turn filter demand.
					Set demand for filter stage during previous stage only */
				e = stageNo-1;
				if (e == 0)
					e = DI_get_stages_count();
				if (get_current_stage() == e)
					set_latched_demand(stageNo, linkNo);
				return 1;

			case 3:
				/* For code 3 set simple non-latched demand */
				set_conditional_demand(stageNo, linkNo);
				return 1;

			case 4:
				/* For code 4 set non-latched demand only if red */
				if (get_link_green_flag(linkNo-1) == LINK_FLAG_NOT_GREEN)
					set_conditional_demand(stageNo, linkNo);
				return 1;

			default:
				/* Shouldn't ever happen! */
				return 0;
		}
	}
	else if (k < 100)
	{
		/* SDCODES 1X,2X,3X,5T,6M */
		/* The case 4T has been removed from the code on 27/01/2014 */

		/* use 'a' as 10's and 'x' as units */
		a = k/10;
		x = k%10;
		/* x=0 => x is stage 10 */
		if (x == 0)
			x = 10;

		switch (a)
		{
			case 1:
				/* For 1X code, set non-latched demand if stage X not yet demanded */
				if (p_stages->demand_marker[x-1] == STAGE_DEMAND_NO_DEMAND)
					set_conditional_demand(stageNo, linkNo);
				return 1;

			case 2:
				/* For 2X code, set non-latched demand if stage X not yet demanded
					or if special demand exists for the link */
				if (p_stages->demand_marker[x-1] == STAGE_DEMAND_NO_DEMAND ||
					get_link_demand_marker(linkNo-1) == LINK_DEMAND_UNLATCHED ||
					get_link_demand_marker(linkNo-1) == LINK_DEMAND_CALL_CANCEL)
					set_conditional_demand(stageNo, linkNo);
				return 1;
			
			case 3:
				/* For 3X code, set latched demand if stage X also demanded
					provided stage X not currently running nor link L now green */
				if (get_link_green_flag(linkNo-1) > LINK_FLAG_NOT_GREEN)
					return 1;

				if (p_stages->demand_marker[x-1] > STAGE_DEMAND_NO_DEMAND && get_current_stage() != x)
					set_latched_demand(stageNo, linkNo);

				/* if latched demand already set then reset in order to check e/p demands */
				if (p_stages->demand_marker[stageNo-1] == STAGE_DEMAND_LATCHED)
					set_latched_demand(stageNo, linkNo);

				return 1;

#ifndef M8_ENABLED
			case 4:
				/* For 4T code (or 4X here), set latched demand if repeat stage required */

				/* Calculate net benefit of repeating stage by calling this stage N.
				T is extra IG from adding stage N to sequence.(T=10*X  0.1s units) */
				a = 0;
				t = 10 * x;
				mm = 0;
				yy = 0;
				
				for (l = 1; l <= DI_get_links_count(); l++)
				{
					/* Save largest link lost-time in A, for use later */
					if (Tcomshr->lostim[l - 1] > a)
						a = Tcomshr->lostim[l - 1];

					dd = get_link_smoothed_flow(l-1) * (100 - get_link_y_value(l-1));

					if (DI_get_link_green_status(stageNo, l) != 0 && DI_get_link_green_status(stageNo, l) != 4)
					{
						/* link has unopposed green in stage N */
						mm += dd;
					}
					else
					{
						/* link is effectively red in stage N */
						yy += dd;
					}
				}

				dd = (long)(a - t)*mm;
				dy = (long)(t + t)*yy;
				dd -= dy;

				/* if positive net benefit set latched demand */
				if (dd >= 0)
					set_latched_demand(stageNo, linkNo);
				return 1;
#endif
			case 5:
				return 1;
			
			case 6: /*this code is not used now*/
				/* For 6M code (6X here), transfer demand from link L to link M */
				/*
				if (get_link_demand_marker(x-1) == LINK_DEMAND_NO)
					set_link_demand_marker(x-1, -1);
				*/
				return 1;

			default:
				return 0;
		}
	}
	else
	{
		/* SDCODES 1XY,2XY,3XY,6MRR,4TT and 5TT. */
		/*4TXY is removed.*/

		/* use 'h' as 100's, 'x' as 10's and 'y' as units */
		h = k/100;
		a = h*100;
		x = (uint8)((k-a)/10);
		b = x*10;
		y = (uint8)(k-a-b);

		/* 6MRR code */
		if (h >= 60)
		{
			/* skip 6MRR code unless now in stage N of 4-stage sequence N -> N+3 */
			if (stageNo != get_current_stage())
				return 1;

			process_6mrr_sdcode(stageNo, linkNo);

			/* 6mmr code covers this and the 3 following stages */
			return 4;
		}

		/* 4TT and 5TT codes */
		if (h == 4 || h == 5)
		{
			/* For 4TT code, set delayed latched demand after TT (0.1s)(TT=10*XY) */
			/* For 5TT code, also require normal demand and delay < TT+150 (+15s) */

			tt = 100*x+10*y;

#if defined (CMOVA_EP) /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */

#ifdef M8_IMPROVED_LINKING

			/*CHECK: check this condition with MC*/
			if(tt == 990)
			{
				if(get_forward_link_delay() > 0)
					tt = get_forward_link_delay();
				else
					return 1;
			}
#endif

			/* Use AA to limit period for setting 5TT demand to TT -> TT+15s */
			aa = tt + 150;

			if (DI_is_ep_link(linkNo)) 
			{
				/* (pseudo?) emerg/priority link; check em/pr detectors. */
				/* Intended to give delayed hurry-calls for two closely-linked MOVAs.
					Assumed to work with latched continuous detection forcing linking */

				/* 5TT code also requires normal demand */
				if (h == 5 && p_stages->demand_marker[stageNo-1] <= STAGE_DEMAND_NO_DEMAND)
					return 1;

				for (i = 1; i <= MAX_EP_DETS; i++) 
				{
					d = DI_get_ep_det_num(linkNo, i);

					if (d > 0 && !is_det_suspected(d-1))
					{
						if (get_ep_link_demand_delay(linkNo-1) >= tt && tt > 0) 
						{
							/* Delay has timed off.  Check candet and set demand if appropriate */
							if (DI_get_ep_cancel_det_num(linkNo) == 0)
							{
								/* set demand if det exists & is OK, delay has timed off,
									and det has not been continuously 'on' >AA */
								if (h == 4 || (h == 5 && get_det_on_time(d) <= aa))
								{
									set_latched_demand(stageNo, linkNo);
									break;
								}
							}
						}
					}
				}
				return 1;
          
			} /*if (DI_is_ep_link(linkNo))     */

#endif /* CMOVA_EP */ /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */

			if (DI_is_ped_link(linkNo))
			{
				/* (pseudo?) pedestrian link; check ped detector. */
				/* Assumed to work with latched continuous ped detection (LTYPE<100) */

				/* The following line was "d = (uint8)DI_get_link_type(linkNo);" in M7.0. This is not correct and has
				been corrected in M8.0 as follow:*/
				d = DI_get_ped_det_num(linkNo);

				if (d > 0)
				{
					if (!is_det_suspected(d-1) && get_det_on_time(d) >= tt)
					{
						/* set demand if det OK, & is 'on' long enough */
						set_latched_demand(stageNo, linkNo);
						return 1;
					}
				}
			}
			return 1;
		}

		/* left with 1XY, 2XY and 3XY codes */

		/* convert stage 0 to stage 10 for X and Y */
		if (x == 0)
			x = 10;
		if (y == 0)
			y = 10;

		switch (h)
		{
			case 1:
				/* For 1XY code, set non-latched demand if stages X,Y not demanded */
				if (p_stages->demand_marker[x-1] == STAGE_DEMAND_NO_DEMAND && p_stages->demand_marker[y-1] == STAGE_DEMAND_NO_DEMAND)
					set_conditional_demand(stageNo, linkNo);
				return 1;
				
			case 2:
				/* For 2XY code, first check for 'special link demand */
				if (get_link_demand_marker(linkNo-1) == LINK_DEMAND_UNLATCHED || 
					get_link_demand_marker(linkNo-1) == LINK_DEMAND_CALL_CANCEL)
				{
					/* Drop through switch statement to process as 3XY code */
				}
				else
					return 1;

			case 3:
				/* For 3XY code, check if competing demands on stages X to Y */
				for (b = x; ;)
				{
					if (p_stages->demand_marker[b-1] > 0)
					{
						set_conditional_demand(stageNo, linkNo);
						return 1;
					}

					/* If all stages in range X -> Y have been processed then return */
					if (b == y)
						break;

					b++;
					if (b > DI_get_stages_count())
						b = 1;
				}
				return 1;
			default:
				return 0;
		}
	}
}


/*!
*	\brief	Process the 6MRR code as one of the SDCODEs for a specific stage and link. 
*
*	\param[in]	stageNo		The stage number.
*	\param[in]  linkNo		The link number.
*
*	\return		The number of stages processed.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static void process_6mrr_sdcode(uint8 stageNo, uint8 linkNo)
{
	int32 a,aa,b,bb,c,d,e,f,g,h,k,ma,mb,ra,rb;
	uint8 w,x,y,m;

	k = DI_get_sdcode(stageNo, linkNo);

	/* use 'h' as 100's, 'x' as 10's and 'y' as units */
	h = k/100;
	a = h*100;
	x = (uint8)((k-a)/10);
	b = x*10;
	y = (uint8)(k-a-b);

	g = h/10;
	g *= 10; /* G is tolerance of 60 = 6-seconds for later use */
	w = (uint8)(h-g);

	/* Check all 4 links demanded.  We know that Right(A)=X, 
	   & Right(B)=Y are both already demanded. 
	   Check others:  Main(A)=L,   Main(B)=W.  Note: L,W,X,Y are LINKs */
	if (get_link_demand_marker(linkNo-1) == LINK_DEMAND_NO ||
		get_link_demand_marker(w-1) == LINK_DEMAND_NO)
		return;

	/* Clear any existing N+1/N+2 stage demands. 
	   Set to -6 to mark as stages which can be skipped */
	p_stages->demand_marker[stageNo] = STAGE_DEMAND_SKIP_AS_NOT_REQ_BY_6MRR;
	p_stages->demand_marker[stageNo+1] = STAGE_DEMAND_SKIP_AS_NOT_REQ_BY_6MRR;

	/* Implement 6MRR code, for the 4 links L,W,X,Y.
	   Set up durations of extra IG with sequences 1,2,4 or 1,3,4 */

	/* Set up e and f as intergreens N->N+1 and N->N+2.
	   Use abs() to allow for the unlikely possibility of negative IG.
	   The +5 allows for 0.5-sec min-green needed. */
	e = abs(DI_get_stage_change_allowance(stageNo, stageNo+1)) +5;
	f = abs(DI_get_stage_change_allowance(stageNo, stageNo+2)) +5;

	/* Choose GREEN needed, or equivalent min green, for each link */
	m = DI_get_lane_num(x, 0);
	aa = DI_get_max_min_green_time(m);

	h = get_lane_red_count_x_det(m-1) * DI_get_satinc_time(m) + DI_get_startup_lost_time(m);
	if (h > aa)
		h = aa;

	ra = max(max(timers.signal_state_timer, h), get_link_var_min_green_time(x-1));

	m = DI_get_lane_num(y,0);
	bb = DI_get_max_min_green_time(m);

	h = get_lane_red_count_x_det(m-1) * DI_get_satinc_time(m) + DI_get_startup_lost_time(m);
	if (h > bb)
		h = bb;

	rb = max(max(timers.signal_state_timer, h), get_link_var_min_green_time(y-1));

	/* Decide if right-turn GREEN difference justifies 3-stage sequence */
	a = ra - rb;
	if (a > e)
	{
		/* Set up 1,2,4 sequence */
		p_stages->demand_marker[stageNo] = STAGE_DEMAND_6MRR;
		p_stages->demand_marker[stageNo+2] = STAGE_DEMAND_6MRR;
		return;
	}
	if (-a > f)
	{
		/* Set up 1,3,4 sequence */
		p_stages->demand_marker[stageNo+1] = STAGE_DEMAND_6MRR;
		p_stages->demand_marker[stageNo+2] = STAGE_DEMAND_6MRR;
		return;
	}
	c = get_junction_smoothed_cycle_time()/10;

	ma = max(DI_get_stage_min_green_time(stageNo+3), DI_get_link_min_green_time(linkNo));
	d = get_link_y_value(linkNo-1)*c/10;
	if ( is_oversat_link(linkNo-1))
		d += g;
	if (d > ma)
		ma = d;

	mb = max(DI_get_stage_min_green_time(stageNo+3), DI_get_link_min_green_time(w));
	d = get_link_y_value(w-1)*c/10;
	if (is_oversat_link(w-1))
		d += g;
	if (d > mb)
		mb = d;

	/* Note AA is MAXMIN for Lane X */
	if (get_link_endsat_marker(x-1) == LINK_ENDSAT_NOT_YET && ra >= aa)
	{
		d = get_link_y_value(x-1)*c/10;
		if (is_oversat_link(x-1))
			d += g;
		if (d > ra)
			ra = d;
	}

	/* Note BB is MAXMIN for Lane Y */
	if (get_link_endsat_marker(y-1) == LINK_ENDSAT_NOT_YET && rb >= bb)
	{
		d = get_link_y_value(y-1)*c/10;
		if (is_oversat_link(y-1))
			d += g;
		if (d > rb)
			rb = d;
	}

	a = ra - rb;
	b = a + (ma - mb);

	/* Decide if main-traffic GREEN-delta + RT-delta justifies 3-stages */

	/* Increase E & F to 3-sec above intergreens N--> N+1 or N+2 */
	e += 25;
	f += 25;

	/* Check need for N+1 */
	if (a > 30 && b > e)
	{
		/* Set up 1,2,4 sequence */
		p_stages->demand_marker[stageNo] = STAGE_DEMAND_6MRR;
		p_stages->demand_marker[stageNo+2] = STAGE_DEMAND_6MRR;
		return;
	}

	/* Check need for N+2 */
	if (-a > 30 && -b > f)
	{
		/* Set up 1,3,4 sequence */
		p_stages->demand_marker[stageNo+1] = STAGE_DEMAND_6MRR;
		p_stages->demand_marker[stageNo+2] = STAGE_DEMAND_6MRR;
		return;
	}

	/* only 2-stage sequence 1,4 justified */
	p_stages->demand_marker[stageNo+2] = STAGE_DEMAND_6MRR;
	return;
}



/*!
*	\brief	Sets a conditional demand for a specific stage by setting its demand marker.
*
*	\param[in]	stageNo		The stage number.
*	\param[in]  linkNo		The link number.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */

static void set_conditional_demand(uint8 stageNo, uint8 linkNo)
{
#if defined (CMOVA_EP)
	/* M7.0.1 - e/p links should not place normal stage demands */
	if (!DI_is_ep_link(linkNo))
	{
#endif
		/* Set conditional demand for stage, unless already latched demand */
		if (p_stages->demand_marker[stageNo-1] == STAGE_DEMAND_NO_DEMAND)
		{
			p_stages->demand_marker[stageNo-1] = STAGE_DEMAND_CONDITIONAL;
			KDEBUG_WRITE2(DEBUG_LEVEL_VERBOSE, CORE, "Setting a conditional demand for stage number: %d at link no=%d", stageNo, linkNo);
		}

#if defined (CMOVA_EP)
	}
	else
	{
		/* Set conditional priority/emergency stage demand if appropriate */
		if (DI_is_priority_link(linkNo) && p_stages->priority_demand_marker[stageNo-1] == PRIORITY_DEMAND_NONE)
		{
			p_stages->priority_demand_marker[stageNo-1] = PRIORITY_DEMAND_UNLATCHED;
			KDEBUG_WRITE2(DEBUG_LEVEL_VERBOSE, CORE, "Setting a conditional priority demand for stage number: %d at link no=%d", stageNo, linkNo);

#ifdef M8_IMPORVED_BUS_PRIORITY
			set_demanded_priority_stage_num_by_a_link(linkNo - 1, stageNo);
#endif
		}

		if (DI_is_emeregency_link(linkNo) && p_stages->emergency_demand_marker[stageNo-1] == EMERGENCY_DEMAND_NONE)
			p_stages->emergency_demand_marker[stageNo-1] = EMERGENCY_DEMAND_UNLATCHED;

	}
#endif
}



/*!
*	\brief	Sets a latched demand for a specific stage by setting its demand marker.
*
*	\param[in]	stageNo		The stage number.
*	\param[in]  linkNo		The link number.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static void set_latched_demand(uint8 stageNo, uint8 linkNo)
{
#if defined (CMOVA_EP)
	/* M7.0.1 - e/p links should not place normal stage demands */
	if (!DI_is_ep_link(linkNo))
	{
#endif
		/* Set latched demand for stage */
		p_stages->demand_marker[stageNo-1] = STAGE_DEMAND_LATCHED;
		KDEBUG_WRITE2(DEBUG_LEVEL_VERBOSE, CORE, "Setting a latched demand for stage number: %d at link no=%d", stageNo, linkNo);

#if defined (CMOVA_EP)
	}
	else
	{
		/* Set latched priority/emergency stage demand if appropriate */
		if (DI_is_priority_link(linkNo))
		{
			p_stages->priority_demand_marker[stageNo-1] = PRIORITY_DEMAND_LATCHED;
			KDEBUG_WRITE2(DEBUG_LEVEL_VERBOSE, CORE, "Setting a latched priority demand for stage number: %d at link no=%d", stageNo, linkNo);

#ifdef M8_IMPORVED_BUS_PRIORITY
			set_demanded_priority_stage_num_by_a_link(linkNo-1, stageNo);
#endif
		}

		if (DI_is_emeregency_link(linkNo))
			p_stages->emergency_demand_marker[stageNo-1] = EMERGENCY_DEMAND_LATCHED;
	}
#endif
}


/*!
*	\brief	Chooses the PFACIL code to use based on:
*			- The link that set the priority demand;
*			- Passed Stage number; and 
*			- PFACIL codes hierarchy.
*
*	\param[in]	stageNumToCheck		The number of the stage in question.
*
*	\return		The chosen PFACIL code.
*
*	\author		Islam Abdelhalim
*	\date		19-09-2016 */
static int16 choose_priority_facility_code(uint8 stageNumToCheck)
{
	uint8		l; /* links loop counter*/
	uint8		p = 0; /*index for the 'pfacil_codes_of_demanding_links' array*/
	uint8		c; /*pfacil_codes_of_demanding_links loop counter*/

	int16		pfacil_codes_of_demanding_links[MAX_LINKS] = {0};
	int16		chosen_pfacil_code;

#ifndef M8_IMPORVED_BUS_PRIORITY
	return DI_get_priority_facility(stageNumToCheck);
#else

	for (l = 0; l < DI_get_links_count(); l++)
	{
		/*Check if this link has demanded any stage*/
		//if (get_demanded_priority_stage_num_by_a_link(l) != 0)
		if(get_link_demand_marker(l) == LINK_DEMAND_PRIORITY)
		{
			pfacil_codes_of_demanding_links[p++] = (int16)DI_get_priority_facility_matrix(stageNumToCheck, l + 1);
		}
	}
	
	/* First, check if there is no demand from the links */
	if (p == 0) 
		return 0; /*No truncation*/


	chosen_pfacil_code = 4; /*Max value*/

	/*The following part chooses the PFACIL based on the following hierarchy:
		-6, -5, -4, -1, -2, -3, 5, 1, 2, 3, 4. 
	  Where -6 is the highest (has the priority to be chosen) and 4 is the lowest.*/
	for (c = 0; c < p; c++)
	{
		if (pfacil_codes_of_demanding_links[c] == 5 && chosen_pfacil_code > 0)
		{
			chosen_pfacil_code = pfacil_codes_of_demanding_links[c];
		}
		else if (chosen_pfacil_code == 5 && pfacil_codes_of_demanding_links[c] < 0)
		{
			chosen_pfacil_code = pfacil_codes_of_demanding_links[c];
		}
		else if (chosen_pfacil_code != 5 && pfacil_codes_of_demanding_links[c] < chosen_pfacil_code)
		{
			chosen_pfacil_code = pfacil_codes_of_demanding_links[c];
		}
	}

	return chosen_pfacil_code;

#endif

}


/*!
*	\brief	Process the priority facility code (PFACILM) for a specific stage. 
*
*	\param[in]	currentStageIdx		The index of the current stage.
*	\param[in]  priorityStageIdx	The index of the priority stage.
*
*	\return		The priority stage number.
*
*	\author		Islam Abdelhalim 
*	\date		19-09-2016 */
int8 process_priority_facility_code(uint8 currentStageIdx /*n-1*/, uint8 priorityStageIdx /*k-1*/)
{
	uint8 current_stage_num = currentStageIdx+1; /*n*/
	uint8 priority_stage_num = priorityStageIdx + 1; /*;*/

	uint8 link_num;

	switch (choose_priority_facility_code(current_stage_num))
	{
	case 0:
		/* no truncation allowed */
		set_is_to_truncate_current_stage(mv_false);
		break;

	case -1:
	case 1:
		/* allow truncation only if no o'sat cycles */
		if (p_stages->oversat_max_counter[currentStageIdx] == 0)
			set_is_to_truncate_current_stage(mv_true);
		else
			set_is_to_truncate_current_stage(mv_false);
		break;

	case -2:
	case 2:
		/* allow truncation if no more than 1 cycle o'sat */
		if (p_stages->oversat_max_counter[currentStageIdx] <= 1)
			set_is_to_truncate_current_stage(mv_true);
		else
			set_is_to_truncate_current_stage(mv_false);
		break;

	case -3:
	case 3:
		set_is_to_truncate_current_stage(mv_true);

		for (link_num=1; link_num<=DI_get_links_count(); link_num++)
		{
			/* Skip irrelevant non-traffic links, or those red in CUSIG */
			
			if (!DI_is_traffic_link(link_num) || get_link_green_flag(link_num-1) == LINK_FLAG_NOT_GREEN)
				continue;

			if (get_link_ep_trunc_indicator(link_num-1) > 0 || get_link_ep_skip_indicator(link_num-1) > 0)
			{
				set_is_to_truncate_current_stage(mv_false);
				break;
			}
		}
		break;

	case 4:
		set_is_to_truncate_current_stage(mv_true);

		if (p_stages->oversat_weighting_factor[currentStageIdx] > 0)
		{
			/* constrain amount of green truncation if stage oversaturated */
			int g = p_stages->lambda[currentStageIdx]*(get_junction_smoothed_cycle_time()/20)/10;
          
			if (timers.signal_state_timer <= g)
			{
				/* Don't allow truncation until stage exceeds half normal green. 
					* If too short, jump to see if skipping of other stages allowed */
				set_is_to_truncate_current_stage(mv_false);
			}
		}
		break;

	case -4:
	case -5:
	case 5:
	default:
		/* PFACIL codes 5, -4, -5 always ignore truncation indicators */
		set_is_to_truncate_current_stage(mv_true);
		break;
	}



	/* Loop through the stages from CUSIG to the demanded priority stage K
		to determine whether intermediate stages can be skipped. */
	for (;;)
	{
		int inhibit_trunc = 0;
		int inhibit_skip = 0;

		current_stage_num++;
		if (current_stage_num > DI_get_stages_count())
			current_stage_num -= DI_get_stages_count();

		/* if priority stage 'priority_stage_num' reached then implement change */
		if (current_stage_num == priority_stage_num)
			break;

		/* skip stage if no demand */
		if (p_stages->demand_marker[current_stage_num-1] <= STAGE_DEMAND_NO_DEMAND)
			continue;

		if (choose_priority_facility_code(current_stage_num) >= 0)
		{
			/* Stage 'current_stage_num' can't be skipped, so it becomes next stage */
			priority_stage_num = current_stage_num;
			break;
		}

		/* Code <= -5 always allows skipping */
		if (choose_priority_facility_code(current_stage_num) <= -5)
			continue;

		/* Check link indicators */
		for (link_num=1; link_num<=DI_get_links_count(); link_num++)
		{
			/* Skip irrelevant e/p links, or those not green in stage N */
			if (DI_is_ep_link(link_num) || DI_get_link_green_status(current_stage_num, link_num) == LINK_STATUS_RED)
				continue;

			/* For unopposed link, with only normal demand, ignore ind's */
			if (DI_get_link_green_status(current_stage_num, link_num) == LINK_STATUS_GREEN_AND_UNOPPOSED)
				if(get_link_demand_marker(link_num-1) <= LINK_DEMAND_LATCHED)
					continue;

			/* Note if link truncated recently */
			if (get_link_ep_trunc_indicator(link_num-1) > 0)
				inhibit_trunc = 1;

			/* Note if link has skip indication set */
			if (get_link_ep_skip_indicator(link_num-1) > 0)
				inhibit_skip = 1;
		}

		if (choose_priority_facility_code(current_stage_num) < -1)
		{
			if (choose_priority_facility_code(current_stage_num) == -2 && p_stages->oversat_max_counter[current_stage_num-1] > 1)
			{
				/* if oversat 2 cycles can't skip */
				priority_stage_num = current_stage_num;
				break;
			}

			if (choose_priority_facility_code(current_stage_num) == -3 && inhibit_trunc > 0)
			{
				priority_stage_num = current_stage_num;
				break;
			}
		}
		else if (p_stages->oversat_max_counter[current_stage_num-1] > 0)
		{
			/* if oversat 1 cycle and pfacil >= 0 then can't skip */
			priority_stage_num = current_stage_num;
			break;
		}

		if (inhibit_skip > 0)
		{
			priority_stage_num = current_stage_num;
			break;
		}
	}


	return priority_stage_num;
}