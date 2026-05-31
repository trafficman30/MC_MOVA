/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         core_interface.h
*
*  TITLE:        Mova Kernel: Core Interface
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

#if !defined (CORE_INTERFACE_H)
#define CORE_INTERFACE_H

#include "mova_types.h"
#include "dynamic_data.h"

EXT_VISIBLE void (*p_divide_error)(int, int, ...);

typedef struct Program		ProgramStruct;
typedef struct Junction		JunctionStruct;
typedef struct Stages		StagesStruct;
typedef struct Links		LinksStruct;
typedef struct Lanes		LanesStruct;
typedef struct Detectors	DetectorsStruct;
typedef struct Messages		MessagesStruct;
typedef struct Timers		TimersStruct;

void CI_init_core_module();

void CI_init_core_dynmaic_data();

/* Each scan functions*/
void CI_set_each_scan_input_data(int currentStage,
									int * linksGreenFlag,						
									int8 * detsOnFlag,
									int * detsPreviousGap,
									int * detsTimeOn,
									int * detsTimeOff,
									int * vehsCount);

mv_bool CI_start_scan();

/* Getters */
int32 CI_get_warmup_counter();
uint8 CI_get_current_stage();
uint8 CI_get_last_stage();
uint8 CI_get_next_stage();
uint8 CI_get_demanded_stage();
int8 CI_get_program_marker();
int8 CI_get_previous_marker();
int8 CI_get_ped_demand_marker();

int32  CI_get_msg_data(uint8 fieldIdx, uint8 msgIdx);
uint8  CI_get_current_msg_num();
MessagesStruct * CI_get_msgs();
int32 CI_get_lane_average_flow(uint8 dayIdx, uint8 periodIdx, uint8 laneIdx);

ProgramStruct *	 const CI_get_program_data_ptr();
JunctionStruct *  const CI_get_junction_data_ptr();
StagesStruct *	 const CI_get_stages_data_ptr();
LinksStruct *	 const CI_get_links_data_ptr();
LanesStruct *	 const CI_get_lanes_data_ptr();
DetectorsStruct * const CI_get_dets_data_ptr();
MessagesStruct *	 const CI_get_msgs_data_ptr();
TimersStruct *	 const CI_get_timers_data_ptr();

uint32 CI_get_program_data_size();
uint32 CI_get_junction_data_size();
uint32 CI_get_stages_data_size();
uint32 CI_get_links_data_size();
uint32 CI_get_lanes_data_size();
uint32 CI_get_dets_data_size();
uint32 CI_get_msgs_data_size();
uint32 CI_get_timers_data_size();
int8  CI_get_detector_suspicion_status(uint8 detIdx);

int32 CI_get_dynamic_data_checksum();

#ifdef M8_IMPROVED_LINKING
mv_bool CI_is_to_send_backward_hold_signal();
mv_bool CI_is_to_send_backward_release_signal();
#endif

/*end of testing area*/

/* Setters */
void CI_set_warmup_counter(int32 val);
void CI_set_current_stage(uint8 val);
void CI_set_last_stage(uint8 val);
void CI_set_next_stage(uint8 val);
void CI_set_demanded_stage(uint8 val);
void CI_set_previous_marker(int8 val);
void CI_set_msg_data(uint8 fieldIdx, uint8 msgIdx, int32 data);

#ifdef M8_DET_FAULT_CHECKING_AND_LOGGING
void CI_set_detector_suspicion_status(uint8 detIdx, int8 status);
#endif

/*------------------ Adders ----------------------*/
#ifdef M8_RT_OPTIMISATION
void CI_add_opposoing_lanes_net_gap(int32 gap_time);
#endif


/* setting pointer(s) for external functions */
void CI_set_external_function_pointers(void (*divideError)(int, int, ...));

/* Resetters*/
void CI_reset_current_msg_number();
void CI_reset_stages_demands();
void CI_reset_lanes_average_flow();

#ifdef 	M8_RT_OPTIMISATION
void CI_reset_opposing_lanes_net_gaps();
#endif

void CI_inc_current_msg_num();

/*The following functions are used within the core only*/
void CI_set_prog_pointer(struct Program * progRef);
void CI_set_junction_pointer(struct Junction * junRef);
void CI_set_stages_pointer(struct Stages * stagesRef);
void CI_set_links_pointer(struct Links * linksRef);
void CI_set_lanes_pointer(struct Lanes * lanesRef);
void CI_set_dets_pointer(struct Detectors * detsRef);
void CI_set_msgs_pointer(struct Messages * msgsRef);


void CI_load_dynamic_data();

#endif /*CORE_INTERFACE_H*/