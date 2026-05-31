/*******************************************************************************
*
*                      MOVA KERNEL ALGORITHM
*
*  FILE:         estimate_queues_size.h
*
*  TITLE:        Mova Kernel: Estimate Queues Size Algorithm
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

#if !defined (ESTIMATE_QUEUE_SIZE_H)
#define ESTIMATE_QUEUE_SIZE_H

#include "mova_types.h"

/* algorithm input data - none of them are modified in the algorithm*/
struct EstimateQueueSizeAlgInput
{
	int8	*	det_suspicion_status; /*SUSDET[dets]*/
	int32	*	det_on_time; /*TIMON[dets]*/

	int32 *	smoothed_flow; /*SMFLOW[lanes]*/
	int32 *	exit_red_count; /*EXITRC[lanes]*/
	int32 *	sink_red_count; /*SINKRC[lanes]*/
	int32 *	red_count_for_in_det; /*REDCIN[lanes]*/
};

struct EstimateQueueSizeAlgInput estimateQueueSizeAlgInput;

void ALG_estimate_queue_beyond_in_det(uint8 laneNum, int32 * extra_veh_beyond_in_det);

#endif /*ESTIMATE_QUEUE_SIZE_H*/