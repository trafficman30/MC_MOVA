/*
    Name:           PCIOScan.c
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Provides input and output "ports" which PCMOVA
                    uses to store inputs (detectors and confirms)
                    and outputs (forces).
                    The MOVA kernel sends and retrieves data from
                    the ports using the InputScan and OutputScan functions 
                    (see IOScan.c for the embedded version equivalents).

                    Synchronisation Note:
                    OutputScan can be called from the term() thread and
                    TRLUserIF() thread - access to the force output ports
                    has therefore been serialised.
                    InputScan is only ever called by the main thread
                    via monitr (which is now in the main thread).

    Last revision:  $Revision:: 6                           $
    Last changed:   $Date:: 19/05/06 13:28                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: pcioscan.c $
 * 
 * *****************  Version 6  *****************
 * User: Ihenderson   Date: 19/05/06   Time: 13:28
 * Updated in $/PCMOVA
    
    *****************  Version 5  *****************
    User: Ihenderson   Date: 12/04/06   Time: 16:07
    Updated in $/PCMOVA
    
    *****************  Version 4  *****************
    User: Ihenderson   Date: 9/03/06    Time: 14:06
    Updated in $/PCMOVA
    
    *****************  Version 3  *****************
    User: Ihenderson   Date: 16/12/05   Time: 10:19
    Updated in $/PCMOVA
    
    *****************  Version 2  *****************
    User: Ihenderson   Date: 5/12/05    Time: 11:03
    Updated in $/PCMOVA
    
    *****************  Version 1  *****************
    User: Ihenderson   Date: 28/11/05   Time: 11:49
    Created in $/PCMOVA
*/

/*
    TODO:
*/


#include "stdafx.h"
#include <memory.h> /* For memset and memcpy */

#include "pcioscan.h"
#include "gbltypes.h" /* For detsin, confin, mxdets, mxconf etc. */

/*TODO: Check if we need to include this here*/
#include "mova_op.h"


#define g_MODULE_NAME                   ("PCIOScan")

/* Index of the CRB in the MOVA kernel confirms array (confin) */
#define g_CONTROLLER_READY_BIT_INDEX    (mxconf - 1)

/* Index of the HI/TO in the MOVA kernel control array (defined in Genstg.c) */
#define g_MOVA_TAKE_OVER_BIT_INDEX      (mxstag)

/* Index of the flag in the Tcomshr->io[] array 
that indicates whether MOVA control is enabled */
#define g_MOVA_ENABLED_IO_FLAG          (27)


static IOPort   g_forcePorts[ xg_FORCE_PORTS_MAX ];
static IOPort   g_detectorPorts[ xg_DETECTOR_PORTS_MAX ];
static IOPort   g_confirmPorts[ xg_CONFIRM_PORTS_MAX ];
static MVBool   g_isControllerReady;
static MVBool   g_isMOVATakeOver;
static MVBool   g_isInitialised = MV_FALSE;

#ifdef M8_IMPROVED_LINKING
/*The following vars are used for the backward linking.*/
static MVBool   g_isHoldSignalOn = MV_FALSE;
static MVBool   g_isReleaseSignalOn = MV_FALSE;
#endif


/* Used to synchronise access to the force output ports - 
OutputScan can be called by the MOVA kernel 
TRLUserIF/term threads as well as the main thread. */
static CRITICAL_SECTION    g_criticalSection;


static void PCIOScan_InitialiseInputs( void );
static void PCIOScan_InitialiseOutputs( void );
static void PCIOScan_ReadBits( char *, MVInt *, MVInt, MVInt, IOPort );
static void PCIOScan_WriteBits( const char *, MVInt *, MVInt, MVInt, IOPort * );
static void PCIOScan_UpdateConfirmPolarity( void );
static void PCIOScan_UpdateDetectorPolarity( void );


