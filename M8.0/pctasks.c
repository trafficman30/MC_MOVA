/*
    Name:           PCTasks.c
    Author (Date):  ihenderson, 24/11/05
    Platform:       PC
    Purpose:        PCTasks provides functionality for running
                    and controlling the MOVA Kernel tasks (threads).
                    Note that remote connections and their associated 
                    threads (answer/phone) are not (yet) supported in PCMOVA.

                    In PCMOVA it is desirable to run the main MOVA Kernel threads
                    (monitr(), detscn(), mova() and genstg()) in the same 
                    thread as the communications with the simulator
                    (via the controller), because we don't want
                    the simulator continuing until the MOVA kernel has
                    completed a full iteration - the simulator must wait
                    for the Kernel to complete an update.  This may not be the
                    case in an embedded system.

                    N.B. that the main MOVA kernel threads,
                    monitr(), detscn(), mova() and genstg(),
                    have been combined to run in the main PCMOVA thread.  
                    Since these threads carry out no I/O, complete
                    their iterations quickly, operate on much
                    the same data, and are dependent on each other running
                    at the right times anyway, it is safer and less confusing
                    to run them synchronously. (I suspect one or two
                    intermittent bugs in MOVA may be related to these threads - 
                    occasionally some data may not be reset by one thread before
                    another thread uses it, or vice versa.)
                    If monitr(), detscn(), mova() and genstg() are run in the 
                    same thread on the Siemens Gemini unit for instance, 
                    the fixed stack for the thread should be increased from 2K 
                    to 8K (to be safe) - i.e. the combined size of the stacks 
                    for the 4 separate monitr(), detscn(), mova() and genstg() 
                    threads used at present.                    

                    Synchronisation Note:
                    PCTasks_Sleep and PCTasks_SuspendOutput are called from
                    the MOVA kernel term/TRLUserIF/output threads - these
                    functions are thread-safe (re-entrant).
                    
    Last revision:  $Revision:: 5                           $
    Last changed:   $Date:: 12/04/06 16:07                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: pctasks.c $
    
    *****************  Version 5  *****************
    User: Ihenderson   Date: 12/04/06   Time: 16:07
    Updated in $/PCMOVA
    
    *****************  Version 4  *****************
    User: Ihenderson   Date: 9/03/06    Time: 14:10
    Updated in $/PCMOVA
    
    *****************  Version 3  *****************
    User: Ihenderson   Date: 16/12/05   Time: 10:26
    Updated in $/PCMOVA
    
    *****************  Version 2  *****************
    User: Ihenderson   Date: 5/12/05    Time: 11:07
    Updated in $/PCMOVA
    
    *****************  Version 1  *****************
    User: Ihenderson   Date: 28/11/05   Time: 11:49
    Created in $/PCMOVA
*/

/*
    TODO:
*/

#include "stdafx.h"
#include <process.h> /* For _beginthreadex() */

#include "nucleus.h" /* MOVA Kernel interface to PCTasks */
#include "pctasks.h" /* PCMOVA interface to PCTasks */

#include "aml_handler.h"


#define g_MODULE_NAME   ("PCTask")

/* Timeout to use when waiting for MOVA threads to terminate (ms) */
#define g_TERMINATE_TIMEOUT         (100)

/* Timeout to use when waiting for Output thread to resume (ms) 
This should be more than the write timeout used by 
TCPClient_WriteBytes() in MVComms.dll (currently 2 seconds) */
#define g_OUTPUT_RESUME_TIMEOUT     (5000)

/* Functions that make up the MOVA kernel */
extern void monitr(     unsigned long, void * );    /* Runs every 100ms*/
extern void output(     unsigned long, void * );    /* Runs every 1000ms*/

#if IS_MOVACOMM_ENABLED
extern void TRLUserIF(unsigned long, void *);    /* Runs continuously*/
extern void term(UNSIGNED argc, void * argv);
#endif

/* The thread entry point functions (wrappers) */
static DWORD WINAPI PCTasks_Output(     LPVOID );
static DWORD WINAPI PCTasks_Term(       LPVOID );
static DWORD WINAPI PCTasks_TRLUserIF(  LPVOID );

static DWORD WINAPI PCTasks_AML(  LPVOID );

static MVStatus PCTasks_Resume(         MOVA_TASK );

