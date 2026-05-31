/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*                                                                         */
/*  TITLE : MOVA / OMU Interface                                           */
/*                                                                         */
/*  ISSUE : See below                                                      */
/*                                                                         */
/*  SUMMARY :                                                              */
/*  This file contains the data items and functions which provide the      */
/*  interface between the Siemens RMS OMU application and the TRL MOVA     */
/*  application when the two applications have been combined to run on the */
/*  same processor.                                                        */
/*                                                                         */
/*  SOURCE FILENAME : MOVA_IF.C                                            */
/*                                                                         */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*                                                                         */
/* This is an unpublished work the copyright in which vests in Siemens     */
/* plc. All rights reserved.                                               */
/*                                                                         */
/* The information contained herein is the property of Siemens plc         */
/* and is supplied without liability for errors or                         */
/* omissions. No part may be reproduced, used or disclosed except          */
/* as authorised by contract or other written permission. The              */
/* copyright and the forgoing restriction on reproduction, use and         */
/* disclosure extend to all the media in which this information may        */
/* be embodied.                                                            */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*                         MODIFICATION RECORD                             */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/* SOURCE                                                                  */
/* IDENT   DATE     NAME    REASON FOR CHANGE                              */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*   a     12-01-00  PC     File created
     b     23-02-00  PC     Modified MOVA_IF_200() so it also clears the MOVA
                            force bits when it switches MOVA off because the
                            licence code is invalid.
     c     01-03-00  PC     Added Determine_MOVA_IO_Setting().
     d     07-03-00  PC     Added 'no MOVA ESP messages' fault.
     e     09-03-00  PC     Modified GetMOVACharFromHandset() so it always
                            returns a uppercase characters - to match the OMU
                            handset.
     f     15-03-00  PC     Modified Check_MOVA_Fault() so it handles all the
                            fault regeneration details, not MV_FAULT_REGEN().
     g     28-03-00  PC     Added MOVA_IF_Clock_Change().
     h     26-04-00  PC     Added new MOVA_DISABLED_FAULT.
     i     27-04-00  PC     Modified Valid_Licence() so the OMU does not have
                            to be configured.
     j     27-04-00  PC     Monitor_MOVA() added which tries to put MOVA back
                            on control if it drops off for some unknown reason.
     k     28-04-00  PC     OMU/MOVA-5: Increase the MOVA serial time-out (MSF)
                            from 10 to 30 secs (matches NPR) to cover switching
                            the power off and back on.
Changes to issue 10:
    1a     25-05-00  PC     Modified MOVA_IF_RX_REMOTE_CHAR() so connection to
                            a phone line sharing MOVA unit looks the same as a
                            combined MOVA/OMU unit, i.e. still wait for three
                            carriage returns.

Changes for MOVA 5.0 (includes Compact MOVA):-
    1b     29-09-04  IRH    New function Datahand_GetSiteName() used to get
                            plan 1 name for Valid_Licence() function.
                            Xsitdat no longer used. #include "Datahand.h" added.
                            
                            Undef TRUE if already defined.
 - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */


const char mova_if_iss[] = {"1b"};

#include <nucleus.h>
#include "BI80.h"
#include "BI86.h"
#include "BI286.h"
#include "odds.h"
#include <nu_omu.h> 
#include <uart_int.h> /* OMU_95 - interface to the uart drivers */
#include <error.h>
#include <inpoup.h>
#include <hardware.h>
#include "gbltypes.h"
#include "errhand.h"
#include "keptdata.h"
#include <diff_bss.h>
#include "licence.h"
#include "datahand.h" /* IRH MOD: M5.0.0: 27/07/04: For new Datahand functionality */
#include <ctype.h>


#define C_TRUE    1	 /* Used to differentiate from PLM, which uses 0xFF as TRUE (now TRUE_FF) */
#ifndef FALSE
#define FALSE   0
/* Boolean "false" value for clearing
				    flags                              */
#endif

/* IRH MOD: M5.0.0: 23/09/04: Undef TRUE if already defined */
#ifdef TRUE 
    #undef TRUE 
#endif 

#define	TRUE	0xff

/*---------------------------------------------------------------------
          External items used by this file
-----------------------------------------------------------------------*/

/* character constants used by MOVA user interface */
const char cLF = 10;
const char cCR = 13;
const char cBackSp = 8;
const char cDelete = 127;
const char sDelChar[] = {"\b \b"};

/* control block used by NUCLEUS, driver requests, set up by NU_Create_Driver */
extern NU_DRIVER ESCC2_Driver;
extern NU_DRIVER PPCSP_Driver;

/* link message address expected to be received from the RMS instation*/
extern unsigned char RM_LMA_RX;
/* function to pass a received remote character into the OMU application */
extern void RM_RX_CHAR( utiny Instation_Connection,unsigned char RM_RXCHAR );
/* function to pass a received remote character into the MOVA application */
extern void MOVA_RX_CHAR( unsigned char RM_RXCHAR );
/* standard routine to terminate a call */
extern void CP_ABANDON_CALL(unsigned char, unsigned char, unsigned int );
/* standard routine to report faults */
extern void II_INITIAL_INTERFACE_CONTROL( void *REPORT_PTR, utiny RRB );
extern unsigned char FT_LSTMSK2;
extern utiny II_ACTIVE_GOING_URGENT_REP;
extern utiny II_MOVA_PHONE_HOME_FAULT;
extern utiny II_MOVA_ESP_TIMEOUT_FAULT;
extern utiny II_MOVA_DISABLED_FAULT;
extern utiny II_RESET_MOVA_FAULTS;


/*---------------------------------------------------------------------
          Dummy declarations of MOVA items for PB680 build
-----------------------------------------------------------------------*/
#if ( defined(PB680) && !defined(PB681) ) || !defined(_PLUTO)
void monitr(UNSIGNED argc, void * argv){}
void detscn(UNSIGNED argc, void * argv){}
void mova(UNSIGNED argc, void * argv){}

void output(UNSIGNED argc, void * argv){}
void phone(UNSIGNED argc, void * argv){}
void answer(UNSIGNED argc, void * argv){}
void TRLUserIF(UNSIGNED argc, void * argv){}
void term(UNSIGNED argc, void * argv){}
#endif


#if defined(PB680) && !defined(PB681)
void genstg(UNSIGNED argc, void * argv){}
unsigned char MOVA_LED_ON;// - in MOVA_IF
void MOVA_RX_CHAR( unsigned char RM_RXCHAR ){}// - in MOVA_IF
void OutputScan(int nMode){} // - in MOVA_IF
#endif



/*---------------------------------------------------------------------
          Application #defines and structure templates
-----------------------------------------------------------------------*/

/* OMU / MOVA interface items */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* declare the space for the globally accessable MOVA interface structure */
/* MOVA_IF_type and MOVA_SC_type are defined in proj_def.h */
#if defined(PB680)
    /* never enabled at startup, (rest of structure will be 0's) */
    MOVA_IF_type MOVA_IF;
#elif defined(PB681)
    /* always enabled in PB681 (rest of structure will be 0's) */
    MOVA_IF_type MOVA_IF = { MOVA_ENABLED };
#elif defined(_PLUTO)
	MOVA_IF_type MOVA_IF __attribute__ ((section (".preservebss")));
#endif
MOVA_SC_type MOVA_SC;

/* flags which remembers when the various MOVA faults have been raised */
/* automatically cleared by INI=1 (or INI=3) which is when RMS clears them */
static struct {
    utiny Phone_Home_Curr;
    utiny Phone_Home_Prev;
    utiny No_Messages_Curr;
    utiny No_Messages_Prev;
    ushort No_Messages_Timer;
    utiny MOVA_Disabled_Curr;
    utiny MOVA_Disabled_Prev;
} MOVA_fault;


