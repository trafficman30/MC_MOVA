/*******************************************************************************
*
*                      MOVA KERNEL ALGORITHM
*
*  FILE:         algorithms_manager.h
*
*  TITLE:        Mova Kernel: Algorithms Manager
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

#if !defined (ALGORITHM_MANAGER_H)
#define ALGORITHM_MANAGER_H

#include "estimate_queues_size.h"
#include "calculate_reduced_lost_time.h"
#include "calculate_bonus_green_time.h"
#include "calculate_short_cycle_control_params.h"
#include "delay_and_stops_optimiser.h"
#include "dynamic_data.h"

void ALG_set_junction_pointer(struct Junction * junRef);
void ALG_set_stages_pointer(struct Stages * stagesRef);
void ALG_set_links_pointer(struct Links * linksRef);
void ALG_set_lanes_pointer(struct Lanes * lanesRef);
void ALG_set_dets_pointer(struct Detectors * detsRef);


void ALG_set_algorithm_params(uint8 algID);

mv_bool ALG_is_det_suspected(uint8 detIdx);

#endif /*ALGORITHM_MANAGER_H*/