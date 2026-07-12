/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "trigger.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "descriptor.h"
#include "log.h"
#include "obj.h"
#include "obj_repo.h"
#include "room.h"
#include "room_repo.h"
#include "thing.h"
#include "world.h"

static void do_echo(being_t *actor, const char *arg) {
    if (!actor || !actor->desc)
        return;
    char msg[256];
    snprintf(msg, sizeof(msg), "%s\r\n", arg);
    descriptor_send(actor->desc, msg);
}

static void do_echoroom(being_t *actor, room_t *room, const char *arg) {
    if (!room)
        return;
    char msg[256];
    snprintf(msg, sizeof(msg), "%s\r\n", arg);
    descriptor_room_echo(room, actor, msg);
}

static void do_emote(room_t *room, const char *self_name, const char *arg) {
    if (!room)
        return;
    char msg[320];
    snprintf(msg, sizeof(msg), "%s %s\r\n", self_name ? self_name : "Something", arg);
    descriptor_room_echo(room, NULL, msg);
}

static void do_teleport(being_t *actor, const char *arg) {
    if (!actor)
        return;
    int vnum = atoi(arg);
    room_t *dest = world_get_room(vnum);
    if (!dest) {
        dest = room_repo_load(vnum);
        if (dest)
            world_register_room(dest);
    }
    if (!dest)
        return;
    thing_set_room(&actor->base, dest);
}

static void do_give(being_t *actor, const char *arg) {
    if (!actor)
        return;
    int vnum = atoi(arg);
    obj_t *o = obj_create_from_proto(vnum);
    if (!o)
        return;
    thing_move_to(&o->base, &actor->base);
    if (actor->base.kind == THING_PC)
        player_inventory_save(actor->player_id, actor);
}

static void do_damage(being_t *actor, const char *arg) {
    if (!actor)
        return;
    int dmg = atoi(arg);
    if (dmg <= 0)
        return;
    actor->progress.hp -= dmg;
    if (actor->progress.hp < 1)
        actor->progress.hp = 1; /* non-lethal, same limitation drink's poison accepted */
}

static void do_log(being_t *actor, const char *arg) {
    game_log(LOG_SILENT, "trigger: %s [%s]", arg, actor ? actor->base.name : "no actor");
}

void trigger_run(const trigger_t *trig, being_t *actor, room_t *room, const char *self_name) {
    if (!trig)
        return;

    char script[TRIGGER_SCRIPT_MAX];
    snprintf(script, sizeof(script), "%s", trig->script);

    char *saveptr = NULL;
    char *line = strtok_r(script, "\n", &saveptr);
    while (line) {
        while (*line == ' ')
            line++;
        char verb[16];
        int vlen = 0;
        while (line[vlen] && line[vlen] != ' ' && vlen < (int)sizeof(verb) - 1) {
            verb[vlen] = line[vlen];
            vlen++;
        }
        verb[vlen] = '\0';
        const char *arg = line + vlen;
        while (*arg == ' ')
            arg++;

        if (strcasecmp(verb, "echo") == 0)
            do_echo(actor, arg);
        else if (strcasecmp(verb, "echoroom") == 0)
            do_echoroom(actor, room, arg);
        else if (strcasecmp(verb, "emote") == 0)
            do_emote(room, self_name, arg);
        else if (strcasecmp(verb, "teleport") == 0)
            do_teleport(actor, arg);
        else if (strcasecmp(verb, "give") == 0)
            do_give(actor, arg);
        else if (strcasecmp(verb, "damage") == 0)
            do_damage(actor, arg);
        else if (strcasecmp(verb, "log") == 0)
            do_log(actor, arg);
        /* Unrecognized verbs are silently skipped -- see trigger.h. */

        line = strtok_r(NULL, "\n", &saveptr);
    }
}

/* Gate for the two visitors below: which vnums actually have a "random"
 * trigger at all, refreshed once per trigger_random_tick() call rather than
 * queried per mob/room (see trigger_repo_random_vnums()'s doc comment --
 * this is the fix for the "every mob/room does a DB round trip every tick"
 * perf bug, user 2026-07-11: "not all mobs need to wander every aitick"). */
#define RANDOM_VNUM_SET_MAX 256
static int g_mob_random_vnums[RANDOM_VNUM_SET_MAX];
static int g_mob_random_count = 0;
static int g_room_random_vnums[RANDOM_VNUM_SET_MAX];
static int g_room_random_count = 0;

static bool vnum_in_set(const int *set, int count, int vnum) {
    for (int i = 0; i < count; i++)
        if (set[i] == vnum)
            return true;
    return false;
}

static void random_visit_mob(being_t *m) {
    if (m->base.kind != THING_MOB || !m->base.roomp)
        return;
    if (!vnum_in_set(g_mob_random_vnums, g_mob_random_count, m->base.id))
        return;

    trigger_t trigs[8];
    int n = trigger_repo_load_for("mob", m->base.id, "random", trigs, 8);
    for (int i = 0; i < n; i++) {
        if (rand() % 100 >= trigs[i].chance_pct)
            continue;
        /* short_descr may start with a color tag (e.g. "<o>a dirty refuse
         * hauler<1>") -- skip it before capitalizing, same bug class already
         * fixed in cmd_look.c/cmd_object.c/cmd_scan.c/mob_ai.c's own
         * cap_first() copies (each file keeps its own rather than sharing
         * one). Without this, toupper() hits '<' (a no-op) and the real
         * first letter stays lowercase -- exactly the "a dirty refuse
         * hauler grumbles..." bug seen live. */
        char capbuf[128];
        snprintf(capbuf, sizeof(capbuf), "%s", m->base.short_descr);
        size_t ci = 0;
        while (capbuf[ci] == '<' && capbuf[ci + 1] != '\0' && capbuf[ci + 2] == '>')
            ci += 3;
        if (capbuf[ci])
            capbuf[ci] = (char)toupper((unsigned char)capbuf[ci]);
        trigger_run(&trigs[i], NULL, m->base.roomp, capbuf[0] ? capbuf : NULL);
    }
}

static void random_visit_room(room_t *r) {
    if (!vnum_in_set(g_room_random_vnums, g_room_random_count, r->vnum))
        return;

    trigger_t trigs[8];
    int n = trigger_repo_load_for("room", r->vnum, "random", trigs, 8);
    for (int i = 0; i < n; i++) {
        if (rand() % 100 >= trigs[i].chance_pct)
            continue;
        trigger_run(&trigs[i], NULL, r, NULL);
    }
}

void trigger_random_tick(long pulse_num) {
    (void)pulse_num;
    g_mob_random_count = trigger_repo_random_vnums("mob", g_mob_random_vnums, RANDOM_VNUM_SET_MAX);
    g_room_random_count = trigger_repo_random_vnums("room", g_room_random_vnums, RANDOM_VNUM_SET_MAX);
    if (g_mob_random_count > 0)
        world_for_each_mob(random_visit_mob);
    if (g_room_random_count > 0)
        world_for_each_room(random_visit_room);
}