/*
    Name:           PCIOScan_Initialise
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Initialises the PCMOVA I/O scanning module.
    Inputs:         
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for 
                    further error information.
    Remarks:
*/
MVStatus PCIOScan_Initialise( void )
{
    MVStatus    status = MV_SUCCESS;

    TRACE_ASSERT( !g_isInitialised, 
        "PC IO scanner module already initialised."  );

    TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
        "Initialising IO scanner..." );

    /* Delete the critical section for synchronising access to force ports */
    InitializeCriticalSection( &g_criticalSection );

    PCIOScan_InitialiseInputs( );
    PCIOScan_InitialiseOutputs( );

    g_isInitialised = MV_TRUE;

    TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
        "IO scanner initialised." );

    return ( status );

} /* PCIOScan_Initialise */


/*
    Name:           PCIOScan_IsInitialised
    Author (Date):  ihenderson, 12/10/05
    Platform:       PC
    Purpose:        Determines whether the IO scanning module is initialised.
    Inputs:         
    Returns:        True if module initialised; False otherwise.
    Remarks:
*/
MVBool PCIOScan_IsInitialised( void )
{
    return ( g_isInitialised );

} /* PCIOScan_IsInitialised */


/*
    Name:           PCIOScan_DeInitialise
    Author (Date):  ihenderson, 04/11/05
    Platform:       PC
    Purpose:        Deinitialises the PCMOVA I/O scanning module.
    Inputs:         
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for 
                    further error information.
    Remarks:
*/
MVStatus PCIOScan_DeInitialise( void )
{
    MVStatus status = MV_SUCCESS;

    if ( g_isInitialised )
    {
        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
            "Deinitialising IO scanner..." );
        
        /* Delete the critical section for synchronising access to force ports */
        DeleteCriticalSection( &g_criticalSection );

        g_isInitialised = MV_FALSE;

        TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, 
            "IO scanner deinitialised." );
    }

    return ( status );

} /* PCIOScan_DeInitialise */


/*
    Name:           PCIOScan_EnableMOVAControl
    Author (Date):  ihenderson, 04/11/05
    Platform:       PC
    Purpose:        Sets the state of the MOVA enabled flag in the MOVA kernel.
                    Enabling MOVA allows MOVA to take control when it's ready
                    providing the controller ready bit (CRB) is set.
    Inputs:         Whether MOVA should be enabled.
    Returns:        Status (success/failure)
                    Call PCError_GetLast... for 
                    further error information.
    Remarks:
*/
MVBool PCIOScan_EnableMOVAControl( MVBool isEnable )
{
    MVBool wasEnabled = 
        ( Tcomshr->io[ g_MOVA_ENABLED_IO_FLAG ] == 0 ? MV_FALSE : MV_TRUE );
    
    Tcomshr->io[ g_MOVA_ENABLED_IO_FLAG ] = ( isEnable ? 1 : 0 );

    return ( wasEnabled );

} /* PCIOScan_EnableMOVAControl */


/*
    Name:           PCIOScan_IsMOVAControlEnabled
    Author (Date):  ihenderson, 04/11/05
    Platform:       PC
    Purpose:        Gets the state of the MOVA enabled flag in the MOVA kernel.
    Inputs:         
    Returns:        Whether MOVA control is enabled.
    Remarks:
*/
MVBool PCIOScan_IsMOVAControlEnabled( void )
{
    return ( Tcomshr->io[ g_MOVA_ENABLED_IO_FLAG ] == 0 ? 
        MV_FALSE : MV_TRUE );

} /* PCIOScan_IsMOVAControlEnabled */


