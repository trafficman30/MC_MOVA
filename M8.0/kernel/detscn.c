#include <nucleus.h>
#include <string.h>
#include <proj_def.h>
#include <nu_omu.h>
#include "gbltypes.h"
#include "obclock.h"
#include "error.h"    /* TRL/SIE: for SOFT_ERROR calls */
#include "errhand.h"  /* TRL/SIE: for MOVA error handler */
#include <hardware.h>
#include <keptdata.h>
#include <diff_bss.h>
#include "datahand.h"
#include "sf.h"
#include "tma_preprint.h"
#include "generalfunc.h"
#include "tma_alerts.h"
#include "tma_mem.h"
#include "kdebug.h"

#include "mova_constants.h"
#include "core_interface.h"
#include "dataset_interface.h"
#include "dataset_handler.h"


#if defined (PC_WRAPPER)
    #include "MVDebug.h"
    #include "MVTrace.h"

    #define g_MODULE_NAME ("Detscn")

    typedef enum
    {
        DETSCN_TASK_INIT_START,
        DETSCN_TASK_UPDATE_END

    } DETSCN_TASK;

    static DETSCN_TASK g_startPoint = DETSCN_TASK_INIT_START;
#endif

#if defined (PCMOVA)
	#include "../pcclock.h"
#endif

#ifdef M8_ENABLED
#include "spec_cond.h"
#endif

/* constants */
#define UNUSED_DETECTOR		(-999) /* IRH MOD: M5.0.0: 28/09/04: Added */
#define MINON				(1)
#define MINOFF				(2)
#define FIFTY_MINS			(30000)
#define FIVE_MINS			(3000)

/* extern variables */
Ccomshr Xcomshr;
Ccomshr *const Tcomshr  = &Xcomshr;

Cerrlog Xerrlog;
Cerrlog *const Terrlog  = &Xerrlog;

Cdinout Xdinout;
Cdinout *const Tdinout = &Xdinout;

char rawdetsin[mxdets] = {0};
char detsin[mxdets];
char detsin_0s[mxdets];
char detsin_1s[mxdets];
char confin[mxconf];

/* TMA data structure */
strcTMA tmaData;
strcTMA *const pTmaData  = &tmaData;

/* Time and date */
static TIMESTAMP_TYPE tTimeDate;
extern TIMESTAMP_TYPE Com_read_rtc(int, ... );

/* Extern vars */
extern char control[NumberOfForce+NumberOfExtra]; /*IA: moved from detscn() body*/

/*Vars that will be used in another components only (i.e., will not be used here) */
int peddet[mxdets]; /*array to identify detectors (set to 999) which are used to trigger all rep ped stages */



/* TODO: Vars to be removed*/
unsigned int MessageCount = 0;
CruiseSpeed CruiseStamps[100][2];
unsigned int CruiseIndex = 0;
/* End of vars to be removed*/


/*----------------Functions prototypes--------------------*/
static void init_start();
static BOOL handle_plan_changing( int * const pnPlanNum, int nStageNum ); /* IRH MOD: M5.0.0: 27/09/04: New function to check for and carry out a plan change */
static void init_detscn_vars(int *comb);
static BOOL check_stage_change_is_permitted();
static BOOL check_comming_from_intergreen();
static BOOL check_new_stage_first_time();
static BOOL check_new_stage_already_running();
static void update_links_greens_overlap();
static void update_links_greens_hard_wired();
static void handle_phone_home();
static void handle_error_count();
static void handle_watchdog();
static void finalize_detscn_cycle(BOOL isDoTimeChecking);
static void update_stage_data();
static void update_links_greens(const int stg);
static void update_tma_data(const int stg);
static BOOL check_error_count_time();
static BOOL check_watchdog_time();
static void reset_multiple_stages_confirm_data();
static BOOL check_multiple_stages_confirm();
static void update_detectors_status();
static BOOL check_new_dataset_loaded();
static void handle_faulty_detector(uint8 det_id);
static void handle_off_detector(int det_id);
static void handle_on_detector(uint8 det_id);

static int get_ioflow_value( int nIOFlowValueOld ); /* IRH MOD: M5.0.0: 28/09/04: Added */
static int set_offdet(int detno, int nlinks,int epdet[][2], int candet[], int ltype[], int waitch[], int dstrig[] );   /*Pankaj MOD: M6.0.3 8/8/07 added waitch[] in the function deceleration*/
static int set_ioflow(int nlinks, int detno, int nlanes, signed char det_[][mxdetTypes], int epdet[][2], int candet[], int ltype[],int waitch[], int ioflow_in[], int dstrig[] );
static int set_peddet(int detno, int nlinks, int ltype[], int waitch[], int redped[], signed char lgreen [][mxlink]);

#ifdef M8_RT_OPTIMISATION
static void update_opposing_link_net_gaps();
#endif

#ifdef M8_DET_FAULT_CHECKING_AND_LOGGING
static void check_chattering_detector(uint8 det_id);
static void	update_chattering_detector_timers_counter_and_status(uint8 detIdx);
#endif

/* Extern'ed functions */
extern void InputScan(void);


/*-------------- DETSCN local vars ---------------*/
/*Stages vars*/
static int newstg;
static int oldstg = 0;
static int savstg = 0;

/*Detectors vars*/
static int	is_comb_det[mxdets];
static int	tton[mxdets]; 
static int	offdet[mxdets];
static int	ioflow[mxdets]; 

/*Phases vars*/
static int	oldphs[6]; 
static int	savphs[6]; 
static int	onphs;
static int	numphs;

/* Counter and timers */
static int watchdog_timer  = -100;
static int wrong_stg_counter    = 0;
static int faulty_det_timer = 0;

/* Others vars*/
static int	was_on_control;
static char dataset_file_name[xgnFILE_NAME_STR_LEN];/* = "------------";*/


static uint32 net_gap_time = 0;
/* End of vars declaration */


#ifdef M8_DET_FAULT_CHECKING_AND_LOGGING

#define CHATTERING_DET_MAX_TIMER		(100)	/* 10 seconds */
#define CHATTERING_DET_MAX_COUNTER		(10)		/* If reached, the det will be consider as "chatter", but not recorded as dets.suspect_status=3 or logged.*/
#define CHATTERING_DET_MAX_OCCURRENCE	(3)		/* If the det was considered as "chatter" for this number, it should be recorded in the dets.suspect_status and logged as well.*/
#define CHATTERING_DET_OCCURRENCES_PERIOD	(9000) /*15 minutes (15*60*10) */		

static uint32	chattering_det_timer[MAX_DETS];  /* Max = CHATTERING_DET_MAX_TIMER*/
static uint32	chattering_det_counter[MAX_DETS]; /* Max = CHATTERING_DET_MAX_COUNTER */
static uint32	chattering_det_occurrence[MAX_DETS]; /*Max = CHATTERING_DET_MAX_OCCURRENCE*/
static uint32	chattering_det_occurrences_timer[MAX_DETS]; /*Max = CHATTERING_DET_OCCURRENCES_PERIOD*/
static mv_bool	chattering_det_hourly_check_flag[MAX_DETS] = {mv_false}; /*This flag is used to check if the flag was judged as chattering in the previous hour.*/
static int8		chattering_det_24_hour_counter[MAX_DETS]; /*counts 1 if the det was chattering for one hour*/
static TIMESTAMP_TYPE current_time;
#endif

