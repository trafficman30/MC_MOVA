/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         lanes_handler.c
*
*  TITLE:        Mova Kernel: Lanes Handler
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

#include "dynamic_data.h"

#include "lanes_handler.h"
#include "lanes_endsat_handler.h"
#include "dataset_interface.h"
#include "junction_handler.h"
#include "stages_handler.h"
#include "links_handler.h"
#include "detectors_handler.h"

#include "timers_manager.h"
#include "algorithms_manager.h"
#include "messages_manager.h"
#include "core_interface.h"

#include "obclock.h"

#include "tma_alerts.h"

#include "math.h"

STATIC struct Lanes	lanes;

static int32 flstrt;
static int32 mcount;
static int32 aveflg;

static int32 calc_initial_average_flow();
static int32 decide_lane_det_fault_status(uint8 linkIdx, uint8 laneIdx);
static int8 get_upstream_det_fault_status(uint8 laneIdx);
static void update_lane_smoothed_flow(uint8 laneIdx, int32 averageFlow, int32 detCount);
static int32 dec_lane_oversat_counters(uint8 linkIdx, uint8 laneIdx);
static int32 inc_lane_oversat_counters(uint8 linkIdx, uint8 laneIdx);
static uint8 select_det_for_oversat_checking(uint8 laneNum, uint8 oversat_det);
static void set_shift_register_cell_value(uint8 laneIdx, int32 cell_position, int32 value);
static void adjust_red_and_sink_counts(uint8 laneIdx, uint8 detNum);

#ifdef M8_IMPORVED_BUS_PRIORITY
static void update_lane_shift_register_and_red_count_for_priority_link(uint8 laneIdx);
#endif

void init_lanes_handler()
{
	/*CHECK: this part was on MOVA under the Mova_InitEnd label*/
	flstrt = 0;
	mcount = -1;
	aveflg = calc_initial_average_flow();
	/*End of the part*/

	set_lanes_pointer_for_endsat_handler(&lanes); /* for the Lanes Endsat Handler */

	CI_set_lanes_pointer(&lanes);
	ALG_set_lanes_pointer(&lanes);
	MSG_set_lanes_pointer(&lanes);
}

void init_lanes_dynamic_data()
{
	uint8 l;

	for(l=0; l<DI_get_lanes_count(); l++)
	{
		lanes.smoothed_flow[l] = 18;
	}
}


/*!
*	\brief	Updates the shift register and red counts for a specific lane.
*
*	\param[in]	laneIdx	The lane index.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void update_lane_shift_register_and_red_counts(uint8 laneIdx)
{
	int32 i;
	uint8 in_det_num; /*d*/
	int32 cruise_in;   /*a*/
	int32 cruise_x;	/*b*/
	int32 count_diff; /*c and h*/
	uint8 x_det_num; /*e*/

	uint8 lane_num;

	lane_num = laneIdx+1;

	cruise_in =  DI_get_in_det_cruise_time(lane_num);
	cruise_x = DI_get_x_det_cruise_time(lane_num);
	in_det_num = DI_get_in_det_num(lane_num);
        
	/* Compact MOVA: Ignore -ve IN dets */
	if (in_det_num <= 0)
		cruise_in = cruise_x;

	/* shifting the shift-register slots */
#ifdef M8_SHIFT_REGISTER_EXTENSION
	for (i = 1; i < REG_SIZE; i++) 
#else
	for (i = 1; i < cruise_in; i++) 
#endif
	{
		lanes.shift_register[laneIdx][i-1] = lanes.shift_register[laneIdx][i];
	}

	/* when no IN-DET only lower end of SHREGA used */
    /* Compact MOVA: Ignore -ve IN dets */
	if (in_det_num > 0)
	{
		count_diff = get_det_counts_diff_old(in_det_num);
		if (count_diff != 0)
			count_diff = 1;

		/* insert IN count this scan at top of SHREGA */
		set_shift_register_cell_value(laneIdx, cruise_in-1, count_diff);
		
		/* and update red count */
		lanes.red_count_in_det[laneIdx] += count_diff;
		
		if (lanes.extra_veh_beyond_in_det[laneIdx] > 0)
			lanes.extra_veh_beyond_in_det[laneIdx] -= count_diff;

		set_shift_register_cell_value(laneIdx, cruise_in, 0);

		if (get_det_on_time(in_det_num) > 0 && get_det_on_time(in_det_num) < 25)
			set_shift_register_cell_value(laneIdx, cruise_in, 1);
	}

	/* Now process X-DET */
	x_det_num = DI_get_x_det_num(lane_num);
	count_diff = get_det_counts_diff_old(x_det_num);

	if (count_diff != 0)
		count_diff = 1;

	lanes.red_count_x_det[laneIdx] += count_diff;

	if(is_det_suspected(x_det_num-1) &&
		in_det_num > 0 &&
		!is_det_suspected(in_det_num-1))
	{
		/* X-DET dud, but IN-DET ok. Leave IN counts throughout SHREGA  */
	}
	else
	{
		/* otherwise insert X count at CRUSX position in SHREGA */
		set_shift_register_cell_value(laneIdx, cruise_x-1, count_diff);
			
		set_shift_register_cell_value(laneIdx, cruise_x, 0);

   	    /* if X-DET ON breiefly, insert vehicle in register */
		if (get_det_on_time(x_det_num) > 0 && get_det_on_time(x_det_num) < 25)
			set_shift_register_cell_value(laneIdx, cruise_x, 1);
	}

#ifdef M8_IMPORVED_BUS_PRIORITY
	update_lane_shift_register_and_red_count_for_priority_link(laneIdx);
#endif
}

/*!
*	\brief	Sets the value of the shift register cell instead of accessing the cell directly.
*			The main reason of this function, is to add the condition of not setting cell value if
*			A bus was insterted in it before (so that not to override the bus weight value).
*
*	\param[in]	laneIdx			The index of the lane to update its shift register.
*	\param[in]	cell_position	The cell index, in which the value will be set.
*	\param[in]	value			The value to be set.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		27-01-2015 */
static void set_shift_register_cell_value(uint8 laneIdx, int32 cell_position, int32 value)
{
#ifdef M8_IMPORVED_BUS_PRIORITY

	uint8	 priority_link_num = 0;
	
	uint8 a; /*associated pri links counter*/
	mv_bool is_cell_contains_weight_value = mv_false;
		

	for (a = 0; a < MAX_PRIORITY_LINKS_ACCOC_TO_LANE; a++)
	{
		priority_link_num = DI_get_associated_priority_link_num_for_a_lane(a, laneIdx + 1);

		if(priority_link_num > 0)
			is_cell_contains_weight_value |= (lanes.shift_register[laneIdx][cell_position] == DI_get_bus_weight(priority_link_num));
	}

	if (!is_cell_contains_weight_value)
	{
		lanes.shift_register[laneIdx][cell_position] = value;
	}

#else
	lanes.shift_register[laneIdx][cell_position] = value;
#endif
}

