/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         dataset_handler.c
*
*  TITLE:        MOVA Kernel: The new (M8) handler of the dataset. Previously, "datahand.c"
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

#include <stdio.h>  // For sprintf
#include <string.h> // For memxxx

#include "dataset_handler.h"

#include "errhand.h"
#include "defines.h"
#include "mova_constants.h"
#include "mova_op.h"
#include "ui_dataaccess.h"
#include "core_interface.h"
#include "xml_dataset.h"

#include "sfstats.h"
#include "getentry.h"
#include "spec_cond.h"


MVInt			flagDstrig; /*user requested to change ToD using dstrig detectors*/


static MVInt8	active_data_plan_number = 0;
static MVBool	has_data_plan[xgnMAX_DATASETS] = { MV_FALSE, MV_FALSE, MV_FALSE, MV_FALSE};

static MVInt	tod_table[TOD_DAYS_NUM][TOD_TIMES_NUM]; /*Time of Day table*/
static MVBool	is_tod_exist = MV_FALSE;
static MVBool	is_loading_dataset = MV_FALSE;

static void			DSH_copy_data_plan_to_tcomshr(MVInt8 plan_num, MVBool is_dp_compatible);
static MVBool		DSH_is_data_plan_compatible(MVInt8 plan_num);
static MVInt		DHS_get_last_tod_plan_change(int day_of_week, int qtr_hour_of_day);

static void			json_str(char * buff, char * param_name, char * param_value);
static void			json_tel(char * buff, char * param_name, int * param_value);
static void			json_int(char * buff, char * param_name, MVInt param_value, MVInt8 format);
static void			json_uint32(char * buff, char * param_name, MVUInt32 param_value);

static void json_1d_array(char * buff, char * param_name, void * param_value, MVInt8 param_size, MVInt array_size, MVInt8 format);
static void json_2d_array(char * buff, char * param_name, void * param_value, MVInt8 param_size, MVInt dim1_size, MVInt dim2_size, MVInt dim2_max_size, MVInt8 format);

#define FMT_AS_IT_IS			0
#define FMT_F_ADD_5_DIV_10		1
#define FMT_I_ADD_5_DIV_10		2
#define FMT_F_DIV_10			3
#define FMT_I_DIV_10			4
#define FMT_F_ADD_10_DIV_10		5
#define FMT_F_DIV_2				6
#define FMT_LTYPE				7
#define FMT_CHANGE				8
#define FMT_TEL					9



/*Previous name: Datahand_GetActivePlan*/
MVInt DSH_get_active_data_plan_number()
{
	return (MVInt)active_data_plan_number;
}

MVBool DSH_is_tod_removed_by_user()
{
	return !is_tod_exist;
}

MVBool DSH_is_loading_dataset()
{
	return is_loading_dataset;
}

void DSH_reset_data_plans_repository()
{
	int i;

	for (i = 0; i < xgnMAX_DATASETS; i++)
	{
		has_data_plan[i] = MV_FALSE;
	}
}

void DSH_flag_data_plan_repository(MVInt8 data_plan_index)
{
	if (data_plan_index < xgnMAX_DATASETS)
	{
		has_data_plan[data_plan_index] = MV_TRUE;
	}
}

/* This function replaces GetActiveRepositoryPtr() that was in the old datahand */
const Ccomshr * DSH_get_active_repository()
{
	if (active_data_plan_number > 0)
		return &g_data_plans[active_data_plan_number - 1];
	else
		return 0x00;
}


