/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         blocka.c
*
*  TITLE:        Mova Kernel: Updating/Housekeeping of mova data
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	26-Jul-1988		1.5.0		..		CL		......			mod to resetting ofPRECDC. New arrays added.
*	30-Aug-1988		1.6.0		..		CL		......			change to sus det routine to use AVEFLO data	
*	27-Oct-1988		2.0.0		..		CL		......			include local arrays INSCDC's etc in global.
*	15-Apr-1993		4.0.0		..		RV		......			Emergency/priority vehicle facilities added 
*	19-Jan-1994		4.0.0		..		RV		......			'Big' Extended stages, links, etc 
*	05-Sep-1994		4.0.0		..		AH		......			Added clamping of yvalue to stop it going to zero
*	26-Jan-1998		4.0.0		..		MC		SIE_00			First undergoing system test
*	03-Mar-1998		4.0.0		..		PC		SIE_01			Overlay on 'Xcomshr', etc., replaced by explicit use of structures.
*	12-May-1998		4.0.0		..		PC		SIE_02			Added call to day_from_date()(Siemens fault report MOVA-27)
*	17-Jan-2000		4.0.0		..		PC		SIE_03			day_from_date() parameters changed. 
*	20-Feb-2002		4.0.0		..		PC		SIE_04			MOVA-85: Emergency/priority detector 'sus' now reset (near S350)
*	22-Feb-2002		4.0.0		..		PC		SIE_05			MOVA-86: TRL changes for 'automatic selection of double-green stage sequencing'
*	29-Sep-2004		5.0.0		..		IH		SIE_06			Delayed demand time array added for emergency/priority links with 4TT and 5TT SDCodes.
*																Use of CMOVA_EP #define to enable/disable emergency/priority code 	
*	11-May-2006		5.0.3		..		IH		SIE_07			Bugfix M5_CRN0021. Average flow array access out of bounds and occasional divide-by-zero.
*	25-Jan-2005		5.1.0		..		RD		TRL_04			Incorporated Pedestrian priorities
*	31-May-2011		7.0.0		AA		PK		CRN003			Several Changes for MOVA 7 release
*	31-Aug-2011		7.0.1		..		PK		CRN004			SatFlow and CANDET improvements
*	12-Dec-2011		7.0.1		AB		AK		CRN005			Further CANDET improvements
*	22-Feb-2012		7.0.3		..		AK		CRN008			Correct compiler warning on check_detectors() and update comments to match code
*	06-Jul-2012		7.0.3		AC		AK		CRN012			Memory leak in TMA module and statistics lost on dataset change
*
*  (c) Copyright TRL Ltd 2012. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/

#include <nucleus.h>
#include <string.h> /* IRH MOD: M5.0.0: 07/10/04: For memset */
#include <proj_def.h>
#include <nu_omu.h>
#include "gbltypes.h"
#include <error.h>    /* TRL/SIE: for SOFT_ERROR calls */
#include "errhand.h"  /* TRL/SIE: for MOVA error handler */
#include "obclock.h"
#include <diff_bss.h>
#include "tma_alerts.h"
#include "tma_strc.h"
#include "tma_preprint.h"
#include "tma_mem.h"


static void end_of_stage_updates();
static void intergreen_updates();
static void check_detectors(int linkNo, int laneNo);
static void stage_green();
static void update_vehicle_link_counts(int linkNo);
static void update_ped_ep_link_counts(int linkNo);
static int get_lane_det_fault_status(int linkNo, int laneNo);
static int get_upstream_det_fault_status(int laneNo, const Ccomshr *const Lcomshr);
static int get_counts_from_dets(int laneNo, const Ccomshr *const Lcomshr);
static void update_lane_smflow(int laneNo, int flow, int cnt);
static int get_ave_flow(int laneNo, const Ccomshr *const Lcomshr);


/* IRH MOD: M5.0.0: 02/09/04: Delayed demand time for emergency/priority links 
 * with 4TT and 5TT SDCodes.  Used externally in ProcessNTTcode() (block p)
 * and below S6400 in blockb(); reset at end of subsection A4 in blocka(). */
int iDemandDelay_[ mxlink ];

#if defined (CMOVA_EP) /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */
extern int cancelLinkEP[mxlink];
#endif


void blocka(void)
{
/*
 //////////////////////////////////////////////////////////////////////
     BLOCK A : END-STAGE/INTERGREEN HOUSEKEEPING ; SET MIN-GREENS
 //////////////////////////////////////////////////////////////////////
     SUBSECTION A1 : IDENTIFY TIME IN CYCLE FOR END-STAGE UPDATES
                      ZERO/RESET VARIABLES AT END-STAGE TIME :-
                      THIS MAY BE SAME AS STAGE-START AFTER ZERO IG.
*/
	if (Tcomshr->cusig == 0)
	{
		if (Tcomshr->presig == 0)
		{
			/* Continuing intergreen */
			Tcomshr->marker = 1;
		}
		else
		{
			/* Start of intergreen */
			end_of_stage_updates();
			Tcomshr->marker = 0;
		}
		intergreen_updates();
	}
	else
	{
		if (Tcomshr->presig != Tcomshr->cusig && Tcomshr->presig != 0)
		{
			/* Stage change with no intergreen */
			end_of_stage_updates();
		}
		stage_green();
	}
	return;
}



