/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         display_msg.c
*
*  TITLE:        MOVA Commissioning screen messages
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	31-May-2011		7.0.0		AA		PK		CRN003			Functions to send several Mova commissioning screen messages 
*																to the output terminal
*	21-Jan-2013		7.0.5		..		AK		CRN022			Buffer overflow in TMA output
*	25-Jan-2013		7.0.5		..		AK		CRN023			Fix compiler warnings
*	31-Jan-2013		7.0.5		AB		AK		M7.0.5			Issue M7.0.5
*
*  (c) Copyright TRL Ltd 2013. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal or
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "defines.h"
#include "obclock.h"
#include "display_msg.h"
#include "generalfunc.h"

#include "mova_constants.h"

static char buf[MAX_OUTPUT_LEN];
static char tmp[TMP_BUF_LEN];


/* 
For all M5 messages, last part is always printing revdem and redcx
*/
static char * printLiLaM5(char *outbuf, const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int indFrom, int noLinks, int noLanes);

/*
Print a string in blocks of 6 digits
*/
static char * printLine6dig(char *outbuf, const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int indFrom, int maxLen);

/*
Print a string in 2 digits in blocks of 6
*/
static char * printLine2dig6blck(char *outbuf, const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int indFrom, int maxLen);

/*
Trim trailing spaces off of a string 
*/
void TrimString(char *stringtochange)
{
	char *current = stringtochange;
	int len = strlen(current);

	current = current + len - sizeof(char);
	while (*current == ' ')
	{
		*current = '\0';
		current--;
	}
}

/*
Print and trim to specific devices
*/
void printMessage(char *MsgBuf, int DeviceID)
{
	TrimString(MsgBuf);

#ifdef TRL_INTEGRATION_TEST
	if (DeviceID == SYS_LOG_PRINTING)
	{
		strcat(buf, "\n");
		SysLogWrite( buf );
	}
	else
#endif
	{
		strcat(buf, "\r\n");
		writeString(DeviceID, buf);
	}

}

/*
Reproducing format for 1.1 Message Type and printing Message type 1.1
Fortran format was as follows

 "('S',i2,i3,':',i2,':',i2,' SMF',5i3,3(1x,5i3),i5' SAT',5i1,3(1x,5i1),'  ',"  "i1' LAM',i2,i3,' CUT=',i1)"

*/
void printStrgM11(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int noLanes, int outDevice)
{	
	int i, j, k;

	i = 2;
	
	/* cursig */
	sprintf(buf, "S %1ld ", strBuff[selRow][i++]); 

	/* time [3] = hours, [4] = min,  [5]= sec*/
	sprintf(tmp, "%.2ld:%.2ld:%.2ld", strBuff[selRow][i], strBuff[selRow][i+1], strBuff[selRow][i+2]);
	i += 3;
	strcat(buf, tmp);

	/* smflow [6] */
	strcat(buf, " SMF ");

	for (j = 0; j < (noLanes / 5); j++)
	{
		for (k = 0; k < 5; k++)
		{
			sprintf(tmp, "%2ld ", strBuff[selRow][i+k]);
			strcat(buf, tmp);
		}

		i += 5;
		strcat(buf, "  ");
	}

	if ((noLanes % 5) > 0)
	{		
		for (j = 0; j < noLanes % 5; j++)
		{
			sprintf(tmp, "%2ld ", strBuff[selRow][i+j]);
			strcat(buf, tmp);
		}

		i += (noLanes % 5);
	}

	/* junsmf */ 
	sprintf(tmp, "%4ld ", strBuff[selRow][i++]); 
	strcat(buf, tmp);

	/* sat [] */
	strcat(buf, " SAT ");
	printLine6dig(tmp, strBuff, selRow, i, noLanes);
	strcat(buf, tmp);
	
	i += noLanes;
	/* laosmx */
	sprintf(tmp, "%2ld ", strBuff[selRow][i++]); 
	strcat(buf, tmp);

	/* lambda */
	sprintf(tmp, " LAM %2ld ", strBuff[selRow][i++]); 
	strcat(buf, tmp);

	/* totalam */
	sprintf(tmp, "%4ld ", strBuff[selRow][i++]); 
	strcat(buf, tmp);

	/* cutmx */
	sprintf(tmp, " CUT %2ld ", strBuff[selRow][i++]); 
	strcat(buf, tmp);
	printMessage(buf, outDevice);
}



