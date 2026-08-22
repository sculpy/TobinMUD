/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "being.h"
#include "combat.h"
#include "skill.h"
#include "thing.h"

/* `know <creature>` -- the Know-X monster-lore cluster (spell/skill audit,
 * "Generic / cross-class": Know animal/demon/giantkin/other/people/reptile/
 * undead/veggie). In real Sneezy these are the consider-family SKILL_CONS_*
 * lore skills that deepen `consider`'s creature read-out (cmd_consider.cc);
 * Tobin's `consider` was ported without them (see cmd_consider.c's header),
 * so this exposes them as their own command instead of bolting eight
 * conditional blocks onto consider.
 *
 * One command auto-selects which of the eight lore skills applies from the
 * target's own race kingdom (mob_race_lore_category(), mob_lore.c) -- you
 * study a dragon with "know reptile", a lich with "know undead", and so on.
 * You must actually know that specific lore skill (immortals bypass), and
 * how MUCH the study reveals scales with your proficiency in it, mirroring
 * Sneezy's learnedness-gated reveal ladder (race always; then est. HP,
 * defenses, and disposition unlocking as proficiency climbs). Every use is
 * a learn-by-doing attempt on that skill. */

static const char *hp_ratio_word(int mob_max, int self_max) {
    if (self_max <= 0)
        self_max = 1;
    /* mob HP as a multiple of the studier's own -- descriptive, never a
     * raw number, same spirit as Sneezy's DescRatio(). */
    int pct = (mob_max * 100) / self_max;
    if (pct >= 400) return "vastly greater than your own";
    if (pct >= 200) return "far greater than your own";
    if (pct >= 130) return "greater than your own";
    if (pct >= 80)  return "about the same as your own";
    if (pct >= 45)  return "less than your own";
    return "far less than your own";
}

static const char *ac_word(int ac) {
    if (ac >= 40) return "all but impenetrable";
    if (ac >= 25) return "very well protected";
    if (ac >= 12) return "well protected";
    if (ac >= 5)  return "lightly protected";
    if (ac > 0)   return "poorly protected";
    return "unarmored";
}

bool cmd_know(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Study your knowledge of what creature?\r\n");
        return true;
    }

    being_t *victim = combat_find_room_target(ch, raw);
    if (!victim) {
        descriptor_send(d, "You don't see that creature here.\r\n");
        return true;
    }
    if (victim == ch) {
        descriptor_send(d, "You know yourself well enough already.\r\n");
        return true;
    }
    if (victim->base.kind == THING_PC) {
        descriptor_send(d, "Monster lore is for monsters -- ask them yourself.\r\n");
        return true;
    }

    mob_lore_t cat = mob_race_lore_category(victim->mob_race);
    const char *skname = mob_lore_skill_name(cat);
    bool imm = being_is_immortal(ch);

    if (!imm && !being_knows_skill(ch, skname)) {
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "You know nothing of %s -- you would need the '%s' skill.\r\n",
                 mob_lore_field_name(cat), skname);
        descriptor_send(d, msg);
        return true;
    }

    /* Learn-by-doing + proficiency read. Immortals read at full mastery. */
    int learn = 100;
    if (!imm) {
        const skill_def_t *sk = skill_find(ch->char_class, skname, false);
        if (sk) {
            learn = skill_learn_from_doing(ch, sk);
            if (learn < skill_proficiency(ch, sk))
                learn = skill_proficiency(ch, sk);
        }
        being_set_wait(ch, 12); /* ~1 combat round of study, like consider */
    }

    /* Nice-cased race name. */
    char race[48];
    snprintf(race, sizeof(race), "%s", mob_race_name(victim->mob_race));
    for (char *p = race; *p; p++)
        *p = (char)tolower((unsigned char)*p);
    const char *art = strchr("aeiou", race[0]) ? "an" : "a";
    const char *vname = victim->base.short_descr[0] ? victim->base.short_descr
                                                    : victim->base.name;

    char out[512];
    int n = snprintf(out, sizeof(out),
                     "<g>Drawing on your knowledge of %s, you study %s.<1>\r\n"
                     "It is %s %s.\r\n",
                     mob_lore_field_name(cat), vname, art, race);

    /* Reveal ladder, gated by proficiency (Sneezy's learnedness tiers). */
    if (learn > 20)
        n += snprintf(out + n, sizeof(out) - n,
                      "<c>Vitality:<1> its constitution seems %s.\r\n",
                      hp_ratio_word(victim->progress.max_hp, ch->progress.max_hp));
    if (learn > 45)
        n += snprintf(out + n, sizeof(out) - n,
                      "<c>Defenses:<1> it appears %s.\r\n",
                      ac_word(being_total_ac(victim)));
    if (learn > 70) {
        const char *disp = victim->mob_align > 200 ? "benevolent"
                         : victim->mob_align < -200 ? "malevolent"
                         : "indifferent";
        n += snprintf(out + n, sizeof(out) - n,
                      "<c>Disposition:<1> it regards the world as %s.\r\n", disp);
    }
    if (!imm && learn <= 20)
        n += snprintf(out + n, sizeof(out) - n,
                      "Deeper study will come with practice.\r\n");
    (void)n;
    descriptor_send(d, out);
    return true;
}
