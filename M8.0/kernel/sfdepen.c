/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         sfdepen.c
*
*  TITLE:        MOVA Saturation flow dependent variables calculator
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	31-May-2011		7.0.0		AA		PK		CRN003			Compute new values for variables affected by change in Satflow.
*	22-Feb-2012		7.0.3		AB		AK		CRN008			Correct typo in MovaSatFlowDependantVar_CalcComtimDefault_()
*	25-Jan-2013		7.0.5		..		AK		CRN023			Fix compiler warnings
*	31-Jan-2013		7.0.5		AC		AK		M7.0.5			Issue M7.0.5
*	15-Apr-2013		7.0.6		..		AK		CRN027			Improvements to SF feedback
*	03-May-2013		7.0.6		..		PA		CRN031			Don't reset SF data on warm restart
*	22-May-2013		7.0.6		AD		AK		M7.0.6			Issue M7.0.6
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


/*************************
*	Includes
**************************/

/* Standard libraries */
#include <stdlib.h>
#include <math.h>

/* MOVA includes */
#include "kdebug.h"
#include "gbltypes.h"
#include "sfdepen.h"
#include "sfmath.h"
#include "sftypes.h"
#include "datahand.h"

#include "dataset_interface.h"

#include "dataset_handler.h"

/*Maximum length of a lane. Assumed constant in MOVA but calculated in Mova setup. 
Confirmed with MRC on the safety of using fixed length */
#define gn16BONTIM_MAX_LENGTH			((int16) 60)

/* Max and Mins for the sat flow derived variables */
/* These limits are defined in AG 45 unless otherwise stated */
#define MAXMINMAX 250
#define MAXMINMIN 10
#define MOVEQXMAX 400
#define MOVEQXMIN 100
#define OSATCCMAX 20
#define OSATCCMIN 1
#define SATGAPMAX 20
#define SATGAPMIN 10
#define CRITGMAX 100
#define CRITGMIN 20
#define CRUSXMAX 60
#define CRUSXMIN 10
#define COMTIMMAX 500
#define COMTIMMIN 0
#define BONTIMMIN 0
#define BONTIMMAX 600
#define BONBCMAX 50
#define BONBCMIN 1
#define OSATTMMIN 10
#define OSATTMMAX 300



static LOGIC	glModuleInitialised = FALSE;
/*static const Repository_Type* pRepo;*/

static const Ccomshr * pRepo;



static LOGIC IsLongLane(int laneId);

static void update_maxmin(uint8 laneNo);
static void update_moveqx(uint8 laneNo);
static void update_osatcc(uint8 laneNo);
static void update_satgap(uint8 laneNo);
static void update_osattm(uint8 laneNo);
static void update_critg(uint8 laneNo);
static void update_comtim(uint8 laneNo);
static void update_bontim(uint8 linkNo);
static void update_bonbc(uint8 linkNo, uint8 laneNo);

static int calc_maxmin(const int dx, const int satinc, const int stlost);
static int calc_moveqx(int maxmin, int dx);
static int calc_osatcc(int dshort, int dx, int crusin, int satinc);
static int calc_osattm(const int osatcc, const int osatcx, const int cspeed, 
					   const int satinc, const int satgap, const int xosat);
static int calc_satgap(const int satinc);
static int calc_critg(const int satgap);
static int calc_comtim(const int satinc, const int stlost, const int assoc1, const int assoc2, const int dshort_assoc1, const int dshort_assoc2);
static int calc_bontim(int satinc, int stlost, int length);
static int calc_bonbc(int cspeed, int dx, int satinc, int stlost, int bontim);

static void rangeCheck(int* const ValueToCheck, const int MAX, const int MIN);




static void rangeCheck(int* const ValueToCheck, const int MIN, const int MAX)
{
	if (*ValueToCheck > MAX)
	{
		*ValueToCheck = MAX;
	}
	else if (*ValueToCheck < MIN)
	{
		*ValueToCheck = MIN;
	}
}


