/*******************************************************************************
*
*                      MOVA KERNEL
*
*  FILE:         aml_messages_decoder_encoder_handler.c
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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>     /* atoi */

#include "jsmn-master/jsmn.h"
#include "aml_messages_decoder_encoder.h"
#include "aml_messages_handler.h"
#include "aml_handler.h"


#define MAX_IN_PARAM_SIZE 100
#define MAX_IN_PARAM_VALUE_SIZE 110

#define AML_MSG_TO_SEND_MAX_SIZE 2048


static char		msg_buffer[AML_MSG_TO_SEND_MAX_SIZE];

static char   param_str[1024];
static char   param_value_formated[1024];


static MVBool	jsoneq(const char *json, jsmntok_t *tok, const char *s);
static void		modify_value_based_on_type(char * value, char * value_formated, jsmntype_t value_type);
static void		format_array(char * value, char * value_formated, jsmntype_t value_type);

static MVInt read_req_msg_param_INT(char * full_msg, jsmntok_t * t, MVInt children_start, MVInt children_end, char * param_name_str_in);
static void read_req_msg_param_STR(char * full_msg, jsmntok_t * t, MVInt children_start, MVInt children_end, char * param_name_str_in, char * param_value_out);
static unsigned int read_req_msg_param_UINT(char * full_msg, jsmntok_t * t, MVInt children_start, MVInt children_end, char * param_name_str_in);



