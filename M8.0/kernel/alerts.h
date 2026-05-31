/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         alerts.h
*
*  TITLE:        MOVA Alerts Interface
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	12-Dec-2011		7.0.1		AA		AK		CRN007			Implement alerts API
*	17-Dec-2012		7.0.5		..		PA		CRN019			Improve alerts interface
*	31-Jan-2013		7.0.5		AB		AK		M7.0.5			Issue M7.0.5
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


/* 
	The functions defined in this header file are called by the MOVA kernel when
	the conditions for alerting the controller/outstation to particular conditions
	have been met, or an existing alert condition is now cleared.

	Implementation of these functions (above the dummy defaults provided) should 
	be completed by the manufacturers.

	N.B. 
		The functions are called from the main MOVA process.  Manufacturers
		must ensure that any implementation completes in a timely fashion, or that 
		intensive tasks are handed off to another thread/process.

		There is no status return to the MOVA kernel.  Implementations must not
		generate error/exception conditions that will compromise stability of
		the MOVA kernel process.

	All functions pass a read-only pointer to the current common data in Tcomshr 
	which should be used to query any additional MOVA information required 
	for implementation.  Do not access Tcomshr directly.

	A read-only pointer to the TMA structure is also provided to enable querying
	of the TMA data which is where the flags that generate these alerts are stored.
*/

#ifndef _ALERTS_H_
#define _ALERTS_H_

#include "gbltypes.h"

/*
	Oversat alert is set when any lane has been oversaturated for its specified 
	number of cycles (specified in MOVA Setup: OSATAL).  This function is only 
	called for the first lane to exceed the threshold, i.e. when the OSAT alert
	flag is set from 0 to 1.  Additional lanes may subsequently also exceed their
	oversat cycles threshold without further calls to this function.  
	The alert is cleared when ALL lanes that had exceeded their OSATAL threshold
	have cleared, not just the lane that caused the alert to be set.

	The overstauration of all lanes can be queried in Lcomshr->laosat[]
	Note array index is laneNo-1
*/
void set_alert_oversat(const Ccomshr *const Lcomshr, const strcTMA *const tma);
void clear_alert_oversat(const Ccomshr *const Lcomshr, const strcTMA *const tma);


/*
	Exit blocking alert is set when any lane has been exit blocked for its specified 
	number of cycles (specified in MOVA Setup: EXITAL).  This function is only 
	called for the first lane to exceed the threshold, i.e. when the EB alert
	flag is set from 0 to 1.  Additional lanes may subsequently also exceed their
	exit blocked cycles threshold without further calls to this function.  
	The alert is cleared when ALL lanes that had exceeded their EXITAL threshold
	have cleared, not just the lane that caused the alert to be set.

	The exit blocking status of each lane is in tma->sAlert._ebStatus[]
	Note array index is laneNo-1
*/
void set_alert_exitblocked(const Ccomshr *const Lcomshr, const strcTMA *const tma);
void clear_alert_exitblocked(const Ccomshr *const Lcomshr, const strcTMA *const tma);


/*
	Occupancy alert is set when the average occupancy of the X detectors on lanes
	that run in a stage exceed the specified threshold (MOVA Setup: OCCUAL On).
	This function is only called for the first stage exceeding its threshold. 
	Additional stages may subsequently also exceed their occupancy thresholds
	without further calls to this function.
	The alert is cleared when all stages that exceeded their OCCUAL On occupancy
	fall below their specified occupancy for clearing the alert 
	(MOVA Setup: OCCUAL Off)
	
	The occupancy status of each stage is in tma->sAlert._occStatus[]
	Note array index is stageNo-1
*/
void set_alert_occupancy(const Ccomshr *const Lcomshr, const strcTMA *const tma);
void clear_alert_occupancy(const Ccomshr *const Lcomshr, const strcTMA *const tma);

#endif