void SetDetectorStatus(char* Status, int NumDetectors)
{
	int index = 0;

	if (NumDetectors > mxdets)
	{
		return;
	}
	for (index = 0; index < NumDetectors; index++)
	{
		detsin[index] = *Status++;
	}

}

void SetSingleDetectorStatus(uint8 DetIdx, MVBool state)
{
	if ((DetIdx > 0) && (DetIdx <= mxdets))
	{
		detsin[DetIdx - 1] = state;
	}
}


void GetRawDetectorsStatus(char * RawDetsStatus)
{
	int index = 0;

	for (index = 0; index < MAX_DETS; index++)
	{
		RawDetsStatus[index] = rawdetsin[index];
	}
}

void GetDetectorsStatus(char * DetsStatus)
{
	int index = 0;

	for (index = 0; index < MAX_DETS; index++)
	{
		DetsStatus[index] = detsin[index];
	}
}

MVBool GetSingleDetectorStatus(uint8 DetIdx)
{
	if ((DetIdx > 0) && (DetIdx <= mxdets))
	{
		return detsin[DetIdx - 1];
	}

	return MV_FALSE;
}

void GetConfirmationBits(char *ConfBits)
{
	int index;

	for ( index=0; index < NumberOfConfirms; index++ )
	{
		ConfBits[index] = confin[index];
	}
}

void SetConfirmStatus(char* Status, int NumConfirms)
{
	int index = 0;

	if (NumConfirms > mxconf)
	{
		return;
	}
	for (index = 0; index < NumConfirms; index++)
	{
		confin[index] = *Status++;
	}

}

/* TRL/SIE: void detscn(void) */
void detscn(UNSIGNED argc, void * argv)
{
	UNUSED(argc);
	UNUSED(argv);

	int i;

#if !defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
    STATUS status;
#endif

#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
	/* IRH MOD: PCMOVA: 07/12/05: Re-entry points for Detscn task. */
	switch ( g_startPoint )
	{
		case DETSCN_TASK_INIT_START: 
			init_start();
			break;

		case DETSCN_TASK_UPDATE_END:
			KDEBUG_WRITE0( DEBUG_LEVEL_VERBOSE, DETSCN, "Update end" );
			break;

		default:
			KDEBUG_WRITE0( DEBUG_LEVEL_VERBOSE, DETSCN, "Unrecognised reentry point for detscn" );
	}
#else
	status = NU_Suspend_Task ( &mova_task_control_block[DETSCN_TSK] ); 
	if ( status != NU_SUCCESS )
		SOFT_ERROR( DETSCN_Suspend_Task );

	init_start();
#endif

#if !defined (PC_WRAPPER)
	while(1)
	{ 
#endif
		if(check_new_dataset_loaded())
		{
			/* New dataset loaded */
		
			init_detscn_vars(is_comb_det);

			/* Update the local dataset name 'dataset_file_name' */
			strncpy( dataset_file_name, Tcomshr->fname, xgnFILE_NAME_STR_LEN );
			dataset_file_name[xgnFILE_NAME_STR_LEN - 1] = '\0'; /* Just in case! */	

			finalize_detscn_cycle(FALSE);
		}
		else /* No changing in the dataset */
		{
			update_detectors_status();

			if(check_multiple_stages_confirm())
			{
				reset_multiple_stages_confirm_data();
			}
			else /* No multi-stage problem */
			{
				Tcomshr->io[MULTI_STAGE_CONF] = 0;

				/* check if in intergreen */
				if(newstg == 0)
				{
					/* Start of intergreen */

					/* check if continuing the intergreen */
					if(oldstg == 0)
					{
						savstg = 0;
						update_links_greens_hard_wired(); /*From S94 to S102*/
					}
					else if(savstg == 0) /* check if in second scan */
					{
						/* It is not a continuing intergreen, however it is the second time DETSCN is called in it*/
						update_links_greens_overlap(); /*From S88 to S92 (without the last goto)*/
						update_links_greens_hard_wired(); /*From S94 to S102*/
					}
					else /* 1st scan*/
					{
						for(i=1; i<=numphs; i++)  
						{
							savphs[i-1] = confin[Tcomshr->stages+i-1];
						}

						savstg = newstg;
					}
				}
				else /* Not in intergreen */
				{
					if(check_new_stage_already_running() || check_new_stage_first_time())
					{
						savstg = newstg;
					}
					else if(check_comming_from_intergreen() || check_stage_change_is_permitted())
					{
						update_stage_data();
						update_links_greens(newstg);
					}
					else
					{
						wrong_stg_counter++; 
						if(wrong_stg_counter >= 5)  /* ignore 5 x 0.1 sec scans*/
						{
							update_stage_data();
							update_links_greens(newstg);
						}
					}
				}
			}
			finalize_detscn_cycle(TRUE);

#ifdef M8_SPEC_COND
			/* store detector status before processing rules */
			GetDetectorsStatus(rawdetsin);
			SC_ProcessAllRules();
#endif
		}

#if !defined (PC_WRAPPER)
		status = NU_Suspend_Task ( &mova_task_control_block[DETSCN_TSK] );
		if ( status != NU_SUCCESS )
			SOFT_ERROR( DETSCN_Suspend_Task );
	}
#endif

	return;
}

static void init_start()
{
	int j;

	KDEBUG_WRITE0( DEBUG_LEVEL_VERBOSE, DETSCN, "Initialise start." );

	tTimeDate = Com_read_rtc( READ_RTC); 

	Tcomshr->io[MULTI_STAGE_CONF] = 0;
	Tcomshr->io[WATCH_DOG] = 0;

	for(j=1; j<=mxstag+2; j++) 
	{
		control[j-1] = Terrlog->stgoff;/* clear force, HI, and TO  bits */
	}

#ifdef M8_IMPROVED_LINKING
	control[HOLD_SIGNAL_BIT] = Terrlog->stgoff;
	control[RELEASE_SIGNAL_BIT] = Terrlog->stgoff;
#endif	
	Switch_MOVA_Off(NO_SAV_MESSAGES, NO_REBOOT);

	Tdinout->dout[DoutDetFault] = 0;  /* walamp[0] */
	Tdinout->dout[DoutMovaFault] = 0; /* walamp[1] */
   
	MovaSatFlow_Initialise_(); /* Initialise saturation flow estimation module */
	
	net_gap_time = 0;

	return;
}

static BOOL check_new_dataset_loaded()
{
	return strncmp( Tcomshr->fname, dataset_file_name, xgnFILE_NAME_STR_LEN );
}

