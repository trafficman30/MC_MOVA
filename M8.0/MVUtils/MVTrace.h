#if !defined (__MVTRACE_H)
#define __MVTRACE_H

/*
    Name:           MVTrace.h
    Author (Date):  ihenderson, 16/11/05
    Platform:       PC
    Purpose:        Basic trace print and assert functionality
                    Note that the C preprocessor that 
                    comes with the Microsoft C compiler currently
                    doesn't support variadic macros - we
                    can't use macros with variable numbers
                    of arguments (as we could with GCC).
                    This requires a macro for each debug print 
                    that has a different number of arguments -
                    safer for portability anyway.
					Note: _TRACE should be defined for both
					debug and release builds.
					_DEBUG should be defined just for debug builds.
*/


/*
	TODO:
*/

#if defined (__cplusplus)
	extern "C" {
#endif

#if defined (_TRACE)

	#include "MVTypes.h"

	typedef enum
	{
		LEVEL_OFF,
		LEVEL_ERROR,
		LEVEL_WARNING,
		LEVEL_INFO,
		LEVEL_VERBOSE
	} TraceLevel;

	void        TRACE_INITIALISE(		const char *, const char * );
	MVBool      TRACE_IS_INITIALISED(	void );
	void        TRACE_DEINITIALISE(		void );

	TraceLevel  TRACE_GET_LEVEL(		void );
	char *      TRACE_GET_FILE_NAME(	char *, MVSize );

	/* Do not call the trace write function directly - use the macros below */
	void Trace_WriteLineIf(		TraceLevel, const char *, const char *, ... );

	/* Do not call the trace assertion functions directly - use the macros below */
	void Trace_Assert(			MVBool, const char *, const char *,
								MVInt32, const char * );
	void Trace_AssertNull(		const void *, const char *, const char *,
								MVInt32, const char * );

	/* Trace Assert macros (always require a message) */
	#define TRACE_ASSERT( expression, message ) \
		Trace_Assert( (MVBool)(expression), #expression, __FILE__, __LINE__, (message) )

	#define TRACE_ASSERT_VALID_ADDRESS( address, message ) \
		Trace_AssertNull( (address), #address, __FILE__, __LINE__, (message) )

	/*	Trace Write macros with a conditional trace level
		(traces always require a prefix, e.g. the module name) */
	#define TRACE_WRITE0_IF( level, prefix, msg ) \
		Trace_WriteLineIf( (level), (prefix), (msg) )

	#define TRACE_WRITE1_IF( level, prefix, msg, arg1 ) \
		Trace_WriteLineIf( (level), (prefix), (msg), (arg1) )
	
	#define TRACE_WRITE2_IF( level, prefix, msg, arg1, arg2 ) \
		Trace_WriteLineIf( (level), (prefix), (msg), (arg1), (arg2) )

	#define TRACE_WRITE3_IF( level, prefix, msg, arg1, arg2, arg3 ) \
		Trace_WriteLineIf( (level), (prefix), (msg), (arg1), (arg2), (arg3) )

	#define TRACE_WRITE4_IF( level, prefix, msg, arg1, arg2, arg3, arg4 ) \
		Trace_WriteLineIf( (level), (prefix), (msg), (arg1), (arg2), (arg3), (arg4) )

	#define TRACE_WRITE5_IF( level, prefix, msg, arg1, arg2, arg3, arg4, arg5 ) \
		Trace_WriteLineIf( (level), (prefix), (msg), (arg1), (arg2), (arg3), (arg4), (arg5) )

	#define TRACE_WRITE6_IF( level, prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6 ) \
		Trace_WriteLineIf( (level), (prefix), (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6) )

	#define TRACE_WRITE7_IF( level, prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7 ) \
		Trace_WriteLineIf( (level), (prefix), (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6), (arg7) )

	#define TRACE_WRITE8_IF( level, prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8 ) \
		Trace_WriteLineIf( (level), (prefix), (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6), (arg7), (arg8) )

	#define TRACE_WRITE9_IF( level, prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9 ) \
		Trace_WriteLineIf( (level), (prefix), (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6), (arg7), (arg8), (arg9) )

	#define TRACE_WRITE10_IF( level, prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10 ) \
		Trace_WriteLineIf( (level), (prefix), (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6), (arg7), (arg8), (arg9), (arg10) )

	/*	Trace Write macros - always output
		(traces always require a prefix, e.g. the module name) */
	#define TRACE_WRITE0( prefix, msg ) \
		Trace_WriteLineIf( LEVEL_OFF, (prefix), (msg) )

	#define TRACE_WRITE1( prefix, msg, arg1 ) \
		Trace_WriteLineIf( LEVEL_OFF, (prefix), (msg), (arg1) )
	
	#define TRACE_WRITE2( prefix, msg, arg1, arg2 ) \
		Trace_WriteLineIf( LEVEL_OFF, (prefix), (msg), (arg1), (arg2) )

	#define TRACE_WRITE3( prefix, msg, arg1, arg2, arg3 ) \
		Trace_WriteLineIf( LEVEL_OFF, (prefix), (msg), (arg1), (arg2), (arg3) )

	#define TRACE_WRITE4( prefix, msg, arg1, arg2, arg3, arg4 ) \
		Trace_WriteLineIf( LEVEL_OFF, (prefix), (msg), (arg1), (arg2), (arg3), (arg4) )

	#define TRACE_WRITE5( prefix, msg, arg1, arg2, arg3, arg4, arg5 ) \
		Trace_WriteLineIf( LEVEL_OFF, (prefix), (msg), (arg1), (arg2), (arg3), (arg4), (arg5) )

	#define TRACE_WRITE6( prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6 ) \
		Trace_WriteLineIf( LEVEL_OFF, (prefix), (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6) )

	#define TRACE_WRITE7( prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7 ) \
		Trace_WriteLineIf( LEVEL_OFF, (prefix), (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6), (arg7) )

	#define TRACE_WRITE8( prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8 ) \
		Trace_WriteLineIf( LEVEL_OFF, (prefix), (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6), (arg7), (arg8) )

	#define TRACE_WRITE9( prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9 ) \
		Trace_WriteLineIf( LEVEL_OFF, (prefix), (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6), (arg7), (arg8), (arg9) )

	#define TRACE_WRITE10( prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10 ) \
		Trace_WriteLineIf( LEVEL_OFF, (prefix), (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6), (arg7), (arg8), (arg9), (arg10) )

#define g_AUTOFLUSH         (MV_TRUE)
#define g_ASSERT_MSG        ("Programming error detected (assertion failure)")
#define g_ASSERT_NULL_MSG	("Programming error detected (assertion failure - invalid address)")
#define g_PROGRAM_ERROR_TITLE ("Programming error. Exit (recommended)?")
#define g_EXIT_MSG         ("Program exited following assertion failure.")

#define g_TRACE_FILE_NAME_DEF       ("trace.log")
#define g_TRACE_FILE_NAME_LEN_MAX   (31)

#define g_TRACE_LEVEL_DEF           (LEVEL_ERROR)

#define g_TRACE_LEVEL_ERROR_NAME    ("TRACE_ERROR")
#define g_TRACE_LEVEL_WARNING_NAME  ("TRACE_WARNING")
#define g_TRACE_LEVEL_INFO_NAME     ("TRACE_INFO")
#define g_TRACE_LEVEL_VERBOSE_NAME  ("TRACE_VERBOSE")
#define g_TRACE_LEVEL_OFF_NAME      ("TRACE_OFF")

#else /* not TRACE */

	#define TRACE_INITIALISE(		level, log_file_name )
	#define TRACE_IS_INITIALISED(		) (0)
	#define TRACE_DEINITIALISE(			)

	#define TRACE_GET_LEVEL(			) (0)
	#define TRACE_GET_FILE_NAME(	file_name, file_name_len ) (NULL)

	/* Define trace assert macros away to nothing */
	#define TRACE_ASSERT( expression, message )
	#define TRACE_ASSERT_VALID_ADDRESS( address, message )

	/* Define trace writes macros away to nothing */
	#define TRACE_WRITE0_IF( level, prefix, msg )
	#define TRACE_WRITE1_IF( level, prefix, msg, arg1 )
	#define TRACE_WRITE2_IF( level, prefix, msg, arg1, arg2 )
	#define TRACE_WRITE3_IF( level, prefix, msg, arg1, arg2, arg3 )
	#define TRACE_WRITE4_IF( level, prefix, msg, arg1, arg2, arg3, arg4 )
	#define TRACE_WRITE5_IF( level, prefix, msg, arg1, arg2, arg3, arg4, arg5 )
	#define TRACE_WRITE6_IF( level, prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6 )
	#define TRACE_WRITE7_IF( level, prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7 )
	#define TRACE_WRITE8_IF( level, prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8 )
	#define TRACE_WRITE9_IF( level, prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9 )
	#define TRACE_WRITE10_IF( level, prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10 )

	#define TRACE_WRITE0( prefix, msg )
	#define TRACE_WRITE1( prefix, msg, arg1 )
	#define TRACE_WRITE2( prefix, msg, arg1, arg2 )
	#define TRACE_WRITE3( prefix, msg, arg1, arg2, arg3 )
	#define TRACE_WRITE4( prefix, msg, arg1, arg2, arg3, arg4 )
	#define TRACE_WRITE5( prefix, msg, arg1, arg2, arg3, arg4, arg5 )
	#define TRACE_WRITE6( prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6 )
	#define TRACE_WRITE7( prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7 )
	#define TRACE_WRITE8( prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8 )
	#define TRACE_WRITE9( prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9 )
	#define TRACE_WRITE10( prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10 )

#endif /* not TRACE */


#if defined (__cplusplus)
	}
#endif

#endif /* __MVTRACE_H */