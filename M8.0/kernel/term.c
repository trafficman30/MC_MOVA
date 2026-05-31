/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         term.c
*
*  TITLE:        MOVA Terminal: Enginering Screen Update and Remote connection. 
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	22-Apr-1988		1.0.0		..		CL		......			Mova strategy conception modules
*	14-Nov-1988		2.0.0		..		CL		......			Changes to COMSHR global
*	02-Mar-1989		2.1.0		..		CL		......			Replace READ with assembler SINE 
*	05-Mar-1989		2.1.0		..		CL		......			Wait 10 sec after an error 
*	13-Mar-1989		2.1.0		..		CL		......			Include LOOK routine
*	10-may-1989		2.2.0		..		CL		......			Initialise DLY every time used
*	24-May-1989		2.2.0		..		CL		......			Change to CODE numbers + LOOK routine timers
*	04-Jun-1989		2.3.0		..		CL		......			Change to Force stage routine
*	05-Jun-1989		2.4.0		..		CL		......			Mods to BUFFER initialisation and LOOK routine
*	10-Jan-1989		4.0.0		..		RV		......			'Big' Extended stages, links etc
*	14-Feb-1989		4.0.0		..		RV		......			Revised O/P of LOOK screens.
*	26-Jan-1998		4.0.0		..		MRC		SIE_00			First Undergoing system test
*	27-Jan-1998		4.0.0		..		MRC		SIE_01			Various minor cosmetic changes
*	3-Feb-1998		4.0.0		..		MRC		SIE_02			lLH_InUse introduced to tell phone when local handset in use. Also, LH not inspected when phone in use.
*	17-Feb-1998		4.0.0		..		MRC		SIE_03			Exits commissioning func when line dropped using WRITE() return code.
*	23-Feb-1998		4.0.0		..		MRC		SIE_04			Status message display time now >= 0.8s
*	26-Feb-1998		4.0.0		..		MRC		SIE_05			Message when phone line in use uses SendStringToLH()
*	03-Mar-1998		4.0.0		..		MRC		SIE_06			Ctrl+Z filtering
*	09-Mar-1998		4.0.0		..		MRC		SIE_07			Empty terminal buffer if phone engaged and terminal just unplugged.
*	10-Mar-1998		4.0.0		..		MRC		SIE_08			Back to start if terminal unplugged in mid LOOK sess..
*	10-Mar-1998		4.0.0		..		MRC		SIE_09			Dets 17 to 32 printed correctly in UpdateScreen(). Safe abandon on prompt to exit commissioning screen.
*																Faster scanning of arrays to improve response to change in states (esp detectors).
*	11-Mar-1998		4.0.0		..		MRC		SIE_10			Forces were being turned off after timeout if no. entered in LOOK when MOVA in control
*																Dets and conf states output on first entry to EngScreen().
*	24-Apr-1998		4.0.0		..		PC		SIE_11			Modified EngScreen() and StatusMessage() so that the fixed time delay guarenteeing that status messages
*																are not overwriten too quickly is removed.
*	27-Apr-1998		4.1.0		..		PC		SIE_12			WRITE() format strings' types changed.
*	29-May-1998		4.1.0		..		PC		SIE_13			'const' added to initialised static's (Siemens MOVA-13)
*	03-Jun-1998		4.1.0		..		PC		SIE_14			Overlay on 'Xcomshr', etc., replaced by explicit use of structures.
*	26-Jun-1998		4.1.0		..		PC		SIE_15			TryForces() modified to give a longer time out (Siemens MOVA-52)
*	16-Feb-1999		4.1.0		..		PC		SIE_16			Clearing the MOVA error count using 'Z' now also clears the soft error count.Changes to PB681.(Siemens fault report MOVA-67)
*	12-Jan-2000		4.1.0		..		PC		SIE_17			Replaced use of con_stat_line_open with MOVA_IF.RComms == MOVAIF_COMMS_TO_MOVA
*	09-Feb-2000		4.1.0		..		PC		SIE_18			Added licence check before allowing the user to enable MOVA.
*	15-Feb-2000		4.1.0		..		PC		SIE_19			Changed sClrScr so that it also moves the cursor to the top left-hand corner.
*	09-Mar-2000		4.1.0		..		PC		SIE_20			Minor changes to function EngScreen(), UpdateScreen().
*	27-Mar-2000		4.1.0		..		PC		SIE_21			Improved UpdateScreen() so short changes to the other state are always shown on the Look screen.
*	27-Apr-2000		4.1.0		..		PC		SIE_22			Reset MOVA_IF.MOVA_Off_Timer when MOVA switched off control using 'C' on the commissioning screen.
*	03-May-2000		4.1.0		..		PC		SIE_23			MOVA-77: Major changes to this file to allow the menu to be called directly from the 'LOOK'commissioning screen.
*	04-Oct-2000		5.0.0		..		PC		SIE_24			MOVA commissioning screen layout modified for 64 dets/32 confs. Changes for MOVA 5.0 (includes Compact MOVA)
*																MOVA commissioning screen options modified to support forcing of stage 10.
*	07-Mar-2005		5.0.0		..		IHR		SIE_25			Confirms were only output on the commissioning screen if there was a change in the first 16.Now changed to check all 31 confirms and the CRB. 
*	12-May-2005		5.0.0		..		IHR		SIE_26			Term thread now responsible for checking whether a local handset (user) connection still exists. 
*																PC-specific sleep function used to pause Term and TRLUserIF threads. 
*	31-May-2011		7.0.0		AA		PK		CRN003			Changes to comissioning screen options to display TMA and SF logs
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

#include <nucleus.h>
#include <stdio.h>  /* IRH MOD: M5.0.0: 07/10/04: For sprintf() */
#include <string.h>
#include <proj_def.h>
#include "gbltypes.h"
#include "obclock.h"
#include "error.h"    /* TRL/SIE: for SOFT_ERROR calls */
#include "errhand.h"  /* TRL/SIE: for MOVA error handler */
#include "mova_op.h"
#include "write.h"
#include <diff_bss.h>
#include "licence.h"
#include "datahand.h" /* IRH MOD: M5.0.0: 27/07/04: For new Datahand functionality */

#include "core_interface.h"

#if defined (PC_WRAPPER)
    /* For PCUserIO_IsConnected() */
    #include "../pcuserio.h"
#endif

#undef CONSOLE
#define CONSOLE 3 /* TRL/SIE: was 1 */