/*
Reproducing format for 1.2 Message Type and printing Message type 1.2
Fortran format was as follows

 "('SMCYC',i4,' DMX',4i3,3(2x,4i3),'SUS',6i1,3(1x,6i1),2x,4(1x,6i1))"

*/

void printStrgM12(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int noStages, int noDet, int outDevice)
{	
	int i, j, k;

	i=2;
	
	/* smcycle[2] */
	sprintf(buf, "SMCYC %3ld", strBuff[selRow][i++]); 

	/* lodfmx or drifmx [3] */
	strcat(buf, " DMX ");

	for (j = 0; j < (noStages / 5); j++)
	{
		for (k = 0; k < 5; k++)
		{
			sprintf(tmp, "%2ld ", strBuff[selRow][i+k]);
			strcat(buf, tmp);
		}

		i += 5;
		strcat(buf, "  ");
	}

	if ((noStages % 5) > 0)
	{
		for (j = 0; j < noStages % 5; j++)
		{
			sprintf(tmp, "%2ld ", strBuff[selRow][i+j]);
			strcat(buf, tmp);
		}

		i += (noStages % 5);
	}
	/* suspect detectors */ 	

	/* sat [] */
	strcat(buf, " SUS ");
	printLine6dig(tmp, strBuff, selRow, i, noDet);
	strcat(buf, tmp);

#ifdef M8_RT_OPTIMISATION
	/* RT info*/
	sprintf(tmp, " IS%1ld G%1ld", strBuff[selRow][i+noDet], strBuff[selRow][i+noDet+1]);
	strcat(buf, tmp);
#endif

	printMessage(buf, outDevice);

}



/*
Reproducing format for 2 Message Type and printing Message type 2
Fortran format was as follows

 "(i3,' SMIN',i2,' LMIN',6i2,3(1x,6i2),'RCX',5i2,3(1x,5i2))"

*/
void printStrgM20(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int noLinks, int noLanes, int outDevice)
{	
	int i, j, k;

	i=2;
	
	/* sistim */
	sprintf(buf, "%3ld", strBuff[selRow][i++]);
	
	/* stamin */
	sprintf(tmp, " SMIN %2ld", strBuff[selRow][i++]);
	strcat(buf, tmp);

	/* link minimums */
	strcat(buf, " LMIN ");
	for (j = 0; j < (noLinks / 6); j++)
	{
		for (k = 0; k < 6; k++)
		{
			sprintf(tmp, "%1ld ", strBuff[selRow][i+k]);
			strcat(buf, tmp);
		}

		i += 6;
		strcat(buf,"  ");
	}

	if ((noLinks % 6) > 0)
	{
		for (j = 0; j < noLinks % 6; j++)
		{
			sprintf(tmp, "%1ld ", strBuff[selRow][i+j]);
			strcat(buf, tmp);
		}

		i += (noLinks % 6);
	}

	/* RCX  */	
	strcat(buf, " RCX ");
	for (j = 0; j < (noLanes / 5); j++)
	{
		for (k = 0; k < 5; k++)
		{
			sprintf(tmp, "%1ld ", strBuff[selRow][i+k]);
			strcat(buf, tmp);
		}

		i += 5;
		strcat(buf, "  ");
	}

	if ((noLanes % 5) > 0)
	{		
		for (j = 0; j < noLanes % 5; j++)
		{
			sprintf(tmp, "%1ld ", strBuff[selRow][i+j]);
			strcat(buf, tmp);
		}
	}
	printMessage(buf, outDevice);
}



/*
Reproducing format for 3.1 Message Type and printing Message type 3.1
Fortran format was as follows

 "(i3,' NX',i2,' ESLI',4(6i1,1x),6(i3,'LA',3i3))"

*/
void printStrgM31(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int noLinks, int noLanes, int lenStr, int outDevice)
{	
	int i, j;

	i=2;
	
	/* sistim */
	sprintf(buf, "%3ld", strBuff[selRow][i++]);
	
	/*NX*/
	sprintf(tmp, " NX %1ld ", strBuff[selRow][i++]);
	strcat(buf, tmp);

	/* ESLI*/
	strcat(buf, " ESLI ");
	if (noLanes <= noLinks)
	{
		printLine6dig(tmp, strBuff, selRow, i, noLanes);
	}
	else
	{
		printLine6dig(tmp, strBuff, selRow, i, noLinks);
	}

	strcat(buf, tmp);
	i += noLinks;	/* AK - Should this be noLanes if noLanes < noLinks ?? */

	/*	LA  */	
	strcat(buf, " ");

	for (j = i; j < lenStr; j += 4)
	{
		sprintf(tmp, "%1ldLA %1ld %1ld %1ld  ", strBuff[selRow][j], strBuff[selRow][j+1], strBuff[selRow][j+2], strBuff[selRow][j+3]);
		strcat(buf, tmp);
	}

#ifdef M8_IMPROVED_LINKING
	sprintf(tmp, " OFFSET_DEF %1ld", strBuff[selRow][j]);
	strcat(buf, tmp);

	sprintf(tmp, " RCX(2) %1ld", strBuff[selRow][j+1]);
	strcat(buf, tmp);
#endif

	printMessage(buf, outDevice);
}



