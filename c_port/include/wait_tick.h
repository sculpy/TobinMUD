/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_WAIT_TICK_H
#define TOBIN_WAIT_TICK_H

/* Decrements every playing character's wait_pulses by 1, once per pulse.
 * Register with pulse_register(1, wait_tick_run) from main.c at startup. */
void wait_tick_run(long pulse_num);

#endif
