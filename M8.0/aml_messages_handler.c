/*******************************************************************************
*
*                      MOVA KERNEL
*
*  FILE:         aml_messages_handler.c
*
*  TITLE:        Mova Kernel: AML
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
#include <stdio.h>
#include <time.h>
#include <string.h>
#include "jsmn-master/jsmn.h"

#if PCMOVA
#include <direct.h>
#endif

#include "mova_constants.h"

#include "aml_messages_handler.h"
#include "aml_messages_decoder_encoder.h"
#include "aml_handler.h"
#include "aml_dataset_file_handler.h"
#include "aml_generic_files_handler.h"
#include "dynamic_data.h"
#include "ui_dataaccess.h"

#include "generalfunc.h" /*Safe_Strncpyr*/

#include "tma_preprint.h" /*The files now is considered as a message/file formatter, so accessing it from the AML module is valid.*/


/*Constants*/

/*Failure reasons*/
#define FR_NONE					  (0)
#define FR_UNABLE_TO_CREATE_FILE  (1)
#define FR_MOVA_IS_ON_CONTROL	  (2)

#define FULL_SITE_BUFFER_SIZE	  (106200)
#define DATE_TIME_SIZE			  (21)
#define BIT_SIZE				  (2)
#define INT_STR_SIZE			  (12)
#define LIMITED_INT_SIZE		  (5) /*Used with Markers and the values that we know its limit (e.g., stages count, ...)*/
#define LONG_STR_SIZE			  (17)
#define FILE_SIZE_STR			  (20)	
#define TMA_FILE_LINK_SIZE		  (2000)
#define TIME_STR_SIZE			  (12)




static 	char full_site_msg_buffer[FULL_SITE_BUFFER_SIZE];

static MVStatus process_message_1_1(MVInt * msg_details);
static MVStatus process_message_1_2(MVInt * msg_details);

static MVStatus process_message_2(MVInt * msg_details);

static MVStatus process_message_3_1(MVInt * msg_details);
static MVStatus process_message_3_2(MVInt * msg_details);

static MVStatus process_message_4(MVInt * msg_details);

static void add_message_5_common_data(MVInt * msg_details, MVInt idx, char * time_in_green_str, char * next_stage_num_str, char * demand_time_str, char * links_demand_marker_str, char * lanes_red_count_str);
static MVStatus process_message_5_1(MVInt * msg_details);
static MVStatus process_message_5_2(MVInt * msg_details);
static MVStatus process_message_5_3(MVInt * msg_details);
static MVStatus process_message_5_4(MVInt * msg_details);

static MVStatus process_message_6_1(MVInt * msg_details);
static MVStatus process_message_6_2(MVInt * msg_details);

static MVStatus process_message_7_1(MVInt * msg_details);

static MVInt aml_prepare_tma_log_file(MVBool is_prep_to_web_downloading);

#define NULL_TERM(str) \
	if(strlen(str) > 0) \
		str[strlen(str) - 1] = 0x00;


#define CHECK_RANGE(value, min, max) \
	if(value < min || value > max) return MV_FAILURE;
	

MVStatus aml_req_mova_time_handler()
{	

	time_t		mova_time;
	struct tm * ts;
	char		date_time_str[DATE_TIME_SIZE];

	mova_time = UI_GetMovaTime();

	ts = localtime(&mova_time);
	
	strftime(date_time_str, sizeof(date_time_str), "%Y-%m-%dT%H:%M:%SZ", ts);

	aml_encode_message_json("RspMOVATime", 1,
							"DateTime", date_time_str, JSMN_STRING);

	return MV_SUCCESS;
}



MVStatus aml_req_force_bits_handler()
{
	MVBool force_bits[NumberOfForce];
	char   force_bits_str[(BIT_SIZE * NumberOfForce) + 1] = { 0 };
	char   take_over_bit_str[BIT_SIZE];
	char   hurry_inhibit_str[BIT_SIZE];

	int i;

	/*1. Get the Force Bits. */
	UI_GetForceBits(force_bits);

	for(i=0; i<NumberOfForce; i++)
	{
		if(force_bits[i] == MV_TRUE)  
			strcat(force_bits_str, "T");
		else
			strcat(force_bits_str, "F");

		if (i + 1 < NumberOfForce)
			strcat(force_bits_str, ",");
	}


	/*2. Get the Take Over Bit. */
	if(UI_GetTakeOverStatus() == MV_TRUE) 
		sprintf(take_over_bit_str, "T");
	else
		sprintf(take_over_bit_str, "F");

	/*3. Get the HI bit. */
	if (UI_GetHurryInhibitStatus() == MV_TRUE)
		sprintf(hurry_inhibit_str, "T");
	else
		sprintf(hurry_inhibit_str, "F");
	
	/*Compose the message */
	return aml_encode_message_json("RspForceBits", 3, 
								"ForceBits", force_bits_str, JSMN_ARRAY + JSMN_PRIMITIVE, 
								"TakeOverBit", take_over_bit_str, JSMN_PRIMITIVE,
								"HurryInhibit", hurry_inhibit_str, JSMN_PRIMITIVE);

}


MVStatus aml_req_output_channel_status_handler()
{
	MVBool output_chans[NumberofOutputChans];
	char   output_chans_str[(BIT_SIZE * NumberofOutputChans) + 1] = { 0 };
	int i;

	UI_GetOutputChannels(output_chans);

	for (i = 0; i < NumberofOutputChans; i++)
	{
		if (output_chans[i] == MV_TRUE)
			strcat(output_chans_str, "T");
		else
			strcat(output_chans_str, "F");

		if (i + 1 < NumberofOutputChans)
			strcat(output_chans_str, ",");
	}

	/*Compose the message */
	return aml_encode_message_json("RspOutputChans", 1,
		"OutputChans", output_chans_str, JSMN_ARRAY + JSMN_PRIMITIVE);
}


MVStatus aml_req_confirm_bits_handler()
{
	MVBool confirm_bits[NumberOfConfirms];
	char   confirm_bits_str[(NumberOfConfirms * BIT_SIZE) + 1] = { 0 };
	MVInt i;

	UI_GetConfirmationBits(confirm_bits);
	
	/*ignore the last bit which holds the CRB*/
	for(i=0; i<NumberOfConfirms-1; i++)
	{
		if(confirm_bits[i] == MV_TRUE)  
			strcat(confirm_bits_str, "T");
		else
			strcat(confirm_bits_str, "F");

		if (i + 1 < NumberOfConfirms-1)
			strcat(confirm_bits_str, ",");
	}

	return aml_encode_message_json("RspConfirmBits", 1, 
									"ConfirmBits", confirm_bits_str, JSMN_ARRAY + JSMN_PRIMITIVE);
}

MVStatus aml_req_operation_flags_handler()
{

	MVByte error_count, phone_home, stage_demanded;
	MVBool on_control, controller_ready, va_mova, phone_connected, multi_stage_confirm;
	MVInt phone_count, warmup_count;

	char  crb[BIT_SIZE], is_mova_enabled[BIT_SIZE], is_on_control[BIT_SIZE], is_multi_stage[BIT_SIZE];
	char  error_count_str[INT_STR_SIZE], warmup_count_str[INT_STR_SIZE], demanded_stage_num_str[LIMITED_INT_SIZE];

	UI_GetFlagDetails(&error_count, &phone_home, &on_control, 
				&controller_ready, &stage_demanded, &va_mova, &phone_connected,
				&multi_stage_confirm, &phone_count, &warmup_count);

	/*Prepare data for sending */
	if (controller_ready == MV_TRUE) Safe_Strncpy(crb,"T", BIT_SIZE); else Safe_Strncpy(crb, "F", BIT_SIZE);
	if (va_mova == MV_TRUE) Safe_Strncpy(is_mova_enabled, "T", BIT_SIZE); else Safe_Strncpy(is_mova_enabled, "F", BIT_SIZE);
	if (on_control == MV_TRUE) Safe_Strncpy(is_on_control, "T", BIT_SIZE); else Safe_Strncpy(is_on_control, "F", BIT_SIZE);
	if (multi_stage_confirm == MV_TRUE) Safe_Strncpy(is_multi_stage, "T", BIT_SIZE); else Safe_Strncpy(is_multi_stage, "F", BIT_SIZE);
	
	sprintf(error_count_str, "%d", error_count);
	sprintf(warmup_count_str, "%d", warmup_count);
	sprintf(demanded_stage_num_str, "%d", stage_demanded);
	
	return aml_encode_message_json("RspOperationFlags", 7,
									"CRB", crb , JSMN_PRIMITIVE,
									"IsMOVAEnabled", is_mova_enabled, JSMN_PRIMITIVE,
									"IsOnControl", is_on_control, JSMN_PRIMITIVE,
									"IsMultiStage", is_multi_stage, JSMN_PRIMITIVE,
									"ErrorCount", error_count_str, JSMN_PRIMITIVE,
									"Warmup", warmup_count_str, JSMN_PRIMITIVE,
									"DemandedStageNum", demanded_stage_num_str, JSMN_PRIMITIVE);
}


MVStatus aml_req_kernel_version_handler()
{
	char*  mova_version;

	mova_version = UI_GetMovaVersion();

	return aml_encode_message_json("RspKernelVersion", 1,
									"Version", mova_version, JSMN_STRING);
}

MVStatus aml_req_raw_dets_status_handler()
{
	MVBool raw_dets_status[MAX_DETS];
	MVInt  i;
	char   raw_dets_status_str[(BIT_SIZE * MAX_DETS) + 1] = { 0 };

	UI_GetRawDetectorsStatus(raw_dets_status);

	for (i = 0; i < MAX_DETS; i++)
	{
		if (raw_dets_status[i] == MV_TRUE)
			strcat(raw_dets_status_str, "T");
		else
			strcat(raw_dets_status_str, "F");

		if (i + 1 < MAX_DETS)
			strcat(raw_dets_status_str, ",");
	}

	return aml_encode_message_json("RspRawDetectorsStatus", 1,
		"RawStatus", raw_dets_status_str, JSMN_ARRAY + JSMN_PRIMITIVE);
}