/*
Reproducing format for 3.2 Message Type and printing Message type 3.2
Fortran format was as follows

 "(i3,' NX',i2,' hold',i1,' ext',i1,' xtimr',i3,' lxtm',6i2,3(1x,6i2),'revdem',6i1,3(1x,6i1))"

*/
void printStrgM32(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int noLinks,  int outDevice)
{	
	int i;

	i=2;
	
	/* sistim */
	sprintf(buf, "%3ld", strBuff[selRow][i++]);
	
	/*snext*/
	sprintf(tmp, " NX %1ld ", strBuff[selRow][i++]);
	strcat(buf, tmp);

	/* ephold*/
	sprintf(tmp, " hold %1ld ", strBuff[selRow][i++]);
	strcat(buf, tmp);

	/* epx*/
	sprintf(tmp, " ext %1ld ", strBuff[selRow][i++]);
	strcat(buf, tmp);

	/* epxmxt*/
	sprintf(tmp, " xtimr %1ld ", strBuff[selRow][i++]);
	strcat(buf, tmp);

	strcat(buf, "   lxtm ");

	printLine2dig6blck(tmp, strBuff, selRow, i, noLinks);
	strcat(buf, tmp);
	i += noLinks;

#ifndef M8_REMOVE_REV_DEMAND 
	strcat(buf, "   revdem ");
	printLine6dig(tmp, strBuff, selRow, i, noLinks);
	strcat(buf, tmp);
#endif

#ifdef M8_IMPROVED_LINKING
	i += noLinks;

	/*Printing the offset*/
	sprintf(tmp, " OFFSET_DEF %1ld", strBuff[selRow][i]);
	strcat(buf, tmp);

	sprintf(tmp, " RCX(2) %1ld", strBuff[selRow][i+1]);
	strcat(buf, tmp);
#endif
	
	printMessage(buf, outDevice);
}


/*
Reproducing format for 4 Message Type and printing Message type 4
Fortran format was as follows

 "(i3,' NX',i2,' BDR',2i3,i4,1x,6(i2,'LK',2i4,2i3))"

*/
void printStrgM4(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int noLinks,  int outDevice)
{	
	int i;
	int linkct;

	i=2;
	
	/* sistim */
	sprintf(buf, "%3ld", strBuff[selRow][i++]);
	
	/*snext*/
	sprintf(tmp, " NX %1ld ", strBuff[selRow][i++]);
	strcat(buf, tmp);

	/* BDR  */
	sprintf(tmp, "BDR%3ld%3ld%4ld ", strBuff[selRow][i], strBuff[selRow][i+1], strBuff[selRow][i+2]); 
	strcat(buf, tmp);
	i += 3;

	/* <N>LK for each link */
	for (linkct = 0; linkct < noLinks; linkct++ )
	{
		sprintf(tmp, "%2ldLK", strBuff[selRow][i++]);
		strcat(buf, tmp);
		sprintf(tmp, "%4ld%4ld%3ld%3ld", strBuff[selRow][i], strBuff[selRow][i+1], strBuff[selRow][i+2], strBuff[selRow][i+3]); 
		strcat(buf, tmp);
		i += 4;
	}

		
#ifdef M8_IMPROVED_LINKING
	/* Output the offset for the improved linking*/
	sprintf(tmp, " OFFSET_DEF %1ld", strBuff[selRow][i]); 
	strcat(buf, tmp);

	sprintf(tmp, " RCX(2) %1ld", strBuff[selRow][i+1]);
	strcat(buf, tmp);
#endif

	printMessage(buf, outDevice);

}



/*
Reproducing format for 5.1 Message Type and printing Message type 5.1
Fortran format was as follows
 "(i3,' NX',i2,i3,':',i2,':',i2,' OPT BDR',i4,i3,i4,' CF',i3,' DEM',6i1,3(1x,6i1),' RCX',5i2,3(1x,5i2))"
 lanes will form blocks of 6 digits similar to links
*/