MVStatus aml_decode_message_json(char * msg)
{
	MVInt i;
	MVInt r;

	// SLM FIXME Need to review these values, sometimes used as is but usually need converting
	MVInt8 id;
	MVInt day_id;
	MVInt index;
	MVInt file_size;

	MVInt8 controller_id;

	MVInt8	dp1_index;
	MVInt8	dp2_index;
	MVInt8	dp3_index;
	MVInt8	dp4_index;
	
	MVUInt32 file_crc32;
	
	MVBool blocking_status;
	MVBool occupancy_status;
	MVBool oversat_status;

	MVBool status;

	MVUInt8 stages_count, links_count, lanes_count;

	char * date_time[MAX_IN_PARAM_SIZE];

	char   file_name[MAX_IN_PARAM_VALUE_SIZE];

	jsmn_parser p;
	jsmntok_t t[20]; /* We expect no more than 20 tokens */
	jsmntok_t *pt = t;

	char msg_type[MAX_IN_PARAM_SIZE * 2];


	jsmn_init(&p);

	r = jsmn_parse(&p, msg, strlen(msg), t, sizeof(t) / sizeof(t[0]));

	if (r < 0)
	{
		/*"Failed to parse the JSON message.*/
		return MV_FAILURE;
	}

	/* Assume the top-level element is an object */
	if (r < 1 || t[0].type != JSMN_OBJECT) 
	{
		/*Object expected*/
		return MV_FAILURE;
	}
	

	/* Loop over all keys of the root object */
	for (i = 1; i < r; i++)
	{
		if (jsoneq(msg, &t[i], "MessageType") == MV_TRUE)
		{
			snprintf(msg_type, t[i+1].end - t[i+1].start + 1, "%s", msg + t[i+1].start);

			if (strcmp(msg_type, "ReqMOVATime") == 0)
			{
				return aml_req_mova_time_handler();
			}
			else if (strcmp(msg_type, "ReqForceBits") == 0)
			{
				return aml_req_force_bits_handler();
			}
			else if (strcmp(msg_type, "ReqOutputChannelStatus") == 0)
			{
				return aml_req_output_channel_status_handler();
			}
			else if (strcmp(msg_type, "ReqConfirmBits") == 0)
			{
				return aml_req_confirm_bits_handler();
			}
			else if (strcmp(msg_type, "ReqOperationFlags") == 0)
			{
				return aml_req_operation_flags_handler();
			}
			else if (strcmp(msg_type, "ReqKernelVersion") == 0)
			{
				return aml_req_kernel_version_handler();
			}
			else if (strcmp(msg_type, "ReqRawDetectorsStatus") == 0)
			{
				return aml_req_raw_dets_status_handler();
			}
			else if (strcmp(msg_type, "ReqDetectorsStatus") == 0)
			{
				return aml_req_dets_status_handler();
			}
			else if (strcmp(msg_type, "ReqDetectorsSusStatus") == 0)
			{
				return aml_req_dets_sus_status_handler();
			}
			else if (strcmp(msg_type, "ReqLaneData") == 0)
			{
				id = (MVInt8)read_req_msg_param_INT(msg, pt, i, r, "ID");

				if (id != -1)
					return aml_req_lane_data_handler(id);
			}
			else if (strcmp(msg_type, "ReqLinkData") == 0)
			{
				id = (MVInt8)read_req_msg_param_INT(msg, pt, i, r, "ID");

				if (id != -1)
					return aml_req_link_data_handler(id);
			}
			else if (strcmp(msg_type, "ReqLastMessageIndex") == 0)
			{
				return aml_req_last_message_index_handler();
			}
			else if (strcmp(msg_type, "ReqMessageByIndex") == 0)
			{
				index = read_req_msg_param_INT(msg, pt, i, r, "Index");

				if (index >= 0)
					return aml_req_message_by_index_handler(index);
			}
			else if (strcmp(msg_type, "ReqErrorCount") == 0)
			{
				return aml_req_error_count_handler();
			}
			else if (strcmp(msg_type, "ReqErrorDetail") == 0)
			{
				index = read_req_msg_param_INT(msg, pt, i, r, "ErrorIndex");

				if (index != -1)
					return aml_req_error_detail_handler(index);
			}
			else if (strcmp(msg_type, "ReqClearErrorLog") == 0)
			{
				return aml_req_clear_error_log_handler();
			}
			else if (strcmp(msg_type, "ReqSatFlowPeriod") == 0)
			{
				index = read_req_msg_param_INT(msg, pt, i, r, "PeriodIndex");

				if (index != -1)
					return aml_req_sat_flow_period_handler(index);
			}
			else if (strcmp(msg_type, "ReqSatFlowDetails") == 0)
			{
				id = (MVInt8)read_req_msg_param_INT(msg, pt, i, r, "LaneID");
				index = read_req_msg_param_INT(msg, pt, i, r, "PeriodIndex");

				if (id != -1 && index != -1)
					return aml_req_sat_flow_details_handler(id - 1, index);
			}
			else if (strcmp(msg_type, "ReqSTLOST_Details") == 0)
			{
				id = (MVInt8)read_req_msg_param_INT(msg, pt, i, r, "LaneID");
				index = read_req_msg_param_INT(msg, pt, i, r, "PeriodIndex");

				if (id != -1 && index != -1)
					return aml_req_stlost_details_handler(id - 1, index);
			}
			else if (strcmp(msg_type, "ReqAvgFlow") == 0)
			{
				day_id = read_req_msg_param_INT(msg, pt, i, r, "DayID");
				id = (MVInt8)read_req_msg_param_INT(msg, pt, i, r, "LaneID");
				index = read_req_msg_param_INT(msg, pt, i, r, "TimeIndex");

				if (id != -1 && day_id != -1 && index != -1)
					return aml_req_avg_flow_handler(day_id, id, index);
			}

			else if (strcmp(msg_type, "ReqMOVATimeSetting") == 0)
			{
				read_req_msg_param_STR(msg, pt, i, r, "DateTime", (char *)date_time);
			
				if (strlen((char *)date_time) == 0) return MV_FAILURE;

				return aml_req_mova_time_setting_handler((char *)date_time);
			}

			else if (strcmp(msg_type, "ReqAlertMonitoringFlags") == 0)
			{
				return aml_req_alert_flags_handler();
			}

			else if (strcmp(msg_type, "ReqAlertStatus") == 0)
			{
				return aml_req_alert_status_handler();
			}
			
			else if (strcmp(msg_type, "ReqAlertMonitoringFlagsSetting") == 0)
			{
				blocking_status = (MVBool)read_req_msg_param_INT(msg, pt, i, r, "BlockingStatus");
				oversat_status = (MVBool)read_req_msg_param_INT(msg, pt, i, r, "OversatStatus");
				occupancy_status = (MVBool)read_req_msg_param_INT(msg, pt, i, r, "OccupancyStatus");

				return aml_req_alert_flags_setting_handler(blocking_status, oversat_status, occupancy_status);
			}
			else if (strcmp(msg_type, "ReqCheckDatasetCompatibility") == 0)
			{
				stages_count = (MVUInt8)read_req_msg_param_UINT(msg, pt, i, r, "StagesCount");
				links_count = (MVUInt8)read_req_msg_param_UINT(msg, pt, i, r, "LinksCount");
				lanes_count = (MVUInt8)read_req_msg_param_UINT(msg, pt, i, r, "LanesCount");
				
				return aml_req_check_dataset_compatibility_handler(stages_count, links_count, lanes_count);
			}
			else if (strcmp(msg_type, "ReqDatasetTransfer") == 0)
			{
				read_req_msg_param_STR(msg, pt, i, r, "FileName", file_name);
				if (strlen(file_name) == 0) return MV_FAILURE;

				file_size = read_req_msg_param_INT(msg, pt, i, r, "FileSize");

				return aml_req_dataset_transfer_handler(file_name, file_size);
			}
			else if (strcmp(msg_type, "ReqCheckTransferedFileIntegrity") == 0)
			{
				file_crc32 = read_req_msg_param_UINT(msg, pt, i, r, "FileCRC32");

				return aml_req_check_transfered_file_integrity_handler(file_crc32);
			}
			else if (strcmp(msg_type, "ReqLoadDatasetIntoMemory") == 0)
			{
				/* No need for it now: controller_id = (MVInt8)read_req_msg_param_INT(msg, &t, i, r, "ControllerID");*/

				dp1_index = (MVInt8)read_req_msg_param_INT(msg, pt, i, r, "DP1_Index");
				dp2_index = (MVInt8)read_req_msg_param_INT(msg, pt, i, r, "DP2_Index");
				dp3_index = (MVInt8)read_req_msg_param_INT(msg, pt, i, r, "DP3_Index");
				dp4_index = (MVInt8)read_req_msg_param_INT(msg, pt, i, r, "DP4_Index");
				
				return aml_req_load_dataset_into_memeory_handler(dp1_index, dp2_index, dp3_index, dp4_index);
			}
			else if (strcmp(msg_type, "ReqActivateDataPlan") == 0)
			{
				index = read_req_msg_param_INT(msg, pt, i, r, "DataPlanIndex");

				return aml_req_activate_data_plan_handler((MVInt8)index);
			}
			else if (strcmp(msg_type, "ReqCurrentActiveDataPlanIndex") == 0)
			{
				return aml_req_current_active_plan_index_handler();					
			}
			else if (strcmp(msg_type, "ReqRemoveCurrentToD") == 0)
			{
				return aml_remove_current_tod_handler();
			}
			else if (strcmp(msg_type, "ReqIsToDRemovedByUser") == 0)
			{
				return aml_is_tod_removed_by_user_handler();
			}
			else if (strcmp(msg_type, "ReqDataPlanTriggeringStatus") == 0)
			{
				return aml_req_data_plan_triggering_status_handler();
			}
			else if (strcmp(msg_type, "ReqDataPlanTriggeringUpdate") == 0)
			{
				status = (MVBool)read_req_msg_param_INT(msg, pt, i, r, "UpdatedStatus");

				return aml_req_data_plan_triggering_update_handler(status);
			}
			else if (strcmp(msg_type, "ReqRetrieveCurrentDataset") == 0)
			{
				return aml_retrieve_current_dataset_handler();
			}
			else if (strcmp(msg_type, "ReqSendDatasetFile") == 0)
			{
				return aml_send_dataset_file_handler();
			}
			else if (strcmp(msg_type, "ReqMOVAEnabledFlagSetting") == 0)
			{
				status = (MVBool)read_req_msg_param_INT(msg, pt, i, r, "Value");

				return aml_req_mova_enabled_flag_setting_handler(status);
			}
			else if (strcmp(msg_type, "ReqOnControlFlagSetting") == 0)
			{
				status = (MVBool)read_req_msg_param_INT(msg, pt, i, r, "Value");

				return aml_req_on_control_flag_setting_handler(status);
			}
			else if (strcmp(msg_type, "ReqResetErrorCount") == 0)
			{
				return aml_req_reset_error_count_handler();
			}
			else if (strcmp(msg_type, "ReqForceStage") == 0)
			{
				id = (MVInt8)read_req_msg_param_INT(msg, pt, i, r, "StageNum");

				return aml_req_force_stage_handler(id);
			}
			else if (strcmp(msg_type, "ReqCancelForcedStage") == 0)
			{
				return aml_req_cancel_forced_stage_handler();
			}
			else if (strcmp(msg_type, "ReqCheckConnectedToRightController") == 0)
			{
				controller_id = (MVInt8)read_req_msg_param_INT(msg, pt, i, r, "ControllerID");

				return aml_req_check_connected_to_right_controller_handler(controller_id);
			}
			else if (strcmp(msg_type, "ReqSiteFullData") == 0)
			{
				return aml_req_site_full_data_handler();
			}
			else if (strcmp(msg_type, "ReqLanesCount") == 0)
			{
				return aml_req_lanes_count_handler();
			}
			else if (strcmp(msg_type, "ReqAllTMA_LogsLink") == 0)
			{
				return aml_req_all_tma_logs_link_handler();
			}
			else if (strcmp(msg_type, "ReqAllTMA_LogsFile") == 0)
			{
				return aml_req_all_tma_logs_file_handler();
			}
			else if (strcmp(msg_type, "ReqSendTMA_LogsFile") == 0)
			{
				return aml_req_send_tma_logs_file_handler();
			}

		}
	}

	return MV_FAILURE;

}


