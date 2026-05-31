//
//  Project    Chameleon
//
//  Copyright 	Dynniq UK Ltd 2018
//
//  All rights reserved
//
//  The information contained herein is the property of Peek Traffic Limited
//  and is supplied without any liability for errors or omissions. No part
//  may be reproduced or copied except as authorised by contract or other
//  written permission. The copyright and foregoing restriction on
//  reproduction and use extend to all media in which the information may be
//  embodied.
//
//!  \file		108Ioscan.c
//!
//!	 \brief		Peek specific I/O processing for Chameleon MOVA
//!
//!	 \author  Andrew Leigh
//!
//!	 \date    27-May-2018
//!


/* --------------------------------------------------------------------------- *
 *                     INCLUDE FILES                                           *
 * --------------------------------------------------------------------------- *
*/
#include "108Ioscan.h"
#include "108MovaLib.h"
#include "108MovaCradle.h"

/*
 * --------------------------------------------------------------------------- *
 *                     SYMBOLIC CONSTANTS                                      *
 * --------------------------------------------------------------------------- *
*/

/* --------------------------------------------------------------------------- *
 *                     MACRO DEFINITIONS                                       *
 * --------------------------------------------------------------------------- *
*/

/*
 * --------------------------------------------------------------------------- *
 *                     TYPE DEFINITIONS                                        *
 * --------------------------------------------------------------------------- *
 */


/* --------------------------------------------------------------------------- *
 *                     GLOBAL DATA REFERENCES                                  *
 * --------------------------------------------------------------------------- *
*/
/*
| The following flag is used to enable the sync pulse output used to synchronise
| the simulator running on the PC with the MOVA unit. To enable the facility,
| either define 'SYNCPULSE' at compilation time, or change the value of the flag
| using the debug kit. By default, the output is disabled as it is not required
| on production MOVA units (only those used for testing) and switching the relay
| twice every second will wear-out the relay after a few years!
*/
volatile unsigned char enable_sync_pulse = SYNC_PULSE_ENABLED;  // DO NOT REMOVE: see below.
// sync pulse processing used to guard against doubtful MOVA kernel force outputs.


/* --------------------------------------------------------------------------- *
 *                     LOCAL FUNCTION REFERENCES                               *
 * --------------------------------------------------------------------------- *
 */
inline static void write_output( int, BOOLEAN,int, int );
inline static void switch_forces_off( void );
inline static void switch_mova_off( void );


/* --------------------------------------------------------------------------- *
 *                     LOCAL DATA REFERENCES                                   *
 * --------------------------------------------------------------------------- *
 */
static int   nCurrentStage = 0;


