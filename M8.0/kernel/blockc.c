
/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         blockc.c
*
*  TITLE:        Mova Kernel: End-Sat and Oversaturation decisions
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	15-Apr-1993		4.0.0		..		RV		......			Emergency/priority vehicle facilities added
*	19-Jan-1994		4.0.0		..		RV		......			'Big' Extended stages, links, etc
*	26-Jan-1998		4.0.0		..		MC		SIE_00			First undergoing system test
*	03-Mar-1998		4.0.0		..		PC		SIE_01			Overlay on 'Xcomshr', etc., replaced by explicit use of structures.
*	29-Sep-2004		5.0.0		..		IH		SIE_02			CheckGreenInFollowingStages() called to check demands for following stages to assess
*																which links are 'relevant' when determining green for the current stage.
*																Use of CMOVA_EP #define to enable/disable emergency/priority code 	
*	31-May-2011		7.0.0		AA		PK		CRN003			Several Changes for MOVA 7 release
*	09-May-2013		7.0.6		..		AK		CRN035			Reset BENLIN for all links prior to recalculation
*	22-May-2013		7.0.6		AB		AK		M7.0.6			Issue M7.0.6
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
    green for the current stage. */
int CheckGreenInFollowingStages(	int nLinkNum, 
                                    int nNextStageNum, 
                                    Ccomshr const * Tcomshr );

