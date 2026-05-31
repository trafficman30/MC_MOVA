

/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         kdebug.c
*
*  TITLE:        MOVA Kernel: Kernel debug logging facilities
*
*  HISTORY:
*   Date            Version     Issue   Intls   Reference       Description
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
#if defined (TRL_HOST_DEBUG)

#include <assert.h>
#include <stdarg.h> /* For variable arg macros */
#include <stdio.h>  /* For vsprintf */
#include <string.h> /* For strlen, strcpy */
#include <time.h>   /* For getting system time */
#include <stdlib.h> /* For exit */


#if defined (WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

#include "kdebug.h"
#include "MVLog.h" 




#define g_DEBUGAUTOFLUSH         (TRUE)
#define g_DEBUG_FILE_NAME_LEN_MAX   (31)

static BOOL		g_isInitialised = FALSE;
static char		g_traceFileName[ g_DEBUG_FILE_NAME_LEN_MAX + 1 ];

char str_output[300];

#if defined (WIN32)
static CRITICAL_SECTION g_criticalSection;
#endif

static FILE *	gp_logFile;

#define LARGE_LINE 1024

/* Print buffers to use prior to output */
static char traceBuffer[ LARGE_LINE ];
static char formatBuffer[ LARGE_LINE ];

void KDebugInitialise( void )
{
	if	( !g_isInitialised )
	{
		strcpy( g_traceFileName, MODULE_DEBUG_OUTPUT_FILE_NAME_ );
		g_isInitialised = TRUE;
		
		/* Don't open log file until the first time we write to it */
		gp_logFile = NULL;
		
		InitializeCriticalSection( &g_criticalSection );
	}
}

void KDebugDeinitialise( void )

{
	if ( g_isInitialised )
	{
		if ( gp_logFile != NULL )
		{
			int result = fclose( gp_logFile );
			
			if ( result == 0 )
			{
				gp_logFile = NULL;
				g_isInitialised = MV_FALSE;
			}
		}
		
		/* Delete the critical section for each log file write (once only) */
		DeleteCriticalSection( &g_criticalSection );
	}

}



void DebugLogWrite( const char * message )
{
	if ( g_isInitialised )
	{
		/* START CRITICAL SECTION */
		EnterCriticalSection( &g_criticalSection );
		
		if ( gp_logFile == NULL )
		{
			/* Open file for writing */
			gp_logFile = fopen( g_traceFileName, FILE_MODE );
		}

		if ( gp_logFile != NULL )
		{
			int result = fputs( message, gp_logFile );
			
			/* Flush stream immediately */
			result = fflush( gp_logFile );
		}
		
		/* END CRITICAL SECTION */
		LeaveCriticalSection( &g_criticalSection );
	}
}

void KDebugPrint( KDebugLevel level, const char * prefix, const char * message, ... )
{

	KDebugLevel currentLevel = DEBUG_LEVEL_OFF;
	const int NoOfModules = sizeof(moduleNames)/sizeof(char*);
	const int NoOfLevels = sizeof(moduleLevels)/sizeof(KDebugLevel);
	int mindex = 0;
	
	/* Find level for module */
	for ( mindex = 0; mindex < NoOfModules; mindex++)
	{
		if (moduleNames[mindex] == prefix)
		{ /*Found module write corresponding level */
			if (mindex > NoOfLevels)
			{ /* We do not have a debugging level for this module */
				break;
			}
			currentLevel = moduleLevels[mindex];
			break;
		}
	}

	/* If module is not in list then default is DEBUG_LEVEL_OFF */

	if (( level <= currentLevel ) && (currentLevel != DEBUG_LEVEL_OFF))
	{
		va_list argList;
		va_start( argList, message );
		
		sprintf( formatBuffer, "%s: %s\r\n", prefix, message );
		vsprintf( traceBuffer, formatBuffer, argList );
		DebugLogWrite( traceBuffer );

#if defined (_DEBUG)
	#if defined	(WIN32)
		OutputDebugString( traceBuffer );
	#endif

	#if defined (_CONSOLE)
		fputs( traceBuffer, stdout );
	#else
		fputs( traceBuffer, stderr );
	#endif
#endif
		va_end( argList );
	}

}

void KDebugNullPointerCheck( const void * module,
							 const void * address,
							 const char * addressStr)
{
	if ( address == NULL )
	{
		sprintf( formatBuffer, "%s:  Failed null pointer check for %s\r\n", (char *)module, addressStr );
		DebugLogWrite( formatBuffer );
#if defined (_DEBUG)
	#if defined	(WIN32)
		OutputDebugString( formatBuffer );
	#endif

	#if defined (_CONSOLE)
		fputs( formatBuffer, stdout );
	#else
		fputs( formatBuffer, stderr );
	#endif
#endif
	}
}

void KDebugLogError( const void * module,
					 BOOL expression, 
					 const char * expressionStr,
					 int lineNo)
{
	if ( !expression )
	{
		sprintf( formatBuffer, "%s: failed line = %d expression: (%s) \r\n", (char *)module, lineNo, expressionStr);
		DebugLogWrite( formatBuffer );
#if defined (_DEBUG)
	#if defined	(WIN32)
		OutputDebugString( formatBuffer );
	#endif

	#if defined (_CONSOLE)
		fputs( formatBuffer, stdout );
	#else
		fputs( formatBuffer, stderr );
	#endif
#endif
	}

}

char * KDebugToString(int arraySize, int * intArray)
{
	int i; /* loop variable */

	char intAsArrayOfChar[10];

	strcpy(str_output,"");

	for(i=0; i<arraySize; i++) 
	{
		_itoa(intArray[i],intAsArrayOfChar, 10);

		strcat(str_output, intAsArrayOfChar);

		if(i<arraySize-1)
			strcat(str_output, ",");
		else
			strcat(str_output, "\0");
	}

	return str_output;
}

#endif /* (TRL_HOST_DEBUG) */