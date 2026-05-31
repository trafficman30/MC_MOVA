/*******************************************************************************
*
*                      MOVA API HEADER
*
*  FILE:         MOVA_API.C
*
*  TITLE:        MOVA API
*
*  HISTORY:
*  Date         Version  Issue  Intls  Reference   Description
*  22-Feb-2013   0.0.1    AA     IA		XX_00      First creation
*
*  (c) Copyright TRL Ltd 2013. Based on and adapted from Crown Copyright
*  material under licence. Rights to and ownership of the software remain
*  unaffected by changes to the software, whether by addition, removal or
*  modification. Agreement has been reached between The MOVA Licensees
*  and TRL Ltd whereby The Licensees are authorised to use and
*  modify the MOVA code. TRL Ltd cannot be held responsible for improper
*  use.
*
*******************************************************************************/

#include "mova_api.h"

#include "memory.h"
#include "kernel/core_interface.h"
#include "kernel/junction_handler.h"
#include "kernel/timers_manager.h"
#include "kernel/errhand.h"
#include "kernel/dataset_handler.h"
#include "kernel/generalfunc.h"
#include "kernel/tma_strc.h"

/*------------------- Internal functions prototypes ---------------------------*/

static void set_detsin_latches();


/*-----------------------------------------------------------------------------*/


/*-----------------------------------------------------------------------------*/
/*---------------------------- The Setters ------------------------------------*/
/*-----------------------------------------------------------------------------*/

/*========================= Detectors functions ===============================*/

/*!
*   \fn			MVStatus MI_set_detector_status(MVUInt8 detector_num, MVUInt8 status)
*
*	\brief		This function replaces the dealing with the 'detsin' array directly by setting
*				the status of a certain detector (detNo).
*				The 'detsin' array holds all the detectors' status and it is updated
*				by the signal controller (PC/IOScan).
*
*	\param[in]	detNo	the detector number which this function sets its status.
*	\param[in]	value	the new value of this detector's status.
*
*	\return		If the setting is done successfully the function returns MV_SUCCESS, otherwise
*				it returns MV_FAILURE.
*
*	\author		Islam Abdelhalim  
*	\date		22-02-2013
*/
MVStatus MI_set_detector_status(MVUInt8 detector_num, MVUInt8 status)
{
	if(detector_num < 0 || detector_num >= mxdets)
	{
		return MV_FAILURE;
	}

	/*To check that the input 'value' is binary*/
	if(status != 0 || status != 1)
	{
		return MV_FAILURE;
	}

	detsin[detector_num] = status;
	
	set_detsin_latches();

	return MV_SUCCESS;
}

/*!
*   \fn			MVStatus MI_set_detectors_status(MVUInt8 * detectors_status)
*
*	\brief		This function replaces the dealing with the 'detsin' array directly by setting
*				the status of all the detectors.
*				The 'detsin' array holds all the detectors' status and it is updated
*				by the signal controller (PC/IOScan).
*
*	\param[in]	detectors_status pointer to the array that will be copied in the 'detsin'
*
*	\return		If the setting is done successfully the function returns MV_SUCCESS, otherwise
*				it returns MV_FAILURE.
*
*	\author		Islam Abdelhalim  
*	\date		22-02-2013
*/
MVStatus MI_set_detectors_status(MVUInt8 * detectors_status)
{
	if( memcpy_s(detsin, mxdets,detectors_status, mxdets) == 0)
	{
		set_detsin_latches();

		return MV_SUCCESS;
	}
	else
	{
		return MV_FAILURE;
	}
}

