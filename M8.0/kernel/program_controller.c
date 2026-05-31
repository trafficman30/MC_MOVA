/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         program_controller.c
*
*  TITLE:        Mova Kernel: Program Controller
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

#include "program_controller.h"

#include "dynamic_data.h"
#include "dataset_interface.h"

#include "junction_handler.h"
#include "stages_handler.h"
#include "links_handler.h"
#include "lanes_handler.h"
#include "detectors_handler.h"

#include "timers_manager.h"
#include "messages_manager.h"
#include "core_interface.h"

#include "tma_alerts.h"

#include "algorithms_manager.h"

#include "kdebug.h"


extern void GS_check_and_update_io_flags();


/* static variables */
static struct Program prog;

/*static functions*/
static mv_bool update_prog_marker_at_green_stage();
static void warmup_check();
static void change_stage();
static void set_prog_marker(int8 marker_value);

void init_program_controller()
{
	CI_set_prog_pointer(&prog);
}

void init_program_dynamic_data()
{
	/* setting the Program dynamic data initialisation values*/
	prog.marker = PROG_MARKER_CONTINUED_INTERGREEN;
	prog.previous_marker = PROG_MARKER_CONTINUED_INTERGREEN;
}



/*!
*	\brief	Implements the control logic of MOVA. It depends mainly on the Prog::marker
*			to branch to the appropriate state.
*
*	\return		TRUE if MOVA still in the warmup state, FALSE otherwise.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
mv_bool program_controller()
{
	uint8 ret;
	mv_bool is_var_min_green_expired;

	KDEBUG_WRITE2( DEBUG_LEVEL_VERBOSE, CORE, "Start the core - Current Stage=%d & Previous Stage=%d", get_current_stage(), get_previous_stage());
	
	handle_junction_scan_initial_updates(prog.marker==PROG_MARKER_START_INTERGREEN);

	if( !is_in_warmup() )
	{
		KDEBUG_WRITE2( DEBUG_LEVEL_VERBOSE, CORE, "Not in Warmup anymore - Current Stage=%d & Previous Stage=%d", get_current_stage(), get_previous_stage());

		handle_junction_oversat_updates();
		handle_junction_bonus_situation();
	}

	handle_junction_ep_ext_updates();

#ifdef M8_IMPROVED_LINKING 
	/*Don't perform any of the forward/backward linking dec if this it is not the downstream
	junction and the following vars are defined.*/
	if(/*DI_get_upstream_junction_intergreen() > 0 &&*/ DI_get_link_distance_from_to_stoplines() > 0)
	{
		handle_junction_linked_mova_operation();
	}
