/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         stages_handler.c
*
*  TITLE:        Mova Kernel: Stages Handler
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

#include <stdlib.h>

#include "gbltypes.h"

#include "dynamic_data.h"
#include "dataset_interface.h"
#include "mova_constants.h"

#include "junction_handler.h"
#include "stages_handler.h"
#include "links_handler.h"
#include "links_ep_handler.h"
#include "lanes_handler.h"
#include "stages_codes_handler.h"
#include "stages_ep_handler.h"

#include "timers_manager.h"
#include "algorithms_manager.h"
#include "messages_manager.h"
#include "core_interface.h"
#include "tma_alerts.h"



STATIC struct Stages	stages;

static mv_bool is_stage_demanded(uint8 stageIdx);

void init_stages_handler()
{
	set_stages_pointer_for_codes_handler(&stages);
	set_stages_pointer_for_ep_handler(&stages);

	CI_set_stages_pointer(&stages);
	ALG_set_stages_pointer(&stages);
	MSG_set_stages_pointer(&stages);
}


/*!
*	\brief	Initialises some variables in the Stages struct. This part used to be
*			in Mova Setup (Mova Tools) and the initial values written in the MDS file.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */	
void init_stages_dynamic_data()
{
	uint8 s; /* stages loop variable */

	int32 total_max_green = 0;

	double lambda;
	
	for(s=0; s < DI_get_stages_count(); s++)
	{
		total_max_green += DI_get_stage_max_green_time(s+1);
	}

	for(s=0; s < DI_get_stages_count(); s++)
	{
		stages.demand_marker[s] = 0;

		p_divide_error(total_max_green, 201, "Total max green was %d", 	total_max_green);
				
		lambda = (DI_get_stage_max_green_time(s+1) / ((double)total_max_green)) * 100;
		stages.alt_lambda[s] = stages.lambda[s] = (lambda >= 0) ? (int32)(lambda + 0.5) : (int32)(lambda - 0.5);
		
		stages.merit_value[s] = 100;
		stages.drift_max[s] = DI_get_stage_max_green_time(s+1);
	}
}


/*!
*	\brief	Updates the oversat weighting factor and the max counter for all 
*			the stages in the junction.
*
*	\param[in]	linkIdx		The index of the link that includes the lane.
*	\param[in]  laneIdx		The lane index to get its oversat counter.
*	\param[in]  tempFactor	The lane weighting factor.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void update_stages_oversat_markers(uint8 linkIdx, uint8 laneIdx, int32 tempFactor)
{
	uint8 s;  /*stages loop variable */

	for (s=0; s<DI_get_stages_count(); s++)
	{
		/* skip if link effectively red in stage 's' */
		if (is_link_effectively_red(s, linkIdx))
			continue;

		/* sum LANEWF's for all lanes green in stage */
		stages.oversat_weighting_factor[s] += tempFactor;
		
		/* Possible 'divide by zero' fault fix */
		if (stages.oversat_weighting_factor[s] < 0)
			stages.oversat_weighting_factor[s] = 0;


		/* Update max no of oversat cycles on any lane in stage */
		if (stages.oversat_max_counter[s] < get_lane_oversat_counter(laneIdx))
			stages.oversat_max_counter[s] = get_lane_oversat_counter(laneIdx);
	}
}



