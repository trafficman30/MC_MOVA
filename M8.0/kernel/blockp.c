/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         blockp.c: 
*
*  TITLE:        Mova Kernel: Prepare data for mova operation
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	15-Apr-1993		4.0.0		..		RV		......			Emergency/priority vehicle facilities added
*	19-Jan-1994		4.0.0		..		RV		......			'Big' Extended stages, links, etc
*	25-Jun-1996		4.0.0		..		MC		......			Variable weighting added for PUFFINs
*	26-Jan-1998		4.0.0		..		MC		SIE_00			First undergoing system test
*	29-Apr-1988		4.0.0		..		MC		SIE_01			'const' added to initialised static's (Siemens MOVA-13)
*	03-Mar-1998		4.0.0		..		PC		SIE_02			Overlay on 'Xcomshr', etc., replaced by explicit use of structures.
*	04-Jan-2000		4.0.0		..		PC		SIE_03			Local integers were declared as size 12, now mxlinks
*	29-Sep-2004		5.0.0		..		IH		SIE_04			CheckGreenInFollowingStages() called to check demands for following stages to assess
*																which links are 'relevant' when determining green for the current stage.
*	25-Jan-2005		5.1.0		..		RD		TRL_04			Incorporated Pedestrian priorities.
*	31-May-2011		7.0.0		AA		PK		CRN003			Several Changes for MOVA 7 release
*	31-Aug-2011		7.0.1		..		PK		CRN004			SatFlow and CANDET improvements
*	12-Dec-2011		7.0.1		AB		AK		CRN005			Further CANDET improvements
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
#include "gbltypes.h"
#include "errhand.h"
#include <diff_bss.h>

#include "tma_alerts.h"
#include "tma_strc.h"
#include "tma_preprint.h"
#include "tma_mem.h"
#include "generalfunc.h"


/*  IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA:
    Determines whether the given long link has IN-dets or not. */
static int IsLongLinkWithINDets( int nLinkNo, Ccomshr const * Tcomshr );

/*  IRH MOD: M5.0.0: 02/09/04: Function for processing 4TT and 5TT SDCodes
 *  (delayed demand for emergency/priority links) */
static void ProcessNTTcode(void);

static void set_ep_extensions(int linkNo);

int PedCycle(int marker, int Stages, int lPedDemand);
int nPedWaitTime;
static int rav1 = 0;
static int lCheckMarker = TRUE;
static int nTimer = 10;
static int lCountDown = TRUE;
static int nCusig = 0;

#if defined (CMOVA_EP) /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */
extern int cancelLinkEP[mxlink];
extern int epdemInIG[mxlink];
#endif