/*!
*   \fn			MI_set_detectors_status_from_ports(MVUInt8 * detector_ports, MVInt8 num_of_det_ports)
*
*	\brief		This function sets the detectors status (by setting the 'detsin' array) based on the ports arrays directly. 
*				The 'detsin' array holds all the detectors' status and it is updated
*				by the signal controller (PC/IOScan).
*
*	\param[in]	detector_ports		pointer to the array which holds the detectors' ports status. Each byte in this arrays 
*									holds the status of 8 detectors.
*	\param[in]	num_of_det_ports	the number of ports in the detector_ports array. In the latest MOVA releases, this number is always 8 because
*									the total number of detectors is 64 (= sizeof(byte) x 8).
*
*	\return		If the setting is done successfully the function returns MV_SUCCESS, otherwise
*				it returns MV_FAILURE.
*
*	\author		Islam Abdelhalim  
*	\date		22-02-2013
*/
MVStatus MI_set_detectors_status_from_ports(MVUInt8 * detector_ports, MVInt8 num_of_det_ports)
{
	/* NOTE: when calling this function; 'detector_ports' should be 'g_detectorPorts' */

	int port_size = 8; //size of a byte in bits
	int bit = 0;
	int i, j = 0;

	/*i here represents the port index */
	for(i = 0; i < num_of_det_ports; i++ )
    {
		if(detector_ports[i] == NULL) return MV_FAILURE;

		for(j= 0; j < port_size; j++) 
		{
			if(bit == mxdets) return MV_FAILURE;

			/* extract the state of the current detector input bit */
			detsin[bit] = (char)(detector_ports[i] & 0x01);												 
        
			/* move onto the next input */
			bit++;
			detector_ports[i] >>= 1;
		}
    }

	set_detsin_latches();

	return MV_SUCCESS;
}


/*========================= Confirmation bits functions ===============================*/


/*!
*   \fn			MVStatus MI_set_stages_confirmation(MVUInt8 * stages_confirmation)
*
*	\brief		This function replaces the dealing with the 'confin' array directly by setting
*				the confirmation status of all the stages.
*				The 'confin' array holds all the stages confirmation bits and it is updated
*				by the signal controller (PC/IOScan).
*
*	\param[in]	stages_confirmation pointer to the array that will be copied in the 'confin'
*
*	\return		If the setting is done successfully the function returns MV_SUCCESS, otherwise
*				it returns MV_FAILURE.
*
*	\author		Islam Abdelhalim  
*	\date		22-02-2013
*/
MVStatus MI_set_stages_confirmation(MVUInt8 * stages_confirmation)
{
	/* TBC: the -1 below. I believe it is correct because the last bit is the CRB bit  */

	if( memcpy_s(confin, mxconf-1,stages_confirmation, mxconf-1)  == 0)
	{
		return MV_SUCCESS;
	}
	else
	{
		return MV_FAILURE;
	}
}



/*!
*   \fn			MI_set_stages_confirmation_from_ports(MVUInt8 * confirmation_ports, MVInt8 num_of_confirm_ports)
*
*	\brief		This function sets the stages confirmation status (by setting the 'confin' array) based on the ports arrays directly. 
*				The 'confin' array holds all the stages confirmation bits and it is updated
*				by the signal controller (PC/IOScan).
*
*	\param[in]	confirmation_ports		pointer to the array which holds the stages confirmation's ports status. Each byte in this arrays 
*										holds the status of 8 stages.
*	\param[in]	num_of_confirm_ports	the number of ports in the confirmation_ports array. In the latest MOVA releases, this number is always 4.
*
*	\return		If the setting is done successfully the function returns MV_SUCCESS, otherwise
*				it returns MV_FAILURE.
*
*	\author		Islam Abdelhalim  
*	\date		26-02-2013
*/
MVStatus MI_set_stages_confirmation_from_ports(MVUInt8 * confirmation_ports, MVInt8 num_of_confirm_ports)
{
	/* NOTE: when calling this function; 'confirmation_ports' should be 'g_confirmPorts' */

	int port_size = 8; //the size of a byte in bits
	int bit = 0;
	int i, j = 0;

	/*i here represents the port index */
	for(i = 0; i < num_of_confirm_ports; i++ )
    {
		if(confirmation_ports[i] == NULL) return MV_FAILURE;

		for(j= 0; j < port_size; j++) 
		{
			if(bit == mxconf-2) break; /*This is to avoid setting the last bit which is reserved for the CRB */

			/* extract the state of the stage confirmation input bit */
			confin[bit] = (char)(confirmation_ports[i] & 0x01);												 
        
			/* move onto the next input */
			bit++;
			confirmation_ports[i] >>= 1;
		}
    }

	return MV_SUCCESS;
}


