#if !defined (__KDEBUG_H)
#define __KDEBUG_H

/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         kdebug.h
*
*  TITLE:        MOVA Kernel: Kernel debug logging facilities
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
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

#if defined (__cplusplus)
	extern "C" {
#endif

#if defined (TRL_HOST_DEBUG)

#include "defines.h"
#include <stddef.h>
#include "kdebugsettings.h"

#if defined (_DEBUG)
#define DEBUG 1
#else
#define DEBUG 0
#endif


	void		KDebugInitialise(				void);
	BOOL		KDEBUG_INITIALISED_STATUS(		void );
	void		KDEBUG_DEINITIALISE(			void );

	/* Do not call print function directly - use the macros below */
	void KDebugPrint(		KDebugLevel, const char *, const char *, ... );
	void KDebugNullPointerCheck( const void * module,
								 const void * address,
								 const char * addressStr);
	void KDebugLogError( const void * module,
						 BOOL expression, 
						 const char * expressionStr,
						 int lineNo);

	char * KDebugToString(int arraySize, int * intArray);

	/*	Trace Write macros with a conditional trace level
		(traces always require a prefix, e.g. the module name) */
	#define KDEBUG_WRITE0( level, prefix, msg ) \
		do { if (DEBUG) KDebugPrint( (level), (prefix), (msg) ); } while (0)

	#define KDEBUG_WRITE1( level, prefix, msg, arg1 ) \
		do { if (DEBUG) KDebugPrint( (level), (prefix), (msg), (arg1) ); } while (0)

	#define KDEBUG_WRITE2( level, prefix, msg, arg1, arg2 ) \
		do { if (DEBUG) KDebugPrint( (level), (prefix), (msg), (arg1), (arg2) ); } while (0)

	#define KDEBUG_WRITE3( level, prefix, msg, arg1, arg2, arg3 ) \
		do { if (DEBUG) KDebugPrint( (level), (prefix), (msg), (arg1), (arg2), (arg3) ); } while (0)

	#define KDEBUG_WRITE4( level, prefix, msg, arg1, arg2, arg3, arg4 ) \
		do { if (DEBUG) KDebugPrint( (level), (prefix), (msg), (arg1), (arg2), (arg3), (arg4) ); } while (0)

	#define KDEBUG_WRITE5( level, prefix, msg, arg1, arg2, arg3, arg4, arg5 ) \
		do { if (DEBUG) KDebugPrint( (level), (prefix), (msg), (arg1), (arg2), (arg3), (arg4), (arg5) ); } while (0)

	#define KDEBUG_WRITE6( level, prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6 ) \
		do { if (DEBUG) KDebugPrint( (level), (prefix), (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6) ); } while (0)

	#define KDEBUG_WRITE7( level, prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7 ) \
		do { if (DEBUG) KDebugPrint( (level), (prefix), (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6), (arg7) ); } while (0)

	#define KDEBUG_WRITE8( level, prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8 ) \
		do { if (DEBUG) KDebugPrint( (level), (prefix), (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6), (arg7), (arg8) ); } while (0)

	#define KDEBUG_WRITE9( level, prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9 ) \
		do { if (DEBUG) KDebugPrint( (level), (prefix), (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6), (arg7), (arg8), (arg9) ); } while (0)

	#define KDEBUG_WRITE10( level, prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10 ) \
		do { if (DEBUG) KDebugPrint( (level), (prefix), (msg), (arg1), (arg2), (arg3), (arg4), (arg5), (arg6), (arg7), (arg8), (arg9), (arg10) ); } while (0)

	#define NULL_POINTER_CHECK( module, address ) \
		do { if (DEBUG) KDebugNullPointerCheck( module, (address), #address ); } while (0)

	#define KDEBUG_EVAL_EXPRESSION(module, expression) \
		do { if (DEBUG) KDebugLogError( module, (expression), #expression, __LINE__); } while (0)

	#define KDEBUG_INITIALISE() \
		do { if (DEBUG) KDebugInitialise(); } while (0)
	

#else

/* TRL_HOST_DEBUG not defined therefore null out debugging macros */

	#define KDEBUG_WRITE0( level, prefix, msg )

	#define KDEBUG_WRITE1( level, prefix, msg, arg1 )

	#define KDEBUG_WRITE2( level, prefix, msg, arg1, arg2 )

	#define KDEBUG_WRITE3( level, prefix, msg, arg1, arg2, arg3 )

	#define KDEBUG_WRITE4( level, prefix, msg, arg1, arg2, arg3, arg4 )

	#define KDEBUG_WRITE5( level, prefix, msg, arg1, arg2, arg3, arg4, arg5 )

	#define KDEBUG_WRITE6( level, prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6 )

	#define KDEBUG_WRITE7( level, prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7 )

	#define KDEBUG_WRITE8( level, prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8 )

	#define KDEBUG_WRITE9( level, prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9 )

	#define KDEBUG_WRITE10( level, prefix, msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10 )

	#define NULL_POINTER_CHECK( module, address )

	#define KDEBUG_EVAL_EXPRESSION(module, expression)

	#define KDEBUG_INITIALISE() 

#endif

#if defined (__cplusplus)
	}
#endif

#endif