/* Phone Line Sharing interface */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* previously declared in dattran (cleared every power-up) */
phone_line_sharing_type phone_line_sharing = {0,0, 0,0,0, 0, 0,0,0};


/* MOVA configuration data from the RMS instation. */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* the fault reporting urgency for the MOVA 'phone home' fault report. */
struct {
    unsigned int    curr_len;
    unsigned char   curr_data;
} mova_phone_home_rut;

/* the last five digits of the OMU phone number converted to a two-byte word for
|  use in checking the licence, such that 00000=0000, 65535=FFFF, 65536=0000,
|  and 99999 = 869F, e.g. 01202 782621 = 82621 = 0x142BD = 42BD. */
struct {
    unsigned int    curr_len;
    unsigned char   curr_LSB;
    unsigned char   curr_MSB;
} omu_phone_number;

/* the fault reporting urgency for the 'MOVA/ST800 Serial Link Failed' fault */
struct {
    unsigned int    curr_len;
    unsigned char   curr_data;
} mova_no_messages_rut;

/* 'MOVA/ST800 Serial Link Failed' raised if no messages for this many seconds */
struct {
    unsigned int    curr_len;
    unsigned char   curr_data;
} mova_no_messages_timeout;


/* Flag which indicates whether the configuration data is good */
extern unsigned char CD_CONFIG_DATA_OK;

/* Total number of MOVA configuration items which can be loaded from RMS */
#define	MV_NUMBER_OF_CONFIGURATION_ITEMS 4

/* MOVA configuration data map */
const struct {
   utiny MV_NUM_ITEMS_IN_MAP;	
   void *MV_ITEM_POINTER[MV_NUMBER_OF_CONFIGURATION_ITEMS];	
   unsigned int	MV_MAX_ITEM_SIZE[MV_NUMBER_OF_CONFIGURATION_ITEMS];	
} MV_CONFIG_MAP =
{
    /* Number of entries */
    MV_NUMBER_OF_CONFIGURATION_ITEMS,
    /* All the pointers first */
    {   &mova_phone_home_rut,
        &omu_phone_number,
        &mova_no_messages_rut,
        &mova_no_messages_timeout,
    },
    {   /* then the lengths of the data */
        1,
        2,
        1,
        1
    }
};

#if defined(_PLUTO) && !defined(WIN32)

/* this is a diab library function and therefore we need to provide
  ourselves for _PLUTO. */

int toascii ( int c )
{
	return ( c & 0x7f );
}

#endif


/**************************************************************************
* PROCEDURE: MOVA_IF_RX_REMOTE_CHAR
*--------------------------------------------------------------------------
* FUNCTION: Called by forward_read_char() in dattran.c with the incoming
* character received over the telephone. It decides whether the character
* should be sent to the OMU application or the MOVA application by detecting
* whether or not the communications start with the identity request message
* from RMS or a CR from MOVAC. Also, if the identity request timer expires,
* see CP_IDENTITY_TIMER(), then MOVA communications are assumed.
* It is also called when a call is initially opened (without a character) so
* that it can immediately decide who the call is for before any characters
* have been received if it can.
**************************************************************************/
void MOVA_IF_RX_REMOTE_CHAR(tbool char_present, utiny read_char)
{
    static utiny CR_count;
    
    /* if remote communications were idle */
    if ( MOVA_IF.RComms == MOVAIF_COMMS_IDLE )
    {
        /* then this is the first pass through in a new call */
        /* initialise the count of carriage returns received */
        CR_count = 0;
        /* if the MOVA application is not enabled */
        if ( MOVA_IF.MOVA_application != MOVA_ENABLED )
        {
            /* pass all characters direct to the OMU application */
            MOVA_IF.RComms = MOVAIF_COMMS_TO_OMU;
        }
        else /* MOVA enabled */
        {
            /* then we need to start checking to see who is talking to us */
            MOVA_IF.RComms = MOVAIF_COMMS_CHECKING;
        }
    }

    /* if a character is present (i.e. given a character that has been received
    |  rather than when the call first opens with no characters yet) */
    if ( char_present )
    {
        /* if checking to see who is talking to us */
        if ( MOVA_IF.RComms == MOVAIF_COMMS_CHECKING )
        {
            /* if the character received is the start of the identity request message */
            if ( read_char == RM_LMA_RX )
            {
                /* if phone line sharing is active */
                if ( phone_line_sharing.active )
                {
                    /* if phone line sharing is active then there is a separate
                    |  OMU on the same telephone line, therefore never let the
                    |  (unconfigured) OMU application in this MOVA unit answer
                    |  the phone otherwise the user may think that their proper
                    |  OMU has been initialised. */
                    /* Just ignore the character as though it were unexpected */
                    CR_count = 0;
                }
                else
                {
                    /* MOVA enabled but phone line sharing is not active */
                    /* then its probably the start of an RMS identity request so
                    |  pass it and all subsequent characters to the OMU */
                    MOVA_IF.RComms = MOVAIF_COMMS_TO_OMU;
                    /* additional parameter required to indicate PSTN connection */
                    RM_RX_CHAR(PSTN_INSTATION, read_char );
                }
            }
            /* if a carriage return (or line feed) character is received */
            else if ( ( read_char == '\r' ) || ( read_char == '\n' ) )
            {
                /* increment the count of carriage returns received */
                CR_count++;
                /* if three have been received in succession */
                if ( CR_count >= 3 )
                {
                    /* then its probably a user wanting to talk to MOVA */
                    /* pass all subsequent characters to the MOVA application */
                    MOVA_IF.RComms = MOVAIF_COMMS_TO_MOVA;
                }
            }
            else /* unexpected character */
            {
                /* clear the count of carriage returns and ignore the character */
                CR_count = 0;
            }
        }
        /* if remote communications are talking to the MOVA application */
        else if ( MOVA_IF.RComms == MOVAIF_COMMS_TO_MOVA )
        {
            /* send characters to the MOVA application */
            MOVA_RX_CHAR( read_char );
        }
        /* if remote communications are talking to the OMU application */
        else if ( MOVA_IF.RComms == MOVAIF_COMMS_TO_OMU )
        {
            /* using incoming message buffer as normal */
            /* additional parameter required to indicate PSTN connection */
            RM_RX_CHAR(PSTN_INSTATION, read_char );
        }
        else
        {
            /* should be no other states */
            /* just set it back to 'idle' just in case */
            MOVA_IF.RComms = MOVAIF_COMMS_IDLE;
        }
    }
}

/**************************************************************************
* PROCEDURE: END_MOVA_CALL
*--------------------------------------------------------------------------
* FUNCTION: Called by various MOVA functions terminate a MOVA remote
* telephone call. Will have no effect if no MOVA telephone call is currently
* in progress.
**************************************************************************/
void END_MOVA_CALL(void)
{
    /* if MOVA remote communications are active */
    if ( MOVA_IF.RComms == MOVAIF_COMMS_TO_MOVA )
    {
        /* terminate the call */
        CP_ABANDON_CALL(0,TRUE,0);
    }
    else
    {
        /* do nothing - no call open or do not want MOVA terminating an RMS call */
    }
}