void printStrgM51(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow,int noLinks, int noLanes, int outDevice)
{	
	int i;

	i=2;

	sprintf(buf, "%3ld NX %1ld ", strBuff[selRow][i], strBuff[selRow][i+1]); 
	i += 2;

	/* time [3] = hours, [4] = min,  [5]= sec*/
	sprintf(tmp, "%.2ld:%.2ld:%.2ld", strBuff[selRow][i], strBuff[selRow][i+1], strBuff[selRow][i+2]);
	i += 3;
	strcat(buf, tmp);

	/*OPT BDR  */
	sprintf(tmp, " OPT BDR %2ld %2ld %2ld", strBuff[selRow][i], strBuff[selRow][i+1], strBuff[selRow][i+2]); 
	strcat(buf, tmp);
    i += 3;

	/* CF */
	sprintf(tmp," CF %2ld ", strBuff[selRow][i++]); 
	strcat(buf, tmp);
	
	/* DEM  and RCX*/
	printLiLaM5(tmp, strBuff, selRow, i, noLinks, noLanes);
	strcat(buf, tmp);

	printMessage(buf, outDevice);
}



/*
Reproducing format for 5.2 Message Type and printing Message type 5.2
Fortran format was as follows
static const char *const F5092a = {
"(i3,' NX',i2,i3,':',i2,':',i2,' MAX CHANGE, DMX+MXP=',i4,' DEM',6i1,3(1x,6i1),' RCX',5i2,3(1x,5i2))"
"(i3,' NX',i2,i3,':',i2,':',i2,' MAX CHANGE, PED MAX=',i3,' DEM',6i1,3(1x,6i1),' RCX',5i2,3(1x,5i2))"
*/
void printStrgM52(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int rav, int noLinks, int noLanes, int outDevice)
{
	int i = 2;

	sprintf(buf, "%2ld NX %1ld ", strBuff[selRow][i], strBuff[selRow][i+1]); 
	i += 2;

	/* time [3] = hours, [4] = min,  [5]= sec*/
	sprintf(tmp, "%.2ld:%.2ld:%.2ld", strBuff[selRow][i], strBuff[selRow][i+1], strBuff[selRow][i+2]);
	i += 3;
	strcat(buf, tmp);
	
	/* The following part is not in M7, and hence commneting it ...*/
	/*
	// New rav element added but miss out initially and print using current rav
	i += 1;	*/

	/*MAX CHANGE, DMX+MXP  OR PED MAX */
	if  (rav > 0)
	{
		sprintf(tmp, " MAX CHANGE, PED MAX= %2ld", strBuff[selRow][i++]); 
	}
	else
	{
		sprintf(tmp, " MAX CHANGE, DMX+MXP= %2ld", strBuff[selRow][i++]);
	}

	strcat(buf, tmp);
	
	/*i--; //adding this to skip displaying RAV, it is just used to control the previous condition*/

	/* DEM  and RCX*/
	printLiLaM5(tmp, strBuff, selRow, i, noLinks, noLanes);
	strcat(buf, tmp);

	printMessage(buf, outDevice);
}



/*
Reproducing format for 5.3 Message Type and printing Message type 5.3
Fortran format was as follows
"(i3,' NX',i2,i3,':',i2,':',i2,' OSAT+ES, PCS',6i2,' DEM',6i1,3(1x,6i1),' RCX',5i2,3(1x,5i2))"
*/
void printStrgM53(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow,int noLinks, int noLanes, int outDevice)
{	
	int i, j, k;

	i = 2;

	sprintf(buf, "%2ld NX %1ld ", strBuff[selRow][i], strBuff[selRow][i+1]); 
	i += 2;

	/* time [3] = hours, [4] = min,  [5]= sec*/
	sprintf(tmp, "%.2ld:%.2ld:%.2ld", strBuff[selRow][i], strBuff[selRow][i+1], strBuff[selRow][i+2]);
	i += 3;
	strcat(buf, tmp);

	/* OSAT+ES, PCS  */
	strcat(buf, " OSAT+ES, PCS ");

	k = (noLanes <= noLinks) ? noLanes : noLinks;

	for (j = i; j < k + i; j++)
	{
		sprintf(tmp, "%1ld ", strBuff[selRow][j]);
		strcat(buf, tmp);
	}

	i += noLinks;

	/* DEM  and RCX*/
	printLiLaM5(tmp, strBuff, selRow, i, noLinks, noLanes);
	strcat(buf, tmp);

	printMessage(buf, outDevice);
}



 /*
Reproducing format for 5.4 Message Type and printing Message type 5.4
Fortran format was as follows
"(i3,' NX',i2,i3,':',i2,':',i2,' EM/PR TRUNC. ENX=',i2,' PNX=',i2,' DEM',6i1,3(1x,6i1),' RCX',5i2,3(1x,5i2))"
*/

