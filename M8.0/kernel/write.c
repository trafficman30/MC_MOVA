/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         write.c
*
*  TITLE:        Mova Messages: Thread Safe Screen/Terminal message handling.
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	26-Jan-1998							MRC		SIE_00			First undergoing system test
*	16-Feb-1998							MRC		SIE_01			First call to initialize. Return error when phone line dropped.
*	09-Mar-1998							MRC		SIE_02			CP_MESSAGE_TIMER() does NOT need to be called here
*	11-Mar-1998							MRC		SIE_03			RB_GOOD_BLK_REC was incorrectly declared
*	27-Apr-1998							PC		SIE_04			Changed format string parameter type from char** to char* and all calls to WRITE() changed as well.
*	03-May-2000							PC		SIE_05			Changed newline sequence from "\n\r" to "\r\n" to try to solve a problem with MOVAC inserting 
*																a blank line after a line of text which finishes near the right-hand edge of the screen.
*	29-Sep-2004							IRH		SIE_06			Added Safe_Strncpy() function for C strings that adds a null-terminator.WRITE() function made thread safe. Changes for MOVA 5.0 (includes Compact MOVA).
*	31-May-2011		7.0.0		AA		PK		CRN002			Updated header inline with new Software Quality Plan	
*	15-Mar-2012		7.0.3		AB		AK		CRN009			Changes to conditional compilation flags
*	09-Jan-2013		7.0.5		..		AK		CRN020			Print leading zeros on dates and times
*	31-Jan-2013		7.0.5		AC		AK		M7.0.5			Issue M7.0.5
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

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h> 
#include "gbltypes.h"
#include "write.h"
#include <diff_bss.h>


/* IRH MOD: 17/03/06: PCMOVA: BUGFIX: 10067.
   Make the WRITE() routine thread-safe.

   It was possible for two threads to call WRITE simultaneously, 
   and some WRITE() data needs to be retained between function calls to 
   WRITE() - e.g. WRITE() will be called a number of times to buffer 
   a line's worth of confirm or detector states to be printed to the 
   commissioning screen in one go.  So serialising WRITE() with 
   critical sections was not enough, as you might get one thread 
   calling WRITE() during a sequence of what should be consecutive 
   WRITE()s by another thread. The solution was therefore to use 
   thread local storage for all static variables (local and global) 
   used by the Write.c module. This approach works because SendStringToLH()
   is serialised in PCMOVA, and we're using multithreaded versions of the
   standard C libraries (although I think that all C library functions below 
   are reentrant anyway). I've run PCMOVA for over 24 hours outputting 
   messages and commissioning screen simultaneously with no problem. 

   Something similar should also be done for embedded MOVA,
   although the probability of failure is much lower.
   In PCMOVA a crash happens consistently when MOVA messages are 
   being output at the same time as the MOVA commissioning screen 
   - the Output thread runs at simulation time, not real-time, 
   making the probability of failure much higher in PCMOVA. 
   
   Note that in future we want to output MOVA messages and the 
   commissioning screen at the same time, for which WRITE() will
   definitely need to be thread-safe in the embedded version. 
   
   Also note that Send_FormatString() in GetEntry.c is not thread-safe - 
   it has a static buffer.  This should probably be made thread-safe too
   just in case it's use by other threads in future.
   
   N.B. WRITE(INITIALIZE) should be called by each thread before using
   this routine. */
#include "mova.h" /* For PC define */
#if defined (PC_WRAPPER) && !defined (_PLUTO)
    #define ThreadVar  __declspec( thread )
#else   
    #define ThreadVar
#endif

#undef toascii
/* IRH MOD: M5.0.0: 06/08/04: Added, as it's non-ANSI */
#define toascii(c)  ((c)&0177)

#define GO 0                  /* Send out string produced from processing WRITE */
#define BASE10 10             /* Number base for character to integer conversion */
#define ZERO 48               /* ASCII number for character '0' */
/* IRH MOD: M5.0.0: 06/08/04: Just in case... */
#define OPSTRSIZE 2048/*1024*/        /* max number of chars allowed in output string */


/* extern functions */
extern void SendStringToLH(const char *sOP_String, int nCharsInString);
extern void SendStringToModem(const char *sOP_String, int nCharsInString);
// SLM FIXME - this is not defined anywhere and is also commented out in the code below
// we should remove both references completely
//extern void CP_MESSAGE_TIMER(void);

/* extern variables */
extern unsigned char RB_GOOD_BLK_REC;

int WRITE(int param1, ...);
char *FORMAT(char *fp, va_list *ap, int *vartype, char *op_str,
               int *holdcursor, int *err);
