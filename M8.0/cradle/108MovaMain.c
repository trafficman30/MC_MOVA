//
//  Project     Chameleon
//
//  Copyright 	Dynniq UK Ltd 2018
//
//  All rights reserved
//
//  The information contained herein is the property of Peek Traffic Limited
//  and is supplied without any liability for errors or omissions. No part
//  may be reproduced or copied except as authorised by contract or other
//  written permission. The copyright and foregoing restriction on
//  reproduction and use extend to all media in which the information may be
//  embodied.
//
//!  \file   108MovaMain.c
//!	\brief  Main process and threads control for MOVA M8.0 kernel
//!	\author Andrew Leigh
//!	\date   27-May-2018
//

static char MovaMain_c_rcsid[] = "$Id: $";



/* --------------------------------------------------------------------------- *
 *                     INCLUDE FILES                                           *
 * --------------------------------------------------------------------------- *
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/shm.h>
#include <sys/ipc.h>

#include "000Common.h"
#include "501DiagLib.h"
#include "502FaultLib.h"
#include "503SktLib.h"
#include "504CmnLib.h"
#include "505EventLib.h"
#include "108MovaCradle.h"
#include "108MovaFaults.h"
#include "108MovaLib.h"

#include "Datahand.h"

/*
 * --------------------------------------------------------------------------- *
 *                     SYMBOLIC CONSTANTS                                      *
 * --------------------------------------------------------------------------- *
*/
//#define DEBUG_START
//#define TRACE_MOVA_SOCKET
/* --------------------------------------------------------------------------- *
 *                     MACRO DEFINITIONS                                       *
 * --------------------------------------------------------------------------- *
*/

/*
 * --------------------------------------------------------------------------- *
 *                     TYPE DEFINITIONS                                        *
 * --------------------------------------------------------------------------- *
 */

/* --------------------------------------------------------------------------- *
 *                     GLOBAL DATA REFERENCES                                  *
 * --------------------------------------------------------------------------- *
*/

iomemshared_t *GshmMem;
MovaLibMemory_t MovaNonVol;

char gMovaDataset[MAX_MOVA_IO_DATASET_LEN+1];

monitor_loop_t 	gDirection = eINIT;

//static iomemshared_t *gprSharedMemory;		// Pointer to shared memory

const char cLF = 0xA ;
const char cCR = 0xD ;
const char cBackSp = 0x8 ;
const char cDelete = 0x7F ;
const char sDelChar[4] = "\b \b" ; //3 characters to delete last character on screen

struct phone_line_share_t phone_line_sharing ;

//unsigned char lLineDropped = 0; // non zero if the modem line has dropped
unsigned char soft_error_count ; // Only set to zero in Mova code
unsigned char RB_GOOD_BLK_REC ; // Repeatedly set something to do with modems
unsigned char IC_WATCHDOG_FAIL_COUNT ; //Only set to zero in Mova code
unsigned char lModemTried;    /* Call received but user in process of being told LH busy */

//char lFirstPowerup; /* cold restart flag to initialize data */

char lFirstPowerup = 0; /* cold restart flag to initialize data */
int mova_first_time_powerup_code ; // set to zero in datahand.c


// interval timer
struct itimerval	Ginterval;
struct timeval	GTimeVal;

// flag to step out of pause() loop
int readytogo = 0;

#ifdef TRACE_MOVA_SOCKET
int g_dload_fd;
#endif // TRACE_MOVA_SOCKET

const char *const fmtVersion = {"('MOVA Version M6.0')"};
const char *const fmtCopyright = {"('(c) Copyright TRL Limited, 2005')"};

/* --------------------------------------------------------------------------- *
 *                           FUNCTION REFERENCES                               *
 * --------------------------------------------------------------------------- *
*/


/*
 * mova prototypes
 */
void term(UNSIGNED argc, char * argv);  // actually this one's from Term.c
void monitr(UNSIGNED argc, void * argv, monitor_loop_t *direction);
void output(UNSIGNED argc, void * argv);
void TRLUserIF(UNSIGNED argc, void * argv);

/*
 * Support Function prototypes
 */
void *monitrThread(void *message);
void *outputThread(void *message);
void *termThread(  void *message);
void *trluiThread( void *message);
static void Exit (void);
static void Initialise(int Argc, char *Argv[]);
void* sig_handler( void* arg );
void* Dataset_handler(void* arg );
void* Aml_handler(void* arg );

void SIGTERMHandler( int siglevel );
void SIGUSR1Handler ( int );
void SIGUSR2Handler ( int );
void SIGPWRHandler ( int );
void ThreadsInit();
void ThreadsDestroy();
void CancelMovaThreads();
void PauseWithKeepalive();


/*
 * --------------------------------------------------------------------------- *
 *                   main() and threads                                        *
 * --------------------------------------------------------------------------- *
 */