/*  
    IO ARRAY (1-16) = FORCE BITS TO UTC INTERFACE
    (17)=ERROR COUNT ; (18)=PHONE HOME  ; (19)=WATCHDOG COUNTER
    (20)=ONCONTROL   ; (21)=PRNTON      ; (22)=CONTROLLER READY FLAG
    (23)=FLOWON      ; (24)=STAGESTUCK  ; (25)=DEMSTA
    (26)=ASSTON      ; (27)=ERROR LOG   ; (28)=MOVON
    (29)=FLAG BETWEEN USER AND OUTPUT WHEN DISPLAYING
    (30)=PHONE LINE CONNECTED(=99) , 0 IF NOT , 9 AFTER 'FI'
    (31)=1 IF MULTI-STAGE CONFIRMATIONS=UNABLE TO CONTROL, 0=OK.
    (32)=INCREMENTED WHILE PHONE LINE IN USE.
TERL SCANS THE TERMINAL INPUT ON PORT 0 AND ACTIVATES "USER"
            WHEN AN ENTRY IS MADE
THE PROGRAM PUTS INPUT INTO GLOBAL "BUFFER(2-20)"; BUFFER(1)=3
    USER RESETS BUFFER TO ZERO WHEN FINISHED
    Last change:  MC   10 Jan 95    1:59 pm
*/

extern const char cLF;
extern const char cCR;
extern const char cBackSp;
extern const char cDelete;
extern const char sDelChar[];

/* ansi strings */
static const char sSP_Esc[] = { " \x1B"};
static const char sClrScr[] = { "[2J \x1B[1;1H" };
static const char sYellow[] = { "[33;40;1m" };
static const char sNormal[] = { "[0m"};
/* WRITE format 'statements'*/
static const char *const F1a = {"(a,\\)"};
static const char *const F2a = {"(2a,\\)"};
static const char *const F3a = {"(3a,\\)"};
static const char *const F4a = {"(4a,\\)"};

static char nOldWD;
static char nOldDS;
static char nOldEC;
static char nOldME;
static char nOldMS;
static char nOldCR;
static char nOldOC;
static int nOldWU;
static char nOldin[mxdets];
static char nOldstg[mxconf];
static char nOldfrc[mxstag+1]; /* +1 to include the TO bit */
static int nDefaultStatusMessageCounter;
static int nForceReq = 0;
static int nOldForce = 0;

extern char Valid_Licence(void);
extern char GetHandsetChar(void); /* TRL.SIE: uses GetHandsetChar */
extern char CollectModemChar(void);
extern void Switch_MOVA_Off(int, int);
extern char lMessOn;              /* Stop term reading handset when messages on */

extern unsigned char lLH_CONNECTED; /* something plugged into handset? Set in Monitr() */
extern unsigned char lLineDropped;
extern unsigned char soft_error_count;
extern unsigned char IC_WATCHDOG_FAIL_COUNT;
unsigned char  lMessReq = FALSE; /* LH/phone conflict control */

unsigned char EngScreen(int LU);
int UpdateScreen(int LU);
void StatusMessage(int LU, const char* cMessage);
void TryForces(int LU);
void term(UNSIGNED argc, void * argv);             /* TRL/MC mod: made into function */

/* This flag controls how the menu and commissioning screens interact:
|  TRUE  = menu can be accessed from the commissioning screen and the
|          commissioning is automatically re-displayed when the option is
|          complete (NEW METHOD)
|  FALSE = the old way, where the menu is redisplayed after options are selected
|          and the commissioning screen is started explicitly by typing 'LOOK'
|          from the menu.
|  The flag is automatically set TRUE on a restart (so its on by default) and
|  whenever menu options are entered from the LOOK screen.
|  The flag is automatically set FALSE when the user explicitly chooses to exit
|  from the LOOK screen. */
int MenuViaLookScreen = TRUE;

/* This flag controls whether 'menu options' or keystrokes are displayed and
|  accepted when using the commissioning screen:
|  TRUE  = the full MOVA menu is displayed and two-character options can be
|          entered.
|  FALSE = commissioning screen single keystroke options can be entered. */
static int ShowMenuOnLookScreen = TRUE;
static int OldShowMenuOnLookScreen;

/* These two characters hold the menu option entered on the commissioning screen */
char MenuOptionFromLookScreen[2];

/* Used to build up the option on the screen as it is typed */
static char MenuOptionChr[3] = {' ',' ','\0'};
/* IRH MOD: M5.0.0: 20/08/04: Changed type from char to int (compiler warning) */  
static int MenuOptionLen = 0;

