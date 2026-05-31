/*******************************************************************************
*
*                      MOVA CONSTANTS
*
*  FILE:         mova_constants.h
*
*  TITLE:        MOVA Kernel: MOVA Constants
*
*  (c) Copyright TRL Ltd 2012. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/


#if !defined (MOVA_CONSTANTS_H)
#define MOVA_CONSTANTS_H


#define MAX_LINKS							(60)
#define MAX_LANES							(30)
#define MAX_LANES_ON_LINK					(3)
#define MAX_DETS							(64)
#define MAX_DETS_TYPES						(11)
#define MAX_EP_DETS							(2)
#define MAX_ASSOC_DET						(2)
#define MAX_STAGES							(10)
#define MAX_PRIORITY_LINKS_ACCOC_TO_LANE	(3) 

#ifdef M8_SHIFT_REGISTER_EXTENSION
	#define	REG_SIZE		(51)
#else
	#define	REG_SIZE		(21)
#endif

#define MAX_TEL			(16)

#ifdef PCMOVA
	#define MAX_MSGS		(50)	/* Number of messages stored*/
#else
	#define MAX_MSGS		(50)	/* Number of messages stored*/
#endif // PCMOVA

/*Increasing the MSG_MAX_LENGTH by 1 (to be 184 instead of 183 to include the IsRead flag at the end of each stored message.*/
#define MSG_MAX_LENGTH	(184)   /* Max length of any message*/

#define MAX_OPP_LANES	(3)
#define MAX_GAPS		(100)   /* Max number of gaps per lane during the GREEN */

/* Detectors Types */
#define IN_DET			(0)
#define X_DET			(1)
#define OUT_DET			(2)
#define IN_SINK_DET		(3)
#define COMB_X_DET		(4)
#define X_SINK_DET		(5)
#define ALT_UP_DET		(6)
#define ALT_DOWN_DET	(7)
#define STOP_LINE_DET	(8)
#define IN_SINK2_DET	(9)
#define X_SINK2_DET		(10)

#define INTERGREEN_STAGE (0)

/* Program :: marker */
#define PROG_MARKER_START_INTERGREEN		(0)
#define PROG_MARKER_CONTINUED_INTERGREEN	(1)
#define PROG_MARKER_ABS_MIN_GREEN			(2)
#define PROG_MARKER_VAR_MIN_GREEN			(3)
#define PROG_MARKER_CHECKING_ENDSAT			(4)
#define PROG_MARKER_DELAY_AND_STOPS_OPTI	(5)
#define PROG_MARKER_DEMANDING_STAGE_CHANGE	(6)
#define PROG_MARKER_MAX_GREEN_EXPIRED		(7)
#define PROG_MARKER_WAITING_STAGE_CHANGE	(8)
#define PROG_MARKER_EP_CHANGE_REQUIRED		(9)

/* Stages::endsat_marker (stensa) */
#define STAGE_ENDSAT_NOT_YET								(0) /* the resetting value at the start of IG*/
#define STAGE_ENDSAT_REACHED_FOR_ALL_REL_LINKS				(1) /* all relevant links in this stage reached endsat*/
#define	STAGE_ENDSAT_VAR_MIN_GREEN_OR_EP_HOLD_EXPIRED		(-1) /* once relevant overlap min-greens expired for change to SNEXT*/
#define	STAGE_ENDSAT_VAR_MIN_GREEN_OR_EP_HOLD_NOT_EXPIRED	(-2)

/* Links::endsat_marker (liensa)*/
#define LINK_ENDSAT_NOT_YET										(0)
#define LINK_ENDSAT_NORMAL										(1)
#define LINK_ENDSAT_SUPERSAT									(2)
#define LINK_ENDSAT_QUEUE_OVER_X_DET							(3)
#define LINK_ENDSAT_LOW_FLOW_EFFICIENCY_FACTOR_WHILE_OVERSAT	(4)
#define LINK_ENDSAT_SHORT_TERM_INEFFICIENCY_WHILE_OVERSAT		(5)
#define LINK_ENDSAT_UPSTREAM_TOO_SMALL_WHILE_OVERSAT			(6)
#define LINK_ENDSAT_MAX_GREEN_EXPIRED							(7)
#define LINK_ENDSAT_MIN_GREEN_EXPIRED_FOR_FAULTY_DET			(8)
#define LINK_ENDSAT_BONUS_GREEN_EXCEED_LOST_TIME				(9)

/* Lanes::endsat_marker (LAENSA)*/
#define LANE_ENDSAT_PART_2_FROM_2PART_CHECKING_IS_DONE			(-2)
#define LANE_ENDSAT_PART_1_FROM_2PART_CHECKING_IS_DONE			(-1)
#define LANE_ENDSAT_NOT_YET										(0)
#define LANE_ENDSAT_NORMAL										(1)
#define LANE_ENDSAT_NO_MORE_GREEN_NEEDED						(2)
#define LANE_ENDSAT_QUEUE_OVER_X_DET							(3)
#define LANE_ENDSAT_PART_1_AND_2_ARE_DONE						(4)
#define LANE_ENDSAT_IRRELEVANT_LANE								(8)
#define LANE_ENDSAT_TOO_MANY_SUSPECTED_DETECTORS				(9)