#ifdef M8_IMPORVED_BUS_PRIORITY
/*!
*	\brief	Update the shift register array for a lane that is associated
*			with e/p link. And also update the bus_weighted_red_count value.
*
*	\param[in]	laneIdx	The index of the lane to update its shift register.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static void update_lane_shift_register_and_red_count_for_priority_link(uint8 laneIdx)
{
	uint8 i; /*ep detectors loop (1 and 2 only)*/
	
	uint8 pri_det_num;
	uint8 pri_link_num;
	uint32 cruise_ep;

	int32 count_diff;
	
	uint8 a; /*associated pri links counter*/

	for (a = 0; a < MAX_PRIORITY_LINKS_ACCOC_TO_LANE; a++)
	{
		pri_link_num = DI_get_associated_priority_link_num_for_a_lane(a, laneIdx + 1);

		/*Exit the assoc for loop if any of them is zero because they are stored always from the first slot.*/
		if (pri_link_num == 0)
			break;

		for (i = 1; i <= MAX_EP_DETS; i++)
		{
			pri_det_num = DI_get_ep_det_num(pri_link_num, i);

			if (pri_det_num > 0)
			{
				if (is_det_suspected(pri_det_num-1))
					continue;

				cruise_ep = DI_get_ep_det_cruise_time(pri_link_num, i);
								
				if (cruise_ep > REG_SIZE || cruise_ep <= 0)
					continue;

				count_diff = get_det_counts_diff_old(pri_det_num);

				if (count_diff != 0)
					count_diff = 1;

				lanes.shift_register[laneIdx][cruise_ep - 1] = DI_get_bus_weight(pri_link_num) * count_diff;
				lanes.shift_register[laneIdx][cruise_ep] = 0;

				lanes.bus_weighted_red_count[laneIdx] += (DI_get_bus_weight(pri_link_num) * count_diff);

				if (get_det_on_time(pri_det_num) > 0 && get_det_on_time(pri_det_num))
					lanes.shift_register[laneIdx][cruise_ep] = DI_get_bus_weight(pri_link_num);
			}
		}
	}
}
#endif

/*!
*	\brief	Corrects the X-DET and IN-DET counts based on the availability and the counts
*			of the OUT-DET, IN-SINK, X-SINK and ASSOCD detectors.
*
*	\param[in]	laneIdx	The index of the lane to correct its detectors counts.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void correct_lane_red_count(uint8 laneIdx)
{
	uint8 i; /* assoc det loop variable */

	uint8 det_num; /*f,g*/
	uint8 lane_num;
	int32 count_diff;
	int32 a;

	lane_num = laneIdx + 1;

	/* If any OUT detector, decrement red counts */
	det_num = DI_get_out_det_num(lane_num);

	if (det_num != 0)
	{
		/* limit red count changes to 1 each scan */
		count_diff = get_det_counts_diff_old(det_num);
		if (count_diff != 0)
			count_diff = 1;
		
		lanes.exit_red_count[laneIdx] += count_diff;

		/* red counts must be non-negative, and non-zero if OUT det on */
		a = 0;
		if (is_det_on(det_num) > 0)
			a = 1;

		lanes.red_count_in_det[laneIdx] -= count_diff;
		if (lanes.red_count_in_det[laneIdx] < a)
			lanes.red_count_in_det[laneIdx] = a;

		lanes.red_count_x_det[laneIdx]  -= count_diff;
		if (lanes.red_count_x_det[laneIdx] < a)
			lanes.red_count_x_det[laneIdx] = a;
	}

	/* Now deal with IN-SINK */
	det_num = DI_get_in_sink_det_num(lane_num);
	if (det_num != 0 && !is_det_suspected(det_num-1))
	{
		count_diff = get_det_counts_diff_old(det_num);
		if (count_diff != 0)
		{
			lanes.red_count_in_det[laneIdx]--;
			lanes.sink_red_count[laneIdx]++;

			if (lanes.red_count_in_det[laneIdx] < 0)
				lanes.red_count_in_det[laneIdx] = 0;
		}
	}

#ifdef M8_ADDITIONAL_SINK_DETS
	/* IN-SINK2 */
	det_num =DI_get_in_sink_2_det_num(lane_num);
	if (det_num != 0 && !is_det_suspected(det_num - 1))
	{
		count_diff = get_det_counts_diff_old(det_num);
		if (count_diff != 0)
		{
			lanes.red_count_in_det[laneIdx]--;
			lanes.sink_red_count[laneIdx]++;

			if (lanes.red_count_in_det[laneIdx] < 0)
				lanes.red_count_in_det[laneIdx] = 0;
		}
	}
#endif

	/* X-SINK */
	det_num = DI_get_x_sink_det_num(lane_num);
	if (det_num > 0 && !is_det_suspected(det_num - 1))
	{
		adjust_red_and_sink_counts(laneIdx, det_num);
	}

#ifdef M8_ADDITIONAL_SINK_DETS
	/* X-SINK2 */
	det_num = DI_get_x_sink_2_det_num(lane_num);
	if (det_num > 0 && !is_det_suspected(det_num - 1))
	{
		adjust_red_and_sink_counts(laneIdx, det_num);
	}
#endif
	
	/* ASSOCD detectors*/
	for (i=1; i<= 2; i++)
	{
		det_num = DI_get_assoc_det_num(lane_num, i);

		if (det_num == 0)
			continue;

		if(is_det_suspected(det_num-1))
			continue;

		count_diff = get_det_counts_diff_old(det_num);
		if (count_diff == 0)
			continue;

		lanes.red_count_in_det[laneIdx]--;
		lanes.exit_red_count[laneIdx]++;
            
		if (lanes.red_count_in_det[laneIdx] < 0)
			lanes.red_count_in_det[laneIdx] = 0;
	}
}

static void adjust_red_and_sink_counts(uint8 laneIdx, uint8 detNum)
{
	int32 count_diff;

	count_diff = get_det_counts_diff_old(detNum);
	if (count_diff != 0)
	{
		lanes.red_count_x_det[laneIdx]--;
		lanes.sink_red_count[laneIdx]++;
		if (lanes.red_count_x_det[laneIdx] < 0)
			lanes.red_count_x_det[laneIdx] = 0;

		/* Maintain demand - if REDCIN=1 don't decrement */
		if (lanes.red_count_in_det[laneIdx] > 1)
			lanes.red_count_in_det[laneIdx]--;
		else if (lanes.red_count_in_det[laneIdx] < 0)
			lanes.red_count_in_det[laneIdx] = 0;
	}
}


/*!
*	\brief	Updates the oversat counter for a specific lane. It also initiates the update 
*			of the oversat weighting factor of the junction's stages and the oversat counters of
*			the related stage.
*
*	\param[in]	linkIdx	The index of the link that contains the lane.
*	\param[in]  laneIdx	The index of the lane to update its oversat marker.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void update_lane_oversat_counters(uint8 linkIdx, uint8 laneIdx)
{
	int32 temp_factor = 0; /* reset to stop stagos going negative (&subseq redlt< 0) */
	int32 oversat_check_time;  /* TIME TO CHECK OVERSAT IN 0.1 SEC UNITS */
	int32 count_diff;
	uint8 det_num;
	uint8 lane_num;
	uint8 link_num;

	uint8 oversat_det;
	int32 oversat_critical_veh_count;

	lane_num = laneIdx + 1;
	link_num = linkIdx + 1;

	/* skip short lanes unless short link, otherwise they can't be oversat */
	if (!DI_is_long_lane(laneIdx) && DI_get_short_link_marker(link_num) == 0)
		return;

	oversat_det = DI_get_oversat_det(lane_num);
	oversat_critical_veh_count = DI_get_oversat_critical_veh_count(lane_num);

#ifdef M8_USE_OF_CMOVA_OVERSAT_CHECKING
	det_num = select_det_for_oversat_checking(lane_num, oversat_det);
	
	/* if the IN-DET to be used for checking oversat and it is suspected, use CompactMOVA
		logic for detecting the oversaturation.*/
	if (det_num > 0 && oversat_det == OVERSAT_DET_IN_DET && is_det_suspected(det_num-1))
	{
		/* if IN-DET to be used for oversat checking and it is suspected use OSATCX instead of OSATCC*/
		oversat_critical_veh_count = DI_get_oversat_critical_veh_count_compact(lane_num);

		if(DI_get_comb_x_det_num(lane_num) == 0)
		{
			/* no comb-x det in this lane, so use the X-DET */
			oversat_det = OVERSAT_DET_CMOVA_X_DET;
		}
		else /* the comb-x det exists in this lane */
		{
			oversat_det = OVERSAT_DET_CMOVA_COMB_X;
		}
	}
