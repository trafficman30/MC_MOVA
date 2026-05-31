/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         userbig.c
*
*  TITLE:        MOVA Functionality Access: Navigating through mova menus/submenus
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description	
*	27-Apr-1988		1.0.0		..		CL		......			Initial Implementation
*	04-May-1988		1.1.0		..		CL		......			include display options,clear det led on CE
*	18-May-1988		1.2.0		..		CL		......			mods on startup to prevent locking up. ask for password every time
*	24-May-1988		1.2.1		..		CL		......			add 'end' after FI 
*	01-Jul-1988		1.3.0		..		CL		......			include version number 'M 2.0'
*	28-Sep-1988		1.6.0		..		CL		......			clear error count flag on clear error log write site-name when logging on
*	14-Nov-1988		2.0.0		..		CL		......			change to global - MAXMIN(12) -> (16)
*	07-Mar-1989		2.1.0		..		CL		......			New WAIT subroutine
*	10-Mar-1989		2.1.0		..		CL		......			Changes to passwords 
*	10-Mar-1989		2.1.0		..		CL		......			Wait 10 sec to restart after error
*	10-Apr-1989		2.2.0		..		CL		......			DLY initialised every time used 
*	13-Apr-1989		2.2.0		..		CL		......			New wait for input routine 
*	17-Apr-1989		2.2.0		..		CL		......			Correction to input routine 
*	27-Apr-1989		2.3.0		..		CL		......			Mods to ensure Logical unit only 2 or 3 
*	05-May-1989		2.4.0		..		CL		......			Mods to initialising BUFFER and OUTPUT Flag
*	25-May-1989		2.5.0		..		CL		......			MAIN MENU change only
*	30-Jun-1989		2.6.0		..		CL		......			add MASTER password on login and output passwords in change password routine
*	18-Aug-1989		2.6.0		..		CL		......			Change to reset time routine 
*	13-Jul-1990		2.7.0		..		CL		......			MAIN MENU change only due to MOVA update
*	23-Aug-1990		2.8.0		..		CL		......			MAIN MENU change only due to GENSTG update 
*	21-Jul-1993		4.0.0		..		CL		......			Major update for emergency/priority control
*	19-Jan-1994		4.0.0		..		RV		......			'Big' Extended stages, links etc 
*	01-Mar-1994		4.0.0		..		RV		......			Revised display/print of site data  
*	26-Jan-1998		4.0.0		..		MC		SIE_00			First undergoing system test
*	27-Jan-1998		4.0.0		..		MC		SIE_01			Code conventions changed
*	02-Feb-1998		4.0.0		..		MC		SIE_02			Disconnect modem message removed
*	17-Feb-1998		4.0.0		..		MC		SIE_03			Stop writes when tel line dropped. Timeout setting for remote comms
*	27-Feb-1998		4.0.0		..		MC		SIE_04			Copyright on startup.
*	27-Feb-1998		4.0.0		..		MC		SIE_05			Change-password sequence cleaned up.
*	02-Mar-1998		4.0.0		..		MC		SIE_06			DF headers corrected.
*	05-Mar-1998		4.0.0		..		MC		SIE_07			Prevent program error from setting io[19] to 0
*	10-Mar-1998		4.0.0		..		MC		SIE_08			Safe abort with prompt to abandon VM
*	11-Mar-1998		4.0.0		..		MC		SIE_09			Revised prompts when RS option used
*	11-Mar-1998		4.0.0		..		MC		SIE_10			Safer abort when terminal disconnected in RS option
*	24-Apr-1998		4.0.0		..		MC		SIE_11			Force data load if three new sets entered
*	24-Apr-1998		4.0.0		..		MC		SIE_12			Improved the text messages which appear when FInish is selected (Siemens fault report MOVA-10).
*	24-Apr-1998		4.0.0		..		MC		SIE_13			Initialisation of data (lMessOn lReadingSiteData)
*	27-Apr-1998		4.0.0		..		PC		SIE_14			Type of WRITE()'s format strings changed
*	28-Apr-1998		4.0.0		..		PC		SIE_15			Added 'Last50MessStored' accessible via option "DE".See Siemens fault report ref.MOVA-3
*	29-Apr-1998		4.0.0		..		PC		SIE_16			'const' added to initialised static's (Siemens fault report MOVA-13)
*	03-May-1998		4.0.0		..		PC		SIE_17			Overlay on 'Xcomshr', etc., replaced by explicit use of structures.
*	05-Jun-1998		4.0.0		..		PC		SIE_18			Modified Sin6() so that 'Control-C' can abort the download at any point.(Siemens fault report MOVA-24)
*	07-Jun-1998		4.0.0		..		PC		SIE_19			Fixed time check as hr, mins, secs are now used and not 'sec'.
*	26-May-1998		4.0.0		..		PC		SIE_20			Corrections to the 'CP' (change password)function so that less than 6 characters are rejected.
*	02-Jun-1998		4.0.0		..		PC		SIE_21			Modified F1923 so that the original text still appears within the new
*	19-Jun-1998		4.0.0		..		PC		SIE_22			Changed main menu text from 'M4.0.1' to 'M4.0.2' and added 'PB681 issue 2'
*	08-Feb-1999		4.0.0		..		PC		SIE_23			Changed main menu text F90:- 'M4.0.2' to 'M4.0.3' and 'PB681 issue 2' to 'PB681 issue 3'
*	16-Feb-1999		4.0.0		..		PC		SIE_24			Clearing the MOVA error and soft error count.(Siemens fault report MOVA-67)
*	13-Jan-2000		4.0.0		..		PC		SIE_25			Modified the 'Finish' case so it switches local communications back to the OMU application.
*	28-Feb-2000		4.0.0		..		PC		SIE_26			Minor change to the text in Finit
*	08-Mar-2000		4.0.0		..		PC		SIE_27			Removed unapplicable options from main menu (F90)
*	15-Mar-2000		4.0.0		..		PC		SIE_28			MOVA-72: Improved 'FI' so call is not dropped before text is output to the screen.
*	28-Mar-2000		4.0.0		..		PC		SIE_29			Mark1 set to 8888 by new common RTC functions so removed line from 'CT' path.
*	31-Mar-2000		4.0.0		..		PC		SIE_30			Changed password from GnPvEZ to GNPVEZ since only uppercase characters are ever read.
*	26-Apr-2000		4.0.0		..		PC		SIE_31			This application was still setting 'CD_IDENT_REQ' which may upset the OMU so deleted it.
*	26-Apr-2000		4.0.0		..		PC		SIE_32			In 'RS' option, if no plan number is entered, return user to the main menu (near S45) so the  user can abort this option.
*	27-Apr-2000		4.0.0		..		PC		SIE_33			Reset MOVA_IF.MOVA_Off_Timer when MOVA switched off control using set flags.
*	27-Apr-2000		4.0.0		..		PC		SIE_34			Improved the password systems.
*	03-May-2000		4.0.0		..		PC		SIE_35			Changes to allow menu options to be	entered while the commissioning screen is active.
*	04-May-2000		4.0.0		..		PC		SIE_36			Fixed bug in 'CN' - ensure ch4 is a null-terminated string before calling WRITE()
*	04-May-2000		4.0.0		..		PC		SIE_37			In 'RS' option, if more plans need to be loaded,unit no longer returns to the 'Enter plan number step but exits as normal (see S689).
*	24-May-2000		4.0.0		..		PC		SIE_38			Modified the error routine S980 so it no longer logs display problems in the error log and now just quietly reboots if the problem persists.			
*	12-Oct-2000		4.0.0		..		PC		SIE_39			Remove question asking user if he wishes to enter Summer Times, option specified using CKA and CKR.
*	29-Oct-2000		4.0.0		..		PC		SIE_40			Update commissioning screen menu, error handing and several bug fixes.				
*	12-May-2006		5.0.0		..		IH		SIE_41			PC-specific sleep function used to pause TRLUserIF thread Reinstated Tcomshr->mark1 (=8888) code to signal
*																that user has changed time/date in PCMOVA. Improved error handing and several bug fixes.
*	07-Mar-2005		5.0.3		..		RD		SIE_42			Added Cruise Speed data collection routine
*	03-Jan-2007		6.0.0		..		RD		SIE_42			Removed notification of "stage stick" flag as it is no longer necessary
*	31-May-2011		7.0.0		AA		PK		CRN003			Several Changes for Kernel 7 release
*	12-Dec-2011		7.0.1		AB		PK		CRN004			SatFlow and CANDET improvements
*	21-Feb-2012		7.0.2		AC		AK		CRN007			Hugemova.sa checksum issue
*	15-Mar-2012		7.0.3		..		AK		CRN009			Changes to conditional compilation flags
*	06-Jul-2012		7.0.3		AD		AK		CRN012			Memory leak in TMA module and statistics lost on dataset change
*	02-Oct-2012		7.0.4		AE		AK		CRN017			Typo in Alerts Menu
*	30-Nov-2012		7.0.5		..		AK		CRN018			Occupancy alert not resetting correctly
*	17-Dec-2012		7.0.5		..		PA		CRN019			Improve alerts interface
*	09-Jan-2013		7.0.5		..		AK		CRN020			Print leading zeros on dates and times
*	25-Jan-2013		7.0.5		..		AK		CRN023			Fix compiler warnings
*	31-Jan-2013		7.0.5		AF		AK		M7.0.5			Issue M7.0.5
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
#include <nucleus.h>
#include <string.h>
#include <ctype.h> /* IRH MOD: M5.0.0: 07/10/04: Added for toupper() */
#include <proj_def.h>
#include "gbltypes.h" 
#include "obclock.h"
#include "error.h"    /* TRL/SIE: for SOFT_ERROR calls */
#include "errhand.h"  /* TRL/SIE: for MOVA error handler */
#include "write.h"
#include <hardware.h> /* for mova_first_time_powerup_code */
#include <keptdata.h>
#include <diff_bss.h>
#include "datahand.h" /* IRH MOD: M5.0.0: 27/07/04: For new Datahand functionality */

/*M7 header files*/
#include "sf.h" 
#include "sfstats.h" 
#include "generalfunc.h"
#include "tma_alerts.h"
#include "tma_mem.h"
#include "select.h"
#include "getentry.h"
#include "tma_preprint.h"

#include "core_interface.h"

#include <ctype.h>
#include <string.h>
#include "kdebug.h"


/* IRH MOD: 30/11/05: PCMOVA: For PCError_ExitKernel */
#if defined (PC_WRAPPER)
    #include "../PCError.h"
	#include "sfdebug.h"
#endif

/*
 ******************************************************************
 1: subroutines amalgamated in the program due to bug in LINKER - 12/87
           USER NEEDS A 2400 BAUD TERMINAL IN PORT 0
            A 1200 BAUD SERIAL PRINTER IN PORT 1
            A 1200 BAUD BT PSN PHONE LINE IN PORT 2
       IO ARRAY (1-16) = FORCE BITS TO UTC INTERFACE M 4.0 - not used
       (17)=ERROR COUNT ; (18)=PHONE HOME  ; (19)=WATCHDOG COUNTER
       (20)=ONCONTROL   ; (21)=PRNTON      ; (22)=CONTROLLER READY FLAG
       (23)=FLOWON      ; (24)=STAGESTUCK  ; (25)=DEMSTA
       (26)=ASSTON      ; (27)=ERROR LOG   ; (28)=MOVON
       (29)=FLAG BETWEEN USER AND OUTPUT FOR DISPLAYING
       (30)=PHONE LINE CONNECTED(=99) , 0 IF NOT , 9 AFTER 'FI'
       (31)=1 IF MULTI-STAGE CONFIRMATIONS=UNABLE TO CONTROL, 0=OK.
       (32)=INCREMENTED WHILE PHONE LINE IN USE.
-----------------------------------------------------------------------
  THIS PROGRAM SUPPLIES THE MOVA FACILITIES TO REMOTE USERS EITHER VIA
  THE TELEPHONE LINE OR THE AUTO-DIAL/ANSWER MODEM. TWO PROGRAMS, TERM
  AND PHON HANDLE THE INPUTS VIA THA GLOBAL ARRAY BUFFER(20). THIS PROG
  SCANS BUFFER AND ACTS WHEN IT IS FILLED. THE FIRST ELEMENT OF THE BUF
  ARRAY CONTAINS THE LOGICAL UNIT NUMBER: 3 FOR THE TERMINAL, 2 FOR THE
  PHONE LINE. LOGICAL UNIT 1 IS THE PRINTER OUTPUT PORT (LOWER PORT ON
  FRONT PANEL). FACILITIES ARE IN THE FORM OF A MAIN MENU, AND A SECOND
  FLAGS MENU.
-----------------------------------------------------------------------
*/


/* function prototypes */
static void	EndLogs( void);

extern int nDisp;  /* from output task */
extern unsigned char lLH_CONNECTED; /* something plugged into handset? Set in Monitr() */
extern unsigned char lLineDropped;
extern unsigned char soft_error_count;
extern unsigned char IC_WATCHDOG_FAIL_COUNT;

extern char GetHandsetChar(void);
extern char CollectModemChar(void);
extern int WRITE(int, ... );
extern unsigned char EngScreen(int LU);
extern int MenuViaLookScreen;
extern char MenuOptionFromLookScreen[2];
extern int Last50MessStored;
extern TIMESTAMP_TYPE Com_read_rtc(int, ... );
extern char lIsClockSet;

extern char control[NumberOfForce+NumberOfExtra];