/*!
*   \fn			MVStatus MI_set_crb_bit(MVBool status)
*
*	\brief		This function sets the status of the CRB (Controller Ready Bit) in the 'confin'
*				array.
*
*	\param[in]	status	the new value of the CRB.
*
*	\return		If the setting is done successfully the function returns MV_SUCCESS, otherwise
*				it returns MV_FAILURE.
*
*	\author		Islam Abdelhalim  
*	\date		26-02-2013
*/
MVStatus MI_set_crb_bit(MVBool status)
{
	if(confin == NULL)
	{
		return MV_FAILURE;
	}
	else
	{
		confin[mxconf - 1] = ( status ? 1 : 0 );

		return MV_SUCCESS;
	}
}


/*
*   \fn			void MI_update_stages_confirmation_polarity()
*
*	\brief		Change confirm polarity according to polarity specified
*               in the current MOVA dataset (only for confirms that are 
*               actually in use - unused confirms should be left as zero).
*               Also set polarity of phase confirms correctly according 
*               to PHASON in MOVA dataset.
*
*	\author		Islam Abdelhalim  
*	\date		26-02-2013
*/
void MI_update_stages_confirmation_polarity()
{
	
	/*TODO: there is no need for inversing the polarity here.*/


	 /* Change stage polarity from inverse to normal if this was 
    specified in the dataset data (default is INVERSE) */
    if ( Tcomshr->stagon == 1 )
    {
        int stage;
        for ( stage = 0; stage != Tcomshr->stages; ++stage )
        {
            confin[ stage ] ^= 1;
        }

    } /* If stage confirms are normal polarity */

    /* Change phase polarity from inverse to normal if this was 
    specified in the dataset data (default is INVERSE) */
    if ( Tcomshr->phason == 1 )
    {
        int link, phase;
        for ( link = 0; link != Tcomshr->nlinks; ++link )
        {
            /* Get the phase number for this link (if there is one) */
            phase = Tcomshr->lphase[ link ];
            if ( phase > 0 )
            {
                confin[ phase - 1 ] ^= 1;
            }
        } 

    } /* If phase confirms are normal polarity */
}

/*========================= Callback functions ===============================*/

/*
MVStatus MI_set_stages_confirmation_from_ports(MVUInt8 * stages_confirmation)
I will postpone implementing this function until testing the detectors one.
*/

/*!
*   \fn			MI_set_input_scan_call(void (*input_scan_func))
*
*	\brief		This function sets the a pointer to the InputScan() function which is defined
*				in the PC/IOScan. The DETSCN component will need this pointer to call the
*				the InputScan() function in the appropriate timing.
*				This function should be called in the initialisation stage and before DETSCN
*				starts working.
*
*	\param[in]	input_scan_func pointer to the InputScan() function.
*
*	\return		If the setting is done successfully the function returns MV_SUCCESS, otherwise
*				it returns MV_FAILURE.
*
*	\author		Islam Abdelhalim  
*	\date		26-02-2013
*/
MVStatus MI_set_input_scan_call(void (*input_scan_func))
{
	if(input_scan_func == NULL)
	{
		return MV_FAILURE;
	}
	else
	{
		m_input_scan_fun = input_scan_func;

		return MV_SUCCESS;
	}
}

