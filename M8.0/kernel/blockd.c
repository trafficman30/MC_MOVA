/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         blockd.c
*
*  TITLE:        Mova Kernel: Flow rate benefits/disbenefits
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
*	11-Jan-2013		7.0.5		..		AK		CRN021			Fix potential divide by zero error when LSMFLO is very small and YVALUE is large
*	31-Jan-2013		7.0.5		AB		AK		M7.0.5			Issue M7.0.5
*	09-May-2013		7.0.6		..		AK		CRN033			Remove unnecessary DivideError statement which may cause spurious errors in log
*	22-May-2013		7.0.6		AC		AK		M7.0.6			Issue M7.0.6
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
#include "gbltypes.h"
#include "errhand.h"
#include <diff_bss.h>


/*  IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: Function to check demands for 
    following stages to assess which links are 'relevant' when determining 
    green for the current stage. Defined in Block C. */
extern int CheckGreenInFollowingStages(	int nLinkNum, 
                                        int nNextStageNum, 
                                        Ccomshr const * Tcomshr );

void blockd(void)
{
	/* SIE_03, var sizes were 12 now mxlink */
	static int benex[mxlink];
	float pdelay,rTemp;
	static int gt[mxlink],t[mxlink],lqueue[mxlink],lrfact[mxlink],l,m,aa,maxbfr,e,b,j,i,g,f,k,c,a,nexsta,custa,ig,d,n,presta,h;
	float wf;
	extern int nPedWaitTime;/* for use instead of Tcomshr->stamax in ped waiting */

	for (l = 0; l < Tcomshr->nlinks; l++)
	{ /* /COMSHR/ */
		lrfact[l] = 100;
		Tcomshr->ben[l] = 0;
		Tcomshr->red[l] = 0;
		if (Tcomshr->benlin[l] == 1)
			Tcomshr->extrag[l] = 0;
	}

	for (m = 1; m <= Tcomshr->nlanes; m++)
	{
		Tcomshr->regtot[m-1] = 0;
	}

	/* initialise 'aa' to count from stopline end of register */
	aa = 1;

	maxbfr = 0;
	Tcomshr->benfit = 0;
	Tcomshr->disben = 0;
/*
 SESE        START LOOP FOR VARIOUS EXTENSIONS (E) OF GREEN
*/
	for (e = Tcomshr->minext; e <= Tcomshr->maxext; e += 2)
	{
		b = 0;
/*
 --- B SUMS BFR FOR THIS E AND ALL LINKS BENEFITTING FROM GREEN EXT
 SLSL
*/
		for (l = 0; l < Tcomshr->nlinks; l++)
		{
			if (Tcomshr->benlin[l] != 1)
				continue;
/*
 --- SKIP IF PED LINK OR NON-BENEFITTING LINK. ELSE IS RELEVANT LINK
*/
			benex[l] = 0;
			j = Tcomshr->llatot[l];
/*
 SMSM
*/
			for (i = 0; i < j; i++)
			{
				m = Tcomshr->llanes[l][i];
				g = e;
/*
 --- CONSTRAIN 'G' SO AS NOT TO COUNT BEYOND REMOTE DETECTOR
*/
				/* IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: Ignore -ve IN dets */
				if (Tcomshr->det[m-1][0] <= 0)
				{
					if (e > Tcomshr->crusx[m-1])
						g = Tcomshr->crusx[m-1] + 1;
				}
				else
				{
					if (e > Tcomshr->crusin[m-1])
						g = Tcomshr->crusin[m-1] + 1;
				}
/*
 --- JUMP IF NO IN-DET.  G IS UPPER LIMIT ON EXT
*/


				f = aa;
/*
 --- 'F' IS TEMPORARY POINTER IN CASE GAMBER > 0 ON FIRST PASS
*/
				if (f <= Tcomshr->gamber[m-1])
					f = Tcomshr->gamber[m-1] + 1;
/*
 --- IGNORE VEH'S NEARER THAN GAMBER+1 (0.5-SEC UNITS) TO STOP-LINE
*/
				if (f > g) goto S295;
/*
 --- JUMP TO 295 AVOIDS ZERO DIVIDE AT 290,
 --- IF GAMBER = MINEXT = E = 6, THEN F > G
 --- SKIP IF 'F' BEYOND LENGTH OF REGISTER; ELSE, SUM CONTENTS.
*/
				for (k = f; k <= g; k++)
				{
					/* sum up vehicles in lane register */
					Tcomshr->regtot[m-1] += (int)Tcomshr->shrega[m-1][k-1];
				}
            
				/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
				DivideError((int)(e-Tcomshr->gamber[m-1]), 501,
					"e was %d, gamber was %d, m was %d", e, Tcomshr->gamber[m-1], m);

				/*  BFR FOR THIS LANE IS 'C' (UNITS VEH/H/10 AS FOR SMFLOWS) */
				c = Tcomshr->regtot[m-1]*720/(e-Tcomshr->gamber[m-1]);

				/* LIMIT BFR TO MAX FEASIBLE FOR ONE LANE. B SUMS ALL LANES */
				if (c > 218)
					c = 218;
				b += c;
				/* LINK BFR FOR THIS EXTENSION SAVED (VEH/H/10) */
				benex[l] += c;

S295:
				if (aa <= 1)
				{
					/* SUM SPARE GREEN FOR LINK ON FIRST PASS (0.1-SEC UNITS) */
					a = Tcomshr->lingre[l] - Tcomshr->redcx[m-1] * Tcomshr->satinc[m-1];
					if (a > 0)
						Tcomshr->extrag[l] = Tcomshr->extrag[l] + a;
				}
				continue;
			}
/*
 EMEM
 --- CONVERT TOTAL SPARE TO MEAN SPARE GREEN/LANE ON FIRST PASS
*/
			if (aa == 1) DivideError((int)j, 502);
			if (aa == 1) Tcomshr->extrag[l] = Tcomshr->extrag[l]/j;

			continue;
		}

		if (b > maxbfr)
		{
			/* SAVE LINK BFR'S (COMPONENTS OF MAXBFR) */
			maxbfr = b;
			for (l = 0; l < Tcomshr->nlinks; l++)
			{
				Tcomshr->ben[l] = benex[l];
			}
		}

		/* UPDATE AA TO POINT TO NEXT SLOT IN SHREGA TO BE COUNTED */
		aa = e+1;
	}
/*
 EEEE        END OF EXT LOOP.
 //////////////////////////////////////////////////////////////////////
     SUBSECTION D2 : INITIALISE MODEL FOR ONE CYCLE PREDICTION
 --- DISBEN SUMS DISBEN VEHS. R SUMS FUTURE RED ESTIMATE FROM NOW
 --- VARIABLES LOCAL TO BLOCKD ARE:-
 --- PRESTA/CUSTA/NEXSTA ARE PREVIOUS/CURRENT/NEXT STAGES THRO CYCLE
 --- IG IS THIS INTERGREEN AS PREDICTION IS MADE THRO' CYCLE
*/
	nexsta = Tcomshr->snext;
	custa = Tcomshr->cusig;
	ig = Tcomshr->change[Tcomshr->cusig-1][Tcomshr->snext-1];

	/* Replace allowable but negative IG's (<-1) by positive value. */
	if (ig < -1)
		ig = -ig;

	Tcomshr->r = ig;
	g = 0;
	Tcomshr->cdelta = 100;

	for (l = 0; l < Tcomshr->nlinks; l++)
	{
		if (Tcomshr->ltype[l] != 0) goto S380;/* M 4.0*/
/*
 --- Jump if pedestrian or emerg/pr link                        ! M 4.0
*/
		if (Tcomshr->benlin[l] != 1) goto S335;
/*
 --- Jump unless traffic link benefitting from extension of CUSIG
*/
		lqueue[l] = 0;
		if (Tcomshr->lgreen[Tcomshr->snext-1][l] >= 2) 
			Tcomshr->red[l] = ig;
/*
 --- SOME BENEFITTING LINKS MAY HAVE ONLY AN IG AS RED.
*/
		goto S380;
S335:
		a = 0;
/*
 --- SUM VEHICLE ARRIVALS IN 'A' FOR ALL LANES ON LINK
*/
		j = Tcomshr->llatot[l];
		for (i = 0; i < j; i++)
		{
			m = Tcomshr->llanes[l][i];
			d = Tcomshr->det[m-1][0];
          
			/* IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: Ignore -ve IN dets */
			if (d <= 0) goto S340;          
			/*if(d == 0) goto S340;*/

			if (Tcomshr->susdet[d-1] > 0) goto S370;
/*
   --- SKIP IF NO IN-DET. JUMP IF DUD. ELSE SUM RED COUNTS
*/
			a = a + Tcomshr->redcin[m-1] + Tcomshr->inxtra[m-1];
			goto S350;
S340:
			d = Tcomshr->det[m-1][1];
			if (Tcomshr->susdet[d-1] > 0) goto S370;
/*
 ---   JUMP IF X-DET DUD. ELSE, USE X RED COUNTS
*/
			a += Tcomshr->redcx[m-1];
S350:
			continue;
		}
/*
 --- SCALE LINK QUEUE TO VEHICLES*3
*/
		lqueue[l] = a*3;
		goto S380;
S370:
/*
 --- IF DUD DETECTORS, SET LQUEUE FROM TIME SINCE END GREEN
*/
		/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error 
		 * and 504 put *before* 503 */
		DivideError((int)Tcomshr->lsmflo[l], 504,
			"link number was %d", ( l + 1 ) );

		/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
		DivideError((int)(1200/Tcomshr->lsmflo[l]), 503,
			"lsmflo was %d, link number was %d", Tcomshr->lsmflo[l], ( l + 1 ) );

		lqueue[l] = Tcomshr->cycltm[l]/(1200/Tcomshr->lsmflo[l]);
S380:
		gt[l] = 0;
		t[l] = 0;
/*
 --- TIME FUTURE START-GREENS FROM NOW USING 'T'
*/
		if (Tcomshr->benlin[l] == 2)
			gt[l] = Tcomshr->lingre[l];
/*
 --- LINKS OVERLAPPING GREEN NOW, HAVE PREVIOUS GREEN SET UP
*/
	}
/*
 ELEL        END INITIALISATION
 //////////////////////////////////////////////////////////////////////
     SUBSECTION D3 : ESTIMATE FUTURE RED + DISBENEFITTING FLOW RATE
              START MODEL
 SNSN
     UPDATE VARIABLES FOR START OF ANOTHER GREEN STAGE
*/
	for (n = 2; n <= Tcomshr->stages; n++)
	{
		presta = custa;
		custa = nexsta;

		/* JUMP TO PAY-OFF WHEN CYCLE COMPLETED */
		if (custa == Tcomshr->cusig)
			break;

		/* ELSE, FIND NEXSTA : SKIPPING UNDEMANDED STAGES IF POSSIBLE */

		/* skip to to the next demanded stage */
		for (;;)
		{
			nexsta += 1;
			if (nexsta > Tcomshr->stages)
				nexsta = 1;

			if (Tcomshr->stadem[nexsta-1] != 0)
				break;
		}

		/* Check whether it is possible to change from custa to nexsta */
		/* Work backwards to find an intermediate stage if not. */
		/* If no valid change possible, ignore the stage demand */
		for (;;)
		{
			if (Tcomshr->change[custa-1][nexsta-1] >= 0)
				break;

			nexsta -= 1;
			if (nexsta <= 0)
				nexsta = Tcomshr->stages;

			if (nexsta == custa)
				goto S570; /* No valid change */
		}


		for (l = 0; l < Tcomshr->nlinks; l++)
		{
			/* link red (or opposed red) last stage */
			if (Tcomshr->lgreen[presta-1][l] == 0 || Tcomshr->lgreen[presta-1][l] == 4)
			{
				gt[l] = 0;
				t[l] += g + ig;

				if (Tcomshr->lgreen[custa-1][l] != 0 && Tcomshr->lgreen[custa-1][l] != 4)
				{
					/* opposed green starting this stage */
					/* note link red factor for non-benefiting links */
					if (Tcomshr->benlin[l] != 1)
						lrfact[l] = Tcomshr->cdelta;
				}
			}

			/* link red (or opposed red) this stage, following green last stage */
			else if (Tcomshr->lgreen[custa-1][l] == 0 || Tcomshr->lgreen[custa-1][l] == 4)
			{
				gt[l] = 0;
				t[l] = ig;
			}

			/* link continuing green */
			else
			{
				gt[l] += g + ig;
				t[l] += g + ig;
			}

			continue;
		}
/*
 ELEL
 //////////////////////////////////////////////////////////////////////
     NOW FIND STAGE GREEN G FROM RELEVANT LINK GREENS SUBJECT TO MIN/MA
*/
		g = Tcomshr->min[custa-1];
		aa = 0;
/*
 --- 'AA' SAVES RED EXPANSION FACTOR FOR CRITICAL LINK ON  STAGE
 SLSL
*/
		for (l = 0; l < Tcomshr->nlinks; l++)
		{
			/* FIND WHICH LINKS POSSIBLY DETERMINE LENGTH OF CUSTA */

			/* skip any links red in custa */
			if (Tcomshr->lgreen[custa-1][l] == 0 || Tcomshr->lgreen[custa-1][l] == 4)
				continue;

			if (Tcomshr->lgreen[custa-1][l] == 1)
			{
				/* if this is the link's only green then the link is relevant */
			}

			else if (Tcomshr->lgreen[nexsta-1][l] == 0 || Tcomshr->lgreen[nexsta-1][l] == 4)
			{
				/* if link red next stage then it is relevant */
			}

			else
			{
				/* IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: should check whether 
				   (as stage demands stand) the link will have (extendable) green again immediately 
				   after the fixed green, as we do when determining benefitting and non-benefitting 
				   links in Block C. If it's green again after the fixed green we should jump
				   as the link (probably) won't determine the length of this stage. */
				a = Tcomshr->min[nexsta-1] + 5;
				if (a < Tcomshr->max[nexsta-1] || CheckGreenInFollowingStages(l, nexsta-1, Tcomshr) == 0)
					continue;
			}


			/* if this is a ped or e/p link */
			if (Tcomshr->ltype[l] != 0)
			{
				h = Tcomshr->lmin[l] - gt[l];
				if (g < h)
					g = h;
            
/* Now deal with pedestrian links only; compute pedestrian green, disbenefit, etc.
*/
				if (Tcomshr->ltype[l] > 0)
				{
					c = Tcomshr->cycltm[l] + t[l] + h;
/*
 ***    C is actual 100-per cent cycle for this link (0.1-Sec)
*/
					/* IRH MOD: M5.0.0: 31/08/04: New divide error 508 added */
					DivideError( ( Tcomshr->cycltm[l] / 10 ), 508,
						"cycltm was %d, link number was %d",
						Tcomshr->cycltm[l], (l+1) );

					a = c * 5 / (Tcomshr->cycltm[l] / 10);
/*
 ***    Alpha/2 is 50 for peds, =*5//10 above. This 50 is then factored
 ***    up by the ratio of: estimated full cycle to current red time;
 ***    A is probably 50<= A <200 for PUFFIN ped links.
 ***    Ped queue, set in BLOCKB, scaled x10, max 240 for PUFFIN links
 ***    Real variables TEMP, WF, PDELAY used to avoid overflow.
*/
					rTemp  = (float)((float)Tcomshr->totbon[l] * (float)a * (float)lrfact[l]);
					pdelay = (float)(rTemp*3.0E-5F);
         /*    printf("link=%d pedq=%d a=%d lrfact=%d rTemp=%e pdelay=%e\n",
                  l,pedq[l],a,lrfact[l],rTemp,pdelay);  */
/*
 ***    PDELAY is extra delay for ped link (scaled to ped*3)
 ***    Now, apply any ped-link delay-weighting set in STOPEN.
*/
					if (Tcomshr->stopen[l] >= 640)
					{
/*
 ***    THEN, calculate variable delay-weight if STOPEN = 640 or 650.
*/
/*              rTemp = (float)((float)Tcomshr->stamax/100.0F); Replaced by line below */
						rTemp = (float)((float)nPedWaitTime/20.0F);
						if(Tcomshr->stopen[l] == 650) rTemp += 1.0F;
/*
 ***       Increase weighting factor if STOPEN = 650. NB STAMAX in 0.1s
*/
						wf = (float)(1.0F + rTemp*rTemp);
              /* printf("link%d stamax=%d rTemp=%4.1f wf=%4.1f pdelay=%4.1f, ",
                     l,Tcomshr->stamax,rTemp,wf,pdelay);  */
						rTemp = (float)(pdelay * wf + 0.5F);
						Tcomshr->disben += (int)(rTemp);
              /* printf("disben=%d\n",Tcomshr->disben); */
					}
					else 
					{
/*
 ***    ELSE, is fixed delay-weight, if any, in data; apply to PDELAY
*/
						wf = (float)((float)Tcomshr->stopen[l]/10);
/*
 ***       Divide STOPEN by 10, as MOVASETUP stores as if 0.1s units
*/
						rTemp = (float)(pdelay * wf + 0.5F);
						Tcomshr->disben += (int)(rTemp);
					}
				}
			}

			/* else this is a traffic link */
			else
			{
				/* NOW FIND TIME TO CLEAR QUEUE ON RELEVANT TRAFFIC LINKS */

				/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error 
				 * and 506 put *before* 505 */
				DivideError((int)Tcomshr->lsmflo[l], 506,
					"link number was %d", (l+1) );

				/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
				DivideError((int)(1200/Tcomshr->lsmflo[l]), 505,
					"lsmflo was %d, link number was %d",
					Tcomshr->lsmflo[l], (l+1) );

				a = lqueue[l] + (t[l] - gt[l]) / (1200 / Tcomshr->lsmflo[l]);
/*
 --- FIRST, ADD ON ARRIVALS FOR PERIOD FROM 'NOW' TO START GREEN
 --- OR, IF DOUBLE GREEN, FOR PERIOD OF LAST RED
*/
				if (a > 327) goto S650;
/*
 --- JUMP TO CHANGE STAGE IF Q>100 VEH. ELSE SCALE Q TO VEH*300
*/
				lqueue[l] = a*100;
				f = 100-Tcomshr->yvalue[l];
          
				/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
				DivideError((int)f, 507,
					"yvalue was %d, link number was %d",
					Tcomshr->yvalue[l], (l+1) );

				d = lqueue[l]/f;
				if (d > 327) goto S650;
/*
 --- 'D' IS DELAYED VEHICLES*3. JUMP TO CHANGE IF TOO LARGE
*/
				/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
				DivideError((int)(Tcomshr->yvalue[l]), 509,
					"link number was %d", (l+1) );

				/* avoid divide by zero when lsmflo is smaller than yvalue */
				b = f * Tcomshr->lsmflo[l] / Tcomshr->yvalue[l];
				if (b == 0)
					k = lqueue[l] * 12;
				else
					k = lqueue[l] / b * 12;
/*
 --- CLEAR TIME (K IN 0.1-SEC UNITS).  ENSURE >= MIN GREEN
*/
				b = Tcomshr->lmin[l] - k;
				if (b < 0) 
					b = 0;
				h = k - gt[l] + b;
/*
 --- NOW REDUCE CLEAR TIME BY GREEN ALREADY HAD. 'H' MAY BE < 0
*/
				if (h <= g)
					goto S530;
/*
 --- JUMP IF LINK DOES NOT (SO FAR) DETERMINE STAGE GREEN
*/
				g = h;
				aa = lrfact[l];
/*
 --- RED FACTOR SCALED UP UNLESS GREEN FIXED BY MINIMUM
*/
				/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
				DivideError((int)(100-Tcomshr->yvalue[l]), 511,
					"link number was %d", (l+1) );

				if (b == 0)
					aa = aa * 100 / (100 - Tcomshr->yvalue[l]);
				if (aa > 300)
					goto S650;
S530:
/*
 --- JUMP TO CHANGE STAGE IF RED EXPANSION FACTOR IS EXCESSIVE
*/
				if (Tcomshr->benlin[l] != 0) goto S540;
/*
 --- JUMP IF NON-DISBENEFITTED LINK. ELSE SUM DISBENEFIT (VEH)
 --- 'D' IS DELAYED VEHICLES*3. 'A' IS (1-ALPHA/2) PER CENT
*/
				c = Tcomshr->cycltm[l] + t[l] + h;
/*
 --- 'C' IS ACTUAL 100-PER CENT CYCLE FOR THIS LINK (0.1-SEC)
*/
				/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
				DivideError((int)((c+Tcomshr->extrag[l])/10), 512,
					"c was %d, extrag was %d, link number was %d", 
					c, Tcomshr->extrag[l], (l+1) );

				a = 100 - c * 5 / ((c + Tcomshr->extrag[l]) / 10);
/*
 --- 'E' IS EXTRA DELAY FOR LINK ADDED TO DISBEN (VEH*3)
*/
				/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error 
				 * and testing 514 *before* 513 now */
				DivideError((int)lrfact[l], 514,
					"link number was %d", (l+1) );

				/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
				DivideError((int)(10000/lrfact[l]), 513,
					"lrfact was %d, link number was %d", lrfact[l], (l+1) );
          
				e = d * a / (10000 / lrfact[l]);
				if (e == 0 && Tcomshr->lindem[l] > 0) e = 3;
/*
 --- ALLOW FOR DEMAND INSERTED BY DUD-DETS. ASSUME 1 VEH*3
*/
				Tcomshr->ldben[l] = e;
				Tcomshr->disben += e;
			}
S540:
			t[l] = 0;
			gt[l] = 0;
			lqueue[l] = 0;

			continue;
		}
/*
 ELEL     ALL RELEVANT LINK GREENS DONE. MAX OF G'S IS STAGE GREEN
*/
		if (g > Tcomshr->max[custa-1]) g = Tcomshr->max[custa-1];
/*
 --- LIMIT G TO STAGE MAX
*/
		ig = Tcomshr->change[custa-1][nexsta-1];
		Tcomshr->r = Tcomshr->r + g + ig;
		if (aa > Tcomshr->cdelta) Tcomshr->cdelta = aa;
/*
 --- INCREASE CYCLE EXPANSION FACTOR TO THAT FOR LATEST STAGE
 SLSL     NOW SAVE RED TIME ETC. FOR BENEFITTING LINKS
*/
		for (l = 0; l < Tcomshr->nlinks; l++)
		{
			if (Tcomshr->benlin[l] != 1) goto S560;
/*
 ---   JUMP UNLESS traffic LINK BENEFITS FROM EXTENSION OF CUSIG
*/
			if (Tcomshr->lgreen[nexsta-1][l] == 0) goto S560;
/*
 ---   JUMP IF LINK RED IN NEXSTA
*/
			if (Tcomshr->lgreen[nexsta-1][l] == 4 && nexsta != Tcomshr->cusig) 
				goto S560;
/*
 ---   JUMP IF OPPOSED GREEN NEXT UNLESS CYCLE IS ENDING
*/
			/*  IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: Check that link 
				isn't extendable green again *after* the fixed time 
				stage given current stage demands. 
				Also added + 5 correction as is done elsewhere. */
			if (Tcomshr->max[nexsta-1] <= (Tcomshr->min[nexsta-1] + 5)
				&& CheckGreenInFollowingStages(l, nexsta-1, Tcomshr) == 1)
			{
				goto S560;
			}
        /*if(Tcomshr->max[nexsta-1] == Tcomshr->min[nexsta-1]) goto S560;*/
/*
 ---   JUMP IF FIXED-TIME GREEN IN NEXSTA; TREATED AS RED
*/
			if (Tcomshr->red[l] > 0) goto S560;
/*
 ---   JUMP IF THIS IS SECOND START-GREEN IN CYCLE; ELSE,
*/
			Tcomshr->red[l] = Tcomshr->r;
/*
 ---   FOR BENEFITTING LINKS SAVE TIME TILL NEXT EXTENDABLE GREEN
*/
			lrfact[l] = Tcomshr->cdelta;
S560:
/*
 ---   SAVE SHIFT FOR START OF THIS LINK'S NEXT GREEN AFTER RED
*/
			continue;
		}
S570:
/*
 ELEL
*/
		continue;
	}

/*
 ENEN        END STAGES LOOP
 //////////////////////////////////////////////////////////////////////
     SUBSECTION D4 : COMPLETE BENEFIT CALCULATIONS.  DO PAY-OFF .
 --- NOTE : BEN(VEH/H/10),RED(0.1-SEC),DISBEN(VEH)
*/
	for (l = 0; l < Tcomshr->nlinks; l++)
	{
		if (Tcomshr->benlin[l] != 1) goto S630;
		a = Tcomshr->red[l] + 10;
		if (Tcomshr->red[l] > 1300) goto S650;
/*
 --- ALLOW FOR ZERO OR HUGE RED CAUSING RANGE PROBLEMS
*/
		/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
		DivideError((int)((a+Tcomshr->extrag[l]+5)/10), 515,
			"a was %d, extrag was %d, link number was %d",
			a, Tcomshr->extrag[l], ( l + 1 ) );

		aa = 5 * a / ((a + Tcomshr->extrag[l] + 5) / 10);
/*
 --- 'AA' IS ALPHA/2 PER CENT. 'F' IS (1-Y)/(1-AA), SCALED*30
*/
      
		/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error 
		 * and 517 put *before* 516 */
		DivideError((int)lrfact[l], 517,
			"link number was %d", ( l + 1 ) );
      
		/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
		DivideError((int)(5000/lrfact[l]), 516,
			"lrfact was %d, link number was %d",
			lrfact[l], ( l + 1 ) );
      
		d = Tcomshr->lsmflo[l]*50/(5000/lrfact[l]);
/*
 --- 'D' IS DISBEN FLOW RATE (VEH/H/10) DUE TO SHIFT OF NEXT CYCLE
*/
		DivideError((int)(100-aa), 518);
		f = (100-Tcomshr->yvalue[l])*30/(100-aa);
/*
 --- 'B' (VEH/H/10) IS NET BENEFITTING FLOW RATE. CAN BE < 0
*/
		/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
		DivideError((int)f, 519,
			"yvalue was %d, aa was %d, red was %d, extrag was %d, link number was %d",
			Tcomshr->yvalue[l], aa, Tcomshr->red[l], Tcomshr->extrag[l], ( l + 1 ) );

		b = (Tcomshr->ben[l] - d) * 30 / f + Tcomshr->ben[l] * aa / 100;
		Tcomshr->netbfr[l] = b;
		if (b == 0) goto S610;
      
		/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error 
		 * and 521 moved *before* 520 */
		DivideError((int)b, 521,
			"ben was %d, d was %d, f was %d, aa was %d, link number was %d",
			Tcomshr->ben[l], d, f, aa, ( l + 1 ) );
      
		/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
		DivideError((int)(9000/b), 520,
			"ben was %d, d was %d, f was %d, aa was %d, link number was %d",
			Tcomshr->ben[l], d, f, aa, ( l + 1 ) );
      
		Tcomshr->benfit = Tcomshr->benfit + 25 * Tcomshr->red[l]/(9000/b);
S610:
		if (Tcomshr->ben[l] == 0) goto S630;
      
		/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error 
		 * and 523 moved *before* 522 */
		DivideError((int)Tcomshr->ben[l], 523,
			"link number was %d", ( l + 1 ) );
      
		/* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
		DivideError((int)(18000/Tcomshr->ben[l]), 522,
			"link number was %d", ( l + 1 ) );
      
		Tcomshr->benfit += 50 * Tcomshr->stopen[l] / (18000/Tcomshr->ben[l]);
S630:
		continue;
	}
	Tcomshr->benfit = Tcomshr->benfit / 10;
	Tcomshr->disben = Tcomshr->disben / 3;
/*
 --- CONVERT BENEFIT AND DISBENEFIT TO UNITS OF VEHICLES
 --- PAY-OFF
*/
	if (Tcomshr->benfit > Tcomshr->disben) goto S700;
S650:
/*
 --- JUMP IF STAGE HELD. ELSE, SET MARKER TO 6 TO CHANGE STAGE
*/
	Tcomshr->marker = 6;
S700:
/*
 **********************************************************************
*/
	return;
}
