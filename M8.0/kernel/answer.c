/*
Module name . . . . . . . . . . . . . . . Answer.c
Module version  . . . . . . . . . . . . . SIE_14
Last modification date  . . . . . . . . . 08/3/00
MOVA version at which this 
         module was last modified . . . . M4.0.1
Authority . . . . . . . . . . . . . . . . TRL Ltd

      Last change:  PC   8 Mar 00    2:39 pm
      
------------ Note on version numbering ------------ 

Module version 

    Current version number for this module. Module versions
    are independent of the overall MOVA version number.

MOVA version at which this module was last modified

    The MOVA version at which the last modification to this 
    module was made.  This may be older than the currently 
    compatible MOVA version if no changes to this module
    were made as part of the current version.
*/
/*
______________________________________________________________________

(c) Copyright TRL Ltd 2004. Rights to and ownership of the software
remain unaffected by changes to the software, whether by addition,
removal or modification. Agreement has been reached between Siemens
Traffic Controls Ltd and TRL Ltd whereby Siemens are authorised to use
and modify the MOVA code. TRL Ltd cannot be held responsible for
improper use. 
______________________________________________________________________

;*                         MODIFICATION RECORD
;*                         -------------------
;*
;* SOURCE
;* IDENT.   DATE        NAME               REASON FOR CHANGE
;* -----    ----        ----               -----------------

   SIE_00   26/1/98     MRC                First undergoing system test
   SIE_01   16/2/98     MRC                Timeout. Call in allowed when call
                                           out requested (with io[17]=99/100)
                                           but failed. Sets up lLineDropped to tell
                                           WRITE() to return error. Initialize WRTIE()
                                           when line has dropped.
   SIE_02   25/2/98     MRC                Set_CP_ABANDON_CALL_FLAG() called only when
                                           con_stat_line_open = true.
   SIE_03   2/3/98      MRC                Wait before resetting io[31]
   SIE_04   9/3/98      MRC                Improved handling when dialout requested but not acted upon
                                           yet.
   SIE_05   24/4/98     MRC                Data initialisation (lLineDropped)
   SIE_06   27/4/98     PC                 Format strings for WRITE() corrected
   SIE_07   03/5/98     PC                 Overlay on 'Xcomshr', etc., replaced
                                           by explicit use of structures.
   SIE_08   13/5/98     PC                 Set 'FG_FAULTS_TRANS' before closing
                                           the call and wait until it the call
                                           has been closed so that CALPRO.C can
                                           reset the retry sequence.
                                           (Siemens fault report MOVA-29)
   SIE_09   13/5/98     PC                 Clear 'FG_CALL_REQ' back down if the
                                           local handset is reconnected
                                           (e.g. during the retry sequence)
                                           (Siemens fault report MOVA-30)
   SIE_10   15/5/98     PC                 Modifications for phone line sharing
                                           (Siemens fault report MOVA-32)
   SIE_11   18/5/98     PC                 Allow the user to set an error limit
                                           forcing the MOVA unit to dial-out
                                           when this is exceeded.
                                           (Siemens fault report MOVA-36)

Changes to PB681 issue 2 for issue 3:-

   SIE_12   16/2/99     PC                 Request for complete initialisation
                                           by setting the phone home flag to 77
                                           now calls the power-up routine
                                           IC_POWER_UP_CONTROL() rather than a
                                           soft error.
                                           (Siemens fault report MOVA-67)

Changes for combined OMU and MOVA:-
   SIE_13   12/1/00     PC    - Set_CP_ABANDON_CALL_FLAG() replaced by new
                                function END_MOVA_CALL().
                              - Replaced use of con_stat_line_open with
                                MOVA_IF.RComms == MOVAIF_COMMS_TO_MOVA
                              - MOVA changed to use its own first time power-up
                                code - 'mova_first_time_powerup_code'.
                              - If MOVA wants to 'phone home', send fault to OMU
                                unless phone line sharing enabled, but never try
                                to dial-out.
                              - Setting the phone home flag to 77 has no effect.

   SIE_14   08/3/00     PC    - if phone home limit is zero, phone home when
                                error count reaches 20 as MOVA doesn't seem very
                                good at doing it itself in the bits of code which
                                increment the error count.
*/   
   
   
/* TRL/SIE: #define SPROTOTYPE */
/* TRL/SIE: #include "fortran.h" */
#include <nucleus.h>
#include <proj_def.h>
#include "gbltypes.h"
#include <nu_omu.h>
#include "obclock.h"
#include "error.h"    /* TRL/SIE: for SOFT_ERROR calls */
#include "errhand.h"  /* TRL/SIE: for MOVA error handler */
#include "write.h"
#include <hardware.h> /* for mova_first_time_powerup_code() */
#include <diff_bss.h>
/*
     ###############################################################
     ###                                                         ###
     ### THE TRL COMPUTER PROGRAM 'ANSWER' IS PROTECTED BY U.K.  ###
     ### CROWN COPYRIGHT  AND MUST NOT BE REPRODUCED WITHOUT THE ###
     ### PERMISSION  OF  THE  SECRETARY OF STATE  FOR  TRANSPORT ###
     ###                                                         ###
     ###############################################################
    *****************************************************************
     VERSION 1.0           by Chris Lines              NOVEMBER 1987
     M 1.0                                                  21/04/88
     M 2.0                                                   8/11/88
     M 2.1  Restart with REST subroutine. Wait 10 sec after  2/ 3/89
            error and abort after 10 errors
     M 2.2  Change to initialise DLY every time used        10/ 4/89
     M 2.4  Change initial wait from 10 to 2.5 sec           5/ 5/89
     M 2.6  Dont phone home if no telephone number           3/10/89
     M 4.0 'Big' Extended stages, links etc R A Vincent     19/01/94
    *****************************************************************
       IO ARRAY (1-16) = FORCE BITS TO UTC INTERFACE - M 4.0 not used
       (17)=ERROR COUNT ; (18)=PHONE HOME  ; (19)=WATCHDOG COUNTER
       (20)=ONCONTROL   ; (21)=PRNTON      ; (22)=CONTROLLER READY FLAG
       (23)=FLOWON      ; (24)=STAGESTUCK  ; (25)=DEMSTA
       (26)=ASSTON      ; (27)=ERROR LOG   ; (28)=MOVON
       (29)=FLAG BETWEEN USER AND OUTPUT WHEN DISPLAYING
       (30)=PHONE LINE CONNECTED(=99) , 0 IF NOT , 9 AFTER 'FI'
       (31)=1 IF MULTI-STAGE CONFIRMATIONS=UNABLE TO CONTROL, 0=OK.
       (32)=INCREMENTED WHILE PHONE LINE IN USE.
  This program tries to continually answer the telephone line.
  When sucessful, output appears on CN02: and is read by PHONE.
  Program extends call every 2 minutes as long as IO(32) is being
   incremented by either USER or OUTPUT.
  Program releases line when either IO(32) is not incremented or
   IO(32)=-1
  note: default answer timeout is 40 sec.
             IO(32) CONTROLS PHONE LINE EXTENSION/RELEASE
                        =-1 OR OLDIO WHEN FINISHED
            IO(30) CONTROLS ANSWERING PHONE - READING UNIT #2
                        = 99 WHEN ACTIVE,0 WHEN NOT
 ************ SET USER SEMAPHORE  MESSAGE # 53 ************************
  TXTA(16) = 16,53,TSKNAME(4),SES(4),OPT(2),RESERVED(4)
                  OF REQUESTING TASK
   OPT = BIT 15=1 = RELEASE LINE ; BIT 14=1 = EXTEND CALL
   ERRORS: 0= OK LINE RELEASED/EXTENDED
         128= NO SEMAPHORES SPECIFIED
         129= CONFLICTING SEMAPHORES
         130= CALL WAS NOT IN PROGRESS SO LINE ALREADY RELEASED
         131= CALL NOT IN PROGRESS SO NOT EXTENDED
         255= FACILITY DISABLED
 !!!!!! NOTE ALL NUMBERS GREATER THAN 127 WILL APPEAR NEGATIVE !!!!!!!
 ********************* PHONE HOME MESSAGE # 49 ************************
  TXEVT(32) = 32,49,TSKNAME(4),SES(4),DIALOPT(2),TEL(16),RESERVED(4)
                   OF REQUESTING TASK
   DIALOPT = BIT 15=1 = DUAL TONE DIAL , =0 = IMPULSE DIAL
   ERRORS: 0 = OK
           1 = CALL IN PROGRESS
           2 = REQUEST MADE TOO SOON AFTER PREVIOUS CALL FAILED
           3 = NO ANSWER TONE
           4 = END OF TONE DETECTED
           5 = ANSWER TONE OF INCORRECT FREQUENCY
           6/127 = QUEUE OF FAILED NUMBERS FULL
           128 = DIAL TONE NOT SUPPORTED
           129 = DIGIT FOR DUAL-TONE FOUND WHEN IMPULSE SPECIFIED
           130 = CHECKSUM ERROR IN TELEPHONE NUMBER
           255 = FACILITY DISABLED
 *************** ANSWER PHONE  MESSAGE # 50 ***************************
  TXEVTA(18) = 18,50,TSKNAME(4),SES(4),OPT(2),TIMEOUT(2),RESERVED(4)
                    OF REQUESTING TASK
    OPT = BIT 15=1 = USER DEFINED TIMEOUT  =0 = DEFAULT (40 SEC)
    TIMEOUT = NO. SEC TO LISTEN FOR RINGING TONE (IF BIT 15=1)
    ERRORS: 0= OK PHONE ANSWERED
            1= CALL IS CURRENTLY IN PROGRESS
            2= NO RINGING TONE DETECTED
            3= RINGING TONE OF INCORRECT FREQUENCY
          128= RING DID NOT STOP AFTER ATTEMPT TO GRAB LINE
          255= FACILITY DISABLED
 *************** AUTODIAL RETURN MESSAGES # 55 ************************
  RCV(16) = 16,55,"ADIA"(4),SES(4),ERROR CODE(2),DIAG ADDRESS(4)
    ERROR CODE: BYTE 1= RECEIVED EVENT CODE (50 OR 53)     [RCV(11)]
                BYTE 2= EVENT ERROR CODE - SEE ABOVE       [RCV(12)]
-----------------------------------------------------------------------
*/

