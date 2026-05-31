/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         messages_manager.h
*
*  TITLE:        Mova Kernel: Messages Manager
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

#if !defined (MESSAGES_MANAGER_H)
#define MESSAGES_MANAGER_H

#include "mova_types.h"

void MSG_set_junction_pointer(struct Junction * junRef);
void MSG_set_stages_pointer(struct Stages * stagesRef);
void MSG_set_links_pointer(struct Links * linksRef);
void MSG_set_lanes_pointer(struct Lanes * lanesRef);
void MSG_set_dets_pointer(struct Detectors * detsRef);

void init_messages_manager();

void message_1_1();
void message_1_2();
void message_2();
void message_3_1();
void message_3_2();
void message_4();
void message_5_1();
void message_5_2();
void message_5_3();
void message_5_4();
void message_6_1();
void message_6_2();

#endif /*MESSAGES_MANAGER_H*/