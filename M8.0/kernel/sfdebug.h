/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         sfprv.h
*
*  TITLE:		Basic debug functionality for the MovaSatFlow module.
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	10-Feb-2003		6.0.0		..		IH		......			Initial implementation for testing with MOVA
*	31-May-2011		7.0.0		AA		PK		CRN003			New Header file inline with MOVA Kernel SQP
*	15-Mar-2012		7.0.3		AB		AK		CRN009			Changes to conditional compilation flags
*	30-Apr-2013		7.0.6		..		PA		CRN030			Modify debug output method
*	22-May-2013		7.0.6		AC		AK		M7.0.6			Issue M7.0.6
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

#if !defined (DEBUG_H)
#define DEBUG_H


#if defined (_DEBUG)

	#include <stdio.h>
	#include <assert.h>

	/* Simple assertion macros */

	#define DEBUG_ASSERT( expr )				assert( expr )
	#define DEBUG_ASSERT_VALID_ADDRESS( addr )	DEBUG_ASSERT( (addr) != NULL )

	/* Simple print string macros */

	#define DEBUG_STRING_LEN					(256)
	#define DEBUG_STRING_PREFIX					("SF: ")

	/* Determine an output function to use */
#if defined (PC_WRAPPER)

	#define PRINTF( str )										\
	{															\
        fputs( DEBUG_STRING_PREFIX, stderr );                   \
		fputs( str, stderr );                                   \
	}

#else

    /* Embedded MOVA - use Siemens Gemini debug output function. */
    #include <cyg/infra/diag.h>

	#define PRINTF( str )											\
	{																\
		diag_printf( DEBUG_STRING_PREFIX, stdout );					\
		diag_printf( (str), stdout );								\
	}

#endif

	#define DEBUG_PRINT0( str )										\
	{																\
		PRINTF( (str) );											\
	}

	#define DEBUG_PRINT1( str, arg1 )								\
	{																\
		char szBuffer[ DEBUG_STRING_LEN ];							\
		sprintf( szBuffer, (str), (arg1) );							\
		PRINTF( szBuffer );											\
	}

	#define DEBUG_PRINT2( str, arg1, arg2 )							\
	{																\
		char szBuffer[ DEBUG_STRING_LEN ];							\
		sprintf( szBuffer, (str), (arg1), (arg2) );					\
		PRINTF( szBuffer );											\
	}

	#define DEBUG_PRINT3( str, arg1, arg2, arg3 )					\
	{																\
		char szBuffer[ DEBUG_STRING_LEN ];							\
		sprintf( szBuffer, (str), (arg1), (arg2), (arg3) );			\
		PRINTF( szBuffer );											\
	}

	#define DEBUG_PRINT4( str, arg1, arg2, arg3, arg4 )				\
	{																\
		char szBuffer[ DEBUG_STRING_LEN ];							\
		sprintf( szBuffer, (str), (arg1), (arg2), (arg3), (arg4) );	\
		PRINTF( szBuffer );											\
	}

#ifndef _STATSA
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
#endif

#else

	/* Define macros away to nothing */
	
	#define DEBUG_ASSERT( expr )
	#define DEBUG_ASSERT_VALID_ADDRESS( addr )
	
	#define DEBUG_PRINT0( str )
	#define DEBUG_PRINT1( str, arg1 )
	#define DEBUG_PRINT2( str, arg1, arg2 )
	#define DEBUG_PRINT3( str, arg1, arg2, arg3 )
	#define DEBUG_PRINT4( str, arg1, arg2, arg3, arg4 )


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

#endif /* defined (_DEBUG) && defined (PC) */


/* Additional logging for analysis of stats */
#if defined (_STATSA) && defined (_DEBUG) && defined (PC_WRAPPER)

#include "MVDebug.h"

	#define STATS_WRITE0( msg ) \
		DEBUG_WRITE0( (msg) )

	#define STATS_WRITE1( msg, arg1 ) \
		DEBUG_WRITE1( (msg), (arg1) )

	#define STATS_WRITE2( msg, arg1, arg2 ) \
		DEBUG_WRITE2( (msg), (arg1), (arg2) )

	#define STATS_WRITE3( msg, arg1, arg2, arg3 ) \
		DEBUG_WRITE3( (msg), (arg1), (arg2), (arg3) )

	#define STATS_WRITE4( msg, arg1, arg2, arg3, arg4 ) \
		DEBUG_WRITE4( (msg), (arg1), (arg2), (arg3), (arg4) )

	#define STATS_WRITE5( msg, arg1, arg2, arg3, arg4, arg5 ) \
		DEBUG_WRITE5( (msg), (arg1), (arg2), (arg3), (arg4), (arg5) )
	
	#define STATS_WRITE6( msg, arg1, arg2, arg3, arg4, arg5, arg6 ) \
		DEBUG_WRITE6( (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6) )
	
	#define STATS_WRITE7( msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7 ) \
		DEBUG_WRITE7( (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6), (arg7) )
	
	#define STATS_WRITE8( msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8 ) \
		DEBUG_WRITE8( (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6), (arg7), (arg8) )
	
	#define STATS_WRITE9( msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9 ) \
		DEBUG_WRITE9( (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6), (arg7), (arg8), (arg9) )
	
	#define STATS_WRITE10( msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10 ) \
		DEBUG_WRITE10( (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6), (arg7), (arg8), (arg9), (arg10) )

#else

	#define STATS_WRITE0( msg )
	#define STATS_WRITE1( msg, arg1 )
	#define STATS_WRITE2( msg, arg1, arg2 )
	#define STATS_WRITE3( msg, arg1, arg2, arg3 )
	#define STATS_WRITE4( msg, arg1, arg2, arg3, arg4 )
	#define STATS_WRITE5( msg, arg1, arg2, arg3, arg4, arg5 )
	#define STATS_WRITE6( msg, arg1, arg2, arg3, arg4, arg5, arg6 )
	#define STATS_WRITE7( msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7 )
	#define STATS_WRITE8( msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8 )
	#define STATS_WRITE9( msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9 )
	#define STATS_WRITE10( msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10 )

#endif /* if defined (_STATSA) && defined (_DEBUG) && defined (PC_WRAPPER) */

#endif /* DEBUG_H */
