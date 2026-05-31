/*******************************************************************************
*
*                      MOVA DYNAMIC DATA
*
*  FILE:         dynamic_data.h
*
*  TITLE:        Mova Kernel: Dynamic Data
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


#if !defined (DYNAMIC_DATA_H)
#define DYNAMIC_DATA_H

#include "mova_constants.h"
#include "mova_types.h"

/**
The Program struct holds all the variables that define the current status of the program.
*/
struct Program
{
	/** The current program marker. Holds one of the values defined in mova_constants.h (PROG_MARKER_...).
		Previous name: MARKER */
	int8 marker;
	
	/** The previous program marker. Holds one of the values defined in mova_constants.h (PROG_MARKER_...).
		Previous name: PREMRK */
	int8 previous_marker;

	/** A marker that is used during the ep situations only in conjunction with 'marker'.
		Holds one of the values defined in mova_constants.h (PROG_MARKER_...).
		Previous name: TEMPMK */	
	int8 ep_marker;
};

/**
The Junction struct holds all the variables related to the junction or the current stage.
*/
struct Junction
{
	/** The currently running stage number. Holds 0 for the intergreen, or from 1 to MAX_STAGES during green.
		Previous name: CUSIG */
	uint8	current_stage;

	/** The previously running stage number. Holds 0 for the intergreen, or from 1 to MAX_STAGES during green.
		Previous name: PRESIG */
	uint8	previous_stage;

	/** The last stage number. Holds 0 for the intergreen, or from 1 to MAX_STAGES during green.
		Previous name: LASTAG */
	uint8	last_stage;

	/** The next stage number to be activated. Holds 0 for the intergreen, or from 1 to MAX_STAGES during green.
		Previous name: SNEXT */
	uint8	next_stage;

	/** The previous next stage number. Holds 0 for the intergreen, or from 1 to MAX_STAGES during green.
		Previous name: PRESNX */
	uint8	previous_next_stage;

	/** The stage number to be demanded. Holds 0 for the intergreen, or from 1 to MAX_STAGES during green.
		Previous name: DEMSTA */
	uint8	demanded_stage;

	/** Marker to control initial take over procedure (warmup period). System monitors signals for one cycle before take over.
		Set to 1 initially in data. Set to 0 at startup. Incremented at each start green during warmup cycle.
		Maximum value = STAGES+1.
		Previous name: WARMUP */
	int32	warmup_counter;

	/** Counter that holds the total number of consecutive over-sat cycles summed for all lanes.
		Previous name: JUNCOS*/
	int32	oversat_counter;

	/** Flag to note if the oversat has been changed for any lane in the junction.
		Previous name: aa (in BlockP)*/
	mv_bool	is_oversat_changed;

	/** Flag to note if the oversat has been increased for any lane in the junction.
		Previous name: bb (in BlockP)*/
	mv_bool	is_oversat_increased;

	/** Maximum of lane oversaturation counters (Lanes::oversat_counter). Used to omit optimiser if  substantial oversaturation,
	and to O/P at start green. It is used also to control the size of critical gaps when deciding end-sat.
	Previous name: LAOSMX */
	int32	max_lane_oversat_counter;

	/** Low total green (equivalent to TOTALG, but when short-cycle-control is enabled).
		Previous name: LOTOTG */
	int32	low_total_green;

	/** The variable min green time for the current stage in the junction (0.1sec).
		Previous name: STAMIN */
	int32	var_min_green_time;

	/** The smoothed cycle time for the junction. It is updated every end-stage time 
		(exponentially smoothed).
		Previous name: SMCYCL*/
	int32	smoothed_cycle_time;

	/** The total lambda value (proportion of cycle which is green) (percent).
		Previous name: TOTLAM */
	int32	total_lambda;

	/** The junction's smoothed flow (veh/h/10).
		Previous name: JUNSMF */
	int32	total_smoothed_flow;

	/** The  extra  amount  of green (in 0.1 sec units) to be added to the stage drifting max 
		to give the overall max green for the current stage, to control the limit cycle.
		Previous name: MXPLUS */
	int32	extra_green_for_drift_max;

	/*-------------- Bonus Variables --------------*/

	/** Marker to cause use of reduced max green times (LOWMAX) or low drifting maximum greens  
		(Stages::low_drift_max) due to bonus effect being sufficiently large to require short cycle.
		Link with bonus must have BONCUT set in data to cause this.
		Previous name: CUTMAX */
	int8	cut_max_times_marker;


	/*-------------- Ped Variables ----------------*/

	/** Marker to hold the ped demands within the junction.
		Previous name: RAV1 */
	int8	ped_demand_marker;