void term(UNSIGNED argc, void * argv)
{
extern unsigned char lModemTried;    /* Call received but user in process of being told LH busy */

/* IRH MOD: M5.0.0: 23/07/04: Let array be initialised to zero (null chars) */
static char n[20];
static int i,j;
static char ivar;
static const char sOP_String[] = {"Phone line in use - try again later"};


    ivar = 0;                      /* /DINOUT/ */
/*
=======================================================================
---------------------------- initialisation ---------------------------
*/
    Terrlog->buffer[0] = 0;        /* M 2.1*/
    if(Tcomshr->io[27] == 2) Tcomshr->io[27] = 0;/* M 2.1*/
    for(i=2; i<=20; i++) {         /* M 2.4*/
       
       /* IRH MOD: M5.0.0: 23/07/04: Let array be initialised to zero (null chars) */
       Terrlog->buffer[i-1] = 0/*32*/;
    }

/*
--------------------------- restart from here after error -------------
*/

S20:
/*
=============================== main loop =============================
*/
   j = 0;
   ivar = 0;
   for ( i=0; i<20; i++ )
      n[i] = 0;

#if defined (PC_WRAPPER)
   /* IRH MOD: PCMOVA: 26/11/05: Term must operate independently of
    Monitr in PCMOVA - it must determine whether a local handset connection
    exists itself.  This is because Monitr only runs when an Update 
    message is received from the simulator, but we want users to be able
    to connect regardless of whether Monitr (+ Detscn, Mova and Genstg)
    are running.  Basically, Term runs at real-time; Monitr at simulation time.
    
    TODO: Create a platform-independent wrapper (a single header file) 
    rather than call PC functions directly from Kernel.
    Then that wrapper can be used by other manufacturers.  Any use
    of extern variables in the PC functions need to be well documented,
    e.g. control[], detsin[], confin[].  Perhaps these
    externs should be placed in an interface header for including by 
    the PCMOVA part (as well as having a header for vice-versa)?  
    Would be better if access to these variables was via functions.
    
    Do this when we know what needs to be in each header,
    e.g. the task functions in the PCKernel, PCUserIO_IsConnected etc..
    */
   lLH_CONNECTED = ( PCUserIO_IsConnected() ? TRUE : FALSE );
#endif

   if ( lLH_CONNECTED != 0 && MOVA_IF.RComms == MOVAIF_COMMS_TO_MOVA )
      lMessReq = TRUE;
   else 
      lMessReq = FALSE;

   /* loop until character found */
   while ( ivar != cCR && j < 20 && lLH_CONNECTED != 0 )
   {
	   /* Wait here while messages are on or reading site data, but only if phone not in use */
	   /* IRH MOD: M5.0.0: 23/09/04: Now using getting download status from Datahand module */
	   /* MenuOptionChr[0]!= ' ' added to avoid reading characters while */
	   while ( ( MenuOptionChr[0]!= ' ' || lMessOn || Datahand_IsDownloadingSiteData( ) /*lReadingSiteData*/ ) 
           && ( MOVA_IF.RComms != MOVAIF_COMMS_TO_MOVA ) )
	   {
		   j = 0;
#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
         PCTasks_Sleep( 500, MOVA_TASK_TERM );

         /* Check whether we're still connected */
         /* BUGFIX: 10038. Check connection flag during MOVA message
         output and dataset download to see if user disconnected. */
         lLH_CONNECTED = ( PCUserIO_IsConnected() ? TRUE : FALSE );
#else
         NU_Sleep(75);          
#endif
      }
      
      /* get character from handset (if any) */
      ivar = GetHandsetChar();      
      if ( ivar != 0 )
      {
         /* if character is either of the delete keys */

		  if ( ( ivar == cBackSp ) || ( ivar == cDelete ) )
         {
            /* if any characters have been stored */
            if ( j > 0 )
            {
                /* decrement count and delete the last character */
                n[--j] = 0;
                /* send string which clears the last character from the screen */
                SendStringToLH(&sDelChar[0], 3);
            }
            else
            {
                n[0] = 0;
            }
         }
         else
         {
             /* store character and increment count */
			 n[j++] = ivar;
         }
      }
      else
      {
#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
          PCTasks_Sleep( 133, MOVA_TASK_TERM );

          /* Check whether we're still connected */
          lLH_CONNECTED = ( PCUserIO_IsConnected() ? TRUE : FALSE );
#else
         NU_Sleep(20); /* @6.67ms/tick = 0.125s */
#endif
      }
   }

   /* if phone session taking place */    
   if ( MOVA_IF.RComms == MOVAIF_COMMS_TO_MOVA && lModemTried == FALSE )
   {
      /* op mess if handset connected and key pressed */
      if ( lMessReq && lLH_CONNECTED != 0 )
      /*if(WRITE(CONSOLE,IOSTAT,NULL,2,FMT,F91,1,0))goto S12; 
              cannot use WRITE() as Phone might be using it */
         SendStringToLH(sOP_String, (int)strlen(sOP_String));

#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
      PCTasks_Sleep( 500, MOVA_TASK_TERM );
#else
      NU_Sleep(75); /* 6.667ms/tick = 0.5s */
#endif
      
      /* if terminal just disconnected */
      if ( lLH_CONNECTED == 0  )
         while ( GetHandsetChar() != '\0' );  /* empty keyboard buffer */
   }
   
   else if ( lLH_CONNECTED != 0 ) /* process input as normal */
   {
      /* IRH MOD: M5.0.0: 23/07/04: If the last character entered 
       * was a CR/LF discard it (may not be if user
       * entered more characters than can fit in buffer - 
       * i.e. if j == 20.) */
      if ( n[j-1] == '\n' || n[j-1] == '\r' )
      {
        n[j-1] = '\0';
      }        

/*
 ----------------------------------------------------------------------
*/
	  Terrlog->buffer[0] = 3;        /* no - OK - fill BUFFER*/      
      for(i=1; i<=j; i++)
	  {
         Terrlog->buffer[i] = n[i-1];/* put data in BUFFER*/
         
         /* IRH MOD: M5.0.0: 23/07/04: Let array be initialised to zero (null chars) */
         n[i-1] = '\0'/*32*/;               /* M 2.1 fill N array with spaces*/
      }
S26:
/*
------------------------------------------ wait here for USER to finish
*/     
      if(Terrlog->buffer[0] != 0)
      {

#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */          
         PCTasks_Sleep( 500, MOVA_TASK_TERM );

         /* If a connection no longer exists quit this loop 
         - lLH_CONNECTED will then be updated at S20: and 
         the TRLUserIF EngScreen routine will quit (UpdateScreen will
         return 1 because a WRITE failed because lLH_CONNECTED was false).
         This isn't very nice.  We have to check for disconnection here
         rather than in Monitr for the reasons given under S20: - the user 
         tasks must be capable of running independently from Monitr - 
         Monitr is only run an Update is received from the simulator. */
         if ( !PCUserIO_IsConnected() )
         {
            Terrlog->buffer[0] = 0;
         }
#else
         NU_Sleep(75); /* 6.667ms/tick = 0.5s */ 
#endif
         goto S26;
      }
/*
----------------------------------------- USER finished - get next data
*/
   }
   else /* No terminal connected */
   {
      /* IRH MOD: M5.0.0: 17/09/04: Reset Terrlog->buffer[0] - this may be 
       * used to indicate whether we're connected, although the
       * lLH_CONNECTED flag should really be used for this.
       * (This was causing Userbig to SOFT_ERROR after about 30 seconds
       * if the user disconnected the terminal program whilst viewing the 
       * Siemens MOVA commissioning screen, because it thought it
       * was still connected, and therefore kept trying to write 
       * to a non-existent terminal - see label S9 in Userbig.c)  */
      Terrlog->buffer[0] = 0;         

#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
      PCTasks_Sleep( 500, MOVA_TASK_TERM );
#else
      NU_Sleep(75); /* 6.667ms/tick = 0.5s */
#endif
   }
   goto S20;

}

/* IRH MOD: M5.0.0: 21/09/04: Modified commissioning screen for 64 dets/32 confs */

