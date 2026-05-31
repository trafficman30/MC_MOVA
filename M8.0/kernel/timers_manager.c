/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         timers_manager.c
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

#include <time.h>
#include <sys/timeb.h>


#include "mova_types.h"
#include "timers_manager.h"
#include "dataset_interface.h"

#include "links_handler.h"

void init_timers_value()
{
	uint8 s; /* stages loop variable*/
	uint8 l; /* links loop variable*/
	
	for(s=0; s<DI_get_stages_count(); s++)
	{
		timers.stage_cycle_timer[s] = 5;
	}

	for(l=0; l<DI_get_links_count(); l++)
	{
		timers.cycle_time_timer[l] = 5;
		timers.link_green_timer[l] = 5;		
	}

#if M8_IMPROVED_LINKING
	timers.backward_hold_timer = 0;
#endif
}

/* this function called in the begining of Block P */
/*!
*	\brief	Updates the following timers at the begining of each scan:
*			signal_state_timer, stage_cycle_timer, cycle_time_timer and
*			link_green_timer.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
void update_timers()
{
	uint8 s;
	uint8 l;

	/* -------------- Junction timers ------------- */
	/* INCREMENT TIMER OF STAGE GREEN OR STAGE-STAGE INTERGREEN */
	timers.signal_state_timer += DI_get_scan_period();

	if (timers.signal_state_timer > 3000)
		timers.signal_state_timer = 3000;


	/* -------------- Stage timers ------------- */
	for (s = 0; s < DI_get_stages_count(); s++)
	{
		timers.stage_cycle_timer[s] += DI_get_scan_period();

		if (timers.stage_cycle_timer[s] > 3200)
			timers.stage_cycle_timer[s] = 3200;
	}

	/* -------------- Links timers ------------- */
	for (l = 0; l < DI_get_links_count(); l++)
	{
		/* Update link cycle timer */
		timers.cycle_time_timer[l] += DI_get_scan_period();

		if (timers.cycle_time_timer[l] > 3200)
			timers.cycle_time_timer[l] = 3200;
		
		/* Update linkg GREEN flag before updating its timer */
		update_link_green_flag(l);

		/* Update GREEN timer */
		if (get_link_green_flag(l) > 0)
			timers.link_green_timer[l] += DI_get_scan_period();

		if (timers.link_green_timer[l] > 3000)
			timers.link_green_timer[l] = 3000;
	}
}