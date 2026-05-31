
/*******************************************************************************
*
*                      MOVA KERNEL
*
*  FILE:         spec_cond.c
*
*  TITLE:        Mova Kernel: Special Conditioning
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



/*
    Name:           spec_cond.c
    Author (Date):  smistry, 04/07/2016
    Platform:       
    Purpose:        SpecCond handles the special conditioning within the 
                    MOVA kernel.  
*/


/*
*************************
*	Includes
*************************
*/

#include <string.h>				// for strlen only

#include "MVTrace.h"

#include "core_interface.h"
#include "dataset_handler.h"
#include "dataset_interface.h"
#include "delay_and_stops_optimiser.h"
#include "detectors_handler.h"
#include "generalfunc.h"				// for Safe_Strncpy only
#include "junction_handler.h"
#include "lanes_handler.h"
#include "links_handler.h"
#include "stages_handler.h"
#include "timers_manager.h"

#include "spec_cond.h"

// defined in detscn.c
extern MVBool GetSingleDetectorStatus(uint8 DetIdx);
extern void GetDetectorsStatus(char * DetsStatus);
extern void SetDetectorStatus(char* Status, int NumDetectors);

// defined in genstg.c
extern void SetForceBits(int index, char val);

#if defined(_DEBUG) && defined(WIN32)
// use to measure time spent in function
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#endif

/*
*************************
*	Defines
*************************
*/

#define g_MODULE_NAME       ("SpecCond")

#define MAX_NUMB_RULES (30)
#define MAX_NUMB_PARAM (30)
#define MAX_STR_LEN (30)

#define STR_RS_DET_CHAN ((xmlChar *)"Detector_Channel")
#define STR_RS_OUT_CHAN ((xmlChar *)"Output_Channel")
#define STR_RS_USER_VAR ((xmlChar *)"User_Variable")

#define STR_PS_DET_CHAN ((xmlChar *)"DetectorChannel")
#define STR_PS_MOVA_VAR ((xmlChar *)"MOVAVariable")
#define STR_PS_NUM_VAL ((xmlChar *)"NumericValue")
#define STR_PS_USER_VAR ((xmlChar *)"UserVariable")



typedef enum
{
	RS_UNKNOWN,
	RS_DET_CHAN,
	RS_OUT_CHAN,
	RS_USER_VAR_LOGIC,
	RS_USER_VAR_NUMB
} ResultSource;


/*
*************************
*	Structures
*************************
*/


/*
 * The parameter value is either logical, numerical or an array
 * of values so use union to hold all types in one place
 */
typedef union
{
	MVBool logic;
	double numb;
	double narray[SC_MAX_NUMB_VALUES];
} ParameterValue;


typedef struct
{
	/* name of parameter operator */
	const xmlChar *op;

	/* pointer to function processing this operator */
	void (*func)(ParameterValue *, ParameterValue);
} ParameterOperator;


typedef struct
{
	/* name of user variable */
	char name[MAX_STR_LEN];

	/* value stored in user variable */
	ParameterValue value;
} UserVariable;


typedef struct Parameter_t
{
	/* number of values stored in array */
	uint8 nvals;
	ParameterValue value;

	/* number of the rule defining the user variable */
	MVUInt unumb;

	/* pointer to the user variable data */
	UserVariable * uptr;

	/* string holding MOVA variable name */
	char mvar[MAX_STR_LEN];

	/* pointer to the operator function */
	void (*opfunc)(ParameterValue *, ParameterValue);

	/* pointer to the value get function */
	void (*getfunc)(struct Parameter_t, ParameterValue *);

	/* index of MOVA variable in array defined below */
	/* Note index in range 1->N, zero means unused   */
	MVUInt mvnumb;
} Parameter;


typedef struct
{
	/* name of MOVA variable */
	const xmlChar *name;

	/* pointer to function processing this variable */
	double (*func)(uint8 index);

	/* pointer to function checking this variables content */
	MVStatus (*check)(Parameter * parm);
} MovaOperator;


typedef struct
{
	// timer settings from configuration file
	uint16 call;
	uint16 cancel;
	uint16 delay;
	uint16 duration;
	uint16 persistence;
	uint16 pulseMark;
	uint16 pulseSpace;

	// starting state
	MVBool prevstate;

	// call / cancel states
	MVBool cc_state;			// current call/cancel state
	uint16 cc_time;				// call/cancel elapsed time
	MVBool allow_delay;			// flag if delay is allowed

	// delay / duration / persistence states
	MVBool delay_state;			// current delay state
	uint16 delay_time;			// delay elapsed time
	uint16 persist_time;		// persistence elapsed time

	// pulse mark / space states
	MVBool pms_state;			// current pulse mark/space state
	uint16 pms_time;			// pulse mark/space elapsed time
	MVBool in_pulse_space;		// flag if in pulse space
} Timers;


typedef struct Result_t
{
	/* output type of this rule */
	ResultSource source;

	/* user variable related data */
	UserVariable user_var;

	/* pointer to putter function associated to this output type*/
	void (*putfunc)(struct Result_t * res, MVBool val);
} Result;


typedef struct
{
	/* number of parameters in this rule*/
	MVUInt numb_param;

	/* array of parameters for this rule */
	Parameter param[MAX_NUMB_PARAM];

	/* timer values for this rule */
	Timers preOutput;

	/* storage for output result data */
	Result res;
} Rule;


typedef struct
{
	/* number of rules defined */
	MVUInt nrules;

	/* storage array for all rules in this data plan */
	Rule rule[MAX_NUMB_RULES];
} Rules;


// Global rule storage
static Rules g_rules[xgnMAX_DATASETS];

// Temporary global rule storage during data file transfer
static Rules g_temp_rules[xgnMAX_DATASETS];

// This holds the output detector values as the rules are being processed
char temp_detsin[mxdets] = { 0 };

static void SC_AndFunc(ParameterValue * pv1, ParameterValue pv2);
static void SC_NandFunc(ParameterValue * pv1, ParameterValue pv2);
static void SC_NorFunc(ParameterValue * pv1, ParameterValue pv2);
static void SC_NxorFunc(ParameterValue * pv1, ParameterValue pv2);
static void SC_OrFunc(ParameterValue * pv1, ParameterValue pv2);
static void SC_XorFunc(ParameterValue * pv1, ParameterValue pv2);
static void SC_EqFunc(ParameterValue * pv1, ParameterValue pv2);
static void SC_GtFunc(ParameterValue * pv1, ParameterValue pv2);
static void SC_LtFunc(ParameterValue * pv1, ParameterValue pv2);
static void SC_MinusFunc(ParameterValue * pv1, ParameterValue pv2);
static void SC_NeFunc(ParameterValue * pv1, ParameterValue pv2);
static void SC_PlusFunc(ParameterValue * pv1, ParameterValue pv2);