//!  \fn     PeekInputScan()
//!
//!  \brief  Peek specific version of InputScan.
//!		Uses the io pin assignments specified by the config file and stored in the
//! 		shared IO memory area.
//! 		Uses common library functions to input MOVA data.
//!  \retval  void none
//
void PeekInputScan(void)
{
    int         i, confnum;
    bit_op_t    bit_val;
    bit_op_t    MO_bit;

#ifdef MOVA_CHECK_TASK_TIME
    struct tm *TimeNow;
    static struct timeval tvStart;
    static struct timeval tvEnd;
    static int timeDiffmS = 0;
    static int countDiagMessages = 0;

    // determine how log this routine is active
    gettimeofday(&tvStart,NULL);
#endif /* MOVA_CHECK_TASK_TIME */

    diag_printf( "PeekInputScan: mxdets=%d, mxconf=%d, stages=%d, detton=%d, stagon=%d, phason=%d, instance=%d\n",
                 mxdets ,mxconf, Tcomshr->stages, Tcomshr->detton, Tcomshr->stagon, Tcomshr->phason, GInstance );

    // NOTE:
    // The dataset configuration values DETON, STAGON and PHASON specify the
    // processing required for each input. The default values are DETON=1,
    // STAGON=0 and PHASON=0.
    // All inputs to the Chameleon have an implicit inversion so the default
    // cases are inverted to get the correct result.
    // Controller ready bit is not configurable in MOVA so we invert anyway.
    // In addition all MOVA IO pins can be configured to be inverted in the
    // Chameleon configuration file and hence individual bit inversions
    // are possible.

    // Read all detectors associated with MOVA stream...
    for (i = 0; i < MAX_MOVA_IO_DETREPBITS; i++)
    {
        if ( GIOshmData->mova_io[GInstance].movaDETREPbits[i].io_pin != VALUE_NOT_SET )
        {
            bit_val = CmnInBitOp( &GIOshmData->eqInterface.rawRead,
                                   GIOshmData->mova_io[GInstance].movaDETREPbits[i].io_pin,
                                   BOP_GET );
            if ( (bit_val != BOP_ERR) && (i < mxdets) )
            {
                // account for Chameleon implicit invertion
                bit_val ^= 1;

                // Each MOVA IO pin can be configured to be inverted
                if ( GIOshmData->mova_io[GInstance].movaDETREPbits[i].invertFlag != FALSE )
                {
                    bit_val ^= 1;
                }
                // pass detector to MOVA kernel
                detsin[i] = (char) bit_val;
            }
            else
            {
                diag_printf( "PeekInputScan: CmnInBitOp() FAILED, bit_val=%d, io_pin=%d, i=%d",
                              bit_val, GIOshmData->mova_io[GInstance].movaDETREPbits[i].io_pin+1, i );
            }
        }
    }
#ifdef MOVAIO_DEBUG_TRACE
    diag_printf( "PeekInputScan: detsin=%d, %d, %d, %d, %d, %d, %d, %d\n",
                 detsin[0], detsin[1], detsin[2], detsin[3], detsin[4], detsin[5], detsin[6], detsin[7] );
#endif

    // read all stage confirms associated with MOVA stream
    confnum = 0;
    for (i = 0; i < Tcomshr->stages; i++)
    {
        if ( GIOshmData->mova_io[GInstance].movaSREPbits[i].io_pin != VALUE_NOT_SET )
        {
            bit_val = CmnInBitOp( &GIOshmData->eqInterface.rawRead,
                                   GIOshmData->mova_io[GInstance].movaSREPbits[i].io_pin,
                                   BOP_GET );
            if ( (bit_val != BOP_ERR) && (confnum < mxconf) )
            {
                // account for Chameleon implicit invertion
                bit_val ^= 1;

                // Each MOVA IO pin can be configured to be inverted
                if ( GIOshmData->mova_io[GInstance].movaSREPbits[i].invertFlag != FALSE )
                {
                    bit_val ^= 1;
                }
                // pass stage confirm to MOVA kernel
                confin[confnum] = (char) bit_val;

                confnum++;
            }
            else
            {
                diag_printf( "PeekInputScan: CmnInBitOp() FAILED, bit_val=%d, io_pin=%d, confnum=%d",
                              bit_val, GIOshmData->mova_io[GInstance].movaSREPbits[i].io_pin+1, confnum );
            }
        }
        else
        {
#ifdef MOVAIO_DEBUG_TRACE
            diag_printf( "PeekInputScan: MOVA IO pin not configured: io_pin=%d",
                         GIOshmData->mova_io[GInstance].movaSREPbits[i].io_pin+1 );
#endif
        }
    }

    // read all phase confirms associated with MOVA stream
    for (i = 0; i < MAX_MOVA_IO_PREPBITS; i++)
    {
        if ( GIOshmData->mova_io[GInstance].movaPREPbits[i].io_pin != VALUE_NOT_SET )
        {
            bit_val = CmnInBitOp( &GIOshmData->eqInterface.rawRead,
                                   GIOshmData->mova_io[GInstance].movaPREPbits[i].io_pin,
                                   BOP_GET );
            if ( (bit_val != BOP_ERR) && (confnum < mxconf) )
            {
                // account for Chameleon implicit invertion
                bit_val ^= 1;

                // Each MOVA IO pin can be configured to be inverted
                if ( GIOshmData->mova_io[GInstance].movaPREPbits[i].invertFlag != FALSE )
                {
                    bit_val ^= 1;
                }
                // pass phase confirm to MOVA kernel
                confin[confnum] = (char) bit_val;

                confnum++;
            }
            else
            {
                diag_printf( "PeekInputScan: CmnInBitOp() FAILED, bit_val=%d, io_pin=%d, confnum=%d",
                              bit_val, GIOshmData->mova_io[GInstance].movaPREPbits[i].io_pin+1, confnum );
            }
        }
    }

    // read CRB bit associated with MOVA stream
    if ( GIOshmData->mova_io[GInstance].movaCRBREPbit.io_pin != VALUE_NOT_SET )
    {
        bit_val = CmnInBitOp( &GIOshmData->eqInterface.rawRead,
                               GIOshmData->mova_io[GInstance].movaCRBREPbit.io_pin,
                               BOP_GET );
        if ( bit_val != BOP_ERR )
        {
            // Contacts closed for controller ready (AG45 section 9.3.9)
            // account for Chameleon implicit invertion
            bit_val ^= 1;

            // Each MOVA IO pin can be configured to be inverted
            if ( GIOshmData->mova_io[GInstance].movaCRBREPbit.invertFlag != FALSE )
            {
                bit_val ^= 1;
            }

	    // if CRB is on turn it off if warmup failed
            if (GIOshmData->mova_io[GInstance].toggleCRB > 0)
            {
                bit_val = 0;
                GIOshmData->mova_io[GInstance].toggleCRB--;
            }

	    // Test for MOVA MO bit in control
	    if (GIOshmData->mova_io[GInstance].movaMObit.procBit != VALUE_NOT_SET)
	    {
		if (GIOshmData->mova_io[GInstance].ETermOverride != FALSE)
		{
		    MO_bit = BOP_SET ;
		}
		else
		{
		    MO_bit = CmnOutBitOp(&GIOshmData->eqInterface.writeProc,
				GIOshmData->mova_io[GInstance].movaMObit.procBit,BOP_GET) ;
		}
	    	if (bit_val == BOP_SET)
	    	{
		    if (MO_bit == BOP_CLR)
		    {
		    	bit_val = BOP_CLR ;
		    }
	    	}
                diag_printf( "PeekInputScan: MRSet, bit_val=%d, proc_pin=%d",
                              MO_bit,GIOshmData->mova_io[GInstance].movaMRbit.procBit) ;
		if (GIOshmData->mova_io[GInstance].movaMRbit.procBit != VALUE_NOT_SET)
		{
		     CmnInBitOp(&GIOshmData->eqInterface.procRead,
		     		GIOshmData->mova_io[GInstance].movaMRbit.procBit,MO_bit) ;
		}
	    }

            // pass Controller Ready Bit to MOVA kernel
            confin[mxconf-1] = (char) bit_val;
            // record value of CRB for use with MOVA Cradle status
            GIOshmData->mova_io[GInstance].statusMovaCtrlReady = (char) bit_val;
        }
        else
        {
            diag_printf( "PeekInputScan: CmnInBitOp() FAILED, bit_val=%d, io_pin=%d",
                         bit_val, GIOshmData->mova_io[GInstance].movaCRBREPbit.io_pin+1 );
        }
    }
#ifdef MOVAIO_DEBUG_TRACE
    diag_printf( "PeekInputScan: confin=%d, %d, %d, %d, %d, %d, %d, %d, CRB=%d\n",
                 confin[0], confin[1], confin[2], confin[3], confin[4], confin[5], confin[6], confin[7], confin[31] );
#endif

    /* latch changes in detector states for the 'Look' screen */
    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    /* For each possible detector input */
    for ( i = 0; i < mxdets; i++ )
    {
        /* clear the '0' latch if the detector sample is a '0' this time */
        detsin_0s[i] &= detsin[i];
        /* set the '1' latch if the detector sample is a '1' this time */
        detsin_1s[i] |= detsin[i];
    }

#ifdef MOVA_CHECK_TASK_TIME
    // determine how log this routine is active
    gettimeofday(&tvEnd,NULL);
    timeDiffmS = ((tvEnd.tv_sec - tvStart.tv_sec)*1000000 + tvEnd.tv_usec - tvStart.tv_usec)/1000;
    if (timeDiffmS > MOVA_INPUT_TOO_LONG)
    {
        if (countDiagMessages < 100)
        {
            DiagLog( DC_WARNING, "%s: Stream:%s, too long in function: %d", __FUNCTION__, gMovaEqId, timeDiffmS);
            countDiagMessages++;
        }
    }
#endif /* MOVA_CHECK_TASK_TIME */
} // Peek1InputScan()