/*The data set has changed, so let's initialize some data elements */
static void init_detscn_vars(int *comb)
{
	int i;
	int j;
	int d;
   
	/* This loop is coming from the dataset_file_name if condition */
	for(d=1; d<=Tcomshr->ndets; d++)
	{
		offdet[d-1] = set_offdet(d, Tcomshr->nlinks, Tcomshr->epdet, Tcomshr->candet, Tcomshr->ltype,Tcomshr->waitch, Tcomshr->dstrig);
		ioflow[d-1] = set_ioflow(Tcomshr->nlinks, d, Tcomshr->nlanes,Tcomshr->det, Tcomshr->epdet, Tcomshr->candet, Tcomshr->ltype,Tcomshr->waitch, ioflow, Tcomshr->dstrig);
		peddet[d-1] = set_peddet(d, Tcomshr->nlinks, Tcomshr->ltype, Tcomshr->waitch, Tcomshr->redped, Tcomshr->lgreen);
	}
    /*End of the incoming statement*/

   onphs = Tcomshr->phason;
   Terrlog->stgoff = 0;

   if(Tcomshr->stgdem == 0) Terrlog->stgoff = 1;  
   
   numphs = 0;
   for(i=1; i<=mxlink; i++) 
   {
      j = Tcomshr->lphase[i-1];

      if(j > numphs)
		  numphs = j;
   }

   if(numphs > Tcomshr->stages) 
	   numphs = numphs-Tcomshr->stages;

   /* set up LANES with combination detectors into COMB(DET)=LANE */
   for(i=1; i<=mxlane; i++) 
   {
      if(Tcomshr->det[i-1][COMB_X_DET] == 0)
	  {
         *(comb + (int)Tcomshr->det[i-1][COMB_X_DET] -1) = 0;
	  }
      else
	  {
         *(comb + (int)Tcomshr->det[i-1][COMB_X_DET] -1) = i;
	  }
   }
}


static void update_detectors_status()
{
	uint8 i = 0;
	uint8 j = 0;
	int comb_det = 0;

	faulty_det_timer++;   /* when = 999 [100 sec] check faulty dets*/

	for (j = 1; j <= Tcomshr->ndets; j++)
	{

#ifdef M8_DET_FAULT_CHECKING_AND_LOGGING
		update_chattering_detector_timers_counter_and_status((uint8)(j-1));
#endif

		if (ioflow[j-1] == UNUSED_DETECTOR)
			continue;

		/* Check for combination detectors */
		if (is_comb_det[j-1] == 0)
		{
			/* It is NOT a combination detector */
			if (detsin[j-1] == Tcomshr->detton)
			{
				handle_on_detector(j);
				continue;
			}
			else
			{
				handle_off_detector(j);
				continue;
			}
		}
		else /* It is a combination detector */
		{
			comb_det = is_comb_det[j-1];

			/* Checking X-DET of COMB-DET*/
			if (detsin[Tcomshr->det[comb_det-1][1]-1] == Tcomshr->detton)
			{
				handle_on_detector(j);
				continue;
			}

			/* Checking ASSOCD-1 of COMB-DET*/
			if (detsin[Tcomshr->assocd[comb_det-1][0]-1] == Tcomshr->detton)
			{
				handle_on_detector(j);
				continue;
			}

			/* Checking ASSOCD-2 of COMB-DET existence */
			if (Tcomshr->assocd[comb_det-1][1] == 0)
			{
				handle_off_detector(j);
				continue;
			}

			/* Checking ASSOCD-2 of COMB-DET status */
			if (detsin[Tcomshr->assocd[comb_det-1][1]-1] == Tcomshr->detton)
			{
				handle_on_detector(j);
				continue;
			}

			handle_off_detector(j);
		}
	}

#ifdef M8_RT_OPTIMISATION
	update_opposing_link_net_gaps();
#endif
	
	/* Check if the faulty detectors checking is due */
	if (faulty_det_timer >= 999)
	{
		for (i = 1; i <= Tcomshr->ndets; i++)
		{
			/* Check if the detector is missing or not used */
			if (ioflow[i-1] == UNUSED_DETECTOR)
				continue;

			/* Check if the detector already marked faulty */
			if (ioflow[i-1] < 0)
			{
				/* reset faulty marker */
				ioflow[i-1] = -960;
				continue;
			}

			/* Check if the detector OK (32 refers to number of 45 min periods, so det on/off continuously for < 24 hrs) */
			if (ioflow[i-1] < 32)
				continue;

			/* left with the detectors that have just reached 24 hrs continuous on/off */
			handle_faulty_detector(i);
		}

		faulty_det_timer = 0;   
	}

	return;
}

/* This function is called to update the ON times when the detector is ON*/
static void handle_on_detector(uint8 det_id)
{

	/* Just to adjust the for the array index */
	det_id--;

	Tcomshr->deton[det_id] = 1;
	Tcomshr->ton[det_id]++;

	/* If ON for more than 50 mins, reset to 5 mins, and increment count of 45 min periods we have counted back */
	if (Tcomshr->ton[det_id] >= FIFTY_MINS)
	{
		Tcomshr->ton[det_id] = FIVE_MINS;
		ioflow[det_id]++;
	}

	/* If the det was previously off but is now on */
	if (Tcomshr->detoff[det_id] == 1)
	{
		Tcomshr->detoff[det_id] = 0;

#ifdef M8_DET_FAULT_CHECKING_AND_LOGGING
		check_chattering_detector(det_id);
#endif

		if (Tcomshr->toff[det_id] < MINOFF)
		{
			/* det was off for an unusually short period - treat as an error and assume continuous on */
			Tcomshr->ton[det_id] += Tcomshr->toff[det_id];
			Tcomshr->toff[det_id] = 0;
			ioflow[det_id] = 0;

			goto TempLbl;
		}
	}
	
	if (Tcomshr->ton[det_id] == MINON)
	{
		Tcomshr->pregap[det_id] = Tcomshr->toff[det_id];
		Tcomshr->toff[det_id] = 0;
		ioflow[det_id] = 0;
	}

TempLbl:
	tton[det_id]++;
			
	pTmaData->_currPeriod._detOnTime[det_id]++;
	pTmaData->sAlert._detOnTime[det_id]++;

	if (tton[det_id] < 0)
		tton[det_id] = MINON;
}

/* This function is called to update the OFF times when the detector is OFF*/
static void handle_off_detector(int det_id)
{
	/* Just to adjust the for the array index */
	det_id--;

	Tcomshr->detoff[det_id] = 1;
	Tcomshr->toff[det_id]++;

	/* If off for >50 mins, reset to 5 mins */
	if (Tcomshr->toff[det_id] >= FIFTY_MINS)
	{
		Tcomshr->toff[det_id] = FIVE_MINS;

		/* increment to count of 45min (50-5mins) 'off' periods for vehicle detectors (offdet==0) */
		if (offdet[det_id] == 0)
			ioflow[det_id]++;
	}

	/* Check if the detector was on but is now off */
	if (Tcomshr->deton[det_id] == 1)
	{
		Tcomshr->deton[det_id] = 0;

		/* Check if det was on for unusually short period - treat as an error and assume it was off */
		if (Tcomshr->ton[det_id] < MINON)
		{
			Tcomshr->toff[det_id] += Tcomshr->ton[det_id];
			Tcomshr->ton[det_id] = 0;
			ioflow[det_id] = 0;
			return;
		}
	}

	if (Tcomshr->toff[det_id] == MINOFF)
	{
		/* count vehicle */
		Tcomshr->vc[det_id]++;
		if (Tcomshr->vc[det_id] < 0)
			Tcomshr->vc[det_id] = 0;

		/* Counting flows for TMA and occupancy alert */
		pTmaData->_currPeriod._detFlow[det_id]++;
		pTmaData->sAlert._detFlow[det_id]++;
	}
	else
	{
		return;
	}

	Tcomshr->ton[det_id] = 0;
	ioflow[det_id] = 0;
}

