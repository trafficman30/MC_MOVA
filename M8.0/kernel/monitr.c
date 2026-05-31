/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         monitor.c
*
*  TITLE:        MOVA Kernel: Control Suite of black box programs. 
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	14-Apr-1988		1.0.0		..		CL		......			Initial Implementation
*	06-Sep-1988		1.6.0		..		CL		......			Multiple stage adds 1 to error count.Time-of-day changes taken from .D1
*	14-Nov-1988		2.0.0		..		CL		......			Changes to Warmup. New global. Change to load data after time change renumbered.
*	07-Mar-1989		2.1.0		..		CL		......			Addition of automatic software RESTart, Use DOUT(11) for 0.5 sec synch pulse.
*	10-Apr-1989		2.2.0		..		CL		......			Clear BUFFER(1) at midnight. DLY initialised every time used. 
*	05-May-1989		2.4.0		..		CL		......			Mods to mult stage confn Force Flag clearing
*	25-May-1989		2.5.0		..		CL		......			call OPSC every 500Msec, now called by GENSTG
*	17-Jul-1989		2.6.0		..		CL		......			call OPSC every 100Msec as well as GENSTG
*	15-Oct-1990		2.8.0		..		CL		......			set KERR to 19 when CODE set to 33 
*	19-Jan-1994		4.0.0		..		RV		......			'Big' extended stages, links 
*	26-Jan-1998		4.0.0		..		MC		SIE_00			First undergoing system test
*	27-Jan-1998		4.0.0		..		MC		SIE_01			Cold restart initialisation procedure
*	16-Feb-1998		4.0.0		..		MC		SIE_02			Init WRITE on warm restart
*	27-Feb-1998		4.0.0		..		MC		SIE_03			Check presence of Terminal
*	04-Mar-1998		4.0.0		..		MC		SIE_04			Year 2000 problem in TOD area
*	05-Mar-1998		4.0.0		..		MC		SIE_05			LED flashes fast or slow (never off)
*	24-Apr-1998		4.0.0		..		MC		SIE_06			Initialization (MOVA_LED_ON/OFF/FLASH)
*	29-Apr-1998		4.0.0		..		MC		SIE_07			'const' added to initialised static's
*	03-May-1998		4.0.0		..		PC		SIE_08			Overlay on 'Xcomshr', etc., replaced by explicit use of structures.
*	11-May-1998		4.0.0		..		PC		SIE_09			Added call to InitHandset() every time the handset is plugged in (MOVA-7)
*	12-May-1998		4.0.0		..		PC		SIE_10			Added call to day_from_date() 
*	15-May-1998		4.0.0		..		PC		SIE_11			InputScan() now called from DETSCN.C
*	01-Jun-1998		4.0.0		..		PC		SIE_12			Corrected value returned from new day_from_date() for timetable.
*	16-Jan-2000		4.0.0		..		PC		SIE_13			Deleted InitHandset() (done by SERSWH now).Replaced use of 
*																'io_input with 'MOVA_IF.LComms == MOVAIF_COMMS_TO_MOVA'
*	17-Jan-2000		4.0.0		..		PC		SIE_14			day_from_date() parameters changed. MOVA-71: Corrected October BST change.
*	09-Mar-2000		4.0.0		..		PC		SIE_15			Decrement error count when MOVA is restarted after multiple stage confirms.
*	27-Mar-2000		4.0.0		..		PC		SIE_16			Clear the 'oct' flag when the clock has passed Sunday 0215 (normal operation) or the March change occurs
*	26-Apr-2000		4.0.0		..		PC		SIE_17			After multiple stage confirms error count not incremented.
*	27-Apr-2000		4.0.0		..		PC		SIE_18			Switched hourly flow logging (Tcomshr->io[22]) back on by default on first time power-up.
*	04-May-2000		4.0.0		..		PC		SIE_19			Fix reason why all force bits appear as '1'	after INI=2 (near S30).
*	22-Feb-2001		4.0.0		..		KS		SIE_20			Added #include "hardware.h" for MAX_EXPANSION_CARDS definition
*	29-Sep-2004		5.0.0		..		IH		SIE_21			Time of day array times[] removed - data and related  processing now in new Datahand.c module
*																New code called for loading datasets from Datahand.c module
*																checks() working dataset checksum calculation function.
*	12-May-2006		5.0.3		..		IH		SIE_22			Monitr runs in main PCMOVA kernel thread 
*																- Starting of user interface threads TRLUserIF and Term 
*																moved to PCMOVA-specific task module (PCTasks).
*																- Monitr initialisation completes in one go (rather than
*																suspending half way through as in embedded version), 
*																and suspends at start of update loop rather than end
*																(PCMOVA Bugfix 10078).
*																- Checking whether local handset connection still exists
*																moved to Term thread (Monitr doesn't run continuously
*																in PCMOVA as Term does - only runs when an update
*																is received with new detector/confirm data from the
*																controller/simulator).
*																- "t" variable no longer used to determine when to run 
*																mova() - now only used for counting quarter-hours for 
*																plan changes (PCMOVA Bugfix 10078).
*																- BST check skipped (PCMOVA Bugfix 10085) - daylight 
*																saving changes potentially confusing when running MOVA 
*																with microsimulations, so no equivalent of Siemens 
*																handset commands CKA and CKR provided in PCMOVA.
*	31-May-2011		7.0.0		AA		PK		CRN003			Several Changes for MOVA 7 release	
*	12-Dec-2011		7.0.1		AB		PK		CRN004			SatFlow and CANDET improvements
*	15-Mar-2012		7.0.3		..		AK		CRN009			Changes to conditional compilation flags
*	06-Jul-2012		7.0.3		AC		AK		CRN012			Memory leak in TMA module and statistics lost on dataset change
*	29-Sep-2012		7.0.4		AD		AK		CRN015			TMA Period initialisation after dataset load 
*	21-Jan-2013		7.0.5		..		AK		CRN024			Reinstate label S118 for Siemens
*	31-Jan-2013		7.0.5		AE		AK		M7.0.5			Issue M7.0.5
*	03-May-2013		7.0.6		..		PA		CRN031			Don't reset SF data on warm restart
*	22-May-2013		7.0.6		AF		AK		M7.0.6			Issue M7.0.6
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
#include <string.h> /* IRH MOD: M5.0.0: 07/10/04: Added for memset() */
#include <proj_def.h>
#include <nu_omu.h>
#include <inpoup.h>
#include "gbltypes.h"
#include "mova_op.h"
#include <error.h>    /* TRL/SIE: for SOFT_ERROR calls */
#include "errhand.h"  /* TRL/SIE: for MOVA error handler */
#include "obclock.h"
#include "write.h"
#include <hardware.h>
#include <keptdata.h>
#include <diff_bss.h>
#include "datahand.h" /* IRH MOD: M5.0.0: 27/07/04: For new Datahand functionality */

#include "tma_preprint.h"
#include "tma_mem.h"
#include "tma_alerts.h"
#include "sfstats.h"

#include "kdebug.h"

#include "core_interface.h"
#include <stdio.h>

#include "dataset_handler.h"

#include "generalfunc.h"

#if defined (MOVA_DEBUG)
	#include <cyg/infra/diag.h>
#endif

#if defined (TRL_INTEGRATION_TEST)
#include "generalfunc.h"
#endif

# define ON		1
# define OFF	0