	/** Flag to note ped demand presence within the function.
		Previous name: lPedDemand */
	mv_bool	is_ped_demand_present;

	/** Max green time for ped.
		Previous name: nPedMax */
	int32	ped_max_green_time;


	/*--------------- E/P Variables ---------------*/

	/** E/P marker that defines the 'hold' status. Holds one of the values defined in mova_constants.h (..._HOLD_PRESENT). 
		Previous name: EPHOLD */
	int8	ep_hold_marker;

	/** E/P marker that defines the 'extension' status. Holds one of the values defined in mova_constants.h (..._EXT_PRESENT). 
		Previous name: EPX */
	int8	ep_ext_marker;

	/** Emergency next stage number to demand.
		Previous name: ESNEXT */
	uint8	next_emeregency_stage_to_demand;

	/** Priority next stage number to demand.
		Previous name: PSNEXT*/
	uint8	next_priority_stage_to_demand;

	/** Flag to note if the current stage to be truncated or not.
		Previous name: TRUNC */
	mv_bool	is_to_truncate_current_stage;

#ifdef M8_IMPORVED_BUS_PRIORITY
	/** Flag to note priority demand presence.
		Used to implement the Improved Bus Priority in M8.0*/
	mv_bool	is_priority_demand_present;
#endif

#ifdef 	M8_RT_OPTIMISATION
	/* ------------- Opposing lanes vars ------------------*/
	/* NOTE: the following vars are for the group of opposing lanes. However, they
			 are included in the Junction struct because only one opposed link can be
			 active at a time (i.e., only one RTIA is active in the junction). */
	
	/** Stores the gaps timings for the group of lanes during the GREEN
		of the opposing lane. Used only in the algorithm: 
		Estimate Right Turners Count During Opposed Stage.*/
	int32	net_gaps[MAX_GAPS];

	/** Counts the gaps over the X-DETs of the opposing lanes.*/
	uint8	net_gaps_count;
#endif

#ifdef	M8_IMPROVED_LINKING
	/* ------------- Linked MOVA vars --------------------*/

	/** This flag is set to TRUE at the beginning of stages to indicate that it is allowed to do 
		backward linking. Once the backward linking is applied, this flag is set to FALSE.*/
	mv_bool	is_to_do_backward_linking;

	/** This flag indicates if the controller (downstream) needs to send a backward 'hold'
		signal to the upstream controller or not. Sending the signal will be by simply setting
		control[13] to ON.*/
	mv_bool	is_to_send_backward_hold_signal;

	/** Store the upper limit value of the Hold timer.*/
	int32 backward_hold_timer_limit;

	/** This flag indicates if the controller (downstream) needs to send a backward 'release'
		signal to the upstream controller or not. Sending the signal will be by simply setting
		control[14] to ON.*/
	mv_bool	is_to_send_backward_release_signal;

	/** Stores the delay time span that the downstream controller should wait before change to stage 1. 
		Used in the forward linking when 4TT SDCODE is used to define the EP link and the EP stage.*/
	int32 forward_link_delay;

	/* Forward/Backward linking time offset.*/
	int32 linking_time_offset;
#endif

}; /*Junction*/


/**
The Stages struct holds all the variables related to the junction's stages. */
struct Stages
{
	/** Stage  oversaturation  weighting  factor; given by sum of lane_weighting_factor's (LANEWF's)
		for oversaturated lanes green in stage.
		Previous name: STAGOS */
	int32	oversat_weighting_factor[MAX_STAGES];

	/** Maximum of lane oversaturation markers (Lanes::oversat_counter's) for any lane green in this stage.
		Previous name: SAOSMX */
	int32	oversat_max_counter[MAX_STAGES];

	/** Drifting maximum green for stage (in 0.1 sec units) when operating to  the  limit  cycle, 
		MOVA first allocates 75% of the total green between stages in  proportion to LAMBDA values:
		this gives the Stages::drift_max value; then, a further amount of Junction::extra_green_for_drift_max  
		is added to the maximum depending on merit of running green.
		Previous name: DRIFMX */
	int32	drift_max[MAX_STAGES];


	/** Low drifting maximum green for stage (0.1 sec). Alternative to Stages::drift_max if Junction::cut_max_times_marker
		set to force short cycle.
		Previous name: LODFMX */
	int32	low_drift_max[MAX_STAGES];

	/** Percentage green/cycle for stage; exponentially smoothed.
		Previous name: LAMBDA */
	int32	lambda[MAX_STAGES];

	/** Alternative lambda value; updated only when green just ended.
		Previous name: ALTLAM */
	int32	alt_lambda[MAX_STAGES];

