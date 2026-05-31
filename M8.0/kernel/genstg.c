/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         genstg.c
*
*  TITLE:        MOVA Kernel: Stage Information Update. 
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	13-Apr-1988		1.0.0		..		CL		......			Initial Implementation
*	13-Jun-1988		1.3.0		..		CL		......			change to output error flag 
*	08-Nov-1988		2.0.0		..		CL		......			change to checking order for flags  
*	08-Mar-1988		2.1.0		..		CL		......			buffer DEMSTA  
*	10-Apr-1989		2.2.0		..		CL		......			Initialise DLY every time used   
*	05-Jun-1989		2.5.0		..		CL		......			add call to OPSC to output force bits immediately MOVA demands new stage
*	22-Aug-1990		2.8.0		..		CL		......			Correct drop/resume-control logic, and critical error-count feature. 
*	24-Nov-1992		2.10.0		..		CL		......			Increase STAGE-DOES-NOT-END time to 15s 
*	18-Jan-1994		4.0.0		..		CL		......			'Big' Extended stages, links etc R A Vincent
*	26-Jan-1998		4.0.0		..		MC		SIE_00			First undergoing system test
*	29-Apr-1998		4.0.0		..		PC		SIE_01			'const' added to initialised static's (Siemens MOVA-13)	
*	03-may-1998		4.0.0		..		PC		SIE_02			Overlay on 'Xcomshr', etc., replaced  by explicit use of structures.
*	29-Sep-2004		5.0.0		..		IH		SIE_03			Amended timeout code so that the expected intergreen time (from Tcomshr->change[][]) isn't included.			
*																New function Errhand_LogMOVAError() used to log errors to Xerrlog, and Errhand_GetMOVAErrorValue()
*																to get a user specified value to add to error count.
*	20-Dec-2004		5.0.1		..		IH		SIE_04			Fix for 'negative' intergreen times (CHANGE<-1)
*	11-May-2004		5.0.0		..		IH		SIE_05			Genstg runs in main PCMOVA kernel thread (same as monitr()),
*																rather than in a separate thread.  Removed sleeps.
*	07-Mar-2005		6.0.0		..		RD		SIE_06			Removed "stage stuck" error at users' request as  it was considered overly sensitive.
*	31-May-2011		7.0.0		AA		PK		CRN003			Several Changes for MOVA 7 release	
*	15-Mar-2012		7.0.3		AB		AK		CRN009			Changes to conditional compilation flags
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

#include <stdlib.h>

/* #define SPROTOTYPE */
#include <nucleus.h>
#include <proj_def.h>
#include <nu_omu.h>
#include "gbltypes.h"
#include "obclock.h"
#include "mova_op.h"
#include "error.h"    /* TRL/SIE: for SOFT_ERROR calls */
#include "errhand.h"  /* TRL/SIE: for MOVA error handler */
#include <diff_bss.h>
#include "kdebug.h"
#include "mova_constants.h"

#include "core_interface.h"
#include "MVTypes.h"

/* Constants */
/* Time to wait (in 0.5 secs) before reporting a 'stuck' intergreen or stage (60 * 0.5 = 30secs) */
#define gnINTERGREEN_NOT_ENDED_TIMEOUT  (60)
#define gnSTAGE_NOT_ENDED_TIMEOUT       (60)
/* End of constants */


/* Functions declarations */
void genstg(UNSIGNED argc, void * argv);

static void init_start();
static void update_end();
static void handle_consistent_crbs();
static void handle_inconsistent_crbs(signed char controller_crb);
static void handle_off_crbs();
static void post_stage_change_checks();
static void wrong_stage_handler();
static void intergreen_ending();
static void stage_ending();
static void error_handler(int error_marker, int error_id, int error_data);
static void finalize_genstg();
static void turn_control_bits_off();

#ifdef M8_IMPROVED_LINKING
static void update_hold_and_release_bits();
#endif
/* End of declaration*/

/* Variables declarations */
char control[NumberOfForce+NumberOfExtra+NumberofOutputChans];