static const char *const ENG = {
/*           1         2         3         4         5         6         7         8 */
/*  12345678901234567890123456789012345678901234567890123456789012345678901234567890 */
"(2a,"

#if defined (PC_WRAPPER)
/* IRH MOD: PCMOVA: 21/04/05: Removed 'SIEMENS' from title */
"    '                         MOVA COMMISSIONING SCREEN     ',2a,/,"
#else
"    '                      SIEMENS MOVA COMMISSIONING SCREEN',2a,/,"
#endif

"  'Detectors:      1--4  5--8    9-12 13-16   17-20 21-24   25-28 29-32',/"
"/"              /* 0000  0000    0000  0000    0000  0000    0000  0000 */
"/,"
"  '               33-36 37-40   41-44 45-48   49-52 53-56   57-60 61-64',/"
"/"              /* 0000  0000    0000  0000    0000  0000    0000  0000 */
"/,"
"  'Confirms:  CRB  1--4  5--8    9-12 13-16   17-20 21-24   25-28 29-31',/"
"/"           /* 0  0000  0000    0000  0000    0000  0000    0000  000  */
"/,"
"  'Force Bits:         HI/TO     1  2  3  4    5  6  7  8    9 10',/" 
"/,"                 /*   0       0  0  0  0    0  0  0  0    0  0 */
"/,"
/*           1         2         3         4         5         6         7         8 */
/*  12345678901234567890123456789012345678901234567890123456789012345678901234567890 */
"2a,'MOVA enabled. .    Warmup. . . . .    Multi stage . .    On control. . .  ',2a,/,"
"  '         Demanded stage . . .    Watchdog . . .    Error count . . .  ',/"
",\\)"
};


/* IRH MOD: M5.0.0: 11/08/04: Removed dataset handling functions and replaced with
 * option to go to dataset handling sub-menu */

const char *const FMenu = { /* CAUTION: FMenu also used by UserBig.c */
/*           1         2         3         4         5         6         7         8 */
/*  12345678901234567890123456789012345678901234567890123456789012345678901234567890 */

#if defined (PC_WRAPPER)
/* IRH MOD: PCMOVA: 21/04/05: Removed 'SIEMENS' from title */
"4a, '                                 MOVA MAIN MENU                    ',2a,/,"
#else
"4a, '                          SIEMENS MOVA MAIN MENU                   ',2a,/,"
#endif

"  'DS - DataSet menu        TM - Display TMA Logs        VM - View MOVA Messages ',/,"
"  '     for operations:     SF - Saturation flow log     DF - Display Flows      ',/,"
"  '     Display, Upload,    AL - ALert Enable/Disable    CT - Check/set Time     ',/,"
"  '     Download            DE - Display Error Log       VE - Mova Kernel Version',/,"
"  '                         CE - Clear Error Log         FI - FInish             ',/,"
",\\)" 
};

/* IRH MOD: M5.0.0: 07/10/04: Allow the user to force stage 10 */
/* IRH MOD: 30/01/06: BUGFIX: M5_CRN0029. changed "M to enabled" to "M to enable" */
static const char *const FLKeys = {
/*           1         2         3         4         5         6         7         8 */
/*  12345678901234567890123456789012345678901234567890123456789012345678901234567890 */
"4a, '                     MOVA COMMISSIONING SCREEN OPTIONS             ',2a,/,"
"  'Press:   M  to enable or disable MOVA;      C  to set MOVA on or off control;',/,"
"  '         R  to refresh the whole screen;    X  to exit Commissioning Screen; ',/,"
"  '         Z  to zero the error count;                                         ',/,"
"  '         1,2,3...0 to force stage 1 to 10 (or F to cancel current force);   ',/,"
"  '                                                                             ',/,"
",\\)"
};

/*                    Screen Position:           1             234567890123456 789*/
/*                      String Length:12   34567890   1234567890123456789012345   678*/
static const char sPrompt[2][39] = { " \x1B[23;1H \x1B[33;40;1m Enter Option: \x1B[0m",
                                     " \x1B[23;1H \x1B[33;40;1m  Press a Key: \x1B[0m"};
static const char sPosCur[3][9] = { "\x1B[23;17H", /* 0 chars entered */
                                    "\x1B[23;18H", /* 1 char  entered */
                                    "\x1B[23;19H"};/* 2 chars entered */

static const char sPosDets_[2][6] = { "[3;0H","[6;0H" };
static const char sPosConf[] = { "[9;0H" };
static const char sPosForc[] = { "[12;0H" };

static const char sPosME[] = { "[14;17H" };
static const char sPosWU[] = { "[14;36H" };
static const char sPosMS[] = { "[14;55H" };
static const char sPosOC[] = { "[14;74H" };
static const char sPosDS[] = { "[15;30H" };
static const char sPosWD[] = { "[15;48H" };
static const char sPosEC[] = { "[15;69H" };
static const char sPosMenu[] = { "[17;0H" };