/*************************************************************************
 *
 * Function: MovaSatFlowDependantVar_Initialise_
 *
 * Description: Initialises module and set up TcomshrOffset structure values to Zero
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010
 *
 * Params: void
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlowDependantVar_Initialise_(void)
{
	if ( glModuleInitialised )
	{
		KDEBUG_WRITE0( DEBUG_LEVEL_ERROR, SFDEPEN, " Already Initialised! " );
	}

	KDEBUG_WRITE0( DEBUG_LEVEL_INFO, SFDEPEN, "MovaSatFlow Stats Dependent Var Calc: initialising" );

	glModuleInitialised = TRUE;

	pRepo = DSH_get_active_repository(); /*Replacing: GetActiveRepositoryPtr();*/
}



/*************************************************************************
 *
 * Function: MovaSatFlowDependantVar_ReInitialise_
 *
 * Description: ReInitialises module and set up TcomshrOffset structure values to Zero
 *
 * Author: PKale
 *
 * Date: 26-Nov-2010
 *
 * Params: void
 *
 * Return: void
 *
 ************************************************************************/
void MovaSatFlowDependantVar_ReInitialise_( void )
{
	if (!glModuleInitialised)
	{
		KDEBUG_WRITE0( DEBUG_LEVEL_ERROR, SFDEPEN, "Not Initialised " );
	}
	KDEBUG_WRITE0( DEBUG_LEVEL_INFO, SFDEPEN, "MovaSatFlow Stats Dependent Var Calc: Reinitialising" );

	pRepo = DSH_get_active_repository(); /*GetActiveRepositoryPtr();*/
}


/*************************************************************************
 *
 * Function:	MovaSatFlowDependantVar_UpdateDependentLaneVariables_
 *
 * Description:	 Update lane dependent variables.
 *
 * Author:		akirkham
 *
 * Date:		11/04/2013
 *
 * Params:		int laneNo		the lane number to operate on
 *
 * Return:		none
 *
 ************************************************************************/
void MovaSatFlowDependantVar_UpdateDependentLaneVariables_(uint8 laneNo)
{
	
	if (pRepo != NULL)
	{
		update_maxmin(laneNo);
		update_moveqx(laneNo);
		update_osatcc(laneNo);
		update_satgap(laneNo);
		update_osattm(laneNo);
		update_critg(laneNo);
		update_comtim(laneNo);
	}
	else
	{
		KDEBUG_WRITE0( DEBUG_LEVEL_ERROR, SFDEPEN, "Mova Sat Flow Lane calculations limited due to no repo available" );
	}
}


/*************************************************************************
 *
 * Function:	MovaSatFlowDependantVar_UpdateDependentLinkVariables_
 *
 * Description:	 Update link dependent variables.
 *
 * Author:		akirkham
 *
 * Date:		11/04/2013
 *
 * Params:		int linkNo		the link number to operate on
 *
 * Return:		none
 *
 ************************************************************************/
void MovaSatFlowDependantVar_UpdateDependentLinkVariables_(uint8 linkNo)
{
	uint8 i;

	if ((pRepo != NULL) && ( DI_get_bonus_green_recalc_time(linkNo) != 0 ))
	{/* If no bonus do not calculate default value */
		update_bontim(linkNo);

		for (i = 0; i < DI_get_link_lanes_count(linkNo); i++)
			update_bonbc(linkNo,  DI_get_lane_num(linkNo, i));
	}
}



int MovaSatFlowDependantVar_GetLinkNoFromLaneNo_(int laneNo)
{
	uint8 linkNo;
	uint8 i;

	for (linkNo = 1; linkNo <= DI_get_links_count(); linkNo++)
	{
		for (i = 0; i < DI_get_link_lanes_count(linkNo); i++)
		{
			if (laneNo == DI_get_lane_num(linkNo,i))
				return (int)linkNo;
		}
	}

	return 0;
}



int MovaSatFlowDependantVar_GetLinkIdFromLaneId_(int laneId)
{
	return (MovaSatFlowDependantVar_GetLinkNoFromLaneNo_(laneId+1) - 1);
}



