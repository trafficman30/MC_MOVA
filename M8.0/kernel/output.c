/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         output.c
*
*  TITLE:        Mova Messages: Thread Safe Screen/Terminal message handling.
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	26-Jan-1998		4.0.0		..		MRC		SIE_00			First undergoing system test
*	17-Feb-1998		4.0.0		..		MRC		SIE_01			Stop writing when tel line dropped (ie when WRITE() returns error)
*	12-Mar-1998		4.0.0		..		MRC		SIE_02			Stop saved messages taking loops out of bounds
*	27-apr-1998		4.0.0		..		PC		SIE_03			WRITE() format strings' types changed 
*	29-Apr-1998		4.0.0		..		PC		SIE_04			'const' added to initialised static's (Siemens MOVA-13).
*	03-May-1998		4.0.0		..		PC		SIE_05			Overlay on 'Xcomshr', etc., replaced by explicit use of structures..
*	19-May-1998		4.0.0		..		PC		SIE_06			nJdx initialised when message log viewed (so that saved messages displayed in full every time).
*	26-May-1998		4.0.0		..		PC		SIE_07			Correction to VM for 9 mins (<=540 rather <540 seconds) (Siemens MOVA-50)
*	29-Sep-2004		5.0.0		..		PC		SIE_08			Use of CMOVA_EP #define to enable/disable emergency/priority code.
*	12-May-2006		5.0.0		..		PC		SIE_09			PC-specific "suspend" function used for pausing Output thread. Incorporated Pedestrian priorities.
*	25-Jan-2005		5.1.0		..		PC		SIE_10			Added Cruise Speed data collection routine.
*	08-Aug-2007		6.0.3		..		PK		TRL_01			Pedmax and curise speed bug fixes.
*	31-May-2011		7.0.0		AA		PK		CRN003			Several Changes for MOVA 7 release	
*	15-Mar-2012		7.0.3		..		AK		CRN009			Changes to conditional compilation flags
*	06-Jul-2012		7.0.3		AB		AK		CRN012			Memory leak in TMA module and statistics lost on dataset change
*	09-Jan-2013		7.0.5		..		AK		CRN020			Print leading zeros on dates and times
*	25-Jan-2013		7.0.5		..		AK		CRN023			Fix compiler warnings
*	31-Jan-2013		7.0.5		AC		AK		M7.0.5			Issue M7.0.5
*	15-Apr-2013		7.0.6		..		AK		CRN027			Improvements to SF feedback
*	14-May-2013		7.0.6		..		AK		CRN036			Correction to formatting of F185 string
*	22-May-2013		7.0.6		AD		AK		M7.0.6			Issue M7.0.6
*
*  (c) Copyright TRL Ltd 2013. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal or
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/

#include <stdlib.h> /* IRH MOD: 22/02/05: PCMOVA: For NULL macro */
#include <nucleus.h>
#include <proj_def.h>
#include <nu_omu.h>
#include "gbltypes.h"
#include "error.h"
#include "errhand.h"
#include "write.h"
#include "obclock.h"
#include <diff_bss.h>
#include "display_msg.h"
#include "tma_preprint.h"
#include "tma_mem.h"
#include "tma_alerts.h"
#include "sfstats.h"
#include "datahand.h"

#include "core_interface.h"

extern char GetHandsetChar(void);
extern int GetOutputDestination( void );
int nDisp;  /* Controls MOVA message output during VM etc, and used in TRLuserIF */

#ifdef IS_MOVACOMM_ENABLED
extern char lOptionDE; /* to print out last 50 messages after crash */    
#else
char lOptionDE; /* to print out last 50 messages after crash */
#endif

/**************************************************************************
       IO ARRAY (1-16) = FORCE BITS TO UTC INTERFACE - M 4.0 not used
       (17)=ERROR COUNT ; (18)=PHONE HOME  ; (19)=WATCHDOG COUNTER
       (20)=ONCONTROL   ; (21)=PRNTON      ; (22)=CONTROLLER READY FLAG
       (23)=FLOWON      ; (24)=STAGESTUCK  ; (25)=DEMSTA
       (26)=ASSTON      ; (27)=ERROR LOG   ; (28)=MOVON
       (29)=FLAG BETWEEN USER AND OUTPUT WHEN DISPLAYING
       (30)=PHONE LINE CONNECTED(=99) , 0 IF NOT , 9 AFTER 'FI'
       (31)=1 IF MULTI-STAGE CONFIRMATIONS=UNABLE TO CONTROL, 0=OK.
       (32)=INCREMENTED WHILE PHONE LINE IN USE.
     PROGRAM SCANS IO(21) - MOVA MESSAGES - MOST 1 SEC
                   IO(27) - ERROR LOG - EVERY 5 SEC
       AND  IO(23)/IO(26) - HOURLY FLOW/ASSESSMENT LOG - EVERY 10 SEC
       EITHER :- 1. DOES NOTHING IF IO(21)= 0
                 2. PRINTS AS GENERATED ON PORT 1 WHEN = 1  W=3
                 3. PRINTS OUT 50 MOVA MESSAGES FROM RAM
                      ON PORT 0 IF IO(21) IS 20/21          W=1
                      ON PORT 1 IF IO(21) IS 10/11          W=3
                      ON PORT 2 IF IO(21) IS 30/31          W=4
  MOD 12/87      4. DISPLAYS X MINS OF MESSAGES ON PORT 0   W=1
  VIEW MESS           WHEN IO(21)=4X
                 5. DISPLAYS X MINS OF MESSAGES ON PORT 2   W=4
                      WHEN IO(21)=5X
                   ****  RETURNS IO(21) TO ZERO  ****
     SCANS IO(27) EVERY 5 SECONDS
       EITHER :- 1. DOES NOTHING IF IO(27) = 0
                 2. PRINTS AS GENERATED ON PORT 1 IF = 1    W=3
                 3. OUTPUTS 100 ERROR MESSAGES FROM RAM
                       ON PORT 0 IF IO(27) IS 20/21         W=1
                       ON PORT 1 IF IO(27) IS 10/11         W=3
                       ON PORT 2 IF IO(27) IS 30/31         W=4
     SCANS IO(23) AND IO(26) EVERY 10 SECONDS
       EITHER :- 1. DOES NOTHING IF IO(23) AND IO(26) = 0
                 2. RECORD DATA ONLY IF = 1
                 3. PRINTS ON PORT 1 AS GENERATED IF = 2
                 4. OUTPUTS WHOLE ASSESSMENT/FLOW LOG FROM RAM
                       ON PORT 0 IF IO(23) OR IO(26) = 20/21/22   W=1
                       ON PORT 1 IF IO(23) OR IO(26) = 10/11/12   W=3
                       ON PORT 2 IF IO(23) OR IO(26) = 30/31/32   W=4
=======================================================================
       Last change:  MC   15 Apr 97    2:46 pm
*/