//!  \fn     main()
//!  \brief Main for module
//!
//!  \param  Argc Number of arguments
//!  \param  Argv Arguments
//!  \note   called with -m 100 -i 0
//!    where -m is the module number and -i is the instance or MOVA stream
//!  \retval  void none
//!
int main(int argc, char **argv)
{
    // Run the Mova code from here

    char *message1 = "Thread 1";
    char *message2 = "Thread 2";
    char *message3 = "Thread 3";
    char *message4 = "Thread 4";
    char *message5 = "Thread 5";
    char *message6 = "Thread 6";
    char *message6 = "Thread 7";
    int  iret1, iret2, iret3, iret4, iret5, iret6, iret7;
    sigset_t signal_set;
    char sFilepath[MAX_PATHNAME_LEN+1];
    int status = 0;
    mova_io_t *movaIo;
    BOOLEAN movaOn = FALSE;
    equipment_t *equipment;
    char tmpString[CONST_LINE_LENGTH];
    char *tmpStrPtr = NULL;
    movaNonVol_t* MovaNonVol ;

#ifdef    TRACE_MOVA_SOCKET
    char dloadPath[MAX_PATHNAME_LEN];
#endif // TRACE_MOVA_SOCKET

    CMN_TRACE_ENTRY;

    GModule         = MOD_MOVA;	 // Module number of task
    GInstance       = 0;        	// Instance number of task
    GMySocket       = 0;        	// Socket number of task

    lLineDropped = 0; // non zero if the modem line has dropped

    // char *argv[] = {"100MOVA","-m100","-i0"};

    // initialisation for process
    // this also parses the process args including GInstance
    Initialise (argc, argv);

    // Get the MOVA shared memory if we have a licence
    MovaLibGetMovaShMem(GInstance) ;

    CmnWaitForGo("main:waiting for go");

    // Wait until the configuration has been loaded
    CmnWaitForNotInitial();

    // This MOVA process has a unique instance GInstance which is provided
    // when startup starts this process.  The MOVA IO section in the chameleon
    // configuration file provides a unique equipment id name which will be
    // referred to as the MOVA stream and a dataset name which is file default
    // dataset filename to load.  The value of GInstance can be used to make
    // sure two MOVA processes are not going to use the same equipment id name

    gMovaEqIdNum = MovaLibInstanceToStream(GInstance, gMovaEqId, gMovaDataset, &movaOn, &gMovaIf);
    if ( gMovaEqIdNum == -1)
    {
        // Prevent MOVA processes starting without a matching configuration,
        // pause them
        DiagLog(DC_WARNING, "%s: No MOVA configuration.  Pausing instance: %d", __FUNCTION__, GInstance);
        MovaCradleSetStatus(MOVAST_NO_CONFIG);
        PauseWithKeepalive();
    }

    if (MovaLibCheckLicence(GInstance) == FALSE)
    {
        DiagLog(DC_WARNING, "%s: No MOVA licence.  Pausing instance: %d", __FUNCTION__, GInstance);
        MovaCradleSetStatus(MOVAST_NO_LICENCE);
        PauseWithKeepalive();
    }

#ifdef DEBUG_START
    MovaCradleSetStatus(MOVAST_DEBUGSTART);
    PauseWithKeepalive();
#endif /* DEBUG_START */

    // format the ring buffer used to communicate between MovaLib and MovaCradle
    MovaCradleSetStatus(MOVAST_INIT);
    if ( MovaLibFormatRing(GInstance) == FALSE)
    {
        DiagLog(DC_WARNING, "%s: Unable to format MOVA ring buffer. Pausing instance: %d", __FUNCTION__, GInstance);
        PauseWithKeepalive();
    }

#ifdef    TRACE_MOVA_SOCKET
    bzero(dloadPath, sizeof(dloadPath));
    strcpy(dloadPath, FSH_BASE_DIR"/trace/dloadMova");
    strcat(dloadPath, gMovaEqId);

    g_dload_fd = creat(dloadPath, 777);
    if ( g_dload_fd == -1 )
    {
        diag_printf("MOVA unable to open file %s", dloadPath);
    }
#endif // TRACE_MOVA_SOCKET

    /* block all signals - inherited to all threads */
    sigfillset( &signal_set );
    pthread_sigmask( SIG_BLOCK, &signal_set, NULL );

    // DiagLog(DC_NOTE, "%s: MOVA:%s, Initialise multi-thread environment",__FUNCTION__, gMovaEqId);
    ThreadsInit();

    // attach backing store to MOVA memory

    MovaNonVol = MovaLibGetMovaShMemPtr() ;

    Tcomshr     = &MovaNonVol->stream[GInstance].Tcom;
    Terrlog     = &MovaNonVol->stream[GInstance].Terr;
    Tdinout     = &MovaNonVol->stream[GInstance].Tdout;
    nonVol      = &MovaNonVol->stream[GInstance].Dataset;
    LastMessage = &MovaNonVol->stream[GInstance].Message;

    if (MovaNonVol->StreamDataLoaded[GInstance] == FALSE)
    {
        DiagLog(DC_NOTE, "%s: MOVA:%s, COLD load initial dataset, MovaOn", __FUNCTION__, gMovaEqId);

        // set MOVA start state 999 == load from PROM
        Tcomshr->mark1 = 9999;

        // set state of MOVA On from config file
        if (movaOn == FALSE)
        {
            Tcomshr->io[27] = 0;
        }
        else
        {
            Tcomshr->io[27] = 1;
        Tcomshr->io[22] = 1;   /* flow on */     /* Automatically start logging */
        Tcomshr->io[25] = 1;   /* asston */

            // Mova control flag below.  Let's leave MOVA warm-up and switch itself
            // on control, so line below is commented out.
            // Tcomshr->io[19] = 1;
        }
        MovaNonVol->StreamDataLoaded[GInstance] = TRUE ;
    } // if backing store recreated
    else
    {
        // set MOVA start state, 1234 = RAM OK
        Tcomshr->mark1 = 1234;
    }

    // load default dataset
    BuildFilePath(sFilepath,CONF_DIR,gMovaDataset);
    status = PCDatSet_SetDefault(sFilepath);
    if (status == MV_SUCCESS)
    {
        DiagLog(DC_NOTE, "%s: MOVA:%s, MOVA loaded dataset %s",__FUNCTION__, gMovaEqId, gMovaDataset);
    }
    else
    {
        DiagLog(DC_NOTE, "%s: MOVA:%s, MOVA failed to load dataset %s",__FUNCTION__, gMovaEqId, gMovaDataset);
        // If we can't load a dataset MOVA will loop logging Dataset Empty errors, so pause the
        // MOVA stream.
        MovaCradleSetStatus(MOVAST_NO_DATASET);
        PauseWithKeepalive();
    }

    // Userbig contains a format string for the MOVA version
    bzero(tmpString, sizeof(tmpString));           // copy const string as strtok modifies it
    strncpy(tmpString, fmtVersion, CONST_LINE_LENGTH);
    tmpStrPtr = strtok(tmpString, "\'");            // find initial quote
    tmpStrPtr = strtok(NULL, "\'");                 // find substring
    if (tmpStrPtr != NULL)
    {
        strcpy(GIOshmData->mova_io[GInstance].MovaVer, tmpStrPtr);
    }

    // setup an interval timer to send a SIGALRM every 0.1 seconds
    // this will be caught by the signal handler thread and set the
    // appropriate condition variable

    Ginterval.it_interval.tv_sec  = 0;
    Ginterval.it_interval.tv_usec = 100000;

    Ginterval.it_value.tv_sec     = 0;
    Ginterval.it_value.tv_usec    = 100000;

    if (setitimer(ITIMER_REAL,&Ginterval,NULL) != 0)
    {
        DiagLog(DC_WARNING, "%s: MOVA:%s, Failed to start itimer %s",
                __FUNCTION__, gMovaEqId, strerror(errno)) ;
        return 0;
    }


    // Create independent threads each of which will execute function

    // the signal handler.  Signals are swiched off from main() so this
    // thread intercepts any signals and processes them for the MOVA
    // threads
    iret5 = pthread_create( &sig_thread, &Signal_attr,  sig_handler, (void*) message5);

    // as in PCMova this is the worker thead.  It is triggered 10 times
    // a second and calls for monitr(), detscn(), mova() and genstg()
    iret1 = pthread_create( &Mova_pthead, &Mova_attr,   monitrThread, (void*) message1);

    // this pcmova this thread is for displaying an output log of MOVA
    // activity and is triggered once a second
    iret4 = pthread_create( &Output_pthread, &Output_attr, outputThread, (void*) message4);

    // these two threads are the UI and should run continuously
    iret2 = pthread_create( &TrlUI_pthead, &TrlUI_attr, (void *) trluiThread, (void*) message2);
    iret3 = pthread_create( &Term_pthread, &Term_attr,  (void *) termThread,  (void*) message3);

    // additional thread created to handle remote downloads from Mova Cradle calls
    iret6 = pthread_create( &Dataset_thread, &Dataset_attr, (void *) Dataset_handler, (void*) message6);
    iret7 = pthread_create( &Aml_thread, &Aml_attr, (void *) Aml_handler, (void*) message7);

    // Wait till threads are complete before main continues. Unless we
    // wait we run the risk of executing an exit which will terminate
    // the process and all threads before the threads have completed.

    // These threads should not return unless cancelled
    pthread_join( Mova_pthead, NULL);
    diag_printf("monitr() thread returns %s", iret1);

    MovaCradleSetStatus(MOVAST_SHUT);

    pthread_join( Output_pthread, NULL);
    diag_printf("output() thread returns %s", iret4);

    pthread_join( TrlUI_pthead, NULL);
    diag_printf("TRLUserIF() thread returns %s", iret2);

    pthread_join( Term_pthread, NULL);
    diag_printf("term() thread returns %s", iret3);

    pthread_join( Dataset_thread, NULL);
    diag_printf("Dataset_handler() thread returns %s", iret6);

    pthread_join( Dataset_thread, NULL);
    diag_printf("Aml_handler() thread returns %s", iret7);

    // MOVA threads are gone so close down
    pthread_cancel( sig_thread);
    pthread_join( sig_thread, NULL);
    diag_printf("sig_handler() thread returns %s", iret5);

    ThreadsDestroy();
    MovaCradleSetStatus(MOVAST_NOTSTARTED);

    CMN_TRACE_EXIT;
    return EX_OK;
}   // main()