static LOGIC IsLongLane(int laneId)
{
	if ((DI_get_in_det_distance((uint8)laneId+1) > 0) || ((DI_get_in_det_distance((uint8)laneId+1) <= 0) && (DI_get_short_lane_length((uint8)laneId+1) == 999)))
		return(TRUE);
	else
		return(FALSE);
}




/*************************************************************************
 *
 * Function:	DerivedDataCalculationRequired
 *
 * Description:	 Determine if the active value in Tcomshr should be updated
 *				 when the derived data calculations are requested.
 *				 Only recalculate active value if Repo values are not a limit
 * 				 and the calculated Repo value is not the limit
 *
 * Author:		panderson
 *
 * Date:		11/04/2013
 *
 * Params:		MAX: Max limit of variable in question
				MIN: Min limit of variable in question
				CalcRepo : The calculated value of the data based on repo values
				RepoValue : The value of the data in the repo

 *
 * Return:		TRUE indicating that calculations are required FALSE otherwise
 *
 * Note: Although this is a small function this functionality is repeated
		 in a number of places and if it is required to be changed in the
		 future it is better contained in the one place
 *
 ************************************************************************/
static BOOL DerivedDataCalculationRequired (const int Max, const int Min, 
											const int CalcRepo, const int RepoValue)
{
	if (((RepoValue == Max) || (RepoValue == Min)) && 
		(RepoValue == CalcRepo))
	{
		return FALSE;
	}
	return TRUE;

}



/* 
 * For each of the update_xxx() functions the basic method is the same...
 * First check the original data (from the repository as Tcomshr may have changed)
 * and see whether the user changed the value from the default.  Save this offset.
 * The calculate the value based on the current data in Tcomshr (with any updated
 * SATINC and STLOST values).
 * And then apply the user offset to the resulting value.
 */
static void update_maxmin(uint8 laneNo)
{
	int maxmin;
	int RepoDefaultCalc = calc_maxmin(pRepo->dx[laneNo-1], pRepo->satinc[laneNo-1], pRepo->stlost[laneNo-1]);
	int offset = 0;
	BOOL CalcReq = DerivedDataCalculationRequired(MAXMINMAX, MAXMINMIN, RepoDefaultCalc, pRepo->maxmin[laneNo-1]);

	if (CalcReq)
	{
		offset = pRepo->maxmin[laneNo-1] - RepoDefaultCalc;
		maxmin = offset + calc_maxmin(DI_get_x_det_distance(laneNo), DI_get_satinc_time(laneNo), DI_get_startup_lost_time(laneNo));
		rangeCheck(&maxmin, MAXMINMIN, MAXMINMAX);
		DI_set_max_min_green_time(laneNo, maxmin);
	}
	KDEBUG_WRITE4( DEBUG_LEVEL_INFO, SFDEPEN, "MAXMIN for lane %d is %d with offset %d calc performed = %d",laneNo, DI_get_max_min_green_time(laneNo), offset, CalcReq);
}

static void update_moveqx(uint8 laneNo)
{
	int moveqx;
	int RepoDefaultCalc = calc_moveqx(pRepo->maxmin[laneNo-1], pRepo->dx[laneNo-1]);
	int offset = 0;
	BOOL CalcReq = DerivedDataCalculationRequired(MOVEQXMAX, MOVEQXMIN, RepoDefaultCalc, pRepo->moveqx[laneNo-1]);
	
	if (CalcReq)
	{
		offset = pRepo->moveqx[laneNo-1] - RepoDefaultCalc;
		moveqx = offset + calc_moveqx(DI_get_max_min_green_time(laneNo), DI_get_x_det_distance(laneNo));
		rangeCheck(&moveqx, MOVEQXMIN, MOVEQXMAX);
		DI_set_moving_q_over_x_det(laneNo, moveqx);
	}
	KDEBUG_WRITE4( DEBUG_LEVEL_INFO, SFDEPEN, "MOVEQX for lane %d is %d with offset %d calc performed = %d ",laneNo, DI_get_moving_q_over_x_det(laneNo), offset, CalcReq);
}

