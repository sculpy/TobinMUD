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
#include "room.h"
#include "thing.h"

/* `look <name>` -- describe another player in the room. Matches a PC by
 * case-insensitive name prefix (self included, so `look <ownname>` works).
 * Shows their appearance if set, else a gender-aware "nothing special" line. */
static bool look_at_target(descriptor_t *d, const char *args) {
    char tok[64];
    if (sscanf(args, "%63s", tok) != 1)
        return false;

    room_t *r = d->character->base.roomp;
    size_t len = strlen(tok);
    being_t *tgt = NULL;
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC)
            continue;
        if (strncasecmp(t->name, tok, len) == 0) {
            tgt = (being_t *)t;
            break;
        }
    }
    if (!tgt) {
        descriptor_send(d, "You don't see anyone by that name here.\r\n");
        return true;
    }

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
        n += snprintf(out + n, sizeof(out) - (size_t)n, "%s is here.\r\n", label);
    }

    descriptor_send(d, out);
    return true;
}