/*     TRL/SIEMENS:
 
       ANSWER has been almost completely re-written to fit into the OMU method
       of handling phone calls. The task itself is a low priority task which runs
       continiously.
    
*/

/* prototype */
void answer(UNSIGNED argc, void * argv);

/* extern variable/structures etc */
extern unsigned char FG_CALL_REQ;
extern unsigned char con_stat_line_fault; /* added */
extern unsigned char FG_FAULTS_TRANS;     /* added */
extern unsigned char soft_error_count;
extern unsigned char IC_WATCHDOG_FAIL_COUNT;

unsigned char lLineDropped = TRUE;

/* perserve value for phone home (for after it has been set to 99 or 100) */
static char old_phone_home;
/* previous error count (cleared on every power-up is safer and also ensures a
|  fault is re-reported if the error count is still too high) */
static char old_error_count;

/* extern functions */
extern TIMESTAMP_TYPE Com_read_rtc(int, ... );
extern void ResetModemBuf(void);


void answer(UNSIGNED argc, void * argv)    /* TRL/MC mod: made task */
{
int nI;
unsigned char lCallingOut = FALSE;
extern int GetMOVA_TelNo(char*);
extern unsigned char lLH_CONNECTED;        /* Disallow phone session if handset plugged in */

/*
-----------------------------------------------------------------------
                          INITIALISATION
*/
   Tcomshr->io[29] = 0;           /* /DINOUT/ */
   Tcomshr->io[31] = 0;           /* M 2.4*/
   lLineDropped = TRUE;
   old_error_count = 0;
   /* old_phone_home - not initialised on every power-up, as it should be
   |                   initialised by the code below from io[17] */

   /* start up delay */
   NU_Sleep(375); /* @6.67ms/tick = 2.5s */

   for ( ; ; ) {
      
      /* Infinite loop: Need to delay task to allow other progs to get in */
      NU_Sleep(75); /* @6.67ms/tick = 0.5s */
#if 0 /* Combined OMU/MOVA unit uses OMU application to dial, not MOVA */
      FG_FAULTS_TRANS = FALSE; /* to enable retries */
#endif
            
      /* just in case the phone home copy has become corrupt, reset it */
      if ( old_phone_home > 20 ) old_phone_home = 0;
      /* if the phone home flag has been changed */
      if ( Tcomshr->io[17] != old_phone_home )
      {
          /* if a new error threshold has been set (0-20) */
          if ( Tcomshr->io[17] <= 20 )
          {
              /* preserve the required value for the phone home flag */
              old_phone_home = Tcomshr->io[17];
          }
          /* if it has been set to 99 or 100 by the software or the user */
          else if ( ( Tcomshr->io[17] == 99 )
                 || ( Tcomshr->io[17] == 100 ) )
          {
              /* the value shows that the unit wants to 'phone home' */
          }
          else /* unrecognised value (including 77 - see USERBIG.C) */
          {
              /* return the phone home flag to its original value */
              Tcomshr->io[17] = old_phone_home;
          }
      }

      /* if the error count has increased */
      if ( Tcomshr->io[16] > old_error_count )
      {
         char error_limit;
         
         /* remember the new value */
         old_error_count = Tcomshr->io[16];
         /* if an error limit has been set using the phone home flag */
         if ( old_phone_home != 0 )
         {
             /* then use that value */
             error_limit = old_phone_home;
         }
         else
         {
             /* otherwise only phone if the error count is >= 20 */
             /* 08/03/00 - This has been added since the MOVA kernal does not
             |  seem particularly good at 'phoning home' when it actually
             |  increments the error count past 20. It only seems to do anything
             |  when it is about to try to put itself back on-control after a
             |  warm-up cycle but this might never happen! */
             error_limit = 20;
         }
         /* if the new value exceeds the limit */
         if ( old_error_count >= error_limit )
         {
            /* then request dial out */
            Tcomshr->io[17] = 99;
         }
      }
      /* otherwise if it has decreased */
      else if ( Tcomshr->io[16] < old_error_count )
      {
         /* just remember the new value so a future increase can be detected */
         old_error_count = Tcomshr->io[16];
      }
      else /* otherwise no change */
      {
         /* do nothing */
      }
      
      
      /* See if call has been established anywhere */
      if ( ( MOVA_IF.RComms == MOVAIF_COMMS_TO_MOVA )
        && ( !lCallingOut || lLH_CONNECTED != 0 ) ) 
      /* incomming call established.  */
      {
         lLineDropped = FALSE;
         Tcomshr->io[29] = 99;
         Tcomshr->io[31] = 1;
         nI = 0;
      /* wait until line has dropped, or TRLUserIF finished. Don't wait
         if TERM currently processing local input */
         while ( MOVA_IF.RComms == MOVAIF_COMMS_TO_MOVA  && 
                   Tcomshr->io[31] != -1 &&
                   Tcomshr->io[29] != 98)
         /* do timeout */
         {
            if (nI++ == 3000)
            {
               nI = 0;
               Tcomshr->io[31]--;
            }
            NU_Sleep(30);
         }
         
         if ( lLH_CONNECTED == 0 )
         {
            /* OK, call has finished or line dropped: tidy up */
            Tcomshr->io[31] = -1; /* Tell TRLUserIF that phone has dropped */
            Tcomshr->io[29] = 0;
         }

         lLineDropped = TRUE;  /* tell WRITE() to return error if used now */
         ResetModemBuf();
         /* Instruct OMU to drop line if still open */
         END_MOVA_CALL();
         while ( MOVA_IF.RComms == MOVAIF_COMMS_TO_MOVA ) NU_Sleep(50); /* wait until line dropped */
         
         if ( lLH_CONNECTED == 0 )
         {
            Tcomshr->io[20] = 0; /* to stop MOVA messages */
            Tcomshr->io[28] = 0; /* set io flag to stop view messages */
            Tcomshr->io[29] = 9; /* set flag to force FI in TRL_USERIF */
            WRITE(INITIALIZE);  /* init write as line may have dropped part way through o/p */
            NU_Sleep(200); /* wait a second to allow io[31] to be seen by TRLUserIF() */
         }
         Tcomshr->io[31] = 0; /* for next phone in */
         Tcomshr->io[29] = 0; 
      }
      /*
      when IO(18)==99 OR 100            PHONE HOME >
      */
      if ( Tcomshr->io[17] == 99 || Tcomshr->io[17] == 100 )
      {
         /* if phone line sharing is active */
         if ( phone_line_sharing.active )
         {
            /* then tell that facility that MOVA wants to dial in */
            phone_line_sharing.fault_rqst = TRUE;
            /* when the fault is sent */
            if ( phone_line_sharing.fault_sent )
            {
                /* then clear down the request */
                phone_line_sharing.fault_rqst = FALSE;
                /* clear flag now event has been picked up */
                phone_line_sharing.fault_sent = FALSE;
                /* and the phone home flag */
                Tcomshr->io[17] = old_phone_home;
            }
         }
         else
#if 1 /* new combined OMU/MOVA unit */
         {
            /* pass the fault to the OMU application */
            MOVA_IF.PhoneHomeFlag = 1;
            /* return the phone home flag to its original value */
            Tcomshr->io[17] = old_phone_home;
         }
#else /* old separate MOVA unit */
         {
            /* normal modem operations */
            /* if the local handset is not connected */
            if ( lLH_CONNECTED == 0 )
            {
               FG_CALL_REQ = TRUE;
               nJunk = GetMOVA_TelNo(cJunk); /* to set lGood_MOVA_TelNo */
               lCallingOut = TRUE;     /* see above */
            }
            else /* local handset connected */
            {
               /* clear the request to dial out until it is disconnected again */
               FG_CALL_REQ = FALSE;
            }
         }
#endif
      }
      
#if 0 /* in a new combined OMU/MOVA unit, MOVA never reports faults */
      if (MOVA_IF.RComms == MOVAIF_COMMS_TO_MOVA && lCallingOut && lLH_CONNECTED == 0 ) 
      /* call established (could be incomming or outgoing) now send message */
      {
         tTimeDate = Com_read_rtc( READ_RTC);
         if ( Tcomshr->io[17] == 99 )
            /* TRL/SIE: changed unit to 2 to direct o/p to modem */
            WRITE(2,IOSTAT,NULL,2,FMT,F91,1,
                 STRG,Tcomshr->fname,12,
                 INT4,tTimeDate.date, INT4,tTimeDate.month, INT4,tTimeDate.year,
                 INT4,tTimeDate.hours,INT4,tTimeDate.minutes,0);

         else if ( Tcomshr->io[17] == 100 )
            WRITE(2,IOSTAT,NULL,2,FMT,F92,1,
                 STRG,Tcomshr->fname,12,
                 INT4,tTimeDate.date, INT4,tTimeDate.month, INT4,tTimeDate.year,
                 INT4,tTimeDate.hours,INT4,tTimeDate.minutes,0);
                   
         NU_Sleep(1500);                   /* Give system a chance to deliver message */
         if ( !con_stat_line_fault )
         {
            /* no longer request a call as all faults have been transferred */
            FG_CALL_REQ = FALSE;    /* no longer request dial-out */
            FG_FAULTS_TRANS = TRUE; /* call is complete (reset retry sequence) */
            Tcomshr->io[17] = old_phone_home;    /* reset MOVA phone-home flag */
            lCallingOut = FALSE;    /* call has been sucessful */
            /* Instruct OMU to drop line if still open */
            if ( MOVA_IF.RComms == MOVAIF_COMMS_TO_MOVA )
            {
               /* Then drop line */
               END_MOVA_CALL();
               /* Wait until line dropped as CALPRO.C only runs once a second.
               |  If this procedure does not wait, then it may set FG_FAULTS_TRANS
               |  FALSE (at the top) before CALPRO detects it set TRUE. */
               while ( MOVA_IF.RComms == MOVAIF_COMMS_TO_MOVA ) NU_Sleep(75);
               /* this flag is cleared down automatically by CALPRO.C */
            }
         }
         else 
         {
            /* call has failed. Answered by non-modem */
            FG_FAULTS_TRANS = FALSE;
         }
      }
#endif

#if 0 /* new combined OMU/MOVA unit - uses INI=2 instead */
      /* User reboot requested */
      if ( Tcomshr->io[17] == 77 )
      {
         /* clear the flag */
          Tcomshr->io[17] = 0;
         /* This resets all (MOVA) RAM on reboot */
         mova_first_time_powerup_code = 0;
         /* also reset the soft error and watchdog fail counters so the fully
         |  'initialised' unit has to fail several times again before it will
         |  hard error */
         soft_error_count = 0;
         IC_WATCHDOG_FAIL_COUNT = 0;
         /* switch all the force bits off */
         Switch_MOVA_Off(NO_SAV_MESSAGES, NO_REBOOT);
         /* this performs (and record the reason for) the reboot */
         SOFT_ERROR(MOVA_USER_REQ_REBOOT);
      }
#endif
      
#if 0 /* new combined OMU/MOVA unit never tries to dial-out */
      /* clear flags if io[17] reset */
      if (  Tcomshr->io[17] < 99 )
      {
         FG_CALL_REQ = FALSE;
         lCallingOut = FALSE;
      }
#endif

   }  /* end infinite loop */
}


