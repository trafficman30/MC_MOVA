/*
    Name:           MVLog.c
    Author (Date):  ihenderson, 13/10/05
    Platform:       PC
    Purpose:        Basic log to file functionality.
*/


/* Include standard Windows headers */
#if defined (WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define MAX_PATH FILENAME_MAX
#include <pthread.h>
#include <string.h>
#endif

#include <stdio.h>
#include "MVTypes.h"

static char             g_logFileName[ MAX_PATH ];
static FILE *	        gp_logFile;
static MVBool	        g_isInitialised = MV_FALSE;
static MVBool           g_isAutoFlush;

#if defined (WIN32)
static CRITICAL_SECTION g_criticalSection;
#else
static pthread_mutex_t g_criticalSection;
#endif

void Log_Initialise( const char * logFileName, MVBool isAutoFlush )
{
	if ( !g_isInitialised )
	{		
        MVSize logNameLen = strlen( logFileName );
        if ( logNameLen < MAX_PATH )
        {                
            strncpy( g_logFileName, logFileName, logNameLen );
            g_logFileName[ logNameLen ] = '\0';

            g_isAutoFlush = isAutoFlush;
			g_isInitialised = MV_TRUE;
            
            /* Don't open log file until the first time we write to it */
            gp_logFile = NULL;
        }

        /* Initialise the critical section for each log file write (once only) */
#if defined (WIN32)
        InitializeCriticalSection( &g_criticalSection );
#else
		{
			pthread_mutexattr_t mutexattr;
			pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE_NP);
			pthread_mutex_init(&g_criticalSection, &mutexattr);
			pthread_mutexattr_destroy(&mutexattr);
		}
#endif

	}
}


void Log_DeInitialise( void )
{
	if ( g_isInitialised )
	{	
        if ( gp_logFile != NULL )
        {
		    MVInt result = fclose( gp_logFile );

            if ( result == 0 )
            {
                gp_logFile = NULL;
                g_isInitialised = MV_FALSE;
            }
        }

        /* Delete the critical section for each log file write (once only) */
#if defined (WIN32)
		DeleteCriticalSection( &g_criticalSection );
#else
		pthread_mutex_destroy(&g_criticalSection);
#endif
	}
}


MVBool Log_IsInitialised( void )
{
	return ( g_isInitialised );
}


void Log_Write( const char * message )
{
	if ( g_isInitialised )
	{
        /* START CRITICAL SECTION */
#if defined (WIN32)
        EnterCriticalSection( &g_criticalSection );
#else
		pthread_mutex_lock(&g_criticalSection);
#endif

        if ( gp_logFile == NULL )
        {
		    /* Open file for writing */
		    gp_logFile = fopen( g_logFileName, "a+" );
        }

        if ( gp_logFile != NULL )
        {
            fputs( message, gp_logFile );

		    if ( g_isAutoFlush )
		    {
			    /* Flush stream immediately */
			    fflush( gp_logFile );
		    }
        }

        /* END CRITICAL SECTION */
#if defined (WIN32)
        LeaveCriticalSection( &g_criticalSection );
#else
		pthread_mutex_unlock(&g_criticalSection);
#endif
	}
}