/*!
*	\brief	Mainly sets Stage::bonus_time and Stage::bonus_link_num. 
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void set_stages_bonus_time_and_link()
{
	uint8 s; /* stages loop variable */
	uint8 l; /* links loop variable */

	uint8 bonus_stage;
	int8 boncut_link;
	
	/* Resetting STABON and STBONL */
	for (s = 0; s < DI_get_stages_count(); s++) 
	{
		stages.bonus_time[s] = 0;
		stages.bonus_link_num[s] = 0;
	}

	/*FIRST; DECIDE WHICH LINK BONUS TO ALLOCATE TO EACH STAGE */
	for (l=0; l<DI_get_links_count(); l++)
	{
		/* Skip links with BONCUT <= 0 , POTENTIALLY NOT DOMINANT */
		if (DI_get_bonus_enabling_marker(l+1) <= 0)
			continue;
        
		/* STAGE IN WHICH LINK 'L' RECALCULATES ITS BONUS IS 'bonus_stage' */
		bonus_stage = get_link_bonus_stage(l);
				
		/* SAVE DOMINANT LINK BONUS; MAY BE OVERWRITTEN BELOW */
		
		stages.bonus_time[bonus_stage-1] = get_link_total_bonus_green_time(l);
		stages.bonus_link_num[bonus_stage-1] = l+1;
	}

	/*SECOND; CHECK LINKS WITH BONCUT < 0 POTENTIALLY SUBSERVIENT */
	for (l=0; l < DI_get_links_count(); l++)
	{
		if (DI_get_bonus_enabling_marker(l+1)>= 0)
			continue;
        
		/* LINK boncut_link DECIDES IF LINK L BONUS TO BE USED */
		boncut_link = -DI_get_bonus_enabling_marker(l+1);
		
		/* IGNORE LINK L BONUS IF DOMINANT LINK boncut_link IS OVERSAT, OR IF LINK L IS NOT O'SAT. ELSE, SAVE BONUS.*/
		if (is_oversat_link(boncut_link-1) || !is_oversat_link(l))
			continue;

		/* THIS OVERWRITES ANY NON-O'SAT DOMINANT LINK BONUS WITH OVERSAT SUBSERVIENT LINK BONUS DATA. */
		bonus_stage = get_link_bonus_stage(l);

		stages.bonus_time[bonus_stage-1] = get_link_total_bonus_green_time(l);
		stages.bonus_link_num[bonus_stage-1] = l+1;
	}

}


/*!
*	\brief	Updates the following parameters of the skipped stages: Stages::lambda,
*			Stages::demand_marker and Timers::stage_cycle_timer.It also 
*			updates Junction::cut_max_times_marker. The function is called 
*			at the end of a stage.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void update_skipped_stages_data_at_endstage()
{
	uint8 s; /*stages loop variable*/

	uint8 skipped_stage; /*n*/
	int8  cutmax;

	int32 a;

	for(s=1; s<=DI_get_stages_count(); s++)
	{
		skipped_stage = get_last_stage() + s;
		if (skipped_stage > DI_get_stages_count())
			skipped_stage -= DI_get_stages_count();
        
		/* decrement CUTMAX for stage just ended and any skipped */
		cutmax = get_junction_cut_max_times_marker(); 
		if (cutmax > 0)
			set_junction_cut_max_times_marker(cutmax-1);

		if (skipped_stage == get_previous_stage())
			break;

		/* update green proportion for skipped stages */
		a = 1;
		if (stages.lambda[skipped_stage-1] <= 5)
			a = 0;
		stages.lambda[skipped_stage-1] = (3*stages.lambda[skipped_stage-1]+a)/4;
        
		/* Zero marker for skipped stage not required by 6MRR code */
		if (stages.demand_marker[skipped_stage-1] == STAGE_DEMAND_SKIP_AS_NOT_REQ_BY_6MRR)
			stages.demand_marker[skipped_stage-1] = STAGE_DEMAND_NO_DEMAND;

		timers.stage_cycle_timer[skipped_stage-1] = 0;
		
		TMA_check_alert_exiting(pTmaData, skipped_stage-1, get_lanes_endsat_marker());
	}
}


/*!
*	\brief	Updates the lambda value (and alt-lambda) for a specific stage.
*			It also sets the smoothed cycle time for the junction.
*
*	\param[in]	stageIdx	The stage index.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void update_stage_lambdas(uint8 stageIdx)
{
	int32 a;
	int32 longI;

	p_divide_error((int32)(timers.stage_cycle_timer[stageIdx]/10), 201, 
					"previous stage was %d, stcycl was %d", 
					get_previous_stage(), 
					timers.stage_cycle_timer[get_previous_stage()-1]);
	
	a = timers.signal_state_timer*10/(timers.stage_cycle_timer[stageIdx]/10);

	/* CALCULATE NEW LAMBDA FOR STAGE JUST ENDED, THEN SMOOTH */
	stages.lambda[stageIdx] = (3*stages.lambda[stageIdx]+a+2)/4;
	stages.alt_lambda[stageIdx] = (3*stages.alt_lambda[stageIdx]+a+2)/4;

	/*  TRL mod:3107961 - Use 4-byte integer, longI, to prevent overflow*/
	longI = (int32)(3 * DI_get_stages_count());
	longI = (int32)(get_junction_smoothed_cycle_time()) * longI;
	longI = longI + (int32)(timers.stage_cycle_timer[stageIdx] +5); 

	p_divide_error((int32)(3*DI_get_stages_count()+1), 202);
	set_junction_smoothed_cycle_time( (int32)(longI/(int32)(3*DI_get_stages_count()+1)) );
}


