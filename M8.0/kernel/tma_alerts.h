/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         tma_alerts.h
*
*  TITLE:        MOVA TMA Alert logs
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	31-May-2011		7.0.0		AA		PK		CRN003			New development and undergoing first full system testing
*	12-Dec-2011		7.0.1		AB		PK		CRN004			SatFlow and CANDET improvements
*	06-Jul-2012		7.0.3		AC		AK		CRN012			Memory leak in TMA module and statistics lost on dataset change
*	21-Jan-2013		7.0.5		..		AK		CRN022			Buffer overflow in TMA output
*	31-Jan-2013		7.0.5		AD		AK		M7.0.5			Issue M7.0.5
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

#if !defined (TMA_ALERTS_H)
#define TMA_ALERTS_H

#include "tma_strc.h"
#include "mova_types.h"

void checkAlertOcc(strcTMA *const pTma);
void displayAlerts(int outDevice, const strcAlert *const stAlert);
void changeFlagAlert(int outDevice, int flagType, strcAlert *const stAlert);
void GetAlertStatus(BOOL *BlockingAlert, BOOL *OverSaturationAlert, BOOL *OccupancyAlert);
void GetAlertMonitoring(BOOL *BlockingStatus, BOOL *OverSaturationStatus, BOOL *OccupancyStatus);
void SetAlertMonitoring(BOOL BlockingStatus, BOOL OccupancyStatus, BOOL OverSaturationStatus);

/*
Checks if a detector (det) have been previously reported faulty.
Values to returned:
the detector is not reported faulty (-1), 
the detector is reported faulty but has not returned to work (position in the array for the detector) and 
the detector is reported faulty and has returned to work (0).

*/
int isReportedFaulty(int detID);

/* The following functions are called form the CORE module*/
void TMA_check_alert_exiting(strcTMA *const pTma, int stgRun, const int8 laensa[]);
void TMA_check_alert_overSat(strcTMA *const pTma);

/* It stores information for faulty detector. It might appear that the faulty detector information has been duplicated
with the history of faulty detectors. 
However, for simplicity, an array of detectors going faulty at any point of the period must be separated from history of faulty detectors, that runs
as long as mova is incharged or a single plan is running. */
void TMA_det_faulty_changed(int detID, int newState);


void TMA_reset_oversat_alert_counter(int laneIdx);
void TMA_inc_oversat_counter(int laneIdx);
void TMA_update_endsat_data(int linksCount, const int8 liensa[]);
void TMA_update_endsat_data_2(int likneCount, const int8 liensa[], int snow);
void TMA_set_stage_start_time(int demsta);

#endif /* TMA_ALERTS_H */
