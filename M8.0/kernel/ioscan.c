/*
Module name . . . . . . . . . . . . . . . ioscan.c
Module version  . . . . . . . . . . . . . SIE_17
Last modification date  . . . . . . . . . 07/10/04
MOVA version at which this 
    module was last modified  . . . . . . M5.0.0
Authority . . . . . . . . . . . . . . . . TRL Ltd

          Last change:  IRH  07 Oct 04    3:24 pm

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
   SIE_01   28/1/98     MRC                The output scan routine now looks 
                                           to the global array 'control' to 
                                           force stages rather than 
                                           receiving parameters.
   SIE_02   10/3/98     MRC                Clears all force stages when using variable
                                           positioned io. walamp[1] set in OutputScan()
   SIE_03   29/4/98     PC                 Modified 'uMOVA_Forc_[]' so that it
                                           uses the #define's for io_output()
                                           declared in INPOUP.H
   SIE_04   29/4/98     PC                 'const' added to initialised static's
                                           (Siemens fault report MOVA-13)
   SIE_05   01/5/98     PC                 Modified all routines so that they
                                           can work with either type of i/o card.
                                           (Siemens fault report MOVA-23)
   SIE_06   03/5/98     PC                 Overlay on 'Xcomshr', etc., replaced
                                           by explicit use of structures.
   SIE_07   05/5/98     PC                 Added the 'phone_line_sharing' facility
                                           (Siemens fault report MOVA-6)
   SIE_08   15/5/98     PC                 InputMap() replaced completely by
                                           InputScan() for new I/O layout changes
                                           (Siemens fault report MOVA-31)
   SIE_09   21/5/98     PC                 Both InputScan() and OutputScan()
                                           modified for I/O layout changes.
                                           (Siemens fault report MOVA-37)
   SIE_10   01/6/98     PC                 'enable_sync_pulse' no longer const
                                           as then cannot be changed when using
                                           a PROM build.
                                           (Siemens fault report MOVA-57)

 Changes to PB681 issue 1 for issue 2:-

   SIE_11   18/6/98     PC                 'modem_disable_signal' sense reversed
                                           so that open circuit means disable
                                           the modem (new I/O cards only)
                                           (Siemens fault report MOVA-61)

 Changes for combined OMU and MOVA:-
   SIE_12   12/1/00     PC  - Modified OutputScan() so it no longer uses the
                              standard OMU function to set outputs since they
                              have been changed so they do not write to outputs
                              used by the MOVA force bits.
                            - Modified InputScan() so it does not read a DM
                              input if an LMU I/O card is fitted.

   SIE_13   01/3/00     PC  - Modified InputScan() so that it reads MOVA inputs
                              from the enhanced serial port (see ESPort.c) 
                              rather than I/O cards depending on MIO setting.

   SIE_14   09/3/00     PC  - Modified InputScan() so that it latches active
                              detectors for the 'Look' screen so that short
                              detections seen by MOVA do actually appear on
                              the screen (and those actually missed do not).

   SIE_15   10/3/00     PC  - Corrected InputScan() - Always read the 'DM'
                              input from the MOVA I/O card even if its not the
                              first card (because PLS can be set to '1' by the
                              user even though an OMU I/O card is fitted first).

   SIE_16   26/3/00     PC  - Improved InputScan() so that detector changes in
                              either direction are latched for the Look screen.
                              
   SIE_17   07/10/04    IRH - Added <string.h> for memset (compiler warning)
*/

/****************************************************************************
 *
 *    Module IOSCAN
 * 
 *    Acts as the interface between the routines that scan the digital inputs
 *    and outputs and MOVA.
 *    
 *    Calls into the module are from DETSCN and GENSTG. Other routines also use
 *    OutputScan() to set other outputs, such as MONITR controlling the sync.
 *    pulse. Additionally, any task may call into the module via Switch_MOVA_Off()
 *    to switch MOVA off-control when errors are detected.
 *    
 ****************************************************************************/

/* #Include files */
#include <nucleus.h>
#include <string.h> /* IRH MOD: M5.0.0: 07/10/04: Added for memset() */
#include <proj_def.h>
#include <hardware.h>
#include <inpoup.h>
#include "gbltypes.h"
#include <keptdata.h>
#include <diff_bss.h>

