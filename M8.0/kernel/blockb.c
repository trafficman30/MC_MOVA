/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         blockb.c
*
*  TITLE:        Mova Kernel: Setting up of mova data
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	31-Oct-1988		2.0.0		..		CL		......			Modification to include reversionary demands			
*	02-Nov-1988		2.0.0		..		CL		......			if MAX has over-ridden MIN + 3 sec.
*	01-Nov-1988		2.0.0		..		CL		......			Mod to make long-off check use AVEFLO not SMFLOW
*	23-May-1989		2.5.0		..		CL		......			additional CALL CANCEL facility 
*	06-Aug-1989		2.6.0		..		CL		......			mods to CALL CANCEL facility 
*	09-Sep-1992		2.10.0		..		RV		......			new SDCODE 3 & modified 3X, unlatched ped dem 
*	16-Mar-1993		4.0.0		..		RV		......			emergency/priority vehicle facilities 
*	05-Dec-1995		4.0.0		..		RV		......			including stage-sequencing & PUFFIN logic
*	09-Dec-1995		4.0.0		..		RV		......			SDCODEs 4TT; PFACIL=4; DATE >1999
*	26-Jan-1998		4.0.0		..		MC		SIE_00			First undergoing system test
*	29-Apr-1998		4.0.0		..		PC		SIE_01			'const' added to initialised static's (Siemens MOVA-13)
*	03-Mar-1998		4.0.0		..		PC		SIE_02			Overlay on 'Xcomshr', etc., replaced by explicit use of structures.
*	12-May-1998		4.0.0		..		PC		SIE_03			Added call to day_from_date()(Siemens fault report MOVA-27)
*	17-Jan-2000		4.0.0		..		PC		SIE_04			day_from_date() parameters changed. 
*	22-Feb-2002		4.0.0		..		PC		SIE_05			MOVA-86: TRL changes for 'automatic selection of double-green stage sequencing'
*	29-Sep-2004		5.0.0		..		IH		SIE_06			No longer using a fixed wait light channel - this is now provided in the Tcomshr->waitch[]
*																Use of CMOVA_EP #define to enable/disable emergency/priority code 	
*	20-Dec-2004		5.0.1		..		IH		SIE_07			Fix for 'negative' intergreen times (CHANGE<-1) and PFACIL = 4 correction corrected. [M5.0.1]
*	11-May-2006		5.0.3		..		IH		SIE_08			Bugfix M5_CRN0010. Emergency\Priority stage maximum now only checked after normal stage maximum has expired.
*	25-Jan-2005		5.1.0		..		IH		SIE_09			Incorporated Pedestrian priorities. 
*	15-Feb-2005		5.1.0		..		RD		SIE_10			Implemented PEDMAX array in Tcomshr and Mova Setup
*	08-Aug-2007		6.0.0		..		RD		TRL_11			Ped max bug fixes
*	31-May-2011		7.0.0		AA		PK		CRN003			Several Changes for MOVA 7 release
*	31-Aug-2011		7.0.1		..		PK		CRN004			SatFlow and CANDET improvements
*	12-Dec-2011		7.0.1		AB		AK		CRN005			Further CANDET improvements
*	22-Feb-2012		7.0.3		AC		AK		CRN008			Remove unused variables
*	08-Apr-2013		7.0.6		..		AK		CRN026			Fix Em/Pr stage called twice when detector active during I/G
*	22-May-2013		7.0.6		AD		AK		M7.0.6			Issue M7.0.6
*
*  (c) Copyright TRL Ltd 2013. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/
#include <nucleus.h>
#include <stdio.h>
#include <stdlib.h> /* IRH MOD: 22/02/05: PCMOVA, embedded too? For abs() */
#include <proj_def.h>
#include <nu_omu.h>
#include "gbltypes.h"
#include <error.h>    /* TRL/SIE: for SOFT_ERROR calls */
#include "errhand.h"  /* TRL/SIE: for MOVA error handler */
#include "obclock.h"
#include <diff_bss.h>
#include "generalfunc.h"
#include "tma_alerts.h"

/* times (in 0.1 sec) after which link demands inserted */
#define QUICK_NUDGE_PERIOD 300
#define STANDARD_NUDGE_PERIOD 900

extern TIMESTAMP_TYPE Com_read_rtc(int, ... );
/*  IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: Function to check demands for 
    following stages to assess which links are 'relevant' when determining 
    green for the current stage. Defined in Block C. */
extern int CheckGreenInFollowingStages(	int nLinkNum, int nNextStageNum, Ccomshr const * Tcomshr );

int lPedDemand;	
int nPedMax;

#if defined (CMOVA_EP) /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */
int cancelLinkEP[mxlink] = { 0 };
int epdemInIG[mxlink] = { 0 };
#endif

static int get_traffic_link_demand(int linkNo);
static int get_special_demand(int linkNo);
static void set_pedestrian_link_demand(int linkNo);
static void set_ep_link_demand(int linkNo);
static void set_stage_demands();
static int Set_Ped_Max(int nOsat_max, int nUsat_max);
static char Demand_Second_Green(int iExtra_ig, int nStage, int nSideStgX, int nSideStgY);
static void set_conditional_demand(int stageNo, int linkNo);
static void set_latched_demand(int stageNo, int linkNo);
static void clear_ep_link_demand(int linkNo);
static int is_epdet_active(int linkNo);
static int process_sdcode(int stageNo, int linkNo);
static void process_6mrr_sdcode(int stageNo, int linkNo);
static int get_emergency_stage_next();
static int get_priority_stage_next();
static int get_stage_next();


