/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>

#include "obj.h"
#include "thing.h"

/* `continue` (user 2026-07-12: "add a continue command so clerics that
 * heal <target> can continue automatically until the target is fully
 * healed or thier holy symbol breaks"). Repeats the caster's most
 * recent successful heal-type `pray` (being_t.last_heal_target/
 * last_heal_spell, set by cmd_pray.c) -- on the SAME target, self or
 * otherwise -- once per holy symbol on hand, all within this single
 * command call ("continue automatically", not requiring the player to
 * re-type it each tick), stopping the moment any of:
 *
 *   1. the target has left the caster's room,
 *   2. the target is fully healed (hp >= max_hp),
 *   3. the caster is out of holy symbols ("their holy symbol breaks").
 *
 * CONTINUE_MAX_ROUNDS is a hard safety cap, not a gameplay limit -- in
 * practice a caster only carries a handful of holy symbols, so #3
 * almost always ends the loop first; the cap just prevents a runaway
 * loop/output flood if someone stockpiles a huge pile of symbols.
 *
 * Deliberately does NOT re-validate class/level/discipline-percentage
 * each round -- those already passed when cmd_pray.c set
 * last_heal_target/last_heal_spell for this exact prayer, and `continue`
 * is just replaying that same prayer's effect + consuming its symbol,
 * not casting anything new. The heal-amount formula and per-round
 * messaging are duplicated from cmd_pray.c's pray_apply_heal() rather
 * than shared, matching this codebase's established "small helpers get
 * duplicated per command file" convention (see cmd_cast.c/cmd_pray.c's
 * own duplicated ci_contains()/find_keyword_item()). */

#define CONTINUE_MAX_ROUNDS 50

static obj_t *find_keyword_item(const being_t *ch, const char *keyword) {
    size_t len = strlen(keyword);
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind == THING_OBJ && thing_name_matches(t->name, keyword, len))
            return (obj_t *)t;
    }
    return NULL;
}

bool cmd_continue(descriptor_t *d, const char *args) {
    (void)args;
    being_t *ch = d->character;
    if (!ch)
        return true;

    if (!ch->last_heal_target) {
        descriptor_send(d, "You aren't healing anyone to continue.\r\n");
        return true;
    }

    char out[4096];
    size_t n = 0;
    int rounds = 0;

    while (rounds < CONTINUE_MAX_ROUNDS && n < sizeof(out) - 200) {
        being_t *target = ch->last_heal_target;
        char capbuf[128];

        if (!target || target->base.roomp != ch->base.roomp) {
            n += (size_t)snprintf(out + n, sizeof(out) - n, "Your target is no longer here.\r\n");
            ch->last_heal_target = NULL;
            break;
        }
        if (target->progress.hp >= target->progress.max_hp) {
            if (target == ch)
                n += (size_t)snprintf(out + n, sizeof(out) - n, "You are fully healed.\r\n");
            else
                n += (size_t)snprintf(out + n, sizeof(out) - n, "%s is fully healed.\r\n",
                                      being_display_name_cap(target, capbuf, sizeof(capbuf)));
            ch->last_heal_target = NULL;
            break;
        }

        obj_t *symbol = find_keyword_item(ch, "symbol");
        if (!symbol) {
            n += (size_t)snprintf(out + n, sizeof(out) - n, "Your holy symbol breaks! You cannot continue.\r\n");
            ch->last_heal_target = NULL;
            break;
        }

        int amount = 8 + ch->progress.level / 2;
        being_heal(target, amount);
        obj_destroy(symbol);

        if (target == ch) {
            n += (size_t)snprintf(out + n, sizeof(out) - n,
                                  "You pray for %s and feel restored! (+%d HP)\r\n", ch->last_heal_spell, amount);
        } else {
            n += (size_t)snprintf(out + n, sizeof(out) - n,
                                  "You pray for %s, and %s is restored! (+%d HP)\r\n",
                                  ch->last_heal_spell, being_display_name(target), amount);
            if (target->desc) {
                char tcapbuf[128], msg[192];
                snprintf(msg, sizeof(msg), "%s prays for %s, restoring you! (+%d HP)\r\n",
                         being_display_name_cap(ch, tcapbuf, sizeof(tcapbuf)), ch->last_heal_spell, amount);
                descriptor_notify(target->desc, msg);
            }
        }
        rounds++;
    }

    if (rounds >= CONTINUE_MAX_ROUNDS)
        n += (size_t)snprintf(out + n, sizeof(out) - n, "You pause, out of breath.\r\n");

    descriptor_send(d, out);
    return true;
}
