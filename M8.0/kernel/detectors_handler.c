/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         detectors_handler.c
*
*  TITLE:        Mova Kernel: Detectors Handler
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

#include "MVTypes.h"

#include "lanes_handler.h"
#include "detectors_handler.h"
#include "dataset_interface.h"
#include "dynamic_data.h"
#include "timers_manager.h"

#include "algorithms_manager.h"
#include "messages_manager.h"
#include "core_interface.h"

#include "tma_alerts.h"

STATIC struct Detectors	dets;


void init_dets_handler()
{
	CI_set_dets_pointer(&dets);
	ALG_set_dets_pointer(&dets);
	MSG_set_dets_pointer(&dets);
}


/*!
*	\brief	Updates all the junction's detectors: On/Off time, Gaps and Cumulative counts.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
void update_dets_data()
{
	uint8 i; /* detectors loop variable */

	for (i = 0; i < DI_get_dets_count(); i++)
	{
		dets.on_det_flag[i] = 0;

		if (input_data.dets_on_flag[i] > 0)
			dets.on_det_flag[i] = 1;

		dets.time_on[i] = input_data.dets_on_time[i];
		dets.time_off[i] = input_data.dets_off_time[i];

		dets.old_gap[i] = input_data.dets_previous_gap[i];

		dets.old_cum_det_count[i] = dets.cum_det_count[i];
		dets.cum_det_count[i] = input_data.dets_veh_count[i];
	}
}

/*!
*	\brief	Updates all the detectors suspicious status for a specific lane based on 
*			their ON/OFF time and the average flow rate.
*
*	\param[in]	linkIdx		Index of the link to update its lane's detectors faulty status.
*	\param[in]  laneIdx		Index of the lane to update its detectors faulty status.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
void update_dets_faulty_status(uint8 linkIdx, uint8 laneIdx)
{
	uint8 det_num;
	int32 a,c;
	int32 aa;
	int32 average_flow; /*f*/

	uint8 i; /* dets types loop variable*/

	p_divide_error(get_lane_smoothed_flow(laneIdx), 207, "lane number was %d", laneIdx+1);

	aa = 32000 / get_lane_smoothed_flow(laneIdx);
	if (aa < 1200)
		aa = 1200;

	for (i = 0; i < MAX_DETS_TYPES; i++) 
	{
		if (i == ALT_UP_DET || i == ALT_DOWN_DET)
			continue;

		det_num = DI_get_det_num(laneIdx+1, i); 
		if (det_num <= 0)
			continue;
		
		/* CHECK#1: Check if the det is already suspicious */
		if (dets.suspect_status[det_num-1] != DET_FAULT_STATUS_OK)
		{
			/* detector was suspect, see if it is ok now */

			/* get the count at the end of the previous cycle,
				noting that sink dets store counts in different variables */
			a = dets.pre_cum_det_count[det_num-1];

			if (i == IN_SINK_DET)
				a = get_in_sink_cum_det_count(laneIdx);

			if (i == X_SINK_DET)
				a = get_x_sink_cum_det_count(laneIdx);

#ifdef M8_ADDITIONAL_SINK_DETS
			if (i == IN_SINK2_DET)
				a = get_in_sink_2_cum_det_count(laneIdx);

			if (i == X_SINK2_DET)
				a = get_x_sink_2_cum_det_count(laneIdx);
#endif
			/* check if cumulative count has increased in the last cycle */
			c = dets.cum_det_count[det_num-1] - a;

#ifdef M8_DET_FAULT_CHECKING_AND_LOGGING
			/*but, skip this check if it is a chattering detector.*/
			if (c > 0 && (dets.suspect_status[det_num - 1] != DET_FAULT_STATUS_SUSPECTED_DUE_TO_CHATTERING && dets.suspect_status[det_num - 1] != DET_FAULT_STATUS_SUSPECTED_DUE_TO_24_HOURS_CHATTERING))
#else
			if (c > 0)
#endif
			{
				/* This condition is a safety net to make sure that the following 
				   subtraction will not generate a positive number. */
				if (c > MV_INT8_MAX) c = MV_INT8_MAX;


#ifdef M8_DET_FAULT_CHECKING_AND_LOGGING
				/*Check if the det was faulty due to 24 hours (ON or OFF)*/
				if (dets.suspect_status[det_num - 1] >= DET_FAULT_STATUS_SUSPECTED_DUE_TO_24_HOURS_OFF)
				{
					/*Subtract 4 to make it as it was fault but not for 24 hours*/
					dets.suspect_status[det_num - 1] -= 4;
				}
#endif

				dets.suspect_status[det_num-1] -= (int8)c;


				if (dets.suspect_status[det_num-1] <= DET_FAULT_STATUS_OK)
				{
					dets.suspect_status[det_num-1] = DET_FAULT_STATUS_OK;

					TMA_det_faulty_changed(det_num-1, DET_FAULT_STATUS_OK);
				}
			}
		}

		/* CHECK#2: Check if the det was ON for a long time */
#ifdef M8_DET_FAULT_CHECKING_AND_LOGGING
		/*Skip long ON checking -> setting if it is already judged as 24 HOURS ON */
		if (dets.suspect_status[det_num - 1] != DET_FAULT_STATUS_SUSPECTED_DUE_TO_24_HOURS_ON)
#endif
		{
			if (dets.time_on[det_num - 1] >= timers.cycle_time_timer[linkIdx] ||
				dets.time_on[det_num - 1] >= 2400) /*4 minutes (2400/10/60)*/
			{
				/* long on dets set SUSDET = 2 */
				dets.suspect_status[det_num - 1] = DET_FAULT_STATUS_SUSPECTED_DUE_TO_LONG_ON;

				TMA_det_faulty_changed(det_num - 1, DET_FAULT_STATUS_SUSPECTED_DUE_TO_LONG_ON);
			}
		}

		/* Get average flows before checking for long OFF dets */
		average_flow = get_lane_current_average_flow(laneIdx);
				
		/* skip long OFF check if low smflow (<=150 veh/h) */
		if (average_flow <= 15)
			continue;

		/* skip long-off check for sink, comb and alt dets */
		if (i > OUT_DET && i < STOP_LINE_DET)
			continue;

		/* CHECK#3: if the det was OFF for a long time */
#ifdef M8_DET_FAULT_CHECKING_AND_LOGGING
		/*Skip long OFF checking -> setting if it is already judged as 24 HOURS OFF */
		if (dets.suspect_status[det_num - 1] != DET_FAULT_STATUS_SUSPECTED_DUE_TO_24_HOURS_OFF)
#endif
		{
			if (dets.time_off[det_num - 1] >= aa)
			{
				/* set susdet=1 for long-off dets */
				dets.suspect_status[det_num - 1] = DET_FAULT_STATUS_SUSPECTED_DUE_TO_LONG_OFF;

				TMA_det_faulty_changed(det_num - 1, DET_FAULT_STATUS_SUSPECTED_DUE_TO_LONG_OFF);
			}
		}
	}
}


