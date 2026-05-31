/*
    Name:           PCMOVA.cpp
    Author (Date):  ihenderson, 28/09/05
    Platform:       PC
    Purpose:        PCMOVA Kernel application entry point.
                    Provides a command line interface to the
                    PCMOVA Kernel.

    Last revision:  $Revision:: 5                           $
    Last changed:   $Date:: 12/04/06 16:07                  $
    Changed by:     $Author:: Ihenderson                    $

    $History: PCMOVA.cpp $
    
    *****************  Version 5  *****************
    User: Ihenderson   Date: 12/04/06   Time: 16:07
    Updated in $/PCMOVA
    
    *****************  Version 4  *****************
    User: Ihenderson   Date: 31/01/06   Time: 15:29
    Updated in $/PCMOVA
    
    *****************  Version 3  *****************
    User: Ihenderson   Date: 16/12/05   Time: 10:22
    Updated in $/PCMOVA
    
    *****************  Version 2  *****************
    User: Ihenderson   Date: 5/12/05    Time: 11:06
    Updated in $/PCMOVA
    
    *****************  Version 1  *****************
    User: Ihenderson   Date: 28/11/05   Time: 11:49
    Created in $/PCMOVA
*/

/* 
    TODO:

    - PCKernel_DeInitialise code isn't re-entrant (see OnExit) - 
    need to make it thread-safe e.g. it's possible for one of the 
    I/O threads in the MOVA kernel to call exit() (via Switch_MOVA_Off) 
    while we're already in the process of calling DeInitialise from the 
    main thread.

    - (non urgent) PCMOVA's exit code - maybe PCMOVAController could pick 
    this up? Could be useful for adding more information to any exceptions
    caused by PCMOVA exiting unexpectedly.  
    E.g. "PCMOVA exited with error code: n"

    - (non urgent) Support for loading multiple time-of-day datasets from the 
    command line?
    
    - (non urgent) support for serial connections to controllers as well as TCP? 
    The connection string argument could contain COM port etc. instead of
    IP address and port number.   
    (Useful for manufacturer testing - not this version, but bear in mind)
*/


#include <iostream>
#include <sstream> // For istringstream/ostringstream

// TODO: Don't really want to include all this.  Want to use this file to show how 
// PCMOVA can be operated programmatically, as well as providing a command line interface.
// Only MOVA header included should be pckernel.h.
#include "stdafx.h" 

#include "CmdLine.h"
#include "pckernel.h"

#if !defined(_DEBUG) && !defined(NO_ERIS)
#include "CopyProtection\ERIS.h"
#endif

#define     g_MODULE_NAME           ("PCMOVA(main)")

#define     g_DATASET_SWITCH        ("-dataset")
#define     g_CONTROLLER_ID_SWITCH  ("-controllerid")
#define     g_JUNCTION_ID_SWITCH    ("-id")
#define     g_CONTROLLER_SWITCH     ("-controller")
#define     g_USER_SWITCH           ("-user")
#define     g_HELP_SWITCH           ("-h")

#define     g_PCMOVA_ERROR_TITLE    ("PCMOVA Error")

// Helper functions
bool    StringToCommsType(      string &, CommsType & );
bool    StringToJunctionID(     const string &, int & );
void    DisplayUsage(           void );
void    DisplayLastError(       void );
void    OnExit(                 void );


