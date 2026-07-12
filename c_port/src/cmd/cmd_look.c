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

/* short_descr/name are stored lowercase-first by convention ("a city
 * watchman", "a torch"); capitalize only when one starts a whole message
 * (mid-sentence uses stay lowercase, e.g. "You conjure a torch..."). Copies
 * into `buf` (does not mutate `label`). Skips any leading inline color
 * tags ("<o>a dirty refuse hauler<1>" -- real seeded content, e.g. mob
 * vnum 33271) before capitalizing: bug found Session 43 continued (user:
 * "sometimes in look the proper capitalization is ignored, fix this") --
 * this used to blindly uppercase buf[0], which for a tag-prefixed label
 * is '<' (a no-op), silently leaving the real first letter lowercase. */
static const char *cap_first(const char *label, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%s", label);
    size_t i = 0;
    while (buf[i] == '<' && buf[i + 1] != '\0' && buf[i + 2] == '>')
        i += 3;
    if (buf[i])
        buf[i] = (char)toupper((unsigned char)buf[i]);
    return buf;
}

/* Renders one room-listing line (an object's ground sentence, or a mob/PC's
 * "<label> is here.") for `t` into `buf`, WITHOUT the trailing "\r\n" --
 * the caller appends that only once per distinct line, after stacking
 * (see room_item_groups() below). PCs are never affected by stacking in
 * practice (each has a unique name, so its rendered line never matches
 * another's), but this still has to render PCs correctly since the room
 * loop walks every kind of thing through the same function. */
static void render_room_item(char *buf, size_t bufsz, const room_t *r, const thing_t *t) {
    const char *label = t->short_descr[0] ? t->short_descr : t->name;
    char capbuf3[128];
    if (t->kind == THING_OBJ) {
        /* Objects use their own ground-listing sentence verbatim (e.g.
         * "A hairball is laying here."), not the generic "<label> is
         * here." used for PCs/mobs -- matches the original's real
         * long_desc convention. Capitalize only the short_descr
         * fallback -- long_descr is already a full sentence. */
        const obj_t *o = (const obj_t *)t;
        char groundbuf3[OBJ_LONG_DESCR_LEN + 32];
        snprintf(buf, bufsz, "%s",
                 o->long_descr[0]
                     ? obj_apply_ground_token(o->long_descr, r, groundbuf3, sizeof(groundbuf3))
                     : cap_first(label, capbuf3, sizeof(capbuf3)));
    } else {
        /* Mob short_descr is lowercase by convention ("a city watchman");
         * capitalize for the sentence start (PC names are already
         * capitalized, so this is a no-op for them). A linkdead PC
         * (user requirement) is tagged so it reads e.g. "Vic is here.
         * (linkdead)" -- visible, but see combat_find_room_target() for
         * why they can't actually be targeted. */
        bool linkdead = t->kind == THING_PC && !((being_t *)t)->desc;
        snprintf(buf, bufsz, "%s is here.%s",
                 cap_first(label, capbuf3, sizeof(capbuf3)),
                 linkdead ? " (linkdead)" : "");
    }
}

/* Groups identical room-listing lines together (user 2026-07-11: "object
 * stacking and mob stacking. for 2 gremlins you would see A gremlin is
 * standing here. (x2)") -- two mobs of the same vnum (or two objects with
 * the same long_desc) render an IDENTICAL line via render_room_item(), so
 * grouping by the rendered string itself (rather than needing a separate
 * vnum-equality check) naturally stacks true duplicates while leaving
 * PCs (always unique names) and visually-distinct objects untouched.
 * First-seen order is preserved. Returns the number of distinct groups
 * (capped at `max_groups`); `lines`/`counts` are the caller's arrays. */
