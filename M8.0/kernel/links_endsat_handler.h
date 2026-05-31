/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         links_endsat_handler.h
*
*  TITLE:        Mova Kernel: Links Endsat Handler
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

#if !defined (LINKS_ENDSAT_HANDLER_H)
#define LINKS_ENDSAT_HANDLER_H

#include "dynamic_data.h"

void set_links_pointer_for_endsat_handler(struct Links * linksRef);

void update_link_endsat_marker(uint8 linkIdx);
mv_bool update_normal_link_endsat_marker_after_lanes_checking(uint8 linkIdx, uint8 laneIdx);

#ifdef M8_RE_ESTABLISHMENT_OF_SATFLOW
mv_bool check_link_has_platoon(uint8 linkIdx);
#endif

#endif /*LINKS_ENDSAT_HANDLER_H*/