void blockb(void)
{
	int linkNo;
	static int d,c,aa,f,a,b,n,k,g;

	lPedDemand = FALSE;

	for (linkNo = 1; linkNo <= Tcomshr->nlinks; linkNo++) 
	{
		if (Tcomshr->ltype[linkNo-1] == 0)
		{
			/* Normal traffic link */
			Tcomshr->lindem[linkNo-1] = get_traffic_link_demand(linkNo);
			Tcomshr->lindem[linkNo-1] = get_special_demand(linkNo);
		}
		else if (Tcomshr->ltype[linkNo-1] < 0)
		{
			/* Emergency link type = -4,  Priority link type = -5 */
			set_ep_link_demand(linkNo);
		}
		else
		{
			/* Pedestrian link */
			set_pedestrian_link_demand(linkNo);
		}
	}

	set_stage_demands();



/*     SUBSECTION B4 : SET POTENTIAL NEXT STAGE, KNOWING STAGE DEMANDS
     *****************************************************************
 --- First deal with any EMERGENCY demands
     *****************************************************************
*/

	k = get_emergency_stage_next();

	if (k > 0)
	{
		Tcomshr->esnext = (signed char)k;
		Tcomshr->psnext = 0;
	}
	else
	{
		/* No emergency demand, check priority demand next */
		Tcomshr->esnext = 0;
		k = get_priority_stage_next();

		if (k > 0)
		{
			Tcomshr->psnext = (signed char)k;
		}
		else
		{
			/* No priority demand either, get normal demand */
			Tcomshr->psnext = 0;
			k = get_stage_next();

			/* if no demands for any stage just return (MARKER = 2,3 as on entry) */
			if (k == 0)
				return;
		}
	}

	Tcomshr->presnx = Tcomshr->snext;
	Tcomshr->snext = k;

/*
 //////////////////////////////////////////////////////////////////////
     SUBSECTION B5 : INCREMENT MAX-GREEN TIMER IF SOME DEMAND
              CHECK IF MIN-GREENS OR EMERGENCY/PRIORITY HOLDS
              EXPIRED FOR RELEVANT LINKS. SET REVERSIONARY DEMANDS;
              CHECK STAGE MAX-GREEN AND DRIFT-MAX, IF MINS EXPIRED.
 //////////////////////////////////////////////////////////////////////
 --- NOTE : continue here only if SNEXT set to new stage.
*/
	/* Increment stage timer for comparison with max-green */
	Tcomshr->stamax += Tcomshr->scan;

	/* Return with marker=2 if in stage abs-min-green */
	if (Tcomshr->stamin == 0)
		return; /* >>>>> leave marker=2 >>>>> */

	/* Check all link min-greens and any e/p link holds */
	aa = 0;
#if defined (CMOVA_EP)
	Tcomshr->ephold = 0;
#endif
/*
 --- Use AA>0 to note if any vari-mins or priority hold not ended,
 --- so reversionary dem set. EPHOLD>0 notes emerg/pr hold not expired.
*/
	a = abs(Tcomshr->stensa[Tcomshr->snext-1]);
	if (a == 1)
	{
		/* Already OK for SNEXT (STENSA=-1 (set in B5) or =1 (set in C5) */
		Tcomshr->marker = 4;
	}
	else
	{
		for (linkNo = 1; linkNo <= Tcomshr->nlinks; linkNo++)
		{
			/* skip links not green now */
			if (Tcomshr->lgreen[Tcomshr->cusig-1][linkNo-1] == 0)
				continue;

			a = Tcomshr->min[Tcomshr->snext-1] + 5;

			/*  IRH MOD 27/01/04: TRL/SIE Compact MOVA: Check 
				whether link has extendable green again *after* the fixed time 
				stage given current stage demands */
			if (Tcomshr->lgreen[Tcomshr->snext-1][linkNo-1] >= 2 &&
				(a < Tcomshr->max[Tcomshr->snext-1] ||
				CheckGreenInFollowingStages(linkNo-1, Tcomshr->snext-1, Tcomshr) == 0))
			{
				/* skip link if it gets another extendable green next stage */
				continue;
			}

			a = Tcomshr->lingre[linkNo-1];
			if (Tcomshr->lgreen[Tcomshr->snext-1][linkNo-1] != 0)
			{
				/* link green now and also in the stage following.
				   Add in the min green length of SNEXT + the IG length */
				a = Tcomshr->lingre[linkNo-1] + Tcomshr->min[Tcomshr->snext-1] 
					+ abs(Tcomshr->change[Tcomshr->cusig-1][Tcomshr->snext-1]);
			}

			/* If link absolute min has not been reached, return,
			   leaving STENSA=0 and MARKER=3 */
			if (a < Tcomshr->lmin[linkNo-1])
				return;

			/* Otherwise, check for e/p demands and vari-mins */

#if defined (CMOVA_EP) /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */

			b = 0;
			d = abs((int)Tcomshr->revdem[linkNo-1]);

			if (Tcomshr->lindem[linkNo-1] == 4 || d == 4)
			{
				/* There is emergency demand for the link */
				/* Check if it is cancelled or hold time has expired */
				if (a < Tcomshr->holdtm[linkNo-1] && Tcomshr->cancel[linkNo-1] == 0)
				{
					b = 4;
					Tcomshr->ephold = 4;
				}
			}
		
			if (Tcomshr->lindem[linkNo-1] == 5 || d == 5)
			{
				/* There is priority demand for the link */
				/* Check if it is cancelled or hold time has expired */
				if (a < Tcomshr->holdtm[linkNo-1] && Tcomshr->cancel[linkNo-1] == 0)
				{
					b = 5;
					Tcomshr->ephold = 5;
				}
			}

			/* if no e/p reversionary demand set, check if vari-min expired */
			if (b == 0 && a < Tcomshr->mingre[linkNo-1])
				b = 1;

			/* clear / reset reversionary demand */
			Tcomshr->revdem[linkNo-1] = (signed char)b;

			/* Setting of any reversionary demand is noted with AA=1 */
			if (b > 0)
				aa = 1;

#endif /* CMOVA_EP */ /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */

		}

		/* At this point all absolute min times must have been reached */

		if (aa > 0)
		{
			/* An e/p hold or vari-min has not ended */
			/* Note this with STENSA=-2 (MARKER=3 still) */
			Tcomshr->stensa[Tcomshr->snext-1] = -2;
		}
		else
		{
			Tcomshr->stensa[Tcomshr->snext-1] = -1;
			Tcomshr->marker = 4;
		}
	}

	/* Finally, check max-greens */

	/* Set max tolerance = 0 for later possible resetting, and note marker */
	Tcomshr->mxplus = 0;
#if defined (CMOVA_EP)
    Tcomshr->tempmk = (char)Tcomshr->marker;
#endif

	if (Tcomshr->cutmax == 0)
	{
		b = Tcomshr->max[Tcomshr->cusig-1];
		d = Tcomshr->drifmx[Tcomshr->cusig-1];
		g = Tcomshr->totalg;
	}
	else
	{
		b = Tcomshr->lowmax[Tcomshr->cusig-1];
		d = Tcomshr->lodfmx[Tcomshr->cusig-1];
		g = Tcomshr->lototg;	
	}

	if (Tcomshr->rav1 != 0 && Tcomshr->pedmax[1][Tcomshr->cusig-1] != 999) 
	{
		b = Set_Ped_Max(Tcomshr->pedmax[1][Tcomshr->cusig-1], 
			Tcomshr->pedmax[0][Tcomshr->cusig-1]);
		d = b;
		nPedMax = b;
	}

	if (Tcomshr->stamax >= b)
	{
		/* max green reached */
#if defined (CMOVA_EP)
		Tcomshr->tempmk = 7;
#else
		Tcomshr->marker = 7;
#endif
	}
	else
	{
		/* Check drift-max */
		a = Tcomshr->min[Tcomshr->cusig-1] + 30;
		if (a > Tcomshr->sistim && Tcomshr->marker < 4)
		{
			/* don't allow drift-max to cut stage if within 3 sec of absolute stage min */
		}
		else if (Tcomshr->stamax < d)
		{
			/* drift-max > stage max */
		}
		else
		{
			/* store total merit factor in 'AA', that for CUSIG in 'A' */
			aa = 0;
			for (n = 1; n <= Tcomshr->stages; n++)
			{
				f = Tcomshr->meritv[n-1] + 25*Tcomshr->stagos[n-1];
				if (n == Tcomshr->cusig)
					a = f;
				aa += f;
			}

			/* leave MXPLUS=0 if merit value for CUSIG < 5 */
			c = a/5;
			if (c != 0)
			{
				DivideError(c, 301);
    
				DivideError((int)(4*aa/c), 302,
					"aa was %d, c was %d", aa, c);

				/* Extra max (tolerance) is MXPLUS (0.1 sec) */
				/* 5/4//5=5/20 provides for 25 per cent tolerance */
				Tcomshr->mxplus = 5*g/(4*aa/c);
			}

			b = d + Tcomshr->mxplus;
			if (Tcomshr->stamax >= b)
			{
				/* flag max reached */
#if defined (CMOVA_EP)
				Tcomshr->tempmk = 7;
#else
				Tcomshr->marker = 7;
#endif
			}
		}
	}


#if defined (CMOVA_EP) /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */
/*
 /////////////////////////////////////////////////////////////////////
     SUBSECTION B6 : UPDATE EMERGENCY/PRIORITY STAGE EXTENSIONS;
                 INCREMENT PRIORITY MAX TIMER IF NEC
                 DECIDE EMERGENCY/PRIORITY ACTION
                 SET FLAGS FOR TRUNCATED LINKS
 /////////////////////////////////////////////////////////////////////
 --- Reset emergency/priority stage-ext from largest of link-ext
 --- timers on links now green and going red/opposed-green next stage
 --- Set reversionary demands in case emergency action/max's cut ext's
     ******************************************************************
*/
	Tcomshr->esxtim = 0;
	Tcomshr->psxtim = 0;
	for (linkNo = 1; linkNo <= Tcomshr->nlinks; linkNo++)
	{
		/* Skip non-emergency/priority links, or links now red */
		if (Tcomshr->ltype[linkNo-1] >= 0 || Tcomshr->lgrefl[linkNo-1] == 0)
			continue;

		/* Skip emerg/priority links now green & staying green next stage */
		if (Tcomshr->lgreen[Tcomshr->snext-1][linkNo-1] != 0 && 
			Tcomshr->lgreen[Tcomshr->snext-1][linkNo-1] != 4)
			continue;

		/* Clear temp revers demand set by previous emerg/priority ext */
		if ((int)Tcomshr->revdem[linkNo-1] < 0)
			Tcomshr->revdem[linkNo-1] = 0;

		/* Now check if need to reset revdem */

		/* Skip links with no emergency/priority extension */
		if (Tcomshr->eplxtm[linkNo-1] == 0)
			continue;

		/* Set e/p reversionary demand, unless DO 810 loop set=1,4,5 */
		if ((int)Tcomshr->revdem[linkNo-1] == 0)
			Tcomshr->revdem[linkNo-1] = (signed char)(Tcomshr->ltype[linkNo-1]);

		if (Tcomshr->ltype[linkNo-1] == -5)
		{
			/* Reset priority-stage ext'n timer if rel. link ext is longer */
			if (Tcomshr->eplxtm[linkNo-1] > Tcomshr->psxtim)
				Tcomshr->psxtim = Tcomshr->eplxtm[linkNo-1];
		}
		else
		{
			/* Reset emergency stage ext'n timer if link ext is longer */
			if (Tcomshr->eplxtm[linkNo-1] > Tcomshr->esxtim)
				Tcomshr->esxtim = Tcomshr->eplxtm[linkNo-1];
		}
	}

	/* increment e/p stage max timer only if other max not yet reached */
    if (Tcomshr->tempmk == 7)
	{
		if (Tcomshr->esxtim > 0 || Tcomshr->psxtim > 0)
			Tcomshr->epxmxt += 5;
	}

/*
     *****************************************************************
 --- Check for emergency/priority-veh control action
     *****************************************************************
 --- Deal first with any emergency action
 --- Emergency action overrides priority extensions or demands.
*/
	/* Clear emergency/priority extension marker */
    Tcomshr->epx = 0;

	/* if emergency hold exists, return, leaving MARKER=3 or 4 */
	if (Tcomshr->ephold == 4)
		return;

	if (Tcomshr->esxtim != 0)
	{
		if (Tcomshr->tempmk != 7 ||
			Tcomshr->epxmxt < Tcomshr->emxmax[Tcomshr->cusig-1])
		{
			/* if emergency extn exists and has not reached max, can't override */
			Tcomshr->epx = 4;
			return;   /* >>>> leave marker=3,4 >>>>>*/
		}
	}

	if (Tcomshr->esnext <= 0)
	{
		/* No emergency change needed.  Check priority holds next */
		if (Tcomshr->ephold == 5)
		{
			/* Return if priority hold prevents any non-emergency change */
			return; /* >>>>> leave marker=3 >>>>>>*/
		}

		if (Tcomshr->psxtim != 0)
		{
			if (Tcomshr->tempmk != 7 ||
				Tcomshr->epxmxt < Tcomshr->prxmax[Tcomshr->cusig-1])
			{
				/* priority extn exists and has not reached max */
				Tcomshr->epx = 5;
				return;  /* >>>>> leave marker=3,4 >>>>>*/
			}
		}

		if (Tcomshr->psnext <= 0 || Tcomshr->trunc <= 0)
		{
			/* reset marker if no priority change or truncation needed */
			Tcomshr->marker = Tcomshr->tempmk;
			return;  /* >>>>> marker=3,4,7 >>>>> */
		}
	}

/*
     ******************************************************************
 --- Set truncation indicators on affected links.
     ******************************************************************
 --- Enter here only if emergency or priority change truncates CUSIG
*/
	for (linkNo = 1; linkNo <= Tcomshr->nlinks; linkNo++)
	{
		/* Skip irrelevant non-traffic links, or those not green in CUSIG */
		if (Tcomshr->ltype[linkNo-1] != 0 || Tcomshr->lgrefl[linkNo-1] == 0)
			continue;

		/* set priority truncation flag = STAGES*2 on links still sat */
		/* For links already flagged end-sat, trunc'n is not a problem */
		if (Tcomshr->liensa[linkNo-1] == 0)
			Tcomshr->eptrun[linkNo-1] = (char)(Tcomshr->stages + Tcomshr->stages);
    }

	/* Mark emergency/priority change required, and return to implement */
    Tcomshr->marker = 9;

#endif /* CMOVA_EP */ /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */

	return;  /* >>>>> marker = 9 >>>>>>> */
}



