/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         calculate_short_cycle_control_params.c
*
*  TITLE:        Mova Kernel: Calculate Short Cycle Control Params Algorithm
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
#include "calculate_short_cycle_control_params.h"

static struct CalcShortCycleCtrlParamAlgInput * pData = &calcShortCycleCtrlParamAlgInput;

static int32 calculate_max_theta();


/*!
*	\brief	Implements the Short Cycle Control parameters calculation algorithm.
*
*	\param[out]	low_total_green	The low total green for the whole junction. 
*	\param[out] low_drift_max	The low drift maximum for all the stages.
*
*	\return		FALSE if to skip the cutmax marker setting after calling this function.
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
mv_bool ALG_calculate_short_cycle_control_params( int32 * low_total_green, int32 * low_drift_max)
{
	uint8 s; /* stages loop variable */

	int32 g;
	int32	max_theta;
	
	max_theta = calculate_max_theta();

	if( max_theta == -1)
	{
		return mv_false; /* this is to skip setting the cutmax*/
	}

	*low_total_green = 0;

	for (s = 1; s <= DI_get_stages_count(); s++)
	{
		g = pData->smoothed_cycle_time/10*pData->alt_lambda[s-1]/100*max_theta;

		if (pData->stage_bonus_time[s-1] > 0)
			g -= pData->stage_bonus_time[s-1]/10*(100-max_theta);
        
		/* To make 'G' 0.1-SEC UNITS INSTEAD OF 0.01-SECONDS */
		g = g/10;

		/* SET LOW DRIFT-MAX'S TO 80% OF REQUIRED GREEN (0.1-SEC UNITS) */
		low_drift_max[s-1] = g*8/10;

		*low_total_green+= g;
	}

	return mv_true; /* this will enable setting the cutmax*/
}



/*!
*	\brief	Calculates the Max Theta to be used in the Short Cycle Control parameters calculation algorithm.
*
*	\return		The calulcated Max Theta or -1 if its value is unreasenable.
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
static int32 calculate_max_theta()
{
	uint8 s; /* stages loop variable */ 
	
	uint8 bonus_link_num;
	
	/*calculation parameters */
	int32 theta;
	int32 total_bonus = 0; /*b*/
	int32 max_lost_time = 0; /*c*/
	int32 max_theta = 0; /*d*/
	int32 g;
	int32 i;
	int32 j;
	
	/* SUM STAGE BONUSES INTO 'total_bonus',  SAVE MAX LOSTIM(L) IN 'max_lost_time', and max THETA in 'max_theta' */
	for (s = 1; s <= DI_get_stages_count(); s++) 
	{
		/* SKIP STAGE IF NO BONUS CURRENTLY SAVED FOR IT */
        if (pData->stage_bonus_time[s-1] == 0)
			continue;

		bonus_link_num = pData->stage_bonus_link_num[s-1];
		
		g = pData->smoothed_cycle_time/10*pData->alt_lambda[s-1]/10;
        
		/* CHECK IF ATTRIBUTING LINK GREEN NOT REACHING BONTIM REGULARLY */
		if (pData->links_oversat_weighting_factor[bonus_link_num-1] <= 0) /* not oversat*/
		{
			/* Skip to use bonus if link o'sat, even if green < BONTIM-2s */
			i = DI_get_bonus_green_recalc_time(bonus_link_num)-g;

			/* JUMP TO CLEAR STAGE BONUS IF MEAN LINK GREEN 2-SECONDS TOO SHORT */
			if (i > 20)
			{
				/* CLEAR STAGE BONUS IF LINK GREEN HAS NOT REACHED BONTIM REGULARLY */
				pData->stage_bonus_time[s-1] = 0;
				pData->stage_bonus_link_num[s-1] = 0;
				continue;
			}
		}

		/* NOW SUM STAGE BONUSES AND FIND MAXIMUM LOST-TIME  AND THETA VALUES */
		total_bonus += pData->stage_bonus_time[s-1];

        if (max_lost_time < DI_get_lost_time(bonus_link_num))
			max_lost_time = DI_get_lost_time(bonus_link_num);
        
		i = (DI_get_bonus_green_recalc_time(bonus_link_num) + pData->stage_bonus_time[s-1]) * 10;
		j = g + pData->stage_bonus_time[s-1];
        
		/* SKIP TO AVOID ZERO DIVIDE */
        if (j < 10)
			continue;

		/* Save max THETA value in 'max_theta' */
		theta = i/(j/10);
        if (max_theta < theta)
			max_theta = theta;
    }


	/* NOW CHECK WHETHER SHORT CYCLE ACTION NEEDED */
    if(max_theta <= 0 || total_bonus <= max_lost_time)
	{
		return -1; /* becaue theta is unreasenable */
	}
	else
	{
		return max_theta;
	}
}