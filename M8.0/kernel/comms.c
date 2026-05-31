/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         comms.c: 
*
*  TITLE:        Mova Kernel: Communication with local handset/telephone line
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	26-Jan-1998		4.0.0		..		MC		SIE_00			First undergoing system test
*	24-Apr-1998		4.0.0		..		MC		SIE_01			Call to ResetModemBuf() made from phone()		
*	29-Apr-1988		4.0.0		..		MC		SIE_02			'const' added to initialised static's (Siemens MOVA-13)
*	03-Mar-1998		4.0.0		..		PC		SIE_03			Overlay on 'Xcomshr', etc., replaced by explicit use of structures.
*	11-May-1998		4.0.0		..		PC		SIE_04			Added InitHandset() (for MONITR.C)
*	16-Jan-2000		4.0.0		..		PC		SIE_05			SendStringToModem() and SendStringToLH() both moved to MOVAIF.C.					
*	09-Mar-2000		4.0.0		..		PC		SIE_06			Modified GetHandsetChar and CollectModemChar() so they do not echo any non-printable ASCII chars.
*	15-Mar-2000		4.0.0		..		PC		SIE_07			GetHandsetChar() does not need to filter out CTRL-Z as CollectModemChar() filters out all CTRL characters.
*	29-Sep-2004		5.0.0		..		IH		SIE_08			Local integers were declared as size 12, now mxlinks
*	29-Sep-2004		5.0.0		..		IH		SIE_09			Added non-echoing CollectModemChar i.e. CollectModemCharNE() as in MOVA 5 we can read in datasets remotely.
*	31-May-2011		7.0.0		AA		PK		CRN003			Updated Header file inline with Mova Kernel Software Quality Plan
*
*  (c) Copyright TRL Ltd 2010. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/

/* Include files */
#include <nucleus.h>
#include <ctype.h> /* IRH MOD: M5.0.0: 07/10/04: Added for toupper() */
#include <string.h>
#include <proj_def.h>
#include "gbltypes.h"
#include <error.h>
#include <nu_omu.h>
#include <uart_int.h>
#include <diff_bss.h>

/* Module #defines */
#define ANSWER     1
#define RELEASE    2

/* External variables */
extern const char cLF;
extern const char cCR;
extern const char cBackSp;
extern const char cDelete;

/* available to GetModemChar and CollectModemChar */
static unsigned char sMOVA_ModemBuf_[128];
static int nBufPtr; /* points to most recent char in buffer */
static int nStartPtr;
static int nEndPtr; /* next char to return */

unsigned char lGood_MOVA_TelNo = FALSE;  /* TRL/SIE: to circumvent variables derived from OMU config data */


char GetHandsetChar(void)
{
    char cResult;

    /* get character received, 0 if none */
    cResult = GetMOVACharFromHandset();

    /* echo character back to port, except delete which is handled later */
    if ( ( cResult != 0 )
      && ( cResult != cBackSp )
      && ( cResult != cDelete ) )
    {
        SendStringToLH( &cResult, 1);
    }
    /* end if */
    return (cResult);
}
         

char GetHandsetCharNE(void)
{
    /* get character received, 0 if none */
    return GetMOVACharFromHandset();
}
         
   
void MOVA_RX_CHAR(unsigned char cModem_char)
/*
   Called from forward_read_char in dattran.c to put character input at Modem port 
   into sMOVA_INT_ModemBuf.
*/
{
   /*   ------------------------------------------------------------   */
   /*   Put any new characters from the Modem into the rolling buffer  */
   /*   ------------------------------------------------------------   */
   
   sMOVA_ModemBuf_[nEndPtr] = cModem_char;
   if ( ++nEndPtr == 128 ) nEndPtr = 0; /* ready for next call. Wrap if neccessary */
   nBufPtr = nEndPtr;
}

char CollectModemChar(void)
{
   char cResult;
   /*   ------------------------------------------------------  */
   /*   Collect any characters from the Modem rolling buffer    */
   /*                                                           */
   /*   If nBufPtr and nStartPtr are equal, there are no new    */
   /*   chars to collect                                        */
   /*   ------------------------------------------------------  */

   cResult = 0;
   if ( nStartPtr != nBufPtr )
   {
      cResult = (char)toupper(sMOVA_ModemBuf_[nStartPtr]);
      if ( ++nStartPtr == 128 ) nStartPtr = 0;
      /* convert line feeds into carriage returns */
      if ( cResult == cLF ) cResult = cCR;
      /* if its a control character except ENTER and DELETE */
      if ( ( cResult < 32 )
        && ( cResult != cCR )
        && ( cResult != cBackSp ) )
      {
          /* then ignore it */
          cResult = 0;
      }
   }
   /* echo character back to port, except delete which is handled later */
   if ( ( cResult != 0 )
     && ( cResult != cBackSp )
     && ( cResult != cDelete ) )
   {
      SendStringToModem(&cResult, 1);
   }
   return (cResult);
}


/* IRH MOD: M5.0.0: 05/09/04: Added non-echoing CollectModemChar as in 
 * MOVA 5 we can now read in dataset remotely (albeit with more 
 * stringent checking - see Data_Handler() in Datahand module). */
char CollectModemCharNE(void)
{
   char cResult;
   /*   ------------------------------------------------------  */
   /*   Collect any characters from the Modem rolling buffer    */
   /*                                                           */
   /*   If nBufPtr and nStartPtr are equal, there are no new    */
   /*   chars to collect                                        */
   /*   ------------------------------------------------------  */

   cResult = 0;
   if ( nStartPtr != nBufPtr )
   {
      cResult = (char)toupper(sMOVA_ModemBuf_[nStartPtr]);
      if ( ++nStartPtr == 128 ) nStartPtr = 0;
      /* convert line feeds into carriage returns */
      if ( cResult == cLF ) cResult = cCR;
      /* if its a control character except ENTER and DELETE */
      if ( ( cResult < 32 )
        && ( cResult != cCR )
        && ( cResult != cBackSp ) )
      {
          /* then ignore it */
          cResult = 0;
      }
   }

   return (cResult);
}


void ResetModemBuf(void)
{
   nStartPtr = 0;
   nBufPtr = 0;
   nEndPtr = 0;
}
   
int GetMOVA_TelNo(char cMOVA_TelNo[16])
/*
   Get the MOVA telephone number and return as a character string. Last value
   in MOVA tel no is a checksum: 256 - sum of values in tel no (always > 10)
*/
{
    int nIdx;
    
    nIdx = 0;
    while (Tcomshr->tel[nIdx] < 10)
    {
       cMOVA_TelNo[nIdx] = (char)(Tcomshr->tel[nIdx] +'0');
       nIdx++;
    }
    
    if ( nIdx > 0 ) /* valid phone number found */
      lGood_MOVA_TelNo = TRUE;
    else
      lGood_MOVA_TelNo = FALSE;
   
    return (--nIdx);
}