/* Data-type containing information about a MOVA task (thread) */
typedef struct MOVATask_Struct
{
    HANDLE                  hThread;
    DWORD                   dwThreadID;
    LPTHREAD_START_ROUTINE  lpRoutine;
    MVInt                   nPriority;

} MOVATask;


/* Global data */
static MVBool       g_isInitialised = MV_FALSE;
static MOVATask     g_tasks[ MOVA_TASKS_NUM ];

/* Event for terminating MOVA threads */
static HANDLE       g_hTerminateEvent;
static MVBool       g_isTerminating = MV_FALSE;

/* Events for controlling Output thread */
static HANDLE       g_hOutputResumeEvent;
static HANDLE       g_hOutputIsResumedEvent;

/* Counter used to determine when to run the Output task */
static MVInt        g_outputResumeCount;


/*
    Name:           PCTasks_Initialise
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Initialises the tasks (threads) that
                    make up the MOVA kernel
    Inputs:         
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Remarks:
*/
MVStatus PCTasks_Initialise( void )
{
    MVStatus    status = MV_SUCCESS;
    BOOL        boSuccess;
    MVInt        taskIndex;
    MOVATask    *pTask;

    TRACE_ASSERT( !g_isInitialised, "PC Tasks module already initialised." );

    TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Initialising MOVA tasks..." );

    /* Siemens MOVA thread priorities (for reference) */

    /*
    #define MONITR_TASK_PRIORITY            13
    #define DETSCN_TASK_PRIORITY            13
    #define GENSTG_TASK_PRIORITY            21
    #define MOVA_TASK_PRIORITY              24
    #define ANSWER_TASK_PRIORITY            24
    #define PHONE_TASK_PRIORITY             24
    #define TERM_TASK_PRIORITY              24
    #define OUTPUT_TASK_PRIORITY            25
    #define TRLUSER_TASK_PRIORITY           25
    */

    /* Windows thread priorities (for reference) */

    /*
    THREAD_PRIORITY_ABOVE_NORMAL Indicates 1 point above normal priority for the priority class. 
    THREAD_PRIORITY_BELOW_NORMAL Indicates 1 point below normal priority for the priority class. 
    THREAD_PRIORITY_HIGHEST Indicates 2 points above normal priority for the priority class. 
    THREAD_PRIORITY_IDLE Indicates a base priority level of 1 for IDLE_PRIORITY_CLASS, BELOW_NORMAL_PRIORITY_CLASS, NORMAL_PRIORITY_CLASS, ABOVE_NORMAL_PRIORITY_CLASS, or HIGH_PRIORITY_CLASS processes, and a base priority level of 16 for REALTIME_PRIORITY_CLASS processes. 
    THREAD_PRIORITY_LOWEST Indicates 2 points below normal priority for the priority class. 
    THREAD_PRIORITY_NORMAL Indicates normal priority for the priority class. 
    THREAD_PRIORITY_TIME_CRITICAL 
    */

    /* Initialise task attributes - thread priority seems ok, 
    but could be adjusted if necessary */

#if IS_MOVACOMM_ENABLED
    g_tasks[ MOVA_TASK_TERM ].lpRoutine = PCTasks_Term;
    g_tasks[ MOVA_TASK_TERM ].nPriority = THREAD_PRIORITY_NORMAL;

	g_tasks[MOVA_TASK_TRLUSERIF].lpRoutine = PCTasks_TRLUserIF;
	g_tasks[MOVA_TASK_TRLUSERIF].nPriority = THREAD_PRIORITY_NORMAL;

	taskIndex = MOVA_TASK_FIRST;
#endif

    g_tasks[ MOVA_TASK_OUTPUT ].lpRoutine = PCTasks_Output;
    g_tasks[ MOVA_TASK_OUTPUT ].nPriority = THREAD_PRIORITY_NORMAL;

	g_tasks[MOVA_TASK_AML].lpRoutine = PCTasks_AML;
	g_tasks[MOVA_TASK_AML].nPriority = THREAD_PRIORITY_NORMAL;

	taskIndex = MOVA_TASK_OUTPUT;

	/* Create all the MOVA threads in a suspended state */

    while ( status == MV_SUCCESS && taskIndex <= MOVA_TASK_LAST )
    {
        pTask = &( g_tasks[ taskIndex ] );

        /* Note that a thread that uses functions from the C run-time libraries 
        should use the _beginthread and _endthread C run-time functions 
        for thread management rather than CreateThread and ExitThread. */
        pTask->hThread = (HANDLE)_beginthreadex( 
            NULL,                   /* Default security attributes (by default all access)*/
            0,                      /* initial stack size default for this application*/
            (_beginthreadex_proc_type)pTask->lpRoutine,       /* thread function*/
            NULL,                   /* thread argument*/
            CREATE_SUSPENDED,       /* creation option (don't start thread yet)*/
            &((unsigned int)(pTask->dwThreadID))  /* thread identifier*/
        );

        if ( pTask->hThread != NULL )
        {
            boSuccess = SetThreadPriority( pTask->hThread, pTask->nPriority );
            if ( boSuccess == FALSE )
            {
                PCError_SetLastWin( PCMOVA_ERROR_TASK_INIT, 
                    GetLastError(), NULL, g_MODULE_NAME  );
                status = MV_FAILURE;
            }
        }
        else
        {
            PCError_SetLastWin( PCMOVA_ERROR_TASK_INIT, 
                GetLastError(), NULL, g_MODULE_NAME );
            status = MV_FAILURE;
        }

        ++taskIndex;
 
    } /* For each MOVA task */

    /* Create event for terminating MOVA threads */    
    if ( status == MV_SUCCESS )
    {
        g_isTerminating = MV_FALSE;
        
        g_hTerminateEvent = CreateEvent( 
            NULL,       /* Default security attributes*/
            TRUE,       /* Manual reset*/
            FALSE,      /* Not signalled to start with*/
            NULL );     /* No name*/

        if ( g_hTerminateEvent == NULL )
        {
            PCError_SetLastWin( PCMOVA_ERROR_TASK_INIT, 
                GetLastError(), NULL, g_MODULE_NAME );
            status = MV_FAILURE;
        }
    }


    /* Create event for continuing Output thread */
    if ( status == MV_SUCCESS )
    {
        /* Counter used to determine when to run the Output task */
        g_outputResumeCount = 0;
        
        g_hOutputResumeEvent = CreateEvent( 
            NULL,       /* Default security attributes*/
            FALSE,      /* Automatic reset*/
            FALSE,      /* Not signalled to start with*/
            NULL );     /* No name*/

        if ( g_hOutputResumeEvent == NULL )
        {
            PCError_SetLastWin( PCMOVA_ERROR_TASK_INIT, 
                GetLastError(), NULL, g_MODULE_NAME );
            status = MV_FAILURE;
        }
    }    


    /* Create event for signalling that Output thread has resumed */
    if ( status == MV_SUCCESS )
    {
        g_hOutputIsResumedEvent = CreateEvent( 
            NULL,       /* Default security attributes*/
            FALSE,      /* Automatic reset*/
            FALSE,      /* Not signalled to start with*/
            NULL );     /* No name*/

        if ( g_hOutputIsResumedEvent == NULL )
        {
            PCError_SetLastWin( PCMOVA_ERROR_TASK_INIT, 
                GetLastError(), NULL, g_MODULE_NAME );
            status = MV_FAILURE;
        }
    }    


    if ( status == MV_SUCCESS )
    {
        g_isInitialised = MV_TRUE;
        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
            "MOVA tasks initialised." );
    }

    return ( status );

} /* PCTasks_Initialise */