/*****************************************************************************
 * end_of_stage_updates()
 * Section A1
 *****************************************************************************/
static void end_of_stage_updates()
{
	/* Zero/reset stage/junction variables at end-stage */

	int linkIdx;
	int a,b,c;
	int n;
	int j;
	long longI;

	if (Tcomshr->warmup > 0)
	{
		for (j = 1; j <= Tcomshr->stages; j++)
		{
			n = Tcomshr->lastag + j;
			if (n > Tcomshr->stages)
				n -= Tcomshr->stages;
        
			/* decrement CUTMAX for stage just ended and any skipped */
			if (Tcomshr->cutmax > 0)
				Tcomshr->cutmax--;

			if (n == Tcomshr->presig)
				break;

			/* update green proportion for skipped stages */
			a = 1;
			if (Tcomshr->lambda[n-1] <= 5)
				a = 0;
			Tcomshr->lambda[n-1] = (3*Tcomshr->lambda[n-1]+a)/4;
        
			/* Zero marker for skipped stage not required by 6MRR code */
			if (Tcomshr->stadem[n-1] == -6)
				Tcomshr->stadem[n-1] = 0;

			Tcomshr->stcycl[n-1] = 0;

			checkAlertExiting(pTmaData, n-1, Tcomshr->laensa);
		}

		if (Tcomshr->stcycl[Tcomshr->presig-1] <= 0 || 
			Tcomshr->warmup <= Tcomshr->stages || Tcomshr->rav1 > 0)
		{
			/* Skip lambda update if already done or in warmup */
		}
		else
		{
			/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
			DivideError((int)(Tcomshr->stcycl[Tcomshr->presig-1]/10), 201, 
				"presig was %d, stcycl was %d", 
				Tcomshr->presig, 
				Tcomshr->stcycl[Tcomshr->presig-1] );

			a = Tcomshr->sistim*10/(Tcomshr->stcycl[Tcomshr->presig-1]/10);/* DIV !*/

			/* CALCULATE NEW LAMBDA FOR STAGE JUST ENDED, THEN SMOOTH */
			Tcomshr->lambda[Tcomshr->presig-1] = (3*Tcomshr->lambda[Tcomshr->presig-1]+a+2)/4;
			Tcomshr->altlam[Tcomshr->presig-1] = (3*Tcomshr->altlam[Tcomshr->presig-1]+a+2)/4;

			/*  TRL mod:3107961 - Use 4-byte integer, longI, to prevent overflow*/
			longI = (long)(3 * Tcomshr->stages);
			longI = (long)(Tcomshr->smcycl) * longI;
			longI = longI + (long)(Tcomshr->stcycl[Tcomshr->presig-1] +5); 
			DivideError((int)(3*Tcomshr->stages+1), 202);
			Tcomshr->smcycl = (int)(longI/(long)(3*Tcomshr->stages+1));     /* DIV */
		}
	}

	checkAlertExiting( pTmaData, Tcomshr->presig-1, Tcomshr->laensa);

	Tcomshr->stcycl[Tcomshr->presig-1] = 0;
	Tcomshr->totlam = 0;

	for (n = 1; n <= Tcomshr->stages; n++)
	{
		Tcomshr->totlam = Tcomshr->totlam + Tcomshr->lambda[n-1];
		Tcomshr->stensa[n-1] = 0;
	}

	/* skip setting of drift-max's until after warmup */
	if (Tcomshr->warmup > Tcomshr->stages)
	{
		c = Tcomshr->smcycl/10;

		/* .87 * .87 gives total drift-max = 75% TOTALG.
		   Remaining 25% is tolerance beyond drift-max */
		DivideError((int)Tcomshr->totlam, 203,
			"lambda[0] was %d, lambda[1] was %d, lambda[2] was %d, "
			"lambda[3] was %d, lambda[4] was %d, lambda[5] was %d, ",
			Tcomshr->lambda[0], Tcomshr->lambda[1], Tcomshr->lambda[2], 
			Tcomshr->lambda[3], Tcomshr->lambda[4], Tcomshr->lambda[5] );

		a = Tcomshr->totalg/10*87/Tcomshr->totlam;/* DIV !*/

		/* 'a' has range 50-800 sec.  High values give excessive max */
		if (a > 180)
		{
			/* avoid overflow and huge max's */
			if (a > 320)
				a = 320;

			/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error 
			 * and changed from 10*c to just c */
			DivideError((int)c, 206,
				"smcycl was %d", Tcomshr->smcycl);
			b = a/10*a/c*10;               /* DIV !*/
		}
		else
		{
			/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
			DivideError((int)c, 204,
				"smcycl was %d", Tcomshr->smcycl);
			b = a*a/c;                     /* DIV !*/
		}

		/* 'b' is green to be apportioned (sec) */
		if (b > 320)
			b = 320;

		/* now update drift-max values */
		for (n = 1; n <= Tcomshr->stages; n++)
		{
			Tcomshr->drifmx[n-1] = (b*Tcomshr->altlam[n-1]+25)/50*5;
		}

#if defined (CMOVA_EP) /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */

		/* Now deal with truncation and skip indicators for links
		   affected by emergency or priority action */

		/* decrement/reset indicators for links previously truncated or skipped
		   or that will be skipped by emergency or priority action */
		for (j = 1; j <= Tcomshr->stages; j++) 
		{
			n = Tcomshr->presig + j;
			if (n > Tcomshr->stages)
				n -= Tcomshr->stages;

			/* First, reduce existing indicators for each stage passed */
			for (linkIdx = 0; linkIdx < Tcomshr->nlinks; linkIdx++) 
			{
				if (Tcomshr->eptrun[linkIdx] > 0)
					Tcomshr->eptrun[linkIdx]--;
				if(Tcomshr->epskip[linkIdx] > 0)
					Tcomshr->epskip[linkIdx]--;
			}

			/* break if have reached next stage to which changing */
			if (n == Tcomshr->snext)
				break;

			/* continue if no emergency/priority change to cause any skipping */
			if (Tcomshr->esnext == 0 && Tcomshr->psnext == 0)
				continue;

			/* continue if no demand for skipped stage */
			if (Tcomshr->stadem[n-1] <= 0)
				continue;

			/* set skip ind's */
			for (linkIdx = 0; linkIdx < Tcomshr->nlinks; linkIdx++)
			{
				/* Skip emerg/priority links, or links red in skipped stage N */
				if (Tcomshr->ltype[linkIdx] < 0 || Tcomshr->lgreen[n-1][linkIdx] == 0)
					continue;

				/* Skip setting of ind's for links not demanded */
				if (Tcomshr->lindem[linkIdx] == 0 && (int)Tcomshr->revdem[linkIdx] == 0)
					continue;

				/* set skip ind's for both ped and traffic links */
				Tcomshr->epskip[linkIdx] = (char)(Tcomshr->stages+Tcomshr->stages); 
			}
		}

		/* clear ind's if last green was enough */
		/* Note: if gets enough green in future, ind's will be cleared in B4 */
		for (linkIdx = 0; linkIdx < Tcomshr->nlinks; linkIdx++) 
		{
			/* Skip emerg/priority links, or links red in prev. stage */
			if (Tcomshr->ltype[linkIdx] < 0 || Tcomshr->lgreen[Tcomshr->presig-1][linkIdx] == 0) 
				continue;

			if (Tcomshr->ltype[linkIdx] == 0)
			{
				/* skip traffic link if still sat'd or had opposed green last stage */
				if (Tcomshr->lgreen[Tcomshr->presig-1][linkIdx] == 4 || Tcomshr->liensa[linkIdx] == 0)
					continue;
			}

			/* clear ind's for all ped links and 
			   traffic links previously end-sat in unopposed green */
			Tcomshr->epskip[linkIdx] = 0;
			Tcomshr->eptrun[linkIdx] = 0;
		}

		/* Clear Tcomshr->cancel for e/p links that were green in previous stage
		   or any stages being skipped this change */
		for (linkIdx = 0; linkIdx < Tcomshr->nlinks; linkIdx++)
		{
			if (Tcomshr->ltype[linkIdx] < 0)
			{
				n = Tcomshr->presig;
				do
				{
					if (Tcomshr->lgreen[n-1][linkIdx] != 0 &&
						Tcomshr->lgreen[n-1][linkIdx] != 4)
					{
						Tcomshr->cancel[linkIdx] = 0;
						cancelLinkEP[linkIdx] = 0;
					}

					n++;
					if (n > Tcomshr->stages)
						n -= Tcomshr->stages;
				}
				while (n != Tcomshr->demsta);
			}
		}
#endif /* CMOVA_EP */ /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */
	}

	/* Now reset variables at end of stage */

	/* Clear normal/emergency/priority demands & priority ext max timer */
	Tcomshr->stadem[Tcomshr->presig-1] = 0;
#if defined (CMOVA_EP) /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */
	Tcomshr->estdem[Tcomshr->presig-1] = 0;
	Tcomshr->pstdem[Tcomshr->presig-1] = 0;
	Tcomshr->epxmxt = 0;
#endif /* CMOVA_EP */ /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */

	Tcomshr->lastag = Tcomshr->presig;
	Tcomshr->sistim = Tcomshr->scan;

	/* initialise signal state timer to time IG */
	Tcomshr->stamax = 0;
	Tcomshr->stamin = 0;
	Tcomshr->waitim = 0;
}