#endif

	/* Compact MOVA: Use the stationary queue formation time (osat check time) 
		for (comb) X-detectors if CMOVA enabled, but use CRUSIN as normal for IN-dets */

	/*check if the IN-DET to be used for oversat checking (not a CMOVA lane) */
	if (oversat_det == OVERSAT_DET_IN_DET)  
	{
		oversat_check_time = 10 * DI_get_in_det_cruise_time(lane_num);
	}
	else /* Oversat det (XOSAT) = OVERSAT_DET_CMOVA_X_DET or OVERSAT_DET_CMOVA_COMB_X */
	{
		oversat_check_time = DI_get_oversat_time(lane_num);
	}

	/* Skip check if beyond the oversat time */
	if (timers.cycle_time_timer[linkIdx] > oversat_check_time)
	{
		update_stages_oversat_markers(linkIdx, laneIdx, temp_factor);
		update_lane_oversat_counter_max(laneIdx);
		return;
	}

	det_num = select_det_for_oversat_checking(lane_num, oversat_det);

	if (is_det_suspected(det_num-1))
	{
		if (lanes.oversat_counter[lane_num-1] == 0)
			return;
	
		/* can't do oversat check with suspect detector */
		temp_factor = dec_lane_oversat_counters(linkIdx, laneIdx);

		update_stages_oversat_markers(linkIdx, laneIdx, temp_factor);
		update_lane_oversat_counter_max(laneIdx);
		return;
	}

	/*Compact MOVA: for combination dets, just check the det in main lane for queue back 
        (this behaviour is similar to standard MOVA in that standard MOVA 
        doesn't check short lanes on long links for queue back) */
	if (oversat_det == OVERSAT_DET_CMOVA_COMB_X)
	{
		/*  If the X-det in the main lane has been on long enough, set osatic
			high enough so that osat will be flagged */

		if (get_det_on_time( DI_get_x_det_num(lane_num) ) >= DI_get_det_min_on_time_for_queue(lane_num))
			lanes.oversat_init_count[laneIdx] = oversat_critical_veh_count;
	}
	else /* Not using a comb-det */
	{
		if (get_det_on_time(det_num) >= DI_get_det_min_on_time_for_queue(lane_num))
			lanes.oversat_init_count[laneIdx] = oversat_critical_veh_count;
	}

	/* Check if it is too soon to do oversat critical count check */
	if (timers.cycle_time_timer[linkIdx] < oversat_check_time)
	{				
		update_stages_oversat_markers(linkIdx, laneIdx, temp_factor);
		update_lane_oversat_counter_max(laneIdx);
		return;
	}

	count_diff = get_det_counts_diff_pre(det_num);
	if (count_diff < 0)
		count_diff = (int32)((long)count_diff+32767L);

	count_diff += lanes.oversat_init_count[laneIdx];

	if (oversat_det == OVERSAT_DET_CMOVA_COMB_X)
		adjust_count_diff_for_comb_x_det(lane_num-1, &count_diff);

	if (count_diff >= oversat_critical_veh_count)
	{
		temp_factor = inc_lane_oversat_counters(linkIdx, laneIdx);

		update_stages_oversat_markers(linkIdx, laneIdx, temp_factor);
		update_lane_oversat_counter_max(laneIdx);
		return;
	}

	if (lanes.oversat_counter[laneIdx] == 0)
		return;
	
	temp_factor = dec_lane_oversat_counters(linkIdx, laneIdx);

	update_stages_oversat_markers(linkIdx, laneIdx, temp_factor);
	update_lane_oversat_counter_max(laneIdx);
}



/*!
*	\brief	Selects the appropriate detector for the oversat checking.
*
*	\param[in]	laneNum		The number of the lane to select a detector from it.
*
*	\return		The selected detector number.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static uint8 select_det_for_oversat_checking(uint8 laneNum, uint8 oversat_det)
{
	uint8 d;

	switch (oversat_det)
	{
		case OVERSAT_DET_IN_DET: /* IN-det */
			/*	N.B. 'd' could theoretically be 0/negative here - 
				we rely on DI_get_oversat_det() (XOSAT) being set correctly in
				MOVA setup to avoid this. */
			d = DI_get_in_det_num(laneNum);
			break;

		case OVERSAT_DET_CMOVA_X_DET: /* X-det */
			d = DI_get_x_det_num(laneNum);
			break;

		case OVERSAT_DET_CMOVA_COMB_X: /* Comb X-det */
			d = DI_get_comb_x_det_num(laneNum);               
			break;
				
		default:
			/*  Should never, ever get here!
				Use X-det by default, as we know there will be one. */
			d = DI_get_x_det_num(laneNum);
			break;
	}

	return d;
}



/*!
*	\brief	Decreases the oversat counter for a specific lane if it is not oversat.
*
*	\param[in]	linkIdx		The index of the link that contains this lane.
*	\param[in]  laneIdx		The index of the lane to decrease its oversat counter. 
*
*	\return		The negative value of the lane weighting factor.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static int32 dec_lane_oversat_counters(uint8 linkIdx, uint8 laneIdx)
{
	int32 factor;

	dec_junction_oversat_counter(lanes.oversat_counter[laneIdx]);

	lanes.oversat_counter[laneIdx] = 0;

	TMA_reset_oversat_alert_counter(laneIdx);
	TMA_check_alert_overSat(pTmaData);

	set_is_oversat_changed(mv_true);

	factor = DI_get_lane_weighting_factor(laneIdx+1);

	dec_link_oversat_weighting_factor_counter(linkIdx, factor);

	return -factor;
}

/* LANE IS OVERSAT, SO INCREASE OVERSAT COUNTERS AS NECESSARY */
/*!
*	\brief	Increase the oversat counter for a specific lane if it is oversat.
*
*	\param[in]	linkIdx		The index of the link that contains this lane.
*	\param[in]  laneIdx		The index of the lane to increase its oversat counter. 
*
*	\return		The lane weighting factor.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static int32 inc_lane_oversat_counters(uint8 linkIdx, uint8 laneIdx)
{
	int32 factor = 0;

	set_is_oversat_increased(mv_true);
		
	TMA_inc_oversat_counter(laneIdx);
	TMA_check_alert_overSat(pTmaData);

	if (lanes.oversat_counter[laneIdx] < 9)
	{
		/* skip if already had 9 consecutive o'sat cycles */

		lanes.oversat_counter[laneIdx]++;
		
		inc_junction_oversat_counter(1);

		/* LINKOS markers only updated if LAOSAT < 2 (already set otherwise) */
		if (lanes.oversat_counter[laneIdx] <= 1)
		{
			set_is_oversat_changed(mv_true);
			
			factor = DI_get_lane_weighting_factor(laneIdx+1);

			inc_link_oversat_weighting_factor_counter(linkIdx, factor);
		}
	}

	return factor;
}



