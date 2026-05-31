//
//  Project     Chameleon
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
//!  \file    108MovaFaults.c
//!	\brief   Fault handling for the MOVA module
//!	\author  Andrew Leigh
//!	\date    27-May-2018
//

static char MovaFaults_c_rcsid[] = "$Id: ";

/* --------------------------------------------------------------------------- *
 *                     INCLUDE FILES                                           *
 * --------------------------------------------------------------------------- *
*/
#include "Gbltypes.h"
#include "000Common.h"
#include "501DiagLib.h"
#include "504CmnLib.h"
#include "peek.h"
#include "108MovaFaults.h"
#include "108MovaLib.h"

/*
 * --------------------------------------------------------------------------- *
 *                     SYMBOLIC CONSTANTS                                      *
 * --------------------------------------------------------------------------- *
*/

#define MOVA_PHONE_HOME_FLAG 17

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

//!  \fn     FaultMonitor()
//!  \brief  Monitors the MOVA kernel and sends the appropriate faults to the Chameleon
//!		 fault log. Also triggers comms every time MOVA requests a call
//!
//!	\param	CmdCode int User command entered at terminal
//!  \retval void none 
//!
void FaultMonitor(void)
{
    static int currentPhoneHome = 0 ;
    BOOLEAN PhFault ;

    if ((Tcomshr->io[MOVA_PHONE_HOME_FLAG] >= 99) && (currentPhoneHome == 0))
    {
	if ( COMMS_ESTABLISHED != GIOshmData->isComms.state ) // If comms not already up request it.
	{
	    CmnSetConnectionRequest() ;
    	    DiagLog( DC_NOTE, "Connection Request MOVA");
	}
	(void)FaultLog("Raising MOVA phone home fault",
			gMovaEqIdNum,
                        FT_MOVA_IDS_ERROR,
                        FST_MOVA_IDS_CALL_HOME,0,0,0);

    	 currentPhoneHome = 1 ;
    }

    if (currentPhoneHome == 1)
    {
        PhFault = FaultExists("Checking MOVA phone home fault", gMovaEqIdNum, FT_MOVA_IDS_ERROR,
                              FST_MOVA_IDS_CALL_HOME, VALUE_NOT_SET, VALUE_NOT_SET, VALUE_NOT_SET) ;
	if (PhFault != FALSE) // Fault has not been cleared
	{
	    if (Tcomshr->io[MOVA_PHONE_HOME_FLAG] < 99)  // MOVA has cleared the fault
	    {
		(void)FaultAutoClear("Clearing MOVA phone home fault",
			gMovaEqIdNum,
                        FT_MOVA_IDS_ERROR,
                        FST_MOVA_IDS_CALL_HOME,0,0,0);

	    	currentPhoneHome = 0 ;
	    }
	}
	else
	{
	    currentPhoneHome = 0 ; // clear the phonehome request
            Tcomshr->io[MOVA_PHONE_HOME_FLAG] = 0 ;
	}
    }
}   // FaultMonitor()



