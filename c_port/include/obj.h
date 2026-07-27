/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_OBJ_H
#define TOBIN_OBJ_H

#include <stdbool.h>
#include <stddef.h>

#include "thing.h"

/* C replacement for the TObj slice of misc/obj.h -- objects (Phase 2C).
 * The original collapses 98 obj/ subclasses via 60 `itemTypeT` values;
 * this collapses further, into ~16 generic categories, with a 4-int `val[]`
 * payload whose meaning depends on category (mirrors the original's
 * val0..val3 generic fields) instead of one C++ class per item type.
 *
 * Object PROTOTYPES already exist in the DB -- the upstream seed's `obj`
 * table (db/tobin/obj.sql, thousands of real rows, PK vnum) is read
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
     *   LIGHT:    val[0]=light radius (flavor only, unused) val[1]=max
     *             burn/fuel capacity (-1=unrefuelable, e.g. a torch --
     *             see cmd_light.c's `refuel`) val[2]=current burn/fuel
     *             remaining val[3]=is lit (0/1). Corrected 2026-07-18 --
     *             an earlier placeholder comment here (val[0]=is lit,
     *             val[1]=max fuel, val[2]=remaining) didn't match the
     *             real seeded data at all once `light`/`extinguish`/
     *             `refuel` actually needed to read it; verified against
     *             real obj rows (vnum 100 lamppost: 15/100/100/1, vnum
     *             105 torch: 5/-1/40/0).
     *   FUEL (obj.type==6/ITEM_FUEL, collapses into OBJ_CAT_OTHER --
     *             see cmd_light.c's `refuel`): val[0]=fuel units
     *             remaining val[1]=max fuel units (a fresh brick's own
     *             capacity, e.g. "a small brick of solid fuel" is 40/40).
     *   WEAPON:   val[0]=damage dice count val[1]=damage dice sides
     *   ARMOR:    val[0]=armor class (protection value)
     *   CONTAINER:val[0]=max weight cap.   val[1]=flags(closeable/closed/locked/pickproof) val[2]=key vnum (0=no lock)
     *             CORPSE sub-case (ephemeral, vnum 0, name=="corpse" --
     *             combat.c's corpse-drop-on-death): val[1] is always 0
     *             (never closeable/locked, see combat.c's own comment),
     *             so val[2] is repurposed there for the source being's
     *             `mob_race` (0/NORACE for a dead PC) -- added for
     *             `cook`'s TYPE_CORPSE ingredients (task_cook.h port,
     *             cook.c), matching mob_race's own "raw MOB_RACE_NAMES[]
     *             index" convention rather than inventing a new scale.
     *   DRINK:    val[0]=max units         val[1]=current units
     *             val[2]=liquid type (liquids.h's liquid_info(), verbatim
     *             liqTypeT ordinal from the original -- real seeded rows
     *             already carry this)
     *   FOOD:     val[0]=max units (hunger)val[1]=current units
     *   MONEY:    val[0]=coin amount
     *   KEY:      val[] unused -- a key is matched by its own OBJ VNUM, not
     *             any val[] field (confirmed against real seeded data and
     *             the original's keyCheck(): a door/container's key_num/
     *             val[2] names the vnum a carried KEY object must have).
     *             See cmd_lock.c's `lock`/`unlock`.
     *   MAGIC_DEVICE: val[0]=charges/uses remaining
     *   WRITTEN (board only, obj.type==24/ITEM_BOARD -- see cmd_board.c):
     *             val[0]=minimum level to `read`/`write` this specific
     *             board (0=everyone); real seeded data, e.g. 52 for
     *             "board bulletin wizard immortal". Every OTHER WRITTEN
     *             sub-type (book/note/audio/card deck) leaves val[]
     *             unused, same as any other unlisted category below.
     *   Spell components / holy symbols (identified by keyword "component"
     *             /"symbol", not a dedicated category -- both collapse
     *             into OTHER): val[0]=current charges/strength
     *             val[1]=max charges/strength, same MAGIC_DEVICE spirit
     *             as above. A component spends exactly 1 charge per cast
     *             attempt (cmd_cast.c's consume_component()); a symbol
     *             loses a random 1-2 strength per prayer attempt
     *             (cmd_pray.c/cmd_continue.c's consume_symbol()) --
     *             genuine decay, not a clean counter, matching the
     *             original's TSymbol. Either is destroyed once it hits 0.
     *             tobin_migrations.sql seeds every existing component/
     *             symbol row to 10/10.
     *   Drug items (identified by keyword "drug", same generic-by-
     *             keyword precedent as components/symbols above, not a
     *             dedicated category): val[0]=drug_type_t (drug.h),
     *             val[1]=current charges, val[2]=max charges -- a
     *             different val[] layout than components/symbols
     *             (those have no type to encode) but the same "spend a
     *             charge, destroy at 0" lifecycle, see cmd_smoke.c.
     *   Planted crops (identified by raw_type==OBJ_PLANT_RAW_TYPE, obj_
     *             plant.h -- an ephemeral OBJ_CAT_OTHER object, not a
     *             dedicated category): val[0]=plant type index (0-14),
     *             val[1]=age in growth ticks, val[2]=lifetime fruit
     *             yield, val[3]=yield lost to vermin. See obj_plant.h.
     * Categories not listed leave val[] unused/decorative for now. */
    int val[4];
    double weight;
    int volume;
    int price;
    int max_struct;
    int cur_struct;
    int material;
    bool can_be_seen;

    /* Object maintenance (Sneezy -> Tobin feature audit) -- the upstream
     * seed's own `decay` column (obj.c's decay_time doc precedent: -1 =
     * never decays/OBJ_NOTIMER, 0 = decays THIS tick, >0 = ticks
     * remaining), loaded verbatim at creation for real prototype rows.
     * Ephemeral objects (corpses, severed limbs -- obj_create_ephemeral(),
     * vnum 0, no prototype row to read from) default to -1 here and have
     * it set explicitly by their creator (combat.c) right after creation,
     * same post-creation-field-set precedent as wear_flag/val[] elsewhere
     * in this file. Only decrements while the object sits DIRECTLY in a
     * room (obj_decay_tick(), world_for_each_obj() -- carried/equipped/
     * nested-in-a-container objects are exempt, matching the original's
     * "decay only ticks in rooms" rule verbatim) -- a corpse looted into
     * someone's inventory stops decaying until dropped again. One Tobin
     * decay tick == one obj_decay_tick() pulse-registered call (~60s,
     * same cadence as obj_pool_decay_tick()/mob_ai_tick()), a deliberate
     * mapping of the upstream unit onto Tobin's own slow-tick cadence --
     * the original's own periodic.cc tick rate isn't part of this bundled
     * source, so real seeded `decay` values (a corpse-adjacent few dozen
     * up to several thousand for long-lived world props) land in a
     * broadly similar real-world range either way. */
    int decay_time;

    /* Object maintenance tasks 3-4 (repair-shop economy + self-repair
     * skill, cmd_repair.c) -- per-INSTANCE state, never reset by a fresh
     * `obj_create_from_proto()` spawn of the same vnum (0/empty by
     * default, matching a never-repaired item). `depreciation` climbs by
     * 1 on every successful repair (self or shop) and caps how high
     * `cur_struct` can ever be restored to again (`max_struct -
     * depreciation`, floored -- see cmd_repair.c), the same "repeated
     * repairs wear an item down for good" idea the real upstream's own
     * `TObj::maxFix()`/`getDepreciation()` (misc/repair.cc) uses.
     * `monogram` is the repairer's own name, stamped on by a successful
     * SELF-repair only (not a shop repair) -- purely cosmetic/flavor
     * (shown in `look`), not a discount like the upstream's
     * `isMonogrammed()` halves-material-cost rule (Tobin's repair
     * pricing has no material-purchase step to discount). Both persist
     * across a reconnect via `player_inventory` (obj_repo.c), NOT the
     * `obj` prototype table -- see tobin_migrations.sql's comment on
     * why. */
    int depreciation;
    char monogram[65]; /* matches repair_repo.h's REPAIR_TICKET_MONOGRAM_LEN+1 exactly */

    /* Raw upstream itemTypeT (DB `obj.type`, verbatim) -- unlike `category`
     * above (which collapses many raw types into one bucket, e.g. scroll/
     * wand/staff/potion all become OBJ_CAT_MAGIC_DEVICE), `use` (cmd_use.c,
     * Magic items, Sneezy -> Tobin feature audit) needs to tell those FOUR
     * apart to know whether an item is single-use, targeted, or room-wide.
     * 0 for an ephemeral (non-prototype) object. */
    int raw_type;

    /* Real per-item stat/AC/HP/Vitality bonuses (Magic items, Sneezy ->
     * Tobin feature audit) -- sourced from the upstream-seeded `objaffect`
     * table (vnum, type, mod1, mod2 -- same real, untouched data
     * obj_load_combat_mods() already reads for weapon hit/damroll), loaded
     * ONCE at creation (obj_create_from_proto(), obj.c) and cached here
     * rather than re-queried on every wear/remove or (for AC, a combat hot
     * path) every hit-roll. Only the subset of applyTypeT this port's
     * simplified 6-stat model actually has a home for: APPLY_STR/INT/WIS/
     * DEX/CON/CHA (1/2/3/4/5/31), APPLY_HIT/MOVE (12/14 -- max_hp/
     * max_vit), and APPLY_ARMOR (11, AC -- NOTE the sign flip: upstream's
     * convention is "negative is better" (verified against real seed data,
     * every real row is <= 0), Tobin's own being_total_ac()/obj_armor_ac()
     * convention is "higher is better" (verified against combat.c's hit-
     * roll formula and the mounted-bonus comment) -- aff_ac below is
     * already negated at load time, so callers just add it directly.
     * Every OTHER applyTypeT (BRA/AGI/FOC/PER/KAR/SPE -- extended stats
     * this port's 6-stat model doesn't have; MANA -- no mana pool exists;
     * SPELL/SPELL_EFFECT/LIGHT/NOISE/CAN_BE_SEEN/VISION/PROTECTION/
     * DISCIPLINE/SPELL_HITROLL/CURRENT_HIT/CRIT_FREQUENCY/GARBLE) is
     * real seeded data too, but left un-applied -- an honest Tobin-scale
     * slice, not a silent invention, same as every other "not every
     * upstream field has a Tobin home yet" gap in this file. */
    int aff_str, aff_dex, aff_con, aff_intel, aff_wis, aff_cha;
    int aff_hit, aff_move;
    int aff_ac;
} obj_t;