void blockp(void)
{
	static int i,n,l,m,a,b,d,j,c,f,e,h,g,aa,bb,k;
	extern int lPedDemand;
	int linkNo, detNo;

	/* IRH MOD: M5.0.0: 31/08/04: Added */
	int nLamdbaSum, nStagosSum;

	for (i = 1; i <= Tcomshr->ndets; i++)
	{
		Tcomshr->ondet[i-1] = 0;
		if (Tcomshr->deton[i-1] > 0)
			Tcomshr->ondet[i-1] = 1;
		Tcomshr->timon[i-1] = Tcomshr->ton[i-1];
		Tcomshr->timoff[i-1] = Tcomshr->toff[i-1];
		Tcomshr->oldgap[i-1] = Tcomshr->pregap[i-1];
		Tcomshr->oldcdc[i-1] = Tcomshr->cdc[i-1];
		Tcomshr->cdc[i-1] = Tcomshr->vc[i-1];
	}
	Tcomshr->presig = Tcomshr->cusig;
	Tcomshr->cusig = Tcomshr->snow;
	
	/* ped cycle control */
	Tcomshr->rav1 = PedCycle(Tcomshr->marker, Tcomshr->stages, lPedDemand);
	/* increment nPedWaitTime for use in ped weighting */
	if (Tcomshr->rav1 > 0) 
		nPedWaitTime++;
	else
		nPedWaitTime = 0;

#if defined (CMOVA_EP) /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */
	/* Record any CANDET coming on */
	for (linkNo = 1; linkNo <= Tcomshr->nlinks; linkNo++)
	{
		int cdetStat = 0;

		detNo = Tcomshr->candet[linkNo-1];
		if (detNo > 0)
		{
			if (Tcomshr->ondet[(detNo%100)-1] > 0)
			{
				/* Is the link currently green or red */
				int stg;
				int colour = 0;

				if (Tcomshr->cusig == 0)
					stg = Tcomshr->demsta;
				else
					stg = Tcomshr->cusig;

				/* assume link is red if there has been an error getting the stg */
				if (stg == 0)
					colour = 0;
				else
				{
					if (Tcomshr->lgreen[stg-1][linkNo-1] != 0 && 
						Tcomshr->lgreen[stg-1][linkNo-1] != 4)
						colour = 1;
					else
						colour = 0;
				}

				/* cancel the demand based on CANDET type */
				/* cancelLinkEP is only used to cancel demand (i.e. before the stage runs)
				   so only set when link is RED and CANDET is > 100 */
				if (detNo > 100 && colour == 0)
				{
					cdetStat = 1;
					cancelLinkEP[linkNo-1] = 1;
				}
			}
			else
			{
				/* CANDET is off - clear any suspect status */
				detNo = detNo % 100;
				if (Tcomshr->susdet[detNo-1] == 2)
				{ 
					Tcomshr->susdet[detNo-1] = 0;
					detFaultyChanged(detNo-1, 0);
				}

			}
		}

		/* Also check the e/p dets here and clear long on faults if appropriate */
		for (i = 0; i < 2; i++)
		{
			detNo = Tcomshr->epdet[linkNo-1][i];
			if (detNo > 0)
			{
				if (Tcomshr->ondet[detNo-1] == 0 && Tcomshr->susdet[detNo-1] == 2)
				{ 
					Tcomshr->susdet[detNo-1] = 0;
					detFaultyChanged(detNo-1, 0);
				}
			}
		}

		/* Also need to record any EPDET activity during IG which could be missed
		   by the time Block B runs during the next stage green */
		if (Tcomshr->cusig == 0)
		{
			int epdem = 0;
			for (i = 0; i < 2; i++)
			{
				detNo = Tcomshr->epdet[linkNo-1][i];
				if (detNo > 0)
				{
					if (Tcomshr->ondet[detNo-1] > 0 && Tcomshr->susdet[detNo-1] == 0)
					{
						epdem = 1;
						break;
					}
				}
			}

			if (cdetStat > 0)
			{
				epdem = 0;
				epdemInIG[linkNo-1] = 0;
			}

			if (epdem > 0)
				epdemInIG[linkNo-1] = 1;
		}
	}
#endif

/*
 --- SCAN INTERVAL OF 0.5 SECONDS. TIMERS IN 0.1 SECOND UNITS.
*/
	Tcomshr->sistim += Tcomshr->scan;
/*
 --- INCREMENT TIMER OF STAGE GREEN OR STAGE-STAGE INTERGREEN
*/
	if (Tcomshr->sistim > 3000)
		Tcomshr->sistim = 3000;
	for (n = 1; n <= Tcomshr->stages; n++)
	{
		Tcomshr->stcycl[n-1] += Tcomshr->scan;
		if (Tcomshr->stcycl[n-1] > 3200)
			Tcomshr->stcycl[n-1] = 3200;
/*
 ---         UPDATE STAGE CYCLE TIMERS
*/
	}
	for (l = 1; l <= Tcomshr->nlinks; l++)
	{
		Tcomshr->cycltm[l-1] += Tcomshr->scan;
/*
 ---   UPDATE LINK CYCLE AND GREEN TIMERS, BUFFER GREEN STATUS
*/
		Tcomshr->lgrefl[l-1] = Tcomshr->greens[l-1];
		if (Tcomshr->lgrefl[l-1] > 0)
			Tcomshr->lingre[l-1] += Tcomshr->scan;
/*
 ---   NEED TO CHECK IF TIMER IS OVERFLOWING. IF SO, CORRECT
*/
		if (Tcomshr->lingre[l-1] > 3000)
			Tcomshr->lingre[l-1] = 3000;
		if (Tcomshr->cycltm[l-1] > 3200)
			Tcomshr->cycltm[l-1] = 3200;
	}
/*
 //////////////////////////////////////////////////////////////////////
     SUBSECTION P2 : UPDATE REGISTERS AND RED COUNTS
 --- NOTE : DO-LOOP FOR LANES SPANS P2,P3,P4
*/
	for (m = 1; m <= Tcomshr->nlanes; m++)
	{
		a = Tcomshr->crusin[m-1];
		b = Tcomshr->crusx[m-1];
		d = Tcomshr->det[m-1][0];
        
		/*  IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: Ignore -ve IN dets */
		if (d <= 0)
			a = b;

		for (i = 1; i < a; i++) 
		{
			Tcomshr->shrega[m-1][i-1] = Tcomshr->shrega[m-1][i];
		}

		/* when no IN dete only lower end of SHREGA used */
        /*  IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: Ignore -ve IN dets */
		if (d > 0)
		{
			c = Tcomshr->cdc[d-1] - Tcomshr->oldcdc[d-1];
			if (c != 0)
				c = 1;

			/* insert IN count this scan at top of SHREGA */
			Tcomshr->shrega[m-1][a-1] = (signed char)c;

			/* and update red count */
			Tcomshr->redcin[m-1] += c;
			if (Tcomshr->inxtra[m-1] > 0)
				Tcomshr->inxtra[m-1] -= c;

			Tcomshr->shrega[m-1][a] = 0;
			if (Tcomshr->timon[d-1] > 0 && Tcomshr->timon[d-1] < 25)
				Tcomshr->shrega[m-1][a] = 1;
		}

		/* now process X detetcor */
		e = Tcomshr->det[m-1][1];
		h = Tcomshr->cdc[e-1] - Tcomshr->oldcdc[e-1];
		if (h != 0)
			h = 1;

		Tcomshr->redcx[m-1] += h;
		if (Tcomshr->susdet[e-1] > 0 && d > 0 && Tcomshr->susdet[d-1] == 0)
		{
			/* X det dud, but IN det ok. Leave IN counts throughout SHREGA  */
		}
		else
		{
			/* otherwise insert X count at CRUSX position in SHREGA */
			Tcomshr->shrega[m-1][b-1] = (signed char)h;
			
			/* if X det on breiefly, insert vehicl in register */
			Tcomshr->shrega[m-1][b] = 0;
			if (Tcomshr->timon[e-1] > 0 && Tcomshr->timon[e-1] < 25)
				Tcomshr->shrega[m-1][b] = 1;
		}
/*
 //////////////////////////////////////////////////////////////////////
     SUBSECTION P3 : ALLOW FOR QUEUE BLOCK-BACK OVER IN-DET
*/
		/* if IN det exists and is ok, and conditions for queue blocking are met */
		if (d > 0 && Tcomshr->susdet[d-1] == 0 &&
			Tcomshr->timon[d-1] >= Tcomshr->qminon[m-1])
		{
			/* check for expected arrivals whilst IN det is on */

			/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error 
			* and 602 moved *before* 601 */
			DivideError((int)Tcomshr->smflow[m-1], 602,
				"lane number was %d", m );
        
			/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
			DivideError((int)(3600/Tcomshr->smflow[m-1]), 601,
				"smflow was %d, lane number was %d",
				Tcomshr->smflow[m-1], m );
        
			c = Tcomshr->timon[d-1] / (3600/Tcomshr->smflow[m-1]);
		
			h = Tcomshr->exitrc[m-1] + Tcomshr->sinkrc[m-1];
			if (h != 0)
			{
				/* need to correct for EXITRC or SINKRC */
				/* for h <= REDCIN, limit multiplier to 2 */
				a = c+c;
				if (h < Tcomshr->redcin[m-1])
				{
					/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */    
					DivideError((int)Tcomshr->redcin[m-1], 603,
						"lane number was %d", m );

					a = c*(h+Tcomshr->redcin[m-1])/Tcomshr->redcin[m-1];
				}
				c = a;
			}

			/* and increase queue beyond IN det if necessary */
			if (Tcomshr->inxtra[m-1] < c)
				Tcomshr->inxtra[m-1] = c;
		}

/*
 //////////////////////////////////////////////////////////////////////
     SUBSECTION P4 : IF ANY 'OUT' DETECTORS, DECREMENT RED COUNTS ETC.
*/
		/* If any OUT detector, decrement red counts */
		f = Tcomshr->det[m-1][2];
		if (f != 0)
		{
			/* limit red count changes to 1 each scan */
			c = Tcomshr->cdc[f-1] - Tcomshr->oldcdc[f-1];
			if (c != 0)
				c = 1;
			Tcomshr->exitrc[m-1] += c;

			/* red counts must be non-negative, and non-zero if OUT det on */
			a = 0;
			if (Tcomshr->ondet[f-1] > 0)
				a = 1;

			Tcomshr->redcin[m-1] -= c;
			if (Tcomshr->redcin[m-1] < a)
				Tcomshr->redcin[m-1] = a;

			Tcomshr->redcx[m-1] -= c;
			if (Tcomshr->redcx[m-1] < a)
				Tcomshr->redcx[m-1] = a;
		}

		/* No deal with IN-SINK */
		f = Tcomshr->det[m-1][3];
		if (f != 0 && Tcomshr->susdet[f-1] == 0)
		{
			c = Tcomshr->cdc[f-1] - Tcomshr->oldcdc[f-1];
			if (c != 0)
			{
				Tcomshr->redcin[m-1]--;
				Tcomshr->sinkrc[m-1]++;
				if (Tcomshr->redcin[m-1] < 0)
					Tcomshr->redcin[m-1] = 0;
			}
		}

		/* X-SINK */
		f = Tcomshr->det[m-1][5];
		if (f > 0 && Tcomshr->susdet[f-1] == 0)
		{
			c = Tcomshr->cdc[f-1] - Tcomshr->oldcdc[f-1];
			if (c != 0)
			{
				Tcomshr->redcx[m-1]--;
				Tcomshr->sinkrc[m-1]++;
				if (Tcomshr->redcx[m-1] < 0)
					Tcomshr->redcx[m-1] = 0;

				/* Maintain demand - if REDCIN=1 don't decrement */
				if (Tcomshr->redcin[m-1] > 1)
					Tcomshr->redcin[m-1]--;
				else if (Tcomshr->redcin[m-1] < 0)
					Tcomshr->redcin[m-1] = 0;
			}
		}

		/*  ASSOCD detectors*/
		for (i = 1; i <= 2; i++)
		{
			g = Tcomshr->assocd[m-1][i-1];
			if (g == 0)
				continue;

			if (Tcomshr->susdet[g-1] > 0)
				continue;

			c = Tcomshr->cdc[g-1] - Tcomshr->oldcdc[g-1];
			if (c == 0)
				continue;

			Tcomshr->redcin[m-1]--;
			Tcomshr->exitrc[m-1]++;
            
			if (Tcomshr->redcin[m-1] < 0)
				Tcomshr->redcin[m-1] = 0;
		}
    }

	
	if (Tcomshr->warmup <= Tcomshr->stages)
		goto S800;
/*
 **********************************************************************
      SUBSECTION P5 : SET/UNSET OVERSATURATION FLAGS
*/
	aa = 0; /* 'aa' notes any change found in o'sat lanes (used in P6) */
	bb = 0; /* 'bb' notes any increase in o'sat cycles on any lane (used in P8) */


	/* clear max o'sat cycles on any lane and stage prior to resetting  */
	Tcomshr->laosmx = 0;
#if defined (CMOVA_EP)
	for (n = 1; n <= Tcomshr->stages; n++)
		Tcomshr->saosmx[n-1] = 0;
#endif

	for (l = 1; l <= Tcomshr->nlinks; l++) 
	{
		/* skip ped and e/p links as they can't be o'sat */
		if (Tcomshr->ltype[l-1] != 0)
			continue;

		for (k = 1; k <= Tcomshr->llatot[l-1]; k++)
		{
			f = 0; /* reset to stop stagos going negative (&subseq redlt< 0) */
			m = Tcomshr->llanes[l-1][k-1];

			/* skip short lanes unless short link, otherwise they can't be o'sat */
			if (!IS_LONG_LANE(m-1) && Tcomshr->shortl[l-1] == 0)
				continue;

			/*  IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: 
				Use the stationary queue formation time (osat check time) 
				for (comb) X-detectors if CMOVA enabled, 
				but use CRUSIN as normal for IN-dets
			*/
			if (Tcomshr->xosat[m-1] == 0)
			{
				c = 10 * Tcomshr->crusin[m-1];
			}
			else /* XOSAT = 1 or 2 */
			{
				c = Tcomshr->osattm[m-1];
			}

/*
 --- TIME TO CHECK O'SAT IS 'C' IN 0.1 SEC UNITS
*/
			if (Tcomshr->cycltm[l-1] > c)
				goto S460;/* M 4.0*/
/*
 --- Skip check if beyond this time; jump (M 4.0 to 460 not 480)
*/
            /*  IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: 
                select appropriate detector for oversaturation check */
			switch (Tcomshr->xosat[m-1])
			{
				case 0: /* IN-det */
					/*	N.B. 'd' could theoretically be 0/negative here - 
						we rely on XOSAT being set correctly in
						MOVA setup to avoid this. */
					d = Tcomshr->det[m-1][0];
					break;

				case 1: /* X-det */

					d = Tcomshr->det[m-1][1];
					break;

				case 2: /* Comb X-det */

					d = Tcomshr->det[m-1][4];               
					break;
				
				default:
					/*  Should never, ever get here!
						Use X-det by default, as we know there will be one. */
					d = Tcomshr->det[m-1][1];
					break;
			}

			if (Tcomshr->susdet[d-1] > 0)
			{
				/* can't do o'sat check with suspect detector */
				goto S420;
			}

			/*  IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: for combination dets, 
                just check the det in main lane for queue back 
                (this behaviour is similar to standard MOVA in that standard MOVA 
                doesn't check short lanes on long links for queue back) */
			if (Tcomshr->xosat[m-1] == 2)
			{
				/*  If the X-det in the main lane has been on long enough, set osatic
					high enough so that osat will be flagged */
				if (Tcomshr->timon[Tcomshr->det[m-1][1]-1] >= Tcomshr->qminon[m-1])
				{
					Tcomshr->osatic[m-1] = Tcomshr->osatcc[m-1];
				}
			}
			else /* Not using a combo-det */
			{
				if (Tcomshr->timon[d-1] >= Tcomshr->qminon[m-1])
				{
					Tcomshr->osatic[m-1] = Tcomshr->osatcc[m-1];
				}
			}

			if (Tcomshr->cycltm[l-1] < c)
			{
				/* too soon to do o'sat critical count check */
				goto S460;
			}

			e = Tcomshr->cdc[d-1] - Tcomshr->precdc[d-1];
			if (e < 0)
				e = (int)((long)e+32767L); /* TRL/MC mod: cast to int */
			e += Tcomshr->osatic[m-1];

			/*  IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: if we are using the combination 
				detector, add the osat initial counts from the associated X-detectors too */
			if (Tcomshr->xosat[m-1] == 2)
			{
				/* For each associated detector */
				for (i = 0; i != 2; ++i )
				{
					g = Tcomshr->assocd[m-1][i];
					
					/* If there is an associated detector that isn't suspect */
					if (g > 0 && Tcomshr->susdet[g-1] == 0)
					{
						/* For each lane */
						for (b = 0; b < Tcomshr->nlanes; ++b)
						{
							/* If the X-det matches the assoc det */
							if (Tcomshr->det[b][1] == g)
							{
								/* Add in the osat initial count for this lane */
								e += Tcomshr->osatic[b];

								/* X-det is unique to lane, so quit loop */
								b = Tcomshr->nlanes;
							}
						}
					}
				}
			}

			if (e >= Tcomshr->osatcc[m-1])
			{
				/* JUMP TO FLAG O'SAT FOR LANE IF Q >= CRITICAL VALUE */
				goto S450;
			}

S420:
/*
 --- LANE IS NOT O'SAT. REDUCE O'SAT MARKERS AS NECESSARY
*/
			/* skip lanes that are not oversaturated */
			if (Tcomshr->laosat[m-1] == 0)
				continue;

			/* reduce markers by LANEWF and note change made in 'aa' */
			Tcomshr->juncos -= Tcomshr->laosat[m-1];
			Tcomshr->laosat[m-1] = 0;

			pTmaData->sAlert._noOveSatCy[m-1] = 0;
			checkAlertOverSat(pTmaData);

			aa = 1;
			f = -Tcomshr->lanewf[m-1];
			Tcomshr->linkos[l-1] += f;
/*
 --- SET UP 'F' TO REDUCE MARKERS AT 460
*/
			goto S460;

S450:
/*
 --- LANE IS O'SAT. INCREASE O'SAT MARKERS AS NECESSARY
*/
			/* set 'bb' to indicate o'sat marker increased */
			bb = 1;
			/* clear the temp factor, 'f' */
			f = 0;

			pTmaData->_currPeriod._Lanes[m-1]._noOveSatCycles++;
			pTmaData->sAlert._noOveSatCy[m-1]++;
			checkAlertOverSat(pTmaData);

			if (Tcomshr->laosat[m-1] < 9)
			{
				/* skip if already had 9 consecutive o'sat cycles */

				Tcomshr->laosat[m-1]++;
				Tcomshr->juncos++;

				if (Tcomshr->laosat[m-1] <= 1)
				{
					/* LINKOS markers only updated if LAOSAT < 2 (already set otherwise) */
					aa = 1;
					f = Tcomshr->lanewf[m-1];
					Tcomshr->linkos[l-1] += f;
				}
			}
			goto S460;


S460:
			/* adjust STAGOS markers using F, and set SAOSMX's */
			for (n = 1; n <= Tcomshr->stages; n++)
			{
				/* skip if link effectively red in stage N */
				if (Tcomshr->lgreen[n-1][l-1] == 0 ||
					Tcomshr->lgreen[n-1][l-1] == 4)
					continue;

				/* sum LANEWF's for all lanes green in stage */
				Tcomshr->stagos[n-1] += f;
				/* MOVA-87: Possible 'divide by zero' fault fix */
				if (Tcomshr->stagos[n-1] < 0)
					Tcomshr->stagos[n-1] = 0;

#if defined (CMOVA_EP) /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */
				/* Update max no of o'sat cycles on any lane in stage */
				if (Tcomshr->saosmx[n-1] < (char)Tcomshr->laosat[m-1])
					Tcomshr->saosmx[n-1] = (char)Tcomshr->laosat[m-1]; /* M 4.0 */
#endif /* CMOVA_EP */ /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */
			}

			/* Update max no of consecutive o'sat cycles on any lane */
			if (Tcomshr->laosat[m-1] > Tcomshr->laosmx)
				Tcomshr->laosmx = Tcomshr->laosat[m-1];
		}
    }
/*
 ELEL
 //////////////////////////////////////////////////////////////////////
     SUBSECTION P6 : UPDATE 'REDUCED LOST TIMES' FOR O'SAT LINKS
*/
	if (aa == 0)
		goto S600;
/*
 ---JUMP IF NO CHANGE IN O'SAT STATUS OF LANES (NOTED BY 'AA')
*/
	b = 1;
/*
 --- SUM FROM B=1 TO AVOID ZERO DIVIDE LATER
 --- SUM ALL LAMBDA-WEIGHTED STAGE W.F.'S FOR JUNCTION
*/
    nLamdbaSum = nStagosSum = 0;
    for (n = 1; n <= Tcomshr->stages; n++) 
	{
		b += Tcomshr->lambda[n-1]*Tcomshr->stagos[n-1];

		/* IRH MOD: M5.0.0: 31/08/04: Sum lambda and stagos for 
		 * DBZ checks 604, 605 and 606 (below) */
		nLamdbaSum += Tcomshr->lambda[n-1];
		nStagosSum += Tcomshr->stagos[n-1];
    }

	for (l = 1; l <= Tcomshr->nlinks; l++) 
	{
		/* Skip ped/emerg/priority link or link not o'sat: 'REDLT' not needed */
		if (Tcomshr->ltype[l-1] != 0 || Tcomshr->linkos[l-1] == 0)
			continue;

		/* sum weighted STAGOSs for stages green to o'sat link */
		c = 0;
		for (n = 1; n <= Tcomshr->stages; n++) 
		{
			if (Tcomshr->lgreen[n-1][l-1] == 0 ||
				Tcomshr->lgreen[n-1][l-1] == 4)
				continue;

			c += Tcomshr->lambda[n-1]*Tcomshr->stagos[n-1];
        }

		/* now compute lost time for link */

		/* MOVA-87: Possible 'divide by zero' fault fix */
		if (c <= 0)
		{
			Tcomshr->redlt[l-1] = 0;
			continue;
		}

		d = Tcomshr->lostim[l-1]/10;

		/* scale values to prevent overflow */
		if (c <= 320)
		{
			/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
			DivideError((int)(b),   606,
				"b was %d, sum of lambda was %d, sum of stagos was %d",
				b, nLamdbaSum, nStagosSum );

			Tcomshr->redlt[l-1] = c*100/b*d/10;
		}
		else if (c <= 3200)
		{
			/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
			DivideError((int)(b/10),  605,
				"b was %d, sum of lambda was %d, sum of stagos was %d",
				b, nLamdbaSum, nStagosSum );

			Tcomshr->redlt[l-1] = c*10/(b/10)*d/10;
		}
		else
		{
			/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
			DivideError((int)(b/100), 604,
				"b was %d, sum of lambda was %d, sum of stagos was %d",
				b, nLamdbaSum, nStagosSum );

			Tcomshr->redlt[l-1] = c/(b/100)*d/10;
		}
    }

S600:
/*
 //////////////////////////////////////////////////////////////////////
     SUBSECTION P7 : UPDATE BONUS-GREEN (SMOOTHED) FOR LINKS
*/
    for (l = 1; l <= Tcomshr->nlinks; l++)
	{
		if (Tcomshr->bontim[l-1] == 0 || 
			Tcomshr->bontim[l-1] != Tcomshr->lingre[l-1])
			goto S680;
		
		if (Tcomshr->lgrefl[l-1] == 0 ||
			Tcomshr->shortl[l-1] > 0 || Tcomshr->liensa[l-1] > 0)
			goto S680;
/*
 --- Skip if: red, no bonus, not bonus time, short link, is end-sat.
 --- Only traffic links remain. Ped & em/pr links have no bonus ! M 4.0
*/
		if (Tcomshr->cusig > 0 && Tcomshr->lgreen[Tcomshr->cusig-1][l-1] == 4) 
			goto S680;
		if (Tcomshr->cusig == 0 && Tcomshr->lgreen[Tcomshr->lastag-1][l-1] == 4) 
			goto S680;
/*
 --- SKIP BONUS CALC FOR LINK WITH UNOPPOSED GREEN IN ANOTHER STAGE
*/
        j = Tcomshr->llatot[l-1];
        b = 0;
        c = 0;
/*
 --- LINK BONUSES SUMMED INTO 'B' AND 'C' IN 0.1 SEC UNITS
*/
        for (k = 1; k <= j; k++) 
		{
            m = Tcomshr->llanes[l-1][k-1];

            /*  IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA:
                Long lanes may exist without IN-dets in CMOVA */
			if (!IS_LONG_LANE(m-1))
				continue;
            
            d = Tcomshr->det[m-1][1];
            if (Tcomshr->susdet[d-1] > 0)
				goto S680;
/*
 ---         SKIP SHORT LANES. SKIP BONUS CALC IF X-DET U/S
*/
			b += (Tcomshr->redcx[m-1]+Tcomshr->exitrc[m-1]-Tcomshr->bonbc[m-1])
				*Tcomshr->satinc[m-1];
            c += Tcomshr->sinkrc[m-1]*Tcomshr->satinc[m-1];
        }

		if (b < 0)
			b = 0;
        
        /* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
        DivideError((int)Tcomshr->nmainl[l-1], 607,
            "link number was %d",
            ( l + 1 ) );

        b = b/Tcomshr->nmainl[l-1];
        c = c/Tcomshr->nmainl[l-1];
/*
 --- BONUSES ARE PER LONG LANE ON LINK. NOW SMOOTH EXPONENTIALLY
*/
        Tcomshr->bonusg[l-1] = (b+Tcomshr->bonusg[l-1]+Tcomshr->bonusg[l-1]+2)/3;
        Tcomshr->snkbon[l-1] = (c+Tcomshr->snkbon[l-1]+Tcomshr->snkbon[l-1]+2)/3;
        Tcomshr->totbon[l-1] = Tcomshr->snkbon[l-1]+Tcomshr->bonusg[l-1];/* M 2.0*/
/*
 --- SAVE TOTAL BONUS FOR LINK IN TOTBON(L)
 --- NOW CHECK WHETHER STAGE-BONUSES TO BE RESET BY THIS LINK
*/
        if (Tcomshr->boncut[l-1] == 0)
			goto S670;
/*
 --- SKIP RESETTING IF NON-DOMINANT LINK
*/
        aa = 1;                    /* M 2.0*/
/*
 --- NOTE LINK BONUS BEING UPDATED USING 'AA' AS MARKER
 --- ONLY CRITICAL BONUSES RESET 'AA' (BONCUT NON-ZERO)
*/
        n = Tcomshr->cusig;
        if (n == 0)
			n = Tcomshr->presig;/* M 2.0*/
        if (n == 0)
			n = Tcomshr->lastag;
/*
 --- SET 'N' TO STAGE TO BE ALLOCATED BONUS FOR LINK 'L'
*/
        Tcomshr->lbonst[l-1] = (char)n; /* TRL/MC mod: cast to char */
S670:
        if(Tcomshr->linkos[l-1] > 0)
			goto S680;
/*
 --- UNLESS LINK IS O'SAT, CHECK WHETHER TO SET END-SAT FLAG
*/
        if (Tcomshr->totbon[l-1] < Tcomshr->lostim[l-1])
			goto S680;/* M 2.1*/
/*
 --- JUMP UNLESS BONUS IS LARGE ENOUGH TO SET END-SAT FOR LINK.
*/
        /*  IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: Don't set end-sat
            at BONTIM unless the link has IN-dets, otherwise we may 
            curtail green too abruptly due to the short-sighted delay-optimiser */
        if (IsLongLinkWithINDets(l-1, Tcomshr))
        {
            Tcomshr->liensa[l-1] = 9;
        }
        /*Tcomshr->liensa[l-1] = 9;*/
S680:
        continue;
    }
/*
 ELEL
 **********************************************************************
     SUBSECTION P8 : DECIDE IF SHORT-CYCLE CONTROL NEEDED
*/
    if (aa == 0)
		goto S705;         /* M 2.0*/
/*
 --- SKIP IF NO CHANGES IN O'SAT lanes (P5) OR STAGE BONUSES (P7)
*/
    if (Tcomshr->juncos <= 1)
		goto S790;
/*
 --- SKIP IF JUNCTION NO LONGER O'SAT. ELSE, RESET LOW DRIFT MAX'S ETC
*/
    for (n = 1; n <= Tcomshr->stages; n++) 
	{/* M 2.0*/
        Tcomshr->stabon[n-1] = 0;  /* M 2.0*/
        Tcomshr->stbonl[n-1] = 0;  /* M 2.0*/
    }
/*
 --- FIRST DECIDE WHICH LINK BONUS TO ALLOCATE TO EACH STAGE
 --- CHECK LINKS WITH BONCUT > 0 , POTENTIALLY DOMINANT         ! M 2.7
*/
    for (l = 1; l <= Tcomshr->nlinks; l++)
	{
        if (Tcomshr->boncut[l-1] <= 0)
			continue;
        
		n = Tcomshr->lbonst[l-1];
/*
 ---   STAGE IN WHICH LINK 'L' RECALCULATES IT'S BONUS IS 'N'   ! M 2.7
 ---   SAVE DOMINANT LINK BONUS; MAY BE OVERWRITTEN BELOW.      ! M 2.7
*/
        Tcomshr->stabon[n-1] = Tcomshr->totbon[l-1];/* M 2.7*/
        Tcomshr->stbonl[n-1] = (char)l; /* M 2.7 & TRL/MC mod: cast to char */
    }
/*
 --- CHECK LINKS WITH BONCUT < 0 POTENTIALLY SUBSERVIENT        ! M 2.7
*/
    for (l = 1; l <= Tcomshr->nlinks; l++)
	{/* M 2.7*/
        if (Tcomshr->boncut[l-1] >= 0)
			continue;/* M 2.7*/
        
		k = -Tcomshr->boncut[l-1]; /* M 2.7*/
/*
 ---   LINK K DECIDES IF LINK L BONUS TO BE USED                ! M 2.7
*/
        if (Tcomshr->linkos[k-1] > 0 || Tcomshr->linkos[l-1] <= 0)
			continue;/* M 2.7*/
/*
 ---   IGNORE LINK L BONUS IF DOMINANT LINK K IS O'SAT, OR      ! M 2.7
 ---   OR IF LINK L IS NOT O'SAT. ELSE, SAVE BONUS.             ! M 2.7
*/
        n = Tcomshr->lbonst[l-1];  /* M 2.7*/
        Tcomshr->stabon[n-1] = Tcomshr->totbon[l-1];/* M 2.7*/
        Tcomshr->stbonl[n-1] = (char)l;  /* M 2.7 & TRL/MC mod: cast to char */
/*
 ---   THIS OVERWRITES ANY NON-O'SAT DOMINANT LINK BONUS WITH   ! M 2.7
 ---   O'SAT SUBSERVIENT LINK BONUS DATA.                       ! M 2.7
*/
    }
S705:
    if (Tcomshr->cutmax > 0)
		goto S800;/* M 2.7*/
/*
 --- Skip if already short-cycling. Else, see if needed
*/
    a = aa + bb;                     /* M 2.0*/
    if (a == 0)
		goto S800;          /* M 2.0*/
/*
 --- Skip if no changes in o'sat or bonuses to require short cycling
*/
    b = 0;                         /* M 2.0*/
    c = 0;
    d = 0;
/*
 --- INITIALISE, THEN: SUM STAGE BONUSES INTO 'B',
 ---                   SAVE MAX LOSTIM(L) IN 'C', and max THETA in 'D'
 SNSN
*/
    for (n = 1; n <= Tcomshr->stages; n++) 
	{
        if (Tcomshr->stabon[n-1] == 0)
			goto S730;
/*
 --- SKIP STAGE IF NO BONUS CURRENTLY SAVED FOR IT
*/
        l = Tcomshr->stbonl[n-1];
/*
 --- CHECK IF ATTRIBUTING LINK GREEN NOT REACHING BONTIM REGULARLY
*/
        g = Tcomshr->smcycl/10*Tcomshr->altlam[n-1]/10;
        if (Tcomshr->linkos[l-1] > 0)
			goto S715;/* M 2.7*/
/*
 --- Skip to use bonus if link o'sat, even if green < BONTIM-2s ! M 2.7
*/
        i = Tcomshr->bontim[l-1]-g;
        if (i > 20)
			goto S720;
S715:
/*
 --- JUMP TO CLEAR STAGE BONUS IF MEAN LINK GREEN 2-SECONDS TOO SHORT
 --- NOW SUM STAGE BONUSES AND FIND MAXIMUM LOST-TIME  AND THETA VALUES
*/
        b += Tcomshr->stabon[n-1];/* M 2.7*/
        if (c < Tcomshr->lostim[l-1])
			c = Tcomshr->lostim[l-1];
        i = (Tcomshr->bontim[l-1] + Tcomshr->stabon[n-1]) * 10;
        j = g + Tcomshr->stabon[n-1];
        if (j < 10)
			goto S730;
/*
 --- SKIP TO AVOID ZERO DIVIDE
*/
        k = i/(j/10);
        if (d < k)
			d = k;
/*
 --- save max THETA value in 'D' if K largest THETA
*/
        goto S730;
S720:
/*
 --- CLEAR STAGE BONUS IF LINK GREEN HAS NOT REACHED BONTIM REGULARLY
*/
        Tcomshr->stabon[n-1] = 0;
        Tcomshr->stbonl[n-1] = 0;
S730:
        continue;
    }
/*
 ENEN
 --- NOW CHECK WHETHER SHORT CYCLE ACTION NEEDED
*/
    if(d <= 0 || b <= c) goto S800;/* M 2.0*/
/*
 --- Skip if THETA unreasonable, or total bonus too small.
 --- Else, reset low drift max's: LOTOTG sums short-cycle greens.
*/
    Tcomshr->lototg = 0;
    for (n = 1; n <= Tcomshr->stages; n++) 
	{
        g = Tcomshr->smcycl/10*Tcomshr->altlam[n-1]/100*d;
        if (Tcomshr->stabon[n-1] > 0)
			g -= Tcomshr->stabon[n-1]/10*(100-d);
        g = g/10;
/*
 ---    'G' IS NOW IN 0.1-SEC UNITS; WAS IN 0.01-SECONDS.
*/
        Tcomshr->lodfmx[n-1] = g*8/10;
/*
 ---    SET LOW DRIFT-MAX'S TO 80% OF REQUIRED GREEN (0.1-SEC UNITS)
*/
        Tcomshr->lototg += g;
    }
    if (Tcomshr->lototg > Tcomshr->totalg)
		goto S790;/* M 2.7*/
/*
 --- Skip, ignore short cycle if exceeds user-specified limit.  ! M 2.7
 --- Else, LOTOTG saved for use in B5 (0.1-SECS) ; set CUTMAX.
*/
    Tcomshr->cutmax = Tcomshr->stages;
    goto S800;
S790:
/*
 --- ZERO CUTMAX TO END SHORT-CYCLING WHEN JUNCTION NO LONGER O'SAT
*/
    Tcomshr->cutmax = 0;
S800:

#if defined (CMOVA_EP) /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */

/*
 **********************************************************************
     SUBSECTION P9 : SET UP EMERGENCY/PRIORITY-VEHICLE EXTENSIONS M 4.0
 --- Note: Emerg/Priority exts may override later max or other changes.
*/
	for (linkNo = 1; linkNo <= Tcomshr->nlinks; linkNo++) 
		set_ep_extensions(linkNo);

    /* IRH MOD: M5.0.0: 02/09/04: Added new subsection for processing
     * 4TT and 5TT SDCodes. */
/*
 **********************************************************************
     SUBSECTION P10 : SET LINK DEMANDS/UPDATE DEMAND DELAY FOR 
     4TT AND 5TT SDCODES (EMERGENCY/PRIORITY LINKS)
*/  
    ProcessNTTcode( );

#endif /* CMOVA_EP */ /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */

/*
 **********************************************************************
*/
    return;
}


