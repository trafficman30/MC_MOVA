#if defined (_TRACE)

/*
    Name:           MVTrace.c
    Author (Date):  ihenderson, 13/10/05
    Platform:       PC
    Purpose:        Basic trace assertion and writing functionality.
                    
                    Trace writes are used in both debug and release
                    builds to write trace output to a given log file.
                    Trace writes can be output or not depending on
                    their TraceLevel, e.g. "verbose", "warning" etc.
                    or always output.
                    
                    Trace asserts are also used in debug and release
                    builds.  In debug builds they cause an assertion
                    failure (fail hard); in release builds they display 
                    an internal program error message to the user allowing 
                    them to continue - at their own risk - or exit.                    
                    Trace asserts are always written to the log file.

    Last revision:  $Revision:: 5                           $
    Last changed:   $Date:: 3/04/06 10:34                   $
    Changed by:     $Author:: Ihenderson                    $

    $History: MVTrace.c $
    
    *****************  Version 5  *****************
    User: Ihenderson   Date: 3/04/06    Time: 10:34
    Updated in $/PCMOVA/MVUtils
    
    *****************  Version 4  *****************
    User: Ihenderson   Date: 9/03/06    Time: 14:16
    Updated in $/PCMOVA/MVUtils
    
    *****************  Version 3  *****************
    User: Ihenderson   Date: 16/12/05   Time: 11:10
    Updated in $/PCMOVA/MVUtils
    
    *****************  Version 2  *****************
    User: Ihenderson   Date: 5/12/05    Time: 11:14
    Updated in $/PCMOVA/MVUtils
    
    *****************  Version 1  *****************
    User: Ihenderson   Date: 28/11/05   Time: 11:53
    Created in $/PCMOVA/MVUtils
*/


/*
	TODO: Consider multithreading aspects - might be called from MOVA kernel
    error handler.

	File names and line numbers appear in release builds even when
	debug symbols are turned off.  They should be removed.

	Log files could grow too big - simple solution is to overwrite each time
	PCMOVA is run - may be best in release builds.  Otherwise need something else.
*/

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

#include "MVTrace.h"
#include "MVLog.h"
#include "MVMath.h"


static TraceLevel   g_traceLevel;
static MVBool       g_isInitialised = MV_FALSE;
static char         g_traceFileName[ g_TRACE_FILE_NAME_LEN_MAX + 1 ];


static MVSize   Trace_GetDateTimeNow(          char * );
static char *   Trace_BuildAssertMessage(   char *, const char *, const char *, 
                                            const char *, MVInt32, const char * );
static void     Trace_RaiseAssert(			const char * );
static void		Trace_Exit(					void );

#if defined (_CONSOLE)
	static void Trace_RecommendExit_CONSOLE( const char * );
#endif

#if defined (WIN32)
    static void Trace_RecommendExit_WIN32( const char * );
#endif


