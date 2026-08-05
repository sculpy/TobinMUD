/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
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

/* Combat music (user, 2026-08-05: "random fight music that will stop
 * when the fight is over") -- MSP `!!MUSIC(...)`. Registered via
 * pulse_register(COMBAT_ROUND_PULSES, combat_music_tick) alongside
 * combat_process_run() itself, same cadence. Rather than hooking every
 * one of the 30+ scattered `->fighting = ...` combat-entry sites across
 * the codebase (cmd_attack.c, cmd_flee.c, mob_ai.c, combat.c's own
 * defeat/flee paths, ...), this just sweeps every connected PC each
 * round and compares "are they fighting now" against "did we think
 * music was already playing for them" (descriptor_t.music_playing) --
 * one single place that only needs to know the two states, not every
 * path that can produce them. */
void combat_music_tick(long pulse_num);

/* Finds another playing character named `name` in the same room as `self`
 * (case-insensitive, DikuMUD-style). Shared by cmd_attack.c and cmd_kill.c
 * so both target the same way. Returns NULL if nobody matches. */
being_t *combat_find_room_target(being_t *self, const char *name);

/* PK opt-in gate (see combat.c's own doc comment) -- exported so any other
 * PC-vs-PC hostile action (Thief `plant`, cmd_plant.c) can reuse the exact
 * same mutual-consent rule combat itself uses, rather than inventing a
 * separate one. Mob targets always allowed; an immortal on either side
 * bypasses it. */
bool combat_pk_allowed(const being_t *self, const being_t *t);

/* The weapon-category item `attacker` is wielding (primary hand first,
 * then off-hand), or NULL if bare-handed -- exported so `deathstroke`
 * (cmd_deathstroke.c, level 20) can require a real wielded weapon the
 * same way combat_strike()'s own messaging/mods lookup does. `struct
 * obj *`, not `obj_t *` -- being.h only forward-declares `struct obj`
 * (avoids a being.h<->obj.h include cycle), so this header can't name
 * the typedef; callers that already have obj.h included (as
 * cmd_deathstroke.c does) can still assign the result to an `obj_t *`
 * freely, same type underneath. */
struct obj *combat_wielded_weapon(const being_t *attacker);

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

/* Fatal fall death (Sneezy → Tobin feature audit, "catfall/catleap"):
 * called from fall.c once a fall's own damage roll is unsurvivable.
 * Same "environmental death, no winner" shape as combat_drown_pc()
 * above -- see its doc comment for why this can't just wrap
 * combat_defeat(). No-op for anything but a PC. */
void combat_fall_kill_pc(being_t *victim);

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

/* Qualitative hit-intensity description (user 2026-07-12: "dont report
 * damage"; follow-up: "take out the damage number and use it to
 * describe how hard the hit was") -- ported from the real upstream's
 * own describe_dam() (misc/combat.cc). `capacity` should be the struck
 * limb's CURRENT (pre-hit) HP, not its max -- captured by the caller
 * BEFORE applying the damage (being_hurt_limb()/combat_apply_skill_
 * damage() zero it out). `verb` is only used to pick "into shreds" vs.
 * "into a bloody pulp" at 100%+ -- pass NULL (or anything other than
 * "slice"/"chop") for damage sources with no cutting/blunt distinction
 * (spells, traps, wand/staff use). Shared across combat.c/cmd_cast.c/
 * cmd_pray.c/cmd_move.c/cmd_use.c rather than reimplemented per file. */
const char *describe_dam(int dam, int capacity, const char *verb);

#endif