char *W_itoa(int num, char str[]);
static ThreadVar int num;           /* multiples of current field */
static ThreadVar int reps;          /* number of repetitions of fields in brackets */
static ThreadVar int bracket_count; /* count of number of '(' encountered */

int WRITE(int param1, ...)
{
   va_list ap;                    /* pointer to variable length argument list */
   int operation;                 /* WRITE operation 0 or MORE */
   int vartype;                   /* variable types in WRITE statement */
   static ThreadVar int err;                /* error status (0 or 1) from processing WRITE */
   static ThreadVar int fmt_set;            /* 0:no format string set, 1:format string set */
   static ThreadVar int device;             /* WRITE output device */
   static ThreadVar int holdcursor;         /* indicates no newline at end of output string */
   static ThreadVar char op_str[OPSTRSIZE]; /* o/p string formed from WRITE statement */
   static ThreadVar char *fmt_str;          /* WRITE format string */
   static ThreadVar char *fp;               /* format string pointer, holds current position */
   extern unsigned char lLineDropped;  /* Return error if line has dropped */
   extern unsigned char lLH_CONNECTED; /* Handset unplugged? */
   
   if (param1!=GO && param1!=MORE && param1!=INITIALIZE) {   /* if not WRITE(0) or WRITE(MORE) 
                                                                 or not initialise */
      va_start(ap, param1);       /* initiate variable length argument pointer */
      if (!fmt_set) {             /* if format statement not already set */
         device = param1;         /* get output device */
         va_arg(ap, int); va_arg(ap, int *); /* skip four unused parameters */
         va_arg(ap, int); va_arg(ap, int);
         fmt_str = va_arg(ap, char *);     /* get format string */
         fp = fmt_str;            /* set pointer to start of string */
         fmt_set = 1;             /* set fmt_set flag */
         err = 0;                 /* clear error flag */
         strcpy(op_str,"");       /* clear output string */
                     num = 0;
                     bracket_count = 0;
         holdcursor = 0;          /* new line at end of output string */
         va_arg(ap, int);         /* skip unused parameters */
         vartype = va_arg(ap, int);   /* get first variable type */
      }
      else {
         vartype = param1;        /* if format set, param1 is variable type */
      }

      operation = vartype;        /* loop until all parameters in WRITE used */
      do {

		  fp = FORMAT(fp, &ap, &vartype, op_str, &holdcursor, &err);

             if (vartype!=GO && vartype!=MORE)  /* if not at end of fmt string */
                vartype = va_arg(ap, int);            /* get next variable type */
             else                                     /* else quit loop */
                operation = vartype;
      } while (operation!=GO && operation!=MORE);
      va_end(ap);       /* clean up variable parameter list processing */

   }
   else {               /* assign GO or MORE to operation */
      operation = param1;
   }

   if (operation==GO) {     /* send output string ? */
      if (!holdcursor) strcat(op_str, "\r\n");  /* newline at end of output */
      fmt_set = 0;                            /* clear format set flag */
      /*printf("%s",op_str);  *//* send o/p string to device */
      /* ** REPLACE PRINTF WITH CALL TO SEND OUTPUT STRING TO DEVICE ** */
      /* IRH MOD: M5.0.0: 30/09/04: Added checks that we are still connected before
       * actually sending a string. */
      if ( device == LOCAL_HANDSET && lLH_CONNECTED )
         SendStringToLH(op_str, (int)strlen(op_str));
      else if ( device == MODEM_PORT && !lLineDropped )
      {
         SendStringToModem(op_str, (int)strlen(op_str));
         /* Force OMU system to think incoming block 
         received to keep message timer from timing out */
         RB_GOOD_BLK_REC = TRUE;
/*         CP_MESSAGE_TIMER(); */
      }
  }
   else if (operation == INITIALIZE)
   {
      fmt_set = 0;             /* clear fmt_set flag on warm start */
   }
   
   /* check for line dropped or terminal unplugged */
   if ( err == 0 )
   {
      if ( lLineDropped && device == MODEM_PORT )
         err = TRUE;
      else if ( device != MODEM_PORT && lLH_CONNECTED == 0 )
         err = TRUE;
   }
   return err;                 /* return 0:no error or 1:error in o/p string */
}