/*****************************************************************************
 * set_pedestrian_link_demand()
 * Section B1, for pedestrian links
 *****************************************************************************/
static void set_pedestrian_link_demand(int linkNo)
{
    static const int pcode[8] = {0,1,6,3,15,21,10,1};
    static const int bincode[3] = {4,2,1};
    int detNo;
    int nPedWaitChannel;
    int nPedWaitSignal;

    /* Initialise pedestrian queue to zero */
    Tcomshr->totbon[linkNo-1] = 0;
    
    if (Tcomshr->ltype[linkNo-1] < 100)
    {
		/* Traditional latched pedestrian detector (LTYPE=detNo) */
		/* Set latched pedestrian demand */
		int c;

		detNo = Tcomshr->ltype[linkNo-1];
		c = Tcomshr->cdc[detNo-1]-Tcomshr->precdc[detNo-1];
		if (c != 0 || Tcomshr->ondet[detNo-1] > 0)
		{
			/* set demand & set Q to nominal 1 pedestrian */
			/* Ped Q scaled by 10. O/P via equiv to TOTBON is /10 */
			Tcomshr->lindem[linkNo-1] = 1;
			Tcomshr->totbon[linkNo-1] = 10;
		}
    }
    else 
    {
		/* Else LTYPE > 100 (Puffin-type ped link) */

		/* IRH MOD: M5.0.0: 01/09/04: No longer using a fixed wait light 
		* channel - may be different wait lights for 
		* different pedestrian detectors. Use a signal from
		* a user-specified wait light channel (Tcomshr->waitch)
		* that can vary for different pedestrian links. */
		nPedWaitChannel = Tcomshr->waitch[linkNo-1];
          
		/* If the user has specified a wait light channel */
		if (nPedWaitChannel > 0)
		{
			/* Get whether it's on or off */
			nPedWaitSignal = Tcomshr->ondet[nPedWaitChannel-1];
		}
		else
		{
			/* Else no wait channel is specified */
			/* Assume wait light is on (signal may already
			* have been combined (ANDed) with the 
			* pedestrian detector (d) in the controller) */
			nPedWaitSignal = 1;
		}

		/* If simple unlatched traditional ped */
		if (Tcomshr->ltype[linkNo-1] < 200) 
		{
			/* Simple unlatched traditional ped (LTYPE=detNo+100) */
			/* Clear unlatched pedestrian demand and reset as necessary */

			Tcomshr->lindem[linkNo-1] = 0;
			detNo = Tcomshr->ltype[linkNo-1]-100;
			/* Clear suspect detector flag prior to possible setting */
			Tcomshr->susdet[detNo-1] = 0;

			/* Detector channel has been constantly 'on' for > 4-minutes
			so flag suspect detector, purely for information */
			if (Tcomshr->ondet[detNo-1] > 2400)
				Tcomshr->susdet[detNo-1] = 4;

			if (Tcomshr->ondet[detNo-1] > 0 && nPedWaitSignal > 0)
			{
				/* use detector output only if wait light is on */
				/* set demand and set queue to nominal one ped */

				Tcomshr->lindem[linkNo-1] = 2;
				Tcomshr->totbon[linkNo-1] = 10; /*pedq*/

				if (Tcomshr->pedmax[1][Tcomshr->cusig-1] != 999)
				{
					lPedDemand = TRUE;	/* to indicate pedestrian demand for PED features */
				}
			}

		} /* If simple unlatched traditional ped */
		else
		{
			/* Else is Puffin volume-sensitive link (LTYPE=detNo+200) */

			/* Decode multi-channel detector.  Accumulate flow code in 'aa'
			Clear unlatched pedestrian demand and reset as necessary. */

			int aa = 1;
			Tcomshr->lindem[linkNo-1] = 0;
			detNo = Tcomshr->ltype[linkNo-1]-200;

			if (nPedWaitSignal > 0)
			{
				/* Use detector output only if WAIT light is 'on' */
				int i;
				for (i = 0; i < 3; i++)
				{
					/* Clear suspect detector flag prior to possible setting */
					Tcomshr->susdet[detNo-1+i] = 0;

					/* If detector channel has been constantly 'on' for > 4-minutes
					flag it as suspect detector, purely for information */
					if (Tcomshr->ondet[detNo-1+i] > 2400)
						Tcomshr->susdet[detNo-1+i] = 4;

					if (Tcomshr->ondet[detNo-1+i] > 0)
						aa += bincode[i];
				}
			}

			if (aa == 8)
				Tcomshr->susdet[detNo-1] = 7;

			/* Put pedestrian Q into PEDQ equivalenced to TOTBON */
			Tcomshr->totbon[linkNo-1] = 10*pcode[aa-1];

			if (aa > 1)
				Tcomshr->lindem[linkNo-1] = 2;

			if (Tcomshr->pedmax[1][Tcomshr->cusig-1] != 999)
			{
				lPedDemand = TRUE;	/* to indicate pedestrian demand for PED features */
			}
		} /* Else is Puffin volume-sensitive link */
    } /* Else LTYPE > 100 (Puffin-type ped link) */
}


/*****************************************************************************
 * set_ep_link_demand()
 * Section B1, for Emergency/Priority links
 *****************************************************************************/
