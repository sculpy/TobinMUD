/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
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
#include "room_repo.h"
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
        /* A light's own on/off state (val[3], obj.h) is otherwise
         * invisible to a player -- same "(linkdead)" tagging convention
         * as the PC branch below, so `light`/`extinguish`/the lamplighter
         * (cmd_light.c/mob_ai.c) have an actual visible effect. */
        bool lit = o->category == OBJ_CAT_LIGHT && o->val[3];
        snprintf(buf, bufsz, "%s%s",
                 o->long_descr[0]
                     ? obj_apply_ground_token(o->long_descr, r, groundbuf3, sizeof(groundbuf3))
                     : cap_first(label, capbuf3, sizeof(capbuf3)),
                 lit ? " (lit)" : "");
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

/* Same "is this OBJECT loose (not worn/held)" check as cmd_object.c's own
 * static is_loose() -- same duplication precedent as this file's own
 * cap_first(). Used by the immortal-only carried-inventory listing below
 * (user 2026-07-19: "immortals can see inventory when looking at a mob
 * or player") so worn/held items (already shown by the equipment
 * listing just above) aren't duplicated into the inventory section. */
static bool is_loose(const being_t *ch, const obj_t *o) {
    if (ch->held[0] == o || ch->held[1] == o)
        return false;
    for (int i = 0; i < LIMB_COUNT; i++)
        if (ch->equipment[i] == o)
            return false;
    return true;
}

/* Immortal-only: renders `tgt`'s loose carried inventory into `out`
 * (starting at *n, advancing it), one level deep into any container
 * among them -- user 2026-07-19: "immortals can see inventory when
 * looking at a mob or player and can also see the contents of any
 * container they carry." Mortals never see this (gated by the caller).
 * Deliberately only one level of container nesting (a bag inside a bag
 * shows the outer bag's contents but not the inner bag's own contents
 * listed a third level down) -- matches `look <container>`'s own
 * single-level "It contains:" convention, not a full recursive dump. */
static void render_immortal_inventory(const being_t *tgt, const char *display,
                                      being_t *viewer, char *out, size_t outsz, size_t *n) {
    if (tgt == viewer)
        *n += (size_t)snprintf(out + *n, outsz - *n, "You are carrying:\r\n");
    else {
        char capbuf[128];
        *n += (size_t)snprintf(out + *n, outsz - *n, "%s is carrying:\r\n",
                               cap_first(display, capbuf, sizeof(capbuf)));
    }

    bool any = false;
    for (thing_t *t = tgt->base.stuff_head; t && *n < outsz; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (!is_loose(tgt, o))
            continue;
        any = true;

        char capbuf[128];
        const char *label = cap_first(o->base.short_descr[0] ? o->base.short_descr : o->base.name,
                                      capbuf, sizeof(capbuf));
        *n += (size_t)snprintf(out + *n, outsz - *n, "  %s\r\n", label);

        if (!obj_is_container(o) || *n >= outsz)
            continue;
        if (obj_container_closed(o)) {
            *n += (size_t)snprintf(out + *n, outsz - *n, "    (closed)\r\n");
            continue;
        }
        bool any_inner = false;
        for (thing_t *ct = o->base.stuff_head; ct && *n < outsz; ct = ct->stuff_next) {
            if (ct->kind != THING_OBJ)
                continue;
            any_inner = true;
            char capbuf2[128];
            const char *ilabel = cap_first(ct->short_descr[0] ? ct->short_descr : ct->name,
                                           capbuf2, sizeof(capbuf2));
            *n += (size_t)snprintf(out + *n, outsz - *n, "    %s\r\n", ilabel);
        }
        if (!any_inner && *n < outsz)
            *n += (size_t)snprintf(out + *n, outsz - *n, "    (nothing)\r\n");
    }
    if (!any && *n < outsz)
        *n += (size_t)snprintf(out + *n, outsz - *n, "  Nothing.\r\n");
}

/* Finds an object by keyword in a thing_t chain (room floor, or a being's
 * own carried/worn/held things) -- shared by look_at_target() below.
 * `ordinal` (see thing_parse_ordinal()) picks the Nth match instead of
 * always the first, e.g. "2.board" -- 1 (the default with no "N."
 * prefix) reproduces the old always-first-match behavior exactly. */
