/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "obj.h"
#include "thing.h"

/* `continue` (user 2026-07-12: "add a continue command so clerics that
 * heal <target> can continue automatically until the target is fully
 * healed or thier holy symbol breaks"). Repeats the caster's most
 * recent successful heal-type `pray` (being_t.last_heal_target/
 * last_heal_spell, set by cmd_pray.c) -- on the SAME target, self or
 * otherwise -- once per point of symbol strength on hand (2026-07-18:
 * symbols now genuinely decay, see cmd_pray.c's consume_symbol(), a
 * duplicated copy of the same helper -- so ONE symbol can power several
 * rounds here before it actually shatters, not strictly "one per
 * symbol" anymore), all within this single command call ("continue
 * automatically", not requiring the player to re-type it each tick),
 * stopping the moment any of:
 *
 *   1. the target has left the caster's room,
 *   2. the target is fully healed (hp >= max_hp),
 *   3. the caster is out of symbols entirely (no item left to decay),
 *   4. the last symbol on hand shatters this round ("their holy symbol
 *      breaks").
 *
 * CONTINUE_MAX_ROUNDS is a hard safety cap, not a gameplay limit -- in
 * practice a caster only carries a handful of holy symbols, so #3/#4
 * almost always end the loop first; the cap just prevents a runaway
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

/* Looks through everything `ch` is carrying, wearing, or holding for
 * an item whose name/keywords contain `keyword` -- used here to find
 * a holy symbol to keep consuming each round. Returns NULL if there
 * isn't one. */
static obj_t *find_keyword_item(const being_t *ch, const char *keyword) {
    size_t len = strlen(keyword);
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind == THING_OBJ && thing_name_matches(t->name, keyword, len))
            return (obj_t *)t;
    }
    return NULL;
}

/* Skips a leading inline color tag before capitalizing -- same
 * duplication precedent as cmd_pray.c/cmd_cast.c's own cap_first(). */
static const char *cap_first(const char *label, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%s", label);
    size_t i = 0;
    while (buf[i] == '<' && buf[i + 1] != '\0' && buf[i + 2] == '>')
        i += 3;
    if (buf[i])
        buf[i] = (char)toupper((unsigned char)buf[i]);
    return buf;
}

/* Spends 1-2 strength from `symbol` (real decay, not a clean counter --
 * see this file's header comment and cmd_pray.c's matching helper) --
 * shatters it only once that was the last of it, appending its own
 * message into the round's accumulated output buffer (this file sends
 * everything once at the end, not per round) instead of sending
 * immediately. Returns true if it shattered. */
static bool consume_symbol(obj_t *symbol, char *out, size_t outsz, size_t *n) {
    int strength = symbol->val[0] > 0 ? symbol->val[0] : 1;
    int decay = 1 + rand() % 2;
    if (strength > decay) {
        symbol->val[0] = strength - decay;
        return false;
    }
    char capbuf[128];
    const char *label = symbol->base.short_descr[0] ? symbol->base.short_descr : symbol->base.name;
    *n += (size_t)snprintf(out + *n, outsz - *n, "%s shatters from the stress of the prayer!\r\n",
                           cap_first(label, capbuf, sizeof(capbuf)));
    obj_destroy(symbol);
    return true;
}

/* Runs the `continue` command: keeps re-praying the caster's last
 * heal on whoever they were healing, one round per holy symbol, until
 * the target is fully healed, the caster runs out of symbols, or the
 * target wanders off. See this file's header comment for the full
 * rules. */
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
            n += (size_t)snprintf(out + n, sizeof(out) - n, "You have no holy symbol left. You cannot continue.\r\n");
            ch->last_heal_target = NULL;
            break;
        }

        int amount = 8 + ch->progress.level / 2;
        being_heal(target, amount);

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

        if (consume_symbol(symbol, out, sizeof(out), &n)) {
            ch->last_heal_target = NULL;
            break;
        }
    }

    if (rounds >= CONTINUE_MAX_ROUNDS)
        n += (size_t)snprintf(out + n, sizeof(out) - n, "You pause, out of breath.\r\n");

    descriptor_send(d, out);
    return true;
}