MVBool DSH_load_dataset_from_memory(MVInt8 plan_num)
{
	extern char control[NumberOfForce + NumberOfExtra];

	char    mova_enabled_flag_old = 0;
	MVBool	is_loaded_succ = MV_FALSE;

	MVBool is_dp_compatible = MV_FALSE;

#if defined (MOVA_DEBUG)    
	diag_printf("Datahand_LoadDataset() called for plan %d\n", nPlanNum);
#endif    

	is_loading_dataset = MV_TRUE;

	/* Log a message that tells the user that the default
	* dataset has been loaded (important if it overwrote a
	* corrupt one - user might wonder where the corrupt one has gone) */
	Errhand_LogMOVAError(ERROR_ID_DEFAULT_LOADED, plan_num);

	is_dp_compatible = DSH_is_data_plan_compatible(plan_num);

	if (!is_dp_compatible)
	{
		/* Turn MOVA/VA flag off */
		mova_enabled_flag_old = Tcomshr->io[MOVA_ON];
		Tcomshr->io[MOVA_ON] = 0;
		
		is_loading_dataset = MV_FALSE;

		/* Turn MOVA off control (this call returns) */
		Switch_MOVA_Off(NO_SAV_MESSAGES, NO_REBOOT);
	}
	
	/* Put plan in working area */
	DSH_copy_data_plan_to_tcomshr(plan_num, is_dp_compatible);
	
	if (DSH_fill_time_of_day_table(plan_num) == MV_FAILURE)
		return MV_FALSE;
	
	active_data_plan_number = plan_num;

	/*When loading a new data plan, the DSTRIG should be enabled (i.e., plans are loaded when the det is active).*/
	flagDstrig = MV_TRUE;
		
	if (!is_dp_compatible)
	{
		/*Now, do some initialisation (the one that M7 was doing if the loaded dataplan is not compatible)*/
		CI_init_core_dynmaic_data();

		/* Mark1 and Mark2 are initialised here instead of reading the initialisation values from the dataset (.mds) file*/
		Tcomshr->mark1 = 0;
		Tcomshr->mark2 = 0;

		Tcomshr->io[MOVA_ON] = mova_enabled_flag_old; /* Replace MOVA/VA flag */
		CI_set_warmup_counter(-1); /* Start of warmup cycle */
		CI_set_last_stage(mxstag); /* Default last stage */
		CI_set_next_stage(mxstag); /* Default next stage */
		CI_set_current_stage(0);  /* Default current stage */
		CI_set_previous_marker(1); /* Warm start */

		/* Zero stage demands */
		CI_reset_stages_demands();

		/* Update the polarity of stage demands (just in case it has changed) */
		if (Tcomshr->stgdem == 0)
			Terrlog->stgoff = 1;
		else
			Terrlog->stgoff = 0;

		/* Clear all outputs (except sync pulse) */
		memset(control, Terrlog->stgoff, ForceArraySize * sizeof(char));
		OutputScan(MOVA_OFF);
	}

	/* Mark the RAM as being OK */
	Tcomshr->mark1 = 1234;

	CI_load_dynamic_data();

#ifdef M8_SPEC_COND
	/* update rules from new data plan */
	if (SC_LoadDatasetRules(plan_num - 1) == MV_FAILURE)
		return MV_FALSE;
#endif

	/* We succeeded in changing the plan */
	is_loaded_succ = MV_TRUE;

	/* Ensure that the Sat Flow stats are aware that a new plan has loaded */
	MovaSatFlowStats_NewDatasetLoaded_();
	
	is_loading_dataset = MV_FALSE;

	return is_loaded_succ;
}

MVStatus DSH_activate_dataplan(MVInt8 plan_num)
{
	if (has_data_plan[plan_num - 1] == MV_TRUE)
	{
		Tcomshr->mark2 = plan_num;
		return MV_SUCCESS;
	}
	else
	{
		return MV_FAILURE;
	}
}

static void DSH_copy_data_plan_to_tcomshr(MVInt8 plan_num, MVBool is_dp_compatible)
{
	signed char		t_ii[mxdets];
	signed char 	t_io[mxdets];

	int				t_ton[mxdets];
	int				t_toff[mxdets];

	signed char		t_deton[mxdets];
	signed char		t_detoff[mxdets];

	int				t_vc[mxdets];
	int				t_pregap[mxdets];

	int				t_greens[mxlink];

	int				t_snow = 0;

	/*Take a copy of the working data in Tcomshr if the data plan is compatible*/
	if (is_dp_compatible)
	{
		memcpy(t_ii, Tcomshr->ii, mxdets*sizeof(signed char));
		memcpy(t_io, Tcomshr->io, mxdets * sizeof(signed char));

		memcpy(t_ton, Tcomshr->ton, mxdets * sizeof(int));
		memcpy(t_toff, Tcomshr->toff, mxdets * sizeof(int));

		memcpy(t_deton, Tcomshr->deton, mxdets * sizeof(signed char));
		memcpy(t_detoff, Tcomshr->detoff, mxdets * sizeof(signed char));


		memcpy(t_vc, Tcomshr->vc, mxdets * sizeof(int));
		memcpy(t_pregap, Tcomshr->pregap, mxdets * sizeof(int));

		memcpy(t_greens, Tcomshr->greens, mxlink * sizeof(int));

		t_snow = Tcomshr->snow;		
	}
	

	memcpy(Tcomshr, &g_data_plans[plan_num - 1], sizeof(g_data_plans[plan_num - 1]));

	/*Restore the working data*/
	if (is_dp_compatible)
	{
		memcpy(Tcomshr->ii, t_ii, mxdets * sizeof(signed char));
		memcpy(Tcomshr->io, t_io, mxdets * sizeof(signed char));

		memcpy(Tcomshr->ton, t_ton, mxdets * sizeof(int));
		memcpy(Tcomshr->toff, t_toff, mxdets * sizeof(int));

		memcpy(Tcomshr->deton, t_deton, mxdets * sizeof(signed char));
		memcpy(Tcomshr->detoff, t_detoff, mxdets * sizeof(signed char));


		memcpy(Tcomshr->vc, t_vc, mxdets * sizeof(int));
		memcpy(Tcomshr->pregap, t_pregap, mxdets * sizeof(int));

		memcpy(Tcomshr->greens, t_greens, mxlink * sizeof(int));

		Tcomshr->snow = t_snow;
	}
}

/*This function checks if the data plan (to be loaded) is compatible with the active one (in Tcomshr).*/
static MVBool DSH_is_data_plan_compatible(MVInt8 plan_num)
{
	if (Tcomshr->nlinks != g_data_plans[plan_num - 1].nlinks) return MV_FALSE;

	if (Tcomshr->nlanes != g_data_plans[plan_num - 1].nlanes) return MV_FALSE;

	if (Tcomshr->stages != g_data_plans[plan_num - 1].stages) return MV_FALSE;

	return MV_TRUE;
}