/*!
*	\brief	Contains the logic to determine the detector's faulty status marker for a specific lane. 
*			The function depends on the detector's suspicion status that 
*			has been set in update_dets_faulty_status().
*
*	\param[in]	linkIdx		The index of the link that contains the lane.
*	\param[in]  laneIdx		The index of the lane to update its detectors faulty status.
*
*	\return		The det_fault_marker of the lane.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static int32 decide_lane_det_fault_status(uint8 linkIdx, uint8 laneIdx)
{
	uint8 i; /* assoc det loop variable */

	int32 retval;
	uint8 aa; /*flag: true if has sus assoc det*/
	uint8 x_det_num; /*b*/
	uint8 alt_det_num; /*e*/
	uint8 assoc_det_num;   /*a*/
	uint8 comb_x_det_num; /*c*/

	retval = lanes.det_fault_marker[laneIdx];

	aa = 0;
	x_det_num = DI_get_x_det_num(laneIdx+1);
	comb_x_det_num = DI_get_comb_x_det_num(laneIdx+1);
	
	if (comb_x_det_num == 0)
	{
		/* No COMBX det so check others */

		/* X-DET is OK? */
		if (!is_det_suspected(x_det_num - 1))
		{
			if (DI_get_in_sink_det_num(laneIdx+1) != 0)
				retval = LANE_DET_FAULT_NEED_2_PART_ENDSAT_CHECK;

#ifdef M8_ADDITIONAL_SINK_DETS
			if (DI_get_in_sink_2_det_num(laneIdx+1) != 0)
				retval = LANE_DET_FAULT_NEED_2_PART_ENDSAT_CHECK;
#endif
			return retval;
		}

		/* Is there an ALT-DWN det */
		alt_det_num =  DI_get_alt_down_det_num(laneIdx+1);
		if (alt_det_num != 0)
		{
			if(!is_det_suspected(alt_det_num-1))
			{
				if (DI_get_in_sink_det_num(laneIdx+1) != 0)
					retval = LANE_DET_FAULT_NEED_2_PART_ENDSAT_CHECK;

#ifdef M8_ADDITIONAL_SINK_DETS
				if (DI_get_in_sink_2_det_num(laneIdx+1) != 0)
					retval = LANE_DET_FAULT_NEED_2_PART_ENDSAT_CHECK;
#endif
				return retval;
			}
		}
	}
	else
	{
		for (i = 1; i <= MAX_EP_DETS; i++) 
		{
			assoc_det_num = DI_get_assoc_det_num(laneIdx+1, i);
			if (assoc_det_num > 0 && is_det_suspected(assoc_det_num -1))
				aa = 1;
		}

		if (!is_det_suspected(x_det_num-1) && aa == 0)
		{
			/* all components of COMB-X are ok */
			if (DI_get_in_sink_det_num(laneIdx+1) != 0)
				retval = LANE_DET_FAULT_NEED_2_PART_ENDSAT_CHECK;

#ifdef M8_ADDITIONAL_SINK_DETS
			if (DI_get_in_sink_2_det_num(laneIdx+1) != 0)
				retval = LANE_DET_FAULT_NEED_2_PART_ENDSAT_CHECK;
#endif
			return retval;
		}

		set_det_suspicion_status(comb_x_det_num, DET_FAULT_STATUS_SUSPECTED_DUE_TO_LONG_OFF);

		TMA_det_faulty_changed(comb_x_det_num -1, DET_FAULT_STATUS_SUSPECTED_DUE_TO_LONG_OFF);

		if (is_det_suspected(x_det_num-1) && aa > 0)
		{
			retval = LANE_DET_FAULT_TOO_MANY_TO_DO_ENDSAT_CHECK;
			/* Compact MOVA: do not assume IN-det means long link */
			if (DI_is_long_lane(laneIdx) || DI_get_short_link_marker(linkIdx+1) != 0)
			{
				inc_link_det_fault_marker(linkIdx,retval);
			}
			return retval;
		}

		if(!is_det_suspected(x_det_num-1))
		{
			alt_det_num = DI_get_alt_up_det_num(laneIdx+1);

			if (alt_det_num == 0 || (alt_det_num > 0 && is_det_suspected(alt_det_num -1)))
			{
				retval = LANE_DET_FAULT_TOO_MANY_TO_DO_ENDSAT_CHECK;
				/* Compact MOVA: do not assume IN-det means long link */
				if (DI_is_long_lane(laneIdx) || DI_get_short_link_marker(linkIdx+1)!= 0)
				{
					inc_link_det_fault_marker(linkIdx,retval);
				}
				return retval;
			}

			/* increment LIDETF so don't do oversat endsat check (C4) */
			inc_link_det_fault_marker(linkIdx,1);

			/* identify lane as needing 2-part ES check */
			return LANE_DET_FAULT_NEED_2_PART_ENDSAT_CHECK;
		}
	}

	/* Can't check endsat on X, COMBX, ALTXDN dets due to faults */

	/* Check upstream alternatives */
	retval = get_upstream_det_fault_status(laneIdx);

	/* Compact MOVA: do not assume IN-det means long link */
	if (DI_is_long_lane(laneIdx) || DI_get_short_link_marker(linkIdx+1) != 0)
		inc_link_det_fault_marker(linkIdx,retval);

	return retval;
}


/*!
*	\brief	Updates the lane's dets status and then returns a det count to be used 
*			in calculating the smoothed flow for this lane. 
*
*	\param[in]	linkIdx		The index of the link that contains the lane.
*	\param[in]  laneIdx		The index of the lane to update its detector's status.
*
*	\return		The detectors count to be used in the smoothed flow calc.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
int32 update_lane_dets_status(uint8 linkIdx, uint8 laneIdx)
{
	int32 det_counts;

	if(is_in_warmup())
	{
		det_counts = -1;
	}
	else
	{
		update_dets_faulty_status(linkIdx, laneIdx);

		set_lane_det_fault_status(linkIdx, laneIdx);

		det_counts = get_dets_count_for_smoothed_flow_calc(laneIdx);
	}

	return det_counts;
}


/*!
*	\brief	Calculates the lane's smoothed flow.
*
*	\param[in]	linkIdx		The index of the link that contains the lane.
*	\param[in]  laneIdx		The index of the lane to calc its smoothed flow.
*	\param[in]  detCount	The detectors count that will be used in the calculation.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void calc_lane_smoothed_flow(uint8 linkIdx, uint8 laneIdx, int32 detCount /*c*/)
{
	int32 average_flow; /*f*/
	int32 det_count_from_history; /*c*/


#ifdef M8_USE_OF_ALT_DOWNSTREAM_DETS
	uint8 x_det_num;
	uint8 alt_up_det_num;
	uint8 alt_down_det_num;

	/* Add here conditions that check if the:
	X-detector is faulty and
	there is no alternative upstream det or it is also faulty and
	there is no alternative downstream det or it is also faulty*/

	x_det_num = DI_get_x_det_num(laneIdx + 1);
	alt_up_det_num = DI_get_alt_up_det_num(laneIdx + 1);
	alt_down_det_num = DI_get_alt_up_det_num(laneIdx + 1);
		
	if(is_det_suspected(x_det_num-1) && 
		(alt_up_det_num == 0 ||is_det_suspected(alt_up_det_num-1)) &&
		(alt_down_det_num == 0 || is_det_suspected(alt_down_det_num - 1)))
	{
		average_flow = get_lane_current_average_flow(laneIdx);

		p_divide_error(average_flow, 403, "link number was %d", (linkIdx + 1));

		/* estimate cyclic flow count from historical flow */
		det_count_from_history = timers.cycle_time_timer[linkIdx] / 10 * average_flow / 360;

		update_lane_smoothed_flow(laneIdx, average_flow, det_count_from_history);

		return;
	}
#endif

	/* Check if the dets count is unreasonable (negative value) */
	if (detCount < 0)
	{
		/* count from dets unreasonable - use historic flow */
			
		average_flow = get_lane_current_average_flow(laneIdx);

		p_divide_error(average_flow, 403, "link number was %d", (linkIdx + 1));
	
		/* estimate cyclic flow count from historical flow */
		det_count_from_history = timers.cycle_time_timer[linkIdx]/10*average_flow/360;

		/* If in warmup, just initialise SMFLOW with the value */
		if (is_in_warmup())
		{
			lanes.smoothed_flow[laneIdx] = average_flow;
		}
		else
		{
			update_lane_smoothed_flow(laneIdx, average_flow, det_count_from_history);
		}
	}
	else
	{
		p_divide_error((int)( timers.cycle_time_timer[linkIdx]/10), 208, "link number was %d", linkIdx+1);

		/* flow rate is count/cycle in sec * 360 for veh/hr/10 */
		average_flow = detCount*360/( timers.cycle_time_timer[linkIdx]/10);

		if (average_flow <= 240)
		{
			update_lane_smoothed_flow(laneIdx, average_flow, detCount);
		}
		else
		{
			/* f is unreasonable - use historic flow instead */
			average_flow = get_lane_current_average_flow(laneIdx);
			/* estimate cyclic flow count from historical flow */
			det_count_from_history = timers.cycle_time_timer[linkIdx]/10*average_flow/360;

			/* If in warmup, just initialise SMFLOW with the value */
			if (is_in_warmup())
			{
				lanes.smoothed_flow[laneIdx] = average_flow;
			}
			else
			{
				update_lane_smoothed_flow(laneIdx, average_flow, det_count_from_history);
			}
		}
	}

}