static void set_ep_link_demand(int linkNo)
{
#if defined (CMOVA_EP)
    int detNo;
	int epdem;

	/* If the link is now green do not modify demand here.
		Hold/extension is cancelled in block P9. */
	if (Tcomshr->lgreen[Tcomshr->cusig-1][linkNo-1] != 0 && 
		Tcomshr->lgreen[Tcomshr->cusig-1][linkNo-1] != 4)
	{
		cancelLinkEP[linkNo-1] = 0;
		epdemInIG[linkNo-1] = 0;
		return;
	}

	if (cancelLinkEP[linkNo-1])
	{
		/* check if there is an e/p det on now */
		epdem = is_epdet_active(linkNo);

		/* check if the candet is still on */
		detNo = Tcomshr->candet[linkNo-1] % 100;
		if (Tcomshr->susdet[detNo-1] == 0 && Tcomshr->ondet[detNo-1] > 0)
		{
			/* candet is on so cancel demand */
			if (Tcomshr->lindem[linkNo-1] > 0)
				clear_ep_link_demand(linkNo);
		}
		else
		{
			/* candet is now off (or faulty) */
			cancelLinkEP[linkNo-1] = 0;

			/* However, the candet may have both gone on and then off again during intergreen.
			   If no current e/p demand, cancel any existing demand. */
			if (epdem == 0 && Tcomshr->lindem[linkNo-1] > 0)
				clear_ep_link_demand(linkNo);
		}
		return;
	}

	if (Tcomshr->lindem[linkNo-1] > 0)
	{
		/* Carry forward existing emerg/priority-vehicle demand */
		epdem = 1;
	}
	else
	{
		/* Otherwise check both e/p detectors on the link */
		epdem = is_epdet_active(linkNo);
	}

	if (epdem > 0)
	{
		/* Set link e/p demand */
		Tcomshr->lindem[linkNo-1] = 4;
		if (Tcomshr->ltype[linkNo-1] == -5)
			Tcomshr->lindem[linkNo-1] = 5;
	}

	return;
#endif
}


/*****************************************************************************
 * get_traffic_link_demand()
 * Section B1, for normal vehicle links
 *****************************************************************************/
static int get_traffic_link_demand(int linkNo)
{
	int laneNo;
	int detNo;
	int i;

	/* If link already demanded no need to check further */
	if (Tcomshr->lindem[linkNo-1] > 0)
		return Tcomshr->lindem[linkNo-1];

	for (i = 1; i <= Tcomshr->llatot[linkNo-1]; i++) 
	{
		laneNo = Tcomshr->llanes[linkNo-1][i-1];

		/* If any red counts on X det, set demand */
		if (Tcomshr->redcx[laneNo-1] > 0)
			return 1;

		/* If any red counts on IN det and no IN-Sink exists, set demand */
		if (Tcomshr->redcin[laneNo-1] > 0 && Tcomshr->det[laneNo-1][3] == 0)
			return 1;

		/* If X detector is currently on (and not faulty) then set demand */
		detNo = Tcomshr->det[laneNo-1][1];
		if (Tcomshr->susdet[detNo-1] == 0 && Tcomshr->ondet[detNo-1] > 0)
			return 1;

		/* Check stopline detector if present */
		detNo = Tcomshr->det[laneNo-1][8];
		if (detNo == 0)
		{
			/* No stopline detector, so check X det more fully */
			detNo = Tcomshr->det[laneNo-1][1];
			if (Tcomshr->susdet[detNo-1] == 0)
			{
				TIMESTAMP_TYPE tTimeDate;
				long dy;
				int PeriodNo;
				int flow;
				int long_off_time;

				tTimeDate = Com_read_rtc(READ_RTC);
				dy = day_from_date(tTimeDate);
				PeriodNo = (int)(tTimeDate.hours*2L + (tTimeDate.minutes +1L)/30L);

				flow = Tcomshr->aveflo[dy-1][PeriodNo-1][laneNo-1];

				/* Skip long-off check if ave. flow <= 150 veh/hr */
				if (flow <= 15)
					continue;

				long_off_time = 32000/flow;
				if (long_off_time < 1200)
					long_off_time = 1200;
				/* drop through to OUT det check if X det off a long time,
					otherwise continue to next lane */
				if (Tcomshr->timoff[detNo-1] < long_off_time)
					continue;
			}

			/* Check OUT det */
			detNo = Tcomshr->det[laneNo-1][2];
			if (detNo == 0)
			{
				if (Tcomshr->redcin[laneNo-1] > 0)
				{
					if (Tcomshr->cycltm[linkNo-1] >= QUICK_NUDGE_PERIOD)
						return 1;
					continue;
				}

				detNo = Tcomshr->det[laneNo-1][0];
				if (detNo > 0 && Tcomshr->susdet[detNo-1] > 0)
				{
					if (Tcomshr->cycltm[linkNo-1] >= QUICK_NUDGE_PERIOD)
						return 1;
					continue;
				}

				if (Tcomshr->cycltm[linkNo-1] >= STANDARD_NUDGE_PERIOD)
					return 1;
			}
			else
			{
				/* if OUT det is on, set demand */
				if (Tcomshr->susdet[detNo-1] > 0 || Tcomshr->ondet[detNo-1] > 0)
					return 1;
			}
		}
		else
		{
			if (Tcomshr->susdet[detNo-1] > 0)
			{
				/* if the stopline det is faulty, insert demand after 30 sec red */
				if (Tcomshr->cycltm[linkNo-1] >= QUICK_NUDGE_PERIOD)
					return 1;
			}
			else
			{
				if (Tcomshr->ondet[detNo-1] > 0)
					return 1;
			}
		}
	}
	/* If no demands set, return unchanged value */
	return Tcomshr->lindem[linkNo-1];
}


/*****************************************************************************
 * get_special_demand()
 * Section B2
 *****************************************************************************/
static int get_special_demand(int linkNo)
{
	int laneNo;
	int detNo;

	int totq;  /* When X and OUT dets ok, stores queue estimate for all lanes */
	int minq;  /* minimum Q for special demand from X-OUT count */
	int i;

	totq = 0;

	/* Skip if no requirement for special demands for this link */
	minq = Tcomshr->mixout[linkNo-1];
	if (minq == 0)
		return Tcomshr->lindem[linkNo-1];

	if (minq < 0)
		minq = -minq;

	for (i = 1; i <= Tcomshr->llatot[linkNo-1]; i++)
	{
		laneNo = Tcomshr->llanes[linkNo-1][i-1];

		/* if lane has no OUT det then special demand not possible */
		detNo = Tcomshr->det[laneNo-1][2];
		if (detNo == 0)
			continue;

		if (Tcomshr->susdet[detNo-1] > 0)
		{
			/* If OUT det suspect, check X det */
			detNo = Tcomshr->det[laneNo-1][1];

			/* If X det is also faulty - assume special demand exists */
			if (Tcomshr->susdet[detNo-1] > 0)
				return 2;

			/* Set special demand if X det red count ok or if on for >= 3 sec */
			if (Tcomshr->redcx[laneNo-1] >= minq || Tcomshr->timon[detNo-1] >= 30)
				return 2;

			totq = totq + Tcomshr->redcx[laneNo-1];
			continue;
		}

		detNo = Tcomshr->det[laneNo-1][1];
		if (Tcomshr->susdet[detNo-1] == 0)
		{
			/* If X det ok, consider setting demand from long-on X det */
			if (Tcomshr->mixout[linkNo-1] < 0 &&
				Tcomshr->lingre[linkNo-1] < Tcomshr->moveqx[laneNo-1])
			{
				/* Skip long-on check until after MOVEQX, if call/cancel specified */
			}
			else
			{
				/* set link special demand if X-det long-on */
				if (Tcomshr->timon[detNo-1] >= Tcomshr->qminon[laneNo-1])
					return 2;
			}

			/* sum queue estimates */
			totq = totq + Tcomshr->redcx[laneNo-1];

			/* if MIXOUT < 0 check call/cancel, otherwise continue */
			if (Tcomshr->mixout[linkNo-1] >= 0)
				continue;
		}

		/* Skip call/cancel check until link min green expired */
		if (Tcomshr->lingre[linkNo-1] < 70)
			continue;

		/* set/maintain call/cancel demand for link */
		/* Using OUT det for c/c if X det suspect or MIXOUT<0 */
		detNo = Tcomshr->det[laneNo-1][2];
		if (Tcomshr->timon[detNo-1] >= 30)
			return 3;
		if (Tcomshr->lindem[linkNo-1] == 3 && Tcomshr->timoff[detNo-1] < 30)
			return 3;
	}

	/* if no vehicles present or link has no OUT det */
	if (totq == 0)
		return Tcomshr->lindem[linkNo-1];

	/* if queue count insufficient for special demand, set normal demand */
	if (totq < minq)
		return 1;

	/* Set special demand type 2 for reasons other than call/cancel */
	return 2;
}


