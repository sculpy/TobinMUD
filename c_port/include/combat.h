/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_COMBAT_H
#define TOBIN_COMBAT_H

#include "being.h"

/* C replacement for the shape of misc/combat.cc's perform_violence() /
 * TBeing::hit() -- trimmed to player-vs-player only (no NPCs/mobs exist
 * yet in Tobin). Registered via pulse_register(COMBAT_ROUND_PULSES,
 * combat_process_run) from main.c; resolves one exchange (attacker strike,
 * then defender retaliation if still standing) for every actively-fighting
 * pair, once per combat round. */
void combat_process_run(long pulse_num);

/* Finds another playing character named `name` in the same room as `self`
 * (case-insensitive, DikuMUD-style). Shared by cmd_attack.c and cmd_kill.c
 * so both target the same way. Returns NULL if nobody matches. */
being_t *combat_find_room_target(being_t *self, const char *name);

/* Immortal-only instant kill (see misc/offense.cc's doKill()/POWER_SLAY in
 * the original -- there it's gated by a wiz-power flag Tobin doesn't have;
 * here it's gated by being_is_immortal() at the call site in cmd_kill.c).
 * Sets target's HP/limbs to 0 and resolves defeat immediately, bypassing
 * the normal multi-round combat_process_run() entirely -- no `fighting`
 * state or wait cost involved. */
void combat_instakill(being_t *attacker, being_t *target);

#endif
