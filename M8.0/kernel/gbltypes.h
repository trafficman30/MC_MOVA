
/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         gbltypes.h
*
*  TITLE:        MOVA kernel core data declarations
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	26-Jan-1998		4.0.0		..		MRC		SIE_00			First undergoing system test
*	03-May-1998		4.0.0		..		PC		SIE_01			Overlay on 'Xcomshr', etc., replaced by explicit use of structures.
*	12-May-1998		4.0.0		..		PC		SIE_02			Added day_from_date() (Siemens fault report MOVA-27)
*	15-May-1998		4.0.0		..		PC		SIE_03			Added detsin[] and confin[] (Siemens fault report MOVA-31)
*	13-Jan-2000		4.0.0		..		PC		SIE_04			Moved day_from_date() from gbltypes.h to OBCLOCK.H
*	09-Mar-2000		4.0.0		..		PC		SIE_05			Added detsin_latched[].
*	27-Mar-2000		4.0.0		..		PC		SIE_06			Replaced detsin_latched[] with and detsin_0s[] and detsin_1s[].
*	29-Sep-2004		5.0.0		..		IHR		SIE_07			Added various defines for 'magic numbers'. Xerrlog.crash[] array replaced 
*																with dbzstr[] string to hold divide-by-zero data.
*	11-May-2006		5.0.0		..		IHR		SIE-08			Header guards added.
*	08-Aug-2007		5.0.0		..		RD		TRL_09			Updated Current Mova Version to 6.0.3
*	31-May-2011		7.0.0		AA		PK		CRN003			Several Changes for MOVA 7 release
*	12-Dec-2011		7.0.1		AB		PK		CRN004			SatFlow and CANDET improvements
*	22-Feb-2012		7.0.3		..		AK		CRN008			Make flagDstrig extern
*	15-Mar-2012		7.0.3		AC		AK		CRN009			Changes to conditional compilation flags
*
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

#if !defined __GBLTYPES_H
#define __GBLTYPES_H

#include <stdbool.h>
#include <time.h>
#include "mova.h"
#include "defines.h"
#include "tma_strc.h"
#include "mova_constants.h"


/* General Logicals */

/* IRH MOD: M5.0.0: 23/09/04: Redefine (if necessary) TRUE and FALSE */
#ifdef TRUE 
    #undef TRUE 
#endif 

#ifdef FALSE 
    #undef FALSE
#endif

/* SLM MOD: M8: 11/11/16: Define TRUE/FALSE  as agreed
   by Jim Ballantine (Siemens) and Andrew Leigh (Dynniq) */
#define FALSE  false
#define TRUE   true


/*  IRH MOD: M5.0.0: 02/02/04: TRL/SIE Compact MOVA:
    Ccomshr.dshort value used for a long lane without IN-detector */
#define LONG_LANE       999

/*  IRH MOD: M5.0.0: 02/02/04: TRL/SIE Compact MOVA:
    Macro for determining whether ANY sort of demand for a stage exists.
    Currently used by CheckGreenInFollowingStages() in Block C. */

#if defined (CMOVA_EP) /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */

    #define IS_STAGE_DEMANDED( stage )  (           \
        (   Tcomshr->stadem[ (stage) ] > 0          \
            || Tcomshr->pstdem[ (stage) ] > 0       \
            || Tcomshr->estdem[ (stage) ] > 0 )     \
    )

#else

    #define IS_STAGE_DEMANDED( stage )  ( Tcomshr->stadem[ (stage) ] > 0 )

#endif /* CMOVA_EP */ /* IRH MOD: M5.0.0: 11/03/04: CMOVA site trials */

/* Error Messages (TRL Nos currently end at 15)     */
#define MAX_MSG_NO            (22*1000+10032)

#define IS_LONG_LANE( lane )    (               \
    Tcomshr->dshort[(lane)] == LONG_LANE ||     \
    Tcomshr->det[(lane)][0] > 0                 \
)

/* IRH MOD: M5.0.0: 11/08/04: MOVA 5: Current version of MOVA */
/*Latest Version is 7.0*/
#define xgszVERSION_MOVA        ("M8.0")
#define xgnTIME_OF_DAY_ARRAY_SIZE   (20)

