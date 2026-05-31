/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         display_msg.h
*
*  TITLE:        MOVA Commissioning screen messages header
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	31-May-2011		7.0.0		AA		PK		CRN003			Functions declarations to send several Mova commissioning screen messages 
*																to the output terminal
*	21-Jan-2013		7.0.5		..		AK		CRN022			Buffer overflow in TMA output
*	25-Jan-2013		7.0.5		..		AK		CRN023			Fix compiler warnings
*	31-Jan-2013		7.0.5		AB		AK		M7.0.5			Issue M7.0.5
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

#include "mova_constants.h"

#if !defined (TMA_MESSAGE_H)
#define TMA_MESSAGE_H



/*
Reproducing format for 1.1 Message Type and printing Message type 1.1
Fortran format was as follows

 "('S',i2,i3,':',i2,':',i2,' SMF',5i3,3(1x,5i3),i5' SAT',5i1,3(1x,5i1),'  ',"  "i1' LAM',i2,i3,' CUT=',i1)"

*/
void printStrgM11(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int noLanes, int outDevice);

/*
Reproducing format for 1.2 Message Type and printing Message type 1.2
Fortran format was as follows

 "('SMCYC',i4,' DMX',4i3,3(2x,4i3),'SUS',6i1,3(1x,6i1),2x,4(1x,6i1))"

*/

void printStrgM12(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int noStages, int noDet, int outDevice);
/*
Reproducing format for 2 Message Type and printing Message type 2
Fortran format was as follows

 "(i3,' SMIN',i2,' LMIN',6i2,3(1x,6i2),'RCX',5i2,3(1x,5i2))"

*/
void printStrgM20(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int noLinks, int noLanes, int outDevice);
/*
Reproducing format for 3.1 Message Type and printing Message type 3.1
Fortran format was as follows

 "(i3,' NX',i2,' ESLI',4(6i1,1x),6(i3,'LA',3i3))"

*/
void printStrgM31(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int noLinks, int noLanes, int lenStr, int outDevice);


/*
Reproducing format for 3.2 Message Type and printing Message type 3.2
Fortran format was as follows

 "(i3,' NX',i2,' hold',i1,' ext',i1,' xtimr',i3,' lxtm',6i2,3(1x,6i2),'revdem',6i1,3(1x,6i1))"

*/
void printStrgM32(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int noLinks, int outDevice);

/*
Reproducing format for 4 Message Type and printing Message type 4
Fortran format was as follows 
"(i3,' NX',i2,' BDR',2i3,i4,1x,6(i2,'LK',2i4,2i3))"
*/

void printStrgM4(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int noLinks,  int outDevice);

/*
Reproducing format for 5.1 Message Type and printing Message type 5.1
Fortran format was as follows
 "(i3,' NX',i2,i3,':',i2,':',i2,' OPT BDR',i4,i3,i4,' CF',i3,' DEM',6i1,3(1x,6i1),' RCX',5i2,3(1x,5i2))"
 lanes will form blocks of 6 digits similar to links
*/

void printStrgM51(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int noLinks, int noLanes, int outDevice);

/*
Reproducing format for 5.2 Message Type and printing Message type 5.2
Fortran format was as follows
static const char *const F5092a = {
"(i3,' NX',i2,i3,':',i2,':',i2,' MAX CHANGE, DMX+MXP=',i4,' DEM',6i1,3(1x,6i1),' RCX',5i2,3(1x,5i2))"
"(i3,' NX',i2,i3,':',i2,':',i2,' MAX CHANGE, PED MAX=',i3,' DEM',6i1,3(1x,6i1),' RCX',5i2,3(1x,5i2))"
*/
void printStrgM52(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int rav, int noLinks, int noLanes, int outDevice);

/*
Reproducing format for 5.3 Message Type and printing Message type 5.3
Fortran format was as follows
"(i3,' NX',i2,i3,':',i2,':',i2,' OSAT+ES, PCS',6i2,' DEM',6i1,3(1x,6i1),' RCX',5i2,3(1x,5i2))"
*/

void printStrgM53(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int noLinks, int noLanes, int outDevice);

 /*
Reproducing format for 5.4 Message Type and printing Message type 5.4
Fortran format was as follows
"(i3,' NX',i2,i3,':',i2,':',i2,' EM/PR TRUNC. ENX=',i2,' PNX=',i2,' DEM',6i1,3(1x,6i1),' RCX',5i2,3(1x,5i2))"
*/

void printStrgM54(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int noLinks, int noLanes, int outDevice);
/*
Reproducing format for 6.1 Message Type and printing Message type 6.1
Fortran format was as follows
"   IG:',i2,' SDEM',8i1,i2,7i1,' BON',6i2,i3,5i2,i3,5i2,' RCIN',5i2,i3,4i2,i3,4i2,i3,4i2)"
*/
void printStrgM61(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int noStages, int noLinks, int noLanes, int outDevice);
/*
Reproducing format for 6.2 Message Type and printing Message type 6.2
Fortran format was as follows
"   IG:',i2,' EM/PR CHANGE. SDEM',8i1,i2,7i1,' TRUN',6i1,3(1x,6i1),' SKIP',6i1,3(1x,6i1))"
*/
void printStrgM62(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int noStages, int noLinks, int outDevice);

#endif /*TMA_MESSAGE_H*/
