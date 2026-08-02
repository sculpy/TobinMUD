/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>

#include "affect.h"
#include "combat.h"
#include "pulse.h"
#include "thing.h"

/* `assist <groupmate>` (TODO.md priority item, user 2026-07-30: "Assist
 * command allowing followers to assist groupmates in combat"). Checked
 * combat_process_run()'s existing pet-auto-assist mechanic first (its
 * own doc comment, combat.c): it already proves Tobin's pairwise
 * `fighting` pointer supports a genuine multi-attacker pile-on WITHOUT
 * any special-casing -- the per-descriptor loop keys purely off each
 * PC's own `a->fighting`, never cross-checking that the target's own
 * `fighting` points back, so a second (or third...) attacker can set
 * their OWN `fighting` at an already-engaged target and the loop
 * resolves a completely independent exchange for that pair every round.
 * `assist` is therefore just "attack whoever this named groupmate is
 * currently fighting" -- the target's own `fighting` pointer is
 * deliberately left untouched (same one-sided precedent the pet
 * auto-assist mechanic already established: it draws its own return
 * fire without displacing who the target is "primarily" paired
 * against). Gated on `being_in_group()` (not plain `follow`) -- matching
 * the user's own "assist GROUPMATES" wording and `gtell`'s identical
 * membership choice, see cmd_gtell.c. */
bool cmd_assist(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!*args) {
        descriptor_send(d, "Assist whom?\r\n");
        return true;
    }
    if (ch->position == POSITION_SLEEPING) {
        descriptor_send(d, "You can't fight in your sleep!\r\n");
        return true;
    }
    if (being_has_affect(ch, AFFECT_FEAR)) {
        descriptor_send(d, "You're too afraid to fight!\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Assist whom?\r\n");
        return true;
    }
    const char *tok;
    int ordinal = thing_parse_ordinal(raw, &tok);
    size_t len = strlen(tok);

    being_t *ally = NULL;
    int seen = 0;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if ((t->kind != THING_PC && t->kind != THING_MOB) || t == &ch->base)
            continue;
        if (thing_name_matches(t->name, tok, len)) {
            seen++;
            if (seen == ordinal) {
                ally = (being_t *)t;
                break;
            }
        }
    }
    if (!ally) {
        descriptor_send(d, "They aren't here.\r\n");
        return true;
    }
    if (!being_in_group(ch, ally)) {
        descriptor_send(d, "You can only assist a member of your own group.\r\n");
        return true;
    }
    if (!ally->fighting) {
        descriptor_send(d, "They aren't fighting anyone.\r\n");
        return true;
    }

    being_t *target = ally->fighting;
    if (ch->position != POSITION_STANDING && ch->position != POSITION_MOUNTED) {
        ch->position = POSITION_STANDING;
        descriptor_send(d, "You scramble to your feet.\r\n");
    }

    ch->feigning = false;
    ch->fighting = target;
    ch->sneaking = false;
    being_set_wait(ch, COMBAT_ROUND_PULSES);

    char msg[160];
    snprintf(msg, sizeof(msg), "You wade in to help %s, attacking %s!\r\n",
             being_display_name(ally), being_display_name(target));
    descriptor_send(d, msg);
    if (target->desc) {
        snprintf(msg, sizeof(msg), "%s wades in to help %s, attacking you!\r\n",
                 ch->base.name, being_display_name(ally));
        descriptor_notify(target->desc, msg);
    }
    if (ally->desc && ally != target) {
        snprintf(msg, sizeof(msg), "%s wades in to help you!\r\n", ch->base.name);
        descriptor_notify(ally->desc, msg);
    }
    return true;
}
