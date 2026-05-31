
/*******************************************************************************
*
*                      MOVA KERNEL
*
*  FILE:         spec_cond.h
*
*  TITLE:        Mova Kernel: PCMOVA
*
*
*  (c) Copyright TRL Ltd 2016. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/



#if !defined (__SPECCOND_H)
#define __SPECCOND_H


#if defined (__cplusplus)
    extern "C" {
#endif

#include <libxml/xmlstring.h>

#define SC_MAX_NUMB_VALUES (10)

void		SC_ProcessAllRules();
MVStatus	SC_SetParameterData(int dpnumb, int nrule, int nparm, xmlChar * psource, uint8 nvals, double *  vals, xmlChar * user, xmlChar * mvar, xmlChar * oper);
void		SC_SetParameterNumber(int dpnumb, int nrule, int nparms);
MVStatus	SC_SetResultData(int dpnumb, int index, xmlChar * rsource, double chan, xmlChar * user, MVBool ulogic);
void		SC_IncRuleNumber(int dpnumb);
MVStatus	SC_SetTimerData(int dpnumb, int index, double call, double cancel, double delay, double duration, double persistence, double pulseMark, double pulseSpace);
void		SC_ResetTempData();
MVStatus	SC_LoadDatasetRules(MVInt8 dpnumb);

#if defined (__cplusplus)
    }
#endif

#endif /* __SPECCOND_H */