/*
    Name:           PCTasks_DeInitialise
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Deinitialises the tasks (threads) that
                    make up the MOVA kernel.
    Inputs:         
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Remarks:
*/
MVStatus PCTasks_DeInitialise( void )
{
    MVStatus status = MV_SUCCESS;

    if ( g_isInitialised )
    {
        HANDLE      tasks[ MOVA_TASKS_NUM ];
        MOVA_TASK   taskID;
        BOOL        isOK;
        DWORD       waitResult;

		MOVA_TASK   firstTaskID;
		MVInt		tasks_num = 0;

        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
            "Deinitialising MOVA tasks..." );
    
        /* Get the thread handles */
#if IS_MOVACOMM_ENABLED
		firstTaskID = MOVA_TASK_FIRST;
		for (taskID = firstTaskID; taskID <= MOVA_TASK_LAST; ++taskID)
#else
		firstTaskID = MOVA_TASK_OUTPUT;
		for (taskID = firstTaskID; taskID <= MOVA_TASK_LAST; taskID++)
#endif        
        {
            tasks[ taskID ] = g_tasks[ taskID ].hThread;
			tasks_num++;
        }               

        /* Tell threads that they must terminate and wait for them to do so */        
        g_isTerminating = MV_TRUE;
        
        isOK = SetEvent( g_hTerminateEvent );        
        if ( isOK == FALSE )
        {
            PCError_SetLastWin( PCMOVA_ERROR_TASK_DEINIT, 
                GetLastError(), NULL, g_MODULE_NAME );
            status = MV_FAILURE;
        }

        if ( status == MV_SUCCESS )
        {
            waitResult = WaitForMultipleObjects(
				tasks_num,         /* number of handles to wait for*/
                &( tasks[firstTaskID] ),          /* handle array*/
                TRUE,                   /* wait for all threads to become signalled*/
                g_TERMINATE_TIMEOUT );  /* time-out interval (ms)*/

            if ( waitResult == WAIT_FAILED )
            {
                PCError_SetLastWin( PCMOVA_ERROR_TASK_DEINIT, 
                    GetLastError(), NULL, g_MODULE_NAME );
                status = MV_FAILURE;
            }

            if ( waitResult == WAIT_TIMEOUT )
            {
                PCError_SetLast( PCMOVA_ERROR_TASK_DEINIT, 
                    "Timed-out waiting for threads to finish.", g_MODULE_NAME );
                status = MV_FAILURE;
            }
                   
            isOK = ResetEvent( g_hTerminateEvent );
            if ( isOK == FALSE )
            {
                PCError_SetLastWin( PCMOVA_ERROR_TASK_DEINIT, 
                    GetLastError(), NULL, g_MODULE_NAME );
                status = MV_FAILURE;
            }
        }

        isOK = CloseHandle( g_hTerminateEvent );
        if ( isOK == FALSE )
        {
            PCError_SetLastWin( PCMOVA_ERROR_TASK_DEINIT, 
                GetLastError(), NULL, g_MODULE_NAME );
            status = MV_FAILURE;
        }

        isOK = CloseHandle( g_hOutputResumeEvent );
        if ( isOK == FALSE )
        {
            PCError_SetLastWin( PCMOVA_ERROR_TASK_DEINIT, 
                GetLastError(), NULL, g_MODULE_NAME );
            status = MV_FAILURE;
        }

        isOK = CloseHandle( g_hOutputIsResumedEvent );
        if ( isOK == FALSE )
        {
            PCError_SetLastWin( PCMOVA_ERROR_TASK_DEINIT, 
                GetLastError(), NULL, g_MODULE_NAME );
            status = MV_FAILURE;
        }

        if ( status == MV_SUCCESS )
        {
            g_isInitialised = MV_FALSE;
            TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
                "MOVA tasks deinitialised." );
        }

    } /* If initialised */
    
    return ( status );

} /* PCTasks_DeInitialise */


