/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "pulse.h"

#include "log.h"

#define MAX_PULSE_PROCESSES 16 /* was 8 -- bumped Session 43 when gametime_tick
                                  filled the last slot. */

typedef struct {
    int trigger_pulse;
    pulse_fn_t fn;
} pulse_process_t;

static pulse_process_t g_processes[MAX_PULSE_PROCESSES];
static int g_process_count = 0;

void pulse_register(int trigger_pulse, pulse_fn_t fn) {
    if (g_process_count >= MAX_PULSE_PROCESSES) {
        /* Previously a silent no-op -- a registration past the cap would
         * just vanish with no error, discovered the hard way once
         * gametime_tick filled the 8th of 8 slots. Now at least logged. */
        log_error("pulse_register: MAX_PULSE_PROCESSES (%d) exceeded, dropping a registration.",
                  MAX_PULSE_PROCESSES);
        return;
    }
    if (trigger_pulse <= 0 || !fn)
        return;
    g_processes[g_process_count].trigger_pulse = trigger_pulse;
    g_processes[g_process_count].fn = fn;
    g_process_count++;
}

void pulse_scheduler_run(long pulse_num) {
    for (int i = 0; i < g_process_count; i++) {
        if (pulse_num % g_processes[i].trigger_pulse == 0)
            g_processes[i].fn(pulse_num);
    }
}
