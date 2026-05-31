/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         messages_manager.c
*
*  TITLE:        Mova Kernel: Messages Manager
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
#include "messages_manager.h"
#include "dataset_interface.h"
#include "timers_manager.h"
#include "delay_and_stops_optimiser.h"
#include "estimate_right_turners_count_during_opposed_stage.h"
#include "core_interface.h"

#include "obclock.h"
#include "gbltypes.h"

#if defined (TRL_INTEGRATION_TEST)
#include "display_msg.h"
#include "generalfunc.h"
#endif

static struct Messages msgs;

static struct Junction * p_junction;
static struct Stages * p_stages;
static struct Links * p_links;
static struct Lanes * p_lanes;
static struct Detectors * p_dets;


static int32 ben_flow_rate[MAX_LINKS];
static int32 future_red[MAX_LINKS];
static int32 net_ben_flow_rate[MAX_LINKS];

static void set_msg_data(uint8 fieldIdx, int32 data);
static void wrap_msgs();

static void message_5_common(uint8 idx);

extern TIMESTAMP_TYPE Com_read_rtc(int, ... );


void init_messages_manager()
{
	CI_set_msgs_pointer(&msgs);
}

void MSG_set_junction_pointer(struct Junction * junRef)
{
	p_junction = junRef;
}

void MSG_set_stages_pointer(struct Stages * stagesRef)
{
	p_stages = stagesRef;
}

void MSG_set_links_pointer(struct Links * linksRef)
{
	p_links = linksRef;
}

void MSG_set_lanes_pointer(struct Lanes * lanesRef)
{
	p_lanes = lanesRef;
}

void MSG_set_dets_pointer(struct Detectors * detsRef)
{
	p_dets = detsRef;
}

static void set_msg_data(uint8 fieldIdx, int32 data)
{
	/*First, reset the the last field of the message line which acts as a IsRead flag.*/
	msgs.msgs_data[msgs.current_msg_num - 1][MSG_MAX_LENGTH - 1] = 0;

	msgs.msgs_data[msgs.current_msg_num-1][fieldIdx] = data;
}


/*!
*	\brief	Wraps the stored messages in Messages::msgs_data by limiting their 
*			count to MAX_MSGS.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static void wrap_msgs()
{
	msgs.current_msg_num++;

	if (msgs.current_msg_num > MAX_MSGS)
		msgs.current_msg_num = 1;
}

/*!
*	\brief	Preparing the first line of the message that appears at the 
*			start of each stage green.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void message_1_1()
{
	/* at start of green output time and stage */
	TIMESTAMP_TYPE tTimeDate;
	uint8 i = 0;
	uint8 m;

	tTimeDate = Com_read_rtc( READ_RTC);

	set_msg_data(i++, 1);
	set_msg_data(i++, 1);
	set_msg_data(i++, p_junction->current_stage);

	set_msg_data(i++, (int32)tTimeDate.hours); 
	set_msg_data(i++, (int32)tTimeDate.minutes);
	set_msg_data(i++, (int32)tTimeDate.seconds);
	
	for (m = 0; m < DI_get_lanes_count(); m++)
		set_msg_data(m+i, p_lanes->smoothed_flow[m]);		

	i += DI_get_lanes_count();

	set_msg_data(i++, p_junction->total_smoothed_flow);
	
	for (m = 0; m < DI_get_lanes_count(); m++)
		set_msg_data(m+i, p_lanes->oversat_counter[m]);

	i += DI_get_lanes_count();

	set_msg_data(i++, p_junction->max_lane_oversat_counter);
	set_msg_data(i++, p_stages->lambda[p_junction->current_stage-1]);
	set_msg_data(i++, p_junction->total_lambda);
	set_msg_data(i, p_junction->cut_max_times_marker);

#if defined (TRL_INTEGRATION_TEST)
	printStrgM11(&msgs.msgs_data[0], msgs.current_msg_num-1, DI_get_lanes_count(), SYS_LOG_PRINTING);
#endif

	wrap_msgs();
}

