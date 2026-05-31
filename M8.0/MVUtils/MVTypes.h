#if !defined (__MVTYPES_H)
#define __MVTYPES_H

/*
    Name:           MVTypes.h
    Author (Date):  ihenderson, 28/09/05
    Platform:       PC
    Purpose:        Defines platform independent types

    Last revision:  $Revision:: 1                           $
    Last changed:   $Date:: 28/11/05 11:53                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: MVTypes.h $
    
    *****************  Version 1  *****************
    User: Ihenderson   Date: 28/11/05   Time: 11:53
    Created in $/PCMOVA/MVUtils
*/

#if defined (__cplusplus)
    extern "C" {
#endif

#include <stddef.h>

/* Platform independent types with bit-widths */
typedef signed char	    MVInt8;
typedef signed short	MVInt16;
typedef signed long	    MVInt32;
typedef unsigned char	MVUInt8;
typedef unsigned short	MVUInt16;
typedef unsigned long	MVUInt32;
typedef MVUInt8			MVByte;

/* Platform dependent types for automatic variables */
/* Do not use for structs.  At least 16-bit. */
typedef unsigned int    MVUInt;
typedef int             MVInt;

/* For array and type sizes, array indexes etc. */
typedef size_t          MVSize;

/* For signed and unsigned pointer arithmetic */
typedef ptrdiff_t       MVPtrDiff;
typedef unsigned long   MVUPtrDiff;

/* Boolean type */
typedef enum			{ MV_FALSE = 0, MV_TRUE = 1 } MVBool;

/* Can be used for some function return values */
typedef enum            { MV_FAILURE = -1, MV_SUCCESS = 0 } MVStatus;

/* Type size limits */
#define MV_INT8_MIN		(-128)
#define MV_INT8_MAX		(127)
#define MV_INT16_MIN	(-32768)
#define MV_INT16_MAX	(32767)
#define MV_INT32_MIN	(-2147483648L)
#define MV_INT32_MAX	(2147483647L)
						
#define MV_UINT8_MIN	(0U)
#define MV_UINT8_MAX	(255U)
#define MV_UINT16_MIN	(0U)
#define MV_UINT16_MAX	(65535U)
#define MV_UINT32_MIN	(0UL)
#define MV_UINT32_MAX	(4294967295UL)


// SLM FIXME temp to hide unused variable warnings
#define UNUSED(x) (void)(x)

#if defined (__cplusplus)
    }
#endif

#endif /* __MVTYPES_H */