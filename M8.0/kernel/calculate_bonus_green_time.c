/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         calculate_bonus_green_time.c
*
*  TITLE:        Mova Kernel: Calculate Bonus Green Timer Algorithm
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

#include "calculate_bonus_green_time.h"
#include "dataset_interface.h"
#include "timers_manager.h"
#include "core_interface.h"

#include "algorithms_manager.h"


static struct CalcBonusGreenTimeAlgInput * pData = &calcBonusGreenTimeAlgInput;

static mv_bool is_long_link_with_in_det(uint8 linkIdx);

/*!
*	\brief	Implements the Bonus Green Time calculation algorithm.
*
*	\param[out]	total_bonus_green_time	The calculated total bonus green time for all the links.
*	\param[out]	link_bonus_stage		The calculated bonus stage for all the links.
*	\param[out]	link_endsat_marker		The determined endsat marker for all the links.
*
*	\return		TRUE if the link bonus has changed in this calculations.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
mv_bool ALG_calculate_bonus_green_time(int32 * total_bonus_green_time,	uint8	* link_bonus_stage, int8 * link_endsat_marker)
{
	uint8 l; /* links loop variable */
	uint8 k; /* lanes loop variable */

	uint8 lane_num;
	uint8 lane_idx;
	uint8 x_det_num;
	uint8 bonus_stage; /*the stage that will be allocated bonus */

	mv_bool is_link_bonus_changed = mv_false;
	mv_bool is_x_det_suspected = mv_false;

	int32 b;
	int32 c;
		
	for (l = 1; l <= DI_get_links_count(); l++)
	{
		/* Skip if: red, no bonus, not bonus time, short link, is end-sat
		   Only traffic links remain. Ped & em/pr links have no bonus */
		if (DI_get_bonus_green_recalc_time(l) == 0 || 
			DI_get_bonus_green_recalc_time(l) != timers.link_green_timer[l-1])
			continue;
		
		if (pData->links_green_flag[l-1] == 0 || 
			DI_get_short_link_marker(l) > 0 ||
			pData->links_endsat_marker[l-1] > 0)
			continue;

		/* SKIP BONUS CALC FOR LINK WITH UNOPPOSED GREEN IN ANOTHER STAGE */
		if (pData->current_stage > 0 &&
			DI_get_link_green_status(pData->current_stage, l) == LINK_STATUS_GREEN_BUT_OPPOSED)
			continue;
		
		if (pData->current_stage == 0 &&
			DI_get_link_green_status(pData->last_stage, l) == LINK_STATUS_GREEN_BUT_OPPOSED)
			continue;

		/*LINK BONUSES SUMMED INTO 'B' AND 'C' IN 0.1 SEC UNITS*/
		b = 0;
		c = 0;

		for (k = 1; k <= DI_get_link_lanes_count(l); k++) 
		{
            lane_num = DI_get_lane_num(l, k-1);
			lane_idx = lane_num - 1;

            /*  Compact MOVA: Long lanes may exist without IN-dets in CMOVA */
			if (!DI_is_long_lane(lane_idx))
				continue;
			
			/* Skip if X-DET is suspected */
            x_det_num = DI_get_x_det_num(lane_num);
			if(ALG_is_det_suspected(x_det_num - 1))
			{
				is_x_det_suspected = mv_true;
				break;
			}

			b += (pData->red_count_x_det[lane_idx] + pData->exit_red_count[lane_idx] - DI_get_bonus_base_count(lane_num))
				*DI_get_satinc_time(lane_num);
            c += pData->sink_red_count[lane_idx] * DI_get_satinc_time(lane_num);
        }

		if(is_x_det_suspected)
		{
			is_x_det_suspected = mv_false;
			continue;
		}

		if (b < 0)
			b = 0;

		p_divide_error(DI_get_link_main_lanes_count(l), 607, "link number was %d", ( l + 1 ) );

		/* Now smooth exponentially */
        b = b/DI_get_link_main_lanes_count(l);
        c = c/DI_get_link_main_lanes_count(l);
		
		/* SAVE TOTAL BONUS FOR LINK IN TOTBON(L) */
		pData->bonus_green_time[l-1] = (b + pData->bonus_green_time[l-1] + pData->bonus_green_time[l-1]+2)/3;
        pData->sink_bonus_green_time[l-1] = (c + pData->sink_bonus_green_time[l-1] + pData->sink_bonus_green_time[l-1]+2)/3;
        
		total_bonus_green_time[l-1] = pData->sink_bonus_green_time[l-1] + pData->bonus_green_time[l-1];
		
		/* NOW CHECK WHETHER STAGE-BONUSES TO BE RESET BY THIS LINK (set in the DS) */
		/* if BONCUT =0, so disable the short-cycle-control for this link */
		if (DI_get_bonus_enabling_marker(l) != 0)
		{
			is_link_bonus_changed = mv_true;

			/* SET 'bonus_stage' TO STAGE TO BE ALLOCATED BONUS FOR LINK 'L' */
			bonus_stage = pData->current_stage;

			if (bonus_stage == 0)
				bonus_stage = pData->previous_stage;
			if (bonus_stage == 0)
				bonus_stage = pData->last_stage;

			link_bonus_stage[l-1] = bonus_stage;
		}

		/*Finally, set the link demand if needed*/

		/*UNLESS LINK IS OVERSAT, CHECK WHETHER TO SET END-SAT FLAG*/
		if(pData->link_oversat_weighting_factor[l-1] > 0)
			continue;

		/* JUMP UNLESS BONUS IS LARGE ENOUGH TO SET END-SAT FOR LINK.*/
		if (total_bonus_green_time[l-1] < DI_get_lost_time(l))
			continue;
			
		/* Compact MOVA: Don't set end-sat at BONTIM unless the link has IN-dets, otherwise we may 
			curtail green too abruptly due to the short-sighted delay-optimiser */
		if (is_long_link_with_in_det(l-1))
			link_endsat_marker[l-1] = LINK_ENDSAT_BONUS_GREEN_EXCEED_LOST_TIME;
	}

	return is_link_bonus_changed;
}


/*!
*	\brief	Determines whether the given long link has IN-DET or not. Used in Compact MOVA.
*
*	\param[in]	linkIdx	The link index.
*
*	\return		TRUE if the given link is long and has IN-DET.
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
static mv_bool is_long_link_with_in_det(uint8 linkIdx)
{
	uint8 k; /*lanes loop variable*/

	uint8 lanes_count = DI_get_link_lanes_count(linkIdx+1); /*nLanesNum*/
    uint8 lane_idx;
    mv_bool has_in_det = mv_true; 

    for (k=0; k<lanes_count; ++k)
    {
        lane_idx = DI_get_lane_num(linkIdx+1,k) - 1;
		
        /* If this lane is a long lane without an IN-detector */
        if ( DI_get_short_lane_length(lane_idx+1) == COMPACT_MOVA_LANE)
        {
            /*	This long link has at least one lane without
                an IN-detector (long lanes on a link should either *all*
                have IN-dets, or *none* have IN-dets - shouldn't be a mix) */
            has_in_det = mv_false;

            k = lanes_count; /* Quit loop */
        }
    }

    return has_in_det;
}