//!  \fn     RaiseMovaErrorAsHistoricalFault()
//!  \brief  Raises a chameleon Fault. As MOVA faults are historical clear it again
//!
//!	\param	CmdCode int User command entered at terminal
//!  \retval void none 
//!
void RaiseMovaErrorAsHistoricalFault(int MovaIDS, int MovaData)
{
    BOOLEAN PhFault ;
    if (MovaIDS > FST_MOVA_IDS_NO_FAULT && MovaIDS < FST_MOVA_IDS_OUT_OF_RANGE)
    {
        FaultLog( "Raising MOVA Kernel IDS fault", gMovaEqIdNum,
                  FT_MOVA_IDS_ERROR, MovaIDS, MovaData,0,0);

        PhFault = FaultExists("Checking MOVA Kernel IDS fault", gMovaEqIdNum,
                              FT_MOVA_IDS_ERROR, MovaIDS, VALUE_NOT_SET, VALUE_NOT_SET, VALUE_NOT_SET) ;
        if (PhFault != FALSE) // Fault has not been cleared
        {
            FaultAutoClear("Clearing MOVA Kernel IDS fault", gMovaEqIdNum,
                           FT_MOVA_IDS_ERROR, MovaIDS, MovaData,0,0);
        }
    }
    else
    {
        FaultLog( "Raising MOVA Kernel IDS fault", gMovaEqIdNum,
                  FT_MOVA_IDS_ERROR, FST_MOVA_IDS_OUT_OF_RANGE, 0,0,0);

        PhFault = FaultExists("Checking MOVA Kernel IDS fault", gMovaEqIdNum,
                              FT_MOVA_IDS_ERROR, FST_MOVA_IDS_OUT_OF_RANGE, VALUE_NOT_SET, VALUE_NOT_SET, VALUE_NOT_SET) ;
        if (PhFault != FALSE) // Fault has not been cleared
        {
            FaultAutoClear("Clearing MOVA Kernel IDS fault", gMovaEqIdNum,
                           FT_MOVA_IDS_ERROR, FST_MOVA_IDS_OUT_OF_RANGE, 0,0,0);
        }
    }
}   // RaiseMovaErrorAsHistoricalFault()



//!  \fn     MonitorMovaStatusFault()
//!  \brief  Monitors the MOVA kernel and sends the appropriate faults to the Chameleon
//!		fault log. Also triggers comms every time MOVA requests a call.  Also raises
//!		a warmup fault if too long in that state
//!
//!	\param	instance const int MOVA stream
//!  \retval void none 
//!
void MonitorMovaStatusFault(const int instance)
{
    static int currentPhoneHome = 0;
    BOOLEAN PhFault ;
    mova_io_t           *movaIo;
    equip_id_text_t     movaEqId;
    equip_id_t          movaEqIdNum;

    CMN_TRACE_ENTRY;

    movaEqIdNum = MovaLibInstanceToStream(instance, movaEqId, 0, 0, 0);
    if (movaEqIdNum >= 0)
    {
        movaIo = &GIOshmData->mova_io[instance];

        // check if this instance has been in warmup state too long
        if (movaIo->warmupWait > 0)
        {
            if (movaIo->currStatus != MOVAST_WARMUP)
            {   //reset count
                movaIo->warmupWait = 0;
            }
            else if (movaIo->warmupWait < MAX_MOVA_WARMUP_SECONDS)
            {
                movaIo->warmupWait++;
            }
            else if (movaIo->warmupWait == MAX_MOVA_WARMUP_SECONDS)
            {   // too long raise a fault
                RaiseMovaStatusFault(movaIo->currStatus);
                // increment count to prevent the fault being logged twice
                movaIo->warmupWait++;

                // toggle CRB just-in-case MOVA is not warming up
                movaIo->toggleCRB = 100;  // samples
            }
        }
        // decide whether we need to tell the instation about a fault
        if (currentPhoneHome == 0 && movaIo->callHome != FALSE)
        {
            if ( COMMS_ESTABLISHED != GIOshmData->isComms.state ) // If comms not already up request it.
            {
                CmnSetConnectionRequest() ;
                DiagLog( DC_NOTE, "Connection Request from MOVA Cradle:%s", movaEqId);
            }
            (void)FaultLog("Raising MOVA Cradle call home fault",
                            movaEqIdNum,
                            FT_MOVA_CRADLE_STATUS,
                            FST_MOVA_STATUS_CALL_HOME,0,0,0);

            currentPhoneHome = 1 ;
        }

        if (currentPhoneHome == 1)
        {
            PhFault = FaultExists("Checking MOVA Cradle call home fault",
                                  movaEqIdNum,
                                  FT_MOVA_CRADLE_STATUS,
                                  FST_MOVA_STATUS_CALL_HOME, VALUE_NOT_SET, VALUE_NOT_SET, VALUE_NOT_SET) ;
            if (PhFault != FALSE) // Fault has not been cleared
            {
                if (movaIo->callHome == FALSE)  // MOVA has cleared the fault
                {
                    (void)FaultAutoClear("Clearing MOVA phone home fault",
                                            movaEqIdNum,
                                            FT_MOVA_CRADLE_STATUS,
                                            FST_MOVA_STATUS_CALL_HOME,0,0,0);

                    currentPhoneHome = 0 ;
                }
            }
            else
            {
                currentPhoneHome = 0 ; // clear the phonehome request
                movaIo->callHome = FALSE;
            }
        }
    }

    CMN_TRACE_EXIT;

}   // MonitorMovaStatusFault()



