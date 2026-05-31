/*******************************************************************************
*
*                      MOVA KERNEL BLOCK HEADER
*
*  FILE:         errhand.h
*
*  TITLE:        MOVA Kernel: Graceful Error handling. 
*
*  HISTORY:
*	Date			Version		Issue	Intls	Reference		Description
*	26-Jan-1998		7.0.0		..		MC		SIE_00			First undergoing system test
*	29-Sep-2004		7.0.0		..		IH		SIE_01			Error IDs enumerated and three more added for Datahand.c module errors.		
*	31-May-2011		7.0.0		AA		PK		CRN03			Updated header inline with new Software Quality Plan
*
*  (c) Copyright TRL Ltd 2010. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/
#if !defined (ERROR_HAND_H)
#define ERROR_HAND_H

#define SAV_MESSAGES        (TRUE)
#define NO_SAV_MESSAGES     (FALSE)
#define RESTART_MOVA        (TRUE)
#define NO_REBOOT           (FALSE)


/* IRH MOD: M5.0.0: 06/10/04: Phone home codes for the
 * phone home flag (these may be treated identically or
 * differently depending on the manufacturer's implementation - 
 * see Answer.c for how they're processed) */
#define xgnMOVA_PHONE_HOME_ERROR    (99)
#define xgnMOVA_PHONE_HOME_RESTART  (100)


/* IRH MOD: M5.0.0: 27/08/04: Available error IDs */
enum ERROR_IDS
{
    ERROR_ID_MOVA_BACK_ON_LINE = 1,
    ERROR_ID_FAULTY_DETECTOR,
    ERROR_ID_STAGE_NOT_ENDED,
    ERROR_ID_INTERGREEN_NOT_ENDED,
    ERROR_ID_INVALID_STAGE_DEMANDED,
    ERROR_ID_WRONG_STAGE_CONFIRMED,
    ERROR_ID_STAGE_STUCK_ON,            /* Deprecated */
    ERROR_ID_WATCHDOG_ROUTINE,
    ERROR_ID_CONTROLLER_READY_BIT_OFF,
    ERROR_ID_CONTROLLER_READY_BIT_ON,
    ERROR_ID_PROGRAM_ERROR,
    ERROR_ID_CHECKSUM_ERROR,
    ERROR_ID_RELOADING_ERROR,
    ERROR_ID_MULTIPLE_STAGE_CONFIRMS,
    ERROR_ID_MOVA_RESTARTED,
    ERROR_ID_DIVIDE_ERROR,
    
    /* IRH MOD: M5.0.0: 17/09/04: New error that occurs if MOVA tries to
     * load a dataset from a blank repository area (e.g.
     * may occur if the user hasn't entered all the datasets 
     * used by the current time of day data) */
    ERROR_ID_EMPTY_REPOSITORY_AREA,
    
    /* IRH MOD: M5.0.0: 17/09/04: Error that occurs if MOVA detects
     * a checksum error in a repository dataset */
    ERROR_ID_REPOSITORY_CHECKSUM,

    /* IRH MOD: M5.0.0: 29/09/04: Error that occurs if MOVA
     * is forced to load the default dataset over a corrupted one */    
    ERROR_ID_DEFAULT_LOADED,
 
    ERROR_IDS_NUM
};



void Errhand_Initialise(void);
void Errhand_ResetErrorCount(void);
int Errhand_GetErrorCount(void);
BOOL Errhand_IsErrorValid(int Row);
BOOL Errhand_IsErrorCountValid(void);
int Errhand_DecrementErrorCount(void);
BOOL Errhand_ExceededMaxErrors(int Row);
void Errhand_PrintMOVAError( int ErrorRow, int DeviceID);
void Errhand_GetMOVAError(int ErrorRow, int* ErrorID, time_t* time, int* add1, int* add2, int*add3 );
void Errhand_SetErrorCount(int Count);



/* Function prototypes */
extern void Switch_MOVA_Off(int, int);
extern void DivideError(int, int, ... );
extern void Errhand_LogMOVAError( int nErrorID, int nErrorData );
extern int  Errhand_GetMOVAErrorValue( int nErrorID );
extern BOOL Errhand_IsMOVAOnControl( void );
extern void Errhand_SetMOVAPhoneHome( int nPhoneHomeCode );

# endif /*ERROR_HAND_H*/