static void handle_faulty_detector(uint8 detID)
{
	ioflow[detID-1] = -960;    /* set det faulty*/
	Tdinout->dout[DoutDetFault] = 1; /* set warning LED - walamp[0] */

	Errhand_LogMOVAError( ERROR_ID_FAULTY_DETECTOR, ( detID * 100 ) );
	
#ifdef M8_DET_FAULT_CHECKING_AND_LOGGING

	if (CI_get_detector_suspicion_status(detID-1) == DET_FAULT_STATUS_SUSPECTED_DUE_TO_LONG_ON)
	{
		CI_set_detector_suspicion_status(detID-1, DET_FAULT_STATUS_SUSPECTED_DUE_TO_24_HOURS_ON);
	}
	else if (CI_get_detector_suspicion_status(detID-1) == DET_FAULT_STATUS_SUSPECTED_DUE_TO_LONG_OFF)
	{
		CI_set_detector_suspicion_status(detID-1, DET_FAULT_STATUS_SUSPECTED_DUE_TO_24_HOURS_OFF);
	}
	else if (CI_get_detector_suspicion_status(detID - 1) == DET_FAULT_STATUS_SUSPECTED_PED_LINK)
	{
		CI_set_detector_suspicion_status(detID - 1, DET_FAULT_STATUS_SUSPECTED_DUE_TO_24_HOURS_PED_LINK_FAULT);
	}

#endif
	
	/* Check if the error count isn't enough to put MOVA off-control */
	if (Tcomshr->io[ERROR_COUNT] < 20)
	{ 
		/* Use function to get user-specified value (may be zero) */
		Tcomshr->io[ERROR_COUNT] += (signed char)Errhand_GetMOVAErrorValue( ERROR_ID_FAULTY_DETECTOR );
          
		/* If the error count is now too high */
		if (Tcomshr->io[ERROR_COUNT] >= 20)
		{
			Tcomshr->io[ON_CONTROL] = 0; /* Reset ON-CONTROL flag */     
			Tcomshr->io[MOVA_ON] = 0;	 /* Reset MOVA/VA flag */             
		}
	}

	Tcomshr->io[PHONE_HOME] = 99; /* Phone home */
}

/* Check for multiple stage confirmations: Set "newstg" to first stage conf.
found. If a second stage confirm is found (ie confin[n-1]==stagon and
"newstg" != 0) there is more than one confirmation found. Shut down MOVA  */
/* Multi-stage ==returns==> TRUE */
/* No Multi-stage ==returns==> FALSE */
static BOOL check_multiple_stages_confirm()
{
	int n;

	newstg = 0;

	for(n=1; n<=Tcomshr->stages; n++) 
	{   
		if(confin[n-1] == Tcomshr->stagon) 
		{ 
			if(newstg != 0)  
			{
				/* Multi confs found */
				return TRUE;
			}
			newstg = n;                         /* 1ST STAGE confirmation */
	  }
	}

	return FALSE;
}

/* Mainly reset GREENS and SNOW in case of multi-stage confirmation error occurred */
static void reset_multiple_stages_confirm_data()
{
	int j; /* links loop variable */

	int nStageOff;
         
	Tcomshr->io[MULTI_STAGE_CONF] = 1;
	oldstg = 0;
	Tcomshr->snow = 0;
	savstg = 0;

	/* Clear green confs */
	for(j=1; j<=Tcomshr->nlinks; j++) 
	{
		Tcomshr->greens[j-1] = 0;
	}
          
	/* If at least one stage is OFF, assume inputs are genuine */
	nStageOff = ( Tcomshr->stagon == 1 ? 0 : 1 );

	if ( memchr( confin, nStageOff, sizeof( confin ) ) != NULL )
	{  
		handle_plan_changing( &( Tcomshr->mark2 ), newstg );      
	}          
	else /* Else ALL stages are on at the same time */
	{
		/* Inputs probably not connected, so don't process
		* any plan change requests */
	}

	newstg = 0;
}

static BOOL check_watchdog_time()
{
	watchdog_timer++;
	return (watchdog_timer > 100);
}

static BOOL check_error_count_time()
{
	static int is_checking_due = 0; /*iprtfg*/
	static int delay_fifty_sec = 0; /*ifive*/

	tTimeDate = Com_read_rtc( READ_RTC);

	if(tTimeDate.minutes == 0) /* If an hour has passed */
		is_checking_due = 2;

	/* To check every 50 seconds */
	delay_fifty_sec++;
	if(delay_fifty_sec < 5)
		return FALSE;
	else
		delay_fifty_sec = 0;
	/* End of the 50 seconds checking */

	if(is_checking_due == 0)
		return FALSE;

	is_checking_due--;
	
	if(is_checking_due == 0)
		return TRUE;
	else
		return FALSE;
}


static void update_tma_data(const int stg)
{
	if (stg > 0)
	{
		pTmaData->_currPeriod._llStage[stg-1]._timeEnded = Com_read_rtc_timet();
		pTmaData->_currPeriod._llStage[stg-1]._trafStgRun++;

	}		
	/* not intergreen, just for security and there is at least one stage that has finished,  lastag =MaxNumber of Stages */
	if (CI_get_last_stage() /*Tcomshr->lastag*/ > 0 && CI_get_last_stage() /*Tcomshr->lastag*/ <= mxstag)
	{
		pTmaData->_currPeriod._llStage[CI_get_last_stage() /*Tcomshr->lastag*/ -1]._timeTotTrafStg  +=
			Com_read_rtc_timet() - (pTmaData->_currPeriod._llStage[CI_get_last_stage() /*Tcomshr->lastag*/-1]._timeEnded );
	}
}

static void update_stage_data()
{
	oldstg = newstg;
	Tcomshr->snow = newstg;

	update_tma_data(newstg);

	/* Check if time for site data to be reloaded */

	/*Minor Change, change data plan based on detector activation*/
	detGetActivePlan(&(Tcomshr->mark2), &Tcomshr->dstrig[0], &Tcomshr->deton[0],flagDstrig);
	
	/* IRH MOD: M5.0.0: 27/09/04: Check for and carry out a plan change if requested and the time is right */
	handle_plan_changing( &( Tcomshr->mark2 ), Tcomshr->snow );
}

static void update_links_greens(const int stg)
{
	int i;

	for(i=1; i<=Tcomshr->nlinks; i++) 
	{
		Tcomshr->greens[i-1] = 0;

		if(Tcomshr->lgreen[stg-1][i-1] != 0)
		{
			Tcomshr->greens[i-1] = 1;
		}
    }

	wrong_stg_counter = 0;
    
	for(i=1; i<=numphs; i++) 
	{
		oldphs[i-1] = -1;
    }
}