void TRACE_INITIALISE( const char * configFileName, const char * traceFileName )
{
    if ( !g_isInitialised )
    {
        /* Set default trace log file and error level */
        strcpy( g_traceFileName, g_TRACE_FILE_NAME_DEF );
        g_traceLevel = g_TRACE_LEVEL_DEF;
        
        /* See if there's an application config file that specifies
        a different trace log file and error level */
        if ( configFileName != NULL )
        {
		    FILE * p_configFile = fopen( configFileName, "r" );

		    if ( p_configFile != NULL )
		    {
                char    lineBuf[ 32 ];
                char *  traceLevel;

                traceLevel = fgets( lineBuf, sizeof( lineBuf ), p_configFile );
            
                /* If a particular trace level was provided, use it */
                if ( traceLevel != NULL )
                {
                    if ( strncmp( traceLevel, g_TRACE_LEVEL_ERROR_NAME, 
                        sizeof( g_TRACE_LEVEL_ERROR_NAME ) ) == 0 )
                    {
                        g_traceLevel = LEVEL_ERROR;
                    }
                    else if ( strncmp( traceLevel, g_TRACE_LEVEL_WARNING_NAME, 
                        sizeof( g_TRACE_LEVEL_WARNING_NAME ) ) == 0 )
                    {
                        g_traceLevel = LEVEL_WARNING;
                    }
                    else if ( strncmp( traceLevel, g_TRACE_LEVEL_INFO_NAME, 
                        sizeof( g_TRACE_LEVEL_INFO_NAME ) ) == 0 )
                    {
                        g_traceLevel = LEVEL_INFO;
                    }
                    else if ( strncmp( traceLevel, g_TRACE_LEVEL_VERBOSE_NAME, 
                        sizeof( g_TRACE_LEVEL_VERBOSE_NAME ) ) == 0 )
                    {
                        g_traceLevel = LEVEL_VERBOSE;
                    }
                    else if ( strncmp( traceLevel, g_TRACE_LEVEL_OFF_NAME, 
                        sizeof( g_TRACE_LEVEL_OFF_NAME ) ) == 0 )
                    {
                        g_traceLevel = LEVEL_OFF;
                    }

                    fclose( p_configFile );

                } /* If a particular trace level was provided, use it */

		    } /* if application config file could be opened */

        } /* if application config file specified */       
            
        /* If a particular trace log file name was provided, use it */
        if ( traceFileName != NULL )
        {
            MVSize traceLogNameLen = strlen( traceFileName );
            if ( traceLogNameLen <= g_TRACE_FILE_NAME_LEN_MAX )
            {                
                strncpy( g_traceFileName, traceFileName, traceLogNameLen );
                g_traceFileName[ traceLogNameLen ] = '\0';
            }

        } /* If a particular trace log file name was provided, use it */

        Log_Initialise( g_traceFileName, g_AUTOFLUSH );

        g_isInitialised = MV_TRUE;
    }
}



MVBool TRACE_IS_INITIALISED( void )
{
    return ( g_isInitialised );

} /* TRACE_IS_INITIALISED */


void TRACE_DEINITIALISE( void )
{
    if ( g_isInitialised )
	{
        Log_DeInitialise( );
        
        g_isInitialised = MV_FALSE;
    }

} /* TRACE_DEINITIALISE */


TraceLevel TRACE_GET_LEVEL( void )
{
    return g_traceLevel;

} /* TRACE_GET_LEVEL */


char * TRACE_GET_FILE_NAME( char * fileName, MVSize fileNameLenMax )
{
    MVSize traceFileNameLen = strlen( g_traceFileName );

    if ( fileNameLenMax > traceFileNameLen )
    {
        /* Copy file name including null terminator */
        strncpy( fileName, g_traceFileName, traceFileNameLen + 1 );
        return ( fileName );
    }
    else
    {
        return ( NULL );
    }

} /* TRACE_GET_FILE_NAME */


void Trace_Assert( MVBool expression, 
                  const char * expressionStr,  
                  const char * file,
                  MVInt32 line,
                  const char * message )
{
    if ( !expression )
    {
        char assertMsg[ 1024 ];

        Trace_BuildAssertMessage( assertMsg,
            g_ASSERT_MSG, expressionStr, file, line, message );

        Trace_WriteLineIf( LEVEL_OFF, NULL, assertMsg );
        Trace_RaiseAssert( assertMsg );
    }

} /* Trace_Assert */


void Trace_AssertNull( const void * address,
                      const char * addressStr,  
                      const char * file,
                      MVInt32 line,
                      const char * message )
{
    if ( address == NULL )
    {
        char assertMsg[ 1024 ];

        Trace_BuildAssertMessage( assertMsg,
            g_ASSERT_NULL_MSG, addressStr, file, line, message );

        Trace_WriteLineIf( LEVEL_OFF, NULL, assertMsg );
        Trace_RaiseAssert( assertMsg );
    }

} /* Trace_AssertNull */


