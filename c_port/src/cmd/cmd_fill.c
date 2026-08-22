/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "liquids.h"
#include "log.h"
#include "obj.h"
#include "thing.h"

/* Same puddle-keyword check obj.c's own pool_decay_visit()/cmd_drink.c use
 * -- a puddle is any OBJ_CAT_TRASH object whose keywords include the
 * literal word "puddle" (obj_grow_pool()'s own convention, obj.c). */
static bool has_puddle_keyword(const char *keywords) {
    size_t tag_len = strlen("puddle");
    const char *p = keywords;
    while (*p) {
        while (*p == ' ')
            p++;
        const char *start = p;
        while (*p && *p != ' ')
            p++;
        size_t wlen = (size_t)(p - start);
        if (wlen == tag_len && strncasecmp(start, "puddle", tag_len) == 0)
            return true;
    }
    return false;
}

/* `fill <container>` (Liquids, Sneezy -> Tobin feature audit, user
 * 2026-07-26: "fill a container from a liquid pool"). Ported from
 * TBaseCup::fillMe() (obj_base_cup.cc), simplified: the original
 * "different liquid spoils into slime" mixing rule is replaced with a
 * flat refusal (matches the "keep it simple" precedent Money/Components
 * already set) -- pour the container out first to switch liquids. A room
 * fountain (real OBJ_CAT_DRINK object, same "never runs dry" convention
 * cmd_drink.c's own fountain branch already uses) is always preferred
 * over a ground puddle when both are present; a puddle is finite --
 * filling from one costs it one obj_grow_pool() growth unit (val[0]),
 * same currency obj_pool_decay_tick() already spends it down in, and it's
 * destroyed outright once drained, exactly like decay would do anyway. */
bool cmd_fill(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Usage: fill <container>\r\n");
        return true;
    }

    obj_t *container = liquid_find_carried_container(ch, raw);
    if (!container) {
        descriptor_send(d, "You aren't carrying that.\r\n");
        return true;
    }
    if (container->val[1] >= container->val[0]) {
        descriptor_send(d, "That is already completely full!\r\n");
        return true;
    }

    obj_t *fount = NULL, *puddle = NULL;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (o->category == OBJ_CAT_DRINK && !fount)
            fount = o;
        else if (o->category == OBJ_CAT_TRASH && has_puddle_keyword(o->base.name) && !puddle)
            puddle = o;
    }
    obj_t *source = fount ? fount : puddle;
    if (!source) {
        descriptor_send(d, "There's no water source here to fill that from.\r\n");
        return true;
    }

    int source_type = fount ? fount->val[2] : liquid_type_from_keywords(puddle->base.name);
    if (container->val[1] > 0 && container->val[2] != source_type) {
        char msg[256];
        snprintf(msg, sizeof(msg), "You can't mix %s with what's already in there -- pour it out first.\r\n",
                 liquid_info(source_type)->name);
        descriptor_send(d, msg);
        return true;
    }

    container->val[2] = source_type;
    container->val[1] = container->val[0];

    if (puddle && !fount) {
        puddle->val[0]--;
        if (puddle->val[0] <= 0)
            obj_destroy(puddle);
    }

    const char *label = container->base.short_descr[0] ? container->base.short_descr : container->base.name;
    char msg[320];
    snprintf(msg, sizeof(msg), "You fill %s.\r\n", label);
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s fills %s.\r\n", ch->base.name, label);
    descriptor_room_echo(ch->base.roomp, ch, msg);

    game_log(LOG_SILENT, "%s filled %s (vnum %d) in room %d",
             ch->base.name, label, container->vnum, ch->base.roomp->vnum);

    return true;
}
