#ifndef TOBIN_FALL_H
#define TOBIN_FALL_H

struct being;

/* Falling (Sneezy → Tobin feature audit, "catfall/catleap"). Checked the
 * real upstream first: TBeing::checkFalling() (misc/physics.cc) drops a
 * being through consecutive DIR_DOWN-linked rooms while each is a "fall"
 * sector (open air, no floor), landing tier decided by how many rooms
 * were fallen through (`count`) against two thresholds -- num1 (max
 * survivable depth, 10 with catfall/feathery descent else 5) and num2
 * (num1-2, the "crush roll" band below outright death). Below num2, an
 * agility save can land you unhurt; between num2 and num1, a CON-scaled
 * roll decides a crushing landing (leg-break + heavy damage) vs. death;
 * beyond num1, it's unconditionally fatal. catfall halves whatever
 * damage lands; water sectors soften it further (a splash beats a
 * sidewalk). Ported with the same behavior shape, adapted to Tobin's
 * real primitives where the original's don't exist:
 *   - SPELL_FEATHERY_DESCENT (a spell) isn't ported -- Tobin's roster
 *     has no equivalent -- so only SKILL_CATFALL gates the better
 *     threshold/halving here.
 *   - getConShock()/isAgile() have no Tobin equivalent -- replaced with
 *     a flat CON/DEX-scaled percentage roll each, same spirit (a more
 *     Constitution/Dexterity-favored character survives worse falls
 *     more often) without porting the exact original formula.
 *   - break_bone() -> being_hurt_limb() on both legs (Tobin's own limb-
 *     injury system, not a separate broken-bone affect).
 *   - Mount/rider dismounting during a fall isn't ported -- Tobin's own
 *     Mount/riding system already exempts a MOUNTED position from ever
 *     entering a fall sector via normal movement (cmd_move.c's own
 *     terrain-cost logic), so the scenario checkFalling() guards
 *     against there doesn't arise the same way here.
 *   - Immortals don't fall at all (exempt outright), rather than the
 *     original's own "bounces like rubber" gag landing -- consistent
 *     with every other environmental hazard in Tobin (drowning,
 *     hunger/thirst) already exempting immortals rather than giving
 *     them a special no-op path through the same code. */

/* Called after any successful move (cmd_move.c) lands `b` in a room --
 * no-op unless that room is a real fall sector (room.h's
 * sector_is_fall()) and `b` isn't flying/immortal. May move `b` through
 * one or more further rooms (each additional DIR_DOWN landing that's
 * ALSO a fall sector) before resolving the actual landing -- same
 * "keep falling until you hit something" shape the real upstream uses.
 * May destroy `b` via combat_fall_kill_pc() on a fatal landing -- the
 * caller must not touch `b` again after calling this if it was a PC (a
 * mob doesn't reach this at all yet, see the .c file). */
void fall_check(struct being *b);

#endif