/**************************************************************************
* PROCEDURE: SendStringToModem
*--------------------------------------------------------------------------
* FUNCTION: Called by various MOVA functions to send characters to the
* instation. Will have no effect if no MOVA telephone call is currently
* in progress.
**************************************************************************/
void SendStringToModem(unsigned char *sOP_String, int nCharsInString)
{
    STATUS nucleus_status;
    NU_DRIVER_REQUEST nucleus_request;

    /* if MOVA remote communications are active */
    if ( MOVA_IF.RComms == MOVAIF_COMMS_TO_MOVA )
    {
        nucleus_request.nu_function = NU_OUTPUT;
        nucleus_request.nu_timeout = NU_NO_SUSPEND;
         
        nucleus_request.nu_request_info.nu_output.nu_buffer_ptr = ( void *)sOP_String;
        nucleus_request.nu_request_info.nu_output.nu_request_size =  nCharsInString;
        nucleus_request.nu_request_info.nu_output.nu_actual_size = 0;
        nucleus_request.nu_request_info.nu_output.nu_logical_unit = MOVA_MODEM_UART;
        /* call to NUCLEUS request directive */
        do
        {
            NU_Sleep(8);
            nucleus_status = NU_Request_Driver( &ESCC2_Driver, &nucleus_request );
        } while ( nucleus_request.nu_status != 0 );
        set_LED_single_flash(IO_MODEM_COMMS_LED_NUM,200);
    }
    else
    {
        /* do nothing - throw text away */
    }
}
   
/**************************************************************************
* PROCEDURE: GetMOVACharFromHandset
*--------------------------------------------------------------------------
* FUNCTION: Called by GetHandsetChar() and GetHandsetCharNE() to read a
* character from the local handset port while MOVA communications are active.
* Returns 0 if no characters have been received or MOVA communications are
* not active.
**************************************************************************/
char GetMOVACharFromHandset(void)
{
    char cResult;
    STATUS nucleus_status;
    NU_DRIVER_REQUEST nucleus_request;

    /* if local communications are talking to the MOVA application */
    if ( MOVA_IF.LComms == MOVAIF_COMMS_TO_MOVA )
    {
        nucleus_request.nu_function = NU_INPUT;
        nucleus_request.nu_timeout = NU_NO_SUSPEND;

        nucleus_request.nu_request_info.nu_input.nu_buffer_ptr = ( void *)&cResult;
        nucleus_request.nu_request_info.nu_input.nu_request_size = 1;
        nucleus_request.nu_request_info.nu_input.nu_actual_size = 0;
        nucleus_request.nu_request_info.nu_input.nu_logical_unit = MOVA_HANDSET_UART;
        /* call to NUCLEUS request driver directive */
        nucleus_status = NU_Request_Driver( &PPCSP_Driver, &nucleus_request );
        if( nucleus_status != NU_SUCCESS )
        {
            SOFT_ERROR(HANDSET_UART_read_failed);
        }
        else
        {
            /* if some characters were read */
            if( nucleus_request.nu_request_info.nu_input.nu_actual_size != 0 )
            {
                /* then cResult already holds the character */
                /* but ensure that all characters are upper case */
                cResult = (char) toupper(cResult);
                /* convert line feeds into carriage returns */
                if ( cResult == cLF ) cResult = cCR;
                /* if its a control character except ENTER and DELETE */
                if ( ( cResult < 32 )
                  && ( cResult != cCR )
                  && ( cResult != cBackSp ) )
                {
                    /* then ignore it */
                    cResult = 0;
                }
            }
            else
            {
                cResult = 0;
            }
        }
    }
    else /* local MOVA communication not active */
    {
        cResult = 0;
    }
    /* end if */
    return (cResult);
}

/**************************************************************************
* PROCEDURE: SendStringToLH
*--------------------------------------------------------------------------
* FUNCTION: Called by various MOVA functions to send text to the local
* handset port during MOVA communications.
**************************************************************************/
void SendStringToLH(char *sOP_String, int nCharsInString)
{
    STATUS nucleus_status;
    NU_DRIVER_REQUEST nucleus_request;

    /* if local communications are talking to the MOVA application */
    if ( MOVA_IF.LComms == MOVAIF_COMMS_TO_MOVA )
    {
        nucleus_request.nu_function = NU_OUTPUT;
        nucleus_request.nu_timeout = NU_NO_SUSPEND;
      
        nucleus_request.nu_request_info.nu_output.nu_buffer_ptr = ( void *)sOP_String;
        nucleus_request.nu_request_info.nu_output.nu_request_size = nCharsInString;
        nucleus_request.nu_request_info.nu_output.nu_actual_size = 0;
        nucleus_request.nu_request_info.nu_output.nu_logical_unit = MOVA_HANDSET_UART;
        /* call to NUCLEUS request directive */
        do
        {
            NU_Sleep(5);
            nucleus_status = NU_Request_Driver( &PPCSP_Driver, &nucleus_request );
        } while ( nucleus_request.nu_status != 0 );
    }
    else
    {
        /* throw text away if local communications are not talking to the MOVA
        |  application. */
    }
}

/**************************************************************************
* PROCEDURE: MOVA_IF_Clock_Change
*--------------------------------------------------------------------------
* FUNCTION: This procedure is called by com_set_clock() to inform MOVA that
* something has changed the clock.
**************************************************************************/
void MOVA_IF_Clock_Change(void)
{
/* Tcomshr and MOVA do not exist in PB680 firmware */
#if defined(PB681) || defined(_PLUTO)
    /*
    -------------- MARK1=8888 is a flag which causes the MONItor program to
    -------------- reload the TIMES(CELL) array from DNUM DTIM site data
    */
    if(Tcomshr->mark1 == 1234) Tcomshr->mark1 = 8888;
#endif
}

/**************************************************************************
* PROCEDURE: Determine_MOVA_IO_Setting
*--------------------------------------------------------------------------
* FUNCTION: This procedure is called by the 200ms periodic routine. It
* attempts to guess whether the MOVA application should be using a MOVA I/O
* card or the enhanced serial link to an ST800 - see ESPort.c
**************************************************************************/
static void Determine_MOVA_IO_Setting(void)
{
    extern utiny ESP_use_ST800_data;
    
    /* just in case - if the state is unknown */
    if ( ( MOVA_IO_Setting != MOVA_IO_UNDEFINED )
      && ( MOVA_IO_Setting != MOVA_IO_CARDS )
      && ( MOVA_IO_Setting != MOVA_IO_USE_ESP ) )
    {
        /* return it to its default value */
        MOVA_IO_Setting = MOVA_IO_UNDEFINED;
    }

    /* if the MOVA application is not enabled */
    if ( MOVA_IF.MOVA_application != MOVA_ENABLED )
    {
        /* then MOVA is not using any I/O */
        MOVA_IO_Setting = MOVA_IO_UNDEFINED;
    }
    /* MOVA enabled - if the setting is still undefined */
    else if ( MOVA_IO_Setting == MOVA_IO_UNDEFINED )
    {
        /* if there are any Bus/MOVA I/O cards fitted */
        if ( ( comms_board_fitted[0] )
          || ( comms_board_fitted[1] )
          || ( comms_board_fitted[2] ) )
        {
            /* then assume MOVA will use one (or more) of these */
            MOVA_IO_Setting = MOVA_IO_CARDS;
            /* but this can be changed using the MIO handset command */
        }
        else /* no Bus/MOVA I/O cards detected */
        {
            /* if the enhanced serial port is active */
            if ( ESP_use_ST800_data )
            {
                /* then assume MOVA will also use this */
                MOVA_IO_Setting = MOVA_IO_USE_ESP;
                /* but this can be changed using the MIO handset command */
            }
            else
            {
                /* wait until either:
                |  1) A MOVA I/O card is fitted to the unit,
                |  2) The ESP link to the controller starts, or
                |  3) The user sets MIO to either '1' or '2'. */
            }
        }
    }
    else
    {
        /* setting made either automatically or by the MIO handset command */
    }
}