/* CONTAINER flag bits, stored VERBATIM in val[1] in the original's bit layout
 * (misc/obj.h) so every already-seeded container works with zero migration --
 * same "stored verbatim" precedent as wear_flag. Only the bits Tobin acts on
 * are named here; the original defines a few more (CONT_EMPTYTRAP, etc.) that
 * are decorative for now. */
#define CONT_CLOSEABLE (1 << 0) /* can be opened/closed at all */
#define CONT_PICKPROOF (1 << 1) /* lock cannot be picked (no lockpicking skill yet) */
#define CONT_CLOSED    (1 << 2) /* currently shut */
#define CONT_LOCKED    (1 << 3) /* currently locked -- see cmd_lock.c's `lock`/`unlock` */

/* Armor class this piece contributes if worn (0 for anything else). The
 * upstream seed's `val0` field ("armor class") is uniformly 0 across
 * every real armor row (val0/val1 IS populated for weapons -- dice count/
 * sides -- this is specifically an armor data gap, confirmed by querying
 * the live DB before writing this), so there's no real AC value to read
 * from `obj` itself. UPDATE (Magic items, Sneezy -> Tobin feature audit):
 * `objaffect` -- the same real, already-partially-used table
 * obj_load_combat_mods() reads for weapon hit/damroll -- DOES carry real
 * per-item AC data (`o->aff_ac`, cached at creation, sign already
 * flipped to Tobin's convention -- see obj_t's own doc comment). Real data
 * applies regardless of `category` -- a real vnum-179 test item proved
 * this the hard way: rings/shields/other jewelry-category worn items
 * carry real objaffect AC rows too, not just OBJ_CAT_ARMOR ones, so
 * gating on category silently dropped their bonus. The placeholder
 * weight formula below is the fallback ONLY for true armor-category
 * items with no real objaffect entry (guessing an AC for a ring with no
 * data would be nonsense): heavier armor protects more, scaled so a
 * fully plate-armored character's total across all worn slots lands in
 * a similar range to the hit-roll's other +/-15ish modifiers (see
 * combat.c's being_total_ac() usage). Either way capped at
 * ARMOR_AC_MAX so one absurd item (real or placeholder) can't dominate. */
