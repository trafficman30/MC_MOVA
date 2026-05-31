/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         sftypes.h
*
*  TITLE:        MOVA Satutation flow - Platform independent type definitions for MovaSatFlow module.
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	10-Feb-2003		6.0.0										Initial implementation for testing with MOVA
*	31-May-2011		7.0.0		AA		PK		CRN003			New Header file inline with MOVA Kernel SQP
*
*  (c) Copyright TRL Ltd 2010. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal or
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/

#if !defined (MOVASATFLOWTYPES_H)
#define MOVASATFLOWTYPES_H

typedef float			REAL;

/* Type size limits */
#define INT8_MIN		(-128)
#define INT8_MAX		(127)
#define INT16_MIN		(-32768)
#define INT16_MAX		(32767)
#define INT32_MIN		(-2147483648)
#define INT32_MAX		(2147483647)
						
#define UINT8_MIN		(0)
#define UINT8_MAX		(255)
#define UINT16_MIN		(0)
#define UINT16_MAX		(65535)
#define UINT32_MIN		(0)
#define UINT32_MAX		(4294967295)


/* Logical defines */


#undef INVALID
#define INVALID			(-1)

typedef int16			LOGIC;


/* 
	Detector types, corresponding to entries in the Comshr.det[mxlane][11] array. 
	This array gives the number/ID of a given type of detector, in a given lane 
	(if present - zero otherwise).
	
	Don't typedef as enum's are ints (= size of the system word)
	but we always want them as 16-bit (?).
*/
enum
{
	DET_TYPE_INVALID = -1,

	DET_TYPE_FIRST,
	DET_TYPE_IN_DET = DET_TYPE_FIRST,
	DET_TYPE_X_DET,
	DET_TYPE_OUT_DET,
	DET_TYPE_IN_SINK,
	DET_TYPE_COMB_X_DET,
	DET_TYPE_X_SINK,
	DET_TYPE_ALT_UP,
	DET_TYPE_ALT_DOWN,
	DET_TYPE_STOP_DET,
	DET_TYPE_IN_SINK2,
	DET_TYPE_X_SINK2,

	DET_TYPE_LAST = DET_TYPE_X_SINK2,

	DET_TYPE_NUM
};
typedef int16			DET_TYPE;


#endif /* MOVASATFLOWTYPES_H */