/*!
*	\brief	Preparing the second line of the message that appears at the 
*			start of each stage green.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void message_1_2()
{
	uint8 i = 0;
	uint8 m;

#ifdef M8_RT_OPTIMISATION
	uint8	veh_count_turned_during_intergreen;
	uint8	veh_count_turned_during_opposing_stage;
#endif

	set_msg_data(i++, 1);
	set_msg_data(i++, 2);
	set_msg_data(i++, (p_junction->smoothed_cycle_time + 5)/10);
	
	/* output drift max (or low drift max when short cycling) */
	for (m = 0; m < DI_get_stages_count(); m++)
	{
		if(p_junction->cut_max_times_marker > 0) 
			set_msg_data(m+i, p_stages->low_drift_max[m]/10);
		else
			set_msg_data(m+i, p_stages->drift_max[m]/10);
	}
	i += DI_get_stages_count();

	/*suspect detectors*/
	for (m = 0; m < DI_get_dets_count(); m++)
		set_msg_data(m+i, p_dets->suspect_status[m]);
	

#ifdef M8_RT_OPTIMISATION
	/*handle the RT info*/
	ALG_rt_get_internel_data_for_message(&veh_count_turned_during_intergreen, &veh_count_turned_during_opposing_stage);

	set_msg_data(i+m, veh_count_turned_during_intergreen);
	set_msg_data(i+m+1, veh_count_turned_during_opposing_stage);

	ALG_rt_reset_internel_data_for_message();
#endif

#if defined (TRL_INTEGRATION_TEST)
	printStrgM12(&msgs.msgs_data[0], msgs.current_msg_num-1, DI_get_stages_count(), DI_get_dets_count(), SYS_LOG_PRINTING);	
#endif

	wrap_msgs();
}


/*!
*	\brief	Preparing the message that appears at the 
*			stage minimum-green time.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */

void message_2()
{
	/* at end of stage min-min-green, output details for varimin */
	uint8 i = 0;
	uint8 m;

	set_msg_data(i++, 2);
	set_msg_data(i++, 48);
	set_msg_data(i++, timers.signal_state_timer);
	set_msg_data(i++, (p_junction->var_min_green_time+5)/10);

	for (m = 0; m < DI_get_links_count(); m++) 
		set_msg_data(m+i, (p_links->var_min_green[m]+5)/10);
	i += DI_get_links_count();
	
	for (m = 0; m < DI_get_lanes_count(); m++) 
		set_msg_data(m+i, p_lanes->red_count_x_det[m]);


#if defined (TRL_INTEGRATION_TEST)
	printStrgM20(&msgs.msgs_data[0], msgs.current_msg_num-1, DI_get_links_count(), DI_get_lanes_count(), SYS_LOG_PRINTING);
#endif

	wrap_msgs();
}

/*!
*	\brief	Preparing the first line of the message that appears while 
*			searching for endsat.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void message_3_1()
{
	/* output data on gap checks every 2 sec at end sat */
	uint8 i;
	uint8 l, m, n;

	i = 0;
	set_msg_data(i++, 3);
	set_msg_data(i++, 0);
	set_msg_data(i++, timers.signal_state_timer);
	set_msg_data(i++, p_junction->next_stage);
	
	for (m=0; m < DI_get_links_count(); m++)
		set_msg_data(m+i, p_links->endsat_marker[m]);
	i += DI_get_links_count();

	for (l=0; l < DI_get_links_count(); l++) 
	{
		/* Mess. for links that will benefit from proposed stage-change */
		if (p_links->benefiting_marker[l] == BENEFIT_RELEVENT_LINK)
		{
			int d,e;

			for (n=0; n < DI_get_link_lanes_count(l+1); n++) 
			{
				m = DI_get_lane_num(l+1, n);

				set_msg_data(i++, m);

				d = DI_get_x_det_num(m);
				e = DI_get_comb_x_det_num(m);

				if (e > 0 && p_dets->suspect_status[e-1] == DET_FAULT_STATUS_OK && 
					timers.link_green_timer[l] >= DI_get_time_to_use_comb_x(m))
				d = e;

				set_msg_data(i++, p_dets->old_gap[d-1]);
				set_msg_data(i++, p_dets->time_off[d-1]);
				set_msg_data(i++, timers.link_waste_green_timer[l]/10);
			}
		}
	}

	set_msg_data(1, i);

