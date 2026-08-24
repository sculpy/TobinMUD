/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
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
#include "trigger_script.h"
#include "world.h"

/* This is the fixed action-vocabulary half of the DG Scripts-style
 * language (trigger_script.h/.c owns the interpreter core: %var%
 * substitution, if/while/switch/break, set/unset/eval/global). Called back
 * from trig_script_exec() for any verb it doesn't itself recognize as
 * control flow. Same seven actions as before the 2026-07-25 revamp, just
 * now %var%-substituted before arriving here. */
void trigger_dispatch_action(trig_ctx_t *ctx, const char *verb, const char *arg) {
    being_t *actor = ctx->actor;
    room_t *room = ctx->room;
    const char *self_name = ctx->self_name;

    if (strcasecmp(verb, "echo") == 0) {
        if (!actor || !actor->desc)
            return;
        char msg[256];
        snprintf(msg, sizeof(msg), "%s\r\n", arg);
        descriptor_send(actor->desc, msg);
    } else if (strcasecmp(verb, "echoroom") == 0) {
        if (!room)
            return;
        char msg[256];
        snprintf(msg, sizeof(msg), "%s\r\n", arg);
        descriptor_room_echo(room, actor, msg);
    } else if (strcasecmp(verb, "emote") == 0) {
        if (!room)
            return;
        char msg[320];
        snprintf(msg, sizeof(msg), "%s %s\r\n", self_name ? self_name : "Something", arg);
        descriptor_room_echo(room, NULL, msg);
    } else if (strcasecmp(verb, "say") == 0) {
        if (!room)
            return;
        char msg[320];
        snprintf(msg, sizeof(msg), "%s says, '%s'\r\n", self_name ? self_name : "Something", arg);
        descriptor_room_echo(room, NULL, msg);
    } else if (strcasecmp(verb, "teleport") == 0) {
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
    } else if (strcasecmp(verb, "give") == 0) {
        if (!actor)
            return;
        int vnum = atoi(arg);
        obj_t *o = obj_create_from_proto(vnum);
        if (!o)
            return;
        thing_move_to(&o->base, &actor->base);
        if (actor->base.kind == THING_PC)
            player_inventory_save(actor->player_id, actor);
    } else if (strcasecmp(verb, "damage") == 0) {
        if (!actor)
            return;
        int dmg = atoi(arg);
        if (dmg <= 0)
            return;
        actor->progress.hp -= dmg;
        if (actor->progress.hp < 1)
            actor->progress.hp = 1; /* non-lethal, same limitation drink's poison accepted */
    } else if (strcasecmp(verb, "log") == 0) {
        game_log(LOG_SILENT, "trigger: %s [%s]", arg, actor ? actor->base.name : "no actor");
    }
    /* Unrecognized verbs are silently skipped -- see trigger.h. */
}

/* Pending `wait`-paused continuations. Unlike the pre-revamp version, the
 * FULL variable scope (`set`/`eval`/`global`-assigned locals) now survives
 * the pause -- only `actor` itself is still deliberately NOT preserved
 * (may have disconnected/died/moved away by the time it resumes; room/
 * self_name are safely re-derived fresh, same as before). `resume_pc` +
 * a copy of the original script text (so line indices stay stable, see
 * trig_script_split()'s doc comment) replace the old "raw remaining text"
 * scheme, since the interpreter now needs real line indices to resume
 * inside a loop correctly instead of always starting a fresh top-level
 * scan. Fixed-size like RANDOM_VNUM_SET_MAX below -- a builder-facing
 * tool, never expected to need more than a handful of these live at once. */
#define PENDING_MAX 32
typedef struct {
    bool active;
    long resume_at_pulse;
    char target_type[8];
    int target_vnum;
    int room_vnum;
    int resume_pc;
    char full_script[TRIGGER_SCRIPT_MAX];
    trig_var_t vars[TRIG_VAR_MAX];
    int var_count;
} pending_trigger_t;
static pending_trigger_t g_pending[PENDING_MAX];

#define TRIGGER_PULSES_PER_SEC 10 /* pulse.h: a pulse is 100ms */

static void schedule_pending(const trigger_t *trig, int room_vnum, const trig_ctx_t *ctx,
                             int resume_pc, int wait_secs, long now_pulse) {
    for (int i = 0; i < PENDING_MAX; i++) {
        if (g_pending[i].active)
            continue;
        g_pending[i].active = true;
        snprintf(g_pending[i].target_type, sizeof(g_pending[i].target_type), "%s", trig->target_type);
        g_pending[i].target_vnum = trig->target_vnum;
        g_pending[i].room_vnum = room_vnum;
        g_pending[i].resume_pc = resume_pc;
        snprintf(g_pending[i].full_script, sizeof(g_pending[i].full_script), "%s", trig->script);
        memcpy(g_pending[i].vars, ctx->vars, sizeof(g_pending[i].vars));
        g_pending[i].var_count = ctx->var_count;
        g_pending[i].resume_at_pulse = now_pulse + (long)wait_secs * TRIGGER_PULSES_PER_SEC;
        return;
    }
    /* Pool full -- dropped silently, same typo-tolerant spirit as an
     * unrecognized verb (trigger.h). */
}

static long g_now_pulse = 0;