/* Menu Options */
typedef enum
{
	UI_OPTION_DS,
	UI_OPTION_TM,
	UI_OPTION_SF,
	UI_OPTION_AL,
	UI_OPTION_DE,
	UI_OPTION_CE,
	UI_OPTION_VM,
	UI_OPTION_DF,
	UI_OPTION_LF,
	UI_OPTION_LF_SET_FLAGS,
	UI_OPTION_LF_LOOK,
	UI_OPTION_LF_CLEAR_FORCE,
	UI_OPTION_LF_SET_FORCE,
	UI_OPTION_CT,
	UI_OPTION_FI,
	UI_OPTION_FL,
	UI_OPTION_CS,
	UI_OPTION_DM,
	UI_OPTION_CP,
	UI_OPTION_RT,
	UI_OPTION_VE,
	UI_MAIN_MENU,
	UI_RESET_UNIT,
	UI_LAST_STATE
} UI_Options_t;

/* System options */
typedef enum 
{
	SYSTEM_UI_INIT,
	SYSTEM_UI_RESET,
	SYSTEM_UI_DISPLAY_MAIN_MENU,
	SYSTEM_UI_LAST
} UI_System_t;

static int TimeResetRequired;

static const char sNormal[] = { "[0m" };

char lMessOn;           /* to tell term/phone that messages are on */
char lOptionDE;         /* to prompt for saved messages are required */

#define PASSWORD_SIZE 6

static char CurrentPassword[PASSWORD_SIZE];
static char DownloadPassword[PASSWORD_SIZE];
static unsigned char CurrentPaswwordInv[PASSWORD_SIZE];
static unsigned char DownloadPasswordInv[PASSWORD_SIZE];

/* Destination for user comms */
/* Modem (w == 2) or local handset (w == 3) */
static int CurrentDevice = 0;

int ShowCruiseSpeed = 0;
int* ShowSatFlow;

static int uerr = 0;

/*String should be around 62 chars but add in some extra */
static char MovaVersionOut[70];

	/* The menu now also appears on the commissioning screen (see term.c) */
	/* Therefore FMenu from term.c is used rather than F90 */
	extern const char *const FMenu;

	static const char *const sLookItem = { 
		   "(2a,'For commissioning screen, type LOOK',2a,//,"
			   "'ENTER OPTION . . .  ',\\)"
	};



	static const char *const F96 = { "(/)" };

	static const char *const F960 = {
		   "(/,' Enter day number {1=Mon,2=Tue,3=Wed,4=Thu,5=Fri',',6=Sat,7=Sun} ? ',\\)"
	};

	static const char *const F965 = {
	   "(' ',a,'                          LANE',/,' Time  ',24i3,/)"
	};

	static const char *const F966 = {
		   "(1x,i2,':30',1x,24i3)"
	};

	static const char *const F967 = {
		   "(1x,i2,':00',1x,24i3)"
	};

	static const char *const F92 = { "(/,' Press Return to continue')" };

static const char *const F99 = {
		"(/,' \x1B[31;4mMova Display TMA Logs Sub-Menu:\x1B[37;0m',//,"

	   "tr4,' Lane flow log . . . . . . . . . . . . . . . . . <L>',//,"

	   "tr4,' Occupancy log . . . . . . . . . . . . . . . . . <O>',//,"

	   "tr4,' Stage count and length log. . . . . . . . . . . <S>',//,"

	   "tr4,' Pedestrian Level-of-Service log . . . . . . . . <P>',//,"

	   "tr4,' End-Sat decision Log. . . . . . . . . . . . . . <E>',//,"

	   "tr4,' Oversaturated cycles Log. . . . . . . . . . . . <C>',//,"

	   "tr4,' Suspect Detector History Log. . . . . . . . . . <D>',//,"

	   "tr4,' Download all the above logs.  . . . . . . . . . <A>',///,"

	   "tr4,' Exit to MOVA Main Menu. . . . . . . . <X>',//,"

	   "' Select option and press Enter ',\\)"
	};
	

	static const char *const F100 = {
		"(/,' \x1B[31;4mMOVA Saturation flow Logs sub-menu:\x1B[37;0m',//,"

		"tr4,' Saturation Flow Log - Screen Display . . . . . . . <S>',//,"

		"tr4,' STLOST Log - Screen Display . . . . .  . . . . . . <T>',//,"

		"tr4,' Saturation Flow Log - verbose (.CSV format). . . . <V>',//,"

		"tr4,' STLOST Log - verbose (.CSV format) . . . . . . . . <P>',//,"

		
		"tr4,' Exit to MOVA Main Menu . . . . . . . . . . . .  <X or E>',//,"

		"' Select option and press enter ',\\)"
	};

	static const char *const F93 = { "(/,12x,a,/,8x,'Enter password (case sensitive) ... ',\\)" };

	/* RDD MOD: 07/03/05: Cruise Speed Header */
	static const char *const F934 = {
		"('Junk|Detector Type|Junk|Effective Lane|Actual Lane|Junk|Time|Date|Duration')"
	};

	static const char *const F929 = {
		   "(//'Enter current site data read-in password (6 characters) ... ',\\)"
	};

	static const char *const F95 = {
			"(//,' FLAG(',i2,') = ',i2,' Enter new value = ',\\)"
	};

	static const char *const Fph77 = { "(//,"
	"' Use the OMU command INI=2 to re-initialise the MOVA application.')" };

	static const char *const F932 = {"(/,'Abandon messages now? (y/n)',\\)"};

	static const char *const F931 = {
		   "(' Enter number of minutes messages output for <1-9> or 0 for continuous  ',/,"
		   " '    (NB - Press ANY key to pause messages)  ENTER NUMBER ... ',\\)"
	};

	static const char *const F933 = {"(//)"};

	static const char *const F943 = {
		   "(/,' Enter new value for MARK',i1,' =',\\)"
	};

	static const char *const F969 = {
		   "(//,' Do you want to Set Flags <S>',/,12x,'Look at Flags <L>',/,9x,'Clear"
		   " force bits <C>',/,11x,'set Force bits <F>',/,'   or Return to MAIN MENU"
		   " <R> ? ',\\)"
	};	

	static const char *const F98 = {
	   "(//,' \x1B[31;4mSET FLAGS:\x1B[37;0m',//,"

	   "tr4,' Flag(17)  Error Count	. . . . . . . . . . <X>',//,"

	   "tr4,' Flag(18)  Phone home	. . . . . . . . . .  <L>',//,"

	   "tr4,' Flag(20)  ON Contol Flag {1=ON Control} .  <C>',//," 

	   "tr4,' Flag(21)  MOVA Message Log  {note 1}  . .  <M>',//," 

	   "tr4,' Flag(27)  Error Log         {note 1}  . .  <E>',//,"

	   "tr4,' Flag(28)  VA {=0} / MOVA {=1} Flag  . . . <V>',//,"

	   "tr4,'           for HELP {notes} enter  . . . . <H>',///,"

	   "tr4,' Exit to MOVA Main Menu. . . . . . . <Q>',//,"

	   "' Select option and press enter ',\\)"
	};


	static char *const F101 = {
		"(/,' \x1B[31;4mMOVA alert display and setting sub-menu:\x1B[37;0m',//,"

		"tr4,'  Oversaturation alert trigger . . ',/,"
		"tr4,'                       status  . . ',//,"
		"tr4,'  Exit blocking alert trigger  . . ',/,"
		"tr4,'                      status . . . ',//,"
		"tr4,'  Occupancy alert trigger  . . . . ',/,"
		"tr4,'                  status . . . . . ',//,"

		"' \x1B[31;4mEnable/Disable Alert triggering:\x1B[37;0m',//,"

		"tr4,'  Oversaturation alert . . . . . . <S>',//,"
		"tr4,'  Exit blocking alert  . . . . . . <E>',//,"
		"tr4,'  Occupancy alert  . . . . . . . . <O>',//,"
		"tr4,'  All alerts . . . . . . . . . . . <A>',///,"

		"tr4,' Exit to MOVA Main Menu . . . . . . . . . . . .  <X>',//,"

		"' Select option and press enter ',\\)"
	};


	/* IRH MOD: M5.0.0: 06/10/04: Updated format string - only 10 stages, not 16 */
	static const char *const F968 = {
		"(//'      stage force bits               BST',/,"
		"  '  1 2 3 4 5 6 7 8 9 10 HI TO     Mar Oct    MARK1 MARK2    FLAG(29-32)',/,"
		"    tr1, 10i2, tr1, 2i3, tr4, 2i4, tr3, 2i6, i5, 3i3,//,"
		"  ' error phone watch con- MOVA ready hour stage  assess error 0=VA',/,"
		"  ' count  home  dog  trol mess  flag flow dmnded -ment   log  1=MOVA',/,"
		"    4(i5,i6),i6,2i7,i6,/)"
	};

	static const char *const F925 = {
		   "(//,'Enter current password for remote connection (6 characters) ... ',\\)"
	};
	static const char *const F926 = {
		   "(//,'Enter new password (must be exactly 6 characters long) ... ',\\)"
	};
	static const char *const F927 = {
		   "(//,'Confirm new password ... ',\\)"
	};
	static const char *const F928 = {
		   "(//,' Do you want to change the password for reading',/,' in site data via"
		   " the front panel   <Y or N >   ? ',\\)"
	};

	static const char *const F944 = {
		   "(/,' Enter time [HrMnSc] ',\\)"
	};

	static const char *const F945 = {
		   "(/,' Enter date [DyMoYr] ',\\)"
	};


	static const char *const F920 = {
		   "(/,' Time is ',i02,'/',i02,'/',i02,' ',i02,':',i02,':',i02,/,' Do you want to"
		   " change times <Y or N> ? ',\\)"
	};

	static const char *const F1923 = {
		   "(//,'MOVA Messages have been saved following a fault.',/,"
			   "'Do you want to display these saved messages now? (y/n)',\\)"
	};
	static const char *const F1924 = {
		   "(//,'Do you want to delete these saved messages? (y/n)',\\)"
	};


	static const char *const F91cr = { "(/,' INCORRECT ENTRY  PLEASE TRY AGAIN')" };

	static const char *const F91 = { "(' INCORRECT ENTRY  PLEASE TRY AGAIN')" };

	/* Used as a general buffer to output text to the terminal */
	char buf[MAX_OUTPUT_LEN];

static int inn2;
static char *const inn = (char*)&inn2;

/*************************************************************************
 *
 * Function: convert digits to number
 *
 * Description: converts string to number considering max size
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params: digits - the string to convert
 *			noValidDigits - max size of string
 			MatchExact - the string should have exactly noValidDigits
 *
 * Return: number converted
 *
 ************************************************************************/
int ConvertDigitsToNumber(char *digits, int noValidDigits, int MatchExact)
{

	int output = 0;
	int index = 1;

	while ((*digits != '\0') && (index <= noValidDigits))
	{
		if ((*digits - '0') < 0 || (*digits - '0') > 9 )
		{
			output = -1;
			break;
		}
	 	output = (output * 10) + (*digits) - '0';
	 	digits++;
	 	index++;
 	}

 	if (MatchExact)
 	{
 		if (index != noValidDigits + 1)
 		{
 			output = -1;
 		}
 	}
	return (output);
}

/*************************************************************************
 *
 * Function: CheckCurrentPasswordIntegrity
 *
 * Description: this subroutine ensures that the passwords held in RAM 
 * are not corrupt if either appears to be corrupt, both passwords are 
 * reinitialised
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:
 *
 * Return: void
 *
 ************************************************************************/
void CheckCurrentPasswordIntegrity(void)
{
	int index;
	int bad = FALSE;
	static const char DefaultPassword[PASSWORD_SIZE + 1] = {"AVOMIN"};
	static const char DefaultDownloadPassword[PASSWORD_SIZE + 1] = {"AVOMGO"};


	for ( index=0; index<PASSWORD_SIZE; index++ )
	{
		/* if the true and inverse copies of the passwords do not agree */
		/* e.g. on first time power-up */
		if ( ( CurrentPassword[index] != (char)(~CurrentPaswwordInv[index]) )
		||   ( DownloadPassword[index] != (char)(~DownloadPasswordInv[index]) ) )
		{
			bad = TRUE;
			break;
		}
	}

	if ( bad )
	{
		/* initialise passwords from the constant default values */
		for ( index=0; index<PASSWORD_SIZE; index++ )
		{

			CurrentPassword[index] = DefaultPassword[index];
			DownloadPassword[index] = DefaultDownloadPassword[index];
			CurrentPaswwordInv[index] = ~DefaultPassword[index];
			DownloadPasswordInv[index] = ~DefaultDownloadPassword[index];
		}
	}
}

/*************************************************************************
 *
 * Function: waitForInput
 *
 * Description: Function to wait for user input
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params: Input string returned.
 *
 * Return: has input been successful
 *
 ************************************************************************/
int WaitForInput(char** InputString)
{
	int nCurrent_w;
	int error = 1;


	nCurrent_w = GetOutputDestination();         /* remember whether LH or modem */
	Terrlog->buffer[0] = 0; /* ready for input */

	do 
	{
		#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
			PCTasks_Sleep( 200, MOVA_TASK_TRLUSERIF );
		#else
			NU_Sleep(30); /* @6.67ms/tick = 0.2s */
		#endif

		/* IRH MOD: 07/04/06: BUGFIX: PCMOVA 10091, M5_CRN0016.  
		Also check whether local handset connection has dropped.  */
		if(Tcomshr->io[31] == -1 || ( nCurrent_w == LOCAL_HANDSET && !lLH_CONNECTED ) )
		{

			/*Tcomshr->io[31] = 0;*/ /* allow phone in */
			error = 0; /* If phone line dropped during session */
			break;
	
		}
	}
	while (Terrlog->buffer[0] == 0);
	
	if ( Terrlog->buffer[0] != nCurrent_w )
	{
		error = 0;
	}
	else
	{
		*InputString = &Terrlog->buffer[1];
	}
	return(error);
}


