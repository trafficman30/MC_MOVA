/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         program_controller.h
*
*  TITLE:        Mova Kernel: Program Controller
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


#if !defined (PROGRAM_CONTROLLER_H)
#define PROGRAM_CONTROLLER_H

#include "mova_types.h"

void init_program_controller();
void init_program_dynamic_data();

mv_bool program_controller();


int8 get_prog_marker();


#endif /*PROGRAM_CONTROLLER_H*/