//!  \fn     write_output()
//!
//!  \brief  Outputs the required MOVA bit.
//! Sets or clears the output bit depending on the following criteria:
//! If STGDEM in the dataset is 1 the output is defined as 'close' to activate,
//! i.e. set output low, otherwise it's set high.
//! If MOVA pin is configured as inverted then invert the ouput.
//! If a UTC control bit is configured on the same io pin and is configured as
//! inverted then invert the MOVA output as the rawWrite digital IO module
//! will automatically invert the bis bit.
//! Function assumes sync pulse, TO, HI and Fault bits use the same sense as
//! Tcomshr->stgdem.
//! Uses common library functions to output MOVA data.
//!
//!	\param	io_pin	bit number of MOVA io pin  
//!	\param	invertFlag	whethet config file specifies invertion of MOVA io pin
//!	\param	bit_op	1 = set output (activate), 0 = clear output (de-activate).  
//!  \retval void none 
//!
inline static void write_output( int io_pin,
                          BOOLEAN   invertFlag,
                          const int procPin,
                          int       bit_op)
{
    int         bit_op_write;
    bit_op_t    bit_val, bit_op_val;

    bit_op_write = bit_op;

    if ((io_pin != VALUE_NOT_SET ) || (procPin != VALUE_NOT_SET))
    {
        // Each MOVA IO pin can be configured to be inverted
        if ( invertFlag != FALSE )
        {
            bit_op_write ^= INV_BIT_OP;
        }
        // MOVA io pin allocation may overlay a configured UTC control bit.
        // In this case any invertion specified by the UTC pin must be removed.
        if ( CmnOutBitOp(&GIOshmData->eqInterface.invertOutputs, io_pin, BOP_GET) == BOP_SET )
        {
            bit_op_write ^= INV_BIT_OP;
        }

        // Finally output the bit
//#ifdef MOVAIO_DEBUG_TRACE
        diag_printf( "write_output: io_pin=%d, invertFlag=%d, bit_op=%d, bit_op_write=%d\n",
                      io_pin+1, invertFlag, bit_op, bit_op_write );
//#endif
        if ( bit_op_write == SET_BIT_OP )
        {
	    bit_op_val = BOP_SET ;
	}
	else
	{
	    bit_op_val = BOP_CLR ;
	}
            // set output
	if ( io_pin != VALUE_NOT_SET )
	    bit_val = CmnOutBitOp( &GIOshmData->eqInterface.writeMOVA, io_pin, bit_op_val);

            diag_printf( "write_output: writeMOVA, io_pin=%d, val=%d %04X", io_pin+1, bit_op_val,GIOshmData->eqInterface.writeMOVA[0] );
	if ( procPin != VALUE_NOT_SET ) {
            diag_printf( "write_output: writeproc, proc_pin=%d, val=%d", procPin+1, bit_op_val );
	    CmnInBitOp( &GIOshmData->eqInterface.procRead, procPin, bit_op_val);
	}

        if ( bit_val == BOP_ERR )
        {
            diag_printf( "write_output: CmnOutBitOp() FAILED, io_pin=%d, requested write=%d", io_pin+1, bit_op_write );
        }
    }
}