#endif

	handle_junction_NTT_code();

	/*------------------------- Previously, Start of BLOCK A -----------------------------------*/
	if (get_current_stage() == INTERGREEN_STAGE)
	{
		if (get_previous_stage() == INTERGREEN_STAGE)
		{
			/* Continuing intergreen */
			set_prog_marker(PROG_MARKER_CONTINUED_INTERGREEN);

			KDEBUG_WRITE1( DEBUG_LEVEL_VERBOSE, CORE, "In continuing intergreen => MARKER=%d", prog.marker);
		}
		else /* the previous stage was not intergreen */
		{
			/* Start of intergreen (i.e., just ending a GREEN stage) */

			handle_junction_end_of_stage_updates();
			set_prog_marker(PROG_MARKER_START_INTERGREEN);

			KDEBUG_WRITE1( DEBUG_LEVEL_VERBOSE, CORE, "In start of intergreen => MARKER=%d", prog.marker);
		}

		handle_junction_intergreen_updates();
	}
	else /* currently in a GREEN stage */
	{
		if (get_previous_stage() != get_current_stage() && get_previous_stage() != INTERGREEN_STAGE)
		{
			/* Stage change with no intergreen */
			handle_junction_end_of_stage_updates();
		}
			
		if( update_prog_marker_at_green_stage() )
		{
			handle_junction_green_stage_updates();
	
			set_prog_marker(PROG_MARKER_VAR_MIN_GREEN);

			KDEBUG_WRITE1( DEBUG_LEVEL_VERBOSE, CORE, "PROG_MARKER_VAR_MIN_GREEN: now the MARKER=%d", prog.marker);
		}

		KDEBUG_WRITE1( DEBUG_LEVEL_VERBOSE, CORE, "At the end of intergreen => MARKER=%d", prog.marker);
	}
	/*------------------------- Previously, End of BLOCK A -----------------------------------*/
		
	warmup_check();
	
	if (prog.marker >= PROG_MARKER_CHECKING_ENDSAT)
	{
		/* If waiting for change no need for Block B */
		change_stage();
	}
	else
	{
		if (prog.marker > PROG_MARKER_CONTINUED_INTERGREEN)
		{
			/* Block B only if not in IG */

			if (get_previous_stage() != get_current_stage())
			{
				message_1_1();
				message_1_2();
			}

			if (prog.marker == PROG_MARKER_VAR_MIN_GREEN  && 
				prog.previous_marker <= PROG_MARKER_ABS_MIN_GREEN)
			{
				/* output end stage min message */
				message_2();
			}
				
			/*------------------------- Previously, Start of BLOCK B -----------------------------------*/
			handle_junction_demands_updates();

			is_var_min_green_expired = mv_false;

			if(handle_junction_next_stage_updates(&is_var_min_green_expired))
			{
				if(is_var_min_green_expired)
				{
					set_prog_marker(PROG_MARKER_CHECKING_ENDSAT);
					KDEBUG_WRITE1(DEBUG_LEVEL_VERBOSE, CORE, "PROG_MARKER_CHECKING_ENDSAT: now the MARKER=%d", prog.marker);
				}

				prog.ep_marker = prog.marker;
				if( is_junction_max_green_expired(prog.marker< PROG_MARKER_CHECKING_ENDSAT) )
				{
					/* max green expired */
					prog.ep_marker = PROG_MARKER_MAX_GREEN_EXPIRED;
					KDEBUG_WRITE1(DEBUG_LEVEL_VERBOSE, CORE, "PROG_MARKER_MAX_GREEN_EXPIRED: now the MARKER=%d", prog.ep_marker);
				}

				ret = handle_junction_ep_timers_and_markers_updates( prog.ep_marker == PROG_MARKER_MAX_GREEN_EXPIRED);
	
				if (ret == RET_NO_PRIORITY_CHANGE_OR_TRUNC_NEEDED)
					set_prog_marker(prog.ep_marker); /* reset program marker as no priority change or truncation needed */

				else if (ret == RET_EP_CHANGE_REQUIRED)
					set_prog_marker(PROG_MARKER_EP_CHANGE_REQUIRED); /* Mark emergency/priority change required, and return to implement */

				else if(ret == RET_NO_EP_CHANGE_REQUIRED) {}

			}
			/*------------------------- Previously, End of BLOCK B -----------------------------------*/
		}

		if (is_in_warmup())
		{
			/* Skip rest of program during warmup */
		}
		else if (prog.marker == PROG_MARKER_MAX_GREEN_EXPIRED) 
		{
			/* max green expired */
			message_5_2();
			change_stage();
		}
		else if (prog.marker == PROG_MARKER_EP_CHANGE_REQUIRED)
		{
			/* emergency/priority change truncating CUSIG */
			message_5_4();
			change_stage();
		}
		else
		{
			/*------------------------- Previously, Start of BLOCK C -----------------------------------*/
			if( handle_junction_endsat_checking(prog.marker >= PROG_MARKER_CHECKING_ENDSAT) )
			{
				/*getting here means that the stage endsat marker was set before, or
				has been set right now (i.e., endsat has been reached)*/
				set_prog_marker(PROG_MARKER_DELAY_AND_STOPS_OPTI);

				KDEBUG_WRITE1( DEBUG_LEVEL_VERBOSE, CORE, "PROG_MARKER_DELAY_AND_STOPS_OPTI: now the MARKER=%d", prog.marker);
			}

			KDEBUG_WRITE3(DEBUG_LEVEL_VERBOSE, CORE, "BLOCK C END ==> stages ENDSAT markers s1=%d, s2=%d and s3=%d", get_stage_endsat_marker(0), get_stage_endsat_marker(1), get_stage_endsat_marker(2) );
			/*------------------------- Previously, Emd of BLOCK C -----------------------------------*/

			if (prog.marker	== PROG_MARKER_START_INTERGREEN)
			{
				/* output start of intergreen message */
				if ( get_next_emeregency_stage_to_demand() > 0 || get_next_priority_stage_to_demand() > 0)
				 	message_6_2();
				else
					message_6_1();
			}
			else if (prog.marker < PROG_MARKER_VAR_MIN_GREEN)
			{
				/* end scan if in cont IG or absolute min */
			}
			else if (is_ep_hold_or_ext_present())
			{
				/* output emerg/priority hold/ext message & then end scan */
				message_3_2();
			}
			else if (prog.marker < PROG_MARKER_CHECKING_ENDSAT) /* MARKER == PROG_MARKER_VAR_MIN_GREEN */
			{
				/* Min green not yet expired so skip to end of scan */
			}
			else if (prog.marker == PROG_MARKER_CHECKING_ENDSAT) 
			{
				/* keep the endsat history (last entry) */
				if ((timers.signal_state_timer % 20) == 0)
				{
					TMA_update_endsat_data(DI_get_links_count(),get_links_endsat_marker());

					/* message 3-1 only output every 2 sec */
					message_3_1();
				}
			}
			else /* MARKER >= PROG_MARKER_DELAY_AND_STOPS_OPTI */
			{
				if (prog.previous_marker <= PROG_MARKER_CHECKING_ENDSAT)
				{
					/* Marker has just changed to PROG_MARKER_DELAY_AND_STOPS_OPTI so do endsat output */
					TMA_update_endsat_data_2(DI_get_links_count(), get_links_endsat_marker(), input_data.current_stage);
									
					message_3_1();
				}

				/* If significant oversaturation (LAOSMX>1) change stage now,
					otherwise optimise end stage decision in Block D */
				if (get_max_lane_oversat_counter() > 1)
				{
					/* output as for max change, then set up stage change */
					message_5_3();
					change_stage();
				}
				else
				{

#ifdef M8_RE_ESTABLISHMENT_OF_SATFLOW
					if(handle_junction_platoon_checking())
					{
						/*getting here means that there a vehs platoon has been detected*/
						set_prog_marker(PROG_MARKER_CHECKING_ENDSAT);
					}
					else
#endif	
					{
						/*------------------------- Previously, Start of BLOCK D -----------------------------------*/
						ALG_set_algorithm_params(ALG_DELAY_AND_STOPS_OPTIMISER);

						if(ALG_delay_and_stops_optimiser() )
						{
							/* Getting here means that the Benfit to continue GREEN is less that the Disbenfit, and thus
							a stage change demand is required */
							set_prog_marker(PROG_MARKER_DEMANDING_STAGE_CHANGE);

							KDEBUG_WRITE1(DEBUG_LEVEL_VERBOSE, CORE, "PROG_MARKER_DEMANDING_STAGE_CHANGE: now the MARKER=%d", prog.marker);
						}
						/*------------------------- Previously, End of BLOCK D -----------------------------------*/

						if (prog.marker == PROG_MARKER_DELAY_AND_STOPS_OPTI)
						{
							message_4();
						}
						else
						{
							message_5_1();
							change_stage();
						}
					}
				}
			}
		}
	}

	handle_junction_average_flow_update();

	/* End of the scan*/

	prog.previous_marker = prog.marker;


	KDEBUG_WRITE7( DEBUG_LEVEL_VERBOSE, CORE,
				   "Timers: cycle_time[0]=%d, link_green[0]=%d, link_waste_green[0]=%d, signal_state=%d, stage_cycle[0]=%d, stage_max_green=%d, wait_stage_change=%d", 
					timers.cycle_time_timer[0],
					timers.link_green_timer[0],
					timers.link_waste_green_timer[0],
					timers.signal_state_timer,
					timers.stage_cycle_timer[0],
					timers.stage_max_green_timer,
					timers.wait_stage_change_timer);

	return is_in_warmup();
}