/*!
*	\brief	After doing the smoothed flow calculation, this function is called to update its
*			value in the Lanes::smoothed_flow variable.
*
*	\param[in]  laneIdx		The index of the lane to update its smoothed flow.
*	\param[in]  averageFlow	The pre-calculated average flow for this lane.
*	\param[in]  detCount	The detectors count of this lane to increment its Lanes::total_flow_veh variable.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static void update_lane_smoothed_flow(uint8 laneIdx, int32 averageFlow, int32 detCount)
{
	int32 a;

	/* restrict flow to be between 0.5 to 1.5 times previous SMFLOW,
		unless flow is small */
	a = (lanes.smoothed_flow[laneIdx] + 1) / 2;

	if (averageFlow < a && a > 3)
		averageFlow = a;
	
	a += lanes.smoothed_flow[laneIdx];

	if (a < 7)
		a = 7;

	if (averageFlow > a)
		averageFlow = a;

	lanes.smoothed_flow[laneIdx] = (2*lanes.smoothed_flow[laneIdx]+averageFlow+2)/3;

	lanes.total_flow_veh[laneIdx] += detCount;
}



/*!
*	\brief	Updates all the detector's types cumulative count for a specific lane.
*
*	\param[in]  laneIdx		The index of the lane to update its det cum count.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void update_lane_dets_cum_counts(uint8 laneIdx)
{
	uint8 i; /*dets types loop variable*/

	uint8 det_num; /*d*/

	/* save cumulative counts for appropriate dets on lane */
	for (i = 0; i < MAX_DETS_TYPES; i++) 
	{
		det_num = DI_get_det_num(laneIdx+1, i);
				
		/* Compact MOVA: Ignore -ve IN dets */
		if (det_num <= 0)
			continue;
					
		if (i == ALT_UP_DET || i == ALT_DOWN_DET)
		{
			continue;
		}
		else if (i == IN_SINK_DET)
		{
			lanes.in_sink_cum_det_count[laneIdx] = get_cum_det_count(det_num-1);
			
		}
		else if (i == X_SINK_DET)
		{
			lanes.x_sink_cum_det_count[laneIdx] = get_cum_det_count(det_num-1);
		}
#ifdef M8_ADDITIONAL_SINK_DETS
		else if (i == IN_SINK2_DET)
		{
			lanes.in_sink_2_cum_det_count[laneIdx] = get_cum_det_count(det_num-1);
		}
		else if (i == X_SINK2_DET)
		{
			lanes.x_sink_2_cum_det_count[laneIdx] = get_cum_det_count(det_num-1);
		}
#endif
		else
		{
			set_det_pre_cum_det_count(det_num-1, get_cum_det_count(det_num-1));
		}
	}

	for (i = 1; i <= 2; i++)
	{
		det_num = DI_get_assoc_det_num(laneIdx+1, i);

		if (det_num == 0)
			continue;

		lanes.assoc_cum_det_count[laneIdx][i-1] = get_cum_det_count(det_num-1);
	}
}


/*!
*	\brief	Calculate the lanes var-min-green in a spicific link. It also sets the link var-min-green if one of the 
*			lanes' var-min-green is larger than the link one
*
*	\param[in]	linkIdx			The index of the link to update its lanes var-min-green.
*	\param[out] linkVarMinGreen	The updates link's var-min-green.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void calc_lanes_var_min_green(uint8 linkIdx, int32 * linkVarMinGreen)
{
	uint8 k; /* lanes loop variable*/

	uint8 lane_idx;
	uint8 lane_num;
	int32 var_min_green; /*b*/
	uint8 det_num;
	int32 red_count; /*c*/

	for (k = 0; k<DI_get_link_lanes_count(linkIdx+1); k++)
	{
		lane_num =  DI_get_lane_num(linkIdx+1, k);
		lane_idx = lane_num - 1;

		/* First check for a queue over the IN (ALT-UP) det */
		det_num = DI_get_alt_up_det_num(lane_num);

		if (det_num > 0 && !is_det_suspected(det_num-1) &&
			is_queue_over_det(lane_idx, det_num-1) )
		{
			/* give enhanced min if queue over ALT_UP */
			var_min_green = DI_get_max_min_green_time(lane_num) + DI_get_satinc_time(lane_num);
		}
		else
		{
			/* Getting here means, there is no queue over ALT_UP or it is not defined or 
			   it is suspected */
			det_num = DI_get_x_det_num(lane_num);

			if(is_det_suspected(det_num-1))
			{
				/* if X-DET faulty, check alternatives */

				/* try ALT_DOWN det */
				det_num = DI_get_alt_down_det_num(lane_num);

				if (det_num == 0 || is_det_suspected(det_num-1))
				{
					/* can't use ALT_DOWN det - try IN (ALT-UP) det next */

					det_num = DI_get_alt_up_det_num(lane_num);

					if (det_num == 0 || is_det_suspected(det_num-1))
					{
						/* no usable detectors, go with the link var_min_green time */
						var_min_green = get_link_var_min_green_time(linkIdx);
					}
					else
					{
						/* use red count from IN-DET as alternative to X-DET */
						red_count = lanes.red_count_in_det[lane_idx];
						var_min_green = DI_get_startup_lost_time(lane_num) + red_count * DI_get_satinc_time(lane_num);

						/* limit lane min green to lane MAXMIN */
						if (var_min_green > DI_get_max_min_green_time(lane_num))
							var_min_green = DI_get_max_min_green_time(lane_num);
					}
				}
				else
				{
					/* Absolute min ok if downstream alt-det ok */
					var_min_green = 0;
				}
			}
			else
			{
				/*det_num now holds the X_DET number */
				if (is_queue_over_det(lane_idx, det_num-1))
				{
					/* if queue over X-DET can't use it - set min = MAXMIN */
					var_min_green = DI_get_max_min_green_time(lane_num);
				}
				else 
				{
					/* As there is no queue over the X_DET, use its red count to calculate min green */
					red_count = lanes.red_count_x_det[lane_idx];
					var_min_green = DI_get_startup_lost_time(lane_num) + red_count * DI_get_satinc_time(lane_num);

					/* limit lane min green to lane MAXMIN */
					if (var_min_green > DI_get_max_min_green_time(lane_num))
							var_min_green = DI_get_max_min_green_time(lane_num);
				}
			}
		}

		/* if var_min_green for this lane is > current link var_min_green, update link var_min_green */
		if(var_min_green > *linkVarMinGreen)
			*linkVarMinGreen = var_min_green;
	}
}



/*!
*	\brief	Decides the lane latched demand based on its detectors counts.
*
*	\param[in]	linkIdx		The index of the link that contains the lane.
*	\param[in]  laneIdx		The index of the lane to decide its demand.
*
*	\return		The decided demand, or to skip to the next lane in the inspected link.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
int8 decide_lane_demand(uint8 linkIdx, uint8 laneIdx)
{
	uint8 det_num;
	int32 flow;
	int32 long_off_time;
	uint8 lane_num;

	lane_num = laneIdx+1;

	/* If any red counts on X_DET, set demand */
	if(lanes.red_count_x_det[laneIdx] > 0)
		return LINK_DEMAND_LATCHED;

	/* If any red counts on IN det and no IN-Sink exists, set demand */