	/** Merit value for current green stage; relates to efficiency of flow in  lanes and number 
		of lanes (percentage: 100=full sat-flow for 1 lane).
		Previous name: MERITV */
	int32	merit_value[MAX_STAGES];

	/*---------------- Stages Markers ------------------------*/

	/** Stage demand marker. Holds one of the values defined in mova_constants.h (STAGE_DEMAND_...).  
		Previous name: STADEM */
	int8	demand_marker[MAX_STAGES];
	
	/** Stage endsat maker. Holds one of the values defined in mova_constants.h (STAGE_ENDSAT_...).  
		Previous name: STENSA */
	int8	endsat_marker[MAX_STAGES];
 

	/*------------------ Bonus Variables ---------------------*/
	
	/** Stage bonus green for each stage having a dominant link resetting its bonus during the stage (0.1 sec).
		Previous name: STABON */
	int32	bonus_time[MAX_STAGES];

	/** Stage bonus link number: saves link providing bonus for each stage into Stages::bonus_time.
		Previous name: STBONL */
	uint8	bonus_link_num[MAX_STAGES];

	/*--------------------  E/P Markers ---------------------*/

	/** Emergency stage demand marker. Holds one of the values defined in mova_constants.h (EMERGENCY_DEMAND_...).
		Previous name: ESTDEM */
	int8	emergency_demand_marker[MAX_STAGES];
	
	/** Priority stage demand marker. Holds one of the values defined in mova_constants.h (PRIORITY_DEMAND_...).
		Previous name: PSTDEM */
	int8	priority_demand_marker[MAX_STAGES];

}; /*Stages*/


/**
The Links struct holds all the variables related to the junction's links. */
struct Links
{
	/** Link green flag. Can be LINK_FLAG_NOT_GREEN or LINK_FLAG_GREEN as defined in mova_constants.h.
		Previous name: LGREFL */
	int8	green_flag[MAX_LINKS];

	/** Reduced lost time (0.1 sec): influences capacity maximising process; links with more oversaturation 
		and higher LANEWF's retain a higher Links::reduced_lost_time,  and  hence are allowed to operate at relatively 
		lower efficiency to clear queues.
		Previous name: REDLT */
	int32	reduced_lost_time[MAX_LINKS];

	/** Extra green time (0.1 sec) for link: this has two interpretations. First, spare green, assuming all flow 
		had been at sat-flow. Second, free flow green after sat flow has ended.
		Previous name: EXTRAG */
	int32	extra_green_time[MAX_LINKS];

	/** Link smoothed flow (veh/h/10); sums Lanes::smoothed_flow for all lanes on link.
		Previous name: LSMFLO */
	int32	smoothed_flow[MAX_LINKS];

	/** Link oversat weighting factors and flag; sums total of LANEWF for oversat lanes on link.
		Previous name: LINKOS */
	int32	oversat_weighting_factor[MAX_LINKS];

	/** Variable min green for link (0.1 sec).
		Previous name: MINGRE */
	int32	var_min_green[MAX_LINKS];
	
	/** Maximum percentage saturation: during capacity maximisation, the efficiency of use of the green 
		(degree of saturation) is checked each scan; max_percent_sat saves the max value up till now in this green.
		Previous name: MAXPCS */
	int32	max_percent_sat[MAX_LINKS];
	
	/** Y-value as a percentage: mean ratio of smoothed flow / satflow for lanes on link; hence, 
		is effectively exponentially smoothed, as is derived from the smoothed flow.
		Previous name: YVALUE */
	int32	y_value[MAX_LINKS];

	/*------------------ Links Markers ---------------------*/

	/** Link end-sat marker. Holds one of the values defined in mova_constants.h (LINK_ENDSAT_...).
		Previous name: LIENSA */
	int8	endsat_marker[MAX_LINKS];

	/** Previous link end-sat marker. Holds one of the values defined in mova_constants.h (LINK_ENDSAT_...).
		Previous name: PRENSA*/
	int8	previous_endsat_marker[MAX_LINKS];
	
	/** Link demand marker. Holds one of the values defined in mova_constants.h (LINK_DEMAND_...).
		Previous name: LINDEM */
	int8	demand_marker[MAX_LINKS];

	/** Link detector fault status marker.
		Previous name: LIDETF */
	int32	det_fault_marker[MAX_LINKS];

	/** Benefiting link marker. Holds one of the values defined in mova_constants.h (BENEFIT_...).
		Previous name: BENLIN */
	int8	benefiting_marker[MAX_LINKS];

#ifndef M8_REMOVE_REV_DEMAND
	/** Reversionary demand for a link, if the link variable min green has not expired by the time 
		the drifting max green facility cuts short the green.
		Previous name: REVDEM */
	int8	rev_demand_marker[MAX_LINKS];
#endif
	