unsigned char EngScreen(int LU)
{
    extern char control[NumberOfForce+NumberOfExtra];
    char cKey;
    char ReqOffControl;
    int lExitEngScreen;
    int i;
    unsigned char lJustEntered = TRUE;
    static const char sCursorOff[] = {" \x1B[CursorOff"};
    static const char sCursorOn[]  = {" \x1B[CursorOn"};

    cKey = '\0';           
    nForceReq = 0;
    nOldForce = 0;
    lExitEngScreen = FALSE;
    lJustEntered = TRUE;
    ReqOffControl = '\0';

    while ( !lExitEngScreen && !Tcomshr->io[31] != -1 )
    {
        if ( lJustEntered )
        {
            /* set to unlikely value to ensure written over */
            nOldWD = 99;
            nOldDS = 99;
            nOldEC = 99;   
            nOldME = 99;   
            nOldMS = 99;   
            nOldCR = 99;   
            nOldOC = 99;
            nOldWU = 99;
            OldShowMenuOnLookScreen = 99;

            memset(nOldin, 9, mxdets);
            memset(nOldstg, 9, mxconf);
            memset(nOldfrc, 9, sizeof(nOldfrc));
            /* also reset the detector change latches */
            memset(detsin_0s, 1, mxdets);
            memset(detsin_1s, 0, mxdets);
            MenuOptionChr[0] = ' ';
            MenuOptionChr[1] = ' ';
            MenuOptionLen = 0;
            /* switch cursor off */
            if(WRITE(LU,IOSTAT,NULL,2,FMT,F1a,1,STRG,sCursorOff,12,0))goto S990;
   
            /* clear screen */
            if (WRITE(LU,IOSTAT,NULL,2,FMT,F4a,1,
                STRG,sSP_Esc,2,STRG,sClrScr,10,
                STRG,sSP_Esc,2,STRG,sNormal,3,0))goto S990;

            /* Write skeleton screen */
            if (WRITE(LU,IOSTAT,NULL,2,FMT,ENG,1,
                STRG,sSP_Esc,2,STRG,sYellow,9,
                STRG,sSP_Esc,2,STRG,sNormal,3,
                STRG,sSP_Esc,2,STRG,sNormal,3,
                STRG,sSP_Esc,2,STRG,sNormal,3,0))goto S990;

            /* set the counter so that the default status message is displayed
            |  on the first pass, so that the screen is basically drawn from top
            |  to bottom. */
            nDefaultStatusMessageCounter = 1;

            lJustEntered = FALSE;
        }
   
        while (cKey == '\0')
        {
            /* action user's requests to force stages */
            TryForces(LU);
            /* update the dynamic parts of the screen */
            if(UpdateScreen(LU))goto S990;  /* check if error */

            /* Clear any status message after 5 sec */
            /* decrement timer */
            nDefaultStatusMessageCounter--;
            /* if timer has expired */
            if(nDefaultStatusMessageCounter <= 0)
            {
                /* (re)display the default status message to clear any old
                |  status message */
                if ( ShowMenuOnLookScreen )
                {
                    StatusMessage(LU,"--- Enter Option from the Menu  or  "
                        "Press <Space> for Screen Keys ---");
                }
                else
                {
                    StatusMessage(LU,"--- Press a key listed above  or  "
                        "Press <Space> for Main Menu ---");
                }
                /* also clear any pending request to switch MOVA off control
                |  since the user has not confirmed the request by pressing the
                |  key a second time before the default status message is
                |  displayed overwritting the instruction that asked them to
                |  press the key again. */
                ReqOffControl = '\0';
            }

            /* get key press if any */
            if ( LU == LOCAL_HANDSET )
            {
                cKey = GetHandsetChar();
            }
            else
            {
                cKey = CollectModemChar();
            }
            /* if no keys pressed */
            if ( cKey == '\0' )
            {
                /* short delay - screen updated and key presses scanned about
                |  every third of a second */

#if defined (PC_WRAPPER)
                /* IRH MOD: 24/02/05: PCMOVA: N.B. EngScreen() is called by 
                TRLUserIF(), not term()! I have made this sleep for 0.1 
                seconds, so that we see changes in detector state more often
                in PCMOVA, which may be running significantly faster than 
                real-time. */

                PCTasks_Sleep( 100/*333*/, MOVA_TASK_TRLUSERIF );
#else
                NU_Sleep(50);
#endif
            }
        }

        /* if the key entered is different from the one that the user needs to
        |  press again to confirm that they wish to put MOVA off control, then
        |  clear the request */
        if ( cKey != ReqOffControl ) ReqOffControl = '\0';
        
        /* if the full menu is being displayed */
        if ( ShowMenuOnLookScreen )
        {
            /* if the SPACE bar is pressed */
            if ( cKey == ' ' )
            {
                /* show the commissioning screen options instead */
                ShowMenuOnLookScreen = FALSE;
                /* clear any menu option which may have been half entered */
                MenuOptionLen = 0;
                MenuOptionChr[0] = ' ';
                MenuOptionChr[1] = ' ';
            }
            /* if a letter has been typed */
            else if ( ( cKey >= 'A' ) && ( cKey <= 'Z' ) )
            {
                /* add if less than two letters already entered */
                if ( MenuOptionLen < 2 )
                {
                   MenuOptionChr[MenuOptionLen++] = cKey;					
					Terrlog->buffer[1] = '\0';
                }
                /* otherwise just ignore it */
            }
            /* if (either) delete key has been pressed */
            else if ( ( cKey == cDelete ) || ( cKey == cBackSp ) )
            {
                /* delete last letter if any entered */
                if ( MenuOptionLen > 0 )
                {
                    MenuOptionChr[--MenuOptionLen] = ' ';
                }
                /* otherwise just ignore it */
            }
            /* if the RETURN key has been pressed */
            else if ( cKey == cCR )
            {
                /* if two letters have not been entered */
                if ( MenuOptionLen < 2 )
                {
                    StatusMessage(LU,
                        "Enter two-letter menu option, then press [ENTER]");
                }
                else
                {
                    /* copy the menu option from the temporary store*/
                    MenuOptionFromLookScreen[0] = MenuOptionChr[0];
                    MenuOptionFromLookScreen[1] = MenuOptionChr[1];
					/*Clear the input characters*/
					MenuOptionChr[0] = ' ';
					MenuOptionChr[1] = ' ';
					/* then exit (which displays its own status message) */
                    lExitEngScreen = TRUE;
                    /* but return to the commissioning screen when option is complete */
                    MenuViaLookScreen = TRUE;
                }
            }
            else
            {
                StatusMessage(LU,"Invalid Key Pressed.");
            }
        }
        else /* single key-press commissioning screen options */
        {
            switch (cKey)
            {
                case 'M': case 'C':
                {
                    /* if trying to enable MOVA or put it on control */
                    if ( ( ( cKey == 'M' ) && ( Tcomshr->io[27] == 0 ) )
                      || ( ( cKey == 'C' ) && ( Tcomshr->io[19] == 0 ) ) )
                    {
                        /* if the error count is too high */
                        if ( Tcomshr->io[16] >= 20 )
                        {
                            StatusMessage(LU,"Cannot enable MOVA:-   "
                                "Error count too high");
                        }
                        /* if the licence number is valid */
                        else if ( Valid_Licence() || check_facility_licence(LIC_MOVA))
                        {
                            if ( cKey == 'M' )
                            {
                                Tcomshr->io[27] = 1;
                                StatusMessage(LU,"MOVA control ENABLED");
                            }
                            else
                            {
                                Tcomshr->io[19] = 1;
                                Tcomshr->io[27] = 1;
                                StatusMessage(LU,"MOVA set to ON-control");
                            }
                        }
                        else
                        {
                            StatusMessage(LU,"Cannot enable MOVA:-   "
                                "Licence number is not valid");
                        }
                    }
                    else if ( ( Tcomshr->io[19] == 2 ) || ( Tcomshr->io[27] == 2 ) )
                    {
                        StatusMessage(LU,"MOVA control currently disabled:-   "
                            "you are testing Force channels");
                    }
                    else /* must be trying to disable MOVA (or put it off control) */
                    {
                        /* if MOVA is currently on control and this is not the
                        |  second time the key has been pressed */
                        if ( ( Tcomshr->io[19] == 1 )
                          && ( cKey != ReqOffControl ) )
                        {
                            StatusMessage(LU,"Press the key again if you sure "
                                "you want to put MOVA off control");
                            ReqOffControl = cKey;
                        }
                        else /* MOVA not on control or key pressed twice */
                        {
                            ReqOffControl = '\0';
                            /* clear the appropriate flag */
                            if ( cKey == 'M' )
                            {
                                Tcomshr->io[27] = 0;
                            }
                            else
                            {
                                Tcomshr->io[19] = 0;
                                
                                /* legitimate reason for MOVA going off control so
                                |  clear the timer - see MOVA_Monitor() */
                                MOVA_IF.MOVA_Off_Timer = 0;
                            }
                            /* need to actually clear the force bits explicitly */
                            for(i=1; i<=mxstag+2; i++)
                            {
                                control[i-1] = Terrlog->stgoff;
                            }
                            Switch_MOVA_Off(NO_SAV_MESSAGES, NO_REBOOT);
                            /* now inform the user (done last as it may take a
                            |  while and so make it more likely to get interrupted) */
                            if ( cKey == 'M' )
                            {
                                StatusMessage(LU,"MOVA control DISABLED");
                            }
                            else
                            {
                                StatusMessage(LU,"MOVA set to OFF-control");
                            }
                        }
                    }
                }
                break;

                case 'Z':
                {
                    StatusMessage(LU,"Error count zeroed");
                    /* reset the MOVA error count */
                    Tcomshr->io[16] = 0;
                    /* also clear the soft error and watchdog fail counters so the unit */
                    /* does not hard error unless another set of errors occur. */
                    soft_error_count = 0;
                    IC_WATCHDOG_FAIL_COUNT = 0;
                }
                break;
                
                
                /* IRH MOD: M5.0.0: 07/10/04: Allow the user to force stage 10 */
                /* User wants to force a particular stage (0-9, 0 = stage 10)*/
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9': 
                {
                    if(LU == 3)
                    {                              
                        /* If the user pressed 0, interpret this as stage 10 */
                        if ( cKey == '0' )
                        {
                            cKey += 10;
                        }
                              
                        /* If the key press represents a valid stage */
                        if (cKey - '0' <= Tcomshr->stages)
                        {
                            nForceReq = cKey - '0';
                        }
                        else
                        {
                            StatusMessage(LU,"Attempt to force invalid stage ignored");
                        }
                    }
                    else
                    {
                        StatusMessage(LU,
                            "Trying forces is NOT PERMITTED from instation");
                    }
                }
                break;
                
                /* IRH MOD: M5.0.0: 07/10/04: Use 'F' to cancel forces, not 0 */
                /* User wants to cancel any current forces */
                case 'F':
                {
                    if(LU == 3)
                    {
                        nForceReq = 0;
                    }
                    else
                    {
                        StatusMessage(LU,
                            "Trying forces is NOT PERMITTED from instation");
                    }                 
                 
                }
                break;

                case ' ':
                {
                    /* show the full main menu instead */
                    ShowMenuOnLookScreen = TRUE;
                }
                break;

                case 'R':
                {
                    /* redraw the whole screen as though we had just started */
                    lJustEntered = TRUE;
                }
                break;

                case 'X':
                {
                    /* then exit (which displays its own status message) */
                    lExitEngScreen = TRUE;
                    /* and don't return to the commissioning screen unless
                    |  explicitly requested again */
                    MenuViaLookScreen = FALSE;
                }
                break;

                default:
                {
                    StatusMessage(LU,"Invalid key pressed.");

                }
            }
        }
        cKey = '\0';
    }
   
    /* Ending this facility */
    nForceReq = 0;
    TryForces(LU);
    /* if just entered a menu command from the commissioning screen */
    if ( MenuViaLookScreen )
    {
        /* then exit quietly as the user will be returned to the commissioning
        |  screen when the entered option has been executed */
    }
    else
    {
        /* inform the user... */
        StatusMessage(LU,"Exiting Commissioning Screen ... ");
#if defined (PC_WRAPPER) 
        /* IRH MOD: 24/02/05: PCMOVA: N.B. EngScreen() is called by 
        TRLUserIF(), not term()! */
        PCTasks_Sleep( 267, MOVA_TASK_TRLUSERIF );
#else
        NU_Sleep(40); /* .5s */
#endif
    }
    /* switch cursor back on */
    if(WRITE(LU,IOSTAT,NULL,2,FMT,F1a,1,STRG,sCursorOn,11,0))goto S990;
    if(WRITE(LU,IOSTAT,NULL,2,FMT,F2a,1,
             STRG,sSP_Esc,2,
             STRG,sClrScr,10,0))goto S990;
    return(0);
S990:
    return(1);
}

