/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         phone.c
*
*  TITLE:		Remote connection via phone line
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	22-Apr-1988		1.0			..		CL		......			
*	18-May-1988		1.2			..		CL		......			changes to NUMERR, 10=error, 100=abort       
*	27-May-1988		1.3			..		CL		......			check IOSTAT in FORTRAN READ                 
*	08-Nov-1988		2.0			..		CL		......			change to READ only when conected 
*	14-Nov-1988		2.0			..		CL		......			sign off if 1st pass has non-ASCII
*	15-Nov-1988		2.0			..		CL		......			have 15 sec dead time on answer  
*	10-Dec-1988		2.0			..		CL		......			change LUNO to timeout after 20 odd sec
*	03-Mar-1989		2.1			..		CL		......			Change READ to assembler subroutine SINN
*	06-Mar-1989		2.1			..		CL		......			remove sign off if 1st pass non ASCII 
*	10-Mar-1989		2.1			..		CL		......			Wait 10 sec to restart after error 
*	10_Apr-1989		2.2			..		CL		......			Change to initialise DLY every time used 
*	14-Apr-1989		2.2			..		CL		......			Change to CODE numbers + remove NUMERR       
*	27-Apr-1989		2.3			..		CL		......			Mods to OPEN statements; add LUN             
*	05-May-1989		2.4			..		CL		......			Mods to BUFFER and Phone Flag initialisation  
*	26-Jan-1998		3.0			..		MC		SIE_00			First undergoing system test
*	03-Feb-1996		3.0			..		MC		SIE_01			Uses Tcomshr->io[29]=98 to tell phone to drop the line
*	17-Feb-1998		3.0			..		MC		SIE_02			Initialize WRITE routine.Tcomshr->io[31] set to give 40 min timeout on LOOK 
*	09-Feb-1998		3.0			..		MC		SIE_03			Use SendStringToLH() when modem connected	
*	24-Feb-1998		3.0			..		MC		SIE_04			Added lModemTried so term() can process terminal i/p 
																whilst phone is delivering  'busy ... ' message
*	10-Mar-1998		3.0			..		MC		SIE_05			EngScreen() returns error code
*	24-Apr-1998		3.0			..		MC		SIE_06			Changed the text within sOP_String
*	24-Apr-1998		3.0			..		MC		SIE_07			Initialise modem buffer pointers from here on startup
*	27-Apr-1998		3.0			..		MC		SIE_08			Type of WRITE() format strings changed
*	29-Apr-1998		3.0			..		MC		SIE_09			'const' added to initialised static's
*	30-Apr-1998		3.0			..		MC		SIE_10			Modified F91 and F92 to improve the user i/f
*	03-May-1998		3.0			..		MC		SIE_11			Overlay on 'Xcomshr', etc., replaced by explicit use of structures.
*	13-Jun-1998		3.0			..		MC		SIE_12			Added '\r' to sOP_String since '\n' is only line feed (without carriage return)
*	12-Jan-2000		4.0.0		..		PC		SIE_13			sOP_String renamed local_con_text.	
*	09-Mar-2000		4.0.0		..		PC		SIE_14			Corrected the code which handles the delete key.
*	15-Mar-2000		4.0.0		..		PC		SIE_15			Removed code which detected CTRL-Z as all CTRL keys are removed by CollectModemChar().
*	03-May-2000		4.0.0		..		PC		SIE_16			Commissioning screen is no longer called from here, see UserBig.c
*	24-May-2000		4.0.0		..		PC		SIE_17			Shortened the 5s delay after 'Please wait' since user has already pressed 
*																[return] to switch to the MOVA unit.
*	29-Sep-2000		5.0.0		..		IH		SIE_18			Reading from remote connection prevented if site data is being downloaded
*	05-May-2011		7.0.0		AA		PK		CRN003			New Header file inline with MOVA Kernel SQP
*
*  (c) Copyright TRL Ltd 2010. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal or
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/

#include <nucleus.h>
#include <string.h>
#include <proj_def.h>
#include "gbltypes.h"
#include "obclock.h"
#include "error.h"    /* TRL/SIE: for SOFT_ERROR calls */
#include "errhand.h"  /* TRL/SIE: for MOVA error handler */
#include "datahand.h" /* IRH MOD: M5.0.0: 23/09/04: Added */
#include "write.h"
#include <diff_bss.h>
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
  PHONE SCANS THE AUTO-DIAL INPUT ON PORT 2 AND ACTIVATES "USER"
                WHEN AN ENTRY IS MADE.
  THE PROGRAM PUTS INPUT INTO GLOBAL "BUFFER(2-20)"; BUFFER(1)=2
        USER RESETS BUFFER TO ZERO WHEN FINISHED
       BUFFER(1) CONTAINS INFO FROM PHONE/TERL TO USER
         WAIT UNTIL IT IS 0 BEFORE READING IN DATA
   IO(30) CONTROLS THE PHONE LINE = 99 WHEN CONNECTED; =0 WHEN NOT
                                  = 9 AFTER USER 'FI'
 ----------------------------------------------------------------------