/*!
*	\brief	Calculates the stage drift max value for all the stages.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void calc_stages_drift_max()
{
	uint8 s; /* stages loop variable*/

	/* calculations params*/
	int32 a;
	int32 b;
	int32 c;

	c = get_junction_smoothed_cycle_time() / 10;

	/* .87 * .87 gives total drift-max = 75% TOTALG.
		Remaining 25% is tolerance beyond drift-max */
	p_divide_error(get_junction_total_lambda(), 203,
				 	"lambda[0] was %d, lambda[1] was %d, lambda[2] was %d, "
					"lambda[3] was %d, lambda[4] was %d, lambda[5] was %d, ",
					stages.lambda[0], stages.lambda[1], stages.lambda[2], 
					stages.lambda[3], stages.lambda[4], stages.lambda[5] );
		
	a = DI_get_total_green_time()/10*87/get_junction_total_lambda();

	/* 'a' has range 50-800 sec.  High values give excessive max */
	if (a > 180)
	{
		/* avoid overflow and huge max's */
		if (a > 320)
			a = 320;
		
		p_divide_error(c, 206, "smoothed_cycle_time was %d", get_junction_smoothed_cycle_time());
		b = a/10*a/c*10;
	}
	else
	{
		p_divide_error(c, 204, "smoothed_cycle_time was %d", get_junction_smoothed_cycle_time());
		b = a*a/c;
	}

	/* 'b' is green to be apportioned (sec) */
	if (b > 320)
		b = 320;

	/* now update drift-max values */
	for (s = 0; s < DI_get_stages_count(); s++)
	{
		stages.drift_max[s] = (b*stages.alt_lambda[s]+25)/50*5;
	}

}


/*!
*	\brief	Calculates the extra green time for drift max based on 
*			the total green time.
*
*	\return		The extra green for drift max.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
 int32 calc_stages_extra_green_for_drift_max(int32 total_green /*g*/)
 {
	uint8 s; /*stage loop variable*/

	int32 total_merit_factor; /*aa*/
	int32 current_stage_merit_factor; /*a*/
	int32 factor; /*f*/
	int32 merit; /*c*/

	total_merit_factor = 0;

	/*The following line is just for safety*/
	current_stage_merit_factor = DI_get_stage_min_green_time(get_current_stage()) + 30;

	
	for (s=0; s<DI_get_stages_count(); s++)
	{
		factor = stages.merit_value[s] + 25*stages.oversat_weighting_factor[s];

		if (s+1 == get_current_stage())
			current_stage_merit_factor = factor;

		total_merit_factor += factor;
	}

	/* leave junction::extra_green_for_drift_max (mxplus) =0 if merit value for CUSIG < 5 */
	merit = current_stage_merit_factor/5;
	if (merit != 0)
	{
		p_divide_error(merit, 301);
		p_divide_error((int)(4*total_merit_factor/merit), 302, "total_merit_factor was %d, merit was %d", total_merit_factor, merit);

		/* Extra max (tolerance) is MXPLUS (0.1 sec) */
		/* 5/4//5=5/20 provides for 25 per cent tolerance */
		return 5*total_green/(4*total_merit_factor/merit);
	}
	else
	{
		return 0;
	}
}


/*!
*	\brief	Updates a specific stage demand marker based on the SDCODE (by calling
*			the process_sdcode() function)).
*
*	\param[in]	stageIdx	The stage index.
*
*	\return		The number of stages processed.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
int32 update_stage_demand(uint8 stageIdx)
{
	uint8 link_idx;
	int32 rcode = 0;

	/* Skip any stage that is part of a 4-stage sequence
	already rejected by 6MRR-type SDOCDE */
	if (stages.demand_marker[stageIdx]== STAGE_DEMAND_SKIP_AS_NOT_REQ_BY_6MRR)
		return 0;

	/* Stages may have normal latched demands:
		STADEM=1 for SDCODE 1 or 3X, or
		STADEM=6 for SDCODE 6MRR that has been selected. */

	/* Clear any conditional demands (STADEM=2) */
	if (stages.demand_marker[stageIdx] == STAGE_DEMAND_CONDITIONAL)
		stages.demand_marker[stageIdx] = STAGE_DEMAND_NO_DEMAND;

