/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         lanes_endsat_handler.h
*
*  TITLE:        Mova Kernel: Lanes Endsat Handler
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

#if !defined (LANES_ENDSAT_HANDLER_H)
#define LANES_ENDSAT_HANDLER_H

void set_lanes_pointer_for_endsat_handler(struct Lanes * lanesRef);

mv_bool update_lane_endsat_marker(uint8 linkIdx, uint8 laneIdx);


int32 calc_lanes_endsat_green_time_threshold(uint8 linkIdx);
void calc_lane_capacity_max_params(	uint8 linkIdx,
									uint8 laneIdx /*m-1*/,
									uint8 detNum,
									int32 * excessGap /*e*/,
									int32 * satflowHeadway /*c*/,
									int32 * registerVeh /*f*/,
									int32 * registerTime /*h*/,
									int32 offTime /*g*/);

#ifdef M8_RE_ESTABLISHMENT_OF_SATFLOW
mv_bool check_lane_has_platoon(uint8 laneIdx);
#endif

#endif /*LANES_ENDSAT_HANDLER_H*/