//!  \fn     RaiseMovaStatusFault()
//!  \brief  Raises a chameleon Fault against a particular instance based upon MOVAST_
//!		states.  These are recorded by the Mova Cradle as they happen, so they can be
//!		current and cleared.  This differs from MOVA IDS faults which are recorded
//!		when MOVA records an error.
//!
//!	\param	MovaStatus const mova_status_t Status of fault to raise/clear  
//!  \retval void none 
//!
void RaiseMovaStatusFault(const mova_status_t MovaStatus)
{
    BOOLEAN PhFault ;
    if (MovaStatus > MOVAST_NOTSTARTED && MovaStatus < MOVAST_MAX)
    {
        FaultLog( "Raising MOVA Status fault", gMovaEqIdNum,
                  FT_MOVA_CRADLE_STATUS, MovaStatus, 0,0,0);
    }
    else
    {
        FaultLog( "Raising MOVA Satus fault", gMovaEqIdNum,
                  FT_MOVA_CRADLE_STATUS, MOVAST_MAX, 0,0,0);

        PhFault = FaultExists("Checking MOVA Status fault", gMovaEqIdNum,
                              FT_MOVA_CRADLE_STATUS, MOVAST_MAX, VALUE_NOT_SET, VALUE_NOT_SET, VALUE_NOT_SET) ;
        if (PhFault != FALSE) // Fault has not been cleared
        {
            FaultAutoClear("Clearing MOVA Status fault", gMovaEqIdNum,
                           FT_MOVA_CRADLE_STATUS, MOVAST_MAX, 0,0,0);
        }
    }
}   // RaiseMovaStatusFault()



//!  \fn     ClearMovaStatusFault()
//!  \brief  Clears a chameleon Fault against a particular instance based upon MOVAST_
//!		states.  These are recorded by the Mova Cradle as they happen, so they can be
//!		current and cleared.  This differs from MOVA IDS faults which are recorded when
//!		MOVA records an error.
//!
//!	\param	MovaStatus const mova_status_t Status of fault to raise/clear  
//!  \retval void none 
//!
void ClearMovaStatusFault(const mova_status_t MovaStatus)
{
    BOOLEAN PhFault ;
    if (MovaStatus > MOVAST_NOTSTARTED && MovaStatus < MOVAST_MAX)
    {
        PhFault = FaultExists("Checking MOVA Status fault", gMovaEqIdNum,
                              FT_MOVA_CRADLE_STATUS, MovaStatus, VALUE_NOT_SET, VALUE_NOT_SET, VALUE_NOT_SET) ;
        if (PhFault != FALSE) // Fault has not been cleared
        {
            FaultAutoClear("Clearing MOVA Status fault", gMovaEqIdNum,
                           FT_MOVA_CRADLE_STATUS, MovaStatus, 0,0,0);
        }
    }
    else
    {
        FaultLog( "Raising MOVA Kernel IDS fault", gMovaEqIdNum,
                  FT_MOVA_CRADLE_STATUS, MOVAST_MAX, 0,0,0);

        PhFault = FaultExists("Checking MOVA Kernel IDS fault", gMovaEqIdNum,
                              FT_MOVA_CRADLE_STATUS, MOVAST_MAX, VALUE_NOT_SET, VALUE_NOT_SET, VALUE_NOT_SET) ;
        if (PhFault != FALSE) // Fault has not been cleared
        {
            FaultAutoClear("Clearing MOVA Kernel IDS fault", gMovaEqIdNum,
                           FT_MOVA_CRADLE_STATUS, MOVAST_MAX, 0,0,0);
        }
    }
}   // ClearMovaStatusFault()