#if defined (CMOVA_EP)
	 /* Clear conditional em/pr demands (PSTDEM=8, ESTDEM=9) */
	if(stages.emergency_demand_marker[stageIdx] != EMERGENCY_DEMAND_LATCHED)
		stages.emergency_demand_marker[stageIdx] = EMERGENCY_DEMAND_NONE;

	if (stages.priority_demand_marker[stageIdx] != PRIORITY_DEMAND_LATCHED)
	{
		stages.priority_demand_marker[stageIdx] = PRIORITY_DEMAND_NONE;

#ifdef M8_IMPORVED_BUS_PRIORITY
		reset_demanded_priority_stage_num(stageIdx+1);
#endif
	}

#endif /*CMOVA_EP*/

	/* Continue to check for new demands, even if already have
		latched demand, because em/pr demands may occur later */
	for (link_idx=0; link_idx < DI_get_links_count(); link_idx++)
	{
#ifndef M8_REMOVE_REV_DEMAND
		/* Check for reversionary demand when link is red.
		Note demand can be <0 for e/p links */
		if ( get_link_rev_demand_marker(link_idx) == 0 || get_link_green_flag(link_idx) != LINK_FLAG_NOT_GREEN)
#endif
		{
			/* skip link if no demand */
			if(get_link_demand_marker(link_idx) == LINK_DEMAND_NO)
				continue;
		}

		rcode = process_sdcode(stageIdx+1, link_idx+1);
		if (rcode == 4) /*check if 6MRR code*/
			break;
	}

	return rcode;
}



/*!
*	\brief	Decides which stage to be activated next.
*
*	\return		The stage number.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
uint8 decide_next_stage()
{
	uint8 pass;
	uint8 k;
	uint8 i;

	for (pass=0; pass<2; pass++)
	{
		for (i = 1; i < DI_get_stages_count(); i++)
		{
			k = get_current_stage() + i;
			
			if (k >  DI_get_stages_count())
				k -=  DI_get_stages_count();

			/* Skip if no demand for this stage (use <=0 to catch -6 from 6MRR) */
			if (stages.demand_marker[k-1] <= STAGE_DEMAND_NO_DEMAND)
				continue;

			while (k != get_current_stage())
			{
				/* if change CUSIG-K allowed then implement */
				if (DI_get_stage_change_allowance( get_current_stage(), k) >= 0)
					return k;

				if (DI_get_stage_change_allowance( get_current_stage(), k) == -1)
				{
					/* if change banned then find an acceptable intermediate stage */
					k--;
					if (k <= 0)
						k = DI_get_stages_count();
				}
				else
				{
					/* if IG < -1 allow change on 2nd pass */
					if (pass > 0)
						return k;
					continue;
				}
			}
		}
	}

	/* no stage demands found that can be implemented */


	/* check reversionary stage */
	if (DI_get_main_stage_num() == 0 ||
		DI_get_main_stage_num() == get_current_stage() ||
		stages.demand_marker[DI_get_main_stage_num() - 1] > STAGE_DEMAND_NO_DEMAND)
		return 0;

	/* if no other demands, insert a reversionary demand for MAINST and find a suitable change */
	stages.demand_marker[DI_get_main_stage_num() - 1] = STAGE_DEMAND_REVERSIONARY;

	return decide_next_stage();
}