MVStatus aml_req_dets_status_handler()
{
	MVBool dets_status[MAX_DETS];
	MVInt  i;
	char   dets_status_str[(BIT_SIZE * MAX_DETS) + 1] = { 0 };

	UI_GetDetectorsStatus(dets_status);

	for (i = 0; i < MAX_DETS; i++)
	{
		if (dets_status[i] == MV_TRUE)
			strcat(dets_status_str, "T");
		else
			strcat(dets_status_str, "F");

		if (i + 1 < MAX_DETS)
			strcat(dets_status_str, ",");
	}

	return aml_encode_message_json("RspDetectorsStatus", 1,
									"Status", dets_status_str, JSMN_ARRAY + JSMN_PRIMITIVE);
}

MVStatus aml_req_dets_sus_status_handler()
{
	MVInt dets_sus_status[MAX_DETS];
	MVInt  i;
	char   dets_sus_status_str[(BIT_SIZE * MAX_DETS) + 2] = { 0 };
	char	tmp[LIMITED_INT_SIZE];

	UI_GetDetectorsSusStatus(dets_sus_status);

	for (i = 0; i < MAX_DETS; i++)
	{
		sprintf(tmp, "%d,", dets_sus_status[i]);
		strcat(dets_sus_status_str, tmp);
	}
	NULL_TERM(dets_sus_status_str);

	return aml_encode_message_json("RspDetectorsSusStatus", 1,
									"SusStatus", dets_sus_status_str, JSMN_ARRAY + JSMN_PRIMITIVE);
}

MVStatus aml_req_lane_data_handler(MVInt8 lane_id)
{
	MVInt8 i;

	MVInt32		red_count_in;
	MVInt32		red_count_x;
	MVInt32		sf_smoothed;
	float		sf_last_cycle;
	MVInt32		shift_register[REG_SIZE];
	MVInt32		oversat_counter;
	MVInt8		endsat;
	MVInt32		extra_veh_beyond_in_det;
	MVInt32		oversat_init_count;


	char red_count_in_str[INT_STR_SIZE], red_count_x_str[INT_STR_SIZE], sf_smoothed_str[INT_STR_SIZE], sf_last_cycle_str[LONG_STR_SIZE];
	char shift_register_str[(INT_STR_SIZE * REG_SIZE) + 1] = { 0 };
	char oversat_counter_str[INT_STR_SIZE], endsat_str[INT_STR_SIZE], extra_veh_beyond_in_det_str[INT_STR_SIZE], oversat_init_count_str[INT_STR_SIZE];
	

	CHECK_RANGE(lane_id, 1, MAX_LANES);

	UI_GetLaneCommData(lane_id, &red_count_in, &red_count_x, &sf_smoothed, &sf_last_cycle, shift_register, &oversat_counter,
						&endsat, &extra_veh_beyond_in_det, &oversat_init_count);

	sprintf(red_count_in_str, "%ld", red_count_in);
	sprintf(red_count_x_str, "%ld", red_count_x);
	sprintf(sf_smoothed_str, "%ld", sf_smoothed);
	sprintf(sf_last_cycle_str, "%.2f", sf_last_cycle);

	for (i = 0; i < REG_SIZE; i++)
	{
		if (shift_register[i] == 0)
			strcat(shift_register_str, "F");
		else
			strcat(shift_register_str, "T");

		if (i + 1 < REG_SIZE)
			strcat(shift_register_str, ",");
	}


	sprintf(oversat_counter_str, "%ld", oversat_counter);
	sprintf(endsat_str, "%d", endsat);

	sprintf(extra_veh_beyond_in_det_str, "%ld", extra_veh_beyond_in_det);
	sprintf(oversat_init_count_str, "%ld", oversat_init_count);

	return aml_encode_message_json("RspLaneData", 9,
		"RedCountIN", red_count_in_str, JSMN_PRIMITIVE,
		"RedCountX", red_count_x_str, JSMN_PRIMITIVE,
		"SFSmoothed", sf_smoothed_str, JSMN_PRIMITIVE,
		"SFLastCycle", sf_last_cycle_str, JSMN_PRIMITIVE,
		"ShiftRegister", shift_register_str, JSMN_ARRAY + JSMN_PRIMITIVE,
		"OversatCounter", oversat_counter_str, JSMN_PRIMITIVE,
		"Endsat", endsat_str, JSMN_PRIMITIVE,
		"QBeyondINDET", extra_veh_beyond_in_det_str, JSMN_PRIMITIVE,
		"LeftOverVehs", oversat_init_count_str, JSMN_PRIMITIVE);
}

MVStatus aml_req_link_data_handler(MVInt8 link_id)
{

	MVInt32 bonus_green_time;
	MVInt32 smoothed_flow;
	MVInt8  endsat;
	MVInt8  demand_type;
	MVInt32 net_ben_flow;
	MVInt32 actual_flow;
	MVInt32 future_red;
	MVInt32 extra_green_time;
	MVInt8  ep_hold_marker;
	MVInt8  ep_ext_marker;
	MVInt32 ep_ext_timer;

	char	bonus_green_time_str[INT_STR_SIZE];
	char	smoothed_flow_str[INT_STR_SIZE];
	char	endsat_str[LIMITED_INT_SIZE];
	char	demand_type_str[LIMITED_INT_SIZE];
	char	net_ben_flow_str[INT_STR_SIZE];
	char	actual_flow_str[INT_STR_SIZE];
	char	future_red_str[INT_STR_SIZE];
	char	extra_green_time_str[INT_STR_SIZE];
	char	ep_hold_marker_str[LIMITED_INT_SIZE];
	char	ep_ext_marker_str[LIMITED_INT_SIZE];
	char	ep_ext_timer_str[INT_STR_SIZE];
	
	CHECK_RANGE(link_id, 1, MAX_LINKS);
	
	UI_GetLinkCommData(link_id, &bonus_green_time, &smoothed_flow, &endsat, &demand_type,
		&net_ben_flow, &actual_flow, &future_red, &extra_green_time,
		&ep_hold_marker, &ep_ext_marker, &ep_ext_timer);

	sprintf(bonus_green_time_str, "%ld", bonus_green_time);
	sprintf(smoothed_flow_str , "%ld", smoothed_flow);
	sprintf(endsat_str, "%d", endsat);
	sprintf(demand_type_str, "%d",demand_type);
	sprintf(net_ben_flow_str, "%ld", net_ben_flow);
	sprintf(actual_flow_str, "%ld", actual_flow);
	sprintf(future_red_str, "%ld", future_red);
	sprintf(extra_green_time_str, "%ld", extra_green_time);
	sprintf(ep_hold_marker_str, "%d", ep_hold_marker);
	sprintf(ep_ext_marker_str, "%d", ep_ext_marker);
	sprintf(ep_ext_timer_str, "%ld", ep_ext_timer);
	
	return aml_encode_message_json("RspLinkData", 11,
								"BonusGreenTime", bonus_green_time_str, JSMN_PRIMITIVE,
								"SmoothedFlow", smoothed_flow_str, JSMN_PRIMITIVE,
								"Endsat", endsat_str, JSMN_PRIMITIVE,
								"DemandType", demand_type_str, JSMN_PRIMITIVE,
								"NetBenFlow", net_ben_flow_str, JSMN_PRIMITIVE,
								"ActualFlow", actual_flow_str, JSMN_PRIMITIVE,
								"FutureRedTime", future_red_str, JSMN_PRIMITIVE,
								"ExtraGreenTime", extra_green_time_str, JSMN_PRIMITIVE,
								"EPHoldMarker", ep_hold_marker_str, JSMN_PRIMITIVE,
								"EPExtMarker", ep_ext_marker_str, JSMN_PRIMITIVE,
								"EPExtTimer", ep_ext_timer_str, JSMN_PRIMITIVE);
}

MVInt	aml_req_last_message_index_handler()
{
	MVInt last_index;
	char  last_index_str[LIMITED_INT_SIZE];
	
	last_index = UI_GetMessageCount() - 1;


	sprintf(last_index_str, "%d", last_index);

	return aml_encode_message_json("RspLastMessageIndex", 1,
									"Index", last_index_str, JSMN_PRIMITIVE);
}

MVStatus aml_req_message_by_index_handler(MVInt msg_index)
{
	MVInt  msg_details[MSG_MAX_LENGTH];

	MVInt last_index;


	last_index = UI_GetMessageCount() - 1;

	if(msg_index == last_index % MAX_MSGS )
		return aml_encode_message_json("RspMessageByIndex", 0);
	

	UI_GetMessage(msg_index, msg_details);
		
	/*Getting the type of the message*/
	if (msg_details[0] == 1 && msg_details[1] == 1)
	{
		return process_message_1_1(msg_details);
	}

	if (msg_details[0] == 1 && msg_details[1] == 2)
	{
		return process_message_1_2(msg_details);
	}

	if (msg_details[0] == 2 && msg_details[1] == 48)
	{
		return process_message_2(msg_details);
	}

	if (msg_details[0] == 3 && msg_details[1] != 2)
	{
		return process_message_3_1(msg_details);
	}

	if (msg_details[0] == 3 && msg_details[1] == 2)
	{
		return process_message_3_2(msg_details);
	}

	if (msg_details[0] == 4)
	{
		return process_message_4(msg_details);
	}
	/*Getting here, means that the read message will not be understood by MT (which is unlikely (impossible) to happen).*/

	if (msg_details[0] == 5 && msg_details[1] == 1)
	{
		return process_message_5_1(msg_details);
	}

	if (msg_details[0] == 5 && msg_details[1] == 2)
	{
		return process_message_5_2(msg_details);
	}

	if (msg_details[0] == 5 && msg_details[1] == 3)
	{
		return process_message_5_3(msg_details);
	}

	if (msg_details[0] == 5 && msg_details[1] == 4)
	{
		return process_message_5_4(msg_details);
	}

	if (msg_details[0] == 6 && msg_details[1] == 1)
	{
			/*Getting here means that the most recent message has been read, so there is no need to do any search.*/
		return process_message_6_1(msg_details);
	}

	if (msg_details[0] == 6 && msg_details[1] == 2)
	{
		return process_message_6_2(msg_details);
	}

	if (msg_details[0] == 7 && msg_details[1] == 1)
	{
		return process_message_7_1(msg_details); /*Sat flow message*/
	}

	/*Getting here, means that the read message will not be understood by MT (which is unlikely (impossible) to happen).*/
	return aml_encode_message_json("RspMessageByIndex", 0);
}