/*
    Name:           PCIOScan_SetInputs
    Author (Date):  ihenderson, 04/11/05
    Platform:       PC
    Purpose:        Copies the given detector and confirm ports
                    to the PCMOVA input ports, preserving their polarity.
                    Also updates the controller ready flag, which is used
                    to indicate to MOVA when the controller is ready for
                    MOVA to take control of the signals.
    Inputs:         Detector and confirm port inputs
                    (detectors should be normal polarity, 
                    confirms should be inverse polarity).
                    Controller ready input
    Returns:        
    Remarks:        Called at the start of each update from the Controller 
                    (which sends PCMOVA the detectors, confirms and CRB)
*/
void PCIOScan_SetInputs( const IOPort * detectors, const IOPort * confirms, 
                        MVBool isControllerReady )
{
    TRACE_ASSERT( g_isInitialised, "PC IO scanner not initialised." );
    TRACE_ASSERT_VALID_ADDRESS( detectors, "No detectors to set." );
    TRACE_ASSERT_VALID_ADDRESS( confirms, "No confirms to set." );

    memcpy( g_detectorPorts, detectors, sizeof( g_detectorPorts ) );
    memcpy( g_confirmPorts, confirms, sizeof( g_confirmPorts ) );
   
    /* Set the controller ready flag - CRB always NORMAL polarity */
    g_isControllerReady = isControllerReady;

} /* PCIOScan_SetInputs */


/*
    Name:           PCIOScan_GetOutputs
    Author (Date):  ihenderson, 04/11/05
    Platform:       PC
    Purpose:        Retrieves the force outputs from the PCMOVA output ports.                    
                    Also retrieves the TO/HI flag (Takeover/Hurry Inhibit),
                    which is used to indicate when MOVA is forcing a stage.
    Inputs:         Force outputs to be set (always set to normal polarity)
                    TO/HI flag to be set
					Hold signal flag
					Release signal flag
    Returns:        
    Remarks:        Called at the end of each update from the Controller 
                    (PCMOVA sends the forces and TO/HI flag back to the 
                    controller).  Force outputs can be updated using
                    OutputScan by different threads, so access is synchronised.
*/
void PCIOScan_GetOutputs( IOPort * forces, MVBool * p_isTakeOver, IOPort * p_isHoldSignalOn,  IOPort * p_isReleaseSignalOn)
{
    MVInt portIndex;

#ifndef M8_IMPROVED_LINKING
	UNUSED(p_isHoldSignalOn);
	UNUSED(p_isReleaseSignalOn);
#endif
    
    TRACE_ASSERT( g_isInitialised, "PC IO scanner not initialised." );
    TRACE_ASSERT_VALID_ADDRESS( forces, "No forces to get." );

    /* START CRITICAL SECTION */
    EnterCriticalSection( &g_criticalSection );

    memcpy( forces, g_forcePorts, sizeof( g_forcePorts ) );

#ifdef M8_IMPROVED_LINKING
	/* Set the backward linking signals */
	(*p_isHoldSignalOn) = g_isHoldSignalOn; 
	(*p_isReleaseSignalOn) = g_isReleaseSignalOn; 
#endif
	
    /* Invert forces if necessary (default is NORMAL polarity) */
    if ( Tcomshr->stgdem == 0 )
    {
        for ( portIndex = 0; portIndex != xg_FORCE_PORTS_MAX; ++portIndex )
        {
            forces[ portIndex ] = ~( forces[ portIndex ] );
        }

#ifdef M8_IMPROVED_LINKING
		(*p_isHoldSignalOn) = ~(g_isHoldSignalOn); 
		(*p_isReleaseSignalOn) = ~(g_isReleaseSignalOn); 
#endif
    }

    /* Set the take over flag - TO always NORMAL polarity */
    (*p_isTakeOver) = g_isMOVATakeOver;

	/* END CRITICAL SECTION */
    LeaveCriticalSection( &g_criticalSection );

} /* PCIOScan_GetOutputs */


