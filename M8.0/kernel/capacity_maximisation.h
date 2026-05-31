/*******************************************************************************
*
*                      MOVA KERNEL ALGORITHM
*
*  FILE:         capactiy_maximisation.h
*
*  TITLE:        Mova Kernel: Capacity Maximisation Algorithm
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

#if !defined (CAPACITY_MAXIMISATION_H)
#define CAPACITY_MAXIMISATION_H

#include "mova_types.h"

struct CapacityMaxAlgInput
{
	int32 	flow_eff_factor; /*a*/
	int32	satflow_headway; /*c*/
	int32	excess_gap; /*e*/

	mv_bool	is_supersat_lane;
	uint8		nLinkMOVQINLaneNum;

	int32		register_veh; /*f*/
	int32		register_time; /*h*/

	int32 *	reduced_lost_time; /* REDLT[links]*/
	int32 *	smoothed_bonus_green_time; /*BONUSG[links]*/

	int32 *	max_percent_sat; /*maxpcs[MAX_LINKS]*/

	/* Timers*/
	int32 *	link_green_timer; /*lingre[links]*/
	int32 *	link_waste_green_timer; /*LWASTE[links]*/
};

int8 ALG_capacity_maximisation(uint8 linkIdx, int8 currentLinkEndsatMarker, 
								struct CapacityMaxAlgInput * pDataRef);

#endif /*CAPACITY_MAXIMISATION_H*/