static MVStatus process_message_1_1(MVInt * msg_details)
{
	MVInt  i = 2, m;
	char    tmp[LIMITED_INT_SIZE];

	/*Message 1.1 fields*/
	char	stage_num_str[LIMITED_INT_SIZE];
	char	time_str[INT_STR_SIZE];
	char	smoothed_flow_str[(INT_STR_SIZE * MAX_LANES) + 1] = { 0 };
	char	total_smoothed_flow_str[INT_STR_SIZE];
	char	oversat_counter_str[(INT_STR_SIZE * MAX_LANES) + 1] = { 0 };
	char	max_lane_oversat_counter_str[INT_STR_SIZE];
	char	lambda_str[INT_STR_SIZE];
	char	total_lambda_str[INT_STR_SIZE];
	char	cut_max_times_marker_str[INT_STR_SIZE];
		
	sprintf(stage_num_str, "%d", msg_details[i++]);
	sprintf(time_str, "%.2d:%.2d:%.2d", msg_details[i], msg_details[i+1], msg_details[i+2]);
	i += 3;

	for (m = 0; m < UI_GetLanesCount(); m++)
	{
		sprintf(tmp, "%d,", msg_details[i + m]);
		strcat(smoothed_flow_str, tmp);
	}
	NULL_TERM(smoothed_flow_str);

	i += UI_GetLanesCount();

	sprintf(total_smoothed_flow_str, "%d", msg_details[i++]);

	for (m = 0; m < UI_GetLanesCount(); m++)
	{
		sprintf(tmp, "%d,", msg_details[i + m]);
		strcat(oversat_counter_str, tmp);
	}
	NULL_TERM(oversat_counter_str);

	i += UI_GetLanesCount();

	sprintf(max_lane_oversat_counter_str, "%d", msg_details[i++]);
	sprintf(lambda_str, "%d", msg_details[i++]);
	sprintf(total_lambda_str, "%d", msg_details[i++]);
	sprintf(cut_max_times_marker_str, "%d", msg_details[i++]);
	
	return aml_encode_message_json("RspMessageByIndex", 10,
								"MsgID", "M1.1", JSMN_STRING,
								"StageNum", stage_num_str, JSMN_PRIMITIVE,
								"Time", time_str, JSMN_STRING,
								"SmoothedFlow", smoothed_flow_str, JSMN_ARRAY + JSMN_PRIMITIVE,
								"TotalSmoothedFlow", total_smoothed_flow_str, JSMN_PRIMITIVE,
								"OversatCounter", oversat_counter_str, JSMN_ARRAY + JSMN_PRIMITIVE,
								"MaxLaneOversatCounter", max_lane_oversat_counter_str, JSMN_PRIMITIVE,
								"Lambda", lambda_str, JSMN_PRIMITIVE,
								"TotalLambda", total_lambda_str, JSMN_PRIMITIVE,
								"CutMaxTimesMarker", cut_max_times_marker_str, JSMN_PRIMITIVE);
}

static MVStatus process_message_1_2(MVInt * msg_details)
{
	MVInt  i = 2, j;
	char    tmp[INT_STR_SIZE];

	/*Message 1.2 fields*/
	char	smoothed_cycle_time_str[INT_STR_SIZE];
	char	drift_max_str[(INT_STR_SIZE * MAX_STAGES) + 1] = { 0 };
	char	suspicion_status_str[(3 * MAX_DETS) + 1] = { 0 };

	sprintf(smoothed_cycle_time_str, "%d", msg_details[i++]);

	for (j = 0; j < UI_GetStagesCount(); j++)
	{
		sprintf(tmp, "%d,", msg_details[i + j]);
		strcat(drift_max_str, tmp);
	}
	NULL_TERM(drift_max_str);

	i += UI_GetStagesCount();

	for (j = 0; j < UI_GetDetsCount(); j++)
	{
		sprintf(tmp, "%d,", msg_details[i + j]);
		strcat(suspicion_status_str, tmp);
	}
	NULL_TERM(suspicion_status_str);

	i += UI_GetDetsCount();

	return aml_encode_message_json("RspMessageByIndex", 4,
								"MsgID", "M1.2", JSMN_STRING,
								"SmoothedCycleTime", smoothed_cycle_time_str, JSMN_PRIMITIVE,
								"StagesDriftMax", drift_max_str, JSMN_ARRAY + JSMN_PRIMITIVE,
								"DetsSusStatus", suspicion_status_str, JSMN_ARRAY + JSMN_PRIMITIVE);
}

static MVStatus process_message_2(MVInt * msg_details)
{
	MVInt	i = 2, j;
	char	tmp[INT_STR_SIZE];

	/*Message 2 fields*/
	char	time_in_green_str[INT_STR_SIZE];
	char	var_min_green_time_str[INT_STR_SIZE];
	char	links_var_min_green_str[(INT_STR_SIZE * MAX_LINKS) + 1] = { 0 };
	char	red_count_x_str[(INT_STR_SIZE * MAX_LANES) + 1] = { 0 };


	sprintf(time_in_green_str, "%d", msg_details[i++]);
	sprintf(var_min_green_time_str, "%d", msg_details[i++]);

	for (j = 0; j < UI_GetLinksCount(); j++)
	{
		sprintf(tmp, "%d,", msg_details[i + j]);
		strcat(links_var_min_green_str, tmp);
	}
	NULL_TERM(links_var_min_green_str);

	i += UI_GetLinksCount();

	for (j = 0; j < UI_GetLanesCount(); j++)
	{
		sprintf(tmp, "%d,", msg_details[i + j]);
		strcat(red_count_x_str, tmp);
	}
	NULL_TERM(red_count_x_str);

	return aml_encode_message_json("RspMessageByIndex", 5,
								"MsgID", "M2", JSMN_STRING,
								"TimeInGreen", time_in_green_str, JSMN_PRIMITIVE,
								"VarMinGreenTime", var_min_green_time_str, JSMN_PRIMITIVE,
								"LinksVarMinGreenTime", links_var_min_green_str, JSMN_ARRAY + JSMN_PRIMITIVE,
								"RedCountX", red_count_x_str, JSMN_ARRAY + JSMN_PRIMITIVE);
}

/*Example: 160 NX 1  ESLI 0000 3LA 15 14 0  4LA 16 12 0 ... */
static MVStatus process_message_3_1(MVInt * msg_details)
{
	MVInt	i = 2, j, links_or_lanes_count;
	char	tmp[INT_STR_SIZE];

	/*Message 3.1 fields*/
	char	time_in_green_str[INT_STR_SIZE];
	char	next_stage_num_str[LIMITED_INT_SIZE];
	
	char	links_end_of_sat_str[(3 * MAX_LINKS) + 1] = { 0 };
	
	char	relevant_lanes_num_str[(3 * MAX_LINKS) + 1] = { 0 };
	char	relevant_lanes_old_gap_str[(INT_STR_SIZE * MAX_LINKS) + 1] = { 0 };
	char	relevant_lanes_current_gap_str[(INT_STR_SIZE * MAX_LINKS) + 1] = { 0 };
	char	relevant_lanes_waste_time_str[(INT_STR_SIZE * MAX_LINKS) + 1] = { 0 };

	sprintf(time_in_green_str, "%d", msg_details[i++]);
	sprintf(next_stage_num_str, "%d", msg_details[i++]);

	if (UI_GetLanesCount() <= UI_GetLinksCount())
		links_or_lanes_count = UI_GetLanesCount();
	else
		links_or_lanes_count = UI_GetLinksCount();

	for (j = 0; j < links_or_lanes_count; j++)
	{
		sprintf(tmp, "%d,", msg_details[i + j]);
		strcat(links_end_of_sat_str, tmp);
	}
	NULL_TERM(links_end_of_sat_str);

	i += UI_GetLinksCount();
		
	for (j = i; j < msg_details[1]; j+=4)
	{
		sprintf(tmp, "%d,", msg_details[j]);
		strcat(relevant_lanes_num_str, tmp);

		sprintf(tmp, "%d,", msg_details[j+1]);
		strcat(relevant_lanes_old_gap_str, tmp);

		sprintf(tmp, "%d,", msg_details[j+2]);
		strcat(relevant_lanes_current_gap_str, tmp);

		sprintf(tmp, "%d,", msg_details[j+3]);
		strcat(relevant_lanes_waste_time_str, tmp);
	}

	NULL_TERM(relevant_lanes_num_str);
	NULL_TERM(relevant_lanes_old_gap_str);
	NULL_TERM(relevant_lanes_current_gap_str);
	NULL_TERM(relevant_lanes_waste_time_str);


	return aml_encode_message_json("RspMessageByIndex", 8,
									"MsgID", "M3.1", JSMN_STRING,
									"TimeInGreen", time_in_green_str, JSMN_PRIMITIVE,
									"NextStageNum", next_stage_num_str, JSMN_PRIMITIVE,
									"LinksEndsat", links_end_of_sat_str, JSMN_ARRAY + JSMN_PRIMITIVE,
									"RelLanesNum", relevant_lanes_num_str, JSMN_ARRAY + JSMN_PRIMITIVE,
									"RelLanesOldGap", relevant_lanes_old_gap_str, JSMN_ARRAY + JSMN_PRIMITIVE,
									"RelLanesCurrentGap", relevant_lanes_current_gap_str, JSMN_ARRAY + JSMN_PRIMITIVE,
									"RelLanesWasteTime", relevant_lanes_waste_time_str, JSMN_ARRAY + JSMN_PRIMITIVE);
}

