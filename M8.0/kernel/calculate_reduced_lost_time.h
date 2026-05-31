/*******************************************************************************
*
*                      MOVA KERNEL ALGORITHM
*
*  FILE:         calculate_reduced_lost_time.h
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

#if !defined (CALCULATE_REDUCED_LOST_TIME_H)
#define CALCULATE_REDUCED_LOST_TIME_H

#include "mova_types.h"

struct CalcReducedLostTimeAlgInput
{
	int32 * lambda; /*LAMBDA[stages]*/
	int32 * stage_oversat_weighting_factor; /*STAGOS[stages]*/

	int32 * links_oversat_weighting_factor; /*LINKOS[links]*/
};


struct CalcReducedLostTimeAlgInput calcReducedLostTimeAlgInput; 

void ALG_calculate_reduced_lost_time(int32 * reduced_lost_time);

#endif /*CALCULATE_REDUCED_LOST_TIME_H*/