/*
=======================================================================
  MONITR IS THE CONTROLLING PROGRAM FOR THE SUITE OF BLACK-BOX PROGRAMS
  IT IS RE-ACTIVATED EVERY 100 MILLISEC AND CALLS IN SEQUENCE:

  EVERY 100 MSEC SCAN  IPSC  -       INPUT SCANNING PROGRAM
  EVERY 100 MSEC SCAN  DETSCN - DETECTOR/STAGE/PHASE PROGRAM
  4 SCANS OUT OF 5       MOVA  -     STRATEGY PROGRAM
	   note MOVA calls GENSTG {stage change program} when required
  EVERY .25 HR CHECKS WHETHER NEW DATA SET REQUIRED
  FOUR SCANS OUT OF FIVE (not MOVA scan) OPSC - OUTPUT SCANNING PROGRAM
  EVERY 100 MSEC MONITR CHECKS THE ERROR CONDITIONS

  Initially MONITR sets up the array containing new DATA times
  from Tcomshr->dattim(20) and Tcomshr->datnum(20) - taken from the .D1 file in SITE(ALLDAT,1)
  and Tcomshr->datnum contains (DATA SET NO.) + (DAY CODE)*100
  day codes are: 0=Sat 1=Sun etc 6=Fri 7=all 8=no Sun 9=weekdays
		   DATNUM=-1 when empty
  Sets up the quarter-hour count in CELL {1 to 7*24*4=672}
  Array TIMES(CELL) contains 0 if no change, or DATA set No. if change.
  Re-loads SITE DATA with entry in TIMES(CELL) if there is one.
  T counts up to 9000  0.1 sec (0.25 Hr); RESET every 0.25 Hr.
  CELL counts Quarter hours (96*7 per week)
   RECALCULATED every Midnight to ensure CELL hasn't drifted off.
  MARK1 AND MARK2 ARE MARKERS AS FOLLOWS:
   MARK1=???? or 50 WHEN RAM EMPTY; =1234 WHEN FILLED OK
   MARK1=9999 LOAD DATA FROM PROM; =8888 RESET TIMES(CELL) ARRAY (USER)
   MARK2=0 USUALLY;   =PLAN NUMBER 1,2 OR 3 WHEN PLAN NEEDS LOADING IN DETSCN
   >>>>>>>>>>>>>>
   MONITR PROGRAM CHECKS MARK1=?  IF NOT = 1234,8888,9999   THEN i)
					IF = 9999 LOAD DATA FROM PROM - then ii)
					IF = 8888 THEN RESET TIMES(CELL) ARRAY - then iv)
					IF = 1234 then iii)
 i) SET LOG MARKERS TO 0 (NERR,NMESS,NDAT) - 1st TIME ONLY.
 ii) OFF-LINE; warmup MOVA; LOAD PROM SITE DATA; CLEAR CRASH;
		  LOAD DATA #1 IN RAM; CLEAR OUTPUTS
 iii) CHECK RAM SITE DATA CHECKSUM; LOAD TIMES(CELL) ARRAY;
 iv)  GET TIME/DATE - CHECK IF OK; CALCULATE CELL; LOOK BACK FOR CORREC
		   DATA SET # TO LOAD (IF ANY ToD CHANGES); LOAD DATA SET IF NECESSA
   DROP INTO MAIN LOOP
   ALSO CHECKS MARK 1 EVERY SCAN. IF = 9999 THEN ii) ; IF = 8888 THEN i
				 IF = 1234 THEN NEXT SCAN ; IF SOMETHING ELSE WELL ?!?!?!?

	***************************************************************
			IO ARRAY (1-16) = FORCE BITS TO UTC INTERFACE - M 4.0 not used
			(17)=ERROR COUNT ; (18)=PHONE HOME  ; (19)=WATCHDOG COUNTER
			(20)=ONCONTROL      ; (21)=PRNTON     ; (22)=CONTROLLER READY FLAG
			(23)=FLOWON         ; (24)=STAGESTUCK  ; (25)=DEMSTA
			(26)=ASSTON         ; (27)=ERROR LOG   ; (28)=MOVON
			(29)=FLAG BETWEEN USER AND OUTPUT WHEN DISPLAYING
			(30)=PHONE LINE CONNECTED(=99) , 0 IF NOT , 9 AFTER 'FI'
			(31)=1 IF MULTI-STAGE CONFIRMATIONS=UNABLE TO CONTROL, 0=OK.
			(32)=INCREMENTED WHILE PHONE LINE IN USE.
	***************************************************************
=======================================================================
*/

extern TIMESTAMP_TYPE Com_read_rtc(int, ... );
extern char lFirstPowerup; /* cold restart flag to initialize data */
char lIsClockSet;   /* initialize clock on cold start */
unsigned char  lLH_CONNECTED = 0; /* something plugged into handset? */
unsigned char MOVA_LED_OFF;   /* initialised in code */
unsigned char MOVA_LED_ON;
unsigned char MOVA_LED_FLASH;

/*Marker to record the stage number when all red ped detectors are served.*/
int PedStageServed = -1;
extern char lMessOn;

#if defined (PC_WRAPPER)    
	/* IRH MOD: PCMOVA: 07/12/05: Re-entry points for Monitr task */
	#define g_MODULE_NAME ("Monitr")

	typedef enum
	{
		MONITR_TASK_INIT_START,
		MONITR_TASK_UPDATE_START

	} MONITR_TASK;

	extern void detscn(UNSIGNED argc, void * argv); /* 100ms */
	extern void mova(UNSIGNED argc, void * argv);   /* 500ms */

	static MONITR_TASK g_startPoint = MONITR_TASK_INIT_START;

	/* The number of times monitr() has been updated.  This is used
	to determine when to call mova(), which must be called every
	0.5 seconds from the start of the simulation.  The timestep
	counter "t" could be used for this, but is also used for
	determining the current quarter hour for time of day changes,
	and changes when the MOVA clock changes.  It's therefore cleaner 
	to have a separate variable for determining when to restart mova(). */
	static int g_updateCount;
#else
	extern NU_TASK   mova_task_control_block[NUM_MOVA_TASKS];
#endif


#ifndef M8_ENABLED 
/* IRH MOD: M5.0.0: 14/07/04: New checksum function */
void checks(Ccomshr const * const, long * const, long * const );
#endif

void monitr(UNSIGNED argc, void * argv);
void SetLED_State(char,char);

static void Monitr_ProcessPedDet();