/* --------------------------------------------------------------------------- *
 *                   SUPPORT FUNCTIONS                                         *
 * --------------------------------------------------------------------------- *
*/

//!  \fn     monitrThread()
//!  \brief Thread for monitr(), detscn(), mova() and genstg()
//!
//!  \param  message thread message
//!  \retval  void none
//!
/*!
    \note from pctasks.c
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
*/
void *monitrThread(void * message)
{

    MVStatus status = MV_FAILURE;

#ifdef MOVA_CHECK_TASK_TIME
    struct tm *TimeNow;
    static struct timeval tvStart;
    static struct timeval tvSaved;
    static struct timeval tvEnd;
    static int timeDiffmS = 0;
    static int countDiagMessages = 0;
#endif /* MOVA_CHECK_TASK_TIME */

    int oldstate = 0;
    int oldtype  = 0;

    CMN_TRACE_ENTRY;

    pthread_setcanceltype(PTHREAD_CANCEL_ENABLE, &oldtype);
    pthread_setcancelstate(PTHREAD_CANCEL_DEFERRED, &oldstate);

    gettimeofday(&tvSaved,NULL);

    for (;;) // forever
    {
        waitfortick(); // wait for 0.1 seconds

#ifdef MOVA_CHECK_TASK_TIME
        // determine how log this routine is active
        gettimeofday(&tvStart,NULL);

        timeDiffmS = ((tvStart.tv_sec - tvSaved.tv_sec)*1000000 + tvStart.tv_usec - tvSaved.tv_usec)/1000;
        if (timeDiffmS > (MOVA_TOO_LONG +100))
        {
            if (countDiagMessages < 100)
            {
                DiagLog( DC_WARNING, "%s: MOVA:%s, too long between calls: %d", __FUNCTION__, gMovaEqId, timeDiffmS);
                countDiagMessages++;
            }
        }
        tvSaved.tv_sec = tvStart.tv_sec ;
        tvSaved.tv_usec = tvStart.tv_usec ;
#endif /* MOVA_CHECK_TASK_TIME */

        CmnUpdateAppAlive ();   // update ourselves

        monitr(0, "" , &gDirection);

#ifdef MOVA_CHECK_TASK_TIME
        // determine how log this routine is active
        gettimeofday(&tvEnd,NULL);
        timeDiffmS = ((tvEnd.tv_sec - tvStart.tv_sec)*1000000 + tvEnd.tv_usec - tvStart.tv_usec)/1000;
        if (timeDiffmS > MOVA_TOO_LONG)
        {
            if (countDiagMessages < 100)
            {
                DiagLog( DC_WARNING, "%s: MOVA:%s, too long in function: %d", __FUNCTION__, gMovaEqId, timeDiffmS);
                countDiagMessages++;
            }
        }
#endif /* MOVA_CHECK_TASK_TIME */

    } // for()

    CMN_TRACE_EXIT;  // Should never get here

    return EX_OK;
}   // monitrThread()