/*  IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: 
    Determines whether the given long link has IN-dets or not. */
static int IsLongLinkWithINDets( int nLinkNo, Ccomshr const * Tcomshr )
{
    int nLanesNum = Tcomshr->llatot[ nLinkNo ];
    int nLaneLoop, nLaneNo;
    int nHasINDets = TRUE; 

    for ( nLaneLoop = 0; nLaneLoop < nLanesNum; ++nLaneLoop )
    {
        nLaneNo = Tcomshr->llanes[ nLinkNo ][ nLaneLoop ] - 1;
		
        /* If this lane is a long lane without an IN-detector */
        if ( Tcomshr->dshort[ nLaneNo ] == LONG_LANE )
        {
            /*	This long link has at least one lane without
                an IN-detector (long lanes on a link should either *all*
                have IN-dets, or *none* have IN-dets - shouldn't be a mix) */
            nHasINDets = FALSE;

            nLaneLoop = nLanesNum; /* Quit loop */
        }
    }

    return nHasINDets;

} /* static int IsLongLinkWithINDets( int nLinkNo, Ccomshr const * Tcomshr ) */


/*  IRH MOD: M5.0.0: 02/09/04: Function for processing 4TT and 5TT SDCodes
 *  (delayed demand for emergency/priority links) */
static void ProcessNTTcode(void)
{
	extern int iDemandDelay_[mxlink]; /* Defined in BLKA */

	/* Local variables */
	int linkIdx;
	int stageNo;
	int detNo;
	int code;
	int delayTime;
	int i;

	for (linkIdx = 0; linkIdx < Tcomshr->nlinks; linkIdx++)
	{
		/* skip non em/pr links */
		if (Tcomshr->ltype[linkIdx] >= 0)
			continue;

		/* Does the link have a 4TT or 5TT code, and if so what is TT? */
		/* Start with CUSIG and process stages in order on the off chance
		   the link demands more than one stage */
		delayTime = 0;
		for (stageNo = 1; stageNo <= Tcomshr->stages; stageNo++)
		{
			code = Tcomshr->sdcode[stageNo-1][linkIdx];

			/* only interested in 4TT or 5TT codes */
			if (code < 400 || code > 599)
				continue;

			delayTime = code % 100;
			code -= delayTime; /* store whether it was a 400 or 500 for later */
			break; /* Note the value of stageNo is also held for later use */
		}

		/* Not a 4TT or 5TT SDCODE on the link so skip it */
		if (delayTime == 0)
			continue;

		if (iDemandDelay_[linkIdx] > 0)
		{
			/* Timer already started.  Increment for this scan */
			iDemandDelay_[linkIdx] += 5;

			/* Now check whether to insert a link demand */
			if (iDemandDelay_[linkIdx] >= delayTime)
			{
				if (code == 400 || Tcomshr->stadem[stageNo-1] > 0)
				{
					Tcomshr->lindem[linkIdx] = 4;
					if (Tcomshr->ltype[linkIdx] == -5)
						Tcomshr->lindem[linkIdx] = 5;
				}
			}
		}
		else
		{
			/* Check both e/p detectors for this link for demand */
			for (i = 0; i <= 1; i++)
			{
				detNo = Tcomshr->epdet[linkIdx][i];
				if (detNo > 0 && Tcomshr->susdet[detNo-1] == 0)
				{
					if (Tcomshr->ondet[detNo-1] > 0)
					{
						/* initialise the delayed demand timer */
						iDemandDelay_[linkIdx] = 5;

						/* do NOT actually demand the link at this time - do that when the timer expires */
						break;
					}
				}
			}
		}
	}
}