char *FORMAT(char *fp, va_list *ap, int *vartype, char *op, int *holdcursor, int *err)
/* Steps through format string until either one variable has been processed */
/* and the next encountered, or until format string has been exhausted.     */
/* Concatenates output to end of output string *op according to format              */
/* specified in format string pointed to by *fp.                          */
/* Modifies pointer to variable argument list as WRITE parameters are used  */
/* Sets flag *holdcursor if newline not required at end of output string.   */
/* Sets flag *err if an error occurs in processing variables.                       */
/* Returns pointer to current position in format string.                  */
{
	int ij;
	int leading_zero;	/* Pad with leading zeros */
	int width;             /* column width for numeric justification */
	int precision;     /* number of decimal places in floating point output */
							/* or min. number of digit places in integer output */
	int var_done;      /* no variable processed on this call */
	int quit;              /* 0:don't quit, 1:quit routine */
	int i;       /* loop counter */
	int nStrlen;           /* length of string pointed to by *nStrvar */
	short sval;            /* variable to output of type short integer */
	int ival;              /* variable to output of type standard integer */
	long lval;             /* variable to output of type long integer */
	double fval;           /* variable to output of type floating point */
	static ThreadVar char *var_start;     /* pointer to start of i/f in format string */
	static ThreadVar char *bracket_start; /* pointer to start of (..) in format string */
/*   char *nStrvar;    */  /* pointer to string variable to add to o/p string */
	/* use union to reserve space as malloc seems to cause trouble */

	/* IRH MOD: M5.0.0: 01/09/04: Size changed from 150 to 256 
	 * (shouldn't be larger than sout, below, as we copy from this into sout)
	 * Made this static, as I'm paranoid about overflowing the small
	 * (currently 4k) stack for MOVA threads. */   
	static ThreadVar union ip_string
	{
		char sStrvar[256];
		char *pStrvar;
	} ip_Strn;
   
	/* IRH MOD: M5.0.0: 01/09/04: Size changed from 100 to 256 - 
	 * enough to accommodate the Terrlog->dbzstr (which can be up 
	 * to 106 chars long for DBZ error messages 401 and 402 in blockc() ) 
	 * Made this static, as I'm paranoid about overflowing the small
	 * (currently 4k) stack for MOVA threads. */   
	static ThreadVar char sout[256];    /* sprintf output string */
   
	/* IRH MOD: M5.0.0: 01/09/04: Size changed from 100 to 32 
	 * (format specifier doesn't need to be that big) */
	char sfmt[32];    /* sprintf format string */
	char conv[20];     /* string used for integer to string conversion */

	width = precision = var_done = quit = leading_zero = 0;   /* initialise local variables */
	/*nStrvar = malloc(100 * sizeof(char)); */      /* reserve string space */

	while(!quit && *vartype!=MORE)
	{
		switch(*fp) 
		{

		case '(' :                 /* "(" encountered in format string */
			++fp;                             /* increment format string pointer */
			if (++bracket_count==2)
			{     /* second '(' encountered ? */
				bracket_start = fp;            /* record position of next char */
				reps = num;          /* record number of times to repeat */
				if (reps < 1) reps = 1;    /* contents of (...) */
				num = 0;
			}
			break;

		case ')' :                 /* ")" encountered in format string */
			if (bracket_count > 1) 
			{      /* if not outer-most brackets */
				if (--reps > 0)                /* if more repetitions to do */
					fp = bracket_start;     /* set pointer to start of brackets */
				else 
				{
					--bracket_count;        /* all reps done, decrement count */
					++fp;                       /* move to next char after ')' */
					reps = 0;         /* clear reps counter */
				}
			}
			else
			{                            /* outer-most bracket */
				bracket_count = 0;
				quit = 1;                      /* quit this function */
			}
			break;

		case '\'' :                    /* "'" encountered in format string */
			while(*++fp != '\'')                /* loop until closing ' encountered */
				strncat(op,fp,1);                /* add all chars between '...' to */
			++fp;                               /* output string */
			break;

		case '\\' :                    /* "\" encountered in format string */
			*holdcursor = 1;          /* set flag holdcursor */
			++fp;
			break;

		case '/' :                     /* "/" encountered in format string */
			strcat(op,"\r\n");        /* add CRLF to o/p string */
			++fp;
			break;
                                             /* digit encountered in format string */
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			num = toascii(*fp)-ZERO;    /* record all adjacent digits and */
			while(isdigit(*++fp))             /* convert to an integer */
				num = (num*BASE10) + toascii(*fp)-ZERO;
			break;

		case 'x' :                   /* "x" encountered in format string */
			for (i=0; i<num; i++)             /* add number of spaces to output */
				strcat(op," ");      /* string indicated by preceding */
			num = 0;                /* string of digits */
			++fp;
			break;

		case 't' :                   /* "tr" encountered in format string */
			if (*++fp == 'r' )
			{
				while(isdigit(*++fp))    /* convert following digits to int */
					num = (num*BASE10) + toascii(*fp)-ZERO;
				for (i=0; i<num; i++)    /* add number of tabs to output */
					strcat(op," ");             /* string indicated by int */
				num = 0;
			}
			break;

		case 'a' :                  /* "a" encountered in format string */
			if (!var_done) 
			{       /* var not already done in this call? */
				if (num < 1) num = 1;
				memset(sout,'\0',sizeof(sout));
				memset(ip_Strn.sStrvar,'\0',sizeof(ip_Strn.sStrvar));
				ip_Strn.pStrvar = va_arg(*ap, char *); /* get string parameter */
				nStrlen = va_arg(*ap, int);           /* get string length parameter */
				strcpy(sfmt, "%");
               
				/* IRH MOD: M5.0.0: 10/08/04: If a string length of 0 is specified
				 * then don't use a width specifier in the format string. */
               
				/* If a non-zero string length was specified */
				if ( nStrlen != 0 )
				{
					/* Add a width specifier */
					strcat(sfmt, W_itoa(nStrlen,conv));    
				}
                              
				strcat(sfmt, "s");
				if (sprintf(sout, sfmt, ip_Strn.pStrvar) < 0) *err = 1 ;
				strcat(op, sout);             /* add string to end of output string */
				if (--num <= 0)               /* more copies of current field to do? */
					while(*fp!=',' && *fp!=')')
						++fp;       /* move fp to end of field */
				var_done = 1;       /* flag indicates variable processed */
			}                                /* during this call to FORMAT */
			else
				quit = 1;            /* variable already processed, quit function */
			break;

		case 'i' : case 'f' : /* "i" or "f" encountered in format string */
			if (!var_done)
			{       /* var not already done in this call? */
				var_start = fp;         /* record start position of i/f format */
				if (num < 1) num = 1;
				while(isdigit(*++fp))     /* read column width required */
				{
					width = (width*BASE10) + toascii(*fp)-ZERO;
					if (width == 0)
						leading_zero = 1;
				}
				if (*fp=='.')
				while(isdigit(*++fp))  /* read precision required */
					precision = (precision*BASE10) + toascii(*fp)-ZERO;

				strcpy(sfmt, "%");
				if (width > 0)
				{
					if (leading_zero)
						strcat(sfmt, "0");
					strcat(sfmt, W_itoa(width,conv));
				}
				if (precision > 0)
				{
					strcat(sfmt, ".");
					strcat(sfmt, W_itoa(precision,conv));
				}

				ij = *vartype;        /* so that it always gets REAL4 vars: strange but true */
				switch(ij) 
				{    /* process variable according to type */
					case MOVA_BYTE :            /* short integer : */
						strcat(sfmt, "hd");
#if defined(_PLUTO)
						sval = (char) va_arg(*ap, int);  /* get var from parameter list */
#else
						sval = va_arg(*ap, char);  /* get var from parameter list */
#endif
						if (sprintf(sout, sfmt, sval) < 0) *err = 1;  /* process */
						break;
					case INT2 :            /* standard integer : */
						strcat(sfmt, "d");
						ival = va_arg(*ap, int);    /* get var from parameter list */
						if (sprintf(sout, sfmt, ival) < 0) *err = 1;  /* process */
						break;
					case INT4 :            /* long integer : */
						strcat(sfmt, "ld");
						lval = va_arg(*ap, long);   /* get var from parameter list */
						if (sprintf(sout, sfmt, lval) < 0) *err = 1;  /* process */
						break;
					case REAL4 :           /* floating point number : */
						strcat(sfmt, "f");
						fval = va_arg(*ap, double); /* get var from parameter list */
						if (sprintf(sout, sfmt, fval) < 0) *err = 1;  /* process */
						break;
				}

				strcat(op, sout);      /* add process number to output string */
				if (--num > 0)         /* more copies of current field to do? */
					fp = var_start;     /* return pointer to start of field */
				else
					num = 0;        /* clear no. of copies of current field */
				var_done = 1;       /* flag indicates variable processed */
			}              /* during this call to FORMAT */
			else
				quit = 1;         /* variable already processed, quit function */
			break;

		default :            /* unrecognised character, move to next */
			++fp;                  /* character in format string */
			break;
		}
	}

	return fp;                       /* return updated position of format pointer */
}


char *W_itoa(int numb, char str[])
/* converts the digits of the integer numb into a null-terminated string */
/* and stores the result in str. Returns pointer to str. Base 10 assumed */
{
	int i, j, sign;
	char c;

	if ((sign = numb) < 0)   /* record sign */
		numb = -numb;      /* make n positive */

	i = 0;
	do 
	{                /* generate digits in reverse order */
		str[i++] = numb % 10 + '0';    /* get next digit */
	} while ((numb /= 10) > 0);     /* delete it */

	if (sign < 0)       /* re-instate sign */
		str[i++] = '-';
	str[i] = '\0';      /* attach string terminator */

						/* reverse order of characters in string */
	for (i = 0, j = (int)strlen(str)-1; i < j; i++, j--)
	{
		c = str[i];
		str[i] = str[j];
		str[j] = c;
	}

	return(str);
}


