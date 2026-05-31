#if !defined (_KDEBUGSETINGS_H)
#define _KDEBUGSETTINGS_H

/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         kdebugsettings
*
*  TITLE:        MOVA Kernel: Kernel debug define level for each module
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*
*  (c) Copyright TRL Ltd 2013. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*r
*******************************************************************************/

#if defined (__cplusplus)
    extern "C" {
#endif

typedef enum
{
	DEBUG_LEVEL_OFF,
	DEBUG_LEVEL_ERROR,
	DEBUG_LEVEL_WARNING,
	DEBUG_LEVEL_INFO,
	DEBUG_LEVEL_VERBOSE
} KDebugLevel;

/* To define the logging level for a module we need to specify three things */
/* 1. The module ID that is called when logging a line */
/* If a name is defined but not a level then no output is produced */

#define SFSTATS "SFSTATS"
#define SFDEPEN "SFDEPEN"
#define OUTPUT "OUTPUT"
#define MONITR "MONITR"
#define SF "SF"
#define DETSCN "DETSCN"
#define GENSTG "GENSTG"
#define MOVAMAIN "MOVA"
#define TMAMEM "TMAMEM"
#define TMAPREPRINT "TMAPREPRINT"
#define WRITEM "WRITEM"
#define DISPLAYMSG "DISPLAYMSG"
#define DATAHAND "DATAHAND"
#define ERRHAND "ERRHAND"
#define PCTASKS "PCTASKS"
#define CORE "CORE"
#define ALG "ALGORITHM"
#define USERBIG "USERBIG"
#define GENFUNC "GENFUNC"
#define BLOCK_A "BLOCK_A"

/* 2. Add Module ID here */
const static char * const moduleNames[] = {SFSTATS, OUTPUT, USERBIG, DISPLAYMSG, ERRHAND, MONITR, OUTPUT, DETSCN, GENSTG, MOVAMAIN, TMAMEM, TMAPREPRINT, CORE, ALG, SF, GENFUNC};

/* 3. Associate level with module here */
/* This is the debug level for the module defined in the same position in the array above */
const static KDebugLevel moduleLevels[] = { DEBUG_LEVEL_OFF, /*SFSTATS*/
											DEBUG_LEVEL_VERBOSE, 
											DEBUG_LEVEL_INFO, 
											DEBUG_LEVEL_VERBOSE, 
											DEBUG_LEVEL_ERROR, 
											DEBUG_LEVEL_INFO, 
											DEBUG_LEVEL_VERBOSE,
											DEBUG_LEVEL_ERROR,
											DEBUG_LEVEL_ERROR,
											DEBUG_LEVEL_INFO,
											DEBUG_LEVEL_INFO,
											DEBUG_LEVEL_INFO,
											DEBUG_LEVEL_INFO,	/*CORE*/
											DEBUG_LEVEL_INFO,   /*ALG*/
										    DEBUG_LEVEL_OFF, 	/*SF*/
											DEBUG_LEVEL_ERROR}; /*GENFUNC*/

/* Name of output file */
#define MODULE_DEBUG_OUTPUT_FILE_NAME_		("debugoutput.log")
/* Either open output file to append or to create new file */
#define FILE_MODE "w"

#if defined (__cplusplus)
    }
#endif

#endif