/* Function prototypes */
void InputScan(void);
void OutputScan(int nMode);

/*
| The following flag is used to enable the sync pulse output used to synchronise
| the simulator running on the PC with the MOVA unit. To enable the facility,
| either define 'SYNCPULSE' at compilation time, or change the value of the flag
| using the debug kit. By default, the output is disabled as it is not required
| on production MOVA units (only those used for testing) and switching the relay
| twice every second will wear-out the relay after a few years!
*/

volatile unsigned char enable_sync_pulse =  0 ;

/* map to access the 6 input ports on the original 'LMU' i/o cards */
extern volatile unsigned char * const digital_map[];

/* Map to access expanded input capability on the comms/mova i/o cards */
extern volatile unsigned char * const exp_digital_map[MAX_EXPANSION_CARDS*MAX_INPUT_PORTS_PER_CARD];

/*******************************************************************************
| InputScan
|
| Read all the detector and confirm inputs from any combination of I/O cards.
|
| The original OMU's LMU I/O cards had two ports each, whereas the new MOVA I/O
| cards have six ports each.
|
| If no MOVA I/O cards are detected then the I/O on three the old LMU I/O will
| cards be used. On these cards, the CRB and confirms immediately follow the
| detectors.
|
| If MOVA I/O cards are fitted, then each card should have enough inputs for up
| to 32 detectors and 15 confirms bits (plus a CRB), and more than enough force
| bit outputs.
|
| 01234567 01234567 01234567 01234567 01234567 01234567   01234567 01234 5 6 7
| <----------- Detectors -----------> |<-- Confirms -->   <--- Forces -> | |  \
|                                    CRB                                MF SP TO
|
| However, if a MOVA I/O card is fitted in the first position (i.e. there are no
| LMU I/O cards fitted as these must come first) then the last four inputs on
| this first card are used by special functions (detecting low supply and
| switching the modem's power) and one input is reserved for the 'DM' disable
| modem input for phone line sharing. Therefore, this limits the number of
| confirm inputs available on the first card.
|
| 01234567 01234567 01234567 01234567 01234567 01234567   01234567 01234 5 6 7
| <----------- Detectors -----------> |<Confirms->|<XX>   <-- Forces --> | |  \
|                                    CRB          DM                    MF SP TO
|
| In theory the 'DM' input should only be required if the MOVA I/O card is the
| first I/O card fitted. If an OMU I/O card is fitted then this implies that
| the OMU application within this unit is being used, not a separate OMU unit
| to which we are 'phone line sharing'.
| Also, if this is not card 0, then the last four bits on port 5 are not used
| to switch off the modem and so could be used to read another four confirm
| bits.
| However, to keep all the I/O descriptions and cableforms the same regardless
| of where the first MOVA I/O card is fitted, and to allow phone line sharing
| even if an OMU I/O card is fitted (since the user can enter PLS=1), only 3
| confirms are read from port 5 plus the 'DM' input and the remaining four are
| not used.
*******************************************************************************/
/* This sub-routine of InputScan() is used to read a number of bits from an
|  input port into the specified array (either detectors or confirms). */
/* These macro's hide the less pretty parts of the calls to ReadBits() */
/* The CRB bit goes in confin[mxconf-1] hence there is one less stage confirm bit */
#define ReadDets(first,last,port) ReadBits(detsin, &dets_num, mxdets  , first, last, (char)(port))
#define ReadConf(first,last,port) ReadBits(confin, &conf_num, mxconf-1, first, last, (char)(port))