/*
    Name:           PCTasks_IsInitialised
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Determines whether the PCMOVA tasks module
                    (and hence the MOVA Kernel) is initialised.
    Inputs:         
    Returns:        True if module initialised; False otherwise.
    Remarks:
*/
MVBool PCTasks_IsInitialised( void )
{
    return ( g_isInitialised );

} /* PCTasks_IsInitialised */


/*
    Name:           PCTasks_Start
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Starts the MOVA Kernel - this is done by calling monitr()
                    only once and resuming each thread only once.
    Inputs:         
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Remarks:        PCTasks_Start starts the MOVA Kernel UI threads, allowing
                    MOVA Comm to connect to MOVA before the simulation starts
                    (e.g. to download additional time-of-day datasets, or
                    initialise MOVA message output).
                    Calling monitr() the first time causes it to call Movsub() 
                    (in Datahand.c), which loads the default dataset.
*/
MVStatus PCTasks_Start( void )
{
    MVStatus status = MV_SUCCESS;

    TRACE_ASSERT( g_isInitialised, "PC Tasks module not initialised." );

    TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Starting MOVA tasks..." );

    /* Initialise the MOVA Kernel by calling monitr() once */
    monitr( 0, NULL );
  
	
#if IS_MOVACOMM_ENABLED

    /* Initialise Term by resuming it */
    status = PCTasks_Resume( MOVA_TASK_TERM );

    if ( status == MV_SUCCESS )
    {
        /* Initialise TRLUserIF by resuming it */
        status = PCTasks_Resume( MOVA_TASK_TRLUSERIF );
	}
#else
	status = PCTasks_Resume(MOVA_TASK_AML);
#endif

    if ( status == MV_SUCCESS )
    {
        /* Initialise Output by resuming it */
        status = PCTasks_Resume( MOVA_TASK_OUTPUT );
    }

    /* TEMP: TEST: simulate some errors occurring during running of the kernel.*/
     /*PCError_TestKernelMessages();*/



    TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "MOVA tasks started." );

    return ( status );

} /* PCTasks_Start */