/* Stages::demand_marker (stadem) */
#define STAGE_DEMAND_NO_DEMAND					(0)
#define STAGE_DEMAND_LATCHED					(1)
#define STAGE_DEMAND_CONDITIONAL				(2)  /*unlatched*/
#define STAGE_DEMAND_REVERSIONARY				(2)
#define STAGE_DEMAND_6MRR						(6)
#define STAGE_DEMAND_SKIP_AS_NOT_REQ_BY_6MRR	(-6)

/* Link demand marker - get_link_demand_marker() - (lindem)*/
#define LINK_DEMAND_NO					(0)
#define LINK_DEMAND_LATCHED				(1)
#define LINK_DEMAND_UNLATCHED			(2) /* special demand - for Ped or RT*/
#define LINK_DEMAND_CALL_CANCEL			(3)
#define LINK_DEMAND_EMERGENCY			(4)
#define LINK_DEMAND_PRIORITY			(5)

/*LGREEN (DI_get_link_green_status())*/
#define LINK_STATUS_RED						(0)
#define LINK_STATUS_GREEN_IN_ONE_STAGE		(1)
#define LINK_STATUS_GREEN_IN_SEVERAL_STAGES	(2)
#define LINK_STATUS_GREEN_AND_UNOPPOSED		(3)  /*but is opposed in another stage*/
#define LINK_STATUS_GREEN_BUT_OPPOSED		(4)  /*link is unopposed in another stage*/

/*get_link_green_flag (lgrefl)*/
#define LINK_FLAG_NOT_GREEN					(0)
#define LINK_FLAG_GREEN						(1)

/* Links: det_fault_marker*/
#define LINK_DET_FAULT_NO		(0)
#define LINK_DET_FAULT_MINOR	(1)
#define LINK_DET_FAULT_MAJOR	(2)

/* Lanes: det_fault_marker (ladetf)*/
#define LANE_DET_FAULT_NO_PROBLEM					(0)
#define LANE_DET_FAULT_YES_BUT_CAN_DO_ENDSAT_CHECK	(1) /* some suspect detectors but can manage end-sat checks for lane (only IN-DET OK)*/
#define LANE_DET_FAULT_TOO_MANY_TO_DO_ENDSAT_CHECK	(2) /* too many suspect detectors; can't do end-sat check*/
#define LANE_DET_FAULT_NEED_2_PART_ENDSAT_CHECK		(-1) /*either faulty ASSOCD or there is an IN-SINK detector on the lane, so need to do 2-part end-sat check using IN-DET +X-DET.*/

/* Detectors faulty status (suspect_status)*/
#define DET_FAULT_STATUS_OK											(0)
#define DET_FAULT_STATUS_SUSPECTED_DUE_TO_LONG_OFF					(1)
#define DET_FAULT_STATUS_SUSPECTED_DUE_TO_LONG_ON					(2)
#define DET_FAULT_STATUS_SUSPECTED_DUE_TO_CHATTERING				(3)
#define DET_FAULT_STATUS_SUSPECTED_PED_LINK							(4)
#define DET_FAULT_STATUS_SUSPECTED_DUE_TO_24_HOURS_OFF				(5)
#define DET_FAULT_STATUS_SUSPECTED_DUE_TO_24_HOURS_ON				(6)
#define DET_FAULT_STATUS_SUSPECTED_DUE_TO_24_HOURS_CHATTERING		(7)
#define DET_FAULT_STATUS_SUSPECTED_DUE_TO_24_HOURS_PED_LINK_FAULT	(8)


/* Benefiting links marker - benefiting_marker (benlin)*/
#define BENEFIT_RESET			(0)
#define BENEFIT_RELEVENT_LINK	(1)
#define BENEFIT_IRRELEVENT_LINK	(2)

/* cut_max_times_marker (cutmax) */
#define CUTMAX_DISABLE_SHORT_CYCLE_CONTROL	(0)


/*EP constants*/

/*ep_cancel_marker (CANCEL)*/
#define EP_CANCEL_CLEAR	(0)	
#define EP_CANCEL_EXT	(1)

/* ep_hold_marker (EPHOLD)*/
#define NO_EP_HOLD_PRESENT			(0)
#define EMERGENCY_HOLD_PRESENT		(4)
#define PRIORITY_HOLD_PRESENT		(5)

/* ep_ext_marker (EPX)*/
#define NO_EP_EXT_PRESENT			(0)
#define EMERGENCY_EXT_PRESENT		(4)
#define PRIORITY_EXT_PRESENT		(5)


