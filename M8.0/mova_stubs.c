/* mova_stubs.c
 * Stub implementations for PC-MOVA functions not needed in CM5 headless build.
 */
#include <string.h>
#include <time.h>
#include <stddef.h>

/* Handset/modem display — not needed headless */
void SendStringToLH(const char *s, int n)     { (void)s; (void)n; }
void SendStringToModem(const char *s, int n)  { (void)s; (void)n; }
int  GetMOVACharFromHandset(void)             { return -1; }

/* TIMESTAMP_TYPE from obclock.h */
typedef struct {
    unsigned long date, month, year, hours, minutes, seconds;
    int day;
} MOVA_TIMESTAMP;

/* TOD test offset — added via MI_set_time_offset(seconds).
 * Shifts the time the kernel sees without touching the system clock.
 * Useful for testing data plan timetable switching. Zero by default. */
static long _time_offset_sec = 0;

void MI_set_time_offset(long seconds) { _time_offset_sec = seconds; }
long MI_get_time_offset(void)         { return _time_offset_sec; }

/* Clock functions — return current system time as TIMESTAMP_TYPE */
MOVA_TIMESTAMP Com_read_rtc(int lMode, ...) {
    MOVA_TIMESTAMP ts = {0};
    time_t now = time(NULL) + _time_offset_sec;
    struct tm *t = localtime(&now);
    if (t) {
        ts.date    = t->tm_mday;
        ts.month   = t->tm_mon + 1;
        ts.year    = t->tm_year % 100;
        ts.hours   = t->tm_hour;
        ts.minutes = t->tm_min;
        ts.seconds = t->tm_sec;
        ts.day     = t->tm_wday;  /* 0=Sun, 1=Mon, ... */
    }
    (void)lMode;
    return ts;
}

long Com_read_rtc_timet(int lMode, ...) { (void)lMode; return (long)(time(NULL) + _time_offset_sec); }

/* IO scan callbacks — set via MI_set_input_scan_call/MI_set_output_scan_call */
/* detscn() updates Tcomshr->snow before calling InputScan(). MI_sync_scan_input_data()
 * forwards the updated snow (current stage) into input_data so program_controller()
 * sees the real stage rather than INTERGREEN_STAGE=0 forever — this is what allows
 * the warmup counter to advance on each stage confirm change.
 *
 * CI_start_scan() runs program_controller(), whose stage timers increment by
 * DI_get_scan_period() (= dataset SCAN value, typically 0.5s) on each call.
 * In the embedded system CI_start_scan runs every 500ms; InputScan is called
 * every 100ms by detscn.  We must gate CI_start_scan to one call per 5
 * InputScan calls so stage timers (min/max green) run at the correct rate. */
extern void MI_sync_scan_input_data(void);
extern int CI_start_scan(void);
static int _scan_div      = 0;
static int _scan_divisor  = 5;   /* default 5 = 500ms scan rate (real time) */
static int _ci_scan_calls = 0;
void InputScan(void) {
    MI_sync_scan_input_data();
    if (++_scan_div >= _scan_divisor) { _scan_div = 0; _ci_scan_calls++; CI_start_scan(); }
}
int MI_get_scan_count(void) { return _ci_scan_calls; }
void MI_set_scan_divisor(int divisor) {
    if (divisor < 1) divisor = 1;
    if (divisor > 5) divisor = 5;
    _scan_divisor = divisor;
    _scan_div     = 0;   /* reset phase so change takes effect immediately */
}
/* Auto-clear io[PHONE_HOME] (io[17]) once MOVA has recovered.
 * In headless operation the modem answer task never runs so the flag
 * stays at 99 permanently.  Clear it when EC=0 and ON_CONTROL=1. */
extern int MI_get_io_flag(int index);
extern int MI_set_io_flag(int index, int value);

void OutputScan(void) {
    MI_set_io_flag(18, 19);  /* pet watchdog — inter-task check is meaningless in PC_WRAPPER */
    if (MI_get_io_flag(17) >= 99   /* PHONE_HOME set */
            && MI_get_io_flag(16) == 0  /* ERROR_COUNT = 0 */
            && MI_get_io_flag(19) == 1) /* ON_CONTROL = 1  */
        MI_set_io_flag(17, 0);
}

/* AML TMA logging — not needed */
void aml_write_tma_log_data(void *data, int len) { (void)data; (void)len; }

/* PC_WRAPPER task functions — no-op in headless single-threaded build */
void PCTasks_Sleep(unsigned long ms, int task)   { (void)ms; (void)task; }
void PCTasks_SuspendOutput(void)                 {}

/* memcpy_s — Windows secure memcpy, not in Linux glibc */
int memcpy_s(void *dest, size_t destSize, const void *src, size_t count) {
    if (!dest || !src || count > destSize) return 1;
    memcpy(dest, src, count);
    return 0;
}