/*****************************************************************************
 * intergreen_updates()
 * Sections A2, A3, A4
 *****************************************************************************/
static void intergreen_updates()
{
	/* continuation of stage to stage intergreen */

	int linkIdx;

	/* Section A2: reset variables and check dets */

	Tcomshr->junsmf = 0;
	for (linkIdx = 0; linkIdx < Tcomshr->nlinks; linkIdx++) 
	{
		/* Sum smoothed flows for all links */
		Tcomshr->junsmf += Tcomshr->lsmflo[linkIdx];

		/* link red (or effective red) in stage just ended */
		if (Tcomshr->lgreen[Tcomshr->lastag-1][linkIdx] == 0 || 
			Tcomshr->lgreen[Tcomshr->lastag-1][linkIdx] == 4)
			continue;

		if (Tcomshr->lgrefl[linkIdx] == 0)
		{
			/* link previously green but not now -- reset now */
		}
		else
		{
			/* link now green but expected to change soon */
			/* reset it later */
			if (Tcomshr->lgreen[Tcomshr->snext-1][linkIdx] == 0)
				continue;

			/* now left with links green from lastag to snext */

			/* skip resets unless SNEXT < LASTAG and cycle is restarting */
			if (Tcomshr->snext > Tcomshr->lastag)
				continue;
			/* or if link has not been green for > cycle time */
			if (Tcomshr->lingre[linkIdx] <= Tcomshr->smcycl)
				continue;
		}

		if (Tcomshr->cycltm[linkIdx] < Tcomshr->sistim)
		{
			/* Link already partially reset */
		}
		else
		{
			if (Tcomshr->ltype[linkIdx] == 0)
			{
				/* normal vehicle links */
				update_vehicle_link_counts(linkIdx+1);
			}
			else
			{
				/* pedestrian/emergency/priority link -- omit lane checks */
				/* save cumulative detector counts */
				update_ped_ep_link_counts(linkIdx+1);
			}

			Tcomshr->cycltm[linkIdx] = 0;
		}

		/* skip timer reset if link is still green */
		if (Tcomshr->lgrefl[linkIdx] > 0)
			continue;

		/* skip if timers already reset on a previous scan */
		if (Tcomshr->lingre[linkIdx] < Tcomshr->sistim)
			continue;

		/* Zero timers at start of amber */
		Tcomshr->prensa[linkIdx] = Tcomshr->liensa[linkIdx];
		Tcomshr->liensa[linkIdx] = 0;
		Tcomshr->lingre[linkIdx] = 0;
		Tcomshr->lwaste[linkIdx] = 0;
		Tcomshr->maxpcs[linkIdx] = 0;
		Tcomshr->mingre[linkIdx] = 0;
		Tcomshr->lindem[linkIdx] = 0;
          
		/* IRH MOD: M5.0.0: 02/09/04: Reset demand delays for
		 * emergency/priority links with 4TT and 5TT SDCodes */
		memset( iDemandDelay_, 0, sizeof( iDemandDelay_ ) );
	}

	return;
}



