/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>

#include "being.h"
#include "combat.h"
#include "skill.h"
#include "thing.h"

/* `know <creature>` -- the Know-X monster-lore cluster (spell/skill audit,
 * "Generic / cross-class": Know animal/demon/giantkin/other/people/reptile/
 * undead/veggie). In real Sneezy these are the consider-family SKILL_CONS_*
 * lore skills that deepen `consider`'s creature read-out (cmd_consider.cc).
 *
 * The reveal itself now lives in mob_lore_try_reveal() (mob_lore.c) so it
 * can also fire automatically off `consider`/`look` (user 2026-08-24: "fix
 * the know* skills to be automatic when you look at or consider the target
 * mob" -- matches real Sneezy, where these are folded into consider's own
 * read-out rather than being a separate command). This command remains as
 * an explicit, on-demand study that always spends the ~1-round wait and
 * gives an explicit refusal when you lack the matching lore skill, instead
 * of silently doing nothing the way the auto-trigger on consider/look does.
 *
 * One command auto-selects which of the eight lore skills applies from the
 * target's own race kingdom (mob_race_lore_category(), mob_lore.c) -- you
 * study a dragon with "know reptile", a lich with "know undead", and so on.
 * You must actually know that specific lore skill (immortals bypass), and
 * how MUCH the study reveals scales with your proficiency in it, mirroring
 * Sneezy's learnedness-gated reveal ladder (race always; then est. HP,
 * defenses, and disposition unlocking as proficiency climbs). Every use is
 * a learn-by-doing attempt on that skill. */

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

    if (!being_is_immortal(ch) && !being_knows_skill(ch, skname)) {
        char msg[160];
        snprintf(msg, sizeof(msg),
                 "You know nothing of %s -- you would need the '%s' skill.\r\n",
                 mob_lore_field_name(cat), skname);
        descriptor_send(d, msg);
        return true;
    }

    char out[512];
    size_t n = 0;
    mob_lore_try_reveal(ch, victim, true, out, sizeof(out), &n);
    descriptor_send(d, out);
    return true;
}