MVStatus DSH_fill_time_of_day_table(MVInt8 plan_num)
{
	MVInt i = 0, j;
	MVInt day_code, dp_num, qtr_hour_index;

	/*0. Check the validity of the passed plan_num*/
	if (has_data_plan[plan_num - 1] == MV_FALSE)
		return MV_FAILURE;


	/*1. Reset the current ToD Table*/
	DSH_reset_time_of_day_table();


	/*2. Does the data plan has ToD table*/
	if (g_data_plans[plan_num - 1].dattim[0] == -1)
		return MV_SUCCESS;

	is_tod_exist = MV_TRUE;

	/*3. Read the DATTIM and DATNUM to fill the ToD table*/
	while (g_data_plans[plan_num - 1].dattim[i] != -1)
	{
		/*g_data_plans[plan_num - 1].datnum[i]*/
		day_code = g_data_plans[plan_num - 1].datnum[i] / 100;
		dp_num = g_data_plans[plan_num - 1].datnum[i] - (day_code * 100);
		qtr_hour_index = g_data_plans[plan_num - 1].dattim[i];

		/* calculate day codes and data set numbers */
		/* Saturday is day 1 */
		if (day_code >= 0 && day_code <= 6)
		{ 
			/* single day code */
			tod_table[day_code][qtr_hour_index] = dp_num;
		}
		else if (day_code == 7) 
		{ 
			/* all days-of-week */
			for (j = 1; j <= 7; j++)
			{
				tod_table[j - 1][qtr_hour_index] = dp_num;
			}
		}
		else if (day_code == 8) 
		{ 
			/* all days-of-week exp Sun */
			for (j = 1; j <= 7; j++)
			{
				if (j != 2) tod_table[j - 1][qtr_hour_index] = dp_num;
			}
		}
		else if (day_code == 9) 
		{ 
			/* all days-of-week exp Sat & Sun */
			for (j = 3; j <= 7; j++)
			{
				tod_table[j - 1][qtr_hour_index] = dp_num;
			}
		}
		i++;
	}

	return MV_SUCCESS;
}

MVInt8 DSH_get_active_data_plan_num_based_on_tod_table()
{
	MovaDateTime mova_date_time;
	MVInt day, hour, min;
	MVInt i, j;

	/*Get the current MOVA date/time using a function that wraps Siemens-specific code */
	Get_DateTime_MANUFAC(&mova_date_time);

	day = mova_date_time.nDayOfWeek;
	hour = mova_date_time.nHours;
	min = mova_date_time.nMinutes;

	/* work out which cell represents current time*/
	j = (hour * 4) + ((min + 7) / 15); /* set i and j to current time */
	i = day;

	/* Use function to get last ToD plan change */
	/* Get the last time of day plan change working backwards from the current time */
	return (MVInt8)DHS_get_last_tod_plan_change(i, j);
}

static MVInt DHS_get_last_tod_plan_change(int day_of_week, int qtr_hour_of_day)
{
	MVInt			tod_plan;
	MVInt			day_loop;
	MVInt			qtr_hour_loop;
	MVBool			is_found = MV_FALSE;
	BOOL			is_finished = MV_FALSE;

	day_loop = day_of_week;
	qtr_hour_loop = qtr_hour_of_day;


	/* loop backwards to locate the most recent time which holds a dataset */
	while (!is_found && !is_finished)
	{
		if (tod_table[day_loop][qtr_hour_loop] != 0)
		{
			is_found = MV_TRUE;
		}
		else if (qtr_hour_loop != 0)
		{
			qtr_hour_loop--;
		}
		else if (day_loop != 0)
		{
			/* 15 minute sector must be zero, so goto sector 95 of previous day*/
			day_loop--;
			qtr_hour_loop = 95;
		}
		else
		{
			/* TimeofDay[0][0] so goto last sector of previous week [6][95]*/
			day_loop = 6;
			qtr_hour_loop = 95;
		}

		/* Determine whether we've got back to where we started
		* (prevents us looping forever) */
		is_finished = ((day_loop == day_of_week) && (qtr_hour_loop == qtr_hour_of_day));
	}

	tod_plan = tod_table[day_loop][qtr_hour_loop];

	if (tod_plan<1 || tod_plan>4) /* check that no errors have been stored*/
	{
		tod_plan = 0; /* if an error set to an invalid plan number */
	}

	return (tod_plan);
}

/* Previous name: Datahand_GetTimeOfDayPlan( ... ) */
MVInt DSH_get_tod_plan(MVInt qtr_hour_of_week)
{
	MVInt tod_plan;
	MVInt day_of_week;
	MVInt qtr_hour_of_day;

	if (is_tod_exist == MV_FALSE)
		return 0;

	/* Get zero-based day of week */
	day_of_week = qtr_hour_of_week / TOD_TIMES_NUM;

	/* Get zero-based quarter hour of day */
	qtr_hour_of_day = qtr_hour_of_week % TOD_TIMES_NUM;

	/* Get the last Time of Day plan change for the current time */
	tod_plan = DHS_get_last_tod_plan_change(day_of_week, qtr_hour_of_day);

	return tod_plan;
} 