#ifdef M8_ADDITIONAL_SINK_DETS
	if(lanes.red_count_in_det[laneIdx] > 0 && (DI_get_in_sink_det_num(lane_num) == 0 && DI_get_in_sink_2_det_num(lane_num) == 0)) {
#else
	if(lanes.red_count_in_det[laneIdx] > 0 && DI_get_in_sink_det_num(lane_num) == 0) {
#endif
		return LINK_DEMAND_LATCHED;
	}

	/* If X-DET is currently ON and not faulty then set demand */
	det_num = DI_get_x_det_num(lane_num);
	if( is_det_on(det_num) && !is_det_suspected(det_num-1))
		return LINK_DEMAND_LATCHED;

	/* Check STOP_LINE_DET if present */
	det_num = DI_get_stop_line_det_num(lane_num);
	if (det_num == 0)
	{
		/* No STOP_LINE_DET, so check X-DET more fully */
		det_num = DI_get_x_det_num(lane_num);
		if ( !is_det_suspected(det_num-1) )
		{
			flow = get_lane_current_average_flow(laneIdx);

			/* Skip long-off check if ave. flow <= 150 veh/hr */
			if (flow <= 15)
				return SKIP_TO_NEXT_LANE;

			long_off_time = 32000/flow;

			if (long_off_time < 1200)
				long_off_time = 1200;

			/* drop through to OUT_DET check if X-DET off a long time,
				otherwise continue to next lane */
			if(get_det_off_time(det_num) < long_off_time)
				return SKIP_TO_NEXT_LANE;
		}

		/* Check OUT-DET*/
		det_num = DI_get_out_det_num(lane_num);
		if (det_num == 0)
		{
			if(lanes.red_count_in_det[laneIdx] > 0)
			{
				if(timers.cycle_time_timer[linkIdx] >= QUICK_NUDGE_PERIOD)
					return LINK_DEMAND_LATCHED;

				return SKIP_TO_NEXT_LANE;
			}

			det_num = DI_get_in_det_num(lane_num);
			if (det_num > 0 && is_det_suspected(det_num-1))
			{
				if (timers.cycle_time_timer[linkIdx] >= QUICK_NUDGE_PERIOD)
					return LINK_DEMAND_LATCHED;

				return SKIP_TO_NEXT_LANE;
			}

			if (timers.cycle_time_timer[linkIdx] >= STANDARD_NUDGE_PERIOD)
				return LINK_DEMAND_LATCHED;
		}
		else /* OUT_DET is defined*/
		{
			/* if OUT_DET is ON, set demand */
			if (is_det_suspected(det_num-1) || is_det_on(det_num))
				return LINK_DEMAND_LATCHED;
		}
	}
	else /* STOP_LINE_DET exists*/
	{
		if (is_det_suspected(det_num-1))
		{
			/* if the stopline det is faulty, insert demand after 30 sec red */
			if (timers.cycle_time_timer[linkIdx] >= QUICK_NUDGE_PERIOD)
				return LINK_DEMAND_LATCHED;
		}
		else
		{
			/* Check if the STOP_LINE_DET is ON*/
			if(is_det_on(det_num))
				return LINK_DEMAND_LATCHED;
		}
	}

	return SKIP_TO_NEXT_LANE;
}


/*!
*	\brief	Decides the lane special (unlatched) demand based on its detectors status, counts and of/off time.
*
*	\param[in]	linkIdx		The index of the link that contains the lane.
*	\param[in]  laneIdx		The index of the lane to decide its demand.
*	\param[in]	minQ		The min veh count between the X-DET and OUT-DET of this lane.
*	\param[out] totalQ		The total queue estimate.
*
*	\return		The decided demand, or to skip to the next lane in the inspected link.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
int8 decide_lane_special_demand(uint8 linkIdx, uint8 laneIdx, int32 minQ, int32 * totalQ)
{
	uint8 det_num;
	uint8 lane_num;

	lane_num = laneIdx + 1;

	/* if lane has no OUT_DET then special demand not possible */
	det_num = DI_get_out_det_num(lane_num);
	if (det_num == 0)
		return SKIP_TO_NEXT_LANE;

	/* Check is OUT-DET suspected */
	if(is_det_suspected(det_num-1))
	{
		/* If OUT-DET suspect, check X-DET */
		det_num = DI_get_x_det_num(lane_num);

		/* If X-DET is also faulty - assume special demand exists */
		if (is_det_suspected(det_num-1))
			return LINK_DEMAND_UNLATCHED;

		/* Set special demand if X-DET red count ok or if ON for >= 3 sec */
		if(lanes.red_count_x_det[laneIdx] >= minQ || get_det_on_time(det_num) >= 30)
			return LINK_DEMAND_UNLATCHED;

		*totalQ += lanes.red_count_x_det[laneIdx];

		return SKIP_TO_NEXT_LANE;
	}

	det_num = DI_get_x_det_num(lane_num);
	if (!is_det_suspected(det_num-1))
	{
		/* If X-DET ok, consider setting demand from long-on X-DET */
		if(DI_get_min_veh_count_between_x_and_out_det(linkIdx+1) < 0 &&
			timers.link_green_timer[linkIdx] < DI_get_moving_q_over_x_det(lane_num))
		{
			/* Skip long-on check until after MOVEQX, if call/cancel specified */
		}
		else
		{
			/* set link special demand if X-det long-on */
			if(is_queue_over_det(laneIdx, det_num-1))
				return LINK_DEMAND_UNLATCHED;
		}

		/* sum queue estimates */
		*totalQ +=  lanes.red_count_x_det[laneIdx];

		/* if MIXOUT < 0 check call/cancel, otherwise continue */
		if(DI_get_min_veh_count_between_x_and_out_det(linkIdx+1) >= 0)
			return SKIP_TO_NEXT_LANE;
	}

	/* Skip call/cancel check until link min green expired */
	if(timers.link_green_timer[linkIdx] < 70)
		return SKIP_TO_NEXT_LANE;

	/* set/maintain call/cancel demand for link */
	/* Using OUT-DET for c/c if X det suspect or MIXOUT<0 */
	det_num = DI_get_out_det_num(lane_num);

#if M8_CALL_CANCEL_TIMES_FOR_IRTGA
	if(get_det_on_time(det_num) >= DI_get_call_time(linkIdx+1))
#else
	if (get_det_on_time(det_num) >= 30)
#endif
		return LINK_DEMAND_CALL_CANCEL;
	
#if M8_CALL_CANCEL_TIMES_FOR_IRTGA
	if(	get_link_demand_marker(linkIdx) == LINK_DEMAND_CALL_CANCEL && get_det_off_time(det_num) < DI_get_cancel_time(linkIdx+1))
#else
	if (get_link_demand_marker(linkIdx) == LINK_DEMAND_CALL_CANCEL && get_det_off_time(det_num) < 30)
#endif
		return LINK_DEMAND_CALL_CANCEL;

	return SKIP_TO_NEXT_LANE;
}



/*!
*	\brief	Checks AVEFLG which counts down every 0.5s. AVEFLG set to 3550 at HR:29/59:50 and 
*			MCOUNT set = -1.  When AVEFLG reaches 0 time checked for next period. When HR:29/59:50 found MCOUNT = 0.
*			Subsequently MCOUNT incremented until all LANES done.	
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void update_lanes_average_flow()
{
	TIMESTAMP_TYPE tTimeDate;
	static int32 pd = -1; /* period number */
	static int32 day = -1;

	uint8 x_det_num;
	uint8 in_det_num;

	aveflg--;

	if (aveflg > 0)
		return;

	if (mcount < 0)
	{
		/* check if time to do updating */
		tTimeDate = Com_read_rtc(READ_RTC);
		aveflg = 1;

		if ((tTimeDate.minutes == 29 || tTimeDate.minutes == 59 ) && 
			tTimeDate.seconds >= 50)
		{
			tTimeDate = Com_read_rtc( READ_RTC);
			day = day_from_date(tTimeDate); 
			pd = (int32)(tTimeDate.hours*2L + (tTimeDate.minutes +1L)/30L); /* period no. */
			mcount = 0;
		}
		return;
	}

	/* do smoothing calc and reset TOTFLO */
	mcount++;

	/* if all lanes updated, reset for next half hour */
	if (mcount > DI_get_lanes_count())
	{
		aveflg = 3550;
		flstrt = 1;
		mcount = -1;
		return;
	}


	
