/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         sfstatsprv.h
*
*  TITLE:         MOVA Satutation flow - Private data types for satflow stats functionality.
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	31-May-2011		7.0.0		AA		PK		CRN003			Definations of Private data Structures for sfstats module
*	12-Dec-2011		7.0.1		AB		PK		CRN004			SatFlow and CANDET improvements
*	15-Apr-2013		7.0.6		..		AK		CRN027			Improvements to SF feedback
*	10-May-2013		7.0.6		..		IA		CRN034			Change n1615thPercentile from int16 to REAL to avoid loss of precision
*	22-May-2013		7.0.6		AC		AK		M7.0.6			Issue M7.0.6
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


#include "sftypes.h"

#if !defined (MOVASATFLOW_STATS_PRIVATE_H)
#define MOVASATFLOW_STATS_PRIVATE_H

typedef struct _PeriodData
{
	int16	n16ID;
	/*To identify type of data stored in this structure e.g. for satflow - ID = 0, for stlost - ID = 2 */

	int16	n16Count;
	/*Varaible used to count the number of raw satflow or stlost values recoreded during the period */

	int16	n16RecordCount;
	/*Count to record number of times raw values met stastical criteria*/

	
	REAL	rMeanS;
	/* To record the average of (raw/rejected satflow/Stlost) values calculated during each period for each lane*/

	REAL	rOldMeanS;
	/* Mean old value - used during calculating the standard deviation */

	REAL	rNewMeanS;
	/* Mean new value - used during calculating the standard deviation */

	REAL	rOldS;
	/* Old S parameter - used during calculating the standard deviation */

	REAL	rNewS;
	/* New S parameter - used during calculating the standard deviation */

	REAL	rMeanSDisp;
	/* Variable used to capture mean (raw/rejected satflow/Stlost) during a period which is later used to display on the screen*/

	REAL	rSmoothed50;
	/*Smoothed Average of raw values, calculated when raw values are statistically significant*/

	
	REAL	rSmoothed15;
	/*Smoothed Average of raw values, calculated when raw values are statistically significant*/
	
	REAL	rStdDevS;
	/* Variable used to calculate and store Standard Deveations - */
	
	REAL	rSumSqrS;
	/*Varaible used to calcuate and store sum of squares*/

	REAL	rStdErrS;
	/*Varaible used to calculate and store standard deveation*/

	LOGIC	lRecAvgMetCriteria;
	/*Flag to indicate wether values recorded in rRaw meets criteria*/

	REAL	rSatinc;
	/*Time increment between veichles discharging at saturation flow*/

	REAL	r15thPercentile;
	/*15th Percentile calculated from raw values. 15th Percentile Sat flow is later fed back in mova to improve performance*/
	
	REAL	rConfInter;
	/*Confidence Interval for values in rRaw. Depends on number of values (n16EstNum)*/

	REAL	rConfLevel;
	/*Confidence level for the values recorded in rRaw*/



} PeriodData;


/* Strucure which is composed of structures for Saflow, Stlost,
   Rejected Satflow, Rejected Stlost which hold data collected for them individually
*/
typedef struct _LaneSatFlowSTlostPeriod
{
	PeriodData		Satflow;
	/*Period structure containing data for SatFlow*/
	PeriodData		Stlost;
	/*Period structure containing data for STlost*/
	PeriodData		SatflowRejected;
	/*Period structure containing data for rejected SatFlow*/
	PeriodData		StlostRejected;
	/*Period structure containing data for rejected STLOST*/

} LaneSatFlowSTlostPeriod;

#endif /* MOVASATFLOW_STATS_PRIVATE_H */
