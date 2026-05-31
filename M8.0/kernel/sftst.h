/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         sftst.h
*
*  TITLE:        MOVA Satutation flow calculator - Public interface to MovaSatFlow testing functionality.
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	10-Feb-2003		6.0.0		..		IH		......			Initial implementation for testing with MOVA
*	24-Mar-2011		7.0.0		AA		PK		CRN003			New Header file inline with MOVA Kernel SQP
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

#if !defined (MOVASATFLOWTEST_H)
#define MOVASATFLOWTEST_H


/* Comment this in/out according to whether the saturation 
   flow module is running with the test-wrapper */
#define TEST_WRAPPER

#if defined (TEST_WRAPPER)

	/* Test output must always be defined for the module to work 
	   with the test-wrapper */
	#define TEST_OUTPUT

#elif defined (PC)
	
	/* Comment this in/out to enable/disable test output when 
	   running with MOVA (simulated with Movasim on PC). */
	#define TEST_OUTPUT

#else /* Embedded MOVA */

    /* No test output until its rewritten for embedded MOVA */
    /*#define TEST_OUTPUT*/
    
#endif


/* Functions to get test output */
#if defined (TEST_OUTPUT)

	#include "sftypes.h"

	/* Buffers passed to 'Get' functions should be at least this size */
	#define gn16TEST_VAR_VALUE_LEN	((int16)16)
															
	char *	MovaSatFlowTest_GetCurrentCycleNum_( 
			char * const szBuffer, const int16 n16LaneNo );	/* Cycle number */
	char *	MovaSatFlowTest_GetLastEstimateNum_( 
			char * const szBuffer, const int16 n16LaneNo );	/* Estimate number */
	char *	MovaSatFlowTest_GetVarMinGreen_(				
			char * const szBuffer, const int16 n16LaneNo );	/* Time in tenths of seconds */
	char *	MovaSatFlowTest_GetLeavingAmber_( 
			char * const szBuffer, const int16 n16LaneNo ); /* Time in tenths of seconds */
	char *	MovaSatFlowTest_GetStartRed_( 
			char * const szBuffer, const int16 n16LaneNo ); /* Time in tenths of seconds */
	char *	MovaSatFlowTest_GetRedCountIN_(					
			char * const szBuffer, const int16 n16LaneNo );	/* Number of vehicles */
	char *	MovaSatFlowTest_GetRedCountX_(					
			char * const szBuffer, const int16 n16LaneNo );	/* Number of vehicles */
	char *	MovaSatFlowTest_GetLQSatCount_(			
			char * const szBuffer, const int16 n16LaneNo );	/* Number of vehicles */
	char *	MovaSatFlowTest_GetSQSatCount_(			
			char * const szBuffer, const int16 n16LaneNo );	/* Number of vehicles */
	char *	MovaSatFlowTest_GetFirstSatVehicle_(			
			char * const szBuffer, const int16 n16LaneNo );	/* Detector state change number */
	char *	MovaSatFlowTest_GetLastSatVehicle_(				
			char * const szBuffer, const int16 n16LaneNo );	/* Detector state change number */
	char *	MovaSatFlowTest_GetSatFlowDetType_(				
			char * const szBuffer, const int16 n16LaneNo );	/* Detector type number */
	char *	MovaSatFlowTest_GetRawSTLOST_(					
			char * const szBuffer, const int16 n16LaneNo );	/* Time in tenths of seconds */
	char *	MovaSatFlowTest_GetRawSatFlow_(					
			char * const szBuffer, const int16 n16LaneNo );	/* Rate of vehicles/hour */
	char *	MovaSatFlowTest_GetLQSatFlow_(			
			char * const szBuffer, const int16 n16LaneNo );	/* Rate of vehicles/hour */
	char *	MovaSatFlowTest_GetSQSatFlow_(			
			char * const szBuffer, const int16 n16LaneNo );	/* Rate of vehicles/hour */
	char *	MovaSatFlowTest_GetSmoothedSatFlow_(			
			char * const szBuffer, const int16 n16LaneNo );	/* Rate of vehicles/hour */
	char *	MovaSatFlowTest_GetSmoothedSTLOST_(				
			char * const szBuffer, const int16 n16LaneNo );	/* Time in tenths of seconds */
	char *	MovaSatFlowTest_GetSmoothedOcc_(				
			char * const szBuffer, const int16 n16LaneNo );	/* Time in tenths of seconds/vehicle */
	char *	MovaSatFlowTest_GetRawFlowRate_( 
			char * const szBuffer, const int16 n16LaneNo ); /* Rate of vehicles/hour */
	char *	MovaSatFlowTest_GetSmoothedFlowRate_(				
			char * const szBuffer, const int16 n16LaneNo );	/* Rate of vehicles/hour */	
	char *	MovaSatFlowTest_GetCurrentTime_(				
			char * const szBuffer );	/* Current Time*/
	char *	MovaSatFlowTest_GetCurrentDate_(				
			char * const szBuffer );	/*Current Date */
	char *	MovaSatFlowTest_GetFullySat_(					
			char * const szBuffer, const int16 n16LaneNo );	/* Flag - TRUE/FALSE */
	char *	MovaSatFlowTest_GetQueueOverXDet_(				
			char * const szBuffer, const int16 n16LaneNo );	/* Flag - TRUE/FALSE */
	char *	MovaSatFlowTest_GetINDetStandingQueue_(			
			char * const szBuffer, const int16 n16LaneNo );	/* Flag - TRUE/FALSE */
	char *	MovaSatFlowTest_GetIsSatFlowAfterVarMinGreen_(	
			char * const szBuffer, const int16 n16LaneNo );	/* Flag - TRUE/FALSE */
	char *	MovaSatFlowTest_GetIsExitBlocked_(				
			char * const szBuffer, const int16 n16LaneNo );	/* Flag - TRUE/FALSE */
	char *	MovaSatFlowTest_GetSQMethodUsed_(		
			char * const szBuffer, const int16 n16LaneNo );	/* Flag - TRUE/FALSE */
	char *	MovaSatFlowTest_GetLQMethodUsed_(		
			char * const szBuffer, const int16 n16LaneNo );	/* Flag - TRUE/FALSE */
	char *	MovaSatFlowTest_GetIsSatFlowDetFaulty_(			
			char * const szBuffer, const int16 n16LaneNo );	/* Flag - TRUE/FALSE */
	char *	MovaSatFlowTest_GetInWarmUpPeriod_(			
			char * const szBuffer, const int16 n16LaneNo );	/* Flag - TRUE/FALSE */


#endif /* defined (TEST_OUTPUT) */


#endif /* MOVASATFLOWTEST_H */