/*!
*	\brief	Updates the endsat marker of the next stage. The function implements the 
*			first part of this update which is called in the 
*			handle_junction_next_stage_updates() function.
*
*	\param[out]	isVarMinGreenEXpired	A flag that indicates if the var_min_green expired or not.
*
*	\return		FALSE if no need to continue the rest of the calling section.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
mv_bool update_next_stage_endsat_marker_part1(mv_bool * isVarMinGreenEXpired)
{
	uint8 next_stage_idx;
	uint8 link_idx;
	int32 green_time;
	mv_bool is_expired = mv_true; /*aa*/ /*is var-min-green or e/p hold time for any link expired*/
	
	next_stage_idx = get_next_stage() - 1;
		
	for (link_idx=0; link_idx < DI_get_links_count(); link_idx++)
	{
		/* skip links not GREEN now */
		if(DI_get_link_green_status( get_current_stage(), link_idx+1) == LINK_STATUS_RED)
			continue;

		/*  Compact MOVA: Check whether link has extendable green again *after* the fixed time 
			stage given current stage demands */
		if(DI_get_link_green_status(next_stage_idx+1, link_idx+1) >= LINK_STATUS_GREEN_IN_SEVERAL_STAGES &&
			((DI_get_stage_min_green_time(next_stage_idx+1) + 5) < DI_get_stage_max_green_time(next_stage_idx+1) ||
				check_green_in_following_stages(link_idx, next_stage_idx) == 0))
			continue; /* skip link if it gets another extendable green next stage */

		green_time = calc_link_current_green_time(next_stage_idx, link_idx);

		/*1: Check absolute-min-green expiry*/
		if (green_time < DI_get_link_min_green_time(link_idx+1))
			return mv_false; /* because the absolute-min-green has not expired yet, so no need to continue the checking*/

		/*2: Check e/p hold time expiry */
		if(is_link_ep_hold_time_expired(link_idx,green_time))
		{		
			/*3: Check var-min-green expiry */
			if (green_time < get_link_var_min_green_time(link_idx))
			{
#ifndef M8_REMOVE_REV_DEMAND 
				set_rev_demand_marker(link_idx, REV_DEMAND_VAR_MIN_GREEN_NOT_EXPIRED);
#endif
				is_expired = mv_false;
			}
#ifndef M8_REMOVE_REV_DEMAND 
			else 
				set_rev_demand_marker(link_idx, REV_DEMAND_NONE);
#endif
		}
		else
		{				
			is_expired = mv_false; /*E/P hold time NOT expired*/
		}
	}

	/* At this point all absolute-min-green times must have been reached */

	if (!is_expired) /*aa>0*/
	{
		/* An e/p hold or vari-min has not expired yet */
		/* Note this with STENSA=-2 (MARKER=3 still) */
		stages.endsat_marker[next_stage_idx] = STAGE_ENDSAT_VAR_MIN_GREEN_OR_EP_HOLD_NOT_EXPIRED;
	}
	else
	{
		stages.endsat_marker[next_stage_idx] = STAGE_ENDSAT_VAR_MIN_GREEN_OR_EP_HOLD_EXPIRED;
		*isVarMinGreenEXpired = mv_true; /* this is going to set prog.marker = PROG_MARKER_CHECKING_ENDSAT in the program_Controller */
	}

	return mv_true;
}



/*!
*	\brief	Updates the endsat marker of the next stage. The function implements the 
*			second part of this update which is called in the 
*			handle_junction_endsat_checking() function.
*
*	\return		TRUE if Prog::maker to be set with PROG_MARKER_DELAY_AND_STOPS_OPTI in the
*				program_controller.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
mv_bool update_next_stage_endsat_marker_part2()
{
	uint8 l; /* links loop variable */

	/* This flag notes if any relevant links still at satflow (did not reach endsat)*/
	mv_bool is_one_rel_link_not_endsat = mv_false;

	
	/*But first, sets the current stage merit value */
	stages.merit_value[ get_current_stage()-1] = calc_links_merit_value();


	/* this loop to find the relevant link that did not reach endsat (if any)*/
	for(l=0; l<DI_get_links_count(); l++)
	{
		/* Skip pedestrian & e/p links, or red links. */
		if(!DI_is_traffic_link(l+1) || get_link_green_flag(l) == LINK_FLAG_NOT_GREEN)
			  continue;
			
		/* Check if this link hasn't reached endsat and will be red in the next stage (relevant link)*/
		if( get_link_endsat_marker(l) == LINK_ENDSAT_NOT_YET &&
			get_link_benefiting_marker(l) == BENEFIT_RELEVENT_LINK)
		{
			is_one_rel_link_not_endsat = mv_true;
		}
    }	

    
	if( !is_one_rel_link_not_endsat )
	{
		/*Getting here means that ALL the relevant links (i.e., links that will be RED
			in next stage) have reached the endsat */
		stages.endsat_marker[ get_next_stage()-1 ] = STAGE_ENDSAT_REACHED_FOR_ALL_REL_LINKS;
		return mv_true;
	}

	/* Finish if any relevant link not yet end-sat*/
	return mv_false;
}



