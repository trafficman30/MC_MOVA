/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         sfstats.h
*
*  TITLE:        MOVA Saturation Flow: Perform Stats and feedback satflow at real-time
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	31-May-2011		7.0.0		AA		PK		CRN003			Public interface to MOVA Satflow feedback functionality.
*	15-Apr-2013		7.0.6		..		AK		CRN027			Improvements to SF feedback
*	03-May-2013		7.0.6		..		PA		CRN031			Don't reset SF data on warm restart
*	22-May-2013		7.0.6		AB		AK		M7.0.6			Issue M7.0.6
*
*  (c) Copyright TRL Ltd 2013. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal or
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/

#if !defined (MOVASATFLOWSTATS_H)
#define MOVASATFLOWSTATS_H


/*************************
*	Includes
**************************/

#include "sftypes.h"
#include "sfstatsprv.h"
#include "sfprv.h"


/*************************
*	Defines
**************************/

#if !defined (__SF_PRIVATE)
	typedef struct _Lane		Lane;
#endif

	/* String used for invalid test output values */
#define gszUNKNOWN_VALUE						("?")
#define gszOUTPUT_VALUE_THREE_NINE				("999")
#define gszOUTPUT_VALUE_THREE_ZERO				("000")
#define gszOUTPUT_VALUE_FOUR_ZERO				("0000")
#define gszOTUPTU_VALUE_ZERO_POINT_ZERO			("0.0")

/* Number used to indentify individual period used to store raw value and perform calculation*/
/* Period 1 to 4 are weekdays, Period 5 reserved for Sat, Period 6 reserved for Sun */
#define gn16PERIOD_INVALID									((int16)0)
#define gn16PERIOD_1										((int16)1)
#define gn16PERIOD_2										((int16)2)
#define gn16PERIOD_3										((int16)3)
#define gn16PERIOD_4										((int16)4)
#define gn16PERIOD_5										((int16)5)
#define gn16PERIOD_6										((int16)6)


/*************************
*	Public functions
**************************/


void	MovaSatFlowStats_Initialise_(			SatData* gSatData );

void	MovaSatFlowStats_ReInitialise_(			void);

void	MovaSatFlowStats_Output_ScreenDisplay_(	const int16  n16DataType);

void	MovaSatFlowStats_Output_SFVerbose_(		void);

void	MovaSatFlowStats_Output_STVerbose_(		void);

void	MovaSatFlowStats_UpdateSatFlowStats_(		void );

void	MovaSatFlowStats_NewDatasetLoaded_(		void);

void	MovaSatFlowStats_RecordData_(			Lane * const pLane, REAL rRawData,const int16 n16ID );

int*	MovaSatFlowStats_GetSatFlowFlagPtr_(	void);

void	MovaSatFlowStats_PerformColdStart(	void );

float MovaSatFlowStats_GetSatESV_Data( int Lane, int PeriodNo );

float MovaSatFlowStats_GetSTLostESV_Data( int Lane, int PeriodNo );

void MovaSatFlowStats_GetSatPeriodTimes( int PeriodNo, int* PeriodStart, int* PeriodEnd );

PeriodData MovaSatFlowStats_GetSatVerbose_Data( int Lane, int PeriodNo );

PeriodData MovaSatFlowStats_GetSTLostVerbose_Data( int Lane, int PeriodNo );

REAL MovaSatFlowTest_GetSmoothedFlowLastCycle(const int16 laneIdx);

#endif /* MOVASATFLOWSTATS_H */

