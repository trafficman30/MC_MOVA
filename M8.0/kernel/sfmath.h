/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         sfprv.h
*
*  TITLE:		Useful maths functions/macros for the MovaSatFlow module.
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	10-Feb-2003		6.0.0		..		IH		......			Initial implementation for testing with MOVA
*	31-May-2011		7.0.0		AA		PK		CRN003			New Header file inline with MOVA Kernel SQP
*
*  (c) Copyright TRL Ltd 2010. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal or
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/


#if !defined (MATH_H)
#define MATH_H


/* Is a number within a range */
#define Math_IsWithinRange( num, min, max )		(	\
	( (num) >= (min) )								\
	&&												\
	( (num) <= (max) )								\
)													

/* Round a number to the nearest integer (beware type conversion probs!) */
#define Math_RoundToInteger( num )				(	\
	(int16)( (REAL)(num) + 0.5 )					\
)											

/* Give the maximum of two numbers */
#define Math_Max( num1, num2 )					(	\
	(num2) > (num1) ? (num2) : (num1)				\
)


#endif /* MATH_H */