//!  \fn     switch_forces_off()
//!  \brief  Deactivate all MOVA force bits
//!
//!  \retval  void none
//!
inline static void switch_forces_off( void )
{
    int i;

    for ( i=0; i < Tcomshr->stages; i++ )
    {
        write_output( GIOshmData->mova_io[GInstance].movaCONbits[i].io_pin,
                      GIOshmData->mova_io[GInstance].movaCONbits[i].invertFlag,VALUE_NOT_SET,
                      CLR_BIT_OP );
    }
}



//!  \fn     switch_mova_off()
//!  \brief  Deactivate all MOVA force bits and the Take Over and Hurry Inhibit bits
//!
//!  \retval  void none
//!
inline static void switch_mova_off( void )
{
    switch_forces_off();
    write_output( GIOshmData->mova_io[GInstance].movaHIbit.io_pin,
                  GIOshmData->mova_io[GInstance].movaHIbit.invertFlag,VALUE_NOT_SET,
                  CLR_BIT_OP );
    write_output( GIOshmData->mova_io[GInstance].movaTObit.io_pin,
                  GIOshmData->mova_io[GInstance].movaTObit.invertFlag,
		  GIOshmData->mova_io[GInstance].movaTObit.procBit,
                  CLR_BIT_OP );

    // record value of TakeOver bit
    GIOshmData->mova_io[GInstance].statusMovaTakeOver = 0;

    nCurrentStage = 0;
    Tdinout->dout[DoutMovaFault] = 0;  /* walamp[1] */
}