static void update_osatcc(uint8 laneNo)
{
	/* For OSATCC, if the user has modified the default value,
	   always retain the user's value - do not calculate and apply offset. */
	int osatcc;
	int offset = 0;
	BOOL CalcReq = FALSE;
	int RepoDefaultCalc;

	RepoDefaultCalc = calc_osatcc(pRepo->dshort[laneNo-1], pRepo->dx[laneNo-1], pRepo->crusin[laneNo-1], pRepo->satinc[laneNo-1]);
	offset = pRepo->osatcc[laneNo-1] - RepoDefaultCalc;

	if (offset == 0)
	{
		CalcReq = DerivedDataCalculationRequired(OSATCCMIN, OSATCCMAX, RepoDefaultCalc, pRepo->osatcc[laneNo-1]);
	
		if (CalcReq)
		{
			osatcc = calc_osatcc(DI_get_short_lane_length(laneNo), DI_get_x_det_distance(laneNo), DI_get_in_det_cruise_time(laneNo), DI_get_satinc_time(laneNo));
			rangeCheck(&osatcc , OSATCCMIN, OSATCCMAX);
			DI_set_oversat_critical_veh_count(laneNo, osatcc);
		}
	}
	KDEBUG_WRITE4( DEBUG_LEVEL_INFO, SFDEPEN, "OSATCC for lane %d is %d with offset %d calc performed = %d", laneNo, DI_get_oversat_critical_veh_count(laneNo), offset, CalcReq);
}

static void update_satgap(uint8 laneNo)
{
	int satgap;
	int offset = 0;
	int RepoDefaultCalc = calc_satgap(pRepo->satinc[laneNo-1]);
	BOOL CalcReq = DerivedDataCalculationRequired(SATGAPMIN, SATGAPMAX, RepoDefaultCalc,  pRepo->satgap[laneNo-1]);

	if (CalcReq)
	{
		offset = pRepo->satgap[laneNo-1] - RepoDefaultCalc;
		satgap = offset + calc_satgap(DI_get_satinc_time(laneNo));
		rangeCheck(&satgap, SATGAPMIN, SATGAPMAX);
		DI_set_satflow_mean_gap(laneNo, satgap);

	}
	KDEBUG_WRITE4( DEBUG_LEVEL_INFO, SFDEPEN, "SATGAP for lane %d is %d with offset %d calc performed = %d",laneNo, DI_get_satflow_mean_gap(laneNo), offset, CalcReq);
}


static void update_osattm(uint8 laneNo)
{
	/* For OSATTM, if the user has modified the default value,
	   always retain the user's value - do not calculate and apply offset. */
	int osattm;
	int offset = 0;
	BOOL CalcReq = FALSE;
	int RepoDefaultCalc;

	RepoDefaultCalc = calc_osattm(pRepo->osatcc[laneNo-1], pRepo->osatcx[laneNo-1], pRepo->cspeed[laneNo-1], pRepo->satinc[laneNo-1], pRepo->satgap[laneNo-1], pRepo->xosat[laneNo-1]);
	offset = pRepo->osattm[laneNo-1]- RepoDefaultCalc;
	
	if (offset == 0)
	{
		CalcReq = DerivedDataCalculationRequired(OSATTMMIN, OSATTMMAX, RepoDefaultCalc, pRepo->osattm[laneNo-1]);

		if (CalcReq)
		{
			osattm = calc_osattm(DI_get_oversat_critical_veh_count(laneNo),
								 DI_get_oversat_critical_veh_count_compact(laneNo),
								 DI_get_cruise_speed(laneNo), 
								 DI_get_satinc_time(laneNo),
								 DI_get_satflow_mean_gap(laneNo),
								 DI_get_oversat_det(laneNo));
			DI_set_oversat_time(laneNo, osattm);
		}
	}
	KDEBUG_WRITE4( DEBUG_LEVEL_INFO, SFDEPEN, "OSATTM for lane %d is %d with offset %d calc performed = %d", laneNo, DI_get_oversat_time(laneNo), offset, CalcReq);
}