/*****************************************************************************
 * check_detectors(int linkNo, int laneNo)
 * Section A3
 * Check detectors on the given lane and update suspect detector flags
 *
 * parameters:
 *	linkNo	- link Number
 *	laneNo	- lane Number
 *****************************************************************************/
static void check_detectors(int linkNo, int laneNo)
{
	int detNo;
	int a,c;
	int aa,f;
	int i;

	/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
	DivideError((int)Tcomshr->smflow[laneNo-1], 207,
		"lane number was %d", laneNo);

	aa = 32000/Tcomshr->smflow[laneNo-1];/* DIV ! M 1.6 CHANGE F TO AA*/
	if (aa < 1200)
		aa = 1200;

	for (i = 1; i <= 9; i++) 
	{
		if (i == 7 || i == 8)
			continue;

		detNo = Tcomshr->det[laneNo-1][i-1];
		if (detNo <= 0)
			continue;

		if (Tcomshr->susdet[detNo-1] != 0)
		{
			/* detector was suspect, see if it is ok now */

			/* get the count at the end of the previous cycle,
				noting that sink dets store counts in different variables */
			a = Tcomshr->precdc[detNo-1];
			if (i == 4)
				a = Tcomshr->inscdc[laneNo-1];
			if (i == 6)
				a = Tcomshr->xskcdc[laneNo-1];

			/* check if cumulative count has increased in the last cycle */
			c = Tcomshr->cdc[detNo-1] - a;
			if (c > 0)
			{
				Tcomshr->susdet[detNo-1] -= c;

				if (Tcomshr->susdet[detNo-1] <= 0)
				{
					Tcomshr->susdet[detNo-1] = 0;
					detFaultyChanged(detNo-1, 0);
				}
			}
		}

		if (Tcomshr->timon[detNo-1] >= Tcomshr->cycltm[linkNo-1] || 
			Tcomshr->timon[detNo-1] >= 2400) 
		{
			/* long on dets set SUSDET = 2 */
			Tcomshr->susdet[detNo-1] = 2;
			detFaultyChanged(detNo-1, 2);
		}

		/* Get average flows before checking for long-off dets */
		f = get_ave_flow(laneNo, Tcomshr);
				
		/* skip long-off check if low smflow (<=150 veh/h) */
		if (f <= 15)
			continue;

		/* skip long-off check for sink, comb and alt dets */
		if (i > 3 && i < 9)
			continue;

		/* set susdet=1 for long-off dets */
		if (Tcomshr->timoff[detNo-1] >= aa)
		{
			Tcomshr->susdet[detNo-1] = 1;
			detFaultyChanged(detNo-1, 1);
		}
	}

	Tcomshr->ladetf[laneNo-1] = get_lane_det_fault_status(linkNo, laneNo);
}