/*!
*   \fn			MI_set_output_scan_call(void (*output_scan_func))
*
*	\brief		This function sets the a pointer to the OutputScan() function which is defined
*				in the PC/IOScan. The GENSTG component will need this pointer to call the
*				the OutputScan() function in the appropriate timing.
*				This function should be called in the initialisation stage and before GENSTG
*				starts working.
*
*	\param[in]	output_scan_func pointer to the OutputScan() function.
*
*	\return		If the setting is done successfully the function returns MV_SUCCESS, otherwise
*				it returns MV_FAILURE.
*
*	\author		Islam Abdelhalim  
*	\date		26-02-2013
*/
MVStatus MI_set_output_scan_call(void (*output_scan_func))
{
	if(output_scan_func == NULL)
	{
		return MV_FAILURE;
	}
	else
	{
		m_output_scan_fun = output_scan_func;

		return MV_SUCCESS;
	}
}

/*!
*   \fn			MI_set_read_rtc_call(TIMESTAMP_TYPE (*read_rtc_func)(int lMode, ... ))
*
*	\brief		This function sets the a pointer to the Com_read_rtc(int lMode, ... ) function which is defined
*				in the PCClock/OBClock. The DETSCN component will need this pointer to call the
*				the Com_read_rtc(int lMode, ... ) function in the appropriate timing.
*				This function should be called in the initialisation stage and before DETSCN
*				starts working.
*
*	\param[in]	read_rtc_func pointer to the Com_read_rtc(int lMode, ... ) function.
*
*	\return		If the setting is done successfully the function returns MV_SUCCESS, otherwise
*				it returns MV_FAILURE.
*
*	\author		Islam Abdelhalim  
*	\date		26-02-2013
*/
MVStatus MI_set_read_rtc_call(TIMESTAMP_TYPE (*read_rtc_func)(int lMode, ... ))
{
	if(read_rtc_func == NULL)
	{
		return MV_FAILURE;
	}
	else
	{
		m_read_rtc_func = read_rtc_func;

		return MV_SUCCESS;
	}
}


/*-----------------------------------------------------------------------------*/
/*---------------------------- The Getters ------------------------------------*/
/*-----------------------------------------------------------------------------*/

/*!
*   \fn			MVStatus  MI_get_control_bits_status(MVUInt8 * control_param)
*
*	\brief		This function gets the controls bits (force bits + HI bit + TO bit + Synch Bit)
*				which are stored in the 'control' array in the GENSTG component.
*
*	\param[in,out]	pointer to the array of the control bits that this function will fill.
*
*	\return		If the getting is done successfully the function returns MV_SUCCESS, otherwise
*				it returns MV_FAILURE.
*
*	\author		Islam Abdelhalim  
*	\date		26-02-2013
*/
MVStatus  MI_get_control_bits_status(MVUInt8 * control_param)
{
	if( memcpy_s(control_param, NumberOfForce+NumberOfExtra ,control, NumberOfForce+NumberOfExtra) == 0)
	{
		return MV_SUCCESS;
	}
	else
	{
		return MV_FAILURE;
	}
}

/*!
*   \fn			MVBool MI_get_take_over_bit_status()
*
*	\brief		This functions gets the take over bit (T.O.) status from the 'control'
*				array.
*
*	\return		Based on the T.O. bit value in the 'control' array, this function returns
*				MV_FALSE (if the bit = 0) or MV_TRUE (if the bit = 1).
*
*	\author		Islam Abdelhalim  
*	\date		26-02-2013
*/
MVBool MI_get_take_over_bit_status()
{
	return control[mxstag] == 0 ? MV_FALSE : MV_TRUE;
}



/*-----------------------------------------------------------------------------*/
/*------------------------ Internal functions----------------------------------*/
/*-----------------------------------------------------------------------------*/
static void set_detsin_latches()
{
	int d;

	for (d = 0; d < mxdets; d++ )
    {
        /* clear the '0' latch if the detector sample is a '0' this time */
        detsin_0s[d] &= detsin[d];

        /* set the '1' latch if the detector sample is a '1' this time */
        detsin_1s[d] |= detsin[d];
    }
}

