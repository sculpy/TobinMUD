/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_AFFECT_H
#define TOBIN_AFFECT_H

#include <stdbool.h>

/* Affects system (buffs/debuffs/status) -- user 2026-07-11's backlog
 * ("Affects system (buffs/debuffs/status)"), self-sequenced after trap
 * mechanics. A small, fixed-size set of TIMED status effects a being
 * can be carrying at once, each just a type + how many combat rounds
 * (COMBAT_ROUND_PULSES, ~1.2s each) it has left. Decremented by
 * affect_tick_run() (a pulse callback, see main.c), which runs for
 * EVERY connected being regardless of whether they're currently
 * fighting -- a buff shouldn't only wear off while you're in combat.
 *
 * v1 scope: one real, working effect (AFFECT_SANCTUARY, wired to the
 * Cleric's "sanctuary" spell -- see cmd_pray.c/combat.c) to prove the
 * system end to end, rather than speculatively building mechanics for
 * every buff-flavored entry in skill.c's roster (same "one concrete
 * example over building everything at once" precedent as weapon depth/
 * trap mechanics). Add more affect_type_t values here as more skills/
 * spells get real mechanics. */

struct being; /* forward declare -- avoids a being.h <-> affect.h cycle,
               * since being.h needs active_affect_t for its own field */

typedef enum {
    AFFECT_NONE = 0,
    /* Halves incoming damage in combat_strike() -- the Cleric spell
     * "sanctuary" ("A strong aura that reduces incoming damage."). */
    AFFECT_SANCTUARY,
    AFFECT_COUNT,
} affect_type_t;

#define MAX_ACTIVE_AFFECTS 4

typedef struct {
    affect_type_t type; /* AFFECT_NONE = this slot is empty */
    int rounds_left;
} active_affect_t;

/* Display name for an affect type ("Sanctuary", ...) -- used by the
 * `affects` command and affect-expiry messages. */
const char *affect_name(affect_type_t type);

/* Whether `b` currently has `type` active. */
bool being_has_affect(const struct being *b, affect_type_t type);

/* Starts (or refreshes, if already active) `type` on `b` for `rounds`
 * more rounds. Silently does nothing if `b`'s affect slots are all full
 * with OTHER affect types (a deliberately small, fixed cap -- see
 * MAX_ACTIVE_AFFECTS). */
void being_apply_affect(struct being *b, affect_type_t type, int rounds);

/* Removes `type` from `b` immediately, if present (no message -- callers
 * that want one print it themselves, e.g. on manual dispel). */
void being_remove_affect(struct being *b, affect_type_t type);

/* Pulse callback (registered in main.c, once every COMBAT_ROUND_PULSES):
 * counts every connected being's active affects down by one round,
 * clearing (and announcing the expiry of) any that hit zero. */
void affect_tick_run(long pulse_num);

#endif