int PedCycle(int marker, int stages, int lPedDemand)
{
	if (rav1 > 0 )
	{
		if ((rav1 == stages && !lPedDemand) || (rav1 < stages && Tcomshr->cusig == nCusig))  
        /* ped demand no longer present - can stop ped cycle only if still in stage in 
           which demand appeared. Otherwise need to complete cycle
           or
           now back to stage in which demand registered */
			rav1 = 0;
        else
		{  /* ped demand present, decrement if stage just changed */
			if (marker == 0)
			{
				if (lCheckMarker == TRUE)
				{
					lCheckMarker = FALSE;
					rav1--;
				}
            }
            else
               lCheckMarker = TRUE;
		}
	}
	else 
	{
		if (lPedDemand)
		{  /* ped demand just come in */
			if (lCountDown)
			{
                /* decrement nTimer and reset countdown if nTimer 
                   down to 0 (ie 5s elapsed since ped demand first seen) */
				if (nTimer-- == 0)
					lCountDown = FALSE;
            }
            else
            { 
				rav1 = stages;
				nCusig = Tcomshr->cusig;
            }
		}
        else 
        {  /* no ped demand */
			lCountDown = TRUE;  /* set so that next time ped demanded, countdown activated */
			nTimer = 10;        /* set to count down for 5 seconds */
			rav1 = 0;           /* reset rav1 */
		}
    }
    return rav1;
}



