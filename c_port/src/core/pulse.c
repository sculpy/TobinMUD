/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "pulse.h"

#include "log.h"

#define MAX_PULSE_PROCESSES 64 /* was 8, then 16 (Session 43, gametime_tick),
                                  then 24 (trigger_pending_tick), then 32 --
                                  and 32 was already exceeded: main.c now
                                  registers 34 tick systems, so the last two
                                  (obj_plant_growth_tick / trophy_pulse_tick)
                                  were being silently dropped at boot
                                  (crops never grew/yielded, trophies never
                                  decayed) -- the overflow guard logs an
                                  error but doesn't stop the boot, so a whole
                                  tick system just quietly never fires until
                                  someone notices. Bumped to 64 (Session 157)
                                  for real headroom rather than chasing the
                                  count by +1/+2 every few sessions. */

typedef struct {
    int trigger_pulse;
    pulse_fn_t fn;
} pulse_process_t;

static pulse_process_t g_processes[MAX_PULSE_PROCESSES];
static int g_process_count = 0;

/* Adds a tick function to the scheduler: `fn` fires every `trigger_pulse`
 * pulses. Called once per subsystem at boot (main.c) to wire up things like
 * regen, planting, and trigger processing without a central switch statement. */
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

/* Called once per game pulse (main.c's heartbeat) to fire every registered
 * process whose trigger interval divides evenly into pulse_num. */
void pulse_scheduler_run(long pulse_num) {
    for (int i = 0; i < g_process_count; i++) {
        if (pulse_num % g_processes[i].trigger_pulse == 0)
            g_processes[i].fn(pulse_num);
    }
}