static MVInt read_req_msg_param_INT(char * full_msg, jsmntok_t * t, MVInt children_start, MVInt children_end, char * param_name_str_in)
{
	MVInt j;

	char param_name_str[MAX_IN_PARAM_SIZE];
	char param_value_str[MAX_IN_PARAM_SIZE];

	/*Loop over all the children to read the params*/
	for (j = children_start + 1; j < children_end; j++)
	{
		if (t[j].end - t[j].start + 1 >= MAX_IN_PARAM_SIZE) continue;

		snprintf(param_name_str, t[j].end - t[j].start + 1, "%s", full_msg + t[j].start);

		if (strcmp(param_name_str, param_name_str_in) == 0)
		{
			if (t[j + 1].end - t[j + 1].start >= MAX_IN_PARAM_SIZE) continue;

			snprintf(param_value_str, t[j + 1].end - t[j + 1].start + 1, "%s", full_msg + t[j + 1].start);

			/*Handling the boolean param firsts*/
			if (strcmp(param_value_str, "true") == 0)
				return MV_TRUE;

			if (strcmp(param_value_str, "false") == 0)
				return MV_FALSE;
						
			return (MVInt)atoi(param_value_str);
		}
	}

	return -1;
}

static unsigned int read_req_msg_param_UINT(char * full_msg, jsmntok_t * t, MVInt children_start, MVInt children_end, char * param_name_str_in)
{
	MVInt j;

	char * ptr;
	char param_name_str[MAX_IN_PARAM_SIZE];
	char param_value_str[MAX_IN_PARAM_SIZE];

	/*Loop over all the children to read the params*/
	for (j = children_start + 1; j < children_end; j++)
	{
		if (t[j].end - t[j].start + 1 >= MAX_IN_PARAM_SIZE) continue;

		snprintf(param_name_str, t[j].end - t[j].start + 1, "%s", full_msg + t[j].start);

		if (strcmp(param_name_str, param_name_str_in) == 0)
		{
			if (t[j + 1].end - t[j + 1].start >= MAX_IN_PARAM_SIZE) continue;

			snprintf(param_value_str, t[j + 1].end - t[j + 1].start + 1, "%s", full_msg + t[j + 1].start);
			
			return strtoul(param_value_str, &ptr, 10);
		}
	}

	return ((unsigned int)(-1));
}