/**************************************************************************
* PROCEDURE: Check_MOVA_Fault
*--------------------------------------------------------------------------
* FUNCTION: This procedure is called by both the 200ms periodic routine and
* by MV_FAULT_REGEN() when the fault handler requires regeneration of a
* MOVA RMS fault.
**************************************************************************/
static void Check_MOVA_Fault(utiny fault, utiny regen)
{
    /* need to check for the OMU application running normally */
    extern utiny IC_SOFTWARE_MODE;
    extern utiny IC_MONITORING;
    /* Power fail has been detected and report placed in system log */
    extern utiny power_fail_report_sent;

    /* MV_FAULT is passed on as a ptr so could be accessed outside this routine */
    static utiny MV_FAULT[3];
    utiny RRB;
    
    /* if the phone home flag should be checked */
    if ( fault == II_MOVA_PHONE_HOME_FAULT )
    {
        /* if the OMU is running normally (there's no point raising the fault if
        |  the OMU is not configured and can't dial the instation.) */
        /* and the MOVA application is enabled */
        if ( ( IC_SOFTWARE_MODE == IC_MONITORING )
          && ( MOVA_IF.MOVA_application == MOVA_ENABLED ) )
        {
            /* Copy the required state of the phone home flag fault from the
            |  volatile MOVA_IF store to the non-volatile store */
            if ( MOVA_IF.PhoneHomeFlag ) MOVA_fault.Phone_Home_Curr = 1;
        }
        else /* either OMU or MOVA not running properly */
        {
            /* just ignore any requests to phone home */
            MOVA_IF.PhoneHomeFlag = 0;
            /* but leave the current state of the fault report as it is,
            |  i.e. leave it set if a fault has already been sent. */
        }

        /* if MOVA wants to phone home */
        /* and a fault has not already been raised */
        /* or we have been asked to regenerate the fault */
        if ( ( MOVA_fault.Phone_Home_Curr )
          && ( !MOVA_fault.Phone_Home_Prev || regen ) )
        {
            /* build up the 3 byte (6 nibble) fault report */
          /*MV_FAULT[0] = set-up by II_INITIAL_INTERFACE_CONTROL() */
            MV_FAULT[1] = II_MOVA_PHONE_HOME_FAULT;
            MV_FAULT[2] = 6;
            /* if the report routing byte has been configured from RMS */
            if ( ( CD_CONFIG_DATA_OK ) && ( mova_phone_home_rut.curr_len != 0 ) )
            {
                /* use it */
                RRB = mova_phone_home_rut.curr_data;
            }
            else
            {
                /* otherwise assume it should be reported as urgent */
                RRB = II_ACTIVE_GOING_URGENT_REP;
            }
            /* adjust the routing byte to set the 'regenerated fault' flag if required */
            if ( regen ) RRB |= FT_LSTMSK2;
            /* report the fault */
            II_INITIAL_INTERFACE_CONTROL( MV_FAULT, RRB );
            /* remember that the fault has been raised */
            MOVA_fault.Phone_Home_Prev = 1;
        }
    }
    /* if the MOVA ESP message time-out fault should be checked */
    else if ( fault == II_MOVA_ESP_TIMEOUT_FAULT )
    {
        /* if the request is not just to generate the fault */
        if ( !regen )
        {
            /* if MOVA is not using the ESP link */
            /* or a message has been received (see ESPort.c) */
            if ( ( MOVA_IO_Setting != MOVA_IO_USE_ESP )
              || ( MOVA_IF.ESP_msg_received ) )
            {
                /* then the fault is not active */
                MOVA_fault.No_Messages_Curr = 0;
                /* reset the timer */
                MOVA_fault.No_Messages_Timer = 0;
                /* reset the flag */
                MOVA_IF.ESP_msg_received = 0;
            }
            /* if the timer is still running */
            else if ( MOVA_fault.No_Messages_Timer < 0xFFFF )
            {
                ushort timeout;
            
                /* increment the timer */
                MOVA_fault.No_Messages_Timer++;
                /* if the timeout has been configured from RMS */
                if ( ( CD_CONFIG_DATA_OK ) && ( mova_no_messages_timeout.curr_len != 0 ) )
                {
                    /* use it, but convert the seconds into 200ms units */
                    timeout = mova_no_messages_timeout.curr_data * 5;
                }
                else
                {
                    /* otherwise default it to 30 seconds */
                    timeout = 30*5;
                }
                /* if the timer reaches the required time-out value */
                if ( MOVA_fault.No_Messages_Timer >= timeout )
                {
                    /* if the OMU has raised a power-fail fault */
                    /* and so must be running on its battery */
                    if ( power_fail_report_sent )
                    {
                        /* don't report a fault since no MOVA messages have been
                        |  received for a while because the power to the site
                        |  has been lost but the OMU/MOVA unit is still running
                        |  on its battery. */
                    }
                    else
                    {
                        /* then a fault should be raised */
                        MOVA_fault.No_Messages_Curr = 1;
                    }
                    /* clear all of the MOVA ESP data held in the unit */
                    memset( &MOVA_IF.ESP_dets[0], 0x00, ESP_MOVA_D_SIZE);
                    memset( &MOVA_IF.ESP_conf[0], 0x00, ESP_MOVA_C_SIZE);
                    MOVA_IF.ESP_CRB = 0x00;
                    /* stop the timer */
                    MOVA_fault.No_Messages_Timer = 0xFFFF;
                }
                /* if no message has been received for 3 seconds (even though
                |  the fault reporting time-out has not expired yet) */
                else if ( MOVA_fault.No_Messages_Timer >= 3*5 )
                {
                    /* clear all of the MOVA ESP data held in the unit */
                    /* (so MOVA stops looking at the old data after 3 secs) */
                    memset( &MOVA_IF.ESP_dets[0], 0x00, ESP_MOVA_D_SIZE);
                    memset( &MOVA_IF.ESP_conf[0], 0x00, ESP_MOVA_C_SIZE);
                    MOVA_IF.ESP_CRB = 0x00;
                }
                else
                {
                    /* timer still running */
                }
            }
            else /* timer has already expired */
            {
                /* if no fault has been raised */
                /* and the OMU is no longer reporting power fail */
                /* (i.e. the mains power to the site has returned) */
                if ( !MOVA_fault.No_Messages_Curr && !power_fail_report_sent )
                {
                    /* restart the timer so a fault will be logged if no MOVA
                    |  message are received for the configured period */
                    MOVA_fault.No_Messages_Timer = 0;
                }
                else
                {
                    /* do nothing - since either:
                    |  1) the 'no messages' fault has been raised and this
                    |     should only be cleared when a good MOVA message is
                    |     received or MIO is no longer set to 2 (which is
                    |     handled by the first path above).
                    |  2) the power has not yet returned (and when it does the
                    |     above path will restart the timer). */
                }
            }
        }

        /* if the current state of the fault does not match the previous state */
        /* or we have been asked to regenerate the fault and its active */
        if ( ( MOVA_fault.No_Messages_Curr != MOVA_fault.No_Messages_Prev )
          || ( regen && MOVA_fault.No_Messages_Curr ) )
        {
            /* build up the 3 byte (6 nibble) fault report */
          /*MV_FAULT[0] = set-up by II_INITIAL_INTERFACE_CONTROL() */
            MV_FAULT[1] = II_MOVA_ESP_TIMEOUT_FAULT;
            MV_FAULT[2] = 6;
            /* if a fault should not be raised */
            if ( !MOVA_fault.No_Messages_Curr )
            {
                /* then adjust the fault code to be the clearance number */
                MV_FAULT[1] += 1;
            }
            /* if the report routing byte has been configured from RMS */
            if ( ( CD_CONFIG_DATA_OK ) && ( mova_no_messages_rut.curr_len != 0 ) )
            {
                /* use it */
                RRB = mova_no_messages_rut.curr_data;
            }
            else
            {
                /* otherwise assume it should be reported as urgent */
                RRB = II_ACTIVE_GOING_URGENT_REP;
            }
            /* adjust the routing byte to set the 'regenerated fault' flag if required */
            if ( regen ) RRB |= FT_LSTMSK2;
            /* report the fault */
            II_INITIAL_INTERFACE_CONTROL( MV_FAULT, RRB );
            /* update the previous state of this fault */
            MOVA_fault.No_Messages_Prev = MOVA_fault.No_Messages_Curr;
        }
    }
    /* if the MOVA disabled fault should be checked */
    else if ( fault == II_MOVA_DISABLED_FAULT )
    {
        /* if the request is not just to generate the fault */
        if ( !regen )
        {
#if defined(PB681) || defined(_PLUTO) /* MOVA and Tcomshr not available in PB680 build */
            /* if the 'MOVA enabled' flag is not set, but it is configured as enabled */
            if ( (Tcomshr->io[27] == 0) && (MOVA_IF.MOVA_application == MOVA_ENABLED) )
            {
                /* set the current state of the MOVA disabled fault */
                MOVA_fault.MOVA_Disabled_Curr = 1;
            }
            else
#endif
            /* MOVA enabled or PB680 firmware or MOVA not configured (on PLUTO) */
            {
                /* clear the current state of the MOVA disabled fault */
                MOVA_fault.MOVA_Disabled_Curr = 0;
            }
        }

        /* if the current state of the fault does not match the previous state */
        /* or we have been asked to regenerate the fault and its active */
        if ( ( MOVA_fault.MOVA_Disabled_Curr != MOVA_fault.MOVA_Disabled_Prev )
          || ( regen && MOVA_fault.MOVA_Disabled_Curr ) )
        {
            /* build up the 3 byte (6 nibble) fault report */
          /*MV_FAULT[0] = set-up by II_INITIAL_INTERFACE_CONTROL() */
            MV_FAULT[1] = II_MOVA_DISABLED_FAULT;
            MV_FAULT[2] = 6;
            /* if the fault is not active */
            if ( !MOVA_fault.MOVA_Disabled_Curr )
            {
                /* then adjust the fault code to be the clearance number */
                MV_FAULT[1] += 1;
            }
            /* always assume fault should be reported as urgent */
            RRB = II_ACTIVE_GOING_URGENT_REP;
            /* adjust the routing byte to set the 'regenerated fault' flag if required */
            if ( regen ) RRB |= FT_LSTMSK2;
            /* report the fault */
            II_INITIAL_INTERFACE_CONTROL( MV_FAULT, RRB );
            /* update the previous state of this fault */
            MOVA_fault.MOVA_Disabled_Prev = MOVA_fault.MOVA_Disabled_Curr;
        }
    }
}