void update_pre_cum_det_count_with_old(uint8 detIdx)
{
	dets.pre_cum_det_count[detIdx] = dets.old_cum_det_count[detIdx];
}

/*!
*	\brief	Calculates the X-DET count difference for the Compact MOVA lanes.
*
*	\param[in]	laneIdx			Lane index of the Compact MOVA lane.
*	\param[out]	countDiff		Detector's count difference.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
void adjust_count_diff_for_comb_x_det(uint8 laneIdx, int32 * countDiff)
{
	uint8 i; /* assoc detectors loop variable */
	uint8 l; /* lanes loop variable */

	uint8 det_num;

	/* For each associated detector */
	for (i=1; i<=2; i++ )
	{
		det_num = DI_get_assoc_det_num(laneIdx+1,i);
					
		/* If there is an associated detector that isn't suspect */
		if (det_num>0 && !is_det_suspected(det_num - 1))
		{
			/* For each lane */
			for (l=0; l<DI_get_lanes_count(); ++l)
			{
				/* If the X-det matches the assoc det */
				if (DI_get_x_det_num(l+1) == det_num)
				{
					/* Add in the osat initial count for this lane */
					*countDiff += get_lane_oversat_init_count(l);

					/* X-det is unique to lane, so quit loop */
					l = DI_get_lanes_count();
				}
			}
		}
	}
}