static void update_critg(uint8 laneNo)
{
	int critg;
	int offset = 0;
	int RepoDefaultCalc = calc_critg(pRepo->satgap[laneNo-1]);
	BOOL CalcReq = DerivedDataCalculationRequired(CRITGMIN, CRITGMAX, RepoDefaultCalc, pRepo->critg[laneNo-1]);

	if (CalcReq)
	{
		offset = pRepo->critg[laneNo-1] - RepoDefaultCalc;
		critg =  offset + calc_critg(DI_get_satflow_mean_gap(laneNo));
		rangeCheck(&critg, CRITGMIN, CRITGMAX);
		DI_set_lane_critical_gap(laneNo, critg);
	}
	KDEBUG_WRITE4( DEBUG_LEVEL_INFO, SFDEPEN, "CRITG for lane %d is %d with offset %d calc performed = %d",laneNo, DI_get_lane_critical_gap(laneNo), offset, CalcReq);
}

static void update_comtim(uint8 laneNo)
{
	int assoc1;
	int assoc2;
	uint8 assoc1_lane;
	uint8 assoc2_lane;
	int assoc1_dshort;
	int assoc2_dshort;
	uint8 lan;

	assoc1 = DI_get_assoc_det_num(laneNo, 1);
	assoc2 = DI_get_assoc_det_num(laneNo, 2);

	if (IsLongLane(laneNo-1) && assoc1 > 0)
	{
		assoc1_lane = 0;
		assoc2_lane = 0;
		assoc1_dshort = 0;
		assoc2_dshort = 0;

		for (lan = 1; lan <= DI_get_lanes_count(); lan++)
		{
			if (DI_get_x_det_num(lan) == assoc1)
			{
				assoc1_lane = lan;
				assoc1_dshort = DI_get_short_lane_length(lan);
				break;
			}
		}

		if (assoc2 > 0)
		{
			for (lan = 1; lan <= DI_get_lanes_count(); lan++)
			{
				if (DI_get_x_det_num(lan) == assoc2)
				{
					assoc2_lane = lan;
					assoc2_dshort = DI_get_short_lane_length(lan);
					break;
				}
			}
		}

		if (assoc1_lane > 0)
		{
			int comtim;
			int offset = 0;
			int RepoDefaultCalc = calc_comtim(pRepo->satinc[laneNo-1], pRepo->stlost[laneNo-1], assoc1, assoc2, assoc1_dshort, assoc2_dshort);
			BOOL CalcReq = DerivedDataCalculationRequired(COMTIMMIN, COMTIMMAX, RepoDefaultCalc, pRepo->comtim[laneNo-1]);

			if (CalcReq)
			{
				offset = pRepo->comtim[laneNo-1] - RepoDefaultCalc;
				comtim = offset + calc_comtim(DI_get_satinc_time(laneNo), 
											  DI_get_startup_lost_time(laneNo), assoc1, assoc2, assoc1_dshort, assoc2_dshort);
				rangeCheck(&comtim, COMTIMMIN, COMTIMMAX);
				DI_set_time_to_use_comb_x(laneNo, comtim);
			}

			KDEBUG_WRITE4( DEBUG_LEVEL_INFO, SFDEPEN, "COMTIM for lane %d is %d with offset %d calc performed = %d",laneNo, DI_get_time_to_use_comb_x(laneNo), offset, CalcReq);
			
			/* COMTIM is also applied to the associated lanes */
			DI_set_time_to_use_comb_x(assoc1_lane, DI_get_time_to_use_comb_x(laneNo));

			if (assoc2_lane > 0)
				DI_set_time_to_use_comb_x(assoc2_lane, DI_get_time_to_use_comb_x(laneNo));
		}
	}
}