#ifdef M8_IMPROVED_LINKING /* Output the offset for the improved linking*/
	set_msg_data(i, p_junction->linking_time_offset - DI_get_upstream_junction_intergreen());
	set_msg_data(i+1, p_lanes->red_count_x_det[1]);
#endif

#if defined (TRL_INTEGRATION_TEST)
	printStrgM31(&msgs.msgs_data[0], msgs.current_msg_num-1, DI_get_links_count(), DI_get_lanes_count(), msgs.msgs_data[msgs.current_msg_num-1][1], SYS_LOG_PRINTING);
#endif

	wrap_msgs();
}



/*!
*	\brief	Preparing the second line of the message that appears while 
*			searching for endsat.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void message_3_2()
{
#if defined (CMOVA_EP)
	/* output info on e/p holds/extensions every 2 sec */
	uint8 i;
	uint8 m;

	/* only output every 2 sec */
	if ((timers.signal_state_timer % 20) != 0)
		return;
	
	i = 0;
	set_msg_data(i++, 3);
	set_msg_data(i++, 2);
	set_msg_data(i++, timers.signal_state_timer);
	set_msg_data(i++, p_junction->next_stage);
	set_msg_data(i++, p_junction->ep_hold_marker);
	set_msg_data(i++, p_junction->ep_ext_marker);
	set_msg_data(i++, timers.ep_ext_max_timer/10);

	/* ep_link_ext_timer (EPLXTM) */
	for (m=0; m < DI_get_links_count(); m++) 
		set_msg_data(m+i, timers.ep_link_ext_timer[m]/10);

	i += DI_get_links_count();

#ifndef M8_REMOVE_REV_DEMAND 
	/* rev_demand_marker (REVDEM) */
	for (m=0; m < DI_get_links_count(); m++) 
		set_msg_data(m+i, p_links->rev_demand_marker[m]);
#endif

#ifdef M8_IMPROVED_LINKING /* Output the offset for the improved linking*/
	i += DI_get_links_count();
	
	set_msg_data(i, p_junction->linking_time_offset - DI_get_upstream_junction_intergreen());	
	set_msg_data(i+1, p_lanes->red_count_x_det[1]);
#endif

#if defined (TRL_INTEGRATION_TEST)
	printStrgM32(&msgs.msgs_data[0], msgs.current_msg_num-1, DI_get_links_count(), SYS_LOG_PRINTING);
#endif

	wrap_msgs();

#endif /* CMOVA_EP */ 
}



/*!
*	\brief	Preparing the message that outputs optimisation 
*			status every 2 sec.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void message_4()
{
	uint8 l;
	uint8 i;

	int32 benfit;
	int32 disbenfit;
	int32 predictedRedTime;
	int32 cycleDelta;
	

	/* if not on a 2 sec boundary just return */
	if ((timers.signal_state_timer % 20) != 0)
		return;



	ALG_ds_get_internel_data_for_message( &benfit,
										&disbenfit,
										&predictedRedTime,
										&cycleDelta,
										ben_flow_rate,
										future_red,
										net_ben_flow_rate);

	set_msg_data(0, 4);
	set_msg_data(2, timers.signal_state_timer);
	set_msg_data(3, p_junction->next_stage);
	set_msg_data(4, benfit);
	set_msg_data(5, disbenfit);

	i = 7;
	predictedRedTime = predictedRedTime/10;
	set_msg_data(6, predictedRedTime);

	for (l = 1; l <= DI_get_links_count(); l++) 
	{
		/* skip irrelevant links (includes all ped links) */
		if(p_links->benefiting_marker[l-1] != 1)
			continue;

		set_msg_data(i, l);
		set_msg_data(i+1, net_ben_flow_rate[l-1]);
		set_msg_data(i+2, ben_flow_rate[l-1]);
		set_msg_data(i+3, future_red[l-1]/10);
		set_msg_data(i+4, p_links->extra_green_time[l-1]/10);

		if (net_ben_flow_rate[l-1] < -99)
			set_msg_data(i+1, -99);
		
		i += 5;
		
		/* if buffer full skip remaining links */
		if (i >= 33)
			break;
	}