/**************************************************************************
* PROCEDURE: MV_FAULT_REGEN
*--------------------------------------------------------------------------
* FUNCTION: This procedure is called by the fault handler when it requires
* regeneration of particular fault reports within this module.
**************************************************************************/
void MV_FAULT_REGEN(utiny FAULT_NUMBER)
{
    /* if the MOVA phone home fault report should be regenerated */
    if ( FAULT_NUMBER == II_MOVA_PHONE_HOME_FAULT )
    {
        /* call the normal routine which checks for the MOVA application wanting
        |  to phone home, but adjust the report routing byte to say that the
        |  fault is a re-report */
        Check_MOVA_Fault(II_MOVA_PHONE_HOME_FAULT,1);
    }
    /* if the MOVA ESP message timeout fault should be regenerated */
    else if ( FAULT_NUMBER == II_MOVA_ESP_TIMEOUT_FAULT )
    {
        /* call the normal routine which checks, but adjust the report routing
        |  byte to say that the fault is a re-report */
        Check_MOVA_Fault(II_MOVA_ESP_TIMEOUT_FAULT,1);
    }
    /* if the MOVA disabled fault should be regenerated */
    else if ( FAULT_NUMBER == II_MOVA_DISABLED_FAULT )
    {
        /* call the normal routine which checks, but adjust the report routing
        |  byte to say that the fault is a re-report */
        Check_MOVA_Fault(II_MOVA_DISABLED_FAULT,1);
    }
}

/**************************************************************************
* PROCEDURE: MV_FAULT_RESET
*--------------------------------------------------------------------------
* FUNCTION: This procedure is called by the fault handler. Its function is
* to reset any currently stored faults. Any further occurances of these
* faults will be reported.
**************************************************************************/
void MV_FAULT_RESET(utiny MV_FAULT_CLASS, utiny MV_FAULT_INDEX)
{
    /* if the request is to reset MOVA faults */
    if ( MV_FAULT_CLASS == II_RESET_MOVA_FAULTS )
    {
        /* NB: Fault index currently ignored */
        /* Clear the copies of the phone home flag. When the MOVA application */
        /* wants to phone home again, a new fault will be raised. */
        MOVA_IF.PhoneHomeFlag = 0;
        MOVA_fault.Phone_Home_Curr = 0;
        MOVA_fault.Phone_Home_Prev = 0;
        /* MOVA ESP no messages fault is not manually reset */
    }
}