//!  \fn     termThread()
//!  \brief Thread to run terminal for UI
//!  \note this the threads term and TrlUI are paired
//!  \param  message thread message
//!  \retval  void none
//!
void *termThread(void * message)
{
    MVStatus status = MV_FAILURE;
    int oldstate = 0;
    int oldtype  = 0;

    CMN_TRACE_ENTRY;

    pthread_setcanceltype(PTHREAD_CANCEL_ENABLE, &oldtype);
    pthread_setcancelstate(PTHREAD_CANCEL_ASYNCHRONOUS, &oldstate);
    for (;;) // forever
    {
        //waitforlongtick(); // wait for 1 second
        diag_printf("calling term()");
        term(0, NULL);
    }

	// Should never get here
    CMN_TRACE_EXIT;

    return EX_OK;
}   // termThread()



//!  \fn     outputThread()
//!  \brief Thread to run output()
//!
//!  \param  message thread message
//!  \retval  void none
//!
void *outputThread(void * message)
{

    MVStatus status = MV_FAILURE;
    MESSAGE_ID MessageId = NO_MESSAGE;

    int oldstate = 0;
    int oldtype  = 0;

    CMN_TRACE_ENTRY;

    pthread_setcanceltype(PTHREAD_CANCEL_ENABLE, &oldtype);
    pthread_setcancelstate(PTHREAD_CANCEL_ASYNCHRONOUS, &oldstate);

    for (;;) // forever
    {
        waitforlongtick(); // wait for 1 second
        diag_printf("calling output()");
        output(0, NULL);
    }

	// Should never get here
    CMN_TRACE_EXIT;

    return EX_OK;
}   // outputThread()



//!  \fn     trluiThread()
//!  \brief Thread to run terminal for UI TTRLUserIF()
//!  \note this the threads term and TrlUI are paired
//!  \param  message thread message
//!  \retval  void none
//!
void *trluiThread(void * message)
{

    MVStatus status = MV_FAILURE;
    MESSAGE_ID MessageId = NO_MESSAGE;

    int oldstate = 0;
    int oldtype  = 0;

    CMN_TRACE_ENTRY;

    pthread_setcanceltype(PTHREAD_CANCEL_ENABLE, &oldtype);
    pthread_setcancelstate(PTHREAD_CANCEL_ASYNCHRONOUS, &oldstate);

    for (;;) // forever
    {
        //waitforlongtick(); // wait for 1 second
        diag_printf("calling TRLUserIF()");
        TRLUserIF(0, NULL);
    }

	// Should never get here
	CMN_TRACE_EXIT;

    return EX_OK;
}   // trluiThread()