void printStrgM54(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow,int noLinks, int noLanes, int outDevice)
{	
	int i;

	i = 2;

	sprintf(buf, "%2ld NX %1ld ", strBuff[selRow][i], strBuff[selRow][i+1]);
	i += 2;

	/* time [3] = hours, [4] = min,  [5]= sec*/
	sprintf(tmp, "%.2ld:%.2ld:%.2ld", strBuff[selRow][i], strBuff[selRow][i+1], strBuff[selRow][i+2]);
	i += 3;
	strcat(buf, tmp);

	/* EM/PR TRUNC. ENX and PNX  */
	sprintf(tmp, "  EM/PR TRUNC. ENX=%2ld   PNX=%2ld ", strBuff[selRow][i], strBuff[selRow][i+1]); 
	strcat(buf, tmp);
    i += 2;

	/* DEM  and RCX*/
	printLiLaM5(tmp, strBuff, selRow, i, noLinks, noLanes);
	strcat(buf, tmp);
	
	printMessage(buf, outDevice);
}



/*
Reproducing format for 6.1 Message Type and printing Message type 6.1
Fortran format was as follows
"   IG:',i2,' SDEM',8i1,i2,7i1,' BON',6i2,i3,5i2,i3,5i2,' RCIN',5i2,i3,4i2,i3,4i2,i3,4i2)"
*/
void printStrgM61(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow,int noStages, int noLinks, int noLanes, int outDevice)
{	
	int i, j, k;

	i=2;

	sprintf(buf, "   IG:%1ld ", strBuff[selRow][i++]); 

	/*SDEM stages*/
	strcat(buf, " SDEM ");
	
	for (j = 0; j < (noStages / 5); j++)
	{
		for (k = 0; k < 5; k++)
		{
			sprintf(tmp, "%1ld", strBuff[selRow][i+k]);
			strcat(buf, tmp);
		}

		i += 5;
		strcat(buf, " ");
	}

	if ((noStages % 5) > 0)
	{
		for (j = 0; j < noStages % 5; j++)
		{
			sprintf(tmp, "%1ld", strBuff[selRow][i+j]);
			strcat(buf, tmp);
		}

		i += (noStages % 5);
	}
	
	/*RCIN links*/
	strcat(buf, " BON ");

	for (j = 0; j < (noLinks / 5); j++)
	{
		for (k = 0; k < 5; k++)
		{
			sprintf(tmp, "%1ld ", strBuff[selRow][i+k]);
			strcat(buf, tmp);
		}

		i += 5;
		strcat(buf, "  ");
	}

	if ((noLinks % 5) > 0)
	{
		for (j = 0; j < noLinks % 5; j++)
		{
			sprintf(tmp,"%1ld ", strBuff[selRow][i+j]);
			strcat(buf, tmp);
		}

		i += (noLinks % 5);
	}

	/*RCIN lanes*/
	strcat(buf, " RCIN ");
	for (j = 0; j < (noLanes / 5); j++)
	{
		for (k = 0; k < 5; k++)
		{
			sprintf(tmp, "%1ld ", strBuff[selRow][i+k]);
			strcat(buf, tmp);
		}

		i += 5;
		strcat(buf, "  ");
	}

	if ((noLanes % 5) > 0)
	{
		for (j = 0; j < noLanes % 5; j++)
		{
			sprintf(tmp, "%1ld ", strBuff[selRow][i+j]);
			strcat(buf, tmp);
		}
	}

	printMessage(buf, outDevice);
}