/*
    Name:           InputScan
    Author (Date):  ihenderson, 04/11/05
    Platform:       PC
    Purpose:        Copies the data in the PCMOVA confirm and detector input 
                    ports (received from the controller using PCIOScan_SetInputs)
                    to the confirm and detector arrays used internally
                    by the MOVA Kernel.  Uses the polarity for detectors
                    and confirms that is specified in the MOVA dataset.
    Inputs:         
    Returns:        
    Remarks:        Called by Detscn.c in the MOVA Kernel every 0.1 seconds
*/
void InputScan( void )
{
    /* These macros hide the less pretty parts of the calls to PCIOScan_ReadBits() */
    /* The CRB bit goes in confin[mxconf-1] hence there is one less stage confirm bit */
    #define ReadDets( count, port ) \
        PCIOScan_ReadBits( detsin, &dets_num, mxdets  , count, port )
    #define ReadConf( count, port ) \
        PCIOScan_ReadBits( confin, &conf_num, mxconf-1, count, port )

    MVInt portIndex;   

    /* Read detector and confirm bits forwards */
    MVInt dets_num = 0;
    MVInt conf_num = 0;

    TRACE_ASSERT( g_isInitialised, "PC IO scanner not initialised." );

    /* Change the polarity of detector inputs to the polarity
    expected by MOVA (specified by DETTON in the dataset) */
    PCIOScan_UpdateDetectorPolarity();

    /* Scan the detector inputs */
    for ( portIndex = 0; portIndex != xg_DETECTOR_PORTS_MAX; ++portIndex )
    {
        ReadDets( xg_PORT_SIZE, g_detectorPorts[ portIndex ] );
    }

    /* Scan the confirm inputs */
    for ( portIndex = 0; portIndex != xg_CONFIRM_PORTS_MAX; ++portIndex )
    {
        ReadConf( xg_PORT_SIZE, g_confirmPorts[ portIndex ] );
    }

    /* Scan the controller ready bit (CRB) */
    confin[ g_CONTROLLER_READY_BIT_INDEX ] = ( g_isControllerReady ? 1 : 0 );

    /* Change the polarity of stage and phase confirms to the polarity
    expected by MOVA (specified by STAGON and PHASON in the dataset) */
    PCIOScan_UpdateConfirmPolarity();

    /* latch changes in detector states for the 'Look' screen */
    /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
    /* For each possible detector input */
    for ( dets_num = 0; dets_num < mxdets; dets_num++ )
    {
        /* clear the '0' latch if the detector sample is a '0' this time */
        detsin_0s[dets_num] &= detsin[dets_num];
        /* set the '1' latch if the detector sample is a '1' this time */
        detsin_1s[dets_num] |= detsin[dets_num];
    }

} /* InputScan */


/*
    Name:           OutputScan
    Author (Date):  ihenderson, 04/11/05
    Platform:       PC
    Purpose:        Copies the force data from the MOVA Kernel to the PCMOVA
                    output ports, preserving polarity (then sent to the 
                    controller using PCIOScan_GetOutputs, which changes the
                    polarity to normal if necessary).
    Inputs:         Mode parameter - unused in PCMOVA, here for compatibility.
                    See OutputScan() implementation in Ioscan.c for use in
                    embedded MOVA.
    Returns:        
    Remarks:        Called by Genstg, Term, Errhand and Datahand in the 
                    MOVA Kernel - access is synchronised since force outputs 
                    can be updated by different threads.
*/
void OutputScan( int nMode )
{
    MVInt portIndex;
    
    /* Control array is defined in Genstg.c */
    extern char control[ NumberOfForce + NumberOfExtra ];

    /* Write force bits backwards from last stage to the first 
    (not including HI(Hurry Inhibit)/TO(Take Over) - this is sent separately) */
    MVInt forces_num = mxstag;

    TRACE_ASSERT( g_isInitialised, "PC IO scanner not initialised." );

    /* START CRITICAL SECTION */
    EnterCriticalSection( &g_criticalSection );

	if(nMode != MOVA_IN_INTERGREEN)
	{
		for ( portIndex = xg_FORCE_PORTS_MAX - 1; portIndex >= 0; --portIndex )
		{
			PCIOScan_WriteBits( control, &forces_num, 0, 
				xg_PORT_SIZE, &( g_forcePorts[ portIndex ] ) );
		}

		/* Get the MOVA take over flag */
		g_isMOVATakeOver = 
			( control[ g_MOVA_TAKE_OVER_BIT_INDEX ] == 0 ? MV_FALSE : MV_TRUE );
	}

#ifdef M8_IMPROVED_LINKING
	/*Setting the backward linking bits*/
	g_isHoldSignalOn = 
	    ( control[HOLD_SIGNAL_BIT] == Tcomshr->stgdem ? MV_TRUE : MV_FALSE );
	
	g_isReleaseSignalOn = 
	    ( control[RELEASE_SIGNAL_BIT] == Tcomshr->stgdem ? MV_TRUE : MV_FALSE );
#endif

	/* END CRITICAL SECTION */
    LeaveCriticalSection( &g_criticalSection );

} /* OutputScan */