/*
    Name:           main
    Author (Date):  Ian Henderson (11/10/05)
    Platform:       PC
    Purpose:        Entry point for the PCMOVA Kernel process
    Inputs:         See DisplayUsage()
    Returns:        0 if successful; PCMOVA error code (>0) if failure
    Remarks:
*/
int main( MVInt argc, char * argv[] )
{
    PCError     pcmovaError = PCMOVA_ERROR_NO_ERROR;        
    MVInt       junctionID;
    CommsType   controllerComms;
    CommsType   userComms;
    string      junctionIDStr;
    string      junctionDataset;
    string      controllerCommsStr;
    string      controllerEndPoint;
    string      userCommsStr;
    string      userEndPoint;
	string      controllerId;

    CCmdLine    cmdLine;


#if !defined(_DEBUG) && !defined(NO_ERIS)
	//Checking PCMOVA licence
	ERIS eris_sim("MOVA Tools – Simulation", 0, NULL, false);
	if (eris_sim.GetLicenceStatusSilent() == 0)
	{
		ERIS eris_pcmova("PCMOVA 2", 0, NULL, false);
		if (eris_pcmova.GetLicenceStatusSilent() == 0)
		{
			cout << endl << "No valid license is installed for PCMOVA." << endl;
			cout << "To register your product, please contact software@trl.co.uk." << endl;

			return (PCMOVA_ERROR_TERMINATE);
		}
	}
#endif

    // Parse the command line
    if ( cmdLine.SplitLine( argc, argv ) < 1 )
    {
        // No switches were given on the command line, abort
        DisplayUsage();
        return ( PCMOVA_ERROR_PARAM_INVALID );
    }

    // Test for a help switch
    if ( cmdLine.HasSwitch( g_HELP_SWITCH ) )
    {
        DisplayUsage();
        return ( PCMOVA_ERROR_NO_ERROR );
    }

    try
    {
        // Get required controller comms arguments
        controllerCommsStr = cmdLine.GetArgument( g_CONTROLLER_SWITCH, 0 );
        controllerEndPoint = cmdLine.GetArgument( g_CONTROLLER_SWITCH, 1 );

        // Get user comms type argument (if it exists)
        if ( cmdLine.HasSwitch( g_USER_SWITCH ) )
        {
            userCommsStr = cmdLine.GetArgument( g_USER_SWITCH, 0 );
        }
        else
        {
            userCommsStr = "TCP";
        }
    }
    catch ( ... )
    {
        // One of the required arguments was missing, abort
        DisplayUsage();
        return ( PCMOVA_ERROR_PARAM_INVALID );    
    }

    // Get optional junction id, dataset and user endpoint arguments
    junctionIDStr = cmdLine.GetSafeArgument( g_JUNCTION_ID_SWITCH, 0, "0" );
    junctionDataset = cmdLine.GetSafeArgument( g_DATASET_SWITCH, 0, "" );
    userEndPoint = cmdLine.GetSafeArgument( g_USER_SWITCH, 1, "" );    
    controllerId = cmdLine.GetSafeArgument(g_CONTROLLER_ID_SWITCH, 0, "1");

	cout << "Controller ID: " << controllerId << endl << endl;

    // Convert argument strings to types suitable for passing to the MOVA kernel
    if ( !StringToJunctionID( junctionIDStr, junctionID ) 
        || !StringToCommsType( controllerCommsStr, controllerComms )
        || !StringToCommsType( userCommsStr, userComms ) )
    {
        DisplayUsage();
        return ( PCMOVA_ERROR_PARAM_INVALID );
    }
    
            
#if defined (_TRACE)
    // If tracing, create trace log file for this junction
    ostringstream traceLogFileName;
    traceLogFileName << "tracejunc" << junctionID << ".log";

    TRACE_INITIALISE( xg_APP_CONFIG_FILE_NAME, traceLogFileName.str().c_str() );
    TRACE_WRITE1_IF( LEVEL_INFO, g_MODULE_NAME, 
        "PCMOVA process started for junction with ID %d.", junctionID );
#endif


    // Handler to be called if PCMOVA calls exit()
    atexit( OnExit );

    // Initialise the PCMOVA Kernel
    MVStatus status = PCKernel_Initialise(
        controllerComms,
        controllerEndPoint.c_str(),
        userComms, 
        ( userEndPoint.length() > 0 ? userEndPoint.c_str() : NULL ),
        junctionID,
        ( junctionDataset.length() > 0 ? junctionDataset.c_str() : NULL ),
		controllerId.c_str()
    );
    
    if ( status == MV_SUCCESS )
    {        
        // Run the PCMOVA kernel.  Only returns if an error occurs or PCMOVA
        // has been asked to terminate by the controller.
        status = PCKernel_Run( );        
        if ( status == MV_FAILURE )
        {
            // The kernel quit due to an error
            DisplayLastError();
            pcmovaError = PCError_GetLast();
        }
    }
    else
    {
        DisplayLastError();
        pcmovaError = PCError_GetLast();
    }

    // Shut down the kernel
    status = PCKernel_DeInitialise();
    if ( status == MV_FAILURE )
    {
        DisplayLastError();
        pcmovaError = PCError_GetLast();
    }
    
    TRACE_WRITE1_IF( LEVEL_INFO, g_MODULE_NAME, 
        "PCMOVA process stopped for junction with ID %d.", junctionID );

    TRACE_DEINITIALISE();

    return ( pcmovaError );

} // main


/*
    Name:           StringToJunctionID
    Author (Date):  Ian Henderson (11/10/05)
    Platform:       PC
    Purpose:        Converts a string read from the command line
                    to a junction ID.  
    Inputs:         The string to convert.
                    The junction ID.
    Returns:        True if the string could be converted to 
                    a valid junction ID; false otherwise.
    Remarks:
*/
bool StringToJunctionID( const string &idStr, int &id )
{
    istringstream idStream( idStr );
  
    if ( idStream >> id )
    {
        return MATH_IS_IN_RANGE( id, xg_JUNCTION_ID_MIN, xg_JUNCTION_ID_MAX );
    }
    else
    {
        return false;
    }

} // StringToJunctionID