/*  IRH MOD: M5.0.0: 18/05/04: Sizes of strings used to describe the dataset
    in the dataset header */
/* File name length was 12 - 
 * DOS 8.3 format needs 8+1+3+1 = 13 chars (including null-terminator) 
 * Not a problem for preceding versions of MOVA - there was only a 2 character
 * extension (DT/PT etc.) - but MOVA 5 uses the extension .MDS */
#define xgnFILE_NAME_STR_LEN     (30)
#define xgnVERSION_STR_LEN       (8)
#define xgnTIME_STR_LEN          (6)
#define xgnDATE_STR_LEN          (9)
#define xgnTITLE_STR_LEN         (37)

#define xgnDBZ_MESSAGE_STR_LEN   (256)

/*  IRH MOD: M5.0.0: 18/05/04: Spare data sizes */
#define xgnSPARE_DATA_ROWS_NUM   (5)
#define xgnSPARE_DATA_COLS_NUM   (64)

/*  IRH MOD: M5.0.0: 13/07/04: 
 * 	Number of errors for which error count increments can be stored */
#define xgnERRORS_NUM_MAX        (30) /* was 10 */

/*  IRH MOD: M5.0.0: 27/08/04: 
 *  Number of errors that are actually stored currently
 * Ideally, this should be in the dataset file as metadata */
#define xgnERRORS_NUM            (9)

/* Indexes for columns in the Tcomshr->errval table */
enum ERROR_DATA_TYPES
{
    ERROR_DATA_TYPE_ID, 
    ERROR_DATA_TYPE_VALUE,
    
    ERROR_DATA_TYPES_NUM
};

/*
Now define the split of data types into int and char. (This is a carry over
from the old Fortran system in which saving space was of prime importance.
Therefore any variable that will never exceed 128 was put to char, or integer*1
in Fortran. Most variables required I*2 however. This, together with the
equvalencing that went on in the Fortran, means the reading of data had to take
account of whether data was 1-byte or 2-byte).
*/
#define SPLITT   (  xgnFILE_NAME_STR_LEN + xgnVERSION_STR_LEN + xgnTIME_STR_LEN + \
                    xgnDATE_STR_LEN + xgnTITLE_STR_LEN )    /* Dataset header */
#define SPLIT1   16                                       /* stages to stgdem */

#define SPLIT2   (mxstag*(mxlink +1))                     /* lgreen and pfacil */

#define SPLIT3   ((mxstag*mxstag)+(mxstag*5)+(mxstag*mxlink)+(mxlink*5)+40)
                                            /* emxmax to nmainl */
#define SPLIT4   mxlink*6                                 /* llanes to candet */
#define SPLIT5   mxlink*11                                /* epext to mixout */
#define SPLIT6   mxlane*13                                /* det and assocd (11 dets + 2 assocd)*/

/*  IRH MOD: M5.0.0: 02/02/04: TRL/SIE Compact MOVA:
    SPLIT7 contains all remaining data, including any new fixed/variable data, 
    which must be integer for the moment. */
#define SPLIT7_FIX  (                                                                        \
    (mxlane*16) + 24 + 16 +                     /* crusin to tel - VER_BASE (fixed) */       \
    (mxlane*6) + mxlink +                       /* cspeed to sparef - MOVA 5 (fixed) */      \
    (mxstag*2) + 				/* PEDMAX[2][mxstag] - MOVA M6 */            \
    (ERROR_DATA_TYPES_NUM * xgnERRORS_NUM_MAX) +                                             \
     /* (xgnSPARE_DATA_ROWS_NUM * xgnSPARE_DATA_COLS_NUM) now using all this plus more!!! */ \
    (mxlane*13) + (mxstag*2) + (xgnSFTIMS_MAX*2) + xgnREDPED_MAX+5 + /* tmaprd to DSTRIG */ \
	(mxlink*mxstag) + /*M8.0: PFACILM*/\
	(mxstag*2) + /*M8.0: BUSMAX 1 and 2 */ \
	mxlink + /*M8.0: BUSWGT */ \
	mxlane + /*M8.0: ASCEPL*/ \
	(mxlink*2) /*M8.0: EPCRT (1 and 2)*/ \
)
/*#define SPLIT7   ((mxlane*17)+(mxstag*6)+(mxlink*7)+(54))*/ 

