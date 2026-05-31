/*******************************************************************************
*
*                      MOVA KERNEL ALGORITHM
*
*  FILE:         calculate_short_cycle_control_param.h
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

#if !defined (CALCULATE_SHORT_CYCLE_CONTROL_PARAMS_H)
#define CALCULATE_SHORT_CYCLE_CONTROL_PARAMS_H

struct CalcShortCycleCtrlParamAlgInput
{
	/* Input only*/
	int32		smoothed_cycle_time; /*SMCYCL*/
	int32	*	alt_lambda; /*ALTLAM[stages]*/
	int32 *	links_oversat_weighting_factor; /*linkos*/

	/* Input/Modified(Output) */
	int32	*	stage_bonus_time; /*STABON[stages]*/
	uint8	*	stage_bonus_link_num; /*STBONL[stages]*/
};


struct CalcShortCycleCtrlParamAlgInput calcShortCycleCtrlParamAlgInput;


mv_bool ALG_calculate_short_cycle_control_params(int32 * low_total_green, int32 * low_drift_max);

#endif /*CALCULATE_SHORT_CYCLE_CONTROL_PARAMS_H*/