/*!
*	\brief	Compact MOVA: If link is green in a fixed stage, then has variable 
*			green in the stage immediately afterwards, it isn't a relevant link. 
*			This function checks for this. This function should be run whenever 
*			there's a change in demand for ANY stage (or every 0.5 seconds). 
*
*	\param[in]	nLinkIdx		The link index.
*	\param[in]  nNextStageIdx	The next stage index.
*
*	\return		TRUE if link is relevant, FALSE otherwise.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */        
mv_bool check_green_in_following_stages(uint8 nLinkIdx, uint8 nNextStageIdx)
{
    /* Assume link is relevant (i.e. no variable green after next stage) */
    mv_bool nIsRelevant = mv_true;

    /* Get the stage following the next stage */
    uint8 nStageLoop = nNextStageIdx + 1;

    if ( nStageLoop == DI_get_stages_count() ) /* Check for wrap-around */
    {
        nStageLoop = 0;
    }

    /* For each stage apart from the next stage itself in numerical order */
    while ( nStageLoop != nNextStageIdx )
    {	
        /*  If we can get to this stage from the next stage and 
            a demand for the stage exists */
        if ( DI_get_stage_change_allowance(nNextStageIdx+1, nStageLoop+1)> 0 
				&& is_stage_demanded(nStageLoop) )				
        {	
            /* If the link has green in this stage */
            if ( DI_get_link_green_status(nStageLoop+1, nLinkIdx+1) > 0 )
            {
                /*  If the stage isn't fixed */
                if ( DI_get_stage_min_green_time(nStageLoop+1)+5 < DI_get_stage_max_green_time(nStageLoop+1) )
                {
                    /*  This link is currently IRRELEVANT - it WILL have continued 
                        (variable) green in a stage following the next stage 
                        given current demands */
                    nIsRelevant = mv_false;
                    break;
                }
                
                /*  Else stage is fixed - go on to check further stages */
            }

            /* Else link has red in this stage */
            else
            {	
                /*  Link is currently RELEVANT (we've come to a stage after the next
                    stage in which the link will be red given the current
                    stage demands). */
                nIsRelevant = mv_true;
                break;
            }
        }
		
        /*  Else can't get to this stage from the next stage, or if we
            can there's no demand for it - go on to check further stages */

        /*  Get the number of the stage numerically after the next stage */
        nStageLoop++;
        if ( nStageLoop == DI_get_stages_count()) /* Check for wrap-around */
        {
            nStageLoop = 0;
        }

    } /*    For each stage apart from the next stage itself in numerical order */

    return nIsRelevant;

}




/*----------------------------------------------------------------*/
/*------------------------- Getters ------------------------------*/
/*----------------------------------------------------------------*/
/*oversatMax --> PEDMAX2 or BUSMAX2*/
/*unsatMax   --> PEDMAX1 or BUSMAX1*/
/*!
*	\brief	This function calculates the revised stage max value in case of ped stage or 
*			emergency (bus) stage.
*
*	\param[in]	oversatMax		the max value that to be used in case of the oversat.
*	\param[in]  unsatMax		the max value that to be used in case of the undersat.
*	\param[in]  stageNum		the stage number.
*	\param[in]  isForPedStage	TRUE if the caller of this function wants to calc the revised max green
*								for the ped stage. FALSE, if for the emergency stage.
*
*	\return		The max stage green time. Or '-1', if there is no need to do this calculation.
*
*	\author		Islam Abdelhalim 
*	\date		04-02-2015 */     
int32 calc_stage_revised_max_green_time(int32 oversatMax, int32 unsatMax, uint8 stageNum, mv_bool isForPedStage)
{
	uint8 l; /*links loop variable*/ /*nLinkNo*/
	uint8 k; /*lanes loop variable*/ /*nLinkNo*/

	int32 max_oversat_counter = 0; /*nMax_laosat*/
	uint8 lane_num; /*nLaneNo*/
	int32 new_stage_max; /*nNewStageMax*/


	/*First , check if the link in oversat of not (to define to use pedmax1/busmax1 to pedmax2/busmax2)*/
    
    /* loop for all links */
    for (l=0 ; l < DI_get_links_count(); l++)
    {
        /* find number of lanes on links green in this stage */
		if ( DI_get_link_green_status(stageNum, l+1) >= LINK_STATUS_GREEN_IN_ONE_STAGE &&  
				DI_get_link_green_status(stageNum, l+1) <= LINK_STATUS_GREEN_AND_UNOPPOSED )
        {
            /* find max laosat value for lanes on this link */
            for (k=0 ; k<DI_get_link_lanes_count(l+1) ; k++ )
            {
                lane_num = DI_get_lane_num(l+1, k);
                max_oversat_counter = max(max_oversat_counter, get_lane_oversat_counter(lane_num-1));
            }
        }
    }
    
    if ( max_oversat_counter < 4 )
    {
#ifdef M8_IMPORVED_BUS_PRIORITY
		if(!isForPedStage && unsatMax < 0) /*i.e., the calculation for the emergency (bus) stage*/
			return SKIP_MAX_GREEN_CALC; /*this is to skip any revision (i.e., use the normal stage max)*/
#endif
        /* unsaturated, so use usat_max (pedmax1 or busmax1) value directly */
        new_stage_max = unsatMax;
    } 
    else /*oversat, so used oversatMax (pedmax2 or busmax2)*/
    {
#ifdef M8_IMPORVED_BUS_PRIORITY
		if(!isForPedStage && unsatMax < 0) /*i.e., the calculation for the emergency (bus) stage*/
			unsatMax = abs(unsatMax);
#endif
        /* lambda in %, smcycl in 0.1s units */
		new_stage_max =  (stages.lambda[stageNum-1] * get_junction_smoothed_cycle_time() * oversatMax)/10000;
    }
    
    /* now add constraints */
    /* first not less than unsaturated max ped green */
    new_stage_max = max(new_stage_max, unsatMax);
    
    /* next not more than normal max's (lowmax if operating short cycles) */
	if (get_junction_cut_max_times_marker() > 0 )
        new_stage_max = min(new_stage_max, DI_get_stage_low_max_green_time(stageNum));
    else
		new_stage_max = min(new_stage_max, DI_get_stage_max_green_time(stageNum));

	/* last not less than normal min greens */
    new_stage_max = max(new_stage_max, ( DI_get_stage_min_green_time(stageNum) + 5)/10);
    
    return new_stage_max;
}