/*If checking the time is not necessary, set it with SKIP_TIME_CHECK */
static void finalize_detscn_cycle(BOOL isDoTimeChecking)
{
	if(isDoTimeChecking)
	{
		if(check_watchdog_time()) /*TRUE every 10 secs */
		{
			handle_watchdog();

			if(check_error_count_time()) /*TRUE every 1 hour */
			{
				handle_error_count();
			}
			else
			{
				handle_phone_home();
			}
		}
	}

	InputScan();
    
	/* Call new Sat flow module if warm up has finished */
	if(CI_get_warmup_counter() > Tcomshr->stages) 
	{
		MovaSatFlow_Update_();
	}

#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
	g_startPoint = DETSCN_TASK_UPDATE_END;
#endif
	return;
}

/* THE SECOND PART */

static void handle_watchdog()
{
	int i;

	watchdog_timer = 0;    

	KDEBUG_WRITE1( DEBUG_LEVEL_VERBOSE, DETSCN, "Checking watchdog is >= 19. Watchdog = %d", Tcomshr->io[WATCH_DOG] );

	if (Tcomshr->io[WATCH_DOG] < 19 && Tcomshr->io[MULTI_STAGE_CONF] == 0 && Tcomshr->io[ERROR_COUNT] < 20) 
	{
		Tcomshr->io[ERROR_COUNT] = (char)((int)Tcomshr->io[ERROR_COUNT] + Errhand_GetMOVAErrorValue(ERROR_ID_WATCHDOG_ROUTINE));    
		Tcomshr->io[ON_CONTROL] = 0;
   
		Errhand_LogMOVAError( ERROR_ID_WATCHDOG_ROUTINE, ( Tcomshr->io[ERROR_COUNT] ) );

		for (i = 1; i <= mxstag+2; i++) 
		{
			control[i-1] = Terrlog->stgoff;/* clear force, HI, & TO bits*/
		}

		Switch_MOVA_Off(SAV_MESSAGES, NO_REBOOT);
	}

	Tcomshr->io[WATCH_DOG] = 0;
}

static void handle_error_count()
{
	if (Tcomshr->io[ERROR_COUNT] >= 20)
	{
		Tcomshr->io[ON_CONTROL] = 0;      
		Tcomshr->io[MOVA_ON] = 0;
	}
	else
	{
		if(Tcomshr->io[ERROR_COUNT] <= 0) 
			Tcomshr->io[ERROR_COUNT] = 1;

		Tcomshr->io[ERROR_COUNT] = (int)Tcomshr->io[ERROR_COUNT]-1; /* decrement error count*/
	
		if (Tcomshr->io[ON_CONTROL] != 1 && Tcomshr->io[MOVA_ON] == 1 && Tcomshr->io[MULTI_STAGE_CONF] == 0) 
		{
			tTimeDate = Com_read_rtc( READ_RTC);
			
			if (CI_get_warmup_counter() /*Tcomshr->warmup*/ > Tcomshr->stages) 
				CI_set_warmup_counter(Tcomshr->stages); /*Tcomshr->warmup = */

			Errhand_LogMOVAError( ERROR_ID_MOVA_BACK_ON_LINE, ( Tcomshr->io[ERROR_COUNT] ) );
 		}
	}
}

static void handle_phone_home()
{
	if (Tcomshr->io[ON_CONTROL] == 1 || was_on_control == 0)
	{
		was_on_control = Tcomshr->io[ON_CONTROL];
		return;
	}

	if (Tcomshr->io[MOVA_ON] == 0 || Tcomshr->io[ERROR_COUNT] < 20)
	{
		was_on_control = Tcomshr->io[ON_CONTROL];
		return;
	}

	/* call home if off-control,was on-control,MOVA bit set,error count 20+*/
	Tcomshr->io[PHONE_HOME] = 99;

	if (Tcomshr->io[ERROR_COUNT] >= 20) 
	{
		Tcomshr->io[MOVA_ON] = 0;
	}
}


static void update_links_greens_hard_wired()
{
	int i;
	int l;
	int n;

	/* continue intergreen */
	
	savstg = 0;

	/* Loop over all the phases in the junction */
	for(i=1; i<=numphs; i++)
	{

		if (confin[Tcomshr->stages+i-1] == oldphs[i-1])
			continue;

		if (confin[Tcomshr->stages+i-1] != savphs[i-1])
		{
			savphs[i-1] = confin[Tcomshr->stages+i-1];
			continue;
		}

		oldphs[i-1] = confin[Tcomshr->stages+i-1];
		
		/* Now ONLY dealing	with hard-wired phase confirms here */
		for (l = 0; l != Tcomshr->nlinks; ++l )
		{
			/* 'N' is no of confirmation channel for phase controlling link L */
			n = Tcomshr->lphase[l];

			/* If there is a wired-in phase confirm for the link */
			if( n > 0 )
			{
				/* If the phase confirm is on */
				if ( confin[n-1] == onphs )
				{
					/* Set GREENS for specified (wired-in) phase confirm */
					Tcomshr->greens[l] = 1;
				}
				else
				{
					Tcomshr->greens[l] = 0;
				}
			}
		}
	}
}

static void update_links_greens_overlap()
{
	int l;

	/*S88:*/

	/*
	 iiiiiiiiiiiiiiiiiiiiiiiiiii GOOD INTERGREEN ROUTINE iiiiiiiiiiiiiiiiii
	*/
	/*
	--------------------------------- clear GREENS and calc overlap phases
	*/

    /*  IRH MOD: M5.0.0: 04/09/03 MOVA Maintenance MON_026: if all our LPHASE
        values are -1, numphs will be 0 in the loop beginning S96
        (as we're not relying on any hard-wired phase confirms) so we
        don't get to the 'for' loop after S96. Therefore we need
        to deal with -1 LPHASE's separately beforehand. Best to do
        it here, just at the start of the intergreen.
        This loop replaces a 'for' loop that set GREENS to 0 unless
        LPHASE was non-zero, which incorporated S90. */
	for (l = 0; l != Tcomshr->nlinks; ++l)
	{
		/* If this link has a calculated overlap phase
			and is green in the preceding and following stages */
		if (Tcomshr->lphase[l] == -1
			&& Tcomshr->io[ON_CONTROL] == 1 /* MOVA on-control */
			&& Tcomshr->lgreen[oldstg-1][l] != 0
			&& Tcomshr->lgreen[ CI_get_demanded_stage() /*Tcomshr->demsta*/-1][l] != 0 )
		{
			/* Flag this link as green */
			Tcomshr->greens[l] = 1;
		}
		else
		{
			/* Link is not green (as far as we know -
				it might be if there's a hard-wired phase
				overlap, in which case we'll pick it up in
				the for loop below S96) */
			Tcomshr->greens[l] = 0;
		}
	} /* For each link */

	/* Set current stage number to zero for intergreen */
	Tcomshr->snow = 0;
	oldstg = 0;
}

static BOOL check_new_stage_already_running() 
{
	return newstg == oldstg;
}

static BOOL check_new_stage_first_time()
{
	return newstg != savstg;
}

static BOOL check_comming_from_intergreen()
{
	return oldstg == 0;
}

static BOOL check_stage_change_is_permitted()
{
	return Tcomshr->change[oldstg-1][newstg-1] != -1;
}