void DSH_reset_time_of_day_table()
{
	/* Zero the Time of Day data */
	memset(tod_table, 0, sizeof(tod_table));

	is_tod_exist = MV_FALSE;

	/* Put a stop to any pending plan change
	* (as it's likely to be connected with the plan
	* we've just erased) */
	Tcomshr->mark2 = 0;
}


/*If you want to enable the DSTRIG (i.e., plans are loaded when the det is active), then set it to MV_TRUE*/
void DSH_set_dataset_triggering(MVBool triggering_status)
{
	flagDstrig = triggering_status;
}

MVBool DSH_get_dataset_triggering()
{
	return flagDstrig;
}

/*Previous name: Datahand_GetSatFlowTimingPeriod*/
void DSH_get_satflow_timing_period(MVInt PeriodInstance, MVInt* PeriodStart, MVInt* PeriodEnd)
{
	*PeriodStart = Tcomshr->sftims[PeriodInstance - 1][0];
	*PeriodEnd = Tcomshr->sftims[PeriodInstance - 1][1];
}


void DSH_get_json_string_of_dataset(char * buff)
{
	int i, j;
	char rev_det[MAX_DETS_TYPES][MAX_LANES]; /*a temp arrays to hold the reversed detectors array*/
	char rev_assocd[MAX_ASSOC_DET][MAX_LANES]; /*a temp arrays to hold the reversed assoc dets array*/


	sprintf(buff, "{\"MessageType\":\"RspSiteFullData\", \"Params\":{");

	json_str(buff, "FileName", Tcomshr->fname);
	json_str(buff, "Title", Tcomshr->title);
	json_str(buff, "Version", Tcomshr->version);
	json_str(buff, "Created_Date", Tcomshr->date);
	json_str(buff, "Created_Time", Tcomshr->time);

	json_int(buff, "STAGES", Tcomshr->stages, FMT_AS_IT_IS);
	json_int(buff, "NLANES", Tcomshr->nlanes, FMT_AS_IT_IS);
	json_int(buff, "NLINKS", Tcomshr->nlinks, FMT_AS_IT_IS);
	json_int(buff, "MAINST", Tcomshr->mainst, FMT_AS_IT_IS);
	json_int(buff, "TOTALG", Tcomshr->totalg, FMT_I_DIV_10);
	json_int(buff, "OPTYPE", Tcomshr->optype, FMT_AS_IT_IS);
	json_int(buff, "DETON", Tcomshr->detton, FMT_AS_IT_IS);
	json_int(buff, "STAGON", Tcomshr->stagon, FMT_AS_IT_IS);
	json_int(buff, "PHASON", Tcomshr->phason, FMT_AS_IT_IS);
	json_int(buff, "STGDEM", Tcomshr->stgdem, FMT_AS_IT_IS);
		
	json_2d_array(buff, "LGREEN", (char*)Tcomshr->lgreen, sizeof(char), Tcomshr->stages, Tcomshr->nlinks, MAX_LINKS, FMT_AS_IT_IS);
	
	json_1d_array(buff, "LPHASE", Tcomshr->lphase, sizeof(int), Tcomshr->nlinks, FMT_AS_IT_IS);
	json_2d_array(buff, "SDCODE", (int*)Tcomshr->sdcode, sizeof(int), Tcomshr->stages, Tcomshr->nlinks, MAX_LINKS, FMT_AS_IT_IS);
	
	/* ------------- Time dependent data */
	json_1d_array(buff, "DATTIM", Tcomshr->dattim, sizeof(int), xgnTIME_OF_DAY_ARRAY_SIZE, FMT_AS_IT_IS);
	json_1d_array(buff, "DATNUM", Tcomshr->datnum, sizeof(int), xgnTIME_OF_DAY_ARRAY_SIZE, FMT_AS_IT_IS);
	

	/*-------------- Stages data -------------- */
	json_1d_array(buff, "MIN", Tcomshr->min, sizeof(int), Tcomshr->stages, FMT_F_ADD_5_DIV_10);
	json_1d_array(buff, "MAX", Tcomshr->max, sizeof(int), Tcomshr->stages, FMT_I_ADD_5_DIV_10);
	json_1d_array(buff, "LOWMAX", Tcomshr->lowmax, sizeof(int), Tcomshr->stages, FMT_I_ADD_5_DIV_10);

	json_1d_array(buff, "OCCUAL_On", Tcomshr->occual[0], sizeof(int), Tcomshr->stages, FMT_F_DIV_10);
	json_1d_array(buff, "OCCUAL_Off", Tcomshr->occual[1], sizeof(int), Tcomshr->stages, FMT_F_DIV_10);

#ifndef M8_IMPORVED_BUS_PRIORITY
	json_1d_array(buff, "PFACIL", Tcomshr->pfacil, sizeof(char), Tcomshr->stages, FMT_AS_IT_IS);
#endif

	json_1d_array(buff, "EMXMAX", Tcomshr->emxmax, sizeof(int), Tcomshr->stages, FMT_F_DIV_10);
	json_1d_array(buff, "PRXMAX", Tcomshr->prxmax, sizeof(int), Tcomshr->stages, FMT_F_DIV_10);
	json_1d_array(buff, "PEDMAX1", Tcomshr->pedmax[0], sizeof(int), Tcomshr->stages, FMT_I_DIV_10);
	json_1d_array(buff, "PEDMAX2", Tcomshr->pedmax[1], sizeof(int), Tcomshr->stages, FMT_AS_IT_IS);
	json_1d_array(buff, "BUSMAX1", Tcomshr->busmax[0], sizeof(int), Tcomshr->stages, FMT_I_DIV_10); /*M8*/
	json_1d_array(buff, "BUSMAX2", Tcomshr->busmax[1], sizeof(int), Tcomshr->stages, FMT_AS_IT_IS); /*M8*/

	json_2d_array(buff, "PFACILM", (int*)Tcomshr->pfacilm, sizeof(int), Tcomshr->stages, Tcomshr->nlinks, MAX_LINKS, FMT_AS_IT_IS); /*M8*/

	json_2d_array(buff, "CHANGE", (int*)Tcomshr->change, sizeof(int), Tcomshr->stages, Tcomshr->stages, MAX_STAGES, FMT_CHANGE);

	/*-------------- Links data -------------- */
	json_1d_array(buff, "LTYPE", Tcomshr->ltype, sizeof(int), Tcomshr->nlinks, FMT_LTYPE);
	json_1d_array(buff, "LMIN", Tcomshr->lmin, sizeof(int), Tcomshr->nlinks, FMT_F_ADD_5_DIV_10);
	json_1d_array(buff, "LLATOT", Tcomshr->llatot, sizeof(int), Tcomshr->nlinks, FMT_AS_IT_IS);
	json_2d_array(buff, "LLANES", (int*)Tcomshr->llanes, sizeof(int), Tcomshr->nlinks, mxlnOnlink, mxlnOnlink, FMT_AS_IT_IS);
	json_1d_array(buff, "ESLMAX", Tcomshr->eslmax, sizeof(int), Tcomshr->nlinks, FMT_I_DIV_10);
	json_1d_array(buff, "LOSTIM", Tcomshr->lostim, sizeof(int), Tcomshr->nlinks, FMT_I_DIV_10);
	json_1d_array(buff, "STOPEN", Tcomshr->stopen, sizeof(int), Tcomshr->nlinks, FMT_I_DIV_10);
	json_1d_array(buff, "WAITCH", Tcomshr->waitch, sizeof(int), Tcomshr->nlinks, FMT_AS_IT_IS);

	json_2d_array(buff, "EPDET", (int*)Tcomshr->epdet, sizeof(int), Tcomshr->nlinks, 2, 2, FMT_AS_IT_IS);
	json_2d_array(buff, "EPEXT", (int*)Tcomshr->epext, sizeof(int), Tcomshr->nlinks, 2, 2, FMT_F_DIV_10);

	json_1d_array(buff, "HOLDTM", Tcomshr->holdtm, sizeof(int), Tcomshr->nlinks, FMT_F_DIV_10);
	json_1d_array(buff, "CANDET", Tcomshr->candet, sizeof(int), Tcomshr->nlinks, FMT_AS_IT_IS);

	json_1d_array(buff, "BUSWGT", Tcomshr->buswgt, sizeof(int), Tcomshr->nlinks, FMT_AS_IT_IS); /*M8*/

	json_1d_array(buff, "EPCRT1", Tcomshr->epcrt[0], sizeof(int), Tcomshr->nlinks, FMT_I_DIV_10); /*M8*/
	json_1d_array(buff, "EPCRT2", Tcomshr->epcrt[1], sizeof(int), Tcomshr->nlinks, FMT_I_DIV_10); /*M8*/


	//---------------- Lanes Data ------------------//
	/*Reverse the DET array to be able to include each detector type in a separate param*/
	for (i = 0; i < MAX_LANES; i++)
		for (j = 0; j < MAX_DETS_TYPES; j++)
			rev_det[j][i] = Tcomshr->det[i][j];

	for (i = 0; i < MAX_LANES; i++)
		for (j = 0; j < MAX_ASSOC_DET; j++)
			rev_assocd[j][i] = Tcomshr->assocd[i][j];

	json_1d_array(buff, "X", rev_det[X_DET], sizeof(char), Tcomshr->nlanes, FMT_AS_IT_IS);
	json_1d_array(buff, "DX", Tcomshr->dx, sizeof(int), Tcomshr->nlanes, FMT_AS_IT_IS);
	json_1d_array(buff, "IN", rev_det[IN_DET], sizeof(char), Tcomshr->nlanes, FMT_AS_IT_IS);
	json_1d_array(buff, "DIN", Tcomshr->din, sizeof(int), Tcomshr->nlanes, FMT_AS_IT_IS);
	json_1d_array(buff, "DSHORT", Tcomshr->dshort, sizeof(int), Tcomshr->nlanes, FMT_AS_IT_IS);
	json_1d_array(buff, "OUT", rev_det[OUT_DET], sizeof(char), Tcomshr->nlanes, FMT_AS_IT_IS);
	json_1d_array(buff, "IN_SINK", rev_det[IN_SINK_DET], sizeof(char), Tcomshr->nlanes, FMT_AS_IT_IS);
	json_1d_array(buff, "IN_SINK2", rev_det[IN_SINK2_DET], sizeof(char), Tcomshr->nlanes, FMT_AS_IT_IS);
	json_1d_array(buff, "X_SINK", rev_det[X_SINK_DET], sizeof(char), Tcomshr->nlanes, FMT_AS_IT_IS);
	json_1d_array(buff, "X_SINK2", rev_det[X_SINK2_DET], sizeof(char), Tcomshr->nlanes, FMT_AS_IT_IS);
	json_1d_array(buff, "COMB_X", rev_det[COMB_X_DET], sizeof(char), Tcomshr->nlanes, FMT_AS_IT_IS);
	json_1d_array(buff, "STOP_LINE", rev_det[STOP_LINE_DET], sizeof(char), Tcomshr->nlanes, FMT_AS_IT_IS);
	json_1d_array(buff, "ALT_UP", rev_det[ALT_UP_DET], sizeof(char), Tcomshr->nlanes, FMT_AS_IT_IS);
	json_1d_array(buff, "ALT_DOWN", rev_det[ALT_DOWN_DET], sizeof(char), Tcomshr->nlanes, FMT_AS_IT_IS);
	
	json_1d_array(buff, "ASOCC_DET_1", rev_assocd[0], sizeof(char), Tcomshr->nlanes, FMT_AS_IT_IS);
	json_1d_array(buff, "ASOCC_DET_2", rev_assocd[1], sizeof(char), Tcomshr->nlanes, FMT_AS_IT_IS);
	
	json_1d_array(buff, "CSPEED", Tcomshr->cspeed, sizeof(int), Tcomshr->nlanes, FMT_AS_IT_IS);
	json_1d_array(buff, "SATINC", Tcomshr->satinc, sizeof(int), Tcomshr->nlanes, FMT_F_DIV_10);
	json_1d_array(buff, "STLOST", Tcomshr->stlost, sizeof(int), Tcomshr->nlanes, FMT_F_ADD_10_DIV_10);
	json_1d_array(buff, "COMTIM", Tcomshr->comtim, sizeof(int), Tcomshr->nlanes, FMT_F_DIV_10);
	json_1d_array(buff, "LANEWF", Tcomshr->lanewf, sizeof(int), Tcomshr->nlanes, FMT_AS_IT_IS);
	json_1d_array(buff, "USESF", Tcomshr->usesf, sizeof(int), Tcomshr->nlanes, FMT_AS_IT_IS);
	json_1d_array(buff, "USEST", Tcomshr->usest, sizeof(int), Tcomshr->nlanes, FMT_AS_IT_IS);
	json_1d_array(buff, "EXITAL", Tcomshr->exital, sizeof(int), Tcomshr->nlanes, FMT_AS_IT_IS);

	json_1d_array(buff, "MAXMIN", Tcomshr->maxmin, sizeof(int), Tcomshr->nlanes, FMT_F_DIV_10);
	json_1d_array(buff, "CRUSIN", Tcomshr->crusin, sizeof(int), Tcomshr->nlanes, FMT_F_DIV_2);
	json_1d_array(buff, "CRUSX", Tcomshr->crusx, sizeof(int), Tcomshr->nlanes, FMT_F_DIV_2);

	json_1d_array(buff, "MOVQIN", Tcomshr->movqin, sizeof(int), Tcomshr->nlanes, FMT_F_DIV_10);
	json_1d_array(buff, "MOVEQX", Tcomshr->moveqx, sizeof(int), Tcomshr->nlanes, FMT_F_DIV_10);
	json_1d_array(buff, "QMINON", Tcomshr->qminon, sizeof(int), Tcomshr->nlanes, FMT_F_DIV_10);

	json_1d_array(buff, "XOSAT", Tcomshr->xosat, sizeof(int), Tcomshr->nlanes, FMT_AS_IT_IS);
	json_1d_array(buff, "OSATTM", Tcomshr->osattm, sizeof(int), Tcomshr->nlanes, FMT_F_DIV_10);
	json_1d_array(buff, "OSATCC", Tcomshr->osatcc, sizeof(int), Tcomshr->nlanes, FMT_AS_IT_IS);

	json_1d_array(buff, "GAMBER", Tcomshr->gamber, sizeof(int), Tcomshr->nlanes, FMT_F_DIV_2);
	json_1d_array(buff, "SATGAP", Tcomshr->satgap, sizeof(int), Tcomshr->nlanes, FMT_F_DIV_10);
	json_1d_array(buff, "CRITG", Tcomshr->critg, sizeof(int), Tcomshr->nlanes, FMT_F_DIV_10);

	json_1d_array(buff, "PCRITG", Tcomshr->pcritg, sizeof(int), Tcomshr->nlanes, FMT_AS_IT_IS); /*M8*/
	json_1d_array(buff, "RCRITG", Tcomshr->rcritg, sizeof(int), Tcomshr->nlanes, FMT_AS_IT_IS); /*M8*/

	json_1d_array(buff, "OSATCX", Tcomshr->osatcx, sizeof(int), Tcomshr->nlanes, FMT_AS_IT_IS);
	json_1d_array(buff, "OSATAL", Tcomshr->osatal, sizeof(int), Tcomshr->nlanes, FMT_AS_IT_IS);

	/*Bonus data (per lane)*/
	json_1d_array(buff, "BONBC", Tcomshr->bonbc, sizeof(int), Tcomshr->nlanes, FMT_AS_IT_IS);

	/*Bonus data (per link)*/
	json_1d_array(buff, "BONTIM", Tcomshr->bontim, sizeof(int), Tcomshr->nlinks, FMT_I_ADD_5_DIV_10);
	json_1d_array(buff, "BONCUT", Tcomshr->boncut, sizeof(int), Tcomshr->nlinks, FMT_AS_IT_IS);
	json_1d_array(buff, "MIXOUT", Tcomshr->mixout, sizeof(int), Tcomshr->nlinks, FMT_AS_IT_IS);

	json_1d_array(buff, "MXCALL", Tcomshr->mxcall, sizeof(int), Tcomshr->nlinks, FMT_F_DIV_10); /*M8*/
	json_1d_array(buff, "MXCNCL", Tcomshr->mxcncl, sizeof(int), Tcomshr->nlinks, FMT_F_DIV_10); /*M8*/
	
	json_1d_array(buff, "SHORTL", Tcomshr->shortl, sizeof(int), Tcomshr->nlinks, FMT_AS_IT_IS);
	json_1d_array(buff, "NMAINL", Tcomshr->nmainl, sizeof(int), Tcomshr->nlinks, FMT_AS_IT_IS);
		

	/*-------------- Constants ----------------------*/
	json_int(buff, "NDETS", Tcomshr->ndets, FMT_AS_IT_IS);
	json_int(buff, "SCAN", Tcomshr->scan, FMT_F_DIV_10);
	json_int(buff, "MINEXT", Tcomshr->minext, FMT_F_DIV_2);
	json_int(buff, "MAXEXT", Tcomshr->maxext, FMT_F_DIV_2);
	json_int(buff, "ADDGAP", Tcomshr->addgap, FMT_F_DIV_10);
	json_int(buff, "SUBGAP", Tcomshr->subgap, FMT_F_DIV_10);

	json_2d_array(buff, "SatFlowTimePeriods", (int*)Tcomshr->sftims, sizeof(int), xgnSFTIMS_MAX, 2, 2, FMT_AS_IT_IS);

	json_1d_array(buff, "DatasetTriggering", Tcomshr->dstrig, sizeof(int), 4, FMT_AS_IT_IS);

	json_2d_array(buff, "ErrorValues", (int*)Tcomshr->errval, sizeof(int), ERROR_DATA_TYPES_NUM, xgnERRORS_NUM_MAX, xgnERRORS_NUM_MAX, FMT_AS_IT_IS);

	json_int(buff, "TMA_Period", Tcomshr->tmaprd, FMT_AS_IT_IS);

	json_tel(buff, "TEL", Tcomshr->tel);

	json_uint32(buff, "CRC32", XML_DataSet_GetCRC32());

	strcat(buff, "\"TempParam\":\"\"}}");
}