void blockc(void)
{
static int l,j,i,m,d,n,b,g,a,c,e,aa,f,h,k,cjl;
/*  IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: For determining the 
    long lane to use for the upstream arrival efficiency check at S395 */
static int nLinkMOVQIN, nLinkMOVQINLaneNum; 

    for(l=0; l<Tcomshr->nlinks; l++) {/* /COMSHR/ */
          if(Tcomshr->ltype[l] != 0) goto S400;
/*
 --- Skip if ped/emerg/priority link, (emerg/pr LTYPE<0 added M4.0)
 --- or, if signal red, or link already end-sat
*/
          if(Tcomshr->lgrefl[l] == 0 || Tcomshr->liensa[l] > 0) goto S400;
          if(Tcomshr->mingre[l] == 0 || Tcomshr->mingre[l] > Tcomshr->lingre[l]) 
            goto S400;
/*
 --- SKIP LINK IF MIN-GREEN NOT YET SET OR EXPIRED.
*/
          if(Tcomshr->lingre[l] < Tcomshr->eslmax[l]) goto S10;
/*
 --- JUMP IF LINK E.S.-MAX-GREEN NOT REACHED. ELSE, FLAG E.S.
*/
          Tcomshr->liensa[l] = 7;
          goto S400;
S10:
          if(Tcomshr->lidetf[l] > Tcomshr->nmainl[l]) goto S150;
/*
 --- JUMP TO SUBSECTION C3 TO CHECK E.S. FOR LINK WITH MANY U/S DETS
*/
          if(Tcomshr->linkos[l] > 0 && Tcomshr->lidetf[l] == 0) goto S250;
/*
 --- JUMP TO SUBSECTION C4 TO CHECK E.S. FOR O'SAT LINK WITH OK DETS
 //////////////////////////////////////////////////////////////////////
     SUBSECTION C2 : CHECK LINK FOR 'NORMAL' END-SATURATION
                      THIS INCLUDES 'VARIMIN-TYPE' LANE CALCULATIONS
 --- LAENSA SET AS FOLLOWS:
 --- 9 IF ES SINCE TOO MANY U/S DETS
 --- 8 IF ES SINCE IRRELEVANT LANE I.E. NOT A MAIN LANE
 --- 3 IF ES SINCE Q OVER X-DET
 --- 2 IF ES SINCE VARIMIN GREEN <=LINK GREEN
 --- 1 IF ES NORMALLY
 --- 0 NOT ES
 ---<0 PROVISIONALLY ES
*/
          j = Tcomshr->llatot[l];
/*
 SMSM            CONSIDER ALL LANES (NO. 'M') ON LINK L
*/
          for(i=0; i<j; i++) {
              m = Tcomshr->llanes[l][i];
              if(Tcomshr->laensa[m-1] >= 8) goto S140;
              if(Tcomshr->laensa[m-1] == -1) goto S100;
/*
 --- SKIP IRREL OR DUD-DET LANE (=8/9) DO OTHER CHECK IF PROV-ES
*/
              if(Tcomshr->laensa[m-1] > 0) goto S130;
/*
 --- JUMP IF LANE ALREADY END-SAT TO CHECK IF LINK E.S.
*/
              /*    IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: 
                    do not assume a long lane has an IN-detector */
              if( IS_LONG_LANE( m-1 ) || Tcomshr->shortl[l] > 0) goto S30;              
              /*if(Tcomshr->det[m-1][0] > 0 || Tcomshr->shortl[l] > 0) goto S30;*/
/*
 --- JUMP TO CHECK LONG LANES OR SHORT-LINK LANES I.E. MAIN LANES
*/
              if(Tcomshr->lingre[l] < Tcomshr->comtim[m-1] && Tcomshr->ladetf[m-1]
                == 0) goto S60;
/*
 --- JUMP TO CHECK SHORT LANE ON LONG LINK BEFORE COMTIM
 --- ELSE, FLAG IRRELEVANT SHORT LANE AND SKIP TO NEXT LANE
*/
              Tcomshr->laensa[m-1] = 8;
              goto S140;
S30:
              if(Tcomshr->ladetf[m-1] <= 0) goto S60;
/*
 --- JUMP IF LANE X-DET OR ALTXDN OK
*/
              if(Tcomshr->ladetf[m-1] == 1) goto S40;
/*
 --- JUMP TO DO VARIMIN TYPE GREEN CHECK, SINCE ONLY IN-DET OK
 --- ELSE, FLAG E.S. FOR LANE WITH ALL DETS U/S
*/
              Tcomshr->laensa[m-1] = 9;
              goto S140;
S40:
/*
 --- NOW DO VARIMIN TYPE E.S. CHECK FOR LANE USING IN-DET
*/
              d = Tcomshr->det[m-1][6];
              n = Tcomshr->redcin[m-1];
              if(Tcomshr->laosat[m-1] > 0) n = n+1;
/*
 --- ALLOW FOR ANOTHER VEHICLE IF LANE IS O'SAT
*/
              b = 0;
              if(Tcomshr->lingre[l] >= Tcomshr->movqin[m-1]) b = 3*Tcomshr->
                crusin[m-1];
/*
 --- ALLOW FOR FREE-FLOWING VEHICLES AFTER MOVQIN TIME (IN 0.1 SEC)
*/
              g = Tcomshr->stlost[m-1]+n*Tcomshr->satinc[m-1]-b;
              a = Tcomshr->exitrc[m-1]+Tcomshr->sinkrc[m-1];
              if(a > 0) goto S50;
/*
 --- JUMP IF LANE NEEDS 2-PART ES CHECK. ELSE, CHECK NORMAL LANE
*/
              if(g >= Tcomshr->lingre[l]) goto S140;
/*
 --- JUMP TO END IF NEED MORE GREEN. ELSE NOTE LANE END-SAT
*/
              Tcomshr->laensa[m-1] = 2;
              goto S130;
S50:
/*
 --- JUMP FOR NORMAL LANE. NOW CHECK LANE NEEDING 2-PART CHECK
*/
              if(g > Tcomshr->lingre[l]) goto S100;
/*
 --- JUMP TO PART 2 IF NEEDS MORE GREEN. ELSE, JUMP TO FLAG PROV ES
*/
              goto S90;
S60:
/*
 --- NOW DO NORMAL GAP CHECKING FOR E.S.
*/
              d = Tcomshr->det[m-1][1];
              if(Tcomshr->susdet[d-1] > 0) d = Tcomshr->det[m-1][7];
              c = Tcomshr->det[m-1][4];
              if(c > 0 && Tcomshr->susdet[c-1] == 0 && Tcomshr->lingre[l] >= 
                Tcomshr->comtim[m-1]) d = c;
/*
 --- USE COMBX-DET IF EXISTS AND LINK HAS BEEN GREEN LONG ENOUGH
 --- X-DET OR ALT OK. CHECK FOR QUEUE OVER DET
*/
              if(Tcomshr->lingre[l] < Tcomshr->moveqx[m-1] || Tcomshr->timon[d-1] 
                < Tcomshr->qminon[m-1]) goto S70;
/*
 --- JUMP IF Q-CHECK FAILS. ELSE, FLAG Q CAUSING E.S.
*/
              Tcomshr->laensa[m-1] = 3;
              Tcomshr->liensa[l] = 3;
/*
 --- FLAG LINK E.S. BY Q-BACK AND SKIP TO NEXT LINK
*/
              goto S400;
S70:
              if(Tcomshr->timon[d-1] >= Tcomshr->scan) goto S140;
/*
 --- SKIP GAP-CHECKS IF DET 'ON' NO LESS THAN SCAN INTERVAL
*/
              e = 0;
              if(Tcomshr->laosmx > 1 && Tcomshr->linkos[l] == 0) e = -Tcomshr->
                subgap;
/*
 --- REDUCE CRITICAL GAPS IF SIGNIFICANT OVERSAT BUT NOT THIS LINK
*/
              aa = Tcomshr->nmainl[l];
              if(Tcomshr->lingre[l] < Tcomshr->comtim[m-1]) aa = Tcomshr->llatot[
                l];
              if(aa > 1) e = e+Tcomshr->addgap;
/*
 --- increase critical gaps if two main lanes on link
*/
              if(Tcomshr->timoff[d-1] >= Tcomshr->critg[m-1]+e) goto S80;
/*
 --- jump to flag E.S. if gap >= single critical gap
 --- 30 changed to 40  following 1 sec taken from STLOST  7/6/89  M 2.5
*/
              if(Tcomshr->lingre[l] < Tcomshr->mingre[l]+40) goto S75;
/*
 --- jump : don't use OLDGAP for checks just after min-green
*/
              if(Tcomshr->timoff[d-1] < 3 && Tcomshr->oldgap[d-1] >= Tcomshr->
                critg[m-1]+e) goto S80;
/*
 --- jump to flag E.S. if gap ended since last scan >= critical
*/
              if(aa > 2) goto S75;
/*
 --- SKIP DOUBLE-GAP CHECK ON 3-MAIN-LANE-LINKS. THEN, WITH ADDGAP,
 --- PROBABILITY OF GAP-OUT SIMILAR FOR 1,2,3 LANE LINKS
*/
              if(Tcomshr->timoff[d-1]+Tcomshr->oldgap[d-1] >= Tcomshr->satgap[m-1]
                +Tcomshr->critg[m-1]+e) goto S80;
S75:
/*
 --- JUMP TO FLAG ES IF DOUBLE GAP >= CRITICAL
*/
              if(Tcomshr->ladetf[m-1] < 0) goto S100;
/*
 --- JUMP IF 2-PART ES CHECK NEEDED. ELSE, FINISH
*/
              goto S140;
S80:
              if(Tcomshr->ladetf[m-1] < 0) goto S90;
/*
 --- JUMP IF  2-PART CHECK NEEDED. ELSE, FLAG NORMAL LANE ES
*/
              Tcomshr->laensa[m-1] = 1;
              goto S130;
S90:
              Tcomshr->laensa[m-1] = Tcomshr->laensa[m-1]-1;
S100:
/*
 --- NOTE PART 1 OF 2-PART CHECK DONE (PROV ES)
 --- NOW DO PART 2 OF 2-PART CHECK UNLESS ALREADY DONE
*/
              if(Tcomshr->laensa[m-1] <= -2) goto S120;
              d = Tcomshr->det[m-1][0];

              /* IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: Ignore -ve IN dets */
              if(d <= 0 || Tcomshr->susdet[d-1] > 0) goto S110;
              /*if(d == 0 || Tcomshr->susdet[d-1] > 0) goto S110;*/
/*
 --- JUMP IF CAN'T DO PART 2 CHECK.ELSE, CHECK CRITICAL GAPS
*/
              if(Tcomshr->timoff[d-1] > Tcomshr->critg[m-1] || Tcomshr->oldgap[d
                -1] > Tcomshr->critg[m-1]) goto S110;
              if(Tcomshr->timon[d-1] >= Tcomshr->qminon[m-1] && Tcomshr->lingre[l]
                >= Tcomshr->movqin[m-1]) goto S110;
              goto S140;
S110:
/*
 --- FINALLY JUMP IF PART 2 CHECK FAILS. ELSE, NOTE PROV ES
*/
              Tcomshr->laensa[m-1] = Tcomshr->laensa[m-1]-2;
S120:
              if(Tcomshr->laensa[m-1] >= -2) goto S140;
/*
 --- JUMP IF BOTH PARTS NOT COMPLETE. ELSE, FLAG ES=4
*/
              Tcomshr->laensa[m-1] = 4;
S130:
              if(Tcomshr->linkos[l] > 0 && Tcomshr->laosat[m-1] == 0 && Tcomshr->
                lidetf[l] < 2) goto S140;
/*
 --- MUST NOT SKIP IF LIDETF>=2, AS O'SAT LANE MAY HAVE LAENSA=9
 --- JUMP IF LINK O'SAT AND THIS LANE IS NOT O'SAT.DON'T FLAG LINK O'SA
 --- ELSE, FLAG LINK E.S. AND SKIP TO NEXT LINK
*/
              Tcomshr->liensa[l] = 1;
              goto S400;
S140:
/*
 --- NOTE : LINK CAN E.S. IF ANY MAIN LANE IS E.S. WITH NO O'SAT ON LIN
 ---       OR, IF ANY O'SAT MAIN LANE IS E.S. WHEN LINK IS O'SAT
*/
              continue;
          }
/*
 EMEM
*/
          goto S400;
S150:
/*
 //////////////////////////////////////////////////////////////////////
     SUBSECTION C3 : CHECK E.S. FOR LINK WITH MOST DETS U/S.
                      CALCULATE GREEN NEEDED FROM SMOOTHED FLOWS.
*/
          d = 0;
          e = 0;
          f = 1;
/*
 --- NOTE: F HAS INITIAL VALUE 1 TO AVOID DIVIDE BY ZERO PROBLEMS
 --- ACCUMULATE : D=STLOST'S,  E=SATINC'S,  F=SMFLOW'S.
*/
          j = Tcomshr->llatot[l];
          for(i=0; i<j; i++) {
              m = Tcomshr->llanes[l][i];

              /*    IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: 
                    do not assume absence of IN-detector means this isn't a long lane */
              if( !IS_LONG_LANE( m-1 ) && Tcomshr->shortl[l] == 0) goto S190;
              /*if(Tcomshr->det[m-1][0] == 0 && Tcomshr->shortl[l] == 0) goto S190;*/
/*
 ---         SKIP SHORT LANES UNLESS 'SHORT LINK'
*/
              d = d+Tcomshr->stlost[m-1];
              e = e+Tcomshr->satinc[m-1];
              f = f+Tcomshr->smflow[m-1];
S190:
              continue;
          }
          
          /* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
          DivideError((int)f, 401,
              "smflow[0] was %d, smflow[1] was %d, smflow[2] was %d, "
              "smflow[3] was %d, smflow[4] was %d, smflow[5] was %d",
              Tcomshr->smflow[0], Tcomshr->smflow[1], Tcomshr->smflow[2],
              Tcomshr->smflow[3], Tcomshr->smflow[4], Tcomshr->smflow[5] );

          /* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
          DivideError((int)(3600/f), 402,
              "smflow[0] was %d, smflow[1] was %d, smflow[2] was %d, "
              "smflow[3] was %d, smflow[4] was %d, smflow[5] was %d",
              Tcomshr->smflow[0], Tcomshr->smflow[1], Tcomshr->smflow[2],
              Tcomshr->smflow[3], Tcomshr->smflow[4], Tcomshr->smflow[5] );

          f = Tcomshr->cycltm[l]/(3600/f);
/*
 --- CONVERT SMFLOWS VEH/H/10 TO VEH THIS CYCLE.
V1.1!IF((F-D).LE.0)GO TO 200 not needed since NOVEMI replaced by STLOST
V1.1!JUMP TO FLAG E.S. IF MIN-GREEN IS ENOUGH.  - also removed -.
*/
          /* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
          DivideError((int)Tcomshr->nmainl[l], 403,
              "link number was %d",
              ( l + 1 ) );

          g = (d+f*e/Tcomshr->nmainl[l])/Tcomshr->nmainl[l];
          if(Tcomshr->linkos[l] > 0) g = g+75;
/*
 --- REQUIRED GREEN IS G IN 0.1 SECONDS . EXTRA GIVEN TO O'SAT LINK
*/
          if(Tcomshr->lingre[l] < g) goto S400;
/*
 --- JUMP IF INSUFFICIENT GREEN YET. ELSE, FLAG E.S.
*/
          Tcomshr->liensa[l] = 8;
          goto S400;
S250:
/*
 //////////////////////////////////////////////////////////////////////
     SUBSECTION C4 : CHECK E.S. FOR OVERSATURATED LINKS
*/
          c = 0;
          e = 0;
          f = 0;
          h = 0;
/*
 --- ACCUMULATE : C=SAT-FLOW HEADWAY(S) FOR LANE(S) ON LINK
 --- E=EXCESS GAPS ON LANE(S)  F=REGISTER VEH  H=REGISTER TIME(S)
*/
          aa = 0;
/*
 --- 'AA' USED TO MARK LINKS WITH SUPERSATURATION ON SOME LANE(S)
*/
          j = Tcomshr->llatot[l];
/*
 SMSM
*/
          /* IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: zero link MOVQIN variables */
          nLinkMOVQINLaneNum = 0;
          nLinkMOVQIN = 0;

          for(k=1; k<=j; k++) {
              m = Tcomshr->llanes[l][k-1];
              if(Tcomshr->laosat[m-1] > 1) aa = 1;
              
              /*    IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: set MOVQIN lane num 
                    for the link to the maximum of the long lane MOVQINs.
                    NOTE: We choose the max MOVQIN of long lanes so we don't 
                    risk ending sat flow as a result of upstream inefficiency until 
                    *all* long lanes on the link have had a chance to clear back 
                    to IN-detectors. */
              if (	Tcomshr->det[m-1][0] > 0 
                    && Tcomshr->movqin[m-1] > nLinkMOVQIN )
              {
                  nLinkMOVQIN = Tcomshr->movqin[m-1];
                  nLinkMOVQINLaneNum = m;
              }

              /* IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: 
                 do not assume IN-detector means long lane */
              if( !IS_LONG_LANE( m-1 ) && Tcomshr->shortl[l] == 0) goto S350;
              /*if(Tcomshr->det[m-1][0] == 0 && Tcomshr->shortl[l] == 0) goto S350;*/
/*
 --- SKIP SHORT LANES UNLESS 'SHORT LINK'
*/
              d = Tcomshr->det[m-1][1];
              if(Tcomshr->susdet[d-1] > 0) d = Tcomshr->det[m-1][7];
              if(d == 0) goto S150;  /* M 2.0 25-1-89*/
/*
 --- jump if C4 entry with dud X-Det & no ALT-DWN
 --- X-Det also IN-SINK and set suspect on other lane
*/
              if(Tcomshr->det[m-1][4] > 0 && Tcomshr->lingre[l] >= Tcomshr->
                comtim[m-1]) d = Tcomshr->det[m-1][4];
/*
 --- USE DOWNSTREAM ALT-DET IF X-DET U/S, OR COMB-DET IF EXISTS
*/
              g = Tcomshr->timoff[d-1];
              a = Tcomshr->timon[d-1];
              if(a <= 25) goto S270;
/*
 --- JUMP UNLESS DETECTOR 'ON' LONGER THAN FOR SLOW LORRY
*/
              if(Tcomshr->lingre[l] >= Tcomshr->moveqx[m-1]) Tcomshr->lwaste[l] = 
                Tcomshr->lwaste[l]+5;
/*
 --- ADD EXCESSIVE 'ON' TIME TO WASTE-TIME. THIS SAVES E.S.=3 TEST
*/
              goto S300;
S270:
/*
 --- THIS SECTION OF CODE DEPENDS ON 'MINON' VALUE (IN DETSCN)
*/
              if(a > 0) goto S280;
/*
 --- JUMP IF DET 'ON' OR 'OFF'=0.1 SEC.   ELSE, IS 'OFF' FULLY
*/
              if(g > Tcomshr->scan+1) goto S290;
/*
 --- ADJUST WASTE-TIMER FOR MEAN GAP AT START GAP. JUMP IF DONE
*/
              Tcomshr->lwaste[l] = Tcomshr->lwaste[l]-Tcomshr->satgap[m-1]+g;
              goto S300;
S280:
              if(a >= Tcomshr->scan) goto S300;
/*
 --- JUMP IF 'ON' ENOUGH FOR NO WASTE SINCE LAST SCAN
*/
              b = a+Tcomshr->oldgap[d-1]-(Tcomshr->scan+1);
              if(b > 0) goto S290;
/*
 --- JUMP UNLESS NEED TO ALLOW FOR SCAN-JUMPING OF SHORT GAP
*/
              Tcomshr->lwaste[l] = Tcomshr->lwaste[l]-Tcomshr->satgap[m-1]+
                Tcomshr->oldgap[d-1];
              goto S300;
S290:
              Tcomshr->lwaste[l] = Tcomshr->lwaste[l]+(Tcomshr->scan-a);
S300:
/*
 --- ALLOW FOR (RESIDUAL) GAP SINCE LAST SCAN
 --- ADD EXCESS GAP FOR LANE (CAN BE <0) TO TOTAL FOR LINK IN 'E'
*/
              if(Tcomshr->oldgap[d-1] > g && Tcomshr->mingre[l]+40 <= Tcomshr->
                lingre[l]) g = Tcomshr->oldgap[d-1];
/*
 --- FIND EXCESS GAP FROM LARGER OF PRESENT AND PREVIOUS GAPS
*/
              e = e+(g-Tcomshr->critg[m-1]);
              c = c+Tcomshr->satinc[m-1];
/*
 --- SAVE SAT-FLOW HEADWAY(S) FOR LANE(S) ON LINK
 --- NOW, COUNT VEHICLES IN THIS LANE'S REGISTER INTO 'F'
*/
              /*    IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: 
                    long links might not have IN-dets
                    in CMOVA - we must check for IN-det too */
              if(Tcomshr->shortl[l] > 0 /* added -> */ || Tcomshr->det[m-1][0] <= 0) goto S350;              
              /*if(Tcomshr->shortl[l] > 0) goto S350;*/
/*
 --- SKIP THIS CHECK IF 'SHORT LINK'
*/
              /*    IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA:
                    Removed superfluous code - d set but not used */
              /*d = Tcomshr->det[m-1][0];*/
/*
     IF(SUSDET(D).GT.0)F=F+99
 --- IF IN-DET U/S SET HUGE VEH COUNT TO STOP THIS CHECK CAUSING ES
*/
              a = Tcomshr->crusx[m-1]+1;
/*
 --- ONLY USE UPPER REG. IN CASE ASSOC-DETS OR SINKS
*/
              b = Tcomshr->crusin[m-1]+1;
/*
 --- NOTE: LONG-ON IN-DET WON'T PUT COUNT IN REGISTER. (SEE P2)
*/
              for(i=a-1; i<b; i++) {
                    f = f+(int)Tcomshr->shrega[m-1][i];
              }
              h = h+(b+1-a);
S350:
/*
 --- SAVE REGISTER TIME(S) FOR LANES ON LINK
*/
              continue;
          }
/*
 EMEM
 --- NOW CALCULATE PERCENTAGE OF LINK SAT-FLOW ACCEPTABLE, 'A'.
*/
          g = Tcomshr->lingre[l];
          d = Tcomshr->redlt[l];
          /*if (Tcomshr->redlt[l-1] < 0)
            Tcomshr->redlt[l-1] = 1;*/
          if(g < Tcomshr->bontim[l] || Tcomshr->bontim[l] == 0) goto S360;
/*
 --- EFFECTIVE-GREEN INCLUDES BONUS ONLY AFTER BONUS-CALC-TIME
*/
          g = g+Tcomshr->bonusg[l];

          /* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
          DivideError((int)(Tcomshr->lostim[l]), 404,
              "link number was %d",
              ( l + 1 ) );

          d = (Tcomshr->lostim[l]-Tcomshr->bonusg[l])*50/Tcomshr->lostim[l]*
            Tcomshr->redlt[l]/50;
/*
 --- CORRECT REDLT FOR BONUS. RESULTS IN SMALLER VALUE
*/
          if(d < 0) d = 0;
S360:
          /* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
          DivideError(Tcomshr->nmainl[l], 405,
              "link number was %d",
              ( l + 1 ) );

          b = Tcomshr->lwaste[l]/Tcomshr->nmainl[l];
/*
 --- ADJUST WASTE-TIME TO VALUE PER MAIN LANE
*/
          cjl = g-b;
          if(cjl > 3276) cjl = 3276; /* ?? fix ?? !*/

          /* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
          DivideError((int)((g+d+5)/10), 406,
              "g was %d, d was %d",
              g, d );

          a = cjl*10/((g+d+5)/10);
/*
 --- ROUNDING ERROR REDUCED BY +5 ABOVE
*/
          if(a > 99) a = 99;
          if(a >= Tcomshr->maxpcs[l]) goto S370;
/*
 --- JUMP UNLESS 'A' IS DECREASING, WHEN NEED TO CHECK FALL-OFF
*/
          /* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
          DivideError((int)(g+d), 407,
              "g was %d, d was %d",
              g, d );

          d = 3200/(g+d);
/*
 --- 'D' IS THE PERCENTAGE TOLERANCE ON 'A' TO NOISE
*/
          if(Tcomshr->maxpcs[l]-a <= d) goto S380;
/*
 --- JUMP IF FALL-OFF IN 'A' IS WITHIN TOLERANCE. ELSE, FLAG E.S.
*/
          Tcomshr->liensa[l] = 4;
          goto S400;
S370:
          Tcomshr->maxpcs[l] = a;
S380:
          if(a < Tcomshr->minpcs[l]) a = Tcomshr->minpcs[l];
/*
 --- NOW CHECK SHORT-TERM LINK EFFICIENCY WITH TEST EFF. 'A'
*/
          /*    IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: 
                Set m to the lane with the greatest MOVQIN in the link 
                (may be zero if this is a short link 
                or long link without IN-dets) */
          m = nLinkMOVQINLaneNum;
          
          e = e+(Tcomshr->nmainl[l]-1)*10;
/*
 --- CORRECT MULTILANE LINKS FOR SOME INDEPENDENCE BETWEEN LANES
*/
          if(c+e <= 0) goto S390;

          /* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
          DivideError((int)(c+e), 408,
              "c was %d, e was %d",
              c, e );

          e = c*100/(c+e);
          if(e > a) goto S390;
/*
 --- JUMP IF EFF HIGH ENOUGH. ELSE, FLAG SHORT-LINK E.S.
*/
          /*    IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: 
                Lane 'm' was not set correctly for S395 - it should be set 
                to the long lane with the greatest MOVQIN in the link.
                Usually, MOVQIN will be the same for all long lanes 
                in a link, but not necessarily. 
                N.B. m is only set for long lanes with IN-dets. */
          if(aa > 0 && m > 0 ) goto S395;
          /*if(aa > 0 && Tcomshr->shortl[l] == 0) goto S395;*/
/*
 --- JUMP TO UPPER REGISTER CHECK IF LONG LINK IS SUPERSATURATED
*/
          Tcomshr->liensa[l] = 5;
          goto S400;
S390:
/*
 --- CONTINUE HERE ONLY IF LINK PASSES SHORT-TERM EFF. TEST
*/
          /*    IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: 
                Use correct lane 'm', ensuring there's an IN-det too. */
          if( aa > 0 || m == 0 ) goto S400;
          /*if(aa > 0 || Tcomshr->shortl[l] > 0) goto S400;*/
S395:
/*
 --- COMPLETE REGISTER CHECK FOR LONG LINKS. CONVERT 'H' TO VEH.
*/
          if(Tcomshr->lingre[l] < Tcomshr->movqin[m-1]+50) goto S400;
/*
 --- ENTER HERE IF LONG LINK FAILS EFF TEST BUT IS SUPERSATURATED
 --- SKIP CHECK  UNTIL TRAFFIC MOVING WELL ON LONG LINK
*/
          b = h-3-Tcomshr->nmainl[l];
/*
 --- ALLOW  2.0/2.5/3.0-SEC TOLERANCE  FOR 1,2,3 LANES
*/
          /* IRH MOD: M5.0.0: 31/08/04: Further info added for divide error */
          DivideError((int)(c*20), 409,
              "satinc[0] was %d, satinc[1] was %d, satinc[2] was %d, "
              "satinc[3] was %d, satinc[4] was %d, satinc[5] was %d",
              Tcomshr->satinc[0], Tcomshr->satinc[1], Tcomshr->satinc[2], 
              Tcomshr->satinc[3], Tcomshr->satinc[4], Tcomshr->satinc[5] );

          h = Tcomshr->nmainl[l]*b*a/(c*20);
          if(f >= h) goto S400;
          Tcomshr->liensa[l] = 6;
          if(aa > 0) 
			  Tcomshr->liensa[l] = 2;
S400:


		
/*
 --- FLAG LINK E.S. IF INSUFFICIENT VEH IN REGISTER(S)
*/
          continue;
    } 
	
	
	
	  
	  /*
 ELEL        END CHECKING OF LINKS FOR E.S.
              NOTE : DO LOOP SPANS C1,C2,C3 AND C4
*/
    if(Tcomshr->marker < 4) return;
/*
 --- SKIP REST IF CAN'T CHANGE STAGE ( IN EITHER IG OR MIN-G )
 --- ALSO, SKIP IF NO DEMANDS (SNEXT=CUSIG AND MARKER LEFT=2,3)
 **********************************************************************
 //////////////////////////////////////////////////////////////////////
     SUBSECTION C5 : CHECK LINKS FOR RELEVANCE TO CHANGE TO SNEXT
                      CHECK IF CHANGE IS PERMITTED
*/
    /*  IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: we should really run the relevance
        checking code whenever there's been a change in stage demands,
        not just when SNEXT has changed, as our assessment of link
        relevance as a result of running CheckGreenInFollowingStages below
        may change. Simplest to just run this code all the time. */
    /*if(Tcomshr->snext == Tcomshr->presnx && Tcomshr->premrk >= 4) goto S460;*/
/*
 --- Skip resetting of benefitting links if no change in snext
 --- and if already set once for this stage
 SLSL
*/
	for (l = 0; l < Tcomshr->nlinks; l++)
	{
		Tcomshr->benlin[l] = 0;

		/* Skip ped and em/pr links */
		if (Tcomshr->ltype[l] != 0)
			continue;

		/* if link red now it is irrelevant to end-sat decision */
		if (Tcomshr->lgreen[Tcomshr->cusig-1][l] == 0)
			continue;

		if (Tcomshr->lgreen[Tcomshr->snext-1][l] == 0 || 
			Tcomshr->lgreen[Tcomshr->snext-1][l] == 4)
		{
			/* link is green now and red / effective red next stage */
			/* therefore link is relevant in determining benefits */
			Tcomshr->benlin[l] = 1;
		}
		else
		{
			a = Tcomshr->min[Tcomshr->snext-1] + 5;

			/* IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA: if link is fixed green 
			   in the next stage AND does not have an extendable green 
			   in a stage immediately following the next stage */

			if (a >= Tcomshr->max[Tcomshr->snext-1] && 
				CheckGreenInFollowingStages(l, Tcomshr->snext-1, Tcomshr) == 1)
			{
				/* fixed green next - link is relevant */
				Tcomshr->benlin[l] = 1;
			}
			else
			{
				/* link overlaps CUSIG/SNEXT and is not relevant */
				Tcomshr->benlin[l] = 2;
			}
		}
	}
    
    if(Tcomshr->stensa[Tcomshr->snext-1] > 0) goto S520;
/*
 --- Skip rest of C5 if all relevant links E.S. for change to SNEXT.
 --- Else, note if any relevant links still at sat flow using 'AA'
 --- Also, sum merit-value for current stage using 'H'
*/
    aa = 0;
    h = 0;
    for(l=0; l<Tcomshr->nlinks; l++) {
          if(Tcomshr->ltype[l] != 0 || Tcomshr->lgrefl[l] == 0) goto S500;/* M 4.0*/
/*
 ---    Skip pedestrian & emerg/pr links, or red links.         ! M 4.0
*/
          f = Tcomshr->yvalue[l];
          k = Tcomshr->llatot[l];
          if(Tcomshr->liensa[l] != 0) goto S480;
/*
 ---    Jump if green link is already end-sat (overlaps incl.)
*/
          if(Tcomshr->benlin[l] == 1) aa = 1;
/*
 ---    Note if relevant link is not yet end-sat
*/
          if(Tcomshr->benlin[l] != 1 && Tcomshr->prensa[l] > 0) goto S480;
/*
 ---    Jump if overlap link was end-sat last cycle
 ---    Else, overlap was, or benefitting link now is, still sat
*/
          f = 100;
          if(Tcomshr->lingre[l] >= Tcomshr->bontim[l]) k = Tcomshr->nmainl[l];
S480:
/*
 ---    Reduce number of lanes after short lanes finish
*/
          h = h+f*k;
S500:
          continue;
    }
/*
 --- Reset merit-value (per cent) for current stage, throughout green
*/
    Tcomshr->meritv[Tcomshr->cusig-1] = h;
    if(aa > 0) return;
/*
 --- Finish if any relevant link not yet end-sat
 **********************************************************************
*/
    Tcomshr->stensa[Tcomshr->snext-1] = 1;
S520:
    Tcomshr->marker = 5;
/*
 --- When DO-loop completes, all links end-sat, mark stage end-sat
 **********************************************************************
*/
    return;
}