static const ParameterOperator opcalls[] =
{
	{ (xmlChar *)"AND", &SC_AndFunc },
	{ (xmlChar *)"NAND", &SC_NandFunc },
	{ (xmlChar *)"NOR", &SC_NorFunc },
	{ (xmlChar *)"NXOR", &SC_NxorFunc },
	{ (xmlChar *)"OR", &SC_OrFunc },
	{ (xmlChar *)"XOR", &SC_XorFunc },
	{ (xmlChar *)"EQ", &SC_EqFunc },
	{ (xmlChar *)"GT", &SC_GtFunc },
	{ (xmlChar *)"LT", &SC_LtFunc },
	{ (xmlChar *)"MINUS", &SC_MinusFunc },
	{ (xmlChar *)"NE", &SC_NeFunc },
	{ (xmlChar *)"PLUS", &SC_PlusFunc },
	{ (xmlChar *)"EndOfStatement", NULL },
};


static double SC_GetGreensLink(uint8 index);
static double SC_GetSnow(uint8 index);
static double SC_GetDemsta(uint8 index);
static double SC_GetAveflo(uint8 index);
static double SC_GetCutmax(uint8 index);
static double SC_GetWaitim(uint8 index);
static double SC_GetStamin(uint8 index);
static double SC_GetStamax(uint8 index);
static double SC_GetStensaStage(uint8 index);
static double SC_GetLiensaLink(uint8 index);
static double SC_GetRedcinLane(uint8 index);
static double SC_GetRedcxLane(uint8 index);
static double SC_GetLaensaLane(uint8 index);
static double SC_GetRegtotLane(uint8 index);
static double SC_GetInxtraLane(uint8 index);
static double SC_GetLaosatLane(uint8 index);
static double SC_GetLadetfLane(uint8 index);
static double SC_GetOsaticLane(uint8 index);
static double SC_GetSusdetDets(uint8 index);
static double SC_GetEsnext(uint8 index);
static double SC_GetPsnext(uint8 index);
static double SC_GetEstdemStage(uint8 index);
static double SC_GetPstdemStage(uint8 index);
static double SC_GetLambdaStage(uint8 index);
static double SC_GetSmflowLane(uint8 index);
static double SC_GetLindemLink(uint8 index);

static MVStatus SC_CheckNoIndex(Parameter * parm);
static MVStatus SC_CheckDetNum(Parameter * parm);
static MVStatus SC_CheckLaneIndex(Parameter * parm);
static MVStatus SC_CheckLinkIndex(Parameter * parm);
static MVStatus SC_CheckStageIndex(Parameter * parm);

static const MovaOperator movacalls[] =
{
	{ (xmlChar *)"GREENS_LINK",			&SC_GetGreensLink,	&SC_CheckLinkIndex },
	{ (xmlChar *)"SNOW",				&SC_GetSnow,		&SC_CheckNoIndex },
	{ (xmlChar *)"DEMSTA",				&SC_GetDemsta,		&SC_CheckNoIndex },
	{ (xmlChar *)"AVEFLO_7_48_LANE",	&SC_GetAveflo,		&SC_CheckLaneIndex },
	{ (xmlChar *)"CUTMAX",				&SC_GetCutmax,		&SC_CheckNoIndex },
	{ (xmlChar *)"WAITIM",				&SC_GetWaitim,		&SC_CheckNoIndex },
	{ (xmlChar *)"STAMIN",				&SC_GetStamin,		&SC_CheckNoIndex },
	{ (xmlChar *)"STAMAX",				&SC_GetStamax,		&SC_CheckNoIndex },
	{ (xmlChar *)"STENSA_STAGE",		&SC_GetStensaStage,	&SC_CheckStageIndex },
	{ (xmlChar *)"LIENSA_LINK",			&SC_GetLiensaLink,	&SC_CheckLinkIndex },
	{ (xmlChar *)"REDCIN_LANE",			&SC_GetRedcinLane,	&SC_CheckLaneIndex },
	{ (xmlChar *)"REDCX_LANE",			&SC_GetRedcxLane,	&SC_CheckLaneIndex },
	{ (xmlChar *)"LAENSA_LANE",			&SC_GetLaensaLane,	&SC_CheckLaneIndex },
	{ (xmlChar *)"REGTOT_LANE",			&SC_GetRegtotLane,	&SC_CheckLaneIndex },
	{ (xmlChar *)"INXTRA_LANE",			&SC_GetInxtraLane,	&SC_CheckLaneIndex },
	{ (xmlChar *)"LAOSAT_LANE",			&SC_GetLaosatLane,	&SC_CheckLaneIndex },
	{ (xmlChar *)"LADETF_LANE",			&SC_GetLadetfLane,	&SC_CheckLaneIndex },
	{ (xmlChar *)"OSATIC_LANE",			&SC_GetOsaticLane,	&SC_CheckLaneIndex },
	{ (xmlChar *)"SUSDET_DETS",			&SC_GetSusdetDets,	&SC_CheckDetNum },
	{ (xmlChar *)"ESNEXT",				&SC_GetEsnext,		&SC_CheckNoIndex },
	{ (xmlChar *)"PSNEXT",				&SC_GetPsnext,		&SC_CheckNoIndex },
	{ (xmlChar *)"ESTDEM_STAGE",		&SC_GetEstdemStage,	&SC_CheckStageIndex },
	{ (xmlChar *)"PSTDEM_STAGE",		&SC_GetPstdemStage,	&SC_CheckStageIndex },
	{ (xmlChar *)"LAMBDA_STAGE",		&SC_GetLambdaStage,	&SC_CheckStageIndex },
	{ (xmlChar *)"SMFLOW_LANE",			&SC_GetSmflowLane,	&SC_CheckLaneIndex },
	{ (xmlChar *)"LINDEM_LINK",			&SC_GetLindemLink,	&SC_CheckLinkIndex },
};