static int Set_Ped_Max(int nOsat_max, int nUsat_max)
{
	int nMax_laosat = 0;
	int nCusig;
	int nLinkNo;
	int nLaneNo;
	int nNewStageMax;
	int nX, nY;

    nCusig = Tcomshr->cusig;
    
    /* loop for all links */
    for ( nLinkNo = 1 ; nLinkNo <= Tcomshr->nlinks ; nLinkNo++ )
    {
        /* find no. lanes on links green in this stage */
        if ( Tcomshr->lgreen[nCusig-1][nLinkNo-1] >= 1 &&  Tcomshr->lgreen[nCusig-1][nLinkNo-1] <= 3)
        {
            nX = Tcomshr->llatot[nLinkNo-1];
        
            /* find max laosat value for lanes on this link */
            for ( nY = 1 ; nY <= nX ; nY++ )
            {
                nLaneNo = Tcomshr->llanes[nLinkNo-1][nY-1];
                nMax_laosat = max(nMax_laosat, Tcomshr->laosat[nLaneNo-1]);
            }
        }
    }
    
    if ( nMax_laosat < 4 )
    {
        /* unsaurated so use usat_max value directly */
        nNewStageMax = nUsat_max;
    } 
    else
    {
        /* lambda in %, smcycl in 0.1s units */
        nNewStageMax =  (Tcomshr->lambda[nCusig-1] * Tcomshr->smcycl * nOsat_max)/10000;
    }
    
    /* now add constraints */
    /* first not less than unsaturated max ped green */
    nNewStageMax = max(nNewStageMax, nUsat_max);
    
    /* next not more than normal max's (lowmax if operating short cycles) */
    if ( Tcomshr->cutmax > 0 )
        nNewStageMax = min(nNewStageMax, Tcomshr->lowmax[nCusig-1]);
    else
        nNewStageMax = min(nNewStageMax, Tcomshr->max[nCusig-1]);

	/* last not less than normal min greens */
    nNewStageMax = max(nNewStageMax, (Tcomshr->min[nCusig-1] + 5)/10);
    
    return nNewStageMax;
}


static char Demand_Second_Green(int iExtra_ig, int nStage, int nSideStgX, int nSideStgY)
{
/*
Function to decide if the 'main road' stage should be run again, between the two side
road stages. 
 Last change:  MRC  18 Jan 102   10:16 am
*/

long iBI_red;                 /* benefit indicator for all links on red */
long iBI_green;               /* benefit indicator for all links on green */
int  iMax_lostim;
long iThis_link_BI;           /* benefit indicator for this link */
char lDemand_Second_Green;    /* tell calling routine if decision is to run second green */
int  nLink;

/*
 --- CODE 4TXY sets conditional demand for double-green stage if
 --- benefit justifies it & demands exist for stages between repeats
 --- but only if there are competing demands for BOTH stages X & Y.
*/
        lDemand_Second_Green = FALSE;
        if(Tcomshr->stadem[nSideStgX-1] != 0 && Tcomshr->stadem[nSideStgY-1] != 0)
        {
/*
 --- 4T part (iExtra_ig here) of a 4TXY code sets an unlatched demand if the
 --- repeat stage is justified by the net benefit of calling stage N.
 --- iExtra_ig is extra IG in 2s units from repeating stage N.
*/
           iExtra_ig -= 40;
           if(iExtra_ig == 0) iExtra_ig = 10;
           iExtra_ig += iExtra_ig;
/*
 --- iExtra_ig is now in seconds
*/
           iBI_red = 0L;
           iBI_green = 0L;
           iMax_lostim = 0;
/*
 --- Save largest link lost-time for use later.
*/
           for(nLink=0; nLink < Tcomshr->nlinks; nLink++)
           {
              if(Tcomshr->lostim[nLink] > iMax_lostim) iMax_lostim = Tcomshr->lostim[nLink];

              iThis_link_BI = Tcomshr->lsmflo[nLink]*(100-Tcomshr->yvalue[nLink]);

              if(Tcomshr->lgreen[nStage-1][nLink] != 0 && Tcomshr->lgreen[nStage-1][nLink] != 4)
              {
/* --- THEN, link has unopposed green in stage N; LGREEN = 1,2,or 3  */
                 iBI_green += iThis_link_BI;
              }
              else
              {
/* --- ELSE, link is effectively red in stage N;  LGREEN = 0 or 4 */
                 iBI_red += iThis_link_BI;
              }
           }
        
/* --- Now compute net benefit. First convert lost-time in A to seconds. */
           iMax_lostim /= 10;
           lDemand_Second_Green = 
              ( iBI_green * (long)(iMax_lostim - iExtra_ig) ) >= ( iBI_red * (long)(iExtra_ig + iExtra_ig) );
/* --- If positive net benefit, jump to set unlatched demand; else, end */

     }
     return lDemand_Second_Green;
}





/*****************************************************************************
 * set_stage_demands()
 * Section B3
 *****************************************************************************/
static void set_stage_demands()
{
	int stageNo;
	int linkNo;
	int rcode;

	for (stageNo = 1; stageNo <= Tcomshr->stages; stageNo++)
	{
		/* Skip any stage that is part of a 4-stage sequence
		   already rejected by 6MRR-type SDOCDE */
		if (Tcomshr->stadem[stageNo-1] == -6)
			continue;

		/* Stages may have normal latched demands:
		   STADEM=1 for SDCODE 1 or 3X, or
		   STADEM=6 for SDCODE 6MRR that has been selected. */

		/* Clear any conditional demands (STADEM=2) or
		   conditional em/pr demands (PSTDEM=8, ESTDEM=9) */
		if (Tcomshr->stadem[stageNo-1] == 2)
			Tcomshr->stadem[stageNo-1] = 0;

#if defined (CMOVA_EP) /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */    
		if (Tcomshr->estdem[stageNo-1] != 4)
			Tcomshr->estdem[stageNo-1] = 0;
		if (Tcomshr->pstdem[stageNo-1] != 5)
			Tcomshr->pstdem[stageNo-1] = 0;
#endif /* CMOVA_EP */ /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */

		/* Continue to check for new demands, even if already have
		   latched demand, because em/pr demands may occur later */

		for (linkNo = 1; linkNo <= Tcomshr->nlinks; linkNo++)
		{
			/* Check for reversionary demand when link is red.
			   Note demand can be <0 for e/p links */

			if ((int)Tcomshr->revdem[linkNo-1] == 0 || Tcomshr->lgrefl[linkNo-1] != 0)
			{
				/* skip link if no demand */
				if (Tcomshr->lindem[linkNo-1] == 0)
					continue;
			}

			rcode = process_sdcode(stageNo, linkNo);
			if (rcode == 4)
			{
				/* 6MRR return - skip next 3 stages as already done */
				stageNo += 3;
				break;
			}
		}
	}
}


