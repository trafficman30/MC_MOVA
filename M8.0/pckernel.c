/*
    Name:           PCKernel
    Author (Date):  ihenderson, 01/10/05
    Platform:       PC
    Purpose:        PCKernel provides top-level functions for
                    initialising, running, and terminating PCMOVA.
                    These are called from PCMOVA.cpp.

                    Synchronisation Note:
                    PCKernel is not thread-safe - all calls to PCKernel
                    should be made from the same thread.

    Last revision:  $Revision:: 5                           $
    Last changed:   $Date:: 12/04/06 16:07                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: pckernel.c $
    
    *****************  Version 5  *****************
    User: Ihenderson   Date: 12/04/06   Time: 16:07
    Updated in $/PCMOVA
    
    *****************  Version 4  *****************
    User: Ihenderson   Date: 16/01/06   Time: 9:55
    Updated in $/PCMOVA
    
    *****************  Version 3  *****************
    User: Ihenderson   Date: 16/12/05   Time: 10:21
    Updated in $/PCMOVA
    
    *****************  Version 2  *****************
    User: Ihenderson   Date: 5/12/05    Time: 11:05
    Updated in $/PCMOVA
    
    *****************  Version 1  *****************
    User: Ihenderson   Date: 28/11/05   Time: 11:49
    Created in $/PCMOVA
*/

/*
    TODO:
*/

#include "stdafx.h"

#include "pckernel.h"
#include "pcioscan.h"
#include "pcuserio.h"
#include "pccontio.h"
#include "pcclock.h"
#include "pcdatset.h"
#include "pctasks.h"
#include "kdebug.h"
#include "xml_dataset.h"


#if defined (TRL_INTEGRATION_TEST)

#include "tma_preprint.h"
#include "generalfunc.h"

#endif

#define g_MODULE_NAME    ("PCKernel")


static MVBool   g_isInitialised = MV_FALSE;

static MVStatus PCKernel_Handshake( MVInt );


/*
    Name:           PCKernel_Initialise
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Initialises the PCMOVA Kernel
    Inputs:         controller type and end point
                    (e.g. IP address and port number for TCP,
                    but may be serial port info in future
                    or IP address/port for SNMP)
                    user communications type (e.g. TCP or serial)
                    user comms end point (may be NULL)
                    unique junction ID [0-255]
                    default dataset filename including full path
                    (may be NULL)
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for 
                    further error information.
    Remarks:
*/
MVStatus PCKernel_Initialise( CommsType controllerComms, 
                              const char * controllerEndPoint, 
                              CommsType userComms,
                              const char * userEndPoint, 
                              MVInt junctionID,
                              const char * datasetDefault,
							  const char * controllerId)
{   
    MVStatus   status = MV_SUCCESS;
    
    TRACE_ASSERT( !g_isInitialised, "PC Kernel module already initialised." );
    TRACE_ASSERT( MATH_IS_IN_RANGE( junctionID, xg_JUNCTION_ID_MIN, xg_JUNCTION_ID_MAX ),
        "Junction ID out of range." );
    TRACE_ASSERT( MATH_IS_IN_RANGE( controllerComms, COMMS_TYPE_FIRST, COMMS_TYPE_LAST ),
        "Controller communications type invalid." );
    TRACE_ASSERT( MATH_IS_IN_RANGE( userComms, COMMS_TYPE_FIRST, COMMS_TYPE_LAST ),
        "User communications type invalid." );
    TRACE_ASSERT_VALID_ADDRESS( controllerEndPoint, "No controller end point." );

    TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "Initialising MOVA kernel..." );

    /* Initialise PCMOVA error handling module */
    PCError_Initialise( );

    if ( controllerEndPoint != NULL
        && MATH_IS_IN_RANGE( junctionID, xg_JUNCTION_ID_MIN, xg_JUNCTION_ID_MAX ) 
        && MATH_IS_IN_RANGE( controllerComms, COMMS_TYPE_FIRST, COMMS_TYPE_LAST )
        && MATH_IS_IN_RANGE( userComms, COMMS_TYPE_FIRST, COMMS_TYPE_LAST ) )
    {

#ifdef M8_ENABLED
		XML_DataSet_Initialise();
#else
		PCDatSet_Initialise();
#endif
        
        if ( datasetDefault != NULL )
        {
#ifdef M8_ENABLED
            status = XML_DataSet_ReadAllDataPlansFromFile( datasetDefault, controllerId);
#else
			status = PCDatSet_SetDefault( datasetDefault );
#endif
        }

        if ( status == MV_SUCCESS )
        {
            status = PCClock_Initialise( );
        }       

        if ( status == MV_SUCCESS )
        {   
            status = PCIOScan_Initialise( );
        }

        if ( status == MV_SUCCESS )
        {   
            /* Initialise user terminal IO */
            status = PCUserIO_Initialise( userComms, userEndPoint );
        }

        if ( status == MV_SUCCESS )
        {
            /* Initialise controller IO */
            status = PCContIO_Initialise( controllerComms, controllerEndPoint );
        }

        if ( status == MV_SUCCESS )
        {
            /* Carry out handshaking with the controller */
            status = PCKernel_Handshake( junctionID );
        }
        
        if ( status == MV_SUCCESS )
        {    
            /* Initialise the tasks that make up the MOVA kernel 
            (monitr, term, TRLUserIF, Output) */
            status = PCTasks_Initialise( );
        }

    } /* If function parameters are valid */

    else
    {
        PCError_SetLast( PCMOVA_ERROR_PARAM_INVALID, NULL, g_MODULE_NAME );
        status = MV_FAILURE;
    }    

    if ( status == MV_SUCCESS )
    {
        g_isInitialised = MV_TRUE;
        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
            "MOVA kernel initialised." );
    }

    return ( status );

} /* PCKernel_Initialise */