/*Example: 80 NX 1  hold 0  ext 4  xtimr 0    lxtm 0 0 0 0 0 0   0 0 1      revdem 111000 00-4*/
static MVStatus process_message_3_2(MVInt * msg_details)
{
	MVInt	i = 2, j;
	char	tmp[LIMITED_INT_SIZE];

	/*Message 3.2 fields*/
	char	time_in_green_str[INT_STR_SIZE];
	char	next_stage_num_str[LIMITED_INT_SIZE];
	char	ep_hold_marker_str[LIMITED_INT_SIZE];
	char	ep_ext_marker_str[LIMITED_INT_SIZE];
	char	ep_ext_max_timer_str[INT_STR_SIZE];

	char	ep_links_ext_timer_str[(INT_STR_SIZE * MAX_LINKS) + 1] = { 0 };

	sprintf(time_in_green_str, "%d", msg_details[i++]);
	sprintf(next_stage_num_str, "%d", msg_details[i++]);
	sprintf(ep_hold_marker_str, "%d", msg_details[i++]);
	sprintf(ep_ext_marker_str, "%d", msg_details[i++]);
	sprintf(ep_ext_max_timer_str, "%d", msg_details[i++]);

	for (j = 0; j < UI_GetLinksCount(); j++)
	{
		sprintf(tmp, "%d,", msg_details[i + j]);
		strcat(ep_links_ext_timer_str, tmp);
	}
	NULL_TERM(ep_links_ext_timer_str);
	
	i += UI_GetLinksCount();


	return aml_encode_message_json("RspMessageByIndex", 7 ,
									"MsgID", "M3.2", JSMN_STRING,
									"TimeInGreen", time_in_green_str, JSMN_PRIMITIVE,
									"NextStageNum", next_stage_num_str, JSMN_PRIMITIVE,
									"EPHoldMarker", ep_hold_marker_str, JSMN_PRIMITIVE,
									"EPExtMarker", ep_ext_marker_str, JSMN_PRIMITIVE,
									"EPExtMaxTimer", ep_ext_max_timer_str, JSMN_PRIMITIVE,
									"LinksExtTimer", ep_links_ext_timer_str, JSMN_ARRAY + JSMN_PRIMITIVE);
}


static MVStatus process_message_4(MVInt * msg_details)
{
	MVInt	i = 2, l;
	MVInt	losing_RoW_links_num = 0; /*RoW = Right-of-Way*/
	char	tmp[INT_STR_SIZE+1];

	/*Message 4 fields*/
	char	time_into_green_str[INT_STR_SIZE];
	char	next_stage_num_str[LIMITED_INT_SIZE];
	char	benefit_str[INT_STR_SIZE];
	char	disbenefit_str[INT_STR_SIZE];
	char	future_red_str[INT_STR_SIZE];

	char	links_losing_RoW_str[(INT_STR_SIZE * MAX_LINKS) + 1] = { 0 };
	char	links_net_benefit_flow_rate_str[(INT_STR_SIZE * MAX_LINKS) + 1] = { 0 };
	char	links_actual_flow_rate_str[(INT_STR_SIZE * MAX_LINKS) + 1] = { 0 };
	char	links_future_red_str[(INT_STR_SIZE * MAX_LINKS) + 1] = { 0 };
	char	links_extra_green_str[(INT_STR_SIZE * MAX_LINKS) + 1] = { 0 };

	/*char	losing_RoW_links_data[6*22];*/ /*RoW = Right-of-Way*/

	sprintf(time_into_green_str, "%d", msg_details[i++]);
	sprintf(next_stage_num_str, "%d", msg_details[i++]);
	sprintf(benefit_str, "%d", msg_details[i++]);
	sprintf(disbenefit_str, "%d", msg_details[i++]);
	sprintf(future_red_str, "%d", msg_details[i++]);

	/*The rest of the message will be put as one field*/
	losing_RoW_links_num = (msg_details[1] - 7) / 5; /*Note: msg_details[1] holds the message size*/

	l = 7;

	for (i = 0; i < losing_RoW_links_num; i++)
	{
		sprintf(tmp, "%d,", msg_details[l++]);
		strcat(links_losing_RoW_str, tmp);

		sprintf(tmp, "%d,", msg_details[l++]);
		strcat(links_net_benefit_flow_rate_str, tmp);

		sprintf(tmp, "%d,", msg_details[l++]);
		strcat(links_actual_flow_rate_str, tmp);

		sprintf(tmp, "%d,", msg_details[l++]);
		strcat(links_future_red_str, tmp);

		sprintf(tmp, "%d,", msg_details[l++]);
		strcat(links_extra_green_str, tmp);
	}

	NULL_TERM(links_losing_RoW_str);
	NULL_TERM(links_net_benefit_flow_rate_str);
	NULL_TERM(links_actual_flow_rate_str);
	NULL_TERM(links_future_red_str);
	NULL_TERM(links_extra_green_str);
	

	return aml_encode_message_json("RspMessageByIndex", 11,
									"MsgID", "M4", JSMN_STRING,
									"TimeIntoGreen", time_into_green_str, JSMN_PRIMITIVE,
									"NextStageNum", next_stage_num_str, JSMN_PRIMITIVE,
									"Benefit", benefit_str, JSMN_PRIMITIVE,
									"Disbenefit", disbenefit_str, JSMN_PRIMITIVE,
									"FutureRed", future_red_str, JSMN_PRIMITIVE,
									"LinksLosingRoW", links_losing_RoW_str, JSMN_ARRAY + JSMN_PRIMITIVE,
									"LinksNetBenefitFlowRate", links_net_benefit_flow_rate_str, JSMN_ARRAY + JSMN_PRIMITIVE,
									"LinksActualFlowRate", links_actual_flow_rate_str, JSMN_ARRAY + JSMN_PRIMITIVE,
									"LinksFutureRed", links_future_red_str, JSMN_ARRAY + JSMN_PRIMITIVE,
									"LinksExtraGreen", links_extra_green_str, JSMN_ARRAY + JSMN_PRIMITIVE);
}


static MVStatus process_message_5_1(MVInt * msg_details)
{
	MVInt	i = 7;

	/*Message 5 common (first part)*/
	char	time_in_green_str[INT_STR_SIZE];
	char	next_stage_num_str[LIMITED_INT_SIZE];
	char	demand_time_str[INT_STR_SIZE+1];
		
	/*Message 5.1 fields*/
	char	benefit_str[INT_STR_SIZE];
	char	disbenefit_str[INT_STR_SIZE];
	char	future_red_time[INT_STR_SIZE];
	char	cycle_expansion_factor[INT_STR_SIZE];

	/*Message 5 common (second part)*/
	char	links_demand_marker_str[(LIMITED_INT_SIZE * MAX_LINKS) + 1] = { 0 };
	char	lanes_red_count_str[(INT_STR_SIZE * MAX_LANES) + 1] = { 0 };
	
	add_message_5_common_data(msg_details, 11, time_in_green_str, next_stage_num_str, demand_time_str, links_demand_marker_str, lanes_red_count_str);

	sprintf(benefit_str, "%d", msg_details[i++]);
	sprintf(disbenefit_str, "%d", msg_details[i++]);
	sprintf(future_red_time, "%d", msg_details[i++]);
	sprintf(cycle_expansion_factor, "%d", msg_details[i++]);

	return aml_encode_message_json("RspMessageByIndex", 10 ,
									"MsgID", "M5.1", JSMN_STRING,
									"TimeIntoGreen", time_in_green_str, JSMN_PRIMITIVE,
									"NextStageNum", next_stage_num_str, JSMN_PRIMITIVE,
									"DemandTime", demand_time_str, JSMN_STRING,
									"Benefit", benefit_str, JSMN_PRIMITIVE,
									"Disbenefit", disbenefit_str, JSMN_PRIMITIVE,
									"FutureRed", future_red_time, JSMN_PRIMITIVE,
									"CycleExpansionFactor", cycle_expansion_factor, JSMN_PRIMITIVE,
									"LinksDemandMarker", links_demand_marker_str, JSMN_ARRAY + JSMN_PRIMITIVE,
									"LanesRedCount", lanes_red_count_str, JSMN_ARRAY + JSMN_PRIMITIVE);
}

static MVStatus process_message_5_2(MVInt * msg_details)
{
	/*Message 5 common (first part)*/
	char	time_in_green_str[INT_STR_SIZE];
	char	next_stage_num_str[LIMITED_INT_SIZE];
	char	demand_time_str[INT_STR_SIZE+1];

	/*Message 5.2 fields*/
	char	ped_demand_marker_str[LIMITED_INT_SIZE];
	char	max_change_str[INT_STR_SIZE];


	/*Message 5 common (second part)*/
	char	links_demand_marker_str[(LIMITED_INT_SIZE * MAX_LINKS) + 1] = { 0 };
	char	lanes_red_count_str[(INT_STR_SIZE * MAX_LANES) + 1] = { 0 };

	add_message_5_common_data(msg_details, 8, time_in_green_str, next_stage_num_str, demand_time_str, links_demand_marker_str, lanes_red_count_str);

	sprintf(ped_demand_marker_str, "%d", UI_GetPedDemandMarker());

	sprintf(max_change_str, "%d", msg_details[7]);

	return aml_encode_message_json("RspMessageByIndex", 8,
									"MsgID", "M5.2", JSMN_STRING,
									"TimeIntoGreen", time_in_green_str, JSMN_PRIMITIVE,
									"NextStageNum", next_stage_num_str, JSMN_PRIMITIVE,
									"DemandTime", demand_time_str, JSMN_STRING,
									"PedDemandMarker", ped_demand_marker_str, JSMN_PRIMITIVE,
									"MaxChange", max_change_str, JSMN_PRIMITIVE,
									"LinksDemandMarker", links_demand_marker_str, JSMN_ARRAY + JSMN_PRIMITIVE,
									"LanesRedCount", lanes_red_count_str, JSMN_ARRAY + JSMN_PRIMITIVE);
}

