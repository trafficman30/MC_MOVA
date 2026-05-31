/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         defines.h
*
*  TITLE:        MOVA Kernel: Common MOVA Constants. 
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	20-May-2011		7.0.0		AA		PK		CRN003			Initial implementation
*	12-Dec-2011		7.0.1		AB		PK		CRN004			SatFlow and CANDET improvements
*
*  (c) Copyright TRL Ltd 2011. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/

#if !defined (DEFINES_H)
#define DEFINES_H

#include "mova.h"

/*  ========================  EXTENDED MOVA  ========================== */
#ifdef EXTMOVA
/* System Limits   */
#define NumberOfDetectors  64  /* Was 32   */
#define NumberOfConfirms   32  /* Inc RTC Was 16   */
#define NumberOfForce      10  /* Was 8    */

#ifdef M8_IMPROVED_LINKING
#define NumberOfExtra       5  /* HI, TO, Hold Signal, Release Signal, Sync pulse  */
#else
#define NumberOfExtra       3  /* HI, TO, Sync pulse  */
#endif

#ifdef M8_ENABLED
#define NumberofOutputChans 8  /* Output channels */
#else
#define NumberofOutPutChans 0
#endif

/* start of Output Channels */
#define StartofOutputChans  (NumberOfForce + NumberOfExtra)

/* TRL mod: added limits */
#define NumberOfMovaLinks      60  /* Was 24 */
#define NumberOfLanes      30  /* Was 20 */
#define NumberOf_din      100
#define NumberOf_dout      24

/* Array Limits  */
#ifndef M8_IMPROVED_LINKING
#define ForceArraySize  (NumberOfForce+2)   /* force bits + HI + TO HI & TO now on MPU  */
#else
#define ForceArraySize  (NumberOfForce+4)   /* force bits + HI + TO HI & TO + Hold + Release now on MPU  */
#endif

/* Din/Out individual bits  */

#define DinStartOfDetectors   0
#define DinStartOfConfirms    NumberOfDetectors  /* Starts after Det's  */
#define DinRTCBit             (NumberOfDetectors+NumberOfConfirms-1)

#define DoutStartOfForce      0
#define DoutHI               17
#define DoutTO               16
#define DoutSync             18
#define DoutDetFault         19
#define DoutMovaFault        20

/* IRH MOD: M5.0.0: 02/09/04: Index of the sync pulse in the control[] output array (Genstg). 
 * N.B. For 'extended' MOVA this is not equal to DoutSync as it is for 'standard' MOVA 
 * Although the sync pulse isn't used by MOVASIM version 5, it could be at some point 
 * if MOVASIM is modified for a different manufacturer or for PC-MOVA. */
#define ControlSync          12

/*
 Din[] Assignments (EXTMOVA)
 -----------------
 0..63      Detectors
 64..94     Stage/Phase Confirms
 95         Ready to Control
 96         RAM Battery (Ferranti system - unused in Monitron MOVA)
 97         +10v Supply (Ferranti system - unused in Monitron MOVA)
 98         MPU Battery (Ferranti system - unused in Monitron MOVA)

 Dout[] Assignments (EXTMOVA)
 ------------------
 0..15      Stage Force
 16,17      HI,TO
 18         Test synch pulse
 19         'DET' Fault LED
 20         'MOVA' fault LED
 21..23     Unused
*/

#endif   /*  =====  END EXTENDED MOVA  ========== */

/* -------------- End of Hardware Specific definitions ---------------*/

/* IRH MOD: M5.0.0: 21/07/04: Added define for BOOL type (16-bit int) 
 * and for specific size types. */
/* IRH MOD: 22/05/06: Use typedef rather than #define for BOOL */
typedef int             BOOL;
typedef signed char     int8; /* ranges from -128 to 128*/
typedef signed short    int16; /* ranges from -32,768 to 32,768*/
typedef signed long     int32; /* ranges from -2,147,483,648 to 2,147,483,648*/
typedef unsigned char   uint8; /* ranges from 0 to 256*/
typedef unsigned short  uint16; /* ranges from 0 to 65,536 */
typedef unsigned long   uint32; /* ranges from 0 to 4,294,967,296 */
typedef float			real32; /* 3.4E +/- 38 (7 digits) */
/* 
 --- Set maximum dimensions for various parameters:
*/
#define mxstag NumberOfForce
#define mxlink NumberOfMovaLinks
#define mxlane NumberOfLanes
#define mxdets NumberOfDetectors
#define mxconf NumberOfConfirms
#define mxlnOnlink 3
#define mxdetTypes 11

#ifdef M8_IMPROVED_LINKING
#define HOLD_SIGNAL_BIT		NumberOfForce + 3
#define RELEASE_SIGNAL_BIT	NumberOfForce + 4
#endif

/*#define IMESSROW 50*//* Number of messages stored*/
/*#define IMESSCOL 184*/ /*length of the IMESS message*/
/*  MRC: MOVA M7.0.0, 5/3/10: defines for two of the new arrays */
#define xgnSFTIMS_MAX           (6)
#define xgnREDPED_MAX           (2)

/*First Dimension for occupancy Alert limits
Dimension 1: Occual ON for each stage
Dimension 2: Occual OFF for each stage*/
#define xgnOCC_ON_OFF_DIM		(2)
#define xgnMAX_DATASETS			(4)

#define MAX_TMA_LOGS			672
#endif /*DEFINES_H*/