/*
 * MI_sync_scan_input_data — refresh input_data from Tcomshr before CI_start_scan().
 *
 * detscn() updates Tcomshr->snow each tick but CI_set_each_scan_input_data() is only
 * called once at MONITR_TASK_INIT_START (monitr.c:327) when snow=0.  program_controller()
 * reads from input_data.current_stage so it always sees INTERGREEN_STAGE and the warmup
 * counter never advances.  Calling this from InputScan() (which detscn invokes after
 * updating snow) fixes the stale current_stage and allows warmup to count stage changes.
 */
void MI_sync_scan_input_data(void)
{
    if (Tcomshr == NULL) return;
    CI_set_each_scan_input_data(
        Tcomshr->snow,
        Tcomshr->greens,
        Tcomshr->deton,
        Tcomshr->pregap,
        Tcomshr->ton,
        Tcomshr->toff,
        Tcomshr->vc
    );
}

/*
 * MI_set_io_flag — write directly to Tcomshr->io[index].
 * Equivalent to what the terminal task does for MOVA_ON (io[27]) and
 * ON_CONTROL (io[19]) when the user presses 'M'/'O' in the embedded UI.
 * Only indices 0–(mxdets-1) are valid; mxdets = NumberOfDetectors = 64.
 */
MVStatus MI_set_io_flag(int index, int value)
{
    if (Tcomshr == NULL) return MV_FAILURE;
    if (index < 0 || index >= mxdets) return MV_FAILURE;
    Tcomshr->io[index] = (signed char)value;
    return MV_SUCCESS;
}

int MI_get_io_flag(int index)
{
    if (Tcomshr == NULL) return -1;
    if (index < 0 || index >= mxdets) return -1;
    return (int)Tcomshr->io[index];
}

/*
 * MI_get_output_channels — read the 8 dataset-conditioned special output channels.
 * These occupy control[StartofOutputChans .. StartofOutputChans+NumberofOutputChans-1]
 * i.e. control[15..22] with M8_ENABLED (NumberOfForce=10, NumberOfExtra=5).
 * MI_get_control_bits_status only copies the first 15 elements; this fills the rest.
 * out_channels must point to a buffer of at least NumberofOutputChans (8) bytes.
 */
MVStatus MI_get_output_channels(MVUInt8 * out_channels)
{
    if (out_channels == NULL) return MV_FAILURE;
    if( memcpy_s(out_channels, NumberofOutputChans,
                 control + StartofOutputChans, NumberofOutputChans) == 0 )
        return MV_SUCCESS;
    return MV_FAILURE;
}

/*
 * MI_genstg — call the genstg() co-routine for one iteration.
 * In PC_WRAPPER mode genstg is not called from within monitr(); it must be
 * driven externally every tick so its state machine advances through
 * update_end() → handle_consistent_crbs() where force bits are set.
 */
extern void genstg(unsigned long argc, void *argv);
void MI_genstg(void)
{
    genstg(0, NULL);
}

int MI_get_prog_marker(void)   { return (int)CI_get_program_marker(); }
int MI_get_current_stage(void) { return (int)CI_get_current_stage(); }
int MI_get_demanded_stage(void){ return (int)CI_get_demanded_stage(); }
int MI_get_next_stage(void)    { return (int)CI_get_next_stage(); }
void MI_set_demanded_stage(int stage) { CI_set_demanded_stage((uint8)stage); }

/* Diagnostic: wait_stage_change_timer (×10 = tenths of a second) */
int MI_get_wait_stage_change_timer(void)
{
    TimersStruct *t = CI_get_timers_data_ptr();
    if (t == NULL) return -1;
    return (int)(t->wait_stage_change_timer * 10);  /* tenths of a second */
}

/* Diagnostic: 1 if still in warmup, 0 if warmup complete */
int MI_is_in_warmup(void)
{
    return (int)is_in_warmup();
}