void monitr(UNSIGNED argc, void * argv)
{
	UNUSED(argc);
	UNUSED(argv);

	/* time and date structure */
	TIMESTAMP_TYPE tTimeDate;
	extern char control[NumberOfForce+NumberOfExtra];

#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */

#else

	STATUS status;

#endif

	static const char listp[4] = {65,86,79,77};

	static int old31,oct,midnit,i,k,d,cell,t,plan;
	static long i4;

#ifndef M8_ENABLED
	static long c4, d4;
#endif
  
	int nTimeOfDayPlan,nErrorID, nActivePlan;
	BOOL boSuccess;


	extern char control[NumberOfForce+NumberOfExtra];

#if defined (PC_WRAPPER)
	
	/* IRH MOD: PCMOVA: 07/12/05: Re-entry points for Monitr task */
	switch ( g_startPoint )
	{
		case MONITR_TASK_INIT_START: goto Monitr_InitStart;
		case MONITR_TASK_UPDATE_START: goto Monitr_UpdateStart;
		default: 
			KDEBUG_WRITE0( DEBUG_LEVEL_ERROR, MONITR, "Unrecognised reentry point for detscn" );
	}

Monitr_InitStart:    

	/* Reset update count */
	g_updateCount = 0;
	KDEBUG_INITIALISE();

	CI_init_core_module();
	CI_set_external_function_pointers(&DivideError);

#if defined (TRL_INTEGRATION_TEST)
	InitialiseIntegrationLogs();
#endif
#else

	/* Make sure tasks are ready for NU_Resume */
	DATA_ELEMENT TaskStatus = NU_SUCCESS;
	static int Mova_fail_count = 0;
	static int Detscn_fail_count = 0;

#endif /* defined (PC) */

/*
=======================================================================
---------------------------- initialisation ---------------------------
*/
	 
	/* First time initialization (set when ic_first_time_powerup_code is initiated) */
	if ( lFirstPowerup )
	{
		Tcomshr->mark1 = 0; /* cold start initialization */
		lIsClockSet = FALSE;
		tTimeDate = Com_read_rtc( READ_RTC); /* initialize clock */
		lFirstPowerup = FALSE;
		MovaSatFlowStats_PerformColdStart();
		 
	}
	  
	/* other initialisation */
	MOVA_LED_OFF   = TRUE;   
	MOVA_LED_ON    = FALSE;  
	MOVA_LED_FLASH = FALSE;

	/* TRL/SIE: Check whether Terminal has been connected */
	/* no connection possible immediately on combine unit */
	lLH_CONNECTED = FALSE; /*io_input(IO_HANDSET_DTR);*/
	  
	WRITE(INITIALIZE);   /* Warm start initialization */
	Tcomshr->io[31] = 0; /* To allow phone connection */
	  
	/* initialise input_data here before it might possibly be used in special conditioning */
	CI_set_each_scan_input_data(Tcomshr->snow, Tcomshr->greens, Tcomshr->deton, Tcomshr->pregap, Tcomshr->ton, Tcomshr->toff, Tcomshr->vc);

	/* TRL/SIE: start other programs here ... */
#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
	/* See PCTasks_Start() */
	/* NOTE: not handling remote connections in PC-MOVA at the moment */
#else
	status = NU_Resume_Task(&mova_task_control_block[USER_TSK]);
	if ( status != NU_SUCCESS )
		SOFT_ERROR( MOVA_general_Task_Resume);

	status = NU_Resume_Task(&mova_task_control_block[TERM_TSK]);
	if ( status != NU_SUCCESS )
		SOFT_ERROR( MOVA_general_Task_Resume);

	status = NU_Resume_Task(&mova_task_control_block[PHONE_TSK]);
	if ( status != NU_SUCCESS )
		SOFT_ERROR( MOVA_general_Task_Resume);

	status = NU_Resume_Task(&mova_task_control_block[ANSWER_TSK]);
	if ( status != NU_SUCCESS )
		SOFT_ERROR( MOVA_general_Task_Resume);
#endif
		  
/*
=======================================================================
--------------------------- program starts here -----------------------
*/
	old31 = 0;
	oct = 0;
	midnit = 0;                                     /* M 2.0*/
/*
-OLD31 is previous value of IO(31), the multiple stage flag (in DETSCN)
------ OCT is a flag which is set when BST times are changed in OCTober
------ prevents clock being put back 1 hour for evermore.
*/
	Terrlog->stgoff = 0;
	if (Tcomshr->stgdem == 0)
		Terrlog->stgoff = 1;/* /DINOUT/ */

/*
------------------------------- check RAM condition -------------------
*/
#if defined (MOVA_DEBUG)
	diag_printf( "Monitr.c Top: Tcomshr->mark1 == %d\n", Tcomshr->mark1 );
#endif       

	if (Tcomshr->mark1 == 1234) 
		goto S36;/* RAM OK */                           
	if (Tcomshr->mark1 == 9999) 
		goto S30;/* load data from PROM*/
	/* IRH MOD: M5.0.0: 10/09/04: S40 label removed */
	if (Tcomshr->mark1 == 8888) 
		goto S56/*S40*/;/* RESET TIMES(CELL)*/       
	   
	Tcomshr->mark1 = 50; /* here mark1 = 0 after cold restart M 1.6*/
/*
-------------------------------- 1st TIME ONLY ------------------------
*/
	Tcomshr->io[19] = 0;             /* Set off-line*/
	CI_set_warmup_counter(-1); /* Tcomshr->warmup = -1;*/
	CI_set_last_stage(mxstag); /* Tcomshr->lastag = mxstag;*/ /* TRL mod: */
	CI_set_current_stage(0);  /*Tcomshr->cusig = 0;*/
	CI_set_next_stage(mxstag);  /*Tcomshr->snext = mxstag;*/  /* TRL mod:  */
	CI_set_previous_marker(1); /*Tcomshr->premrk = 1;*/             /* Warmup*/   
		   
	Errhand_ResetErrorCount();
	CI_reset_current_msg_number();

	Terrlog->ndat = 1;                       /* Reset log positions*/   
	Tcomshr->io[22] = 1;   /* flow on */     /* MOD: ARH 28/10/94 was 0 Automatically start logging */
	Tcomshr->io[25] = 1;   /* asston */
	Tcomshr->io[26] = 0;   /* error log */   /* reset flags*/
	Tcomshr->io[20] = 0;   /* on control */
	Tcomshr->io[16] = 0;   /* error count */
	Tcomshr->io[27] = 0;   /* movon */
	Terrlog->buffer[24] = 71;
	Terrlog->buffer[25] = 79;
	Terrlog->buffer[30] = 73;
	Terrlog->buffer[31] = 78;
	   
	/* IRH MOD: M5.0.0: 31/08/04: No longer using crash array - use
	 * DBZ error string instead */
	memset( Terrlog->dbzstr, 0, xgnDBZ_MESSAGE_STR_LEN );

	CI_reset_lanes_average_flow();

	Errhand_Initialise();

	for (i = 1; i <= 4; i++)
	{
//		for (j = 1; j <= 100; j++) 
//		{
//			Terrlog->ierr[i-1][j-1] = 0;/* clear error log*/
//		}

		for (k = 1; k <= 1100; k++) 
		{
			Terrlog->idat[i-1][k-1] = 0;/* clear assessment log*/
		}

		Terrlog->buffer[20L+(long)i-1] = listp[i-1];
		Terrlog->buffer[26L+(long)i-1] = listp[i-1];
	}

S30:
/*
=======================================================================
------------------------ load data from PROM --------------------------
*/
	   
	/* Get the plan that should be active at the moment 
	 * If there isn't one, use the first (guaranteed to be there) */   
	plan = DSH_get_active_data_plan_number();
	if ( plan == 0 )
	{
		plan = 1;
	}
	flagDstrig=1 ;  /*change DoT using dstring detectors*/

	/* IRH MOD: M5.0.0: 21/07/04: Movsub needs to know which plan to try 
	 * to load for the new data handling code. Therefore call to 
	 * Movsub placed below plan selection code. */

/*
****************** load RAM from EEPROM with site data ****************
Siemens MOVA - actually load data from stores held in RAM (DATAHAND),
   unless those are corrupt, load hugemova.mds from the flash memory.
*/
#ifdef M8_ENABLED
	boSuccess = DSH_load_dataset_from_memory((MVInt8)plan);
#else
	/* IRH MOD: M5.0.0: 21/07/04: Use new Movsub() function to load default
	 * dataset into RAM repository (if the given 'plan' doesn't
	 * exist or is corrupt) - works similarly to old Movsub() */
	Movsub( plan );

	/* IRH MOD: M5.0.0: 02/09/04: Datahand now handles loading of datasets */
	/* Load the requested dataset from the repository to working memory */
	boSuccess = Datahand_LoadDataset( plan );    
#endif

	if (!boSuccess)
	{
		/* If we didn't load the new dataset successfully 
		 * (highly unlikely - RAM repository data unlikely to be corrupted
		 * between call to Movsub and Datahand_LoadDataset) */
		/* Re-enter to load default dataset from flash with Movsub */
		goto S30;
	}

	/* Initialise TMA logs */
	TMA_Init_();

	goto S56;

S36:
/*
=======================================================================
------------- site data already loaded - check data is OK -------------
*/
	if(Tcomshr->stages < 2 || Tcomshr->stages > mxstag) goto S38; /*TRL:*/  
	
#ifndef M8_ENABLED /*In M8 we don't check sum in this way.*/
	/* IRH MOD: M5.0.0: 14/07/04: New checksum function */
	checks( Tcomshr, &c4, NULL );
	  
	d4 = Tcomshr->chksm1[0]*1000L + Tcomshr->chksm1[1]; /* TRL/MC mod: L added 21-1-93  */      
	if (c4 == d4) goto S56;                 /* checksum OK > */
#else
	goto S56;
#endif

S38:
/*
-------------------------------------------------------- checksum error
*/
	/* IRH MOD: M5.0.0: 27/08/04: Use function to report error */
	Errhand_LogMOVAError( ERROR_ID_CHECKSUM_ERROR, 0 );
	
	   
	/* Checksum invalid - RAM probably corrupt
	 * Load data from flash with Movsub */
	goto S30;
	  
S52:
/*
=======================================================================
-------------------------- error in times - clear force bits ----------
*/
/* TRL/SIE: new routine to turn MOVA off */
	Tcomshr->io[19] = 0;
	for (i = 1; i <= ForceArraySize; i++)
	{
		control[i-1] = Terrlog->stgoff;
	}
	Switch_MOVA_Off(NO_SAV_MESSAGES, NO_REBOOT);
S56:
/*
-------------------------- get correct number in CELL -----------------
*/
 
#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
	/* IRH MOD: 21/03/06: PCMOVA: BUGFIX: 10078.       
	Don't wait here - doesn't make much sense to halt mid-initialisation,
	and in PCMOVA we want to determine which Time of Day plan should run
	the first time monitr() is called (the "initialisation" call), rather
	than on the second time monitr() is called (the first "update" call), 
	which won't occur until the simulation starts.
	This allows the user to check what ToD plan should load by using the 
	"T" (View titles and timestamps) option in the Dataset submenu
	before the simulation begins.
	We also don't want to wait here when the clock has been updated
	in PCMOVA - i.e. when Tcomshr->mark1 == 8888. 
	   
	TODO: This change is probably appropriate for embedded MOVA too - 
	currently whenever the clock is changed in embedded MOVA, one
	0.1 second update will be skipped/missed because of the wait below. */
#else
	status = NU_Suspend_Task ( &mova_task_control_block[MONITR_TSK] );
	if ( status != NU_SUCCESS )
		SOFT_ERROR(MONITR_Suspend_Task); /* soft error */
#endif
	/* TRL/SIE */
	tTimeDate = Com_read_rtc( READ_RTC);
	   
	/* check time OK. Loop until it is */
	if (tTimeDate.month < 1L || tTimeDate.month > 12L || 
		tTimeDate.date  < 1L || tTimeDate.date  > 31L) goto S52;
		  
	if (tTimeDate.year < 0 || tTimeDate.year > 99 || 
		tTimeDate.seconds < 0UL || tTimeDate.seconds > 86400UL) goto S52;
/*
---------------------------------------- set up CELL from time and date
*/
	/* extract day 1-7 (Mon-Sun) from date */
	d = day_from_date(tTimeDate);
	/* convert to 0-6 (Sat-Fri), i.e. Monday was '1', should be '2', etc.*/
	d = (d+1) % 7;
	/* calculate which quarter of an hour period is required */
	i4 =  tTimeDate.hours * 4L;
	i4 += tTimeDate.minutes/15L; /*sec/900L;*/
	cell = (int)( (long)(d*96)+i4+1L ); /* TRL/MC Mod: Cast to int    */
	t = (int)(10L*((tTimeDate.minutes *60L + tTimeDate.seconds) % 900L) ); /* no 1/10th secs into current quarter */

	if (midnit != 0) 
	{                        /* M 2.0  If checking*/
		midnit = 0;                             /* M 2.0  at midnight*/
		Terrlog->buffer[0] = 0;    /* M 2.1*/
		goto S100;                              /* M 2.0  - skip the*/
	}
	/*
------------------------------------------------------- set up data set
*/

	/* IRH MOD: M5.0.0: 31/08/04: Get the plan for the current ToD
	 * from the new data handling module. This plan will
	 * be zero if there is no time-of-day data, or 1-4
	 * if there is time-of-day data. */
	nTimeOfDayPlan = DSH_get_tod_plan(cell - 1);
	nActivePlan = DSH_get_active_data_plan_number();
			  
	/* If there is a valid and new Time of Day plan */
	if ( ( nTimeOfDayPlan > 0 ) && ( nTimeOfDayPlan != nActivePlan ) )
	{
		Tcomshr->mark2 = nTimeOfDayPlan;
	}
	   
	/* Reset mark1 */
	Tcomshr->mark1 = 1234; 
	 
	oct = 0;
  
S100:

/*
=======================================================================
								   -------------
#############################| MAIN LOOP |#############################
											  -------------
*/

#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
	/* IRH MOD: 21/03/06: PCMOVA: BUGFIX: 10078.
	In PCMOVA we wait at the start of each update rather than at 
	the end - we've got rid of the mid-initialisation wait under S56
	so we need to wait here instead to prevent an update immediately 
	after initialisation. */
	g_startPoint = MONITR_TASK_UPDATE_START;
	return;
Monitr_UpdateStart:

	/* This is a new update so increment the update count */
	g_updateCount++;
	KDEBUG_WRITE1( DEBUG_LEVEL_VERBOSE, MONITR, "Update start; update count == %d.", g_updateCount);

#endif


/* -------------- re-activate INPUT scanning program every 100 Msec ------*/

/*  IRH MOD: 22/02/05: PCMOVA: MOVA_IF is a Siemens struct.  
	Use appropriate PC function - this is done in Term - see note there.
	The Term thread is now responsible for checking whether a local handset 
	connection exists - monitr() only runs when the simulation runs.
*/
#if !defined (PC_WRAPPER)
	if ( MOVA_IF.LComms == MOVAIF_COMMS_TO_MOVA )
	{
		/* if handset WAS not connected */
		if ( !lLH_CONNECTED )
		{
			/* then re-initialise the drivers, etc. */
			/* InitHandset(); - done by SERSWH, not MOVA */
		}
		/* inform the rest of the system that the handset is connected */
		lLH_CONNECTED = TRUE;
	}
	else /* handset is not connected */
	{
		/* inform the rest of the system that the handset is not connected */
		lLH_CONNECTED = FALSE;
	}
#endif

/*
 ----------------------------- re-activate DETSCN every 100 Msec ------
*/           
	   /* Check Detscn is ready to run */
#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
	detscn( 0, NULL );
#else
	status = NU_Task_Information(&mova_task_control_block[DETSCN_TSK],
							&TaskName[8],
							&TaskStatus,
							&TaskCount,
							&priority, &preempt, &time_slice,
							&stack_base, &stack_size, &min_stack);

	if ( status != NU_SUCCESS )
	{
		SOFT_ERROR(DETSCN_Check_Task)
	}

	if ( TaskStatus == NU_PURE_SUSPEND )
	{
		status = NU_Resume_Task ( &mova_task_control_block[DETSCN_TSK] );
		if ( status != NU_SUCCESS ) 
			SOFT_ERROR(DETSCN_Resume_Task);/* error handling to be inserted */
	} 
	else
		Detscn_fail_count++;
#endif
	  
/*
------------------------------ re-activate MOVA every 500 Msec --------
*/

#if defined (PC_WRAPPER)

	/* IRH MOD: 21/03/06: PCMOVA: BUGFIX: 10078.
	If this isn't the fifth update, don't run mova() etc. 
	No longer using "t" for update counting - it's cleaner to
	use a separate variable, as "t" is also used for counting
	quarter-hours for plan changes.  In PCMOVA it is necessary
	that mova() runs at the end of each 0.5 seconds from the
	*start* of simulation - "t" doesn't guarantee this, e.g.
	its value will change when the user sets the MOVA clock, 
	at the start of each quarter-hour, and after midnight.  */
	if ( g_updateCount != 5 ) goto S110;

	/* Fifth update, so reset update count */
	g_updateCount = 0;
#else
	if ( t % 5 != 1) goto S110;
#endif
	   
	   
	/* IRH MOD: M5.0.0: 02/09/04: Now using ControlSync instead of
	 * DoutSync (see definition of ControlSync in Gbltypes.h) 
	 * This was causing the array to be written to out of bounds. */
	/* TRL/MC mod: This next line was after line below */
	control[ControlSync] = 1; /* set synch pulse M 2.1  */

	OutputScan(SYNC_PULSE_ON); /* activate pulse */       
	Tdinout->dout[DoutMovaFault] = Tcomshr->io[19];  /* walamp[1] */
	/* TRL/SIE: set LED state */  
	SetLED_State(Tdinout->dout[DoutDetFault], Tdinout->dout[DoutMovaFault]);
	/* check if >1 stg confrmn*/
	if (Tcomshr->io[30] != 0 || old31 != 0) goto S124;
S102:
			 
	/* Check MOVA is ready to run */
#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */

	/*PK - Function to inspect detectors(pedestrain) for first start time(among all ped det) and time the last ON ped det is OFF*/
	Monitr_ProcessPedDet();
	mova( 0, NULL );
#else
	status = NU_Task_Information(&mova_task_control_block[MOVA_TSK],
								&TaskName[8],
								&TaskStatus,
								&TaskCount,
								&priority, &preempt, &time_slice,
								&stack_base, &stack_size, &min_stack);
	if ( status != NU_SUCCESS )
	{
		SOFT_ERROR(MOVA_Check_Task)
	}

	if ( TaskStatus == NU_PURE_SUSPEND )
	{
		status = NU_Resume_Task ( &mova_task_control_block[MOVA_TSK] );
		if ( status != NU_SUCCESS ) 
			SOFT_ERROR(MOVA_Resume_Task); /* error handling */
	}
	else
		Mova_fail_count++;
#endif

S104:
	if (t < 9000) goto S112;                  /* is .25hr up ?*/
/*
---------- increment CELL and check TIMES(CELL) every quarter hour ----
*/
	t = 1;
	cell++;  /* TRL mod: use ++ operator, was cell = cell+1; */
   
#ifdef PARTTIME   
	if (Tcomshr->io[19] == 1) Tcomshr->io[16] = (char)((int)Tcomshr->io[16]-2);   
	if ((Tcomshr->io[19] == 1)&&(Tcomshr->io[16] <= 0)) Tcomshr->io[16] = 1;
	if ((Tcomshr->io[19] == 1)&&(Tcomshr->io[16] == 255)) Tcomshr->io[16] = 1;
	if ((Tcomshr->io[19] == 1)&&(Tcomshr->io[16] == 254)) Tcomshr->io[16] = 1;             
#endif

#if defined (PC_WRAPPER)
	/*  IRH MOD: 05/04/06: PCMOVA: BUGFIX: 10085. 
		Skip BST check - PCMOVA (and Siemens MOVA) no longer do advance/retard 
		clock from within the kernel (PCMOVA doesn't change the clock at all 
		- confusing in simulation).
		Note that in the embedded version this means the time of day data will 
		only get refreshed the next time midnight occurs after the clock goes 
		back/forwards, so any ToD changes occurring on the day of the clock 
		change itself will be an hour out (ToD changes on Sundays would be 
		highly unlikely anyway though). */
	if (cell == 105) goto S106;
#else
	if (cell == 105) goto S118;       /* check BST at 0200 SUN morning */
#endif

	/* allow October change again once clock past 0200 Sunday */
	if (cell > 105) oct = 0;
S106:
	if (cell > 672) cell = 1; /*cell is reset every 7 days (15 x 672)/60/24 = 7*/
	   
	/* IRH MOD: M5.0.0: 31/08/04: Get the plan for the current ToD
	 * from the new data handling module. This plan will
	 * be zero if there is no time-of-day data, or 1-4
	 * if there is time-of-day data. */
	nTimeOfDayPlan = DSH_get_tod_plan(cell - 1);
	nActivePlan = DSH_get_active_data_plan_number();
			  
	/* If there is a valid and new Time of Day plan */
	if ((nTimeOfDayPlan > 0) && (nTimeOfDayPlan != nActivePlan))
	{
		// Tcomshr->mark2 = nTimeOfDayPlan;
	}
	   
/*
------------------------------ reset times every midnight -------------
*/
	/* SIE/TRL
	if(fifi2mod(cell-1,96) == 0 || cell == 1) *//* M 2.0*/
	if ((cell-1) % 96 == 0 || cell == 1)
	{
		midnit = 1;                             /* M 2.0*/
		goto S56;                               /* M 2.0*/
	}
	goto S112;                                      /* M 2.0*/
S110:
	   
	/* IRH MOD: M5.0.0: 02/09/04: Now using ControlSync instead of
	 * DoutSync (see definition of ControlSync in Gbltypes.h) 
	 * This was causing the array to be written to out of bounds. */
	control[ControlSync] = 0;                      /* turn synch pulse off M 2.1 */

	OutputScan(SYNC_PULSE_OFF);  /* activate sync pulse */
S112:
/*
---------------------------------- not a MOVA scan --------------------
*/

	t++;                 /* Use increment operator = t+1;  next tenth sec */

S114:
/*
----------------------------------- no error condition ----------------
*/

#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
	/* Now waiting at beginning of update - see S100 */
#else
	status = NU_Suspend_Task ( &mova_task_control_block[MONITR_TSK] );
	if ( status != NU_SUCCESS ) 
		SOFT_ERROR ( MONITR_Suspend_Task );/* error handling */        
#endif
		
	if (Tcomshr->mark1 == 1234) goto S100;
	/* IRH MOD: M5.0.0: 10/09/04: S40 label removed */
	if (Tcomshr->mark1 == 8888) goto S56/*S40*/;
	if (Tcomshr->mark1 == 9999) goto S30;
  
#if defined (PC_WRAPPER)
	/* Should never get here - Tcomshr->mark1 should be one of the above. 
	Timing will get out of step if we loop back to S114 in PCMOVA.  */
	KDEBUG_WRITE0( DEBUG_LEVEL_ERROR, MONITR, "Unrecognised Tcomshr->mark1 in monitr()." );
#endif

/*
################################ end of main loop #####################
*/
	goto S114;                                      /* not for us : continue !?!?*/

#if !defined (PC_WRAPPER)
S118:
/*
 ======================================================================
*/
	/* fifidate(mm,dd,yy); */                     /* BST  routine*/
	/* TRL/SIE: time routine */
	tTimeDate = Com_read_rtc( READ_RTC);
	if (tTimeDate.month == 3L) goto S120;
	if (tTimeDate.month != 10L || oct != 0) goto S106;
	if (tTimeDate.date != (unsigned long)Tcomshr->io[15]) goto S106;
	/* i = -3600; SIE/TRL, subtract 1 hour */
	i = -1;
	oct = 1;
	goto S122;                                      /* OCTOBER - back one hour*/

S120:
	if (tTimeDate.date != (unsigned long)Tcomshr->io[14]) goto S106;
	/* i = 3600; SIE/TRL, add 1 hour*/                /* MARCH - forward one hour*/
	i = 1; /* MOVA-75: line added - where was 'i' setup otherwise? */
	oct = 0; /* allow October change again (helps when testing the unit) */
S122:

	/* TRL/SIE: update clock */
	tTimeDate.hours += (long)i;       
	/* SIE/TRL: uses Com_read_clock */
	tTimeDate = Com_read_rtc(SET_RTC, tTimeDate);
	goto S106;

#endif

S124:
/*
 eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee
 ERROR ROUTINE > 1 STAGE CONFIRMATION WHEN IO(31)=1 FROM DETS
						   IO(31) set in DETS only : scanned in MONI only
  IO(31)  0  1 0  0  1  1     1  0  1  1     0  0     check every .5 sec
  OLD31   0  1 0  0  1  2     2 -1  2  2 -1  0
					  gltch              off  gltch               on
*/
	if (Tcomshr-> io[30] == 0) goto S128;/* IO(31)=1 error now*/
	if (old31 == -1)
	{                        /* glitch went OFF once*/
		old31 = 2;
		goto S104;                              /* put back OFF     RETURN*/
	}
	if (old31 == 0)
	{                         /* just gone OFF*/
		old31 = 1;                              /* set OLD31 MARKER to 1*/
		goto S104;                              /* RETURN*/
	}
	Tcomshr->io[19] = 0;             /* M 2.4 reset off-control every scan*/
	   
	for (i = 1; i <= ForceArraySize; i++)
	{ /* M 2.4 TRL mod: was i<16 */
		control[i-1] = Terrlog->stgoff;/* clear outputs  TRL mod: control used*/ 
	}
	Switch_MOVA_Off(NO_SAV_MESSAGES, NO_REBOOT);
	if (old31 > 1) goto S104;         /* M 2.4*/
/*
--- OLD31=1 error has occured twice. set off-control, skip calling MOVA
*/
	old31 = 2;                                      /* skip calling MOVA*/
	   
	/* IRH MOD: M5.0.0: 27/08/04: Use non-cryptic error numbers */       
	nErrorID = ERROR_ID_MULTIPLE_STAGE_CONFIRMS;
	/*i = 24000;*/
	   
	goto S132;                                      /* mult STAGES 'ON'*/
S128:
/*
---------- multiple stage confirmation stopped - only 1 stg confrmn now
------------------------ here if IO(31)=0 and OLD31.NE.0 --------------
*/
	Tcomshr->io[18] = 19;            /* count up watchdog*/
	if (old31 < 1) goto S130;
	if (old31 == 1)
	{
		old31 = 0;                              /* glitch - put back ON*/
		goto S104;
	}
	old31 = -1;                                     /* Put OLD31=-1 and check 2 scans*/
	goto S104;
S130:
	old31 = 0;
	   
	/* IRH MOD: M5.0.0: 27/08/04: Use non-cryptic error numbers */       
	nErrorID = ERROR_ID_MOVA_RESTARTED;       
	/*i = 25000;*/                                      /* Mult STAGES OFF*/       
S132:
	if (Tcomshr->io[16] >= 20) goto S134;/* M 1.6 skip if too many errors*/

/* 26/4/00 Paul Cox Siemens - do not increment the error count on multiple stage
|  confirmations - it is clear to see on the commissioning screen that multiple
|  stage confirms are being received. Also, when the mains to a site fails, then
|  a battery supported OMU/MOVA unit will increment the error count when the
|  controller powers down, but will also increment it a second time when it
|  reboots when the mains power returns. Thus, decrementing when G1/G2 ceases
|  still leaves a net gain of one error count. */
#if 0
	/* if multiple stage confirms fault */
	if(i == 24000)
	{
		/* M 1.6 increment error count */
		Tcomshr->io[16]++;
	}
	/* if MOVA restarted after multiple stage confirms */
	else if( ( i == 25000 ) && ( Tcomshr->io[16] != 0 ) )
	{
		/* 9/3/00  PC  Decrement error count */
		Tcomshr->io[16]--;
	}
#endif

	/* IRH MOD: M5.0.0: 27/08/04: Use function to report error */
	Errhand_LogMOVAError(nErrorID, 0);

	/* IRH MOD: M5.0.0: 01/09/04: If the error count isn't enough to put MOVA off-control */
	if (Tcomshr->io[16] < 20)
	{
		/* Use function to get user-specified value (may be zero) */
		Tcomshr->io[16] += (signed char)Errhand_GetMOVAErrorValue( nErrorID );
		  
		/* If the error count is now too high */
		if ( Tcomshr->io[16] >= 20 )
		{
			Tcomshr->io[19] = 0; /* Reset ON-CONTROL flag */     
			Tcomshr->io[27] = 0; /* Reset MOVA/VA flag */ 
		}
	}
	
	   
S134:
	/* IRH MOD: M5.0.0: 31/08/04: Use non-cryptic error ID */
	if( nErrorID == ERROR_ID_MULTIPLE_STAGE_CONFIRMS )
	{
		goto S104;   
	}
	   
	/*if(i == 24000) goto S104;*/        /* Mult STAGES ON*/
	CI_set_warmup_counter(-1); /* Tcomshr->warmup = -1;*/
	CI_set_last_stage(mxstag); /* Tcomshr->lastag = mxstag;*/ /* TRL mod: */
	CI_set_current_stage(0);  /*Tcomshr->cusig = 0;*/
	CI_set_next_stage(mxstag);  /*Tcomshr->snext = mxstag;*/  /* TRL mod:  */
	CI_set_previous_marker(1); /*Tcomshr->premrk = 1;	*/

	goto S102;                                      /* call MOVA now*/
	 

} /* ============= End of Monitr()   ================= */



