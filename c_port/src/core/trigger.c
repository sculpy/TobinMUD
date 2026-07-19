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

static void do_say(room_t *room, const char *self_name, const char *arg) {
    if (!room)
        return;
    char msg[320];
    snprintf(msg, sizeof(msg), "%s says, '%s'\r\n", self_name ? self_name : "Something", arg);
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

/* Pending `wait`-paused continuations -- see trigger.h's `wait` doc for the
 * design (actor is deliberately NOT preserved; target_type/target_vnum +
 * room_vnum are, so a mob/room's identity is safely re-derived fresh at
 * resume time instead of holding a raw pointer across the pause). Fixed-
 * size like RANDOM_VNUM_SET_MAX below -- a builder-facing tool, not
 * expected to ever need more than a handful of these live at once. */
#define PENDING_MAX 32
typedef struct {
    bool active;
    long resume_at_pulse;
    char target_type[8];
    int target_vnum;
    int room_vnum;
    char remaining[TRIGGER_SCRIPT_MAX];
} pending_trigger_t;
static pending_trigger_t g_pending[PENDING_MAX];

#define TRIGGER_PULSES_PER_SEC 10 /* pulse.h: a pulse is 100ms */
#define TRIGGER_WAIT_MAX_SECS 3600

static void schedule_pending(const char *target_type, int target_vnum, int room_vnum,
                             const char *remaining, int wait_secs, long now_pulse) {
    if (wait_secs < 1)
        wait_secs = 1;
    if (wait_secs > TRIGGER_WAIT_MAX_SECS)
        wait_secs = TRIGGER_WAIT_MAX_SECS;
    for (int i = 0; i < PENDING_MAX; i++) {
        if (g_pending[i].active)
            continue;
        g_pending[i].active = true;
        snprintf(g_pending[i].target_type, sizeof(g_pending[i].target_type), "%s", target_type);
        g_pending[i].target_vnum = target_vnum;
        g_pending[i].room_vnum = room_vnum;
        snprintf(g_pending[i].remaining, sizeof(g_pending[i].remaining), "%s", remaining);
        g_pending[i].resume_at_pulse = now_pulse + (long)wait_secs * TRIGGER_PULSES_PER_SEC;
        return;
    }
    /* Pool full -- dropped silently, same typo-tolerant spirit as an
     * unrecognized verb (trigger.h). */
}

static long g_now_pulse = 0;

/* Runs `script_text` (a newline-separated action list -- either a whole
 * trigger's script, or the tail of one resuming after a `wait`) against
 * this context. `trig` supplies target_type/target_vnum, needed only if a
 * `wait` line is hit (to schedule its continuation) -- may be NULL when
 * resuming (the continuation record already captured what a fresh `wait`
 * inside it would need). Manual line-scanning over the ORIGINAL text
 * (not strtok_r) so hitting `wait` can hand off "everything after this
 * line, verbatim" without needing to un-mutate anything. */
static void run_script(const trigger_t *trig, being_t *actor, room_t *room,
                       const char *self_name, const char *script_text) {
    const char *cursor = script_text;
    while (cursor && *cursor) {
        const char *nl = strchr(cursor, '\n');
        size_t linelen = nl ? (size_t)(nl - cursor) : strlen(cursor);
        if (linelen > 255)
            linelen = 255;
        char line[256];
        memcpy(line, cursor, linelen);
        line[linelen] = '\0';
        const char *next = nl ? nl + 1 : NULL;

        char *p = line;
        while (*p == ' ')
            p++;
        char verb[16];
        int vlen = 0;
        while (p[vlen] && p[vlen] != ' ' && vlen < (int)sizeof(verb) - 1) {
            verb[vlen] = p[vlen];
            vlen++;
        }
        verb[vlen] = '\0';
        const char *arg = p + vlen;
        while (*arg == ' ')
            arg++;

        if (strcasecmp(verb, "wait") == 0) {
            if (trig && next && *next)
                schedule_pending(trig->target_type, trig->target_vnum,
                                 room ? room->vnum : -1, next, atoi(arg), g_now_pulse);
            return; /* stop here regardless -- either scheduled or nothing left to do */
        }
        if (strcasecmp(verb, "echo") == 0)
            do_echo(actor, arg);
        else if (strcasecmp(verb, "echoroom") == 0)
            do_echoroom(actor, room, arg);
        else if (strcasecmp(verb, "emote") == 0)
            do_emote(room, self_name, arg);
        else if (strcasecmp(verb, "say") == 0)
            do_say(room, self_name, arg);
        else if (strcasecmp(verb, "teleport") == 0)
            do_teleport(actor, arg);
        else if (strcasecmp(verb, "give") == 0)
            do_give(actor, arg);
        else if (strcasecmp(verb, "damage") == 0)
            do_damage(actor, arg);
        else if (strcasecmp(verb, "log") == 0)
            do_log(actor, arg);
        /* Unrecognized verbs are silently skipped -- see trigger.h. */

        cursor = next;
    }
}

void trigger_run(const trigger_t *trig, being_t *actor, room_t *room, const char *self_name) {
    if (!trig)
        return;
    run_script(trig, actor, room, self_name, trig->script);
}

/* short_descr may start with a color tag (e.g. "<o>a dirty refuse
 * hauler<1>") -- skip it before capitalizing, same bug class already
 * fixed in cmd_look.c/cmd_object.c/cmd_scan.c/mob_ai.c's own cap_first()
 * copies (each file keeps its own rather than sharing one). */
static const char *cap_mob_name(const char *short_descr, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%s", short_descr);
    size_t i = 0;
    while (buf[i] == '<' && buf[i + 1] != '\0' && buf[i + 2] == '>')
        i += 3;
    if (buf[i])
        buf[i] = (char)toupper((unsigned char)buf[i]);
    return buf[0] ? buf : NULL;
}

void trigger_pending_tick(long pulse_num) {
    g_now_pulse = pulse_num;
    for (int i = 0; i < PENDING_MAX; i++) {
        if (!g_pending[i].active || g_pending[i].resume_at_pulse > pulse_num)
            continue;

        pending_trigger_t p = g_pending[i];
        g_pending[i].active = false; /* free the slot before running -- a
                                       * fresh `wait` inside p.remaining is
                                       * free to reuse it (or any slot) */

        room_t *room = world_get_room(p.room_vnum);
        if (!room)
            continue; /* room gone -- drop silently */

        const char *self_name = NULL;
        char capbuf[128];
        if (strcasecmp(p.target_type, "mob") == 0) {
            being_t *self = NULL;
            for (thing_t *t = room->base.stuff_head; t; t = t->stuff_next) {
                if (t->kind == THING_MOB && t->id == p.target_vnum) {
                    self = (being_t *)t;
                    break;
                }
            }
            if (!self)
                continue; /* mob no longer here -- drop silently */
            self_name = cap_mob_name(self->base.short_descr, capbuf, sizeof(capbuf));
        }
        /* target_type "room": self_name stays NULL, same "Something"
         * fallback do_emote()/do_say() already use. */

        trigger_t fake_trig = {0};
        snprintf(fake_trig.target_type, sizeof(fake_trig.target_type), "%s", p.target_type);
        fake_trig.target_vnum = p.target_vnum;

        run_script(&fake_trig, NULL, room, self_name, p.remaining);
    }
}

void trigger_pending_force_all(void) {
    for (int i = 0; i < PENDING_MAX; i++) {
        if (g_pending[i].active)
            g_pending[i].resume_at_pulse = g_now_pulse; /* due "now" on the next tick */
    }
    trigger_pending_tick(g_now_pulse);
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
        char capbuf[128];
        const char *name = cap_mob_name(m->base.short_descr, capbuf, sizeof(capbuf));
        trigger_run(&trigs[i], NULL, m->base.roomp, name);
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