static MVStatus process_message_5_3(MVInt * msg_details)
{
	MVInt	i = 7, j;
	char	tmp[INT_STR_SIZE];

	/*Message 5 common (first part)*/
	char	time_in_green_str[INT_STR_SIZE];
	char	next_stage_num_str[LIMITED_INT_SIZE];
	char	demand_time_str[INT_STR_SIZE+1];

	/*Message 5.3 fields*/
	char	max_percent_sat_str[(INT_STR_SIZE * MAX_LINKS) + 1] = { 0 };
	
	/*Message 5 common (second part)*/
	char	links_demand_marker_str[(LIMITED_INT_SIZE * MAX_LINKS) + 1] = { 0 };
	char	lanes_red_count_str[(INT_STR_SIZE * MAX_LANES) + 1] = { 0 };

	for (j = 0; j < UI_GetLinksCount(); j++)
	{
		sprintf(tmp, "%d,", msg_details[i + j]);
		strcat(max_percent_sat_str, tmp);
	}
	NULL_TERM(max_percent_sat_str);

	i += UI_GetLinksCount();
	
	add_message_5_common_data(msg_details, i, time_in_green_str, next_stage_num_str, demand_time_str, links_demand_marker_str, lanes_red_count_str);
	
	return aml_encode_message_json("RspMessageByIndex", 7,
									"MsgID", "M5.3", JSMN_STRING,
									"TimeIntoGreen", time_in_green_str, JSMN_PRIMITIVE,
									"NextStageNum", next_stage_num_str, JSMN_PRIMITIVE,
									"DemandTime", demand_time_str, JSMN_STRING,
									"MaxPercentChange", max_percent_sat_str, JSMN_ARRAY + JSMN_PRIMITIVE,
									"LinksDemandMarker", links_demand_marker_str, JSMN_ARRAY + JSMN_PRIMITIVE,
									"LanesRedCount", lanes_red_count_str, JSMN_ARRAY + JSMN_PRIMITIVE);
}



static MVStatus process_message_5_4(MVInt * msg_details)
{
	MVInt	i = 7;

	/*Message 5 common (first part)*/
	char	time_in_green_str[INT_STR_SIZE];
	char	next_stage_num_str[LIMITED_INT_SIZE];
	char	demand_time_str[TIME_STR_SIZE];

	/*Message 5.4 fields*/
	char	next_emeregency_stage_to_demand_str[LIMITED_INT_SIZE];
	char	next_priority_stage_to_demand_str[LIMITED_INT_SIZE];

	/*Message 5 common (second part)*/
	char	links_demand_marker_str[(LIMITED_INT_SIZE * MAX_LINKS) + 1] = { 0 };
	char	lanes_red_count_str[(INT_STR_SIZE * MAX_LANES) + 1] = { 0 };

	sprintf(next_emeregency_stage_to_demand_str, "%d", msg_details[i++]);
	sprintf(next_priority_stage_to_demand_str, "%d", msg_details[i++]);
	
	add_message_5_common_data(msg_details, 9, time_in_green_str, next_stage_num_str, demand_time_str, links_demand_marker_str, lanes_red_count_str);
	
	return aml_encode_message_json("RspMessageByIndex", 8,
									"MsgID", "M5.4", JSMN_STRING,
									"TimeIntoGreen", time_in_green_str, JSMN_PRIMITIVE,
									"NextStageNum", next_stage_num_str, JSMN_PRIMITIVE,
									"DemandTime", demand_time_str, JSMN_STRING,
									"NextEmgStageToDemand", next_emeregency_stage_to_demand_str, JSMN_PRIMITIVE,
									"NextPriStageToDemand", next_priority_stage_to_demand_str, JSMN_PRIMITIVE,		
									"LinksDemandMarker", links_demand_marker_str, JSMN_ARRAY + JSMN_PRIMITIVE,
									"LanesRedCount", lanes_red_count_str, JSMN_ARRAY + JSMN_PRIMITIVE);
}


static void add_message_5_common_data(MVInt * msg_details, MVInt idx, char * time_in_green_str, char * next_stage_num_str, char * demand_time_str, char * links_demand_marker_str, char * lanes_red_count_str)
{
	MVInt	i = 2, j, m;
	char	tmp[LIMITED_INT_SIZE];
	
	/*First part of 5 common*/
	sprintf(time_in_green_str, "%d", msg_details[i++]);
	sprintf(next_stage_num_str, "%d", msg_details[i++]);
	sprintf(demand_time_str, "%.2d:%.2d:%.2d", msg_details[i], msg_details[i+1], msg_details[i+2]);

	
	/*Second part of 5 common*/
	for (j = 0; j < UI_GetLinksCount(); j++)
	{
		sprintf(tmp, "%d,", msg_details[idx + j]);
		strcat(links_demand_marker_str, tmp);
	}
	NULL_TERM(links_demand_marker_str);

	j = idx + UI_GetLinksCount();

	for (m = 0; m < UI_GetLanesCount(); m++)
	{
		sprintf(tmp, "%d,", msg_details[j+m]);
		strcat(lanes_red_count_str, tmp);
	}
	NULL_TERM(lanes_red_count_str);
}


static MVStatus process_message_6_1(MVInt * msg_details)
{
	MVInt	i = 2, j;
	char	tmp[INT_STR_SIZE];

	/*Message 6.1 fields*/
	char	start_ig_time_str[INT_STR_SIZE];
	char	stages_demand_marker_str[(LIMITED_INT_SIZE * MAX_STAGES) + 1] = { 0 };
	char	links_bonus_green_str[(INT_STR_SIZE * MAX_LINKS) + 1] = { 0 };
	char	red_count_in_str[(INT_STR_SIZE * MAX_LINKS) + 1] = { 0 };


	sprintf(start_ig_time_str, "%d", msg_details[i++]);

	for (j = 0; j < UI_GetStagesCount(); j++)
	{
		sprintf(tmp, "%d,", msg_details[i + j]);
		strcat(stages_demand_marker_str, tmp);
	}
	NULL_TERM(stages_demand_marker_str);

	i += UI_GetStagesCount();

	for (j = 0; j < UI_GetLinksCount(); j++)
	{
		sprintf(tmp, "%d,", msg_details[i + j]);
		strcat(links_bonus_green_str, tmp);
	}
	NULL_TERM(links_bonus_green_str);

	i += UI_GetLinksCount();

	for (j = 0; j < UI_GetLanesCount(); j++)
	{
		sprintf(tmp, "%d,", msg_details[i + j]);
		strcat(red_count_in_str, tmp);
	}
	NULL_TERM(red_count_in_str);

	return aml_encode_message_json("RspMessageByIndex", 5,
								"MsgID", "M6.1", JSMN_STRING,
								"StartIG_Time", start_ig_time_str, JSMN_PRIMITIVE,
								"StagesDemandMarker", stages_demand_marker_str, JSMN_ARRAY + JSMN_PRIMITIVE,
								"LinksBonusGreen", links_bonus_green_str, JSMN_ARRAY + JSMN_PRIMITIVE,
								"RedCountIN", red_count_in_str, JSMN_ARRAY + JSMN_PRIMITIVE);
}


static MVStatus process_message_6_2(MVInt * msg_details)
{
	MVInt i = 2, j;
	char	tmp[INT_STR_SIZE];

	/*Message 6.2 fields*/
	char	msg_time_in_seconds_str[TIME_STR_SIZE];
	char	ep_stage_demand_marker_str[(4 * MAX_STAGES) +1] = { 0 };
	char	ep_trunc_indicator_str[(7 * MAX_LINKS) + 1] = { 0 };
	char	ep_skip_indicator_str[(7 * MAX_LINKS) + 1] = { 0 };

	sprintf(msg_time_in_seconds_str, "%d", msg_details[i++]);

	for (j = 0; j < UI_GetStagesCount(); j++)
	{
		sprintf(tmp, "%d,", msg_details[i + j]);
		strcat(ep_stage_demand_marker_str, tmp);
	}
	NULL_TERM(ep_stage_demand_marker_str);

	i += UI_GetStagesCount();

	for (j = 0; j < UI_GetLinksCount(); j++)
	{
		sprintf(tmp, "%d,", msg_details[i + j]);
		strcat(ep_trunc_indicator_str, tmp);
	}
	NULL_TERM(ep_trunc_indicator_str);

	i += UI_GetLinksCount();

	for (j = 0; j < UI_GetLinksCount(); j++)
	{
		sprintf(tmp, "%d,", msg_details[i + j]);
		strcat(ep_skip_indicator_str, tmp);
	}
	NULL_TERM(ep_skip_indicator_str);

	return aml_encode_message_json("RspMessageByIndex", 5 ,
									"MsgID", "M6.2", JSMN_STRING,
									"MsgTimeInSecs", msg_time_in_seconds_str, JSMN_PRIMITIVE,
									"EPStageDemandMarker", ep_stage_demand_marker_str, JSMN_ARRAY + JSMN_PRIMITIVE,
									"EPLinksTruncIndicator", ep_trunc_indicator_str, JSMN_ARRAY + JSMN_PRIMITIVE,
									"EPLinksSkipIndicator", ep_skip_indicator_str, JSMN_ARRAY + JSMN_PRIMITIVE);
}


