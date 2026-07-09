/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_OBJ_H
#define TOBIN_OBJ_H

#include <stdbool.h>

#include "thing.h"

/* C replacement for the TObj slice of misc/obj.h -- objects (Phase 2C).
 * The original collapses 98 obj/ subclasses via 60 `itemTypeT` values;
 * this collapses further, into ~16 generic categories, with a 4-int `val[]`
 * payload whose meaning depends on category (mirrors the original's
 * val0..val3 generic fields) instead of one C++ class per item type.
 *
 * Object PROTOTYPES already exist in the DB -- the upstream seed's `obj`
 * table (db/sneezy/obj.sql, thousands of real rows, PK vnum) is read
 * directly by obj_repo.c, no new prototype table needed. Only in-world
 * INSTANCES (this struct) are new state; see obj_repo.h for what of that
 * gets persisted. */

struct being; /* forward decl only -- avoids a being.h<->obj.h include cycle,
                 same idiom thing.h already uses for `struct room`. */

typedef enum {
    OBJ_CAT_LIGHT,
    OBJ_CAT_WEAPON,
    OBJ_CAT_AMMO,
    OBJ_CAT_ARMOR,
    OBJ_CAT_CONTAINER,
    OBJ_CAT_DRINK,
    OBJ_CAT_FOOD,
    OBJ_CAT_MONEY,
    OBJ_CAT_KEY,
    OBJ_CAT_MAGIC_DEVICE,
    OBJ_CAT_TOOL,
    OBJ_CAT_FURNITURE,
    OBJ_CAT_TREASURE,
    OBJ_CAT_WRITTEN,
    OBJ_CAT_TRASH,
    OBJ_CAT_OTHER,
    OBJ_CAT_COUNT
} obj_category_t;

/* Display name for a category ("weapon", "container", ...). */
const char *obj_category_name(obj_category_t cat);

#define OBJ_LONG_DESCR_LEN 256

typedef struct obj {
    thing_t base;             /* kind = THING_OBJ, id = vnum; base.name is
                                  the get/drop keyword string (the prototype's
                                  `name` column, often several space-separated
                                  keywords); base.short_descr is the prototype's
                                  short_desc ("a hairball") shown in inventory/
                                  equipment listings and room-floor "X is here"
                                  text; parent/stuff_head/stuff_next (via
                                  thing_move_to()/thing_remove_from_parent())
                                  is the ONE containment mechanism for every
                                  obj_t, whether on a room floor, carried, or
                                  worn/held -- being_t.equipment[]/held[] are
                                  fast-lookup pointers INTO that same set, not
                                  separate storage (see being.h) */
    int vnum;
    char long_descr[OBJ_LONG_DESCR_LEN]; /* ground-listing sentence, e.g.
                                  "A hairball is laying here." -- shown as-is,
                                  not thing.h's short_descr + "is here." */
    obj_category_t category;  /* collapsed from the original's 60 itemTypeT --
                                  see category_for_item_type() */
    int wear_flag;            /* stored VERBATIM from the DB's original
                                  upstream bit layout (ITEM_WEAR_TAKE=1,
                                  _FINGERS=2, _NECK=4, _BODY=8, _HEAD=16,
                                  _LEGS=32, _FEET=64, _HANDS=128, _ARMS=256,
                                  _BACK=1024, _WAIST=2048, _WRISTS=4096,
                                  _HOLD=16384, _THROW=32768) so every already-
                                  seeded object "just works" with no data
                                  migration -- translated to a Tobin limb_t
                                  (or the held[] slots) only at wear time, see
                                  wear_slot_for_flag(). */
    /* Generic 4-int payload; meaning depends on `category` (placeholder
     * formulas, same precedent as the XP curve / regen rate -- revisit per
     * category as real gameplay lands):
     *   LIGHT:    val[0]=is lit (0/1)      val[1]=max fuel (-1=infinite) val[2]=fuel remaining
     *   WEAPON:   val[0]=damage dice count val[1]=damage dice sides
     *   ARMOR:    val[0]=armor class (protection value)
     *   CONTAINER:val[0]=max weight cap.   val[1]=flags(closeable/closed/locked/pickproof) val[2]=key vnum (0=no lock)
     *   DRINK:    val[0]=max units         val[1]=current units
     *   FOOD:     val[0]=max units (hunger)val[1]=current units
     *   MONEY:    val[0]=coin amount
     *   KEY:      val[0]=vnum this key unlocks
     *   MAGIC_DEVICE: val[0]=charges/uses remaining
     * Categories not listed leave val[] unused/decorative for now. */
    int val[4];
    double weight;
    int volume;
    int price;
    int max_struct;
    int cur_struct;
    int material;
    bool can_be_seen;
} obj_t;