/*!
*	\brief	Chooses the right detector based on its availability and status to get 
*			its count that will be used in the Smoothed Flow calculation. 
*
*	\param[in]	laneIdx		The lane index of the lane that includes the detectors.
*	\param[in]  yy	The ....
*
*	\return		Choosed detector counts.
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
int32 get_dets_count_for_smoothed_flow_calc(uint8 laneIdx)
{
	uint8 i;  /*assoc det loop variable */

	int32 b,c;
	uint8 d, e;
	uint8 lane_num;

	lane_num = laneIdx + 1;

	d = DI_get_x_det_num(lane_num);
	if (get_det_suspicion_status(d) == DET_FAULT_STATUS_OK)
	{
		/* X det ok so use it */
		c =  get_det_counts_diff_pre(d);
		if (c < 0) 
			c = (int32) ((long)c+32767L); /* TRL/MC mod: cast to int */

		return c;
	}

	/* Try ALT-DWN det */
	d = DI_get_alt_down_det_num(lane_num);
	if (d > 0)
	{
		if (get_det_suspicion_status(d) == DET_FAULT_STATUS_OK)
		{
			c = get_det_counts_diff_pre(d);
			if (c < 0)
				c = (int32) ((long)c+32767L); /* TRL/MC mod: cast to int */
			return c;
		}
	}

	/* Try ALT-UP det */
    d = DI_get_alt_up_det_num(lane_num);
	if (d == 0)
		return -1;

	if (get_det_suspicion_status(d) > DET_FAULT_STATUS_OK)
		return -1;

	c = get_det_counts_diff_pre(d);
	if (c < 0)
		c = (int32) ((long)c+32767L); /* TRL/MC mod: cast to int */
			
	/* Check for an INSINK det.  If present and OK, adjust count */
	e = DI_get_in_sink_det_num(lane_num);
	if (e > 0)
	{
		if (get_det_suspicion_status(e) > DET_FAULT_STATUS_OK)
			return -1;

		/* IN-SINK is ok so subtract count from IN count */
		b = dets.cum_det_count[e-1] - get_in_sink_cum_det_count(laneIdx);
		if (b < 0)
			b = (int32) ((long)b+32767L); /* TRL/MC mod: cast to int */
		c = c - b;
	}

#ifdef M8_ADDITIONAL_SINK_DETS
	/* Check for an IN-SINK2 det.  If present and OK, adjust count */
	e = DI_get_in_sink_2_det_num(lane_num);
	if (e > 0)
	{
		if (get_det_suspicion_status(e) > DET_FAULT_STATUS_OK)
			return -1;

		/* IN-SINK2 is ok so subtract count from IN count */
		b = dets.cum_det_count[e-1] - get_in_sink_2_cum_det_count(laneIdx);
		if (b < 0)
			b = (int32) ((long)b+32767L); /* TRL/MC mod: cast to int */
		c = c - b;
	}
#endif

	/* check ASSOCD dets */
	for (i = 1; i <= MAX_EP_DETS; i++)
	{
		e = DI_get_assoc_det_num(lane_num, i);
		if (e == 0)
			continue;

		if (get_det_suspicion_status(e) > DET_FAULT_STATUS_OK)
		{
			return -1;
		}
		else
		{
			b = dets.cum_det_count[e-1] - get_assoc_cum_det_count(laneIdx, i-1);

            if (b < 0)
				b = (int32) ((long)b+32767L); /* TRL/MC mod: cast to int */
            c = c - b;
		}
	}

	return c;
}


/*!
*	\brief	Chooses the detector that will be used in the Capacity Maximisation algorithm.
*
*	\param[in]	linkIdx	The index of link that contains the lane.
*	\param[in]  laneIdx	The index of the lane that contains the detectors to choose between them,
*
*	\return		The chosen detector number.
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
int16 get_det_num_for_capacity_max_calc(uint8 linkIdx, uint8 laneIdx)
{
	uint8 det_num;
	uint8 lane_num = laneIdx + 1;

	/* Getting the X-DET number */
	det_num = DI_get_x_det_num(lane_num);
		
	if(is_det_suspected(det_num-1))
		det_num  = DI_get_alt_down_det_num(lane_num); /* Trying with the ALT-DWN */

    if(det_num == 0) 
		return NO_SUITABLE_DET;
				
	/* OR COMB-DET IF EXISTS */
	
	if(DI_get_comb_x_det_num(lane_num) > 0 && 
		timers.link_green_timer[linkIdx] >= DI_get_time_to_use_comb_x(lane_num)) 
	{
		det_num  = DI_get_comb_x_det_num(lane_num);
	}

	return det_num;
}


/*!
*	\brief	Inspects the status of all the e/p detectors (availability, faulty, On or Off)
*			for a specific link.
*
*	\param[in]	linkIdx				The index of the link that contains the e/p detector.
*	\param[in]  isEPDemandedInRed	A flag that determines if there is an e/p demand that happened during RED.
*
*	\return		TRUE if one of the e/p detectors is ON, otherwise FALSE.
*
*	\author		Islam Abdelhalim 
*	\date		12-03-2013 */
mv_bool is_ep_det_active(uint8 linkIdx, mv_bool isEPDemandedInRed)
{
	uint8 i; /*ep dets loop variable*/

	uint8 det_num;
	
	for (i=1; i<=MAX_EP_DETS; i++)
	{
		det_num = DI_get_ep_det_num(linkIdx+1, i);

		/* Skip if detector not present or suspect */
		if (det_num== 0)
			continue;

		if (is_det_suspected(det_num-1))
			continue;

		if (is_det_on(det_num) || isEPDemandedInRed)
		{
			/* if the epdet is ON indicate demand*/
			return mv_true;
		}
	}

	return mv_false;
}