/*************************************************************************
 *
 * Function: LogCommsError
 *
 * Description: Logs comms error
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:
 *
 * Return:
 *
 ************************************************************************/
void LogCommsError(void)
{
	/*
	 @@@@@@@@@@@@@@@@@@@@@@@@@@@@@ ERRORS @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	--- here if UNIT or READ or WRITE or ASYNCH QUEUE errors
	*/

	/*after 10 consecutive error messages USER program will ABORT !!! */
	uerr = uerr+1;
	if(uerr < 10) 
	{
		#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
					PCTasks_Sleep( 1000, MOVA_TASK_TRLUSERIF );
		#else
					NU_Sleep(150); /* @6.67ms/tick = 1s */
		#endif
		
		return ;   /* TRL/SIE: don't want fresh start */ 
	
	}

	/* FATAL ERROR JUST QUIETLY REBOOT */
	#if defined (PC_WRAPPER)
		PCError_ExitKernel( PCMOVA_ERROR_KERNEL_USER_FATAL );
	#else
		SOFT_ERROR(MOVA_USER_REQ_REBOOT);
	#endif
}


/*************************************************************************
 *
 * Function: DisplayReturnToContinue
 *
 * Description: Ping a display to return string to the screen
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:
 *
 * Return: next state
 *
 ************************************************************************/
UI_Options_t DisplayReturnToContinue(void)
{
	int w = GetOutputDestination();
	char *InputString = NULL;

	WRITE(INITIALIZE);
	if(WRITE(w,IOSTAT,NULL,2,FMT,F92,1,0))
	{
		LogCommsError();
		return UI_RESET_UNIT; 
	}

	
	if (!WaitForInput(&InputString))
	{
		return UI_RESET_UNIT; 
	}

	return UI_MAIN_MENU;
}

/*************************************************************************
 *
 * Function: MonitorOutputAndSaveErrorLog
 *
 * Description: Display errors via DE and setting error flag
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:
 *
 * Return: next state
 *
 ************************************************************************/
UI_Options_t MonitorOutputAndSaveErrorLog(void)
{
	int w = GetOutputDestination();
/*
	 ++++++++++++++++++++++++ DISPLAY ERROR LOG +++++++++++++++++++++++++++
	------------ controlled by IO(27)=20,21,22 for TERM, 30,31,32 for PHONe
	------------ reset by OUTPUT when finished
	*/
	int i = 0;
	int OutputNotFinished = TRUE;
	char *InputString = NULL;
	
	do 
	{

		for (i=0; i < 100; i++)
		{ /* Every cycle is 0.5s */

			/* Nucleus
			dly = 5; */
			/* rvl = ldelay(&dly); */ 
			/* wait 0.5 sec*/

			#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
				PCTasks_Sleep( 500, MOVA_TASK_TRLUSERIF );
			#else
				NU_Sleep(75); /* @6.67ms/tick = 0.5s */
			#endif

			/* Output has finished */
			if(Tcomshr->io[28] == 0)
			{
				OutputNotFinished = FALSE;

				/* save messages but only if errors displayed */
				if(lOptionDE)
				{
					/* and only if any have been stored */
					if (Last50MessStored)
					{
						/* "Do you want to output saved messages? (y/n)" */
						if(WRITE(w,IOSTAT,NULL,2,FMT,F1923,1,0))
						{
							LogCommsError();
							return(UI_RESET_UNIT); 
						}

						if (!WaitForInput(&InputString))
						{
							return UI_RESET_UNIT;
						}

						if ( tolower(*InputString) == 'y' )
						{
							switch (w)
							{
						 		case MODEM_PORT: 
									Tcomshr->io[20] = 30; 
									break;
						 		case LOCAL_HANDSET: 
									Tcomshr->io[20] = 10; 
									break;
								default:
									KDEBUG_WRITE1( DEBUG_LEVEL_ERROR, USERBIG, "MonitorOutputAndSaveErrorLog: Bad device ID = %d", w);
									break;
							}
							Tcomshr->io[28] = 27;
							/* TRL/SIE: now set flags to print as if printing last 50 messages */

							/* stay here until output finished */
							while ( Tcomshr->io[28] != 0 )
							{
								#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
									PCTasks_Sleep( 667, MOVA_TASK_TRLUSERIF );
								#else
									NU_Sleep(100);
								#endif
							}
						}

						/* "Do you want to clear these saved messages? (y/n)" */
						if(WRITE(w,IOSTAT,NULL,2,FMT,F1924,1,0))
						{
							LogCommsError();
							return(UI_RESET_UNIT); 
						}

						if (!WaitForInput(&InputString))
						{
							return UI_RESET_UNIT;
						}
						if (tolower(*InputString) == 'y') 
						{
							Last50MessStored = FALSE;
						}
					}
					lOptionDE = FALSE;
				}
				break;
			}

		} /*delay max of 50 seconds */

		if (w == MODEM_PORT) Tcomshr->io[31] = 1; /*TRL/SIE:++; */	

	} while (OutputNotFinished);

	/* output has finished return to main menu */
	return DisplayReturnToContinue();

}

/*************************************************************************
 *
 * Function: PerformOptionCE
 *
 * Description: Perform main command CE
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:
 *
 * Return: next state
 *
 ************************************************************************/
UI_Options_t PerformOptionCE(void)
{
	int CurrentOut = GetOutputDestination();
	static const char *const msg = {
		"(//,'MOVA Error Log Cleared.',/)" };

	Errhand_Initialise();
	/* IRH MOD: M5.0.0: 31/08/04: Terrlog->crash deprecated - clear DBZ
	* error string instead */

	memset( Terrlog->dbzstr, 0, xgnDBZ_MESSAGE_STR_LEN );

	Tdinout->dout[DoutDetFault] = 0; /* clear DET LED - walamp[0] */
	Tcomshr->io[16] = 0; /* M 1.6 clear FLAG */
	/* also clear the soft error and watchdog fail counters so the unit */
	/* does not hard error unless another set of errors occur. */
	soft_error_count = 0;
	IC_WATCHDOG_FAIL_COUNT = 0;
	Errhand_ResetErrorCount();
	if(WRITE(CurrentOut,IOSTAT,NULL,2,FMT,msg,2,0))
	{
		LogCommsError();
		return UI_RESET_UNIT; 
	}

	return DisplayReturnToContinue();

}

/*************************************************************************
 *
 * Function: EnableErrorLogOutput
 *
 * Description: Turn on error logging
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params: SetDevice: Set device to receive output
 *
 * Return: void
 *
 ************************************************************************/

void EnableErrorLogOutput(int SetDevice)
{
	if (SetDevice)
	{
		Tcomshr->io[26] = (int)Tcomshr->io[26]+20;
		if(GetOutputDestination() == MODEM_PORT) Tcomshr->io[26] = (int)Tcomshr->io[26]+10;
	}

	Tcomshr->io[28] = 26;            /* display error log*/
	lOptionDE = TRUE;

}


/*************************************************************************
 *
 * Function: PerformOptionDM
 *
 * Description: Perform main command DM
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:
 *
 * Return: next state
 *
 ************************************************************************/
UI_Options_t PerformOptionDM(void)
{

	int nCurrent_device = GetOutputDestination();
	UI_Options_t returnValue;
	/*
	 ******************* DISPLAY MOVA MESSAGES ****************************
	--- MOVA messages displayed by the OUTPUT program using the IO(21) flag
	--- IO(21)=20,21,22 display on TERMinal; =30,31,32 display on PHONe
	*/

	Tcomshr->io[20] = (int)Tcomshr->io[20]+20;/* display on TERMinal*/
	if(nCurrent_device == MODEM_PORT) Tcomshr->io[20] = (int)Tcomshr->io[20]+10;/* display on PHONe line*/

	/*
	------- IO(29) flag used to stop USER program until OUTPUT has finished
	*/

	Tcomshr->io[28] = 22;            /* display MOVA messages*/
	returnValue = MonitorOutputAndSaveErrorLog();

	return (returnValue);
}


/*************************************************************************
 *
 * Function: ProcessSetFlagsInput
 *
 * Description: process flag input
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params: flag option, new value of flag, redisplay same option
 *
 * Return: next state
 *
 ************************************************************************/
UI_Options_t ProcessSetFlagsInput(int option, int InputDigits, int *ReDisplayCurrentOption)
{
	UI_Options_t retValue = UI_OPTION_LF;
	int w = GetOutputDestination();	

	*ReDisplayCurrentOption = 0;

	switch(option)
	{
		case 'x':
			/* Flag 17 Error Count */
			/* if count cleared*/
			Tcomshr->io[16] = (char) InputDigits;
			if ( Tcomshr->io[16] == 0 )
			{
				/* also clear the soft error and watchdog fail counters so the unit */
				/* does not hard error unless another set of errors occur. */
				soft_error_count = 0;
				IC_WATCHDOG_FAIL_COUNT = 0;
			}
			break;
		case 'l':
			/* Flag 18 Phone home */
			/* if about to set the phone home flag to 77 */
			if ( InputDigits == 77 )
			{
				/* display message (value will be cleared by ANSWER.C) */
				if(WRITE(w,IOSTAT,NULL,2,FMT,Fph77,1,0))
				{
					LogCommsError();
					retValue = UI_RESET_UNIT;
					break;
				}

				/* redisplay the current value of the phone home flag */
				*ReDisplayCurrentOption = 1;
			}
			else
			{
				Tcomshr->io[17] = (char) InputDigits;
			}
			break;
		case 'c':
			/* Flag 20 ON control flag */
			/* if the above has just put MOVA on/off control */
			Tcomshr->io[19] = (char) InputDigits;
			if ( Tcomshr->io[19] == 0 )
			{
				/* this is a legitimate reason for MOVA going off control */
				/* so initialise the timer - see MOVA_Monitor() */
				MOVA_IF.MOVA_Off_Timer = 0;
			}
			break;
		case 'm':
			/* Flag 21 Message Log */
			Tcomshr->io[20] = (char) InputDigits;
			break;
		case 'e':
			/* Flag 27 Error Log */
			Tcomshr->io[26] = (char) InputDigits;
			if(Tcomshr->io[26] < 20)
			{
				retValue = UI_OPTION_LF; /* jump to display flags*/  
			}
			else
			{
				EnableErrorLogOutput(FALSE);
				retValue =  MonitorOutputAndSaveErrorLog();
			}
			break;
		case 'v':
			/* Flag 28 VA/MOVA control */
			Tcomshr->io[27] = (char) InputDigits;
			break;
		case '\0':
			retValue = UI_OPTION_LF;
			break;
		default:
			retValue = UI_OPTION_LF;
			KDEBUG_WRITE1( DEBUG_LEVEL_ERROR, USERBIG, "ProcessSetFlagsInput received input = %d", option );
			break;
	}

	return(retValue);

}

/*************************************************************************
 *
 * Function: HandleSetFlagsMenuInput
 *
 * Description: Set flag status
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params: flag option
 *
 * Return: next state
 *
 ************************************************************************/
UI_Options_t HandleSetFlagsMenuInput(int option)
{
	int w = GetOutputDestination();
	int InputDigits = 0;
	int IO_No = 0;
	int InvalidValue = 0;
	int ReDisplayCurrentFlag = 0;
	UI_Options_t retValue = UI_OPTION_LF;
	char *InputString = NULL;
	

	switch(option)
	{
		case 'x':
			IO_No = 16;
			break;
		case 'l':
			IO_No = 17;
			break;
		case 'c':
			IO_No = 19;
			break;
		case 'm':
			/* Flag 21 Message Log */
			IO_No = 20;
			break;
		case 'e':
			/* Flag 27 Error Log */
			IO_No = 26;
			break;
		case 'v':
			/* Flag 28 VA/MOVA control */
			IO_No = 27;
			break;
		case '\0':
			break;
		default:
			/* Didn't find a valid option try again */
			if(WRITE(w,IOSTAT,NULL,2,FMT,F91cr,1,0))
			{ /* "INCORRECT ENTRY PLEASE TRY AGAIN"*/
				LogCommsError();
				return(UI_RESET_UNIT); 
			}
			return (UI_OPTION_LF_SET_FLAGS);
	}

	do
	{ /* Keep trying until a valid value is input */

		InvalidValue = 0;
		ReDisplayCurrentFlag = 0;

		if(WRITE(w,IOSTAT,NULL,2,FMT,F95,1,INT2,IO_No + 1,MOVA_BYTE,Tcomshr->io[IO_No],0))
		{
			LogCommsError();
			return(UI_RESET_UNIT); 
		}

		if (!WaitForInput(&InputString))
		{
			return(UI_RESET_UNIT);
		}
		/* if the first character is a space (e.g. when nothing entered) */
		if ( *InputString == '\0'/*32*/ )
		{
			/* don't change the flag, just redisplay them all */
			return(UI_OPTION_LF);
		}

		/* Flags can only have two digits max */
		InputDigits = ConvertDigitsToNumber(&Terrlog->buffer[1], 2, FALSE);

		/* range check */
		if(InputDigits == -1)
		{
			if(WRITE(w,IOSTAT,NULL,2,FMT,F91cr,1,0))
			{
				LogCommsError();
				return(UI_RESET_UNIT); 
			}
			InvalidValue = 1;
		}

		if (!InvalidValue)
		{
			retValue = ProcessSetFlagsInput(option, InputDigits, &ReDisplayCurrentFlag);
		}

	} while (InvalidValue || ReDisplayCurrentFlag);

	return(retValue);

}

/*************************************************************************
 *
 * Function: HandleMarkInput
 *
 * Description: handle hidden mark input for flags menu
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params: What mark
 *
 * Return: next state
 *
 ************************************************************************/