/* return value is the number of stages processed */
static int process_sdcode(int stageNo, int linkNo)
{
	/* IRH MOD: M5.0.0: 02/09/04: Delayed demand time for emergency/priority links 
	 * with 4TT and 5TT SDCodes. Defined in Block A. */
	extern int iDemandDelay_[mxlink];

	int k;
	int a,aa,b,d,dd,dy,e,h,i,mm,x,y,t,tt,yy;

	k = Tcomshr->sdcode[stageNo-1][linkNo-1];
		
	/* skip link if it doesn't demand the stage we are considering */
	if (k == 0)
		return 1;

	/* if we are considering the current stage, only consider 6MRR demands */
	if (stageNo == Tcomshr->cusig && k < 6000)
		return 1;

	if (k < 10)
	{
		/* SDCODES 1,2,3,4 */
		switch (k)
		{
			case 1:
				/* For code 1 set latched demand */
				set_latched_demand(stageNo, linkNo);
				return 1;

			case 2:
				/* For code 2 cater for left turn filter demand.
					Set demand for filter stage during previous stage only */
				e = stageNo-1;
				if (e == 0)
					e = Tcomshr->stages;
				if (Tcomshr->cusig == e)
					set_latched_demand(stageNo, linkNo);
				return 1;

			case 3:
				/* For code 3 set simple non-latched demand */
				set_conditional_demand(stageNo, linkNo);
				return 1;

			case 4:
				/* For code 4 set non-latched demand only if red */
				if (Tcomshr->lgrefl[linkNo-1] == 0)
					set_conditional_demand(stageNo, linkNo);
				return 1;

			default:
				/* Shouldn't ever happen! */
				return 0;
		}
	}
	else if (k < 100)
	{
		/* SDCODES 1X,2X,3X,4T,5T,6M */

		/* use 'a' as 10's and 'x' as units */
		a = k/10;
		x = k%10;
		/* x=0 => x is stage 10 */
		if (x == 0)
			x = 10;

		switch (a)
		{
			case 1:
				/* For 1X code, set non-latched demand if stage X not yet demanded */
				if (Tcomshr->stadem[x-1] == 0)
					set_conditional_demand(stageNo, linkNo);
				return 1;

			case 2:
				/* For 2X code, set non-latched demand if stage X not yet demanded
					or if special demand exists for the link */
				if (Tcomshr->stadem[x-1] == 0 ||
					Tcomshr->lindem[linkNo-1] == 2 || Tcomshr->lindem[linkNo-1] == 3)
					set_conditional_demand(stageNo, linkNo);
				return 1;
			
			case 3:
				/* For 3X code, set latched demand if stage X also demanded
					provided stage X not currently running nor link L now green */
				if (Tcomshr->lgrefl[linkNo-1] > 0)
					return 1;

				if (Tcomshr->stadem[x-1] > 0 && Tcomshr->cusig != x)
					set_latched_demand(stageNo, linkNo);

				/* if latched demand already set then reset in order to check e/p demands */
				if (Tcomshr->stadem[stageNo-1] == 1)
					set_latched_demand(stageNo, linkNo);

				return 1;

			case 4:
				/* For 4T code (or 4X here), set latched demand if repeat stage required */

				/* Calculate net benefit of repeating stage by calling this stage N.
					T is extra IG from adding stage N to sequence.(T=10*X  0.1s units) */
				a = 0;
				t = 10*x;
				mm = 0;
				yy = 0;

				for (e = 1; e <= Tcomshr->nlinks; e++)
				{
					/* Save largest link lost-time in A, for use later */
					if (Tcomshr->lostim[e-1] > a)
						a = Tcomshr->lostim[e-1];

					dd = Tcomshr->lsmflo[e-1] * (100-Tcomshr->yvalue[e-1]);
					if (Tcomshr->lgreen[stageNo-1][e-1] != 0 && Tcomshr->lgreen[stageNo-1][e-1] != 4)
					{
						/* link has unopposed green in stage N */
						mm += dd;
					}
					else
					{
						/* link is effectively red in stage N */
						yy += dd;
					}
				}

				dd = (long)(a-t)*mm;
				dy = (long)(t+t)*yy;
				dd -= dy;

				/* if positive net benefit set latched demand */
				if (dd >= 0)
					set_latched_demand(stageNo, linkNo);
				return 1;
			
			case 5:
				return 1;
			
			case 6:
				/* For 6M code (6X here), transfer demand from link L to link M */
				if (Tcomshr->lindem[x-1] == 0)
					Tcomshr->lindem[x-1] = -1;
				return 1;

			default:
				return 0;
		}
	}
	else
	{
		/* SDCODES 1XY,2XY,3XY,4TT,5TT,4TXY,6MRR */

		/* use 'h' as 100's, 'x' as 10's and 'y' as units */
		h = k/100;
		a = h*100;
		x = (k-a)/10;
		b = x*10;
		y = k-a-b;

		/* 6MRR code */
		if (h >= 60)
		{
			/* skip 6MRR code unless now in stage N of 4-stage sequence N -> N+3 */
			if (stageNo != Tcomshr->cusig)
				return 1;

			process_6mrr_sdcode(stageNo, linkNo);

			/* 6mmr code covers this and the 3 following stages */
			return 4;
		}

		/* 4TXY code */
		if (h >= 40)
		{
			if (Demand_Second_Green(h, stageNo, x, y))
				set_conditional_demand(stageNo, linkNo);
			return 1;
		}

		/* 4TT and 5TT codes */
		if (h == 4 || h == 5)
		{
			/* For 4TT code, set delayed latched demand after TT (0.1s)(TT=10*XY) */
			/* For 5TT code, also require normal demand and delay < TT+150 (+15s) */

			tt = 100*x+10*y;

#if defined (CMOVA_EP) /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */

			/* Use AA to limit period for setting 5TT demand to TT -> TT+15s */
			aa = tt + 150;

			if (Tcomshr->ltype[linkNo-1] < 0) 
			{
				/* (pseudo?) emerg/priority link; check em/pr detectors. */
				/* Intended to give delayed hurry-calls for two closely-linked MOVAs.
					Assumed to work with latched continuous detection forcing linking */

				/* 5TT code also requires normal demand */
				if (h == 5 && Tcomshr->stadem[stageNo-1] <= 0)
					return 1;

				for (i = 1; i <= 2; i++) 
				{
					d = Tcomshr->epdet[linkNo-1][i-1];
					if (d > 0 && Tcomshr->susdet[d-1] == 0) 
					{
						if (iDemandDelay_[linkNo-1] >= tt && tt > 0) 
						{
							/* Delay has timed off.  Check candet and set demand if appropriate */
							if (Tcomshr->cancel[linkNo-1] == 0)
							{
								/* set demand if det exists & is OK, delay has timed off,
									and det has not been continuously 'on' >AA */
								if (h == 4 || (h == 5 && Tcomshr->timon[d-1] <= aa))
								{
									set_latched_demand(stageNo, linkNo);
									break;
								}
							}
						}
					}
				}
				return 1;
          
			} /* if(Tcomshr->ltype[l-1] < 0) */        

#endif /* CMOVA_EP */ /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */

			if (Tcomshr->ltype[linkNo-1] > 0)
			{
				/* (pseudo?) pedestrian link; check ped detector. */
				/* Assumed to work with latched continuous ped detection (LTYPE<100) */

				d = Tcomshr->ltype[linkNo-1];
				if (Tcomshr->susdet[d-1] == 0 && Tcomshr->timon[d-1] >= tt)
				{
					/* set demand if det OK, & is 'on' long enough */
					set_latched_demand(stageNo, linkNo);
					return 1;
				}
			}
			return 1;
		}

		/* left with 1XY, 2XY and 3XY codes */

		/* convert stage 0 to stage 10 for X and Y */
		if (x == 0)
			x = 10;
		if (y == 0)
			y = 10;

		switch (h)
		{
			case 1:
				/* For 1XY code, set non-latched demand if stages X,Y not demanded */
				if (Tcomshr->stadem[x-1] == 0 && Tcomshr->stadem[y-1] == 0)
					set_conditional_demand(stageNo, linkNo);
				return 1;
				
			case 2:
				/* For 2XY code, first check for 'special link demand */
				if (Tcomshr->lindem[linkNo-1] == 2 || Tcomshr->lindem[linkNo-1] == 3)
				{
					/* Drop through switch statement to process as 3XY code */
				}
				else
					return 1;

			case 3:
				/* For 3XY code, check if competing demands on stages X to Y */
				for (b = x; ;)
				{
					if (Tcomshr->stadem[b-1] > 0)
					{
						set_conditional_demand(stageNo, linkNo);
						return 1;
					}

					/* If all stages in range X -> Y have been processed then return */
					if (b == y)
						break;

					b++;
					if (b > Tcomshr->stages)
						b = 1;
				}
				return 1;
				
			default:
				return 0;
		}
	}
}