static int get_lane_det_fault_status(int linkNo, int laneNo)
{
	int retval;
	int aa, a, b, c, e;
	int i;

	retval = Tcomshr->ladetf[laneNo-1];
	aa = 0;
	b = Tcomshr->det[laneNo-1][1];
	c = Tcomshr->det[laneNo-1][4];
	
	if (c == 0)
	{
		/* No COMBX det so check others */

		/* X det is OK? */
		if (Tcomshr->susdet[b-1] == 0)
		{
			if (Tcomshr->det[laneNo-1][3] != 0)
				retval = -1;
			return retval;
		}

		/* Is there an ALT-DWN det */
		e = Tcomshr->det[laneNo-1][7];
		if (e != 0)
		{
			if (Tcomshr->susdet[e-1] == 0)
			{
				if (Tcomshr->det[laneNo-1][3] != 0)
					retval = -1;
				return retval;
			}
		}
	}
	else
	{
		for (i = 1; i <= 2; i++) 
		{
			a = Tcomshr->assocd[laneNo-1][i-1];
			if (a > 0 && Tcomshr->susdet[a-1] > 0)
				aa = 1;
		}

		if (Tcomshr->susdet[b-1] == 0 && aa == 0)
		{
			/* all components of COMB-X are ok */
			if (Tcomshr->det[laneNo-1][3] != 0)
				retval = -1;
			return retval;
		}

		Tcomshr->susdet[c-1] = 1;
		detFaultyChanged(c-1, 1);

		if (Tcomshr->susdet[b-1] > 0 && aa > 0)
		{
			retval = 2;
			/* IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: do not assume IN-det means long link */
			if (IS_LONG_LANE(laneNo-1) || Tcomshr->shortl[linkNo-1] != 0)
				Tcomshr->lidetf[linkNo-1] += retval;
			return retval;
		}

		if (Tcomshr->susdet[b-1] == 0)
		{
			e = Tcomshr->det[laneNo-1][6];
			if (e == 0 || ( e > 0 && Tcomshr->susdet[e-1] > 0 ) )
			{
				retval = 2;
				/* IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: do not assume IN-det means long link */
				if (IS_LONG_LANE(laneNo-1) || Tcomshr->shortl[linkNo-1] != 0)
					Tcomshr->lidetf[linkNo-1] += retval;
				return retval;
			}

			/* increment LIDETF so don't do o'sat ES check (C4) */
			Tcomshr->lidetf[linkNo-1] += 1;

			/* identify lane as needing 2-part ES check */
			return -1;
		}
	}

	/* Can't check ES on X, COMBX, ALTXDN dets due to faults */

	/* Check upstream alternatives */
	retval = get_upstream_det_fault_status(laneNo, Tcomshr);

	/* IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: do not assume IN-det means long link */
	if (IS_LONG_LANE(laneNo-1) || Tcomshr->shortl[linkNo-1] != 0)
		Tcomshr->lidetf[linkNo-1] += retval;
	return retval;
}


static int get_upstream_det_fault_status(int laneNo, const Ccomshr *const Lcomshr)
{
	int d,e;

	/* Try to susbstitute ALT-UP (IN) det */

	/* Check the ALT-UP det exists and is working */
	e = Lcomshr->det[laneNo-1][6];
	if (e == 0)
		return 2;
	if (Lcomshr->susdet[e-1] > 0)
		return 2;

	/* next check whether there is an OUT det which could corrupt the counts */
	if (Lcomshr->det[laneNo-1][2] > 0)
		return 2;

	/* a faulty IN-SINK or X-SINK could corrupt counts too */
	e = Lcomshr->det[laneNo-1][3];
	if (e > 0 && Lcomshr->susdet[e-1] > 0)
		return 2;

	d = Lcomshr->det[laneNo-1][5];
	if (d > 0 && Lcomshr->susdet[d-1] > 0)
		return 2;

	return 1;
}



/*****************************************************************************
 * stage_green()
 * Section A5
 *****************************************************************************/
