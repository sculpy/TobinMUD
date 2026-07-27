/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "obj_plant.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "descriptor.h"
#include "obj.h"
#include "room.h"
#include "thing.h"
#include "world.h"

typedef struct {
    int seed_vnum;
    int fruit_vnum;
    int lifespan_ticks; /* see obj_plant.h's doc comment -- compressed from
                            the original's real-year lifespans, ordering
                            preserved */
    const char *keyword;    /* plain get/look keyword, e.g. "apple tree" */
    const char *display;    /* color-tagged singular noun, e.g. "<r>apple<1> tree" */
} plant_type_t;

/* Verbatim seed_to_plant() order (obj_plant.cc) -- index IS val[0]. */
static const plant_type_t PLANT_TYPES[PLANT_TYPE_COUNT] = {
    { 13880, 14348, 180, "tomato plant",      "<r>tomato<1> <g>plant<1>" },
    { 13881, 28917, 260, "red rose bush",     "<r>red<1> rose bush" },
    { 13882,  8936, 220, "apple tree",        "<r>apple<1> tree" },
    { 13883, 28918, 260, "white rose bush",   "<W>white<1> rose bush" },
    { 13884, 28919, 260, "yellow rose bush",  "<Y>yellow<1> rose bush" },
    { 13885,   432, 220, "orange tree",       "<o>orange<1> tree" },
    { 13886,    13,  60, "money tree",        "<g>money<1> tree" },
    { 34213, 34212,  45, "pipeweed bush",     "<w>pipe<g>weed<1><o> bush<1>" },
    { 33526, 33507,  40, "pumpkin vine",      "<o>pumpkin<1> vine" },
    { 35485, 33508,  40, "turnip plant",      "<g>turnip<1> plant" },
    { 35499, 33525,  40, "lettuce plant",     "<G>lettuce<1> plant" },
    { 33600, 33601,  45, "pot plant",         "<g>pot<1> plant" },
    { 34738, 34737,  45, "catnip plant",      "<p>catnip<1> <g>plant<1>" },
    { 29410, 29405,  35, "candy heart tree",  "<P>candy heart<1> tree" },
    { 34216, 34215, 260, "gray grape vine",   "vine of <k>gray grapes<1>" },
};

/* Looks up which PLANT_TYPES entry a given seed item's vnum sows; returns
 * false (leaving *out_type untouched) if seed_vnum isn't a known seed. */
bool plant_type_for_seed_vnum(int seed_vnum, int *out_type) {
    for (int i = 0; i < PLANT_TYPE_COUNT; i++) {
        if (PLANT_TYPES[i].seed_vnum == seed_vnum) {
            *out_type = i;
            return true;
        }
    }
    return false;
}

/* Vnum of the fruit/harvest item a mature plant of `type` yields; 0 for an
 * out-of-range type. */
int plant_fruit_vnum(int type) {
    if (type < 0 || type >= PLANT_TYPE_COUNT)
        return 0;
    return PLANT_TYPES[type].fruit_vnum;
}

/* Same stage-boundary formula as TPlant::updateDesc() (age/10, capped at
 * 3), a 4th stage (withered, index 4) added once age exceeds the type's
 * (compressed) lifespan. */
static int plant_stage(const obj_t *o) {
    int type = o->val[0];
    int age = o->val[1];
    if (type < 0 || type >= PLANT_TYPE_COUNT)
        type = 0;
    if (age > PLANT_TYPES[type].lifespan_ticks)
        return 4;
    int stage = age / 10;
    return stage > 3 ? 3 : stage;
}

/* Rewrites a plant object's keyword/short-desc/long-desc to match its
 * current type and growth stage (dirt mound -> sprout -> small -> mature ->
 * withered). Called after plant_age changes so the room always shows the
 * right description without a separate "look" recompute path. */
void obj_plant_refresh_desc(obj_t *o) {
    static const char *const KEYWORDS[5] = {
        "mound dirt", "sprout tiny", "plant small %s", "plant %s", "plant withered %s",
    };
    static const char *const NAMES[5] = {
        "a small mound of <o>dirt<1>", "a tiny sprout", "a small %s", "a %s",
        "an old, withered %s",
    };
    static const char *const DESCS[5] = {
        "A small mound of <o>dirt<1> is here.",
        "A tiny sprout is growing here.",
        "A small %s is here.",
        "A %s is here.",
        "An old, withered %s is here.",
    };

    int type = o->val[0];
    if (type < 0 || type >= PLANT_TYPE_COUNT)
        type = 0;
    int stage = plant_stage(o);
    const char *display = PLANT_TYPES[type].display;
    const char *keyword = PLANT_TYPES[type].keyword;

    char buf[256];
    if (stage >= 2) {
        snprintf(buf, sizeof(buf), KEYWORDS[stage], keyword);
        snprintf(o->base.name, sizeof(o->base.name), "%.63s", buf);
        snprintf(buf, sizeof(buf), NAMES[stage], display);
        snprintf(o->base.short_descr, sizeof(o->base.short_descr), "%.127s", buf);
        snprintf(buf, sizeof(buf), DESCS[stage], display);
        snprintf(o->long_descr, sizeof(o->long_descr), "%s", buf);
    } else {
        snprintf(o->base.name, sizeof(o->base.name), "%s", KEYWORDS[stage]);
        snprintf(o->base.short_descr, sizeof(o->base.short_descr), "%s", NAMES[stage]);
        snprintf(o->long_descr, sizeof(o->long_descr), "%s", DESCS[stage]);
    }
}