//!  \fn     Exit()
//!  \brief  Exit the program, close and remove socket first
//!
//!  \note   (Called from CmnExit()). Setup: CmnRegisterExitFunction(Exit);
//!  \param  Argv Arguments (refer to main for description)
//!     threads
//!  \retval  void none
//!
static void Exit (void)
{
    CMN_TRACE_ENTRY;

    SktCloseAndRemoveSocket ("Shutting down module", GMySocket, GModule);
    DiagLog(DC_WARNING, "MOVA:%s, exit handler called", gMovaEqId);

    CancelMovaThreads();
    pthread_cancel(sig_thread);
    ThreadsDestroy();

#ifdef TRACE_MOVA_SOCKET
    if ( g_dload_fd > 0 )
    {
        diag_printf("MOVA exit handler closing file. fd=%d\n", g_dload_fd);
        close(g_dload_fd);
    }
#endif // TRACE_MOVA_SOCKET

    CMN_TRACE_EXIT;
} // Exit()



//!  \fn     Initialise()
//!  \brief Perform task initialisation
//!
//!  \param  Argc Number of arguments
//!  \param  Argv Arguments (refer to main for description)
//!     threads
//!  \retval  void none
//!
static void Initialise(int Argc, char *Argv[])
{
    DiagInit (GModule);

    // SIGTERMHandler and SIGCHLDHandler are now called by sig_handler
    // thread.
    CmnSetScheduler(MOVA_PRIORITY) ;

    CmnGetCLParam ("Initialise:getting instance number",Argc, Argv, 'i', &GInstance);

    CmnDiagnosticsSetup(GModule, GInstance);     // setup tracing without signal handlers
    CmnInitialiseTracing (Argc, Argv);           // Enable tracing

    CmnTrace (GTraceFileId, "MOVA:%s, Initialising", gMovaEqId);

    GMySocket = SktCreateSocket ("Initialise:Creating own socket",
            GModule, GInstance );

    // Get a pointer to shared memory
    GshmMem = CmnGetShm();

    CmnSendInitComplete("Initialise:Sending InitComplete",
            GMySocket, GModule, GInstance);

    CmnRegisterExitFunction (Exit);

    CMN_TRACE_EXIT;
}   // Initialise()


//!  \fn     sig_handler()
//!  \param arg
//!  \brief  enable signal handling and set condition variables for the other
//!     threads
//!  \retval  void none
//!
void* sig_handler( void* arg )
{
  sigset_t signal_set;
  int sig;
  int count = 0;
  int outputTrigger = OUTPUT_TRIGGER_TICKS;

  int oldstate = 0;
  int oldtype  = 0;
  mova_io_t *prMovaIO;
  int instance = 0;
  proc_if_led_t utcLedState = PROC_LED_OFF ;
  BOOLEAN movaOn = FALSE;

  CMN_TRACE_ENTRY;

  pthread_setcanceltype(PTHREAD_CANCEL_ENABLE, &oldtype);
  pthread_setcancelstate(PTHREAD_CANCEL_ASYNCHRONOUS, &oldstate);

  sigfillset(&signal_set);

  for(;;) // forever
  {
    sigwait( &signal_set,  &sig);

    /* when we get this far, we've caught a signal */

    // the definition for SIGRTMIN is a __libc call
    if (sig == SIGRTMIN)
    {
      diag_printf("received SIGRTMIN");
      DiagLog(DC_NOTE, "MOVA:%s, received thread cancel request", gMovaEqId);
      CancelMovaThreads();
    }
    else switch( sig )
    {
      case SIGALRM:
      diag_printf("received SIGALRM");
      pthread_mutex_lock(&tick_mutex);
      pthread_cond_signal(&tick);
      pthread_mutex_unlock(&tick_mutex);

      count++;
      if (count >= outputTrigger)
      {
        pthread_mutex_lock(&longtick_mutex);
        pthread_cond_signal(&longtick);
        pthread_mutex_unlock(&longtick_mutex);

        // use ported PCTasks* routines which control
        // the SuspendOutput condition variable.
        diag_printf("received %d SIGALARM's, now calling PCTasks_Resume()",count);
        PCTasks_Resume(MOVA_TASK_OUTPUT);
        count = 0;

        utcLedState = PROC_LED_OFF ;
        // process All MOVA streams from this stream
        for (instance = 0; instance < GIOshmData->numMovaIOEntries; instance++ )
        {
          // refresh the LED

          diag_printf("Movaon %d MovatTO %d utcLedState %d",
          GIOshmData->mova_io[instance].statusMovaOn,
          GIOshmData->mova_io[instance].statusMovaTakeOver,
          utcLedState) ;

          if (GIOshmData->mova_io[instance].statusMovaOn != FALSE &&
            utcLedState == PROC_LED_OFF)
          {
              utcLedState = PROC_LED_FLASH2 ; // Signal MOVA on if no stream faulty
          }

          if (GIOshmData->mova_io[instance].statusMovaOn != FALSE &&
              GIOshmData->mova_io[instance].statusMovaTakeOver == 0)
          {
              utcLedState = PROC_LED_FLASH1 ; // If stream required but off line signal fault
          }

              // Place slow per-stream scheduling events here
        }

        GIOshmData->utcLedCtrl.Mova = utcLedState ;

        // Place other slow this-stream based scheduling events here
        MonitorMovaStatusFault(GInstance);
        MonitorManualForce(GInstance);
      }
      break;

      case SIGUSR1:
        diag_printf("received SIGUSR1");
        CmnSIGUSR1Handler(sig);
        break;

      case SIGUSR2:
        diag_printf("received SIGUSR2");
        CmnSIGUSR2Handler(sig);
        break;

      case SIGCHLD:
        diag_printf("received SIGCHLD");
        CmnSIGCHLDHandler(sig);
        break;

      case SIGPWR:
        diag_printf("received SIGPWR");
        CmnSIGPWRHandler(sig);
        break;

      case SIGHUP:
        // fallthrough

      case SIGINT:
        // fallthrough

      case SIGTERM:
        diag_printf("received SIGTERM, or SIGINT or SIGHUP processing as SIGTERM");
        CmnSIGTERMHandler(sig);
        break;

      /* whatever you need to do for other signals */
      default:
        break;
    }
  } //forever
  CMN_TRACE_EXIT;
  return (void*)0;
} // sig_handler()