static void stage_green()
{
/*
 --- RETURN AFTER HOUSEKEEPING DURING INTERGREEN
/////////////////////////////////////////////////////////////////////
     SUBSECTION A5 : LIGHTS ARE IN STAGE GREEN PART OF CYCLE
                      SKIP IF WAITING FOR CHANGE. ELSE, AT MIN(CUSIG):
                      SET MIN-GREENS FOR LINKS STARTING THIS STAGE
                      SET STAGE-MIN TO MAX OF THESE MINS
                      SUBJECT TO ABSOLUTE STAGE-MIN
*/
	int linkNo, laneNo, detNo;
	int b,c;
	int i;

	/* initialise for stage start unless merely continuing green */
	if (Tcomshr->cusig != Tcomshr->presig)
		Tcomshr->sistim = Tcomshr->scan;

	if (Tcomshr->waitim != 0)
	{
		/* waiting for change to next stage (DEMSTA) */
		Tcomshr->waitim += Tcomshr->scan;
		Tcomshr->marker = 8;
		return;
	}

    /* Mark junction as in stg-green */
	Tcomshr->marker = 2;

    if (Tcomshr->stamin > 0)
    {
		/* Withing stage variable min green - set marker and return */
		Tcomshr->marker = 3;
		return;
    }

    /* If stage absolute min-green not reached then return */
    if (Tcomshr->sistim < Tcomshr->min[Tcomshr->cusig-1])
		return;

	Tcomshr->stamin = Tcomshr->min[Tcomshr->cusig-1];

	for (linkNo = 1; linkNo <= Tcomshr->nlinks; linkNo++)
	{
		/* Skip link if it is red now */
		if (Tcomshr->lgrefl[linkNo-1] == 0)
			continue;

		if (Tcomshr->mingre[linkNo-1] != 0)
		{
			if (Tcomshr->lgreen[Tcomshr->cusig-1][linkNo-1] == 3 && 
				Tcomshr->lgreen[Tcomshr->lastag-1][linkNo-1] != 3)
			{
				/* Overlap link (opposed sometime) reset min green now */

				if (Tcomshr->ltype[linkNo-1] <= 0)
				{
					Tcomshr->lingre[linkNo-1] = Tcomshr->sistim;
					if (Tcomshr->ltype[linkNo-1] == 0)
					{
						/* Reset green timier. Skip e/p links which have no lanes */
						Tcomshr->liensa[linkNo-1] = 0;
						Tcomshr->lwaste[linkNo-1] = 0;
						Tcomshr->maxpcs[linkNo-1] = 0;

						for (i = 1; i <= Tcomshr->llatot[linkNo-1]; i++)
						{
							laneNo = Tcomshr->llanes[linkNo-1][i-1];
							Tcomshr->laensa[laneNo-1] = 0;
						}
					}
				}
			}
			else
				continue;
		}


		/* set absolute min green for link */
		Tcomshr->mingre[linkNo-1] = Tcomshr->lmin[linkNo-1];

		if (Tcomshr->ltype[linkNo-1] == 0)
		{
			/* variable min only set for vehicle links */

			for (i = 1; i <= Tcomshr->llatot[linkNo-1]; i++)
			{
				laneNo = Tcomshr->llanes[linkNo-1][i-1];

				/* First check for a queue over the IN (ALT-UP) det */
				detNo = Tcomshr->det[laneNo-1][6];			
				if (detNo > 0 && Tcomshr->susdet[detNo-1] == 0
					&& Tcomshr->timon[detNo-1] >= Tcomshr->qminon[laneNo-1])
				{
					/* give enhanced min if queue over IN det */
					b = Tcomshr->maxmin[laneNo-1] + Tcomshr->satinc[laneNo-1];
				}
				else
				{
					detNo = Tcomshr->det[laneNo-1][1];
					if (Tcomshr->susdet[detNo-1] > 0)
					{
						/* if X det faulty, check alternatives */
						/* try ALT-DWN (OUT) det */
						detNo = Tcomshr->det[laneNo-1][7];
						if (detNo == 0 || Tcomshr->susdet[detNo-1] > 0)
						{
							/* can't use OUT det - try IN (ALT-UP) det next */
							detNo = Tcomshr->det[laneNo-1][6];
							if (detNo == 0 || Tcomshr->susdet[detNo-1] > 0)
							{
								/* no usable detectors, go with a default value */
								/* M7: use average min green rather than MAXMIN */
								/*b = Tcomshr->maxmin[laneNo-1];*/
								b = Tcomshr->mingre[linkNo-1];
							}
							else
							{
								/* use red count from IN det as alternative to X det */
								c = Tcomshr->redcin[laneNo-1];
								b = Tcomshr->stlost[laneNo-1] + c * Tcomshr->satinc[laneNo-1];
								/* limit lane min green to lane MAXMIN */
								if (b > Tcomshr->maxmin[laneNo-1])
									b = Tcomshr->maxmin[laneNo-1];
							}
						}
						else
						{
							/* Absolute min ok if downstream alt-det ok */
							b = 0;
						}
					}
					else
					{
						if (Tcomshr->timon[detNo-1] >= Tcomshr->qminon[laneNo-1])
						{
							/* if queue over X det can't use it - set min = MAXMIN */
							b = Tcomshr->maxmin[laneNo-1];
						}
						else
						{
							/* use the X det red count to calculate min green */
							c = Tcomshr->redcx[laneNo-1];
							b = Tcomshr->stlost[laneNo-1] + c * Tcomshr->satinc[laneNo-1];
							/* limit lane min green to lane MAXMIN */
							if (b > Tcomshr->maxmin[laneNo-1])
								b = Tcomshr->maxmin[laneNo-1];
						}
					}
				}

				/* if vari-min for this lane is > current link vari-min, update link vari-min */
				if (b > Tcomshr->mingre[linkNo-1])
					Tcomshr->mingre[linkNo-1] = b;
			}
		}

		if(Tcomshr->lgreen[Tcomshr->cusig-1][linkNo-1] != 1)
			continue;

		/* If only green stage for this link is current stage
		 * set stage min-green as max of mins for single-green links*/
		b = Tcomshr->mingre[linkNo-1] - (Tcomshr->lingre[linkNo-1] - Tcomshr->sistim);
		if (b > Tcomshr->stamin)
			Tcomshr->stamin = b;
    }

	/* mark variable min stage length set */
    Tcomshr->marker = 3;
	return;
}