//!  \fn     PeekOutputScan()
//!
//!  \brief   Peek specific version of InputScan.
//! Uses the io pin assignments specified by the config file and stored in the
//! shared IO memory area.
//! Uses common linbrary functions to input MOVA data.
//!
//!	\param	Mode	One of the MOVA modes used by OutputScan that
//!    	determines what action is required.
//!		Set MOVA stage forces, sync puls, fault, hurry inhibit and take over outputs.
//!  \retval  
//!
void PeekOutputScan( int Mode )
{
    // Perform Peek configurable IO processing
    int          nNewStage;
    static       int  sp_count = 0;   // sync pulse counter
    int          i;
    bit_op_t     bit_op;
    static       unsigned int status_count;
    mova_io_t   *movaIo;


#ifdef MOVA_CHECK_TASK_TIME
    struct tm *TimeNow;
    static struct timeval tvStart;
    static struct timeval tvEnd;
    static int timeDiffmS = 0;
    static int countDiagMessages = 0;

    // determine how log this routine is active
    gettimeofday(&tvStart,NULL);
#endif /* MOVA_CHECK_TASK_TIME */

    LockIoScan();
    diag_printf( "PeekOutputScan: Mode=%d, instance=%d, stages=%d, stgdem=%d, nCurrentStage=%d\n",
                 Mode, GInstance, Tcomshr->stages, Tcomshr->stgdem, nCurrentStage );

    // record status values every 10 cycles
    status_count++;
    if (status_count > 10)
    {
        status_count = 0;
        movaIo = &GIOshmData->mova_io[GInstance];

        movaIo->statusMovaOn     = Tcomshr->io[27];
        movaIo->statusMovaMulti  = Tcomshr->io[30];
        movaIo->statusMovaWarmup = Tcomshr->warmup;
        movaIo->statusMovaStages = Tcomshr->stages;

	if ((movaIo->warmupWait >= MAX_MOVA_WARMUP_SECONDS) ||
		(movaIo->statusMovaOn == 0) ||
		(movaIo->statusMovaMulti == 1))
	{
            diag_printf( "PeekOutputScan: MOVA_FAULT_ON\n" );
            write_output( GIOshmData->mova_io[GInstance].movaFLTbit.io_pin,
                          GIOshmData->mova_io[GInstance].movaFLTbit.invertFlag,
                          GIOshmData->mova_io[GInstance].movaFLTbit.procBit,
                          SET_BIT_OP );
	}
	else
	{
            diag_printf( "PeekOutputScan: MOVA_FAULT_OFF\n" );
            write_output( GIOshmData->mova_io[GInstance].movaFLTbit.io_pin,
                          GIOshmData->mova_io[GInstance].movaFLTbit.invertFlag,
                          GIOshmData->mova_io[GInstance].movaFLTbit.procBit,
                          CLR_BIT_OP );
	}
        // record for state machine
        if (movaIo->statusMovaOn == 0)
        {
            MovaCradleSetStatus(MOVAST_OFF);
        }
        else if (movaIo->statusMovaMulti == 1)
        {
            MovaCradleSetStatus(MOVAST_MULTI);
        }
        else if (movaIo->statusMovaCtrlReady == 0)
        {
            MovaCradleSetStatus(MOVAST_NO_CRB);
        }
        else if (movaIo->statusMovaTakeOver == 0)
        {
            MovaCradleSetStatus(MOVAST_WARMUP);
        }

        Safe_Strncpy(movaIo->statusMovaDataset, (nonVol->stcDescription)[5].sFname_, xgnFILE_NAME_STR_LEN);
    }

    nNewStage = 0;
    for ( i=0; i < Tcomshr->stages; i++ )
    {
        if ( Tcomshr->stgdem == 1 )
        {
            // normal case, control not inverted.
            if ( control[i] != 0 )
            {
                nNewStage = i+1;  /* stages are 1-based */
            }
        }
        else
        {
            // control bits will be inverted.
            if ( control[i] == 0 )
            {
                nNewStage = i+1;  /* stages are 1-based */
            }
        }
    }
#ifdef MOVAIO_DEBUG_TRACE
    diag_printf( "PeekOutputScan: control=%d, %d, %d, %d, %d, %d, %d, %d, nNewStage=%d\n",
                 control[0], control[1], control[2], control[3], control[4], control[5], control[6], control[7], nNewStage  );
#endif

    // make sure that stage value is sensible before starting to force
    if ( Mode == MOVA_JUST_ON )
    {
        if ( (nNewStage <= 0) || (nNewStage > Tcomshr->stages) )
        {
            Mode = MOVA_OFF;
        }
    }

    // make sure that stage values are sensible before changing force
    if ( Mode == CHANGE_OF_STAGE )
    {
        if ( (nCurrentStage < 0) ||
              (nCurrentStage > Tcomshr->stages) ||
              (nNewStage <= 0) ||
              (nNewStage > Tcomshr->stages) )
        {
            Mode = MOVA_OFF;
        }
        else
        {
            if ( (nCurrentStage == 0) && (nNewStage != 0) )
            {
                /* Error in call 'cause stage has not been running */
                Mode = MOVA_JUST_ON;
            }
            else if ( nCurrentStage == nNewStage )
            {
                Mode = NO_ACTION;
            }
        }
    }

    switch ( Mode )
    {
        case SYNC_PULSE_ON:
        case SYNC_PULSE_OFF:
            if ( enable_sync_pulse == SYNC_PULSE_ENABLED )
            {
                if ( sp_count < SYNC_PULSE_ON_TIME )
                {
                    write_output( GIOshmData->mova_io[GInstance].movaSPbit.io_pin,
                                  GIOshmData->mova_io[GInstance].movaSPbit.invertFlag,VALUE_NOT_SET,
                                  SET_BIT_OP );
                }
                else
                {
                    write_output( GIOshmData->mova_io[GInstance].movaSPbit.io_pin,
                                  GIOshmData->mova_io[GInstance].movaSPbit.invertFlag,VALUE_NOT_SET,
                                  CLR_BIT_OP );
                }

                sp_count++;
                if ( sp_count >= SYNC_PULSE_PERIOD )
                {
                    sp_count = 0;
                }
            }

            /* safety check in case MOVA kernel has cleared control[] array */
            if ( 0 == nNewStage )
            {
                switch_mova_off();
            }
            break;


        case MOVA_OFF:
            diag_printf( "PeekOutputScan: MOVA_OFF\n" );
            switch_mova_off();
            break;


        case MOVA_JUST_ON:
            diag_printf( "PeekOutputScan: MOVA_JUST_ON\n" );
            switch_forces_off();  /* clear any residual forces first */
            write_output( GIOshmData->mova_io[GInstance].movaCONbits[nNewStage-1].io_pin,
                          GIOshmData->mova_io[GInstance].movaCONbits[nNewStage-1].invertFlag,VALUE_NOT_SET,
                          SET_BIT_OP );
            write_output( GIOshmData->mova_io[GInstance].movaHIbit.io_pin,
                          GIOshmData->mova_io[GInstance].movaHIbit.invertFlag,VALUE_NOT_SET,
                          SET_BIT_OP );
            write_output( GIOshmData->mova_io[GInstance].movaTObit.io_pin,
                          GIOshmData->mova_io[GInstance].movaTObit.invertFlag,
                          GIOshmData->mova_io[GInstance].movaTObit.procBit,
                          SET_BIT_OP );

            // record value of TakeOver bit
            GIOshmData->mova_io[GInstance].statusMovaTakeOver = 1;  // MOVA modules redefine TRUE of 255
            MovaCradleSetStatus(MOVAST_CONTROL);

            nCurrentStage = nNewStage;
            Tdinout->dout[DoutMovaFault] = 1;  /* walamp[1] */
            break;


        case CHANGE_OF_STAGE:
            diag_printf( "PeekOutputScan: CHANGE_OF_STAGE\n" );
            /* switch off the old force bit and switch on the new */
            switch_forces_off();  /* clear previouds and any residual forces first */
            write_output( GIOshmData->mova_io[GInstance].movaCONbits[nNewStage-1].io_pin,
                          GIOshmData->mova_io[GInstance].movaCONbits[nNewStage-1].invertFlag,VALUE_NOT_SET,
                          SET_BIT_OP );

            nCurrentStage = nNewStage;
            break;

/*  MOVA_FAULT_ON is never used
        case MOVA_FAULT_ON:
            diag_printf( "PeekOutputScan: MOVA_FAULT_ON\n" );
            write_output( GIOshmData->mova_io[GInstance].movaFLTbit.io_pin,
                          GIOshmData->mova_io[GInstance].movaFLTbit.invertFlag,
                          GIOshmData->mova_io[GInstance].movaFLTbit.procBit,
                          SET_BIT_OP );
            break;


        case MOVA_FAULT_OFF:
            diag_printf( "PeekOutputScan: MOVA_FAULT_OFF\n" );
            write_output( GIOshmData->mova_io[GInstance].movaFLTbit.io_pin,
                          GIOshmData->mova_io[GInstance].movaFLTbit.invertFlag,
                          GIOshmData->mova_io[GInstance].movaFLTbit.procBit,
                          CLR_BIT_OP );
            break;
*/

        case NO_ACTION :
            diag_printf( "PeekOutputScan: NO_ACTION\n" );
            // diag_printf( "PeekOutputScan: NO_ACTION: \n" );
            break;


        default :
            diag_printf( "PeekOutputScan: default: \n" );
            DiagLog( DC_WARNING, "PeekOutputScan: Incorrect Mode=%d, switching off MOVA outputs!", Mode );
            switch_mova_off();
            break;
    }

    UnlockIoScan();

#ifdef MOVA_CHECK_TASK_TIME
    // determine how log this routine is active
    gettimeofday(&tvEnd,NULL);
    timeDiffmS = ((tvEnd.tv_sec - tvStart.tv_sec)*1000000 + tvEnd.tv_usec - tvStart.tv_usec)/1000;
    if (timeDiffmS > MOVA_OUTPUT_TOO_LONG)
    {
        if (countDiagMessages < 100)
        {
            DiagLog( DC_WARNING, "%s: Stream:%s, too long in function: %d", __FUNCTION__, gMovaEqId, timeDiffmS);
            countDiagMessages++;
        }
    }
#endif /* MOVA_CHECK_TASK_TIME */

}
