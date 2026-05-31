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
//!  \file    108MovaFaults.h
//!	\brief   Header for fault handling for the MOVA module
//!	\author  Andrew Leigh
//!	\date    27-May-2018

//! \defgroup MovaFaults  Chameleon MOVA M8 Fault Processing
//!  callable fault functions for M8 cradle
//! \{
//! \} 


static char MovaFaults_h_rcsid[] = "$Id: ";

/* --------------------------------------------------------------------------- *
 *                     INCLUDE FILES                                           *
 * --------------------------------------------------------------------------- *
*/
#include "000Common.h"
#include "501DiagLib.h"
#include "504CmnLib.h"
#include "MVTypes.h"
#include "pcerror.h"
/*
 * --------------------------------------------------------------------------- *
 *                     SYMBOLIC CONSTANTS                                      *
 * --------------------------------------------------------------------------- *
*/
#ifndef __108MOVAFAULTS_H
#define __108MOVAFAULTS_H

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

/* --------------------------------------------------------------------------- *
 *                     FUNCTION REFERENCE                                      *
 * --------------------------------------------------------------------------- *
*/
//! \addtogroup MovaFaults
//! \{
void RaiseMovaErrorAsHistoricalFault(int MovaIDS, int MovaData);
void FaultMonitor(void);
void RaiseMovaStatusFault(const mova_status_t MovaStatus);
void ClearMovaStatusFault(const mova_status_t MovaStatus);
void MonitorMovaStatusFault(const int instance);

//! \} // end of MovaFaults

#endif /* __100MOVAFAULTS_H */
