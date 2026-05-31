/*******************************************************************************
*
*                      MOVA KERNEL
*
*  FILE:         aml_messages_handler.h
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

#if !defined (AML_MESSAGES_HANDLER_H)
#define AML_MESSAGES_HANDLER_H

#include "MVTypes.h"

MVStatus aml_req_mova_time_handler();
MVStatus aml_req_force_bits_handler();
MVStatus aml_req_confirm_bits_handler();
MVStatus aml_req_operation_flags_handler();
MVStatus aml_req_kernel_version_handler();
MVStatus aml_req_raw_dets_status_handler();
MVStatus aml_req_dets_status_handler();
MVStatus aml_req_dets_sus_status_handler();
MVStatus aml_req_lane_data_handler(MVInt8 lane_id);
MVStatus aml_req_link_data_handler(MVInt8 link_id);
MVStatus aml_req_error_count_handler();
MVStatus aml_req_reset_error_count_handler();
MVStatus aml_req_error_detail_handler(MVInt error_index);
MVStatus aml_req_clear_error_log_handler();
MVStatus aml_req_sat_flow_period_handler(MVInt period_index);
MVStatus aml_req_sat_flow_details_handler(MVInt lane_id, MVInt period_index);
MVStatus aml_req_stlost_details_handler(MVInt lane_id, MVInt period_index);
MVStatus aml_req_avg_flow_handler(MVInt day_id, MVInt lane_index, MVInt time_index);
MVStatus aml_req_mova_time_setting_handler(char * date_time_str);
MVStatus aml_req_alert_flags_handler();
MVStatus aml_req_alert_status_handler();
MVStatus aml_req_alert_flags_setting_handler(MVBool blocking_status, MVBool oversat_status, MVBool occupancy_status);
MVStatus aml_req_dataset_transfer_handler(char * file_name, int file_size);
MVStatus aml_req_check_transfered_file_integrity_handler(MVUInt32 file_crc32);
MVStatus aml_req_check_dataset_compatibility_handler(MVUInt8 stages_count, MVUInt8 links_count, MVUInt8 lanes_count);
MVStatus aml_req_load_dataset_into_memeory_handler(MVInt8 dp1_index, MVInt8 dp2_index, MVInt8 dp3_index, MVInt8 dp4_index);
MVStatus aml_req_activate_data_plan_handler(MVInt8 data_plan_index);
MVStatus aml_req_current_active_plan_index_handler();
MVStatus aml_remove_current_tod_handler();
MVStatus aml_is_tod_removed_by_user_handler();
MVStatus aml_req_data_plan_triggering_status_handler();
MVStatus aml_req_data_plan_triggering_update_handler(MVBool updated_status);
MVStatus aml_retrieve_current_dataset_handler();
MVStatus aml_send_dataset_file_handler();
MVStatus aml_req_mova_enabled_flag_setting_handler(MVBool value);
MVStatus aml_req_on_control_flag_setting_handler(MVBool value);
MVStatus aml_req_reset_error_count_handler();
MVStatus aml_req_force_stage_handler(MVInt stage_num);
MVStatus aml_req_cancel_forced_stage_handler();
MVStatus aml_req_check_connected_to_right_controller_handler(MVInt8 controller_id);
MVStatus aml_req_site_full_data_handler();
MVStatus aml_req_lanes_count_handler();
MVStatus aml_req_all_tma_logs_link_handler();
MVStatus aml_req_all_tma_logs_file_handler();
MVStatus aml_req_send_tma_logs_file_handler();
MVStatus aml_req_message_by_index_handler(MVInt msg_index);
MVInt	 aml_req_last_message_index_handler();
MVStatus aml_req_output_channel_status_handler();

#endif /*AML_MESSAGES_HANDLER_H*/