/*
    Name:           StringToCommsType
    Author (Date):  Ian Henderson (11/10/05)
    Platform:       PC
    Purpose:        Converts a string read from the command line
                    to a communications type.  
    Inputs:         The string to convert.
                    The communications type.
    Returns:        True if the string could be converted to 
                    a valid comms type; false otherwise.
    Remarks:
*/
bool StringToCommsType( string &commsStr, CommsType &comms )
{
    // Convert comms string to uppercase
    for ( string::iterator ch = commsStr.begin(); ch != commsStr.end(); ++ch )
    {
        *ch = (char)toupper( *ch );
    }

    if ( commsStr == "TCP" )
    {
        comms = COMMS_TYPE_TCP;
    }
    else if ( commsStr == "SERIAL" )
    {
        comms = COMMS_TYPE_SERIAL;
    }
    else
    {
        return false;
    }

    return true;

} // StringToCommsType


/*
    Name:           DisplayUsage
    Author (Date):  Ian Henderson (11/10/05)
    Platform:       PC
    Purpose:        Message displayed to the user if command
                    line arguments were invalid, missing,
                    or help was requested (-h). 
    Inputs:         
    Returns:        
    Remarks:
*/
void DisplayUsage( void )
{
    cout << endl << "Usage: PCMOVA.exe -controller <type> <endpoint>" << endl << endl;

    cout << "Where:" << endl << endl;

    cout << "<type> is the method of communication with the controller" << endl;
    cout << "e.g. \"TCP\" or \"Serial\" (if supported)." << endl << endl;

    cout << "<endpoint> is the end point of the controller" << endl;
    cout << "e.g. \"127.0.0.1:1026\" or \"COM1,19200,8,N,1.\"" << endl << endl;

    cout << "Optional:" << endl << endl;

    cout << "-user <type> <endpoint>" << endl << endl;
    
    cout << "<type> is the method of communication with the user terminal" << endl;
    cout << "communications program (MOVA Comm), e.g. \"TCP\" or \"Serial\"." << endl << endl;

    cout << "<endpoint> is the end point for user communications." << endl;
    cout << "e.g. \"127.0.0.1:1026\" or \"COM1,19200,8,N,1.\"." << endl << endl;

    cout << "If no endpoint is supplied, PCMOVA sets its own end point based on the" << endl;
    cout << "type. If the -user option is not used at all, type is set to \"TCP\"." << endl;
    cout << "The user end point is sent to the controller during initialisation." << endl << endl;

    cout << "-id <number>" << endl << endl;

    cout << "<number> is a unique identifier for this instance of PCMOVA," << endl;
    cout << "used for handshaking with the controller [0-255]." << endl << endl;

    cout << "IDs are useful if the controller supports more than one PCMOVA junction." << endl;
    cout << "If no junction ID is provided, a default ID of 0 is used." << endl << endl;

    cout << "-dataset <file>" << endl << endl;

    cout << "<file> is a MOVA dataset file name including full path." << endl;
    cout << "e.g. \"D:\\MOVA Datasets\\myjunc.mova\"" << endl << endl;

	cout << "- controllerid <id>" << endl << endl;
	cout << "<id> The controller ID that will be read from the dataset file." << endl;

    /*cout << "If no dataset is supplied, the default \"Hugemova\" dataset is used." << endl;*/

} // DisplayUsage


/*
    Name:           DisplayLastError
    Author (Date):  Ian Henderson (11/10/05)
    Platform:       PC
    Purpose:        Displays a messagebox to the user describing
                    the error that occurred.
                    Should only be used for fatal errors.
    Inputs:         
    Returns:        
    Remarks:
*/
void DisplayLastError( void )
{    
    MVInt result;
    char pcmovaErrorMsg[ xg_ERROR_MSG_LEN_MAX + 1 ];
    ostringstream errorMsg;

    errorMsg << "A fatal error occurred in PCMOVA.exe" << endl;
    errorMsg << "Error message:" << endl << endl;

    PCError_BuildLastMessage( pcmovaErrorMsg, sizeof( pcmovaErrorMsg ) );

    errorMsg << pcmovaErrorMsg;

    result = MessageBox( 
        NULL, 
        errorMsg.str().c_str(), 
        g_PCMOVA_ERROR_TITLE, 
        MB_OK | MB_ICONERROR );

} // DisplayLastError


/*
    Name:           OnExit
    Author (Date):  Ian Henderson (11/10/05)
    Platform:       PC
    Purpose:        OnExit is called when PCMOVA exits following
                    a fatal error. Ensures PCMOVA exits cleanly by 
                    deinitialising the kernel.                    
    Inputs:         
    Returns:        
    Remarks:        None of the functions called by PCKernel_DeInitialise 
                    should cause exit() to be called 
                    - i.e. PCKernel_DeInitialise will not assert.
                   (Asserts call exit() rather than abort() in release builds.)
*/
void OnExit( void )
{
    MVStatus status = PCKernel_DeInitialise( );
    DEBUG_ASSERT( status != MV_FAILURE );

    TRACE_WRITE0_IF( LEVEL_INFO, g_MODULE_NAME, "PCMOVA process exited." );
    TRACE_DEINITIALISE();

} // OnExit