*/

extern unsigned char EngScreen(int LU);

/* IRH MOD: M5.0.0: 23/09/04: MOVA 5: No longer used.  
 * Downloading information stored in Datahand.c module */
/*extern char lReadingSiteData;*/

extern char lMessOn;      /* Stop phone operation when messages on */
extern unsigned char lLineDropped;
extern void ResetModemBuf(void);
unsigned char lModemTried = FALSE;       /* To allow term to prosess i/p as if phone not used */


/* function prototype */
void phone (UNSIGNED argc, void * argv);

void phone(UNSIGNED argc, void * argv)
{
	extern char CollectModemChar( void );
	extern unsigned char lLH_CONNECTED;        /* Disallow phone session if handset plugged in */
	/* IRH MOD: M5.0.0: 14/09/04: Initialise to all null chars */
	static char n[20];
	static int abo,code,errp,ans,lun,j,i;
	static char ivar;

	static const char *const F91 = {
		"(/,'Please wait...')"
	};
	static const char *const F92 = {
		"('MOVA remote communications - please press Return')"
	};

	static const char *const local_con_text = {"\r\nLocal handset connected - try again later...\r\n\r\n"};
	extern const char cCR;
	extern const char cBackSp;
	extern const char cDelete;
	extern const char sDelChar[];

    errp = 0;
    ivar = 0;                      /* /DINOUT/*/

	/*
	=======================================================================
	--------------------------- initialisation ----------------------------
	*/
    Tcomshr->io[29] = 0;           /* M 2.1*/
    Terrlog->buffer[0] = 0;        /* M 2.4*/
S1:
	/*
	------------------------------ restart here after error ---------------
	*/

    abo = 980;
    ans = 0;
    lun = 0;
    ResetModemBuf();

S10:
	/*
	=======================================================================
	------------------------------ main loop ------------------------------
	*/
    NU_Sleep(75); /* @6.67ms/tick = 0.5s */

S12:

    if(Tcomshr->io[29] != 99) 
	{    /* Dont READ if not connected or finished*/
        ans = 1;                   /* Set 'first-pass' flag*/
        goto S10;
    }
	/*
	 ------ continue when ANSWER program answers the line - IO(30)=99 -----
	*/
    lun = 1;
    code = 90;
      
	/*
	 -------------- activate phone program --------------------------------
	*/
    if(ans != 1) goto S14;         /* dont READ if BUFFER full*/
	/*
	 ----- here when 1st phoned - 15 sec tone to put off wrong numbers ----
	*/

    code = 91;
    
    /* send message if LH in use */
    if ( lLH_CONNECTED != 0 )
    {
       /*WRITE(2,IOSTAT,NULL,2,FMT,F90,1,0); can't use as LH might be in middle of using it*/ 
       lModemTried = TRUE;
       SendStringToModem(local_con_text, strlen(local_con_text));
       NU_Sleep(700); /* time to deliver message */
       Tcomshr->io[29] = 98; /* force line to drop ( see answer() ) */
       while ( MOVA_IF.RComms == MOVAIF_COMMS_TO_MOVA ) NU_Sleep(50);
       lModemTried = FALSE;
       goto S12;
    }
    
    WRITE(INITIALIZE);
    if(WRITE(2,IOSTAT,NULL,2,FMT,F91,1,0)) goto S980;
    /* TRL/SIE: changed unit to 2 to direct o/p to modem */
    lun = 0;
  
    NU_Sleep(300); /* shorten delay for combined OMU/MOVA unit */
    lun = 1;
    code = 92; 
    code = 93;
    if(WRITE(2,IOSTAT,NULL,2,FMT,F92,1,0)) goto S980;
    /* TRL/SIE: changed unit to 2 to direct o/p to modem */

S14:
	/*
	 zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz
	*/

   j = 0;
   ivar = 0;
   for ( i=0; i<20; i++ )
      n[i] = 0;

   while ( ivar != cCR && j < 20 )
   {
      /* wait here until messages are off */
      /* IRH MOD: M5.0.0: 05/09/04: Stop reading from remote connection when reading site data */
      /* IRH MOD: M5.0.0: 23/09/04: Now using getting download status from Datahand module */
      while ( lMessOn || Datahand_IsDownloadingSiteData( ) /*lReadingSiteData*/ )
      {
         NU_Sleep(75);
      }
      if ( lLineDropped )
      {
          /* IRH MOD: M5.0.0: 17/09/04: Reset Terrlog->buffer[0] - 
           * just in case code is using this (instead of 
           * lLineDropped) to indicate whether a connection
           * is still active. */
          Terrlog->buffer[0] = 0;
          goto S12; /* line dropped */
      }
      
      ivar = CollectModemChar();       
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
                SendStringToModem( &sDelChar[0], 3);
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
         NU_Sleep(20); /* @6.67ms/tick = 0.125s */
   }
    
   /* IRH MOD: M5.0.0: 14/09/04: If the last character entered 
    * was a CR/LF discard it (may not be if user
    * entered more characters than can fit in buffer - 
    * i.e. if j == 20.) */
   if ( n[j-1] == '\n' || n[j-1] == '\r' )
   {
       n[j-1] = '\0';
   }  

	/*
	 ---------CALL LOOK IF INPUT IS 'LOOK' --------------------------------
	*/

    /*
	 ----------------------------------------------------------------------
	*/
    lun = 0;    
    Terrlog->buffer[0] = 2;        /* good read - put 2 in BUFFER*/
    i = 0;                         /* M 1.3*/

S18:

    i = i+1;                       /* M 1.3*/
    Terrlog->buffer[(long)i] = n[i-1];/* fill BUFFER*/
    /* IRH MOD: M5.0.0: 14/09/04: Set to null chars, not spaces */
    n[i-1] = '\0';                  /* M 2.1 fill N array with spaces*/
    if(i < j) goto S18;            /* M 1.3*/
    ans = 0;                       /* clear 'first-pass' flag*/
    errp = 0;                      /* good read - clear error count*/
S20:
	/*
	------------------------------------ data read in OK ------------------
	*/
 
    NU_Sleep(60); /* @6.67ms/tick = 0.4s */
    if(Terrlog->buffer[0] != 0) goto S20;
    goto S12;                      /* USER finished - get next data*/

S980:

/* IRH MOD: M5.0.0: 02/09/04: If we had an error writing to the modem
 * just end the function - the user has probably (been) disconnected
 * in the middle of something, so we don't really want to send
 * anything to the error log in my opinion, as with Userbig.
 * Telling them that a 'PROGRAM ERROR' occurred isn't very enlightening
 * and hopefully it should be obvious what happened. */

#if 0 
	/*
	=======================================================================
	----------------------- error routine ---------------------------------
	*/
    tTimeDate = Com_read_rtc( READ_RTC);
    Terrlog->ierr[0][Terrlog->nerr-1] = (int)(21000L + tTimeDate.date);/* TRL/SIE: 21000L+dd; */
    Terrlog->ierr[1][Terrlog->nerr-1] = (int)(tTimeDate.month*100L+tTimeDate.year);/* TRL/SIE: mm*100L+yy; */
    Terrlog->ierr[2][Terrlog->nerr-1] = (int)(tTimeDate.hours*100L+tTimeDate.minutes);/* TRL/SIE: hr*100L+mn; */
    Terrlog->ierr[3][Terrlog->nerr-1] = code*100;
    Terrlog->nerr = Terrlog->nerr+1;
    if(Terrlog->nerr > 100) Terrlog->nerr = 1;

#endif    	
	/*
	--------------------------- need 10 consecutive error messages to abort
	*/

    errp = errp+1;
    if(errp < 10) 
	{
		NU_Sleep(1500); /* @6.67ms/tick = 10s */
        goto S1;
    }

	/*
	------------------------------------ 10 consecutive read errors - ABORT
	*/
    Terrlog->stgoff = 0;
    if(Tcomshr->stgdem == 0) Terrlog->stgoff = 1;
    for(i=1; i<=mxstag+2; i++)
	{
        Tdinout->dout[i-1] = Terrlog->stgoff;/* clear force bits*/
    }
    Tdinout->dout[17] = 1;
    Tcomshr->io[19] = 0;
    Tcomshr->io[27] = 0;           /* set LED and clear flags*/
    Switch_MOVA_Off(SAV_MESSAGES, RESTART_MOVA);
   
	/*
	-----------------------------------------------------------------------
	*/

}