//To remove
#include <stdio.h>

/*!
*	\brief	Updates the Prog::marker and checks the absolute-min-green time expiry
*			and based on that it sets the var-min-green time.
*
*	\return		 TRUE if the following functions to be called, FALSE otherwise.
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static mv_bool update_prog_marker_at_green_stage()
{
	/* initialise for stage start unless merely continuing green */
	if (get_current_stage() != get_previous_stage())
	{
		timers.signal_state_timer = DI_get_scan_period();
	}

	if(timers.wait_stage_change_timer != 0)
	{
		/* waiting for change to next stage (DEMSTA) */
		timers.wait_stage_change_timer += DI_get_scan_period();

		set_prog_marker(PROG_MARKER_WAITING_STAGE_CHANGE);

		KDEBUG_WRITE1(DEBUG_LEVEL_VERBOSE, CORE, "PROG_MARKER_WAITING_STAGE_CHANGE: now the MARKER=%d", prog.marker);
		return mv_false;
	}

    /* Mark junction as in stg-green */
	set_prog_marker(PROG_MARKER_ABS_MIN_GREEN);

	KDEBUG_WRITE1(DEBUG_LEVEL_VERBOSE, CORE, "PROG_MARKER_ABS_MIN_GREEN: now the MARKER=%d", prog.marker);

    if (get_var_min_green_time() > 0)
    {
		/* Withing stage variable min green - set marker and return */
		set_prog_marker(PROG_MARKER_VAR_MIN_GREEN);

		KDEBUG_WRITE1(DEBUG_LEVEL_VERBOSE, CORE, "PROG_MARKER_VAR_MIN_GREEN: now the MARKER=%d", prog.marker);
		return mv_false;
    }

    /* If stage absolute min-green not reached then return */
	if (timers.signal_state_timer < DI_get_stage_min_green_time( get_current_stage()) )
		return mv_false;


	return mv_true; /* the function returns TRUE if the following functions to be called */
}