void output()
{
	extern int nLast50Mess_[MSG_MAX_LENGTH][MAX_MSGS];

#ifdef IS_MOVACOMM_ENABLED
	extern int ShowCruiseSpeed;
#else
	int ShowCruiseSpeed = 0;
#endif

	extern char control[NumberOfForce+NumberOfExtra];
	static TIMESTAMP_TYPE CurrentTime;
	extern TIMESTAMP_TYPE Com_read_rtc(int, ... );

#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
				/* No extra task-related data for PCMOVA */
#else
	extern NU_TASK mova_task_control_block[NUM_MOVA_TASKS];
	STATUS status;
#endif

	static float rx;

	static int itemp[64],abo,ierr1,ierr3,ierr4,w,imonm,imone,imona,icount,
		wm,nmx,nm,imemm,i,kn,l,NoOfErrors,CurrentError,m,mm,x,na,
		imema,n,nl,dd,f,lx;
	/* IRH MOD: M5.0.0: 06/10/04: Initialise nJdx (compiler warning) */
	int nIdx, nJdx = 0;

	static unsigned long StoredMinute;

	/*static const char *const F94 = {
		"(i3,' NX',i2,' BDR',2i3,i4,1x,6(i2,'LK',2i4,2i3))"
	};*/

	/* IRH MOD: M5.0.0: 01/09/04: Format string for DBZ error */
	static const char *const szDBZFormatString = {
		"(' DATA FOR LAST DIVIDE ERROR: ',a)"
	};
	static const char *const F295 = {
		"(' ',i2,'/',i2,i3,':00 TOTIN=',i4,' TOTX=',i4,' XFL=',2(1x,4i4),/,' XFL=',"
		"4(1x,4i4))"
	};
	/*static const char *const F295a = {
		"(' SatFlow=',15i5,/,' SatFlow=',15i5)"
	};static const char *const F295b = {
		"(' STLOST=',15(i3,'.',i1),/,' STLOST=',15(i3,'.',i1))"
	};*/
	static const char *const F296 = {
		"(' ',i2,'/',i2,i3,':',i2,'-',i2,':',i2,' XFL=',i5,' OCC=',f5.3,\\)"
	};
	static const char *const F2961 = {
		"(' F=',i1' S/TM',3(i2,'=',i3,'/',i3)/'  S/TM',7(i3,'=',i3,'/',i3))"
	};
	/* TRL/MC addition */
	/*static const char *const F301 = {
	   "(' MPU DET COMMS FAIL, OFF-LINE AT ',i02,'/',i02,'/',i02,' ',i02,':',i02,'"
	   " ERR=',i3)"
	};*/

	extern unsigned int MessageCount;
	int NoOfMessages;
	extern CruiseSpeed CruiseStamps[100][2];
	extern unsigned int CruiseIndex;

#ifdef IS_MOVACOMM_ENABLED
	extern char lMessOn;
#else
	char lMessOn = 0;
#endif

	static const char *const F400 = {
		"('|IN| Detector ',i2,' Lane|',i2,'|',i2,'|  ON  at |',i02,':',i02,':',i02,'|',i02,'/',i02,'/',i02)"
	};
	static const char *const F401 = {
		"('|X| Detector ',i2,' Lane|',i2,'|',i2,'|  ON  at |',i02,':',i02,':',i02,'|',i02,'/',i02,'/',i02)"
	};
	static const char *const F402 = {
		"('|IN| Detector ',i2,' Lane|',i2,'|',i2,'|  was ON for|||',i4)"
	};
	static const char *const F403 = {
		"('|X| Detector ',i2,' Lane|',i2,'|',i2,'|  was ON for|||',i4)"
	};

	/*
	 ----------------------------------------------------------------------
							 INITIALISATION
	*/
		abo = 940;
		ierr1 = 0;                     /* ABOrt code for OUTPUT is 940*/
		ierr3 = 0;
		ierr4 = 0;                     /* reset error counters*/
		w = 1;                         /* set Logical UNit NO. for printer*/
		/* TRL/SIE: Printer output will go to handset (if ever enabled) */
    
		/* TRL/SIE: Opening of units not necessary. Call to new WRITE function deals
					with the directing of the output to port appropriately */             
                
		/* TRL/SIE: Use USER port definitions
					1 = printer
					2 = phone
					3 = local handset
		*/
S1:

		imonm  = 0;
		imone  = 0;
		imona  = 0;                     /* clear initialising flags */
		icount = 0;                     /* scan counter - 0 to 9 */

		CurrentTime = Com_read_rtc( READ_RTC);
		StoredMinute = CurrentTime.minutes;

		/* IRH MOD: 18/03/06: BUGFIX: PCMOVA: 10072; MOVA M5: M5_CRN0020
		Stop outputting messages on (re)initialisation if messages were started 
		by setting this flag directly in the "LF" menu. Otherwise, if the user
		quits the terminal program while messages are being output this way,
		the error count will reach 20 and MOVA will switch off. 
		Note that this problem didn't apply to the "VM" option: this flag is reset 
		by the code under S34 when the messages are started using "VM". */
		Tcomshr->io[20] = 0;
		

#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */

    /* IRH MOD: 17/03/06: BUGFIX: PCMOVA: 10067.
       Ensure Write is initialised on (re)initialisation.  
       It is necessary to do this in Output because
       WRITE has now been made thread-safe using thread local
       storage: every thread using WRITE must initialize it. */
    WRITE(INITIALIZE);

    /* IRH: PCMOVA: BUGFIX: 10038. Stop outputting messages on (re)initialisation. 
    Note: Do this *before* we suspend output, as nDisp might be legitimately
    set by the user while output is suspended.*/
    nDisp   = 0;                     /* time to display VM messages on screen */
    
	PCTasks_SuspendOutput();
#else
    status = NU_Suspend_Task(&mova_task_control_block[OUTPUT_TSK] );
    if ( status != NU_SUCCESS )
       SOFT_ERROR(OUTPUT_Suspend_Task);    
    nDisp   = 0;
#endif    
    
/* ============== Normal entry point =====================     */
    
S10:
/*
============================ main loop ================================
*/

	if(ShowCruiseSpeed)
	{
		if ( MessageCount > 0 ) 
		{
			NoOfMessages = MessageCount;
			MessageCount = 0;
			CruiseIndex = 1 - CruiseIndex;
			// Output time-date stamps for Cruise Speed Data Collection
			for ( i = 1; i <= NoOfMessages; i++ )
			{
				if(CruiseStamps[i-1][1-CruiseIndex].State == 1)
				{
					if(CruiseStamps[i-1][1-CruiseIndex].DetType == 0)
					{
						if(WRITE(w,IOSTAT,NULL,2,FMT,F400,1,MORE))goto S990;
					} else {
						if(WRITE(w,IOSTAT,NULL,2,FMT,F401,1,MORE))goto S990;
					}
			        if(WRITE(INT2,CruiseStamps[i-1][1-CruiseIndex].Detector,MORE))goto S990;
			        if(WRITE(INT2,CruiseStamps[i-1][1-CruiseIndex].EffectiveLane,MORE))goto S990;
			        if(WRITE(INT2,CruiseStamps[i-1][1-CruiseIndex].ActualLane,MORE))goto S990;
			        if(WRITE(INT4,CruiseStamps[i-1][1-CruiseIndex].Hours,MORE))goto S990;
			        if(WRITE(INT4,CruiseStamps[i-1][1-CruiseIndex].Minutes,MORE))goto S990;
			        if(WRITE(INT4,CruiseStamps[i-1][1-CruiseIndex].Seconds,MORE))goto S990;
			        if(WRITE(INT4,CruiseStamps[i-1][1-CruiseIndex].Date,MORE))goto S990;
			        if(WRITE(INT4,CruiseStamps[i-1][1-CruiseIndex].Month,MORE))goto S990;
			        if(WRITE(INT4,CruiseStamps[i-1][1-CruiseIndex].Year,MORE))goto S990;
				    if(WRITE(0)) goto S990;
				} else {
					if(CruiseStamps[i-1][1-CruiseIndex].DetType == 0)
					{
						if(WRITE(w,IOSTAT,NULL,2,FMT,F402,1,MORE))goto S990;
					} else {
						if(WRITE(w,IOSTAT,NULL,2,FMT,F403,1,MORE))goto S990;
					}
			        if(WRITE(INT2,CruiseStamps[i-1][1-CruiseIndex].Detector,MORE))goto S990;
			        if(WRITE(INT2,CruiseStamps[i-1][1-CruiseIndex].EffectiveLane,MORE))goto S990;
			        if(WRITE(INT2,CruiseStamps[i-1][1-CruiseIndex].ActualLane,MORE))goto S990;
			        if(WRITE(INT4,CruiseStamps[i-1][1-CruiseIndex].Duration,MORE))goto S990;
				    if(WRITE(0)) goto S990;
				}

			}

		}
		
	}

	CurrentTime = Com_read_rtc( READ_RTC);

	/*Update TMA and SF Logs every minute*/
	if (StoredMinute != CurrentTime.minutes)
	{
		TMA_PreP_UpdateTMALogs(pTmaData);
		MovaSatFlowStats_UpdateSatFlowStats_();
		StoredMinute = CurrentTime.minutes;
	}
		
    icount = icount+1;
    if(icount >= 10) icount = 0;
    if(icount == 2 || icount == 8) goto S100;/* DINOUT*/
    if(icount == 5) goto S200;     /* Check error 2 in 10*/
S15:
	/*
	================== do MOVA Message log every 1 second =================
	*/
	

	/* IRH MOD: 19/03/06: BUGFIX: PCMOVA: 10066; MOVA M5: M5_CRN0018
	If messages are being displayed using "VM", check whether
	the user has turned them off.  This check only becomes necessary 
	if the user abandons messages before they have been initialised 
	under S34.  This is possible in the embedded version (since 
	Output only runs every second), but more likely in PCMOVA as the 
	Output routine may not be running at all when the user does "VM"
	and then abandons messages.  Note that it is not thread safe to
	set Tcomshr->io[20] to zero in Userbig under S57 where the 
	abandon messages request is processed - Output may be in the 
	middle of processing the Tcomshr->io[20] flag at the time (S34). 
	Also note that it is possible that one line of messages may get 
	printed out after the user has asked for the messages to be abandoned
	but before the code below turns them off - not worth fixing IMO. */	
	if ( lMessOn == FALSE )
	{
		nDisp = 0;
	}	
	
	if((nDisp > 0 && nDisp <= 540) || nDisp == 999) goto S16;/* Check asmnt 1 in 10*/
    if(Tcomshr->io[20] <= 0) goto S68;/* Not set - wait*/
    if(Tcomshr->io[20] == 1) goto S18;/* Print messages*/
    if(Tcomshr->io[20] >= 40) goto S34;/* View Messages for X mins*/
    if(Tcomshr->io[20] >= 10) goto S30;/* output message log*/
	/*
	-------------------- ! IO(21) is 2 to 9 - not allowed ! ---------------
	*/
    Tcomshr->io[20] = 0;
    goto S68;                      /* ignore - wait*/
S16:
	/*
	-------------- View MOVA Messages - displaying for DISP secs on port WM
	*/
    if ( nDisp != 999 ) nDisp--; /* TRL/SIE:  = nDisp-1; */
    w = wm;                        /* WM is remembered LUNO*/
    /* TRL/SIE: interruptible messages */
    if(nDisp <= 0 ) Tcomshr->io[28] = 0;/* finished*/
    goto S20;
S18:
	/*
	========================== IO(21)=1 print as generated ================
	*/
    w = 3;                         /* printer LUNO*/
S20:
    if(imonm <= 0) 
	{
        nmx = CI_get_current_msg_num();
        imonm = 1;                 /* 1st time only*/
    }
    if(CI_get_current_msg_num()== nmx) goto S70;/* jump if no new data*/
    nm = nmx;
    nmx++;  
    if(nmx > 50) nmx = 1;          /*  new data*/
    goto S45;                      /* print it out*/
S30:
	/*
	=============================== IO(21)>=10 write message log ==========
	*/
    nm = 1;                        /* start at record #1*/
    nJdx = 0;
    w = 3;                         /* IO(21)<20, use W=2, LOCAL_HANDSET*/
    imemm = 0;                     /* clear error counter*/
    if(Tcomshr->io[20] < 20) goto S50;
    w = 3;                         /* 19<IO(21)<30, terminal, W=3, CN00*/
    if(Tcomshr->io[20] >= 30) w = 2;/* IO(21)>29, phone, W=2, CN02*/
    goto S45;
S34:
	/*
	================== <VM> FOR X MINS  -  IO(21)=4X or 5X ================
	*/
    w = 3;                         /* IO(21)=5X, terminal, W=3, CN00*/
    if(Tcomshr->io[20] < 50) w = 2;/* IO(21)=4X, phone, W=2, CN02*/
    i = (int)Tcomshr->io[20]/10;   /* I=5 for term, =4 for phone*/
    /* nDisp control MOVA message output: changed in USER VM option as well */
    nDisp = ((int)Tcomshr->io[20]-i*10)*60;/* nDisp is time to View Messages in sec*/
    Tcomshr->io[20] = 0;           /* reset print flag IO(21)*/
    if ( nDisp == 0 )
      nDisp = 999;                  /* TRL/SIE: new! continuous until interrupted */
    wm = w;                        /* remember LUNO in WM*/
    goto S20;
S40:
/*
=======================================================================
*/

#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
	PCTasks_SuspendOutput();
#else
    status = NU_Suspend_Task ( &mova_task_control_block[OUTPUT_TSK] );
    if ( status != NU_SUCCESS )
       SOFT_ERROR(OUTPUT_Suspend_Task);
#endif

S45:
	/*
	 ******************************* PRINT OUT ****************************
	 **********************************************************************
	*/
    
    /* keep telephone line open*/
	if(w == 2) Tcomshr->io[31] = 1; 

S50:
	/* If printing save messages, copy from nLast50Mess_ to current imess */
	if ( lOptionDE )
	{
		for ( nIdx=0; nIdx<=56; nIdx++ )
		{
			/* make sure subscript references are within bounds */
			if ( nIdx == 1 && nLast50Mess_[nIdx][nJdx] > MAX_MSGS )
				nLast50Mess_[nIdx][nJdx] = MAX_MSGS;
			CI_set_msg_data((uint8)nIdx, (uint8)nm-1, nLast50Mess_[nIdx][nJdx]);
		}
		nJdx++;
	}

	/*
	--- Note: IMESS(NM,2) contains (a) length of variable-length message
	---               or, (b) number of alternative fixed-length message
	---       IMESS array has dimensions of 50 messages x 57 items
	*/

	if(!ShowCruiseSpeed)				/*Pankaj 8/8/07 to avoid printing mova messages when cruise speed selected*/
	{
		switch(CI_get_msg_data(0,(uint8)nm-1))
		{
		case 1: goto S51;
		case 2: goto S52;
		case 3: goto S53;
		case 4: goto S54;
		case 5: goto S55;
		case 6: goto S56;
		default: break;
		}
		
S501:
		/*
		--- 1 of 6 sorts of message
		*/
			imemm++;
			if(imemm >= 2) nm = MAX_MSGS;
		/*
		--- Message number error : increment count. If more than 2 then end
		*/
	}			goto S62;
	
S51:
	/*
	-------------------- 2 sorts of MOVA message type 1 : 1-1, 1-2 -------
	*/
	if(CI_get_msg_data(1, (uint8)nm-1) == 2)
		goto S1002;
	/*
		Jump to second part of fixed-length message; else o/p first part.
	-------------------------------- MOVA message type 1-1 ----------------
	*/
	//printStrgM11(CI_get_msgs_data(), nm-1, Tcomshr->nlanes, GetOutputDestination());
#ifdef IS_MOVACOMM_ENABLED
	printStrgM11(CI_get_msgs()->msgs_data, nm-1, Tcomshr->nlanes, GetOutputDestination());
#endif
	goto S60;
	
S1002:
	/*
	-------------------------------- MOVA message type 1-2 ----------------
	*/
#ifdef IS_MOVACOMM_ENABLED
	printStrgM12(CI_get_msgs()->msgs_data, nm-1, Tcomshr->stages, Tcomshr->ndets, GetOutputDestination());
#endif
	/*printStrgM12 goes wrong goto S990;*/
	goto S60;

S52:
	/*
	-------------------------------- MOVA message type 2 ------------------
	*/
#ifdef IS_MOVACOMM_ENABLED
	printStrgM20(CI_get_msgs()->msgs_data, nm-1, Tcomshr->nlinks, Tcomshr->nlanes, GetOutputDestination());
#endif
	goto S60;

S53:
	/*
	--------------------- 2 sorts of MOVA message type 3 ------------------
	*/
	#if defined (CMOVA_EP)   
		if(CI_get_msg_data(1,(uint8)nm-1) == 2) goto S3002;
	#endif /* CMOVA_EP */ 
	/*
		Jump to second part of fixed-length message; else o/p first part.
	-------------------------------- MOVA message type 3-1 ----------------
	---  Variable length of message given in IMESS(NM,2)
	*/
#ifdef IS_MOVACOMM_ENABLED
	/*Output ESLI values upto last traffic link*/
	printStrgM31(CI_get_msgs()->msgs_data, nm-1, Tcomshr->nlinks, Tcomshr->nlanes, CI_get_msg_data(1, (uint8)nm-1), GetOutputDestination());
#endif
	goto S60;

#if defined (CMOVA_EP) /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */

S3002:
	/*
	-------------------------------- MOVA message type 3-2 ----------------
	*/
#ifdef IS_MOVACOMM_ENABLED
	printStrgM32(CI_get_msgs()->msgs_data, nm-1, Tcomshr->nlinks, GetOutputDestination());
#endif
	/*printStrgM32 goes wrong goto S990;*/
	goto S60;

#endif /* CMOVA_EP */ /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */

S54:
	/*
	-------------------------------- MOVA message type 4 ------------------
	---  Variable length of message given in IMESS(NM,2)
	*/

#ifdef IS_MOVACOMM_ENABLED	
	printStrgM4(CI_get_msgs()->msgs_data, nm-1, ((CI_get_msg_data(1, (uint8)nm-1) - 7) / 5), GetOutputDestination());
#endif
	/*printStrgM4 goes wrong goto S990;*/

	/*TODO: check the following comment.*/
	/* The following block was commented by IA in 17/06/2015 because the function printStrgM4 should handle this*/
	/*
	if(WRITE(w,IOSTAT,NULL,2,FMT,F94,1,MORE))goto S990;
	for(j=2; j<CI_get_msg_data(1, (uint8)nm-1); j++) 
	{
		if(WRITE(INT2,CI_get_msg_data((uint8)j, (uint8)nm-1),MORE))goto S990;
	}
	if(WRITE(0)) goto S990;
	*/
	
	goto S60;
S55:
	/*
	--------------------- 4 sorts of MOVA message type 5 ------------------
	*/
	kn = CI_get_msg_data(1, (uint8)nm-1);
	switch(kn)
	{
		case 1: goto S5001;
		case 2: goto S5002;
		case 3: goto S5003;
#if defined (CMOVA_EP) 
		case 4: goto S5004;
#endif /* CMOVA_EP */ 
		default: break;
	}
   goto S501;                     /* error*/

S5001:
	/*
	-------------------------------- MOVA message type 5-1 ----------------
	*/
#ifdef IS_MOVACOMM_ENABLED   
	printStrgM51(CI_get_msgs()->msgs_data, nm-1, Tcomshr->nlinks, Tcomshr->nlanes, GetOutputDestination());
#endif
	/*printStrgM51 goes wrong goto S990;*/
	goto S60;
S5002:
	/*
	-------------------------------- MOVA message type 5-2 ----------------
	*/
#ifdef IS_MOVACOMM_ENABLED
	printStrgM52(CI_get_msgs()->msgs_data, nm-1, CI_get_ped_demand_marker(), Tcomshr->nlinks, Tcomshr->nlanes, GetOutputDestination());
#endif
	/*printStrgM52 goes wrong goto S990;*/
	goto S60;
S5003:
	/*
	-------------------------------- MOVA message type 5-3 ----------------
	*/
#ifdef IS_MOVACOMM_ENABLED
	printStrgM53(CI_get_msgs()->msgs_data, nm-1, Tcomshr->nlinks, Tcomshr->nlanes, GetOutputDestination());
#endif
	/*printStrgM53 goes wrong goto S990;*/
	goto S60;

#if defined (CMOVA_EP) 

S5004:
	/*
	-------------------------------- MOVA message type 5-4 ----------------
	*/  
#ifdef IS_MOVACOMM_ENABLED
	printStrgM54(CI_get_msgs()->msgs_data, nm-1, Tcomshr->nlinks, Tcomshr->nlanes, GetOutputDestination());
#endif
	/*printStrgM54 goes wrong goto S990;*/
	goto S60;

#endif /* CMOVA_EP */ /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */

S56:
	/*
	--------------------- 2 sorts of MOVA message type 6 ------------------
	*/
    kn = CI_get_msg_data(1, (uint8)nm-1);
    switch(kn)
    {
        case 1: goto S6001;
#if defined (CMOVA_EP)
        case 2: goto S6002;
#endif /* CMOVA_EP */ 
        default: break;
    }
    goto S501;                     /* error*/
S6001:
	/*
	-------------------------------- MOVA message type 6-1 ----------------
	*/
#ifdef IS_MOVACOMM_ENABLED
	printStrgM61(CI_get_msgs()->msgs_data, nm-1, Tcomshr->stages, Tcomshr->nlinks, Tcomshr->nlanes, GetOutputDestination());
#endif
	goto S60;

#if defined (CMOVA_EP) 

S6002:
	/*
	-------------------------------- MOVA message type 6-2 ----------------
	*/
#ifdef IS_MOVACOMM_ENABLED
	printStrgM62(CI_get_msgs()->msgs_data, nm-1, Tcomshr->stages, Tcomshr->nlinks, GetOutputDestination());
#endif
	goto S60;

#endif /* CMOVA_EP */ /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */

S60:
	/*
	======================== MOVA message been printed ====================
	----------------- clear relevant LUNO error counter -------------------
	*/
    if(w == 3) ierr1 = 0;
    if(w == 1) ierr3 = 0;
    if(w == 2) ierr4 = 0; 
S62:
    if(Tcomshr->io[20] < 10) goto S70;
	/*
	--------------------------------- end if printing messages as generated
	*/
    nm++; 
    if(nm <= MAX_MSGS) goto S40;
	/*
	--------------------------- otherwise do next message in log up to 50th
	*/
    Tcomshr->io[28] = 0;           /* finished - tell USER program*/
S67:
	/*
	---------------------------------------- clear down message flag IO(21)
	*/
    if(Tcomshr->io[20] < 10) goto S68;
    Tcomshr->io[20] = (char)((int)Tcomshr->io[20]-10); /* TRL/MC: cast to char */
    goto S67;
S68:
    w = 1;
    nDisp = 0;                      /* reset LUNO and nDisp*/
	/*
	---------------- clear initialising marker unless printing as generated
	*/
    if(Tcomshr->io[20] <= 0) imonm = 0;
    goto S70;                      /* that's all - finish >*/
S100:
	/*
	=======================================================================
	---------------------- DO ERROR LOG EVERY 5 SECONDS -------------------
	*/
    if(Tcomshr->io[26] <= 0) {     /* if flag not set  then*/
        NoOfErrors = Errhand_GetErrorCount();
        goto S15;                  /* skip to MOVA messages*/
    }
    if(Tcomshr->io[26] >= 10) goto S130;/* output error log*/
	/*
	------------------------------------------- IO(27)=1 print as generated
	*/
	if (imone == 0)
	{ /* 1st time only*/
		NoOfErrors = Errhand_GetErrorCount();
		imone = 1;
	}

    if( NoOfErrors == Errhand_GetErrorCount() ) goto S15;/* skip to MOVA messages if no new data*/
	/*
	--------------------- new errors have been detected : set LUNO for printer, do next data item
	*/
    w = 1;
    CurrentError = NoOfErrors;
    NoOfErrors++;
    if((Errhand_ExceededMaxErrors(NoOfErrors))) NoOfErrors = 1;
	/*
	--------------------------------------- check error message count is ok -------
	*/
    if (Errhand_IsErrorValid(CurrentError)) goto S150;
	/* Consider next error */
	NoOfErrors = Errhand_DecrementErrorCount();
	goto S70;                      /* finish >*/

S130:
	/*
	======================== IO(27)>=10 output error log ==================
	*/
    CurrentError = 1;
    w = 1;                         /* IO(27)=1X, printer, CN01*/
    if(Tcomshr->io[26] < 20) goto S150;/* output error log*/
    w = 3;                         /* IO(27)=2X, terminal, CN00*/
    if(Tcomshr->io[26] >= 30) 
	{
        w = 2;                     /* IO(27)=3X, phone line, CN02*/
        Tcomshr->io[31] = 1; /*TRL/SIE:++; *//* = Tcomshr->io[31]+1; update phone line timer TRL:
        ++ operator used */
    }

S150:
/*
	-------------------------- output error log ---------------------------
*/
	while ( !(Errhand_ExceededMaxErrors(CurrentError)) )
	{

		if (Errhand_IsErrorValid(CurrentError))
		{
#ifdef IS_MOVACOMM_ENABLED
			Errhand_PrintMOVAError( CurrentError, GetOutputDestination() );
#endif
			/*
			------------------------------------- clear relevant LUNO error counter
			*/
			if(w == 3) ierr1 = 0;
			if(w == 1) ierr3 = 0;
			if(w == 2) 
			{ /* if phone line -*/
				ierr4 = 0;
				/* TRL/MC mod: line below cast to int */
				Tcomshr->io[31] = 1;/*TRL/SIE:(char)((int)Tcomshr->io[31]+1);*//* increment phone flag*/
			}
			if(Tcomshr->io[26] < 10) goto S70;/* to keep line open*/
			/* if printing as generated - finish*/
			/* Delay before processing next entry */
#if defined (PC_WRAPPER)
			PCTasks_SuspendOutput();
#else
			status = NU_Suspend_Task( &mova_task_control_block[OUTPUT_TSK] );
			if ( status != NU_SUCCESS )
				SOFT_ERROR ( OUTPUT_Suspend_Task );
#endif
		}
		else
		{ /* Invalid error entry */
			/*Nothing printed therefore do not introduce delay*/

			if(Tcomshr->io[26] < 10)
				goto S70;
			/*
			--------------------------------------- ignore if printing as generated
			*/

		}
		CurrentError++;
	}
	
	/*
	-------------------- finished outputting error log - output CRASH array
	*/
    /* IRH MOD: M5.0.0: 01/09/04: Output DBZ error string, if there is one */
    if ( Terrlog->dbzstr[0] != '\0' )
    {
        if(WRITE(w,IOSTAT,NULL,2,FMT,szDBZFormatString,1,
            STRG,Terrlog->dbzstr,0,0 )) goto S990;
    }

	Tcomshr->io[28] = 0;           /* clear USER/OUTPUT flag*/
S167:
	/*
	--------------------------------------- decrement error flag until < 10
	*/
    if(Tcomshr->io[26] < 10) goto S70;/* finish >*/
    Tcomshr->io[26] = (char)((int)Tcomshr->io[26]-10); /* TRL/MC mod: cast to char */
    goto S167;
S200:
	/*
	=============================== END ===================================
	===================== DO ASSESSMENT/HRLY FLOWS EVERY 10 SEC ===========
	---------------- X is marker to prevent continually outputting full log
	*/
    x = 0;                         /* M 1.4*/
    if(Tcomshr->io[22] >= 10 || Tcomshr->io[25] >= 10) goto S230;/* output log*/
    if(Tcomshr->io[22] < 2 && Tcomshr->io[25] < 2) goto S262;/* skip to MOVA messages*/
/*
-------------------------------- print as generated -------------------
*/
    if(imona > 0) goto S220;
/*
----------- 1st time only, remember array pointer, set LUNO for printer
*/
    na = Terrlog->ndat;
    w = 1;
    imona = 1;
S220:
    if(Terrlog->ndat == na) goto S15;/* no new data - skip to MOVA messages*/
    goto S245;                     /* output messages*/
S230:
/*
------------------------------------- output assessment/hourly flow log
*/
    na = 1;
    w = 1;
    imema = 0;                     /* IO(23)=1x, printer, CN01*/
    if(Tcomshr->io[22] < 20 && Tcomshr->io[25] < 20) goto S245;
    w = 3;                         /* IO(23)=2x, terminal, CN00*/
    if(Tcomshr->io[22] >= 30 || Tcomshr->io[25] >= 30) /* IO(23)=3x, phone line,CN02*/
	{
		w = 2;
		Tcomshr->io[31] = 1;
    }
    goto S245;                     /* output messages*/

S240:
/*
----------------------- wait here for next 1 sec scan -----------------
*/

#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
	PCTasks_SuspendOutput();
#else
    status = NU_Suspend_Task( &mova_task_control_block[OUTPUT_TSK] );
    if ( status != NU_SUCCESS )
      SOFT_ERROR ( OUTPUT_Suspend_Task );
#endif

S245:
/*
----------------------------- output flow/assessment messages ---------
*/
    m = Terrlog->idat[0][na-1]/1000;
    n = na;
    if(m == 21 || m == 22) goto S255;/* assessment*/
    if(m == 23) goto S249;         /* hourly lane flows*/
	/*
	---------------- error - M out of range 21 - 23 -----------------------
	*/
    if(Tcomshr->io[22] >= 10 || Tcomshr->io[25] >= 10) goto S248;/* hourly flows*/
    if(Tcomshr->io[22] <= 2 && Tcomshr->io[25] <= 2) goto S262;/* output log*/
S248:
    imema = imema+1;               /* increment error count*/
    goto S261;
S249:
/*
---------------------------- output hourly flows ----------------------
*/
    l = 0;
    nl = Terrlog->idat[3][na-1]/1000;
    mm = Terrlog->idat[3][na-1]/50-nl*20;
    dd = Terrlog->idat[3][na-1]-mm*50-nl*1000;
S251:
    i = 0;
    na = na+1;
    if(na > 1100) {
        na = 1;
        x = 1;
    }
S252:
    l = l+1;
    i = i+1;
    itemp[l-1] = Terrlog->idat[i-1][na-1];
	/*
	 --- Transfer flows from possibly several log records into ITEMP.
	*/
    if(l >= nl) goto S253;
    
	if( ( l % 4 ) == 0) goto S251;
	/*
	 --- Jump back to 251 if log record comprising 4 stores is finished.
	 --- Else, continue from 252 dealing with same log.
	*/
    goto S252;
S253:
    if(WRITE(w,IOSTAT,NULL,2,FMT,F295,2,INT2,dd,INT2,mm,INT2,Terrlog->idat[0][n-1]
      -23000,INT2,Terrlog->idat[1][n-1],INT2,Terrlog->idat[2][n-1],MORE))goto S990;
    for(i=1; i<=nl; i++) {
        if(WRITE(INT2,itemp[i-1],MORE))goto S990;
    }
    if(WRITE(0)) goto S990;
	
    goto S260;    

S255:
	/*
	---------------------------------- output assessments -----------------
	*/
    l = 0;
S256:
    i = 0;
    na = na+1;
    if(na > 1100) {
        na = 1;
        x = 1;
    }
S257:
    l = l+1;
    i = i+1;
    itemp[l-1] = Terrlog->idat[i-1][na-1]/100;
    itemp[l+mxstag-1] = Terrlog->idat[i-1][na-1]-itemp[l-1]*100;
	/*
	 --- Transfer stage info from possibly several log records into ITEMP.
	*/
    if(l >= Tcomshr->stages) goto S258;    
    if ( (l % 4) == 0) goto S256;
	/*
	 --- Jump back to 256 if log record comprising 4 stores is finished.
	 --- Else, continue from 257 dealing with same log.
	*/
    goto S257;
S258:
    mm = Terrlog->idat[1][n-1]/100;
    f = Terrlog->idat[0][n-1]/100-m*10;
    lx = Tcomshr->stages;
    rx = (float)Terrlog->idat[3][n-1];
    i = Terrlog->idat[0][n-1]-m*1000-f*100;
    rx /= 1000.0f; /* TRL/MC mod: was rx = rx/1000.0 */
	/*
	 --- M 4.0 Can't see why can't have 8 stages on line 1. IF(LX.GT.7)LX=7
	*/
    if(WRITE(w,IOSTAT,NULL,2,FMT,F296,2,
      INT2,Terrlog->idat[1][n-1]-mm*100,
      INT2,mm,
      INT2,Tcomshr->asstim[(long)i*2L-2]/60,
      INT2,Tcomshr->asstim[(long)i*2L-2]-Tcomshr->asstim[(long)i*2L-2]/60*60,
      INT2,Tcomshr->asstim[(long)i*2L-1]/60,
      INT2,Tcomshr->asstim[(long)i*2L-1]-Tcomshr->asstim[(long)i*2L-1]/60*60,
      INT2,Terrlog->idat[2][n-1],
      REAL4,rx,0))goto S990;
    
    if(WRITE(w,IOSTAT,NULL,2,FMT,F2961,2,INT2,f,MORE))goto S990;
    for(l=1; l<=lx; l++) {
        if(WRITE(INT2,l,INT2,itemp[l-1],INT2,itemp[l+mxstag-1],MORE))goto S990;
    }
    if(WRITE(0)) goto S990;
S260:
	/*
	------------------------------- finish outputting ---------------------
	------------------------------------- clear relevant LUNO error counter
	*/
    if(w == 3) ierr1 = 0;
    if(w == 1) ierr3 = 0;
    if(w == 2) 
	{
        ierr4 = 0;        
        Tcomshr->io[31] = 1;
    }
S261:
    na++;
    if(na > 1100) 
	{
        na = 1;
        x = 1;
    }
	/*
	--------------------------------- output next record if doing whole log
	*/
    if(Tcomshr->io[22] >= 10 || Tcomshr->io[25] >= 10) goto S263;
    goto S70;                      /* finish if printing as generaded*/
S262:
	/*
	------------------------------------------------- skip to MOVA messages
	*/
    imona = 0;
    goto S15;
S263:
	/*
	-------------------------------------------------------- do next record
	*/
    if(imema <= 5 && x == 0) goto S240;
	/*
	-------- error or finished outputting whole log. Clear USER/OUTPUT flag
	*/
    imema = 0;
    Tcomshr->io[28] = 0;
S267:
	/*
	------------------------ decrement flow and assessment flags to be < 10
	*/
    /* TRL/MC mod: Next two lines cast to char */
    if(Tcomshr->io[22] >= 10) Tcomshr->io[22] = (char)((int)Tcomshr->io[22]-10);
    if(Tcomshr->io[25] >= 10) Tcomshr->io[25] = (char)((int)Tcomshr->io[25]-10);
    if(Tcomshr->io[22] >= 10 || Tcomshr->io[25] >= 10) goto S267;
	/*
	---------------------------------- clear initialisation flag and finish
	*/
    imona = 0;
    goto S70;
S70:
	/*
	================================= END =================================
	============================= WAIT 0.5 SEC ============================
	*/

#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
	PCTasks_SuspendOutput();
#else
    status = NU_Suspend_Task( &mova_task_control_block[OUTPUT_TSK] );
    if ( status != NU_SUCCESS )
      SOFT_ERROR ( OUTPUT_Suspend_Task );
#endif

    goto S10;          /* next scan */
S990:
	/*
	 !!!!!!!!!!!!!!!!!!!!!! WRITE ERROR !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	*/
    Tcomshr->io[28] = 0;
    Tcomshr->io[26] = 0;
    if(w == 1) goto S993;
    if(w == 2) goto S994;
    ierr1 = ierr1+1;               /* printer error UNIT 1*/
    if(ierr1 < 20) goto S1;        /* printing as generated*/
    goto S9999;                    /* > 20 errors - abort*/
S993:
    ierr3 = ierr3+1;               /* terminal error UNIT 3*/
    if(ierr3 < 20) goto S1;        /* carry on if < 20 errors*/
    goto S9999;                    /* > 20 errors - abort*/
S994:
    ierr4 = ierr4+1;
    if(ierr4 < 20) goto S1;        /* carry on if < 20 errors*/
	/*
	---------------------------------- abort ------------------------------
	*/   
S9999:
    Terrlog->stgoff = 0;
    if(Tcomshr->stgdem == 0) Terrlog->stgoff = 1;
    for(i=1; i<=mxstag+2; i++) {
        control[i-1] = Terrlog->stgoff;/* clear force, HI, & TO bits*/
    }
    Tcomshr->io[19] = 0;
    Tcomshr->io[27] = 0;             /* set CONTROL flag off, VA/MOVA flag off*/
    Tdinout->dout[DoutDetFault] = 1; /* set MOVA error LED - walamp[0] */
    Switch_MOVA_Off(SAV_MESSAGES, RESTART_MOVA);

} /* End of output() */

