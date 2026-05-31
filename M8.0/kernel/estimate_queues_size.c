/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         estimate_queues_size.c
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


#include "estimate_queues_size.h"
#include "dataset_interface.h"
#include "core_interface.h"

#include "algorithms_manager.h"

static struct EstimateQueueSizeAlgInput * pData = &estimateQueueSizeAlgInput;


/*!
*	\brief	Implements the algorithm of estimating the queue size beyond the IN-DET.
*
*	\param[in]	laneNum					Lane number to estimate its queue.
*	\param[out] extra_veh_beyond_in_det	Estimated number of vehicle beyond the IN-DET.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
void ALG_estimate_queue_beyond_in_det(uint8 laneNum, int32 * extra_veh_beyond_in_det)
{
	uint8 in_det_num; /*d*/
	uint8 lane_idx = laneNum - 1;
	
	/*An estimate of the number of vehicles queued beyond the IN-DET*/
	int32 queue_estimate = -1; /* number of vehicles in the queue (came during the IN-DET ON time*/
	int32 queue_estimate_correction; /*a*/

	/*Number of vehicles that left the lane from: OUT, SINK or Associated X-DET */
	int32 exit_count; /*h*/

	in_det_num = DI_get_in_det_num(laneNum);

	/* if IN det exists and is ok, and conditions for queue blocking are met */
	if (in_det_num > 0 &&
		!ALG_is_det_suspected(in_det_num-1) &&
		pData->det_on_time[in_det_num-1] >= DI_get_det_min_on_time_for_queue(laneNum))
	{
		/* check for expected arrivals whilst IN det is on */
		p_divide_error(pData->smoothed_flow[lane_idx], 602, "lane number was %d", laneNum);
		p_divide_error((int)(3600/pData->smoothed_flow[lane_idx]), 601, "smflow was %d, lane number was %d",
						pData->smoothed_flow[lane_idx], laneNum );
        
		queue_estimate = pData->det_on_time[in_det_num-1] / (3600/pData->smoothed_flow[lane_idx]);
		
		exit_count = pData->exit_red_count[lane_idx] + pData->sink_red_count[lane_idx];
		if (exit_count != 0)
		{
			/* need to correct for EXITRC or SINKRC for exit_count <= REDCIN, limit multiplier to 2 */
			queue_estimate_correction = queue_estimate + queue_estimate;

			if (exit_count < pData->red_count_for_in_det[lane_idx])
			{
				p_divide_error(pData->red_count_for_in_det[lane_idx], 603, "lane number was %d", laneNum );

				queue_estimate_correction = queue_estimate * (exit_count + pData->red_count_for_in_det[lane_idx])/pData->red_count_for_in_det[lane_idx];
			}
			queue_estimate = queue_estimate_correction;
		}
	}

	/* the calcaultion done ... now set the 'extra_veh_beyond_in_det' */

	if(queue_estimate != -1)
	{
		/* increase queue beyond IN det if necessary */
		if (extra_veh_beyond_in_det[lane_idx] < queue_estimate)
		{
			extra_veh_beyond_in_det[lane_idx] = queue_estimate;
		}
	}
}