UI_Options_t HandleMarkInput(int MarkNo)
{
	int MarkValue;
	int w = GetOutputDestination();
	char *InputString = NULL;

	do
	{
		if(WRITE(w,IOSTAT,NULL,2,FMT,F943,1,INT2,MarkNo,0))
		{
			LogCommsError();
			return(UI_RESET_UNIT); 
		}

		if (!WaitForInput(&InputString))
		{
			 return(UI_RESET_UNIT);
		}

		/* Marks have max of four digits */
		MarkValue = ConvertDigitsToNumber(InputString, 4, FALSE);

	} while ( MarkValue == -1 );
	
	if(MarkNo == 1)
	{
		Tcomshr->mark1 = MarkValue;
	}
	if(MarkNo == 2)
	{
		Tcomshr->mark2 = MarkValue;
	}

	return(UI_OPTION_LF);
}

/*************************************************************************
 *
 * Function: DisplaySetFlagsMenu
 *
 * Description: Display set flags 
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:
 *
 * Return: next state
 *
 ************************************************************************/
UI_Options_t  DisplaySetFlagsMenu(void)
{
	char option;
	UI_Options_t CurrentOption = UI_LAST_STATE;
	int nCurrent_device = GetOutputDestination();
	char *InputString = NULL;

	static const char *const F981 = {
		"(/,'  note 1: 0=no print ',' 1=log printed as generated (port 1)',/,'       "
		"  10=print complete log (port 1)  20=write complete log',' (port 0)',/,'  "
		"       30=write log to telephone line',/,\\)"
	};

	if(WRITE(nCurrent_device, IOSTAT,NULL,2,FMT,F98,7,0))
	{
		LogCommsError();
		return(UI_RESET_UNIT); 
	}
	
	do
	{ /*Loop round main flags menu */

		if (!WaitForInput(&InputString))
		{
			return(UI_RESET_UNIT);
		}

		option = (char) tolower(*InputString);

		switch(option)
		{
			case 'h':

			if(WRITE(nCurrent_device, IOSTAT,NULL,2,FMT,F981,6,0))
			{
				LogCommsError();
				return(UI_RESET_UNIT); 
			}
			break;

			case 'y':
			CurrentOption = HandleMarkInput(1);
			break;

			case 'z':
			CurrentOption = HandleMarkInput(2);
			break;

			case 'q':
			CurrentOption = UI_MAIN_MENU;
			break;

			case '\0':
			CurrentOption = UI_MAIN_MENU;
			break;

			default:
			CurrentOption = HandleSetFlagsMenuInput(option);
			break;
		}

	} while ( CurrentOption == UI_OPTION_LF_SET_FLAGS);
	return (CurrentOption);
}



/*************************************************************************
 *
 * Function: PerformOptionLF
 *
 * Description: Display flags option via command LF
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:
 *
 * Return: next state
 *
 ************************************************************************/
UI_Options_t PerformOptionLF(void)
{
	int i,j, k = 0;
	int w = GetOutputDestination();
	int lowerInput;
	int returnValue = 0;
	UI_Options_t CurrentOptionLFMenu = UI_OPTION_LF;
	/* ansi strings */
	static const char sClrScr[] = { "[2J \x1B[1;1H" };
	static const char sSP_Esc[] = { " \x1B"};

	/* WRITE format 'statements'*/
	static const char *const F4a = {"(4a,\\)"};
	char *InputString = NULL;

	do
	{
		switch (CurrentOptionLFMenu)
		{
			case UI_OPTION_LF:
		
				returnValue = 0;
				/*
				 """""""""""""""""""""""""""""""" LOOK AT FLAGS """""""""""""""""""""""
				*/
				/*Clear Screen*/
				WRITE(w,IOSTAT,NULL,2,FMT,F4a,1,
					STRG,sSP_Esc,2,STRG,sClrScr,10,
					STRG,sSP_Esc,2,STRG,sNormal,3,0);


				if(w == MODEM_PORT) Tcomshr->io[31] = 1; /*TRL/SIE:(int)Tcomshr->io[31]+1;*//* increment phone counter*/
				WRITE(w,IOSTAT,NULL,2,FMT,F968,5,MORE);
				for(i=1; i<=mxstag+2; i++) 
				{
					/*WRITE(MOVA_BYTE,Tdinout->dout[i-1],MORE);*/
					WRITE(MOVA_BYTE,control[i-1],MORE);
				}

				WRITE(MOVA_BYTE,Tcomshr->io[14],MOVA_BYTE,Tcomshr->io[15],INT2,Tcomshr->mark1,INT2,Tcomshr->mark2,MORE);

				for(j=29; j<=32; j++) 
				{
					 WRITE(MOVA_BYTE,Tcomshr->io[j-1],MORE);
				}

				WRITE(MORE);

				for(k=17; k<=23; k++) 
				{
					WRITE(MOVA_BYTE,Tcomshr->io[k-1],MORE);
				}

				/* No longer need to display io[23] as stage stuck testing is no longer performed */
				WRITE(MORE);

				for(k=25; k<=28; k++) 
				{
					 WRITE(MOVA_BYTE,Tcomshr->io[k-1],MORE);
				}

				if(WRITE(0))
				{
					LogCommsError();
					return(UI_RESET_UNIT);
				}
				if(WRITE(w,IOSTAT,NULL,2,FMT,F969,3,0))
				{
					LogCommsError();
					return(UI_RESET_UNIT); 
				}

				if (!WaitForInput(&InputString))
				{
					return UI_RESET_UNIT;
				}

				lowerInput = tolower(*InputString);

				/*Clear Screen*/
				WRITE(w,IOSTAT,NULL,2,FMT,F4a,1,
					STRG,sSP_Esc,2,STRG,sClrScr,10,
					STRG,sSP_Esc,2,STRG,sNormal,3,0);

				if(lowerInput == 's')
				{ /* S  set*/
					CurrentOptionLFMenu = UI_OPTION_LF_SET_FLAGS;
				}
				else if(lowerInput == 'l')
				{
					/* L  look*/
					CurrentOptionLFMenu = UI_OPTION_LF_LOOK;
				}
				else if(lowerInput == 'c') 
				{ //CLEAR
					CurrentOptionLFMenu = UI_OPTION_LF_CLEAR_FORCE;
				}
				else if(lowerInput == 'f') 
				{ //SET
					CurrentOptionLFMenu = UI_OPTION_LF_SET_FORCE;
				}
				else
				{
					return UI_MAIN_MENU;
				}

				break;

			case UI_OPTION_LF_SET_FLAGS:

				returnValue = DisplaySetFlagsMenu();
				if (returnValue == UI_RESET_UNIT)
				{
					return UI_RESET_UNIT;
				}
				else if (returnValue == UI_OPTION_LF)
				{
					CurrentOptionLFMenu = UI_OPTION_LF;
				}
				else if (returnValue == UI_MAIN_MENU)
				{
					return UI_MAIN_MENU;
				}

				/* shouldn't come down here !!*/
				CurrentOptionLFMenu = UI_OPTION_LF;
				break;

			case UI_OPTION_LF_LOOK:
				 CurrentOptionLFMenu = UI_OPTION_LF;
				break;

			case UI_OPTION_LF_CLEAR_FORCE:

				/* C  clear*/
				Tcomshr->io[19] = 0;
				Tcomshr->io[27] = 0;
				Terrlog->stgoff = 0;
				if(Tcomshr->stgdem == 0) Terrlog->stgoff = 1;
				for(i=1; i<=mxstag+2; i++) 
				{
				  control[i-1] = Terrlog->stgoff;
				}

				Switch_MOVA_Off(NO_SAV_MESSAGES, NO_REBOOT);
				 CurrentOptionLFMenu = UI_OPTION_LF;
				break;

			case UI_OPTION_LF_SET_FORCE:
				/* F set force */
				Tcomshr->io[19] = 1;
				Tcomshr->io[27] = 1;
				 CurrentOptionLFMenu = UI_OPTION_LF;
				break;

			default:
				KDEBUG_WRITE1( DEBUG_LEVEL_ERROR, USERBIG, "Invalid option in PerformOptionLF = %d", CurrentOptionLFMenu);
				break;
		}

	} while ((CurrentOptionLFMenu != UI_MAIN_MENU) && (CurrentOptionLFMenu != UI_RESET_UNIT));

	return CurrentOptionLFMenu;

}


/*************************************************************************
 *
 * Function: PerformOptionDF
 *
 * Description: DF option selected
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:
 *
 * Return: next state
 *
 ************************************************************************/
UI_Options_t PerformOptionDF(void)
{
	char cDay[4];

	static const char cDay_[7][4] = {
		"MON","TUE","WED","THU","FRI","SAT","SUN"
	};

	/*	Maximum number of lanes that can be displayed 
		in the Display Flows (DF) table. */
	#define gnFLOW_TABLE_LANES_MAX	(24)


	/*
	 $$$$$$$$$$$$$$$$$$$$$$$$$ DISPLAY AVERAGE FLOWS $$$$$$$$$$$$$$$$$$$$$$
	*/

	int id, i;
	uint8 lane, i1;
	int wx = GetOutputDestination();
	char *InputString = NULL;

	/*
	 $$$$$$$$$$$$$$$$$$$$$$$$$$ PRINT AVERAGE FLOWS $$$$$$$$$$$$$$$$$$$$$$$
	*/	
	do
	{ /* input should be 1-7*/
		if(WRITE(wx,IOSTAT,NULL,2,FMT,F960,1,0))
		{
			LogCommsError();
			return(UI_RESET_UNIT); 
		}
		
		if (!WaitForInput(&InputString))
		{
			return UI_RESET_UNIT;
		}

		id = ConvertDigitsToNumber(InputString,1, TRUE);
	}	while ((id < 1) || (id > 7));

	/*
	-------------------------------------- HALF HOURLY FLOWS --------------
	*/
	/* TRL/SIE:   Replace because of change to rday initialization    */
	/*       WRITE(wx,IOSTAT,NULL,2,FMT,F965,1,REAL4,rday[id-1],MORE); */

	if(WRITE(wx,IOSTAT,NULL,2,FMT,F933,1,0))
	{
		LogCommsError();
		return(UI_RESET_UNIT); 
	}
	/* IRH MOD: M5.0.0: 06/08/04: strncpy safer - does not assume null-terminated */
	strncpy(cDay, cDay_[id-1], sizeof( cDay ) );
	WRITE(wx,IOSTAT,NULL,2,FMT,F965,1,STRG,cDay,3,MORE);

	/* IRH MOD: M5.0.0: 04/10/04: Only print out as many lanes as specified
	* in the format strings F965-7 (24i3) 
	* (with mxlane=30, this was occasionally causing a crash) */
	for(i=1; i<=gnFLOW_TABLE_LANES_MAX/*mxlane*/; i++) 
	{
		WRITE(INT2,i,MORE);
	}

	if(WRITE(0))
	{
		LogCommsError();
		return(UI_RESET_UNIT); 
	}

	for ( i=1; i<=24; i++) 
	{
		i1 = (uint8)(i*2-1);
		WRITE(wx,IOSTAT,NULL,2,FMT,F966,1,INT2,i-1,MORE);

		for(lane=1; lane<=gnFLOW_TABLE_LANES_MAX/*mxlane*/; lane++) 
		{
			WRITE(MOVA_BYTE, (char)CI_get_lane_average_flow((uint8)(id-1), i1-1, lane-1) /*Tcomshr->aveflo[id-1][i1-1][lane-1]*/,MORE);
		}

		if(WRITE(0))
		{
			LogCommsError();
			return(UI_RESET_UNIT); 
		}

		WRITE(wx,IOSTAT,NULL,2,FMT,F967,1,INT2,i,MORE);

		for(lane=1; lane<=gnFLOW_TABLE_LANES_MAX/*mxlane*/; lane++) 
		{

			WRITE(MOVA_BYTE, (char)CI_get_lane_average_flow((uint8)(id-1), i1, lane-1)/*Tcomshr->aveflo[id-1][(long)i1][lane-1]*/,MORE);

		}

		if(WRITE(0))
		{
			LogCommsError();
			return(UI_RESET_UNIT); 
		}
	}

	return DisplayReturnToContinue();

}


/*************************************************************************
 *
 * Function: PerformOptionCP
 *
 * Description: CP option selected, change password.
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:
 *
 * Return: next state
 *
 ************************************************************************/