/* This sub-routine of InputScan() is used to read a number of bits from an
|  input port into the specified array (either detectors or confirms). */
/* Lowest bit in first port = detector 1; highest bit in last port = detector 64 */
/* Lowest bit in first port = confirm 1; highest bit in last port = confirm 32 */
static void PCIOScan_ReadBits( char * array, MVInt * p_counter, MVInt limit,
                              MVInt count, IOPort port )
{
    MVInt bit = 0;

    TRACE_ASSERT( g_isInitialised, "PC IO scanner not initialised." );
    TRACE_ASSERT_VALID_ADDRESS( array, "Invalid array to read bits into." );
    TRACE_ASSERT_VALID_ADDRESS( p_counter, "Invalid array counter." );

    /* loop while MOVA can accept more of these inputs */
    /* and more inputs need to be read from this port */
    while ( ( *p_counter < limit ) && ( bit != count ) )
    {
        /* extract the state of the current detector or confirm input bit */
        array[*p_counter] = (char)(port & 0x01);
        
        /* move onto the next input */
        (*p_counter)++; bit++; port >>= 1;
    }

} /* PCIOScan_ReadBits */


/* This sub-routine of OutputScan() is used to write a number of bits to an
|  output port from the specified array (of forces). */
/* Lowest bit in first port = force 1; highest bit in last port = force 16 */
static void PCIOScan_WriteBits( const char * array, MVInt * p_counter, 
                               MVInt limit, MVInt count, IOPort * p_port )
{
    MVInt   bit;

    TRACE_ASSERT( g_isInitialised, "PC IO scanner not initialised." );
    TRACE_ASSERT_VALID_ADDRESS( array, "Invalid array to read bits from." );
    TRACE_ASSERT_VALID_ADDRESS( p_counter, "Invalid array counter." );    

    /* Determine how many bits to write from the array */
    bit = (*p_counter) % count;
    if ( bit == 0 )
    {
        bit = count;
    }

    /* Wipe the port before writing to it (ensures unused force bits are zero) */
    (*p_port) = 0;

    /* loop while MOVA has more of these outputs */
    /* and more inputs need to be written to this port */
    while ( ( *p_counter > limit ) && ( bit != 0 ) )
    {
        /* move onto the next output */
        (*p_counter)--; bit--; (*p_port) <<= 1;

        /* insert the state of the current force output */
        (*p_port) |= array[*p_counter] & 0x01;
    }

} /* PCIOScan_WriteBits */