/* Reset warmup counter to trigger another GS_check attempt.
 * Set to Tcomshr->stages (not stages+1) so warmup_check() sees WC < stages+1
 * and calls GS_check on the next detscn cycle. */
void MI_set_warmup_counter(int value)
{
    if (Tcomshr == NULL) return;
    CI_set_warmup_counter(value);
}

/* Diagnostic: return Tcomshr->stages (dataset stage count seen by C kernel) */
int MI_get_kernel_stages(void)
{
    if (Tcomshr == NULL) return -1;
    return (int)Tcomshr->stages;
}

/* Diagnostic: return confin[mxconf-1] (CRB as seen by the C kernel) */
int MI_get_kernel_crb(void)
{
    extern char confin[];
    if (Tcomshr == NULL) return -1;
    return (int)confin[mxconf - 1];
}

/* Return Tdinout->dout[DoutDetFault] — detector fault LED output from kernel. */
int MI_get_det_fault(void)
{
    if (Tdinout == NULL) return 0;
    return (int)Tdinout->dout[DoutDetFault];
}

/* Return MOVA fault state for the FLT output.
 *
 * io[PHONE_HOME] (io[17]) is set to 99 by the kernel on any serious fault
 * (EC>=20, detector fault, watchdog) but in headless operation it is never
 * cleared by the modem answer task.  We treat phone-home as an active fault
 * only when EC>=20 or MOVA_ON was dropped (ON_CONTROL=0 while io[27] was 1).
 *
 * This mirrors the real hardware: FLT drives when MOVA goes off-control due
 * to an error, same way HI drives when MOVA is in intergreen on control.
 */
int MI_get_mova_fault(void)
{
    if (Tcomshr == NULL) return 0;
    /* EC>=20: hard fault — requires manual reset */
    if ((int)Tcomshr->io[ERROR_COUNT] >= 20)
        return 1;
    /* Phone home with EC elevated: serious comms fault */
    if ((int)Tcomshr->io[PHONE_HOME] >= 99 && (int)Tcomshr->io[ERROR_COUNT] > 0)
        return 1;
    return 0;
}

/* Message ring buffer access.
 * The kernel stores up to 50 decision messages (MAX_MSGS=50, MSG_MAX_LENGTH=184).
 * current_msg_num is 1-based and wraps 1→50. After wrap_msgs() it points to the
 * next slot to write, so the last written message is at slot current_msg_num-2. */

int MI_get_current_msg_num(void)
{
    struct Messages * m = CI_get_msgs();
    if (m == NULL) return 0;
    return (int)m->current_msg_num;
}

/* Read one int32 field from msgs_data[slot][field].
 * slot: 0-based (0..49), field: 0-based (0..183). */
int MI_get_msg_field(int slot, int field)
{
    if (slot < 0 || slot >= 50)    return 0;  /* MAX_MSGS */
    if (field < 0 || field >= 184) return 0;  /* MSG_MAX_LENGTH */
    struct Messages * m = CI_get_msgs();
    if (m == NULL) return 0;
    return (int)m->msgs_data[slot][field];
}

/* ── Error store access ──────────────────────────────────────────────────────
 * ErrorStore[4][MAX_NO_OF_ERRORS_STORED] holds up to 100 logged errors.
 * ErrorCount is 1-based; wraps back to 1 after 100. The most recent error
 * is at slot (ErrorCount - 2) mod 100, zero-indexed.
 *
 * Encoding per slot:
 *   field 0 (ID_AND_DAY):       (10 + errorID) * 1000 + day-of-month
 *   field 1 (MONTH_AND_YEAR):   month * 100 + year (2-digit)
 *   field 2 (HOURS_AND_MINUTES):hours * 100 + minutes
 *   field 3 (DATA):             error-specific data value
 */
int MI_get_error_count(void)
{
    return Errhand_GetErrorCount();
}