UI_Options_t PerformOptionCP(void)
{
	char PasswordInput[6];
	char *InputString = NULL;
	int i;
	int w = GetOutputDestination();
	char NewPassword[PASSWORD_SIZE]; /* holds new password until it is confirmed */

	if(WRITE(w,IOSTAT,NULL,2,FMT,F925,1,0))
	{ /* enter current password */
		LogCommsError();
		return(UI_RESET_UNIT); 
	}
	if (!WaitForInput(&InputString))
	{
		return(UI_RESET_UNIT);
	}
	memcpy(PasswordInput, InputString, PASSWORD_SIZE);
	CheckCurrentPasswordIntegrity();
	if ( memcmp(PasswordInput, CurrentPassword, PASSWORD_SIZE) != 0 ) 
	{
		return(UI_MAIN_MENU);  /* not same: return to main menu */
	}
	
	do
	{
		if(WRITE(w,IOSTAT,NULL,2,FMT,F926,1,0))
		{ /* enter new password */
			LogCommsError();
			return(UI_RESET_UNIT); 
		}
 		if (!WaitForInput(&InputString))
		{
			return(UI_RESET_UNIT);
		}
		/* store entered string into NewPassword[] for now */
		memcpy(NewPassword, InputString, PASSWORD_SIZE);

		/* return to main menu if this contains any spaces (or was not PASSWORD_SIZE characters long) */
		if (( memchr( NewPassword, ' ', PASSWORD_SIZE)) || ( memchr( NewPassword, '\0', PASSWORD_SIZE))) return(DisplayReturnToContinue());

		if(WRITE(w,IOSTAT,NULL,2,FMT,F927,1,0))
		{ /* confirm password */
			LogCommsError();
			return(UI_RESET_UNIT); 
		}
		if (!WaitForInput(&InputString))
		{
			return(UI_RESET_UNIT);
		}
		memcpy(PasswordInput, InputString, PASSWORD_SIZE);

	} while ( memcmp(PasswordInput, NewPassword, PASSWORD_SIZE) != 0 );

	/* store new password */
	memcpy(CurrentPassword, NewPassword, PASSWORD_SIZE);
	for ( i=0; i<PASSWORD_SIZE; i++ )
	{
		CurrentPaswwordInv[i] = ~NewPassword[i];
	}
 
	if(WRITE(w,IOSTAT,NULL,2,FMT,F928,2,0))
	{
		LogCommsError();
		return(UI_RESET_UNIT); 
	}

	if (!WaitForInput(&InputString))
	{
		return(UI_RESET_UNIT);
	}

	/* want to do data download password ? */
	if( tolower(*InputString) != 'y' ) return(DisplayReturnToContinue());

	/* Yes. Change data download password */
	if(WRITE(w,IOSTAT,NULL,2,FMT,F929,1,0))
	{
		LogCommsError();
		return(UI_RESET_UNIT); 
	}

	if (!WaitForInput(&InputString))
	{
		return(UI_RESET_UNIT);
	}
	memcpy(PasswordInput, InputString, PASSWORD_SIZE);
	if ( memcmp(PasswordInput, DownloadPassword, PASSWORD_SIZE) != 0 ) return(UI_MAIN_MENU);  /* not same: return to main menu */

	do
	{
		if(WRITE(w,IOSTAT,NULL,2,FMT,F926,1,0))
		{
			LogCommsError();
			return(UI_RESET_UNIT); 
		}
		if (!WaitForInput(&InputString))
		{
			return(UI_RESET_UNIT);
		}
		memcpy(NewPassword, InputString, PASSWORD_SIZE);
		/* return to main menu if this contains any spaces (or was not PASSWORD_SIZE characters long) */
		if (( memchr( NewPassword, ' ', PASSWORD_SIZE) ) || ( memchr( NewPassword, '\0', PASSWORD_SIZE))) return(DisplayReturnToContinue());
		if(WRITE(w,IOSTAT,NULL,2,FMT,F927,1,0))
		{ /* confirm password */
			LogCommsError();
			return(UI_RESET_UNIT); 
		}
		if (!WaitForInput(&InputString))
		{
			return(UI_RESET_UNIT);
		}
		memcpy(PasswordInput, InputString, PASSWORD_SIZE);

	} while ( memcmp(PasswordInput, NewPassword, PASSWORD_SIZE) != 0 );

	/* store new password */
	memcpy(DownloadPassword, NewPassword, PASSWORD_SIZE);
	for ( i=0; i<PASSWORD_SIZE; i++ )
	{
		DownloadPasswordInv[i] = ~NewPassword[i];
	}

	return(DisplayReturnToContinue());
}


/*************************************************************************
 *
 * Function: PerformOptionFI
 *
 * Description: FI option selected
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:
 *
 * Return: next state
 *
 ************************************************************************/
 UI_Options_t PerformOptionFI(void)
{
	int w = GetOutputDestination();

	/*
	 %%%%%%%%%%%%%%%%%%%%%%%%%% FINISH %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
	*/

	/* Siemens fault report MOVA-10: Improve text messages when FInish selected */
	/* if(WRITE(w,IOSTAT,NULL,2,FMT,F975,1,0)) 
		{
			LogCommsError();
			return(9); 
		};
	*/

	/* if remote communications (i.e. modem) */
	if ( w == MODEM_PORT )
	{
		static const char *const msg = {
			"('Handset session finished, closing call...',/,"
			 "'(Press F10 to exit MOVAC when the modem indicates \"NO CARRIER\")')" };
		if(WRITE(w,IOSTAT,NULL,2,FMT,msg,1,0))
		{
			LogCommsError();
			return(UI_RESET_UNIT); 
		}
	}
	else /* local handset */
	{
		#if defined (PC_WRAPPER)
				/* IRH MOD: 05/04/06: PCMOVA: "OMU" is Siemens-specific */
				static const char *const msg = {
					"(//,'MOVA handset session finished...',//)" };
		#else
				static const char *const msg = {
					"(//,'MOVA handset session finished, returning to OMU...',//)" };
		#endif
		
		/* output message to the local handset port */
		if(WRITE(w,IOSTAT,NULL,2,FMT,msg,1,0))
		{
			LogCommsError();
			return(UI_RESET_UNIT); 
		}
	}

	/* wait to allow text to appear and then indicate the user has finished */

	#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
		PCTasks_Sleep( 400, MOVA_TASK_TRLUSERIF );
	#else
		NU_Sleep(60); /* @6.67ms/tick = 0.4s */
	#endif

	/* if remote communications (i.e. modem) */
	if(w == MODEM_PORT)
	{
		/* -- for PHONe line only, ANSWER program will release line when IO(32)=-1
		   -- PHONe program will not work while IO(30) is .NE. 0 ie connected by
		   -- the ANSWER program */
		Tcomshr->io[31] = -1;
		Tcomshr->io[29] = 9;

	}
	else
	{
		/* switch local communications back to the OMU application */
		MOVA_IF.LComms = MOVAIF_COMMS_TO_OMU;
	}
	Terrlog->buffer[0] = 0;
	return(UI_RESET_UNIT);
	
}


/*************************************************************************
 *
 * Function: PerformOptionVM
 *
 * Description: VM option selected
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:
 *
 * Return: next state
 *
 ************************************************************************/
 UI_Options_t PerformOptionVM(void)
{
	int mx = 0;
	int w = GetOutputDestination();
	char cVar;
	int nOldDisp;
	char *InputString = NULL;
	/*
	 ::::::::::::::::::::::: VIEW MOVA MESSAGES :::::::::::::::::::::::::::
	---------- MOVA messages are output by the OUTPUT program for X minutes
	---------- controlled by setting IO(21)=50+X for TERM, 40+X for PHONe
	*/
	do {

		Tcomshr->io[20] = 0;             /* Reset message flag to 0*/
		if(WRITE(w,IOSTAT,NULL,2,FMT,F931,1,0))
		{
			LogCommsError();
			return(UI_RESET_UNIT); 
		}
		if (!WaitForInput(&InputString))
		{
			return(UI_RESET_UNIT);
		}
		mx = ConvertDigitsToNumber(InputString, 1, TRUE);
		Terrlog->buffer[0] = 0; /* Tell term/phone to wait until messages end */
		/* TRL/SIE: enter 0 for continuous, interruptible messages */
	} while ( mx == -1);

	if(WRITE(w,IOSTAT,NULL,2,FMT,F933,1,0))
	{
		LogCommsError();
		return(UI_RESET_UNIT); 
	}
	Tcomshr->io[20] = (signed char)((w+2)*10+mx);
	Tcomshr->io[28] = 23;            /* view MOVA messages*/
	lMessOn = TRUE;                  /* to disable term or phone */
	if(w == MODEM_PORT) Tcomshr->io[31] = 3;  /*TRL/SIE:++; allows 1hour approx*/
	cVar = '\0';
	
	while ( Tcomshr->io[28] == 23 && Tcomshr->io[31] != -1 )
	{
		/* Interruptible MOVA messages */
		if ( w == MODEM_PORT )
			cVar = CollectModemChar();

		#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
			PCTasks_Sleep( 500, MOVA_TASK_TRLUSERIF );
		#else
			NU_Sleep(75);
		#endif

		/* Interruptible MOVA messages */
		if ( w == MODEM_PORT )
		{
			cVar = CollectModemChar();
		}
		else
		{
			cVar = GetHandsetChar(); /* can use here because term disabled at this point */
		}

		if ( cVar != '\0' ) /* key pressed */ 
		{
			Tcomshr->io[28] = 0;                 /* switch messages off for now */
			nOldDisp = nDisp;                    /* remember nDisp as in OUTPUT */
			nDisp = 0;

			#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
					PCTasks_Sleep( 500, MOVA_TASK_TRLUSERIF );
			#else
					NU_Sleep(75); /* Wait for messages to go off */
			#endif
			
			/* abandon messages ? */
			if(WRITE(w,IOSTAT,NULL,2,FMT,F932,2,0))
			{   
				/* IRH MOD: 26/04/06: BUGFIX: M5_CRN0027. 
				If an error occurs, ensure MOVA message output is turned off
				before jumping to error handler. */
				lMessOn = FALSE;
				LogCommsError();
				return(UI_RESET_UNIT); 
			}

			cVar = '\0';
			while ( cVar == '\0' ) 
			{

				if ( w == LOCAL_HANDSET )
					cVar = (char)toupper(GetHandsetChar());
				else
					cVar = (char)toupper(CollectModemChar());

				#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
						   PCTasks_Sleep( 133, MOVA_TASK_TRLUSERIF );
				#else
						   NU_Sleep(20);
				#endif

				if ( !lLH_CONNECTED && lLineDropped ) cVar = 'Y';
			}

			if ( cVar != 'Y' ) /* then continue with messages */
			{
				Tcomshr->io[28] = 23;
				nDisp = nOldDisp;
			}
			else
			   lMessOn = FALSE;

			if(WRITE(w,IOSTAT,NULL,2,FMT,F933,1,0))
			{
				/* IRH MOD: 26/04/06: BUGFIX: M5_CRN0027. 
				If an error occurs, ensure MOVA message output is turned off
				before jumping to error handler. */
				lMessOn = FALSE;
				LogCommsError();
				return(UI_RESET_UNIT); 
			}

			cVar = '\0';

		 }

	}

	lMessOn = FALSE;

	if ( Tcomshr->io[31] == -1 )
	{
		/*Tcomshr->io[31] = 0;*/
		return(UI_RESET_UNIT);
	}
	return(DisplayReturnToContinue());
}

/*************************************************************************
 *
 * Function: PerformOptionFL
 *
 * Description: FL option selected
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:
 *
 * Return: next state
 *
 ************************************************************************/
UI_Options_t PerformOptionFL(void)
{
	int w = GetOutputDestination();
	char *InputString = NULL;

	(*ShowSatFlow) = 1 - (*ShowSatFlow);
 	if(*ShowSatFlow) 
 	{
		if(WRITE(w,IOSTAT,NULL,2,FMT,"(/,'Saturation Flow Display Mode ON',//,'Press Enter to Continue')",1,0))
		{
			LogCommsError();
			return(UI_RESET_UNIT); 
		}
		WaitForInput(&InputString);
		return(UI_RESET_UNIT);
 	}
 	else
 	{	
		if(WRITE(w,IOSTAT,NULL,2,FMT,"(/,'Saturation Flow Display Mode OFF',//,'Press Enter to Continue')",1,0))
		{
			LogCommsError();
			return(UI_RESET_UNIT); 
		}
		WaitForInput(&InputString);
		return(UI_RESET_UNIT);
 	}
}

/*************************************************************************
 *
 * Function: displayAlertStatus
 *
 * Description: Function to display the latest status of alerts
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:
 *
 * Return: void
 *
 ************************************************************************/
void displayAlertStatus(void)
{
	BOOL BlockingActive = 0;
	BOOL OverSatActive = 0;
	BOOL OccActive = 0;
	BOOL BlockingStatus = 0;
	BOOL OverSatStatus = 0;
	BOOL OccStatus = 0;
	int w = GetOutputDestination();

	GetAlertStatus(&BlockingStatus, &OverSatStatus, &OccStatus);
	GetAlertMonitoring(&BlockingActive, &OverSatActive, &OccActive);

	if (OverSatActive)
	{
		strcpy(buf, "\x1B[4;38H Enabled\x1B[K ");
	}
	else
	{
		strcpy(buf, "\x1B[4;38H Disabled ");
	}

	if (OverSatStatus)
	{
		strcat(buf, "\x1B[5;38H On\x1B[K ");
	}
	else
	{
		strcat(buf, "\x1B[5;38H Off ");
	}


	if (BlockingActive)
	{
		strcat(buf, "\x1B[7;38H Enabled\x1B[K ");
	}
	else
	{
		strcat(buf, "\x1B[7;38H Disabled ");
	}

	if (BlockingStatus)
	{
		strcat(buf, "\x1B[8;38H On\x1B[K ");
	}
	else
	{
		strcat(buf, "\x1B[8;38H Off ");
	}

	if (OccActive)
	{
		strcat(buf, "\x1B[10;38H Enabled\x1B[K ");
	}
	else
	{
		strcat(buf, "\x1B[10;38H Disabled ");
	}

	if (OccStatus)
	{
		strcat(buf, "\x1B[11;38H On\x1B[K ");
	}
	else
	{
		strcat(buf, "\x1B[11;38H Off ");
	}

	/* Clear any text user has entered at cursor */
	strcat(buf, "\x1B[26;40H\x1B[K ");

	writeString(w, buf);

}


/*************************************************************************
 *
 * Function: ConvertInputBufferToUIType
 *
 * Description: converts buffer into menu option
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	Input buffer
 *
 * Return: menu option selected
 *
 ************************************************************************/