static void json_str(char * buff, char * param_name, char * param_value)
{
	char	tmp_buffer[256];

	sprintf(tmp_buffer, "\"%s\":\"%s\",", param_name, param_value);

	strcat(buff, tmp_buffer);
}

static void json_int(char * buff, char * param_name, MVInt param_value, MVInt8 format)
{
	char tmp_buffer[24];
		
	switch (format)
	{
		case FMT_AS_IT_IS:
			sprintf(tmp_buffer, "\"%s\":%d,", param_name, param_value);
			break;

		case FMT_I_DIV_10:
			sprintf(tmp_buffer, "\"%s\":%d,", param_name, param_value/10);
			break;

		case FMT_F_DIV_10:
			sprintf(tmp_buffer, "\"%s\":%.1f,", param_name, (float)(param_value/10.0F));
			break;

		case FMT_F_DIV_2:
			sprintf(tmp_buffer, "\"%s\":%.1f,", param_name, (float)(param_value / 2.0F));
			break;

		default:
			sprintf(tmp_buffer, "\"%s\":%d,", param_name, param_value / 10);
			break;
	}
	
	strcat(buff, tmp_buffer);
}

static void json_uint32(char * buff, char * param_name, MVUInt32 param_value)
{
	char tmp_buffer[30];

	sprintf(tmp_buffer, "\"%s\":%lu,", param_name, param_value);

	strcat(buff, tmp_buffer);
}