static void read_req_msg_param_STR(char * full_msg, jsmntok_t * t, MVInt children_start, MVInt children_end, char * param_name_str_in, char * param_value_out)
{
	MVInt j;

	char param_name_str[MAX_IN_PARAM_SIZE];

	/*Loop over all the children to read the params*/
	for (j = children_start + 1; j < children_end; j++)
	{
		if (t[j].end - t[j].start + 1 >= MAX_IN_PARAM_SIZE) continue;

		snprintf(param_name_str, t[j].end - t[j].start + 1, "%s", full_msg + t[j].start);

		if (strcmp(param_name_str, param_name_str_in) == 0)
		{
			if (t[j + 1].end - t[j + 1].start >= MAX_IN_PARAM_VALUE_SIZE) continue;

			snprintf(param_value_out, t[j + 1].end - t[j + 1].start + 1, "%s", full_msg + t[j + 1].start);
			return;
		}
	}

	param_value_out[0] = 0;
}

MVStatus aml_encode_message_json(char * msg_type, int param_count, ...)
{
	va_list valist;
	int i;

	char *		param_name;
	char *		param_value;
	jsmntype_t  param_type;
	
	va_start(valist, param_count);


	sprintf(msg_buffer, "{\"MessageType\": \"%s\",", msg_type);

	if (param_count > 0)
	{
		strcat(msg_buffer, "\"Params\":{");

		for (i = 0; i < param_count * 3; i += 3)
		{
			param_name = va_arg(valist, char*);
			param_value = va_arg(valist, char*);
			param_type = va_arg(valist, jsmntype_t);

			modify_value_based_on_type(param_value, param_value_formated, param_type);

			sprintf(param_str, "\"%s\":%s", param_name, param_value_formated);

			/*Check if this is not the last param*/
			if(i+3 < param_count*3)
				strcat(param_str, ",");

			strcat(msg_buffer, param_str);
		}

		strcat(msg_buffer, "}");
	}

	strcat(msg_buffer, "}");

	aml_send_message_to_user((char*)msg_buffer);

	/* clean memory reserved for valist */
	va_end(valist);

	return MV_SUCCESS;
}