int obj_armor_ac(const obj_t *o);

/* One-word (color-tagged) structure-condition summary (Object
 * maintenance, Sneezy → Tobin feature audit) -- the real 11-tier ladder
 * AND real per-tier colors ported verbatim from `TObj::equip_condition()`
 * (misc/info.cc, e.g. `"<C>like new<1>"`), not invented -- previously
 * Tobin had its own uncolored 6-tier wording ("is in excellent
 * condition", "has seen some wear", ...) rather than checking the real
 * source first. The returned string already carries Tobin colorstring.c
 * `<letter>` tags (translated to ANSI or stripped downstream per the
 * viewer's color toggle, same as any other pre-colored string). Returns
 * NULL if the prototype never set a max_struct (0 -- most sandbox/test
 * fixtures, some real content), same "nothing meaningful to show"
 * convention as before. Shown in parens right after an item's
 * short_descr wherever one is listed (inventory/equipment), e.g.
 * "a long sword (<C>brand new<1>)". */
const char *obj_condition_word(const obj_t *o);

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

/* Sneezy's `$$g`/`$g` token (misc/show.cc): replaces every occurrence in
 * `text` with `room`'s ground-surface word (room_ground_type(), room.h) --
 * e.g. an object description "The ring lies half-buried in the $$g."
 * reads "...in the sand." on a beach, "...in the street." in a city.
 * Writes into `buf` (size `bufsz`) and returns it; `text` unchanged (no
 * token present) is just copied through. Investigated + added per user
 * request (Session 43) -- not present in the currently-migrated obj/
 * objextra data, but the substitution mechanism itself is now real
 * infrastructure for future hand-authored descriptions to use. */