/*  IRH MOD: M5.0.0: 27/01/04: TRL/SIE Compact MOVA:
    If link is green in a fixed stage, then has variable 
    green in the stage immediately afterwards, it isn't a relevant link. 
    This function checks for this. This function should be run whenever 
    there's a change in demand for ANY stage (or every 0.5 seconds). 
    
    Returns TRUE if link is relevant, FALSE otherwise
    
    Called externally from Block D and B */
int CheckGreenInFollowingStages(    int nLinkNum, 
                                    int nNextStageNum, 
                                    Ccomshr const * Tcomshr )
{
    /* Assume link is relevant (i.e. no variable green after next stage) */
    int nIsRelevant = TRUE;

    /* Get the stage following the next stage */
    int nStageLoop = nNextStageNum + 1;
    if ( nStageLoop == Tcomshr->stages ) /* Check for wrap-around */
    {
        nStageLoop = 0;
    }

    /* For each stage apart from the next stage itself in numerical order */
    while ( nStageLoop != nNextStageNum )
    {	
        /*  If we can get to this stage from the next stage and 
            a demand for the stage exists */
        if (    Tcomshr->change[ nNextStageNum ][ nStageLoop ] > 0 
                && IS_STAGE_DEMANDED( nStageLoop ) )				
        {	
            /* If the link has green in this stage */
            if ( Tcomshr->lgreen[ nStageLoop ][ nLinkNum ] > 0 )
            {
                /*  If the stage isn't fixed */
                if ( Tcomshr->min[ nStageLoop ] + 5 < Tcomshr->max[ nStageLoop ] )
                {
                    /*  This link is currently IRRELEVANT - it WILL have continued 
                        (variable) green in a stage following the next stage 
                        given current demands */
                    nIsRelevant = FALSE;
                    break;
                }
                
                /*  Else stage is fixed - go on to check further stages */
            }

            /* Else link has red in this stage */
            else
            {	
                /*  Link is currently RELEVANT (we've come to a stage after the next
                    stage in which the link will be red given the current
                    stage demands). */
                nIsRelevant = TRUE;
                break;
            }
        }
		
        /*  Else can't get to this stage from the next stage, or if we
            can there's no demand for it - go on to check further stages */

        /*  Get the number of the stage numerically after the next stage */
        nStageLoop++;
        if ( nStageLoop == Tcomshr->stages ) /* Check for wrap-around */
        {
            nStageLoop = 0;
        }

    } /*    For each stage apart from the next stage itself in numerical order */

    return nIsRelevant;

} /* static int OptProto_CheckGreenInFollowingStages(   int nLinkNum, ... ) */