static int wrong_stage_flag; /* wrong stage flag, 0=OK, 1=wrong stage*/    
static int old_snow;
static int old_demsta;
static int leaving_stage_counter; /*was kst*/
static int intergreen_counter; /*was kig*/

#if !defined (PC_WRAPPER)
    extern NU_TASK mova_task_control_block[NUM_MOVA_TASKS];
#endif

/* detsin and confin mapped in InputMap() in module detscn */
extern char confin[mxconf];

/* IRH MOD: 15/01/06: PCMOVA: Made variable static - function returns rather 
than sleeps under S30, so need to preserve between function calls. */
#if defined (PC_WRAPPER)
    static int nIntergreenTime;
#else
    int nIntergreenTime;
    STATUS status;
#endif


/* IRH MOD: 24/02/05: PCMOVA: Genstg must be capable of running faster
than real-time to work with microsimulations - unlike 
user communication routines like TRLUserIF() and term().
Therefore the calls to NU_Sleep (a real-time function) have been
removed, and instead the function returns. 

A 'state' var has been added and can be set to:

1) waiting_for_ig_end
2) waiting_for_stage_end 
3) stage_changed (the state Genstg is in most of the time, when the current 
demand has been satisfied but a new demand has not yet been generated).  

On entering at S10, a switch control block is used with this state variable to 
jump to the right label.  This change can be applied for both the embedded and 
PC versions - the two NU_Sleeps are both for 0.5s, which is the rate that 
Genstg is called (suspended/resumed) at anyway.  It would constitute a 
simplification of the embedded version by removing a portion 
of unnecessary manufacturer-specific code. */


/* IRH MOD: PCMOVA: 07/12/05: Re-entry points for Genstg task */
#define g_MODULE_NAME ("Genstg")

typedef enum
{
	GENSTG_TASK_INIT_START,
	GENSTG_TASK_STAGE_ENDING,
	GENSTG_TASK_INTERGREEN_ENDING,
	GENSTG_TASK_UPDATE_END

} GENSTG_TASK;

static GENSTG_TASK g_startPoint = GENSTG_TASK_INIT_START;

void GetForceBits(char *Force)
{
	int index;

	for ( index=0; index < NumberOfForce; index++ )
	{
		Force[index] = control[index];
	}
}

void SetForceBits(int index, char val)
{
	control[index] = val;
}

char GetTakeOverStatus(void)
{
	return (control[NumberOfForce+1]);
}

char GetHurryInhibitStatus(void)
{
	return (control[NumberOfForce]);
}

void GetOutputChannels(char *OutChan)
{
	int index;

	for (index = 0; index < NumberofOutputChans; index++)
	{
		OutChan[index] = control[StartofOutputChans+index];
	}
}


//To remove
#include <stdio.h>

/* GENSTG called every 0.5s */
void genstg(UNSIGNED argc, void * argv)
{
	UNUSED(argc);
	UNUSED(argv);

	#if !defined (PC_WRAPPER) 
	while(1)
	{
	#endif

		/*Re-entry points for Genstg task */
		switch(g_startPoint)
		{
			case GENSTG_TASK_INIT_START:
				init_start();
				break;

			case GENSTG_TASK_UPDATE_END: 
				#if !defined PC_WRAPPER
				status = NU_Suspend_Task(&mova_task_control_block[GENSTG_TSK]);
				if(status != NU_SUCCESS) 
					SOFT_ERROR(GENSTG_Suspend_Task);
				#endif

				update_end();
				break;
		
			case GENSTG_TASK_INTERGREEN_ENDING:
				#if !defined PC_WRAPPER
				NU_Sleep(75); /* @6.67ms/tick = 0.5s */
				#endif

				intergreen_ending();
				break;
						
			case GENSTG_TASK_STAGE_ENDING: 
				#if !defined PC_WRAPPER
				NU_Sleep(75); /* @6.67ms/tick = 0.5s */
				#endif
				stage_ending();
				break;
		
			default:
				KDEBUG_WRITE0( DEBUG_LEVEL_ERROR, GENSTG, "Unrecognised reentry point for GENSTG" );
		}

	
	#if !defined (PC_WRAPPER) 
	}
	#endif
		
	return;
}