static void json_1d_array(char * buff, char * param_name, void * param_value, MVInt8 param_size, MVInt array_size, MVInt8 format)
{
	MVInt i;

	char v_str[11];
	char tmp_buffer[20 + (11 * 64)];
	void * param_value_ptr = param_value;
	MVInt v;


	sprintf(tmp_buffer, "\"%s\":[", param_name);

	for (i = 0; i < array_size; i++)
	{
		if (param_size == sizeof(char))
		{
			/*No checking on the format here because the usage of this function with 'char' is always AS_IT_IS*/
			sprintf(v_str, "%d", *(char*)(param_value_ptr));
		}
		else
		{
			v = *(int*)(param_value_ptr);

			switch (format)
			{
				case FMT_AS_IT_IS:
					sprintf(v_str, "%d", v);
					break;

				case FMT_F_ADD_5_DIV_10:
					sprintf(v_str, "%.1f", (v+5)/10.0F);
					break;

				case FMT_I_ADD_5_DIV_10:
					sprintf(v_str, "%d", (v + 5) / 10);
					break;

				case FMT_F_DIV_10:
					sprintf(v_str, "%.1f", ( v / 10.0F));
					break;

				case FMT_I_DIV_10:
					sprintf(v_str, "%d", (v / 10));
					break;

				case FMT_F_ADD_10_DIV_10:
					sprintf(v_str, "%.1f", (v + 10) / 10.0F);
					break;
					
				case FMT_F_DIV_2:
					sprintf(v_str, "%.1f", (v / 2.0F));
					break;

				case FMT_LTYPE:
					if (v == gnEMERGENCY_LINK) v = 999;
					if (v == gnPRIORITY_LINK) v = 99;
					sprintf(v_str, "%d", v);
					break;
			
				default:
					sprintf(v_str, "%d", 0); /*Should never come here*/
			}
			
		}
		strcat(tmp_buffer, v_str);

		if (i < (array_size - 1))
			strcat(tmp_buffer, ",");

		/*Casting to char here to advance the pointer with bytes (not based on its type)*/
		param_value_ptr = (char *)param_value_ptr + param_size;
	}

	strcat(tmp_buffer, "],");
	
	strcat(buff, tmp_buffer);
}