/*
    Name:           PCKernel_DeInitialise
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Deinitialises the PCMOVA Kernel
    Inputs:         
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for 
                    further error information.
    Remarks:
*/
MVStatus PCKernel_DeInitialise( void )
{
    MVStatus status = MV_SUCCESS;
    
    if ( g_isInitialised )
    {
        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
            "Deinitialising MOVA kernel..." );

#if defined (TRL_INTEGRATION_TEST)
		/* Print TMA logs for system tests */			
		print_FlowLog(pTmaData , SYS_LOG_PRINTING, Tcomshr->nlanes);
		print_OccupancyLog(pTmaData, SYS_LOG_PRINTING, Tcomshr->stages);
		print_TraSevLog(pTmaData, SYS_LOG_PRINTING,Tcomshr->stages);
		print_PedSevLog(pTmaData, SYS_LOG_PRINTING,xgnREDPED_MAX,&Tcomshr->redped[0]);
		print_EndSatLog(pTmaData, SYS_LOG_PRINTING, Tcomshr->stages);
		print_OversatLog(pTmaData, SYS_LOG_PRINTING, Tcomshr->nlanes);
		print_FaultyDetecLog(pTmaData, SYS_LOG_PRINTING);
#endif

        /* Shut down the MOVA kernel (monitr, term, TRLUserIF, Output) */
        status = PCTasks_DeInitialise();
        DEBUG_ASSERT( status == MV_SUCCESS );

        // Controller communications
        status = PCContIO_DeInitialise( );
        DEBUG_ASSERT( status == MV_SUCCESS );

        // User terminal communications
        status = PCUserIO_DeInitialise();
        DEBUG_ASSERT( status == MV_SUCCESS );

        status = PCIOScan_DeInitialise();
        DEBUG_ASSERT( status == MV_SUCCESS );
        
        status = PCClock_DeInitialise( );
        DEBUG_ASSERT( status == MV_SUCCESS );

#if M8_ENABLED
		XML_DataSet_DeInitialise();
#else
		PCDatSet_DeInitialise();
#endif  
     
        if ( status == MV_SUCCESS )
        {
            g_isInitialised = MV_FALSE;
            TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
                "MOVA kernel deinitialised." );            
        }        
    }

    return ( status );

} /* PCKernel_DeInitialise */


/*
    Name:           PCKernel_IsInitialised
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Determines whether the PCMOVA Kernel module 
                    is initialised.
    Inputs:         
    Returns:        True if module initialised; False otherwise.
    Remarks:
*/
MVBool PCKernel_IsInitialised( void )
{
    return ( g_isInitialised );

} /* PCKernel_IsInitialised */


