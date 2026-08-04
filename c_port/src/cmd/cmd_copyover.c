/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "game_loop.h"
#include "log.h"
#include "obj.h"
#include "player_repo.h"
#include "room.h"
#include "thing.h"
#include "world.h"

/* Runtime-state persistence across copyover (TODO.md priority item, user
 * 2026-07-30): a copyover's exec() wipes the whole in-memory world --
 * every loaded room's dynamic contents (loose objects, spawned mobs)
 * vanish, since only PLAYER connections/rooms are recorded in the
 * recovery file; the next visit to a room reloads it fresh from its
 * static DB prototype, discarding anything dropped/loaded/spawned into
 * it since boot. Verified live before fixing (not guessed): a `load
 * obj`+`drop` in a room, then a copyover, made the dropped item vanish.
 * Scoped to top-level room contents only (not nested container/corpse
 * contents, not equipment worn by a mob) -- a real, disclosed
 * limitation, same "bounded scope" precedent every large item in this
 * audit takes. Mob HP/position round-trips too; duplication against the
 * PERIODIC zone-reset pass (zone.c's own `max_exist` world-wide count
 * gate, zone_cmd_mob()/zone_cmd_place()) is already prevented by that
 * existing gate, so restoring zone-seeded mobs/objects here is safe, not
 * just player-dropped ones. `world_for_each_room()` has no user-data
 * parameter, so `g_copyover_dump_file` is a short-lived, single-threaded,
 * file-scope handle for the one dump pass below -- not a real global. */
static FILE *g_copyover_dump_file = NULL;

/* 1-based count of same-vnum mobs in `r` at or before `target` in stuff_head
 * order -- the ordinal game_loop.c's copyover_recover() re-finds `target`
 * by on the other side of the exec (mobs have no stable identity beyond
 * vnum+room, so "the Nth zombie in this room" is the best available key).
 * Relies on the dump happening before anything reorders the room's list. */
static int mob_ordinal_in_room(const room_t *r, const thing_t *target) {
    int n = 0;
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind == THING_MOB && t->id == target->id) {
            n++;
            if (t == target)
                return n;
        }
    }
    return 0;
}

static void copyover_dump_room_contents(room_t *r) {
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind == THING_MOB) {
            being_t *m = (being_t *)t;
            fprintf(g_copyover_dump_file, "mob %d %d %d %d\n",
                    r->vnum, t->id, m->progress.hp, (int)m->position);
        } else if (t->kind == THING_OBJ) {
            obj_t *o = (obj_t *)t;
            if (o->vnum > 0) /* skip ephemeral (vnum 0) items -- never DB-backed anyway */
                fprintf(g_copyover_dump_file, "obj %d %d\n", r->vnum, o->vnum);
        }
    }
}

/* `copyover`: reboot the server in place without dropping a single
 * connection -- Erwin Andreasen's classic Diku "copyover/hotboot" trick,
 * NOT a port (the original SneezyMUD never had one). Every player fd is
 * inherited across an exec() of the (possibly freshly rebuilt) binary at
 * /proc/self/exe; the recovery file written here tells the new process
 * how to rebuild each descriptor (see game_loop.c's copyover_recover()
 * and descriptor_copyover_adopt()).
 *
 * The user's original "lock out any commands" requirement comes almost
 * for free: Tobin is single-threaded, so the whole copyover runs inside
 * this one command execution and no other input can interleave before
 * the exec. `wait_pulses` (swing lag) is still cleared -- nobody resumes
 * mid-swing lagged from an action that will never resolve -- but the
 * fight itself (who's fighting whom) is recorded and re-linked by
 * game_loop.c's copyover_recover() once both sides exist again post-exec
 * (user 2026-08-03: "fights should persist after copyover"; previously
 * every fighting pointer was unconditionally cleared here, silently
 * ending every fight on every copyover). Progress is still saved before
 * the recovery line is written, same as always.
 *
 * Connections still in login/menu/creation states can't be resumed
 * mid-dialog; they get a "please reconnect" note and are closed by the
 * exec itself via FD_CLOEXEC (nothing is destroyed by hand, so if exec
 * fails we can simply clear the flag and carry on unharmed). */
#define COPYOVER_FILE "copyover.dat"