/*!
*	\brief	Check if still in warmup and call  GS_check_and_update_io_flags
*			in the GENSTF module if needed.
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		01-09-2013 */
static void warmup_check()
{
	if (!is_in_warmup())
	{
		/* already completed first cycle monitoring period */
		return;
	}

	if (prog.marker <= PROG_MARKER_CONTINUED_INTERGREEN || 
		prog.previous_marker > PROG_MARKER_CONTINUED_INTERGREEN)
	{
		/* if not at start of stage green nothing to do */
		return;
	}

	/* MARKER must be >=2 and PREMRK be <=1, so is start green */

	/* Increment WARMUP counter at start-green for each stage */
    inc_warmup_counter();
		
	if (is_in_warmup())
	{
		/* warmup not completed yet */
		return;
	}	
	
	GS_check_and_update_io_flags();
}

static void change_stage()
{
	KDEBUG_WRITE1( DEBUG_LEVEL_VERBOSE, CORE, "In change_stage() and the marker=%d", prog.marker);

	handle_junction_stage_changing();

	if (get_demanded_stage() > INTERGREEN_STAGE) /* just to be sure it is not intergreen*/
		TMA_set_stage_start_time(get_demanded_stage());		
}

static void set_prog_marker(int8 marker_value)
{
	prog.marker = marker_value;
}


/* Used only once in the SF*/
int8 get_prog_marker()
{
	return prog.marker;
}