/* Generic functions to get/check value stored in Parameter */
static MVStatus SC_CheckMovaVariable(Parameter * parm);
static MVStatus SC_CheckNumeric(Parameter * parm);
static MVStatus SC_CheckUserVariable(int dpnumb, Parameter * parm, xmlChar * word);
static void SC_CopyNarray(Parameter * parm, uint8 nvals, double * vals);
static void SC_GetDetector(Parameter parm, ParameterValue * pv);
static void SC_GetMovaVariable(Parameter parm, ParameterValue * pv);
static void SC_GetNumeric(Parameter parm, ParameterValue * pv);
static void SC_GetUserLogic(Parameter parm, ParameterValue * pv);
static void SC_GetUserNumb(Parameter parm, ParameterValue * pv);

/* Generic functions to put value stored in Result */
static void SC_PutDetector(Result * res, MVBool val);
static void SC_PutOutput(Result * res, MVBool val);
static void SC_PutUserLogic(Result * res, MVBool val);

static void SC_ProcessParameters(Rule * rule, ParameterValue * pv);
static void SC_ProcessCallCancel(Rule * rule, MVBool val);
static void SC_ProcessDelay(Rule * rule);
static void SC_ProcessDelayDuration(Rule * rule);
static void SC_ProcessPulse(Rule * rule);

#define SC_SAFE_STRNCPY(ptr, word) \
	Safe_Strncpy(ptr, word, strlen(word) + 1); \


/*** Start of debug code for timers ***************************/
/* Toggle DEBUG_SC_TIMERS to enable/disable timer debug code*/
#define DEBUG_SC_TIMERS 0

#if DEBUG_SC_TIMERS && defined(_DEBUG) && defined(WIN32)
/* Toggle here to enable/disable individual timer debug code */
// #define SC_DebugCCTimerState(func, name, state, val) SC_DebugTimerState(func, name, state, val)
#define SC_DebugCCTimerState(func, name, state, val)

// #define SC_DebugDelayTimerState(func, name, state, val) SC_DebugTimerState(func, name, state, val)
#define SC_DebugDelayTimerState(func, name, state, val)

// #define SC_DebugPulseTimerState(func, name, state, val) SC_DebugTimerState(func, name, state, val)
#define SC_DebugPulseTimerState(func, name, state, val)

static void SC_TestTimers(int dpnumb);
static void SC_DebugTimerState(const char * func, const char * name, const char * state, uint16 val);
static void SC_DebugTimerInit(const char * name, double value);
#else
#define SC_DebugCCTimerState(func, name, state, val)
#define SC_DebugDelayTimerState(func, name, state, val)
#define SC_DebugPulseTimerState(func, name, state, val)
#define SC_TestTimers(dpnumb)
#define SC_DebugTimerState(func, name, state, val)
#define SC_DebugTimerInit(name, value)
#endif
/*** End of debug code for timers ***************************/


void SC_ResetTempData()
{
	memset(g_temp_rules, 0, sizeof(g_temp_rules));
}


MVStatus SC_LoadDatasetRules(MVInt8 dpnumb)
{
	MVStatus ret = MV_SUCCESS;

	TRACE_WRITE1_IF(LEVEL_INFO, g_MODULE_NAME,
		"SC_LoadDatasetRules loading data plan : %d", (dpnumb + 1));

	if (g_temp_rules[dpnumb].nrules > 0)
	{
		MVUInt i, j;

		memcpy(&(g_rules[dpnumb]), &(g_temp_rules[dpnumb]), sizeof(Rules));

		for (i = 0; i < g_rules[dpnumb].nrules && ret == MV_SUCCESS; i++)
		{
			Rule * rule = &(g_rules[dpnumb].rule[i]);

			for (j = 0; j < (rule->numb_param) && ret == MV_SUCCESS; j++)
			{
				Parameter * param = &(rule->param[j]);
				
				if (param->getfunc == SC_GetDetector)
				{
					ret = SC_CheckDetNum(param);
				}
				else if (param->mvnumb > 0 )
				{
					// check if Lane/Link/Stage Index is within range
					ret = movacalls[param->mvnumb-1].check(param);
				}
				else if (param->unumb > 0)
				{
					/* The memory copy above does not preserve pointer addresses to non */
					/* static memory so we must update user variable information here   */
					param->uptr = &(g_rules[dpnumb].rule[param->unumb-1].res.user_var);
				}
				else if (param->getfunc == SC_GetNumeric)
				{
					ret = SC_CheckNumeric(param);

				}
			}
		}
	}
	else
	{
		TRACE_WRITE0_IF(LEVEL_INFO, g_MODULE_NAME,
			"SC_LoadDatasetRules did not load data plan : no rules defined");
	}

	return ret;
}


static MVStatus SC_GetOperator(Parameter * parm, xmlChar * oper)
{
	int limit = sizeof(opcalls) / sizeof(ParameterOperator);
	int i;

	for (i = 0; i < limit; i++)
	{
		if (xmlStrEqual(oper, opcalls[i].op))
		{
			parm->opfunc = opcalls[i].func;
			return MV_SUCCESS;
		}
	}

	TRACE_WRITE1_IF(LEVEL_ERROR, g_MODULE_NAME,
		"Unknown operator in SC_GetOperator : %s", oper);

	return MV_FAILURE;
}


static void SC_CopyNarray(Parameter * parm, uint8 nvals, double * vals)
{
	int i;

	parm->nvals = nvals;

	for (i = 0; i < nvals; i++)
	{
		parm->value.narray[i] = vals[i];
	}
}


MVStatus SC_SetParameterData(int dpnumb, int nrule, int nparm, xmlChar * psource, uint8 nvals, double * vals, xmlChar * user, xmlChar * mvar, xmlChar * oper)
{
	MVStatus ret = MV_SUCCESS;

	if ((nrule < MAX_NUMB_RULES) && (nparm < MAX_NUMB_PARAM))
	{
		Parameter * parm = &(g_temp_rules[dpnumb].rule[nrule].param[nparm]);

		if (xmlStrEqual(psource, STR_PS_DET_CHAN))
		{
			SC_CopyNarray(parm, nvals, vals);
			parm->getfunc = &SC_GetDetector;
		}
		else if (xmlStrEqual(psource, STR_PS_MOVA_VAR))
		{
			char * word = (char *)mvar;

			SC_SAFE_STRNCPY(parm->mvar, word);
			SC_CopyNarray(parm, nvals, vals);
			parm->getfunc = &SC_GetMovaVariable;
			ret = SC_CheckMovaVariable(parm);
		}
		else if (xmlStrEqual(psource, STR_PS_USER_VAR))
		{
			ret = SC_CheckUserVariable(dpnumb, parm, (xmlChar *)user);
		}
		else if (xmlStrEqual(psource, STR_PS_NUM_VAL))
		{
			SC_CopyNarray(parm, nvals, vals);
			parm->getfunc = &SC_GetNumeric;
		}
		else
		{
			TRACE_WRITE1_IF(LEVEL_ERROR, g_MODULE_NAME,
				"Unknown ParameterSource in ParameterData : %s", psource);

			return MV_FAILURE;
		}

		if (ret == MV_SUCCESS)
		{
			ret = SC_GetOperator(parm, oper);
		}
	}

	return ret;
}