bool cmd_copyover(descriptor_t *d, const char *args) {
    (void)args;

    int listen_fd = game_loop_listen_fd();
    if (listen_fd < 0) {
        descriptor_send(d, "Copyover unavailable: no listening socket.\r\n");
        return true;
    }

    /* 5-second warning to every connection (user requirement), then a
     * literal sleep: the select loop is blocked for the duration, so no
     * command can run and no combat round can resolve in the window --
     * the "lock out everything" guarantee is the sleep itself. The
     * warning bytes are written to the sockets immediately (socket_write
     * is direct), so players see it before the freeze.
     *
     * Deliberately descriptor_send(), NOT descriptor_notify() -- reviewed
     * during the Session 43 "editors get absolute quiet" audit and kept as
     * an intentional exception: the interruption is happening in 5 seconds
     * regardless of what anyone is doing, so holding this for catchup
     * would mean their connection just silently drops with no warning at
     * all (the held message would only surface after the copyover already
     * happened). Same reasoning for the two reborn/reconnect lines below. */
    for (descriptor_t *it = g_descriptors; it; it = it->next)
        descriptor_send(it,
            "\r\n<c>*** COPYOVER in 5 seconds -- the world is about to be reborn. ***<z>\r\n");
    sleep(5);

    FILE *f = fopen(COPYOVER_FILE, "w");
    if (!f) {
        descriptor_send(d, "Copyover failed: cannot write the recovery file.\r\n");
        return true;
    }

    log_info("Copyover initiated by %s.",
             d->character ? d->character->base.name : "(unknown)");
    fprintf(f, "listen %d\n", listen_fd);

    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        /* NOT `state == CONN_PLAYING` -- that excluded anyone mid-edroom/
         * edplayer/edzone from the recovery line entirely, losing their
         * session (forced to a fresh reconnect) across a copyover even
         * though they were legitimately online (Session 43 audit). Their
         * edit itself still doesn't survive (menu-editor sub-state isn't
         * in the recovery format either -- they resume as CONN_PLAYING). */
        if (it->character) {
            /* Fight persistence (user 2026-08-03: "fights should persist
             * after copyover") -- record the opponent's identity BEFORE
             * clearing `fighting`, so game_loop.c's copyover_recover() can
             * re-link the pair once both sides exist again post-exec. A PC
             * opponent is keyed by name (both fighters are conn lines, so
             * this account is naturally redundant/symmetric); a mob
             * opponent has no stable identity beyond room+vnum+ordinal
             * (mob_ordinal_in_room() above). If the opponent is neither
             * (shouldn't happen -- thing_kind_t is just PC/MOB for a
             * living combatant) the fight is silently dropped, same as
             * before this feature existed. */
            being_t *foe = it->character->fighting;
            if (foe && foe->base.kind == THING_PC) {
                fprintf(f, "fight %s pc %s\n", it->character->base.name, foe->base.name);
            } else if (foe && foe->base.kind == THING_MOB && foe->base.roomp) {
                int ord = mob_ordinal_in_room(foe->base.roomp, &foe->base);
                if (ord > 0)
                    fprintf(f, "fight %s mob %d %d %d\n", it->character->base.name,
                            foe->base.roomp->vnum, foe->base.id, ord);
            }

            it->character->fighting = NULL;
            it->character->wait_pulses = 0;
            it->edit_kind = EDIT_NONE; /* editor buffers don't survive exec */
            player_progress_save(it->character->player_id, &it->character->progress);

            int room_vnum = it->character->base.roomp ? it->character->base.roomp->vnum : 1;
            fprintf(f, "conn %d %ld %d %d %s %s %s\n",
                    it->fd, it->account.account_id, room_vnum,
                    it->color_enabled ? 1 : 0,
                    it->ip[0] ? it->ip : "?", it->character->base.name,
                    it->account.name);
            descriptor_send(it, "\r\n<c>Time stops for a moment as the world is reborn...<z>\r\n");
        } else {
            descriptor_send(it, "\r\n<c>The world is being reborn -- please reconnect in a moment.<z>\r\n");
            fcntl(it->fd, F_SETFD, FD_CLOEXEC);
        }
    }

    /* Dump loose room contents (mobs + top-level objects) -- see this
     * file's own header comment on why, and the scoped-down limits. */
    g_copyover_dump_file = f;
    world_for_each_room(copyover_dump_room_contents);
    g_copyover_dump_file = NULL;

    fclose(f);

    /* Flush anything still queued (descriptor_write(), descriptor.c) --
     * a backlog lives only in this process's heap, so unflushed bytes
     * would simply vanish across the exec below. One more non-blocking
     * attempt is all that's safe here (this can't block on a slow
     * client) -- the 5-second warning sleep above already gives normal
     * backlogs time to drain, so in practice this is a no-op. */
    for (descriptor_t *it = g_descriptors; it; it = it->next)
        descriptor_flush_output(it);

    /* Exec by PATH, not /proc/self/exe: the path resolves to a freshly
     * rebuilt binary, while /proc/self/exe would pin the deleted old inode
     * after a rebuild and silently relaunch the OLD code (found the hard
     * way -- see STATUS.md Session 21). argv[0] carries the full path so
     * the successor can resolve it again for the next copyover. */
    const char *binpath = tobin_binary_path();
    execl(binpath, binpath, "--copyover", COPYOVER_FILE, (char *)NULL);

    /* Only reachable if exec itself failed -- undo everything and let the
     * world carry on as it was. */
    log_error("copyover: exec of '%s' failed", binpath);
    unlink(COPYOVER_FILE);
    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (!it->character) /* matches the branch condition above */
            fcntl(it->fd, F_SETFD, 0);
    }
    descriptor_send(d, "Copyover failed at exec -- the world continues unchanged.\r\n");
    return true;
}
