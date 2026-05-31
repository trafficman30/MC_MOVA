/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         calculate_reduced_lost_time.c
*
*  TITLE:        Mova Kernel: Calculate Reduced Lost Time Algorithm
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
#include "calculate_reduced_lost_time.h"
#include "core_interface.h"

struct CalcReducedLostTimeAlgInput * pData = &calcReducedLostTimeAlgInput;  

static int32 calculate_link_reduced_lost_time(uint8 linkNum, int32 stageLambdaWeightSum, int32 LambdaSum, int32 stagOversatSum);


/*!
*	\brief	Implements the Reduced Lost Time calculation algorithm for all the links.
*
*	\param[out]	reduced_lost_time	The reduced lost time for all the links.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
void ALG_calculate_reduced_lost_time(int32 * reduced_lost_time)
{
	uint8 s; /* stages loop variable */
	uint8 l; /* links loop variable */

	/*SUM FROM stage_lambda_weight_sum=1 TO AVOID ZERO DIVIDE LATER
	  SUM ALL LAMBDA-WEIGHTED STAGE W.F.'S FOR JUNCTION */
	int32	stage_lambda_weight_sum = 1; /*b*/

	int32 lamdba_sum = 0; /*nLamdbaSum*/
	int32 stag_oversat_sum = 0; /*nStagosSum*/

	for (s = 1; s <= DI_get_stages_count(); s++) 
	{
		stage_lambda_weight_sum += pData->lambda[s-1] * pData->stage_oversat_weighting_factor[s-1];

		/* Sum lambda and stagos for DBZ checks 604, 605 and 606 (below) */
		lamdba_sum += pData->lambda[s-1];
		stag_oversat_sum += pData->stage_oversat_weighting_factor[s-1];
    }

	for (l = 1; l <= DI_get_links_count(); l++) 
	{
		/* Skip ped/emerg/priority link or link not oversat: 'REDLT' not needed */
		if (!DI_is_traffic_link(l) || pData->links_oversat_weighting_factor[l-1] <= 0)
			continue;

		reduced_lost_time[l-1] = calculate_link_reduced_lost_time(l, stage_lambda_weight_sum, lamdba_sum, stag_oversat_sum);
    }
}

/*!
*	\brief	Implements the Reduced Lost Time calculation algorithm for a single link.
*
*	\param[in]	linkNum					The number of this link.
*	\param[in]	stageLambdaWeightSum	The weighted lambda sum for all the stages.
*	\param[in]	LambdaSum				The lambda sum for all the stages.
*	\param[in]	stagOversatSum			The stage oversat weighting factor sum for all the stages.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
int32 calculate_link_reduced_lost_time(uint8 linkNum, int32 stageLambdaWeightSum, int32 LambdaSum, int32 stagOversatSum)
{
	uint8 s; /* stages loop variable */

	int32 c;
	int32 lost_time; /*d*/

	/* sum weighted STAGOSs for stages green to oversat link */
	c = 0;
	for (s = 1; s <= DI_get_stages_count(); s++) 
	{
		if (DI_get_link_green_status(s,linkNum) == 0 ||
			DI_get_link_green_status(s,linkNum) == 4)
			continue;

		c += pData->lambda[s-1] * pData->stage_oversat_weighting_factor[s-1];
    }

	/* Now compute lost time for link */

	if (c <= 0)
	{
		return 0; /* reduced lost time*/
	}

	lost_time = DI_get_lost_time(linkNum)/10;

	/* scale values to prevent overflow */
	if (c <= 320)
	{
		p_divide_error(stageLambdaWeightSum,   606, "stageLambdaWeightSum was %d, sum of lambda was %d, sum of stagos was %d",
								stageLambdaWeightSum, LambdaSum, stagOversatSum );
		
		return c*100/stageLambdaWeightSum*lost_time/10; /* reduced lost time*/
	}
	else if (c <= 3200)
	{
		p_divide_error((stageLambdaWeightSum/10),  605, "stageLambdaWeightSum was %d, sum of lambda was %d, sum of stagos was %d",
						stageLambdaWeightSum, LambdaSum, stagOversatSum );

		return c*10/(stageLambdaWeightSum/10)*lost_time/10; /* reduced lost time */
	}
	else
	{
		p_divide_error((stageLambdaWeightSum/100), 604, "stageLambdaWeightSum was %d, sum of lambda was %d, sum of stagos was %d",
			stageLambdaWeightSum, LambdaSum, stagOversatSum );

		return c/(stageLambdaWeightSum/100)*lost_time/10; /* reduced lost time */
	}
}



