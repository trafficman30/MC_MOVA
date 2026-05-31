/*******************************************************************************
*
*                      MOVA KERNEL ALGORITHM
*
*  FILE:         calculate_bonus_green_time.h
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

#if !defined (CALCULATE_BONUS_GREEN_TIME_H)
#define CALCULATE_BONUS_GREEN_TIME_H

#include "mova_types.h"

struct CalcBonusGreenTimeAlgInput
{
	/* Input */
	uint8 	current_stage;		/*cusig*/
	uint8	last_stage;			/*lastag*/
	uint8	previous_stage;	/*presig*/

	int8	*	links_green_flag; /*lgrefl[links]*/
	int8	*	links_endsat_marker; /*liensa[links]*/

	int32 *	red_count_x_det;	/*redcx[lanes]*/
	int32	*	exit_red_count; /*EXITRC[lanes]*/
	int32	*	sink_red_count; /*SINKRC[lanes]*/
	int8	*	det_suspicion_status; /*SUSDET[dets]*/	
	int32	*	link_oversat_weighting_factor; /*LINKOS[links]*/

	/* Input & Modified(Output) */
	int32	*	bonus_green_time;	/*BONUSG[links]*/
	int32	*	sink_bonus_green_time;	/*SNKBON[links]*/
};

struct CalcBonusGreenTimeAlgInput calcBonusGreenTimeAlgInput;



mv_bool ALG_calculate_bonus_green_time(int32 * total_bonus_green_time, /*TOTBON[links]*/
									uint8	* link_bonus_stage, /*LBONST[links]*/
									int8 * link_endsat_marker);

#endif /*CALCULATE_BONUS_GREEN_TIME_H*/