static void Trace_RaiseAssert( const char * message )
{

#if defined (_DEBUG)
	/* Exit immediately - assert() calls abort()
	(should therefore only be used in debug builds) */
	UNUSED(message);
    assert( MV_FALSE );
#else    
	/* Exit gracefully - using exit(), allowing cleanup. */
    #if defined (WIN32)
        Trace_RecommendExit_WIN32( message );
    #elif defined (_CONSOLE)
        Trace_RecommendExit_CONSOLE( message );
	#else		
		Trace_Exit();
    #endif
#endif 

} /* Trace_RaiseAssert */


#if defined (WIN32)
static void Trace_RecommendExit_WIN32( const char * message )
{
    MVInt result = MessageBox( 
        NULL, 
        message, 
        g_PROGRAM_ERROR_TITLE, 
        MB_YESNO | MB_ICONERROR );

    if ( result == IDYES )
    {
        Trace_Exit();
    }

} /* Trace_RecommendExit_WIN32 */
#endif /* (WIN32) */


#if defined (_CONSOLE)
static void Trace_RecommendExit_CONSOLE( const char * message )
{
    char answer[ BUFSIZ ];

    fputs( message, stdout );
    fputs( g_PROGRAM_ERROR_TITLE, stdout );

	if ( fgets( answer, sizeof( answer ), stdin ) != NULL )
	{
		if ( toupper( answer[ 0 ] ) == 'Y' )
		{
			Trace_Exit();
		}
	}

} /* Trace_RecommendExit_CONSOLE */
#endif /* (_CONSOLE) */


static void Trace_Exit( void )
{
	Trace_WriteLineIf( LEVEL_OFF, NULL, g_EXIT_MSG );
	Log_DeInitialise();

	/* exit() calls any cleanup functions registered using atexit() */
	exit( EXIT_FAILURE );

} /* Trace_Exit */


void Trace_WriteLineIf( TraceLevel level, const char * prefix, 
                       const char * message, ... )
{
    if ( level <= g_traceLevel )
    {
        char traceBuffer[ 1024 ];
        char formatBuffer[ 1024 ];

        va_list  argList;       
        va_start( argList, message );    

        if ( prefix != NULL )
        {
            sprintf( formatBuffer, "%s: %s\r\n", prefix, message );
        }
        else
        {
            sprintf( formatBuffer, "%s\r\n", message );
        }
        vsprintf( traceBuffer, formatBuffer, argList );

        Log_Write( traceBuffer );

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

} /* Trace_WriteLineIf */


static char * Trace_BuildAssertMessage( char * msgBuffer,
                                       const char * prefix,
                                       const char * expression,
                                       const char * file,
                                       MVInt32 line,
                                       const char * message )
{   
    char dateTime[ 32 ];
    MVSize dateTimeLen = Trace_GetDateTimeNow( dateTime );

    /* Remove the new line from the end of the date/time string */
    dateTime[ dateTimeLen - 1 ] = '\0';
    
    if ( message != NULL )
    {
        sprintf( msgBuffer, 
            "%s\r\nDate and time: %s\r\nMessage: %s\r\nLocation: %s (%ld)\r\nExpression: %s",
            prefix, dateTime, message, file, line, expression );
    }
    else
    {
        sprintf( msgBuffer, 
            "%s\r\nDate and time: %s\r\nLocation: %s (%ld)\r\nExpression: %s",
            prefix, dateTime, file, line, expression );
    }

    return ( msgBuffer );

} /* Trace_BuildAssertMessage */


static MVSize Trace_GetDateTimeNow( char * dateTimeBuffer )
{
    struct tm * timeNow;
    time_t      clock;

	/* Get time in seconds */
    time( &clock );

	/* Convert time to a struct */
    timeNow = localtime( &clock );
   
    /* Print local time as a string (exactly 26 chars length) */
	sprintf( dateTimeBuffer, "%s", asctime( timeNow ) );

	return ( strlen( dateTimeBuffer ) );

} /* Trace_GetDateTimeNow */

#endif /* (_TRACE) */