/* Spawns a fresh plant (starting life as a mound of dirt, age 0) in `room`
 * and drops it on the floor -- the end result of the planting_tick_run()
 * sow sequence in planting.c finishing its final tick. */
void obj_plant_create(room_t *room, int type) {
    if (!room || type < 0 || type >= PLANT_TYPE_COUNT)
        return;

    obj_t *o = obj_create_ephemeral("mound dirt", "a small mound of <o>dirt<1>",
                                     "A small mound of <o>dirt<1> is here.", OBJ_CAT_OTHER);
    if (!o)
        return;
    o->raw_type = OBJ_PLANT_RAW_TYPE;
    o->wear_flag = 0; /* scenery -- not takeable, matches obj_grow_pool()'s puddle precedent */
    o->val[0] = type; /* planttype */
    o->val[1] = 0;     /* plantage */
    o->val[2] = 0;     /* plantyield */
    o->val[3] = 0;     /* verminated */
    obj_plant_refresh_desc(o);

    thing_move_to(&o->base, &room->base);
}

/* `world_for_each_obj()` only visits room-floor objects, so `o->base.parent`
 * is guaranteed to be a room_t (same idiom as obj.c's decay_visit()). */
static void plant_growth_visit(obj_t *o) {
    if (o->raw_type != OBJ_PLANT_RAW_TYPE)
        return;
    if (o->decay_time >= 0)
        return; /* already withered and handed off to obj_decay_tick() */

    o->val[1] += 1 + rand() % 3; /* plantage, TPlant::updateAge()'s number(1,3) */
    int stage_before = plant_stage(o);
    obj_plant_refresh_desc(o);
    int stage = plant_stage(o);
    room_t *r = (room_t *)o->base.parent;
    (void)stage_before;

    if (stage == 4) {
        /* Withered -- hand off to the existing obj_decay_tick() machinery
         * rather than destroying it here directly (same "set decay_time,
         * let the existing decay pass finish it off" reuse obj.h's own
         * decay_time doc comment establishes). A short countdown so the
         * withered description is visible for a little while first. */
        o->decay_time = 5;
        return;
    }

    if (stage < 2)
        return; /* dirt mound / sprout -- too young to yield fruit yet */

    /* Mature (stage 3): ~25% chance/tick while <=4 fruit already dropped
     * nearby this cycle; withered-but-not-yet-decayed (stage 4 handled
     * above) never reaches here. TPlant::updateDesc()'s !number(0,3)/
     * !number(0,11) rolls, ported as flat percentages. */
    int chance = (stage == 3) ? 25 : 8;
    if (rand() % 100 >= chance)
        return;

    int count = 0;
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next)
        if (t->kind == THING_OBJ && ((obj_t *)t)->vnum == PLANT_TYPES[o->val[0]].fruit_vnum)
            count++;
    if (count > 4)
        return;

    /* Small chance the yield is lost to vermin instead of actually
     * dropping -- TPlant's own `verminated` counter, simplified to a flat
     * 10% roll rather than a separate pest-spawn sub-system. */
    if (rand() % 100 < 10) {
        o->val[3]++; /* verminated */
        return;
    }

    obj_t *fruit = obj_create_from_proto(PLANT_TYPES[o->val[0]].fruit_vnum);
    if (!fruit)
        return;
    thing_move_to(&fruit->base, &r->base);
    o->val[2]++; /* plantyield */

    char cap[128];
    snprintf(cap, sizeof(cap), "%s", o->base.short_descr[0] ? o->base.short_descr : o->base.name);
    cap[0] = (char)toupper((unsigned char)cap[0]);
    char msg[320];
    const char *fruit_label = fruit->base.short_descr[0] ? fruit->base.short_descr : fruit->base.name;
    snprintf(msg, sizeof(msg), "%.127s yields %.127s.\r\n", cap, fruit_label);
    descriptor_room_echo(r, NULL, msg);
}

/* Periodic hook (registered with the pulse scheduler) that ages every
 * planted object in the world one step via plant_growth_visit(); this is
 * the sole driver of plants sprouting, maturing, fruiting, and withering. */
void obj_plant_growth_tick(long pulse_num) {
    (void)pulse_num;
    world_for_each_obj(plant_growth_visit);
}