//!  \fn     Dataset_handler(void)
//!  \param arg
//!  \brief  enables asynchronous handling of download data from MovaLibDataset()
//!  \retval  void none
//!
void* Dataset_handler( void* arg )
{

    MVStatus status = MV_FAILURE;
    MESSAGE_ID MessageId = NO_MESSAGE;
    static const char cPaswrd[6] = {"AVOMGO"};
    mova_io_t *movaIo;

    int oldstate   = 0;
    int oldtype    = 0;

    CMN_TRACE_ENTRY;

    pthread_setcanceltype(PTHREAD_CANCEL_ENABLE, &oldtype);
    pthread_setcancelstate(PTHREAD_CANCEL_ASYNCHRONOUS, &oldstate);

    movaIo = &GIOshmData->mova_io[GInstance];

    // WRITE will be called later so init it now.
    WRITE(INITIALIZE);
    for (;;) // forever
    {
        waitforDatasetRequest();
        diag_printf("calling Data_Handler()");

        // clean up anything left over from a attempt
        // do it here so evidence is left in the pipe if Data_Handler()
        // fails
        MovaCradleFormatRcvPipe(GInstance);

        movaIo->DatasetThreadActive = TRUE;
        Data_Handler( MODEM_PORT, (char*)cPaswrd );
        movaIo->DatasetThreadActive = FALSE;
    }

	// Should never get here
	CMN_TRACE_EXIT;

    return EX_OK;
} // Dataset_handler()


//!  \fn     Aml_handler(void)
//!  \param arg
//!  \brief  enables asynchronous handling of aml messages
//!  \retval  void none
//!
void* Aml_handler( void* arg )
{

    MVStatus status = MV_FAILURE;
    MESSAGE_ID MessageId = NO_MESSAGE;
    static const char cPaswrd[6] = {"AVOMGO"};
    mova_io_t *movaIo;

    int oldstate   = 0;
    int oldtype    = 0;

    CMN_TRACE_ENTRY;

    pthread_setcanceltype(PTHREAD_CANCEL_ENABLE, &oldtype);
    pthread_setcancelstate(PTHREAD_CANCEL_ASYNCHRONOUS, &oldstate);

    movaIo = &GIOshmData->mova_io[GInstance];

    // WRITE will be called later so init it now.
    WRITE(INITIALIZE);
    for (;;) // forever
    {
        waitforAmlRequest();
        diag_printf("calling Aml_Handler()");

        // clean up anything left over from a attempt
        // do it here so evidence is left in the pipe if Data_Handler()
        // fails
        MovaCradleFormatRcvPipe(GInstance);

        movaIo->DatasetThreadActive = TRUE;
        Data_Handler( MODEM_PORT, (char*)cPaswrd );
        movaIo->DatasetThreadActive = FALSE;
    }

	// Should never get here
	CMN_TRACE_EXIT;

    return EX_OK;
} // Aml_handler()



