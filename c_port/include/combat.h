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

/* Admin/debug tool (`hurtlimb`, cmd_hurtlimb.c): sets `target`'s `limb` HP
 * directly to `hp` (clamped to [0, that limb's current max]) and runs the
 * exact same injury-tier-crossing/sever/decapitate logic a normal
 * combat_strike() hit would -- lets injury-tier messages, severing, and
 * decapitation all be tested deterministically instead of waiting on combat
 * RNG to land on a specific limb. `actor` (the immortal typing the command)
 * is credited as the attacker in any resulting messages/kill. Returns true
 * iff this decapitated (and thus killed via combat_defeat()) `target`. */
bool combat_debug_set_limb_hp(being_t *actor, being_t *target, limb_t limb, int hp);

/* Drowning death (Sneezy → Tobin feature audit, "Water, drowning,
 * flight"): called from vitals_tick_run() (vitals.c) when its own
 * underwater-without-water-breathing damage roll drops `victim` to 0
 * HP or below. An environmental death, not a kill -- no `winner`, so
 * this is NOT a thin wrapper around combat_defeat(); see its own doc
 * comment in combat.c for the full rationale. No-op for anything but a
 * PC (mobs don't drown -- they have no vitals tick at all yet). */
void combat_drown_pc(being_t *victim);

/* Shared "deal skill-combat damage, then handle defeat" pipeline for
 * bash/kick (cmd_bash.c/cmd_kick.c, Skill-based combat, Sneezy → Tobin
 * feature audit) -- these are EXTRA player-triggered actions layered on
 * top of the automatic per-round exchange (combat_process_run()), not a
 * replacement for it, so they need the same "damage might finish the
 * fight" handling combat_process_run() already does for a normal swing,
 * just without the full weapon/crit/decapitation machinery
 * combat_strike() runs (scoped down on purpose -- see cmd_bash.c's own
 * header comment). Zeroes damage against an immortal defender, same
 * immunity rule combat_strike() enforces. Returns true iff this defeated
 * (and for a mob, DESTROYED -- being_destroy() frees it) `defender` --
 * the caller MUST NOT touch `defender` again if so. */
bool combat_apply_skill_damage(being_t *attacker, being_t *defender, int dmg, limb_t limb);

#endif