static void json_2d_array(char * buff, char * param_name, void * param_value, MVInt8 param_size, MVInt dim1_size, MVInt dim2_size, MVInt dim2_max_size, MVInt8 format)
{
	MVInt i; /*First dim loop counter*/
	MVInt j; /*Second dim loop counter*/
	MVInt v; /*The value*/
	MVInt t = 5; /*Temp var for FMT_CHANGE*/

	char v_str[11];
	char tmp_buffer[ 20 + (11 * 64 * 64)];

	sprintf(tmp_buffer, "\"%s\":[", param_name);

	for (i = 0; i < dim1_size; i++)
	{
		strcat(tmp_buffer, "[");

		for (j = 0; j < dim2_size; j++)
		{
			if (param_size == sizeof(char))
			{
				/*No checking on the format here because the usage of this function with 'char' is always AS_IT_IS*/
				sprintf(v_str, "%d", *(((char*)param_value + i*dim2_max_size) + j));
			}
			else
			{
				v = *(((int*)param_value + i*dim2_max_size) + j);

				switch (format)
				{
					case FMT_AS_IT_IS:
						sprintf(v_str, "%d", v);
						break;

					case FMT_F_DIV_10:
						sprintf(v_str, "%.1f", (v / 10.0F));
						break;

					case FMT_CHANGE:
						if (v < 0) t = -5;
						if (v == -1) t = -10;
						v = (v + t) / 10;
						sprintf(v_str, "%d", v);
						break;

					default:
						sprintf(v_str, "%d", 0); /*Should never come here*/
				}
			}
			

			strcat(tmp_buffer, v_str);

			if (j < (dim2_size - 1))
				strcat(tmp_buffer, ",");
		}

		strcat(tmp_buffer, "]");

		if (i < (dim1_size - 1))
			strcat(tmp_buffer, ",");
	}

	strcat(tmp_buffer, "],");

	strcat(buff, tmp_buffer);
}


static void	json_tel(char * buff, char * param_name, int * param_value)
{
	MVInt8 i;
	MVInt sum = 0;

	char	tel_str[MAX_TEL + 1];
	char	tmp_buffer[MAX_TEL + 20];
	char	tmp_digit[5];

	tel_str[0] = 0;

	for (i = 0; i < MAX_TEL; i++)
	{
		/*This is condition is here to skip adding the TEL_Checksum that is stored at the end of tel number. */
		if (param_value[i] > 9) 
			break;

		snprintf(tmp_digit, sizeof(tmp_digit), "%d", param_value[i]);
		strcat(tel_str, tmp_digit);

		sum += param_value[i];
	}

	if (sum > 0)
	{
		sprintf(tmp_buffer, "\"%s\":\"%s\",", param_name, tel_str);
	}
	else
	{
		sprintf(tmp_buffer, "\"%s\":\"0\",", param_name);
	}

	strcat(buff, tmp_buffer);
}
