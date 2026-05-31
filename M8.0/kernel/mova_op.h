/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         mova.h
*
*  TITLE:        MOVA Kernel: TRL MOVA and Siemens MS OMU interface
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	26-Jan-1988		4.0.0		..		MC		......			First undergoing system test
*	05-May-1998		4.0.0		..		MC		......			Added MOVA_FAULT_... defines.
*	31-May-2011		7.0.0		AA		PK		CRN03			Updated header inline with new Software Quality Plan	
*	22-Feb-2012		7.0.3		AB		AK		CRN008			Additional definitions for Siemens 
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

#define SYNC_PULSE_ON   1
#define SYNC_PULSE_OFF  2
#define CHANGE_OF_STAGE 3
#define MOVA_JUST_ON    4
#define MOVA_OFF        5
#define MOVA_FAULT_ON   6 /* added these two (Siemens fault report MOVA-6) */
#define MOVA_FAULT_OFF  7
#define NO_ACTION       8

#define MOVA_IN_INTERGREEN 9 /*Add in M8*/

extern void OutputScan(int);