/*----- phone_line_sharing_facility ------------------------------------------
| Called every 200ms by MOVA_IF_200() to handle the phone line sharing facility
| which allows this MOVA unit to share the same phone line as a separate OMU.
| The OMU normally activates the 'disable modem' signal (leaving it open circuit)
| which forces the MOVA unit to switch off its modem (so only the OMU will answer
| a call). When the MOVA unit wants to dial out, it activate the 'MOVA Fault'
| signal. When the OMU confirms this signal and has initiated sending a fault
| report to the instation, it de-activates (closes) the 'disable modem' signal
| following which the MOVA unit drops the 'MOVA fault' signal which, in turn,
| causes the OMU to re-raise (open) the 'disable modem' signal.
| When the instation wishes to call the MOVA unit, it first dials the OMU and
| lowers (closes) the 'disable modem' signal. This is confirmed by the MOVA
| unit which then switches on (and configures) its modem. The instation would
| then drop the call to the OMU and ring back using the MOVA instation. When a
| call to the MOVA is successfully terminated, then the MOVA unit pulses the
| 'MOVA fault' signal (on and then off) which tells the OMU to raise (open) the
| 'disable modem' signal again. Alternatively, the OMU will raise (open) the
| signal after five minutes in case no successful call to the MOVA unit occurs.
|
|                Power-Up        MOVA Fault               Enable MOVA         
|                ___________     __________        __     __              __
| Disable Modem: XXXXXXX/                  \______/         \___.._______/  
|                                     <-1s->                                
|                     /              /       \  /                      /    
|                   _               ___________                      _      
| MOVA Fault:    __/ \______     __/           \_____     ______..__/ \_____
|
| Note that if this facility is not required, then the 'disable modem' input
| should be connected directly to 0 volts (Comm's/MOVA I/O card only).
+---------------------------------------------------------------------------*/
static void phone_line_sharing_facility(void)
{
    /* NB: phone_line_sharing initialised to all zero's every power-up, so... */
    /* previous state of the 'DM' input - value on power-up does not matter as
    |  the confirm counter will be reset on power-up whether the input has
    |  changed state or not */
    static tbool previous_input;
    /* time delays in the state table - initialised on power-up by state 0
    |  because the state variable is cleared. */
    static utiny timer;

    /* This routine uses a state table where each step is normally executed
    |  every 200ms. The #define's below improve the readability of this state
    |  table.
    |
    | WARNING: These macro's include the 'break' command and so must be the last
    |          line that is intended to be executed within each path.
    */
#define PLS_STAY_HERE        break;
#define PLS_NEXT_STEP        phone_line_sharing.state++; break;
#define PLS_GOTO(new_state)  phone_line_sharing.state = PLS_##new_state##_STATE; break;

#define PLS_PULSE_OUTPUT_STATE   5
#define PLS_NOT_ACTIVE_STATE     9
#define PLS_IDLE_STATE          10
#define PLS_SEND_FAULT_STATE    15

    /* If not already set-up, the unit needs to determine whether or not phone
    |  line sharing to a separate OMU is required. */
    if ( ( phone_line_sharing_kept.initialised != PSL_INIT_CODE )
      || ( phone_line_sharing_kept.setting > 1 ) )
    {
        /* if the first card fitted is a MOVA I/O card */
        if ( comms_board_fitted[0] )
        {
            /* then it doesn't look like the OMU application is going to be used
            |  so assume that phone line sharing to a separate OMU is required */
            phone_line_sharing_kept.setting = 1;
        }
        else
        {
            /* then it looks like the OMU application is going to be used so
            |  assume that phone line sharing to a separate OMU is not required */
            phone_line_sharing_kept.setting = 0;
        }
        /* remember that this information has been set up */
        phone_line_sharing_kept.initialised = PSL_INIT_CODE;
    }

    /* NB: 'phone_line_sharing_kept.setting' can also be modified using the
    |  handset command 'PLS' (as well as the above) so we need to check to see
    |  if the facility has just been enabled or disabled. */
    
    /* if the MOVA application is not enabled */
    /* or phone line sharing is not required */
    /* but the facility is currently active */
    if ( ( MOVA_IF.MOVA_application != MOVA_ENABLED )
      || ( ( phone_line_sharing_kept.setting == 0 )
        && ( phone_line_sharing.active ) ) )
    {
        /* turn the output off */
        OutputScan(MOVA_FAULT_OFF);
        /* go immediately to (and sit in) the not active state */
        phone_line_sharing.state = PLS_NOT_ACTIVE_STATE;
        /* switch off the facility */
        phone_line_sharing_kept.setting = 0;
        phone_line_sharing.active = FALSE;
        /* do not switch off the modem */
        phone_line_sharing.disable_modem = FALSE;
    }

    /* if phone line sharing is required */
    /* but the facility is not active */
    if ( ( phone_line_sharing_kept.setting == 1 )
      && ( !phone_line_sharing.active ) )
    {
        /* clear the facility's working data as though a power-up had occured */
        memset(&phone_line_sharing,0,sizeof(phone_line_sharing));
        /* and make the facility active */
        phone_line_sharing.active = TRUE;
    }

    
    /* if this unit has just powered-up, then pulse the dial request output
    |  as this will automatically force the OMU (if configured correctly) to
    |  disable this MOVA unit's modem */
    switch ( phone_line_sharing.state )
    {
        /* Power-up (because the state variable has been cleared) */
        case  0:
        {
            /* ensure the MOVA fault output is initially inactive so no fault is
            |  raise by the OMU unit */
            OutputScan(MOVA_FAULT_OFF);
            /* also tidy-up local static variables */
            previous_input = 0;
            timer = 0;
            PLS_NEXT_STEP;
        }

        /* after about two seconds activate the output */
        /* (should give the OMU time to power-up) */
        case  1: 
        {
            if ( ++timer < 10 )
            {
                PLS_STAY_HERE;
            }
            else
            {
                PLS_GOTO(PULSE_OUTPUT);
            }
        }

        /* activate the output to the OMU */
        case PLS_PULSE_OUTPUT_STATE: /* 5 */
        {
            OutputScan(MOVA_FAULT_ON);
            PLS_NEXT_STEP;
        }

        case  6: PLS_NEXT_STEP;

        /* 400ms later, de-activate the output */
        /* this will force the OMU to raise the 'disable modem' signal if so configured */
        case  7:
        {
            OutputScan(MOVA_FAULT_OFF);
            PLS_GOTO(NOT_ACTIVE);
        }

        /* wait until the phone line sharing system becomes active */
        case PLS_NOT_ACTIVE_STATE:
        {
            if ( !phone_line_sharing.active )
            {
                PLS_STAY_HERE;
            }
            else
            {
                timer = 0;
                PLS_GOTO(IDLE);
            }
        }

        /* idle state, decide what to do next */
        case PLS_IDLE_STATE: /* 10 */
        {
            /* when a call finishes */
            if ( phone_line_sharing.call_complete )
            {
                /* pulse the output */
                phone_line_sharing.call_complete = FALSE;
                PLS_GOTO(PULSE_OUTPUT);
            }
            else
            {
                /* if conditions are not right to inform the OMU about a MOVA fault */
                /* i.e. OMU signal is not in its normally active state */
                /*      or the MOVA unit does not want to dial out */
                /*      or the MOVA unit is currently on-line */
                if ( ( !phone_line_sharing.disable_modem )
                  || ( !phone_line_sharing.input_from_OMU )
                  || ( !phone_line_sharing.fault_rqst )
                  || ( con_stat_line_open ) )
                {
                    /* then reset the timer (see below) */
                    timer = 0;
                    PLS_STAY_HERE;
                }
                else
                {
                    /* wait 3 seconds and then send the fault to the OMU */
                    timer++;
                    if ( timer < 15 )
                    {
                        /* wait a bit longer */
                        PLS_STAY_HERE;
                    }
                    else
                    {
                        timer = 0;
                        PLS_GOTO(SEND_FAULT);
                    }
                }
            }
        }

        /* activate the fault output to the OMU */
        case PLS_SEND_FAULT_STATE: /* 15 */
        {
            OutputScan(MOVA_FAULT_ON);
            PLS_NEXT_STEP;
        }

        /* wait until the OMU confirms this */
        case 16:
        {
            /* if the modem disable signal is still active (open circuit) */
            /* (either this or the previous sample) */
            if ( phone_line_sharing.input_from_OMU || previous_input )
            {
                /* then stay here with the fault output active */
                PLS_STAY_HERE;
            }
            else /* signal was inactive (closed circuit) on two samples */
            {
                /* remove MOVA fault output */
                OutputScan(MOVA_FAULT_OFF);
                /* tell ANSWER.C that its request to dial out has been processed */
                phone_line_sharing.fault_rqst = FALSE;
                phone_line_sharing.fault_sent = TRUE;
                /* go back to idle state */
                PLS_GOTO(IDLE);
            }
        }
                    
        /* unexpected state - log error and re-boot */
        default:
        {
            SOFT_ERROR(dattran_task_unex_PLS_state);
        }
    }

    /* debounce the 'disable modem' signal from the OMU */
    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

    /* if the input from the OMU has changed */
    if ( phone_line_sharing.input_from_OMU != previous_input )
    {
        /* restart the confirm counter */
        phone_line_sharing.input_confirm_counter = 0;
        /* and remember the new state */
        previous_input = phone_line_sharing.input_from_OMU;
    }
    /* if the confirm counter is running */
    else if ( phone_line_sharing.input_confirm_counter < 255 )
    {
        /* increment the confirm counter */
        phone_line_sharing.input_confirm_counter++;
        /* if the counter times 1 seconds */
        if ( phone_line_sharing.input_confirm_counter >= 5 )
        {
            /* stop the confirm counter */
            phone_line_sharing.input_confirm_counter = 255;
            /* the input has been confirmed (use 'previous_input' as the
            |  the 'live' input may have just changed state */
            phone_line_sharing.confirmed_input = previous_input;
            /* if the input from the OMU to disable the modem is active */
            /* and the facility is required */
            if ( ( phone_line_sharing.confirmed_input )
              && ( phone_line_sharing_kept.setting == 1 ) )
            {
                /* disable the modem */
                phone_line_sharing.disable_modem = TRUE;
            }
            else
            {
                /* disable modem not requested, because either:
                | a) The DM input is now inactive because the OMU has closed
                |    its output allowing MOVA to answer the next telephone call.
                | b) The DM input is inactive because it has been connected to
                |    ground to disable the facility (at old installations only
                |    the MOVA unit could dial a modem and printer).
                | c) The facility is not required (possibly because the OMU
                |    within this combined unit is going to be used). */
                /* allow the modem on */
                phone_line_sharing.disable_modem = FALSE;
            }
        }
    }
    else
    {
        /* input debounced and idle */
    }
}