static obj_t *find_obj_here(thing_t *chain, const char *tok, size_t len, int ordinal) {
    int seen = 0;
    for (thing_t *t = chain; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        if (thing_name_matches(t->name, tok, len)) {
            seen++;
            if (seen == ordinal)
                return (obj_t *)t;
        }
    }
    return NULL;
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
 * line derived from cur_struct/max_struct (when the prototype set one).
 *
 * Supports the "N.keyword" ordinal prefix (user 2026-07-18: "make look
 * board, look 2.board to look at second board... make it true as part of
 * everything that can exist, l mob, l 2.mob, kill 2.mob, etc.") --
 * `thing_parse_ordinal()` already backed `kill`/`get` (combat.c/
 * cmd_object.c); `look` was the one gap. Plain `look <name>` (no "N."
 * prefix, ordinal defaults to 1) reproduces the old always-first-match
 * behavior exactly -- no behavior change for existing muscle memory. */
bool look_at_target(descriptor_t *d, const char *args) {
    char raw[64];
    if (sscanf(args, "%63s", raw) != 1)
        return false;

    const char *tok;
    int ordinal = thing_parse_ordinal(raw, &tok);

    room_t *r = d->character->base.roomp;
    size_t len = strlen(tok);
    being_t *tgt = NULL;
    int seen = 0;
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC && t->kind != THING_MOB)
            continue;
        if (thing_name_matches(t->name, tok, len)) {
            seen++;
            if (seen == ordinal) {
                tgt = (being_t *)t;
                break;
            }
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
         * this always fits. Widened for the immortal-only carried-
         * inventory + one-level container-contents listing below (user
         * 2026-07-19) -- every append into it is already bounds-checked
         * against sizeof(out), so this is purely "give it more room to
         * show," not a correctness requirement. */
        char out[BEING_APPEARANCE_LEN + 4096];
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
        /* Immortal-only carried inventory + one-level container contents
         * (user 2026-07-19: "immortals can see inventory when looking
         * at a mob or player and can also see the contents of any
         * container they carry"). Mortals looking at another PC/mob see
         * nothing beyond the equipment listing above, same as before. */
        if (n < sizeof(out) && being_is_immortal(d->character))
            render_immortal_inventory(tgt, display, d->character, out, sizeof(out), &n);
        descriptor_send(d, out);
        return true;
    }

    /* Not a PC/mob -- try an object: the room floor first, then whatever
     * the looker is carrying/wearing/holding. */
    obj_t *o = find_obj_here(r->base.stuff_head, tok, len, ordinal);
    if (!o)
        o = find_obj_here(d->character->base.stuff_head, tok, len, ordinal);
    if (!o) {
        /* Extra descriptions (classic Diku "look <keyword>" reveals a
         * hidden room detail -- e.g. a wall poster or a bed, never a
         * real object) -- the room's own `roomextra` rows, checked last
         * since a real PC/mob/object match should always win over one.
         * See room_repo_extra_desc()'s own doc comment: 8,861 real
         * seeded rows existed with no code reading them until this. */
        char extra[2048];
        if (room_repo_extra_desc(r->vnum, tok, extra, sizeof(extra))) {
            char extra_out[2048 + 4];
            snprintf(extra_out, sizeof(extra_out), "%s\r\n", extra);
            descriptor_send(d, extra_out);
            return true;
        }
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
    const char *cond = obj_condition_word(o);
    if (cond && (size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n, "It is %s.\r\n", cond);
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
    descriptor_page_start(d, out, 0);
    return true;
}

/* The `look` command: with an argument, describes a matching player/mob/
 * object/exit in the room (look_at_target()); bare, renders the room
 * itself -- name, description, exits, and contents, gated by the
 * darkness/light check below before any of that is shown. */
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

    /* Weather & light levels (Sneezy → Tobin feature audit): a dark,
     * unlit outdoor room at night with no personal light source shows
     * nothing but darkness -- classic MUD convention, and the actual
     * payoff that makes carried light sources (cmd_light.c) matter for
     * the first time. Deliberately does NOT also block `look <target>`
     * (look_at_target(), just above) -- scoped to the informational room
     * description only, same "don't over-reach past what was asked" call
     * as everywhere else this session. */
    if (room_is_dark_for((struct room *)r, d->character)) {
        descriptor_send(d, "It is pitch black... you cannot see a thing.\r\n");
        return true;
    }

    /* Tint the room by its sector: the NAME gets the bright (uppercase)
     * variant, the DESCRIPTION only the dim (lowercase) one (user spec). */
    char dim = sector_color(r->sector);
    char bright = (char)toupper((unsigned char)dim);

    /* Most seeded room descriptions carry their own trailing "\n" (upstream
     * SneezyMUD convention -- ~95% of rows do), which stacked with the \r\n
     * this function already appends produced a blank line between the
     * description and Exits that shouldn't be there (user: "one too many
     * \r\n" there). Trimmed on a copy, not r->description itself -- other
     * consumers (redit's preload, etc.) still see the raw stored text. */
    char desc[ROOM_DESCRIPTION_MAX];
    snprintf(desc, sizeof(desc), "%s", r->description);
    size_t dlen = strlen(desc);
    while (dlen > 0 && (desc[dlen - 1] == '\n' || desc[dlen - 1] == '\r'))
        desc[--dlen] = '\0';

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
                     dim, desc);
    } else {
        n = snprintf(out, sizeof(out), "\r\n<%c>%s<z>\r\n<%c>%s<z>\r\n",
                     bright, r->base.name, dim, desc);
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
    for (int pass = 0; pass < 2; pass++) {
        char lines[64][ROOM_ITEM_LINE_LEN];
        int counts[64];
        int groups = group_room_items(r, d->character, pass == 0, lines, counts, 64);
        for (int i = 0; i < groups; i++) {
            if ((size_t)n >= sizeof(out))
                break;
            if (counts[i] > 1)
                n += snprintf(out + n, sizeof(out) - (size_t)n, "%s (x%d)\r\n", lines[i], counts[i]);
            else
                n += snprintf(out + n, sizeof(out) - (size_t)n, "%s\r\n", lines[i]);
        }
    }

    descriptor_send(d, out);
    return true;
}
