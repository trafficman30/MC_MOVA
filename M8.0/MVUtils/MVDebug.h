#if !defined (__MVDEBUG_H)
#define __MVDEBUG_H

/*
    Name:           MVDebug.h
    Author (Date):  ihenderson, 28/09/05
    Platform:       PC
    Purpose:        Basic debug print and assert functionality
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


#if defined (_DEBUG)
	
	#include "MVTrace.h"
	
	/* Simple assertion macros */
	#define DEBUG_ASSERT( expression )	\
		TRACE_ASSERT( (expression), NULL )
	
	#define DEBUG_ASSERT_VALID_ADDRESS( address )	\
		TRACE_ASSERT_VALID_ADDRESS( (address), NULL )

	/* Simple write string macros */
	#define DEBUG_WRITE0( msg ) \
		TRACE_WRITE0( NULL, (msg) )

	#define DEBUG_WRITE1( msg, arg1 ) \
		TRACE_WRITE1( NULL, (msg), (arg1) )

	#define DEBUG_WRITE2( msg, arg1, arg2 ) \
		TRACE_WRITE2( NULL, (msg), (arg1), (arg2) )

	#define DEBUG_WRITE3( msg, arg1, arg2, arg3 ) \
		TRACE_WRITE3( NULL, (msg), (arg1), (arg2), (arg3) )

	#define DEBUG_WRITE4( msg, arg1, arg2, arg3, arg4 ) \
		TRACE_WRITE4( NULL, (msg), (arg1), (arg2), (arg3), (arg4) )

	#define DEBUG_WRITE5( msg, arg1, arg2, arg3, arg4, arg5 ) \
		TRACE_WRITE5( NULL, (msg), (arg1), (arg2), (arg3), (arg4), (arg5) )

	#define DEBUG_WRITE6( msg, arg1, arg2, arg3, arg4, arg5, arg6 ) \
		TRACE_WRITE6( NULL, (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6) )

	#define DEBUG_WRITE7( msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7 ) \
		TRACE_WRITE7( NULL, (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6), (arg7) )

	#define DEBUG_WRITE8( msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8 ) \
		TRACE_WRITE8( NULL, (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6), (arg7), (arg8) )

	#define DEBUG_WRITE9( msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9 ) \
		TRACE_WRITE9( NULL, (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6), (arg7), (arg8), (arg9) )

	#define DEBUG_WRITE10( msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10 ) \
		TRACE_WRITE10( NULL, (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6), (arg7), (arg8), (arg9), (arg10) )

#else

	/* Define macros away to nothing */
	
	#define DEBUG_ASSERT( expr )
	#define DEBUG_ASSERT_VALID_ADDRESS( addr )
	
	#define DEBUG_WRITE0( msg )
	#define DEBUG_WRITE1( msg, arg1 )
	#define DEBUG_WRITE2( msg, arg1, arg2 )
	#define DEBUG_WRITE3( msg, arg1, arg2, arg3 )
	#define DEBUG_WRITE4( msg, arg1, arg2, arg3, arg4 )
	#define DEBUG_WRITE5( msg, arg1, arg2, arg3, arg4, arg5 )
	#define DEBUG_WRITE6( msg, arg1, arg2, arg3, arg4, arg5, arg6 )
	#define DEBUG_WRITE7( msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7 )
	#define DEBUG_WRITE8( msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8 )
	#define DEBUG_WRITE9( msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9 )
	#define DEBUG_WRITE10( msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10 )

#endif /* defined (_DEBUG) */



#if defined (__cplusplus)
	}
#endif

#endif /* __MVDEBUG_H */