/**************************************************************************
* PROCEDURE: Update_MOVA_SC
*--------------------------------------------------------------------------
* FUNCTION: Called by MOVA_IF_200() every 200ms to update the information
* provided to special conditioning.
**************************************************************************/
static void Update_MOVA_SC(void)
{
#if defined(PB681) || defined(_PLUTO) /* Tcomshr not available in PB680 build */
    /* if the MOVA application is enabled */
    if ( MOVA_IF.MOVA_application == MOVA_ENABLED )
    {
        /* tell conditioning that the MOVA application is enabled */
        MOVA_SC.MOVA_application = 0xFF;
        /* Update the little-endian copies of the force bits */
        MOVA_SC.Outputs_LSB = (utiny)MOVA_IF.Outputs;
        MOVA_SC.Outputs_MSB = (utiny)(MOVA_IF.Outputs>>8);
        /* if the 'MOVA enabled' flag is set */
        if ( Tcomshr->io[27] == 1 )
        {
            /* Set the MOVA enabled flag for special conditioning */
            MOVA_SC.MOVA_Enabled = 0xFF;
            /* If the 'On Control' flag is set */
            if ( Tcomshr->io[19] == 1 )
            {
                /* Set the 'On Control' flag for special conditioning */
                MOVA_SC.On_Control = 0xFF;
            }
            else
            {
                /* Clear the 'On Control' flag for special conditioning */
                MOVA_SC.On_Control = 0x00;
            }
        }
        else
        {
            /* Clear the 'MOVA enabled' flag for special conditioning */
            MOVA_SC.MOVA_Enabled = 0x00;
            /* Clear the 'On Control' flag for special conditioning */
            MOVA_SC.On_Control = 0x00;
        }
    }
    else
#endif
    {
        /* Clear all the information as the MOVA tasks are not enabled */
        memset( &MOVA_SC, 0x00, sizeof(MOVA_SC) );
    }
}

/**************************************************************************
* PROCEDURE: Valid_Licence
*--------------------------------------------------------------------------
* FUNCTION: This function returns TRUE if the licence number is valid.
* It is called every 200ms by MOVA_IF_200() to keep checking that the
* licence is still valid, and also by USERBIG.C to stop the user enabling
* MOVA if the licence is not valid.
*
* Serial Number                  Site Name               Phone Number
*     321                       J05SCN3.DT               01202 578731
*    0x141            1st 6:  J  0  5  S  C  N      Last 5:     78731
*      / |            ASCII: 4A 30 35 53 43 3E      Hex:      0x1338B
*     4  1                   -A -0 -5 -3 -3 -E             3  3  8  B
*    S1 S2                   M1 M2 M3 M4 M5 M6            P1 P2 P3 P4
*
* Scramble:
*    S1:M1:P1        S2:M2:P2                M3:M5:P3        M4:M6:P4
*     4  A  3         1  0  3                 5  3  8         3  E  B
*            \       /                               \       /
*  Add:       5  A  6                                 9  2  3
*                |                                       |
*  Decimal:    1446                                    2339
*                |                                       |
*  Add Offset: 3099                                    3992
*
* Thus the licence number for 321 / J05SCN3.DT / 01202 578731 is "LIC=3099 3992"
*******************************************************************************/
char Valid_Licence(void)
{
    short Part1L, Part2L, Part1C, Part2C;
    utiny M[6+1];
    utiny P[4+1];
    char result = 1;
    int n;
    char szSiteName[ xgnFILE_NAME_STR_LEN ];
    
    /* IRH MOD: M5.0.0: 21/07/04: Get site name for plan 1 from new Datahand module */
    Datahand_GetSiteName( 1, szSiteName );
    
    /* copy the first 6 characters out of MOVA's plan 1 backup store */
    memcpy(&M[1], &( szSiteName[0] ), 6 );    
    
    /* just need the least signicant nibble (NB: already held as ASCII) */
    for ( n=1; n<=6; n++ ) M[n] &= 0x0F;

    /* extract the parts of the phone number - note that this has already been
    |  converted to hex by RMS and if it was not downloaded as part of the
    |  configuration, any old value has been cleared - see MOVA_IF_200(). */
    P[1] = (utiny)(licence.pnum_MSB >> 4);
    P[2] = (utiny)(licence.pnum_MSB & 0x0F);
    P[3] = (utiny)(licence.pnum_LSB >> 4);
    P[4] = (utiny)(licence.pnum_LSB & 0x0F);

    /* extract the two parts of the licence and subtract the offset */
    Part1L = (short)(licence.lic1 - 1653);
    Part2L = (short)(licence.lic2 - 1653);

    /* recalculate what they should be from the raw data */

    /* the first part is made up of S1:M1:P1 + S2:M2:P2 */
    /* the unit does not know the serial number so only compare bottom 2 nibbles */
    Part1C  = (short)( (M[1]<<4) + P[1] );
    Part1C += (short)( (M[2]<<4) + P[2] );
    /* if the licence does not match the calculated value, licence is invalid */
    if ( ( Part1L & 0x0FF ) != ( Part1C & 0x0FF ) ) result=0;

    /* the second part is made up of M3:M5:P3 + M4:M6:P4 */
    Part2C  = (short)( (M[3]<<8) + (M[5]<<4) + P[3] );
    Part2C += (short)( (M[4]<<8) + (M[6]<<4) + P[4] );
    /* if the licence does not match the calculated value, licence is invalid */
    if ( Part2L != Part2C ) result=0;

    /* if the licence number appears invalid
    |  but the OMU phone number does not appear to have been configured */
    if ( ( result == 0 )
      && ( licence.pnum_MSB == 0x00 )
      && ( licence.pnum_LSB == 0x00 ) )
    {
        /* then check to see if the only reason the licence number has failed is
        |  because the OMU has not been configured yet, i.e. re-check the
        |  licence number but ignore the phone number nibbles.
        |
        |  The above calculated values for the licence number (Part1C & Part2C)
        |  were constructed with Pn=0. However, the real licence number (Part1L
        |  & Part2L) could have had any values for Pn and so the addition of
        |  P1+P2 in the first part of the licence number and the addition of
        |  P3+P4 in the second could have overflowed, incrementing the Mn parts
        |  of the licence number. Therefore, if we are to check whether or not
        |  the licence number is valid for just the MOVA site name nibbles, we
        |  must do the comparison of the Mn nibbles with and without an overflow
        |  from the Pn nibbles. Note that we do not have to worry about any
        |  overflow from the M5+M6 nibble into the M3+M4 nibble since this will
        |  have been included in the original calculation of Part2C.
        |
        |                    Part1L                   Part1C
        |         
        |               S1  :   M1  : P1          00:   M1  :00
        |          +    S2  :   M2  : P2        + 00:   M2  :00
        |            =====================        =============
        |               XX  : M1+M2 :P1+P2        XX: M1+M2 :00
        |         or    XX  :M1+M2+1:P1+P2    
        |
        |
        |                    Part2L                   Part2C
        |         
        |               M3  :   M5  : P3             M3  :   M5  :00
        |          +    M4  :   M6  : P4        +    M4  :   M6  :00
        |            ================-----        ==================
        |             M3+M4 : M5+M6 :P3+P4         M3+M4 : M5+M6 :00
        |         or M3+M4+1: M5+M6 :P3+P4     or M3+M4+1: M5+M6 :00
        |         or  M3+M4 :M5+M6+1:P3+P4
        |         or M3+M4+1:M5+M6+1:P3+P4
        */
        
        /* if the MOVA site name nibbles in the first half of the licence number
        |  and the MOVA site name nibbles in the second half match */
        if ( ( ( (Part1L & 0x0F0) == (Part1C & 0x0F0) )
            || ( (Part1L & 0x0F0) == ((Part1C+0x010) & 0x0F0) ) )
          && ( ( (Part2L & 0xFF0) == (Part2C & 0xFF0) )
            || ( (Part2L & 0xFF0) == ((Part2C+0x010) & 0xFF0) ) ) )
        {
            /* then the licence number is actually valid */
            result = 1;
        }
    }
    
    /* return the result to the caller (0=invalid, !0=valid) */
    return (result);
}

