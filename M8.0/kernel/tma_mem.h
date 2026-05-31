/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         tma_mem.h
*
*  TITLE:        MOVA TMA Logs: Memory Management
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	31-May-2011		7.0.0		AA		PK		CRN003			New development and undergoing first full system testing
*	12-Dec-2011		7.0.1		AB		PK		CRN004			SatFlow and CANDET improvements
*	06-Jul-2012		7.0.3		AC		AK		CRN012			Memory leak in TMA module and statistics lost on dataset change
*
*  (c) Copyright TRL Ltd 2012. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal or
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/

#if !defined (TMA_MEMORY_H)
#define TMA_MEMORY_H

#include "tma_strc.h"

void TMA_Init_( void);

void TMA_setPeriod(void);
void TMA_SetFstPeriodTime(void);
void TMA_setAlertsHis(void);
void TMA_check_clock_change(void);
time_t GetNextPeriodBoundary();

#endif /*TMA_MEMORY_H*/