#ifdef M8_DET_FAULT_CHECKING_AND_LOGGING

/*!
*	\brief	Update the suspicious status of ped dets.
*
*	\param[in]	linkIdx				The index of the link that contains the e/p detector.
*	\param[in]  isEPDemandedInRed	A flag that determines if there is an e/p demand that happened during RED.
*
*	\return		TRUE if one of the e/p detectors is ON, otherwise FALSE.
*
*	\author		Islam Abdelhalim
*	\date		12-03-2013 
*	\Note		This function was previously (M7) for the unlatched dets only. Starting from M8 the ped sus det
				will be for latched and unlateched ped dets.
*/

void update_ped_det_sus_status(uint8 detNum)
{
	if (detNum > 0)
	{
		/* Detector channel has been constantly 'on' for > 4-minutes
		so flag suspect detector, purely for information */
		if (get_det_on_time(detNum) > 2400)
		{
			if (get_det_suspicion_status(detNum) != DET_FAULT_STATUS_SUSPECTED_DUE_TO_24_HOURS_PED_LINK_FAULT)
			{
				set_det_suspicion_status(detNum, DET_FAULT_STATUS_SUSPECTED_PED_LINK);

				TMA_det_faulty_changed(detNum - 1, DET_FAULT_STATUS_SUSPECTED_PED_LINK);
			}
		}
		else
		{
			if (get_det_suspicion_status(detNum) == DET_FAULT_STATUS_SUSPECTED_PED_LINK ||
				get_det_suspicion_status(detNum) == DET_FAULT_STATUS_SUSPECTED_DUE_TO_24_HOURS_PED_LINK_FAULT)
			{
				TMA_det_faulty_changed(detNum - 1, DET_FAULT_STATUS_OK);
			}

			set_det_suspicion_status(detNum, DET_FAULT_STATUS_OK);
		}
	}
}
#endif

/*--------------------------------------------------------*/
/*----------------- Getters ------------------------------*/
/*--------------------------------------------------------*/
int32 get_cum_det_count(uint8 detIdx)
{
	return dets.cum_det_count[detIdx];
}

int32 get_det_counts_diff_old(uint8 detNum)
{
	return dets.cum_det_count[detNum-1] - dets.old_cum_det_count[detNum-1];
}

int32 get_det_counts_diff_pre(uint8 detNum)
{
	return dets.cum_det_count[detNum-1] - dets.pre_cum_det_count[detNum-1];
}

int8 get_det_suspicion_status(uint8 detNum)
{
	return dets.suspect_status[detNum-1];
}

int32 get_det_on_time(uint8 detNum)
{
	return dets.time_on[detNum-1];
}

int32 get_det_off_time(uint8 detNum)
{
	return dets.time_off[detNum-1];
}

int32 get_det_old_gap(uint8 detIdx)
{
	return dets.old_gap[detIdx];
}

mv_bool is_det_on(uint8 detNum)
{
	return dets.on_det_flag[detNum-1] > 0;
}

mv_bool is_queue_over_det(uint8 laneIdx, uint8 detIdx)
{	
	return dets.time_on[detIdx] >= DI_get_det_min_on_time_for_queue(laneIdx+1);
}

mv_bool is_det_suspected(uint8 detIdx)
{
	if (detIdx > MAX_DETS || detIdx < 0)
		return mv_false;

#ifdef M8_DET_FAULT_CHECKING_AND_LOGGING

	if (dets.suspect_status[detIdx] == DET_FAULT_STATUS_SUSPECTED_DUE_TO_CHATTERING || dets.suspect_status[detIdx] == DET_FAULT_STATUS_SUSPECTED_DUE_TO_24_HOURS_CHATTERING)
		return mv_false;
#endif

	return dets.suspect_status[detIdx] > DET_FAULT_STATUS_OK;
}


/*--------------------------------------------------------*/
/*----------------- Setters ------------------------------*/
/*--------------------------------------------------------*/
void set_det_suspicion_status(uint8 detNum, int8 status)
{
	dets.suspect_status[detNum-1] = status;
}

void set_det_pre_cum_det_count(uint8 detIdx, int32 val)
{
	dets.pre_cum_det_count[detIdx] = val;
}