void SC_SetParameterNumber(int dpnumb, int nrule, int nparms)
{
	if ((nrule < MAX_NUMB_RULES) && (nparms < MAX_NUMB_PARAM))
	{
		g_temp_rules[dpnumb].rule[nrule].numb_param = nparms;
	}
	else
	{
		/* set to zero to invalidate all rules */
		g_temp_rules[dpnumb].nrules = 0;
		TRACE_WRITE4_IF(LEVEL_ERROR, g_MODULE_NAME,
			"Either rules(%d) > MAX_NUMB_RULES(%d) or parameters(%d) > MAX_NUMB_PARAM(%d)", 
			nrule, MAX_NUMB_RULES, nparms, MAX_NUMB_PARAM);
	}
}

void SC_IncRuleNumber(int dpnumb)
{
	if (g_temp_rules[dpnumb].nrules <= MAX_NUMB_RULES - 1)
	{
		g_temp_rules[dpnumb].nrules++;
	}
	else
	{
		/* set to zero to invalidate all rules */
		g_temp_rules[dpnumb].nrules = 0;
		TRACE_WRITE1_IF(LEVEL_ERROR, g_MODULE_NAME,
			"Trying to set number of rules greater than MAX_NUMB_RULES (%d)", MAX_NUMB_RULES);
	}
}


MVStatus SC_SetResultData(int dpnumb, int index, xmlChar * rsource, double chan, xmlChar * user, MVBool ulogic)
{
	if (index < MAX_NUMB_RULES)
	{
		Rule * rule = &(g_temp_rules[dpnumb].rule[index]);
		Result * result = &(rule->res);

		if (xmlStrEqual(rsource, STR_RS_DET_CHAN))
		{
			result->source = RS_DET_CHAN;
			result->user_var.value.numb = chan;
			result->putfunc = &SC_PutDetector;
		}
		else if (xmlStrEqual(rsource, STR_RS_OUT_CHAN))
		{
			result->source = RS_OUT_CHAN;
			result->putfunc = &SC_PutOutput;
			result->user_var.value.numb = chan;
			if ((chan < 1.0) || (chan > NumberofOutputChans))
			{
				TRACE_WRITE2_IF(LEVEL_ERROR, g_MODULE_NAME,
					"Output channel number (%.0f) greater than number of OutputChannels (%d)", 
					chan, NumberofOutputChans);

				return MV_FAILURE;
			}
		}
		else if (xmlStrEqual(rsource, STR_RS_USER_VAR))
		{
			SC_SAFE_STRNCPY(result->user_var.name, (char*)user);
			if (ulogic)
			{
				result->source = RS_USER_VAR_LOGIC;
				result->putfunc = &SC_PutUserLogic;
			}
			else
			{
				result->source = RS_USER_VAR_NUMB;
				result->putfunc = NULL;
			}
		}
		else
		{
			TRACE_WRITE1_IF(LEVEL_ERROR, g_MODULE_NAME,
				"Unknown ResultSource in ResultData : %s", rsource);

			return MV_FAILURE;
		}
		return MV_SUCCESS;
	}

	return MV_FAILURE;
}


// use this function to round up values and make them 10x bigger
static uint16 SC_Round(double val)
{
	return ((uint16)(val * 100))/10;
}

MVStatus SC_SetTimerData(int dpnumb, int index, double call, double cancel, double delay,
	double duration, double persistence, double pulseMark, double pulseSpace)
{
	Timers * pre;

	if ((index >= MAX_NUMB_RULES) || (call < 0) || (cancel < 0) ||
		(delay < 0) || (duration < 0) || (persistence < 0) ||
		(pulseMark < 0) || (pulseSpace < 0))
		return MV_FAILURE;

	pre = &(g_temp_rules[dpnumb].rule[index].preOutput);

	pre->call = SC_Round(call);
	pre->cancel = SC_Round(cancel);
	pre->delay = SC_Round(delay);
	pre->duration = SC_Round(duration);
	pre->persistence = SC_Round(persistence);
	pre->pulseMark = SC_Round(pulseMark);
	pre->pulseSpace = SC_Round(pulseSpace);

	// initialise starting time / state
	pre->prevstate = MV_FALSE;

	// initialise call / cancel states
	pre->cc_state = MV_FALSE;
	pre->cc_time = 0;
	pre->allow_delay = MV_FALSE;

	// initialise delay / duration / persistence states
	pre->delay_state = MV_FALSE;
	pre->delay_time = 0;
	pre->persist_time = 0;

	// initialise pulse mark / space states
	pre->pms_state = MV_FALSE;
	pre->pms_time = 0;
	pre->in_pulse_space = MV_FALSE;

	return MV_SUCCESS;
}


/* 
   NOTE - SC_ProcessAllRules must be fast, so need to
   completely execute in under 20ms.
*/
void SC_ProcessAllRules()
{
	MVInt dpnumb = DSH_get_active_data_plan_number() - 1;

#if defined(_DEBUG) && defined(WIN32)
	LARGE_INTEGER frequency, start, end;
	double interval;

	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&start);
#endif 

	/* only enter here if there are some rules to process */
	if (g_rules[dpnumb].nrules > 0)
	{
		ParameterValue output;
		Rule * rule;
		MVUInt i;

		/* copy detector inputs before rules change output values */
		GetDetectorsStatus(temp_detsin);

		/* loop through rules */
		for (i = 0; i < g_rules[dpnumb].nrules; i++)
		{
			rule = &(g_rules[dpnumb].rule[i]);

			/* process rule */
			SC_ProcessParameters(rule, &output);

			/* more likely to be logical than numeric so check for logic first */
			if (rule->res.source != RS_USER_VAR_NUMB)
			{
				/* logic values must pass through timer logic */
				SC_ProcessCallCancel(rule, output.logic);
			}
			else
			{
				rule->res.user_var.value.numb = output.numb;
			}
		}

		/* set output detector values resulting from rules */
		SetDetectorStatus(temp_detsin, mxdets);
	}

