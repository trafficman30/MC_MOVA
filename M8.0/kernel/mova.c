/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         mova.c
*
*  TITLE:        MOVA Kernel: TRL MOVA and Siemens MS OMU interface
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	13-Apr-1988		1.0.0		..		CL		......			Initial Implementation
*	06-May-1988		1.1.0		..		CL		......			change below label 50 
*	06-Sep-1988		1.6.0		..		CL		......			check errors after warmup
*	14-Nov-1988		2.0.0		..		CL		......			larger COMSHR global, MAXMIN bigger
*	10-Apr-1989		2.2.0		..		CL		......			no changes here 
*	20-Sep-1989		2.6.0		..		CL		......			output DEM=9 when reversionary demand
*	23-Aug-1990		2.8.0		..		CL		......			call GENSTG every 0.5s (not released) 
*	12-Dec-1990		2.9.0		..		CL		......			Changes to GENSTG
*	23-Mar-1993		4.0.0		..		RV		......			 Bus priority features 
*	08-Dec-1993		4.0.0		..		RV		......			'Big' Increased stages, links
*	26-Jan-1998		4.0.0		..		MC		SIE_00			First undergoing system test
*	24-Apr-1998		4.0.0		..		MC		SIE_01			Initialisation (flstrt mcount)
*	29-Apr-1998		4.0.0		..		PC		SIE_02			'const' added to initialised static's
*	03-May-1998		4.0.0		..		PC		SIE_03			Overlay on 'Xcomshr', etc., replaced by explicit use of structures.
*	12-May-1998		4.0.0		..		PC		SIE_04			Modified the warmup code so that it uses confin[] to check for the controller ready bit.
*	17-Jan-2000		4.0.0		..		PC		SIE_06			day_from_date() parameters changed.
*	29-Sep-2004		5.0.0		..		IH		SIE_07			Use of CMOVA_EP #define to enable/disable emergency/priority code.
*	12-May-2005		5.0.3		..		IH		SIE_08			PCMOVA: Mova() runs in main PCMOVA kernel thread.
*	07-Mar-2005		5.1.0		..		RD		SIE_09			Added Cruise Speed data collection routine
*	08-Aug-2007		6.0.3		..		PK		TRL_10			PEDMAX used to display value in 10th of a sec. Ensured PEDMAX value is displayed in 10th of sec.
*	31-May-2011		7.0.0		AA		PK		CRN003			Several Changes for MOVA 7 release	
*	12-Dec-2011		7.0.1		AB		AK		CRN006			Correct compiler warnings
*	15-Mar-2012		7.0.3		..		AK		CRN009			Changes to conditional compilation flags
*	28-Jun-2012		7.0.3		..		AK		CRN011			Working data not cleared on dataset load
*	06-Jul-2012		7.0.3		AC		AK		CRN012			Memory leak in TMA module and statistics lost on dataset change
*	25-Jan-2013		7.0.5		..		AK		CRN023			Fix compiler warnings
*	31-Jan-2013		7.0.5		AD		AK		M7.0.5			Issue M7.0.5
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

#include <nucleus.h>
#include <string.h> /* IRH MOD: M5.0.0: 07/10/04: For memset */
#include <proj_def.h>
#include <nu_omu.h>
#include "gbltypes.h"
#include "obclock.h"
#include <error.h>    /* TRL/SIE: for SOFT_ERROR calls */
#include "errhand.h"  /* TRL/SIE: for MOVA error handler */
#include <diff_bss.h>
#include "kdebug.h"
#include "mova_constants.h"
#include "mova_types.h"

#include "dataset_interface.h"
#include "program_controller.h"
#include "core_interface.h"

#include "MVTypes.h"

#if defined (TRL_INTEGRATION_TEST)
#include "display_msg.h"
#include "generalfunc.h"
#include "ui_dataaccesstest.h"
#endif