static int set_offdet(int detno, int nlinks,int epdet[][2], int candet[], int ltype[],int waitch[],int  dstrig[] )
{
	/*
	Set offdet to 1 to flag detectors not requiring long-off (24hr) check:
	   These are:
		   Simple latched ped dets
		   Unlatched ped (det + 100)
		   Emergency and priority dets
	*/
	int Offdet=0;
	int i;
	for (i = 1; i <= nlinks; i++ )
	{
		if (detno == ltype[i-1]) 
		{
			/* Is simple latched ped det */
			Offdet = 1;
		}
		else if (ltype[i-1] -100 == detno) 
		{
			/* Is latched ped det */
			Offdet = 1;
		}
		else if (detno == waitch[i-1])
		{
			/*Is a waitch Channel*/
			Offdet = 1;
		}									

	  /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials, if ltype[i-1] < 0 is emergency/priority link M4 */
#if defined (CMOVA_EP) 

		else if (ltype[i-1] < 0) 
		{
			if (epdet[i-1][0] == detno || epdet[i-1][1] == detno || candet[i-1]%100 == detno)
				Offdet = 1;
		}

#endif /* CMOVA_EP */ /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */

	}
	
	/* Modified:- data set trigger by detector activation*/
	for (i = 0; i < xgnMAX_DATASETS; i++)
	{
		if (dstrig[i] == detno )
			Offdet = 1;
	}
	
	/* End of modification  */

	return Offdet;
}


static int set_ioflow(int nlinks, int detno, int nlanes, signed char det_[][mxdetTypes], int epdet[][2],  int candet[], int ltype[], int waitch[], int ioflow_in[], int  dstrig[] )
{
	/* function to return 0 or -999 to calling program depending on whether
	   a detector is found or not (ie -999 implies `missing' detector)   
	*/

	int m, l, j, Ioflow;

	for (m = 1; m <= nlanes; m++) 
	{
		for (j = 1; j <= mxdetTypes; j++) 
		{      
		  /*    IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: check for -ve IN-dets 
				that we're using just for calculating occupancy */ 
            
		   /* If we found a detector (positive or negative) */            
			if (det_[m-1][j-1] == detno || 
				det_[m-1][j-1] == -detno) 
			{
				/* IRH MOD: M5.0.0: 28/09/04: MOVA 5: Retain current IOFlow value if
				 * detector existed before we changed plans */
				/*Ioflow = 0;*/
				Ioflow = get_ioflow_value( ioflow_in[detno-1] );          
				return Ioflow;
			}
		}
	}

	/*
	 *** Det D check against all lane vehicle dets;
	 *** Now check other link (L) det Nos
	 ***   Normal ped dets are given by      LTYPE=(det No)
	 ***   Unlatched pedestrian dets have    LTYPE=(det No + 100)
	 ***    (NB   This method of identification of unlatched ped dets differ
	 ***          from original M2.10 method using det Nos. >20 as unlatched
	 ***   Emerg/pr links have dets in       EPDET and CANDET
	*/
	for (l = 1; l <= nlinks; l++)
	{
		/* Check all LTYPE(link) for dets:  */

		if (ltype[l-1] == detno) 
		{
			/*    THEN, is simple ped det no in LTYPE=D.  */
		  
			/* IRH MOD: M5.0.0: 28/09/04: MOVA 5: Retain current IOFlow value if
			 * detector existed before we changed plans */
		  
			Ioflow = get_ioflow_value( ioflow_in[detno-1] );
			return Ioflow;

		}
	  
		/*    Check for links with unlatched ped dets.  */	  
		else if(ltype[l-1] == detno+100) 
		{		  
			/*  THEN, is unlatched ped det link; uses detector K=LTYPE-100
				Thus no. of unlatched ped det on link is = D
			*/
		  
			/* IRH MOD: M5.0.0: 28/09/04: MOVA 5: Retain current IOFlow value if
			 * detector existed before we changed plans */
		  
			Ioflow = get_ioflow_value( ioflow_in[detno-1] );
			return Ioflow;
		}
	  
		/*    Check for three-channel PUFFIN detectors  */  

		else if (detno == ltype[l-1] - 200 || detno == ltype[l-1] - 199 || detno == ltype[l-1] - 198 ) 
		{
			/*  THEN is one of the three channels defined for Vol sensitive PUFFIN
				detectors, where ltype=200 + d defines the first of three successive
				channels
			*/

			/* IRH MOD: M5.0.0: 28/09/04: MOVA 5: Retain current IOFlow value if
			 * detector existed before we changed plans */
		  
			Ioflow = get_ioflow_value( ioflow_in[detno-1] );
			return Ioflow;
		}
		/*Pankaj MOD M6.0.3 8/8/07 added an extra else if statement to get the ioflow value for waith channel as well */
		else if (detno == waitch[l-1])
		{
			Ioflow = get_ioflow_value( ioflow_in[detno-1] );
			return Ioflow;
		}	

#if defined (CMOVA_EP) /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */
		/*    M 4.0 check for emerg/pr links   THEN, is emergency/priority link  M 4  */  
		else if (ltype[l-1] < 0)
		{
			if (epdet[l-1][0] == detno || epdet[l-1][1] == detno || (candet[l-1]%100) == detno) 
			{     
				Ioflow = get_ioflow_value( ioflow_in[detno-1] );/* IRH MOD: M5.0.0: 28/09/04: MOVA 5: Retain current IOFlow value if detector existed before we changed plans */  
				return Ioflow;
			}
		}
#endif /* CMOVA_EP */ /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */

	}
   
	/*Modified, Minor Change 1.16,to follow the pattern of this function, I have used a goto (no my preference), 
				There is not need to record any flow   */
	for (j = 0; j < xgnMAX_DATASETS; j++)
	{
		if (detno == dstrig[j])
		{
			Ioflow = 1; /*so it is not set as a faulty detector*/
			return Ioflow;
		}
	}
	/* End of modification, Minor Change 1.16 3/Sep/2010  */ 
   
	/*
	***   If falls through all checks, detector is not referenced:
	***   so, flag unused detectors.
	*/

	Ioflow = UNUSED_DETECTOR;
	return Ioflow;

////Exit:
	////return Ioflow;
}


/*************************************************************
 *
 * Function      : get_ioflow_value
 *
 * Author (Date) : Ian Henderson (28/09/2004)
 *
 * Description   : Returns the IO flow value to use for a 
 *                 particular detector that exists
 *                 in the current plan.
 *                 Should be called after plan change to
 *                 update the ioflow[] array.
 *                 
 * Parameters    : nIOFlowValueOld [in] - old IOFlow
 *                  value from previous plan
 *
 * Return value  : nIOFlowValueNew - value to set IOFlow array
 *                  entry to for the given detector
 *
 * Side effects  : None
 *
 *************************************************************/
static int get_ioflow_value( int nIOFlowValueOld )
{
	int nIOFlowValueNew;

	/* If detector existed in last plan */
	if ( nIOFlowValueOld != UNUSED_DETECTOR )
	{
		/* Retain current value (whatever that is) */
		nIOFlowValueNew = nIOFlowValueOld;
	}
          
	/* Else the detector is new for this plan */          
	else
	{
		nIOFlowValueNew = 0; 
	}
    
	return ( nIOFlowValueNew );
 
} /* static int get_ioflow_value( ... ) */


