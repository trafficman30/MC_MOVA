#pragma once
/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         dataset_handler.h
*
*  TITLE:        MOVA Kernel: The new (M8) handler of the dataset. Previously, "datahand.h"
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	13-Oct-2004		5.0.0		..		MC/IH	SIE_00			First undergoing system test
*	20-Dec-2004		5.0.1		..		IH		SIE_01			Fixed display site data - CHANGE array (intergreen times) now displays 10 stages.
*	04-Mar-2005		5.0.2		..		IH		SIE_02			Fixed stack corruption with larger datasets in Datahand_userds().
*	11-May-2006		5.0.3		..		IH		SIE_03			PCMOVA: Stdlib.h included for compatibility and "environ" pointer removed.
*																PCMOVA: Reject dataset downloads that don't have same number of links/lanes/stages as active data.
*	31-May-2011		7.0.0		AA		PK		CRN003			Several Changes for MOVA 7 release
*	22-Feb-2012		7.0.3		..		AK		CRN008			Ensure includes are lower case.  Add flagDstrig.
*	15-Mar-2012		7.0.3		..		AK		CRN009			Changes to conditional compilation flags
*	26-Jun-2012		7.0.3		AB		AK		CRN010			Fix definition of rtoppo in Repository_Struct
*	25-Jan-2013		7.0.5		..		AK		CRN023			Fix compiler warnings
*	31-Jan-2013		7.0.5		AC		AK		M7.0.5			Issue M7.0.5
*	15-Apr-2013		7.0.6		..		AK		CRN027			Improvements to SF feedback
*	03-May-2013		7.0.6		..		AK		CRN032			Error returning to menu after DS->T command in MOVA Comm
*	22-May-2013		7.0.6		AD		AK		M7.0.6			Issue M7.0.6
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

#include <MVTypes.h>
#include "gbltypes.h"

#define gnEMERGENCY_LINK           (-4)
#define gnPRIORITY_LINK            (-5)

Ccomshr							g_data_plans[xgnMAX_DATASETS];


MVBool		DSH_is_tod_removed_by_user();
MVInt		DSH_get_active_data_plan_number();
void		DSH_reset_data_plans_repository();
void		DSH_flag_data_plan_repository(MVInt8 data_plan_index);

MVBool		DSH_load_dataset_from_memory(MVInt8 plan_num);
MVStatus	DSH_activate_dataplan(MVInt8 plan_num);
MVStatus	DSH_fill_time_of_day_table(MVInt8 plan_num);
MVInt8		DSH_get_active_data_plan_num_based_on_tod_table();
MVInt		DSH_get_tod_plan(MVInt qtr_hour_of_week);
void		DSH_reset_time_of_day_table();
void		DSH_set_dataset_triggering(MVBool triggering_status);
MVBool		DSH_get_dataset_triggering();
void DSH_get_satflow_timing_period(MVInt PeriodInstance, MVInt* PeriodStart, MVInt* PeriodEnd);

void		DSH_get_json_string_of_dataset(char * buff);
MVBool		DSH_is_loading_dataset();

const Ccomshr * DSH_get_active_repository();