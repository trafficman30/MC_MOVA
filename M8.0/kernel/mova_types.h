#if !defined (MOVA_TYPES_H)
#define MOVA_TYPES_H

/* Including "defines.h" here to re-use the primitive data types within the core (int8, int16,... etc.) */
#include "defines.h"


typedef enum { mv_false, mv_true } mv_bool;



#ifdef UNIT_TESTING
	#define STATIC
	#define EXT_VISIBLE __declspec(selectany)
#else
	#define EXT_VISIBLE
	#define STATIC static
#endif






#endif /*MOVA_TYPES_H*/