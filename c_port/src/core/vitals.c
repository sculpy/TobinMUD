/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "vitals.h"

#include <stdbool.h>
#include <stdlib.h>

#include "affect.h"
#include "balance.h"
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
/* How many hunger/thirst points to drain this tick given a race upkeep
 * multiplier (race_balance.food_mult/drink_mult): the whole-number part
 * always drains, the fractional part drains probabilistically. 1.0 =>
 * exactly 1/tick (unchanged); 0.75 => 1 about three ticks in four; 1.5 =>
 * 1 or 2. Neutral for every race until balanced. */
static int upkeep_decay(float mult) {
    if (mult <= 0.0f)
        return 0;
    int whole = (int)mult;
    float frac = mult - (float)whole;
    if (frac > 0.0f && (rand() % 1000) < (int)(frac * 1000.0f))
        whole++;
    return whole;
}

static void vitals_tick_impl(long pulse_num, bool affect_players) {
    (void)pulse_num;
    if (!affect_players)
        return;

    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        being_t *b = d->character;
        if (!b || being_is_immortal(b))
            continue;

        const balance_mod_t *rb = race_balance_get(b->race);
        for (int n = upkeep_decay(rb->food_mult); n > 0 && b->progress.hunger > 0; n--)
            b->progress.hunger--;
        for (int n = upkeep_decay(rb->drink_mult); n > 0 && b->progress.thirst > 0; n--)
            b->progress.thirst--;

        /* (c) per-sector effects: hot/arid and hard-travel terrain burn
         * thirst/hunger faster, on top of the race-based drain above. In
         * spirit from the original's TerrainInfo thirst/hunger columns fed
         * to TBeing::foodNDrink() (obj/obj_food.cc): the baseline rate 2
         * every ordinary sector carries contributes nothing (over<=0), so
         * behaviour outside deserts/mountains is unchanged. Probability
         * rises ~15%/step above baseline -- a desert (thirst 6) sheds an
         * extra thirst point ~60% of ticks; mountains (hunger 4) an extra
         * food point ~30%. Guarded by the same >0 floor as the race drain. */
        if (b->base.roomp) {
            int sector = b->base.roomp->sector;
            int t_over = sector_thirst_rate(sector) - 2;
            int h_over = sector_hunger_rate(sector) - 2;
            if (t_over > 0 && b->progress.thirst > 0 && (rand() % 100) < t_over * 15)
                b->progress.thirst--;
            if (h_over > 0 && b->progress.hunger > 0 && (rand() % 100) < h_over * 15)
                b->progress.hunger--;
        }

        /* Tobin-original heat subsystem (user 2026-08-17) -- a deliberate
         * INVENTION, not a port (upstream defines TerrainInfo heat but no
         * engine code reads it; see room.h). Outdoors in a temperature
         * extreme, the sector bites once per drain tick: heatstroke past
         * HEAT_DAMAGE_HOT (deserts/lava), hypothermia at/below
         * HEAT_DAMAGE_COLD (deep arctic). A 1-HP chip, non-lethal and
         * floored at 1 -- the same convention starvation uses above.
         * ROOM_FLAG_INDOORS shelters entirely; the race heat/cold resist
         * roll (being_race_resists) is a per-tick save, so a heat-resistant
         * race can shrug off the desert sun. The milder STRESS band is a
         * cosmetic sweat/shiver cue in cmd_move.c, not a drain effect. */
        if (b->base.roomp && !(b->base.roomp->room_flag & ROOM_FLAG_INDOORS)) {
            int heat = sector_heat(b->base.roomp->sector);
            if (heat >= HEAT_DAMAGE_HOT && !being_race_resists(b, RESIST_HEAT)) {
                b->progress.hp--;
                if (b->progress.hp < 1)
                    b->progress.hp = 1;
                descriptor_send(d, "<R>The blistering heat sears your skin.<z>\r\n");
            } else if (heat <= HEAT_DAMAGE_COLD && !being_race_resists(b, RESIST_COLD)) {
                b->progress.hp--;
                if (b->progress.hp < 1)
                    b->progress.hp = 1;
                descriptor_send(d, "<C>The bitter cold bites deep into your bones.<z>\r\n");
            }
        }

        if (b->progress.hunger == 0 || b->progress.thirst == 0) {
            b->progress.hp--;
            if (b->progress.hp < 1)
                b->progress.hp = 1;
        }

        /* `alcoholism` (missing-skill audit batch C, 2026-08-09):
         * intoxication sobers up on its own over time, same shape
         * hunger/thirst decay above use. Real upstream has no single
         * fixed DRUNK decay rate either (it only ever falls via one-off
         * gainCondition(DRUNK,-1) calls scattered through disease.cc/
         * combat.c) -- a flat -2/tick here means a drink's real effect
         * is felt for a while without lingering indefinitely. */
        if (b->progress.drunk > 0) {
            b->progress.drunk -= 2;
            if (b->progress.drunk < 0)
                b->progress.drunk = 0;
        }

        /* Passing out from drink -- direct port of real upstream's own
         * TBeing::passOut() chance formula (periodic.cc), minus its
         * plotStat(CON)-based scaling (no equivalent stat-curve helper
         * in Tobin) -- a disclosed simplification, not an invented
         * number. Only mortals past the real threshold (drunk > 14)
         * roll; a hit knocks the character straight to sleep, same
         * "drugged unconscious" shape drug.c's own frogslime overdose
         * already uses. */
        if (b->progress.drunk > 14 && b->position != POSITION_SLEEPING) {
            int over = b->progress.drunk - 14;
            int chance = (int)(4.17 * over + 8.33);
            if (chance > 0 && (rand() % 100) < chance) {
                b->position = POSITION_SLEEPING;
                if (b->desc)
                    descriptor_notify(b->desc, "<y>The room spins -- you pass out from too much drink.<z>\r\n");
            }
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
            /* `snofalte` (Monk, missing-skill audit, 2026-08-09): real
             * upstream help text -- "causes bleeding to slow to a
             * trickle... lessens the damage of bleeding as well as
             * limits the amount of actual blood-loss... automatically
             * attempted whenever the practitioner is bleeding." Same
             * per-tick chip-reduction shape as `bandage`'s own
             * proficiency-scaled reduction just above -- stacks with it
             * (a Monk who both self-treats via Snofalte AND gets
             * bandaged reduces the chip twice, matching the "on top of
             * real medical attention" framing in the help text). */
            if (being_knows_skill(b, "snofalte")) {
                const skill_def_t *snofalte_sk = skill_find(b->char_class, "snofalte", false);
                if (snofalte_sk) {
                    int snofalte_prof = skill_learn_from_doing(b, snofalte_sk);
                    dmg -= dmg * (snofalte_prof / 2) / 100;
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

/* See vitals.h's own doc comment for the full formula rationale. */
void being_gain_drunk(being_t *ch, int raw_value) {
    if (!ch || being_is_immortal(ch) || raw_value == 0)
        return;

    int value = raw_value;
    if (value > 0 && being_knows_skill(ch, "alcoholism")) {
        const skill_def_t *sk = skill_find(ch->char_class, "alcoholism", false);
        if (sk) {
            int prof = skill_learn_from_doing(ch, sk);
            value = (int)((double)value * (double)(105 - prof) / 100.0);
        }
    }

    int drunk = ch->progress.drunk + value;
    ch->progress.drunk = drunk < 0 ? 0 : (drunk > 100 ? 100 : drunk);
}