const char *obj_apply_ground_token(const char *text, const struct room *room,
                                    char *buf, size_t bufsz);

/* Loads the prototype row for `vnum` (obj_repo_load()) and allocates a fresh
 * in-world obj_t instance from it -- NOT yet attached to any room/being
 * (caller does that via thing_move_to()). Returns NULL if no such vnum
 * exists in the `obj` table. */
obj_t *obj_create_from_proto(int vnum);

/* Builds a fresh, non-prototype obj_t instance -- vnum 0 (never backed by a
 * real DB row, never persisted, like other ephemeral in-memory-only state in
 * this port, e.g. `fighting`/`desc`). Pickup-able (WEAR_TAKE) by default. NOT
 * yet attached to any room/being -- caller does that via thing_move_to().
 * Used for one-off flavor objects generated at runtime rather than loaded
 * from a prototype (e.g. a severed limb, combat.c). */
obj_t *obj_create_ephemeral(const char *name, const char *short_descr,
                            const char *long_descr, obj_category_t category);

/* Drops (or grows) a non-takeable ground puddle in `room` (category
 * OBJ_CAT_TRASH, so an ACT_SCAVENGER mob eventually cleans it up --
 * mob_ai.c). For flavor commands/reactions that leave something behind on
 * the floor (`pee`, and the bleeding blood-pool reaction, combat.c). If a
 * puddle of the same `type_tag` ("pee"/"blood") already exists in `room`,
 * it grows a size tier instead of a separate object being created (user,
 * 2026-07-11: "pools should grow in size if multiple puddles of the same
 * type are created in a room") -- puddle -> pool -> large pool, tracked in
 * val[0]. `keywords` are the get/look keywords (should include "puddle"/
 * "pool" plus `type_tag` itself, e.g. "puddle pool pee urine"); `noun` is
 * the plain-text substance name ("pee"/"blood") the size-tier phrase wraps
 * around ("a pool of blood"). Already attaches to `room` -- unlike
 * obj_create_ephemeral(), the caller does NOT also call thing_move_to(). */
void obj_grow_pool(struct room *room, const char *type_tag, const char *keywords,
                    const char *noun);

/* Ages every ground puddle in the world down one size tier, destroying it
 * entirely once it decays past "puddle" -- see obj.c for the full doc
 * comment. Pulse-registered in main.c; also forced by `aitick` for
 * deterministic testing. */
void obj_pool_decay_tick(long pulse_num);

/* Drains 1 burn/fuel unit (val[2]) from every currently-LIT OBJ_CAT_LIGHT
 * object in the world, extinguishing it (val[3]=0) once it hits 0 -- see
 * obj.h's val[] comment and cmd_light.c's `light`/`extinguish`/`refuel`.
 * Silent, same "no message" precedent as obj_pool_decay_tick() (a pure
 * ambient world tick, not tied to any one connected player) -- pulse-
 * registered in main.c at the same ~60s cadence. */
void obj_light_burn_tick(long pulse_num);