/**************************************************************************
* PROCEDURE: MOVA_Monitor
*--------------------------------------------------------------------------
* FUNCTION: This procedure is called every 200ms (by MOVA_IF_200) to
* monitor the MOVA application.
* 1) It disables MOVA if the licence number is no longer valid.
* 2) It tries to put MOVA back on-control if it appears to have gone off
*    control for some reason (so we don't have to wait until the next hour)
**************************************************************************/
void MOVA_Monitor(void)
{
#if defined(PB681) || defined(_PLUTO) /* Tcomshr NOT available in PB680 build */
    /* Check that the licence number is valid every 200ms */
    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    /* if the licence is not valid but 'MOVA enabled' is set to '1'
	   NB: Allow both the old style licence (using LIC + site data name) and the
	       new style licence (using LIN, LIF + card serial number) */
    if ( ( Tcomshr->io[27] == 1 ) && !Valid_Licence() && !check_facility_licence(LIC_MOVA))
    {
        extern char control[NumberOfForce+NumberOfExtra];
        int i;
        
        /* clear 'MOVA enabled' back to zero */
        Tcomshr->io[27] = 0;
        /* clear the 'On Control' flag if it is also set */
        if ( Tcomshr->io[19] == 1 ) 
        {
            Tcomshr->io[19] = 0;
        }
        /* also need to actually clear the force, HI and TO bits */
        for(i=1; i<=ForceArraySize; i++)
        {
            control[i-1] = Terrlog->stgoff;
        }
        Switch_MOVA_Off(NO_SAV_MESSAGES, NO_REBOOT);
    }

    /* Check that MOVA is actually configured/enabled (since on PLUTO platform it may not be) */
    if(MOVA_IF.MOVA_application == MOVA_ENABLED)
    {
        /* If MOVA drops off control for some reason, try to put it back on */
        /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
        /* if MOVA appears to be off for a proper reason */
        if ( ( Tcomshr->io[27] != 1 )   /* MOVA not enabled */
          || ( Tcomshr->io[21] == 0 )   /* or Controller not ready */
          || ( Tcomshr->io[30] != 0 )   /* or multiple stage confirms */
          || ( Tcomshr->warmup <= Tcomshr->stages ) ) /* or still in warmup */
        {
            /* then just reset the timer */
            MOVA_IF.MOVA_Off_Timer = 0;
        }
        /* else, if MOVA is really on control */
        else if ( ( Tcomshr->io[27] == 1 )       /* MOVA enabled */
               && ( Tcomshr->io[19] == 1 )       /* MOVA on-control */
               && ( MOVA_IF.Outputs & 0x8000 ) ) /* TO bit is active */
        {
            /* keep restarting the timer */
            MOVA_IF.MOVA_Off_Timer = 1;
        }
        /* else (MOVA not on control) */
        /* if MOVA was not previously on control */
        /* (or we've already tried to put it back on control) */
        else if ( MOVA_IF.MOVA_Off_Timer == 0 )
        {
            /* then do nothing */
        }
        /* if the MOVA gone off control timer has expired */
        /* (the timer gives MOVA the chance to put itself back on control or make
        |   it clear why it has gone off control) */
        else if ( MOVA_IF.MOVA_Off_Timer >= 3*5 )
        {
            /* try to put MOVA back on control by decrementing the warmup counter */
            /* so on the next stage change, MOVA should go back on control */
            if(Tcomshr->warmup > Tcomshr->stages) Tcomshr->warmup = Tcomshr->stages;
            /* and reset the timer */
            MOVA_IF.MOVA_Off_Timer = 0;
        }
        else
        {
            /* increment the timer */
            MOVA_IF.MOVA_Off_Timer++;
        }
    }
#endif
}
    
/**************************************************************************
* PROCEDURE: MOVA_IF_200
*--------------------------------------------------------------------------
* FUNCTION: This procedure is called every 200ms (by the serial switcher)
* to perform various periodic tasks in the OMU/MOVA interface, whether the
* MOVA tasks are enabled or not.
**************************************************************************/
void MOVA_IF_200(void)
{
    extern utiny IC_SOFTWARE_MODE;
    extern utiny IC_MONITORING;

    /* Periodically try to determine whether MOVA should be using an I/O */
    /* card or the enhanced serial link - see ESPort.C ~~~~~~~~~~~~~~~~~ */
    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    Determine_MOVA_IO_Setting();
    
    /* Periodically check to see if the MOVA application wants to dial-out */
    /* Or whether a MOVA ESP message time-out fault should be reported.    */
    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    Check_MOVA_Fault(II_MOVA_PHONE_HOME_FAULT,0);
    Check_MOVA_Fault(II_MOVA_ESP_TIMEOUT_FAULT,0);
    Check_MOVA_Fault(II_MOVA_DISABLED_FAULT,0);

    /* Run MOVA's phone line sharing facility once every 200ms */
    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    phone_line_sharing_facility();

    /* Set-up items for the OMU special conditioning to read */
    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    Update_MOVA_SC();

    /* Update the preserved copy of the OMU's phone number every 200ms */
    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    /* if the OMU has been configured and is operating normally */
    if ( IC_SOFTWARE_MODE == IC_MONITORING )
    {
        /* if the special phone number data has been loaded */
        if ( omu_phone_number.curr_len == 2 )
        {
            /* make a copy which will not normally be re-initialised */
            licence.pnum_LSB = omu_phone_number.curr_LSB;
            licence.pnum_MSB = omu_phone_number.curr_MSB;
        }
        /* the special phone number data has not been configured */
        else
        {
            /* The OMU has been configured without the new phone number item.
            |  Set the copy to all zeroes so a licence code which does not need
            |  the phone number will be accepted. */
            licence.pnum_LSB = 0x00;
            licence.pnum_MSB = 0x00;
        }
    }
    else
    {
        /* while the OMU is not running normally (e.g. is not configured) do not
        |  clear or try to set-up the copy so that any old value is preserved.*/
    }

    /* Monitor MOVA every 200ms */
    /* ~~~~~~~~~~~~~~~~~~~~~~~~ */
    MOVA_Monitor();

}
/**************************************************************************
* PROCEDURE: Load_Default_MOVA_Config
*--------------------------------------------------------------------------
* FUNCTION: This procedure is called when LDV=7 is entered on the handset.
* The MOVA facility is selected for use, although it will not be fully
* enabled until the correct licence code is entered.
**************************************************************************/
#if defined(_PLUTO)
void Load_Default_MOVA_Config (unsigned char LDV_value)
{
    switch( LDV_value )
    {
        case MOVA_DEFAULTS :
        {
            MOVA_IF.MOVA_application = MOVA_ENABLED;
            break;
        }
        default:
            break;
    }
}
#endif