static MVBool jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
	if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
		strncmp(json + tok->start, s, tok->end - tok->start) == 0) 
	{
		return MV_TRUE;
	}

	return MV_FALSE;
}


static void modify_value_based_on_type(char * value, char * value_formated, jsmntype_t value_type)
{
	/*Check if Ints, Floats, Boolean...*/
	if (value_type == JSMN_PRIMITIVE)
	{
		/* Bool - True */
		if (strcmp(value, "T") == 0)
		{
			sprintf(value_formated, "%s", "true");
			return;
		}

		/* Bool - True */
		if (strcmp(value, "F") == 0)
		{
			sprintf(value_formated, "%s", "false");
			return;
		}

		sprintf(value_formated, value);
		return; /*Ints and Floats don't require any wrapping*/
	}
		
	
	/*String (include Time and Date)*/
	if (value_type == JSMN_STRING)
	{
		sprintf(value_formated, "\"%s\"", value);
		return;
	}

	/* 1D Array */
	if (value_type == JSMN_ARRAY + JSMN_PRIMITIVE)
	{
		format_array(value, value_formated, JSMN_PRIMITIVE);
		return;
	}
	if (value_type == JSMN_ARRAY + JSMN_STRING)
	{
		format_array(value, value_formated, JSMN_STRING);
		return;
	}
}


static void format_array(char * value, char * value_formated, jsmntype_t value_type)
{
	char element_formated[20];

	char *token = strtok(value, ",");

	sprintf(value_formated, "[");

	while (token) 
	{
		modify_value_based_on_type(token, element_formated, value_type);

		strcat(value_formated, element_formated);

		token = strtok(NULL, ",");

		if(token != NULL)
			strcat(value_formated, ",");
	}

	strcat(value_formated, "]");
}