static MVStatus process_message_7_1(MVInt * msg_details)
{
	MVInt i = 2;

	char lane_num_str[LIMITED_INT_SIZE];

	char is_exit_blocked_str[BIT_SIZE];
	char in_det_stand_q_str[BIT_SIZE];
	char q_over_x_det_str[INT_STR_SIZE];
	char is_sat_flow_after_var_min_green_str[BIT_SIZE];
	char fully_sat_str[BIT_SIZE];
	char long_q_sat_count_str[LONG_STR_SIZE];
	char long_queue_sat_flow_str[LONG_STR_SIZE];
	char short_queue_sat_flow_str[LONG_STR_SIZE];
	char raw_sat_flow_str[LONG_STR_SIZE];
	char raw_stlost_str[LONG_STR_SIZE];
	char raw_flow_rate_str[LONG_STR_SIZE];
	char smoothed_occ_str[LONG_STR_SIZE];
	

	sprintf(lane_num_str, "%d", msg_details[i++]);

	if (msg_details[i++] == TRUE)  sprintf(is_exit_blocked_str, "%s", "T");  else sprintf(is_exit_blocked_str, "%s", "F");
	if (msg_details[i++] == TRUE)  sprintf(in_det_stand_q_str, "%s", "T");  else sprintf(in_det_stand_q_str, "%s", "F");
	if (msg_details[i++] == TRUE)  sprintf(q_over_x_det_str, "%s", "T");  else sprintf(q_over_x_det_str, "%s", "F");
	if (msg_details[i++] == TRUE)  sprintf(is_sat_flow_after_var_min_green_str, "%s", "T");  else sprintf(is_sat_flow_after_var_min_green_str, "%s", "F");
	if (msg_details[i++] == TRUE)  sprintf(fully_sat_str, "%s", "T");  else sprintf(fully_sat_str, "%s", "F");
	
	sprintf(long_q_sat_count_str, "%d", msg_details[i++]);
	sprintf(long_queue_sat_flow_str, "%d", msg_details[i++]);
	sprintf(short_queue_sat_flow_str, "%d", msg_details[i++]);
	sprintf(raw_sat_flow_str, "%d", msg_details[i++]);
	sprintf(raw_stlost_str, "%d", msg_details[i++]);
	sprintf(raw_flow_rate_str, "%d", msg_details[i++]);
	sprintf(smoothed_occ_str, "%d", msg_details[i++]);
	
	return aml_encode_message_json("RspMessageByIndex", 14,
								"MsgID", "M7.1", JSMN_STRING,
								"LaneID", lane_num_str, JSMN_PRIMITIVE,
								"IsExitBlocked", is_exit_blocked_str, JSMN_PRIMITIVE,
								"IN_DetStandingQ", in_det_stand_q_str, JSMN_PRIMITIVE,
								"Q_OverX_Det", q_over_x_det_str, JSMN_PRIMITIVE,
								"IsSatFlowAfterVarMinGreen", is_sat_flow_after_var_min_green_str, JSMN_PRIMITIVE,
								"FullySat", fully_sat_str, JSMN_PRIMITIVE,
								"LongQ_SatCount", long_q_sat_count_str, JSMN_PRIMITIVE,
								"LongQ_SatFlow", long_queue_sat_flow_str, JSMN_PRIMITIVE,
								"ShortQ_SatFlow", short_queue_sat_flow_str, JSMN_PRIMITIVE,
								"RawSatFlow", raw_sat_flow_str, JSMN_PRIMITIVE,
								"RawSTLOST", raw_stlost_str, JSMN_PRIMITIVE,
								"RawFlowRate", raw_flow_rate_str, JSMN_PRIMITIVE,
								"SmoothedOcc", smoothed_occ_str, JSMN_PRIMITIVE);
}

MVStatus aml_req_error_count_handler()
{
	MVInt		error_count = 0;
	char		error_count_str[INT_STR_SIZE];

	error_count = UI_GetErrorCount();

	sprintf(error_count_str, "%d", error_count);

	return aml_encode_message_json("RspErrorCount", 1, 
									"Count", error_count_str, JSMN_PRIMITIVE);
}

MVStatus aml_req_error_detail_handler(MVInt error_index)
{
	MVByte	error_id;
	time_t	error_time;
	MVInt	add1;
	MVInt	add2;
	MVInt	add3;

	struct tm * ts;
	char		date_time_str[DATE_TIME_SIZE];
	char		error_id_str[INT_STR_SIZE];
	char		add1_str[INT_STR_SIZE];
	char		add2_str[INT_STR_SIZE];
	char		add3_str[INT_STR_SIZE];

	CHECK_RANGE(error_index, 1, MAX_NO_OF_ERRORS_STORED);

	UI_GetError(error_index, &error_id, &error_time, &add1, &add2, &add3);
	
	sprintf(error_id_str, "%d", error_id);
	
	ts = localtime(&error_time);

	strftime(date_time_str, sizeof(date_time_str), "%Y-%m-%dT%H:%M:%SZ", ts);
	
	sprintf(add1_str, "%d", add1);
	sprintf(add2_str, "%d", add2);
	sprintf(add3_str, "%d", add3);
	
	return aml_encode_message_json("RspErrorDetail", 5,
								"ErrorID", error_id_str, JSMN_PRIMITIVE,
								"DateTime", date_time_str, JSMN_STRING,
								"Add1", add1_str, JSMN_PRIMITIVE,
								"Add2", add2_str, JSMN_PRIMITIVE,
								"Add3", add3_str, JSMN_PRIMITIVE);
}


MVStatus aml_req_clear_error_log_handler()
{
	UI_ClearErrorLog();
	
	return aml_encode_message_json("RspClearErrorLog", 0);	
}


MVStatus aml_req_sat_flow_period_handler(MVInt period_index)
{
	MVInt	period_start;
	MVInt	period_end;

	char	period_start_str[INT_STR_SIZE];
	char	period_end_str[INT_STR_SIZE];

	CHECK_RANGE(period_index, 1, xgnSFTIMS_MAX);


	/*This is to get the periods of the active area.*/
	UI_GetSatFlowTimingPeriod(period_index, &period_start, &period_end);

	if (period_index < 1 || period_index > 6)
		return MV_FAILURE;

	sprintf(period_start_str, "%2.2d:%2.2d", period_start / 60, period_start % 60);
	sprintf(period_end_str, "%2.2d:%2.2d", period_end / 60, period_end % 60);

	return aml_encode_message_json("RspSatFlowPeriod", 2,
									"Start", period_start_str, JSMN_STRING,
									"End", period_end_str, JSMN_STRING);
}

MVStatus aml_req_sat_flow_details_handler(MVInt lane_id, MVInt period_index)
{
	MVInt	esv;
	MVInt	satinc;
	MVInt	last_mean;
	MVInt	conf_level;
	MVBool	stats_valid;
	MVInt	total_count;
	MVInt	mean;
	MVInt	smoothed_15;
	MVInt	sum_squared;

	char	esv_str[INT_STR_SIZE];
	char	satinc_str[INT_STR_SIZE];
	char	last_mean_str[INT_STR_SIZE];
	char	conf_level_str[INT_STR_SIZE];
	char	stats_valid_str[INT_STR_SIZE];
	char	total_count_str[INT_STR_SIZE];
	char	mean_str[INT_STR_SIZE];
	char	smoothed_15_str[LONG_STR_SIZE];
	char	sum_squared_str[LONG_STR_SIZE];

	CHECK_RANGE(lane_id, 0, MAX_LANES - 1);
	CHECK_RANGE(period_index, 1, 6);


	UI_GetSatFlowDetails((MVByte)lane_id, period_index,
		&esv, &satinc, &last_mean, &conf_level, &stats_valid, &total_count, &mean, &smoothed_15, &sum_squared);

	sprintf(esv_str, "%d", esv);
	sprintf(satinc_str, "%d", satinc);
	sprintf(last_mean_str, "%d", last_mean);
	sprintf(conf_level_str, "%d", conf_level);

	if(stats_valid == MV_TRUE)
		sprintf(stats_valid_str, "T");
	else
		sprintf(stats_valid_str, "F");

	sprintf(total_count_str, "%d", total_count);
	sprintf(mean_str, "%d", mean);
	sprintf(smoothed_15_str, "%d", smoothed_15);
	sprintf(sum_squared_str, "%d", sum_squared);
	
	return aml_encode_message_json("RspSatFlowDetails", 9,
								"ESV", esv_str, JSMN_PRIMITIVE,
								"SATINC", satinc_str, JSMN_PRIMITIVE,
								"LastVaueMeasured", last_mean_str, JSMN_PRIMITIVE,
								"ConfLevel", conf_level_str, JSMN_PRIMITIVE,
								"IsStatsOk", stats_valid_str, JSMN_PRIMITIVE,
								"TotalCount", total_count_str, JSMN_PRIMITIVE,
								"Mean", mean_str, JSMN_PRIMITIVE,
								"Smoothed15", smoothed_15_str, JSMN_PRIMITIVE,
								"SumSquared", sum_squared_str, JSMN_PRIMITIVE);
}

MVStatus aml_req_stlost_details_handler(MVInt lane_id, MVInt period_index)
{
	MVInt	esv;
	MVInt	last_mean;
	MVInt	conf_level;
	MVBool	stats_valid;
	MVInt	total_count;
	MVInt	mean;
	MVInt	sum_squared;

	char	esv_str[INT_STR_SIZE];
	char	last_mean_str[INT_STR_SIZE];
	char	conf_level_str[INT_STR_SIZE];
	char	stats_valid_str[INT_STR_SIZE];
	char	total_count_str[INT_STR_SIZE];
	char	mean_str[INT_STR_SIZE];
	char	sum_squared_str[LONG_STR_SIZE];

	CHECK_RANGE(lane_id, 0, MAX_LANES - 1);
	CHECK_RANGE(period_index, 1, 6);


	UI_GetST_LostDetails((MVByte)lane_id, period_index,
		&esv, &last_mean, &conf_level, &stats_valid, &total_count, &mean, &sum_squared);

	sprintf(esv_str, "%d", esv);
	sprintf(last_mean_str, "%d", last_mean);
	sprintf(conf_level_str, "%d", conf_level);

	if (stats_valid == MV_TRUE)
		sprintf(stats_valid_str, "T");
	else
		sprintf(stats_valid_str, "F");

	sprintf(total_count_str, "%d", total_count);
	sprintf(mean_str, "%d", mean);
	sprintf(sum_squared_str, "%d", sum_squared);

	return aml_encode_message_json("RspSTLOST_Details", 7,
									"ESV", esv_str, JSMN_PRIMITIVE,
									"LastVaueMeasured", last_mean_str, JSMN_PRIMITIVE,
									"ConfLevel", conf_level_str, JSMN_PRIMITIVE,
									"IsStatsOk", stats_valid_str, JSMN_PRIMITIVE,
									"TotalCount", total_count_str, JSMN_PRIMITIVE,
									"Mean", mean_str, JSMN_PRIMITIVE,
									"SumSquared", sum_squared_str, JSMN_PRIMITIVE);
}

MVStatus aml_req_avg_flow_handler(MVInt day_id, MVInt lane_index, MVInt time_index)
{
	MVInt	avg_flow;

	char	avg_flow_str[INT_STR_SIZE];

	CHECK_RANGE(day_id, 1, 7);
	CHECK_RANGE(lane_index, 1, MAX_LANES);
	CHECK_RANGE(time_index, 1, 48);
	
	avg_flow = UI_GetAverageFlow((MVByte)day_id, (MVByte)time_index, (MVByte)lane_index);

	sprintf(avg_flow_str, "%d", avg_flow);

	return aml_encode_message_json("RspAvgFlow", 1, 
									"AvgFlow", avg_flow_str, JSMN_PRIMITIVE);
}

