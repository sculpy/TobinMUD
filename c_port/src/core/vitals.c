/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "vitals.h"

#include <stdbool.h>
#include <stdlib.h>

#include "affect.h"
#include "being.h"
#include "combat.h"
#include "descriptor.h"
#include "player_repo.h"
#include "room.h"
#include "skill.h"

/* Periodic hook (registered with the pulse scheduler) that drains hunger
 * and thirst for every connected, non-immortal player, applies starvation/
 * dehydration chip damage once either hits zero, rolls underwater drowning
 * damage (routed through combat_drown_pc() since this one can actually
 * kill), and persists the resulting progress. */
static void vitals_tick_impl(long pulse_num, bool affect_players) {
    (void)pulse_num;
    if (!affect_players)
        return;

    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        being_t *b = d->character;
        if (!b || being_is_immortal(b))
            continue;

        if (b->progress.hunger > 0)
            b->progress.hunger--;
        if (b->progress.thirst > 0)
            b->progress.thirst--;

        if (b->progress.hunger == 0 || b->progress.thirst == 0) {
            b->progress.hp--;
            if (b->progress.hp < 1)
                b->progress.hp = 1;
        }

        /* Drowning (Sneezy → Tobin feature audit, "Water, drowning,
         * flight"): the original's procCharDrowning deals 1d10 to a PC
         * underwater without AFF_WATERBREATH every 3.6 real seconds via
         * checkDrowning()/reconcileDamage() -- genuinely lethal. Same
         * 1d10 roll here, just on this tick's own slower ~60s cadence
         * instead, so it's already far gentler in practice without
         * softening the roll itself. AFFECT_FLYING also exempts (same
         * "flight bypasses drowning" rule the original's canFly() ->
         * checkFalling() chain uses). Unlike hunger/thirst above, this
         * one CAN kill (user, AskUserQuestion) -- routed through
         * combat_drown_pc() (combat.c) rather than clamped at 1 HP. */
        if (b->base.roomp && sector_is_underwater(b->base.roomp->sector)
            && !being_has_affect(b, AFFECT_WATERBREATH)
            && !being_has_affect(b, AFFECT_FLYING)) {
            int dmg = 1 + rand() % 10;

            /* `swim` (docs/Spell Assignments.xlsx gap audit, 2026-08-08)
             * -- real upstream mitigates drowning damage (physics.cc);
             * this unconditional 1d10 tick had nothing reducing it
             * before. Up to half off at 100% proficiency, trained on
             * every roll (win or lose) same as every other passive in
             * this audit. */
            if (!being_is_immortal(b) && being_knows_skill(b, "swim")) {
                const skill_def_t *swim_sk = skill_find(b->char_class, "swim", false);
                if (swim_sk) {
                    int swim_prof = skill_learn_from_doing(b, swim_sk);
                    dmg -= dmg * (swim_prof / 2) / 100;
                    if (dmg < 1)
                        dmg = 1;
                }
            }

            b->progress.hp -= dmg;
            if (b->progress.hp <= 0) {
                combat_drown_pc(b);
                continue; /* `b` is freed -- do not touch it or save below */
            }
            descriptor_send(d, "<r>You struggle against the water, your lungs burning!<z>\r\n");
        }

        /* `bandage` (docs/Spell Assignments.xlsx gap audit, 2026-08-08):
         * a limb marked `bleeding` (combat.c, set the same moment it
         * spawns a blood pool) chips a small amount of HP per tick until
         * treated. Non-lethal, floored at 1 -- same convention hunger/
         * thirst above use, deliberately NOT drowning's lethal shape
         * (drowning's own lethality was a real, explicit user decision;
         * this is a new passive tick with no such ask, so the safe
         * default applies). `bandage`'s own skill proficiency reduces
         * the chip, same shape `swim` uses for drowning damage. */
        int bleeding_limbs = 0;
        for (int i = 0; i < LIMB_COUNT; i++)
            if (b->limbs[i].bleeding)
                bleeding_limbs++;
        if (bleeding_limbs > 0) {
            int dmg = bleeding_limbs;
            if (being_knows_skill(b, "bandage")) {
                const skill_def_t *bandage_sk = skill_find(b->char_class, "bandage", false);
                if (bandage_sk) {
                    int bandage_prof = skill_learn_from_doing(b, bandage_sk);
                    dmg -= dmg * (bandage_prof / 2) / 100;
                }
            }
            if (dmg < 1)
                dmg = 1;
            b->progress.hp -= dmg;
            if (b->progress.hp < 1)
                b->progress.hp = 1;
            descriptor_send(d, "You wince as your wounds bleed.\r\n");
        }

        player_progress_save(b->player_id, &b->progress);
    }
}

void vitals_tick_run(long pulse_num) {
    vitals_tick_impl(pulse_num, true);
}

/* See vitals.h's doc comment -- `aitick`'s forced-tick variant, no-op by
 * design (a real live incident: 1000 forced vitals ticks silently
 * starved and nearly killed a bystander player). */
void vitals_tick_force_world_only(long pulse_num) {
    vitals_tick_impl(pulse_num, false);
}
