/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         sf.h
*
*  TITLE:        MOVA Satutation flow calculator - Public interface
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	10-Feb-2003		6.0.0		..		IH		......			Initial implementation for testing with MOVA
*	31-May-2011		7.0.0		AA		PK		CRN003			New Header file inline with MOVA Kernel SQP
*	22-Feb-2012		7.0.3		AB		AK		CRN008			Change include of sftypes.h to sfprv.h (which in turn includes sftypes.h)
*
*  (c) Copyright TRL Ltd 2012. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal or
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/

#if !defined (MOVASATFLOW_H)
#define MOVASATFLOW_H


/*************************
*	Includes
**************************/

#include "sfprv.h"


/*************************
*	Defines
**************************/

#if !defined (__SF_PRIVATE)
	typedef struct _Lane		Lane;
	typedef struct _Detector	Detector;
#endif
	


/*************************
*	Public functions
**************************/

/* Main module initialisation and update functions */
void				MovaSatFlow_Initialise_(							void );
void				MovaSatFlow_ReInitialise_(							void );
void				MovaSatFlow_Update_(								void );
		
/* Data access functions */

int16			MovaSatFlow_GetLanesNum_(							void );
int16			MovaSatFlow_GetDetectorsNum_(						void );
Lane *			MovaSatFlow_GetLane_(								const int16 n16LaneNo );
Detector *		MovaSatFlow_GetDetector_(							const int16 n16DetNo );
REAL			MovaSatFlow_GetCurrentSatFlow_(						const Lane * const pLane );
DET_TYPE		MovaSatFlow_GetDetectorType_(						const Detector * const pDetector );
Detector *		MovaSatFlow_GetDetectorInLane_(						const Lane * const pLane,const DET_TYPE n16DetTypeNo );


REAL			MovaSatFlow_GetCriticalGapFactor_(					void );
REAL			MovaSatFlow_GetSatFlowSmoothingFactor_(				void );
REAL			MovaSatFlow_GetDetOccupancySmoothingFactor_(		void );
REAL			MovaSatFlow_GetFlowRateProportionalErrorMax_(		void );
int16			MovaSatFlow_GetStoplineSTLOSTVehiclesNum_(			void );
int16			MovaSatFlow_GetXDetSTLOSTVehiclesNum_(				void );
Detector *		MovaSatFlow_GetSaturationFlowDetector_(				const Lane * const pLane );
int16			MovaSatFlow_GetDetectorExitBlockedThreshold_(		const Detector * const pDetector );

REAL			MovaSatFlow_GetLongSlowVehicleDetOccupancyFactor_(	void );
void			MovaSatFlow_SetLongSlowVehicleDetOccupancyFactor_(	const REAL rOccupancyFactor );
void			MovaSatFlow_SetCriticalGapFactor_(					const REAL rGapFactor );
void			MovaSatFlow_SetSatFlowSmoothingFactor_(				const REAL rSmoothingFactor );
void			MovaSatFlow_SetDetOccupancySmoothingFactor_(		const REAL rSmoothingFactor );
void			MovaSatFlow_SetFlowRateProportionalErrorMax_(		const REAL rFlowRateError );
void			MovaSatFlow_SetStoplineSTLOSTVehiclesNum_(			const int16 n16STLOSTVehiclesNum );
void			MovaSatFlow_SetXDetSTLOSTVehiclesNum_(				const int16 n16STLOSTVehiclesNum );
LOGIC			MovaSatFlow_SetSaturationFlowDetector_(				Lane * const pLane, const Detector * const pDetector );
void			MovaSatFlow_SetDetectorExitBlockedThreshold_(		const int16 n16ThresholdNew,Detector * const pDetector );


#endif /* MOVASATFLOW_H */

