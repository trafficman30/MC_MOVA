/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         write.h
*
*  TITLE:        Mova Messages: Thread Safe Screen/Terminal message handling header.
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	26-Jan-1998		4.0.0		..		MC		SIE_00			First undergoing system test
*	16-Feb-1998		4.0.0		..		MC		SIE_01			Added INITIALIZE constant
*	29-Apr-2004		5.0.0		..		IH		SIE_02			Added header guards and Safe_Strncpy() prototype 
*	05-May-2011		7.0.0		AA		PK		CRN003			New Header file inline with MOVA Kernel SQP
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
#ifndef INCLUDED_Write
#define INCLUDED_Write


int WRITE(int, ... );

/* IRH MOD: M5.0.0: 20/7/04: End header guard here -
 * TODO: Don't want to prevent constants below being included multiple
 * times until I'm sure there's no file relying on that.  E.g. BYTE 
 * could be defined as something different elsewhere. */
#endif /* INCLUDED_Write */

#define LH_PORT 1
#define PS_PORT 2

#define LOCAL_HANDSET 3
#define MODEM_PORT 2

#define  MOVA_BYTE   14
#define  INT2	     11
#define  INT4	     15
#define  REAL4       16
#define  STRG	     20
#define  MORE	      9
#define  IOSTAT      30
#define  FMT         31
#define  INITIALIZE  33
