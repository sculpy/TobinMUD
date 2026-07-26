/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_LIQUIDS_H
#define TOBIN_LIQUIDS_H

#include <stddef.h>

/* Liquids (Sneezy -> Tobin feature audit, user 2026-07-26: "Liquids --
 * drinkable liquids; pouring one out pools on the ground" + "fill -- fill
 * a container from a liquid pool"). Ported from the real `liquids.cc`'s
 * `liqTypeT`/`liqEntry` table, scoped down the same way Money/Components
 * already were: the original's ordinals 35+ are all magic potions/poisons
 * tied to the full spellcasting/alchemy system (LIQ_POT_ and LIQ_POISON_
 * types, 2400+ combined lines across obj_component.cc/disc_alchemy.cc) --
 * out of scope here, same as the full commodity/alchemy system already was.
 * Ordinals 0-34 are the real MUNDANE drink types (water, beer, wine, ...)
 * and are a genuine 1:1 port of liquids.cc's own numbers -- these are the
 * exact same ordinals real seeded `obj.val2` rows already use (confirmed
 * live: vnum rows with type=17/ITEM_DRINKCON hold val2 values like 0, 2, 5,
 * 9, 10, 12, 13, 15, 17, 19, all within this mundane range, never a magic
 * potion index), so this table's ordering must stay index-for-index
 * identical to the original's enum or every already-seeded drinkcon in the
 * DB would suddenly show the wrong liquid. */

#define LIQUID_TYPE_COUNT 35 /* ordinals 0-34, LIQ_WATER..LIQ_ISLA_VERDE */
#define LIQUID_TYPE_DEFAULT 0 /* LIQ_WATER -- fallback for anything out of
                                  range (a magic-potion ordinal, or a bad
                                  seed value) so nothing ever mis-renders */

typedef struct {
    const char *name;   /* color-tagged display noun, e.g. "<c>water<1>" */
    int thirst;         /* progress_t.thirst gained per drink/sip, 0-100 scale */
    int hunger;          /* progress_t.hunger delta (some drinks are filling,
                            some -- coffee, whisky -- are dehydrating) */
} liquid_type_t;

/* Returns the liquid_type_t for `type` (any obj.val[2] value), clamped to
 * LIQUID_TYPE_DEFAULT if out of the mundane 0-34 range. Never returns
 * NULL. */
const liquid_type_t *liquid_info(int type);

struct being;
struct obj;

/* Finds an OBJ_CAT_DRINK container `ch` is carrying (loose inventory,
 * held, or worn -- all live on the same stuff_head chain, see
 * cmd_object.c's is_loose() for that convention) matching keyword `tok`,
 * with the usual "N.keyword" ordinal prefix. Shared by cmd_drink.c/
 * cmd_sip.c/cmd_pour.c/cmd_fill.c rather than duplicated four times. */
struct obj *liquid_find_carried_container(const struct being *ch, const char *tok);

/* Strips every "<X>" color tag out of liquid_info(type)->name, e.g.
 * "<c>water<1>" -> "water", "<k>dark<1> <o>ale<1>" -> "dark ale". Used as
 * the bare, lowercase word obj_grow_pool() (obj.c) needs for its
 * type_tag/keywords/noun params -- same convention cmd_pee.c's own
 * PEE_LIQUIDS table already follows (a puddle's own color always comes
 * from pool_noun_color()'s generic scheme, not the liquid's "native"
 * color -- same simplification that table already accepted). */
void liquid_bare_name(int type, char *out, size_t outsz);

/* The reverse of liquid_bare_name(): recovers which liquid type a puddle
 * holds from its own `keywords` (obj_t.base.name, e.g. "puddle pool
 * wine" -- exactly what cmd_pour.c's obj_grow_pool() call tags it with),
 * by checking each liquid's own bare name against it. Falls back to
 * LIQUID_TYPE_DEFAULT for a puddle that isn't a real liquid at all (e.g.
 * `pee`/blood, cmd_pee.c/combat.c's own obj_grow_pool() calls) -- filling
 * from one of those was never a sensible action anyway. */
int liquid_type_from_keywords(const char *keywords);

#endif
