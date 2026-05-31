#if !defined (__MVMATH_H)
#define __MVMATH_H

/*
    Name:           MVMath.h
    Author (Date):  ihenderson, 28/09/05
    Platform:       PC
    Purpose:        Generic maths functions/macros used by PCMOVA

    Last revision:  $Revision:: 1                           $
    Last changed:   $Date:: 28/11/05 11:53                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: MVMath.h $
    
    *****************  Version 1  *****************
    User: Ihenderson   Date: 28/11/05   Time: 11:53
    Created in $/PCMOVA/MVUtils
*/

#if defined (__cplusplus)
    extern "C" {
#endif

#include "MVTypes.h"

/* Is a number within a range */
#define MATH_IS_IN_RANGE( num, min, max )		(	\
	( (num) >= (min) )								\
	&&												\
	( (num) <= (max) )								\
)													

/* Round a number to the nearest integer (beware type conversion probs!) */
#define MATH_ROUND_TO_INT( num )				(	\
	(MVInt)( (float)(num) + 0.5 )					\
)											

/* Give the maximum of two numbers */
#define MATH_MAX( num1, num2 )					(	\
	(num2) > (num1) ? (num2) : (num1)				\
)

#define MATH_ARRAY_LEN( array ) ( ( sizeof array ) / ( sizeof array[ 0 ] ) )

#if defined (__cplusplus)
    }
#endif

#endif /* __MVMATH_H */