/*************************************************************
 *
 * Function      : handle_plan_changing
 *
 * Author (Date) : Ian Henderson (27/09/2004)
 *
 * Description   : Checks whether a plan change request exists
 *                 and whether the time is right to carry it
 *                 out.  If so, the plan change is carried out.
 *                 Should be called at *start* of stage 1.
 *
 * Parameters    : pnPlanNum [in/out] - the requested plan
 *                  (0 if no request). If a plan change
 *                  is attempted, this will be reset to 0.
 *                 nStageNum [in] - the current stage
 *                  Plan changes can only occur at the start of stage 1
 *
 * Return value  : boSuccess - indicates whether a 
 *                  plan change was carried out successfully
 *
 * Side effects  : See Datahand_LoadDataset.
 *
 *************************************************************/
static BOOL handle_plan_changing( int * const pnPlanNum, int nStageNum )
{
	BOOL boIsPlanChange, boSuccess;
	 
	boIsPlanChange = ( ( *pnPlanNum >= xgnPLAN_NUM_MIN ) 
					&& ( *pnPlanNum <= xgnPLAN_NUM_MAX ) );
                    
	/* If there is a request, we're in stage 1, and not 
	 * downloading a new dataset */
	if (boIsPlanChange &&  (nStageNum == 1) && (!DSH_is_loading_dataset()))
    {
		/* Try to load the requested dataset from the repository to working memory */
				
#ifdef M8_ENABLED
		boSuccess = DSH_load_dataset_from_memory((MVInt8)(*pnPlanNum));
#else
		boSuccess = Datahand_LoadDataset(*pnPlanNum);
#endif

		if (boSuccess)
		{
			TMA_setAlertsHis();
			TMA_SetFstPeriodTime();
		}        
		/* Reset plan change request marker (regardless of
		 * whether the load was successful.  If it wasn't,
		 * we don't want to keep trying ad-infinitum - 
		 * an error will have been logged by Datahand_LoadDataset) */
		*pnPlanNum = 0;       

	}    
	else
	{
		boSuccess = FALSE;
	}
    
	return ( boSuccess );
} /* static BOOL handle_plan_changing( ... ) */


/*************************************************************************
 *
 * Function: set_peddet
 *
 * Description: Function to determine if a detector is ped detector served in all red ped detector
 *
 * Author: pkale
 *
 * Date: 14-Jan-2011	
 *
 * Params:	detno - detector number in consideration
 *			nlink - Total number of links 
 *			ltype - Link type (used to indicate if the link is a traffic latched/unlatched ped detector etc)
 *			redped - All Red pedestrians stage indicator. 
 *			lgreen - Link green			
 *
 * Return: int PedDet - return 999 is a ped detector working in all red ped stage else returns 0.
 *
 ************************************************************************/

static int set_peddet(int detno, int nlinks, int ltype[], int waitch[], int redped[], signed char lgreen [][mxlink])
{
	int linkloop,PedDet;
	for (linkloop = 1; linkloop <= nlinks; linkloop++) 
	{
		/* Check all LTYPE(link) for dets:  */
		if (ltype[linkloop-1] == detno				/*Simple Ped Detector*/
			|| ltype[linkloop-1] == detno + 100		/*Unlatched Ped Detector*/
			|| detno == ltype[linkloop-1] - 200 || detno == ltype[linkloop-1] - 199 || detno == ltype[linkloop-1] - 198 /*Volume sensitive puffin detector*/
			|| detno == waitch[linkloop-1]	) 
		{	   

			if (lgreen[redped[0]-1][linkloop-1]== 1 || lgreen[redped[1]-1][linkloop-1]== 1   )
			{
				return(PedDet = 999);
			}
		}
	}
	return(PedDet = 0);

}


#ifdef M8_DET_FAULT_CHECKING_AND_LOGGING
/*************************************************************
*
* Function      : check_chattering_detector
*
* Author (Date) : Islam Abdelhalim (22/09/2016)
*
* Description   : Checks if the detector is doing a chattering behaviour.
*
* Parameters    : det_id: the detector index in question.
*
*************************************************************/
static void check_chattering_detector(uint8 det_id)
{
	/*Getting here means that the det was OFF and then it is ON */

	uint8 l, i, j;
	mv_bool is_found = mv_false;
	uint8 det_num = det_id + 1;

	/*First, check the det type and skip the checking if it is not (X, IN, SINK or OUT dets).*/

	/* As there is no direct way to know the det type, we need to scan all the lanes and check.*/
	for (l = 1; l <= DI_get_lanes_count(); l++)
	{
		if (DI_get_det_num(l, IN_DET) == det_num ||
			DI_get_det_num(l, X_DET) == det_num ||
			DI_get_det_num(l, IN_SINK_DET) == det_num ||
			DI_get_det_num(l, IN_SINK2_DET) == det_num ||
			DI_get_det_num(l, X_SINK_DET) == det_num ||
			DI_get_det_num(l, X_SINK2_DET) == det_num)
		{
			is_found = mv_true;
			break;
		}

		/*Check the OUT det if MIXOUT <> -99*/
		if (DI_get_det_num(l, OUT_DET) == det_num)
		{
			for (i = 0; i < DI_get_links_count(); i++)
			{
				for (j = 0; j < MAX_LANES_ON_LINK; j++)
				{
					if (l == DI_get_lane_num(i+1, j))
					{
						if (DI_get_min_veh_count_between_x_and_out_det(i + 1) != USE_CALL_CANCEL)
						{
							is_found = mv_true;
							break;
						}
					}
				}
			}
		}
	}

	

	/*Skip the checking of the chattering issue if the type doesn't match*/
	if (is_found == mv_false)
		return;


	chattering_det_counter[det_id]++;
	
	if (chattering_det_timer[det_id] == 0)
		chattering_det_timer[det_id] = 1; /*Once initialised, it will move on.*/


	/* There is no need to check the timer here, because the counter get reset if the timer exceeded the limit.*/
	if (chattering_det_counter[det_id] >= CHATTERING_DET_MAX_COUNTER)
	{
		chattering_det_occurrence[det_id]++;

		if(chattering_det_occurrences_timer[det_id] == 0)
			chattering_det_occurrences_timer[det_id] = 1; /*Initialise the occurences timer so it starts timing till CHATTERING_DET_OCCURRENCES_PERIOD.*/

		if (chattering_det_occurrence[det_id] >= CHATTERING_DET_MAX_OCCURRENCE)
		{	
			/*Make sure that is was not already logged as chatter to avoid multiple logging of the error.*/
			if (CI_get_detector_suspicion_status(det_id) != DET_FAULT_STATUS_SUSPECTED_DUE_TO_CHATTERING && 
				CI_get_detector_suspicion_status(det_id) != DET_FAULT_STATUS_SUSPECTED_DUE_TO_24_HOURS_CHATTERING)
			{
				CI_set_detector_suspicion_status(det_id, DET_FAULT_STATUS_SUSPECTED_DUE_TO_CHATTERING);

				chattering_det_occurrence[det_id] = 0;
				chattering_det_occurrences_timer[det_id] = 0;
				
				TMA_det_faulty_changed(det_id, DET_FAULT_STATUS_SUSPECTED_DUE_TO_CHATTERING);
			}

			chattering_det_hourly_check_flag[det_id] = mv_true;
		}

		/*No need for the counter or timer anymore after the judgement*/
		chattering_det_counter[det_id] = 0;
		chattering_det_timer[det_id] = 0;
	}	
}

