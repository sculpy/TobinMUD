/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_THING_H
#define TOBIN_THING_H

#include <stdbool.h>
#include <stddef.h>

/* C replacement for misc/thing.h's TThing.  TThing is the abstract root of
 * every game entity (rooms, beings, objects) with 199 virtual methods, the
 * vast majority of which are never actually overridden by more than one or
 * two kinds -- habitual `virtual`, not real polymorphism (confirmed by
 * reading thing.h directly). We flatten this with:
 *
 *   - a `kind` tag enum (this already exists by hand in the original as
 *     TThing::getKind() / TThingKind)
 *   - "C-style inheritance": every concrete type embeds struct thing as its
 *     literal first member, so a `being_t*`/`room_t*` is always safely
 *     castable to `thing_t*` and back (same trick as GObject/CPython)
 *
 * Phase 1 only needs enough fields to support login + look + who, not the
 * full field surface of the original TThing -- more gets added as later
 * phases (obj/, combat, etc) land. See c_port/STATUS.md for what's next.
 */

typedef enum {
    THING_ROOM,
    THING_PC,
    THING_MOB,
    THING_OBJ,
    THING_COUNT
} thing_kind_t;

typedef struct thing {
    thing_kind_t kind;
    int id;                    /* room vnum / player id / mob vnum, per kind */
    char name[64];
    char short_descr[128];

    struct thing *parent;      /* room/container I'm inside of */
    struct thing *stuff_head;  /* intrusive list: things I contain */
    struct thing *stuff_next;  /* intrusive list: next sibling in my parent's stuff */

    struct room *roomp;        /* room I'm currently in (NULL for rooms themselves) */
} thing_t;

/* Adds `t` to `parent`'s contents list and sets t->parent. */
void thing_move_to(thing_t *t, thing_t *parent);

/* Removes `t` from whatever parent it's currently in, if any. */
void thing_remove_from_parent(thing_t *t);

/* Moves `t` into room `r`'s contents list and sets t->roomp. `r` must be a
 * room_t*, passed as `struct room *` here to avoid a header dependency on
 * room.h -- safe because room_t embeds thing_t as its first member. */
void thing_set_room(thing_t *t, struct room *r);

/* True iff `tok` (length `tok_len`) is a case-insensitive prefix of any ONE
 * space-separated word in `keywords` -- e.g. "vrock" or "demon" both match
 * the keyword string "vrock demon". Generic over any thing_t.name (objects,
 * mobs, and single-word PC names alike -- for a single-word name this is
 * behavior-identical to a plain prefix check). Shared by combat targeting
 * (combat.c) and `look <target>` (cmd_look.c); cmd_object.c keeps its own
 * copy (predates this, already tested, not worth the churn to switch). */
bool thing_name_matches(const char *keywords, const char *tok, size_t tok_len);

/* Parses a leading "N." ordinal off `arg` (classic DikuMUD multi-target
 * convention, user 2026-07-11: "mob 2.mob 3.mob etc should attack the 1st
 * 2nd and 3rd, same for getting multiple objects, obj 2.obj 3.obj") --
 * "2.sword" means the SECOND sword among matches, not always the first.
 * Writes the remainder (the part after "N.", or all of `arg` if there was
 * no ordinal prefix) into `*rest` and returns the ordinal, always >= 1.
 * A malformed prefix (e.g. "2." with nothing after it, or a leading "0.")
 * is treated as no prefix at all -- `*rest` is set back to the original
 * `arg` and 1 is returned, so a legitimately weird item name starting
 * with a digit ("2.you get it" isn't real, but this errs toward not
 * eating input the caller didn't mean as an ordinal). */
int thing_parse_ordinal(const char *arg, const char **rest);

#endif
