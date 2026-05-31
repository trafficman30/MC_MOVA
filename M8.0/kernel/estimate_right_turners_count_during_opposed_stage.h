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

#ifdef M8_RT_OPTIMISATION

#if !defined (ESTIMATE_RIGHT_TURNERS_COUNT_DURING_OPPOSED_STAGE_H)
#define ESTIMATE_RIGHT_TURNERS_COUNT_DURING_OPPOSED_STAGE_H

#include "mova_types.h"


uint8 ALG_estimate_right_turners_count_during_opposed_stage(uint8 stageNum,
															uint32 laneNum, 
															int32 * netGapsTime, 
															uint8 netGapsCount);

void ALG_rt_get_internel_data_for_message(uint8 * vehCountTurnedDuringIG, uint8 * vehCountTurnedDuringOpposingStage);
void ALG_rt_reset_internel_data_for_message();


#endif /*ESTIMATE_RIGHT_TURNERS_COUNT_DURING_OPPOSED_STAGE_H*/

#endif