/* Interesting note: The GCC compiler/preprocessor will (correctly) ignore 
 * spaces after the '\' character used to split a macro over several lines.
 * However, this isn't the case in Visual C++ .NET - syntax errors will ensue
 * if there are any spaces after the '\'. */
#define SPLIT7_VAR (                                                                            \
    10 + (mxstag*6)+(mxlink*7) + (mxlane*1) +     /* lastag to smflow - VER_BASE (variable) */  \
    (xgnSPARE_DATA_ROWS_NUM * xgnSPARE_DATA_COLS_NUM) /* sparev - MOVA 5 (variable) */          \
)

#define SPLIT7_CHK  (4)

#define SPLIT7 (SPLIT7_FIX + SPLIT7_VAR + SPLIT7_CHK)

/*  IRH MOD: M5.0.0: 02/02/04: TRL/SIE Compact MOVA:
    SPLIT9 to 12 are moved and renamed from 
    ARR1SIZE to ARR4SIZE in mova--m4.c */
/*#define SPLIT8	(57*50 + 1)*/
#define SPLIT9	(3*mxstag + 15*mxlink + 16)
#define SPLIT10 (2*mxstag +  2*mxlink + 21*mxlane)
#define SPLIT11 (1*mxstag +  3*mxlink + 15*mxlane + 8*mxdets + 6)
#define SPLIT12	(3*mxstag +  3*mxlink + 6)

/* Shared I/O Data (dinout) */
/* ~~~~~~~~~~~~~~~~~~~~~~~~ */

typedef struct {
    /* MOVA Inputs - din[] no longer used (see confin[] and detsin[]) */
    char din[NumberOf_din];
    /* MOVA Outputs - see DoutStartOfForce, etc. above */
    char dout[NumberOf_dout];
} Cdinout;




/* Shared Working Data (comshr) */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