/*
Reproducing format for 6.2 Message Type and printing Message type 6.2
Fortran format was as follows
"   IG:',i2,' EM/PR CHANGE. SDEM',8i1,i2,7i1,' TRUN',6i1,3(1x,6i1),' SKIP',6i1,3(1x,6i1))"
*/
void printStrgM62(const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int noStages, int noLinks, int outDevice)
{	
	int i, j, k;
	 
	i = 2;
	sprintf(buf, "   IG:%1ld ", strBuff[selRow][i++]); 

	/*EM/PR CHANGE. SDEM  stages*/
	strcat(buf, " EM/PR CHANGE. SDEM ");
	
	for (j = 0; j < (noStages / 5); j++)
	{
		for (k = 0; k < 5; k++)
		{
			sprintf(tmp, "%1ld", strBuff[selRow][i+k]);
			strcat(buf, tmp);
		}

		i += 5;
		strcat(buf, " ");
	}

	if ((noStages % 5) > 0)
	{	
		for (j = 0; j < noStages % 5; j++)
		{
			sprintf(tmp, "%1ld", strBuff[selRow][i+j]);
			strcat(buf, tmp);
		}

		i += (noStages % 5);
	}
		
	/*TRUN links*/
	strcat(buf, " TRUN ");
	printLine2dig6blck(tmp, strBuff, selRow, i, noLinks);
	strcat(buf, tmp);
	i += noLinks;

	/*SKIP links*/
	strcat(buf, " SKIP ");
	printLine2dig6blck(tmp, strBuff, selRow, i, noLinks);
	strcat(buf, tmp);
	i += noLinks;

	printMessage(buf, outDevice);
}



/* 
For all M5 messages, last part is always printing revdem and redcx
*/
static char * printLiLaM5(char *outbuf, const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int indFrom, int noLinks, int noLanes)
{
	char tmpbuf[8];
	int i, j, k;

	i = indFrom;
	
	/* DEM */
	sprintf(outbuf, " DEM ");

	for (j = 0; j < (noLinks / 6); j++)
	{
		for (k = 0; k < 6; k++)
		{
			sprintf(tmpbuf, "%1ld", strBuff[selRow][i+k]);
			strcat(outbuf, tmpbuf);
		}

		i += 6;
		strcat(outbuf, " ");
	}

	if ((noLinks % 6) > 0)
	{	
		for (j = 0; j < noLinks % 6; j++)
		{
			sprintf(tmpbuf, "%1ld", strBuff[selRow][i+j]);
			strcat(outbuf, tmpbuf);
		}

		i += (noLinks % 6);
	}

	/* RCX  */	
	strcat(outbuf, " RCX ");
	
	for (j = 0; j < (noLanes / 5); j++)
	{
		for (k = 0; k < 5; k++)
		{
			sprintf(tmpbuf, "%1ld ", strBuff[selRow][i+k]);
			strcat(outbuf, tmpbuf);
		}

		i += 5;
		strcat(outbuf, "  ");
	}

	if ((noLanes % 5) > 0)
	{		
		for (j = 0; j < noLanes % 5; j++)
		{
			sprintf(tmpbuf, "%1ld ", strBuff[selRow][i+j]);
			strcat(outbuf, tmpbuf);
		}
	}

	return outbuf;
}



/*
Print a string in blocks of 6 digits
*/

static char * printLine6dig(char *outbuf, const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int indFrom, int maxLen)
{
	char tmpbuf[8];
	int i, j, k;

	i = indFrom;
	*outbuf = 0;
	
	for (j = 0; j < (maxLen / 6); j++)
	{
		for (k = 0; k < 6; k++)
		{
			sprintf(tmpbuf, "%1ld", strBuff[selRow][i+k]);
			strcat(outbuf, tmpbuf);
		}

		i += 6;
		strcat(outbuf, " ");
	}

	if ((maxLen % 6) > 0)
	{
		for (j = 0; j < maxLen % 6; j++)
		{
			sprintf(tmpbuf, "%1ld", strBuff[selRow][i+j]);
			strcat(outbuf, tmpbuf);
		}
	}

	return outbuf;
}



/*
Print a string in 2 digits in blocks of 6
*/

static char * printLine2dig6blck(char *outbuf, const int32 strBuff[MAX_MSGS][MSG_MAX_LENGTH], int selRow, int indFrom, int maxLen)
{
	char tmpbuf[8];
	int i, j, k;

	i = indFrom;
	*outbuf = 0;
	
	for (j = 0; j < (maxLen / 6); j++)
	{
		for (k = 0; k < 6; k++)
		{
			sprintf(tmpbuf, "%1ld ", strBuff[selRow][i+k]);
			strcat(outbuf, tmpbuf);
		}

		i += 6;
		strcat(outbuf, "  ");
	}

	if ((maxLen % 6) > 0)
	{		
		for (j = 0; j < maxLen % 6; j++)
		{
			sprintf(tmpbuf, "%1ld ", strBuff[selRow][i+j]);
			strcat(outbuf, tmpbuf);
		}
	}

	return outbuf;
}
