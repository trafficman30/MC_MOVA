/*
Module name . . . . . . . . . . . . . . . MOVADATA.C
Module version  . . . . . . . . . . . . . SIE_12
Last modification date  . . . . . . . . . 29/09/04
MOVA version at which this 
         module was last modified . . . . M5.0.0
Authority . . . . . . . . . . . . . . . . TRL Ltd

          Last change:  IRH   29 Sep 04    10:26 am

------------ Note on version numbering ------------ 

Module version 

    Current version number for this module. Module versions
    are independent of the overall MOVA version number.

MOVA version at which this module was last modified

    The MOVA version at which the last modification to this 
    module was made.  This may be older than the currently 
    compatible MOVA version if no changes to this module
    were made as part of the current version.
*/
/*
______________________________________________________________________

(c) Copyright TRL Ltd 2004. Rights to and ownership of the software
remain unaffected by changes to the software, whether by addition,
removal or modification. Agreement has been reached between Siemens
Traffic Controls Ltd and TRL Ltd whereby Siemens are authorised to use
and modify the MOVA code. TRL Ltd cannot be held responsible for
improper use. 
______________________________________________________________________

;*                         MODIFICATION RECORD
;*                         -------------------
;*
;* SOURCE
;* IDENT.   DATE        NAME               REASON FOR CHANGE
;* -----    ----        ----               -----------------

   SIE_00   26/1/98     MRC                First undergoing system test
   SIE_01   24/4/98     PC                 Modified the #pragma statements so
                                           that the address can be specified in
                                           the .lnk file and not here.
   SIE_02   24/4/98     MRC                Initialisation removed from lIsClockSet
   SIE_03   03/5/98     PC                 Overlay on 'Xcomshr', etc., replaced
                                           by explicit use of structures.
   SIE_04   12/5/98     PC                 Added day_from_date()
                                           (Siemens fault report MOVA-27)
   SIE_05   15/5/98     PC                 Added detsin[] and confin[]
                                           (Siemens fault report MOVA-31)
   SIE_06   18/5/98     PC                 Commented out use of SECT_MOVA_config
                                           since config resides in normal PROM
                                           space so that it is covered by the
                                           firmware checksum.
                                           (Siemens fault report MOVA-22)
Changes for combined OMU and MOVA
   SIE_07   13/1/00     PC               - Moved day_from_date() from MOVADATA.C
                                           to OBCLOCK.C
                                         - Added 'extern' to declaration of 'environ'
                                           since it is also defined in DATTRAN.
   SIE_08   10/2/00     PC               - Modified Movsub() so that it only loads
                                           the site data from prom if the RAM copies
                                           appear corrupt.
                                         - Xsitdat moved to KEPTDATA
   SIE_09   09/3/00     PC               - Added detsin_latched[].
   SIE_10   27/3/00     PC               - Replaced detsin_latched[] with
                                           and detsin_0s[] and detsin_1s[].
   SIE_11   26-02-01    KLS				 - Added #include "hardware.h" for 
										   MAX_EXPANSION_CARDS definition
                                          
Changes for MOVA 5.0 (includes Compact MOVA):-
   SIE_12   29/09/04    IRH     Functionality and data for MOVA plan store 
                                now handled by new Datahand.c module (no longer
                                using Xsitdat). SavedPlanNo, Movsub() and
                                #include "bigsim.sa" removed.
                                This file now essentially just contains global 
                                MOVA working data (Xcomshr, Xerrlog etc.).
*/

#include <nucleus.h>
#include <proj_def.h>
#include "gbltypes.h"
#include <hardware.h>
#include <keptdata.h>
#include <diff_bss.h>


#ifndef MSDOS
extern char **environ;
#endif

/* Shared MOVA Data */

/* the following have all been replaced by their structures */
#ifdef EXTMOVA
/* char Xcomshr[14238]; */
/* char Xsitdat[14856]; */
/*char Xaveflo[8064];*/ /* IRH MOD: M5.0.0: 14/07/04: This seems to be no longer used. */
/* char Xerrlog[9694];  */
/* char Xdinout[NumberOf_din+NumberOf_dout]; */
#else
/* char Xcomshr[15528]; */
/* char Xsitdat[6528];  */
/* char Xerrlog[9694];  */
/* char Xdinout[NumberOf_din+NumberOf_dout]; */
#endif

//Ccomshr Xcomshr;
//Ccomshr *const Tcomshr = &Xcomshr;

//Cerrlog Xerrlog;
//Cerrlog *const Terrlog = &Xerrlog;
//
//Cdinout Xdinout;
//Cdinout *const Tdinout = &Xdinout;

#if 0 /* IRH MOD: M5.0.0: 07/04: Now in new Datahand module */

    void Movsub(void);

/* Add here the MOVA data as generated by the utility program CONVRT */