void ReadBits( char *array,
               int* counter_ptr, int limit,
               int first_bit, int last_bit, char port)
{
    int bit;

    /* if MOVA can accept any more of these inputs */
    if ( *counter_ptr < limit )
    {
        /* scroll the port data down until the first required bit is in bit 0 */
        port = port >> first_bit;
    
        /* loop while MOVA can accept more of these inputs */
        /* and more inputs need to be read from this port */
        bit = first_bit;
        while ( ( *counter_ptr < limit ) && ( bit <= last_bit ) )
        {
            /* extract the state of the current detector input bit */
            array[*counter_ptr] = (char)(port & 0x01);
            /* move onto the next input */
            (*counter_ptr)++; bit++; port >>= 1;
        }
    }
}
void InputScan(void) 
{
    /* count the number of detectors and confirm bits read so far */
    int dets_num = 0; /* used by ReadDets() or the loop if no MOVA I/O cards found */
    int conf_num = 0; /* used by ReadConf() or the loop if no MOVA I/O cards found */

    /* assume the 'DM' input is active if we can't read it */
    /* NB: this will only switch off the modem if PLS:1 */
    int modem_disable_signal = TRUE;

    /* find and read all the MOVA I/O cards fitted */
    int card, port_num;
    int found = FALSE;

    /* for each possible comms/mova card */
    for ( card=0; card<MAX_EXPANSION_CARDS; card++ )
    {
        /* if a comms/mova card is fitted in this position */
        if ( comms_board_fitted[card] )
        {
            /* determine the overall port number for the first port on this card */
            port_num = MAX_INPUT_PORTS_PER_CARD * card;

            /* if MOVA is configured to use one or more I/O cards */
            if ( MOVA_IO_Setting == MOVA_IO_CARDS )
            {
                /* read the 32 detector inputs on this card */
                ReadDets(0, 7, *exp_digital_map[port_num+0] ^ 0xFF);
                ReadDets(0, 7, *exp_digital_map[port_num+1] ^ 0xFF);
                ReadDets(0, 7, *exp_digital_map[port_num+2] ^ 0xFF);
                ReadDets(0, 7, *exp_digital_map[port_num+3] ^ 0xFF);
            }

            /* if this is the first MOVA I/O card we have found */
            if ( found == FALSE )
            {
                /* if MOVA is configured to use one or more I/O cards */
                if ( MOVA_IO_Setting == MOVA_IO_CARDS )
                {
					/* If MOVA has been given access to the I/O card */
					if(MOVA_IF.IO_card != 0xFF)
					{
						/* read the Controller Ready Bit */
						confin[mxconf-1] = (*exp_digital_map[port_num+4] ^ 0xFF) & 0x01;
					}
					else /* Another function is using the card, so... */
					{
						/* Force the Controller Ready Bit inactive */
						confin[mxconf-1] = 0;
					}
                    /* read 7 confirm bits from the rest of port 4 */
                    ReadConf(1, 7, *exp_digital_map[port_num+4] ^ 0xFF);
                    /* read 3 confirm bits from the start of port 5 */
                    ReadConf(0, 2, *exp_digital_map[port_num+5] ^ 0xFF);
                }
                /* Always read the DM input regardless of MIO. */
                /* The DM bit is the forth bit on port 5. */
                /* NB: open circuit (1) means disable modem so no invert */
                modem_disable_signal = *exp_digital_map[port_num+5] & 0x08;

                /* The other four bits on this last port are used to switch off
                |  the modem's power and to read low supply if this is card 0.
                |  They are still not used on the first MOVA I/O card even if
                |  it is not card 0 to keep the cableforms, etc., consistant. */

                /* indicate that at least one MOVA I/O card has been found */
                found = TRUE;
            }
            else /* second or third MOVA I/O card */
            {
                /* if MOVA is configured to use one or more I/O cards */
                if ( MOVA_IO_Setting == MOVA_IO_CARDS )
                {
                    /* read 8 confirm bits from port 4 */
                    ReadConf(0, 7, *exp_digital_map[port_num+4] ^ 0xFF);
                    /* read 8 confirm bits from port 5 */
                    ReadConf(0, 7, *exp_digital_map[port_num+5] ^ 0xFF);
                }
            }
        }
    }

    /* if MOVA is configured to use one or more I/O cards */
    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    if ( MOVA_IO_Setting == MOVA_IO_CARDS )
    {
        /* if one or comms/mova i/o cards were found */
        if ( found )
        {
            /* do nothing more - inputs already read by the above */
        }
        else
        {
            int input_num;
            unsigned char input, port_data;

            /* then assume three lmu i/o cards are fitted with two ports each */
            /* with detectors immediately followed by CRB and then confirms */
            port_num = 0;
            /* for each posible input */
            for ( input_num = 0; input_num < 3*2*8; input_num++ )
            {
                /* if this is the first input on the port */
                /* if this is input 0, 8, 16, etc. (quicker than '% 8 == 0')*/
                if ( ( input_num & 7 ) == 0 )
                {
                    /* then read the port contents */
                    port_data = *digital_map[port_num++];
                }
                /* extract the state of the current input */
                input = (unsigned char)(port_data & 0x01);
                /* scroll the data down ready for the next input */
                port_data >>= 1;

#ifdef TEST_MOVA
                /* always read the maximum number of detectors and confirms */
                if ( input_num < mxdets )
                {
                    detsin[dets_num++] = input;
                }
                else if ( conf_num < mxconf )
                {
                    confin[conf_num++] = input;
                }
#else
                /* read in the configured number of detectors */
                if ( input_num < Tcomshr->ndets )
                {
                    detsin[dets_num++] = input;
                }
                /* then the controller ready bit */
                else if ( input_num == Tcomshr->ndets )
                {
                    confin[mxconf-1] = input;
                }
                /* and then the confirms */
                else if ( conf_num < mxconf-1 )
                {
                    confin[conf_num++] = input;
                }
#endif
            }
            /* the modem disable signal uses the very last input on 3 OMU I/O cards */
            /* modem_disable_signal = *digital_map[5] & 0x80; - not any more...
            | There is no requirement for MOVA to read the 'DM' input from OMU
            | I/O cards as no unit is ever equipped like this. If phone line
            | sharing is required, a MOVA I/O card must be fitted.
            | If phone line sharing is enabled, then the unit should always
            | switch off its modem (which is the default state of the flag)
            | since it must assume that it is/will be using the ESP link and an
            | OMU I/O card has been fitted for some reason. It should not switch
            | the modem on and off depending on the state of an input which the
            | handbook does not identify as the location of the 'DM' input on
            | OMU I/O cards.
            | If phone line sharing is disabled, then the unit is a combined
            | OMU/MOVA unit with no MOVA I/O cards fitted and as such must be
            | going to use or is using the ESP link. In this case the 'DM'
            | input will be ignored. */
        }
    } /* end of if MOVA using I/O cards */

    /* otherwise, if it should use the data received over the ESP link */
    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    else if ( MOVA_IO_Setting == MOVA_IO_USE_ESP )
    {
        int count;
        
        /* for each byte of detector data from the controller */
        for ( count=0; count<ESP_MOVA_D_SIZE; count++ )
        {
            /* read all 8 detectors from that byte */
            /* NB: ReadDets() will only read as many as MOVA can handle */
            ReadDets(0, 7, MOVA_IF.ESP_dets[count]);
        }
        /* for each byte of confirm data from the controller */
        for ( count=0; count<ESP_MOVA_C_SIZE; count++ )
        {
            /* read all 8 confirm bits from that byte */
            /* NB: ReadConf() will only read as many as MOVA can handle */
            ReadConf(0, 7, MOVA_IF.ESP_conf[count]);
        }
        /* copy the CRB bit to MOVA's last confirm bit channel */
        confin[mxconf-1] = (char)( MOVA_IF.ESP_CRB & 0x01 );
    } /* end of if MOVA using ESP link */

    /* otherwise, MIO still set to 0 */
    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    else
    {
        /* not yet determined where MOVA should read its input from */
        /* is so clear all of its inputs to the open-circuit state */
        memset( detsin, 0, sizeof(detsin) );
        memset( confin, 0, sizeof(confin) );
    }

    /* process the modem disable signal from a separate OMU */
    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    /* if the modem disable signal is active (open circuit) */
    if ( modem_disable_signal )
    {
        /* then indicate that a separate OMU wants to disable this
        |  MOVA unit's modem - see phone_line_sharing() in MOVA_IF.C */
        phone_line_sharing.input_from_OMU = TRUE;
    }
    else
    {
        phone_line_sharing.input_from_OMU = FALSE;
    }

    /* latch changes in detector states for the 'Look' screen */
    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    /* For each possible detector input */
    for ( dets_num = 0; dets_num < mxdets; dets_num++ )
    {
        /* clear the '0' latch if the detector sample is a '0' this time */
        detsin_0s[dets_num] &= detsin[dets_num];
        /* set the '1' latch if the detector sample is a '1' this time */
        detsin_1s[dets_num] |= detsin[dets_num];
    }
}

 
/*******************************************************************************
| OutputScan
|
| Accept various output requests and adjust them to match the I/O cards fitted.
|
| If one or more new MOVA/Comms I/O cards are detected, then the MOVA outputs
| are allocated to the first of these cards. The test sync pulse (for TRL's
| simulator) and MOVA fault signal (to a separate OMU) use the first two
| outputs followed by the TO and force bits.
|
| If no MOVA/Comms I/O cards are detected, then this function assumes that three
| 'old' LMU I/O cards are fitted and allocates the MOVA outputs to the eleven
| outputs on these cards.
|
| To avoid calling the function io_set_output_bit() for every force bit, this
| function just changes the outpus that require changing. This assumes that an
| initialising call is made to MOVA_OFF to set relays all off.
| 
| This function can be passed one of the following commands (see mova_op.h)
|   SYNC_PULSE_ON          - Switch on the sync pulse channel
|   SYNC_PULSE_OFF         - Switch off the sync pulse channel
|   CHANGE_OF_STAGE        - Switch off the nOldStage, and switch on the nNewStage
|   MOVA_JUST_ON           - Switch on the nNewStage and the HI & TO bits
|   MOVA_OFF               - Make sure all stage force and HI & TO are off
|   MOVA_FAULT_ON          - Switch on the 'MOVA fault' output to a separate OMU
|   MOVA_FAULT_OFF         - Switch off the 'MOVA fault' output
*******************************************************************************/
void OutputScan(int nMode)
{
    extern char control[NumberOfForce+NumberOfExtra];
    /* LED control */
    /* static char *const walamp  = (char*)(Xdinout+(NumberOf_din+DoutDetFault)); */
    int nI;
    /* static int Initialise=TRUE; - no longer used */
    static int nCurrentStage=0, nNewStage;
    /* calculated output line numbers for each function */
    int output_to, output_hi, output_s1, output_se, output_mf, output_sp;


#ifdef TEST_MOVA
    output_s1 = 0;   /* force use first 8 outputs */
    output_se = 7;
    output_hi = 8;   /* HI uses the next output */
    output_to = 9;   /* then Take Over */
    output_sp = 10;  /* and then Sync Pulse */
    output_mf = 255; /* MOVA fault not used */
#else
    /* decide what i/o cards have been fitted */
    {
        int card, found=FALSE;

        /* for each possible comms/mova card */
        for ( card=0; (card<MAX_EXPANSION_CARDS) && (!found); card++ )
        {
            /* if a comms/mova card is fitted in this position */
            if ( comms_board_fitted[card] )
            {
                /* then don't search for any more */
                found = TRUE;
            }
        }

        /* if a MOVA I/O card was found */
        /* or MOVA is using the ESP link (which also uses the 16 bit layout) */
        if ( ( found )
          || ( MOVA_IO_Setting == MOVA_IO_USE_ESP ) )
        {
            /* rather than changing all the following so they only specify
            |  numbers in the range 0 to 15 for MOVA_IF.Outputs below, just set
            |  the card number to 0. */
            card = 0;
            /* and assume that MOVA will use the 16 output on this card */
            output_s1 = card*16 + 0;  /* force bits start at the first output */
            /* if this card is in the first slot */
            if ( card == 0 )
            {
                /* then a MOVA fault output is also required */
                output_se = card*16 + 12; /* last force bit */
                output_mf = card*16 + 13; /* MOVA fault uses the third last output */
                output_sp = card*16 + 14; /* sync pulse uses the second last output */
                output_to = card*16 + 15; /* TO uses the last output */
                /* WARNING: IC_HARD_ERROR_HANDLER() in SYSCTL assumes MF  */
                /*          output uses output 13 on the first MOVA I/O   */
                /*          card. Ensure that any changes made to the     */
                /*          above code are also reflected in SYSCTL.C     */
            }
            /* no MOVA fault output required, how about the sync pulse? */
            else if ( enable_sync_pulse )
            {
                /* then a sync pulse is required */
                output_se = card*16 + 13; /* last force bit */
                output_sp = card*16 + 14; /* sync pulse uses the second last output */
                output_to = card*16 + 15; /* TO uses the last output */
            }
            else
            {
                /* no MOVA fault or sync pulse outputs required */
                output_se = card*16 + 14; /* last force bit */
                output_to = card*16 + 15; /* TO uses the last output */
            }
            output_hi = 255;          /* HI is not provided */
        }
        else /* no comms/mova i/o cards were found */
        {
            /* then assume three lmu i/o cards are fitted */
            output_to = 0;    /* TO uses the first output */
            output_s1 = 1;    /* force bits use 1 ... */
            output_se = 8;    /* ... to 8 */
            output_mf = 9;    /* MOVA fault uses 9 */
            output_sp = 10;   /* sync pulse uses 10 */
            output_hi = 255;  /* HI is not provided */
        }
    }
#endif

    /* regardless of whether a slot has been allocated to it or not */
    /* if the sync pulse is not required */
    /* (see definition of 'enable_sync_pulse' for more information) */
    if ( !enable_sync_pulse )
    {
        /* disable the sync pulse output */
        output_sp = 255;
    }

    /* set nNesStage to value of demanded stage */ 
    nNewStage = 0;
    for ( nI=0; nI<Tcomshr->stages; nI++ )
    {
        if ( control[nI] != 0 ) nNewStage = nI+1;  /* stages are 1-based */
    }

    /* compare with stage no. currently on */
    if ( nMode == CHANGE_OF_STAGE )
    {
        if ( nCurrentStage == 0 && nNewStage != 0 ) /* Error in call 'cause stage has not been running */ 
            nMode = MOVA_JUST_ON;
        else if ( nCurrentStage == nNewStage )
            nMode = NO_ACTION;
    }

#define SET_OUTPUT(bit) if ((bit)<16) MOVA_IF.Outputs |=  bit_masks[bit];
#define CLR_OUTPUT(bit) if ((bit)<16) MOVA_IF.Outputs &= ~bit_masks[bit];

    switch ( nMode )
    {
        case SYNC_PULSE_ON:
            SET_OUTPUT(output_sp);
        break;

        case SYNC_PULSE_OFF:
            CLR_OUTPUT(output_sp);
        break;

        case MOVA_OFF:
            /* if output are active high */
            if ( Tcomshr->stgdem == 1 )
            {
                for ( nI=output_s1; nI<=output_se; nI++ ) CLR_OUTPUT(nI);
                CLR_OUTPUT(output_hi);
                CLR_OUTPUT(output_to);
            }
            else /* active low outputs */
            {
                for ( nI=output_s1; nI<=output_se; nI++ ) SET_OUTPUT(nI);
                SET_OUTPUT(output_hi);
                SET_OUTPUT(output_to);
            }
            nCurrentStage = 0;
            Tdinout->dout[DoutMovaFault] = 0;  /* walamp[1] */
        break;

        case MOVA_JUST_ON:
            /* if output are active high */
            if ( Tcomshr->stgdem == 1 )
            {
                SET_OUTPUT(output_s1+nNewStage-1);
                SET_OUTPUT(output_hi);
                SET_OUTPUT(output_to);
            }
            else /* active low outputs */
            {
                CLR_OUTPUT(output_s1+nNewStage-1);
                CLR_OUTPUT(output_hi);
                CLR_OUTPUT(output_to);
            }
            nCurrentStage = nNewStage;
            Tdinout->dout[DoutMovaFault] = 1;  /* walamp[1] */
        break;

        case CHANGE_OF_STAGE:
            /* if output are active high */
            if ( Tcomshr->stgdem == 1 )
            {
                /* switch off the old force bit and switch on the new */
                CLR_OUTPUT(output_s1+nCurrentStage-1);
                SET_OUTPUT(output_s1+nNewStage-1);
            }
            else /* active low outputs */
            {
                SET_OUTPUT(output_s1+nCurrentStage-1);
                CLR_OUTPUT(output_s1+nNewStage-1);
            }
            nCurrentStage = nNewStage;
        break;

        case MOVA_FAULT_ON:
            SET_OUTPUT(output_mf);
        break;

        case MOVA_FAULT_OFF:
            CLR_OUTPUT(output_mf);
        break;

        case NO_ACTION :
        break;

        default :
        break;
    }
}