/*
    Name:           PCKernel_Run
    Author (Date):  ihenderson, 24/11/05
    Platform:       PC
    Purpose:        PCKernel_Run starts and runs PCMOVA and the MOVA Kernel.
                    It acts as a top-level "game loop" for PCMOVA, which is 
                    driven by messages received from the Controller.
                    E.g. The MOVA Kernel is run whenever an Update message is 
                    received from the controller (Update messages contain 
                    0.1 seconds worth of detector and confirm data).
                    PCKernel_Run only returns when a Terminate message is 
                    received from the controller, or if an error occurs.
    Inputs:         
    Returns:        Status: Success = PCMOVA completed successfully 
                    (in response to a Terminate message from the controller)
                    Failure = An error occurred while running PCMOVA.
                    Call PCError_GetLast... for further error information.
    Remarks:
*/
MVStatus PCKernel_Run( void )
{
    MessageType msgTypeNext = MSG_TYPE_INVALID;
    MVStatus    status = MV_SUCCESS;

    TRACE_ASSERT( g_isInitialised, "PC Kernel module not initialised." );
    
#if 0
    // TEMP: TEST: Get MOVA kernel to initialise without requiring an Initialise message
    PCTasks_Start( );
    PCIOScan_EnableMOVAControl( MV_TRUE );
    while ( status == MV_SUCCESS )
    {
        // TEMP: TEST: Get MOVA kernel to update without requiring an Update message
        // after a sleep so we don't run the kernel ridiculously fast
        // Rate ~= real-time.
        Sleep( 100 );
        PCClock_Tick( 100 );
        status = PCTasks_Update( );
    }
    return ( status );
#endif

    /* Process messages until an error occurs or the Controller 
       requests termination */
    while ( status == MV_SUCCESS && msgTypeNext != MSG_TYPE_TERMINATE )
    {    
        /* Wait for the next message from the controller */
        msgTypeNext = PCContIO_Peek();

        switch ( msgTypeNext )
        {
            case MSG_TYPE_INITIALISE:
            {                
                MVUInt32    date, time;
                char        userEndPoint[ xg_END_POINT_LEN_MAX ];
                MVSize      userEndPointLen;

                TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
                    "Processing initialise message..." );

                /* Receive the initialise request data from the controller */
                PCContIO_ReceiveInitialise( &date, &time );
                
                /* Set the PCMOVA clock */
                status = PCClock_SetDateTime( date, time );

                if ( status == MV_SUCCESS )
                {
                    /* Initialise the MOVA Kernel (by initialising the tasks that 
                    make up the kernel).  This starts the UI threads, allowing 
                    the user to connect with MOVA Comm. */

                    status = PCTasks_Start( );

                }

                if ( status == MV_SUCCESS )
                {
                    /* Set MOVA so that it can take control when it wants to 
                    (after a warmup) */
                    PCIOScan_EnableMOVAControl( MV_TRUE );

                    userEndPointLen = PCUserIO_GetEndPoint( 
                        &( userEndPoint[0] ), xg_END_POINT_LEN_MAX );
                    TRACE_ASSERT( userEndPointLen > 0, "No user end point." );

                    /* Send the initialise reply data to the controller */
                    status = PCContIO_SendInitialise( 
                        userEndPoint, userEndPointLen );
                }

                if ( status == MV_SUCCESS )
                {
                    TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
                        "Initialise message processed." );
                }

                break;
            }

            case MSG_TYPE_UPDATE:
            {
                IOPort detectors[ xg_DETECTOR_PORTS_MAX ];
                IOPort confirms[ xg_CONFIRM_PORTS_MAX ];
                IOPort forces[ xg_FORCE_PORTS_MAX ];
                MVBool isControllerReady;
                MVBool isTakeOver;

				IOPort isHoldSignalOn;
				IOPort isReleaseSignalOn;


                //DEBUG_WRITE0( "Processing update message..." );

                /* Receive the update request data from the controller */
                PCContIO_ReceiveUpdate( &( detectors[0] ), &( confirms[0] ), 
                    &isControllerReady );

                /* Set the input ports with the update request data */
                PCIOScan_SetInputs( &( detectors[0] ), &( confirms[0] ), 
                    isControllerReady );

                /* Update the PCMOVA clock - a tenth of a second 
                (in simulation time) should pass between updates */
                PCClock_Tick( 100 );

                /* Run the MOVA Kernel to generate output data */
                status = PCTasks_Update( );

                if ( status == MV_SUCCESS )
                {    
                    /* Get the update reply data from the output ports */
                    PCIOScan_GetOutputs( &( forces[0] ), &isTakeOver, &isHoldSignalOn, &isReleaseSignalOn);

                    /* Send the update reply data to the controller */
                    status = PCContIO_SendUpdate( &( forces[0] ), isTakeOver, isHoldSignalOn, isReleaseSignalOn);
                }

                if ( status == MV_SUCCESS )
                {
                    //DEBUG_WRITE0( "Update message processed." );
                }

                break;
            }

            case MSG_TYPE_TERMINATE:
            {
                TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
                    "Processing terminate message..." );

                /* Send reply to tell the controller to wait 
                for PCMOVA to terminate. */
                status = PCContIO_SendTerminate( );

                if ( status == MV_SUCCESS )
                {
                    TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
                        "Terminate message processed." );
                }

                break;
            }
            
            case MSG_TYPE_INVALID:
            {
                /* Something has gone wrong - safest to quit.
                If we get an invalid message type, an error
                will have been set at a lower level so no need here */
                status = MV_FAILURE;
                break;
            }

            default:
            {
                /* Something has gone wrong - safest to quit */
                PCError_SetLast( PCMOVA_ERROR_MSG_TYPE, NULL, g_MODULE_NAME );
                status = MV_FAILURE;
                break;
            }

        } /* Switch message type */

    } /* While processing messages */

    return ( status );

} /* PCKernel_Run */


