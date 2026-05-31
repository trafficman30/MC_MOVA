#if !defined (__PROJ_DEF_H)
#define __PROJ_DEF_H

#include "base_types.h"

/* particular value for 'MOVA_application' */
/* (to make it less likely that a corruption will enable the application) */
#define MOVA_ENABLED 123

#define MOVAIF_COMMS_TO_OMU   2
#define MOVAIF_COMMS_TO_MOVA  3

/* TODO: Need to replicate some of functionality below */

/* OMU/MOVA interface items */
typedef struct {
    
    ///* flag to indicate if the MOVA application is enabled, see MOVA_ENABLED above */
    utiny MOVA_application;
    ///* remote communications state, see MOVAIF_COMMS_ values above */
    utiny RComms;
    ///* local communications state, see MOVAIF_COMMS_ values above */
    utiny LComms;

#if 0
    ///* does the MOVA application wants to phone home */
    //utiny PhoneHomeFlag;
    ///* required states of the MOVA outputs */
    //ushort Outputs;
    ///* I/O card used by MOVA - see io_hw_port() and SOP handset command */
    //utiny IO_card;
    ///* detectors received from the controller over the ESP link */
    //utiny ESP_dets[ESP_MOVA_D_SIZE];
    ///* controller ready bit received from the controller over the ESP link */
    //utiny ESP_CRB;
    ///* confirms received from the controller over the ESP link */
    //utiny ESP_conf[ESP_MOVA_C_SIZE];
    ///* flag set when a MOVA ESP message is received */
    //utiny ESP_msg_received;
#endif

    /* timer used to confirm MOVA has dropped off control */    
    utiny MOVA_Off_Timer;

} MOVA_IF_type;
extern MOVA_IF_type MOVA_IF;


extern char GetMOVACharFromHandset(void);
extern void SendStringToModem(const char *sOP_String, int nCharsInString);
extern void SendStringToLH(const char *sOP_String, int nCharsInString);

#endif /* __PROJ_DEF_H */