int32 get_stage_drift_max(uint8 stageIdx)
{
	return stages.drift_max[stageIdx];
}

int32	 get_stage_lambda(uint8 stageIdx)
{
	return stages.lambda[stageIdx];
}

int32  get_stage_low_drift_max(uint8 stageIdx)
{
	return stages.low_drift_max[stageIdx];
}

int8	get_stage_endsat_marker(uint8 stageIdx)
{
	return stages.endsat_marker[stageIdx];
}

int8 get_stage_demand_marker(uint8 stageIdx)
{
	return stages.demand_marker[stageIdx];
}

int8 get_stage_emergency_demand_marker(uint8 stageIdx)
{
	return stages.emergency_demand_marker[stageIdx];
}

int8 get_stage_priority_demand_marker(uint8 stageIdx)
{
	return stages.priority_demand_marker[stageIdx];
}

int32 * get_stages_low_drift_max()
{
	return stages.low_drift_max;
}

uint8 * get_stages_bonus_link_num()
{
	return stages.bonus_link_num;
}

#if defined (TRL_HOST_DEBUG)
int32 * get_stages_bonus_time()
{
	return stages.bonus_time;
}
#endif

static mv_bool is_stage_demanded(uint8 stageIdx)
{
	return	stages.demand_marker[stageIdx] > 0 ||
			stages.emergency_demand_marker[stageIdx] > 0 ||
			stages.priority_demand_marker[stageIdx] > 0;
}


/*----------------------------------------------------------------*/
/*------------------------- Resetters -----------------------------*/
/*----------------------------------------------------------------*/
void reset_stages_endsat_marker()
{
	uint8 s; /*stages loop variable*/

	for (s=0; s<DI_get_stages_count(); s++)
	{
		stages.endsat_marker[s] = STAGE_ENDSAT_NOT_YET;
	}
}

void reset_stages_oversat_max_marker()
{
	uint8 s; /* stages loop variable */

	/* But first, clear max oversat cycles on any lane and stage prior to resetting  */
	reset_max_lane_oversat_counter();	
		
	for (s=0; s<DI_get_stages_count(); s++)
	{
		stages.oversat_max_counter[s] = 0;
	}	
}

void reset_previous_stage_demand_markers()
{
	int8 previous_stage_idx = get_previous_stage() - 1;

	if(previous_stage_idx < 0)
		return;

	stages.demand_marker[previous_stage_idx] = STAGE_DEMAND_NO_DEMAND;

	stages.emergency_demand_marker[previous_stage_idx] = EMERGENCY_DEMAND_NONE;
	stages.priority_demand_marker[previous_stage_idx] = PRIORITY_DEMAND_NONE;

#ifdef M8_IMPORVED_BUS_PRIORITY
	reset_demanded_priority_stage_num(previous_stage_idx+1);
#endif

}