static void init_start()
{
	KDEBUG_WRITE0(DEBUG_LEVEL_VERBOSE, GENSTG, "Initialise start");
	
	wrong_stage_flag = 0;
	
	if(Tcomshr->stgdem == 0)
		Terrlog->stgoff = 1; /* ERRLOG */
	else
		Terrlog->stgoff = 0;

	Tcomshr->io[DEMAND_STAGE] = 0;
	Tcomshr->io[CONTROLLER_READY] = 0;

	g_startPoint = GENSTG_TASK_UPDATE_END;
	return;
}


static void update_end()
{
	const signed char internal_crb = Tcomshr->io[CONTROLLER_READY];
	const signed char external_crb =  confin[mxconf-1]; /*CRB from the controller directly */

	KDEBUG_WRITE0(DEBUG_LEVEL_VERBOSE, GENSTG, "Update end");

	Tcomshr->io[DEMAND_STAGE] = (char)CI_get_demanded_stage();  /*Tcomshr->demsta;*/
	
	if(Tcomshr->io[MOVA_ON] == 0) 
		Tcomshr->io[ON_CONTROL] = 0;
	
	/* if testing forces =>skip */
	if(Tcomshr->io[ON_CONTROL] == 2)
	{
		finalize_genstg();
	}
	else
	{
		if(internal_crb == 1 && external_crb == 1) /* The OK scenario */
			handle_consistent_crbs();   /* goto S20; */

		else if(internal_crb != external_crb)
			handle_inconsistent_crbs(external_crb); /*goto S40*//* goto S50*/

		else /*both are zeros*/
			handle_off_crbs(); /*goto S70*/
	}
}


/* Internal and external CRBs are 1 */
static void handle_consistent_crbs()
{
	/* SNOW is current STAGE set in program DETSCN
	   DEMSTA is STAGE demanded by MOVA. STAGES are from SITE data */
	
	old_snow = Tcomshr->snow;
	old_demsta = CI_get_demanded_stage(); /* Tcomshr->demsta;*/

	/* Check if the demanded stage is not within the range */
	if(old_demsta > Tcomshr->stages || old_demsta <= 0)
	{
		error_handler(0, ERROR_ID_INVALID_STAGE_DEMANDED,  Tcomshr->snow*1000+old_demsta*100);
	}
	else
	{
		turn_control_bits_off();

		 /* Don't set forces if ON-CONTROL=0 */
		if(Tcomshr->io[ON_CONTROL] == 0)
		{
			finalize_genstg();
		}
		else
		{
			control[old_demsta-1] = (char)Tcomshr->stgdem; /* 'old_demsta' is a confusing name; but is 'demsta' when new stage required */

			/* Set stage force bit, and next, the HI & TO bits */
			control[(long)mxstag] = (char)Tcomshr->stgdem;
			control[(long)mxstag+1] = (char)Tcomshr->stgdem;

#ifdef M8_IMPROVED_LINKING
			update_hold_and_release_bits(); 
#endif			
			OutputScan(CHANGE_OF_STAGE);

			leaving_stage_counter = 0;
			intergreen_counter = 0;

			post_stage_change_checks();
		}
	}
}

/* This function will be called if the internal CRB is different than the one in the signal controller (externl CRB) */
static void handle_inconsistent_crbs(signed char controller_crb)
{
	Tcomshr->io[CONTROLLER_READY] = controller_crb;

	/* Skip error message if on VA */
	if(Tcomshr->io[MOVA_ON] == 0)
	{
		turn_control_bits_off();
		OutputScan(MOVA_OFF);
		finalize_genstg();
	}
	else
	{
		if(controller_crb == 0)
		{
			error_handler(50, ERROR_ID_CONTROLLER_READY_BIT_OFF, 0);
		}
		else if(controller_crb == 1)
		{
			/* PC_WRAPPER: do NOT clamp warmup counter. In the embedded RTOS this
			 * guard prevented GS_check from being called twice due to task-scheduling
			 * races between monitr and detscn. In single-threaded PC_WRAPPER mode
			 * that race cannot occur, and the clamp (WC→stages) causes is_in_warmup()
			 * to return TRUE, which silently skips change_stage() in program_controller
			 * and freezes the kernel on the first forced stage indefinitely. */
			error_handler(99, ERROR_ID_CONTROLLER_READY_BIT_ON, Tcomshr->io[ERROR_COUNT]);
		}
	}
}