#if defined(_DEBUG) && defined(WIN32)
	QueryPerformanceCounter(&end);
	interval = ((double)(end.QuadPart - start.QuadPart) / frequency.QuadPart) * 1000.0;

	TRACE_WRITE1_IF(LEVEL_INFO, g_MODULE_NAME, 
		"SC_ProcessAllRules completed in %.4f ms\n", interval);
#endif 
}


static void SC_ProcessParameters(Rule * rule, ParameterValue * val)
{
	MVUInt limit = rule->numb_param - 1;
	Parameter p1 = rule->param[0];
	ParameterValue * pv1 = val;
	Parameter p2;
	ParameterValue pv2;
	MVUInt i;

	p1.getfunc(p1, pv1);

	/* loop through parameters */
	for (i = 1; i <= limit; i++)
	{
		p2 = rule->param[i];
		p2.getfunc(p2, &pv2);

		p1.opfunc(pv1, pv2);

		p1 = p2;
	}
}


/*
	This function processes the call cancel timer. The detector state must be
	ON for the call timer period to enter the call state. In the call ON
	state, the cancel timer is run and when it expires the call state is OFF.
*/
static void SC_ProcessCallCancel(Rule * rule, MVBool val)
{
	Timers * pre = &(rule->preOutput);
	pre->prevstate = val;

	// input is high and call time exists
	if ((pre->prevstate == MV_TRUE) && (pre->call > 0))
	{
		// not in call state so check call timer
		if (pre->cc_state == MV_FALSE)
		{
			if (pre->cc_time < pre->call)
			{
				// increment call timer
				pre->cc_time++;
			}
			else
			{
				// turn on call state
				pre->cc_state = MV_TRUE;
				pre->cc_time = 0;
				pre->allow_delay = MV_TRUE;
				SC_DebugCCTimerState("SC_ProcessCallCancel1", "Call", "ON", pre->cc_time);
			}
		}
		else
		{
			pre->cc_time = 0;
		}
	}
	// input is low and cancel timer exists
	else if ((pre->prevstate == MV_FALSE) && (pre->cancel > 0))
	{
		if (pre->cc_state == MV_TRUE)
		{
			if (pre->cc_time < pre->cancel)
			{
				// increment cancel timer
				pre->cc_time++;
			}
			else
			{
				// cancel state has just finished
				pre->cc_state = MV_FALSE;
				pre->cc_time = 0;
				pre->allow_delay = MV_FALSE;
				SC_DebugCCTimerState("SC_ProcessCallCancel2", "Cancel", "OFF", pre->cc_time);
			}
		}
		else
		{
			pre->cc_time = 0;
		}
	}
	else
	{
		pre->cc_time = 0;

		// only update on state change
		if (pre->cc_state != val)
		{
			pre->cc_state = val;
			pre->allow_delay = val;
			SC_DebugCCTimerState("SC_ProcessCallCancel3",
				((val) ? "Call" : "Cancel"),
				((val) ? "ON" : "OFF"),
				pre->cc_time);
		}
	}

	/* now check for delay timer */
	SC_ProcessDelay(rule);
}


/*
	This function processes the delay timer. The call state must be ON for
	the delay timer period to enter the delay state. In the delay ON state,
	the duration timer is run and when it expires the delay state is OFF. The
	delay state stays ON for the duration timer period but if the call state
	turns OFF then the delay state can only stay ON for the persistence timer
	period. The delay state is only entered once during a call ON period.
*/
static void SC_ProcessDelay(Rule * rule)
{
	Timers * pre = &(rule->preOutput);

	/* if no delay time then just mirror call/cancel state */
	if (pre->delay == 0)
	{
		// only check when delay starts
		if ((pre->cc_state == MV_TRUE) && (pre->delay_state == MV_FALSE))
		{
			pre->delay_time = 0;

			// during call/cancel state, only allow one delay change
			if (pre->allow_delay == MV_TRUE)
			{
				pre->delay_state = pre->cc_state;
				pre->allow_delay = MV_FALSE;
				SC_DebugDelayTimerState("SC_ProcessDelay1", "Delay", "ON", pre->delay_time);
			}
		}

		if (pre->delay_state == MV_TRUE)
		{
			SC_ProcessDelayDuration(rule);
		}
	}
	/* checking for delay period */
	else if ((pre->delay_state == MV_FALSE) && (pre->delay > 0) && (pre->allow_delay == MV_TRUE))
	{
		// inside call/cancel state so check for delay state
		if (pre->cc_state == MV_TRUE)
		{
			if (pre->delay_time < pre->delay)
			{
				// increment delay timer
				pre->delay_time++;
			}
			else
			{
				// turn on delay state
				pre->delay_state = MV_TRUE;
				pre->delay_time = 1;
				pre->persist_time = 0;
				pre->allow_delay = MV_FALSE;
				SC_DebugDelayTimerState("SC_ProcessDelay3", "Delay", "ON", pre->delay_time);
			}
		}
		else
		{
			pre->delay_time = 0;
			pre->persist_time = 0;
		}
	}
	/* during delay period */
	else if (pre->delay_state == MV_TRUE)
	{
		SC_ProcessDelayDuration(rule);
	}
	else if (pre->delay_time > 0)
	{
		// delay is off so come in here once to reset timer
		pre->delay_time = 0;
	}

	/* now check for pulse timer */
	SC_ProcessPulse(rule);
}


