/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "obj.h"
#include "room.h"
#include "thing.h"

/* Finds an object by keyword in a thing_t chain (room floor, or a being's
 * own carried/worn/held things) -- shared by look_at_target() below. */
static obj_t *find_obj_here(thing_t *chain, const char *tok, size_t len) {
    for (thing_t *t = chain; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        if (thing_name_matches(t->name, tok, len))
            return (obj_t *)t;
    }
    return NULL;
}

/* A one-word condition summary from cur_struct/max_struct, or NULL if the
 * prototype never set a max (0 -- most sandbox/test fixtures, some real
 * content) -- in which case `look <item>` just omits the condition line
 * rather than showing a meaningless "0/0". */
static const char *obj_condition_text(const obj_t *o) {
    if (o->max_struct <= 0)
        return NULL;
    int pct = (o->cur_struct * 100) / o->max_struct;
    if (pct > 100) pct = 100;
    if (pct < 0) pct = 0;
    if (pct >= 100) return "is in excellent condition";
    if (pct >= 75)  return "is in good condition";
    if (pct >= 50)  return "has seen some wear";
    if (pct >= 25)  return "is showing serious wear";
    if (pct > 0)    return "is falling apart";
    return "is destroyed";
}

/* `look <name>` -- describe another player, a mob (Phase 2D), or an object
 * (Phase 2C) in the room or in your own inventory/equipment. Matches by
 * case-insensitive keyword prefix (self included, so `look <ownname>`
 * works; a mob's name or an object's keywords can be multi-word, e.g.
 * "vrock demon", matched per-keyword -- see thing_name_matches()). A
 * being shows its appearance/description if set, else a gender-aware
 * "nothing special" line -- a mob's `description` column is loaded into
 * this same `appearance` field by being_create_mob(), so this needs no
 * mob-specific branch. An object shows its long_descr plus a condition
 * line derived from cur_struct/max_struct (when the prototype set one). */
static bool look_at_target(descriptor_t *d, const char *args) {
    char tok[64];
    if (sscanf(args, "%63s", tok) != 1)
        return false;

    room_t *r = d->character->base.roomp;
    size_t len = strlen(tok);
    being_t *tgt = NULL;
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC && t->kind != THING_MOB)
            continue;
        if (thing_name_matches(t->name, tok, len)) {
            tgt = (being_t *)t;
            break;
        }
    }
    if (tgt) {
        char out[BEING_APPEARANCE_LEN + 128];
        if (tgt->appearance[0])
            snprintf(out, sizeof(out), "You look at %s.\r\n%s\r\n",
                     tgt->base.name, tgt->appearance);
        else
            snprintf(out, sizeof(out),
                     "You look at %s.\r\nYou see nothing special about %s.\r\n",
                     tgt->base.name,
                     tgt == d->character ? "yourself" : gender_object(tgt->gender));
        descriptor_send(d, out);
        return true;
    }

    /* Not a PC/mob -- try an object: the room floor first, then whatever
     * the looker is carrying/wearing/holding. */
    obj_t *o = find_obj_here(r->base.stuff_head, tok, len);
    if (!o)
        o = find_obj_here(d->character->base.stuff_head, tok, len);
    if (!o) {
        descriptor_send(d, "You don't see that here.\r\n");
        return true;
    }

    char out[OBJ_LONG_DESCR_LEN + 512];
    int n = snprintf(out, sizeof(out), "%s\r\n",
                      o->long_descr[0] ? o->long_descr : o->base.short_descr);
    const char *cond = obj_condition_text(o);
    if (cond && (size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n, "It %s.\r\n", cond);
    /* A container also shows what's inside, when it's open. */
    if (obj_is_container(o) && (size_t)n < sizeof(out)) {
        if (obj_container_closed(o)) {
            n += snprintf(out + n, sizeof(out) - (size_t)n, "It is closed.\r\n");
        } else {
            n += snprintf(out + n, sizeof(out) - (size_t)n, "It contains:\r\n");
            bool any = false;
            for (thing_t *t = o->base.stuff_head; t && (size_t)n < sizeof(out); t = t->stuff_next) {
                if (t->kind != THING_OBJ)
                    continue;
                any = true;
                const char *label = t->short_descr[0] ? t->short_descr : t->name;
                n += snprintf(out + n, sizeof(out) - (size_t)n, "  %s\r\n", label);
            }
            if (!any && (size_t)n < sizeof(out))
                n += snprintf(out + n, sizeof(out) - (size_t)n, "  Nothing.\r\n");
        }
    }
    descriptor_send(d, out);
    return true;
}