static void handle_off_crbs()
{
	turn_control_bits_off();
	OutputScan(MOVA_OFF);

	finalize_genstg();
	
	return;
}

/*S24 */
static void post_stage_change_checks()
{
	
	if(Tcomshr->snow == old_demsta)
	{
		/* STAGE changed - no IG !*/
		finalize_genstg();
	}
	else if(Tcomshr->snow == 0) /* Check if we have entered the intergreen */
	{
		/*  Get the time we expect to wait during this intergreen - including predefined 
			amber/red-amber and all-red times (we shouldn't count these in the long intergreen timeout). */

		nIntergreenTime = Tcomshr->change[old_snow][old_demsta];
        
		/* If change is illegal (shouldn't be) */
		if (nIntergreenTime == -1) 
		{
			nIntergreenTime = 0;
		}
		else
		{         
			/* Get the absolute values. CHANGE values can be < -1 for changes that can only be made during quiet times */
			nIntergreenTime = abs(nIntergreenTime);
         
			/* Convert from 0.1 to 0.5 second units (intergreen time will be divisible by 5). */
			nIntergreenTime /= 5;            
		}
        
		/* THE FOLLOWING CONDITIONS WILL NEVER BE TRUE */
		/* S30:
		if(Tcomshr->snow == old_demsta) 
		{
			// OK - new STAGE running
			finalize_genstg();
			return;
		}
		else if(Tcomshr->snow != old_snow && Tcomshr->snow != 0)
		{
			wrong_stage_handler();
			return;
		}
		*/
		g_startPoint = GENSTG_TASK_INTERGREEN_ENDING;
	} 
	else if(Tcomshr->snow != old_snow)
	{
		wrong_stage_handler();
	}
	else
	{
		g_startPoint = GENSTG_TASK_STAGE_ENDING;
	}
}

static void wrong_stage_handler()
{
	if(wrong_stage_flag == 0)
	{
		wrong_stage_flag = 1;
		leaving_stage_counter--;

		g_startPoint = GENSTG_TASK_STAGE_ENDING;
	}
	else
	{
		error_handler(0, ERROR_ID_WRONG_STAGE_CONFIRMED, Tcomshr->snow*1000+old_demsta*100);
	}
}

static void intergreen_ending()
{
	KDEBUG_WRITE0(DEBUG_LEVEL_VERBOSE, GENSTG, "Intergreen ending");

	intergreen_counter++;

	/* Don't include the expected predefined intergreen time within the timeout */
	if(intergreen_counter <= (gnINTERGREEN_NOT_ENDED_TIMEOUT + nIntergreenTime))
	{
		if(Tcomshr->snow == old_demsta) 
		{
			// OK - new STAGE running
			finalize_genstg();
		}
		else if(Tcomshr->snow != old_snow && Tcomshr->snow != 0)
		{
			wrong_stage_handler();
		}

#ifdef M8_IMPROVED_LINKING
		update_hold_and_release_bits();
		OutputScan(MOVA_IN_INTERGREEN);
#endif
	}
	else if(Tcomshr->io[ON_CONTROL] == 0)
	{
		/* No STAGE but just OFF-CONTROL*/
		turn_control_bits_off();

		OutputScan(MOVA_OFF);
		finalize_genstg();
	}
	else
	{
		error_handler(0, ERROR_ID_INTERGREEN_NOT_ENDED, 0);
	}

	return;

}

static void stage_ending()
{
	KDEBUG_WRITE0( DEBUG_LEVEL_VERBOSE, GENSTG, "Stage ending" );

	leaving_stage_counter++;

	if(leaving_stage_counter <= gnSTAGE_NOT_ENDED_TIMEOUT )
	{
		/* keep waiting for stage to end and inter-green to appear*/
		post_stage_change_checks();
	}
	else if(Tcomshr->io[ON_CONTROL] == 0)
	{
		/* No intergreen but just OFF-CONTROL*/
		turn_control_bits_off();
		OutputScan(MOVA_OFF);
		finalize_genstg();
	}
	else
	{
		error_handler(0, ERROR_ID_STAGE_NOT_ENDED, Tcomshr->snow*1000+old_demsta*100);
	}
}