/*
 Delay state is on so check for change in call/cancel state.
 During this period also need to process duration and 
 persistence timer.
*/
static void SC_ProcessDelayDuration(Rule * rule)
{
	Timers * pre = &(rule->preOutput);

	// delay state is on so check for change in call/cancel state
	if (pre->cc_state == MV_TRUE)
	{
		// check if duration timer has expired
		if ((pre->delay_time < pre->duration) || (pre->duration == 0))
		{
			// increment delay timer but only if duration timer exists
			if (pre->duration > 0)
			{
				SC_DebugDelayTimerState("SC_ProcessDelay4a", "Dur", "---", pre->delay_time);
				pre->delay_time++;
			}
		}
		else
		{
			// turn off delay state
			pre->delay_state = MV_FALSE;
			pre->delay_time = 0;
			pre->persist_time = 0;
			pre->allow_delay = MV_FALSE;
			SC_DebugDelayTimerState("SC_ProcessDelay4", "Delay", "OFF", pre->delay_time);
		}
	}
	else
	{
		// inside delay state, check that persistence and duration timer has not expired
		if ((pre->persist_time < pre->persistence) && (pre->delay_time < pre->duration))
		{
			SC_DebugDelayTimerState("SC_ProcessDelay5a", "Dur", "---", pre->delay_time);
			SC_DebugDelayTimerState("SC_ProcessDelay5b", "Pers", "---", pre->persist_time);
			pre->delay_time++;
			pre->persist_time++;
		}
		else
		{
			// turn off delay state
			pre->delay_state = MV_FALSE;
			pre->delay_time = 0;
			pre->persist_time = 0;
			pre->allow_delay = MV_FALSE;
			SC_DebugDelayTimerState("SC_ProcessDelay5", "Delay", "OFF", pre->delay_time);
		}
	}
}


/*
	This function processes the pulse mark/space timer. The delay state must be
	ON for pulse mark/space state to change. The output is ON during the pulse 
	mark state for the pulse mark timer period. The output is OFF during the
	pulse space state for the pulse space timer period.
*/
static void SC_ProcessPulse(Rule * rule)
{
	Timers * pre = &(rule->preOutput);
	Result * res = &(rule->res);

	/* during mark/space period */
	if (pre->delay_state == MV_TRUE)
	{
		// check if inside pulse mark state
		if (pre->pms_state == MV_TRUE)
		{
			// check if pulse mark timer exists
			if (pre->pulseMark > 0)
			{
				// check if pulse mark timer has expired
				if (pre->pms_time < pre->pulseMark)
				{
					pre->pms_time++;
					(*res).putfunc(res, MV_TRUE);
				}
				else if (pre->pulseSpace > 0)
				{
					// enter pulse space only if pulse space timer exists
					pre->pms_state = MV_FALSE;
					pre->in_pulse_space = MV_TRUE;
					pre->pms_time = 1;
					SC_DebugPulseTimerState("SC_ProcessPulse", "PulseSpace1", "OFF", pre->pms_time);
					(*res).putfunc(res, MV_TRUE);
				}
			}
			else
			{
				// there is no pulse mark timer so signal is constantly on
				SC_DebugPulseTimerState("SC_ProcessPulse", "PulseMark1", "ON", pre->pms_time);
				(*res).putfunc(res, MV_TRUE);
			}
		}
		else
		{
			// check if in pulse mark or pulse space state
			if (pre->in_pulse_space == MV_FALSE)
			{
				// enter pulse mark state
				pre->pms_state = MV_TRUE;
				pre->pms_time = 1;
				SC_DebugPulseTimerState("SC_ProcessPulse", "PulseMark2", "ON", pre->pms_time);
				(*res).putfunc(res, MV_TRUE);
			}
			else
			{
				// check if pulse space timer has expired
				if (pre->pms_time < pre->pulseSpace)
				{
					pre->pms_time++;
				}
				else
				{
					// enter pulse mark state
					pre->pms_state = MV_TRUE;
					pre->in_pulse_space = MV_FALSE;
					pre->pms_time = 1;
					SC_DebugPulseTimerState("SC_ProcessPulse", "PulseMark3", "ON", pre->pms_time);
					(*res).putfunc(res, MV_TRUE);
				}
			}
		}
	}
	else
	{
		/* if delay state = False => pulse state = False */
		if (pre->pms_state != MV_FALSE)
		{
			pre->pms_state = MV_FALSE;
			pre->pms_time = 0;
			SC_DebugPulseTimerState("SC_ProcessPulse", "PulseSpace2", "OFF", pre->pms_time);
			(*res).putfunc(res, MV_FALSE);
		}
	}
}


static void SC_AndFunc(ParameterValue * pv1, ParameterValue pv2)
{
	pv1->logic = pv1->logic & pv2.logic;
}


static void SC_NandFunc(ParameterValue * pv1, ParameterValue pv2)
{
	pv1->logic = !(pv1->logic & pv2.logic);
}


static void SC_NorFunc(ParameterValue * pv1, ParameterValue pv2)
{
	pv1->logic = !(pv1->logic | pv2.logic);
}


static void SC_NxorFunc(ParameterValue * pv1, ParameterValue pv2)
{
	pv1->logic = !(pv1->logic ^ pv2.logic);
}


static void SC_OrFunc(ParameterValue * pv1, ParameterValue pv2)
{
	pv1->logic = pv1->logic | pv2.logic;
}


static void SC_XorFunc(ParameterValue * pv1, ParameterValue pv2)
{
	pv1->logic = pv1->logic ^ pv2.logic;
}


static void SC_EqFunc(ParameterValue * pv1, ParameterValue pv2)
{
	pv1->logic = (pv1->numb == pv2.numb);
}


static void SC_GtFunc(ParameterValue * pv1, ParameterValue pv2)
{
	pv1->logic = (pv1->numb > pv2.numb);
}


static void SC_LtFunc(ParameterValue * pv1, ParameterValue pv2)
{
	pv1->logic = (pv1->numb < pv2.numb);
}


static void SC_MinusFunc(ParameterValue * pv1, ParameterValue pv2)
{
	pv1->numb = (pv1->numb - pv2.numb);
}


static void SC_NeFunc(ParameterValue * pv1, ParameterValue pv2)
{
	pv1->logic = !(pv1->numb == pv2.numb);
}


static void SC_PlusFunc(ParameterValue * pv1, ParameterValue pv2)
{
	pv1->numb = (pv1->numb + pv2.numb);
}


static void SC_GetDetector(Parameter parm, ParameterValue * pv)
{
	pv->logic = GetSingleDetectorStatus((uint8)parm.value.numb);
}


static MVStatus SC_CheckMovaVariable(Parameter * parm)
{
	int limit = sizeof(movacalls) / sizeof(MovaOperator);
	xmlChar * mvar = (xmlChar *)(parm->mvar);
	int i;

	for (i = 0; i < limit; i++)
	{
		if (xmlStrEqual(mvar, movacalls[i].name))
		{
			parm->mvnumb = i + 1;
			return MV_SUCCESS;
		}
	}

	TRACE_WRITE1_IF(LEVEL_ERROR, g_MODULE_NAME,
		"Unknown Mova variable in SC_CheckMovaVariable : %s", mvar);

	return MV_FAILURE;
}