static void update_vehicle_link_counts(int linkNo)
{
	int m;
	int a,b,c,d,f;
	int i,j,k;

	/* normal vehicle links */
	Tcomshr->lidetf[linkNo-1] = 0;
	Tcomshr->lsmflo[linkNo-1] = 0;
	Tcomshr->extrag[linkNo-1] = 0;
	Tcomshr->yvalue[linkNo-1] = 24;
	j = Tcomshr->llatot[linkNo-1];

	for (k = 0; k < j; k++) 
	{
		/* for all lanes on link, reset red counts & zero end sat markers */

		m = Tcomshr->llanes[linkNo-1][k];

		/* before zeroing REDCX, save spare green for all lanes */
		a = Tcomshr->lingre[linkNo-1]-Tcomshr->redcx[m-1]*Tcomshr->satinc[m-1];
		if (a > 0)
			Tcomshr->extrag[linkNo-1] += a;
			
		a = Tcomshr->gamber[m-1];
		/* count to exit of register if OUT det exists */
		if (Tcomshr->det[m-1][2] > 0)
			a = 1;

		b = Tcomshr->crusx[m-1];
		Tcomshr->redcx[m-1] = 0;
		Tcomshr->exitrc[m-1] = 0;
		Tcomshr->sinkrc[m-1] = 0;
		Tcomshr->ladetf[m-1] = 0;
		Tcomshr->inxtra[m-1] = 0;
		Tcomshr->laensa[m-1] = 0;
			
		/* skip if GAMBER > CRUSX */
		if (a <= b)
		{
			for (i = a-1; i < b; i++) 
				Tcomshr->redcx[m-1] += (int)Tcomshr->shrega[m-1][i];
		}

		Tcomshr->osatic[m-1] = Tcomshr->redcx[m-1];

		/* Ignore lanes with no (or negative) IN dets */
		if (Tcomshr->det[m-1][0] > 0)
		{
			c = Tcomshr->crusin[m-1];
			Tcomshr->redcin[m-1] = Tcomshr->redcx[m-1];
				
			/* set o'sat count from IN det unless X det specified  */
			b++;
			for (i = b-1; i < c; i++) 
				Tcomshr->redcin[m-1] += (int)Tcomshr->shrega[m-1][i];

			if (Tcomshr->xosat[m-1] == 0)
				Tcomshr->osatic[m-1] = Tcomshr->redcin[m-1];
		}


		if (Tcomshr->warmup <= Tcomshr->stages)
			c = -1;
		else
		{
			/* SUBSECTION A3 : CHECK DETECTORS */
			check_detectors(linkNo, m);

			/* SUBSECTION A4 : UPDATE SMOOTHED FLOW (VEH/H/10) */
			c = get_counts_from_dets(m, Tcomshr);
		}

		if (c < 0)
		{
			/* count from dets unreasonable - use historic flow */
			f = get_ave_flow(m, Tcomshr);
			/* estimate cyclic flow count from historical flow */
			c = Tcomshr->cycltm[linkNo-1]/10*f/360;

			/* If in warmup, just initialise SMFLOW with the value */
			if (Tcomshr->warmup <= Tcomshr->stages)
				Tcomshr->smflow[m-1] = f;
			else
				update_lane_smflow(m, f, c);
		}
		else
		{
			/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
			DivideError((int)(Tcomshr->cycltm[linkNo-1]/10), 208,
				"link number was %d", linkNo);

			/* flow rate is count/cycle in sec * 360 for veh/hr/10 */
			f = c*360/(Tcomshr->cycltm[linkNo-1]/10);/* DIV !*/

			if (f <= 240)
			{
				update_lane_smflow(m, f, c);
			}
			else
			{
				/* f is unreasonable - use historic flow instead */
				f = get_ave_flow(m, Tcomshr);
				/* estimate cyclic flow count from historical flow */
				c = Tcomshr->cycltm[linkNo-1]/10*f/360;

				/* If in warmup, just initialise SMFLOW with the value */
				if (Tcomshr->warmup <= Tcomshr->stages)
					Tcomshr->smflow[m-1] = f;
				else
					update_lane_smflow(m, f, c);
			}
		}


		Tcomshr->yvalue[linkNo-1] += (Tcomshr->satinc[m-1] * Tcomshr->smflow[m-1]);
		Tcomshr->lsmflo[linkNo-1] += Tcomshr->smflow[m-1];

		/* save cumulative counts for appropriate dets on lane */
		for (i = 1; i <= 9; i++) 
		{
			d = Tcomshr->det[m-1][i-1];
			/* IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: Ignore -ve IN dets */
			if (d <= 0)
				continue;
					
			if (i == 7 || i == 8)
				continue;
			else if (i == 4)
				Tcomshr->inscdc[m-1] = Tcomshr->cdc[d-1];
			else if (i == 6)
				Tcomshr->xskcdc[m-1] = Tcomshr->cdc[d-1];
			else
				Tcomshr->precdc[d-1] = Tcomshr->cdc[d-1];
		}

		for (i = 1; i <= 2; i++)
		{
			d = Tcomshr->assocd[m-1][i-1];
			if (d == 0)
				continue;

			Tcomshr->asscdc[m-1][i-1] = Tcomshr->cdc[d-1];
		}
	} /* end for(k = 0 to llatot) */

	/* convert sum(3600*Y) for lanes to Y% per link */

	/* Causing divide by zero with BIGSIM. j meant to be no. of lanes! */
	DivideError((int)(36*j), 210);
	Tcomshr->yvalue[linkNo-1] = Tcomshr->yvalue[linkNo-1]/(36*j); /* DIV */
	if (Tcomshr->yvalue[linkNo-1] > 90)
		Tcomshr->yvalue[linkNo-1] = 90;
	if (Tcomshr->yvalue[linkNo-1] < 1)
		Tcomshr->yvalue[linkNo-1] = 1;              /* MOD: ARH 05/09/95 */

	/* convert spare green sum to mean freeflow green (0.1sec) */

	DivideError((int)(j), 211);
	/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
	DivideError((int)(100-Tcomshr->yvalue[linkNo-1]), 212,
		"link number was %d", linkNo);

	Tcomshr->extrag[linkNo-1] = Tcomshr->extrag[linkNo-1]/j*10/(100-Tcomshr->yvalue[linkNo-1])*10;  /* DIV !*/

	if(Tcomshr->extrag[linkNo-1] > Tcomshr->lingre[linkNo-1]) 
		Tcomshr->extrag[linkNo-1] = Tcomshr->lingre[linkNo-1];
}