/*      
    ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
    ³ Conversion to C:-                                                ³
    ³ ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ                                                ³
    ³ The original Fortran code was converted to C originally by       ³
    ³ Monitron using the Promula Fortran Converter. For version M4.0,  ³
    ³ the Fortran source was modified to include Bus Priority features ³
    ³ and used PARAMETERised variables to declare array sizes and      ³
    ³ loop-limits etc. This was converted by TRL (Mark Crabtree) using ³
    ³ the Promula Fortran Converter incorporating the changes that     ³
    ³ Monitron found necessary to complete the original convertion.    ³
    ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
      
     THIS IS THE STATEGY PROGRAM - CALLED EVERY 500 Msec BY MONITR
       IO ARRAY (1-16) = FORCE BITS TO UTC INTERFACE
       (17)=ERROR COUNT ; (18)=PHONE HOME  ; (19)=WATCHDOG COUNTER
       (20)=ONCONTROL   ; (21)=PRNTON      ; (22)=CONTROLLER READY FLAG
       (23)=FLOWON      ; (24)=STAGESTUCK  ; (25)=DEMSTA
       (26)=ASSTON      ; (27)=ERROR LOG   ; (28)=MOVON
       (29)=FLAG BETWEEN USER AND OUTPUT WHEN DISPLAYING
       (30)=PHONE LINE CONNECTED(=99) , 0 IF NOT , 9 AFTER 'FI'
       (31)=1 IF MULTI-STAGE CONFIRMATIONS=UNABLE TO CONTROL, 0=OK.
       (32)=INCREMENTED WHILE PHONE LINE IN USE.
	   
      Last change:  MC   15 Feb 95    2:34 pm
*/


#if defined (PC_WRAPPER)

    /* IRH MOD: PCMOVA: 07/12/05: Re-entry points for Mova task */
	#define g_MODULE_NAME ("Mova")

	typedef enum
	{
		MOVA_TASK_INIT_START,
		MOVA_TASK_INIT_END,
		MOVA_TASK_UPDATE_END

	} MOVA_TASK;

	extern void genstg(UNSIGNED argc, void * argv); /* 500ms*/

	static MOVA_TASK g_startPoint = MOVA_TASK_INIT_START;

#endif

/*  IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA:
	Removed the 'magic numbers' above - now using splits specified
	in gbltypes.h to get a size and offset (in bytes) for all data 
	to be zeroed at the start of mova().
	TODO: We should probably explicitly zero each of the variables in this
	area of shared data; the current method risks difficult to trace bugs (memory 
	overwrites) if we get the sizes wrong - too dangerous a shortcut IMO. 
	Other areas where this 'bulk initialisation' technique is used
	should also be reviewed. I've tried to simplify it here however. */
#define INIT_DATA_SIZE	(			\
	SPLIT9 * sizeof( int ) +		\
	SPLIT10 * sizeof( char ) +		\
	SPLIT11 * sizeof( int ) +		\
	SPLIT12 * sizeof( char )		\
)
#define INIT_DATA_OFFSET	( sizeof( Ccomshr ) - INIT_DATA_SIZE )


extern TIMESTAMP_TYPE Com_read_rtc(int, ... );


static void initialise_working_data();


/*
M7.0.1 - kernel modules MUST NOT include PCMOVA headers!
#include "..\pcclock.h" 
*/

