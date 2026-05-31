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
//!  \file    108Ioscan.h
//!	\brief   Header for Peek specific I/O processing for Chameleon MOVA
//!	\author  Andrew Leigh
//!	\date    27-May-2018

//! \defgroup MovaIO Chameleon MOVA M8 IO 
//!  callable MovaIO functions for M8 cradle
//! \{
//! \} 

#ifndef __108IOSCAN_H
#define __108IOSCAN_H


/* --------------------------------------------------------------------------- *
 *                     INCLUDE FILES                                           *
 * --------------------------------------------------------------------------- *
*/
#include "peek.h"
#include "gbltypes.h"
#include "mova_op.h"

/*
 * --------------------------------------------------------------------------- *
 *                     SYMBOLIC CONSTANTS                                      *
 * --------------------------------------------------------------------------- *
*/
#define SYNC_PULSE_DISABLED  0
#define SYNC_PULSE_ENABLED   (!SYNC_PULSE_DISABLED)

/* Allow sync pulse duty cycle control:
 * By default the sync pulse is updated every 100ms by MOVA kernal, 100ms ON and
 * 400ms OFF.
 * MOVASIM and Siemens use 400ms ON, 600ms OFF.
 * Use the defines below to control the period and duty cycle.
 * If the default is required use SYNC_PULSE_ON_TIME=1 and SYNC_PULSE_PERIOD=5.
*/
#define SYNC_PULSE_ON_TIME   4
#define SYNC_PULSE_PERIOD    10

#define CLR_BIT_OP     (int) 0  // de-activate MOVA output bit
#define SET_BIT_OP     (int) 1  // active (force) MOVA output bit
#define INV_BIT_OP     (int) 1  // invert MOVA output bit


/* define MOVAIO_DEBUG_TRACE if tracing is required, else comment out */
/*#define MOVAIO_DEBUG_TRACE*/


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
extern char                     control[NumberOfForce+NumberOfExtra];
extern volatile unsigned char   enable_sync_pulse;
extern int                      GInstance;


/* --------------------------------------------------------------------------- *
 *                     FUNCTION PROTOTYPES                                     *
 * --------------------------------------------------------------------------- *
*/
//! \addtogroup MovaIO
//! \{
extern void PeekInputScan(void);
extern void PeekOutputScan(int);

//! \} // end of MovaIO

#endif /* __100IOSCAN_H */