static void update_ped_ep_link_counts(int linkNo)
{
	int detNo;
	int i;

	detNo = Tcomshr->ltype[linkNo-1];
	if (detNo > 0)
	{
		/* Then is ped or e/p link which may have LTYPE = D or D+100 or D+200 */
		/* (d adjustments added for PB681 issue 14 - MOVA-85) */
		detNo = detNo % 100;
		Tcomshr->precdc[detNo-1] = Tcomshr->oldcdc[detNo-1];
		return;
	}

#if defined (CMOVA_EP) /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */
	for (i = 1; i <= 2; i++)
	{
		detNo = Tcomshr->epdet[linkNo-1][i-1];
		if (detNo > 0) 
			Tcomshr->precdc[detNo-1] = Tcomshr->oldcdc[detNo-1];
	}

	detNo = Tcomshr->candet[linkNo-1] % 100;
	if (detNo > 0)
		Tcomshr->precdc[detNo-1] = Tcomshr->oldcdc[detNo-1];

#endif /* CMOVA_EP */ /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */
}


static int get_counts_from_dets(int laneNo, const Ccomshr *const Lcomshr)
{
	int b,c,d,e;
	int i;

	d = Lcomshr->det[laneNo-1][1];
	if (Lcomshr->susdet[d-1] == 0)
	{
		/* X det ok so use it */
		c = Lcomshr->cdc[d-1] - Lcomshr->precdc[d-1];
		if (c < 0) 
			c = (int) ((long)c+32767L); /* TRL/MC mod: cast to int */
		return c;
	}

	/* Try ALT-DWN det */
	d = Lcomshr->det[laneNo-1][7];
	if (d > 0)
	{
		if (Lcomshr->susdet[d-1] == 0)
		{
			c = Lcomshr->cdc[d-1] - Lcomshr->precdc[d-1];
			if (c < 0)
				c = (int) ((long)c+32767L); /* TRL/MC mod: cast to int */
			return c;
		}
	}

	/* Try ALT-UP det */
    d = Lcomshr->det[laneNo-1][6];
	if (d == 0)
		return -1;
	if (Lcomshr->susdet[d-1] > 0)
		return -1;

	c = Lcomshr->cdc[d-1] - Lcomshr->precdc[d-1];
	if (c < 0)
		c = (int) ((long)c+32767L); /* TRL/MC mod: cast to int */
			
	/* Check for an INSINK det.  If present and OK, adjust count */
	e = Lcomshr->det[laneNo-1][3];
	if (e > 0)
	{
		if (Lcomshr->susdet[e-1] > 0)
			return -1;

		/* INSINK is ok so subtract count from IN count */
		b = Lcomshr->cdc[e-1] - Lcomshr->inscdc[laneNo-1];
		if (b < 0)
			b = (int) ((long)b+32767L); /* TRL/MC mod: cast to int */
		c = c - b;
	}

	/* check ASSOCD dets */
	for (i = 1; i <= 2; i++)
	{
		e = Lcomshr->assocd[laneNo-1][i-1];
		if (e == 0)
			continue;

		if (Lcomshr->susdet[e-1] > 0)
			return -1;
		else
		{
            b = Lcomshr->cdc[e-1] - Lcomshr->asscdc[laneNo-1][i-1];
            if (b < 0)
				b = (int) ((long)b+32767L); /* TRL/MC mod: cast to int */
            c = c - b;
		}
	}
	return c;
}


static void update_lane_smflow(int laneNo, int flow, int cnt)
{
	int a;

	/* restrict flow to be between 0.5 to 1.5 times previous SMFLOW,
		unless flow is small */
	a = (Tcomshr->smflow[laneNo-1] + 1) / 2;
	if (flow < a && a > 3)
		flow = a;
	
	a += Tcomshr->smflow[laneNo-1];
	if (a < 7)
		a = 7;
	if (flow > a)
		flow = a;

	Tcomshr->smflow[laneNo-1] = (2*Tcomshr->smflow[laneNo-1]+flow+2)/3;
	Tcomshr->totflo[laneNo-1] += cnt;
}



static int get_ave_flow(int laneNo, const Ccomshr *const Lcomshr)
{
	int iPd;
	int iFlow;
	long nDay;
	TIMESTAMP_TYPE tTimeDate;

	tTimeDate = Com_read_rtc(READ_RTC);
	nDay = day_from_date(tTimeDate);

	iPd = (int)(tTimeDate.hours*2L + (tTimeDate.minutes +1L)/30L);
	iFlow = Lcomshr->aveflo[nDay-1][iPd-1][laneNo-1];

	if (iFlow == 0)
		iFlow = 15;

	return iFlow;
}