typedef struct {
 
    /* IRH MOD: M5.0.0: 15/07/04: Now dataset is at top of shared working data - simpler
     * and allows us to cast Xsitdat.site to Ccomshr* if needed 
     * (e.g. to calculate checksum).  The working data that was previously
     * at the top of Ccomshr is now between SPLIT7 and SPLIT8 with the other
     * working data. */


    /***********************
     * Dataset data - FIXED
     ***********************/
 
    /* SPLIT T */
    char    fname[ xgnFILE_NAME_STR_LEN ],
            version[ xgnVERSION_STR_LEN ],
            time[ xgnTIME_STR_LEN ],
            date[ xgnDATE_STR_LEN ], 
            title[ xgnTITLE_STR_LEN ];
   
    /* SPLIT 1 */
    int stages, nlinks, nlanes, ndets, scan, minext, maxext, addgap, subgap,
      mainst, totalg, optype, /* TRL: replaces utc */
      detton, stagon, phason, stgdem;

    /* SPLIT 2 */
    signed char lgreen[mxstag][mxlink], pfacil[mxstag];

    /* SPLIT 3 */
    int emxmax[mxstag], prxmax[mxstag], lphase[mxlink], min[mxstag],
      max[mxstag], lowmax[mxstag], change[mxstag][mxstag], 
      dattim[ xgnTIME_OF_DAY_ARRAY_SIZE ], datnum[ xgnTIME_OF_DAY_ARRAY_SIZE ], 
      sdcode[mxstag][mxlink], ltype[mxlink], shortl[mxlink],
      llatot[mxlink], nmainl[mxlink];

    /* SPLIT 4 */
    int llanes[mxlink][mxlnOnlink], epdet[mxlink][2], candet[mxlink];

    /* SPLIT 5 */
    int epext[mxlink][2], holdtm[mxlink], lmin[mxlink], eslmax[mxlink], 
      lostim[mxlink], stopen[mxlink], bontim[mxlink], boncut[mxlink], 
      minpcs[mxlink], mixout[mxlink];

    /* SPLIT 6 */
    signed char det[mxlane][mxdetTypes], assocd[mxlane][2];
 
    /* SPLIT 7 FIX */
    int crusin[mxlane], crusx[mxlane], gamber[mxlane], stlost[mxlane],
      satinc[mxlane], satgap[mxlane], critg[mxlane], movqin[mxlane],
      moveqx[mxlane], qminon[mxlane], comtim[mxlane], osatcc[mxlane],
      xosat[mxlane], bonbc[mxlane], lanewf[mxlane], maxmin[mxlane],
      asstim[24], tel[MAX_TEL];
      
    /* end of MOVA 4 fixed data */
	
    /*  IRH MOD: M5.0.0: 02/02/04: TRL/SIE Compact MOVA:
        New MOVA 5 and CMOVA data.
        N.B. vlen is in the MOVA data files that MovaSetup produces,
        although it's no longer used (vehicle lengths are assumed to be 
        6m average for all lanes by MovaSetup). However, I've left it
        in 'just in case' we want it in future. */
	int cspeed[mxlane], vlen[mxlane], din[mxlane], 
	  dx[mxlane], dshort[mxlane], osattm[mxlane],
          waitch[mxlink], errval[ ERROR_DATA_TYPES_NUM ][ xgnERRORS_NUM_MAX ];

	int pedmax[2][mxstag];

    /* MRC: M7.0.0 5/3/10 more fixed data
     * TMA, satflow, alerts and sundry other fixes in MOVA M7 
     */
	int tmaprd, 
	  usesf[mxlane], usest[mxlane], sftims[xgnSFTIMS_MAX][2],
	  rtstor[mxlane], rtrad[mxlane], rtprop[mxlane], rtoppo[mxlane][MAX_OPP_LANES],
	  redped[xgnREDPED_MAX],
	  osatcx[mxlane],
	  osatal[mxlane], exital[mxlane], occual[xgnOCC_ON_OFF_DIM][mxstag],  dstrig[xgnMAX_DATASETS];

	/*------------- M8 data elements -------------*/
	/*M8: Improved bus priority data*/
	int pfacilm[mxstag][mxlink];
	int busmax[MAX_EP_DETS][mxstag];
	int buswgt[mxlink];
	int ascprl[MAX_PRIORITY_LINKS_ACCOC_TO_LANE][mxlane];
	int epcrt[MAX_EP_DETS][mxlink];

	/*M8: Link Maximum data*/
	int	lnkmax[mxlink];

	/*M8: Call-cancel times for IRTGA */
	int mxcall[mxlink], mxcncl[mxlink];

	/*M8: Premature end-sat decisions*/
	int pcritg[mxlane];
	
	/*M8: Re-establishment of satflow*/	
	int rcritg[mxlane];
	

    /***********************
     * Dataset data - VARIABLE
     ***********************/
 
    /* SPLIT 7 VAR */
    int /*lastag, snext, warmup, marker, premrk, junsmf, smcycl, totlam,*/
      mark1, mark2/*, stadem[mxstag], lambda[mxstag],
      stcycl[mxstag], meritv[mxstag], drifmx[mxstag], altlam[mxstag],
      cycltm[mxlink], lindem[mxlink], lingre[mxlink], yvalue[mxlink],
      lsmflo[mxlink], extrag[mxlink], prensa[mxlink], smflow[mxlane]*/;

    /* end of MOVA 4 variable data */

     int sparev[ xgnSPARE_DATA_ROWS_NUM ][ xgnSPARE_DATA_COLS_NUM ];

    /* end of MOVA 5 variable data */

    /* IRH: New VARIABLE data in future versions should go HERE */

    /***********************
     * Dataset data - CHECKSUM
     ***********************/

    /* SPLIT 7 CHK */
    /*  IRH MOD: M5.0.0: 02/02/04: TRL/SIE Compact MOVA:
        checksums moved to end of alldata and MOVASetup files - 
        if more data is added in future, these should *remain* at the end */
	int chksm1[2], chksum[2];

    /*  end of Checksum data */

 
    /***********************
     * Working data
     ***********************/
       
    signed char ii[mxdets], io[mxdets];       /* mxdets used for convenience */
    int ton[mxdets], toff[mxdets];
    signed char deton[mxdets], detoff[mxdets];
    int vc[mxdets], pregap[mxdets], greens[mxlink], snow/*, demsta*/;
    /*char aveflo[7][48][mxlane];*/

    /* SPLIT 8 */
    /*int imess[IMESSCOL][IMESSROW], nmess, */

    
    /***********************
     * Working data initialised to zero at start of MOVA__M4.c
     ***********************/

    /* SPLIT 9 */
      /*cusig, presig, sistim, juncos, cutmax, waitim, stamin, stamax,
      cdelta, disben, r, benfit, presnx, laosmx, mxplus, lototg, stensa[mxstag],
      stagos[mxstag], lodfmx[mxstag], mingre[mxlink], liensa[mxlink],
      benlin[mxlink], bonusg[mxlink], lwaste[mxlink], redlt[mxlink],
      linkos[mxlink], lgrefl[mxlink], lidetf[mxlink], ben[mxlink], red[mxlink],
      maxpcs[mxlink], snkbon[mxlink], ldben[mxlink], netbfr[mxlink];*/

    /* SPLIT 10 */
    /*signed char shrega[mxlane][21], stbonl[mxstag], lbonst[mxlink], rav8[mxstag],
       revdem[mxlink];*/

    /* SPLIT 11 */
    /*int redcin[mxlane], redcx[mxlane], laensa[mxlane], regtot[mxlane],
      inxtra[mxlane], exitrc[mxlane], laosat[mxlane], ladetf[mxlane],
      osatic[mxlane], sinkrc[mxlane], totflo[mxlane], ondet[mxdets],
      timon[mxdets], timoff[mxdets], oldgap[mxdets], cdc[mxdets], oldcdc[mxdets],
      susdet[mxdets], precdc[mxdets], stabon[mxstag], inscdc[mxlane],
      xskcdc[mxlane], asscdc[mxlane][2], totbon[mxlink], ephold, epx, epxmxt,
      esxtim, psxtim, eplxtm[mxlink], rav12[mxlink], rav1;*/
    
    /* SPLIT 12 */
    /*signed char esnext, psnext, tempmk, trunc, estdem[mxstag], pstdem[mxstag],
      saosmx[mxstag], cancel[mxlink], epskip[mxlink], eptrun[mxlink], rav2, rav3;*/
} Ccomshr;