void mova(UNSIGNED argc, void * argv)
{
	UNUSED(argc);
	UNUSED(argv);

	mv_bool is_in_warmup;

#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
    /* No extra task-related data for PCMOVA */
#else
	STATUS status;
	extern NU_TASK mova_task_control_block[NUM_MOVA_TASKS];
#endif


#if defined (PC_WRAPPER)
	/* IRH MOD: PCMOVA: 07/12/05: Re-entry points for Mova task */	
	switch ( g_startPoint )
	{
		case MOVA_TASK_INIT_START: goto Mova_InitStart;
		case MOVA_TASK_INIT_END: goto Mova_InitEnd;
		case MOVA_TASK_UPDATE_END: goto Mova_UpdateEnd;
		default: 
			KDEBUG_WRITE0( DEBUG_LEVEL_ERROR, MOVAMAIN, "Unrecognised reentry point for mova()" );
	}

Mova_InitStart:
	KDEBUG_WRITE0( DEBUG_LEVEL_INFO, MOVAMAIN, "Initialise start" );

#else
	DATA_ELEMENT TaskStatus = NU_SUCCESS;
	static int Genstg_fail_count = 0;
#endif /* defined (PC) */

	initialise_working_data();
 

#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
	g_startPoint = MOVA_TASK_INIT_END;
	return;
Mova_InitEnd:
	KDEBUG_WRITE0( DEBUG_LEVEL_VERBOSE, MOVAMAIN, "Initialise end" );
#else
	status = NU_Suspend_Task( &mova_task_control_block[MOVA_TSK] );
	if ( status != NU_SUCCESS )
		SOFT_ERROR(MOVA_Suspend_Task);
#endif

	goto S12000;
   
   
/* ^^^ ABOVE CODE ONLY NEEDED TO INITIALISE ~ SUBSEQUENTLY START FROM 10 */
   
S10:
	
	CI_set_each_scan_input_data(Tcomshr->snow, Tcomshr->greens, Tcomshr->deton,	Tcomshr->pregap,Tcomshr->ton, Tcomshr->toff, Tcomshr->vc);

	is_in_warmup = CI_start_scan();

	KDEBUG_WRITE3( DEBUG_LEVEL_VERBOSE, MOVAMAIN, "Just finished the scan, warmup=%d, the demanded stage=%d and next stage=%d",CI_get_warmup_counter(), CI_get_demanded_stage(), CI_get_next_stage());
		
/*
 --- REMEMBER POSITION IN CYCLE FOR COMPARISON NEXT SCAN --------------
 --- start stage change prog GENSTG; moved here from before 90050 M 2.8
 --- but, only call GENSTG after WARMUP completed.
*/
	if( !is_in_warmup ) 
	{
		/* Check GENSTG is ready to run */
		#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
		genstg( 0, NULL );
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
				status = NU_Resume_Task(&mova_task_control_block[GENSTG_TSK] );
				if ( status != NU_SUCCESS )
					SOFT_ERROR(GENSTG_Resume_Task);
			} else
				Genstg_fail_count++;
		#endif       
	}

S12000:
/*
 ~~~~~~~~~~~~~~~~~~~~~~~~~ FINISH ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
=======WAIT HERE UNTIL CALLED IN 500 M SEC ADD 1 TO WATCHDOG IO(19)====
*/
	Tcomshr->io[WATCH_DOG]++;
	
#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
	g_startPoint = MOVA_TASK_UPDATE_END;
	return;
Mova_UpdateEnd:
	KDEBUG_WRITE0( DEBUG_LEVEL_VERBOSE, MOVAMAIN, "Update end" );
#else
	status = NU_Suspend_Task(&mova_task_control_block[MOVA_TSK] );
	if ( status != NU_SUCCESS )
		SOFT_ERROR(MOVA_Suspend_Task);
#endif
	KDEBUG_WRITE0( DEBUG_LEVEL_VERBOSE, MOVAMAIN, "Resuming MOVA" );

/*
 ----------------------------------------------------------------------
 ------ TURN OFF MOVA. CONTINUE FROM HERE WHEN RE-STARTED -------------
 ----------------------------------------------------------------------
*/
	goto S10;
}



static void initialise_working_data()
{
	Tcomshr->io[ON_CONTROL] = 0;    /* set ON-CONTROL flag off*/
	Tcomshr->io[23] = 0;           /* WATCHDOG FLAG*/ /*TODO-T: Check this because the comment is not compatible with number 23 (STAGE_STUCK)*/
}