static void SC_GetMovaVariable(Parameter parm, ParameterValue * pv)
{
	uint8 i;

	pv->numb = 0.0;

	for (i = 0; i < parm.nvals; i++)
	{
		pv->numb += movacalls[parm.mvnumb - 1].func((uint8)parm.value.narray[i]);
	}
}


static MVStatus SC_CheckNumeric(Parameter * parm)
{
	// this function is not currently used but may be needed later
	UNUSED(parm);
	return MV_SUCCESS;
}


static void SC_GetNumeric(Parameter parm, ParameterValue * pv)
{
	pv->numb = parm.value.numb;
}


static MVStatus SC_CheckUserVariable(int dpnumb, Parameter * parm, xmlChar * word)
{
	MVUInt i;

	for (i = 0; i < g_temp_rules[dpnumb].nrules; i++)
	{
		Result * res = &(g_temp_rules[dpnumb].rule[i].res);

		if ((res->source == RS_USER_VAR_LOGIC) || (res->source == RS_USER_VAR_NUMB))
		{
			xmlChar * uname = (xmlChar *)(res->user_var.name);

			if (xmlStrEqual(word, uname))
			{
				parm->unumb = i + 1;
				parm->getfunc = (res->source == RS_USER_VAR_LOGIC) ? &SC_GetUserLogic : &SC_GetUserNumb;
				return MV_SUCCESS;
			}
		}
	}
	
	TRACE_WRITE1_IF(LEVEL_ERROR, g_MODULE_NAME,
		"Unknown user variable in SC_CheckUserVariable : %s", word);
	
	return MV_FAILURE;
}


static void SC_GetUserLogic(Parameter parm, ParameterValue * pv)
{
	pv->logic = parm.uptr->value.logic;
}


static void SC_GetUserNumb(Parameter parm, ParameterValue * pv)
{
	pv->numb = parm.uptr->value.numb;
}


static void SC_PutDetector(Result * res, MVBool val)
{
	uint8 DetIdx = (uint8)((*res).user_var.value.numb);

	if ((DetIdx > 0) && (DetIdx <= mxdets))
	{
		temp_detsin[DetIdx - 1] = val;
	}
}


static void SC_PutOutput(Result * res, MVBool val)
{
	int index = StartofOutputChans + (int)((*res).user_var.value.numb) - 1;
	SetForceBits(index, (char)val);
}


static void SC_PutUserLogic(Result * res, MVBool val)
{
	(*res).user_var.value.logic = val;
}


static double SC_GetGreensLink(uint8 index)
{
	return (double)(input_data.links_green_flag[index-1]);
}


static double SC_GetSnow(uint8 index)
{
	UNUSED(index);

	return (double)(input_data.current_stage);
}


static double SC_GetDemsta(uint8 index)
{
	UNUSED(index);

	return (double)CI_get_demanded_stage();
}


static double SC_GetAveflo(uint8 index)
{
	return (double)get_lane_current_average_flow(index-1);
}


static double SC_GetCutmax(uint8 index)
{
	UNUSED(index);

	return (double)get_junction_cut_max_times_marker();
}


static double SC_GetWaitim(uint8 index)
{
	UNUSED(index);

	return (double)(timers.wait_stage_change_timer);
}


static double SC_GetStamin(uint8 index)
{
	UNUSED(index);

	return (double)get_var_min_green_time();
}


static double SC_GetStamax(uint8 index)
{
	UNUSED(index);

	return (double)(timers.wait_stage_change_timer);
}


static double SC_GetStensaStage(uint8 index)
{
	return (double)get_stage_endsat_marker(index-1);
}


static double SC_GetLiensaLink(uint8 index)
{
	return (double)get_link_endsat_marker(index-1);
}


static double SC_GetRedcinLane(uint8 index)
{
	return (double)get_lane_red_count_in_det(index-1);
}


static double SC_GetRedcxLane(uint8 index)
{
	return (double)get_lane_red_count_x_det(index-1);
}


static double SC_GetLaensaLane(uint8 index)
{
	return (double)get_lane_endsat_marker(index-1);
}


static double SC_GetRegtotLane(uint8 index)
{
	return (double)get_lane_register_total(index-1);
}


static double SC_GetInxtraLane(uint8 index)
{
	return (double)get_lane_extra_veh_beyond_in_det(index-1);
}


static double SC_GetLaosatLane(uint8 index)
{
	return (double)get_lane_oversat_counter(index-1);
}


static double SC_GetLadetfLane(uint8 index)
{
	return (double)get_lane_det_fault_marker(index-1);
}


static double SC_GetOsaticLane(uint8 index)
{
	return (double)get_lane_oversat_init_count(index-1);
}


static double SC_GetSusdetDets(uint8 index)
{
	return (double)get_det_suspicion_status(index);
}


static double SC_GetEsnext(uint8 index)
{
	UNUSED(index);

	return (double)get_next_emeregency_stage_to_demand();
}


static double SC_GetPsnext(uint8 index)
{
	UNUSED(index);

	return (double)get_next_priority_stage_to_demand();
}


static double SC_GetEstdemStage(uint8 index)
{
	return (double)get_stage_emergency_demand_marker(index-1);
}


static double SC_GetPstdemStage(uint8 index)
{
	return (double)get_stage_priority_demand_marker(index-1);
}


static double SC_GetLambdaStage(uint8 index)
{
	return (double)get_stage_lambda(index-1);
}


static double SC_GetSmflowLane(uint8 index)
{
	return (double)get_lane_smoothed_flow(index-1);
}


static double SC_GetLindemLink(uint8 index)
{
	return (double)get_link_demand_marker(index-1);
}


static MVStatus SC_CheckNoIndex(Parameter * parm)
{
	UNUSED(parm);

	return MV_SUCCESS;
}


static MVStatus SC_CheckDetNum(Parameter * parm)
{
	MVStatus retval = MV_SUCCESS;
	uint8 ndets = DI_get_dets_count();
	uint8 i;

	for (i = 0; i < parm->nvals; i++)
	{
		if (parm->value.narray[i] > ndets)
		{
			TRACE_WRITE2_IF(LEVEL_ERROR, g_MODULE_NAME,
				"SC_CheckDetNum : requested detector (%.0f) out of (%i)", parm->value.narray[i], ndets);

			retval = MV_FAILURE;
		}
	}

	return retval;
}