/* Shared Error Log (errlog) */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~ */

typedef struct {
    /* IRH MOD: M5.0.0: 31/08/04: Deprecated */
    /*long crash[6];*/
    char dbzstr[ xgnDBZ_MESSAGE_STR_LEN ];
    int  idat[4][1100];
    int  ndat;
    char buffer[32];
    char flag[32];
    char stgoff;
    char stgon;
} Cerrlog;


extern Ccomshr *const Tcomshr ;
extern Ccomshr Xcomshr;

extern Cerrlog *const Terrlog ;
extern Cerrlog Xerrlog;

extern Cdinout *const Tdinout ;

extern char detsin[mxdets];
extern char detsin_0s[mxdets];
extern char detsin_1s[mxdets];
extern char confin[mxconf];

extern int flagDstrig; /*user requested to change ToD using dstrig detectors*/

/* Function Prototypes */
/* =================== */

/* function to output text to the handset or modem */
extern int WRITE(int, ... );

/* IRH MOD: M5.0.0: 19/07/04: Changed to all int (new datahand module assumes this) */
/* IRH MOD: 28/09/05: PCMOVA: Embedded version doesn't really need to be "const"
   and PCMOVA version can't be as the default dataset is loaded at run-time. */
#if defined (PC_WRAPPER)
    typedef struct
#else
    typedef const struct
#endif
{
    char sitename[SPLITT];
    int stages_to_stgdem[SPLIT1];
    int lgreen_to_pfacil[SPLIT2];
    int emxmax_to_nmainl[SPLIT3];
    int llanes_to_candet[SPLIT4];
    int epext_to_mixout[SPLIT5];
    int det_to_assocd[SPLIT6];
    int cruisin_to_checksm[SPLIT7];

} MOVA_CONST_DATA_STRUCT;
	

/* max (seems to be missing from stdlib.h) */
#define max(a,b)    (((a) > (b)) ? (a) : (b))

/* IRH MOD: M5.0.0: 20/07/04: min() isn't defined in stdlib.h (I guess it's not part
 * of the C ISO standard - just happens to be in MSVC's stdlib) */
#define min(a,b)    (((a) < (b)) ? (a) : (b))

/* IRH MOD: 28/09/05: PCMOVA: Prevent multiple includes (do for embedded too) */
#endif /* __GBLTYPES_H */
