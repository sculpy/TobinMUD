/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "log.h"
#include "obj.h"
#include "thing.h"

#define SIP_POISON_CHANCE_PCT 10
#define SIP_POISON_MIN_DMG 1
#define SIP_POISON_MAX_DMG 3

/* Same keyword-abbreviation matching as cmd_drink.c's own local copy
 * (duplicated rather than shared, same precedent as that file's own
 * note on cmd_object.c's cap_first() duplication). */
static bool keyword_matches(const char *keywords, const char *tok) {
    size_t tok_len = strlen(tok);
    if (tok_len == 0)
        return false;
    const char *p = keywords;
    while (*p) {
        while (*p == ' ')
            p++;
        const char *start = p;
        while (*p && *p != ' ')
            p++;
        size_t wlen = (size_t)(p - start);
        if (wlen >= tok_len && strncasecmp(start, tok, tok_len) == 0)
            return true;
    }
    return false;
}

/* `sip <liquid>` (Sneezy port, user 2026-07-12). Per Sneezy's own help
 * text: "less risk of damage... but does not fill you up as much as if
 * you had drunk fully" -- so this targets the exact same puddles/
 * fountains `drink` (cmd_drink.c) does, just with a much lower poison
 * chance/damage and its own "taste" flavored messaging, and (unlike
 * `drink`) never actually satisfies thirst/nutrition -- Tobin has no
 * hunger/thirst meter yet (task 22, "Vital statistics") for either
 * command to actually move, so this is honest about only being a
 * flavor/risk distinction for now. */
bool cmd_sip(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Usage: sip <puddle|fountain>\r\n");
        return true;
    }
    const char *tok;
    int ordinal = thing_parse_ordinal(raw, &tok);

    /* Ordinal support (user 2026-07-18: "make it true as part of
     * everything that can exist"), same "count matches, ordinal
     * defaults to 1" convention as cmd_drink.c's own copy of this loop. */
    obj_t *pool = NULL, *fount = NULL;
    int seen = 0;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (!keyword_matches(o->base.name, tok))
            continue;
        bool is_pool = keyword_matches(o->base.name, "puddle");
        bool is_fount = o->category == OBJ_CAT_DRINK;
        if (!is_pool && !is_fount)
            continue;
        seen++;
        if (seen != ordinal)
            continue;
        if (is_pool)
            pool = o;
        else
            fount = o;
        break;
    }

    if (!pool && !fount) {
        descriptor_send(d, "You don't see that here to sip.\r\n");
        return true;
    }

    if (fount && !pool) {
        const char *label = fount->base.short_descr[0] ? fount->base.short_descr : fount->base.name;
        char msg[320];
        snprintf(msg, sizeof(msg), "You sip a bit of water from %s. Tastes fine.\r\n", label);
        descriptor_send(d, msg);
        snprintf(msg, sizeof(msg), "%s takes a sip from %s.\r\n", ch->base.name, label);
        descriptor_room_echo(ch->base.roomp, ch, msg);
        return true;
    }

    const char *label = pool->base.short_descr[0] ? pool->base.short_descr : pool->base.name;
    char msg[320];
    snprintf(msg, sizeof(msg), "You taste a tiny bit of %s. Blech!\r\n", label);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s tastes a bit of %s.\r\n", ch->base.name, label);
    descriptor_room_echo(ch->base.roomp, ch, msg);

    if (rand() % 100 < SIP_POISON_CHANCE_PCT) {
        int dmg = SIP_POISON_MIN_DMG + rand() % (SIP_POISON_MAX_DMG - SIP_POISON_MIN_DMG + 1);
        ch->progress.hp -= dmg;
        if (ch->progress.hp < 1)
            ch->progress.hp = 1;
        descriptor_send(d, "You feel a faint twinge -- that wasn't entirely safe!\r\n");
        game_log(LOG_SILENT, "%s was mildly poisoned sipping %s (vnum %d) in room %d",
                 ch->base.name, label, pool->vnum, ch->base.roomp->vnum);
    } else {
        descriptor_send(d, "It doesn't seem to have hurt you.\r\n");
    }

    return true;
}