#define ROOM_ITEM_LINE_LEN (OBJ_LONG_DESCR_LEN + 32)
static int group_room_items(const room_t *r, const being_t *viewer, bool want_fixture,
                            char lines[][ROOM_ITEM_LINE_LEN], int *counts, int max_groups) {
    int groups = 0;
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t == &viewer->base)
            continue;
        bool is_fixture = t->kind == THING_OBJ && !obj_takeable(((obj_t *)t)->wear_flag);
        if (is_fixture != want_fixture)
            continue;

        char line[ROOM_ITEM_LINE_LEN];
        render_room_item(line, sizeof(line), r, t);

        int i;
        for (i = 0; i < groups; i++) {
            if (strcmp(lines[i], line) == 0) {
                counts[i]++;
                break;
            }
        }
        if (i == groups && groups < max_groups) {
            snprintf(lines[groups], ROOM_ITEM_LINE_LEN, "%s", line);
            counts[groups] = 1;
            groups++;
        }
    }
    return groups;
}

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
bool look_at_target(descriptor_t *d, const char *args) {
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
        /* thing_t.name is the raw keyword-match list ("man dirty refuse
         * hauler" for mob vnum 33271) -- fine for a PC (whose name IS
         * their proper name), but wrong for a mob: bug found Session 43
         * continued (user: "You look at man dirty refuse hauler. should
         * read You look at a dirty refuse hauler."). Mobs display their
         * short_descr instead -- lowercase, uncapitalized, since it's
         * mid-sentence here (after "You look at"), same convention as
         * the room-floor listing's use of cap_first() only at a
         * sentence START. */
        const char *display = tgt->base.name;
        if (tgt->base.kind == THING_MOB && tgt->base.short_descr[0])
            display = tgt->base.short_descr;

        /* Headroom beyond BEING_APPEARANCE_LEN + display-name + a full
         * equipment listing so gcc's -Wformat-truncation worst-case
         * estimate (sums every %s field's own declared bound) can prove
         * this always fits. */
        char out[BEING_APPEARANCE_LEN + 2048];
        size_t n;
        if (tgt->appearance[0])
            n = (size_t)snprintf(out, sizeof(out), "You look at %s.\r\n%s\r\n",
                                  display, tgt->appearance);
        else
            n = (size_t)snprintf(out, sizeof(out),
                                  "You look at %s.\r\nYou see nothing special about %s.\r\n",
                                  display,
                                  tgt == d->character ? "yourself" : gender_object(tgt->gender));
        /* Worn equipment (user 2026-07-12: "when you look at someone you
         * should also see what equipment thier wearing") -- same renderer
         * `equipment` (cmd_object.c) uses on yourself. */
        if (n < sizeof(out)) {
            if (tgt == d->character) {
                n += (size_t)snprintf(out + n, sizeof(out) - n, "You are using:\r\n");
            } else {
                /* Sentence-initial here, unlike "You look at <display>"
                 * above -- a mob's display is lowercase by convention, so
                 * capitalize it same as the room-listing lines do. */
                char capbuf2[128];
                n += (size_t)snprintf(out + n, sizeof(out) - n, "%s is using:\r\n",
                                      cap_first(display, capbuf2, sizeof(capbuf2)));
            }
        }
        if (n < sizeof(out))
            being_render_equipment(tgt, out, sizeof(out), &n);
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
    char capbuf[128]; /* matches thing_t.short_descr's size */
    char groundbuf[OBJ_LONG_DESCR_LEN + 32]; /* $$g -> "ocean floor" etc, a few bytes longer */
    int n = snprintf(out, sizeof(out), "%s\r\n",
                      o->long_descr[0]
                          ? obj_apply_ground_token(o->long_descr, r, groundbuf, sizeof(groundbuf))
                          : cap_first(o->base.short_descr, capbuf, sizeof(capbuf)));
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
                char capbuf2[128];
                const char *label = cap_first(t->short_descr[0] ? t->short_descr : t->name,
                                              capbuf2, sizeof(capbuf2));
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

    /* "Obvious exits: north east ..." -> "[Exits:] North East ..." (user,
     * 2026-07-11), colorized: the "[Exits:]" label in cyan (matching the
     * sector-name bracket's own color a few lines up), the direction list
     * itself in the room's own SECTOR color (user follow-up, 2026-07-11:
     * "the exit messages in a room should reflect the sector type and be
     * colored like name") -- `bright`, the same tag the room NAME above
     * already uses, not a fixed color. */
    {
        char exits_buf[128] = "";
        size_t en = 0;
        int any_exit = 0;
        for (int i = 0; i < ROOM_NUM_EXITS; i++) {
            if (r->exits[i] < 0)
                continue;
            if (r->exit_cond[i] & EXIT_COND_SECRET)
                continue; /* undiscovered -- still walkable if you know the direction */
            any_exit = 1;
            char dirbuf[16];
            snprintf(dirbuf, sizeof(dirbuf), "%s", DIR_NAMES[i]);
            dirbuf[0] = (char)toupper((unsigned char)dirbuf[0]);
            en += (size_t)snprintf(exits_buf + en, sizeof(exits_buf) - en, "%s%s", en ? " " : "", dirbuf);
        }
        if ((size_t)n < sizeof(out)) {
            if (any_exit)
                n += snprintf(out + n, sizeof(out) - (size_t)n, "<c>[Exits:]<z> <%c>%s<z>\r\n", bright, exits_buf);
            else
                n += snprintf(out + n, sizeof(out) - (size_t)n, "<c>[Exits:]<z> none\r\n");
        }
    }

    /* Two passes (user, 2026-07-11: "permanent items such as a lamppost or
     * a fountain should be listed first in look room code"): non-takeable
     * fixture objects (fountains, furniture, statuary -- anything without
     * WEAR_TAKE) print before everything else (ordinary loot, mobs, PCs),
     * which otherwise print in plain stuff_head/insertion order. Within
     * each pass, identical entries are stacked into one line with a
     * "(xN)" suffix (user 2026-07-11: "object stacking and mob stacking.
     * for 2 gremlins you would see A gremlin is standing here. (x2)"). */
    int any = 0;
    for (int pass = 0; pass < 2; pass++) {
        char lines[64][ROOM_ITEM_LINE_LEN];
        int counts[64];
        int groups = group_room_items(r, d->character, pass == 0, lines, counts, 64);
        for (int i = 0; i < groups; i++) {
            if ((size_t)n >= sizeof(out))
                break;
            if (!any) {
                n += snprintf(out + n, sizeof(out) - (size_t)n, "\r\n");
                any = 1;
            }
            if (counts[i] > 1)
                n += snprintf(out + n, sizeof(out) - (size_t)n, "%s (x%d)\r\n", lines[i], counts[i]);
            else
                n += snprintf(out + n, sizeof(out) - (size_t)n, "%s\r\n", lines[i]);
        }
    }

    descriptor_send(d, out);
    return true;
}