#ifndef M8_ENABLED 

/*************************************************************
 *
 * Function      : checks
 *
 * Author (Date) : Ian Henderson (14/07/2004)
 *
 * Description   : Calculates 'fixdat' and 'alldat' checksums
 *                 for MOVA 5 datasets held in the shared data
 *                 area (Ccomshr).
 *
 * Parameters    : Tcomshr [in] - pointer to dataset data
 *                 pnCheckSumFix [out] - variable for fixed data checksum
 *                  May be NULL if not required.
 *                 pnCheckSumAll [out] - variable for all data checksum
 *                  (i.e. for fixed AND variable dataset data)
 *                  May be NULL if not required.
 *
 * Return value  : void
 *
 * Side effects  : None
 *
 *************************************************************/
void checks(    Ccomshr const * const Tcomshr, 
				long * const pnCheckSumFix,
				long * const pnCheckSumAll )
{
	long nCheckSumFix = 0;
	long nCheckSumAll;
	int nLoop1, nLoop2;
			
	/* Calculate checksums */
			
	/* Stage data */
	for ( nLoop1 = 0; nLoop1 != mxstag; ++nLoop1 )
	{          
		  /* Fixed */
		  nCheckSumFix = nCheckSumFix + Tcomshr->min[ nLoop1 ] +
			   Tcomshr->max[ nLoop1 ] + Tcomshr->lowmax[ nLoop1 ] +
			   Tcomshr->pfacil[ nLoop1 ] + Tcomshr->emxmax[ nLoop1 ] +
			   Tcomshr->prxmax[ nLoop1 ] + Tcomshr->pedmax[ 0 ][ nLoop1 ] +
			   Tcomshr->pedmax[ 1 ][ nLoop1 ];

		  /*Added Mova 7.0 data*/
		  nCheckSumFix = nCheckSumFix + Tcomshr->occual[0][nLoop1]+ Tcomshr->occual[1][nLoop1]; //for OCCUAL


		  for ( nLoop2 = 0; nLoop2 != mxstag; ++nLoop2 )
		  {
				nCheckSumFix = nCheckSumFix + 
					Tcomshr->change[ nLoop1 ][ nLoop2 ];
		  }
		  
		  /*M8.0: fixed data elements*/
		  for ( nLoop2 = 0; nLoop2 != MAX_EP_DETS; ++nLoop2 )
		  {
			nCheckSumFix = nCheckSumFix + Tcomshr->busmax[nLoop2][nLoop1]; 
		  }

		  /* Variable */
		  /*nCheckSumVar = nCheckSumVar + Tcomshr->stadem[ nLoop1 ] +
			   Tcomshr->lambda[ nLoop1 ] + Tcomshr->stcycl[ nLoop1 ] +
			   Tcomshr->meritv[ nLoop1 ] + Tcomshr->drifmx[ nLoop1 ] +
			   Tcomshr->altlam[ nLoop1 ];*/
	}
	
	/* Link data */
	for ( nLoop1 = 0; nLoop1 != mxlink; ++nLoop1 )
	{          
		  /* Fixed */
			  nCheckSumFix = nCheckSumFix + Tcomshr->ltype[ nLoop1 ] +
					Tcomshr->shortl[ nLoop1 ] + Tcomshr->llatot[ nLoop1 ] +
					Tcomshr->nmainl[ nLoop1] + Tcomshr->lmin[ nLoop1 ] +
					Tcomshr->eslmax[ nLoop1 ] + Tcomshr->lostim[ nLoop1 ] +
					Tcomshr->stopen[ nLoop1 ] + Tcomshr->bontim[ nLoop1 ] +
					Tcomshr->boncut[ nLoop1 ] + Tcomshr->minpcs[ nLoop1 ] +
					Tcomshr->mixout[ nLoop1 ] + Tcomshr->lphase[ nLoop1 ] +
					Tcomshr->candet[ nLoop1 ] + Tcomshr->holdtm[ nLoop1 ] +
					Tcomshr->waitch[ nLoop1 ];
 
			  for ( nLoop2 = 0; nLoop2 != mxstag; ++nLoop2 )
			  {         
					nCheckSumFix = nCheckSumFix + 
						  Tcomshr->lgreen[ nLoop2 ][ nLoop1 ] +
						  Tcomshr->sdcode[ nLoop2 ][ nLoop1 ] + 
						  Tcomshr->pfacilm[ nLoop2 ][ nLoop1 ]; /*M8: Imporved Bus Priority*/
			  }

			  for ( nLoop2 = 0; nLoop2 != 3; ++nLoop2 )
			  {                             
					nCheckSumFix = nCheckSumFix + 
						  Tcomshr->llanes[ nLoop1 ][ nLoop2 ];
			  }
			  
			  for ( nLoop2 = 0; nLoop2 != 2; ++nLoop2 )
			  {                              
					nCheckSumFix = nCheckSumFix + 
						  Tcomshr->epdet[ nLoop1 ][ nLoop2 ] +
						  Tcomshr->epext[ nLoop1 ][ nLoop2 ];
			  }

			  /*M8.0: bus priority data elements*/
			  nCheckSumFix = nCheckSumFix + Tcomshr->buswgt[nLoop1];

			 for ( nLoop2 = 0; nLoop2 != MAX_EP_DETS; ++nLoop2 )
			 {
				 nCheckSumFix = nCheckSumFix + Tcomshr->epcrt[nLoop2][nLoop1]; 
			 }
			  
			  
			  /* Variable */
			  /*nCheckSumVar = nCheckSumVar + Tcomshr->cycltm[ nLoop1 ] +
					Tcomshr->lindem[ nLoop1 ] + Tcomshr->lingre[ nLoop1 ] +                  
					Tcomshr->yvalue[ nLoop1 ] + Tcomshr->lsmflo[ nLoop1 ] +                  
					Tcomshr->extrag[ nLoop1 ] + Tcomshr->prensa[ nLoop1 ];*/                
		}
  
		/* Lane data */
	for ( nLoop1 = 0; nLoop1 != mxlane; ++nLoop1 )
	{
		  /* Fixed */
			  nCheckSumFix = nCheckSumFix + Tcomshr->crusin[ nLoop1 ] +
					Tcomshr->crusx[ nLoop1 ] + Tcomshr->gamber[ nLoop1 ] +
					Tcomshr->stlost[ nLoop1] + Tcomshr->satinc[ nLoop1 ] +
					Tcomshr->satgap[ nLoop1 ] + Tcomshr->critg[ nLoop1 ] +
					Tcomshr->movqin[ nLoop1 ] + Tcomshr->moveqx[ nLoop1 ] +
					Tcomshr->qminon[ nLoop1 ] + Tcomshr->comtim[ nLoop1 ] +
					Tcomshr->osatcc[ nLoop1 ] + Tcomshr->xosat[ nLoop1 ] +
					Tcomshr->bonbc[ nLoop1 ] + Tcomshr->lanewf[ nLoop1 ] +
					Tcomshr->maxmin[ nLoop1 ] + Tcomshr->cspeed[ nLoop1 ] +
					Tcomshr->vlen[ nLoop1 ] + Tcomshr->din[ nLoop1 ] +
					Tcomshr->dx[ nLoop1 ] + Tcomshr->dshort[ nLoop1 ] +
					Tcomshr->osattm[ nLoop1 ];


			 /*Added Mova 7.0 data (USESF, USEST, RTSTOR, RTRAD, RTPROP, PCRITG, RCRITG, OSATCX, OSATAL, EXITAL)*/

			  nCheckSumFix = nCheckSumFix + Tcomshr->usesf[nLoop1]+ Tcomshr->usest[nLoop1]+Tcomshr->rtstor[nLoop1]+
				  Tcomshr->rtrad[nLoop1]+ Tcomshr->rtprop[nLoop1]+Tcomshr->pcritg[nLoop1]+
				  Tcomshr->rcritg[nLoop1]+ Tcomshr->osatcx[nLoop1]+Tcomshr->osatal[nLoop1]+Tcomshr->exital[nLoop1];
			  
			  /*Added Mova 7.0 data RTOPPO*/
			  /*For consistency using nLoop2*/
				for ( nLoop2 = 0; nLoop2 <3; ++nLoop2 )
						nCheckSumFix = nCheckSumFix +  Tcomshr->rtoppo[ nLoop1 ][ nLoop2 ];
			  

			  for ( nLoop2 = 0; nLoop2 != mxdetTypes; ++nLoop2 )
			  {                                
					nCheckSumFix = nCheckSumFix + 
						  Tcomshr->det[ nLoop1 ][ nLoop2 ];
			  }

			  for ( nLoop2 = 0; nLoop2 != 2; ++nLoop2 )
			  {  
					nCheckSumFix = nCheckSumFix + 
						  Tcomshr->assocd[ nLoop1 ][ nLoop2 ];
			  }
			  
			  /*M8.0: lanes data elemts*/
			  nCheckSumFix = nCheckSumFix + Tcomshr->ascepl[nLoop1];

			  /* Variable */
				/* nCheckSumVar = nCheckSumVar + Tcomshr->smflow[ nLoop1 ];*/
					  
	}
	
	/* Fixed-length array data */
	
	/* Fixed */
	for ( nLoop1 = 0; nLoop1 != 20; ++nLoop1 )
	{              
		  nCheckSumFix = nCheckSumFix + 
				Tcomshr->dattim[ nLoop1 ] + Tcomshr->datnum[ nLoop1 ];
	}
	for ( nLoop1 = 0; nLoop1 != 24; ++nLoop1 )
	{  
		  nCheckSumFix = nCheckSumFix + Tcomshr->asstim[ nLoop1 ];
	}      
	for ( nLoop1 = 0; nLoop1 != 16; ++nLoop1 )
	{              
		  nCheckSumFix = nCheckSumFix + Tcomshr->tel[ nLoop1 ];
	}

	/*Added Mova 7.0 data*/
	/*TMAPRD, SFTIMS, REDPED*/
	nCheckSumFix = nCheckSumFix+ Tcomshr->tmaprd + Tcomshr->redped[0] + Tcomshr->redped[1]; 
	for ( nLoop1 = 0; nLoop1 <6; ++nLoop1 )        
		 for ( nLoop2 = 0; nLoop2 < 2; ++nLoop2 )
			 nCheckSumFix = nCheckSumFix + Tcomshr->sftims[ nLoop1 ][ nLoop2 ];

	for ( nLoop1 = 0; nLoop1 <4; ++nLoop1 )    
		nCheckSumFix = nCheckSumFix + Tcomshr->dstrig[ nLoop1 ];
	/* Variable */
	for ( nLoop1 = 0; nLoop1 != ERROR_DATA_TYPES_NUM; ++nLoop1 )
	{            
		  for ( nLoop2 = 0; nLoop2 != xgnERRORS_NUM_MAX; ++nLoop2 )
		  {                   
				nCheckSumFix = nCheckSumFix + 
					Tcomshr->errval[ nLoop1 ][ nLoop2 ];
		  }
	}
	for ( nLoop1 = 0; nLoop1 != xgnSPARE_DATA_ROWS_NUM; ++nLoop1 )
	{            
		  for ( nLoop2 = 0; nLoop2 != xgnSPARE_DATA_COLS_NUM; ++nLoop2 )
		  {                   
				nCheckSumFix = nCheckSumFix + 
					Tcomshr->sparev[ nLoop1 ][ nLoop2 ];
		  }
	}                            

	
	/* Non-array data */
						   
	/* Fixed */
	/* N.B. Use detton, not deton (which will give the deton array address)
	 * In the Setup program detton *is* called deton though - beware of this. */
	nCheckSumFix = nCheckSumFix + Tcomshr->stages + Tcomshr->nlinks + Tcomshr->nlanes +
						  Tcomshr->ndets + Tcomshr->scan + Tcomshr->minext + Tcomshr->maxext +
						  Tcomshr->addgap + Tcomshr->subgap + Tcomshr->mainst + Tcomshr->totalg +
						  Tcomshr->optype + Tcomshr->detton + Tcomshr->stagon + Tcomshr->phason +
						  Tcomshr->stgdem;
 
		/* Variable */
	/*nCheckSumVar = nCheckSumVar + Tcomshr->lastag + Tcomshr->snext + Tcomshr->warmup +
					  Tcomshr->marker + Tcomshr->premrk + Tcomshr->junsmf + Tcomshr->smcycl +
					  Tcomshr->totlam + Tcomshr->mark1 + Tcomshr->mark2;*/

	/* Now calculate total checksum */

	/*CHECK: Commenting the following line until stop adding/loading the variable data to/from the .mds file */
	/* nCheckSumAll = nCheckSumFix + nCheckSumVar; */

	/*CHECK: the following line is a workaround until stop adding/loading the variable data to/from the .mds file */
	nCheckSumAll = (Tcomshr->chksum[0] * 1000L) + Tcomshr->chksum[1];

	
	if ( pnCheckSumFix != NULL )
	{
		*pnCheckSumFix = nCheckSumFix;
	}

	if ( pnCheckSumAll != NULL )
	{
		*pnCheckSumAll = nCheckSumAll;
	}
	
} /* void checks( Ccomshr const * const Tcomshr, ... ) */
#endif