/*
    Name:           PCTasks_Update
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Updates the MOVA Kernel - should be called every 
                    0.1 seconds in simulation time, after new confirm and
                    detector inputs have been received from the controller
                    (see update message processing in PCKernel_Run).
                    Also update the Output task, which must run every second
                    in simulation time.
    Inputs:         
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Remarks:        Note that the MOVA Kernel will always wait for the Output 
                    task to complete. PCMOVA must run with simulations at 
                    speeds significantly faster than real-time.  When running 
                    on-street, Output should always complete before a second 
                    has elapsed, and if it doesn't, we shouldn't hold up the 
                    MOVA kernel waiting for Output to complete (it is better 
                    to miss out a few MOVA messages, than to hold up 
                    the code that is actually controlling the junction).
                    However, simulations run much faster than real-time 
                    (e.g. anything up to 1000xRT+ with MOVASIM), so Output is 
                    much less likely to complete within a second of 
                    *simulation* time.  Coupled with this is the fact that the 
                    output from the Output task is more important in simulation 
                    - simulations are often being run specifically to assess 
                    performance of MOVA, so a complete log of MOVA messages 
                    without missing entries is more important.  Even in this 
                    case, we shouldn't risk hanging the program by waiting 
                    forever, so a wait timeout has been specified.
*/
MVStatus PCTasks_Update( void )
{
    MVStatus    status = MV_SUCCESS;
    
    TRACE_ASSERT( g_isInitialised, "PC Tasks module not initialised." );
        
    /* DEBUG_WRITE0( "Updating kernel..." );*/

    /* Update the MOVA Kernel by calling monitr() */
    monitr( 0, NULL );

    /* Run Output only every 1000ms in simulation time (as Siemens do) */
    g_outputResumeCount++;
    if ( g_outputResumeCount == 10 )
    {
        BOOL    isOK;
        DWORD   waitResult;

        /* DEBUG_WRITE0( "Updating output..." );    */

        isOK = SetEvent( g_hOutputResumeEvent );
        if ( isOK == FALSE )
        {
            PCError_SetLastWin( PCMOVA_ERROR_TASK_RESUME, 
                GetLastError(), NULL, g_MODULE_NAME );
            status = MV_FAILURE;
        }

        /* BUGFIX: 10040. Suspend execution of the main PCMOVA kernel 
        thread until Output has resumed.  This behaviour is ONLY suitable
        for PCMOVA, not for an embedded implementation. */
        waitResult = WaitForSingleObject( 
            g_hOutputIsResumedEvent, g_OUTPUT_RESUME_TIMEOUT );
        if ( waitResult == WAIT_FAILED )
        {
            TRACE_WRITE0_IF( LEVEL_ERROR, g_MODULE_NAME, 
                "Thread error WAIT_FAILED (waiting for Output task to resume)." );

            PCError_SetLastWin( PCMOVA_ERROR_TASK_RESUME, 
                GetLastError(), NULL, g_MODULE_NAME );
            status = MV_FAILURE;
        }

        if ( waitResult == WAIT_TIMEOUT )
        {
            TRACE_WRITE0_IF( LEVEL_ERROR, g_MODULE_NAME, 
                "Thread error WAIT_TIMEOUT (waiting for Output task to resume)." );

            PCError_SetLast( PCMOVA_ERROR_TASK_RESUME, NULL, g_MODULE_NAME );
            status = MV_FAILURE;
        }

        g_outputResumeCount = 0;

    } /* If it is time to resume the Output task */



    return ( status );

} /* PCTasks_Update */