static void update_bontim(uint8 linkNo)
{
	/* BONTIM is link based not lane based.  Update with the greatest value provided by any one lane on the link */
	/* We don't know the distance used to calculate BONTIM for any of the lanes.  Assume it is the same for all
	   lanes and just pick the lane that gives the largest value for a hypothetical distance.
	   Then update the current value of BONTIM with an offset based on how much
	   SATINC has changed from the dataset value. */
	int val;
	uint8 laneForBontim;
	int currentMaxBontim;
	uint8 i;
	int bontim;
	uint8 lan;

	/* Get the lane providing the maximum BONTIM value */
	currentMaxBontim = 0;
	laneForBontim = 0;
	for (i = 0; i < DI_get_link_lanes_count(linkNo); i++)
	{
		/* Set lan as the lane number of the i-th lane on the link */
		lan = DI_get_lane_num(linkNo, i);
		
		/* ignore any lane that isn't a long lane */
		if (DI_get_short_lane_length(lan) != 0)
			continue;

		val = calc_bontim(pRepo->satinc[lan-1], pRepo->stlost[lan-1], gn16BONTIM_MAX_LENGTH);
		if (val > currentMaxBontim )
		{
			currentMaxBontim = val;
			laneForBontim = lan;
		}
	}

	if (laneForBontim > 0)
	{
		/* Since we don't know the distance of the bonus, approximate the offset to
		   bontim that would result from any change to satinc.  Changes to stlost 
		   have a much smaller effect on bontim therefore are ignored.
		   A +0.1 change in SATINC gives a +1 change to BONTIM (approximately).
		   SATINC is stored in tenths, therefore apply SATINC offset directly */
		int offset = DI_get_satinc_time(laneForBontim) - pRepo->satinc[laneForBontim-1];


		bontim = DI_get_bonus_green_recalc_time(linkNo) + offset;
		rangeCheck(&bontim, BONTIMMIN, BONTIMMAX);
		DI_set_bonus_green_recalc_time(linkNo, bontim);

		KDEBUG_WRITE3( DEBUG_LEVEL_INFO, SFDEPEN, "The BONTIM value for link number %d after updating is: %d with offset: %d", linkNo, DI_get_bonus_green_recalc_time(linkNo), offset);
	}
}

static void update_bonbc(uint8 linkNo, uint8 laneNo)
{

	if (pRepo->bonbc[laneNo-1] > 0)
	{
		int bonbc;
		int offset = 0;
		int RepoDefaultCalc = calc_bonbc(pRepo->cspeed[laneNo-1], pRepo->dx[laneNo-1], pRepo->satinc[laneNo-1], pRepo->stlost[laneNo-1], pRepo->bontim[linkNo-1]);
		BOOL CalcReq = DerivedDataCalculationRequired(BONBCMIN, BONBCMAX, RepoDefaultCalc, pRepo->bonbc[laneNo-1]);

		if (CalcReq)
		{
			offset = pRepo->bonbc[laneNo-1] - RepoDefaultCalc;
			bonbc = offset + calc_bonbc(DI_get_cruise_speed(laneNo), 
										DI_get_x_det_distance(laneNo),
										DI_get_satinc_time(laneNo),
										DI_get_startup_lost_time(laneNo),
										DI_get_bonus_green_recalc_time(linkNo));
			rangeCheck(&bonbc, BONBCMIN, BONBCMAX);
			DI_set_bonus_base_count(laneNo, bonbc);
		}
		KDEBUG_WRITE4( DEBUG_LEVEL_INFO, SFDEPEN, "BONBC for lane %d is %d with offset %d calc performed = %d",laneNo , DI_get_bonus_base_count(laneNo), offset, CalcReq);
	}
}



static int calc_maxmin(const int dx, const int satinc, const int stlost)
{
	/* (STLOST-1.0s) + (DX x SATINC)/6.0 (secs to nearest 0.5s) */

	int maxmin;

	/* stlost stored in units of (value - 1) in 0.1 seconds */
	maxmin = (stlost + ((dx * satinc) / 6));

	/* Round to the nearest 0.5s */
	maxmin = ((((maxmin * 2) + 5 ) / 10) * 10);
	maxmin = maxmin / 2;

	rangeCheck(&maxmin, MAXMINMIN, MAXMINMAX);

	/* return in 0.1s*/
	return maxmin;
}