/* IRH MOD: M5.0.0: 21/09/04: Use a defined constant for number of dets per line*/
#define gnDETECTORS_PER_LINE    (32)

int UpdateScreen(int LU)
{
   /* Engineer's screen */
   extern char confin[mxconf];
   extern char control[NumberOfForce+NumberOfExtra];
   int i,j,k,nL,nP,nQ;
   char sPosDets[6];
   int m;
   
/* IRH MOD: M5.0.0: 21/09/04: New strings for commissioning screen data */
#if 1

   static const char *const szDetectorData = { 
       "(2a,'            ',4(i5,3i1,i3,3i1),\\)" };
   static const char *const szConfirmData = { 
       "(2a,'         ',i5,i3,3i1,3(i3,3i1,i5,3i1),i3,2i1\\)" };
   static const char *const szForceData = {        
       "(2a,'                ',i7,'   ',2(i5,3i3),i5,i3\\)" };       
#else
   static const char *const F94 = { "(2a,'          ',4(i5,3i3),\\)" };
   static const char *const F95 = { "(2a,'               ',i7,'  ',2(i5,3i3),\\)" };   
   static const char *const F96 = { "(3a,\\)" };
#endif   
   
   static const char *const F97 = { "(2a,i2,\\)" };

   nL = (Tcomshr->ndets-1) / gnDETECTORS_PER_LINE;
   m = 0;
   nP = 0;
   
   /* for each line */
   for ( i=0; i<=nL; i++ )
   {
      /* for the 32 detectors on this line */
      for(j=0; j<gnDETECTORS_PER_LINE; j++)
      {
         /* The following code ensures that short changes to the opposite state
         |  are always displayed on the screen, even if the detector has since
         |  returned to the original state. */
         /* NB: The conditions below also ensure that a change is seen when the
         |      old state is '9' on the first pass through so all the detectors
         |      are displayed for the first time. */
         /* if the detector was not displayed as a '1' last time
         |  but it has been seen as a '1' since then */
         if ( ( nOldin[nP] != 1 ) && ( detsin_1s[nP] != 0 ) )
         {
            /* set the state of the detector to '1' */
            nOldin[nP] = 1;
            /* reset this detector's latches */
            detsin_1s[nP] = 0;
            detsin_0s[nP] = 1;
            /* and remember that at least one detector has changed state */
            m = 1;
         }
         /* else if the detector was not displayed as a '0' last time
         |  but it has been seen as a '0' since then */
         else if ( ( nOldin[nP] != 0 ) && ( detsin_0s[nP] == 0 ) )
         {
            /* set the state of the detector to '0' */
            nOldin[nP] = 0;
            /* reset this detector's latches */
            detsin_1s[nP] = 0;
            detsin_0s[nP] = 1;
            /* and remember that at least one detector has changed state */
            m = 1;
         }
         /* increment the detector number counter */
         nP++;
      }

      /* if a change has been seen in one or more detectors on this line */
      if(m == 1) 
      {
         nQ = i * gnDETECTORS_PER_LINE;
         /* IRH MOD: M5.0.0: 06/08/04: strncpy safer - does not assume null-terminated */
         strncpy(sPosDets, sPosDets_[i], sizeof( sPosDets ) );
         /*strcpy(sPosDets, sPosDets_[i]);*/
         WRITE(LU,IOSTAT,NULL,2,FMT,szDetectorData/*F94*/,1,
               STRG,sSP_Esc,2,
               STRG,sPosDets,5,MORE);
         WRITE(MORE);
         /* display the 'old' state just set-up by the above */
         for(k=1; k<=gnDETECTORS_PER_LINE; k++) {
            WRITE(MOVA_BYTE,nOldin[nQ++],MORE);
         }
         if(WRITE(0))goto S990;
         m = 0;
      }
   }
/*
 ---------------------- SCAN STAGE/PHASE/RTC CONFIRMATIONS ------------
*/
   m = 0;
   i = 0;
   
   /* IRH MOD: SIE_25: 07/03/05: BUGFIX! Check for a confirm change for all 31
    * confirms and the CRB, not just the first 16 as previously. */
   for(j=1; j <= mxconf; j++) {
      if(confin[j-1] != nOldstg[j-1]) {
         m = 1;
         nOldstg[j-1] = confin[j-1];
      }
   }
   if(m == 1) {
      WRITE(LU,IOSTAT,NULL,2,FMT,szConfirmData/*F94*/,1,
               STRG,sSP_Esc,2,
               STRG,sPosConf,5,MORE);
               
      /* Write out CRB (at end of confin[] array) */
      WRITE(MOVA_BYTE,confin[mxconf-1],MORE);          /*  CRB   */
      
      /* Write out 31 confirms (at start of confin[] array) */
      for(k=0; k<mxconf-1; k++) {
         WRITE(MOVA_BYTE,confin[k],MORE);
      }
      if(WRITE(0))goto S990;
      m = 0;
   }
/*
----------------------------- FORCE STAGES ----------------------------
*/
   m = 0;
   for ( j=0; j<(mxstag+1); j++ )
   {
       if ( control[j] != nOldfrc[j] )
       {
           m = 1;
           nOldfrc[j] = control[j];
       }
   }
   
   if(m == 1) {
       WRITE(LU,IOSTAT,NULL,2,FMT,szForceData/*F95*/,2,
                STRG,sSP_Esc,2,
                STRG,sPosForc,6,MORE);
                
       /* Write out HI/TO bit (first bit after 10 forces) */
       WRITE(MOVA_BYTE,control[mxstag],MORE);    /* HI/TO */
       
       /* Write out forces (only as many as there are stages) */
       for(k=0; k<Tcomshr->stages; k++ )
          WRITE(MOVA_BYTE,control[k],MORE);
          
       if(WRITE(0))goto S990;
       m = 0;
   }


/*
----------------------------- FLAGS -----------------------------
*/
   if ( nOldME != Tcomshr->io[27] )
   {
     if(WRITE(LU,IOSTAT,NULL,2,FMT,F97,1,
              STRG,sSP_Esc,2,
              STRG,sPosME,7,
              MOVA_BYTE,Tcomshr->io[27],0))goto S990;
     nOldME = Tcomshr->io[27];
   }
   if ( nOldWU != CI_get_warmup_counter() /*Tcomshr->warmup*/ )
   {
     if(WRITE(LU,IOSTAT,NULL,2,FMT,F97,1,
              STRG,sSP_Esc,2,
              STRG,sPosWU,7,
              INT2,CI_get_warmup_counter() /*Tcomshr->warmup*/,0))goto S990;
     nOldWU = CI_get_warmup_counter() /*Tcomshr->warmup*/;
   }
   if ( nOldMS != Tcomshr->io[30] ) /* check this */
   {
     if(WRITE(LU,IOSTAT,NULL,2,FMT,F97,1,
              STRG,sSP_Esc,2,
              STRG,sPosMS,7,
              MOVA_BYTE,Tcomshr->io[30],0))goto S990;
     nOldMS = Tcomshr->io[30];
   }
   if ( nOldOC != Tcomshr->io[19] )
   {
     if(WRITE(LU,IOSTAT,NULL,2,FMT,F97,1,
              STRG,sSP_Esc,2,
              STRG,sPosOC,7,
              MOVA_BYTE,Tcomshr->io[19],0))goto S990;
     nOldOC = Tcomshr->io[19];
   }
   if ( nOldDS != Tcomshr->io[24] )
   {
     if(WRITE(LU,IOSTAT,NULL,2,FMT,F97,1,
              STRG,sSP_Esc,2,
              STRG,sPosDS,7,
              MOVA_BYTE,Tcomshr->io[24],0))goto S990;
     nOldDS = Tcomshr->io[24];
   }
   if ( nOldWD != Tcomshr->io[18] )
   {
     if(WRITE(LU,IOSTAT,NULL,2,FMT,F97,1,
              STRG,sSP_Esc,2,
              STRG,sPosWD,7,
              MOVA_BYTE,Tcomshr->io[18],0))goto S990;
     nOldWD = Tcomshr->io[18];
   }
   if ( nOldEC != Tcomshr->io[16] )
   {
     if(WRITE(LU,IOSTAT,NULL,2,FMT,F97,1,
              STRG,sSP_Esc,2,
              STRG,sPosEC,7,
              MOVA_BYTE,Tcomshr->io[16],0))goto S990;
     nOldEC = Tcomshr->io[16];
   }
   if ( nOldCR != Tcomshr->io[21] )
   {
	   nOldCR = Tcomshr->io[21];
   }

/*
----------------------------- MENU -----------------------------
*/
   if ( ShowMenuOnLookScreen != OldShowMenuOnLookScreen )
   {
        /* if the full menu should be displayed */
        if ( ShowMenuOnLookScreen )
        {
            /* display the full menu */
            if (WRITE(LU,IOSTAT,NULL,2,FMT,FMenu,1,
                STRG,sSP_Esc,2,STRG,sPosMenu,6,
                STRG,sSP_Esc,2,STRG,sYellow,9,
                STRG,sSP_Esc,2,STRG,sNormal,3,0))goto S990;
        }
        else
        {
            /* display the single keystroke LOOK screen option */
            if (WRITE(LU,IOSTAT,NULL,2,FMT,FLKeys,1,
                STRG,sSP_Esc,2,STRG,sPosMenu,6,
                STRG,sSP_Esc,2,STRG,sYellow,9,
                STRG,sSP_Esc,2,STRG,sNormal,3,0))goto S990;
        }
        OldShowMenuOnLookScreen = ShowMenuOnLookScreen;
        /* force the default status message to be displayed since it is
        |  different for each of the menus above */
        nDefaultStatusMessageCounter = 1;
   }

/*
----------------------------- CURSOR -----------------------------
*/
   /* put cursor in bottom left corner and display current menu option, if any */
   MenuOptionChr[2] = '\0'; /* ensure its a null-terminated string */
   if ( ShowMenuOnLookScreen )
   {
       if(WRITE(LU,IOSTAT,NULL,2,FMT,F2a,1,
                STRG,sPrompt[0],38,
                STRG,&MenuOptionChr[0],2,0))goto S990;
   }
   else
   {
       if(WRITE(LU,IOSTAT,NULL,2,FMT,F2a,1,
                STRG,sPrompt[1],38,
                STRG,&MenuOptionChr[0],2,0))goto S990;
   }
   /* put cursor at the end of the entered option */
   if(WRITE(LU,IOSTAT,NULL,2,FMT,F1a,1,STRG,sPosCur[MenuOptionLen],8,0))goto S990;

   goto S991;
S990:
   return(1);
S991:
   return(0);
}

