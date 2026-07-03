#include "cmd_internal.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "game_loop.h"
#include "log.h"
#include "player_repo.h"
#include "room.h"

/* `copyover`: reboot the server in place without dropping a single
 * connection -- Erwin Andreasen's classic Diku "copyover/hotboot" trick,
 * NOT a port (the original SneezyMUD never had one). Every player fd is
 * inherited across an exec() of the (possibly freshly rebuilt) binary at
 * /proc/self/exe; the recovery file written here tells the new process
 * how to rebuild each descriptor (see game_loop.c's copyover_recover()
 * and descriptor_copyover_adopt()).
 *
 * The user's two requirements come almost for free:
 * - "lock out any commands": Tobin is single-threaded -- the whole
 *   copyover runs inside this one command execution, so no other input
 *   can interleave before the exec.
 * - "stop all fighting": every fighting pointer and wait-state is
 *   cleared (and progress persisted) before the state is written out, so
 *   nobody resumes mid-swing with a stale opponent pointer.
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
     * is direct), so players see it before the freeze. */
    for (descriptor_t *it = g_descriptors; it; it = it->next)
        descriptor_send(it,
            "\r\n*** COPYOVER in 5 seconds -- the world is about to be reborn. ***\r\n");
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
        if (it->state == CONN_PLAYING && it->character) {
            it->character->fighting = NULL;
            it->character->wait_pulses = 0;
            it->editing_help = false; /* editor buffers don't survive exec */
            player_progress_save(it->character->player_id, &it->character->progress);

            int room_vnum = it->character->base.roomp ? it->character->base.roomp->vnum : 1;
            fprintf(f, "conn %d %ld %d %d %s %s\n",
                    it->fd, it->account.account_id, room_vnum,
                    it->color_enabled ? 1 : 0, it->character->base.name,
                    it->account.name);
            descriptor_send(it, "\r\nTime stops for a moment as the world is reborn...\r\n");
        } else {
            descriptor_send(it, "\r\nThe world is being reborn -- please reconnect in a moment.\r\n");
            fcntl(it->fd, F_SETFD, FD_CLOEXEC);
        }
    }
    fclose(f);

    execl("/proc/self/exe", "tobin_c", "--copyover", COPYOVER_FILE, (char *)NULL);

    /* Only reachable if exec itself failed -- undo everything and let the
     * world carry on as it was. */
    log_error("copyover: exec of /proc/self/exe failed");
    unlink(COPYOVER_FILE);
    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (!(it->state == CONN_PLAYING && it->character))
            fcntl(it->fd, F_SETFD, 0);
    }
    descriptor_send(d, "Copyover failed at exec -- the world continues unchanged.\r\n");
    return true;
}