/*************************************************************
*
* Function      : update_chattering_detector_timers_counter_and_status
*
* Author (Date) : Islam Abdelhalim (22/09/2016)
*
* Description   :Update the chattering detectors timers, counter and status (if needed).
*				 This function is called every single scan.
*
* Parameters    : detIdx: the detector index in question.
*/

void update_chattering_detector_timers_counter_and_status(uint8 detIdx)
{
	/*This flags is just to make sure that no more than one checking for the head of the hour will be done.*/
	static mv_bool is_head_of_the_hour_checked[MAX_DETS] = { mv_false }; 

	/*Update the chattering det timer if it was initialised*/
	if (chattering_det_timer[detIdx] > 0)
	{
		chattering_det_timer[detIdx]++;

		/* The counter and timer of the chatter det has no meaning if the timer exceeded its limit.*/
		if (chattering_det_timer[detIdx] > CHATTERING_DET_MAX_TIMER)
		{
			chattering_det_timer[detIdx] = 0;
			chattering_det_counter[detIdx] = 0;
		}
	}

	/*Update the chattering det occurrences timer if it was initialised*/
	if (chattering_det_occurrences_timer[detIdx] > 0)
	{
		chattering_det_occurrences_timer[detIdx]++;

		/* The counter and timer of the chatter det has no meaning if the timer exceeded its limit.*/
		if (chattering_det_occurrences_timer[detIdx] > CHATTERING_DET_OCCURRENCES_PERIOD)
		{
			chattering_det_occurrences_timer[detIdx] = 0;
			chattering_det_occurrence[detIdx] = 0;
		}
	}

	/*See if we can reset the det status.*/
	if (CI_get_detector_suspicion_status(detIdx) == DET_FAULT_STATUS_SUSPECTED_DUE_TO_CHATTERING ||
		CI_get_detector_suspicion_status(detIdx) == DET_FAULT_STATUS_SUSPECTED_DUE_TO_24_HOURS_CHATTERING)
	{
		/*Checking if we are on the head of the hour*/
		current_time = Com_read_rtc(READ_RTC);

		//TESTING: if ((current_time.minutes % 10 == 0) && is_head_of_the_hour_checked[detIdx] == mv_false) /*Check every 1 hour*/
		if (current_time.minutes == 0 && is_head_of_the_hour_checked[detIdx] == mv_false) /*Check every 1 hour*/
		{
			if (chattering_det_hourly_check_flag[detIdx] == mv_false)
			{
				/*Getting here means that the det was NOT judged as chattering in the previous hour at all.*/
				CI_set_detector_suspicion_status(detIdx, DET_FAULT_STATUS_OK);
				TMA_det_faulty_changed(detIdx, DET_FAULT_STATUS_OK);

				chattering_det_24_hour_counter[detIdx] = 0;
			}
			else
			{
				/*Getting here means that the det is judged as chattering in the previous hour.*/
				
				chattering_det_hourly_check_flag[detIdx] = mv_false;

				if (chattering_det_24_hour_counter[detIdx] < 24-1)
				{
					chattering_det_24_hour_counter[detIdx]++;
				}
				else if(CI_get_detector_suspicion_status(detIdx) != DET_FAULT_STATUS_SUSPECTED_DUE_TO_24_HOURS_CHATTERING)
				{
					CI_set_detector_suspicion_status(detIdx, DET_FAULT_STATUS_SUSPECTED_DUE_TO_24_HOURS_CHATTERING);

					/* The error will only get logged once.*/
					handle_faulty_detector(detIdx+1);
				}

			}

			is_head_of_the_hour_checked[detIdx] = mv_true;
		}
		
		//TESTING: if (current_time.minutes % 11 == 0) /*Just to make sure that the head of the hour has passed, so we can reset the checking flag.*/
		if(current_time.minutes == 1) /*Just to make sure that the head of the hour has passed, so we can reset the checking flag.*/
		{
			is_head_of_the_hour_checked[detIdx] = mv_false;
		}
	}
}


#endif /*M8_DET_FAULT_CHECKING_AND_LOGGING*/

#ifdef M8_RT_OPTIMISATION

/*!
*	\brief	Updates the net gaps of the opposing link. The gaps will be used in the 
*			algorithm: ALG_estimate_right_turners_count_during_opposed_stage(...).
*
*	\return		void
*
*	\author		Islam Abdelhalim 
*	\date		21-03-2014 */

static void update_opposing_link_net_gaps()
{
	uint8 i; /* links containing the RT loop variable */
	uint8 j; /* lanes in the opposed link loop variable */
	uint8 k; /* lanes in the opposing link loop variable */

	uint8 rt_lane_idx;
	uint8 opp_lane_idx;
	uint8 x_det_idx;
	uint8 opposing_lanes_count;

	uint8 off_x_dets_count = 0;


	for(i=0; i<Tcomshr->nlinks; i++)
	{
		/* Check if that link is opposed at the moment*/
		if( Tcomshr->lgreen[CI_get_current_stage()-1][i]  == LINK_STATUS_GREEN_BUT_OPPOSED
			&& Tcomshr->mixout[i] == USE_CALL_CANCEL)
		{
			/* loop over all the lanes of this link - should run only once */
			for (j=0; j<Tcomshr->llatot[i]; j++) 
			{
				/*getting the lane index of the opposed lane*/
				rt_lane_idx = Tcomshr->llanes[i][j] - 1;

				opposing_lanes_count = DI_get_opposing_lanes_count(rt_lane_idx+1);

				/* loop over all the opposing lanes*/
				for(k=0; k<opposing_lanes_count; k++)
				{
					/* getting the idx of the opposing lane*/
					opp_lane_idx = Tcomshr->rtoppo[rt_lane_idx][k] - 1;

					x_det_idx = Tcomshr->det[opp_lane_idx][X_DET] - 1;

					//TOCHECK-E: do I need to check if it is suspected or not?
					//TOCHECK: There is no need to check the COMB-X det because it consists on X-DETs any way.
					/* check if the x-det of the opposing lane is defined and ON*/
					if(x_det_idx > -1 && Tcomshr->detoff[x_det_idx] == 1)
					{
						off_x_dets_count++;
					}
					else /* one of the X-DETS of the opposing lanes is ON*/
					{
						/* do not add the gap time unless is has a value  0*/
						if (Tcomshr->ton[x_det_idx] == MINON && net_gap_time > 0)
						{
							CI_add_opposoing_lanes_net_gap(net_gap_time);
							net_gap_time = 0;
						}
					}
				}

				/* check if ALL the X-DETs of the opposing lanes were OFF*/
				if(off_x_dets_count == opposing_lanes_count)
				{
					/* getting here means that it is a net gap (all the x-dets are OFF*/
					net_gap_time++;
				}
			}
			break; /* break from the all the junction links loop (i) because only one link will be in RTIA at a time*/
		}
	}
	
}
#endif //M8_RT_OPTIMISATION
