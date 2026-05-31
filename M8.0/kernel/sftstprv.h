/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         sftstprv.h
*
*  TITLE:        MOVA Satutation flow - Private data types for satflow functionality.
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	10-Feb-2003		6.0.0		..		IH		......			Initial implementation for testing with MOVA
*	24-Mar-2011		7.0.0		AA		PK		CRN003			New Header file inline with MOVA Kernel SQP
*	12-Dec-2011		7.0.1		AB		PK		CRN004			SatFlow and CANDET improvements
*
*  (c) Copyright TRL Ltd 2011. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal or
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/

/*
	Description:
	Defines, structs and functions used by MovaSatFlow module for testing.
	Private - should only be included by SF.c.
*/


#if !defined (MOVASATFLOWTEST_PRIVATE_H)
#define MOVASATFLOWTEST_PRIVATE_H

#if defined (TEST_OUTPUT)

	#include <stdio.h>
	#include "sftypes.h"
	/* Test output filename defines (filenames limited to MS-DOS 8.3 format) */
	#define gszTEST_FILE_PATH						("SFTstOut\\")
	#define gszTEST_FILE_LANE						("L")
	#define gszTEST_FILE_CYCLE						("C")
	#define gszTEST_FILE_HISTORY					("_Hst")
	#define gszTEST_FILE_DET_DATA					("_D")
	#define gszTEST_FILE_HEAVIES					("Heavies")
	#define gszTEST_FILE_EXTENSION					(".log")
	#define gn16TEST_FILE_NAME_LEN					(32)

	/* Names of test output variables */
	#define gszCURRENT_CYCLE_NUM			("Cycle no.")
	#define gszLAST_ESTIMATE_NUM			("Last est. no.")
	#define gszSAT_FLOW_DET_TYPE			("Sat flow det")
	#define gszRED_IN_COUNT					("REDCIN")
	#define gszIN_QUEUE						("IN queue")
	#define gszRED_X_COUNT					("REDCX")
	#define gszVAR_MIN_GREEN				("MINGRE")
	#define gszLEAVING_AMBER				("Leave amber")
	#define gszSTART_RED					("Start red")
	#define gszIN_WARM_UP					("Warm-up")
	#define gszSAT_FLOW_DET_FAULTY			("Faulty sat det")
	#define gszEXIT_BLOCKED					("Exit-blocked")
	#define gszFIRST_SAT_VEHICLE			("First sat veh")
	#define gszLAST_SAT_VEHICLE				("Last sat veh")
	#define gszFULLY_SAT					("Fully sat")
	#define gszX_QUEUE						("X queue")
	#define gszSAT_AFTER_VAR_MIN_GREEN		("Sat after MINGRE")
	#define gszLONG_QUEUE_USED				("LQ method used")
	#define gszSHORT_QUEUE_USED				("SQ method used")
	#define gszLONG_QUEUE_SAT_COUNT			("LQ sat count")
	#define gszSHORT_QUEUE_SAT_COUNT		("SQ sat count")
	#define gszLONG_QUEUE_SAT_FLOW			("LQ sat flow")
	#define gszSHORT_QUEUE_SAT_FLOW			("SQ sat flow")
	#define gszRAW_SAT_FLOW					("Raw sat flow")
	#define gszRAW_STLOST					("Raw STLOST")
	#define gszSMOOTHED_SAT_FLOW			("Smooth sat flow")
	#define gszSMOOTHED_STLOST				("Smooth STLOST")
	#define gszSMOOTHED_OCCUPANCY			("Smooth occ")
	#define gszRAW_FLOW_RATE				("Raw flow rate")
	#define gszSMOOTHED_FLOW_RATE			("Smooth flow rate")
	#define gszSAT_FLOW_TIME				("Sat flow time")
	#define gszDET_TRANSITION_NUM			("Det transition no.")
	#define gszDET_TRANSITION_STATE			("Det state changed to")
	#define gszDET_TRANSITION_TIME			("Det transition time")
	

	/* Macro for saving variables to the output struct (above) 
	   with a quick and dirty debug print of the variable and its value */
	   

	#define MovaSatFlow_Output_Record_( var, save_var, lane )										\
	{																								\
		(save_var) = (var);																		\
/*		if( Tcomshr->io[28] == 23 )																		\
		{																							\
			(save_var) = (var);																		\
			sprintf( OutputString, ("SF|Lane| %d|" #save_var "|%.2f\n"), (lane) + 1, (float)(var));	\
		 	diag_printf( OutputString, stdout );													\
		}																							\
*/	}


	/* Struct for storing output data */
	typedef struct
	{
		uint16		u16CurrentCycleNum;				/* 1-based cycle number */
		uint16		u16LastEstimateNum;				/* 1-based estimate number */
		int16		n16VarMinGreen;					/* Time in tenths of seconds */
		int16		n16LeavingAmber;				/* Time in tenths of seconds */
		int16		n16StartRed;					/* Time in tenths of seconds */
		int16		n16RedCountIN;					/* Number of vehicles */
		int16		n16RedCountX;					/* Number of vehicles */
		int16		n16LongQueueSatCount;			/* Number of vehicles */
		int16		n16ShortQueueSatCount;			/* Number of vehicles */
		int16		n16FirstSatVehicle;				/* Detector state change number */
		int16		n16LastSatVehicle;				/* Detector state change number */
		DET_TYPE	n16SatFlowDetType;				/* Detector type number */
		REAL		rRawSTLOST;					/* Time in tenths of seconds */
		REAL		rRawSatFlow;					/* Rate of vehicles/hour */
		REAL		rLongQueueSatFlow;				/* Rate of vehicles/hour */
		REAL		rShortQueueSatFlow;				/* Rate of vehicles/hour */
		REAL		rSmoothedSatFlow;				/* Rate of vehicles/hour */
		REAL		rSmoothedSTLOST;				/* Time in tenths of seconds */		
		REAL		rSmoothedOcc;					/* Time in tenths of seconds/vehicle */
		REAL		rRawFlowRate;					/* Rate of vehicles/hour */
		REAL		rSmoothedFlowRate;				/* Rate of vehicles/hour */
		int16		n16SatFlowTime;					/* Time in tenths of seconds */	
		LOGIC		lFullySat;						/* Flag - TRUE/FALSE */
		LOGIC		lIsSatFlowDetFaulty;			/* Flag - TRUE/FALSE */
		LOGIC		lQueueOverXDet;					/* Flag - TRUE/FALSE */
		LOGIC		lINDetStandingQueue;			/* Flag - TRUE/FALSE */
		LOGIC		lIsSatFlowAfterVarMinGreen;		/* Flag - TRUE/FALSE */
		LOGIC		lIsExitBlocked;					/* Flag - TRUE/FALSE */
		LOGIC		lShortQueueMethodUsed;			/* Flag - TRUE/FALSE */
		LOGIC		lLongQueueMethodUsed;			/* Flag - TRUE/FALSE */
		LOGIC		lInWarmUpPeriod;				/* Flag - TRUE/FALSE */	

	} LaneOutput;

	static void MovaSatFlowTest_TestOutputInitialise_(	void );
	static void	MovaSatFlowTest_TestOutputReset_(		const int16 n16LaneNo );

#else

	/* Define to nothing */
	#define MovaSatFlow_Output_Record_( var, save_var, lane )

	#define MovaSatFlowTest_TestOutputInitialise_(			 )
	#define	MovaSatFlowTest_TestOutputReset_(			lane )

#endif /* defined (TEST_OUTPUT) */

#endif /* MOVASATFLOWTEST_PRIVATE_H */
