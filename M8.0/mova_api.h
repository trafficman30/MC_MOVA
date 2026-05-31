/*******************************************************************************
*
*                      MOVA API HEADER
*
*  FILE:         MOVA_API.H
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

#ifndef MOVA_API_H
#define MOVA_API_H

#include "MVTypes.h"
#include "defines.h"
#include "obclock.h"
#include "gbltypes.h"

/* ------------------------------------------------------------------------- */
/* External variables */
/* ------------------------------------------------------------------------- */
extern char control[ NumberOfForce + NumberOfExtra ];

/* ------------------------------------------------------------------------- */
/* The interface member variables*/
/* ------------------------------------------------------------------------- */
/*Pointer to the InputScan function */
void (*m_input_scan_fun); 

/*Pointer to the OutputScan function */
void (*m_output_scan_fun); 
 
/* Pointer to the Com_read_rtc(int lMode, ... ) function */
TIMESTAMP_TYPE (*m_read_rtc_func)(int lMode, ... ); 




/* ------------------------------------------------------------------------- */
/* API functions declarations*/
/* ------------------------------------------------------------------------- */
MVStatus MI_set_detector_status(MVUInt8 detNo, MVUInt8 value);

MVStatus MI_set_detectors_status(MVUInt8 * detectors_status);

MVStatus MI_set_detectors_status_from_ports(MVUInt8 * detector_ports, MVInt8 num_of_det_ports);

MVStatus MI_set_stages_confirmation(MVUInt8 * stages_confirmation);

MVStatus MI_set_stages_confirmation_from_ports(MVUInt8 * confirmation_ports, MVInt8 num_of_confirm_ports);

MVStatus MI_set_crb_bit(MVBool status);

MVStatus MI_set_input_scan_call(void (*input_scan_func));

MVStatus MI_set_output_scan_call(void (*input_scan_func));

MVStatus MI_set_read_rtc_call(TIMESTAMP_TYPE (*read_rtc_func)(int lMode, ... ));

MVStatus  MI_get_control_bits_status(MVUInt8 * control_param);

/* Sync input_data from Tcomshr each scan -- call before CI_start_scan() */
void MI_sync_scan_input_data(void);

/* Set an io[] flag directly in Tcomshr (index 0-63, e.g. 27=MOVA_ON, 19=ON_CONTROL) */
MVStatus MI_set_io_flag(int index, int value);

/* Read an io[] flag from Tcomshr — returns value or -1 on error */
int MI_get_io_flag(int index);

/* Read the 8 dataset-conditioned special output channels (control[StartofOutputChans..+8]) */
MVStatus MI_get_output_channels(MVUInt8 * out_channels);

/* Call the genstg() co-routine directly — must be called every tick alongside monitr() */
void MI_genstg(void);

/* Read stage-machine diagnostic values */
int MI_get_prog_marker(void);
int MI_get_current_stage(void);
int MI_get_demanded_stage(void);
int MI_get_next_stage(void);
int MI_get_scan_count(void);

/* Directly set the junction demanded stage (for simulation/testing only) */
void MI_set_demanded_stage(int stage);

/* Diagnostics */
int MI_get_wait_stage_change_timer(void);  /* wait_stage_change_timer ×10 (tenths of a second) */
int MI_is_in_warmup(void);                 /* 1 = still in warmup, 0 = warmup complete */
int MI_get_kernel_stages(void);            /* Tcomshr->stages as seen by C kernel */
int MI_get_kernel_crb(void);               /* confin[mxconf-1] — CRB in C kernel */
int MI_get_det_fault(void);                /* Tdinout->dout[DoutDetFault] — detector fault LED */
int MI_get_mova_fault(void);               /* MOVA fault: off-control OR phone home */
const char* MI_get_kernel_version(void);   /* Kernel version string e.g. "M8.0.0.435" */
void MI_set_time_offset(long seconds);     /* Shift kernel clock for TOD testing (0 = real time) */
long MI_get_time_offset(void);             /* Current offset in seconds */

/* Read back true detector states after kernel tick, including special conditioning outputs.
 * out is caller-allocated array of length max_dets (0-based). */
void MI_get_all_detectors(signed char *out, int max_dets);

/* TMA detector count for the current TMAPRD period (det 0-based, matches _detFlow[]) */
int MI_get_tma_det_flow(int det);       /* kernel vehicle count for detector det this period */

/* TMA per-lane flow counts — populated at period end only, not during current period */
int MI_get_tma_lane_xflow(int lane);    /* X (stopline) vehicle count, completed periods */
int MI_get_tma_lane_inflow(int lane);   /* IN (advance) vehicle count this period */
int MI_get_tma_lane_xsuspect(int lane); /* X suspect: 0=ok 1=active 2=inactive */
int MI_get_tma_lane_insuspect(int lane);/* IN suspect: 0=ok 1=active 2=inactive */
long MI_get_tma_period_start(void);     /* Unix timestamp of current period start */
int MI_get_tma_period_sec(void);        /* TMAPRD period length in seconds */

/* ------------------------------------------------------------------------- */
/* Other functions declarations (will be used within the interface only)*/
/* ------------------------------------------------------------------------- */




#endif

void MI_gs_check(void);