UI_Options_t ConvertInputBufferToUIType(char *chars)
{

	#define MAX_ACRONYMS 17

	const char *menuinput[MAX_ACRONYMS] = {"ds", "tm", "sf", "al", "de", "ce",
										   "vm", "df", "lf", "ct", "fi", "fl",
										   "cs", "dm", "cp", "rt", "ve"};
	/*Expand this to cover every option */
	const UI_Options_t MenuOptions[] = { UI_OPTION_DS, UI_OPTION_TM, UI_OPTION_SF, 
										UI_OPTION_AL, UI_OPTION_DE, UI_OPTION_CE, 
										UI_OPTION_VM, UI_OPTION_DF, UI_OPTION_LF, 
										UI_OPTION_CT, UI_OPTION_FI, UI_OPTION_FL,
										UI_OPTION_CS, UI_OPTION_DM, UI_OPTION_CP, 
										UI_OPTION_RT, UI_OPTION_VE };

	/*two input chars + terminator */
	char temp[3];
	char *start = chars;
	int i;

	for (i=0; i < (int) strlen(chars); i++)
	{ /* treat upper and lower input as the same */
		temp[i] = (char) tolower(*start++);
	}

	temp[i] = '\0';

	for (i=0; i < MAX_ACRONYMS; i++ )
	{
		if (!strcmp(temp, menuinput[i]))
		{
			return (MenuOptions[i]);
		}
	}

	return (UI_LAST_STATE);
}

/*************************************************************************
 *
 * Function: displayMainMenu
 *
 * Description: Displays the main menu
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	
 *
 * Return: next state
 *
 ************************************************************************/
UI_Options_t displayMainMenu(void)
{
	int w = GetOutputDestination();
	char *InputString = NULL;

	static const char sSpEsc[] = { " \x1B" };  /* sSpESC */
	static const char sYellow[] = { "[33;40;1m" };
	static const char sCursOn[] = { "[CursorOn" };

	/*
	 ---------------------------MAIN MENU----------------------------------
	*/

	/* if option enabled, automatically return the commissioning screen when
	|  any menu option is complete. */

	UI_Options_t NewMenu;


	MenuOptionFromLookScreen[0] = ' ';

	if ( MenuViaLookScreen )
	{
		WRITE(INITIALIZE);
		/* extend phone timeout from 20mins to 40mins */
		if ( w == MODEM_PORT ) Tcomshr->io[31] = 2;
		EngScreen(w);
		if ( w == MODEM_PORT ) Tcomshr->io[31] = 1;
	}

	/* if no menu option was entered on the commissioning screen */
	if ( MenuOptionFromLookScreen[0] == ' ' )
	{

		/* show the menu and wait for a response */
		WRITE(INITIALIZE);
		clearScreen(w);
		/* insert a blank line before the menu */
		if(WRITE(w,IOSTAT,NULL,2,FMT,F96,1,0))
		{
			LogCommsError();
			return(UI_RESET_UNIT); 
		}
		if(WRITE(w,IOSTAT,NULL,2,FMT,FMenu,1,
		   STRG,sSpEsc,2,STRG,sCursOn,9,
		   STRG,sSpEsc,2,STRG,sYellow,9,
		   STRG,sSpEsc,2,STRG,sNormal,3,0))
		{
			LogCommsError();
			return(UI_RESET_UNIT); 
		}

		if(WRITE(w,IOSTAT,NULL,2,FMT,sLookItem,3,
		   STRG,sSpEsc,2,STRG,sYellow,9,
		   STRG,sSpEsc,2,STRG,sNormal,3, 2,0))
		{
			LogCommsError();
			return(UI_RESET_UNIT); 
		}

		if(w == MODEM_PORT) Tcomshr->io[31] = 1; /*TRL/SIE: (int)Tcomshr->io[31]+1;*//* Extend phone.*/

		if (!WaitForInput(&InputString))
		{
			return UI_RESET_UNIT;
		}

		inn[0] = Terrlog->buffer[1];
		inn[1] = Terrlog->buffer[2];   /* data input*/

		/*
		---------------- INN(1-2) is EQUIVALENCED with INN2 which is used below
		*/

		/* start a new line after the user's option */
		if(WRITE(w,IOSTAT,NULL,2,FMT,F96,1,0))
		{
			LogCommsError();
			return(UI_RESET_UNIT); 
		}
		/* IRH MOD: M5.0.0: 15/09/04: Now using null chars instead of spaces 
		* so changed from 32 below */           
		/* if nothing entered, redisplay menu */
		if(inn[0] == '\0'/*32*/) return UI_MAIN_MENU;
		/* if the commissioning screen is requested */

		if ( memcmp(&Terrlog->buffer[1],"LOOK",4) == 0 )
		{
			/* enable the menu via look screen option */
			MenuViaLookScreen = TRUE;
			/* and go back to the top to display the commissioning screen */
			return UI_MAIN_MENU;

		}

	}
	else /* menu option entered on commissioning screen */	   
	{
		/* start a new line at the top of the cleared screen */
		if(WRITE(w,IOSTAT,NULL,2,FMT,F96,1,0))
		{
			LogCommsError();
			return(UI_RESET_UNIT); 
		}
		/* copy the option entered on commissioning screen */
		inn[0] = MenuOptionFromLookScreen[0];
		inn[1] = MenuOptionFromLookScreen[1];

	}

	NewMenu = ConvertInputBufferToUIType(inn);

	if (NewMenu != UI_LAST_STATE)
	{
		return (NewMenu); /* a correct mnemonic found */
	}
	
	/* no correct mnemonic */

	if(WRITE(w,IOSTAT,NULL,2,FMT,F91,1,0))
	{
		LogCommsError();
		return(UI_RESET_UNIT); 
	}          /* held in IN() [caps]*/
	/* 2/5/00 PC - Go back right back to 'press return to continue' so if the
	|  user entered the menu option from the Look screen, they get chance to
	|  read the above error message and then are returned to Look screen. */
	return DisplayReturnToContinue();
}

/*************************************************************************
 *
 * Function: HandlePassword
 *
 * Description: Handle Password Input at reset.
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	
 *
 * Return: TRUE if error count not exceeded
 *
 ************************************************************************/
BOOL HandlePassword(void)
{
	int i, PasswordErrorCount = 0;
	int w = GetOutputDestination();
	char PasswordInput[PASSWORD_SIZE];
	char *InputString = NULL;
	static const char DefaultResetPassword[PASSWORD_SIZE + 1] = {"GNPVEZ"};
	int returnValue = 0;

	if(w == MODEM_PORT) 
	{ /* If telephone line check password*/
		PasswordErrorCount = 0;
		do
		{
			if(WRITE(w,IOSTAT,NULL,2,FMT,F93,1,STRG,Tcomshr->fname,12,0))
			{ /* "enter password"*/
				LogCommsError();
				return 0;
			}
			if (!WaitForInput(&InputString))
			{
				return 0;
			}
			for(i=1; i<=PASSWORD_SIZE; i++) 
			{
				PasswordInput[i-1] = *InputString++;
			}
			CheckCurrentPasswordIntegrity();
			PasswordErrorCount = PasswordErrorCount+1;
		} while  (memcmp(PasswordInput, CurrentPassword, PASSWORD_SIZE) != 0 && memcmp(PasswordInput, DefaultResetPassword, PASSWORD_SIZE) != 0 && (PasswordErrorCount < 8));

		if(PasswordErrorCount >= 8)
		{

			#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
					PCTasks_Sleep( 60000, MOVA_TASK_TRLUSERIF );
			#else
					NU_Sleep(9000); /* @6.67ms/tick = 60s */
			#endif

			returnValue = PerformOptionFI();
			if (returnValue == UI_RESET_UNIT)
			{
				return 0;
			}
		}
	}
	return (1);
}

/*************************************************************************
 *
 * Function: ResetUnitConnection
 *
 * Description: Reset connection and associated functions connected with
 password input and time check.
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	
 *
 * Return: 
 *
 ************************************************************************/
void ResetUnitConnection(void)
{

	do
	{

		#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
			PCTasks_Sleep( 500, MOVA_TASK_TRLUSERIF );
		#else
			NU_Sleep(75); /* @6.67ms/tick = 0.5s */
		#endif

	   /* IRH MOD: M5.0.0: 17/09/04: Reset the display error count if 
		* a connection no longer exists 
		* We don't want the error count persisting between sessions 
		* otherwise if the user disconnects the terminal 10 times when
		* in the commissioning screen (without accessing any of the 
		* options under S11 where uerr is also reset), we'll call SOFT_ERROR.  
		* As far as I can see, this should only happen for 10 display 
		* errors in the *same* terminal session. */
		uerr = 0;

	} while ( !( ( Terrlog->buffer[0] == MODEM_PORT && !lLineDropped )
			|| ( Terrlog->buffer[0] == LOCAL_HANDSET && lLH_CONNECTED ) ) ) ;

	/* Record Input */
	 CurrentDevice = Terrlog->buffer[0];
}

/*************************************************************************
 *
 * Function: GetTimeInput
 *
 * Description: Obtain time from user
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	new date and time input
 *
 * Return: TRUE if valid time input FALSE if error found
 *
 ************************************************************************/
int GetTimeInput (TIMESTAMP_TYPE *tTimeDate)
{
	BOOL invalidData = FALSE;
	int w = GetOutputDestination();
	char *InputString = NULL;

	if(WRITE(w,IOSTAT,NULL,2,FMT,F944,1,0))
	{
		LogCommsError();
		return (FALSE);
		}
	if (!WaitForInput(&InputString))
	{
		return (FALSE);
	}
	tTimeDate->hours = (unsigned long) ConvertDigitsToNumber(&Terrlog->buffer[1],2, TRUE);
	if (tTimeDate->hours >=24)
	{
		invalidData = TRUE;
	}
	if (!invalidData)
	{
		tTimeDate->minutes = (unsigned long) ConvertDigitsToNumber(&Terrlog->buffer[3],2, TRUE);
		if (tTimeDate->minutes >= 60)
		{
			invalidData = TRUE;
		}
	}
	if (!invalidData)
	{
		 tTimeDate->seconds = (unsigned long) ConvertDigitsToNumber(&Terrlog->buffer[5],2, TRUE);
		if (tTimeDate->seconds >= 60)
		{
			invalidData = TRUE;
		}
	}
	return (invalidData);
}

/*************************************************************************
 *
 * Function: GetDateInput
 *
 * Description: Obtain date from user
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	converted time and date
 *
 * Return: TRUE if date input is OK FALSE if error found
 *
 ************************************************************************/
int GetDateInput(TIMESTAMP_TYPE *tTimeDate)
{
	int invalidData = FALSE;
	int w = GetOutputDestination();
	char *InputString = NULL;

	invalidData = FALSE;

	if(WRITE(w,IOSTAT,NULL,2,FMT,F945,1,0))
	{
		LogCommsError();
		return (FALSE);
	}
	if (!WaitForInput(&InputString))
	{
		return (FALSE);
	}
	tTimeDate->date = (unsigned long) ConvertDigitsToNumber(&Terrlog->buffer[1],2, TRUE);
	if((tTimeDate->date < 1) || (tTimeDate->date > 31) )
	{
		invalidData = TRUE;
	}
	if (!invalidData)
	{
		tTimeDate->month = (unsigned long) ConvertDigitsToNumber(&Terrlog->buffer[3],2, TRUE);
		if((tTimeDate->month < 1) || (tTimeDate->month > 12) )
		{
			invalidData = TRUE;
		}
	}
	if (!invalidData)
	{
		tTimeDate->year = (unsigned long) ConvertDigitsToNumber(&Terrlog->buffer[5],2, TRUE);
		if(((long)tTimeDate->year < 0) || ((long)tTimeDate->year > 99) )
		{
			invalidData = TRUE;
		}
	}
	return (invalidData);
}
/*************************************************************************
 *
 * Function: checkTime
 *
 * Description: Allow user to confirm current time or select new one
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	
 *
 * Return: TRUE if time confirmed/changed OK FALSE if error found
 *
 ************************************************************************/
