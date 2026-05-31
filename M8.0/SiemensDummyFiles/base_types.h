#if !defined (__BASE_TYPES_H)
#define __BASE_TYPES_H

#include "gbltypes.h"

/* This for MOVA_IF dummy struct */
typedef unsigned char           utiny;  /* for general unsigned byte values */

typedef unsigned long           UNSIGNED;

#if !defined (PC_WRAPPER)
// SLM FIXME NUM_MOVA_TASKS does not appear to be defined anywhere
#define  NUM_MOVA_TASKS MOVA_TASKS_NUM
typedef long                    SIGNED;
typedef unsigned char           DATA_ELEMENT;
typedef DATA_ELEMENT            OPTION;
typedef int                     STATUS;
typedef unsigned char           UNSIGNED_CHAR;
typedef char                    CHAR;
typedef int                     INT;
typedef void                    VOID;
#endif

#endif /* __BASE_TYPES_H */