static MVStatus SC_CheckLinkIndex(Parameter * parm)
{
	MVStatus retval = MV_SUCCESS;
	uint8 nlinks = DI_get_links_count();
	uint8 i;

	for (i = 0; i < parm->nvals; i++)
	{
		if (parm->value.narray[i] > nlinks)
		{
			TRACE_WRITE2_IF(LEVEL_ERROR, g_MODULE_NAME,
				"SC_CheckLinkIndex : requested link (%.0f) out of (%i)", parm->value.narray[i], nlinks);

			retval = MV_FAILURE;
		}
	}

	return retval;
}


static MVStatus SC_CheckLaneIndex(Parameter * parm)
{
	MVStatus retval = MV_SUCCESS;
	uint8 nlanes = DI_get_lanes_count();
	uint8 i;

	for (i = 0; i < parm->nvals; i++)
	{
		if (parm->value.narray[i] > nlanes)
		{
			TRACE_WRITE2_IF(LEVEL_ERROR, g_MODULE_NAME,
				"SC_CheckLaneIndex : requested lane (%.0f) out of (%i)", parm->value.narray[i], nlanes);

			retval = MV_FAILURE;
		}
	}

	return retval;
}


static MVStatus SC_CheckStageIndex(Parameter * parm)
{
	MVStatus retval = MV_SUCCESS;
	uint8 nstages = DI_get_stages_count();
	uint8 i;

	for (i = 0; i < parm->nvals; i++)
	{
		if (parm->value.narray[i] > nstages)
		{
			TRACE_WRITE2_IF(LEVEL_ERROR, g_MODULE_NAME,
				"SC_CheckStageIndex : requested stage (%.0f) out of (%i)", parm->value.narray[i], nstages);

			retval = MV_FAILURE;
		}
	}

	return retval;
}


/* leave timer debug code off for now */
#if DEBUG_SC_TIMERS && defined(_DEBUG) && defined(WIN32)

// global timer count
static uint16 tmrcnt = 0;

/* 
 * Debug function to test timer functionality.
 * Define a rule with one input detector linked to one output detector
 */
void SC_TestTimers(int dpnumb)
{
	MVStatus ret = MV_SUCCESS;
	MVBool state = MV_FALSE;
	Rule * rule = &(g_rules[dpnumb].rule[0]);
	double vals[] = { 1.0, 3.0 };

	/***** Only change the values here to simulate different scenarios *****/
	double call = 1.0;
	double cancel = 2.0;
	double delay = 3.0;
	double duration = 4.0;
	double persistence = 5.0;
	double pulseMark = 0.4;
	double pulseSpace = 0.6;

	// input data as increment and state change
	uint32 data[] = { 3, 5, 6, 8, 1, 7, 5, 2};
	/***********************************************************************/

	/* check input data is valid, then convert to 10 ticks per second */
	uint32 limit = sizeof(data) / sizeof(uint32);
	uint32 endtime = 0;
	uint32 dataIdx = 0;
	uint32 i;

	for (i = 0; i < limit; i++)
	{
		if (data[i] <= 0)
		{
			TRACE_WRITE2_IF(LEVEL_ERROR, g_MODULE_NAME,
				"SC_TestTimers : invalid data value (%i) <= 0 at index %i", data[i], i);
			return;
		}

		endtime += (data[i] * 10);
		data[i] = endtime;
	}

	/* set final time 1sec after last input data */
	endtime += 10;

	/* only having one rule so set value now */
	g_rules[dpnumb].nrules = 1;

	ret = SC_SetParameterData(0, 0, 0, STR_PS_DET_CHAN, 2, vals, (xmlChar *)"", NULL, (xmlChar *)"EndOfStatement");
	if (ret == MV_FAILURE)
	{
		TRACE_WRITE0_IF(LEVEL_ERROR, g_MODULE_NAME,
			"SC_TestTimers : failed calling SC_SetParameterData, check the values used.");
		return;
	}

	ret = SC_SetTimerData(0, 0, call, cancel, delay, duration, persistence, pulseMark, pulseSpace);
	if (ret == MV_FAILURE)
	{
		TRACE_WRITE0_IF(LEVEL_ERROR, g_MODULE_NAME,
			"SC_TestTimers : failed calling SC_SetTimerData, check the values used.");
		return;
	}

	ret = SC_SetResultData(0, 0, STR_RS_DET_CHAN, 2.0, (xmlChar *)"", MV_FALSE);
	if (ret == MV_FAILURE)
	{
		TRACE_WRITE0_IF(LEVEL_ERROR, g_MODULE_NAME,
			"SC_TestTimers : failed calling SC_SetResultData, check the values used.");
		return;
	}

	/* output initial values first */
	SC_DebugTimerInit("Call", call);
	SC_DebugTimerInit("Cancel", cancel);
	SC_DebugTimerInit("Delay", delay);
	SC_DebugTimerInit("Duration", duration);
	SC_DebugTimerInit("Persistence", persistence);
	SC_DebugTimerInit("PulseMark", pulseMark);
	SC_DebugTimerInit("PulseSpace", pulseSpace);
	SC_DebugTimerState("SC_TestTimers", "Input", ((state) ? "ON" : "OFF"), 0);

	for (tmrcnt = 0; tmrcnt <= endtime; tmrcnt++)
	{
		/* check for state change */
		if ((dataIdx < limit) && (data[dataIdx] == tmrcnt))
		{
			state = (state) ? MV_FALSE : MV_TRUE;
			dataIdx++;
			SC_DebugTimerState("SC_TestTimers", "Input", ((state) ? "ON" : "OFF"), tmrcnt);
		}

		SC_ProcessCallCancel(rule, state);
	}

	/* only here to use as debug break point */
	endtime = endtime;
}

static void SC_DebugTimerState(const char * func, const char * name, const char * state, uint16 val)
{
	TRACE_WRITE4_IF(LEVEL_ERROR, g_MODULE_NAME, "%-22s : %5.1f %-11s -> %s",
		func, (((float)val)/10), name, state);
}

void SC_DebugTimerInit(const char * name, double value)
{
	char buf[10];

	sprintf(buf, "%.1f", value);
	SC_DebugTimerState("Initial", name, buf, 0);
}

#endif