#ifdef M8_IMPROVED_LINKINGD /* Output the offset for the improved linking*/
	set_msg_data(i, p_junction->linking_time_offset - DI_get_upstream_junction_intergreen());	
	set_msg_data(i+1, p_lanes->red_count_x_det[1]);
#endif

#if defined (TRL_INTEGRATION_TEST)
	printStrgM4( &msgs.msgs_data[0],  msgs.current_msg_num-1, ((i - 7) / 5), SYS_LOG_PRINTING);
#endif

	set_msg_data(1, i);

	wrap_msgs();
}


/*!
*	\brief	Preparing the first line of the message that appears
*			for delay-and-stops optimiser change.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void message_5_1()
{
	int32 benfit;
	int32 disbenfit;
	int32 predictedRedTime;
	int32 cycleDelta;

	ALG_ds_get_internel_data_for_message( &benfit,
										&disbenfit,
										&predictedRedTime,
										&cycleDelta,
										ben_flow_rate,
										future_red,
										net_ben_flow_rate);

	/* optimiser change message */
	set_msg_data(1, 1);
	set_msg_data(7, benfit);
	set_msg_data(8, disbenfit);
	set_msg_data(9, predictedRedTime/10);
	set_msg_data(10, cycleDelta);
	message_5_common(11);

#if defined (TRL_INTEGRATION_TEST)
	printStrgM51(&msgs.msgs_data[0], msgs.current_msg_num-1, DI_get_links_count(), DI_get_lanes_count(), SYS_LOG_PRINTING);
#endif

	wrap_msgs();
}


/*!
*	\brief	Preparing the second line of the message that appears
*			for deal-and-stops optimiser change.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void message_5_2()
{
	/* MAX change message */
	int32 d;

	set_msg_data(1, 2);

	d = p_stages->drift_max[p_junction->current_stage-1];

	if (p_junction->cut_max_times_marker > 0)
		d = p_stages->low_drift_max[p_junction->current_stage-1];

	d += p_junction->extra_green_for_drift_max;
	
	if (p_junction->ped_demand_marker > 0)
		d = p_junction->ped_max_green_time;

	set_msg_data(7, d);
	message_5_common(8);

#if defined (TRL_INTEGRATION_TEST)
	printStrgM52(&msgs.msgs_data[0], msgs.current_msg_num-1, p_junction->ped_demand_marker, DI_get_links_count(), DI_get_lanes_count(), SYS_LOG_PRINTING);
#endif

	wrap_msgs();
}

/*!
*	\brief	Preparing the third line of the message that appears
*			for delay-and-stops optimiser change.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void message_5_3()
{
	/* oversaturation change message */
	uint8 linkNo;
	uint8 i;
    
	set_msg_data(1, 3);
	i = 7;
    
	for (linkNo = 0; linkNo<DI_get_links_count(); linkNo++)
		set_msg_data(i+linkNo, p_links->max_percent_sat[linkNo]);

	i += DI_get_links_count();
	message_5_common(i);

#if defined (TRL_INTEGRATION_TEST)
	printStrgM53(&msgs.msgs_data[0], msgs.current_msg_num-1, DI_get_links_count(), DI_get_lanes_count(), SYS_LOG_PRINTING);
#endif

	wrap_msgs();
}

/*!
*	\brief	Preparing the fourth line of the message that appears
*			for deal-and-stops optimiser change.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void message_5_4()
{
#if defined (CMOVA_EP)
	/* e/p stage truncation change message */
	set_msg_data(1, 4);
	set_msg_data(7, p_junction->next_emeregency_stage_to_demand);
	set_msg_data(8, p_junction->next_priority_stage_to_demand);
	message_5_common(9);

#if defined (TRL_INTEGRATION_TEST)
	printStrgM54(&msgs.msgs_data[0], msgs.current_msg_num-1, DI_get_links_count(), DI_get_lanes_count(), SYS_LOG_PRINTING);	
#endif
	wrap_msgs();
#endif
}