/* Decrements `decay_time` for every object sitting DIRECTLY in a room
 * (world_for_each_obj() -- carried/held/worn/nested objects are exempt,
 * see obj_t's own decay_time doc comment), destroying any that reach 0:
 * a container's contents are relocated to the room FIRST (thing_move_to()
 * for each child) so nothing gets silently orphaned, then a room-wide
 * message announces it (unlike obj_pool_decay_tick()/obj_light_burn_
 * tick()'s deliberate silence -- a corpse or item actually vanishing is
 * a real, player-visible event worth announcing, not ambient scenery
 * upkeep). Pulse-registered in main.c at the same ~60s cadence; also
 * forced by `aitick` for deterministic testing. */
void obj_decay_tick(long pulse_num);

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

/* Applies (sign=+1) or reverses (sign=-1) `o`'s cached equipment affects
 * (stat/HP/Vitality -- Magic items, Sneezy -> Tobin feature audit; see
 * obj_t's own doc comment above for where these come from). Used by
 * cmd_wear()/cmd_remove() (cmd_object.c) on the actual wear/remove
 * moment. AC isn't handled here -- being_total_ac()/obj_armor_ac() read
 * o->aff_ac live off whichever items are CURRENTLY equipped, so it never
 * needs an explicit apply/reverse step. See obj_apply_equip_load_affects()
 * below for why RECONNECT deliberately does NOT just call this again. */
void obj_apply_equip_affects(struct being *ch, const obj_t *o, int sign);

/* On reconnect ONLY (player_inventory_load(), obj_repo.c) -- reapplies
 * just the STAT half (STR/DEX/CON/INT/WIS/CHA) of an already-equipped
 * item's affects, deliberately excluding HIT/MOVE. `attrs_t` is only
 * ever persisted at character creation or via the immortal edplayer
 * editor, so a wear-time stat bonus lives purely in memory and vanishes
 * on reconnect unless reapplied here; `progress` (max_hp/max_vit),
 * though, gets saved to DB every ~60s for any connected mortal
 * regardless of what changed (vitals_tick_run(), several commands), so
 * by the time a real session disconnects a HIT/MOVE bonus from a still-
 * worn item is already baked into the saved value -- reapplying it here
 * too would double it, compounding further on every subsequent relog.
 * See obj.c's definition for the one accepted gap this leaves
 * (immortals, who vitals_tick_run() skips entirely). */
void obj_apply_equip_load_affects(struct being *ch, const obj_t *o);

/* Decodes `wear_flag`'s bits into a readable "[ TAKE ] [ BODY ] ..." run
 * (user 2026-07-12: "in stat action flags and wear flags should be
 * readable, not numbers") -- same bracket-per-flag convention as
 * room.h's `room_flag_names()`. The original's own bit layout, kept
 * verbatim including its two never-assigned bits (see obj.c). */
const char *obj_wear_flag_names(int flags, char *buf, size_t size);

/* Decodes the DB `obj.type` column's raw itemTypeT value into its real
 * upstream name ("WEAPON", "MARTIAL_WEAPON", ...) -- user 2026-07-12:
 * "stat obj, get names for type, action_flag". Distinct from
 * category_for_item_type()/obj_category_name() above, which collapse
 * the same value into one of Tobin's 16 coarser gameplay categories;
 * `stat` wants the real seeded value, not the collapsed one. Out-of-
 * range values render as "?". */
const char *obj_type_name(int raw_type);

/* Decodes `action_flag`'s bits (the original's extraFlags bitmask, 32
 * bits) into a readable "[ GLOW ] [ MAGIC ] ..." run, same bracket-per-
 * flag convention as obj_wear_flag_names()/room.h's room_flag_names().
 * Currently loaded-but-unused by Tobin gameplay code (like mob.faction),
 * but `stat` should still show a seeded value honestly. */
const char *obj_action_flag_names(int flags, char *buf, size_t size);

/* Per-bit accessors for `oedit`'s Take Flags / Extra Flags toggle
 * submenus (cmd_edobject.c) -- same "count + name(index)" shape as
 * room.h's room_flag_count()/room_flag_name(), iterating the same
 * WEAR_FLAG_NAMES/OBJ_ACTION_FLAG_NAMES tables obj_wear_flag_names()/
 * obj_action_flag_names() already use for read-only display. */
int obj_wear_flag_count(void);
const char *obj_wear_flag_name(int bit);
int obj_action_flag_count(void);
const char *obj_action_flag_name(int bit);

#endif