MVStatus aml_req_mova_time_setting_handler(char * date_time_str)
{
	time_t		date_time;
	struct tm	tm;
	
	MVInt		day, month, year;
	MVInt		hour, minute, second;
	
	if (date_time_str == NULL)
		return MV_FAILURE;
		
	if (sscanf(date_time_str, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) return MV_FAILURE;

	
	tm.tm_mday = day;
	tm.tm_mon = month -1;
	tm.tm_year = year - 1900;

	tm.tm_hour = hour;
	tm.tm_min = minute;
	tm.tm_sec = second;
	tm.tm_isdst = -1;

	date_time = mktime(&tm);

	if (date_time != -1)
	{
		UI_SetMovaTime(&date_time);
	}

	return aml_encode_message_json("RspMOVATimeSetting", 0);
}


MVStatus aml_req_alert_flags_handler()
{
	MVBool blocking_status;
	MVBool oversat_status;
	MVBool occupancy_status;

	char blocking_status_str[BIT_SIZE];
	char oversat_status_str[BIT_SIZE];
	char occupancy_status_str[BIT_SIZE];

	UI_GetAlertMonitoring(&blocking_status, &oversat_status, &occupancy_status);

	if (blocking_status)
		sprintf(blocking_status_str, "%s", "T");
	else
		sprintf(blocking_status_str, "%s", "F");


	if (oversat_status)
		sprintf(oversat_status_str, "%s", "T");
	else
		sprintf(oversat_status_str, "%s", "F");


	if (occupancy_status)
		sprintf(occupancy_status_str, "%s", "T");
	else
		sprintf(occupancy_status_str, "%s", "F");
	


	return aml_encode_message_json("RspAlertMonitoringFlags", 3,
									"BlockingStatus", blocking_status_str, JSMN_PRIMITIVE,
									"OversatStatus", oversat_status_str, JSMN_PRIMITIVE,
									"OccupancyStatus", occupancy_status_str, JSMN_PRIMITIVE);

}

MVStatus aml_req_alert_status_handler()
{
	MVBool blocking_status;
	MVBool oversat_status;
	MVBool occupancy_status;

	char blocking_status_str[BIT_SIZE];
	char oversat_status_str[BIT_SIZE];
	char occupancy_status_str[BIT_SIZE];

	UI_GetAlertStatus(&blocking_status, &oversat_status, &occupancy_status);

	if (blocking_status)
		sprintf(blocking_status_str, "%s", "T");
	else
		sprintf(blocking_status_str, "%s", "F");


	if (oversat_status)
		sprintf(oversat_status_str, "%s", "T");
	else
		sprintf(oversat_status_str, "%s", "F");


	if (occupancy_status)
		sprintf(occupancy_status_str, "%s", "T");
	else
		sprintf(occupancy_status_str, "%s", "F");



	return aml_encode_message_json("RspAlertStatus", 3,
		"BlockingStatus", blocking_status_str, JSMN_PRIMITIVE,
		"OversatStatus", oversat_status_str, JSMN_PRIMITIVE,
		"OccupancyStatus", occupancy_status_str, JSMN_PRIMITIVE);
}

MVStatus aml_req_alert_flags_setting_handler(MVBool blocking_status, MVBool oversat_status, MVBool occupancy_status)
{
	UI_SetAlertMonitoring(blocking_status, occupancy_status, oversat_status);

	return aml_encode_message_json("RspAlertMonitoringFlagsSetting", 0);
}

MVStatus aml_req_check_dataset_compatibility_handler(MVUInt8 stages_count, MVUInt8 links_count, MVUInt8 lanes_count)
{
	MVBool is_ok = MV_TRUE;
	char is_ok_str[BIT_SIZE];


	if (stages_count != UI_GetStagesCount() || links_count != UI_GetLinksCount() || lanes_count != UI_GetLanesCount())
	{
		is_ok = MV_FALSE;
	}

	aml_dataset_set_compatibility(is_ok);

	if (is_ok == MV_TRUE)
		sprintf(is_ok_str, "%s", "T");
	else
		sprintf(is_ok_str, "%s", "F");


	return aml_encode_message_json("RspCheckDatasetCompatibility", 1,
		"IsOk", is_ok_str, JSMN_PRIMITIVE);

}

MVStatus aml_req_dataset_transfer_handler(char * file_name, int file_size)
{
	MVBool is_ready_for_transfer = MV_TRUE;
	MVUInt8 failure_reason = 0;

	char is_ready_for_transfer_str[BIT_SIZE];
	char failure_reason_str[INT_STR_SIZE];


	/*Check if the dataset that will be sent is compatible with the active one. 
	Note: The previous message (ReqCheckDatasetCompatibility) has done this check, and the following function
	just recalls the results of this checking.*/
	if (aml_dataset_is_compatible())
	{
		if (aml_enable_dataset_receiving(file_name, file_size) == MV_FAILURE)
		{
			/*Getting here means that the kernel didn't manage to create the file that will host the transferred dataset.*/
			is_ready_for_transfer = MV_FALSE;
			failure_reason = FR_UNABLE_TO_CREATE_FILE;
		}
	}
	else
	{
		/*The dataset (that will be sent) is not compatible with the active one, so let's check if MOVA is On-Control*/
		if (UI_IsMOVAOnControl())
		{
			/*MOVA in On-control, so there is no way to accept this dataset file.*/
			is_ready_for_transfer = MV_FALSE;
			failure_reason = FR_MOVA_IS_ON_CONTROL;
		}
		else
		{
			if (aml_enable_dataset_receiving(file_name, file_size) == MV_FAILURE)
			{
				/*Getting here means that the kernel didn't manage to create the file that will host the transferred dataset.*/
				is_ready_for_transfer = MV_FALSE;
				failure_reason = FR_UNABLE_TO_CREATE_FILE;
			}
		}
	}
	
	if (is_ready_for_transfer)
		sprintf(is_ready_for_transfer_str, "%s", "T");
	else
		sprintf(is_ready_for_transfer_str, "%s", "F");
	
	sprintf(failure_reason_str, "%d", failure_reason);

	return aml_encode_message_json("RspDatasetTransfer", 2,
									"IsReadyForTransfer", is_ready_for_transfer_str, JSMN_PRIMITIVE,
									"FailureReason", failure_reason_str, JSMN_PRIMITIVE);
}

MVStatus aml_req_check_transfered_file_integrity_handler(MVUInt32 file_crc32)
{
	MVBool is_file_ok = MV_FALSE;
	char is_file_ok_str[BIT_SIZE];
	
	is_file_ok = aml_dataset_file_check_integrity(file_crc32);

	if (is_file_ok)
		sprintf(is_file_ok_str, "%s", "T");
	else
		sprintf(is_file_ok_str, "%s", "F");


	return aml_encode_message_json("RspCheckTransferedFileIntegrity", 1,
									"IsFileOk", is_file_ok_str, JSMN_PRIMITIVE);
	
}

MVStatus aml_req_load_dataset_into_memeory_handler(MVInt8 dp1_index, MVInt8 dp2_index, MVInt8 dp3_index, MVInt8 dp4_index)
{
	MVStatus is_successful = MV_FAILURE;

	char is_successful_str[BIT_SIZE];
	char dataset_file_name[FILENAME_MAX];
	

	aml_dataset_get_file_name(dataset_file_name);

	if (strlen(dataset_file_name) != 0)
	{
		is_successful = UI_LoadDatasetFromXMLFileIntoMemeory(dataset_file_name, dp1_index, dp2_index, dp3_index, dp4_index);
	}
	else
	{
		/*Getting here means that the dataset file was not initialised by the message "ReqDatasetTrasnfer"*/
		is_successful = MV_FAILURE;
	}

	if (is_successful == MV_SUCCESS)
		sprintf(is_successful_str, "%s", "T");
	else
		sprintf(is_successful_str, "%s", "F");


	return aml_encode_message_json("RspLoadDatasetIntoMemory", 1,
		"IsSuccessful", is_successful_str, JSMN_PRIMITIVE);
}

MVStatus aml_req_activate_data_plan_handler(MVInt8 data_plan_index)
{
	MVStatus is_successful = MV_FAILURE;
	char is_successful_str[BIT_SIZE];

	CHECK_RANGE(data_plan_index, 1, 4)

	is_successful = UI_ActivateDataPlan(data_plan_index);

	if (is_successful == MV_SUCCESS)
		sprintf(is_successful_str, "%s", "T");
	else
		sprintf(is_successful_str, "%s", "F");


	return aml_encode_message_json("RspActivateDataPlan", 1,
		"IsSuccessful", is_successful_str, JSMN_PRIMITIVE);
}

MVStatus aml_req_current_active_plan_index_handler()
{
	MVInt active_data_plan_index = 0;
	char active_data_plan_index_str[LIMITED_INT_SIZE];

	active_data_plan_index = UI_GetActiveDataPlanNumber();

	sprintf(active_data_plan_index_str, "%d", active_data_plan_index);

	return aml_encode_message_json("RspCurrentActiveDataPlanIndex", 1,
		"PlanIndex", active_data_plan_index_str, JSMN_PRIMITIVE);
}

MVStatus aml_remove_current_tod_handler()
{
	UI_ResetTimeOfDayTable();

	return aml_encode_message_json("RspRemoveCurrentToD", 0);
}

MVStatus aml_is_tod_removed_by_user_handler()
{
	MVBool is_removed;
	char is_removed_str[BIT_SIZE];

	is_removed = UI_IsToDRemovedByUser();

	if (is_removed == MV_TRUE)
		sprintf(is_removed_str, "%s", "T");
	else
		sprintf(is_removed_str, "%s", "F");


	return aml_encode_message_json("RspIsToDRemovedByUser", 1,
		"IsRemoved", is_removed_str, JSMN_PRIMITIVE);

}

MVStatus aml_req_data_plan_triggering_status_handler()
{
	MVBool	dp_trig_status;
	char	dp_trig_status_str[BIT_SIZE];

	dp_trig_status = UI_GetDatasetTriggering();

	if (dp_trig_status == MV_TRUE)
		sprintf(dp_trig_status_str, "%s", "T");
	else
		sprintf(dp_trig_status_str, "%s", "F");


	return aml_encode_message_json("RspDataPlanTriggeringStatus", 1,
		"IsEnabled", dp_trig_status_str, JSMN_PRIMITIVE);
}

MVStatus aml_req_data_plan_triggering_update_handler(MVBool updated_status)
{
	UI_SetDatasetTriggering(updated_status);

	return aml_encode_message_json("RspDataPlanTriggeringUpdate", 0);
}

MVStatus aml_retrieve_current_dataset_handler()
{	
	MVStatus is_successful = MV_FAILURE;
	MVInt file_size = 0;
	MVInt8 controller_id = 0;


	char dataset_file_name[FILENAME_MAX] = { 0 };
	char is_successful_str[BIT_SIZE];
	char file_size_str[FILE_SIZE_STR];
	char controller_id_str[INT_STR_SIZE];
	

	/*
	NOTE: After discussing this with AK and MC, we decided that we will not update the stored XML file
	when retrieving it. The update was used to reflect any change that has been done by the SF module.
	
	is_successful = UI_UpdateDatasetFile_XDS();
			
	if(is_successful == MV_SUCCESS)
	*/
	
	is_successful = UI_GetDatasetFileInfo_XDS(dataset_file_name, &file_size, &controller_id);
	
	if (is_successful == MV_SUCCESS)
		sprintf(is_successful_str, "%s", "T");
	else
		sprintf(is_successful_str, "%s", "F");

	sprintf(file_size_str, "%d", file_size);
	sprintf(controller_id_str, "%d", controller_id);

	
	return aml_encode_message_json("RspRetrieveCurrentDataset", 4,
									"IsSuccessful", is_successful_str, JSMN_PRIMITIVE,
									"FileName", dataset_file_name, JSMN_STRING,
									"FileSize", file_size_str, JSMN_PRIMITIVE,
									"ControllerID", controller_id_str, JSMN_PRIMITIVE);
}


MVStatus aml_send_dataset_file_handler()
{
	char dataset_file_path_and_name[FILENAME_MAX] = { 0 };
	MVInt file_size = 0;

	UI_GetLatestDatasetFileInfo_XDS(dataset_file_path_and_name, &file_size);
	
	aml_dataset_set_file_name(dataset_file_path_and_name);

	return aml_file_read_and_send(dataset_file_path_and_name, file_size);

}

MVStatus aml_req_mova_enabled_flag_setting_handler(MVBool value)
{
	MVInt8 failure_reason = 0;
	MVBool is_ok = MV_TRUE;

	char failure_reason_str[LIMITED_INT_SIZE];
	char is_ok_str[BIT_SIZE];

	is_ok = UI_SetOpFlag(MOVA_ON, value, &failure_reason);

	if (is_ok == MV_TRUE)
		sprintf(is_ok_str, "%s", "T");
	else
		sprintf(is_ok_str, "%s", "F");


	sprintf(failure_reason_str, "%d", failure_reason);

	return aml_encode_message_json("RspMOVAEnabledFlagSetting", 2,
									"IsOk", is_ok_str, JSMN_PRIMITIVE,
									"FailureReason", failure_reason_str, JSMN_PRIMITIVE);
}

MVStatus aml_req_on_control_flag_setting_handler(MVBool value)
{
	MVInt8 failure_reason = 0;
	MVBool is_ok = MV_TRUE;

	char failure_reason_str[LIMITED_INT_SIZE];
	char is_ok_str[BIT_SIZE];

	is_ok = UI_SetOpFlag(ON_CONTROL, value, &failure_reason);

	if (is_ok == MV_TRUE)
		sprintf(is_ok_str, "%s", "T");
	else
		sprintf(is_ok_str, "%s", "F");
	
	sprintf(failure_reason_str, "%d", failure_reason);

	return aml_encode_message_json("RspOnControlFlagSetting", 2,
									"IsOk", is_ok_str, JSMN_PRIMITIVE,
									"FailureReason", failure_reason_str, JSMN_PRIMITIVE);
}

MVStatus aml_req_reset_error_count_handler()
{
	UI_ResetErrorCount();

	return aml_encode_message_json("RspResetErrorCount", 0);
}

MVStatus aml_req_force_stage_handler(MVInt stage_num)
{
	MVInt8 failure_reason = 0;
	MVBool is_ok = MV_TRUE;

	char failure_reason_str[LIMITED_INT_SIZE];
	char is_ok_str[BIT_SIZE];

	CHECK_RANGE(stage_num, 1, MAX_STAGES);

	is_ok = UI_ForceStage(stage_num, &failure_reason);

	if (is_ok == MV_TRUE)
		sprintf(is_ok_str, "%s", "T");
	else
		sprintf(is_ok_str, "%s", "F");

	sprintf(failure_reason_str, "%d", failure_reason);

	return aml_encode_message_json("RspForceStage", 2,
									"IsOk", is_ok_str, JSMN_PRIMITIVE,
									"FailureReason", failure_reason_str, JSMN_PRIMITIVE);
}

MVStatus aml_req_cancel_forced_stage_handler()
{
	UI_CancelForcedStage();

	return aml_encode_message_json("RspCancelForcedStage", 0);
}

MVStatus aml_req_check_connected_to_right_controller_handler(MVInt8 controller_id)
{
	MVBool is_ok = MV_TRUE;
	MVInt8 running_controller_id;
	char is_ok_str[BIT_SIZE];


	CHECK_RANGE(controller_id, 1, 128);

	running_controller_id = UI_GetRunningControllerID_XDS();


	is_ok = (running_controller_id == controller_id);

	if (is_ok == MV_TRUE)
		sprintf(is_ok_str, "%s", "T");
	else
		sprintf(is_ok_str, "%s", "F");

	return aml_encode_message_json("RspCheckConnectedToRightController", 1,
									"IsOk", is_ok_str, JSMN_PRIMITIVE);
}

MVStatus aml_req_site_full_data_handler()
{
	UI_GetJsonStringOfDataset(full_site_msg_buffer);

	aml_send_message_to_user((char*)full_site_msg_buffer);

	return MV_SUCCESS;
}


MVStatus aml_req_lanes_count_handler()
{
	MVUInt8 lanes_count;
	char	lanes_count_str[LIMITED_INT_SIZE];


	lanes_count = UI_GetLanesCount();

	sprintf(lanes_count_str, "%d", lanes_count);


	return aml_encode_message_json("RspLanesCount", 1,
									"Count", lanes_count_str, JSMN_PRIMITIVE);
}


MVStatus aml_req_all_tma_logs_link_handler()
{
	char	tma_file_link[TMA_FILE_LINK_SIZE];
	MVInt	file_size;
	
	file_size = aml_prepare_tma_log_file(MV_TRUE);

	if (file_size == -1)
		return MV_FAILURE;
	
	aml_get_tma_log_file_uri(tma_file_link);

	if (tma_file_link == 0x00)
		return MV_FAILURE;
	
	return aml_encode_message_json("RspAllTMA_LogsLink", 1,
									"Link", tma_file_link, JSMN_STRING);
}


MVStatus aml_req_all_tma_logs_file_handler()
{
	MVInt	file_size;
	char	file_size_str[FILE_SIZE_STR];

	file_size = aml_prepare_tma_log_file(MV_FALSE);

	if (file_size == -1)
		return MV_FAILURE;

	sprintf(file_size_str, "%d", file_size);

	return aml_encode_message_json("RspAllTMA_LogsFile", 1,
									"FileSize", file_size_str, JSMN_PRIMITIVE);
}

MVStatus aml_req_send_tma_logs_file_handler()
{
	char	tma_file_path_and_location[TMA_FILE_LINK_SIZE];
	MVInt	file_size;

	file_size = aml_get_tma_file_size();

	aml_get_tma_log_file_path_and_name(tma_file_path_and_location);

	return aml_file_read_and_send(tma_file_path_and_location, file_size);
}

static MVInt aml_prepare_tma_log_file(MVBool is_prep_to_web_downloading)
{
	if (aml_init_tma_logs_file(is_prep_to_web_downloading) == MV_FAILURE)
		return -1;

	/*1. Flow Log*/
	aml_write_tma_log_data("{LogType: FlowLog\n");
	print_FlowLog(pTmaData, WRITE_TO_TMA_LOG_FILE, UI_GetLanesCount());
	aml_write_tma_log_data("}\n");

	/*2. Occupancy Log*/
	aml_write_tma_log_data("{LogType: OccupancyLog\n");
	print_OccupancyLog(pTmaData, WRITE_TO_TMA_LOG_FILE, UI_GetStagesCount());
	aml_write_tma_log_data("}\n");

	/*3.*/
	aml_write_tma_log_data("{LogType: StageCountAndLengthLog\n");
	print_TraSevLog(pTmaData, WRITE_TO_TMA_LOG_FILE, UI_GetStagesCount());
	aml_write_tma_log_data("}\n");

	/*4. Peds logs*/
	aml_write_tma_log_data("{LogType: PedestrianLevelOfServiceLog\n");
	print_PedSevLog(pTmaData, WRITE_TO_TMA_LOG_FILE, xgnREDPED_MAX, UI_GetRedPedPtr());
	aml_write_tma_log_data("}\n");

	/*5. End sat logs*/
	aml_write_tma_log_data("{LogType: EndSatLog\n");
	print_EndSatLog(pTmaData, WRITE_TO_TMA_LOG_FILE, UI_GetStagesCount());
	aml_write_tma_log_data("}\n");

	/*6. Oversat logs*/
	aml_write_tma_log_data("{LogType: OversatLog\n");
	print_OversatLog(pTmaData, WRITE_TO_TMA_LOG_FILE, UI_GetLanesCount());
	aml_write_tma_log_data("}\n");

	/*7. Faulty dets*/
	aml_write_tma_log_data("{LogType: FaultyDetsLog\n");
	print_FaultyDetecLog(pTmaData, WRITE_TO_TMA_LOG_FILE);
	aml_write_tma_log_data("}\n");

	aml_close_tma_log_file();

	return aml_get_tma_file_size();
}



