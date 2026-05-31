/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         alerts.h
*
*  TITLE:        MOVA Alerts Interface
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	12-Dec-2011		7.0.1		AA		AK		CRN007			Implement alerts API
*	17-Dec-2012		7.0.5		..		PA		CRN019			Improve alerts interface
*	31-Jan-2013		7.0.5		AB		AK		M7.0.5			Issue M7.0.5
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

#include "MVTypes.h"
#include "alerts.h"

/*
	Dummy (null) implementations of alerts provided by TRL.
	Manufacturers should implement function stubs as desired.

	See comments in alerts.h for more details.
*/

void set_alert_oversat(const Ccomshr *const Lcomshr, const strcTMA *const tma)
{
	UNUSED(Lcomshr);
	UNUSED(tma);
}

void clear_alert_oversat(const Ccomshr *const Lcomshr, const strcTMA *const tma)
{
	UNUSED(Lcomshr);
	UNUSED(tma);
}


void set_alert_exitblocked(const Ccomshr *const Lcomshr, const strcTMA *const tma)
{
	UNUSED(Lcomshr);
	UNUSED(tma);
}

void clear_alert_exitblocked(const Ccomshr *const Lcomshr, const strcTMA *const tma)
{
	UNUSED(Lcomshr);
	UNUSED(tma);
}


void set_alert_occupancy(const Ccomshr *const Lcomshr, const strcTMA *const tma)
{
	UNUSED(Lcomshr);
	UNUSED(tma);
}

void clear_alert_occupancy(const Ccomshr *const Lcomshr, const strcTMA *const tma)
{
	UNUSED(Lcomshr);
	UNUSED(tma);
}