/* CONTAINER flag bits, stored VERBATIM in val[1] in the original's bit layout
 * (misc/obj.h) so every already-seeded container works with zero migration --
 * same "stored verbatim" precedent as wear_flag. Only the bits Tobin acts on
 * are named here; the original defines a few more (CONT_EMPTYTRAP, etc.) that
 * are decorative for now. */
#define CONT_CLOSEABLE (1 << 0) /* can be opened/closed at all */
#define CONT_PICKPROOF (1 << 1) /* lock cannot be picked (unused until keys) */
#define CONT_CLOSED    (1 << 2) /* currently shut */
#define CONT_LOCKED    (1 << 3) /* currently locked (unlock/keys deferred) */

/* True iff the object is a container (OBJ_CAT_CONTAINER). */
static inline bool obj_is_container(const obj_t *o) {
    return o && o->category == OBJ_CAT_CONTAINER;
}
/* True iff a container is currently closed (non-containers are never closed). */
static inline bool obj_container_closed(const obj_t *o) {
    return obj_is_container(o) && (o->val[1] & CONT_CLOSED);
}
/* Total weight currently inside a container (sums its THING_OBJ children). */
double obj_contained_weight(const obj_t *container);

/* Loads the prototype row for `vnum` (obj_repo_load()) and allocates a fresh
 * in-world obj_t instance from it -- NOT yet attached to any room/being
 * (caller does that via thing_move_to()). Returns NULL if no such vnum
 * exists in the `obj` table. */
obj_t *obj_create_from_proto(int vnum);

/* Detaches (thing_remove_from_parent) and frees an obj_t. Safe to call on a
 * worn/held item too (the caller is responsible for first clearing whatever
 * being_t.equipment[]/held[] slot pointed at it -- obj_destroy() doesn't scan
 * for those, since it has no way to reach an arbitrary being_t from here). */
void obj_destroy(obj_t *o);

/* Collapses one of the original's 60 itemTypeT values (the DB `obj.type`
 * column, verbatim upstream numbering) into a Tobin obj_category_t. Out-of-
 * range values map to OBJ_CAT_OTHER. */
obj_category_t category_for_item_type(int orig_item_type);

/* Sentinel values wear_slot_for_flag() can return alongside a real limb_t
 * index: */
#define WEAR_SLOT_HELD          (-1) /* goes into being_t.held[], not a limb */
#define WEAR_SLOT_NOT_WEARABLE  (-2) /* wear_flag has no Tobin-limb equivalent
                                        (e.g. hands/wrists/back-only items --
                                        this port's 13-limb set doesn't have
                                        those slots, see STATUS.md's Limbs
                                        decision row); carriable, not wearable */
#define WEAR_SLOT_NO_ROOM       (-3) /* wearable in principle, but every
                                        matching slot on `fitter` is already
                                        occupied */

/* True iff wear_flag has the original's ITEM_WEAR_TAKE bit -- an object
 * without it is fixed scenery (`get` refuses it). */
bool obj_takeable(int wear_flag);

/* Maps `wear_flag` (the original's bit layout, see obj_t's field comment) to
 * a limb_t slot on `fitter`, or one of the WEAR_SLOT_* sentinels above.
 * Multi-slot bits (FINGERS/ARMS/LEGS/FEET) pick the first empty of the L/R
 * pair, preferring right. HOLD maps to WEAR_SLOT_HELD; being.c's wear
 * command picks fitter->held[0] or [1] based on handed_right/occupancy. */
int wear_slot_for_flag(int wear_flag, const struct being *fitter);

#endif
