/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         generalfunc.h
*
*  TITLE:        MOVA General Functions
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	31-May-2011		7.0.0		AA		PK		CRN003			New development and undergoing first full system testing
*	12-Dec-2011		7.0.1		AB		PK		CRN004			SatFlow and CANDET improvements
*	15-Mar-2012		7.0.3		..		AK		CRN009			Changes to conditional compilation flags
*	06-Jul-2012		7.0.3		AC		AK		CRN012			Memory leak in TMA module and statistics lost on dataset change
*
*  (c) Copyright TRL Ltd 2012. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal or
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/

#if !defined (TMA_GENERALFUNC_H)
#define TMA_GENERALFUNC_H

#include "gbltypes.h"
#include "defines.h"
#include "MVTypes.h"
#include "tma_strc.h"

#if defined (PC_WRAPPER)
    /* For PCUserIO_IsConnected() */
    #include "../pcuserio.h"
#endif

typedef signed char MovaDetArray_t[mxdetTypes];
typedef signed char MovaLgreenArray_t[mxlink];
typedef int MovaLlaneArray_t[mxlnOnlink];
typedef int MovaOccalArray_t[mxstag];

#if defined (TRL_INTEGRATION_TEST)
/* Define number that is not known device ID to enable printing to logs */
#define SYS_LOG_PRINTING 123

#define MAX_PATH_AND_FILENAME 500

#endif

/*Initialise the Module*/
// SLM FIXME this does not appear to be used anywhere
//void TMA_Gen_Init_(void );


/*General functions for Mova 7*/

int GetNoStages			(void);
int GetNoLinks			(void);
int GetNoLanes			(void);
int GetNoDetectors		(void);
const MovaDetArray_t		* GetDetArray(void);
const MovaLlaneArray_t		* GetLlaneArray(void);
const MovaLgreenArray_t		* GetLgreenArray(void);

const int GetTMAprd(void);
const int *GetOsatAlArray(void);
const int *GetExitAlArray(void);
const MovaOccalArray_t *GetOcualArray(void);

/* 
  Find the links that run in a given stage (srStage) and returns those links in *linksRun.
  LGREEN is required
*/
void lanesRunStage(int srStage, int *lanesRun,const MovaLlaneArray_t *llanes, const MovaLgreenArray_t *lgreen);

/*
Clear lane's fields with zero, and initializes lane's ID with a consecutive number, given an strLane array
and the number of lanes, (size of the array)
*/
void linksRunStage(int srStage, int *linksRun,const MovaLgreenArray_t *lgreen);

/*
Sends to the proper terminal outputs, caters for only formatted strings. 
op_str: pointer to string to be printed.
It was intended to avoid WRITE function because of the complexity of WRITE function
*/
void writeString(int terminal,  const char *op_str);

/*
  Clear screen using the old write function, please do not use WRITE(INITIALIZE), it has
  too much clutter just to clean the screen.
  It is a copy and paste  from Ian H. Term.c code, no idea why the overuse of const/static for declaration
*/
int clearScreen(int outDevice);

/* 
 Converts integer to char for displaying purposes
*/
 char conSuspectToCh(susDetec susDetA);

 /* 
Checks if a giving detector number (detNo) is an X detector. "XorIn" refers to the index in
 "det" array, which is in turn information from tcomshr.
 If the detector is found, the lane number (laneNo) is returned.
 det is an array coming always from Tcomshr
Careful with index, here detector is 1 based and tcomshr is 0-based*/
int findXdetLane(int detNo, int XorIN, int noLanes, const MovaDetArray_t *det);

void detGetActivePlan(int *mark2Plan, int dstrig[], signed char deton[],int userOverwrites);

/* Make data access functions available */
/*Get the MOVA version Number */
char* GetMovaVersionNumber(void);

/* 
 Get average flow
*/
 int32 GetAverageFlow(int32 Day, int32 HalfHourIncrement, int32 Lane);



#if defined (TRL_INTEGRATION_TEST)
void InitialiseIntegrationLogs( void );
/* Print out message log files for integration tests */
void SysLogWrite( const char * message );
/* Print out sat flow messages for integration tests */
void SatLogWrite( const char * message );
/* Print out errors for integration tests */
void ErrorLogWrite( const char * message );
/* Print out errors for integration tests */
void TMALogWrite( const char * message );
/* Trim string for logs */
void TrimLogString(char *stringtochange);
/* Get single message from message log  */
#endif

void GetMessageDetails(MVInt MessageIndex, int *MessageDetails);
/* IRH MOD: M5.0.0: 20/7/04: New function to copy strings
* (places null-terminator at end of output string) */
char * Safe_Strncpy(char * const szStrOut,	const char * const szStrIn,	const int nCharsNum);


extern void SendStringToLH(const char *sOP_String, int nCharsInString);
extern void SendStringToModem(const char *sOP_String, int nCharsInString);

extern int GetOutputDestination( void );


#if PCMOVA
MVStatus GetWindowsTempFolder(char * folder_path);
#endif

void *  MV_MALLOC(size_t size);
void	MV_FREE(void * ptr);

#endif /*TMA_GENERALFUNC_H*/
