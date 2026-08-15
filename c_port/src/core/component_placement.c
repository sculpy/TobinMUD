/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "component_placement.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "descriptor.h"
#include "log.h"
#include "obj.h"
#include "gametime.h"
#include "room.h"
#include "thing.h"
#include "weather.h"
#include "world.h"

/* A place action spawns the reagent; a remove action destroys it. Stored
 * as 0/1 so the DB column can be a plain ENUM('place','remove'). */
#define CACT_PLACE 0
#define CACT_REMOVE 1

typedef struct {
    int action;        /* CACT_PLACE / CACT_REMOVE */
    int room1;         /* low room vnum of the range */
    int room2;         /* high room vnum, or -1 for a single room */
    int comp_vnum;     /* the reagent object to spawn/remove */
    int chance;        /* percent chance to fire on a matching tick (1-100) */
    int hour1;         /* window start hour 0-23, or -1 for "any hour" */
    int hour2;         /* window end hour (exclusive), or -1 for "just hour1" */
    int weather;       /* bitmask of (1<<weather_t) states, or -1 for "any" */
    int max_per_room;  /* don't place if the room already holds this many */
    char message[256]; /* room echo on a successful place/remove ("" = none) */
} placement_t;

static placement_t g_rules[512];
static int g_rule_count = 0;

/* True iff the current game hour falls inside a rule's [hour1,hour2) window.
 * hour1 == -1 means "any hour". hour2 == -1 means "exactly hour1". A window
 * that wraps past midnight (hour1 > hour2, e.g. 22..4) is honored. */
static bool hour_in_window(int hour1, int hour2) {
    if (hour1 < 0)
        return true;
    int now = gametime_hour();
    if (hour2 < 0)
        return now == hour1;
    if (hour1 <= hour2)
        return now >= hour1 && now < hour2;
    return now >= hour1 || now < hour2; /* wraps midnight */
}

/* True iff the current world weather satisfies a rule's mask (-1 = any). */
static bool weather_matches(int mask) {
    if (mask < 0)
        return true;
    return (mask & (1 << (int)weather_current())) != 0;
}

/* Count how many instances of `vnum` are sitting on room `r`'s floor. */
static int count_on_floor(room_t *r, int vnum) {
    int n = 0;
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next)
        if (t->kind == THING_OBJ && ((obj_t *)t)->vnum == vnum)
            n++;
    return n;
}

/* Pick a room in [room1,room2] that actually exists. Single-room rules
 * (room2 < 0) just resolve room1. For a range, try a few random draws
 * (matching Sneezy's own bounded retry) so placement spreads across the
 * zone rather than always hitting the low end. Returns NULL if none load. */
static room_t *pick_room(const placement_t *p) {
    if (p->room2 < 0 || p->room2 <= p->room1)
        return world_get_room(p->room1);
    for (int tries = 0; tries < 20; tries++) {
        int v = p->room1 + rand() % (p->room2 - p->room1 + 1);
        room_t *r = world_get_room(v);
        if (r)
            return r;
    }
    return NULL;
}

/* Spawn one reagent onto `r`'s floor and echo the rule's message, if any. */
static void do_place(const placement_t *p, room_t *r) {
    int cap = p->max_per_room > 0 ? p->max_per_room : 1;
    if (count_on_floor(r, p->comp_vnum) >= cap)
        return;
    obj_t *o = obj_create_from_proto(p->comp_vnum);
    if (!o)
        return;
    thing_move_to(&o->base, &r->base);
    if (p->message[0]) {
        char msg[288];
        snprintf(msg, sizeof(msg), "%s\r\n", p->message);
        descriptor_room_echo(r, NULL, msg);
    }
    log_info("component_placement: placed obj %d in room %d.", p->comp_vnum,
             r->vnum);
}

/* Destroy every matching reagent on `r`'s floor and echo once if any went. */
static void do_remove(const placement_t *p, room_t *r) {
    bool any = false;
    thing_t *t = r->base.stuff_head;
    while (t) {
        thing_t *next = t->stuff_next;
        if (t->kind == THING_OBJ && ((obj_t *)t)->vnum == p->comp_vnum) {
            obj_destroy((obj_t *)t);
            any = true;
        }
        t = next;
    }
    if (any && p->message[0]) {
        char msg[288];
        snprintf(msg, sizeof(msg), "%s\r\n", p->message);
        descriptor_room_echo(r, NULL, msg);
    }
}

void component_placement_load(void) {
    g_rule_count = 0;
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db) {
        log_error("component_placement_load: could not open DB; wild "
                  "component foraging disabled.");
        return;
    }
    if (db_query(db,
                 "select action, room1, room2, comp_vnum, chance, hour1, "
                 "hour2, weather, max_per_room, message from "
                 "component_placement where enabled=1 order by id")) {
        while (db_fetch_row(db)) {
            if (g_rule_count >= (int)(sizeof(g_rules) / sizeof(g_rules[0])))
                break;
            placement_t *p = &g_rules[g_rule_count];
            const char *act = db_get(db, "action");
            p->action = (act && strcasecmp(act, "remove") == 0) ? CACT_REMOVE
                                                                : CACT_PLACE;
            p->room1 = atoi(db_get(db, "room1"));
            p->room2 = atoi(db_get(db, "room2"));
            p->comp_vnum = atoi(db_get(db, "comp_vnum"));
            p->chance = atoi(db_get(db, "chance"));
            p->hour1 = atoi(db_get(db, "hour1"));
            p->hour2 = atoi(db_get(db, "hour2"));
            p->weather = atoi(db_get(db, "weather"));
            p->max_per_room = atoi(db_get(db, "max_per_room"));
            const char *m = db_get(db, "message");
            snprintf(p->message, sizeof(p->message), "%s", m ? m : "");
            g_rule_count++;
        }
    } else {
        log_error("component_placement_load: query failed (table missing?); "
                  "wild component foraging disabled.");
    }
    db_close(db);
    log_info("component_placement_load: loaded %d placement rule(s).",
             g_rule_count);
}

void component_placement_tick(long pulse_num) {
    (void)pulse_num;
    for (int i = 0; i < g_rule_count; i++) {
        placement_t *p = &g_rules[i];
        if (!hour_in_window(p->hour1, p->hour2))
            continue;
        if (!weather_matches(p->weather))
            continue;
        if (p->chance < 100 && (rand() % 100) >= p->chance)
            continue;
        if (p->action == CACT_REMOVE) {
            /* Removal sweeps the whole range, not one random room. */
            int hi = (p->room2 < 0) ? p->room1 : p->room2;
            for (int v = p->room1; v <= hi; v++) {
                room_t *r = world_get_room(v);
                if (r)
                    do_remove(p, r);
            }
        } else {
            room_t *r = pick_room(p);
            if (r)
                do_place(p, r);
        }
    }
}
