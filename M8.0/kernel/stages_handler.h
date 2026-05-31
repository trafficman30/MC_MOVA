/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         stages_handler.h
*
*  TITLE:        Mova Kernel: Stages Handler
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	24-Oct-2013		7.5.0		AA		IA		......			Newly created
*
*  (c) Copyright TRL Ltd 2013. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/

#if !defined (STAGES_HANDLER_H)
#define STAGES_HANDLER_H

void init_stages_handler();
void init_stages_dynamic_data();

void update_stages_oversat_markers(uint8 linkIdx, uint8 laneIdx, int32 tempFactor);
void set_stages_bonus_time_and_link();
void update_skipped_stages_data_at_endstage();
void update_stage_lambdas(uint8 stageIdx);
void calc_stages_drift_max();
int32 calc_stages_extra_green_for_drift_max(int32 total_green);
int32 update_stage_demand(uint8 stageIdx);
uint8 decide_next_stage();
mv_bool update_next_stage_endsat_marker_part1(mv_bool * isVarMinGreenEXpired);
mv_bool update_next_stage_endsat_marker_part2();


/*Helper functions*/
mv_bool check_green_in_following_stages(uint8 nLinkIdx, uint8 nNextStageIdx);

/* Getters (with work)*/
int32 calc_stage_revised_max_green_time(int32 oversatMax, int32 unsatMax, uint8 stageNum, mv_bool isForPedStage);

/* Getters */
int32 get_stage_drift_max(uint8 stageIdx);
int32	get_stage_lambda(uint8 stageIdx);
int32 get_stage_low_drift_max(uint8 stageIdx);
int8	get_stage_endsat_marker(uint8 stageIdx);
int8 get_stage_demand_marker(uint8 stageIdx);
int8 get_stage_emergency_demand_marker(uint8 stageIdx);
int8 get_stage_priority_demand_marker(uint8 stageIdx);
int32 * get_stages_low_drift_max();
uint8 * get_stages_bonus_link_num();

#if defined (TRL_HOST_DEBUG)
int32 * get_stages_bonus_time();
#endif

/*Reseters*/
void reset_stages_oversat_max_marker();
void reset_stages_endsat_marker();
void reset_previous_stage_demand_markers();


#endif /*STAGES_EP_HANDLER_H*/