int checkTime(void)
{
	/*Allow total of 5 retries */
	#define MAX_DATE_TIME_RETRIES 5
	/* time and date structure */
	TIMESTAMP_TYPE tTimeDate;
	int BadInputRetriesCount;
	BOOL NewTimeRequired = FALSE;
	BOOL invalidData = FALSE;
	int w = GetOutputDestination();
	char *InputString = NULL;

	/* Ensure time reset only happens first time in and not in a reset */
	TimeResetRequired= 0;

	do
	{
		tTimeDate = Com_read_rtc( READ_RTC);
		NewTimeRequired = FALSE;
		BadInputRetriesCount = 0;

		if(WRITE(w,IOSTAT,NULL,2,FMT,F920,2,INT4, (int) tTimeDate.date,INT4, (int) tTimeDate.month,INT4,(int) tTimeDate.year,INT4,tTimeDate.hours,INT2,
		(int)tTimeDate.minutes,INT2,(int)tTimeDate.seconds,0))
		{
			LogCommsError();
			return(0); 
		}

		if (!WaitForInput(&InputString))
		{
			return (0);
		}

		if( tolower(*InputString) == 'y' )
		{ /* check if new date required */
			NewTimeRequired = TRUE;
		}
		else
		{ /* check current date and time part are in range */
			if ( (  tTimeDate.year  > 99 )
			 || ( tTimeDate.month < 1 ) || ( tTimeDate.month > 12 )
			 || ( tTimeDate.date  < 1 ) || ( tTimeDate.date  > 31 )
			 || ( tTimeDate.hours > 23 )
			 || ( tTimeDate.minutes > 59 )
			 || ( tTimeDate.seconds > 59 ) )
			{
				NewTimeRequired = TRUE;
			}
		}

		if (!NewTimeRequired)
		{

			#if !defined (PC_WRAPPER)
			/*	IRH MOD: 05/04/06: PCMOVA: BUGFIX: 10085. Daylight saving changes cannot 
				be carried out from within MOVA itself, as with Siemens MOVA.
				The message below about setting BST outside MOVA is Siemens-specific, 
				so has been removed for PCMOVA (instead, nothing will be displayed).
				Other embedded implementations of MOVA should probably do the same as the
				Siemens implementation - daylight saving time changes should not be carried
				out by MOVA itself. */
			static const char *const F921 = {
				/*"(/,' Do you want to enter British Summer Time days <Y or N> ',\\)"*/
				"(/,' Use CKA and CKR handset commands to enter Summer Time days <CR> ',\\)"
			};

			/*	IRH MOD: 05/04/06: PCMOVA: Daylight saving changes cannot be carried out
				from within MOVA, as is the case with the Siemens implementation.
				Other embedded implementations of MOVA should probably do the same as the
				Siemens implementation - daylight saving time changes should not be carried
				out by MOVA itself. */

				/* change BST days? */
				if(WRITE(w,IOSTAT,NULL,2,FMT,F921,1,0))
				{
					LogCommsError();
					return(UI_RESET_UNIT); 
				}
				if (!WaitForInput(&InputString))
				{
					return UI_RESET_UNIT;
				}
			#endif

			return (TRUE);
		}

		do
		{ /* Check time info */
			invalidData = FALSE;
			BadInputRetriesCount = BadInputRetriesCount+1;
			invalidData = GetTimeInput (&tTimeDate);

		} while (invalidData && (BadInputRetriesCount < MAX_DATE_TIME_RETRIES));

		if (!invalidData)
		{ /* Check Date Info */
			do
			{
				BadInputRetriesCount = BadInputRetriesCount+1;
				invalidData = FALSE;
				invalidData = GetDateInput(&tTimeDate);
			} while (invalidData && (BadInputRetriesCount <= MAX_DATE_TIME_RETRIES));
		}

		if (!invalidData)
		{ /* Update clock with new time */

			/* set the day of week field invalid so it is computed from the date */
			tTimeDate.day = 100;
			tTimeDate = Com_read_rtc( SET_RTC, tTimeDate);

			TMA_check_clock_change();

			/* MARK1=8888 is a flag which causes the MONItor program to
				reload the TIMES(CELL) array from DNUM DTIM site data
				now done in MOVA_IF_Clock_Change() which is called by com_set_clock()
				which is called by Com_read_rtc( SET_RTC,...) as well as by other OMU
				functions, such as "TOD=hh mm ss", which change the clock.
			*/

			#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */

				   /* Reinstated this code: in PCMOVA, the clock is exclusive to MOVA
				   (see Siemens comment above).
				   TODO: We should do this as Siemens have done though - set from
				   Com_read_rtc() via a function in a module for handling MOVA Kernel
				   interactions, like MOVA_IF.c.  If doing that, we might not 
				   want to set mark1 when changing from/to BST - see monitr.c 
				   under S118.  Probably wouldn't matter though. */
				if(Tcomshr->mark1 == 1234) Tcomshr->mark1 = 8888;

				PCTasks_Sleep( 500, MOVA_TASK_TRLUSERIF );
				#else
					NU_Sleep(75); /* @6.67ms/tick = 0.5s */
			#endif
		}

	}while (NewTimeRequired);

	return TRUE;

}

/*************************************************************************
 *
 * Function: ProcessTMA_SubMenuOption
 *
 * Description:  process tma options
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	tma option selected
 *
 * Return: next state
 *
 **********************************************************************/
UI_Options_t ProcessTMA_SubMenuOption(char *option)
{
	UI_Options_t retValue = UI_LAST_STATE;
	int w = GetOutputDestination();
	int lowerInput = tolower(*option);
	clearScreen(w);

	if(lowerInput == 'l')
	{
		/* Lane flow log - L */
		print_FlowLog(pTmaData, w, Tcomshr->nlanes);
		EndLogs();
		retValue = UI_OPTION_TM;
	}
	else if(lowerInput == 'o')
	{
		/* Display Occupancy log - O */
		print_OccupancyLog(pTmaData, w, Tcomshr->stages);
		EndLogs();
		retValue = UI_OPTION_TM;
	}
	else if(lowerInput == 's')
	{
		/* Stage count and length log  - S */
		print_TraSevLog(pTmaData, w, Tcomshr->stages);
		EndLogs();
		retValue = UI_OPTION_TM;
	}
	else if(lowerInput == 'p')
	{
		/* Pedestrian Level-of-Service log - P  */
		print_PedSevLog(pTmaData, w, xgnREDPED_MAX, &Tcomshr->redped[0]);
		EndLogs();
		retValue = UI_OPTION_TM;
	}
	else if(lowerInput == 'e')
	{
		/* Display End-Sat decision Log - E */
		print_EndSatLog(pTmaData, w, Tcomshr->stages);
		EndLogs();
		retValue = UI_OPTION_TM;
	}
	else if(lowerInput == 'c')
	{
		/* Display Oversaturated cycles Log - c */
		print_OversatLog(pTmaData, w, Tcomshr->nlanes);
		EndLogs();
		retValue = UI_OPTION_TM;
	}
	else if(lowerInput == 'd')
	{
		/* Display Suspect Detector History Log - D */
		print_FaultyDetecLog(pTmaData, w);
		EndLogs();
		retValue = UI_OPTION_TM;
	}
	else if(lowerInput == 'a')
	{
		/*ALL LOGS */
		print_FlowLog(pTmaData , w, Tcomshr->nlanes);
		print_OccupancyLog(pTmaData,w, Tcomshr->stages);
		print_TraSevLog(pTmaData,w,Tcomshr->stages);
		print_PedSevLog(pTmaData,w,xgnREDPED_MAX,&Tcomshr->redped[0]);
		print_EndSatLog(pTmaData,w, Tcomshr->stages);
		print_OversatLog(pTmaData,w, Tcomshr->nlanes);
		print_FaultyDetecLog(pTmaData,w);
		EndLogs();
		retValue = UI_OPTION_TM;
	}
	else
	{
		retValue = UI_MAIN_MENU;
	}

	return retValue;
}


/*************************************************************************
 *
 * Function: ResetUnit
 *
 * Description: Reset connection and associated functions connected with
 password input and time.
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	
 *
 * Return: next state
 *
 ************************************************************************/
UI_System_t ResetUnit(void)
{
	int TimeOK = 1;
	int OldTimeResetRequired = TimeResetRequired;
	UI_Options_t NewState;

	static const char *const fmtVersion = {"('MOVA Version M7.0')"};
	static const char *const fmtCopyright = {"('(c) Copyright TRL Limited, 2013'/)"};
	static const char *const fmtDataSetName = { "('DataSet Name:- ',a,/)" };
	int w;

	do
	{

		ResetUnitConnection();

		w = GetOutputDestination();

		if (!HandlePassword())
		{
			return SYSTEM_UI_RESET;
		}

		/* Display MOVA version info */
		if(WRITE(w,IOSTAT,NULL,2,FMT,fmtVersion,3,0))
		{
			LogCommsError();
			return SYSTEM_UI_RESET;
		}
		if(WRITE(w,IOSTAT,NULL,2,FMT,fmtDataSetName,1,STRG,Tcomshr->fname,strlen(Tcomshr->fname),0))
		{
			LogCommsError();
			return SYSTEM_UI_RESET;
		}
		if(WRITE(w,IOSTAT,NULL,2,FMT,fmtCopyright,3,0))
		{
			LogCommsError();
			return SYSTEM_UI_RESET;
		}

		/* Time reset required */
		if ( OldTimeResetRequired )
		{
			 TimeOK = checkTime();
		}

	} while (!TimeOK);
	
	/*Get access to data structures and variables to display*/
	ShowSatFlow = MovaSatFlowStats_GetSatFlowFlagPtr_();

	if ( OldTimeResetRequired )
	{ /* Pause if first time in */

		NewState = DisplayReturnToContinue();
	
		if (NewState == UI_MAIN_MENU)
		{
			return SYSTEM_UI_DISPLAY_MAIN_MENU;
		}
		else
		{
			return SYSTEM_UI_RESET;
		}
	}
	else
	{
		return SYSTEM_UI_DISPLAY_MAIN_MENU;
	}
}


/*************************************************************************
 *
 * Function: displayAlertScreen
 *
 * Description: Display screen with alerts and status
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	
 *
 * Return: next state
 *
 ************************************************************************/
UI_Options_t displayAlertScreen(void)
{
	int w = GetOutputDestination();
	clearScreen(w);

	if  (WRITE(w,IOSTAT,NULL,2,FMT,F101,1,0))
		return (UI_RESET_UNIT); 

	displayAlertStatus();

	return (UI_MAIN_MENU);
 
}

/*************************************************************************
 *
 * Function: ProcessAlerts_SubMenuOption
 *
 * Description: Display screen with alerts and status
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	alerts option selected
 *
 * Return: next state
 *
 ************************************************************************/
UI_Options_t ProcessAlerts_SubMenuOption(char *a)
{
	UI_Options_t retValue = UI_LAST_STATE;
	int w = GetOutputDestination();
	int lowerInput = tolower(*a);
	char *InputString = NULL;

	if(lowerInput == 's')
	{
		/* Oversaturation alert . . . . . . <S> */
		changeFlagAlert(w,0, &pTmaData->sAlert);
		displayAlertStatus();
		retValue = UI_OPTION_AL;

	}
	else if(lowerInput == 'e')
	{
		/* Exit blocking alert  . . . . . . <E> */
		changeFlagAlert(w,1, &pTmaData->sAlert);
		displayAlertStatus();
		retValue = UI_OPTION_AL;

	}
	else if(lowerInput == 'o')
	{
		/* Occupancy alert  . . . . . . . . <O> */
		changeFlagAlert(w,2, &pTmaData->sAlert);
		displayAlertStatus();
		retValue = UI_OPTION_AL;

	}
	else if(lowerInput == 'a')
	{
		/* All alerts on  . . . . . . . . . <A>  */
		changeFlagAlert(w,3, &pTmaData->sAlert);
		displayAlertStatus();
		retValue = UI_OPTION_AL;

	}
	else if(( lowerInput == 'x' ) /* Exit to Mova Main Menu - X */
		|| 	( lowerInput == 'e' ) /* Exit to Mova Main Menu - E */
		||  ( lowerInput == '\0' )) /* User Pressed Enter without any letter*/
	{
		retValue = UI_MAIN_MENU;
	}
	else
	{
		/* Incorrect Entry - Please try again*/
		if(WRITE(w,IOSTAT,NULL,2,FMT,F91cr,1,0))
		{
			retValue = UI_RESET_UNIT;
		}
		else
		{
			WaitForInput(&InputString);
			retValue = UI_MAIN_MENU;
		}
	}

	return (retValue);
}

/*************************************************************************
 *
 * Function: ProcessSatFlow_SubMenuOption
 *
 * Description: Perform sat flow options
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	sat flow option selected
 *
 * Return: Next state
 *
 ************************************************************************/

UI_Options_t ProcessSatFlow_SubMenuOption(char *a)
{
	UI_Options_t retValue = UI_MAIN_MENU;
	int w = GetOutputDestination();
	int lowerInput = tolower(*a);
	char *InputString = NULL;

	/*Parameters for used in the function call for displaying satflow logs*/

	#define gn16SATFLOW ((int16) 0)
	#define gn16STLOST ((int16) 1)



	clearScreen(w);

	if (lowerInput == 's')
	{
		MovaSatFlowStats_Output_ScreenDisplay_(gn16SATFLOW);
		EndLogs();
		retValue = UI_OPTION_SF;
	}
	else if (lowerInput == 't')
	{
		MovaSatFlowStats_Output_ScreenDisplay_(gn16STLOST);
		EndLogs();
		retValue = UI_OPTION_SF;
	}
	else if (lowerInput == 'v')
	{
		MovaSatFlowStats_Output_SFVerbose_();
		EndLogs();
		retValue = UI_OPTION_SF;
	}		
	else if (lowerInput == 'p')
	{
		MovaSatFlowStats_Output_STVerbose_();
		EndLogs();
		retValue = UI_OPTION_SF;
	}					
	else if ((lowerInput == 'x')			/* Exit to Mova Main Menu - X */
		|| 	(lowerInput == 'e')			/* Exit to Mova Main Menu - E */
		|| 	(lowerInput == '\0'))		/* User Pressed Enter without any letter*/
	{
		/* Exit to Mova Main Menu - X */
		retValue = UI_MAIN_MENU;
	}
	else
	{
		/* Incorrect Entry - Please try again*/
		if(WRITE(w,IOSTAT,NULL,2,FMT,F91cr,1,0))
		{
			retValue = UI_RESET_UNIT;
		}
		else
		{
			WaitForInput(&InputString);
			retValue = UI_MAIN_MENU;
		}
	}
	return (retValue);
}

/*************************************************************************
 *
 * Function: showCruiseSpeeds
 *
 * Description: show cruise speeds
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	
 *
 * Return: Next state
 *
 ************************************************************************/