/*
    Name:           PCTasks_Output
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Wrapper for the MOVA Kernel "Output" task (Output.c)
                    Output sends error, assessment and MOVA message
                    logs to the MOVA Comm terminal communications 
                    program.
    Inputs:         Pointer to parameters (unused)
    Returns:        Always 0 (MOVA tasks don't return an error value)
    Remarks:        
*/
static DWORD WINAPI PCTasks_Output( LPVOID lpParameter )
{
	UNUSED(lpParameter);

    TRACE_ASSERT( g_isInitialised, "PC Tasks module not initialised." );

    output( 0, NULL );
    return 0;

} /* PCTasks_Output */


/*
    Name:           PCTasks_Term
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Wrapper for the MOVA Kernel "Term" task (Term.c)
                    Term reads characters from MOVA Comm via
                    the local handset port (Serial/TCP)
    Inputs:         Pointer to parameters (unused)
    Returns:        Always 0 (MOVA tasks don't return an error value)
    Remarks:        
*/
static DWORD WINAPI PCTasks_Term( LPVOID lpParameter )
{
	UNUSED(lpParameter);

#if IS_MOVACOMM_ENABLED
	
	TRACE_ASSERT(g_isInitialised, "PC Tasks module not initialised.");

	term(0, NULL);
#endif

	return 0;
	
} /* PCTasks_Term */


/*
    Name:           PCTasks_TRLUserIF
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Wrapper for the MOVA Kernel "TRLUserIF" task
                    (Userbig.c, some parts in Term.c and Datahand.c)
                    TRLUserIF displays the MOVA Kernel user interface to the 
                    user via MOVA Comm, including the commissioning screen,
                    MOVA Main Menu and Dataset submenu.
    Inputs:         Pointer to parameters (unused)
    Returns:        Always 0 (MOVA tasks don't return an error value)
    Remarks:        
*/
static DWORD WINAPI PCTasks_TRLUserIF( LPVOID lpParameter )
{
	UNUSED(lpParameter);

#if IS_MOVACOMM_ENABLED
    TRACE_ASSERT( g_isInitialised, "PC Tasks module not initialised." );
    
    TRLUserIF( 0, NULL );
#endif

	return 0;

} /* PCTasks_TRLUserIF */



  /*
  Name:           PCTasks_AML
  Author (Date):  iabdelhalim, 11/05/2015
  Platform:       PC
  Purpose:        Resumes (starts) the given MOVA Kernel task.
  Inputs:         Number of the task to start
  (the MOVA_TASK enum is defined in nucleus.h)
  Returns:        Status (success/failure)
  */
static DWORD WINAPI PCTasks_AML( LPVOID lpParameter )
{
	UNUSED(lpParameter);

	TRACE_ASSERT( g_isInitialised, "PC Tasks module not initialised." );
    
    aml_handler(0, NULL);
    return 0;

} /* PCTasks_AML */

/*
    Name:           PCTasks_Resume
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Resumes (starts) the given MOVA Kernel task.
    Inputs:         Number of the task to start
                    (the MOVA_TASK enum is defined in nucleus.h)
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Remarks:        
*/
static MVStatus PCTasks_Resume( MOVA_TASK enTask )
{
    MVStatus    status = MV_SUCCESS;
    DWORD       dwResumeStatus;
    MOVATask    *pTask;

    TRACE_ASSERT( g_isInitialised, "PC Tasks module not initialised." );
    
    pTask = &( g_tasks[ enTask ] );

    dwResumeStatus = ResumeThread( pTask->hThread );

    /* MOVA Threads should never have their suspend count incremented beyond 1 
    and we shouldn't be trying to resume a thread that's already going (status=0) */
    if ( dwResumeStatus != 1 )
    {
        TRACE_WRITE2_IF( LEVEL_ERROR, g_MODULE_NAME, 
            "Thread error (resume): task number %d. Resume status: %d.", 
            enTask, dwResumeStatus );

        PCError_SetLastWin( PCMOVA_ERROR_TASK_RESUME, 
            GetLastError(), NULL, g_MODULE_NAME );
        status = MV_FAILURE;
    }

    return ( status );

} /* PCTasks_Resume */