static void process_6mrr_sdcode(int stageNo, int linkNo)
{
	int a,aa,b,bb,c,d,e,f,g,h,k,m,ma,mb,ra,rb,w,x,y;

	k = Tcomshr->sdcode[stageNo-1][linkNo-1];

	/* use 'h' as 100's, 'x' as 10's and 'y' as units */
	h = k/100;
	a = h*100;
	x = (k-a)/10;
	b = x*10;
	y = k-a-b;

	g = h/10;
	g *= 10; /* G is tolerance of 60 = 6-seconds for later use */
	w = h-g;

	/* Check all 4 links demanded.  We know that Right(A)=X, 
	   & Right(B)=Y are both already demanded. 
	   Check others:  Main(A)=L,   Main(B)=W.  Note: L,W,X,Y are LINKs */
	if (Tcomshr->lindem[linkNo-1] == 0 || Tcomshr->lindem[w-1] == 0)
		return;

	/* Clear any existing N+1/N+2 stage demands. 
	   Set to -6 to mark as stages which can be skipped */
	Tcomshr->stadem[stageNo] = -6;
	Tcomshr->stadem[stageNo+1] = -6;

	/* Implement 6MRR code, for the 4 links L,W,X,Y.
	   Set up durations of extra IG with sequences 1,2,4 or 1,3,4 */

	/* Set up e and f as intergreens N->N+1 and N->N+2.
	   Use abs() to allow for the unlikely possibility of negative IG.
	   The +5 allows for 0.5-sec min-green needed. */
	e = ABS(Tcomshr->change[stageNo-1][stageNo]) +5;
	f = ABS(Tcomshr->change[stageNo-1][stageNo+1]) +5;

	/* Choose GREEN needed, or equivalent min green, for each link */
	m = Tcomshr->llanes[x-1][0];
	aa = Tcomshr->maxmin[m-1];

	h = Tcomshr->redcx[m-1] * Tcomshr->satinc[m-1] + Tcomshr->stlost[m-1];
	if (h > aa)
		h = aa;

	ra = max(max(Tcomshr->sistim, h), Tcomshr->mingre[x-1]);

	m = Tcomshr->llanes[y-1][0];
	bb = Tcomshr->maxmin[m-1];

	h = Tcomshr->redcx[m-1] * Tcomshr->satinc[m-1] + Tcomshr->stlost[m-1];
	if (h > bb)
		h = bb;

	rb = max(max(Tcomshr->sistim, h), Tcomshr->mingre[y-1]);

	/* Decide if right-turn GREEN difference justifies 3-stage sequence */
	a = ra - rb;
	if (a > e)
	{
		/* Set up 1,2,4 sequence */
		Tcomshr->stadem[stageNo] = 6;
		Tcomshr->stadem[stageNo+2] = 6;
		return;
	}
	if (-a > f)
	{
		/* Set up 1,3,4 sequence */
		Tcomshr->stadem[stageNo+1] = 6;
		Tcomshr->stadem[stageNo+2] = 6;
		return;
	}
	c = Tcomshr->smcycl/10;

	ma = max(Tcomshr->min[stageNo+2], Tcomshr->lmin[linkNo-1]);
	d = Tcomshr->yvalue[linkNo-1]*c/10;
	if (Tcomshr->linkos[linkNo-1] > 0)
		d += g;
	if (d > ma)
		ma = d;

	mb = max(Tcomshr->min[stageNo+2], Tcomshr->lmin[w-1]);
	d = Tcomshr->yvalue[w-1]*c/10;
	if (Tcomshr->linkos[w-1] > 0)
		d += g;
	if (d > mb)
		mb = d;

	/* Note AA is MAXMIN for Lane X */
	if (Tcomshr->liensa[x-1] == 0 && ra >= aa)
	{
		d = Tcomshr->yvalue[x-1]*c/10;
		if (Tcomshr->linkos[x-1] > 0)
			d += g;
		if (d > ra)
			ra = d;
	}

	/* Note BB is MAXMIN for Lane Y */
	if (Tcomshr->liensa[y-1] == 0 && rb >= bb)
	{
		d = Tcomshr->yvalue[y-1]*c/10;
		if (Tcomshr->linkos[y-1] > 0)
			d += g;
		if (d > rb)
			rb = d;
	}

	a = ra - rb;
	b = a + (ma - mb);

	/* Decide if main-traffic GREEN-delta + RT-delta justifies 3-stages */

	/* Increase E & F to 3-sec above intergreens N--> N+1 or N+2 */
	e += 25;
	f += 25;

	/* Check need for N+1 */
	if (a > 30 && b > e)
	{
		/* Set up 1,2,4 sequence */
		Tcomshr->stadem[stageNo] = 6;
		Tcomshr->stadem[stageNo+2] = 6;
		return;
	}

	/* Check need for N+2 */
	if (-a > 30 && -b > f)
	{
		/* Set up 1,3,4 sequence */
		Tcomshr->stadem[stageNo+1] = 6;
		Tcomshr->stadem[stageNo+2] = 6;
		return;
	}

	/* only 2-stage sequence 1,4 justified */
	Tcomshr->stadem[stageNo+2] = 6;
	return;
}


static void set_conditional_demand(int stageNo, int linkNo)
{
#if defined (CMOVA_EP)
	/* M7.0.1 - e/p links should not place normal stage demands */
	if (Tcomshr->ltype[linkNo-1] >= 0)
	{
#endif
		/* Set conditional demand for stage, unless already latched demand */
		if (Tcomshr->stadem[stageNo-1] == 0)
			Tcomshr->stadem[stageNo-1] = 2;

#if defined (CMOVA_EP)
	}
	else
	{
		/* Set conditional priority/emergency stage demand if appropriate */
		if (Tcomshr->ltype[linkNo-1] == -5 && Tcomshr->pstdem[stageNo-1] == 0)
			Tcomshr->pstdem[stageNo-1] = 9;
		
		if (Tcomshr->ltype[linkNo-1] == -4 && Tcomshr->estdem[stageNo-1] == 0)
			Tcomshr->estdem[stageNo-1] = 8;
	}
#endif
}


static void set_latched_demand(int stageNo, int linkNo)
{
#if defined (CMOVA_EP)
	/* M7.0.1 - e/p links should not place normal stage demands */
	if (Tcomshr->ltype[linkNo-1] >= 0)
	{
#endif
		/* Set latched demand for stage */
		Tcomshr->stadem[stageNo-1] = 1;

#if defined (CMOVA_EP)
	}
	else
	{
		/* Set latched priority/emergency stage demand if appropriate */
		if (Tcomshr->ltype[linkNo-1] == -5)
			Tcomshr->pstdem[stageNo-1] = 5;

		if (Tcomshr->ltype[linkNo-1] == -4)
			Tcomshr->estdem[stageNo-1] = 4;
	}
#endif
}


static void clear_ep_link_demand(int linkNo)
{
#if defined (CMOVA_EP)
	int stgNo;

	/* Clear any existing demand for the e/p link and also clear any stage demands
	   that the link demand may have caused */

	/* Cancel any previously set e/p link demand */
	Tcomshr->lindem[linkNo-1] = 0;

	/* Ideally ought to check SDCODES to find which stage was demanded, 
		but the quick solution is to just go through the stages in order 
		from CUSIG+1 and if this e/p link is green in that stage, 
		clear any e/p demand for the stage. */

	/* Find the next stage in which the e/p link gets a full green */
	stgNo = Tcomshr->cusig;
	for (;;)
	{
		stgNo++;
		if (stgNo > Tcomshr->stages)
			stgNo -= Tcomshr->stages;
		if (stgNo == Tcomshr->cusig)
			break;

		if (Tcomshr->lgreen[stgNo-1][linkNo-1] != 0 &&
			Tcomshr->lgreen[stgNo-1][linkNo-1] != 4)
		{
			/* if link is green in stgNo and there was an e/p
				demand set, clear it */
			if (Tcomshr->ltype[linkNo-1] == -4 &&
				Tcomshr->estdem[stgNo-1] == 4)
			{
				Tcomshr->estdem[stgNo-1] = 0;
				break;
			}
			else if (Tcomshr->ltype[linkNo-1] == -5 &&
				Tcomshr->pstdem[stgNo-1] == 5)
			{
				Tcomshr->pstdem[stgNo-1] = 0;
				break;
			}
		}
	}
#endif
}


static int is_epdet_active(int linkNo)
{
	int detNo;
	int i;

	for (i = 0; i < 2; i++)
	{
		detNo = Tcomshr->epdet[linkNo-1][i];

		/* Skip if detector not present or suspect */
		if (detNo == 0)
			continue;
		if (Tcomshr->susdet[detNo-1] > 0)
			continue;

		if (Tcomshr->ondet[detNo-1] > 0  || epdemInIG[linkNo-1] > 0)
		{
			/* if the epdet is on indicate demand and clear the IG demand flag
			   as it is no longer intergreen */
			epdemInIG[linkNo-1] = 0;
			return 1;
		}
	}
	return 0;
}


