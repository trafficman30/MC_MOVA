/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         timers_manager.h
*
*  TITLE:        Mova Kernel: Timers Manager
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

#if !defined (TIMERS_MANAGER_H)
#define TIMERS_MANAGER_H

#include "mova_constants.h"
#include "mova_types.h"

/* The Timers struct holds all the kinds of timers inside the Core */
struct Timers
{
	/** Signal  state  timer  (0.1 sec). Times either the current green or intergreen
		Prevoius name: SISTIM */
	int32	signal_state_timer;

	/** Stage  cycle timer (0.1 sec); times from end stage (i.e. start IG)
		to current time in cycle.
		Previous name: stcycl*/
	int32	stage_cycle_timer[MAX_STAGES];

	/** Cycle time  timer  for  link (0.1 sec); times from start amber for link 
		to current time in cycle.
		Previous name: cycltm*/
	int32	cycle_time_timer[MAX_LINKS];

	/** Link green timer (0.1 sec); times current link greens.
		Previous name: lingre*/
	int32	link_green_timer[MAX_LINKS];


	/** Link  waste green  timer  (0.1 sec);  time wasted in current green 
		compared with full sat flow.
		Previous name: lwaste*/
	int32	link_waste_green_timer[MAX_LINKS];

	/** Waiting for a stage change timer (0.1 sec).
		Previous name: waitim*/
	int32	wait_stage_change_timer;

	/** Stage maximum green timer (0.1 sec) for current green.
		Previous name: stamax*/
	int32	stage_max_green_timer;
	
	/**	Ped waiting timer (0.1 sec).
		Previous name: nPedWaitTime*/
	int32	ped_wait_timer;
	
	/*------------------ E/P timers ---------------------*/

	/** Emergency/priority extension maximum timer. 
		Previous name: epxmxt*/
	int32	ep_ext_max_timer;

	/** Emergency-stage extension timer. 
		Previous name: esxtim*/
	int32	emergency_stage_ext_timer;

	/** Priority-stage extension timer.
		Previous name: psxtim*/
	int32	priority_stage_ext_timer;

	/** Emergency/priority-link extension timer. 
		Previous name: */
	int32	ep_link_ext_timer[MAX_LINKS];

#if M8_IMPROVED_LINKING
	/* ------------- Linked MOVA timers --------------------*/

	/** The timer that defines for how long should the downstream controller keeps the 'hold' signal ON
		which is received by the upstream controller.*/
	int32	backward_hold_timer;
#endif
	
};

EXT_VISIBLE struct Timers timers;


void init_timers_value();
void update_timers();

#endif /*TIMERS_MANAGER_H*/