	/*--------------------- Bonus Variables -----------------------*/
	
	/** Bonus green time (0.1 sec) exponentially smoothed.  
		Previous name: BONUSG*/
	int32	bonus_green_time[MAX_LINKS];

	/** Total bonus green for a link; includes Links::bonus_green_time and Links::sink_bonus_green_time.
		Previous name: TOTBON*/
	int32	total_bonus_green_time[MAX_LINKS];

	/** Link bonus stage; number of the stage in which BONTIM occurs, and to which this link 
		attributes its bonus if used to set Stages::bonus_time.
		Previous name: LBONST */
	uint8	bonus_stage[MAX_LINKS];

	/** Sink  bonus green  time  (0.1 sec). Exponentially smoothed value.
		Previous name: SNKBON*/
	int32	sink_bonus_green_time[MAX_LINKS];

	/*----------------------- E/P Variables --------------------------*/

	/** Marker to note operation of cancel detector this cycle for an e/p link. 
		Holds one of the values defined in mova_constants.h (EP_CANCEL_...).
		Previous name: CANCEL*/
	int8	ep_cancel_marker[MAX_LINKS];
	
	/** Emergency/priority skip indicator for each link to show if link was in a skipped stage within the last cycle.
		Previous name: EPSKIP*/
	int32	ep_skip_indicator[MAX_LINKS];
	
	/** Emergency/priority truncation indicator for each link to show if link was in a stage cut short within the last cycle.
		Previous name: EPTRUN*/
	int32	ep_trunc_indicator[MAX_LINKS];

	/** Delayed demand time for e/p links with 4TT and 5TT SDCode
		Previous name: iDemandDelay_*/
	int32	ep_demand_delay[MAX_LINKS];

	/** Flag to determine whether to cancel ep demand in red or not.
		Previous name: cancelLinkEP*/
	mv_bool	is_to_cancel_ep_demand_in_red[MAX_LINKS];

	/** Flag to determine whether the ep demand exists or not.
		Previous name: epdemInIG*/
	mv_bool	is_ep_demanded_in_red[MAX_LINKS];

#ifdef M8_IMPORVED_BUS_PRIORITY
	/** The number of the priority link which demanded the stage (priority demand).*/
	uint8	demanded_priority_stage_num[MAX_LINKS];
#endif

}; /*Links*/


/**
The Lanes struct holds all the variables related to the junction's lanes. */
struct Lanes
{
	/** Shift register model of approaching traffic on lane: contains series of 0's and 1's;  1's represent vehicles.
		Previous name: SHREGA */
	int32	shift_register[MAX_LANES][REG_SIZE];

	/** Smoothed flow on lane (veh/h/10); exponentially smoothed.
		Previous name: SMFLOW */
	int32	smoothed_flow[MAX_LANES];
	
	/** Average historical flow matrix; for each lane, each half hour of the day, each day of the week, in veh/h/10.
		Previous name: AVEFLO */
	int32	average_flow[7][48][MAX_LANES];

	/** Accumulator for flow per half hour (vehicles) for use in historical data sub program.
		Previous name: TOTFLO */
	int32	total_flow_veh[MAX_LANES];

	/** Red count for X-DET det (veh).
		Previous name: REDCX */
	int32	red_count_x_det[MAX_LANES];

	/** Red count for IN-DET det (veh).
		Previous name: REDCIN */
	int32	red_count_in_det[MAX_LANES];

	/** Sink red counts (veh): saves counts decremented from Lane::red_count_in_det and Lane::red_count_x_det 
		as vehicles leave lane via IN Sink or X Sink detectors.
		Previous name: SINKRC */
	int32	sink_red_count[MAX_LANES];

	/** Exiting red counts (veh): count from start effective red of those vehicles leaving lane via an 
		associated X-DET or an OUT-DET.
		Previous name: EXITRC */
	int32	exit_red_count[MAX_LANES];

	/** Cumulative detector count (veh) on an IN-SINK detector.
		Previous name: INSCDC*/
	int32	in_sink_cum_det_count[MAX_LANES];

	/** Cumulative detector count (veh) on an X-SINK detector.
		Previous name: XSKCDC*/
	int32	x_sink_cum_det_count[MAX_LANES];

	/** Cumulative detector count (veh) on an IN-SINK2 detector.
		New in M8.*/
	int32	in_sink_2_cum_det_count[MAX_LANES];