/*
    Name:           PCKernel_Handshake
    Author (Date):  ihenderson, 24/11/05
    Platform:       PC
    Purpose:        Carries out the process of handshaking with the controller,
                    by comparing the junction ID passed to this instance of 
                    PCMOVA as a command line parameter against the junction ID
                    associated with this instance by the controller.
    Inputs:         The ID of the junction PCMOVA is controlling.
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for further error information.
    Remarks:        Called during PCMOVA kernel initialisation.
*/
static MVStatus PCKernel_Handshake( MVInt junctionID )
{
    MVStatus        status;
    MessageType     msgTypeNext;
    MVInt           controllerJunctionID = 0;

    TRACE_ASSERT( !g_isInitialised, "PC Kernel already initialised." );
    TRACE_ASSERT( MATH_IS_IN_RANGE( junctionID, xg_JUNCTION_ID_MIN, xg_JUNCTION_ID_MAX ),
        "Junction ID out of range." );
    
    TRACE_WRITE1_IF( LEVEL_INFO, g_MODULE_NAME, 
        "Handshaking for junction with ID %d...", junctionID );

    status = PCContIO_SendHandshake( junctionID );
    if ( status == MV_SUCCESS )
    {    
        /* Wait for the next message */
        msgTypeNext = PCContIO_Peek();
        if ( msgTypeNext == MSG_TYPE_HANDSHAKE )
        {
            PCContIO_ReceiveHandshake( &controllerJunctionID );
        }
        else
        {                   
            status = MV_FAILURE;

            /* Only set a message type error if the type 
            is valid but wrong.  If it was invalid, an error
            will have been set at a lower level */
            if ( msgTypeNext != MSG_TYPE_INVALID )
            {
                PCError_SetLast( PCMOVA_ERROR_MSG_TYPE, NULL, g_MODULE_NAME );
            }
        }
    }

    if ( status == MV_SUCCESS )
    {
        if ( controllerJunctionID == junctionID )
        {            
            TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
                "Handshaking completed successfully." );
        }
        else
        {
            PCError_SetLast( PCMOVA_ERROR_HANDSHAKE, NULL, g_MODULE_NAME );
            status = MV_FAILURE;
        }
    }

    return ( status );

} /* PCKernel_Handshake */
