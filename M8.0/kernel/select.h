
/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         select.h
*
*  TITLE:		Defines for Mova Menu Selection
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	29-Sep-2004		5.0.0		..		IH		SIE_00			First undergoing system test
*	31-May-2011		7.0.0		AA		PK		CRN003			New Header file inline with MOVA Kernel SQP
*
*  (c) Copyright TRL Ltd 2010. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal or
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/


#ifndef INCLUDED_Select
#define INCLUDED_Select

#define LOAD_DATA       1  
#define READ_DATA       2
#define DISPLAY_DATA    3
#define TITLE_INFO      4
#define UPLOAD_DATA     5
#define REMOVE_TOD_DATA 6
#define DIS_ENABLE_TRIG 7
/* IRH MOD: M5.0.0: 10/08/04: Exit changed from 9 to 7 (9 just repeats
 * datahandling sub-menu) */
#define EXIT_           8
#define ABORT_          -1  /* if ctrl-C entered */

/* other psuedo logicals */
/* IRH MOD: M5.0.0: 23/09/04: DOWNLOAD_ added to prevent conflict with START_TIMER
 * in Siemens hardward_escc2.h */
#define START_DOWNLOAD_TIMER 1
#define STOP_DOWNLOAD_TIMER  0

#define PROMPT    1
#define NO_PROMPT 0

#define LOCAL_CONNECTION  1
#define REMOTE_CONNECTION 2

#endif /* INCLUDED_Select */