UI_Options_t showCruiseSpeeds(void)
{
	int w = GetOutputDestination();
	int nOldDisp;
	char cVar;
	/*
	 ************************** CRUISE SPEEDS **************************************
	*/
   /* RDD MOD 1/3/05
	* Added a hidden cruise speed flag which will output time-date stamps
	* of any detector activations/deactivations for lanes at end of saturation
	* which can be toggled by typing CS at the menu screen
	*/
   ShowCruiseSpeed = 1;
	if(WRITE(w,IOSTAT,NULL,2,FMT,F934,1,0))
	{
		LogCommsError();
		return(UI_RESET_UNIT); 
	}
	Tcomshr->io[20] = (signed char)((w+2)*10);	
	Tcomshr->io[28] = 23; /* view MOVA messages*/
	lMessOn = TRUE; /* to disable term or phone */
	if(w == MODEM_PORT) Tcomshr->io[31] = 3;  /*TRL/SIE:++; allows 1hour approx*/
	cVar = '\0';
	while ( Tcomshr->io[28] == 23 && Tcomshr->io[31] != -1 )
	{

		#if defined (PC_WRAPPER) /* IRH MOD: 19/05/06: PCMOVA */
				PCTasks_Sleep( 500, MOVA_TASK_TRLUSERIF );
		#else
				NU_Sleep(75); /* Wait for messages to go off */
		#endif
	 
		/* Interruptible MOVA messages */
		if ( w == MODEM_PORT )
			cVar = CollectModemChar();
		else
			cVar = GetHandsetChar(); /* can use here because term disabled at this point */   
		
		if ( cVar != '\0' ) /* key pressed */
		{
			Tcomshr->io[28] = 0; /* switch messages off for now */
			nOldDisp = nDisp; /* remember nDisp as in OUTPUT */
			nDisp = 0;

			#if defined (PC_WRAPPER) /* IRH MOD: 19/05/06: PCMOVA */
						PCTasks_Sleep( 500, MOVA_TASK_TRLUSERIF );
			#else
						NU_Sleep(75); /* Wait for messages to go off */
			#endif
			/* abandon messages ? */
			if(WRITE(w,IOSTAT,NULL,2,FMT,F932,2,0))
			{  
				/* IRH MOD: 19/05/06: BUGFIX: M5_CRN0027. 
				If an error occurs, ensure MOVA message output is turned off
				before jumping to error handler. */
				lMessOn = FALSE;
				LogCommsError();
				return(UI_RESET_UNIT); 
			}

			cVar = '\0';
			while ( cVar == '\0' ) 
			{
				if ( w == LOCAL_HANDSET )
					cVar = (char)toupper(GetHandsetChar());
				else
					cVar = (char)toupper(CollectModemChar());

				#if defined (PC_WRAPPER) /* IRH MOD: 19/05/06: PCMOVA */
					PCTasks_Sleep( 133, MOVA_TASK_TRLUSERIF );
				#else
					NU_Sleep(20);
				#endif

				if ( !lLH_CONNECTED && lLineDropped ) cVar = 'Y';

			}

			if ( cVar != 'Y' ) /* then continue with messages */
			{
				Tcomshr->io[28] = 23;
				nDisp = nOldDisp;
			}
			else
			{
				lMessOn = FALSE;
				ShowCruiseSpeed = 0;
			}

			if(WRITE(w,IOSTAT,NULL,2,FMT,F933,1,0))
			{
				/* IRH MOD: 19/05/06: BUGFIX: M5_CRN0027. 
				If an error occurs, ensure MOVA message output is turned off
				before jumping to error handler. */
				lMessOn = FALSE;
				LogCommsError();
				return(UI_RESET_UNIT);
			}
			cVar = '\0';

		}

	}
	lMessOn = FALSE;
	if ( Tcomshr->io[31] == -1 )
	{
		return(UI_RESET_UNIT);
	}
	return(UI_MAIN_MENU);
}

/*************************************************************************
 *
 * Function: HandleMainMenuOptions
 *
 * Description: handle main menu options
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	main menu selected
 *
 * Return: Next state
 *
 ************************************************************************/
UI_Options_t HandleMainMenuOptions(UI_Options_t menuoption)
{
	TIMESTAMP_TYPE tTimeDate;
	int w = GetOutputDestination();
	char *InputString = NULL;
	UI_Options_t NewState = UI_LAST_STATE;

	switch(menuoption)
	{
		/* IRH MOD: M5.0.0: 11/08/04: MOVA 5: DS option remapped to new
		* Datahandler sub-menu */

		case UI_OPTION_DS:
			{
				BOOL	boIsRepeat;
				do
				{
					/* Data_Handler() passed download site data password for 
					* reading in new site data */
#ifndef M8_ENABLED
					boIsRepeat = Data_Handler( w, DownloadPassword );
#endif
				}
				while ( boIsRepeat );
				NewState = UI_MAIN_MENU;
				break;

			}

		case UI_OPTION_TM: /* Display TMA Logs */

			do
			{
				if (WRITE(w,IOSTAT,NULL,2,FMT,F99,1,0))
				{
					LogCommsError();
					return UI_RESET_UNIT; 
				}

				if (!WaitForInput(&InputString))
				{
					return UI_RESET_UNIT;
				}

				NewState = ProcessTMA_SubMenuOption(InputString);
			
				if (NewState != UI_OPTION_TM)
				{
					NewState = UI_MAIN_MENU;
				}

			} while (NewState == UI_OPTION_TM);

			break;

		case UI_OPTION_SF: /* Saturation Flow Logs */

			do
			{
				if(WRITE(w,IOSTAT,NULL,2,FMT,F100,1,0))
				{
					LogCommsError();
					return UI_RESET_UNIT; 
				}
				if (!WaitForInput(&InputString))
				{
					return UI_RESET_UNIT;
				}
				NewState = ProcessSatFlow_SubMenuOption(InputString);
				if (NewState != UI_OPTION_SF)
				{
					NewState = UI_MAIN_MENU;
				}
			} while (NewState == UI_OPTION_SF);

			break;

		case UI_OPTION_AL: /* Alert Enable/Disable Logs */

			do
			{
				NewState = displayAlertScreen();

				if (NewState == UI_RESET_UNIT) 
				{
					LogCommsError();
					return UI_RESET_UNIT;
				}

				if (!WaitForInput(&InputString))
				{
					return UI_RESET_UNIT;
				}

				NewState = ProcessAlerts_SubMenuOption(InputString);

				if (NewState != UI_OPTION_AL)
				{
					NewState = UI_MAIN_MENU;
				}

			} while (NewState == UI_OPTION_AL);
			break;

		case UI_OPTION_DE: /* Display Error Log  */
			EnableErrorLogOutput(TRUE);
			NewState =  MonitorOutputAndSaveErrorLog();
			break;

		case UI_OPTION_CE: /* Clear Error Log */
			 NewState = PerformOptionCE();
			break;

		case UI_OPTION_VM:
			NewState = PerformOptionVM(); 
			break;

		case UI_OPTION_DF:
			NewState = PerformOptionDF();
			break;

		case UI_OPTION_LF:
			NewState = PerformOptionLF();
			break;

		case UI_OPTION_CT:
			if (checkTime())
			{
				NewState = UI_MAIN_MENU;
			}
			else
			{
				NewState = UI_RESET_UNIT;
			}
			break;

		case UI_OPTION_FI: 
			NewState = PerformOptionFI();
			break;

		case UI_OPTION_FL:
			NewState = PerformOptionFL();
			break;

		case UI_OPTION_CS: 
			NewState = showCruiseSpeeds();
			break;

		case UI_OPTION_DM: 
			NewState = PerformOptionDM();
			break;

		case UI_OPTION_CP:
			NewState = PerformOptionCP();
			break;

		case UI_OPTION_RT:/* re-initialize clock hidden	*/
			lIsClockSet = FALSE;
			tTimeDate = Com_read_rtc(READ_RTC);
			NewState = UI_MAIN_MENU;
			break;
        
		case UI_OPTION_VE: /* Mova Kernel Version */
			strcpy(MovaVersionOut, "Mova Kernel Version ");
			strcat(MovaVersionOut, GetMovaVersionNumber());
			strcat(MovaVersionOut, " \r\n\r\n Press Enter to continue");
			writeString(w, MovaVersionOut);

			WaitForInput(&InputString);
			NewState = UI_RESET_UNIT;
			break;

		default: 
			KDEBUG_WRITE1( DEBUG_LEVEL_ERROR, USERBIG, "HandleMainMenuOptions menu state = %d \n", NewState);
			NewState = UI_MAIN_MENU;
			break;
	}
	return NewState;
}


/*************************************************************************
 *
 * Function: HandleMainMenu
 *
 * Description: handle main menu
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	
 *
 * Return: Next state
 *
 ************************************************************************/
UI_System_t HandleMainMenu(void)
{
	UI_Options_t NewState;

	NewState = displayMainMenu();
	if (NewState == UI_MAIN_MENU)
	{
		return SYSTEM_UI_DISPLAY_MAIN_MENU; 
	}
	else if (NewState == UI_RESET_UNIT)
	{
		return SYSTEM_UI_RESET; 
	}
	else if (NewState == UI_LAST_STATE)
	{
		KDEBUG_WRITE1( DEBUG_LEVEL_ERROR, USERBIG, "HandleMainMenu received bad state value = %d", NewState);
		return SYSTEM_UI_LAST;
		
	}
	else
	{
		uerr = 0; /* reset error count*/
		/* process user option*/
		NewState = HandleMainMenuOptions(NewState);
		if (NewState == UI_MAIN_MENU)
		{
			return SYSTEM_UI_DISPLAY_MAIN_MENU; 
		}
		else if (NewState == UI_RESET_UNIT)
		{
			return SYSTEM_UI_RESET; 
		}
		else
		{
			KDEBUG_WRITE1( DEBUG_LEVEL_ERROR, USERBIG, "HandleMainMenuOptions sent bad last state value = %d", NewState);
			return SYSTEM_UI_LAST;
		}
	}
}


/*************************************************************************
 *
 * Function: InitialiseUI
 *
 * Description: Initialise the UI
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	
 *
 * Return: Next state
 *
 ************************************************************************/
void InitialiseUI(void)
{
	uerr = 99;
	CheckCurrentPasswordIntegrity();
	/* INITIALISATION
		UERR is USER error count - OK up to 7 consecutive then abort
		reset to zero when MAIN MENU output
		set to >7 now so ABORT if any problem with ASYNCH Queue
	*/
	uerr = 0;	/* reset USER error code*/
	TimeResetRequired= 1;
	lMessOn = FALSE;
	lOptionDE = FALSE;
	Terrlog->buffer[0] = 0;
	Tcomshr->io[28] = 0;             /* M 2.4*/
	#if defined (PC_WRAPPER) /* IRH MOD: 24/02/05: PCMOVA */
		   PCTasks_Sleep( 2000, MOVA_TASK_TRLUSERIF );
	#else
			NU_Sleep(300); /* @6.67ms/tick = 2s */
	#endif
	Tcomshr->io[19] = 0;             /* set off-control*/
}

/*************************************************************************
 *
 * Function: TRLUserIF
 *
 * Description: main UI procedure maintain UI system state machine
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	
 *
 * Return: Next state
 *
 ************************************************************************/
void TRLUserIF(UNSIGNED argc, void * argv)
{

	UI_System_t CurrentDisplayState = SYSTEM_UI_INIT;
	UI_System_t CalculatedDisplayState = SYSTEM_UI_LAST;

	do
	{
		switch (CurrentDisplayState) {

		case SYSTEM_UI_INIT:
			InitialiseUI();
			CalculatedDisplayState = SYSTEM_UI_RESET;
			break;

		case SYSTEM_UI_RESET:
			CalculatedDisplayState = ResetUnit();
			break;

		case SYSTEM_UI_DISPLAY_MAIN_MENU:
			CalculatedDisplayState = HandleMainMenu();
			break;

		case SYSTEM_UI_LAST:
		default:
			KDEBUG_WRITE1( DEBUG_LEVEL_ERROR, USERBIG, "\n Unknown state in main display handler value = %d \n ", CurrentDisplayState);
			break;
		}

		//Do some error checking here later
		CurrentDisplayState =  CalculatedDisplayState ;

	} while ((CalculatedDisplayState == SYSTEM_UI_RESET) || (CalculatedDisplayState  == SYSTEM_UI_DISPLAY_MAIN_MENU));

	KDEBUG_WRITE1( DEBUG_LEVEL_ERROR, USERBIG, "TRLUserIF Main loop exited with state = %d ",CalculatedDisplayState);

}

/*************************************************************************
 *
 * Function: GetOutputDestination
 *
 * Description: Gets destination for user comms.
                Modem (w == 2) or local handset (w == 3).
 *
 * Author: PAnderson
 *
 * Date: 01-March-2013
 *
 * Params:	
 *
 * Return: device ID
 *
 ************************************************************************/
int GetOutputDestination( void )
{
    return ( CurrentDevice );

} /* GetOutputDestination */

/*
    Name:           EndLogs
    Author (Date):  Pkale (31/03/11)
    Platform:       PC
    Purpose:        Displays Dataset Name and Time at the end of
					SF and TMA logs. Used by Mova Comm while saving the logs
    Inputs:         N/A 
    Returns:        N/A     
*/

static void EndLogs( void )
{	
	static const char *const fmtDataSetName = { "(/,'DataSet Name:- ',a,/)" };
	static const char *const DisplayTime = { "('Time is ',i02,'/',i02,'/',i02,' ',i02,':',i02,':',i02,/)" };
	int w = GetOutputDestination();

	TIMESTAMP_TYPE tTimeDate;
	tTimeDate = Com_read_rtc(READ_RTC);

	WaitKeyAt(w, NO_PROMPT);
		
	WRITE(w,IOSTAT,NULL,2,FMT,fmtDataSetName,3,STRG,Tcomshr->fname,strlen(Tcomshr->fname),0);		
	WRITE(w,IOSTAT,NULL,2,FMT,DisplayTime,2,INT4,tTimeDate.date,INT4,tTimeDate.month,INT4,tTimeDate.year,INT4,tTimeDate.hours,INT2,tTimeDate.minutes,INT2,tTimeDate.seconds,0);
	clearScreen(w);

}