/* Top-level entry point for firing a trigger: splits its script into lines
 * and runs it from the top via trig_script_exec(). If the script hits a
 * `wait`, the remaining lines (plus the full variable scope) are handed off
 * to schedule_pending() to resume later rather than blocking here -- the
 * game loop can't stall waiting on a mob's script. Called from wherever a
 * trigger condition matches (command/speech/random ticks, etc). */
void trigger_run(const trigger_t *trig, being_t *actor, room_t *room, const char *self_name) {
    if (!trig)
        return;

    char buf[TRIGGER_SCRIPT_MAX];
    char *lines[TRIG_LINES_MAX];
    int nlines = trig_script_split(trig->script, buf, sizeof(buf), lines);

    trig_ctx_t ctx = {0};
    ctx.actor = actor;
    ctx.room = room;
    ctx.self_name = self_name;
    ctx.arg = trig->match_text[0] ? trig->match_text : NULL;

    int resume_pc, wait_secs;
    trig_exec_result_t r = trig_script_exec(&ctx, lines, nlines, 0, &resume_pc, &wait_secs);
    if (r == TRIG_EXEC_WAIT)
        schedule_pending(trig, room ? room->vnum : -1, &ctx, resume_pc, wait_secs, g_now_pulse);
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

/* Periodic hook (registered with the pulse scheduler) that resumes any
 * pending `wait`-paused trigger scripts whose resume_at_pulse has arrived.
 * Re-derives room/self_name fresh (the mob/room may no longer be there, in
 * which case the resume is dropped silently) since `actor` is deliberately
 * not preserved across a wait -- see the pending_trigger_t doc comment
 * above. A script that hits another `wait` mid-resume gets rescheduled the
 * same way trigger_run() does. */
void trigger_pending_tick(long pulse_num) {
    g_now_pulse = pulse_num;
    for (int i = 0; i < PENDING_MAX; i++) {
        if (!g_pending[i].active || g_pending[i].resume_at_pulse > pulse_num)
            continue;

        pending_trigger_t p = g_pending[i];
        g_pending[i].active = false; /* free the slot before running -- a
                                       * fresh `wait` inside can reuse it
                                       * (or any slot) */

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
         * fallback trigger_dispatch_action()'s emote/say use. */

        char buf[TRIGGER_SCRIPT_MAX];
        char *lines[TRIG_LINES_MAX];
        int nlines = trig_script_split(p.full_script, buf, sizeof(buf), lines);

        trig_ctx_t ctx = {0};
        ctx.actor = NULL; /* deliberately not preserved -- see doc comment above */
        ctx.room = room;
        ctx.self_name = self_name;
        ctx.arg = NULL;
        memcpy(ctx.vars, p.vars, sizeof(ctx.vars));
        ctx.var_count = p.var_count;

        int resume_pc, wait_secs;
        trigger_t fake_trig = {0};
        snprintf(fake_trig.target_type, sizeof(fake_trig.target_type), "%s", p.target_type);
        fake_trig.target_vnum = p.target_vnum;
        snprintf(fake_trig.script, sizeof(fake_trig.script), "%s", p.full_script);

        trig_exec_result_t r = trig_script_exec(&ctx, lines, nlines, p.resume_pc, &resume_pc, &wait_secs);
        if (r == TRIG_EXEC_WAIT)
            schedule_pending(&fake_trig, p.room_vnum, &ctx, resume_pc, wait_secs, g_now_pulse);
    }
}

/* Immediately fires every currently-pending waiting trigger, regardless of
 * how much of their wait timer is left, by marking them all due "now" and
 * running one tick. Used for things like a shutdown or zone reset where
 * waiting for scripts to time out naturally isn't acceptable. */
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

/* Linear membership check against one of the g_mob_random_vnums /
 * g_room_random_vnums arrays -- small, per-tick-refreshed sets, so a plain
 * scan beats maintaining a hash/sorted structure. */
static bool vnum_in_set(const int *set, int count, int vnum) {
    for (int i = 0; i < count; i++)
        if (set[i] == vnum)
            return true;
    return false;
}

/* world_for_each_mob() visitor: if `m`'s vnum has a "random" trigger
 * registered (per the gate set refreshed by trigger_random_tick()), rolls
 * each one's chance_pct and fires it. Skipped entirely for vnums with no
 * random trigger at all, which is the whole point of the gate set -- most
 * mobs don't need this check every tick. */
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

/* world_for_each_room() visitor -- same idea as random_visit_mob() above,
 * for rooms with a "random" trigger. */
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

/* Periodic hook (registered with the pulse scheduler) that drives "random"
 * triggers: refreshes the mob/room vnum gate sets from the DB once, then
 * visits every live mob and room to roll their random triggers. Refreshing
 * once per call (instead of querying per mob/room) is the fix for a real
 * perf bug -- see the gate set's doc comment above. */
void trigger_random_tick(long pulse_num) {
    (void)pulse_num;
    g_mob_random_count = trigger_repo_random_vnums("mob", g_mob_random_vnums, RANDOM_VNUM_SET_MAX);
    g_room_random_count = trigger_repo_random_vnums("room", g_room_random_vnums, RANDOM_VNUM_SET_MAX);
    if (g_mob_random_count > 0)
        world_for_each_mob(random_visit_mob);
    if (g_room_random_count > 0)
        world_for_each_room(random_visit_room);
}
