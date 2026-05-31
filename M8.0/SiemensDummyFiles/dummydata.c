#include "nucleus.h"
#include "nu_omu.h"
#include "proj_def.h"

/* From MOVA_IF.c */
/* always enabled in PB681 (rest of structure will be 0's) */
MOVA_IF_type MOVA_IF = { MOVA_ENABLED };

/* From Siemens sysctl.c, only ever reset (=0) from within MOVA kernel */
utiny IC_WATCHDOG_FAIL_COUNT;
utiny soft_error_count; /* count of number of soft errors */

/* From Answer.c - not using phone line, so always TRUE */
unsigned char lLineDropped = TRUE;

/* From Phone.c */
unsigned char lModemTried = FALSE;       /* To allow term to process i/p as if phone not used */

/* From dummy_data.c, used in Write.c - maybe this isn't even used in Siemens MOVA any more */
unsigned char RB_GOOD_BLK_REC = FALSE;

/* From MOVA_IF.c.  Method of licensing will be different in PC-MOVA, so this always returns 1 */
int Valid_Licence( void ) { return 1; }

/*********************************
 * Used (non-dummy) data, but only temporarily here.  Where is best? (TODO)
 *********************************/

/*  These from Siemens MOVA_IF.C - used in Comms.c, Term.c, Phone.c.
    MOVA_IF seems a strange place for these to be.  Not sure where's best. */
/* character constants used by MOVA user interface */
const char cLF = 10;
const char cCR = 13;
const char cBackSp = 8;
const char cDelete = 127;
const char sDelChar[] = {"\b \b"};

/* This from nuc_if.c - again, a strange place for this to be */
/* used by MOVA on cold start (see MONITR) */ 
char lFirstPowerup = TRUE;