static void message_5_common(uint8 idx)
{
	/* Common output for all end-stage (type 5) messages */

	/* Parameter idx is the index in imess to begin writing - the
	   individual types write a variable abount of data items. */

	TIMESTAMP_TYPE tTimeDate;
	uint8 j,m;

	tTimeDate = Com_read_rtc(READ_RTC);

	set_msg_data(0, 5);
	set_msg_data(2, timers.signal_state_timer);
	set_msg_data(3, p_junction->next_stage);
	set_msg_data(4, (int32)tTimeDate.hours);
	set_msg_data(5, (int32)tTimeDate.minutes);
	set_msg_data(6, (int32)tTimeDate.seconds);

	for (m = 0; m < DI_get_links_count(); m++)
	{
#ifndef M8_REMOVE_REV_DEMAND
		if ((p_links->demand_marker[m] == 0) && (p_links->rev_demand_marker[m] != 0)) 
			set_msg_data(idx + m, 9);
		else
			set_msg_data(idx + m, p_links->demand_marker[m]);
#else
		set_msg_data(idx + m, p_links->demand_marker[m]);
#endif
	}
	m = idx + DI_get_links_count();
	
	for (j = 0; j < DI_get_lanes_count(); j++)
		set_msg_data(m+j, p_lanes->red_count_x_det[j]);

	set_msg_data(m+j, p_junction->extra_green_for_drift_max/10);
}


/*!
*	\brief	Preparing the first line of the message that appears
*			at the start of intergreen after normal change.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void message_6_1()
{
	TIMESTAMP_TYPE tTimeDate;
	uint8 i = 0;
	uint8 n;

	tTimeDate = Com_read_rtc(READ_RTC);
	set_msg_data(i++, 6);
	set_msg_data(i++, 1);
	set_msg_data(i++, (int)tTimeDate.seconds);

	for (n = 0; n < DI_get_stages_count(); n++)
		set_msg_data(i+n, p_stages->demand_marker[n]);
	i += DI_get_stages_count();
    
	for (n = 0; n < DI_get_links_count(); n++)
		set_msg_data(i+n, (p_links->total_bonus_green_time[n]+5)/10);
	i += DI_get_links_count();

	for (n = 0; n < DI_get_lanes_count(); n++) 
		set_msg_data(i+n, p_lanes->red_count_in_det[n]);

#if defined (TRL_INTEGRATION_TEST)
	printStrgM61(&msgs.msgs_data[0], msgs.current_msg_num-1, DI_get_stages_count(), DI_get_links_count(), DI_get_lanes_count(), SYS_LOG_PRINTING);
#endif

	wrap_msgs();
}


/*!
*	\brief	Preparing the second line of the message that appears
*			at the start of intergreen after emergency/priority change.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void message_6_2()
{
#if defined (CMOVA_EP)
	TIMESTAMP_TYPE tTimeDate;
	uint8 i = 0;
	uint8 n;
	int8 d;

	tTimeDate = Com_read_rtc(READ_RTC);
	set_msg_data(i++, 6);
	set_msg_data(i++, 2);
	set_msg_data(i++, (int)tTimeDate.seconds);

	for (n = 0; n < DI_get_stages_count(); n++)
	{
		d = p_stages->demand_marker[n];
		
		if (p_stages->priority_demand_marker[n] > 0)
			d = p_stages->priority_demand_marker[n];

		if (p_stages->emergency_demand_marker[n] > 0)
			d = p_stages->emergency_demand_marker[n];

		set_msg_data(i+n, d);
	}
	i += DI_get_stages_count();

	for (n = 0; n < DI_get_links_count(); n++) 
		set_msg_data(i+n, p_links->ep_trunc_indicator[n]);

	i += DI_get_links_count();

	for (n = 0; n < DI_get_links_count(); n++)
		set_msg_data(i+n, p_links->ep_skip_indicator[n]);


#if defined (TRL_INTEGRATION_TEST)
	printStrgM62(&msgs.msgs_data[0], msgs.current_msg_num-1, DI_get_stages_count(), DI_get_links_count(), SYS_LOG_PRINTING);
#endif
	
	wrap_msgs();

#endif
}