//!  \fn     ThreadsInit(void)
//!  \brief  Initialise pthreads datastructures
//!  \retval  void none
//!
void ThreadsInit()
{
    size_t stacksize = 0;
    int status = 0;
    int policy;
    struct sched_param sched;

    // comments below from PCTasks_Initialise() from PCMova

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

    // initialize thread attributes

    pthread_attr_init(&Mova_attr);
    pthread_attr_init(&TrlUI_attr);
    pthread_attr_init(&Term_attr);
    pthread_attr_init(&Output_attr);
    pthread_attr_init(&Signal_attr);
    pthread_attr_init(&Dataset_attr);
    pthread_attr_init(&Aml_attr);

    pthread_attr_getstacksize(&Mova_attr, &stacksize);
    // diag_printf("MOVA thread stacksize = %d",stacksize);

    // double stack size for all threads

/*    stacksize = stacksize *2;
    pthread_attr_setstacksize(&Mova_attr,   stacksize);
    pthread_attr_setstacksize(&TrlUI_attr,  stacksize);
    pthread_attr_setstacksize(&Term_attr,   stacksize);
    pthread_attr_setstacksize(&Output_attr, stacksize);
    pthread_attr_setstacksize(&Signal_attr, stacksize);
    pthread_attr_setstacksize(&Dataset_attr,  stacksize);
*/
    // set thread detached state and priority here if required (lets keep them the same for now)

    pthread_attr_setdetachstate(&Mova_attr,   PTHREAD_CREATE_JOINABLE);
    pthread_attr_setdetachstate(&TrlUI_attr,  PTHREAD_CREATE_JOINABLE);
    pthread_attr_setdetachstate(&Term_attr,   PTHREAD_CREATE_JOINABLE);
    pthread_attr_setdetachstate(&Output_attr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setdetachstate(&Signal_attr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setdetachstate(&Dataset_attr,  PTHREAD_CREATE_JOINABLE);
    pthread_attr_setdetachstate(&Aml_attr,  PTHREAD_CREATE_JOINABLE);

    pthread_attr_getschedpolicy(&Mova_attr,   &policy);
    // diag_printf("MOVA thread policy = %d", policy);

    // default Linux thread policy is: SCHED_OTHER
    pthread_attr_setschedpolicy(&Mova_attr,   SCHED_FIFO);
    pthread_attr_setschedpolicy(&TrlUI_attr,  SCHED_FIFO);
    pthread_attr_setschedpolicy(&Term_attr,   SCHED_FIFO);
    pthread_attr_setschedpolicy(&Output_attr, SCHED_FIFO);
    pthread_attr_setschedpolicy(&Signal_attr, SCHED_FIFO);
    pthread_attr_setschedpolicy(&Dataset_attr,  SCHED_FIFO);
    pthread_attr_setschedpolicy(&Aml_attr,  SCHED_FIFO);

//    pthread_attr_getschedpolicy(&Mova_attr,   &policy);
    // diag_printf("MOVA thread policy = %d", policy);

//    pthread_attr_getschedparam(&Mova_attr,    &sched);
    // diag_printf("MOVA thread priority = %d", sched.__sched_priority);

    // for the moment don't change the thread priority
    // comments above show PCMOVA & SIEMENS priorities
    sched.sched_priority = MOVA_PRIORITY  ;

    pthread_attr_setschedparam(&Mova_attr,   &sched);
    pthread_attr_setschedparam(&TrlUI_attr,  &sched);
    pthread_attr_setschedparam(&Term_attr,   &sched);
    pthread_attr_setschedparam(&Output_attr, &sched);
    pthread_attr_setschedparam(&Signal_attr, &sched);
    pthread_attr_setschedparam(&Dataset_attr, &sched);
    pthread_attr_setschedparam(&Aml_attr, &sched);

    pthread_attr_setinheritsched(&Mova_attr,   PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setinheritsched(&TrlUI_attr,  PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setinheritsched(&Term_attr,   PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setinheritsched(&Output_attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setinheritsched(&Signal_attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setinheritsched(&Dataset_attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setinheritsched(&Aml_attr, PTHREAD_EXPLICIT_SCHED);



// initialize condition variables

    pthread_condattr_init(&SuspOut_attr);
    pthread_cond_init(&SuspendOutput, &SuspOut_attr);
    pthread_condattr_init(&tick_attr);
    pthread_cond_init(&tick, &tick_attr);
    pthread_condattr_init(&longtick_attr);
    pthread_cond_init(&longtick, &tick_attr);
    pthread_condattr_init(&GetOutput_attr);
    pthread_cond_init(&GetOutput, &GetOutput_attr);
    pthread_condattr_init(&DatasetActive_attr);
    pthread_cond_init(&DatasetActive, &AmlActive_attr);
    pthread_condattr_init(&AmltActive_attr);
    pthread_cond_init(&AmlActive, &AmlActive_attr);

    // initialize mutexes

    // Use a normal mutex which has no error checking as its
    // locking and unlocking are tightly coupled.
    pthread_mutexattr_init(&SuspOut_mutex_attr);
    pthread_mutexattr_init(&tick_mutex_attr);
    pthread_mutexattr_init(&longtick_mutex_attr);
    pthread_mutexattr_init(&GetOutput_mutex_attr);
    pthread_mutexattr_init(&Ringbuf_mutex_attr);
    pthread_mutexattr_init(&Dataset_mutex_attr);
    pthread_mutexattr_init(&TrlUi_mutex_attr);
    pthread_mutexattr_init(&Aml_mutex_attr);

    // Linux threads uses a non-standard normal mutex
    pthread_mutexattr_settype(&SuspOut_mutex_attr, PTHREAD_MUTEX_ERRORCHECK_NP);
    pthread_mutex_init(&SuspOut_mutex, &SuspOut_mutex_attr);
    pthread_mutexattr_settype(&tick_mutex_attr, PTHREAD_MUTEX_ERRORCHECK_NP);
    pthread_mutex_init(&tick_mutex, &tick_mutex_attr);
    pthread_mutexattr_settype(&longtick_mutex_attr, PTHREAD_MUTEX_ERRORCHECK_NP);
    pthread_mutex_init(&longtick_mutex, &tick_mutex_attr);
    pthread_mutexattr_settype(&GetOutput_mutex_attr, PTHREAD_MUTEX_ERRORCHECK_NP);
    pthread_mutex_init(&GetOutput_mutex, &GetOutput_mutex_attr);
    pthread_mutexattr_settype(&Ringbuf_mutex_attr, PTHREAD_MUTEX_ERRORCHECK_NP);
    pthread_mutex_init(&Ringbuf_mutex, &Ringbuf_mutex_attr);
    pthread_mutexattr_settype(&Dataset_mutex_attr, PTHREAD_MUTEX_ERRORCHECK_NP);
    pthread_mutex_init(&Dataset_mutex, &Dataset_mutex_attr);
    pthread_mutexattr_settype(&TrlUi_mutex_attr, PTHREAD_MUTEX_ERRORCHECK_NP);
    pthread_mutex_init(&TrlUi_mutex, &TrlUi_mutex_attr);
    pthread_mutexattr_settype(&Aml_mutex_attr, PTHREAD_MUTEX_ERRORCHECK_NP);
    pthread_mutex_init(&Aml_mutex, &Aml_mutex_attr);
} // ThreadsInit()



//!  \fn     ThreadsDestroy(void)
//!  \brief  Cleanup pthreads datastructures
//!  \retval  void none
//!
void ThreadsDestroy()
{
    // destroy thread attributes

    pthread_attr_destroy(&Mova_attr);
    pthread_attr_destroy(&TrlUI_attr);
    pthread_attr_destroy(&Term_attr);
    pthread_attr_destroy(&Output_attr);
    pthread_attr_destroy(&Signal_attr);
    pthread_attr_destroy(&Dataset_attr);
    pthread_attr_destroy(&Aml_attr);

    // destroy condition variables

    pthread_condattr_destroy(&SuspOut_attr);
    pthread_cond_destroy(&SuspendOutput);
    pthread_condattr_destroy(&tick_attr);
    pthread_cond_destroy(&tick);
    pthread_condattr_destroy(&longtick_attr);
    pthread_cond_destroy(&longtick);
    pthread_condattr_destroy(&GetOutput_attr);
    pthread_cond_destroy(&GetOutput);
    pthread_condattr_destroy(&DatasetActive_attr);
    pthread_cond_destroy(&DatasetActive);
    pthread_condattr_destroy(&AmlActive_attr);
    pthread_cond_destroy(&AmlActive);

    // destroy mutexes

    pthread_mutexattr_destroy(&SuspOut_mutex_attr);
    pthread_mutex_destroy(&SuspOut_mutex);
    pthread_mutexattr_destroy(&tick_mutex_attr);
    pthread_mutex_destroy(&tick_mutex);
    pthread_mutexattr_destroy(&longtick_mutex_attr);
    pthread_mutex_destroy(&longtick_mutex);
    pthread_mutexattr_destroy(&GetOutput_mutex_attr);
    pthread_mutex_destroy(&GetOutput_mutex);
    pthread_mutexattr_destroy(&Ringbuf_mutex_attr);
    pthread_mutex_destroy(&Ringbuf_mutex);
    pthread_mutexattr_destroy(&Dataset_mutex_attr);
    pthread_mutex_destroy(&Dataset_mutex);
    pthread_mutexattr_destroy(&TrlUi_mutex_attr);
    pthread_mutex_destroy(&TrlUi_mutex);
    pthread_mutexattr_destroy(&Aml_mutex_attr);
    pthread_mutex_destroy(&Aml_mutex);
} // ThreadsDestroy()



//!  \fn     CancelMovaThreads(void)
//!  \brief  Cancel MOVA threads
//!  \retval  void none
//!
void CancelMovaThreads()
{
    // cancel only MOVA threads +additional Dataset_thread as the sig_handler
    // thread will be calling this function

    pthread_cancel( Output_pthread);
    pthread_cancel( TrlUI_pthead);
    pthread_cancel( Term_pthread);
    pthread_cancel( Mova_pthead);
    pthread_cancel( Dataset_thread);
    pthread_cancel( Aml_thread);

} // CancelMovaThreads()



//!  \fn     PauseWithKeepalive(void)
//!  \brief  A pause() will cause the chameleon subsystem to notice the process
//!     is frozen so loop notifying the chameleon
//!
//!  \note Global variable readytogo
//!    flag == 0 => loop, !=0 => continue
//!  \retval  void none
//!
void PauseWithKeepalive()
{
    while(readytogo == 0)
    {
        CmnUpdateAppAlive();   // update ourselves
        sleep(1);
    }
} // PauseWithKeepalive()
