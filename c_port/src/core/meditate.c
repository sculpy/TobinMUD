/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "meditate.h"

#include "affect.h"
#include "being.h"
#include "descriptor.h"
#include "skill.h"

/* Periodic hook that advances every connected, currently-meditating
 * character by one heal roll -- see being.h's `meditating` doc comment
 * and cmd_yoginsa.c for where the task starts/stops on command. */
void meditate_tick_run(long pulse_num) {
    (void)pulse_num;

    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        being_t *ch = d->character;
        if (!ch || !ch->meditating)
            continue;

        /* Starting meditation (cmd_yoginsa.c/cmd_meditate.c/
         * cmd_position.c's auto_start_meditating()) sets position to
         * POSITION_MEDITATE itself, so any OTHER position by the time
         * this tick runs means something external knocked it out from
         * under them (forced to stand, knocked down, ...). */
        if (ch->position != POSITION_MEDITATE) {
            ch->meditating = false;
            descriptor_send(d, "Your meditation is broken.\r\n");
            continue;
        }
        if (ch->fighting) {
            ch->meditating = false;
            descriptor_send(d, "Your meditation is broken -- you're fighting!\r\n");
            continue;
        }

        /* Resource-aware (user 2026-08-06: "meditate sits a character
         * down and meditates back to his max mana" -- `meditate`
         * (cmd_meditate.c, Mage/Druid) and `yoginsa` (cmd_yoginsa.c,
         * Monk) both just toggle this same `meditating` flag and land
         * here; which SKILL a being actually knows decides which real
         * resource this tick restores -- Mana for a `meditate`-knowing
         * Mage (being_calc_max_mana() is Mage-only, see its own doc
         * comment for why not Druid), HP/Vitality for anyone else
         * (Monk's `yoginsa`, or a Druid whose own `meditate` entry has
         * no numeric resource to restore yet either -- same disclosed
         * Lifeforce gap as before). An immortal always succeeds and
         * always gets the HP/Vitality path (no skill to check against). */
        bool imm = being_is_immortal(ch);
        /* Class alone decides the resource, same as being_calc_max_mana()
         * itself (no imm check there either) -- an immortal playing a
         * Mage still has a real mana pool and should meditate it back
         * up, not silently fall through to the HP/Vitality path just
         * because they're immune to the skill-roll/being_knows_skill()
         * gate below. Found live (2026-08-06): an immortal Mage's
         * meditate ended instantly claiming "fully rested" because it
         * was checking already-full HP/Vit instead of their actual
         * (not-full) mana. */
        bool mana_mode = ch->char_class == CLASS_MAGE || ch->char_class == CLASS_DRUID; /* Druid Lifeforce mirrors mana (user 2026-08-10) */
        const skill_def_t *sk = skill_find(ch->char_class, mana_mode ? "meditate" : "yoginsa", imm);
        bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));
        if (!success) {
            descriptor_send(d, "You try to meditate, but your mind won't settle.\r\n");
            continue;
        }

        /* Snapshot "was this being actually short on the resource before
         * this tick's heal?" (regen.c's own was_short_hp/was_short_vit
         * pattern) -- needed below so the auto-stand-when-topped-off
         * logic only fires on the tick that ACTUALLY finishes healing
         * them, not on every tick while they happen to already be full.
         * Found live (groundfighting smoke test, 2026-08-09): a Monk
         * who sits down already at full HP/Vitality knows `yoginsa`
         * (auto_start_meditating(), cmd_position.c) so meditation starts
         * immediately -- without this guard, topped_off was true on
         * that very first tick and stood them right back up, so `sit`
         * never actually stuck for a healthy Monk. */
        bool was_short = mana_mode
            ? (ch->progress.mana < ch->progress.max_mana)
            : (ch->progress.hp < ch->progress.max_hp || ch->progress.vit < ch->progress.max_vit);

        int heal = 5 + ch->progress.level / 2;
        if (mana_mode) {
            being_heal_mana(ch, heal);
            descriptor_send(d, "<G>Meditating focuses your mind!<z>\r\n");
        } else {
            being_heal(ch, heal);
            being_heal_vit(ch, heal);
            descriptor_send(d, "<G>Meditating focuses your inner harmonies!<z>\r\n");
        }

        /* `wohlin meditation` (Monk, level 25, level-25 audit batch:
         * "While meditating, unlocks bonus self-cure effects."). This is
         * the "chained secondary cures (self-salve, cure poison,
         * sterilize, cure disease)" cmd_yoginsa.c's own doc comment
         * flagged as blocked on this skill not existing yet -- it now
         * does. Scoped to poison + every disease (Tobin's real cure
         * targets, same affect.h range this whole audit already reuses
         * for `cure poison`/`cure disease`), rolled once per meditation
         * tick alongside the HP/Vitality heal above, not a separate
         * command. */
        if (being_knows_skill(ch, "wohlin meditation")) {
            bool cured_something = false;
            if (being_has_affect(ch, AFFECT_POISON)) {
                being_remove_affect(ch, AFFECT_POISON);
                cured_something = true;
            }
            for (int i = 0; i < MAX_ACTIVE_AFFECTS; i++) {
                if (affect_is_disease(ch->affects[i].type)) {
                    being_remove_affect(ch, ch->affects[i].type);
                    cured_something = true;
                }
            }
            if (cured_something)
                descriptor_send(d, "<G>Your meditation burns away poison and sickness alike!<z>\r\n");
        }

        /* User 2026-08-03: "when completely rested or when yoginsa or
         * meditate gets you to full you should automatically stand" --
         * same "full HP and vitality -> back on your feet" rule
         * regen.c's own plain-rest version uses, applied here for the
         * meditation/yoginsa path specifically. Also ends the
         * meditation itself -- nothing left to meditate for once fully
         * healed, same as any other meditation-breaking condition
         * above (position change, fighting). Checked last so this
         * tick's own wohlin cure (just above) still applies before
         * standing up. */
        bool topped_off = was_short && (mana_mode
            ? (ch->progress.mana >= ch->progress.max_mana)
            : (ch->progress.hp >= ch->progress.max_hp && ch->progress.vit >= ch->progress.max_vit));
        if (topped_off) {
            ch->meditating = false;
            ch->position = POSITION_STANDING;
            descriptor_send(d, "You feel completely focused and stand up.\r\n");
        }
    }
}