static int get_emergency_stage_next()
{
#if defined (CMOVA_EP)
	int k;
	int i;

	for (i = 1; i < Tcomshr->stages; i++)
	{
		k = Tcomshr->cusig + i;
		if (k > Tcomshr->stages)
			k -= Tcomshr->stages;

		if (Tcomshr->estdem[k-1] == 0)
			continue;

		while (k != Tcomshr->cusig)
		{
			if (Tcomshr->change[Tcomshr->cusig-1][k-1] != -1)
			{
				/* if stage demanded and direct change is allowed then implement */
				return k;
			}

			/* if can't change directly to stage, find the closest intermediate */
			k--;
			if (k <= 0)
				k = Tcomshr->stages;
		}

		/* If the while loop completes there are no allowed moves between
		   CUSIG and ESTDEM. Highly unlikely so just ignore the demand */
		continue;
	}

#endif
	/* No valid emergency stage demand found */
	return 0;
}


static int get_priority_stage_next()
{
#if defined (CMOVA_EP)
	int linkNo;
	int k,n;
	int i;

	for (i = 1; i < Tcomshr->stages; i++)
	{
		k = Tcomshr->cusig + i;
		if (k > Tcomshr->stages)
			k -= Tcomshr->stages;

		if (Tcomshr->pstdem[k-1] == 0)
			continue;

		/* Priority change to stage K demanded */
		/* Check if truncation of current stage CUSIG is inhibited (can't skip CUSIG) */

		/* First check if any indicators to be cleared during green */
		for (linkNo = 1; linkNo <= Tcomshr->nlinks; linkNo++)
		{
			if (Tcomshr->ltype[linkNo-1] < 0 || Tcomshr->lgrefl[linkNo-1] == 0)
			{
				/* e/p link or link now red */
				continue;
			}

			/* Ped links always get enough green.  Check vehicle links */
			if (Tcomshr->ltype[linkNo-1] == 0)
			{
				/* check if opposed green */
				if (Tcomshr->lgreen[Tcomshr->cusig-1][linkNo-1] != 4)
				{
					/* don't clear ind's if link still sat */
					if (Tcomshr->liensa[linkNo-1] == 0)
						continue;
				}
				else if (Tcomshr->lindem[linkNo-1] == 2 || Tcomshr->lindem[linkNo-1] == 3)
					continue;
			}

			Tcomshr->eptrun[linkNo-1] = 0;
			Tcomshr->epskip[linkNo-1] = 0;
		}

		/* Now look to truncate CUSIG.  Start with the assumption it is possible */
		n = Tcomshr->cusig;

		switch ((int)Tcomshr->pfacil[n-1])
		{
		case 0:
			/* no truncation allowed */
			Tcomshr->trunc = 0;
			break;

		case -1:
		case 1:
			/* allow truncation only if no o'sat cycles */
			if (Tcomshr->saosmx[n-1] == 0)
				Tcomshr->trunc = 1;
			else
				Tcomshr->trunc = 0;
			break;

		case -2:
		case 2:
			/* allow truncation if no more than 1 cycle o'sat */
			if (Tcomshr->saosmx[n-1] <= 1)
				Tcomshr->trunc = 1;
			else
				Tcomshr->trunc = 0;
			break;

		case -3:
		case 3:
			Tcomshr->trunc = 1;

			for (linkNo = 1; linkNo <= Tcomshr->nlinks; linkNo++)
			{
				/* Skip irrelevant non-traffic links, or those red in CUSIG */
				if (Tcomshr->ltype[linkNo-1] != 0 || Tcomshr->lgrefl[linkNo-1] == 0)
					continue;

				if (Tcomshr->eptrun[linkNo-1] > 0 || Tcomshr->epskip[linkNo-1] > 0)
				{
					Tcomshr->trunc = 0;
					break;
				}
			}
			break;

		case 4:
			Tcomshr->trunc = 1;
			if (Tcomshr->stagos[n-1] > 0)
			{
				/* constrain amount of green truncation if stage oversaturated */
				int g = Tcomshr->lambda[n-1]*(Tcomshr->smcycl/20)/10;
          
				if (Tcomshr->sistim <= g)
				{
					/* Don't allow truncation until stage exceeds half normal green. 
					 * If too short, jump to see if skipping of other stages allowed */
					Tcomshr->trunc = 0;
				}
			}
			break;

		case -4:
		case -5:
		case 5:
		default:
			/* PFACIL codes 5, -4, -5 always ignore truncation indicators */
			Tcomshr->trunc = 1;
			break;
		}

		/* Loop through the stages from CUSIG to the demanded priority stage K
		   to determine whether intermediate stages can be skipped. */
		for (;;)
		{
			int inhibit_trunc = 0;
			int inhibit_skip = 0;

			n++;
			if (n > Tcomshr->stages)
				n -= Tcomshr->stages;

			/* if priority stage k reached then implement change */
			if (n == k)
				break;

			/* skip stage if no demand */
			if (Tcomshr->stadem[n-1] <= 0)
				continue;

			if (Tcomshr->pfacil[n-1] >= 0)
			{
				/* Stage N can't be skipped, so it becomes next stage */
				k = n;
				break;
			}

			/* Code <= -5 always allows skipping */
			if (Tcomshr->pfacil[n-1] <= -5)
				continue;

			/* Check link indicators */
			for (linkNo = 1; linkNo <= Tcomshr->nlinks; linkNo++)
			{
				/* Skip irrelevant e/p links, or those not green in stage N */
				if (Tcomshr->ltype[linkNo-1] < 0 || Tcomshr->lgreen[n-1][linkNo-1] == 0)
					continue;

				/* For unopposed link, with only normal demand, ignore ind's */
				if (Tcomshr->lgreen[n-1][linkNo-1] == 3)
					if (Tcomshr->lindem[linkNo-1] <= 1)
						continue;

				/* Note if link truncated recently */
				if (Tcomshr->eptrun[linkNo-1] > 0)
					inhibit_trunc = 1;

				/* Note if link has skip indication set */
				if (Tcomshr->epskip[linkNo-1] > 0)
					inhibit_skip = 1;
			}

			if (Tcomshr->pfacil[n-1] < -1)
			{
				if (Tcomshr->pfacil[n-1] == -2 && Tcomshr->saosmx[n-1] > 1)
				{
					/* if oversat 2 cycles can't skip */
					k = n;
					break;
				}

				if (Tcomshr->pfacil[n-1] == -3 && inhibit_trunc > 0)
				{
					k = n;
					break;
				}
			}
			else if (Tcomshr->saosmx[n-1] > 0)
			{
				/* if oversat 1 cycle and pfacil >= 0 then can't skip */
				k = n;
				break;
			}

			if (inhibit_skip > 0)
			{
				k = n;
				break;
			}
		}

		/* change to stage K (or closest intermediate stage if direct change banned) */
		while (k != Tcomshr->cusig)
		{
			if (Tcomshr->change[Tcomshr->cusig-1][k-1] != -1)
			{
				/* if stage demanded and direct change is allowed then implement */
				return k;
			}

			/* if can't change directly to stage, find the closest intermediate */
			k--;
			if (k <= 0)
				k = Tcomshr->stages;
		}
	}

#endif
	/* No valid priority stage demand found */
	return 0;
}


static int get_stage_next()
{
	int pass;
	int k;
	int i;

	for (pass = 0; pass < 2; pass++)
	{
		for (i = 1; i < Tcomshr->stages; i++)
		{
			k = Tcomshr->cusig + i;
			if (k > Tcomshr->stages)
				k -= Tcomshr->stages;

			/* Skip if no demand for this stage (use <=0 to catch -6 from 6MRR) */
			if (Tcomshr->stadem[k-1] <= 0)
				continue;

			while (k != Tcomshr->cusig)
			{
				/* if change CUSIG-K allowed then implement */
				if (Tcomshr->change[Tcomshr->cusig-1][k-1] >= 0)
					return k;

				if (Tcomshr->change[Tcomshr->cusig-1][k-1] == -1)
				{
					/* if change banned then find an acceptable intermediate stage */
					k--;
					if (k <= 0)
						k = Tcomshr->stages;
				}
				else
				{
					/* if IG < -1 allow change on 2nd pass */
					if (pass > 0)
						return k;
					continue;
				}
			}
		}
	}

	/* no stage demands found that can be implemented */

	/* check reversionary stage */
	if (Tcomshr->mainst == 0 || Tcomshr->mainst == Tcomshr->cusig
		|| Tcomshr->stadem[Tcomshr->mainst-1] > 0)
		return 0;

	/* if no other demands, insert a reversionary demand for MAINST and find a suitable change */
	Tcomshr->stadem[Tcomshr->mainst-1] = 2;
	return get_stage_next();
}

