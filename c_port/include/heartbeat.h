/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_HEARTBEAT_H
#define TOBIN_HEARTBEAT_H

/* Pulse callback (main.c): every hour, on the half hour (real wall-clock
 * time, NOT the fictional mud clock -- see gametime.h for that), sends a
 * single blank line to every connection -- no text, no [TAG], nothing to
 * read -- so a "tick" is visible (the terminal shows something moved) with
 * no actual message (user: "every hour on the half hour send a blank line
 * of uinput to the game so a tick becomes apparent to the player without
 * any messages"). */
void heartbeat_tick(long pulse_num);

#endif
