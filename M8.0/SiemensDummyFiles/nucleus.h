#if !defined (__NUCLEUS_H)
#define __NUCLEUS_H

/* These functions are in here, as they are equivalents to NU_Sleep, NU_Resume_Task and NU_Suspend_Task
 * that Siemens use and inlcude via nucleus.h in the embedded version
 TODO: Should not use nucleus.h?  Use pctasks.h instead?  (Also for pcclock/obclock.h?) */

#if !defined (PC_WRAPPER)
typedef enum
{
    MOVA_TASK_INVALID = -1,

        MOVA_TASK_FIRST,
    MOVA_TASK_TERM = MOVA_TASK_FIRST,
    MOVA_TASK_TRLUSERIF,
	MOVA_TASK_OUTPUT,
	MOVA_TASK_AML,

        MOVA_TASK_LAST = MOVA_TASK_AML,

    MOVA_TASKS_NUM

} MOVA_TASK;
#else
/* PC_WRAPPER: enum suppressed to avoid conflict; define integer constants so
 * callers (getentry.c, userbig.c, term.c) still compile. PCTasks_Sleep is a no-op. */
#define MOVA_TASK_INVALID    (-1)
#define MOVA_TASK_TERM         0
#define MOVA_TASK_TRLUSERIF    1
#define MOVA_TASK_OUTPUT       2
#define MOVA_TASK_AML          3
#define MOVA_TASK_FIRST        MOVA_TASK_TERM
#define MOVA_TASK_LAST         MOVA_TASK_AML
#define MOVA_TASKS_NUM         4
#endif

void PCTasks_Sleep( unsigned long u32Milliseconds, int enTask );
void PCTasks_SuspendOutput( void );


#if !defined (PC_WRAPPER)
/* These defines 'define to nothing' Siemens task code.  But really this is 
 * trying to be too clever, and probably obscuring matters.  #if(n)def PC will be
 * used in the MOVA kernel directly for the moment. */

#include "base_types.h"

typedef int NU_TASK; /* Not really an int, it's a struct in Siemens MOVA */

#define NU_SUCCESS      0
#define NU_PURE_SUSPEND 1

/* Sleep and suspend functions only appropriate if running MOVA multithreaded (e.g. Siemens) */
#define NU_Sleep( time )
#define NU_Suspend_Task( task ) NU_SUCCESS
#define NU_Resume_Task( task ) NU_SUCCESS
#define NU_Task_Information( task, name, status, count, priority, preempt, time_slice, stack_base, stack_size, min_stack) NU_SUCCESS

#endif


#endif /*__NUCLEUS_H */