	/** Cumulative detector count (veh) on an X-SINK2 detector.
		New in M8*/
	int32	x_sink_2_cum_det_count[MAX_LANES];

	/** Cumulative detector count (veh) on an associated detector, saved from Detectors::cum_det_count
		from previous cycle at end green. 
		Previous name: 	ASSCDC*/
	int32	assoc_cum_det_count[MAX_LANES][2];

	/** Extra inflow vehicles: estimate of Q back beyond IN-DET (veh).
		Previous name: INXTRA */
	int32	extra_veh_beyond_in_det[MAX_LANES];

	/** Lane oversaturated counter: counts number of consecutive oversaturated cycles for lane.
		Previous name: LAOSAT */
	int32	oversat_counter[MAX_LANES];

	/** Oversaturation initial  count: saves number of vehicles caught between IN-DET (or X-DET if XOSAT=1)
		and stop line at start amber, excluding  those  expected  to  continue;  these  plus  subsequent
		early red arrivals determine setting of oversat flags.
		Previous name: OSATIC */
	int32	oversat_init_count[MAX_LANES];

#ifdef M8_IMPORVED_BUS_PRIORITY
	int32	bus_weighted_red_count[MAX_LANES];
#endif

	/*--------------------- Lanes Markers ------------------------*/
	
	/** Lane detector fault status marker. Holds one of the values defined in mova_constants.h (LANE_DET_FAULT_...).
		Previous name: LADETF */
	int32	det_fault_marker[MAX_LANES];

	/** Lane end-sat marker. Holds one of the values defined in mova_constants.h ( LANE_ENDSAT_...).
		Previous name: LAENSA */
	int8	endsat_marker[MAX_LANES];

}; /* Lanes */

struct Detectors
{
	/** Last completed 'off' period (0.1 sec) for detector.
		Previous name: OLDGAP */
	int32		old_gap[MAX_DETS];

	/** Detector ON flag.
		Previous name: ONDET*/
	int8		on_det_flag[MAX_DETS];

	/** Duration of ON time (0.1 sec) if detector is now ON.
		Previous name: TIMON */
	int32		time_on[MAX_DETS];

	/** Duration of OFF time (0.1 sec) if detector is now OFF.
		Previous name: TIMOFF */
	int32		time_off[MAX_DETS];
	
	/** Cumulative detector count (veh).
		Previous name: CDC*/
	int32		cum_det_count[MAX_DETS];
	
	/** Old cumulative detector count (veh).
		Previous name: OLDCDC*/
	int32		old_cum_det_count[MAX_DETS];	/*oldcdc*/

	/** Previous cumulative detector count (veh).
		Previous name: PRECDC*/
	int32		pre_cum_det_count[MAX_DETS];

	/** Suspect detector flag. Holds one of the values defined in mova_constants.h (DET_FAULT_STATUS_...).
		Previous name: SUSDET */
	int8		suspect_status[MAX_DETS];

}; /* Detectors */


struct Messages 
{
	/** The current message number.
		Previous name: NMESS*/	
	uint8		current_msg_num;

	/** All the messages data.
		Previous name: IMESS*/	
	int32		msgs_data[MAX_MSGS][MSG_MAX_LENGTH];
};


/** The InputData struct holds the fields that are set externally (i.e, outside the core) by 
	DETSCN and used (reading only) in the core (Handlers) */
struct InputData
{
	/** The current active stage. Maps to SNOW in Tcomshr*/
	int	current_stage;

	/** The links green flag. Maps to GREENS in Tcomshr*/
	int * links_green_flag;

	/*------------------- Detectors Variables------------------- */

	/** The detectors ON flag. Maps to DETON in Tcomshr */
	signed char* dets_on_flag;

	/** The detectors ON time. Maps to TON in Tcomshr */
	int	* dets_on_time;

	/** The detectors OFF time. Maps to TOFF in Tcomshr */
	int	* dets_off_time;

	/** The detectors previous gap. Maps to PREGAP in Tcomshr*/
	int	* dets_previous_gap;

	/** The detectors vehicle count. Maps to VC in Tcomshr*/
	int	* dets_veh_count;

}; /* InputData*/

EXT_VISIBLE struct InputData input_data;



/** The DynamicData struct holds pointers for all the dynamic data fields structs. This struct
	is used mainly to save/restore the current dynamic data.*/
/*
struct DynamicData
{
	struct Program * p_program;
	struct Junction * p_junction;
	struct Stages * p_stages;
	struct Links * p_links;
	struct Lanes * p_lanes;
	struct Detectors * p_dets;
	struct Messages * p_msgs;

	int32 checksum;
};*/




#endif /*DYNAMIC_DATA_H*/