/*****************************************************************************
 * set_ep_extensions()
 * Section P9
 *****************************************************************************/
static void set_ep_extensions(int linkNo)
{
	int detNo;
	int ext;
	int i;

	/* Skip non e/p links */
	if (Tcomshr->ltype[linkNo-1] >= 0)
		return;

	/* Decrement existing emerg/priority-link extension timer */
	if (Tcomshr->eplxtm[linkNo-1] > 0)
		Tcomshr->eplxtm[linkNo-1] -= 5;

	/* Check the CANDET to see if e/p extension should be cancelled */
	detNo = Tcomshr->candet[linkNo-1];
	if (detNo > 0)
	{
		if (Tcomshr->timon[(detNo%100)-1] < 1000)
		{
			if (Tcomshr->ondet[(detNo%100)-1] > 0)
			{
				/* Is the link currently green or red */
				int stg;
				int colour = 0;

				if (Tcomshr->cusig == 0)
					stg = Tcomshr->demsta;
				else
					stg = Tcomshr->cusig;

				/* assume link is red there has been an error getting the stg */
				if (stg == 0)
					colour = 0;
				else
				{
					if (Tcomshr->lgreen[stg-1][linkNo-1] != 0 && 
						Tcomshr->lgreen[stg-1][linkNo-1] != 4)
						colour = 1;
					else
						colour = 0;
				}

				/* cancel the extension based on CANDET type */
				/* Can only cancel extensions when link is green */
				/* CANDET=1nn => only cancel during red so it can't cancel extensions */
				if (colour == 1 && (detNo < 100 || detNo > 200))
				{
					Tcomshr->cancel[linkNo-1] = 1;
					Tcomshr->eplxtm[linkNo-1] = 0;
				}
			}
			else
			{
				/* CANDET is an inhibit, so as soon as it goes off clear the cancel flag */
				Tcomshr->cancel[linkNo-1] = 0;
			}
		}
		else
		{
			/* flag 'long-on' (>100s) det suspect */
			Tcomshr->susdet[(detNo%100)-1] = 2;
			detFaultyChanged((detNo%100)-1, 2);
		}
	}

	/* Set-up link extensions on all emerg/pr links */
	/* Check both possible detectors on link */
	for (i = 0; i < 2; i++) 
	{
		detNo = Tcomshr->epdet[linkNo-1][i];

		if (detNo == 0)
			continue;

		if (Tcomshr->timon[detNo-1] < 1000)
		{
			if (Tcomshr->ondet[detNo-1] == 0)
				continue;

			if (Tcomshr->cancel[linkNo-1] == 0)
			{
				/* Set new priority extension if longer than existing one. */
				ext = Tcomshr->epext[linkNo-1][i];
				if (ext > Tcomshr->eplxtm[linkNo-1])
					Tcomshr->eplxtm[linkNo-1] = ext;
			}
		}
		else
		{
			/* flag 'long-on' (>100s) det suspect */
			Tcomshr->susdet[detNo-1] = 2;
			detFaultyChanged(detNo-1, 2);
		}
	}
}