void SetLED_State(char det_fault, char mova_fault)
{
   /* Set up the LED states from walamp states (ie Tdinout->dout[16 & 17])
	  The MOVA_LED_... variables are used in mdm_ctl_control_LEDs() */
	  
   MOVA_LED_OFF = FALSE;
   MOVA_LED_ON = FALSE;
   MOVA_LED_FLASH = FALSE;
   
   if ( det_fault == 0 && mova_fault == 0 ) /* MOVA off, no fault */
	  MOVA_LED_OFF = TRUE;
   else if ( det_fault == 0 && mova_fault == 1 )/* MOVA on, no fault */
	  MOVA_LED_ON = TRUE;
   else if ( det_fault == 1) /* fault */
	  MOVA_LED_FLASH = TRUE;
}
/*************************************************************************
 *
 * Function: Monitr_ProcessPedDet
 *
 * Description:	Function to record the start time when the first ped detectors is ON
 *				and the time when the last ped detectors is OFF. 
 *				Use this information to prepare for statistical calculation.
 *				Ultimately this information would be used to generate Ped Logs.				
 *
 * Author: PKale
 *
 * Date: 14/01/11
 *
 * Params:	void
 *
 * Return:	void
 *
 ************************************************************************/
// SLM FIXME - this function is never used anywhere
static void Monitr_ProcessPedDet()
{
	/*Variable used in for loops to loop over detectors*/
	static int detloop,detloop2;
	float  pedMean;
	extern int peddet[mxdets];
	time_t pedStartT,pedEndT;		

	/*This flag indicates that the mova has been successful in allocating dynamic memory for the variables used in TMA logs*/
	for(detloop2 = 1; detloop2 <= Tcomshr->ndets; detloop2++)
	{
		/*Enter only when the current detectors is set as the one responsible for all red ped detector*/
		if(peddet[detloop2-1]==999)
		{	
			/*Enter only if none of the ped detectors have been ON previously OR 
															the TMA time period/interval has changed and we need to find the first ON ped detector in this time period*/
			if (pTmaData->_currPeriod._Ped._recStarted != TRUE || pTmaData->_currPeriod._Ped._pedPending == TRUE)
			{
				/*If time ON is less then 10msec, the detector is not counted as ON*/
				if (Tcomshr->ton[detloop2-1] > 10)
				{
					/*Do not process in case where all ped detectors are served in a stage (all ped det are OFF)
					and in the same stage one of the ped detectors is activated (any one of ped det goes ON) */
					if(PedStageServed != Tcomshr->snow)
					{
						/*Clear any start/end time recorded during last phase of all red ped stage*/
						pTmaData->_currPeriod._Ped._timeDemanded = 0;
						pTmaData->_currPeriod._Ped._timeServiced = 0;

						/*pedPending indicates that ped detectors was ON during the previous period 
						and as a result the ON time should be added to the total time in the subsequent period*/
						if(pTmaData->_currPeriod._Ped._pedPending == TRUE)
						{
							/*This is the first all red ped det to go ON*/
							pTmaData->_currPeriod._Ped._timeDemanded = Com_read_rtc_timet()-pTmaData->_currPeriod._Ped._pedLastStgTime;
						
							/*Once the boundary case is catered for reset the flag which indicates the boundary case*/
							pTmaData->_currPeriod._Ped._pedPending = FALSE;
						}
						else
						{
							/*Most of the time the code comes here and records the det ON time*/
							pTmaData->_currPeriod._Ped._timeDemanded = Com_read_rtc_timet();
						}

						/*Flag to indicate that the first all red ped det have been encountered (Restrict other all red ped det to set the start time)*/
						pTmaData->_currPeriod._Ped._recStarted = TRUE;

					}/*end of - if(PedStageServed != Tcomshr->snow)*/					

				}/*end of  - if (Tcomshr->ton[detloop2-1] > 10)*/			

			}/*end of  - if (pTmaData->_currPeriod._Ped._recStarted != TRUE || pTmaData->_currPeriod._Ped._pedPending == TRUE)*/
			else
			{
				/*Initially assume that the All red ped detector are OFF*/
				pTmaData->_currPeriod._Ped._redEnded = TRUE;

				for(detloop = 1; detloop <= Tcomshr->ndets; detloop++)
				{
					/*Check if any of the all red ped detectors are on*/
					/*----------det on?--------------------Is det marked as all red ped det?----*/
					if(	Tcomshr->deton[detloop-1]== 1   &&    peddet[detloop-1]==999 )
					{
						/*One of red ped det is on. So mark that this is not the end of all red ped detector*/
						pTmaData->_currPeriod._Ped._redEnded = FALSE;

						/*Just one oN ped red detector is enough to NOT mark the end time of this detector as end of all red ped det
						so quit the loop*/
						detloop = Tcomshr->ndets;						

					}/*end of  - if(	Tcomshr->deton[detloop-1] == TRUE && peddet[detloop-1]==999 )*/

				}/*end of  - for(detloop = 1; detloop <= Tcomshr->ndets; detloop++)*/

				/*This will be true only when the all the red ped detectors are OFF*/
				if(pTmaData->_currPeriod._Ped._redEnded == TRUE)
				{	
					/*Mark the OFF time as the end time of all red ped detector*/	
					pedEndT = pTmaData->_currPeriod._Ped._timeServiced = Com_read_rtc_timet();						
			
					/*Identify the detector which triggered the all red ped demand (found by checking if time demanded is non zero)*/
					/* AK M7.01 removed if(... != 0) as it was lacking an else statement to set pedStartT otherwise */
					/*retrieve the start time*/
					pedStartT = pTmaData->_currPeriod._Ped._timeDemanded;

					/*Gives the total time the all red ped detectors were on*/
					pedMean = (float)(pedEndT - pedStartT);

					pTmaData->_currPeriod._Ped._timeTotPedStgP += pedMean;
					pTmaData->_currPeriod._Ped._timeSqrPedStgP += pedMean*pedMean;
					pTmaData->_currPeriod._Ped._pedStgRunP ++;

					/* In MovaSim and Vissim, ped detectors are simulated as vehicles. 
					As a result when all red ped stage get a green signal, the red ped detectors will go off,
					But if there is a queue of pedestrians then this detector will go ON/OFF for multiple times during this stage.*/

					/*Solution - record the stage when all red ped det served, and ignore subsequent detector triggers for this satge */
					PedStageServed = Tcomshr->snow;

					pTmaData->_currPeriod._Ped._timeDemanded = 0;
					pTmaData->_currPeriod._Ped._timeServiced = 0;

					pTmaData->_currPeriod._Ped._recStarted = FALSE;
					pTmaData->_currPeriod._Ped._redEnded = TRUE;
						
				}/*end of  - if(pTmaData->_currPeriod._Ped[j-1]._redEnded == TRUE)*/

			}/*end of - else part of for(detloop2 = 1; detloop2 <= Tcomshr->ndets; detloop2++)*/

		}/*end of  - if(peddet[detloop2-1]==999)*/

	}/*end of - for(detloop2 = 1; detloop2 <= Tcomshr->ndets; detloop2++)*/

}