static int calc_moveqx(int maxmin, int dx)
{
	/* MOVEQX = MAX(0.4 * DX, MAXMIN + 3) */

	int moveqx;
	
	moveqx = maxmin + 30;
	if (moveqx < (4 * dx))
		moveqx = 4 * dx;

	/* round to nearest 0.5 sec (but keep in tenths) */
	moveqx = (((((moveqx * 2) + 5 ) / 10) * 10) / 2);

	rangeCheck(&moveqx, MOVEQXMIN, MOVEQXMAX);

	return moveqx;
}

static int calc_osatcc(int dshort, int dx, int crusin, int satinc)
{
	/* OSATCC = (2 * CRUISIN) / SATINC  for long lanes, or
	   OSATCC = DX / 7.2  otherwise. */
	/* value is truncated to an integer */
	int osatcc;

	if (satinc > 0)
	{
		if (dshort == 0) /* long lane */
		{
			/* cruisin stored in 0.5 sec units already */
			osatcc = (crusin * 10) / satinc;
		}
		else /* short lane or compact mova lane */
		{
			osatcc = (dx * 10) / 72;
		}

		rangeCheck(&osatcc, OSATCCMIN, OSATCCMAX);
	}
	else
	{
		osatcc = 0;
	}

	return osatcc;
}

static int calc_osattm(const int osatcc, const int osatcx, const int cspeed,
					   const int satinc, const int satgap, const int xosat)
{ 
	int i;
	double rIntegral;
	REAL rLocalSatinc;
	REAL rLocalSatGap;
	REAL rLocalCspeed;
	REAL rSatDist;
	REAL rGamber;
	REAL rReactTime;
	REAL rMovaGap;
	REAL rVehiDecel;
	REAL rTempOsattm;
	REAL rTemp;

	/* Either use osatcc or osatcx */
	int osatccx;

	rGamber = 2.5F;			/* " GAMBER for oversat purposes" */
	rReactTime = 0.5F;		/* " Driver reaction time" */
	rSatDist = 1.2F;		/* "average stationary distance between vehicles" */

	rLocalSatinc = ((REAL)satinc) / 10.0F;
	rLocalSatGap = ((REAL)satgap) / 10.0F;
	rLocalCspeed = (REAL)cspeed;

	/* if using the X-detector (possible Comb-X) */
	if (xosat == 1 || xosat == 2)
	{ /* Use osatcc */
		osatccx = osatcc;
	}
	else
	{ /* Use osatcx */
		osatccx = osatcx;
	}

	if (osatccx > 0 && rLocalSatGap > 0 && rLocalCspeed > 0 && rLocalSatinc > 0)
	{
		/* SATGAPMAX is stored in 1/10 of a seconds*/
		if (rLocalSatGap > SATGAPMAX/10.0F) 
			rLocalSatGap = SATGAPMAX/10.0F;

		/* calculate gap between moving vehicles at start of leaving amber*/
		rMovaGap = (rLocalCspeed * rLocalSatGap) - rSatDist;

		/* calculate deceleration of each subsequent vehicle based on
		   preceding vehicle, up to the last OSATCC vehicle */
		rVehiDecel = rLocalCspeed / (rGamber + rLocalSatinc - rReactTime);

		for (i = 1; i < osatccx; i++)
		{
			rVehiDecel = rLocalCspeed / ((2 * ((rMovaGap/rLocalCspeed) - rReactTime)) + (rLocalCspeed / rVehiDecel));
		}

		/* Calculate the time taken for last OSATCC vehicle to come to halt */
		rTemp = (rReactTime * osatccx) + (rLocalCspeed / rVehiDecel);

		/* Round to the nearest half second */
		modf((REAL)((rTemp * 2.0) + 0.5), &rIntegral);
		rTempOsattm = (REAL)(rIntegral / 2 );
	}
	else
	{
		rTempOsattm = 0;
	}

	return ((int)(rTempOsattm * 10));
}

