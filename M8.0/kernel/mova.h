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
*	29-Sep-2004		5.0.0		..		MC		......			CMOVA_EP Define added for enabling/disabling emergency/priority code in MOVA.
*	31-May-2011		7.0.0		AA		PK		CRN003			Updated header inline with new Software Quality Plan	
*	15-Mar-2012		7.0.3		AB		AK		CRN009			Changes to conditional compilation flags
*	25-Jan-2013		7.0.5		..		AK		CRN023			Fix compiler warnings
*	31-Jan-2013		7.0.5		AC		AK		M7.0.5			Issue M7.0.5
*
*  (c) Copyright TRL Ltd 2013. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/
/*
   TRL: M4.0g [these defines were previously within gbltypes.h]
   
     MOVA system definitions
     
     a) standard/extended definition
     b) PC/target definition
     c) PART TIME definition     
*/

#if !defined (MOVA_H)
#define MOVA_H

/* For standard MOVA (ie original input/output channel sizes use the following
line */
/*#undef EXTMOVA*/

/* For extended MOVA (ie with increased input/output channels use the
following line */
/* This define should always be enabled in MOVA 5 and above */
#define EXTMOVA

/* To run MOVA in PC environment (i.e. using the PCMOVA kernel wrapper) use following line */
/*#define PC_WRAPPER*/

/* To use PCMOVA specific functionality (or lack thereof, e.g. refusing to reinitialise) use the following */
/*#define PCMOVA*/

/* To set MOVA up for PART TIME operation use the following line */
/* #define PARTTIME */

/* On Siemens platform, CRB bit will be first channel after ndets. To avoid 
rewiring of the interface, the following define permits the continued use of 
the original input channel spec. */
#undef TEST_MOVA


/*  IRH MOD: M5.0.0: 11/03/04:
    TRL define for enabling emergency/priority code in MOVA.
    Removing this code made the Compact MOVA executable image 
    small enough to fit into Siemen's old kit (3U/OMU) for 
    testing at the Compact MOVA test sites.  This define
    should typically be left on. */
#define CMOVA_EP


/*  IRH MOD: M5.0.0: 29/09/04: TRL define for turning on/off 
 * any MOVA kernel debugging code */
/*#define MOVA_DEBUG*/

#endif