bool cmd_look(descriptor_t *d, const char *args) {
    if (!d->character || !d->character->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (d->character->position == POSITION_SLEEPING) {
        descriptor_send(d, "You can't see anything -- you're fast asleep!\r\n");
        return true;
    }

    /* `look <name>` describes a player in the room; bare `look` shows it. */
    while (*args == ' ')
        args++;
    if (*args)
        return look_at_target(d, args);

    room_t *r = d->character->base.roomp;

    /* Tint the room by its sector: the NAME gets the bright (uppercase)
     * variant, the DESCRIPTION only the dim (lowercase) one (user spec). */
    char dim = sector_color(r->sector);
    char bright = (char)toupper((unsigned char)dim);

    char out[ROOM_DESCRIPTION_MAX + 512];
    int n;
    /* Immortals get the builder's header -- vnum, sector, flags around the
     * room name (user spec: "[room vnum] room name [other info]"); mortals
     * see the plain name. */
    if (being_is_immortal(d->character)) {
        char flagbuf[256];
        n = snprintf(out, sizeof(out), "\r\n[%d] <%c>%s<z> <c>[ %s ]<z> <p>%s<z>\r\n<%c>%s<z>\r\n",
                     r->vnum, bright, r->base.name, sector_name(r->sector),
                     room_flag_names(r->room_flag, flagbuf, sizeof(flagbuf)),
                     dim, r->description);
    } else {
        n = snprintf(out, sizeof(out), "\r\n<%c>%s<z>\r\n<%c>%s<z>\r\n",
                     bright, r->base.name, dim, r->description);
    }
    if (n < 0)
        n = 0;

    if ((size_t)n < sizeof(out)) {
        n += snprintf(out + n, sizeof(out) - (size_t)n, "Obvious exits:");
        int any_exit = 0;
        for (int i = 0; i < ROOM_NUM_EXITS && (size_t)n < sizeof(out); i++) {
            if (r->exits[i] < 0)
                continue;
            if (r->exit_cond[i] & EXIT_COND_SECRET)
                continue; /* undiscovered -- still walkable if you know the direction */
            any_exit = 1;
            n += snprintf(out + n, sizeof(out) - (size_t)n, " %s", DIR_NAMES[i]);
        }
        if ((size_t)n < sizeof(out))
            n += snprintf(out + n, sizeof(out) - (size_t)n, "%s\r\n", any_exit ? "" : " none");
    }

    int any = 0;
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t == &d->character->base)
            continue;
        if ((size_t)n >= sizeof(out))
            break;
        if (!any) {
            n += snprintf(out + n, sizeof(out) - (size_t)n, "\r\n");
            any = 1;
        }
        if ((size_t)n >= sizeof(out))
            break;
        const char *label = t->short_descr[0] ? t->short_descr : t->name;
        if (t->kind == THING_OBJ) {
            /* Objects use their own ground-listing sentence verbatim (e.g.
             * "A hairball is laying here."), not the generic "<label> is
             * here." used for PCs/mobs -- matches the original's real
             * long_desc convention. */
            const obj_t *o = (const obj_t *)t;
            n += snprintf(out + n, sizeof(out) - (size_t)n, "%s\r\n",
                          o->long_descr[0] ? o->long_descr : label);
        } else {
            n += snprintf(out + n, sizeof(out) - (size_t)n, "%s is here.\r\n", label);
        }
    }

    descriptor_send(d, out);
    return true;
}