/*emergency_demand_marker (ESTDEM) */
#define EMERGENCY_DEMAND_NONE		(0)
#define EMERGENCY_DEMAND_LATCHED	(4)
#define EMERGENCY_DEMAND_UNLATCHED	(8) /*conditional demand*/

/*priority_demand_marker (PSTDEM) */
#define PRIORITY_DEMAND_NONE		(0)
#define PRIORITY_DEMAND_LATCHED		(5)
#define PRIORITY_DEMAND_UNLATCHED	(9) /*conditional demand*/


#ifndef M8_REMOVE_REV_DEMAND
/*rev_demand_marker (REVDEM)*/
#define REV_DEMAND_NONE								(0)
#define REV_DEMAND_VAR_MIN_GREEN_NOT_EXPIRED		(1)
#define REV_DEMAND_EMERGENCY_HOLD_PRESENT			(4)
#define REV_DEMAND_PRIORITY_HOLD_PRESENT			(5)
#define REV_DEMAND_EMERGENCY_EXT_PRESENT			(-4)
#define REV_DEMAND_PRIORITY_EXT_PRESENT				(-5)
#endif

#define	NO_SUITABLE_DET			(-1)
#define SKIP_TO_NEXT_LANE		(-1)
#define SKIP_MAX_GREEN_CALC		(-1)
#define INVALID_LINK			(-1)


/* possible return values for the function 'handle_junction_ep_timers_and_markers_updates()'*/

#define RET_NO_EP_CHANGE_REQUIRED				(0)
#define RET_NO_PRIORITY_CHANGE_OR_TRUNC_NEEDED	(1)
#define RET_EP_CHANGE_REQUIRED					(2)


/* times (in 0.1 sec) after which link demands inserted */
#define QUICK_NUDGE_PERIOD		(300)
#define STANDARD_NUDGE_PERIOD	(900)

/*Algorithms ID*/
#define ALG_CALC_BONUS_GREEN_TIME			(1)
#define ALG_CALC_REDUCED_LOST_TIME			(2)
#define ALG_CALC_SHORT_CYCLE_CONTROL_PARAMS	(3)
#define ALG_CAPACITY_MAXIMISATION			(4)
#define ALG_DELAY_AND_STOPS_OPTIMISER		(5)
#define ALG_ESTIMATE_QUEUES_SIZE			(6)



/*IO array*/
#define ERROR_COUNT		(16)
#define PHONE_HOME		(17)
#define WATCH_DOG		(18)
#define ON_CONTROL		(19)
#define PRINT_ON		(20)
#define CONTROLLER_READY (21)
#define FLOW_ON			(22)
#define STAGE_STUCK		(23)
#define DEMAND_STAGE	(24)
#define ASS_LOG_ON		(25)  /* Check */
#define ERROR_LOG		(26)
#define MOVA_ON			(27)
#define USER_OUTPUT_FLAG (28)
#define PHONE_LINE_CONNECTED (29)
#define MULTI_STAGE_CONF (30)
#define INC_WHILE_PHONE_IN_USE (31)

/*Compact MOVA*/
#define COMPACT_MOVA_LANE       999

/* DI_get_min_veh_count_between_x_and_out_det(...) (MIXOUT)*/
#define USE_EARLY_CUT_OFF (99)
#define USE_CALL_CANCEL (-99)

/* DI_get_oversat_det - XOSAT */
#define OVERSAT_DET_IN_DET			(0) /* Not a Compact MOVA lanes and will use IN-DET for checking the oversat */
#define OVERSAT_DET_CMOVA_X_DET		(1) /* Compact MOVA lane and will use X-DET for checking the oversat */
#define OVERSAT_DET_CMOVA_COMB_X	(2) /* Compact MOVA lane and will use COMB-X detector for checking the oversat */

/*Linked MOVA*/
#define NO_LINKING_IS_REQUIRED		(0)
#define FORWARD_LINK				(1)
#define BACKWARD_LINK				(2)


/*ToD Constants*/
#define TOD_DAYS_NUM      (7)
#define TOD_TIMES_NUM     (96) /* 24 * 4 = 96 quarter-hour periods */


/*Operation flags failure reason*/
#define OP_FLAG_FR_TOO_MANY_ERRORS				(1)
#define OP_FLAG_FR_INVALID_LIC					(2)
#define OP_FLAG_FR_TESTING_OF_FORCE_CHANNELS	(3)


/*Force stage failure reasons*/
#define FORCE_STAGE_FR_INVALID_STAGE_NUMBER		(1)
#define FORCE_STAGE_FR_MOVA_IS_ENABLED			(2)
#define FORCE_STAGE_FR_MULTIPLE_CONF_RECEIVED	(3)
#define FORCE_STAGE_FR_ALREADY_FORCED			(4)

#define MAX_NO_OF_ERRORS_STORED (100)

#endif /*MOVA_CONSTANTS_H*/