static int calc_satgap(const int satinc)
{
	/* SATGAP = ((SATINC + 0.2)/3) + 0.7 */
	int satgap;

	satgap = (((satinc + 2) / 3) + 7);

	rangeCheck(&satgap, SATGAPMIN, SATGAPMAX);

	return satgap; /* return value in tenths */
}

static int calc_critg(const int satgap)
{
	/* CRITG = SATGAP + 2.0 sec */
	int critg;

	critg = satgap + 20;

	rangeCheck(&critg, CRITGMIN, CRITGMAX);

	return critg; /* return value in tenths */
}

static int calc_comtim(const int satinc, const int stlost, const int assoc1, 
					   const int assoc2, const int dshort_assoc1, const int dshort_assoc2)
{
	/* T = (STLOST-1.0s) + (DSHORT * SATINC)/6 (to nearest second) */
	/* COMTIM= T-(0.65*SQRT(T)+0.5) Truncated to integer */

	int comtim;
	int dshort = 0;

	if (assoc1 == 0)
		return 0;

	if (assoc2 > 0 && dshort_assoc2 > dshort_assoc1)
		dshort = dshort_assoc2;
	else
		dshort = dshort_assoc1;

	/* T = (STLOST-1.0s) + (DSHORT * SATINC)/6 (to nearest second) */
	/* stlost stored as (value -1.0) (in tenths), and satinc in tenths */
	comtim = (stlost + (dshort * satinc) / 6); /* gives T in tenths */

	/* round T to the nearest second (in tenths) */
	comtim = ((comtim + 5) / 10) * 10;
	
	/* COMTIM= T-(0.65*SQRT(T)+0.5) Truncated to integer */
	comtim = comtim - (int)((0.65 * sqrt((REAL)(comtim / 10)) * 10) + 5);
	comtim = ((comtim / 10) * 10);

	rangeCheck(&comtim, COMTIMMIN, COMTIMMAX);

	/* comtim returned in tenths */
	return comtim;
}

static int calc_bontim(int satinc, int stlost, int length)
{
	/* T = (STLOST-1.0s) + (D * SATINC)/6 (to nearest sec) */
	/* BONTIM = T + (0.65 * SQRT(T) + 0.5s) (truncated integer) */

	int bontim;

	/* T = (STLOST-1.0s) + (D x SATINC)/6 (to nearest sec) */
	/* stlost stored as (value -1.0) (in tenths), and satinc in tenths */
	bontim = (stlost + (length * satinc) / 6); /* gives T in tenths */

	/* round T to the nearest second (in tenths) */
	bontim = ((bontim + 5) / 10) * 10;

	if (bontim > 0)
	{
		/* BONTIM = T + (0.65 * SQRT(T) + 0.5s) (truncated integer) */
		bontim = bontim + (int)((0.65 * sqrt((REAL)(bontim / 10)) * 10) + 5);
		bontim = (bontim / 10) * 10;
	}
	else
	{
		bontim = 0;
	}

	rangeCheck(&bontim, BONTIMMIN, BONTIMMAX);

	/* bontim returned in tenths */
	return bontim;
}

static int calc_bonbc(int cspeed, int dx, int satinc, int stlost, int bontim)
{
	/* BONBC = (BONTIM - (STLOST - 1.0s) + CRUSX) / SATINC, rounded to integer */
	int crusx;
	int bonbc;
	
	/*Avoid divide by zero error*/
	if (cspeed > 0 && satinc > 0)
	{
		/* CRUSX = (DX-5m) / CSPEED */
		/* need it in tenths so multiply up the distance */
		crusx = (((((dx - 5) * 10) / cspeed) / 10) * 10);

		rangeCheck(&crusx, CRUSXMIN, CRUSXMAX);

		bonbc = ((bontim - stlost + crusx) * 10) / satinc; /* multiply by 10 to keep the decimal */

		/* then round up to integer */
		bonbc = (bonbc + 9) / 10;
	}
	else
	{
		bonbc = 0;
	}

	rangeCheck(&bonbc, BONBCMIN, BONBCMAX);

	return bonbc;
}