#ifdef M8_USE_OF_ALT_DOWNSTREAM_DETS
	/*
	At this point we need to return if:
	the X-det is faulty and
	there is either no IN-det or the IN-det is also faulty. (It makes no difference whether the stopline det is present, working or specified as alternative downstream or not.)

	If there is a working IN det and it is specified as an alternative UPSTREAM detector then we must NOT return - we must continue to update the historic flows. */
		
	x_det_num = DI_get_x_det_num((uint8)mcount);
	
	in_det_num = DI_get_in_det_num((uint8)mcount);

	if (x_det_num > 0)
	{
		if (is_det_suspected(x_det_num-1) &&
			(in_det_num == 0 || is_det_suspected(in_det_num-1)))
		{
			return;
		}
	}
#endif
	

	/* Only update AVEFLO after first complete half hour, and */
	/* only if half hour count reasonable */
	if (flstrt != 0 && lanes.total_flow_veh[mcount-1] < 1200)
	{
		/* Update AVEFLO if there is a previous value */
		if (lanes.average_flow[day-1][pd-1][mcount-1] > 0)
		{
			lanes.average_flow[day-1][pd-1][mcount-1] = 
				((2*lanes.average_flow[day-1][pd-1][mcount-1] + lanes.total_flow_veh[mcount-1]/5+1)/3);
		}
		else /* otherwise initialise AVEFLO */
		{
			/* ensure AVEFLO will be initialised to >0 */
			if (lanes.total_flow_veh[mcount-1] < 5)
				lanes.total_flow_veh[mcount-1] = 5;

			lanes.average_flow[day-1][pd-1][mcount-1] = (lanes.total_flow_veh[mcount-1]/5);
		}
	}

	/* Finally, reset lane count */
	lanes.total_flow_veh[mcount-1] = 0;
	return;
}


/*!
*	\brief	Calculates the initial value of the average flow based on the current time.
*
*	\return		The calculated average flow value.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static int32 calc_initial_average_flow()
{
	TIMESTAMP_TYPE tTimeDate;
	int32 hr, mn, sec;
	int32 init_aveflg;

	tTimeDate = Com_read_rtc(READ_RTC);
	hr = tTimeDate.hours; 
	mn = tTimeDate.minutes; 
	sec = tTimeDate.seconds; 

	if (mn >= 30)
		mn = mn-30L;
	
	init_aveflg = (int32)(3570L-mn*120L-sec*2L);
	if (init_aveflg < 0)
		init_aveflg = 1;

	return init_aveflg;
}



/*!
*	\brief	Gets the upstream detectors faulty status for a specific lane.
*
*	\param[in]  laneIdx		The index of the lane to get its det faulty status.
*
*	\return		The lane's det faulty status (LANE_DET_FAULT_...).
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static int8 get_upstream_det_fault_status(uint8 laneIdx)
{
	uint8 d,e;

	/* Try to substitute ALT-UP (IN) det */

	/* Check the ALT-UP det exists and is working */
	e = DI_get_alt_up_det_num(laneIdx+1);

	if (e == 0)
		return LANE_DET_FAULT_TOO_MANY_TO_DO_ENDSAT_CHECK;

	if (is_det_suspected(e-1))
		return LANE_DET_FAULT_TOO_MANY_TO_DO_ENDSAT_CHECK;

	/* next check whether there is an OUT det which could corrupt the counts */
	if (DI_get_out_det_num(laneIdx+1) > 0)
		return LANE_DET_FAULT_TOO_MANY_TO_DO_ENDSAT_CHECK;

	/* a faulty IN-SINK or X-SINK could corrupt counts too */
	e = DI_get_in_sink_det_num(laneIdx+1);
	if (e > 0 && is_det_suspected(e-1) )
		return LANE_DET_FAULT_TOO_MANY_TO_DO_ENDSAT_CHECK;

	d = DI_get_x_sink_det_num(laneIdx+1);
	if (d > 0 && is_det_suspected(d-1))
		return LANE_DET_FAULT_TOO_MANY_TO_DO_ENDSAT_CHECK;

#ifdef M8_ADDITIONAL_SINK_DETS
	/* a faulty IN-SINK2 or X-SINK2 could corrupt counts too */
	e = DI_get_in_sink_2_det_num(laneIdx+1);
	if (e > 0 && is_det_suspected(e-1) )
		return LANE_DET_FAULT_TOO_MANY_TO_DO_ENDSAT_CHECK;

	d = DI_get_x_sink_2_det_num(laneIdx+1);
	if (d > 0 && is_det_suspected(d-1))
		return LANE_DET_FAULT_TOO_MANY_TO_DO_ENDSAT_CHECK;
#endif

	return LANE_DET_FAULT_YES_BUT_CAN_DO_ENDSAT_CHECK;
}

/*!
*	\brief	Gets the current average flow for a specific lane based on the historical flows
*			stored in Labes:average_flow and the current time.
*
*	\param[in]  laneIdx		The index of the lane to get its average flow.
*
*	\return		The lane's current average flow.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
int32 get_lane_current_average_flow(uint8 laneIdx)
{
	int32 iPd;
	int32 iFlow;
	int32 nDay;
	
	TIMESTAMP_TYPE tTimeDate;

	tTimeDate = Com_read_rtc(READ_RTC);
	nDay = day_from_date(tTimeDate);

	iPd = (int32)(tTimeDate.hours*2L + (tTimeDate.minutes +1L)/30L);

	iFlow = lanes.average_flow[nDay-1][iPd-1][laneIdx];

	if (iFlow == 0)
		iFlow = 15;

	return iFlow;
}

#ifdef M8_RT_OPTIMISATION
/*!
*	\brief	Decrement the X-DET count during red for this lane.
*
*	\param[in]	laneIdx		The index of the lane.
*	\param[in]	decBy		The counts to be subtracted from current count.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		24-03-2014 */
void dec_lane_red_count_x_det(uint8 laneIdx, int32 decBy)
{
	lanes.red_count_x_det[laneIdx] -= decBy;

	//TOCHECK:
	if(lanes.red_count_x_det[laneIdx] < 1)
		lanes.red_count_x_det[laneIdx] = 1;

}
#endif

#ifdef M8_IMPROVED_LINKING

/*!
*	\brief	The calculated offset is the time taken for the first vehicle leaving the upstream lanes' stop line
*			to reach the back of the queue of the downstream lane.
*			This function implements the calculations listed in M8 requirements document, Section: Improved Linking.
*
*	\param[in]	laneIdx		The index of the lane.
*
*	\return		offset value (+ or -).
*
*	\author		Islam Abdelhalim 
*	\date		19-05-2015 */
int32 calc_lane_time_offset_for_linked_mova(uint8 laneIdx)
{
	uint8 lane_num = laneIdx + 1;

	/*CHECK: We will put 'k' with the REDCX for now, but this need to be more accurate to identify the queue length.*/
	const int32 k = lanes.red_count_x_det[laneIdx];	/*The number of vehicles in the queue.*/

	const int32 l = DI_get_vehicle_length(lane_num);	/*The average vehicle length plus headway whilst queuing.*/
	const int32 a = DI_get_assumed_acceleration_to_reach_cruise_speed();
	const int32 LKDIST = DI_get_link_distance_from_to_stoplines();
	const int32 CSPEED = DI_get_cruise_speed(lane_num);
	const int32 SATINC = DI_get_satinc_time(lane_num) / 10.0; /*divide by 10 to make it in "seconds" (not 10th of sec).*/
	const int32 STLOST = DI_get_startup_lost_time(lane_num) / 10.0; /*divide by 10 to make it in "seconds" (not 10th of sec).*/

	int32 W;		/*The time taken for the start wave to reach the back of the queue.*/
	int32 D;		/*The distance from the back of the downstream queue to the upstream stop line.*/
	int32 d1;		/*The distance required to reach CSPEED.*/
	int32 O;		/*The offset between the stage start times as the upstream and downstream junctions.
					 A negative value indicating that the downstream stop line start before the upstream.*/
	int32 t;		/*The time taken for the veh starting from the upstream stop line to reach the back of the queue.*/

	printf("Calculating the offset for lane %d using RCX = %ld\n", lane_num, k);
	
	/*The following condition should never happens, but it is here for safety.*/
	if(a == 0 || CSPEED == 0) 
		return -1;
	
	/*1- Calculate the W value.*/
	W = ( (k/2.0) * SATINC) +  (STLOST+1); /*Adding '1' because we are using the real STLOST rather than the effective.*/

	/*2- Calculate the d1 value*/
	d1 = (CSPEED * CSPEED) /  (2 * a);

	/*3- Calculate 'D' value.*/
	D = LKDIST - k*l;

	/*'D' must be cupped to zero.*/
	if(D < 0) 
		D = 0;

	/*4- Calculate 't' value*/
	if(D <= d1)
	{
		t = sqrt((float)2 * D);
	}
	else /*(D > d1) */
	{
		t = (CSPEED/a) + ((D-d1)/CSPEED); 
	}

	/*5- Calculate the offset 'O'*/
	O = t - W;

	return O * 10; /*Multiply by 10 to convert the result to 10th of second instead of seconds.*/
}
#endif