/* Pardon the unorthodox use of #include!
  
   Use the include file to add the constant
   definition of the MOVA data required. Add new ones as required.
   
   NB. the form of the data file to include is 
   
           "filename.sa"
           
   as created from the 'convrt' process
   
   When done, re-compile and link this file. The const defined values will be 
   held on EPROM. Movsub() will load the EPROM MOVA data into the RAM areas as
   required.
*/

/* code added to place data at start of PROM */ 
/* Old code used an address explicitly */
/* #pragma section MCONFIG far-absolute R address=0xffe80000 */
/* #pragma use_section MCONFIG mova_site_data_1, mova_site_data_2, mova_site_data_3 */
/* New code using segment name which is located by the .lnk files */

/* The following code has been commented out (also see PB681.LNK) so that the config data
|  resides in normal memory space in the firmware PROM so that it is covered by the firmware
|  PROM checksum. */

/* #ifndef MSDOS                                                                             */
/* #pragma section SECT_MOVA_config "SECT_MOVA_config" "" far-absolute R                     */
/* #pragma use_section SECT_MOVA_config mova_site_data_1, mova_site_data_2, mova_site_data_3 */
/* #endif                                                                                    */

#include "bigsim.sa"
/* #include "default.sa" */

void Movsub(void)
{
    utiny plan;
    long c4,d4;
    int chksm1[2];
    tbool bad_data = FALSE;
    
    /* IRH MOD: M5.0.0: 14/07/04: New checksum function */
    extern void checks(Ccomshr const * const, long * const, long * const );

    /* for each of the three plans */
    for ( plan=0; plan<3; plan++ )
    {
        /* calculate the checksum for the data in this plan */
        /* IRH MOD: M5.0.0: 14/07/04: Cast to Ccomshr pointer */
        checks( (Ccomshr *)( &( Xsitdat.site[plan*alldat] ) ), &c4, NULL );
        /* extract the fixed checksum from the end of this plan's data */
        /* (the two 4-byte checksums are last, we want the 1st of these) */
        memcpy(&chksm1[0], &Xsitdat.site[(plan*alldat)+(alldat-2)], 4 );
        /* and convert it to a single value */
        d4 = chksm1[0]*1000 + chksm1[1];
        /* if the checksums do not match or the plan name starts with all zero's
        |  when it should normally be ASCII characters (need this check because
        |  the checksum may look valid if all the data is 0's) */
        if ( ( c4 != d4 )
          || ( Xsitdat.site[plan*alldat] == 0 ) )
        {
            /* then the plan data held in ram is corrupt */
            bad_data = TRUE;
        }
    }

    /* if one or plans in RAM appears corrupt */
    if ( bad_data )
    {
        /* reload all three plans from the default site data held in PROM */
        /* the following code replaces the that below between the #if/#endif */

        /*  IRH MOD: M5.0.0: 03/02/04: TRL/SIE Compact MOVA:
            now using version-sensitive LoadData() function */
        /*LoadData( &Xsitdat.site[0*alldat], (long *)&mova_site_data_1 );
        LoadData( &Xsitdat.site[1*alldat], (long *)&mova_site_data_2 );
        LoadData( &Xsitdat.site[2*alldat], (long *)&mova_site_data_3 );*/
        memcpy( &Xsitdat.site[0*alldat], &mova_site_data_1, alldat*4 );
        memcpy( &Xsitdat.site[1*alldat], &mova_site_data_2, alldat*4 );
        memcpy( &Xsitdat.site[2*alldat], &mova_site_data_3, alldat*4 );
    }
    else
    {
        /* the 3 plans appear correct so do not overwrite them even if the MOVA
        |  application has been initialised (since it is now initialised after
        |  the plan data has been loaded!) */
    }

#if 0
    char    *DestPtr;
    char    *SrcPtr1;
    char    *SrcPtr2;
    char    *SrcPtr3;
    int     i;


          SrcPtr1 = (char*)&mova_site_data_1;
          SrcPtr2 = (char*)&mova_site_data_2;
          SrcPtr3 = (char*)&mova_site_data_3;
          DestPtr = &Xsitdat[0];

   /* Copy data set 1 to RAM */
          for (i = 0; i < alldat*4 ; i++  ) {   
                    *DestPtr = *SrcPtr1;
                    DestPtr++;                  /* Inc's Separate to ensure done after copy     */
                    SrcPtr1++;
          }
          
   /* Copy data set 2 to RAM */
          for (i = 0; i < alldat*4 ; i++  ) {   
                    *DestPtr = *SrcPtr2;
                    DestPtr++;                  /* Inc's Separate to ensure done after copy     */
                    SrcPtr2++;
          }

   /* Copy data set 3 to RAM */
          for (i = 0; i < alldat*4 ; i++  ) {   
                    *DestPtr = *SrcPtr3;
                    DestPtr++;                  /* Inc's Separate to ensure done after copy     */
                    SrcPtr3++;
          }
#endif
}

#endif /* IRH MOD: M5.0.0: 07/04: Now in new Datahand module */
