/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         repository.h
*
*  TITLE:        MOVA Repository Type Structure
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	15-Apr-2013		7.0.6		..		AK		CRN027			Improvements to SF feedback
*	22-May-2013		7.0.6		AA		AK		M7.0.6			Issue M7.0.6
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


#ifndef _MOVA_REPOSITORY_H_
#define _MOVA_REPOSITORY_H_

#include "gbltypes.h"

/* IRH MOD: M5.0.0: 18/08/04: Made this a type, it's used in Datahand_userds() and
 * by the remote data download checking */
/* Structure to take nDataArea_[nChosen][]. This is needed as the output in userds
jumps all over the place and printing directly from nDataArea_ would be impossible */
/* IRH MOD: M5.0.0: 20/07/04: Struct name added, MOVA M5 data added in the right place.
 * KEEP THIS UP TO DATE WITH Xcomshr IN GBLTYPES.H.
 * --------------- FIXED DATA ONLY !!! ----------------- */

#ifndef M8_ENABLED


typedef struct Repository_Struct {

int stages, nlinks, nlanes, ndets, scan, minext, maxext, addgap, subgap,
    mainst, totalg, optype, detton, stagon, phason, stgdem,

    lgreen[mxstag][mxlink], pfacil[mxstag],

    emxmax[mxstag], prxmax[mxstag], lphase[mxlink], min[mxstag],
    max[mxstag],   lowmax[mxstag], change[mxstag][mxstag], 
    dattim[ xgnTIME_OF_DAY_ARRAY_SIZE ], datnum[ xgnTIME_OF_DAY_ARRAY_SIZE ],    
    sdcode[mxstag][mxlink], ltype[mxlink], /*pedconfirm[mxlink],*/
    shortl[mxlink],llatot[mxlink], nmainl[mxlink],

    llanes[mxlink][3], epdet[mxlink][2], candet[mxlink],

    epext[mxlink][2], holdtm[mxlink], lmin[mxlink],   eslmax[mxlink], 
    lostim[mxlink],   stopen[mxlink], bontim[mxlink], boncut[mxlink], 
    minpcs[mxlink],   mixout[mxlink],

    det[mxlane][mxdetTypes], assocd[mxlane][2],
   
    crusin[mxlane], crusx[mxlane],  gamber[mxlane], stlost[mxlane],
    satinc[mxlane], satgap[mxlane], critg[mxlane],  movqin[mxlane],
    moveqx[mxlane], qminon[mxlane], comtim[mxlane], osatcc[mxlane],
    xosat[mxlane],  bonbc[mxlane],  lanewf[mxlane], maxmin[mxlane],
    asstim[24], tel[16],
         
    /* IRH MOD: M5.0.0: 20/07/04: MOVA M5 data added here (see Gbltypes.h) */
    cspeed[mxlane], vlen[mxlane], din[mxlane], 
    dx[mxlane], dshort[mxlane], osattm[mxlane], waitch[mxlink],
    errval[ ERROR_DATA_TYPES_NUM ][ xgnERRORS_NUM_MAX ],      
    pedmax[2][mxstag],

    /*fixed data: TMA, satflow, alerts and sundry other fixes in MOVA M7  */
    tmaprd, 
    usesf[mxlane], usest[mxlane], sftims[xgnSFTIMS_MAX][2],
    rtstor[mxlane], rtrad[mxlane], rtprop[mxlane], rtoppo[mxlane][3],
    pcritg[mxlane], rcritg[mxlane],
    redped[xgnREDPED_MAX],
    osatcx[mxlane],
    osatal[mxlane], exital[mxlane], occual[xgnOCC_ON_OFF_DIM][mxstag], dstrig[4];
         
} Repository_Type;*/


#endif


#endif

