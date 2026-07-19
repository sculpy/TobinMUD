/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "log.h"
#include "obj.h"
#include "obj_repo.h"
#include "thing.h"

/* Same case-insensitive per-keyword prefix match cmd_drink.c/cmd_sip.c
 * each keep their own local copy of. */
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

/* `eat <food>` (Sneezy → Tobin feature audit, "Vital statistics"). Only
 * searches carried inventory (ch->base.stuff_head), not the room floor or
 * worn/held slots -- food isn't wearable, and "eat" reaching across the
 * room to a dropped sandwich isn't how any of this codebase's other
 * consumption commands (drink/sip) work either. Every seeded FOOD object's
 * val[1] ("current units") is uniformly 0 in the real DB (a genuine
 * upstream data gap, same class as the components/symbols val0/val1 gap
 * tobin_migrations.sql already fixed once) -- rather than backfill it,
 * this sidesteps val[1] entirely: eating always consumes the WHOLE object
 * in one bite (matches how a "loaf of bread" or "steak" realistically
 * works for a small MUD; a multi-serving "ration"/"provision" is flavor
 * text, not modeled separately). Nutrition gained = val[0] (already
 * well-populated, 1-24 across the real seed) added directly to hunger's
 * 0-100 scale, clamped -- a big steak (24) is a much better meal than a
 * rat tail (1), using real seeded data with zero migration needed. */
bool cmd_eat(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Eat what?\r\n");
        return true;
    }
    const char *tok;
    int ordinal = thing_parse_ordinal(raw, &tok);

    obj_t *food = NULL;
    int seen = 0;
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (o->category != OBJ_CAT_FOOD)
            continue;
        if (!keyword_matches(o->base.name, tok))
            continue;
        seen++;
        if (seen != ordinal)
            continue;
        food = o;
        break;
    }

    if (!food) {
        descriptor_send(d, "You aren't carrying that to eat.\r\n");
        return true;
    }

    const char *label = food->base.short_descr[0] ? food->base.short_descr : food->base.name;
    char msg[320]; /* short_descr (128) + fixed text */
    snprintf(msg, sizeof(msg), "You eat %s. Yum!\r\n", label);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s eats %s.\r\n", ch->base.name, label);
    descriptor_room_echo(ch->base.roomp, ch, msg);

    if (!being_is_immortal(ch)) {
        ch->progress.hunger += food->val[0];
        if (ch->progress.hunger > 100)
            ch->progress.hunger = 100;
        player_progress_save(ch->player_id, &ch->progress);
    }

    game_log(LOG_SILENT, "%s ate %s (vnum %d)", ch->base.name, label, food->vnum);
    obj_destroy(food);
    player_inventory_save(ch->player_id, ch);
    return true;
}
