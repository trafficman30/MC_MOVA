/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         detectors_handler.h
*
*  TITLE:        Mova Kernel: Detectors Handler
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

#if !defined (DETECTORS_HANDLER_H)
#define DETECTORS_HANDLER_H

#include "mova_constants.h"
#include "mova_types.h"

void init_dets_handler();

void update_dets_data();
void update_dets_faulty_status(uint8 linkIdx, uint8 laneIdx);
void update_pre_cum_det_count_with_old(uint8 detIdx);
void adjust_count_diff_for_comb_x_det(uint8 laneIdx, int32 * countDiff);

#ifdef M8_DET_FAULT_CHECKING_AND_LOGGING
void update_ped_det_sus_status(uint8 detNum);
#endif

/* Getters (with work)*/
int32 get_dets_count_for_smoothed_flow_calc(uint8 laneIdx);
int16 get_det_num_for_capacity_max_calc(uint8 linkIdx, uint8 laneIdx);

/* Getters */
int32 get_cum_det_count(uint8 detIdx);
int32 get_det_counts_diff_old(uint8 detNum);
int32 get_det_counts_diff_pre(uint8 detNum);
int8 get_det_suspicion_status(uint8 detNum);
int32 get_det_on_time(uint8 detNum);
int32 get_det_off_time(uint8 detNum);
int32 get_det_old_gap(uint8 detIdx);

mv_bool is_det_on(uint8 detNum);
mv_bool is_queue_over_det(uint8 laneIdx, uint8 detIdx);
mv_bool is_det_suspected(uint8 detIdx);
mv_bool is_ep_det_active(uint8 linkIdx, mv_bool isEPDemandedInRed);

/* Setters */
void set_det_suspicion_status(uint8 detNum, int8 status);
void set_det_pre_cum_det_count(uint8 detIdx, int32 val);

#endif /*DETECTORS_HANDLER_H*/