/* row: 0-based (0 .. MAX_NO_OF_ERRORS_STORED-1), field: 0-3 */
int MI_get_error_store(int row, int field)
{
    extern int ErrorStore[4][100];   /* MAX_NO_OF_ERRORS_STORED = 100 */
    if (row < 0 || row >= 100) return 0;
    if (field < 0 || field >= 4)  return 0;
    return ErrorStore[field][row];
}

/* ── Active data plan ────────────────────────────────────────────────────────
 * Returns the 1-based plan number currently active in the kernel (0 = none).
 */
int MI_get_active_plan(void)
{
    return (int)DSH_get_active_data_plan_number();
}

/* Switch to a different data plan (1-based). Returns 1 on success, 0 on fail. */
int MI_switch_plan(int plan_num)
{
    if (plan_num < 1) return 0;
    return DSH_load_dataset_from_memory((MVInt8)plan_num) ? 1 : 0;
}

/* Return which plan the TOD table says should be active right now (1-based). */
int MI_get_tod_plan(void)
{
    return (int)DSH_get_active_data_plan_num_based_on_tod_table();
}

/* Kernel version string — e.g. "M8.0.0.435" */
const char* MI_get_kernel_version(void)
{
    return GetMovaVersionNumber();
}

/* ── TMA per-lane flow counts (current period) ───────────────────────────────
 * lane: 0-based (0 .. mxlane-1)
 * _xFlow  = vehicles counted over X (stopline) detectors for this lane
 * _inFlow = vehicles counted over IN (advance) detectors for this lane
 * Suspect status: 0=NoFault, 1=Active, 2=Inactive
 * These accumulate from the start of the current TMAPRD period.
 */
/* Read back all detector states from detsin[] after kernel tick.
 * detsin[] is the authoritative detector state array: it reflects both the inputs
 * we injected via MI_set_detectors_status() AND any modifications made by the
 * Special Conditioning module (SC_ProcessAllRules → SetDetectorStatus).
 * out: caller-supplied array, 0-based, length max_dets. */
void MI_get_all_detectors(signed char *out, int max_dets)
{
    extern void GetDetectorsStatus(char *);
    char tmp[mxdets];
    int  i;
    GetDetectorsStatus(tmp);
    for (i = 0; i < max_dets && i < mxdets; i++)
        out[i] = tmp[i];
}

/* Per-detector vehicle count for the current period (det 0-based, same indexing as _detFlow[]). */
int MI_get_tma_det_flow(int det)
{
    if (pTmaData == NULL || det < 0 || det >= mxdets) return -1;
    return pTmaData->_currPeriod._detFlow[det];
}

int MI_get_tma_lane_xflow(int lane)
{
    if (pTmaData == NULL || lane < 0 || lane >= mxlane) return -1;
    return pTmaData->_currPeriod._Lanes[lane]._xFlow;
}

int MI_get_tma_lane_inflow(int lane)
{
    if (pTmaData == NULL || lane < 0 || lane >= mxlane) return -1;
    return pTmaData->_currPeriod._Lanes[lane]._inFlow;
}

int MI_get_tma_lane_xsuspect(int lane)
{
    if (pTmaData == NULL || lane < 0 || lane >= mxlane) return -1;
    return (int)pTmaData->_currPeriod._Lanes[lane]._xSuspect;
}

int MI_get_tma_lane_insuspect(int lane)
{
    if (pTmaData == NULL || lane < 0 || lane >= mxlane) return -1;
    return (int)pTmaData->_currPeriod._Lanes[lane]._inSuspect;
}

/* Unix timestamp (seconds) when the current TMA period started. */
long MI_get_tma_period_start(void)
{
    if (pTmaData == NULL) return 0;
    return (long)pTmaData->currPerStarted;
}

/* TMA logging period length in seconds (TMAPRD from dataset, e.g. 3600 = 60 min). */
int MI_get_tma_period_sec(void)
{
    if (pTmaData == NULL) return 0;
    return pTmaData->periodSec;
}