void StatusMessage(int LU, const char* cMessage)
{
   static const char sStatPos[] = { "\x1B[24;0H" };
   static const char sWhiteBlue[] = { "\x1B[44;1m" };
   static const char strNormal[] = {"\x1B[0m"};
   int nLen, nPad;
   char text[80];

   /* ensure built up string is always null-terminated */
   memset(&text[0],'\0',sizeof(text));

   /* obtain the length of the given message */
   nLen = (int)strlen(cMessage);

   /* put spaces in front so the message appears in the middle */
   nPad = 0;
   while ( nPad+nLen+nPad < 78 ) text[nPad++] = ' ';

   /* put the given message in the buffer (upto 78 chars just in case) */
   strncat(text,cMessage,78);
   nLen = (int)strlen(text);

   /* add spaces to the end to make a complete line which should always
   |  overwrite any old display */
   while ( nLen < 78 ) text[nLen++] = ' ';
   
   /* draw the status message */
   if (WRITE(LU,IOSTAT,NULL,2,FMT,F4a,1,
       STRG,sStatPos,7,     /* move to status message position */
       STRG,sWhiteBlue,7,   /* define its colour */
       STRG,&text[0],nLen,  /* display the constructed status message line */
       STRG,strNormal,4,      /* return colours to normal */
       0))goto S990;

   /* put the cursor back to the end of the entered menu option, if any */
   if(WRITE(LU,IOSTAT,NULL,2,FMT,F1a,1,STRG,sPosCur[MenuOptionLen],8,0))goto S990;
   
   /* hold message up for at least 0.8 s */
   /* NU_Sleep(120); */
   /* Ref Siemens fault report MOVA-1 */
   /* Above delay removed and instead the counter controlling when the default */
   /* status message is displayed is simply reset. */
   nDefaultStatusMessageCounter = 15;
   
S990:;
}
void TryForces(int LU)
{
	extern char control[NumberOfForce+NumberOfExtra];
   int i;
   static int nScanCount = 0;

   if (nForceReq != nOldForce)
   {
      if (Tcomshr->io[27] == 1)
      {
         StatusMessage(LU,"MOVA enabled:- disable before trying forces");
         nForceReq = 0;
      }
      else if (Tcomshr->io[30] == 1 && nForceReq != 0)
      {
         StatusMessage(LU,
            "Multiple stage confirms being received: cannot test forces");
      }
      else
      {
         if (nForceReq == 0)
         {
            for ( i=0; i<=mxstag+1; i++ ) control[i] = 0;
            Switch_MOVA_Off(NO_SAV_MESSAGES, NO_REBOOT);
            Tcomshr->io[19] = 0;            
            Tcomshr->io[27] = 0;
            StatusMessage(LU,"Testing of forces terminated");
         }
         else
         {
            char text[80];
            
            Tcomshr->io[19] = 2;
            Tcomshr->io[27] = 2;
            
            if ( nOldForce == 0 )
            {
               control[nForceReq-1] = 1;
               control[mxstag] = 1;    /* HI */
               control[mxstag+1] = 1;  /* TO */
               OutputScan(MOVA_JUST_ON);
               
               /* IRH MOD: M5.0.0: 07/10/04: Now supporting stage 10 */
               sprintf( text, "Testing of forces started - forcing stage %d", 
                        nForceReq );
               /*strcpy(text,"Testing of forces started - forcing stage X");
               text[strlen(text)-1] = '0'+nForceReq;*/
               
               StatusMessage(LU,text);
            }
            else
            {
               control[nForceReq-1] = 1;
               control[nOldForce-1] = 0;
               OutputScan(CHANGE_OF_STAGE);
               
               /* IRH MOD: M5.0.0: 07/10/04: Now supporting stage 10 */
               sprintf( text, "Testing forces - Now forcing stage %d", 
                        nForceReq );                        
               /*strcpy(text,"Testing forces - Now forcing stage X");
               text[strlen(text)-1] = '0'+nForceReq;*/
                              
               StatusMessage(LU,text);
            }
         }
      }
      nScanCount = 0;
      nOldForce = nForceReq;
   }
   /* no change and still forcing a stage */
   else if ( nForceReq != 0 )
   {
      nScanCount++;
   }

   /* if forcing a stage for more than about a minute */
   if ( ( nForceReq != 0 ) && ( nScanCount >= 200 ) )
   {
      for ( i=0; i<=mxstag+1; i++ ) control[i] = 0;
      Tcomshr->io[19] = 0;
      Tcomshr->io[27] = 0;
      Switch_MOVA_Off(NO_SAV_MESSAGES, NO_REBOOT);
      nForceReq = 0;
      nOldForce = 0;
      StatusMessage(LU,"Testing of forces terminated - forcing same stage for too long");
   }
}