/*----------------------------------------------------------------*/
/*------------------------- Getters ------------------------------*/
/*----------------------------------------------------------------*/
int32 get_lane_oversat_counter(uint8 laneIdx) /*laosat*/
{
	return lanes.oversat_counter[laneIdx]; 
}

int32	get_lane_red_count_x_det(uint8 laneIdx)
{
	return lanes.red_count_x_det[laneIdx];
}

int32 get_lane_red_count_in_det(uint8 laneIdx)
{
	return lanes.red_count_in_det[laneIdx]; 
}

int32 * get_lanes_extra_veh_beyond_in_det()
{
	return lanes.extra_veh_beyond_in_det;
}

int32 get_lane_extra_veh_beyond_in_det(uint8 laneIdx)
{
	return lanes.extra_veh_beyond_in_det[laneIdx];
}

int32 get_lane_smoothed_flow(uint8 laneIdx)
{
	return lanes.smoothed_flow[laneIdx];
}

int32 get_in_sink_cum_det_count(uint8 laneIdx) /*inscdc*/
{
	return lanes.in_sink_cum_det_count[laneIdx];
}

int32 get_x_sink_cum_det_count(uint8 laneIdx) /*xskcdc*/
{
	return lanes.x_sink_cum_det_count[laneIdx];
}

#ifdef M8_ADDITIONAL_SINK_DETS
int32 get_in_sink_2_cum_det_count(uint8 laneIdx)
{
	return lanes.in_sink_2_cum_det_count[laneIdx];
}

int32 get_x_sink_2_cum_det_count(uint8 laneIdx)
{
	return lanes.x_sink_2_cum_det_count[laneIdx];
}
#endif

int32 get_assoc_cum_det_count(uint8 laneIdx, uint8 assocDetIdx)
{
	return lanes.assoc_cum_det_count[laneIdx][assocDetIdx];
}

int8 get_lane_endsat_marker(uint8 laneIdx) /*laensa*/
{
	return lanes.endsat_marker[laneIdx];
}

/* used in one TMA and logging function only */
int8 * get_lanes_endsat_marker() /*laensa[]*/
{
	return lanes.endsat_marker;
}

int32 get_lane_oversat_init_count(uint8 laneIdx) /*osatic*/
{
	return lanes.oversat_init_count[laneIdx];
}

int32 get_lane_det_fault_marker(uint8 laneIdx)
{
	return lanes.det_fault_marker[laneIdx]; /* LADETF */
}



/*----------------------------------------------------------------*/
/*------------------------- Restters -----------------------------*/
/*----------------------------------------------------------------*/
void reset_lane_counts_and_markers(uint8 laneIdx)
{
	int32 i; /* shift register loop variable */

	int32 a;
	int32 b;
	int32 c;

	uint8 in_det_num;
				
	a = DI_get_gamber_time(laneIdx+1);

	/* count to exit of register if OUT det exists */ 
	if (DI_get_out_det_num(laneIdx+1) > 0)
		a = 1;	

	b = DI_get_x_det_cruise_time(laneIdx+1);
	
	lanes.red_count_x_det[laneIdx] = 0;
	lanes.exit_red_count[laneIdx] = 0;
	lanes.sink_red_count[laneIdx] = 0;
	lanes.extra_veh_beyond_in_det[laneIdx] = 0;
			
	/* skip if GAMBER > CRUSX */
	if (a <= b)
	{
		for (i = a-1; i < b; i++) 
		{
#ifndef M8_IMPORVED_BUS_PRIORITY
			lanes.red_count_x_det[laneIdx] += lanes.shift_register[laneIdx][i];
#else
			if (lanes.shift_register[laneIdx][i] == 1)
			{
				lanes.red_count_x_det[laneIdx] += lanes.shift_register[laneIdx][i];
			}
#endif
		}
	}

	lanes.oversat_init_count[laneIdx] = lanes.red_count_x_det[laneIdx];

	/* Ignore lanes with no (or negative) IN dets */
	if(DI_get_in_det_num(laneIdx+1) > 0)
	{
		c = DI_get_in_det_cruise_time(laneIdx+1);
		lanes.red_count_in_det[laneIdx] = lanes.red_count_x_det[laneIdx];
				
		/* set oversat count from IN det unless X det specified  */
		b++;
		for (i = b-1; i < c; i++)
		{
#ifndef M8_IMPORVED_BUS_PRIORITY
			lanes.red_count_in_det[laneIdx] += lanes.shift_register[laneIdx][i];
#else
			if (lanes.shift_register[laneIdx][i] == 1)
			{
				lanes.red_count_in_det[laneIdx] += lanes.shift_register[laneIdx][i];
			}
#endif
		}

		
		 /* check if the IN-DET to be used in the oversat checking (not a Compact MOVA lane)*/
		if (DI_get_oversat_det(laneIdx+1) == OVERSAT_DET_IN_DET)
		{
#ifdef M8_USE_OF_CMOVA_OVERSAT_CHECKING
			in_det_num = DI_get_in_det_num(laneIdx+1);
			if(!is_det_suspected(in_det_num-1))
#endif
			{
				lanes.oversat_init_count[laneIdx] = lanes.red_count_in_det[laneIdx];
			}
		}
	}

#ifdef M8_IMPORVED_BUS_PRIORITY

	lanes.bus_weighted_red_count[laneIdx] = 0;

	for (i = a-1; i < REG_SIZE; i++)
	{
		if (lanes.shift_register[laneIdx][i] > 1)
		{
			lanes.bus_weighted_red_count[laneIdx] += lanes.shift_register[laneIdx][i];
		}
	}
#endif


	/* Now reset the markers*/
	lanes.det_fault_marker[laneIdx] = LANE_DET_FAULT_NO_PROBLEM;
	lanes.endsat_marker[laneIdx] = LANE_ENDSAT_NOT_YET;
}


void reset_lanes_endsat_marker(uint8 linkIdx)
{
	uint8 k; /* lanes loop variable*/

	uint8 lane_idx;

	for (k = 0; k<DI_get_link_lanes_count(linkIdx+1); k++)
	{
		lane_idx = DI_get_lane_num(linkIdx+1, k) - 1;

		lanes.endsat_marker[lane_idx] = LANE_ENDSAT_NOT_YET;
	}
}




/*----------------------------------------------------------------*/
/*------------------------- Setters ------------------------------*/
/*----------------------------------------------------------------*/
void set_lane_det_fault_status(uint8 linkIdx, uint8 laneIdx)
{
	lanes.det_fault_marker[laneIdx] = decide_lane_det_fault_status(linkIdx, laneIdx);
}