/*
    Name:           PCTasks_Sleep
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Suspends the given MOVA Kernel task for a set 
                    amount of time.
    Inputs:         Time to sleep for (ms)
                    Number of the task to suspend
                    (the MOVA_TASK enum is defined in nucleus.h)
    Returns:        
    Remarks:        Included for use within the MOVA kernel via nucleus.h
*/
void PCTasks_Sleep( unsigned long milliSecs, MOVA_TASK enTask )
{
    DWORD waitResult;
    
    TRACE_ASSERT( g_isInitialised, "PC Tasks module not initialised." );

    /* Suspend execution of this thread (whatever that is) 
    unless a terminate event is received. */
    waitResult = WaitForSingleObject( g_hTerminateEvent, milliSecs );
    if ( waitResult == WAIT_FAILED )
    {
        TRACE_WRITE2_IF( LEVEL_ERROR, g_MODULE_NAME, 
            "Thread error (sleep): task number %d; Windows error code: %d.", 
            enTask, GetLastError() );
        
        /* Only assert in DEBUG builds - Sleep is used by UI tasks
        term and TRLUserIF - not a problem unless it fails repeatedly 
        (will adversely affect performance) */
        DEBUG_ASSERT( MV_FALSE );
    }

    /* MOVA kernel tasks don't deinitialise properly before returning 
    - may as well end the thread here */
    if ( g_isTerminating )
    {
        _endthreadex( 0 );
    }

} /* PCTasks_Sleep */


/*
    Name:           PCTasks_SuspendOutput
    Author (Date):  ihenderson, 10/12/05
    Platform:       PC
    Purpose:        Suspends the MOVA Kernel "Output" task until
                    it is resumed by PCTasks_Update().  Once resumed,
                    sends a message to the main PCMOVA kernel thread
                    to allow it to continue (see PCTasks_Update() for info).
    Inputs:         
    Returns:        
    Remarks:        Included for use within the MOVA kernel via nucleus.h
*/
void PCTasks_SuspendOutput( void )
{
    DWORD       waitResult;
    HANDLE      outputTaskEvents[2];

    TRACE_ASSERT( g_isInitialised, "PC Tasks module not initialised." );
    
    /* Wait until an event is received */
    outputTaskEvents[0] = g_hOutputResumeEvent;
    outputTaskEvents[1] = g_hTerminateEvent;
    waitResult = WaitForMultipleObjects( 
        2, 
        &( outputTaskEvents[0] ), 
        FALSE, 
        INFINITE );    

    if ( waitResult == WAIT_FAILED )
    {
        TRACE_WRITE1_IF( LEVEL_ERROR, g_MODULE_NAME, 
            "Thread error (suspending Output task). Windows error code: %d.",
            GetLastError() );
        
        TRACE_ASSERT( MV_FALSE, "Error suspending Output task." );
    }

    /* MOVA kernel tasks don't deinitialise properly before returning 
    - may as well end the thread here */
    if ( g_isTerminating )
    {
        _endthreadex( 0 );
    }
    else
    {
        /* The signalled event must be the Output event, so resume 
        and tell main PCMOVA thread that we've resumed */
        BOOL isOK = SetEvent( g_hOutputIsResumedEvent );
        if ( isOK == FALSE )
        {
            TRACE_WRITE1_IF( LEVEL_ERROR, g_MODULE_NAME, 
                "Thread error (signalling Output task resumed)."
                " Windows error code: %d.", GetLastError() );
        
            TRACE_ASSERT( MV_FALSE, "Error signalling Output task resumed." );
        }
    }

} /* PCTasks_SuspendOutput */

MVBool PCTasks_IsTerminating()
{
	return g_isTerminating;
}

#if 0

/* Now using Events for controlling suspension/resumption of Output task */
void PCTasks_Suspend( MOVA_TASK enTask )
{
    DWORD       dwSuspendStatus;
    MOVATask    *pTask;

    TRACE_ASSERT( g_isInitialised, "PC Tasks module not initialised." );
    TRACE_ASSERT( g_isTerminating, "PC Tasks module terminating." );

    pTask = &( g_tasks[ enTask ] );

    dwSuspendStatus = SuspendThread( pTask->hThread );

    /* We shouldn't be trying to resume a thread that's already 
    suspended (status>0) */
    if ( dwSuspendStatus != 0 )
    {
        TRACE_WRITE2_IF( LEVEL_ERROR, g_MODULE_NAME, 
            "Thread error (suspend): task number %d. Suspend status: %d.", 
            enTask, dwSuspendStatus );
        
        DEBUG_ASSERT( MV_FALSE );
    }

} /* PCTasks_Suspend */

#endif