/*
    Name:           PCIOScan_InitialiseInputs
    Author (Date):  ihenderson, 04/11/05
    Platform:       PC
    Purpose:        Initialises the PCMOVA confirm and detector input ports
    Inputs:         
    Returns:        
    Remarks:        
*/
static void PCIOScan_InitialiseInputs( void )
{
    TRACE_ASSERT( !g_isInitialised, "PC IO scanner already initialised." );

    g_isControllerReady = MV_FALSE;

    /* Unused stage confirms are by custom set to zero 
    regardless of Tcomshr->stagon/phason polarity */
    memset( g_confirmPorts, 0, sizeof( g_confirmPorts ) );
    
    /* Tcomshr->detton polarity not available at initialisation 
    (dataset not loaded), so assume normal polarity */
    memset( g_detectorPorts, 0, sizeof( g_detectorPorts ) );

} /* PCIOScan_InitialiseInputs */


/*
    Name:           PCIOScan_InitialiseOutputs
    Author (Date):  ihenderson, 04/11/05
    Platform:       PC
    Purpose:        Initialises the PCMOVA force output ports
    Inputs:         
    Returns:        
    Remarks:        
*/
static void PCIOScan_InitialiseOutputs( void )
{
    TRACE_ASSERT( !g_isInitialised, "PC IO scanner already initialised." );

    g_isMOVATakeOver = MV_FALSE;

    /* Tcomshr->stgdem polarity not available at initialisation 
    (dataset not loaded), so assume normal polarity */
    memset( g_forcePorts, 0, sizeof( g_forcePorts ) );

#ifdef M8_IMPROVED_LINKING
	g_isHoldSignalOn = MV_FALSE;
	g_isReleaseSignalOn = MV_FALSE;
#endif

} /* PCIOScan_InitialiseOutputs */


/*
    Name:           PCIOScan_UpdateConfirmPolarity
    Author (Date):  ihenderson, 04/11/05
    Platform:       PC
    Purpose:        Change confirm polarity according to polarity specified
                    in the current MOVA dataset (only for confirms that are 
                    actually in use - unused confirms should be left as zero).
                    Also set polarity of phase confirms correctly according 
                    to PHASON in MOVA dataset.
    Inputs:         
    Returns:        
    Remarks:        BUGFIX: 10047.
*/
static void PCIOScan_UpdateConfirmPolarity( void )
{
    TRACE_ASSERT( g_isInitialised, "PC IO scanner not initialised." );

    /* Change stage polarity from inverse to normal if this was 
    specified in the dataset data (default is INVERSE) */
    if ( Tcomshr->stagon == 1 )
    {
        MVInt stage;
        for ( stage = 0; stage != Tcomshr->stages; ++stage )
        {
            confin[ stage ] ^= 1;
        }

    } /* If stage confirms are normal polarity */

    /* Change phase polarity from inverse to normal if this was 
    specified in the dataset data (default is INVERSE) */
    if ( Tcomshr->phason == 1 )
    {
        MVInt link, phase;
        for ( link = 0; link != Tcomshr->nlinks; ++link )
        {
            /* Get the phase number for this link (if there is one) */
            phase = Tcomshr->lphase[ link ];
            if ( phase > 0 )
            {
                confin[ phase - 1 ] ^= 1;
            }
        } 

    } /* If phase confirms are normal polarity */

} /* PCIOScan_UpdateConfirmPolarity */


/*
    Name:           PCIOScan_UpdateDetectorPolarity
    Author (Date):  ihenderson, 04/11/05
    Platform:       PC
    Purpose:        Change detector polarity according to polarity specified
                    in the current MOVA dataset
    Inputs:         
    Returns:        
    Remarks:        
*/
static void PCIOScan_UpdateDetectorPolarity( void )
{
    TRACE_ASSERT( g_isInitialised, "PC IO scanner not initialised." );

    /* Change detector polarity from normal to inverse if this was 
    specified in the dataset data (default is NORMAL) */
    if ( Tcomshr->detton == 0 )
    {       
        MVInt portIndex;
        for ( portIndex = 0; portIndex != xg_DETECTOR_PORTS_MAX; ++portIndex )
        {
            g_detectorPorts[ portIndex ] = ~( g_detectorPorts[ portIndex ] );
        }
    }

} /* PCIOScan_UpdateDetectorPolarity */