/*S60*/
static void error_handler(int error_marker, int error_id, int error_data)
{
	/* No error message if high error count	*/
	if(Tcomshr->io[ERROR_COUNT] >= 20)
	{
		Tcomshr->io[ON_CONTROL] = 0;
		Tcomshr->io[MOVA_ON] = 0;  
	}
	else if(error_marker == 99)
	{
		Errhand_LogMOVAError(error_id, error_data);
	}
	else 
	{
		if(error_marker != 50)
			Tcomshr->io[ERROR_COUNT] = (char)((int)Tcomshr->io[ERROR_COUNT] + Errhand_GetMOVAErrorValue(error_id));
	
		error_data += Tcomshr->io[ERROR_COUNT]; 
		Tcomshr->io[ON_CONTROL] = 0;

		Errhand_LogMOVAError(error_id, error_data);
	}


	turn_control_bits_off();

	OutputScan(MOVA_OFF);

	finalize_genstg();

	return;
}

static void finalize_genstg()
{
	wrong_stage_flag = 0;

	g_startPoint = GENSTG_TASK_UPDATE_END;
	return;
}

static void turn_control_bits_off()
{
	int i;

	for(i=1; i<=mxstag+2; i++) 
	{
		control[i-1] = Terrlog->stgoff;
	}
}
#ifdef M8_IMPROVED_LINKING
static void update_hold_and_release_bits()
{
	

			if(CI_is_to_send_backward_hold_signal())
			{
				control[HOLD_SIGNAL_BIT] = (char)Tcomshr->stgdem;
				//printf("Setting the Hold signal ON \n");
			}
			else
			{
				control[HOLD_SIGNAL_BIT] = Terrlog->stgoff;
				//printf("Setting the Hold signal OFF \n");
			}
			
			if(CI_is_to_send_backward_release_signal())
			{
				control[RELEASE_SIGNAL_BIT] = (char)Tcomshr->stgdem;
				//printf("Setting the RELEASE signal ON \n");
			}
			else
			{
				control[RELEASE_SIGNAL_BIT] = Terrlog->stgoff;
				//printf("Setting the RELEASE signal OFF \n");
			}
}
#endif
void GS_check_and_update_io_flags()
{
#if !defined (PC_WRAPPER)
	DATA_ELEMENT TaskStatus = NU_SUCCESS;
	static int Genstg_fail_count = 0;
#endif

	if (Tcomshr->io[ERROR_COUNT] >= 20)
	{
		/* If error count too high drop off control */
		Tcomshr->io[MOVA_ON] = 0;
		return;
	}

	/* Warmup complete.  Set MOVA in control if CRB on */
	if (confin[mxconf-1] == 1 )
	{
		Tcomshr->io[CONTROLLER_READY] = 1;  /* set ready-to-control flag = 1 if RTC BIT =1  !M 2.8 */
		Tcomshr->io[ON_CONTROL] = Tcomshr->io[MOVA_ON];   /* M 2.8 */

		CI_set_demanded_stage( CI_get_current_stage() );
		
		/* need to set force bits - call genstg */
                     
		/* Check GENSTG is ready to run */
#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
		genstg(0, NULL);
#else
		status = NU_Task_Information(&mova_task_control_block[GENSTG_TSK],
									&TaskName[8],
									&TaskStatus,
									&TaskCount,
									&priority, &preempt, &time_slice,
									&stack_base, &stack_size, &min_stack);
		if ( status != NU_SUCCESS )
		{
			SOFT_ERROR(GENSTG_Check_Task)
		}
		if ( TaskStatus == NU_PURE_SUSPEND )
		{
			status = NU_Resume_Task( &mova_task_control_block[GENSTG_TSK] );
			if ( status != NU_SUCCESS )
				SOFT_ERROR(GENSTG_Resume_Task);
		} else
			Genstg_fail_count++;
#endif
    }

	return;
}