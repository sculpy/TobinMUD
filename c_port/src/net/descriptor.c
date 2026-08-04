/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "descriptor.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "balance.h"
#include "cmd.h"
#include "colorstring.h"
#include "log.h"
#include "material.h"
#include "net.h"
#include "obj_repo.h"
#include "player_repo.h"
#include "multiplay.h"
#include "news_repo.h"
#include "room_repo.h"
#include "rules_repo.h"
#include "socials.h"
#include "world.h"
#include "zone.h"

descriptor_t *g_descriptors = NULL;

/* Minimal telnet negotiation (RFC 854): ask the client to let us do the
 * echoing and to send character-at-a-time instead of buffering a whole
 * line client-side before sending it. */
enum { TN_IAC = 255, TN_WILL = 251, TN_WONT = 252, TN_DO = 253, TN_DONT = 254,
       TN_SB = 250, TN_SE = 240, TN_ECHO = 1, TN_SGA = 3 };

/* Allocates and initializes a fresh descriptor for a newly-accepted socket:
 * negotiates telnet echo/SGA, links it into g_descriptors, and sends the
 * banner/keep art ending in the "Account name:" prompt (CONN_GET_ACCOUNT_NAME,
 * the state machine's entry point). */
descriptor_t *descriptor_create(int fd) {
    descriptor_t *d = calloc(1, sizeof(*d));
    if (!d)
        return NULL;

    d->fd = fd;
    d->state = CONN_GET_ACCOUNT_NAME;
    d->color_enabled = true;
    d->last_active = (long)time(NULL);
    d->next = g_descriptors;
    g_descriptors = d;

    unsigned char negotiate[] = {
        TN_IAC, TN_WILL, TN_ECHO,
        TN_IAC, TN_WILL, TN_SGA,
        TN_IAC, TN_DO, TN_SGA,
    };
    descriptor_write(d, (const char *)negotiate, sizeof(negotiate));

    /* The TobinMUD banner (user-supplied art, Session 21 -- the <_> in the
     * O of "Tobin" is an unrecognized color tag and passes through
     * literally by design), now preceded by a keep's gate (user-supplied
     * wireframe keep1.txt, Session 47) you enter through, with the logo
     * displayed just below it. An earlier version also opened with a
     * distant castle-skyline piece (castle1.txt) ahead of this, but the
     * user cut it: "remove the castle art from the connection screen,
     * its too long displaying 2 seperate ascii art pieces." */
    descriptor_send(d,
        "\r\n"
        "   /\\                                                        /\\\r\n"
        "  |  |                                                      |  |\r\n"
        " /----\\                                                    /----\\\r\n"
        "[______]                                                  [______]\r\n"
        " |    |         _____                        _____         |    |\r\n"
        " |[]  |        [     ]                      [     ]        |  []|\r\n"
        " |    |       [_______][ ][ ][ ][][ ][ ][ ][_______]       |    |\r\n"
        " |    [ ][ ][ ]|     |  ,----------------,  |     |[ ][ ][ ]    |\r\n"
        " |             |     |/'    ____..____    '\\|     |             |\r\n"
        "  \\  []        |     |    /'    ||    '\\    |     |        []  /\r\n"
        "   |      []   |     |   |o     ||     o|   |     |  []       |\r\n"
        "   |           |  _  |   |     _||_     |   |  _  |           |\r\n"
        "   |   []      | (_) |   |    (_||_)    |   | (_) |       []  |\r\n"
        "   |           |     |   |     (||)     |   |     |           |\r\n"
        "   |           |     |   |      ||      |   |     |           |\r\n"
        " /''           |     |   |o     ||     o|   |     |           ''\\\r\n"
        "[_____________[_______]--'------''------'--[_______]_____________]\r\n"
        "\r\n"
        "___________   ___.   .__           _____   ____ ___________\r\n"
        "\\__    ___/___\\_ |__ |__| ____    /     \\ |    |   \\______ \\\r\n"
        "  |    | /  _ \\| __ \\|  |/    \\  /  \\ /  \\|    |   /|    |  \\\r\n"
        "  |    |(  <_> ) \\_\\ \\  |   |  \\/    Y    \\    |  / |    `   \\\r\n"
        "  |____| \\____/|___  /__|___|  /\\____|__  /______/ /_______  /\r\n"
        "                   \\/        \\/         \\/                 \\/\r\n"
        "\r\nTobinMUD -- a derivative of SneezyMUD and DikuMUD.\r\n\r\n"
        "Account name: ");
    return d;
}

descriptor_t *descriptor_copyover_adopt(int fd, long account_id, int room_vnum,
                                        bool color_enabled, const char *peer_ip,
                                        const char *char_name,
                                        const char *account_name) {
    account_t acct;
    if (!account_load(account_name, &acct) || acct.account_id != account_id) {
        log_error("copyover: account '%s' (#%ld) not restorable", account_name, account_id);
        close(fd);
        return NULL;
    }

    being_t *b = player_load(char_name, account_id);
    if (!b) {
        log_error("copyover: character '%s' not restorable", char_name);
        close(fd);
        return NULL;
    }

    descriptor_t *d = calloc(1, sizeof(*d));
    if (!d) {
        being_destroy(b);
        close(fd);
        return NULL;
    }
    d->fd = fd;
    d->color_enabled = color_enabled;
    d->last_active = (long)time(NULL);
    snprintf(d->ip, sizeof(d->ip), "%s", peer_ip ? peer_ip : "?");
    d->account = acct;
    d->character = b;
    b->desc = d;
    d->next = g_descriptors;
    g_descriptors = d;

    /* Put them back where they were standing (not their load_room -- an
     * immortal mid-goto shouldn't snap home), with the usual lazy room
     * load and a fallback to room 1. */
    room_t *r = world_get_room(room_vnum);
    if (!r) {
        r = room_repo_load(room_vnum);
        if (r)
            world_register_room(r);
    }
    if (!r) {
        r = world_get_room(1);
        if (!r) {
            r = room_repo_load(1);
            if (r)
                world_register_room(r);
        }
    }
    if (r)
        thing_set_room(&b->base, r);

    d->state = CONN_PLAYING;
    descriptor_send(d, "\r\nThe world reforms around you -- copyover complete!\r\n");
    cmd_dispatch(d, "look"); /* prompt comes from the game loop's prompter */
    log_info("Copyover: restored %s (fd %d).", b->base.name, fd);
    return d;
}

/* ---- Held messages: no game interruptions while editing or paging ----- *
 * People in an editor (the redit menu / hedit / addnews) or mid-pager
 * (e.g. reading `news` a page at a time) don't get game messages pushed
 * at them -- those buffer here and are reviewed with `catchup`, expiring
 * after HELD_MSG_TTL. Async senders use descriptor_notify instead of
 * descriptor_send. */

/* True while `d` is inside ANY editor -- the shared line editor
 * (edit_kind) or a menu-driven one (redit/edplayer/edzone, each a
 * contiguous CONN_* range) -- or mid-pager (page_len > 0). Bug found
 * Session 43 (user: "when in the editors, no messages to interrupt...
 * thats what catchup is for"): this only ever checked the CONN_REDIT_*
 * range, so descriptor_notify()'s hold-for-catchup behavior silently
 * never applied to edplayer or edzone -- every broadcast that correctly
 * calls descriptor_notify() (system, wiznet, newbie, the death taunt,
 * ...) was still interrupting anyone mid-edplayer/edzone. New editors
 * MUST add their CONN_* range here. Pager silence added later the same
 * session (user: "silence all messaging like youve done for the
 * editors... but for pagination") -- since `news` is mortal-accessible,
 * `catchup` was widened from immortal-only to mortal-level at the same
 * time (cmd_table.c), or a held-during-pager mortal would have no way to
 * retrieve it. */
bool descriptor_in_editor(const descriptor_t *d) {
    return d->edit_kind != EDIT_NONE
        || (d->state >= CONN_REDIT_MENU && d->state <= CONN_REDIT_QUIT_CONFIRM)
        || (d->state >= CONN_EDPLAYER_MENU && d->state <= CONN_EDPLAYER_QUIT_CONFIRM)
        || (d->state >= CONN_EDZONE_MENU && d->state <= CONN_EDZONE_QUIT_CONFIRM)
        || (d->state >= CONN_OEDIT_MENU && d->state <= CONN_OEDIT_QUIT_CONFIRM)
        || (d->state >= CONN_BALANCE_MENU && d->state <= CONN_BALANCE_QUIT_CONFIRM)
        || (d->state >= CONN_EDACCOUNT_MENU && d->state <= CONN_EDACCOUNT_PASSWORD)
        || (d->state >= CONN_EDSOCIAL_LIST && d->state <= CONN_EDSOCIAL_DELETE_CONFIRM)
        || (d->state >= CONN_TRIGEDIT_LIST && d->state <= CONN_TRIGEDIT_SCRIPT)
        || (d->state >= CONN_MEDIT_MENU && d->state <= CONN_MEDIT_QUIT_CONFIRM)
        || (d->state >= CONN_EDSUIT_LIST && d->state <= CONN_EDSUIT_DELETE_CONFIRM)
        || d->page_len > 0; /* mid-pager -- same "no interruptions" treatment */
}

/* Appends `msg` to d's held-message backlog (see the section comment above),
 * evicting the oldest entry first if the ring is already full. */
static void descriptor_hold(descriptor_t *d, const char *msg) {
    if (d->held_count >= HELD_MSG_MAX) {
        /* Full -- drop the oldest to make room for the newest. */
        memmove(&d->held[0], &d->held[1], sizeof(d->held[0]) * (HELD_MSG_MAX - 1));
        d->held_count = HELD_MSG_MAX - 1;
    }
    d->held[d->held_count].when = (long)time(NULL);
    snprintf(d->held[d->held_count].text, HELD_MSG_LEN, "%s", msg);
    d->held_count++;
}

/* Real player-to-player/immortal communication (tell, say, shout,
 * whisper, wiznet, the author-driven `system` broadcast, the newbie
 * channel, direct group notices) -- held while the recipient is mid-
 * editor and replayed via `catchup`, same "silence all messaging for
 * editors" rule the project already established. This is the ONLY
 * category `catchup` replays (user 2026-07-26: "catchup command should
 * only record communications not theme messages") -- everything else
 * uses the plain descriptor_notify() below, which no longer holds
 * anything at all. */
void descriptor_notify_comm(descriptor_t *d, const char *msg) {
    if (descriptor_in_editor(d))
        descriptor_hold(d, msg);
    else
        descriptor_send(d, msg);
}

/* Ambient/theme messages (room echoes, combat, mob AI, weather, object
 * actions, ...) -- simply DROPPED while the recipient is mid-editor
 * rather than held, so `catchup` doesn't fill up with things that were
 * never actually said TO anyone (user 2026-07-26, see
 * descriptor_notify_comm() above for the real-communication half of
 * this split). Async senders that ARE genuine communication must use
 * descriptor_notify_comm() instead. */
void descriptor_notify(descriptor_t *d, const char *msg) {
    if (descriptor_in_editor(d))
        return;
    descriptor_send(d, msg);
}

/* Formats and records a tagged log line: always goes to the log file, and
 * (unless LOG_SILENT) is echoed live to every online immortal whose
 * severity bitmask has this type enabled -- except those mid-editor, whose
 * screen must stay intact, and except everyone if `type` is a personalized
 * type meant for one specific immortal only. */
void game_log(log_type_t type, const char *fmt, ...) {
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    /* Always record to the file/console, tagged with the type. */
    log_info("[%s] %s", log_type_name(type), msg);

    /* LOG_SILENT is recorded but never echoed to immortals (anti-spam). */
    if (type == LOG_SILENT)
        return;

    /* Echo to online immortals with a colored [TYPE] tag. Editors are never
     * interrupted (their screen must stay intact -- the line is still in the
     * file, reachable via `log`). Personalized types reach only their one
     * immortal. */
    const char *personal = log_type_personal_name(type);
    char line[600];
    snprintf(line, sizeof(line), "<c>[%s]<z> %s\r\n", log_type_name(type), msg);
    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (!it->character || !being_is_immortal(it->character))
            continue;
        if (descriptor_in_editor(it))
            continue;
        if (personal && strcasecmp(it->character->base.name, personal) != 0)
            continue;
        if (!(it->character->severity & (1 << type)))
            continue;
        descriptor_send(it, line);
    }
}

/* See descriptor.h. */
void descriptor_keepalive(long pulse_num) {
    (void)pulse_num;
    /* IAC NOP -- a telnet no-op. Keeps the TCP connection warm so idle
     * players aren't dropped by NAT/router timeouts; clients ignore it.
     * Sent aggressively (see the pulse interval in main.c) to survive short
     * NAT/router idle windows. */
    static const unsigned char nop[2] = {255, 241};
    for (descriptor_t *d = g_descriptors; d; d = d->next)
        descriptor_write(d, (const char *)nop, sizeof(nop));
}

/* See descriptor.h. */
void descriptor_idle_timeout(long pulse_num) {
    (void)pulse_num;
    /* Deliberate idle-out (user spec): a playing MORTAL idle past the limit
     * is disconnected; immortals are immune (never idle-dropped). The
     * network-side keepalive above is separate -- this is a policy timeout. */
    long now = (long)time(NULL);
    descriptor_t *d = g_descriptors;
    while (d) {
        descriptor_t *next = d->next; /* destroy() unlinks d, so save next */
        if (d->state == CONN_PLAYING && d->character
            && !being_is_immortal(d->character)
            && now - d->last_active > IDLE_DISCONNECT_SECS) {
            descriptor_send(d,
                "\r\n<y>You have been idle too long. Goodbye.<z>\r\n");
            descriptor_destroy(d);
        }
        d = next;
    }
}

/* See descriptor.h. */
void descriptor_held_expire(long pulse_num) {
    (void)pulse_num;
    long now = (long)time(NULL);
    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        int w = 0;
        for (int i = 0; i < d->held_count; i++) {
            if (now - d->held[i].when <= HELD_MSG_TTL) {
                if (w != i)
                    d->held[w] = d->held[i];
                w++;
            }
        }
        d->held_count = w;
    }
}

/* Shown when leaving an editor if messages piled up while inside. */
static void descriptor_editor_exit_notice(descriptor_t *d) {
    if (d->held_count > 0) {
        char msg[144];
        snprintf(msg, sizeof(msg),
            "<c>[ %d message%s arrived while you were editing -- "
            "type 'catchup' to read them. ]<z>\r\n",
            d->held_count, d->held_count == 1 ? "" : "s");
        descriptor_send(d, msg);
    }
}

/* See descriptor.h. Walks the room's stuff list rather than iterating
 * g_descriptors so only PCs actually standing in `r` are considered. */
void descriptor_room_echo(struct room *r, being_t *except, const char *msg) {
    if (!r)
        return;
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC || (except && t == &except->base))
            continue;
        being_t *other = (being_t *)t;
        if (other->desc)
            descriptor_notify(other->desc, msg);
    }
}

/* See descriptor.h. Tears down a connection: restores a possessed mob (if
 * any) to its unpuppeted state, unlinks `d` from g_descriptors, parks a
 * live character as linkdead (rather than destroying it) with a room
 * announcement and PIO log line, unhooks any snoop relationship in either
 * direction, then closes the socket and frees `d` itself. */
void descriptor_destroy(descriptor_t *d) {
    if (!d)
        return;

    /* `possess`/`return` (cmd_possess.c): a disconnect while possessing a
     * mob must restore the immortal's own PC into `d->character` FIRST,
     * so the link-drop handling below (room announce, log, linkdead
     * parking) applies to the real character, not the puppeted mob --
     * and so the mob goes back to being a normal, un-puppeted, desc==NULL
     * mob rather than getting stuck wearing a PC's link-drop state.
     *
     * A POLYMORPHED form (being_start_polymorph(), being.c) is left the
     * SAME way, deliberately NOT destroyed here even though it's a
     * temporary body that (unlike a real `possess`d target) has nowhere
     * else to go -- a real, live crash was traced to destroying a being
     * from inside this exact disconnect path while OTHER game state
     * (an unresolved fight, another descriptor mid-iteration in
     * game_loop_run's own g_descriptors walk) could still reference it;
     * see STATUS.md for the fuller writeup. Left as an ordinary orphaned
     * mob instead -- a real but strictly smaller gap (a stray "brown
     * bear" left behind after a player's raw disconnect mid-polymorph,
     * cleanable like any other stray mob) than the crash it replaces.
     * AFFECT_CHARMED never needed this branch at all -- a charmed PET
     * mob has no descriptor of its own. */
    if (d->possess_original) {
        being_t *mob = d->character;
        d->character = d->possess_original;
        d->possess_original = NULL;
        if (mob)
            mob->desc = NULL;
    }

    descriptor_t **cur = &g_descriptors;
    while (*cur && *cur != d)
        cur = &(*cur)->next;
    if (*cur)
        *cur = d->next;

    if (d->character) {
        /* A vanishing player shouldn't just silently blink out of the
         * room (user requirement) -- this is the link-drop path; a
         * deliberate quit! announces separately in cmd_quit.c. */
        /* Gender-specific pronoun (his/her/its), not a blanket "their" (user
         * 2026-07-09 -- standing habit going forward for ALL mud output). */
        const char *possess = gender_possess(d->character->gender);
        char msg[160];
        if (d->character->base.roomp) {
            snprintf(msg, sizeof(msg), "%s has lost %s link.\r\n",
                     d->character->base.name, possess);
            descriptor_room_echo(d->character->base.roomp, d->character, msg);
        }
        /* Link-drops are logged, and the log line is repeated to every
         * immortal online -- except those mid-editor, whose screen must
         * not be corrupted (user requirement, Session 21; same idea as
         * the original's vlogf-to-imms). */
        /* A link-drop is a typed player-io event: logged to the file and
         * echoed to online immortals with a colored [PIO] tag. d is already
         * unlinked (above), so game_log won't try to notify the departing
         * connection. Editors aren't interrupted; the line stays in `log`. */
        game_log(LOG_PIO, "%s has lost %s link. [%s]",
                 d->character->base.name, possess, descriptor_display_host(d));
        /* Detach, don't destroy (user requirement): the character stays put
         * in its room, linkdead, until the same account reconnects to it
         * (enter_world() checks world_find_linkdead_pc() first, then does a
         * FRESH DB load and discards this body -- see there for why: it
         * must never reuse this stale in-memory copy directly) or the
         * process ends (a normal reboot, or copyover -- which only recovers
         * beings still attached to a live descriptor, see
         * descriptor_copyover_adopt(); a linkdead body's memory simply ends
         * with the old process). Deliberately NOT persisted here: an eager
         * save of stale in-memory progress would clobber any DB-side change
         * made while still connected (e.g. an admin `UPDATE player_progress`
         * or a promote/set edit) with the pre-disconnect snapshot -- exactly
         * the create-then-SQL-promote-then-close pattern several smoke
         * tests already rely on. The being stays alive in memory, so
         * nothing is at risk under normal operation; only an ungraceful
         * crash before reconnect could lose progress since the last real
         * save, an accepted, narrow trade-off. NOTE this trade-off is
         * knowingly reintroduced 5 minutes later by world.c's auto-purge
         * (linkdead_purge_tick()) -- the user chose save-then-destroy
         * there (matching real Sneezy's nukeLdead()) over this function's
         * own discard-only precedent, so an admin DB edit made to a
         * character that's been linkdead 5+ minutes CAN still be
         * clobbered by the stale pre-disconnect snapshot. A real,
         * disclosed trade-off, not an oversight -- see world.h's
         * world_purge_stale_linkdead() doc comment. */
        d->character->desc = NULL;
        d->character->linkdead_since = time(NULL);
    }

    /* Unhook any live `snoop` relationship in either direction so neither
     * side is left pointing at a descriptor about to be freed. If someone
     * was snooping THIS connection, tell them their target just vanished
     * (user 2026-07-17: "when you are snooping and the player loses
     * connection, send a message to the snooper saying you are no longer
     * snooping <target name>") -- otherwise a snooper watching a link-drop
     * gets no indication their snoop silently ended. */
    if (d->snoop_target)
        d->snoop_target->snooped_by = NULL;
    if (d->snooped_by) {
        char msg[128];
        snprintf(msg, sizeof(msg), "<c>You are no longer snooping %s.<z>\r\n",
                 d->character ? d->character->base.name : "them");
        descriptor_send(d->snooped_by, msg);
        d->snooped_by->snoop_target = NULL;
    }

    close(d->fd);
    free(d);
}

/* See descriptor.h. */
const char *descriptor_display_host(const descriptor_t *d) {
    return d->hostname[0] ? d->hostname : d->ip;
}

/* See descriptor.h. Replaces a bare socket_write() everywhere in this file
 * -- that always either sent everything or silently dropped whatever
 * didn't fit in one write() call, with no retry. Under bursty output
 * (several new connections landing in the same select() tick, each
 * getting the banner + several prompts in a row) that could drop real
 * bytes the client was still waiting on: the server believed the reply
 * had gone out, the client's recv() then blocked forever. Found
 * 2026-07-17 chasing the long-standing "a session silently stalls under
 * concurrent load" bug. */
void descriptor_write(descriptor_t *d, const char *data, size_t len) {
    if (len == 0)
        return;

    size_t sent = 0;
    if (d->out_len == 0) {
        int n = socket_write(d->fd, data, len);
        if (n < 0)
            return; /* hard error -- the next read will discover the dead connection */
        sent = (size_t)n;
        if (sent == len)
            return; /* common case: it all went out in one call */
    }

    size_t remain = len - sent;
    if (d->out_len + remain > sizeof(d->out_buf)) {
        log_error("descriptor fd %d: output backlog full (%zu bytes pending), dropping %zu bytes",
                  d->fd, d->out_len, remain);
        remain = sizeof(d->out_buf) > d->out_len ? sizeof(d->out_buf) - d->out_len : 0;
        if (remain == 0)
            return;
    }
    memcpy(d->out_buf + d->out_len, data + sent, remain);
    d->out_len += remain;
}

/* See descriptor.h. */
bool descriptor_flush_output(descriptor_t *d) {
    if (d->out_len == 0)
        return true;

    int n = socket_write(d->fd, d->out_buf, d->out_len);
    if (n < 0)
        return false;
    if ((size_t)n == d->out_len) {
        d->out_len = 0;
        return true;
    }
    memmove(d->out_buf, d->out_buf + n, d->out_len - (size_t)n);
    d->out_len -= (size_t)n;
    return true;
}

/* Sends `msg`, normalizing any bare '\n' (not preceded by '\r') to "\r\n"
 * first. This matters because DB-sourced text (room descriptions in
 * particular, seeded from the original SneezyMUD dump) uses Unix-style
 * line endings internally -- a bare '\n' doesn't reset the cursor to
 * column 0 on a real telnet client, producing a "staircase" effect.
 * Everything we compose ourselves already uses "\r\n", so this is a
 * no-op overhead-wise for those, and a real fix for DB text. */
void descriptor_send(descriptor_t *d, const char *msg) {
    size_t len = strlen(msg);

    /* Color translation runs first (see colorstring.h) -- ANSI escapes
     * never contain a bare '\n', so this ordering is safe with the CRLF
     * normalization pass below. */
    size_t color_cap = colorstring_translate_maxlen(len);
    char *colored = malloc(color_cap);
    if (!colored) {
        descriptor_write(d, msg, len); /* best-effort fallback */
        return;
    }
    size_t clen = colorstring_translate(msg, colored, color_cap, d->color_enabled);

    char *normalized = malloc(clen * 2 + 1);
    if (!normalized) {
        descriptor_write(d, colored, clen); /* best-effort fallback */
        free(colored);
        return;
    }

    size_t out = 0;
    for (size_t i = 0; i < clen; i++) {
        if (colored[i] == '\n' && (i == 0 || colored[i - 1] != '\r'))
            normalized[out++] = '\r';
        normalized[out++] = colored[i];
    }

    descriptor_write(d, normalized, out);

    /* `snoop` mirror: whatever this descriptor sees, its watcher (if any)
     * sees too -- the exact same bytes already rendered for THIS
     * descriptor's own color preference (matching a real snoop: you're
     * watching their literal screen, not re-rendering for your own). A
     * direct descriptor_write on the WATCHER's own backlog, not a
     * recursive descriptor_send() call, so a chain/cycle of snoops can
     * never recurse.
     *
     * A "% " marker (same literal prefix the typed-command mirror below
     * already uses) is written ahead of every mirrored output chunk too
     * (user 2026-07-11: "add a special prompt to messages sent in snoop
     * (%) snooped content") -- before this, only the target's typed
     * commands were marked; their OWN output was mirrored completely
     * unmarked, indistinguishable from the snooper's own screen. */
    if (d->snooped_by) {
        static const char marker[] = "% ";
        descriptor_write(d->snooped_by, marker, strlen(marker));
        descriptor_write(d->snooped_by, normalized, out);
    }

    free(normalized);
    free(colored);
    d->needs_prompt = true; /* the game loop's prompter picks this up */
}

/* Emits the next page_size lines of the pager buffer; appends a MORE prompt
 * if content remains, or clears the pager (releasing the normal "> " prompt)
 * when done. */
static void descriptor_page_next(descriptor_t *d) {
    if (d->page_pos >= d->page_len) {
        d->page_len = 0;
        d->page_pos = 0;
        return;
    }
    size_t start = d->page_pos;
    int lines = 0;
    size_t i = start;
    while (i < d->page_len && lines < d->page_size) {
        if (d->page_buf[i] == '\n')
            lines++;
        i++;
    }
    char saved = d->page_buf[i];
    d->page_buf[i] = '\0';
    descriptor_send(d, d->page_buf + start);
    d->page_buf[i] = saved;
    d->page_pos = i;

    if (d->page_pos < d->page_len)
        descriptor_send(d, "\r\n<c>[ <C>ENTER<c> for more, <C>Q<c> to stop ]<z>");
    else {
        d->page_len = 0;
        d->page_pos = 0;
    }
}

/* See descriptor.h. */
void descriptor_page_start(descriptor_t *d, const char *text, int page_size) {
    d->page_size = page_size > 0 ? page_size : 20;
    snprintf(d->page_buf, sizeof(d->page_buf), "%s", text);
    d->page_len = strlen(d->page_buf);
    d->page_pos = 0;
    descriptor_page_next(d);
}

/* States where the line being typed must not be echoed back to the client
 * (account/character password entry) -- checked by drain_lines() below. */
static bool is_password_state(conn_state_t s) {
    return s == CONN_GET_PASSWORD || s == CONN_GET_NEW_PASSWORD
        || s == CONN_CONFIRM_PASSWORD || s == CONN_CHAR_DELETE_PASSWORD
        || s == CONN_ACCOUNT_DELETE_PASSWORD;
}

/* Consumes as much of d->raw[d->raw_pos .. d->raw_len) as forms complete
 * lines, calling handle_line() for each. Leftover partial-line bytes stay
 * buffered (d->raw_pos/d->raw_len persist for the next call). Telnet IAC
 * sequences are consumed and discarded; backspace/DEL edit the in-progress
 * line; typed characters are echoed back unless the current state is a
 * password prompt. Returns false if a line handler requested disconnect. */
static bool handle_line(descriptor_t *d, const char *line);

static bool drain_lines(descriptor_t *d) {
    while (d->raw_pos < d->raw_len) {
        unsigned char b = d->raw[d->raw_pos++];

        /* Inside an IAC SB ... IAC SE subnegotiation (possibly resumed from
         * a previous read) -- swallow payload bytes until the terminator. */
        if (d->in_subneg) {
            if (d->subneg_prev == TN_IAC && b == TN_SE) {
                d->in_subneg = false;
                d->subneg_prev = 0;
            } else {
                d->subneg_prev = b;
            }
            continue;
        }

        if (b == TN_IAC) {
            if (d->raw_pos >= d->raw_len) { d->raw_pos--; break; } /* wait for more */
            unsigned char cmd = d->raw[d->raw_pos++];
            if (cmd == TN_WILL || cmd == TN_WONT || cmd == TN_DO || cmd == TN_DONT) {
                if (d->raw_pos >= d->raw_len) { d->raw_pos -= 2; break; }
                d->raw_pos++; /* skip option byte */
            } else if (cmd == TN_SB) {
                d->in_subneg = true;
                d->subneg_prev = 0;
            }
            continue;
        }

        if (b == '\r' || b == '\n') {
            if (b == '\r' && d->raw_pos < d->raw_len && d->raw[d->raw_pos] == '\n')
                d->raw_pos++;
            descriptor_write(d, "\r\n", 2);
            d->line[d->line_len] = '\0';
            char line_copy[DESC_LINE_MAX];
            snprintf(line_copy, sizeof(line_copy), "%s", d->line);
            d->line_len = 0;
            if (!handle_line(d, line_copy))
                return false;
            continue;
        }

        if (b == 8 || b == 127) { /* backspace / DEL */
            if (d->line_len > 0) {
                d->line_len--;
                if (!is_password_state(d->state))
                    descriptor_write(d, "\b \b", 3);
            }
            continue;
        }

        if (b < 32)
            continue;

        if (d->line_len + 1 < DESC_LINE_MAX) {
            d->line[d->line_len++] = (char)b;
            if (!is_password_state(d->state)) {
                char echo[2] = { (char)b, '\0' };
                descriptor_write(d, echo, 1);
            }
        }
    }
    return true;
}

/* See descriptor.h. Compacts already-consumed bytes off the front of
 * d->raw, does one non-blocking read() to top it up, then hands whatever's
 * available to drain_lines() for line extraction and dispatch. */
bool descriptor_process_input(descriptor_t *d) {
    /* Compact already-consumed bytes off the front before reading more. */
    if (d->raw_pos > 0) {
        memmove(d->raw, d->raw + d->raw_pos, d->raw_len - d->raw_pos);
        d->raw_len -= d->raw_pos;
        d->raw_pos = 0;
    }

    if (d->raw_len >= DESC_RAW_BUF) {
        /* Line too long without a terminator -- drop it (abuse protection). */
        d->raw_len = 0;
    }

    ssize_t n = read(d->fd, d->raw + d->raw_len, DESC_RAW_BUF - d->raw_len);
    if (n == 0)
        return false; /* peer closed */
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return true; /* nothing new right now */
        return false;
    }
    d->raw_len += (int)n;

    return drain_lines(d);
}

/* Total point-buy points spent so far across all six attributes (each point
 * above ATTR_BASE counts as one) -- used by the CONN_CHAR_CREATE_ATTRS
 * screen to show/enforce the remaining budget. */
static int attrs_allocated(const attrs_t *a) {
    return (a->strength - ATTR_BASE) + (a->dexterity - ATTR_BASE) + (a->constitution - ATTR_BASE) +
           (a->intelligence - ATTR_BASE) + (a->wisdom - ATTR_BASE) + (a->charisma - ATTR_BASE);
}

/* Refreshes d->char_list from the DB and prints the account menu. */
/* Returns the ON-SCREEN character count of a "<X>"-tagged string (see
 * colorstring.h): each well-formed tag renders to either an ANSI escape
 * (color on) or nothing at all (color off) -- zero screen columns either
 * way -- so box/column alignment has to measure around them, not count
 * them. Multi-byte UTF-8 box-drawing glyphs are never fed through this
 * (they're only ever emitted directly by send_boxed_menu() itself), so a
 * plain byte scan is safe here. */
static size_t visible_len(const char *s) {
    size_t len = strlen(s);
    size_t vis = 0;
    for (size_t i = 0; i < len; ) {
        if (s[i] == '<' && i + 2 < len && s[i + 2] == '>') {
            i += 3;
            continue;
        }
        vis++;
        i++;
    }
    return vis;
}

/* Renders `lines` inside a double-line Unicode box (user-supplied
 * wireframe, box1.txt -- the ╔═╗║╚╝ glyph set is the intended style for
 * every character-facing letter-menu, not a literal fixed-size template:
 * each box is auto-sized to its own widest line). Lines may use the
 * normal "<X>" color-tag markup; padding is computed on the tag-free
 * visible width (see visible_len()) so a color-off render still lines up. */
static void send_boxed_menu(descriptor_t *d, const char *const *lines, int count) {
    size_t maxw = 0;
    for (int i = 0; i < count; i++) {
        size_t w = visible_len(lines[i]);
        if (w > maxw)
            maxw = w;
    }
    size_t inner = maxw + 2; /* one space of padding each side */

    char out[4096];
    size_t n = 0;
    n += (size_t)snprintf(out + n, sizeof(out) - n, "\r\n\xe2\x95\x94");
    for (size_t i = 0; i < inner && n < sizeof(out); i++)
        n += (size_t)snprintf(out + n, sizeof(out) - n, "\xe2\x95\x90");
    n += (size_t)snprintf(out + n, sizeof(out) - n, "\xe2\x95\x97\r\n");

    for (int i = 0; i < count && n < sizeof(out); i++) {
        size_t pad = maxw - visible_len(lines[i]);
        n += (size_t)snprintf(out + n, sizeof(out) - n, "\xe2\x95\x91 %s%*s \xe2\x95\x91\r\n",
                              lines[i], (int)pad, "");
    }

    n += (size_t)snprintf(out + n, sizeof(out) - n, "\xe2\x95\x9a");
    for (size_t i = 0; i < inner && n < sizeof(out); i++)
        n += (size_t)snprintf(out + n, sizeof(out) - n, "\xe2\x95\x90");
    n += (size_t)snprintf(out + n, sizeof(out) - n, "\xe2\x95\x9d\r\n");

    descriptor_send(d, out);
}

/* Boxed, numbered character list (user: "colorize the number list with
 * <C>") -- shared by show_account_menu()'s revealed listing and the
 * delete flow's own "which one?" picker (user, 2026-07-26: "when
 * deleting a character, the player should be presented a list of his
 * characters so he could choose properly"), each adding its own trailing
 * prompt line since the two contexts want different wording. Assumes
 * d->char_list/char_count are already fresh. */
static void show_char_list_box(descriptor_t *d) {
    char lines[MAX_CHARS_PER_ACCOUNT][96];
    const char *line_ptrs[MAX_CHARS_PER_ACCOUNT];
    for (int i = 0; i < d->char_count; i++) {
        /* Same convention as `who`: immortals show their rank title,
         * mortals show the level number. A character already in the
         * world (on any connection) is marked (user request). */
        bool online = false;
        for (descriptor_t *it = g_descriptors; it && !online; it = it->next) {
            if (it->state == CONN_PLAYING && it->character
                && strcasecmp(it->character->base.name, d->char_list[i]) == 0)
                online = true;
        }
        const char *title = being_level_title(d->char_levels[i]);
        if (title)
            snprintf(lines[i], sizeof(lines[i]), "<C>%d.<z> %s [%s]%s",
                     i + 1, d->char_list[i], title, online ? " (connected)" : "");
        else
            snprintf(lines[i], sizeof(lines[i]), "<C>%d.<z> %s [Level %d]%s",
                     i + 1, d->char_list[i], d->char_levels[i], online ? " (connected)" : "");
        line_ptrs[i] = lines[i];
    }
    send_boxed_menu(d, line_ptrs, d->char_count);
}

/* Refreshes d->char_list/char_levels/char_count from the DB and renders the
 * CONN_ACCOUNT_MENU screen: the boxed letter-menu while the character list
 * is still hidden, or the full numbered list (via show_char_list_box())
 * once revealed. An account with no characters yet skips straight to its
 * own "none yet" prompt. */
static void show_account_menu(descriptor_t *d) {
    player_list_by_account(d->account.account_id, d->char_list, d->char_levels,
                           MAX_CHARS_PER_ACCOUNT, &d->char_count);

    /* Hidden by default (user: "hide the character list until C") -- a
     * boxed letter-menu (box1.txt wireframe) instead of the full listing.
     * Nothing to hide for a brand-new account, so that case skips
     * straight to the (empty) listing below instead. */
    if (!d->char_list_shown && d->char_count > 0) {
        static const char *const box[] = {
            "<C>C<z>  Connect Player",
            "<C>N<z>  New Player",
            "<C>D<z>  Delete Player",
            "<C>X<z>  Delete Account",
            "<C>Q<z>  Quit Game",
        };
        send_boxed_menu(d, box, 5);
        descriptor_send(d, "> ");
        return;
    }

    if (d->char_count == 0) {
        descriptor_send(d,
            "\r\n-- Your players --\r\n  (none yet)\r\n"
            "\r\n  <C>N<z> create   <C>D<z> <name> delete   <C>X<z> delete account   <C>Q<z> quit\r\n\r\n> ");
        return;
    }

    show_char_list_box(d);
    descriptor_send(d, "Choose a number to connect that player to the game: ");
}

/* Unloads the current character and returns to the account menu, without
 * closing the connection. Used by `quit!` while playing (see cmd_quit.c);
 * the account menu's own `quit!` (see the CONN_ACCOUNT_MENU case below)
 * closes the connection instead. */
void descriptor_leave_to_menu(descriptor_t *d) {
    if (d->character) {
        /* Auto-save (user, 2026-07-12: "the game should automatically
         * save a char upon death or quit") -- this is the one choke
         * point both `quit!` (cmd_quit.c) and a PC's combat defeat
         * (combat.c's combat_defeat(), loser->desc route) already share,
         * so a single call here covers both without duplicating it at
         * each call site. Must run BEFORE being_destroy() frees the
         * character. */
        player_save(d->character->player_id, d->character);
        being_destroy(d->character); /* also removes it from its room */
        d->character = NULL;
    }
    d->edit_kind = EDIT_NONE; /* a mid-edit defeat/quit discards the edit */
    d->state = CONN_ACCOUNT_MENU;
    d->char_list_shown = false; /* fresh arrival at the menu -- start hidden again */
    show_account_menu(d);
}

/* Prints the current point-buy allocation and remaining pool (user
 * wireframe, 2026-07-26): a numbered 2-column grid (1-6) instead of the
 * old one-stat-per-line list, so a player can either type the familiar
 * "str 30" directly OR type a bare number (1-6) and get asked for the
 * amount separately (CONN_CHAR_CREATE_ATTR_AMOUNT) -- both paths call the
 * same apply_attr_delta() helper. Handedness/gender/alignment/appearance
 * moved OUT of this screen entirely into the second boxed menu
 * (show_options_screen(), shown once this one's "done"). */
static void show_attr_screen(descriptor_t *d) {
    int remaining = ATTR_POOL - attrs_allocated(&d->new_char_attrs);

    char head[96];
    snprintf(head, sizeof(head), "-- Allocate attributes for %s --", d->new_char_name);

    char intro1[96], intro2[96], intro3[64];
    snprintf(intro1, sizeof(intro1), "Every attribute starts at %d. Raise or lower any attribute by up to", ATTR_BASE);
    snprintf(intro2, sizeof(intro2), "%d in either direction -- lowering one frees up room to raise another.", ATTR_DELTA_CAP);
    snprintf(intro3, sizeof(intro3), "Net pool: %d points. Commands:", ATTR_POOL);

    char row1[64], row2[64], points[48];
    snprintf(row1, sizeof(row1), "<C>1)<z> Str: %-3d   <C>2)<z> Dex: %-3d   <C>3)<z> Con: %-3d",
             d->new_char_attrs.strength, d->new_char_attrs.dexterity, d->new_char_attrs.constitution);
    snprintf(row2, sizeof(row2), "<C>4)<z> Int: %-3d   <C>5)<z> Wis: %-3d   <C>6)<z> Cha: %-3d",
             d->new_char_attrs.intelligence, d->new_char_attrs.wisdom, d->new_char_attrs.charisma);
    snprintf(points, sizeof(points), "Points remaining: %d", remaining);

    const char *const lines[] = {
        intro1, intro2, intro3,
        "",
        "  str/dex/con/int/wis/cha <amount>   set that attribute's adjustment,",
        "                                      e.g. \"str 30\" or \"wis -20\"",
        "",
        row1, row2,
        "",
        points,
        "",
        "  Choose a number to adjust each stat, or",
        "  <C>D<z>)one   <C>R<z>eset   <C>A<z>bort or Quit",
    };
    descriptor_send(d, "\r\n");
    descriptor_send(d, head);
    send_boxed_menu(d, lines, (int)(sizeof(lines) / sizeof(lines[0])));
    descriptor_send(d, "> ");
}

/* Sub-prompt after a bare numbered pick (1-6) at the attr screen. */
static const char *const ATTR_PICK_NAMES[6] = {
    "Strength", "Dexterity", "Constitution", "Intelligence", "Wisdom", "Charisma",
};

/* CONN_CHAR_CREATE_ATTR_AMOUNT: asks "how much?" for the attribute picked
 * by a bare number at the attr screen (d->new_char_attr_pick); the typed
 * reply is applied by apply_attr_delta() back in CONN_CHAR_CREATE_ATTRS. */
static void show_attr_amount_prompt(descriptor_t *d) {
    char msg[128];
    snprintf(msg, sizeof(msg), "\r\nEnter the adjustment for %s (-%d to +%d), or blank to cancel: ",
             ATTR_PICK_NAMES[d->new_char_attr_pick - 1], ATTR_DELTA_CAP, ATTR_DELTA_CAP);
    descriptor_send(d, msg);
}

/* Shared by both the direct "str <amount>" typed command and the
 * numbered-pick-then-amount flow -- one field, one rule, one message set. */
static bool apply_attr_delta(descriptor_t *d, int *field, int amount) {
    if (amount < -ATTR_DELTA_CAP || amount > ATTR_DELTA_CAP) {
        char msg[96];
        snprintf(msg, sizeof(msg), "Adjustment must be between -%d and +%d.\r\n",
                 ATTR_DELTA_CAP, ATTR_DELTA_CAP);
        descriptor_send(d, msg);
        return false;
    }
    int old_value = *field;
    *field = ATTR_BASE + amount;
    if (attrs_allocated(&d->new_char_attrs) > ATTR_POOL) {
        *field = old_value; /* would overspend the net pool -- reject */
        descriptor_send(d, "Not enough points remaining for that.\r\n");
        return false;
    }
    return true;
}

/* Second boxed menu (user wireframe, 2026-07-26) -- handedness, gender,
 * alignment, and appearance, each its own numbered sub-menu. Shown once
 * the attribute screen is "done"; ITS "done" is what actually creates the
 * character now (see the CONN_CHAR_CREATE_OPTIONS case). */
static void show_options_screen(descriptor_t *d) {
    char head[96];
    snprintf(head, sizeof(head), "-- Finish up %s --", d->new_char_name);

    const char *align_word = d->new_char_alignment > 0 ? "Good"
                            : d->new_char_alignment < 0 ? "Evil" : "Neutral";
    const char *appear = d->new_char_appearance[0] ? d->new_char_appearance : "(none set)";

    char row1[80], row2[64], row3[BEING_APPEARANCE_LEN + 32];
    snprintf(row1, sizeof(row1), "<C>1)<z> Handedness: %-8s   <C>2)<z> Gender: %s",
             d->new_char_handed ? "Right" : "Left", gender_name(d->new_char_gender));
    snprintf(row2, sizeof(row2), "<C>3)<z> Alignment: %s", align_word);
    snprintf(row3, sizeof(row3), "<C>4)<z> Appearance: %s", appear);

    const char *const lines[] = {
        row1,
        row2,
        row3,
        "",
        "  Choose a number to adjust each, or",
        "  <C>D<z>)one   <C>R<z>eset   <C>A<z>bort or Quit",
    };
    descriptor_send(d, "\r\n");
    descriptor_send(d, head);
    send_boxed_menu(d, lines, (int)(sizeof(lines) / sizeof(lines[0])));
    descriptor_send(d, "> ");
}

/* CONN_CHAR_CREATE_OPT_HAND / _OPT_GENDER / _OPT_APPEARANCE: the three
 * numbered sub-menus reached from show_options_screen()'s options 1/2/4. */
static void show_opt_hand_screen(descriptor_t *d) {
    static const char *const lines[] = {
        "<C>1)<z> Left",
        "<C>2)<z> Right",
    };
    descriptor_send(d, "\r\n-- Choose your primary hand --");
    send_boxed_menu(d, lines, 2);
    descriptor_send(d, "Enter a number (1-2), or 'quit!' to cancel: ");
}

static void show_opt_gender_screen(descriptor_t *d) {
    static const char *const lines[] = {
        "<C>1)<z> Male",
        "<C>2)<z> Female",
        "<C>3)<z> Neuter",
    };
    descriptor_send(d, "\r\n-- Choose your gender --");
    send_boxed_menu(d, lines, 3);
    descriptor_send(d, "Enter a number (1-3), or 'quit!' to cancel: ");
}

static void show_opt_appearance_screen(descriptor_t *d) {
    descriptor_send(d, "\r\nDescribe how you look to others (blank to clear, 'quit!' to cancel): ");
}

/* CONN_CHAR_CREATE_RACE / CONN_CHAR_CREATE_TERRITORY / CONN_CHAR_CREATE_CLASS /
 * CONN_CHAR_CREATE_OPT_ALIGN (user 2026-07-11: "implement races, 6 player
 * races" / "implement classes, 6 player classes" / "ask player to choose
 * initial alignment"; territory is the Territory/Homeland audit item, user
 * 2026-08-03: "should be a choice after choosing race", see being.h's
 * player_territory_t doc comment): short numbered-choice steps on the way
 * to creating the character. Race, territory, and class are all chosen
 * FIRST, before attribute point-buy (user 2026-07-12: "selection of race
 * and class should go before picking attributes" -- territory slots into
 * that same rule, right after race, matching the real upstream's own
 * ordering), then each applies a fixed stat bonus/penalty on top of the
 * point-buy result once it's finished (race_stat_bonus()/
 * territory_stat_bonus()/class_stat_bonus(), being.c, applied in the
 * CONN_CHAR_CREATE_ATTRS "done" handler) -- the ACTUAL mechanics.
 *
 * NOTE this is a BREAKING change to the creation sequence for any test
 * script that used to send race-then-class directly (10-step pattern:
 * name/y/pw/pw/new/name/race/class/done/done) -- an EARLIER draft this
 * session made territory optional (options-menu-only) specifically to
 * avoid this, but the user asked for it right after race instead, matching
 * the real upstream. Every such script now needs ONE more numeric input
 * (a homeland pick, 1-3) between race and class. Not retrofitted across
 * every existing test in this pass (same "documented, not everywhere at
 * once" precedent other breaking changes in this codebase have used) --
 * only tests/smoke_test_territory.py (this feature's own test) was updated.
 *
 * What's shown here, though, is deliberately NOT those numbers (user
 * 2026-07-12: "char creation, dont tell the player number bonuses, tell
 * them this class X or this race X. be descriptive so they can imagine
 * the rest") -- each entry is a short evocative sentence in the same
 * direction as its real stat shift (an Elf really is quick and clever,
 * an Ogre really is strong and thick, etc), letting a new player feel
 * out a race or class by what it's LIKE rather than reading a spreadsheet
 * before they've even seen the game. The exact deltas still apply and
 * still show up for real in `score`'s attribute line once played. */
static void show_race_screen(descriptor_t *d) {
    char head[96];
    snprintf(head, sizeof(head), "-- Choose a race for %s --", d->new_char_name);
    descriptor_send(d, "\r\n");
    static const char *const lines[] = {
        "Each race leaves its own mark on you -- some traits sharpened,",
        "others dulled -- on top of whatever attributes you allocate next.",
        "",
        "<C>1)<z> Human   -- Adaptable and unremarkable in the best way: no",
        "              particular gift, but no particular weakness either.",
        "<C>2)<z> Elf     -- Graceful and quick-witted, with keen reflexes and a",
        "              mind suited to magic -- but their slight frames tire",
        "              and break more easily than sturdier folk.",
        "<C>3)<z> Ogre    -- Towering brutes of raw physical power, built to",
        "              smash through anything in their path -- wit and",
        "              charm were never their strong suits.",
        "<C>4)<z> Dwarf   -- Stout and famously hardy, shrugging off punishment",
        "              that would fell lesser folk -- though their stocky",
        "              build makes them a touch clumsy, and their bluntness",
        "              doesn't win many friends.",
        "<C>5)<z> Hobbit  -- Small, nimble, and quick on their feet -- but they",
        "              lack the brute strength or staying power of the",
        "              bigger races.",
        "<C>6)<z> Gnome   -- Brilliant and inventive, with minds built for study",
        "              and spellcraft -- but their small stature leaves",
        "              them physically fragile.",
    };
    descriptor_send(d, head);
    send_boxed_menu(d, lines, (int)(sizeof(lines) / sizeof(lines[0])));
    descriptor_send(d, "Enter a number (1-6), or 'quit!' to cancel: ");
}

/* Territory/Homeland (Sneezy -> Tobin feature audit item, see being.h's
 * player_territory_t doc comment for the scope-down rationale): a forced
 * step right after race, before class -- matches the real upstream's own
 * ordering (user, 2026-08-03: "should be a choice after choosing race",
 * overriding an earlier draft that made it optional/options-menu-only to
 * avoid disturbing existing creation-sequence test scripts). Same
 * "evocative sentence, no raw numbers" convention as show_race_screen()/
 * show_class_screen() above. */
static void show_territory_screen(descriptor_t *d) {
    char head[96];
    snprintf(head, sizeof(head), "-- Choose a homeland for %s --", d->new_char_name);
    descriptor_send(d, "\r\n");
    static const char *const lines[] = {
        "Where you grew up left its own mark too, on top of your race.",
        "",
        "<C>1)<z> Urban    -- Raised in a city, sharp-tongued and quick-",
        "               witted -- but soft hands and a soft frame never",
        "               had to survive the wild.",
        "<C>2)<z> Rural    -- Raised in a farming village, practical and",
        "               sure-footed from a life of real work -- plain-",
        "               spoken, without the city folk's polish.",
        "<C>3)<z> Wilds    -- Raised on the frontier, tough and strong",
        "               from a hard life with little shelter -- blunt",
        "               and unworldly, with no patience for cleverness.",
    };
    descriptor_send(d, head);
    send_boxed_menu(d, lines, (int)(sizeof(lines) / sizeof(lines[0])));
    descriptor_send(d, "Enter a number (1-3), or 'quit!' to cancel: ");
}

static void show_class_screen(descriptor_t *d) {
    char head[96];
    snprintf(head, sizeof(head), "-- Choose a class for %s --", d->new_char_name);
    descriptor_send(d, "\r\n");
    static const char *const lines[] = {
        "Your class shapes you further still, on top of your race.",
        "",
        "<C>1)<z> Mage     -- Wields raw arcane power through study and",
        "               spellcraft, formidable at a distance -- but",
        "               relies on the mind over muscle, and can't take",
        "               a hit.",
        "<C>2)<z> Cleric   -- A devoted channel for divine power, wise beyond",
        "               their years, healing allies and smiting the",
        "               wicked -- deliberate and grounded rather than",
        "               quick or forceful.",
        "<C>3)<z> Warrior  -- A hardened fighter built for the front line,",
        "               tough as nails and strong as an ox -- not known",
        "               for tact or deep wisdom.",
        "<C>4)<z> Thief    -- Fast, agile, and light on their feet, relying on",
        "               speed and cunning over brute force -- deadly",
        "               from the shadows, but not built to trade blows.",
        "<C>5)<z> Druid    -- In tune with nature, sturdy and wise in the ways",
        "               of the wild -- but bookish arcane study was",
        "               never their calling.",
        "<C>6)<z> Monk     -- A disciplined martial artist honed through",
        "               rigorous training, strong and resilient -- but",
        "               their austere ways leave little room for charm.",
    };
    descriptor_send(d, head);
    send_boxed_menu(d, lines, (int)(sizeof(lines) / sizeof(lines[0])));
    descriptor_send(d, "Enter a number (1-6), or 'quit!' to cancel: ");
}

/* Alignment sub-menu (option 3 of show_options_screen()) -- same three
 * choices the old standalone CONN_CHAR_CREATE_ALIGNMENT screen offered,
 * just reached via the options menu now instead of always being forced
 * right after attrs; defaults to Neutral if never visited. */
static void show_opt_align_screen(descriptor_t *d) {
    descriptor_send(d, "\r\n-- Choose an alignment --");
    static const char *const lines[] = {
        "<C>1)<z> Good     -- other good-aligned mobs leave you be; evil ones may",
        "               target you, and you'll never be picked on by mobs that",
        "               favor good",
        "<C>2)<z> Neutral  -- no faction leans your way, but you may draw the odd",
        "               taunt from evil or word of support from good",
        "<C>3)<z> Evil     -- the mirror of Good",
    };
    send_boxed_menu(d, lines, (int)(sizeof(lines) / sizeof(lines[0])));
    descriptor_send(d, "Enter a number (1-3), or 'quit!' to cancel: ");
}

/* Shared finish-up for both "play an existing character" and "just
 * finished creating one": place them in their load room and start play. */
static void enter_world(descriptor_t *d, being_t *b) {
    /* If this player has a linkdead body sitting somewhere (user
     * requirement: stay in the room until reconnect), resume in THAT room
     * instead of the load room -- but `b` is still a fresh DB load, so any
     * change made while linkdead (a promotion, an edplayer edit) still
     * takes effect; only the room carries over, not stale in-memory state.
     * The old body is discarded once its room is captured. */
    int linkdead_room_vnum = -1;
    being_t *linkdead = world_find_linkdead_pc(b->player_id);
    if (linkdead && linkdead != b) {
        if (linkdead->base.roomp)
            linkdead_room_vnum = linkdead->base.roomp->vnum;
        being_destroy(linkdead);
    }

    /* Duplicate character instance gate (spell/skill... no -- TODO.md
     * priority item, user 2026-07-30): the SAME character (player_id)
     * logged in twice simultaneously, even across different accounts (a
     * data-corruption scenario) -- prevented outright by reclaiming the
     * old connection, matching SneezyMUD's own reclaim-on-relogin
     * behavior. The linkdead case above already ran and would have
     * destroyed+cleared a linkdead match, so anything found here by
     * world_find_active_pc() is guaranteed to be a genuinely ACTIVE
     * (desc != NULL) duplicate, never a stale linkdead body. `existing`
     * is a DIFFERENT being_t than `b` (this call's own fresh DB load) --
     * descriptor_destroy() properly closes/frees the old socket (same
     * teardown a real link-loss gets: room announce, log, snoop
     * unhooking) and leaves `existing` linkdead-but-alive, so it must be
     * explicitly being_destroy()ed right after or it leaks forever
     * (nothing will ever reconnect to IT specifically -- the new
     * connection resumes via the fresh `b` instead). Its room is
     * captured into `linkdead_room_vnum` first, same "resume where you
     * left off" precedent the block above already established. Doesn't
     * call descriptor_destroy() on the old descriptor directly -- see
     * descriptor.h's `pending_destroy` doc comment for why that's unsafe
     * from here (a real SIGSEGV caught live) and what actually tears it
     * down instead (game_loop_run()). */
    being_t *existing = world_find_active_pc(b->player_id);
    if (existing && existing != b && existing->desc) {
        descriptor_send(existing->desc,
            "\r\nYour connection has been taken over from another location.\r\n");
        if (existing->base.roomp)
            linkdead_room_vnum = existing->base.roomp->vnum;
        existing->desc->character = NULL; /* so the deferred destroy skips the link-drop/detach block */
        existing->desc->pending_destroy = true;
        existing->desc = NULL;
        being_destroy(existing);
    }

    /* Multiplay gate: unless the game flag is on, a MORTAL account may have
     * only one character in the world at a time. Immortals are exempt (they
     * can hold several connections). */
    if (!being_is_immortal(b) && !multiplay_allowed()) {
        for (descriptor_t *it = g_descriptors; it; it = it->next) {
            if (it != d && it->character
                && it->character->account_id == b->account_id) {
                descriptor_send(d,
                    "You already have a character in the game -- "
                    "multiplaying is not allowed.\r\n");
                being_destroy(b);
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return;
            }
        }
    }

    b->desc = d;
    d->character = b;

    int room_vnum;
    if (linkdead_room_vnum >= 0) {
        room_vnum = linkdead_room_vnum;
    } else {
        room_vnum = player_load_room(b->base.name, d->account.account_id);
        if (room_vnum < 0)
            room_vnum = DEFAULT_LOAD_ROOM_MORTAL;
        /* Immortals default home to room 1 (user spec) -- only when their
         * stored load room is still the mortal default; an explicit loadroom
         * choice wins. */
        if (being_is_immortal(b) && room_vnum == DEFAULT_LOAD_ROOM_MORTAL)
            room_vnum = DEFAULT_LOAD_ROOM_IMMORTAL;
    }

    room_t *r = world_get_room(room_vnum);
    if (!r) {
        r = room_repo_load(room_vnum);
        if (!r)
            r = room_repo_load(1);
        if (r)
            world_register_room(r);
    }
    if (r)
        thing_set_room(&b->base, r);

    char welcome[128];
    if (linkdead_room_vnum >= 0)
        snprintf(welcome, sizeof(welcome), "Welcome back, %s! You resume where you left off.\r\n",
                 b->base.name);
    else
        snprintf(welcome, sizeof(welcome), "Welcome, %s!\r\n", b->base.name);
    descriptor_send(d, welcome);

    /* Unseen-news notice ("News follow-ups", user 2026-07-17 batch: "show
     * unseen news at login (per-player last-seen)"). Never shows a count or
     * id (house rule: no numbers in news text, news.sql) -- just whether
     * anything posted since this player last ran `news`. Cleared by
     * cmd_news.c the next time they actually read the feed, not here, so a
     * login that doesn't check `news` gets reminded again next time. */
    long newest_news_id = news_repo_max_id(false);
    if (newest_news_id > 0 && player_get_news_last_seen(b->player_id) < newest_news_id)
        descriptor_send(d, "<y>There is new news! Type 'news' to catch up.<z>\r\n");

    /* Same notice, immortal-only wiznews channel (user 2026-07-25: "add a
     * message like this for wiznews as well"). Gated on being_is_immortal()
     * since a mortal can't reach `wiznews` at all -- showing the reminder
     * to someone who could never act on it would just be noise. */
    if (being_is_immortal(b)) {
        long newest_wiznews_id = news_repo_max_id(true);
        if (newest_wiznews_id > 0 && player_get_wiznews_last_seen(b->player_id) < newest_wiznews_id)
            descriptor_send(d, "<y>There is new wiznews! Type 'wiznews' to catch up.<z>\r\n");
    }

    /* Connect is a typed player-io event, symmetric to the link-loss line
     * in descriptor_destroy(): logged to the file and echoed to online
     * immortals with a colored [PIO] tag, carrying the IP. */
    game_log(LOG_PIO, linkdead_room_vnum >= 0 ? "%s has reconnected. [%s]" : "%s has connected. [%s]",
             b->base.name, descriptor_display_host(d));
    d->state = CONN_PLAYING;
    cmd_dispatch(d, "look"); /* prompt comes from the game loop's prompter */
}

/* ---- Menu-driven room builder (redit), CONN_REDIT_* ------------------- *
 * Working-copy model: the room is copied into d->redit_work on entry; every
 * field edit mutates that copy; (S)ave applies the copy to the live room +
 * DB, (Q)uit can discard. Replaces the old one-shot `redit <field> <args>`
 * command form. Menu shape is the user's wireframe. */

/* Column width every ed* editor's `/format` wraps prose to -- a
 * conservative width safe on any real terminal, matching the assumption
 * cmd_help.c's send_columns() already makes about display width. */
#define EDITOR_FORMAT_WIDTH 78

/* Reflows d->edit_buf into hard-wrapped lines at EDITOR_FORMAT_WIDTH
 * columns: words are re-joined and re-broken to the width, but a blank
 * line (2+ consecutive newlines) is kept as a paragraph break. The
 * original line breaks *within* a paragraph are discarded -- the classic
 * MUD editor "type without worrying about line length, then format"
 * convenience. Every paragraph's FIRST line gets a 2-space indent (user
 * 2026-07-11: "in the /format command in the editor, always indent a
 * paragraph with 2 spaces") -- wrapped continuation lines within the same
 * paragraph do not get re-indented, standard prose-paragraph style.
 * Tobin-specific (no equivalent in the original -- see TODO.md). */
static void editor_format(descriptor_t *d) {
    char out[HELP_BODY_MAX];
    size_t oi = 0;
    size_t col = 0;
    bool at_para_start = true;
    const char *p = d->edit_buf;

    while (*p && oi + 1 < sizeof(out)) {
        int newlines = 0;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            if (*p == '\n')
                newlines++;
            p++;
        }
        if (!*p)
            break;

        if (newlines >= 2 && !at_para_start) {
            if (oi + 2 >= sizeof(out))
                break;
            out[oi++] = '\n';
            out[oi++] = '\n';
            col = 0;
            at_para_start = true;
        }

        const char *wstart = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
            p++;
        size_t wlen = (size_t)(p - wstart);

        if (at_para_start) {
            if (oi + 2 >= sizeof(out))
                break;
            out[oi++] = ' ';
            out[oi++] = ' ';
            col = 2;
        } else {
            if (col + 1 + wlen > EDITOR_FORMAT_WIDTH) {
                if (oi + 1 >= sizeof(out))
                    break;
                out[oi++] = '\n';
                col = 0;
            } else {
                if (oi + 1 >= sizeof(out))
                    break;
                out[oi++] = ' ';
                col++;
            }
        }

        if (oi + wlen >= sizeof(out))
            break;
        memcpy(out + oi, wstart, wlen);
        oi += wlen;
        col += wlen;
        at_para_start = false;
    }
    if (oi + 1 < sizeof(out))
        out[oi++] = '\n';
    out[oi] = '\0';

    memcpy(d->edit_buf, out, oi + 1);
    d->edit_len = (int)oi;
}

/* Shared string-editor line handling: /s save, /a abort, /b blank (clear),
 * /f format, else append the line into edit_buf. The caller owns what SAVE
 * and ABORT mean. */
typedef enum { EDITOR_CONTINUE, EDITOR_SAVE, EDITOR_ABORT } editor_action_t;

static editor_action_t editor_feed(descriptor_t *d, const char *line) {
    /* One consistent slash-command set across every editor (user 2026-07-07),
     * each keyed to its action's first letter: /s Save, /a Abort, /b Blank
     * (clear the buffer), /f Format. Any other line is literal text appended
     * to the buffer -- so a line of just "." or "~" is now content, not a
     * command (the old single-key/`/clear`/`/format` forms were removed). */
    if (strcmp(line, "/s") == 0)
        return EDITOR_SAVE;
    if (strcmp(line, "/a") == 0)
        return EDITOR_ABORT;
    if (strcmp(line, "/b") == 0) {
        d->edit_buf[0] = '\0';
        d->edit_len = 0;
        descriptor_send(d, "Buffer blanked.\r\n] ");
        return EDITOR_CONTINUE;
    }
    if (strcmp(line, "/f") == 0) {
        editor_format(d);
        char out[HELP_BODY_MAX + 64];
        snprintf(out, sizeof(out), "Reformatted:\r\n%s] ", d->edit_buf);
        descriptor_send(d, out);
        return EDITOR_CONTINUE;
    }
    /* /r <topics>: set the help topic's "Related: ..." footer (user
     * 2026-07-11: "in the help editor we should be able to set related
     * topics in there") -- help-topic editing only; appended back onto
     * edit_buf on save rather than typed as literal body text. Bare "/r"
     * clears it. Scoped to EDIT_HELP_TOPIC so a "/r"-prefixed line in any
     * other editor (room descriptions, news, ...) is just ordinary text. */
    if (d->edit_kind == EDIT_HELP_TOPIC && (strcmp(line, "/r") == 0 || strncmp(line, "/r ", 3) == 0)) {
        const char *rel = line[2] == ' ' ? line + 3 : line + 2;
        while (*rel == ' ')
            rel++;
        snprintf(d->edit_related, sizeof(d->edit_related), "%s", rel);
        if (d->edit_related[0]) {
            char out[192];
            snprintf(out, sizeof(out), "Related topics set to: %s\r\n] ", d->edit_related);
            descriptor_send(d, out);
        } else {
            descriptor_send(d, "Related topics cleared.\r\n] ");
        }
        return EDITOR_CONTINUE;
    }
    size_t add = strlen(line);
    if ((size_t)d->edit_len + add + 2 < sizeof(d->edit_buf)) {
        memcpy(d->edit_buf + d->edit_len, line, add);
        d->edit_len += (int)add;
        d->edit_buf[d->edit_len++] = '\n';
        d->edit_buf[d->edit_len] = '\0';
    } else {
        descriptor_send(d, "The text is full -- /s to save or /a to abort.\r\n");
    }
    descriptor_send(d, "] ");
    return EDITOR_CONTINUE;
}

/* Direction token -> index (name prefix or 0-9), or -1. */
static int redit_parse_dir(const char *tok) {
    if (isdigit((unsigned char)tok[0])) {
        int dir = atoi(tok);
        return (dir >= 0 && dir < ROOM_NUM_EXITS) ? dir : -1;
    }
    size_t len = strlen(tok);
    if (len == 0)
        return -1;
    for (int i = 0; i < ROOM_NUM_EXITS; i++)
        if (strncasecmp(DIR_NAMES[i], tok, len) == 0)
            return i;
    return -1;
}

/* Renders the CONN_REDIT_MENU top-level screen for the current redit_work
 * working copy: name, sector, flags, the numbered field menu, description,
 * exit summary, and a "* unsaved changes *" marker when redit_dirty. */
static void show_redit_menu(descriptor_t *d) {
    room_t *w = &d->redit_work;
    char flagbuf[256], exitbuf[640];
    size_t e = 0;
    exitbuf[0] = '\0';
    bool any = false;
    for (int i = 0; i < ROOM_NUM_EXITS; i++) {
        if (w->exits[i] < 0)
            continue;
        e += (size_t)snprintf(exitbuf + e, sizeof(exitbuf) > e ? sizeof(exitbuf) - e : 0,
                              "%s%s->%d%s%s", any ? "  " : "", DIR_NAMES[i], w->exits[i],
                              w->exit_door[i] ? "/" : "",
                              w->exit_door[i] ? door_type_name(w->exit_door[i]) : "");
        any = true;
        if (e >= sizeof(exitbuf))
            break;
    }
    if (!any)
        snprintf(exitbuf, sizeof(exitbuf), "none");

    const char *descnl = (w->description[0] &&
                          w->description[strlen(w->description) - 1] != '\n') ? "\r\n" : "";
    char out[ROOM_DESCRIPTION_MAX + 1280];
    snprintf(out, sizeof(out),
             "\r\n<c>Room Name:<z> %s\r\n"
             "<c>Number:<z> %d      <c>Sector Type:<z> <c>[ %s ]<z>\r\n"
             "<c>Flags:<z> <p>%s<z>\r\n\r\n"
             "<c>Menu:<z>\r\n"
             "   <c>1)<z> <p>Name<z>              <c>2)<z> <p>Description<z>\r\n"
             "   <c>3)<z> <p>Flags<z>             <c>4)<z> <p>Sector Type<z>\r\n"
             "   <c>5)<z> <p>Exits<z>             <c>6)<z> <p>Max Capacity<z>: %d\r\n"
             "   <c>7)<z> <p>Room Height<z>: %d    <c>8)<z> <p>Extra Descriptions<z>\r\n\r\n"
             "Description:\r\n%s%s"
             "Exits: %s\r\n\r\n"
             "   <c>C)<z> <p>Clear room out<z>    <c>S)<z> <p>Save<z>    <c>Q)<z> <p>Quit<z>\r\n%s[edit room] ",
             w->base.name, w->vnum, sector_name(w->sector),
             room_flag_names(w->room_flag, flagbuf, sizeof(flagbuf)),
             w->capacity, w->height,
             w->description, descnl, exitbuf,
             d->redit_dirty ? "<c>* unsaved changes *<z>\r\n" : "");
    descriptor_send(d, out);
    d->state = CONN_REDIT_MENU;
}

/* Renders the redit room-flags toggle submenu (CONN_REDIT_FLAGS): every
 * known room flag bit numbered, with an [x]/[ ] marker read straight off
 * the working copy. */
static void show_redit_flags(descriptor_t *d) {
    char out[2048];
    size_t n = (size_t)snprintf(out, sizeof(out),
        "\r\nRoom flags for %d -- toggle by number, blank to return:\r\n",
        d->redit_work.vnum);
    for (int b = 0; b < room_flag_count(); b++) {
        n += (size_t)snprintf(out + n, sizeof(out) > n ? sizeof(out) - n : 0,
            "  <c>%2d<z> [%c] <p>%-14s<z>%s", b,
            (d->redit_work.room_flag & (1 << b)) ? 'x' : ' ',
            room_flag_name(b), (b % 2 == 1) ? "\r\n" : "");
        if (n >= sizeof(out))
            break;
    }
    if (room_flag_count() % 2 != 0 && n < sizeof(out))
        snprintf(out + n, sizeof(out) - n, "\r\n");
    descriptor_send(d, out);
    descriptor_send(d, "flag> ");
    d->state = CONN_REDIT_FLAGS;
}

/* Renders the redit terrain/sector-type picker submenu (CONN_REDIT_TERRAIN),
 * listing every sector type by number. */
static void show_redit_terrain(descriptor_t *d) {
    char out[4096];
    size_t n = (size_t)snprintf(out, sizeof(out),
        "\r\nTerrain for %d (current %d: %s) -- choose a number, blank to return:\r\n",
        d->redit_work.vnum, d->redit_work.sector, sector_name(d->redit_work.sector));
    for (int s = 0; s < MAX_SECTOR_TYPES; s++) {
        n += (size_t)snprintf(out + n, sizeof(out) > n ? sizeof(out) - n : 0,
            "  <c>%2d<z> <p>%-22s<z>%s", s, sector_name(s), (s % 3 == 2) ? "\r\n" : "");
        if (n >= sizeof(out))
            break;
    }
    if (MAX_SECTOR_TYPES % 3 != 0 && n < sizeof(out))
        snprintf(out + n, sizeof(out) - n, "\r\n");
    descriptor_send(d, out);
    descriptor_send(d, "terrain> ");
    d->state = CONN_REDIT_TERRAIN;
}

/* Renders the redit exits overview (CONN_REDIT_EXITS): every direction with
 * its target/door/condition summary, or "(none)" if unset. Picking a
 * direction here drops into show_redit_exit_menu()'s per-exit detail. */
static void show_redit_exits(descriptor_t *d) {
    room_t *w = &d->redit_work;
    char out[1400];
    size_t n = (size_t)snprintf(out, sizeof(out),
        "\r\nExits for %d -- choose a direction (name or number) to edit, blank to return:\r\n",
        w->vnum);
    for (int i = 0; i < ROOM_NUM_EXITS; i++) {
        char info[96];
        if (w->exits[i] < 0) {
            snprintf(info, sizeof(info), "(none)");
        } else {
            char cbuf[192];
            snprintf(info, sizeof(info), "-> %-7d door: %-11s cond: %s",
                     w->exits[i], door_type_name(w->exit_door[i]),
                     exit_cond_names(w->exit_cond[i], cbuf, sizeof(cbuf)));
        }
        n += (size_t)snprintf(out + n, sizeof(out) > n ? sizeof(out) - n : 0,
            "  <c>%2d)<z> <p>%-10s<z> %s\r\n", i, DIR_NAMES[i], info);
        if (n >= sizeof(out))
            break;
    }
    descriptor_send(d, out);
    descriptor_send(d, "exit> ");
    d->state = CONN_REDIT_EXITS;
}

/* Renders one exit's detail submenu (CONN_REDIT_EXIT_MENU) for the direction
 * in d->redit_exit_dir: target room, door type, conditions, and a remove
 * option. */
static void show_redit_exit_menu(descriptor_t *d) {
    room_t *w = &d->redit_work;
    int dir = d->redit_exit_dir;
    char tgt[24], cbuf[192], out[512];
    if (w->exits[dir] < 0)
        snprintf(tgt, sizeof(tgt), "(none)");
    else
        snprintf(tgt, sizeof(tgt), "%d", w->exits[dir]);
    snprintf(out, sizeof(out),
        "\r\n<c>Exit %s<z>\r\n"
        "  Target: %s   Door: %s   Cond: %s\r\n\r\n"
        "  <c>1)<z> <p>Target room<z>      <c>2)<z> <p>Door type<z>\r\n"
        "  <c>3)<z> <p>Conditions<z>       <c>4)<z> <p>Remove this exit<z>\r\n"
        "  blank) back\r\nexit-%s> ",
        DIR_NAMES[dir], tgt, door_type_name(w->exit_door[dir]),
        exit_cond_names(w->exit_cond[dir], cbuf, sizeof(cbuf)), DIR_NAMES[dir]);
    descriptor_send(d, out);
    d->state = CONN_REDIT_EXIT_MENU;
}

/* Renders the door-type picker submenu (CONN_REDIT_EXIT_DOORTYPE) for the
 * exit direction in d->redit_exit_dir. */
static void show_redit_doortype(descriptor_t *d) {
    int dir = d->redit_exit_dir;
    char out[640];
    size_t n = (size_t)snprintf(out, sizeof(out),
        "\r\nDoor type for %s (current: %s) -- choose a number, blank to keep:\r\n",
        DIR_NAMES[dir], door_type_name(d->redit_work.exit_door[dir]));
    for (int t = 0; t < door_type_count(); t++) {
        n += (size_t)snprintf(out + n, sizeof(out) > n ? sizeof(out) - n : 0,
            "  <c>%2d<z> <p>%-14s<z>%s", t, door_type_name(t), (t % 3 == 2) ? "\r\n" : "");
        if (n >= sizeof(out))
            break;
    }
    if (door_type_count() % 3 != 0 && n < sizeof(out))
        snprintf(out + n, sizeof(out) - n, "\r\n");
    descriptor_send(d, out);
    descriptor_send(d, "door> ");
    d->state = CONN_REDIT_EXIT_DOORTYPE;
}

/* Renders the exit-conditions toggle submenu (CONN_REDIT_EXIT_CONDITIONS)
 * for the exit direction in d->redit_exit_dir, one [x]/[ ] bit per known
 * condition flag. */
static void show_redit_conditions(descriptor_t *d) {
    int dir = d->redit_exit_dir;
    int cond = d->redit_work.exit_cond[dir];
    char out[1024];
    size_t n = (size_t)snprintf(out, sizeof(out),
        "\r\nConditions for %s -- toggle by number, blank to return:\r\n",
        DIR_NAMES[dir]);
    for (int b = 0; b < exit_cond_count(); b++) {
        n += (size_t)snprintf(out + n, sizeof(out) > n ? sizeof(out) - n : 0,
            "  <c>%2d<z> [%c] <p>%-18s<z>%s", b, (cond & (1 << b)) ? 'x' : ' ',
            exit_cond_name(b), (b % 2 == 1) ? "\r\n" : "");
        if (n >= sizeof(out))
            break;
    }
    if (exit_cond_count() % 2 != 0 && n < sizeof(out))
        snprintf(out + n, sizeof(out) - n, "\r\n");
    descriptor_send(d, out);
    descriptor_send(d, "cond> ");
    d->state = CONN_REDIT_EXIT_CONDITIONS;
}

/* Extra Descriptions list (redit menu 8) -- freshly queried from the DB
 * every time, not cached in the working copy (see room_repo.h's comment on
 * why extras commit immediately). The case handler for a typed digit
 * re-queries the same way and indexes by the same position; the only race
 * this leaves is a genuinely concurrent edit by a second builder in the
 * split second between display and keystroke, same class of race the
 * account editor already accepts (see edaccount_id's comment). */
static void show_redit_extra_menu(descriptor_t *d) {
    int vnum = d->redit_work.vnum;
    char names[ROOM_EXTRA_MAX_LIST][ROOM_EXTRA_NAME_LEN];
    int n = room_repo_extra_list(vnum, names, ROOM_EXTRA_MAX_LIST);

    char out[4096];
    size_t len = (size_t)snprintf(out, sizeof(out),
        "\r\nExtra Descriptions for %d -- choose a number to edit, blank to return:\r\n",
        vnum);
    if (n == 0) {
        len += (size_t)snprintf(out + len, sizeof(out) > len ? sizeof(out) - len : 0,
            "  (none yet)\r\n");
    } else {
        for (int i = 0; i < n && len < sizeof(out); i++) {
            len += (size_t)snprintf(out + len, sizeof(out) > len ? sizeof(out) - len : 0,
                "  <c>%2d)<z> %s\r\n", i + 1, names[i]);
        }
    }
    if (len < sizeof(out)) {
        len += (size_t)snprintf(out + len, sizeof(out) - len, "\r\n  <c>A)<z> <p>Add new<z>\r\n");
        if (n > 0 && len < sizeof(out))
            len += (size_t)snprintf(out + len, sizeof(out) - len, "  <c>Z)<z> <p>Delete ALL<z>\r\n");
        if (len < sizeof(out))
            snprintf(out + len, sizeof(out) - len, "extra> ");
    }
    descriptor_send(d, out);
    d->state = CONN_REDIT_EXTRA_MENU;
}

/* One extra description's detail submenu (mirrors show_redit_exit_menu()'s
 * target/door/conditions/remove shape). d->redit_extra_name names which
 * roomextra row this operates on. */
static void show_redit_extra_item(descriptor_t *d) {
    char desc[4096];
    if (!room_repo_extra_get(d->redit_work.vnum, d->redit_extra_name, desc, sizeof(desc)))
        desc[0] = '\0'; /* shouldn't happen (just listed/created), degrade gracefully */

    const char *descnl = (desc[0] && desc[strlen(desc) - 1] != '\n') ? "\r\n" : "";
    char out[sizeof(desc) + 512];
    snprintf(out, sizeof(out),
        "\r\n<c>Extra Description: %s<z>\r\n\r\n"
        "Description:\r\n%s%s\r\n"
        "  <c>1)<z> <p>Keywords<z>         <c>2)<z> <p>Description<z>\r\n"
        "  <c>3)<z> <p>Delete<z>\r\n"
        "  blank) back\r\nextra-desc> ",
        d->redit_extra_name, desc, descnl);
    descriptor_send(d, out);
    d->state = CONN_REDIT_EXTRA_ITEM;
}

/* Loads a room into the world if absent; returns NULL if it doesn't exist. */
static room_t *redit_room_get(int vnum) {
    room_t *r = world_get_room(vnum);
    if (!r) {
        r = room_repo_load(vnum);
        if (r)
            world_register_room(r);
    }
    return r;
}

/* Applies the working copy's exits to the live room + DB, diffing against the
 * live room's current exits. New targets are auto-created; the reverse exit
 * is fixed on link; removing an exit also removes the neighbour's reverse
 * exit when it points back here (no dangling one-way stubs). */
static void redit_apply_exits(descriptor_t *d, room_t *live) {
    for (int dir = 0; dir < ROOM_NUM_EXITS; dir++) {
        int wdest = d->redit_work.exits[dir];
        int ldest = live->exits[dir];

        if (wdest < 0) {
            if (ldest >= 0) {
                room_repo_delete_exit(live->vnum, dir);
                room_t *to = redit_room_get(ldest);
                if (to) {
                    int rev = REV_DIR[dir];
                    if (to->exits[rev] == live->vnum) {
                        to->exits[rev] = -1;
                        room_repo_delete_exit(to->vnum, rev);
                    }
                }
            }
            live->exits[dir] = -1;
            live->exit_door[dir] = 0;
            live->exit_cond[dir] = 0;
            continue;
        }

        room_t *to = redit_room_get(wdest);
        if (!to) {
            to = room_create(wdest, "An unfinished room",
                             "This freshly dug room has not been described yet.\n",
                             live->sector);
            if (!to || !room_repo_save(to)) {
                room_destroy(to);
                continue; /* skip this exit on failure */
            }
            world_register_room(to);
        }
        live->exits[dir] = wdest;
        live->exit_door[dir] = d->redit_work.exit_door[dir];
        live->exit_cond[dir] = d->redit_work.exit_cond[dir];
        room_repo_save_exit(live->vnum, dir, wdest,
                            live->exit_door[dir], live->exit_cond[dir]);
        int rev = REV_DIR[dir];
        if (to->exits[rev] < 0) {
            to->exits[rev] = live->vnum;
            room_repo_save_exit(to->vnum, rev, live->vnum, 0, 0);
        }
    }
}

/* Commits the redit working copy back to the live room + DB: scalars are
 * applied first so any auto-created exit target inherits the new sector,
 * then redit_apply_exits() reconciles the exit table. Clears redit_dirty on
 * success. */
static void redit_save(descriptor_t *d) {
    room_t *live = world_get_room(d->redit_work.vnum);
    if (!live) {
        descriptor_send(d, "That room is gone -- nothing saved.\r\n");
        return;
    }
    /* Scalars first so auto-created exit targets inherit the new sector. */
    snprintf(live->base.name, sizeof(live->base.name), "%s", d->redit_work.base.name);
    snprintf(live->description, sizeof(live->description), "%s", d->redit_work.description);
    live->sector = d->redit_work.sector;
    live->room_flag = d->redit_work.room_flag;
    live->capacity = d->redit_work.capacity;
    live->height = d->redit_work.height;
    redit_apply_exits(d, live);
    if (room_repo_save(live)) {
        d->redit_dirty = false;
        descriptor_send(d, "Room saved.\r\n");
    } else {
        descriptor_send(d, "Save failed -- the DB rejected it.\r\n");
    }
}

/* Resets the redit working copy to a blank "unfinished room" (name,
 * description, sector, flags, capacity, height, and every exit), marking it
 * dirty -- the (C)lear room out menu option. Nothing is written to the DB
 * until a subsequent (S)ave. */
static void redit_clear(descriptor_t *d) {
    snprintf(d->redit_work.base.name, sizeof(d->redit_work.base.name), "%s",
             "An unfinished room");
    snprintf(d->redit_work.description, sizeof(d->redit_work.description), "%s",
             "This room has not been described yet.\n");
    d->redit_work.sector = 0;
    d->redit_work.room_flag = 0;
    d->redit_work.capacity = 0;
    d->redit_work.height = 0;
    for (int i = 0; i < ROOM_NUM_EXITS; i++) {
        d->redit_work.exits[i] = -1;
        d->redit_work.exit_door[i] = 0;
        d->redit_work.exit_cond[i] = 0;
    }
    d->redit_dirty = true;
}

/* Quits the room editor back to CONN_PLAYING, discarding any unsaved
 * working-copy changes, and flushes any held messages that piled up. */
static void redit_leave(descriptor_t *d) {
    d->state = CONN_PLAYING;
    d->redit_dirty = false;
    descriptor_send(d, "Leaving the room editor.\r\n");
    descriptor_editor_exit_notice(d);
}

/* See descriptor.h. */
bool descriptor_redit_begin(descriptor_t *d, int vnum) {
    room_t *live = redit_room_get(vnum);
    if (!live) {
        /* No room yet -- create a blank one and open THAT, rather than
         * refusing (2026-07-25, user: "if one doesn't exist a blank one
         * should be created", then "objects and rooms should behave the
         * same" -- same "An unfinished room" convention redit_apply_exits()
         * already uses for an exit target that doesn't exist yet). */
        live = room_create(vnum, "An unfinished room",
                           "This freshly dug room has not been described yet.\n", 0);
        if (!live || !room_repo_save(live)) {
            room_destroy(live);
            return false;
        }
        world_register_room(live);
        descriptor_send(d, "No room existed at that vnum -- created a blank one.\r\n");
    }
    d->redit_work = *live; /* field copy; only scalars + exits are ever used
                              or applied back, never the base pointers. */
    d->redit_dirty = false;
    show_redit_menu(d);
    return true;
}

/* Renders the CONN_EDPLAYER_MENU top-level screen for the edplayer_work
 * snapshot: level/XP/HP, attributes, gender, title, load room, handedness,
 * class, race, and an "* unsaved changes *" marker when edplayer_dirty. */
static void show_edplayer_menu(descriptor_t *d) {
    being_t *w = &d->edplayer_work;
    char out[900];
    snprintf(out, sizeof(out),
             "\r\n<c>Editing player:<z> %s\r\n\r\n"
             "   <c>1)<z> <p>Level<z>: %d              <c>2)<z> <p>Experience<z>: %ld\r\n"
             "   <c>3)<z> <p>HP/Max HP<z>: %d/%d       <c>4)<z> <p>Attributes<z> (str/dex/con/int/wis/cha)\r\n"
             "   <c>5)<z> <p>Gender<z>: %s      <c>6)<z> <p>Title<z>: %s\r\n"
             "   <c>7)<z> <p>Load Room<z>: %d          <c>8)<z> <p>Handedness<z>: %s\r\n"
             "   <c>9)<z> <p>Class<z>: %s       <c>0)<z> <p>Race<z>: %s\r\n\r\n"
             "   <c>S)<z> <p>Save<z>    <c>Q)<z> <p>Quit<z>%s\r\n[edit player] ",
             w->base.name, w->progress.level, w->progress.experience,
             w->progress.hp, w->progress.max_hp,
             gender_name(w->gender), w->title[0] ? w->title : "(none)",
             d->edplayer_load_room, w->handed_right ? "right" : "left",
             class_name(w->char_class), race_name(w->race),
             d->edplayer_dirty ? "\r\n   <c>* unsaved changes *<z>" : "");
    descriptor_send(d, out);
    d->state = CONN_EDPLAYER_MENU;
}

/* Writes the working copy back to the DB, and -- if that player happens to
 * be connected and playing right now -- syncs their live being_t too, so
 * the change takes effect without a relog (same courtesy `promote` gives
 * an online target). */
static void edplayer_save(descriptor_t *d) {
    being_t *w = &d->edplayer_work;
    bool ok = player_progress_save(w->player_id, &w->progress)
        && player_attrs_save(w->player_id, &w->attrs)
        && player_set_title(w->base.name, w->account_id, w->title)
        && player_set_load_room(w->base.name, w->account_id, d->edplayer_load_room)
        && player_set_gender_by_name(w->base.name, w->gender)
        && player_set_handed_by_name(w->base.name, w->handed_right)
        && player_set_class_by_name(w->base.name, w->char_class)
        && player_set_race_by_name(w->base.name, w->race);
    if (!ok) {
        descriptor_send(d, "Save failed -- the DB rejected part of it.\r\n");
        return;
    }
    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        /* NOT `state == CONN_PLAYING` -- misses a target mid-edit
         * themselves (Session 43 audit), leaving their live copy stale. */
        if (it->character
            && strcasecmp(it->character->base.name, w->base.name) == 0) {
            it->character->progress = w->progress;
            it->character->attrs = w->attrs;
            snprintf(it->character->title, sizeof(it->character->title), "%s", w->title);
            it->character->gender = w->gender;
            it->character->handed_right = w->handed_right;
            it->character->char_class = w->char_class;
            it->character->race = w->race;
            break;
        }
    }
    d->edplayer_dirty = false;
    descriptor_send(d, "Player saved.\r\n");
}

/* Quits the player editor back to CONN_PLAYING, discarding any unsaved
 * working-copy changes, and flushes any held messages that piled up. */
static void edplayer_leave(descriptor_t *d) {
    d->state = CONN_PLAYING;
    d->edplayer_dirty = false;
    descriptor_send(d, "Leaving the player editor.\r\n");
    descriptor_editor_exit_notice(d);
}

/* See descriptor.h. The loaded snapshot's desc/fighting pointers are
 * scrubbed before copying into the working copy -- edplayer_work is never a
 * live connection or combat participant of its own. */
bool descriptor_edplayer_begin(descriptor_t *d, const char *name) {
    int load_room = -1;
    being_t *loaded = player_load_admin(name, &load_room);
    if (!loaded)
        return false;
    d->edplayer_work = *loaded;
    d->edplayer_work.desc = NULL;    /* never a live connection of our own */
    d->edplayer_work.fighting = NULL;
    being_destroy(loaded);
    d->edplayer_load_room = load_room;
    d->edplayer_dirty = false;
    show_edplayer_menu(d);
    return true;
}

/* Renders the CONN_EDZONE_MENU top-level screen for the edzone_work
 * snapshot: name, enabled, lifespan, vnum range, and the assigned-builder
 * list (queried fresh, not part of the working copy -- see the
 * CONN_EDZONE_BUILDER enum comment), plus an "* unsaved changes *" marker. */
static void show_edzone_menu(descriptor_t *d) {
    zone_t *w = &d->edzone_work;
    char owners[8][64];
    int oc = zone_repo_load_owner_names(w->zone_nr, owners, 8);
    char ownerbuf[400] = "none";
    if (oc > 0) {
        int op = 0;
        for (int i = 0; i < oc; i++)
            op += snprintf(ownerbuf + op, sizeof(ownerbuf) - (size_t)op,
                            "%s%s", i ? ", " : "", owners[i]);
    }
    char out[1024];
    snprintf(out, sizeof(out),
             "\r\n<c>Editing zone:<z> %s (#%d)\r\n\r\n"
             "   <c>1)<z> <p>Name<z>: %s\r\n"
             "   <c>2)<z> <p>Enabled<z>: %s              <c>3)<z> <p>Lifespan (minutes)<z>: %d\r\n"
             "   <c>4)<z> <p>Vnum range<z>: %d-%d\r\n"
             "   <c>5)<z> <p>Assigned builders<z>: %s\r\n\r\n"
             "   <c>R)<z> <p>Reset this zone now<z>\r\n"
             "   <c>S)<z> <p>Save<z>    <c>Q)<z> <p>Quit<z>%s\r\n[edit zone] ",
             w->name, w->zone_nr, w->name, w->enabled ? "yes" : "no", w->lifespan,
             w->bottom, w->top, ownerbuf,
             d->edzone_dirty ? "\r\n   <c>* unsaved changes *<z>" : "");
    descriptor_send(d, out);
    d->state = CONN_EDZONE_MENU;
}

/* Commits the edzone working copy's scalar properties back to the DB.
 * Builder assignment isn't included -- it applies immediately, not through
 * this Save (see the CONN_EDZONE_MENU enum comment). */
static void edzone_save(descriptor_t *d) {
    if (!zone_repo_save(&d->edzone_work)) {
        descriptor_send(d, "Save failed -- the DB rejected part of it.\r\n");
        return;
    }
    d->edzone_dirty = false;
    descriptor_send(d, "Zone saved.\r\n");
}

/* Quits the zone editor back to CONN_PLAYING, discarding any unsaved
 * working-copy changes, and flushes any held messages that piled up. */
static void edzone_leave(descriptor_t *d) {
    d->state = CONN_PLAYING;
    d->edzone_dirty = false;
    descriptor_send(d, "Leaving the zone editor.\r\n");
    descriptor_editor_exit_notice(d);
}

/* See descriptor.h. */
bool descriptor_edzone_begin(descriptor_t *d, int zone_nr) {
    zone_t loaded;
    if (!zone_repo_load_one(zone_nr, &loaded))
        return false;
    d->edzone_work = loaded;
    d->edzone_dirty = false;
    show_edzone_menu(d);
    return true;
}

/* What val[0..3] mean for the CURRENT item type -- shown inline next to
 * "Four values" so a builder doesn't have to go dig up obj.h's own doc
 * comment to know what they're actually setting (user 2026-07-25: "should
 * be defined in the editor"). Keyed by category (obj.h's own val[]
 * comment is organized the same way -- most raw types collapse into a
 * handful of categories that share a val[] layout), with `type` itself
 * consulted first for the two cases that DON'T follow category (FUEL
 * collapses into OBJ_CAT_OTHER but has its own real layout; WRITTEN's
 * val[0] only means something for the board sub-type, ITEM_BOARD=24).
 * Component/holy-symbol/drug val[] layouts are identified by KEYWORD, not
 * type/category, so they can't be hinted here -- same disclosed gap. */
static const char *oedit_val_hint(int type) {
    if (type == 6) /* ITEM_FUEL */
        return "fuel remaining/max fuel/-/-";
    if (type == 24) /* ITEM_BOARD */
        return "min read level/-/-/-";
    switch (category_for_item_type(type)) {
        case OBJ_CAT_LIGHT:        return "radius (unused)/max fuel (-1=none)/cur fuel/lit(0-1)";
        case OBJ_CAT_WEAPON:       return "damage dice count/damage dice sides/-/-";
        case OBJ_CAT_ARMOR:        return "armor class/-/-/-";
        case OBJ_CAT_CONTAINER:    return "max weight cap/flags(closed/locked/pickproof)/key vnum/-";
        case OBJ_CAT_DRINK:        return "max units/current units/-/-";
        case OBJ_CAT_FOOD:         return "max units (hunger)/current units/-/-";
        case OBJ_CAT_MONEY:        return "coin amount/-/-/-";
        case OBJ_CAT_KEY:          return "unused -- matched by this obj's own vnum, not val[]";
        case OBJ_CAT_MAGIC_DEVICE: return "charges/uses remaining/-/-/-";
        default:                   return "unused for this type";
    }
}

/* Renders the CONN_OEDIT_MENU top-level screen for the oedit_work snapshot:
 * every editable field of the object prototype, numbered per the real
 * upstream's own field order (see the CONN_OEDIT_MENU enum comment), with
 * oedit_val_hint() annotating what val[0..3] mean for this item's type and
 * an "* unsaved changes *" marker when oedit_dirty. */
static void show_oedit_menu(descriptor_t *d) {
    obj_proto_t *w = &d->oedit_work;
    char wearbuf[256], actionbuf[512], antiracebuf[128];
    obj_wear_flag_names(w->wear_flag, wearbuf, sizeof(wearbuf));
    obj_action_flag_names(w->action_flag, actionbuf, sizeof(actionbuf));
    obj_anti_race_flag_names(w->anti_race_flag, antiracebuf, sizeof(antiracebuf));
    char out[2432];
    snprintf(out, sizeof(out),
             "\r\n<c>Editing object:<z> %s (#%d)\r\n\r\n"
             "   <c>1)<z> <p>Name<z>: %s\r\n"
             "   <c>2)<z> <p>Short description<z>: %s\r\n"
             "   <c>3)<z> <p>Item type<z>: %s (#%d)\r\n"
             "   <c>4)<z> <p>Long description<z>: %s\r\n"
             "   <c>5)<z> <p>Weight<z>: %.1f                     <c>6)<z> <p>Volume<z>: %d\r\n"
             "   <c>7)<z> <p>Extra flags<z>: %s\r\n"
             "   <c>8)<z> <p>Take flags<z>: %s\r\n"
             "   <c>9)<z> <p>Cost/value<z>: %d\r\n"
             "  <c>10)<z> <p>Four values<z>: %d %d %d %d  (%s)\r\n"
             "  <c>11)<z> <p>Decay time<z>: %d                  <c>12)<z> <p>Max struct points<z>: %d\r\n"
             "  <c>13)<z> <p>Struct points<z>: %d                <c>14)<z> <p>Material<z>: %d (%s)\r\n"
             "  <c>15)<z> <p>Can be seen<z>: %s                  <c>16)<z> <p>Special proc<z>: %d\r\n"
             "  <c>17)<z> <p>Max exist<z>: %d                    <c>18)<z> <p>Anti-race flags<z>: %s\r\n\r\n"
             "   <c>S)<z> <p>Save<z>    <c>Q)<z> <p>Quit<z>%s\r\n[oedit] ",
             w->name, w->vnum,
             w->name, w->short_descr, obj_type_name(w->type), w->type,
             w->long_descr, w->weight, w->volume,
             actionbuf, wearbuf, w->price,
             w->val[0], w->val[1], w->val[2], w->val[3], oedit_val_hint(w->type),
             w->decay_time, w->max_struct,
             w->cur_struct, w->material, material_tier_name(material_tier_for_id(w->material)),
             w->can_be_seen ? "yes" : "no", w->spec_proc,
             w->max_exist, antiracebuf,
             d->oedit_dirty ? "\r\n   <c>* unsaved changes *<z>" : "");
    descriptor_send(d, out);
    d->state = CONN_OEDIT_MENU;
}

/* Renders the oedit "Item type" picker (CONN_OEDIT_TYPE) -- a full
 * numbered listing of every raw itemTypeT name (obj_type_name()/
 * obj_item_type_count(), obj.c), two per line, so a builder can browse
 * and pick rather than guess a number blind (Object editor: item-type
 * flag listing/picker, TODO.md priority item, 2026-08-02 -- previously
 * this prompt just took a raw number with no listing at all, the error
 * message even told builders to go check `stat`/`vnum` on some other
 * object first). */
static void show_oedit_type_picker(descriptor_t *d) {
    char out[2560];
    size_t n = (size_t)snprintf(out, sizeof(out),
        "\r\nItem types for %d (currently #%d, %s) -- enter a number, blank to cancel:\r\n",
        d->oedit_work.vnum, d->oedit_work.type, obj_type_name(d->oedit_work.type));
    int count = obj_item_type_count();
    for (int t = 0; t < count; t++) {
        n += (size_t)snprintf(out + n, sizeof(out) > n ? sizeof(out) - n : 0,
            "  <c>%2d<z> %-16s%s", t, obj_type_name(t), (t % 2 == 1) ? "\r\n" : "");
        if (n >= sizeof(out))
            break;
    }
    if (count % 2 != 0 && n < sizeof(out))
        n += (size_t)snprintf(out + n, sizeof(out) - n, "\r\n");
    if (n < sizeof(out))
        snprintf(out + n, sizeof(out) - n, "type> ");
    descriptor_send(d, out);
    d->state = CONN_OEDIT_TYPE;
}

/* Renders the oedit "Extra flags" (obj action flag) toggle submenu
 * (CONN_OEDIT_ACTION_FLAGS), one [x]/[ ] bit per known flag. */
static void show_oedit_action_flags(descriptor_t *d) {
    char out[2048];
    size_t n = (size_t)snprintf(out, sizeof(out),
        "\r\nExtra flags for %d -- toggle by number, blank to return:\r\n",
        d->oedit_work.vnum);
    for (int b = 0; b < obj_action_flag_count(); b++) {
        n += (size_t)snprintf(out + n, sizeof(out) > n ? sizeof(out) - n : 0,
            "  <c>%2d<z> [%c] <p>%-14s<z>%s", b,
            (d->oedit_work.action_flag & (1 << b)) ? 'x' : ' ',
            obj_action_flag_name(b), (b % 2 == 1) ? "\r\n" : "");
        if (n >= sizeof(out))
            break;
    }
    if (obj_action_flag_count() % 2 != 0 && n < sizeof(out))
        snprintf(out + n, sizeof(out) - n, "\r\n");
    descriptor_send(d, out);
    descriptor_send(d, "flag> ");
    d->state = CONN_OEDIT_ACTION_FLAGS;
}

/* Renders the oedit "Take flags" (obj wear flag) toggle submenu
 * (CONN_OEDIT_WEAR_FLAGS), one [x]/[ ] bit per known flag. */
static void show_oedit_wear_flags(descriptor_t *d) {
    char out[1024];
    size_t n = (size_t)snprintf(out, sizeof(out),
        "\r\nTake flags for %d -- toggle by number, blank to return:\r\n",
        d->oedit_work.vnum);
    for (int b = 0; b < obj_wear_flag_count(); b++) {
        n += (size_t)snprintf(out + n, sizeof(out) > n ? sizeof(out) - n : 0,
            "  <c>%2d<z> [%c] <p>%-14s<z>%s", b,
            (d->oedit_work.wear_flag & (1 << b)) ? 'x' : ' ',
            obj_wear_flag_name(b), (b % 2 == 1) ? "\r\n" : "");
        if (n >= sizeof(out))
            break;
    }
    if (obj_wear_flag_count() % 2 != 0 && n < sizeof(out))
        snprintf(out + n, sizeof(out) - n, "\r\n");
    descriptor_send(d, out);
    descriptor_send(d, "flag> ");
    d->state = CONN_OEDIT_WEAR_FLAGS;
}

/* Renders the oedit "Anti-race flags" toggle submenu (CONN_OEDIT_
 * ANTI_RACE_FLAGS, Object anti-race flags, TODO.md priority item,
 * 2026-08-02), one [x]/[ ] bit per known race -- same pattern as
 * show_oedit_wear_flags() just above, own field since this is a
 * Tobin-only bitmask (obj.h) with no upstream column to share. */
static void show_oedit_anti_race_flags(descriptor_t *d) {
    char out[512];
    size_t n = (size_t)snprintf(out, sizeof(out),
        "\r\nAnti-race flags for %d -- toggle by number, blank to return:\r\n",
        d->oedit_work.vnum);
    for (int b = 0; b < obj_anti_race_flag_count(); b++) {
        n += (size_t)snprintf(out + n, sizeof(out) > n ? sizeof(out) - n : 0,
            "  <c>%2d<z> [%c] <p>%-14s<z>%s", b,
            (d->oedit_work.anti_race_flag & (1 << b)) ? 'x' : ' ',
            obj_anti_race_flag_name(b), (b % 2 == 1) ? "\r\n" : "");
        if (n >= sizeof(out))
            break;
    }
    if (obj_anti_race_flag_count() % 2 != 0 && n < sizeof(out))
        snprintf(out + n, sizeof(out) - n, "\r\n");
    descriptor_send(d, out);
    descriptor_send(d, "flag> ");
    d->state = CONN_OEDIT_ANTI_RACE_FLAGS;
}

/* Commits the oedit working copy back to the DB via obj_proto_save(). */
static void oedit_save(descriptor_t *d) {
    if (!obj_proto_save(&d->oedit_work)) {
        descriptor_send(d, "Save failed -- the DB rejected part of it.\r\n");
        return;
    }
    d->oedit_dirty = false;
    descriptor_send(d, "Object saved.\r\n");
}

/* Quits the object editor back to CONN_PLAYING, discarding any unsaved
 * working-copy changes, and flushes any held messages that piled up. */
static void oedit_leave(descriptor_t *d) {
    d->state = CONN_PLAYING;
    d->oedit_dirty = false;
    descriptor_send(d, "Leaving the object editor.\r\n");
    descriptor_editor_exit_notice(d);
}

/* See descriptor.h. */
bool descriptor_oedit_begin(descriptor_t *d, int vnum) {
    obj_proto_t loaded;
    if (!obj_proto_load(vnum, &loaded)) {
        /* No row yet -- create a blank one and open THAT, rather than
         * refusing (2026-07-25, user: "if one doesn't exist a blank one
         * should be created", then "objects and rooms should behave the
         * same" -- matches edroom's own room_create() fallback below). */
        if (!obj_proto_create_blank(vnum) || !obj_proto_load(vnum, &loaded))
            return false;
        descriptor_send(d, "No object existed at that vnum -- created a blank one.\r\n");
    }
    d->oedit_work = loaded;
    d->oedit_dirty = false;
    show_oedit_menu(d);
    return true;
}

/* ---- Menu-driven mob-prototype editor (medit), CONN_MEDIT_* ----------- */

static void show_medit_menu(descriptor_t *d) {
    mob_proto_t *w = &d->medit_work;
    char out[4096];
    snprintf(out, sizeof(out),
             "\r\n<c>Editing mob:<z> %s (#%d)\r\n\r\n"
             "   <c>1)<z> <p>Name<z>: %s\r\n"
             "   <c>2)<z> <p>Short desc<z>: %s\r\n"
             "   <c>3)<z> <p>Long desc<z>: %s\r\n"
             "   <c>4)<z> <p>Description<z>: %s\r\n"
             "   <c>5)<z> <p>Action flags<z>: %d                 <c>6)<z> <p>Affect flags<z>: %d\r\n"
             "   <c>7)<z> <p>Attacks<z>: %.1f                     <c>8)<z> <p>Level<z>: %d\r\n"
             "   <c>9)<z> <p>Hitroll<z>: %d                      <c>10)<z> <p>Armor Level<z>: %.1f\r\n"
             "  <c>11)<z> <p>HP Level<z>: %.1f                    <c>12)<z> <p>Damage<z>: %.1f +%d\r\n"
             "  <c>13)<z> <p>Gold<z>: %d                          <c>14)<z> <p>Race<z>: %d (%s)\r\n"
             "  <c>15)<z> <p>Sex<z>: %s                       <c>16)<z> <p>Max exist<z>: %d\r\n"
             "  <c>17)<z> <p>Default position<z>: %s          <c>18)<z> <p>Class (bitmask)<z>: %d\r\n"
             "  <c>19)<z> <p>Height/Weight<z>: %d/%d               <c>20)<z> <p>Vision<z>: %d\r\n"
             "  <c>21)<z> <p>Can be seen<z>: %s                   <c>22)<z> <p>Skin<z>: %d\r\n"
             "  <c>23)<z> <p>Alignment<z>: %d\r\n\r\n"
             "   <c>S)<z> <p>Save<z>    <c>Q)<z> <p>Quit<z>%s\r\nMob Editor> ",
             w->name, d->medit_vnum,
             w->name, w->short_descr, w->long_descr, w->description,
             w->actions, w->affects,
             w->attacks, w->level,
             w->tohit, w->ac,
             w->hpbonus, w->damage_level, w->damage_precision,
             w->gold, w->race, mob_race_name(w->race),
             gender_name((gender_t)w->sex), w->max_exist,
             position_name((position_t)w->def_position), w->class_mask,
             w->height, w->weight, w->vision,
             w->can_be_seen ? "yes" : "no", w->skin,
             w->align,
             d->medit_dirty ? "\r\n   <c>* unsaved changes *<z>" : "");
    descriptor_send(d, out);
    d->state = CONN_MEDIT_MENU;
}

/* Sets the working copy's 6 real Tobin characteristics from level and
 * class, the same way a live mob gets its attrs at spawn time
 * (being_create_mob()) -- 2026-07-25, user: "Characteristics should be
 * automatically calculated like players", then "according to race and
 * class". Reuses being_create_mob()'s own level-driven base formula and
 * class_stat_bonus() (via the same mob_class_mask_to_tobin() mapping
 * being_create_mob() itself uses) so a saved prototype and a freshly
 * spawned instance of it agree. The race half is a disclosed gap: mob.race
 * is a raw upstream MOB_RACE_NAMES index, not a Tobin player_race_t, and
 * no mapping between the two scales exists yet (being.c's class mapper
 * has no race equivalent) -- only class contributes beyond the level base
 * for now. */
static void medit_apply_characteristics(mob_proto_t *w) {
    int base = ATTR_BASE + w->level;
    if (base > ATTR_MAX)
        base = ATTR_MAX;
    attrs_t a;
    a.strength = a.dexterity = a.constitution = a.intelligence = a.wisdom = a.charisma = base;

    player_class_t cls;
    if (mob_class_mask_to_tobin(w->class_mask, &cls))
        class_stat_bonus(cls, &a);

    w->str = a.strength;
    w->dex = a.dexterity;
    w->con = a.constitution;
    w->intel = a.intelligence;
    w->wis = a.wisdom;
    w->cha = a.charisma;
}

/* Recomputes the working copy's derived Tobin characteristics (str/dex/etc.,
 * via medit_apply_characteristics()) then commits it back to the DB via
 * mob_proto_save(). */
static void medit_save(descriptor_t *d) {
    medit_apply_characteristics(&d->medit_work);
    if (!mob_proto_save(d->medit_vnum, &d->medit_work)) {
        descriptor_send(d, "Save failed -- the DB rejected part of it.\r\n");
        return;
    }
    d->medit_dirty = false;
    descriptor_send(d, "Mob saved.\r\n");
}

/* Quits the mob editor back to CONN_PLAYING, discarding any unsaved
 * working-copy changes, and flushes any held messages that piled up. */
static void medit_leave(descriptor_t *d) {
    d->state = CONN_PLAYING;
    d->medit_dirty = false;
    descriptor_send(d, "Leaving the mob editor.\r\n");
    descriptor_editor_exit_notice(d);
}

/* See descriptor.h. */
bool descriptor_medit_begin(descriptor_t *d, int vnum) {
    mob_proto_t loaded;
    if (!mob_proto_load(vnum, &loaded)) {
        /* No row yet -- create a blank one and open THAT, rather than
         * refusing (2026-07-25, user: "if one doesn't exist a blank one
         * should be created", then "objects and rooms should behave the
         * same"). */
        if (!mob_proto_create_blank(vnum) || !mob_proto_load(vnum, &loaded))
            return false;
        descriptor_send(d, "No mob existed at that vnum -- created a blank one.\r\n");
    }
    d->medit_vnum = vnum;
    d->medit_work = loaded;
    d->medit_dirty = false;
    show_medit_menu(d);
    return true;
}

/* Renders the CONN_BALANCE_MENU top-level screen for the balance_work
 * snapshot (a class or race's balance_mod_t, per balance_is_class): HP/
 * damage multipliers, to-hit/AC modifiers, and an "* unsaved changes *"
 * marker when balance_dirty. */
static void show_balance_menu(descriptor_t *d) {
    const char *kind = d->balance_is_class ? "class" : "race";
    const char *name = d->balance_is_class ? class_name((player_class_t)d->balance_index)
                                            : race_name((player_race_t)d->balance_index);
    balance_mod_t *w = &d->balance_work;
    char out[512];
    snprintf(out, sizeof(out),
             "\r\n<c>Balancing %s:<z> %s\r\n\r\n"
             "   <c>1)<z> <p>HP multiplier<z>: %.2f          <c>2)<z> <p>Damage multiplier<z>: %.2f\r\n"
             "   <c>3)<z> <p>To-hit modifier<z>: %+d         <c>4)<z> <p>AC modifier<z>: %+d\r\n\r\n"
             "   <c>S)<z> <p>Save<z>    <c>Q)<z> <p>Quit<z>%s\r\n[balance %s] ",
             kind, name, (double)w->hp_mult, (double)w->dmg_mult, w->tohit_mod, w->ac_mod,
             d->balance_dirty ? "\r\n   <c>* unsaved changes *<z>" : "", kind);
    descriptor_send(d, out);
    d->state = CONN_BALANCE_MENU;
}

/* Commits the balance working copy to the DB AND the live in-memory cache
 * (class_balance_set()/race_balance_set(), not the raw *_save() functions
 * directly, which would leave the cache stale -- see the CONN_BALANCE_MENU
 * enum comment) so the new numbers apply gamewide immediately. */
static void balance_save(descriptor_t *d) {
    bool ok = d->balance_is_class
        ? class_balance_set((player_class_t)d->balance_index, &d->balance_work)
        : race_balance_set((player_race_t)d->balance_index, &d->balance_work);
    if (!ok) {
        descriptor_send(d, "Save failed -- the DB rejected it.\r\n");
        return;
    }
    d->balance_dirty = false;
    descriptor_send(d, "Balance saved -- applies gamewide immediately.\r\n");
}

/* Quits the balance editor back to CONN_PLAYING, discarding any unsaved
 * working-copy changes, and flushes any held messages that piled up. */
static void balance_leave(descriptor_t *d) {
    d->state = CONN_PLAYING;
    d->balance_dirty = false;
    descriptor_send(d, "Leaving the balance editor.\r\n");
    descriptor_editor_exit_notice(d);
}

/* See descriptor.h. */
bool descriptor_balance_begin(descriptor_t *d, bool is_class, int index) {
    if (index < 0 || (is_class && index >= CLASS_COUNT) || (!is_class && index >= RACE_COUNT))
        return false;
    d->balance_is_class = is_class;
    d->balance_index = index;
    d->balance_work = is_class ? *class_balance_get((player_class_t)index)
                                : *race_balance_get((player_race_t)index);
    d->balance_dirty = false;
    show_balance_menu(d);
    return true;
}

/* No working copy to re-render from -- every edaccount action commits
 * immediately (see the CONN_EDACCOUNT_MENU enum comment), so the menu
 * always re-reads the row fresh. Falls back to CONN_PLAYING if the
 * account vanished out from under the editor (e.g. someone else wiped it
 * concurrently) rather than showing a menu for a row that's gone. */
static void show_edaccount_menu(descriptor_t *d) {
    account_t acct;
    if (!account_load_by_id(d->edaccount_id, &acct)) {
        descriptor_send(d, "That account no longer exists.\r\n");
        d->state = CONN_PLAYING;
        descriptor_editor_exit_notice(d);
        return;
    }

    char names[MAX_CHARS_PER_ACCOUNT][PLAYER_NAME_LEN];
    int levels[MAX_CHARS_PER_ACCOUNT];
    int count = 0;
    player_list_by_account(acct.account_id, names, levels, MAX_CHARS_PER_ACCOUNT, &count);

    char charlist[512];
    size_t n = 0;
    charlist[0] = '\0';
    for (int i = 0; i < count && n < sizeof(charlist); i++)
        n += (size_t)snprintf(charlist + n, sizeof(charlist) - n, "%s%s (%d)",
                              i ? ", " : "", names[i], levels[i]);
    if (count == 0)
        snprintf(charlist, sizeof(charlist), "(none)");

    char out[900];
    snprintf(out, sizeof(out),
             "\r\n<c>Editing account:<z> %s\r\n\r\n"
             "   Characters: %s\r\n\r\n"
             "   <c>1)<z> <p>Rename account<z>          <c>2)<z> <p>Reset password<z>\r\n\r\n"
             "   <c>Q)<z> <p>Quit<z>\r\n[edit account] ",
             acct.name, charlist);
    descriptor_send(d, out);
    d->state = CONN_EDACCOUNT_MENU;
}

/* Quits the account editor back to CONN_PLAYING and flushes any held
 * messages that piled up. Nothing to discard -- every edaccount action
 * commits immediately, so there's no working copy to abandon. */
static void edaccount_leave(descriptor_t *d) {
    d->state = CONN_PLAYING;
    descriptor_send(d, "Leaving the account editor.\r\n");
    descriptor_editor_exit_notice(d);
}

/* See descriptor.h. */
bool descriptor_edaccount_begin(descriptor_t *d, const char *name) {
    account_t acct;
    if (!account_load(name, &acct))
        return false;
    d->edaccount_id = acct.account_id;
    show_edaccount_menu(d);
    return true;
}

/* The 8 upstream message fields (social_repo.h), in display/menu order.
 * Longest label is "Others (target found)" (22 chars) -- every %-22s below
 * lines up against that, so the message text starts in the same column on
 * every row regardless of which field it is. */
#define EDSOCIAL_FIELD_COUNT 8
static const char *const EDSOCIAL_FIELD_LABELS[EDSOCIAL_FIELD_COUNT] = {
    "Self (no target)", "Others (no target)",
    "Self (target found)", "Others (target found)", "Target (found)",
    "Not found",
    "Self (self-target)", "Others (self-target)",
};

/* Maps a numbered field (1-8, matching EDSOCIAL_FIELD_LABELS) to a pointer
 * to its message buffer inside `s`, so the generic CONN_EDSOCIAL_FIELD
 * single-line prompt can read/overwrite whichever field is being edited
 * without a field-by-field switch at every call site. NULL for an
 * out-of-range field. */
static char *edsocial_field_ptr(social_t *s, int field) {
    switch (field) {
        case 1: return s->self_no_arg;
        case 2: return s->others_no_arg;
        case 3: return s->self_found;
        case 4: return s->others_found;
        case 5: return s->vict_found;
        case 6: return s->not_found;
        case 7: return s->self_auto;
        case 8: return s->others_auto;
        default: return NULL;
    }
}

/* Full social list -- unpaged, same precedent as show_redit_extra_menu()'s
 * small list (a builder tool, not the paginated player-facing `socials`
 * command, cmd_socials.c). Same 4-column %-14s layout cmd_socials.c uses,
 * so names line up into even columns. */
static void show_edsocial_list(descriptor_t *d) {
    social_cache_load(); /* fresh, in case another builder just edited concurrently */
    int cnt = social_cache_count();

    char out[8192];
    size_t n = (size_t)snprintf(out, sizeof(out), "\r\n<c>=== Socials (%d) ===<z>\r\n", cnt);
    for (int i = 0; i < cnt && n < sizeof(out); i++) {
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-14s", social_cache_at(i)->name);
        if (i % 4 == 3)
            n += (size_t)snprintf(out + n, sizeof(out) > n ? sizeof(out) - n : 0, "\r\n");
    }
    if (cnt % 4 != 0 && n < sizeof(out))
        n += (size_t)snprintf(out + n, sizeof(out) - n, "\r\n");
    if (n < sizeof(out))
        snprintf(out + n, sizeof(out) - n,
            "\r\nType a name to edit it, <y>new<z> to add one, or blank to quit.\r\nedsocial> ");
    descriptor_send(d, out);
    d->state = CONN_EDSOCIAL_LIST;
}

/* One social's detail view. No working copy -- re-reads fresh every time
 * (every edit commits immediately, see the CONN_EDSOCIAL_* enum comment),
 * so a concurrent edit by a second builder is always reflected, same
 * tradeoff show_edaccount_menu()/show_redit_extra_item() already accept.
 * The top two lines share one label width (19, "Minimum position:" is the
 * longer), and the 8 numbered fields share another (22) -- see
 * EDSOCIAL_FIELD_LABELS' comment -- so values consistently start in the
 * same column top to bottom. */
static void show_edsocial_item(descriptor_t *d) {
    social_t s;
    if (!social_repo_get(d->edsocial_name, &s)) {
        descriptor_send(d, "That social no longer exists.\r\n");
        show_edsocial_list(d);
        return;
    }

    char out[4096];
    size_t n = (size_t)snprintf(out, sizeof(out),
        "\r\n<c>Editing social: %s<z>\r\n\r\n"
        "  %-19s%s\r\n"
        "  %-19s%s\r\n\r\n",
        s.name,
        "Hide unseen actor:", s.hide ? "yes" : "no",
        "Minimum position:", position_name((position_t)s.min_position));

    for (int i = 0; i < EDSOCIAL_FIELD_COUNT && n < sizeof(out); i++) {
        const char *val = edsocial_field_ptr(&s, i + 1);
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  <c>%d)<z> <p>%-22s<z> %s\r\n",
                              i + 1, EDSOCIAL_FIELD_LABELS[i], val[0] ? val : "(empty)");
    }

    /* Field width bumped 22 -> 34 to absorb the 12 invisible tag bytes
     * ("<c></z>" x2 + "<p></z>" x2) each label below now carries, so the
     * VISIBLE column width (and second-column start) stays the same as
     * before colorizing -- %-Ns pads on byte length, which doesn't know
     * a <X> tag renders to zero screen columns. */
    if (n < sizeof(out))
        n += (size_t)snprintf(out + n, sizeof(out) - n,
            "\r\n  %-34s%-34s\r\n  %-34s%-34s\r\n",
            "<c>H)<z> <p>Toggle hide-unseen<z>", "<c>P)<z> <p>Set minimum position<z>",
            "<c>R)<z> <p>Rename<z>", "<c>D)<z> <p>Delete this social<z>");
    if (n < sizeof(out))
        snprintf(out + n, sizeof(out) - n,
            "\r\n  blank) back to list\r\nedsocial-%s> ", s.name);
    descriptor_send(d, out);
    d->state = CONN_EDSOCIAL_ITEM;
}

/* Quits the social editor back to CONN_PLAYING and flushes any held
 * messages that piled up. Nothing to discard -- every edsocial action
 * commits immediately, so there's no working copy to abandon. */
static void edsocial_leave(descriptor_t *d) {
    d->state = CONN_PLAYING;
    descriptor_send(d, "Leaving the social editor.\r\n");
    descriptor_editor_exit_notice(d);
}

/* See descriptor.h. */
void descriptor_edsocial_begin(descriptor_t *d, const char *name) {
    social_t s;
    if (name && name[0] && social_repo_get(name, &s)) {
        snprintf(d->edsocial_name, sizeof(d->edsocial_name), "%s", s.name);
        show_edsocial_item(d);
    } else {
        show_edsocial_list(d);
    }
}

/* ---- Menu-driven trigger manager (edit trigger), CONN_TRIGEDIT_* ------ *
 * See descriptor.h's CONN_TRIGEDIT_* enum comment for the overall shape. */

static const char *trigedit_valid_types(const char *target_type) {
    if (strcasecmp(target_type, "room") == 0)
        return "enter, random";
    if (strcasecmp(target_type, "mob") == 0)
        return "greet, speech, death, random";
    return "get, wear"; /* obj */
}

/* True if `trigger_type` is one of the types actually usable on
 * `target_type` (room/mob/obj) -- the set trigedit_valid_types() above
 * displays as a hint. Gates CONN_TRIGEDIT_NEW_TYPE's typed input. */
static bool trigedit_type_valid(const char *target_type, const char *trigger_type) {
    if (strcasecmp(target_type, "room") == 0)
        return strcasecmp(trigger_type, "enter") == 0 || strcasecmp(trigger_type, "random") == 0;
    if (strcasecmp(target_type, "mob") == 0)
        return strcasecmp(trigger_type, "greet") == 0 || strcasecmp(trigger_type, "speech") == 0
              || strcasecmp(trigger_type, "death") == 0 || strcasecmp(trigger_type, "random") == 0;
    return strcasecmp(trigger_type, "get") == 0 || strcasecmp(trigger_type, "wear") == 0;
}

/* Renders the CONN_TRIGEDIT_LIST screen: every trigger already attached to
 * trigedit_target_type/vnum, numbered, with its match text or (for
 * "random") its chance percent shown inline. */
static void show_trigedit_list(descriptor_t *d) {
    trigger_t trigs[32];
    int n = trigger_repo_list_for(d->trigedit_target_type, d->trigedit_target_vnum, trigs, 32);

    char out[3072];
    size_t len = (size_t)snprintf(out, sizeof(out),
        "\r\n<c>=== Triggers on %s %d ===<z>\r\n",
        d->trigedit_target_type, d->trigedit_target_vnum);
    if (n == 0 && len < sizeof(out))
        len += (size_t)snprintf(out + len, sizeof(out) - len, "  (none yet)\r\n");
    for (int i = 0; i < n && len < sizeof(out); i++) {
        char suffix[TRIGGER_MATCH_LEN + 24] = "";
        if (trigs[i].match_text[0])
            snprintf(suffix, sizeof(suffix), " match=\"%s\"", trigs[i].match_text);
        else if (strcasecmp(trigs[i].trigger_type, "random") == 0)
            snprintf(suffix, sizeof(suffix), " chance=%d%%", trigs[i].chance_pct);
        len += (size_t)snprintf(out + len, sizeof(out) - len,
            "  <c>%2d)<z> <p>%-10s<z>%s\r\n", i + 1, trigs[i].trigger_type, suffix);
    }
    if (len < sizeof(out))
        snprintf(out + len, sizeof(out) - len,
            "\r\n  <c>A)<z> <p>Add a new trigger<z>    blank) quit\r\ntrigedit> ");
    descriptor_send(d, out);
    d->state = CONN_TRIGEDIT_LIST;
}

/* Renders one trigger's detail view (CONN_TRIGEDIT_ITEM), re-read fresh
 * from the DB by id (d->trig_edit_id) every time -- no working copy, since
 * every field here commits immediately (see the CONN_TRIGEDIT_* enum
 * comment). Falls back to the list if the trigger vanished underneath it.
 * Shows the script body inline (user 2026-07-26: "need a chance to read
 * the trigger") so a builder can read what's already there without having
 * to open the script editor (option 3) first. Addressed by list position/
 * vnum only in everything sent to the player -- the raw db id (d->
 * trig_edit_id) stays purely an internal lookup key (user: "forget the
 * use of id, use only vnums"). */
static void show_trigedit_item(descriptor_t *d) {
    trigger_t t;
    if (!trigger_repo_get(d->trig_edit_id, &t)) {
        descriptor_send(d, "That trigger no longer exists.\r\n");
        show_trigedit_list(d);
        return;
    }
    char out[TRIGGER_MATCH_LEN + TRIGGER_SCRIPT_MAX + 384];
    snprintf(out, sizeof(out),
        "\r\n<c>Editing %s trigger:<z> %s %d\r\n\r\n"
        "   <c>1)<z> <p>Match text/keyword<z>: %s\r\n"
        "   <c>2)<z> <p>Chance percent<z>:     %d\r\n"
        "   <c>3)<z> <p>Edit script<z>\r\n"
        "   <c>D)<z> <p>Delete this trigger<z>\r\n\r\n"
        "<c>Current script:<z>\r\n%s\r\n"
        "   blank) back to list\r\ntrigedit-item> ",
        t.trigger_type, t.target_type, t.target_vnum,
        t.match_text[0] ? t.match_text : "(none)",
        t.chance_pct,
        t.script[0] ? t.script : "  (empty)\r\n");
    descriptor_send(d, out);
    d->state = CONN_TRIGEDIT_ITEM;
}

/* Arms the shared line editor for a trigger's script body -- either a
 * brand-new trigger (d->trig_edit_id == 0, `existing` NULL) or re-editing
 * an existing one's script (d->trig_edit_id > 0, `existing` its current
 * script text, shown below the banner same as cmd_hedit.c's own existing-
 * topic convention). Caller has already populated d->trig_target_type/
 * vnum/trigger_type/match_text/chance_pct. */
static void trigedit_arm_script_editor(descriptor_t *d, const char *existing) {
    if (existing) {
        snprintf(d->edit_buf, sizeof(d->edit_buf), "%s", existing);
        d->edit_len = (int)strlen(d->edit_buf);
    } else {
        d->edit_buf[0] = '\0';
        d->edit_len = 0;
    }
    d->edit_kind = EDIT_TRIGGER;

    char head[896];
    snprintf(head, sizeof(head),
        "\r\n-- Writing trigger: %s %d %s%s%s%s --\r\n"
        "Type the script, one command per line. Actions: echo/echoroom/emote/say/"
        "teleport/give/damage/log/wait. Variables: set/unset/eval/global, %%self%%/"
        "%%actor%%/%%arg%%/%%time%%/%%random.N%%. Control flow: if <expr>/elseif/else/"
        "end, while <expr>/done, switch <val>/case/default/done, break. Expr operators: "
        "== != < > <= >= && || !.\r\n"
        "/s saves, /a aborts, /b blanks, /f reflows to width.\r\n",
        d->trig_target_type, d->trig_target_vnum, d->trig_trigger_type,
        d->trig_match_text[0] ? " keyword=" : "", d->trig_match_text,
        existing ? " (existing shown below)" : "");
    descriptor_send(d, head);
    if (existing && existing[0]) {
        descriptor_send(d, existing);
        if (existing[strlen(existing) - 1] != '\n')
            descriptor_send(d, "\r\n");
    }
    descriptor_send(d, "] ");
    d->state = CONN_TRIGEDIT_SCRIPT;
}

/* See descriptor.h. */
void descriptor_trigedit_begin(descriptor_t *d, const char *target_type, int target_vnum) {
    snprintf(d->trigedit_target_type, sizeof(d->trigedit_target_type), "%s", target_type);
    d->trigedit_target_vnum = target_vnum;
    show_trigedit_list(d);
}

/* Renders the CONN_EDSUIT_LIST screen: the suit's own scalar fields plus
 * every suit_item row, numbered, each showing the item's real short
 * description (not just its bare vnum) and its quantity -- same
 * "numbered list + lettered commands" shape as show_trigedit_list()
 * (menu-driven loadsuit editor, TODO.md priority item, 2026-08-02). */
static void show_edsuit_list(descriptor_t *d) {
    char name[32], description[128];
    int class_restrict, race_restrict;
    if (!suit_repo_get(d->edsuit_id, name, sizeof(name), &class_restrict, &race_restrict, description, sizeof(description))) {
        descriptor_send(d, "That suit no longer exists.\r\n");
        d->state = CONN_PLAYING;
        return;
    }

    int vnums[SUIT_MAX_ITEMS], qtys[SUIT_MAX_ITEMS];
    int n = suit_repo_load_items_qty(d->edsuit_id, vnums, qtys, SUIT_MAX_ITEMS);

    char out[3072];
    size_t len = (size_t)snprintf(out, sizeof(out),
        "\r\n<c>=== Suit:<z> %s <c>===<z>\r\n"
        "  <p>Class<z>: %s\r\n  <p>Race<z>: %s\r\n  <p>Description<z>: %s\r\n\r\n",
        name, class_restrict < 0 ? "any" : class_name((player_class_t)class_restrict),
        race_restrict < 0 ? "any" : race_name((player_race_t)race_restrict), description);
    if (n == 0 && len < sizeof(out))
        len += (size_t)snprintf(out + len, sizeof(out) - len, "  (no items yet)\r\n");
    for (int i = 0; i < n && len < sizeof(out); i++) {
        obj_proto_t proto;
        const char *label = obj_proto_load(vnums[i], &proto) ? proto.short_descr : "(unknown vnum)";
        len += (size_t)snprintf(out + len, sizeof(out) - len,
            "  <c>%2d)<z> <p>%-24s<z> vnum %-6d x%d\r\n", i + 1, label, vnums[i], qtys[i]);
    }
    if (len < sizeof(out))
        snprintf(out + len, sizeof(out) - len,
            "\r\n  <c>A)<z> <p>Add an item<z>    <c>C)<z> <p>Set class restriction<z>\r\n"
            "  <c>R)<z> <p>Set race restriction<z>  <c>D)<z> <p>Set description<z>\r\n"
            "  <c>X)<z> <p>Delete this suit<z>\r\n"
            "  blank) quit\r\nedsuit> ");
    descriptor_send(d, out);
    d->state = CONN_EDSUIT_LIST;
}

/* Renders one suit_item's detail view (CONN_EDSUIT_ITEM), addressed by
 * obj_vnum (d->edsuit_item_vnum) rather than a raw db row id -- same "no
 * raw ids in the UI" spirit as show_trigedit_item(). Falls back to the
 * list if the item vanished underneath it (deleted from another
 * connection, an edge case but cheap to guard). */
static void show_edsuit_item(descriptor_t *d) {
    int vnums[SUIT_MAX_ITEMS], qtys[SUIT_MAX_ITEMS];
    int n = suit_repo_load_items_qty(d->edsuit_id, vnums, qtys, SUIT_MAX_ITEMS);
    int qty = -1;
    for (int i = 0; i < n; i++)
        if (vnums[i] == d->edsuit_item_vnum)
            qty = qtys[i];
    if (qty < 0) {
        descriptor_send(d, "That item is no longer in the suit.\r\n");
        show_edsuit_list(d);
        return;
    }

    obj_proto_t proto;
    const char *label = obj_proto_load(d->edsuit_item_vnum, &proto) ? proto.short_descr : "(unknown vnum)";

    char out[384];
    snprintf(out, sizeof(out),
        "\r\n<c>Editing suit item:<z> %s (vnum %d)\r\n\r\n"
        "   <c>1)<z> <p>Quantity<z>: %d\r\n"
        "   <c>D)<z> <p>Delete this item<z>\r\n\r\n"
        "   blank) back to list\r\nedsuit-item> ",
        label, d->edsuit_item_vnum, qty);
    descriptor_send(d, out);
    d->state = CONN_EDSUIT_ITEM;
}

/* See descriptor.h. */
void descriptor_edsuit_begin(descriptor_t *d, int suit_id) {
    d->edsuit_id = suit_id;
    show_edsuit_list(d);
}

/* The top-level input dispatcher: called by drain_lines() once per complete
 * typed line. Refreshes last_active, intercepts the localhost-only
 * "@test ..." smoke-test announce hook regardless of state, then switches
 * on d->state to route the line through the right stage of the login/
 * character-creation state machine, the right menu-driven editor's CONN_*
 * range, or (CONN_PLAYING) the shared line editor / pager / ordinary
 * command dispatch. Returns false if handling this line means the
 * connection should be torn down. */
static bool handle_line(descriptor_t *d, const char *line) {
    d->last_active = (long)time(NULL); /* any input clears the (idle) flag */

    /* Smoke-test announce hook: a loopback connection may emit a [TEST] log
     * line at ANY connection state (before login, mid-menu, or playing) so
     * an immortal watching the game -- and the day's log file -- can see
     * which smoke test is currently running. Test-harness only: gated to
     * localhost so a remote player can never inject log lines, and the
     * "@test " prefix collides with nothing a real client sends (account
     * names can't contain a space). Every smoke test announces itself at
     * startup AND at finish (see the announce()/announce_done() helpers in
     * the test scripts) -- "@test done <name>" logs a "finished" line
     * instead of "running" so the two are easy to tell apart in the log. */
    if (strncmp(line, "@test ", 6) == 0
        && (strcmp(d->ip, "127.0.0.1") == 0 || strcmp(d->ip, "::1") == 0)) {
        const char *arg = line + 6;
        if (strncmp(arg, "done ", 5) == 0) {
            game_log(LOG_TEST, "finished %s", arg + 5);
            log_test_clear_running();
        } else {
            game_log(LOG_TEST, "running %s", arg);
            log_test_set_running(arg);
        }
        descriptor_send(d, "ok\r\n");
        return true;
    }

    switch (d->state) {
        case CONN_GET_ACCOUNT_NAME: {
            if (!line[0]) {
                descriptor_send(d, "Account name: ");
                return true;
            }
            snprintf(d->account_name, sizeof(d->account_name), "%s", line);
            if (account_load(d->account_name, &d->account)) {
                descriptor_send(d, "Password: ");
                d->state = CONN_GET_PASSWORD;
            } else {
                char msg[160];
                snprintf(msg, sizeof(msg),
                    "New account. Are you sure you want to create the account "
                    "%s? (y/n): ", d->account_name);
                descriptor_send(d, msg);
                d->state = CONN_CONFIRM_NEW_ACCOUNT;
            }
            return true;
        }

        /* Guards against a mistyped existing account name silently starting
         * a brand new account instead (user request, 2026-07-10): confirm
         * before falling into password-creation. "y" proceeds; "n" (or
         * anything else) sends them back to re-enter the account name. */
        case CONN_CONFIRM_NEW_ACCOUNT: {
            bool is_yes = strcasecmp(line, "y") == 0 || strcasecmp(line, "yes") == 0;
            if (is_yes) {
                descriptor_send(d, "Choose a password (3+ characters): ");
                d->state = CONN_GET_NEW_PASSWORD;
                return true;
            }
            descriptor_send(d, "Account name: ");
            d->state = CONN_GET_ACCOUNT_NAME;
            return true;
        }

        case CONN_GET_PASSWORD: {
            if (!account_verify_password(&d->account, line)) {
                descriptor_send(d, "Incorrect password.\r\nAccount name: ");
                d->state = CONN_GET_ACCOUNT_NAME;
                return true;
            }
            /* Apply this account's saved color preference to the connection. */
            d->color_enabled = d->account.color_pref;
            d->state = CONN_ACCOUNT_MENU;
            show_account_menu(d);
            return true;
        }

        case CONN_GET_NEW_PASSWORD: {
            if (strlen(line) < 3) {
                descriptor_send(d, "Too short -- choose a password (3+ characters): ");
                return true;
            }
            /* Confirmation step (user request; the original has one):
             * stash the first entry, ask again, create only on a match. */
            snprintf(d->new_password, sizeof(d->new_password), "%s", line);
            descriptor_send(d, "Retype the password to confirm: ");
            d->state = CONN_CONFIRM_PASSWORD;
            return true;
        }

        case CONN_CONFIRM_PASSWORD: {
            if (strcmp(line, d->new_password) != 0) {
                memset(d->new_password, 0, sizeof(d->new_password));
                descriptor_send(d, "The passwords do not match. Choose a password (3+ characters): ");
                d->state = CONN_GET_NEW_PASSWORD;
                return true;
            }
            bool created = account_create(d->account_name, d->new_password, &d->account);
            memset(d->new_password, 0, sizeof(d->new_password)); /* don't keep plaintext */
            if (!created) {
                descriptor_send(d, "Could not create that account.\r\nAccount name: ");
                d->state = CONN_GET_ACCOUNT_NAME;
                return true;
            }
            /* Ask the color preference once, right after account creation
             * (user spec) -- persisted so it sticks across logins. */
            descriptor_send(d,
                "\r\nThis MUD supports ANSI color. Enable it? (Y/n): ");
            d->state = CONN_GET_COLOR_PREF;
            return true;
        }

        case CONN_GET_COLOR_PREF: {
            bool is_no  = strcasecmp(line, "n") == 0 || strcasecmp(line, "no") == 0;
            bool is_yes = strcasecmp(line, "y") == 0 || strcasecmp(line, "yes") == 0
                          || line[0] == '\0'; /* blank = accept the (Y) default */

            if (is_no || is_yes) {
                bool color_on = !is_no;
                d->color_enabled = color_on;
                d->account.color_pref = color_on;
                account_set_color(d->account.account_id, color_on);
                descriptor_send(d, color_on
                    ? "Color enabled. You can change it later with `color off`.\r\n"
                    : "Color disabled. You can change it later with `color on`.\r\n");
                descriptor_send(d,
                    "\r\nTobinMUD's server clock runs on Eastern time. Enter the "
                    "difference between your time zone and Eastern (e.g. Pacific "
                    "is 3 hours behind, so enter -3; Central is -1, Mountain is "
                    "-2), or press Enter for none: ");
                d->state = CONN_GET_TIMEZONE;
                return true;
            }

            /* Not a yes/no answer -- keep the default (color ON) and treat the
             * input as an account-menu command instead. This keeps existing
             * clients/scripts that don't expect the prompt working: they land
             * on the menu and their next word (e.g. "new") still acts there. */
            d->color_enabled = true;
            d->account.color_pref = true;
            account_set_color(d->account.account_id, true);
            d->state = CONN_ACCOUNT_MENU;
            return handle_line(d, line); /* re-dispatch on the menu */
        }

        case CONN_GET_TIMEZONE: {
            if (line[0] == '\0') {
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }

            char *endptr = NULL;
            long hours = strtol(line, &endptr, 10);
            if (endptr == line || *endptr != '\0' || hours < -23 || hours > 23) {
                descriptor_send(d,
                    "That doesn't look like a whole number of hours (-23 to 23). "
                    "Try again, or press Enter for none: ");
                return true;
            }

            d->account.time_adjust = (int)hours;
            account_set_timezone(d->account.account_id, (int)hours);
            descriptor_send(d,
                "Time zone set. You can change it later with `time <difference>`.\r\n");
            d->state = CONN_ACCOUNT_MENU;
            show_account_menu(d);
            return true;
        }

        case CONN_ACCOUNT_MENU: {
            if (!line[0]) {
                show_account_menu(d);
                return true;
            }

            /* Lettered menu commands (user spec, Session 21): C connect,
             * N new, D delete, Q quit -- case-insensitive; the menu is the
             * ONE place a bare q may quit. The pre-letter inputs (bare
             * number, "new", "delete <name>", "quit!") all keep working. */
            char letter = (char)tolower((unsigned char)line[0]);
            bool single = (line[1] == '\0');

            if (strcasecmp(line, "new") == 0 || (single && letter == 'n')) {
                descriptor_send(d, "New character name (or 'quit!' to cancel): ");
                d->state = CONN_CHAR_CREATE_NAME;
                return true;
            }

            if (strcasecmp(line, "quit!") == 0 || (single && letter == 'q')) {
                descriptor_send(d, "Goodbye!\r\n");
                return false; /* actually disconnect -- unlike `quit!` while playing */
            }

            if (letter == 'c' && (single || line[1] == ' ')) {
                const char *arg = line + 1;
                while (*arg == ' ')
                    arg++;
                const char *pick = NULL;
                if (!*arg) {
                    if (d->char_count == 1) {
                        pick = d->char_list[0];
                    } else if (d->char_count == 0) {
                        descriptor_send(d, "No characters yet -- N creates one.\r\n");
                        show_account_menu(d);
                        return true;
                    } else {
                        d->char_list_shown = true; /* bare C reveals the list (user spec) */
                        show_account_menu(d);
                        return true;
                    }
                } else {
                    char *cend = NULL;
                    long cnum = strtol(arg, &cend, 10);
                    if (cend != arg && *cend == '\0' && cnum >= 1 && cnum <= d->char_count) {
                        pick = d->char_list[cnum - 1];
                    } else {
                        for (int i = 0; i < d->char_count; i++) {
                            if (strcasecmp(d->char_list[i], arg) == 0) {
                                pick = d->char_list[i];
                                break;
                            }
                        }
                    }
                }
                if (!pick) {
                    descriptor_send(d, "No such character. C <number or name>\r\n");
                    show_account_menu(d);
                    return true;
                }
                being_t *b = player_load(pick, d->account.account_id);
                if (!b) {
                    descriptor_send(d, "That character could not be loaded.\r\n");
                    show_account_menu(d);
                    return true;
                }
                enter_world(d, b);
                return true;
            }

            if ((strncasecmp(line, "delete", 6) == 0 && (line[6] == '\0' || line[6] == ' '))
                || (letter == 'd' && (single || line[1] == ' '))) {
                const char *target = line + (tolower((unsigned char)line[0]) == 'd'
                                             && strncasecmp(line, "delete", 6) != 0 ? 1 : 6);
                while (*target == ' ')
                    target++;
                /* User, 2026-07-26: "when deleting a character, the player
                 * should be presented a list of his characters so he could
                 * choose properly" -- bare D/delete (no target) reveals the
                 * numbered list (same char_list_shown mechanism `C` already
                 * uses), same "show what you can pick" spirit as connect. */
                if (!*target) {
                    if (d->char_count == 0) {
                        descriptor_send(d, "No characters yet to delete.\r\n");
                        show_account_menu(d);
                        return true;
                    }
                    show_char_list_box(d);
                    descriptor_send(d, "Delete which number or name (or 'quit!' to cancel)? ");
                    d->state = CONN_CHAR_DELETE_PICK;
                    return true;
                }
                const char *pick = NULL;
                char *tend = NULL;
                long tnum = strtol(target, &tend, 10);
                if (tend != target && *tend == '\0' && tnum >= 1 && tnum <= d->char_count) {
                    pick = d->char_list[tnum - 1];
                } else {
                    for (int i = 0; i < d->char_count; i++) {
                        if (strcasecmp(d->char_list[i], target) == 0) { pick = d->char_list[i]; break; }
                    }
                }
                if (!pick) {
                    descriptor_send(d, "No character by that name or number on this account.\r\n");
                    show_account_menu(d);
                    return true;
                }
                snprintf(d->delete_char_name, sizeof(d->delete_char_name), "%s", pick);
                char confirm[192];
                snprintf(confirm, sizeof(confirm),
                         "\r\nReally delete '%s'? This cannot be undone.\r\n"
                         "Type YES (all caps) to confirm, or anything else to cancel: ",
                         d->delete_char_name);
                descriptor_send(d, confirm);
                d->state = CONN_CHAR_DELETE_CONFIRM;
                return true;
            }

            if (strcasecmp(line, "delete account") == 0 || (single && letter == 'x')) {
                char confirm[256];
                snprintf(confirm, sizeof(confirm),
                         "\r\nReally delete your ENTIRE ACCOUNT '%s' and all %d "
                         "character(s) on it? This cannot be undone.\r\n"
                         "Type YES (all caps) to confirm, or anything else to cancel: ",
                         d->account.name, d->char_count);
                descriptor_send(d, confirm);
                d->state = CONN_ACCOUNT_DELETE_CONFIRM;
                return true;
            }

            char *end = NULL;
            long choice = strtol(line, &end, 10);
            if (end != line && *end == '\0' && choice >= 1 && choice <= d->char_count) {
                being_t *b = player_load(d->char_list[choice - 1], d->account.account_id);
                if (!b) {
                    descriptor_send(d, "That character could not be loaded.\r\n");
                    show_account_menu(d);
                    return true;
                }
                enter_world(d, b);
                return true;
            }

            descriptor_send(d, "Huh? C connects, N creates, D <name> deletes, X deletes the account, Q quits.\r\n");
            show_account_menu(d);
            return true;
        }

        case CONN_CHAR_CREATE_NAME: {
            if (strcasecmp(line, "quit!") == 0) {
                descriptor_send(d, "Character creation cancelled.\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }
            if (!line[0]) {
                descriptor_send(d, "New character name (or 'quit!' to cancel): ");
                return true;
            }
            if (d->char_count >= MAX_CHARS_PER_ACCOUNT) {
                descriptor_send(d, "This account already has the maximum number of characters.\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }
            /* Same rule as the original's _parse_name_safe() (misc/parse.cc):
             * 3-15 characters, letters only. (Its illegal-name/mob-name
             * blocklists are not ported -- no such lists exist in Tobin.) */
            {
                size_t name_len = strlen(line);
                bool has_bad_char = false;
                for (size_t i = 0; i < name_len; i++) {
                    if (!isalpha((unsigned char)line[i])) {
                        has_bad_char = true;
                        break;
                    }
                }
                if (name_len < 3) {
                    descriptor_send(d,
                        "That name is too short -- names must be at least 3 letters long.\r\n"
                        "New character name (or 'quit!' to cancel): ");
                    return true;
                }
                if (name_len > 15) {
                    descriptor_send(d,
                        "That name is too long -- names must be no more than 15 letters long.\r\n"
                        "New character name (or 'quit!' to cancel): ");
                    return true;
                }
                if (has_bad_char) {
                    descriptor_send(d,
                        "Names may only contain letters -- no numbers, spaces, or symbols.\r\n"
                        "New character name (or 'quit!' to cancel): ");
                    return true;
                }
                /* Duplicate names are not allowed anywhere in the world
                 * (user spec) -- checked across ALL accounts, before the
                 * point-buy screen rather than as a failed insert later. */
                if (player_name_exists(line)) {
                    descriptor_send(d,
                        "That name is already taken.\r\n"
                        "New character name (or 'quit!' to cancel): ");
                    return true;
                }
            }
            snprintf(d->new_char_name, sizeof(d->new_char_name), "%s", line);
            being_normalize_name(d->new_char_name);
            d->new_char_attrs = (attrs_t){ ATTR_BASE, ATTR_BASE, ATTR_BASE, ATTR_BASE, ATTR_BASE, ATTR_BASE };
            d->new_char_handed = 1; /* right unless chosen otherwise */
            d->new_char_gender = GENDER_NEUTER; /* neuter unless chosen otherwise */
            d->new_char_appearance[0] = '\0';   /* no appearance unless set */
            d->new_char_race = RACE_HUMAN;      /* placeholder until CONN_CHAR_CREATE_RACE */
            d->new_char_territory = TERRITORY_NONE; /* placeholder until CONN_CHAR_CREATE_TERRITORY */
            d->new_char_class = CLASS_MAGE;     /* placeholder until CONN_CHAR_CREATE_CLASS */
            d->new_char_alignment = 0;          /* placeholder until CONN_CHAR_CREATE_ALIGNMENT */
            d->state = CONN_CHAR_CREATE_RACE;
            show_race_screen(d);
            return true;
        }

        case CONN_CHAR_CREATE_RACE: {
            if (strcasecmp(line, "quit!") == 0) {
                descriptor_send(d, "Character creation cancelled.\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }
            int choice = 0;
            if (sscanf(line, "%d", &choice) != 1 || choice < 1 || choice > RACE_COUNT) {
                descriptor_send(d, "Enter a number from 1 to 6, or 'quit!'.\r\n");
                show_race_screen(d);
                return true;
            }
            /* Bonus applied later, once attrs are point-bought (see the
             * CONN_CHAR_CREATE_ATTRS "done" handler) -- picking race here
             * only records the choice. */
            d->new_char_race = (player_race_t)(choice - 1);
            d->state = CONN_CHAR_CREATE_TERRITORY;
            show_territory_screen(d);
            return true;
        }

        case CONN_CHAR_CREATE_TERRITORY: {
            if (strcasecmp(line, "quit!") == 0) {
                descriptor_send(d, "Character creation cancelled.\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }
            int choice = 0;
            if (sscanf(line, "%d", &choice) != 1 || choice < 1 || choice > TERRITORY_COUNT) {
                descriptor_send(d, "Enter a number from 1 to 3, or 'quit!'.\r\n");
                show_territory_screen(d);
                return true;
            }
            /* Bonus applied later too, alongside race's/class's (see the
             * CONN_CHAR_CREATE_ATTRS "done" handler). */
            d->new_char_territory = (player_territory_t)(choice - 1);
            d->state = CONN_CHAR_CREATE_CLASS;
            show_class_screen(d);
            return true;
        }

        case CONN_CHAR_CREATE_CLASS: {
            if (strcasecmp(line, "quit!") == 0) {
                descriptor_send(d, "Character creation cancelled.\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }
            int choice = 0;
            if (sscanf(line, "%d", &choice) != 1 || choice < 1 || choice > CLASS_COUNT) {
                descriptor_send(d, "Enter a number from 1 to 6, or 'quit!'.\r\n");
                show_class_screen(d);
                return true;
            }
            /* Bonus applied later too, alongside race's (see the
             * CONN_CHAR_CREATE_ATTRS "done" handler). */
            d->new_char_class = (player_class_t)(choice - 1);
            d->state = CONN_CHAR_CREATE_ATTRS;
            show_attr_screen(d);
            return true;
        }

        case CONN_CHAR_CREATE_ATTRS: {
            if (strcasecmp(line, "quit!") == 0 || strcasecmp(line, "quit") == 0
                || strcasecmp(line, "abort") == 0 || strcasecmp(line, "a") == 0) {
                descriptor_send(d, "Character creation cancelled.\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }

            if (strcasecmp(line, "done") == 0 || strcasecmp(line, "d") == 0) {
                /* Race, territory, and class are already chosen by this point
                 * (user 2026-07-12: "selection of race and class should go
                 * before picking attributes"; territory 2026-08-03 slots into
                 * the same rule, right after race) -- their stat bonuses fold
                 * into the point-buy result now, same as before the reorder,
                 * just applied here instead of at selection time so
                 * attrs_allocated() still measures pure point-buy spend, not
                 * race/territory/class deltas. */
                race_stat_bonus(d->new_char_race, &d->new_char_attrs);
                territory_stat_bonus(d->new_char_territory, &d->new_char_attrs);
                class_stat_bonus(d->new_char_class, &d->new_char_attrs);
                d->state = CONN_CHAR_CREATE_OPTIONS;
                show_options_screen(d);
                return true;
            }

            if (strcasecmp(line, "reset") == 0 || strcasecmp(line, "r") == 0) {
                d->new_char_attrs = (attrs_t){ ATTR_BASE, ATTR_BASE, ATTR_BASE, ATTR_BASE, ATTR_BASE, ATTR_BASE };
                show_attr_screen(d);
                return true;
            }

            /* Numbered pick (user wireframe, 2026-07-26): a bare 1-6 asks
             * for the amount separately instead of requiring the full
             * "str <amount>" in one line. */
            int pick = 0;
            if (sscanf(line, "%d", &pick) == 1 && pick >= 1 && pick <= 6
                && line[strspn(line, "0123456789 ")] == '\0') {
                d->new_char_attr_pick = pick;
                d->state = CONN_CHAR_CREATE_ATTR_AMOUNT;
                show_attr_amount_prompt(d);
                return true;
            }

            char tok[32];
            int amount = 0;
            if (sscanf(line, "%31s %d", tok, &amount) != 2) {
                descriptor_send(d, "Usage: <attribute> <amount>, a number (1-6), 'reset', 'done', or 'quit!'.\r\n");
                show_attr_screen(d);
                return true;
            }

            int *field = attrs_field(&d->new_char_attrs, tok);
            if (!field) {
                descriptor_send(d, "Unknown attribute. Try: str, dex, con, int, wis, cha.\r\n");
                show_attr_screen(d);
                return true;
            }
            apply_attr_delta(d, field, amount);
            show_attr_screen(d);
            return true;
        }

        case CONN_CHAR_CREATE_ATTR_AMOUNT: {
            if (strcasecmp(line, "quit!") == 0 || strcasecmp(line, "quit") == 0) {
                descriptor_send(d, "Character creation cancelled.\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }
            if (line[0] == '\0') {
                /* Blank cancels just this one pick, back to the grid. */
                d->state = CONN_CHAR_CREATE_ATTRS;
                show_attr_screen(d);
                return true;
            }
            int amount = 0;
            if (sscanf(line, "%d", &amount) != 1) {
                descriptor_send(d, "Enter a number, or leave blank to cancel: ");
                return true;
            }
            static const char *const FIELD_TOKS[6] = { "str", "dex", "con", "int", "wis", "cha" };
            int *field = attrs_field(&d->new_char_attrs, FIELD_TOKS[d->new_char_attr_pick - 1]);
            apply_attr_delta(d, field, amount);
            d->state = CONN_CHAR_CREATE_ATTRS;
            show_attr_screen(d);
            return true;
        }

        case CONN_CHAR_CREATE_OPTIONS: {
            if (strcasecmp(line, "quit!") == 0 || strcasecmp(line, "quit") == 0
                || strcasecmp(line, "abort") == 0 || strcasecmp(line, "a") == 0) {
                descriptor_send(d, "Character creation cancelled.\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }

            if (strcasecmp(line, "reset") == 0 || strcasecmp(line, "r") == 0) {
                d->new_char_handed = 1;
                d->new_char_gender = GENDER_NEUTER;
                d->new_char_alignment = 0;
                d->new_char_appearance[0] = '\0';
                show_options_screen(d);
                return true;
            }

            if (strcasecmp(line, "done") == 0 || strcasecmp(line, "d") == 0) {
                being_t *b = player_create(d->new_char_name, d->account.account_id,
                                           &d->new_char_attrs, d->new_char_handed,
                                           d->new_char_gender, d->new_char_appearance,
                                           d->new_char_class, d->new_char_race,
                                           d->new_char_alignment, d->new_char_territory);
                if (!b) {
                    descriptor_send(d, "Could not create that character (name may already be taken).\r\n");
                    d->state = CONN_ACCOUNT_MENU;
                    show_account_menu(d);
                    return true;
                }
                /* One-time nudge (user 2026-07-12: "i want it so a first
                 * time player of this game will feel comfortable playing
                 * because he knows where to find game play information")
                 * -- shown only right here, at genuine character
                 * creation, not on every later login (see enter_world()'s
                 * "Welcome, X!"), so a veteran never sees it again. */
                descriptor_send(d, "\r\nNew to TobinMUD? Type 'help playing' any time for an overview of the basics.\r\n");
                enter_world(d, b);
                return true;
            }

            int choice = 0;
            if (sscanf(line, "%d", &choice) != 1 || choice < 1 || choice > 4) {
                descriptor_send(d, "Enter a number (1-4), 'done', 'reset', or 'quit!'.\r\n");
                show_options_screen(d);
                return true;
            }
            switch (choice) {
                case 1: d->state = CONN_CHAR_CREATE_OPT_HAND; show_opt_hand_screen(d); break;
                case 2: d->state = CONN_CHAR_CREATE_OPT_GENDER; show_opt_gender_screen(d); break;
                case 3: d->state = CONN_CHAR_CREATE_OPT_ALIGN; show_opt_align_screen(d); break;
                case 4: d->state = CONN_CHAR_CREATE_OPT_APPEARANCE; show_opt_appearance_screen(d); break;
            }
            return true;
        }

        case CONN_CHAR_CREATE_OPT_HAND: {
            if (strcasecmp(line, "quit!") == 0 || strcasecmp(line, "quit") == 0) {
                descriptor_send(d, "Character creation cancelled.\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }
            int choice = 0;
            if (sscanf(line, "%d", &choice) != 1 || choice < 1 || choice > 2) {
                descriptor_send(d, "Enter a number from 1 to 2, or 'quit!'.\r\n");
                show_opt_hand_screen(d);
                return true;
            }
            d->new_char_handed = (choice == 2) ? 1 : 0;
            d->state = CONN_CHAR_CREATE_OPTIONS;
            show_options_screen(d);
            return true;
        }

        case CONN_CHAR_CREATE_OPT_GENDER: {
            if (strcasecmp(line, "quit!") == 0 || strcasecmp(line, "quit") == 0) {
                descriptor_send(d, "Character creation cancelled.\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }
            int choice = 0;
            if (sscanf(line, "%d", &choice) != 1 || choice < 1 || choice > 3) {
                descriptor_send(d, "Enter a number from 1 to 3, or 'quit!'.\r\n");
                show_opt_gender_screen(d);
                return true;
            }
            static const gender_t GENDER_CHOICES[3] = { GENDER_MALE, GENDER_FEMALE, GENDER_NEUTER };
            d->new_char_gender = GENDER_CHOICES[choice - 1];
            d->state = CONN_CHAR_CREATE_OPTIONS;
            show_options_screen(d);
            return true;
        }

        case CONN_CHAR_CREATE_OPT_ALIGN: {
            if (strcasecmp(line, "quit!") == 0 || strcasecmp(line, "quit") == 0) {
                descriptor_send(d, "Character creation cancelled.\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }
            int choice = 0;
            if (sscanf(line, "%d", &choice) != 1 || choice < 1 || choice > 3) {
                descriptor_send(d, "Enter a number from 1 to 3, or 'quit!'.\r\n");
                show_opt_align_screen(d);
                return true;
            }
            /* 1 Good -> +500, 2 Neutral -> 0, 3 Evil -> -500 -- solidly in
             * alignment_word()'s "good"/"neutral"/"evil" tiers (>=350/
             * between/<=-350), leaving room to drift toward saintly/demonic
             * through play. */
            static const int ALIGNMENT_CHOICES[3] = { 500, 0, -500 };
            d->new_char_alignment = ALIGNMENT_CHOICES[choice - 1];
            d->state = CONN_CHAR_CREATE_OPTIONS;
            show_options_screen(d);
            return true;
        }

        case CONN_CHAR_CREATE_OPT_APPEARANCE: {
            if (strcasecmp(line, "quit!") == 0 || strcasecmp(line, "quit") == 0) {
                descriptor_send(d, "Character creation cancelled.\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }
            snprintf(d->new_char_appearance, sizeof(d->new_char_appearance), "%s", line);
            d->state = CONN_CHAR_CREATE_OPTIONS;
            show_options_screen(d);
            return true;
        }

        case CONN_CHAR_DELETE_PICK: {
            if (strcasecmp(line, "quit!") == 0 || strcasecmp(line, "quit") == 0) {
                descriptor_send(d, "Cancelled.\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }
            const char *pick = NULL;
            char *tend = NULL;
            long tnum = strtol(line, &tend, 10);
            if (tend != line && *tend == '\0' && tnum >= 1 && tnum <= d->char_count) {
                pick = d->char_list[tnum - 1];
            } else {
                for (int i = 0; i < d->char_count; i++) {
                    if (strcasecmp(d->char_list[i], line) == 0) { pick = d->char_list[i]; break; }
                }
            }
            if (!pick) {
                descriptor_send(d, "No character by that name or number. Delete which number or name (or 'quit!' to cancel)? ");
                return true;
            }
            snprintf(d->delete_char_name, sizeof(d->delete_char_name), "%s", pick);
            char confirm[192];
            snprintf(confirm, sizeof(confirm),
                     "\r\nReally delete '%s'? This cannot be undone.\r\n"
                     "Type YES (all caps) to confirm, or anything else to cancel: ",
                     d->delete_char_name);
            descriptor_send(d, confirm);
            d->state = CONN_CHAR_DELETE_CONFIRM;
            return true;
        }

        case CONN_CHAR_DELETE_CONFIRM: {
            if (strcmp(line, "YES") != 0) {
                descriptor_send(d, "Cancelled.\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }
            /* Typed YES is not enough on its own: re-verify the account
             * password before the irreversible delete (matches the
             * original's delete-character flow). */
            descriptor_send(d, "Enter your account password to confirm: ");
            d->state = CONN_CHAR_DELETE_PASSWORD;
            return true;
        }

        case CONN_CHAR_DELETE_PASSWORD: {
            if (!account_verify_password(&d->account, line)) {
                descriptor_send(d, "Incorrect password. Deletion cancelled.\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }
            /* A character disconnected via a raw socket close (never
             * `quit!`) leaves a linkdead being_t standing in its room
             * (descriptor_destroy()'s own documented behavior) -- look it
             * up BEFORE the DB row goes away (world_find_linkdead_pc()
             * matches by player_id, the same lookup a real reconnect uses
             * in enter_world()) so deleting the character doesn't leave
             * that body orphaned forever, now pointing at a player_id
             * that no longer exists. */
            long pid = player_id_for_name(d->delete_char_name);
            if (player_delete(d->delete_char_name, d->account.account_id)) {
                if (pid >= 0) {
                    being_t *linkdead = world_find_linkdead_pc(pid);
                    if (linkdead)
                        being_destroy(linkdead);
                }
                log_info("Character %s deleted (account %s). [%s]",
                         d->delete_char_name, d->account.name, descriptor_display_host(d));
                descriptor_send(d, "Character deleted.\r\n");
            } else {
                descriptor_send(d, "Could not delete that character.\r\n");
            }
            d->state = CONN_ACCOUNT_MENU;
            show_account_menu(d);
            return true;
        }

        case CONN_ACCOUNT_DELETE_CONFIRM: {
            if (strcmp(line, "YES") != 0) {
                descriptor_send(d, "Cancelled.\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }
            /* Typed YES is not enough on its own: re-verify the account
             * password before the irreversible delete (same precedent as
             * character deletion above). */
            descriptor_send(d, "Enter your account password to confirm: ");
            d->state = CONN_ACCOUNT_DELETE_PASSWORD;
            return true;
        }

        case CONN_ACCOUNT_DELETE_PASSWORD: {
            if (!account_verify_password(&d->account, line)) {
                descriptor_send(d, "Incorrect password. Deletion cancelled.\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }
            /* player.account_id cascades on delete (ON DELETE CASCADE FK),
             * so every character on this account goes with it. */
            long deleted_id = d->account.account_id;
            char deleted_name[80];
            snprintf(deleted_name, sizeof(deleted_name), "%s", d->account.name);
            bool ok = account_delete(deleted_id);
            if (ok) {
                log_info("Account %s (id %ld) deleted, %d character(s) with it. [%s]",
                         deleted_name, deleted_id, d->char_count, descriptor_display_host(d));
                descriptor_send(d, "Your account has been deleted. Goodbye!\r\n");
            } else {
                descriptor_send(d, "Could not delete the account.\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }
            return false; /* the account is gone -- disconnect */
        }

        case CONN_REDIT_MENU: {
            room_t *w = &d->redit_work;
            /* A leading digit selects a numbered field (Sneezy-style menu);
             * the letters C/S/Q are the working-copy actions. */
            if (isdigit((unsigned char)line[0])) {
                switch (atoi(line)) {
                    case 1: {
                        char msg[128];
                        snprintf(msg, sizeof(msg),
                            "\r\nCurrent name: %s\r\nEnter new room name (blank to cancel): ",
                            w->base.name);
                        descriptor_send(d, msg);
                        d->state = CONN_REDIT_NAME;
                        break;
                    }
                    case 2:
                        snprintf(d->edit_buf, sizeof(d->edit_buf), "%s", w->description);
                        d->edit_len = (int)strlen(d->edit_buf);
                        descriptor_send(d,
                            "\r\n-- Editing description. /s saves, /a aborts, "
                            "/b blanks, /f reflows to width. --\r\n");
                        if (w->description[0]) {
                            descriptor_send(d, w->description);
                            if (w->description[strlen(w->description) - 1] != '\n')
                                descriptor_send(d, "\r\n");
                        }
                        descriptor_send(d, "] ");
                        d->state = CONN_REDIT_DESC;
                        break;
                    case 3: show_redit_flags(d); break;
                    case 4: show_redit_terrain(d); break;
                    case 5: show_redit_exits(d); break;
                    case 6:
                        descriptor_send(d, "\r\nEnter max capacity (blank to cancel): ");
                        d->state = CONN_REDIT_CAPACITY;
                        break;
                    case 7:
                        descriptor_send(d, "\r\nEnter room height (blank to cancel): ");
                        d->state = CONN_REDIT_HEIGHT;
                        break;
                    case 8:
                        show_redit_extra_menu(d);
                        break;
                    default:
                        descriptor_send(d, "Pick a menu number (1-8), or C/S/Q.\r\n");
                        show_redit_menu(d);
                        break;
                }
                return true;
            }
            switch ((char)toupper((unsigned char)line[0])) {
                case 'C':
                    descriptor_send(d,
                        "\r\nReally blank this room -- name, description, terrain, "
                        "flags AND exits? (yes/no): ");
                    d->state = CONN_REDIT_CLEAR_CONFIRM;
                    break;
                case 'S':
                    redit_save(d);
                    show_redit_menu(d);
                    break;
                case 'Q':
                    if (d->redit_dirty) {
                        descriptor_send(d,
                            "\r\nYou have unsaved changes. (S)ave, (D)iscard, (C)ancel: ");
                        d->state = CONN_REDIT_QUIT_CONFIRM;
                    } else {
                        redit_leave(d);
                    }
                    break;
                default:
                    descriptor_send(d, "Pick a menu number (1-8), or C/S/Q.\r\n");
                    show_redit_menu(d);
                    break;
            }
            return true;
        }

        case CONN_REDIT_NAME: {
            if (line[0]) {
                snprintf(d->redit_work.base.name, sizeof(d->redit_work.base.name), "%s", line);
                d->redit_dirty = true;
            }
            show_redit_menu(d);
            return true;
        }

        case CONN_REDIT_TERRAIN: {
            if (!line[0]) { show_redit_menu(d); return true; }
            char *end;
            long s = strtol(line, &end, 10);
            if (end == line || s < 0 || s >= MAX_SECTOR_TYPES) {
                descriptor_send(d, "Not a valid terrain number.\r\n");
                show_redit_terrain(d);
                return true;
            }
            d->redit_work.sector = (int)s;
            d->redit_dirty = true;
            show_redit_menu(d);
            return true;
        }

        case CONN_REDIT_FLAGS: {
            if (!line[0]) { show_redit_menu(d); return true; }
            char *end;
            long b = strtol(line, &end, 10);
            if (end == line || b < 0 || b >= room_flag_count()) {
                descriptor_send(d, "Not a valid flag number.\r\n");
                show_redit_flags(d);
                return true;
            }
            d->redit_work.room_flag ^= (1 << (int)b);
            d->redit_dirty = true;
            show_redit_flags(d); /* stay in the flag menu, re-render */
            return true;
        }

        case CONN_REDIT_CAPACITY: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= 0) {
                    d->redit_work.capacity = (int)v;
                    d->redit_dirty = true;
                } else {
                    descriptor_send(d, "Capacity must be a non-negative number.\r\n");
                }
            }
            show_redit_menu(d);
            return true;
        }

        case CONN_REDIT_HEIGHT: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= 0) {
                    d->redit_work.height = (int)v;
                    d->redit_dirty = true;
                } else {
                    descriptor_send(d, "Height must be a non-negative number.\r\n");
                }
            }
            show_redit_menu(d);
            return true;
        }

        case CONN_REDIT_EXITS: {
            if (!line[0]) { show_redit_menu(d); return true; }
            char tok[16];
            sscanf(line, "%15s", tok);
            int dir = redit_parse_dir(tok);
            if (dir < 0) {
                descriptor_send(d, "Which direction? (north..southwest, or 0-9)\r\n");
                show_redit_exits(d);
                return true;
            }
            d->redit_exit_dir = dir;
            show_redit_exit_menu(d);
            return true;
        }

        case CONN_REDIT_EXIT_MENU: {
            if (!line[0]) { show_redit_exits(d); return true; }
            int dir = d->redit_exit_dir;
            switch (atoi(line)) {
                case 1:
                    descriptor_send(d,
                        "\r\nEnter target room vnum (-1 removes the exit, blank cancels): ");
                    d->state = CONN_REDIT_EXIT_TARGET;
                    break;
                case 2:
                    if (d->redit_work.exits[dir] < 0) {
                        descriptor_send(d, "Set a target room first.\r\n");
                        show_redit_exit_menu(d);
                    } else {
                        show_redit_doortype(d);
                    }
                    break;
                case 3:
                    if (d->redit_work.exits[dir] < 0) {
                        descriptor_send(d, "Set a target room first.\r\n");
                        show_redit_exit_menu(d);
                    } else {
                        show_redit_conditions(d);
                    }
                    break;
                case 4:
                    if (d->redit_work.exits[dir] >= 0) {
                        d->redit_work.exits[dir] = -1;
                        d->redit_work.exit_door[dir] = 0;
                        d->redit_work.exit_cond[dir] = 0;
                        d->redit_dirty = true;
                        descriptor_send(d, "Exit removed (Save to persist).\r\n");
                    }
                    show_redit_exits(d);
                    break;
                default:
                    descriptor_send(d, "Pick 1-4, or blank to go back.\r\n");
                    show_redit_exit_menu(d);
                    break;
            }
            return true;
        }

        case CONN_REDIT_EXIT_TARGET: {
            if (!line[0]) { show_redit_exit_menu(d); return true; }
            char *end;
            long v = strtol(line, &end, 10);
            if (end == line) {
                descriptor_send(d, "Enter a room number, or -1 to remove.\r\n");
                show_redit_exit_menu(d);
                return true;
            }
            if (v >= 0 && (int)v == d->redit_work.vnum) {
                descriptor_send(d, "An exit into the same room? Pick another target.\r\n");
                show_redit_exit_menu(d);
                return true;
            }
            d->redit_work.exits[d->redit_exit_dir] = (v < 0) ? -1 : (int)v;
            if (v < 0) {
                d->redit_work.exit_door[d->redit_exit_dir] = 0;
                d->redit_work.exit_cond[d->redit_exit_dir] = 0;
            }
            d->redit_dirty = true;
            if (v >= 0 && !world_get_room((int)v) && !room_repo_exists((int)v))
                descriptor_send(d,
                    "(That room doesn't exist yet -- it will be created on save.)\r\n");
            show_redit_exit_menu(d);
            return true;
        }

        case CONN_REDIT_EXIT_DOORTYPE: {
            if (!line[0]) { show_redit_exit_menu(d); return true; }
            char *end;
            long t = strtol(line, &end, 10);
            if (end == line || t < 0 || t >= door_type_count()) {
                descriptor_send(d, "Not a valid door type number.\r\n");
                show_redit_doortype(d);
                return true;
            }
            d->redit_work.exit_door[d->redit_exit_dir] = (int)t;
            d->redit_dirty = true;
            show_redit_exit_menu(d);
            return true;
        }

        case CONN_REDIT_EXIT_CONDITIONS: {
            if (!line[0]) { show_redit_exit_menu(d); return true; }
            char *end;
            long b = strtol(line, &end, 10);
            if (end == line || b < 0 || b >= exit_cond_count()) {
                descriptor_send(d, "Not a valid condition number.\r\n");
                show_redit_conditions(d);
                return true;
            }
            d->redit_work.exit_cond[d->redit_exit_dir] ^= (1 << (int)b);
            d->redit_dirty = true;
            show_redit_conditions(d); /* stay in the condition menu, re-render */
            return true;
        }

        case CONN_REDIT_DESC: {
            editor_action_t act = editor_feed(d, line);
            if (act == EDITOR_SAVE) {
                snprintf(d->redit_work.description, sizeof(d->redit_work.description),
                         "%s", d->edit_buf);
                d->redit_dirty = true;
                descriptor_send(d, "Description updated (not yet saved to the DB).\r\n");
                show_redit_menu(d);
            } else if (act == EDITOR_ABORT) {
                descriptor_send(d, "Description unchanged.\r\n");
                show_redit_menu(d);
            }
            return true;
        }

        case CONN_REDIT_EXTRA_MENU: {
            if (!line[0]) {
                show_redit_menu(d);
                return true;
            }
            if (line[1] == '\0' && (char)toupper((unsigned char)line[0]) == 'A') {
                d->redit_extra_name[0] = '\0';
                descriptor_send(d, "\r\nEnter keywords (space-separated, blank to cancel): ");
                d->state = CONN_REDIT_EXTRA_KEYWORDS;
                return true;
            }
            if (line[1] == '\0' && (char)toupper((unsigned char)line[0]) == 'Z') {
                descriptor_send(d,
                    "\r\nReally delete ALL extra descriptions for this room? (yes/no): ");
                d->state = CONN_REDIT_EXTRA_DELETE_ALL_CONFIRM;
                return true;
            }
            if (isdigit((unsigned char)line[0])) {
                char names[ROOM_EXTRA_MAX_LIST][ROOM_EXTRA_NAME_LEN];
                int n = room_repo_extra_list(d->redit_work.vnum, names, ROOM_EXTRA_MAX_LIST);
                int idx = atoi(line) - 1;
                if (idx >= 0 && idx < n) {
                    snprintf(d->redit_extra_name, sizeof(d->redit_extra_name), "%s", names[idx]);
                    show_redit_extra_item(d);
                    return true;
                }
            }
            descriptor_send(d, "Pick a number from the list, A to add, or blank to return.\r\n");
            show_redit_extra_menu(d);
            return true;
        }

        case CONN_REDIT_EXTRA_ITEM: {
            if (!line[0]) {
                show_redit_extra_menu(d);
                return true;
            }
            switch (line[0]) {
                case '1': {
                    char msg[ROOM_EXTRA_NAME_LEN + 96];
                    snprintf(msg, sizeof(msg),
                        "\r\nCurrent keywords: %s\r\nEnter new keywords (blank to cancel): ",
                        d->redit_extra_name);
                    descriptor_send(d, msg);
                    d->state = CONN_REDIT_EXTRA_KEYWORDS;
                    break;
                }
                case '2': {
                    char desc[4096];
                    if (!room_repo_extra_get(d->redit_work.vnum, d->redit_extra_name, desc, sizeof(desc)))
                        desc[0] = '\0';
                    snprintf(d->edit_buf, sizeof(d->edit_buf), "%s", desc);
                    d->edit_len = (int)strlen(d->edit_buf);
                    descriptor_send(d,
                        "\r\n-- Editing extra description. /s saves, /a aborts, "
                        "/b blanks, /f reflows to width. --\r\n");
                    if (d->edit_buf[0]) {
                        descriptor_send(d, d->edit_buf);
                        if (d->edit_buf[strlen(d->edit_buf) - 1] != '\n')
                            descriptor_send(d, "\r\n");
                    }
                    descriptor_send(d, "] ");
                    d->redit_extra_is_new = false;
                    d->state = CONN_REDIT_EXTRA_DESC;
                    break;
                }
                case '3': {
                    char msg[ROOM_EXTRA_NAME_LEN + 64];
                    snprintf(msg, sizeof(msg),
                        "\r\nReally delete the extra description \"%s\"? (yes/no): ",
                        d->redit_extra_name);
                    descriptor_send(d, msg);
                    d->state = CONN_REDIT_EXTRA_DELETE_CONFIRM;
                    break;
                }
                default:
                    descriptor_send(d, "Pick 1-3, or blank to return.\r\n");
                    show_redit_extra_item(d);
                    break;
            }
            return true;
        }

        case CONN_REDIT_EXTRA_KEYWORDS: {
            bool adding_new = d->redit_extra_name[0] == '\0';
            if (!line[0]) {
                descriptor_send(d, "Cancelled.\r\n");
                if (adding_new)
                    show_redit_extra_menu(d);
                else
                    show_redit_extra_item(d);
                return true;
            }
            if (adding_new) {
                snprintf(d->redit_extra_name, sizeof(d->redit_extra_name), "%s", line);
                d->edit_buf[0] = '\0';
                d->edit_len = 0;
                descriptor_send(d,
                    "\r\n-- Now enter the description. /s saves, /a aborts, "
                    "/b blanks, /f reflows to width. --\r\n] ");
                d->redit_extra_is_new = true;
                d->state = CONN_REDIT_EXTRA_DESC;
            } else {
                char old_name[ROOM_EXTRA_NAME_LEN];
                snprintf(old_name, sizeof(old_name), "%s", d->redit_extra_name);
                if (room_repo_extra_rename(d->redit_work.vnum, old_name, line)) {
                    snprintf(d->redit_extra_name, sizeof(d->redit_extra_name), "%s", line);
                    descriptor_send(d, "Keywords updated.\r\n");
                } else {
                    descriptor_send(d,
                        "Rename failed -- that exact keyword set may already be used here.\r\n");
                }
                show_redit_extra_item(d);
            }
            return true;
        }

        case CONN_REDIT_EXTRA_DESC: {
            editor_action_t act = editor_feed(d, line);
            if (act == EDITOR_SAVE) {
                if (room_repo_extra_save(d->redit_work.vnum, d->redit_extra_name, d->edit_buf))
                    descriptor_send(d, "Extra description saved.\r\n");
                else
                    descriptor_send(d, "Save failed.\r\n");
                show_redit_extra_item(d);
            } else if (act == EDITOR_ABORT) {
                descriptor_send(d, "Extra description unchanged.\r\n");
                /* A brand-new entry that was never saved doesn't exist yet --
                 * go back to the list, not a nonexistent item's detail view. */
                if (d->redit_extra_is_new)
                    show_redit_extra_menu(d);
                else
                    show_redit_extra_item(d);
            }
            return true;
        }

        case CONN_REDIT_EXTRA_DELETE_CONFIRM: {
            if (strcasecmp(line, "yes") == 0) {
                room_repo_extra_delete(d->redit_work.vnum, d->redit_extra_name);
                descriptor_send(d, "Extra description deleted.\r\n");
            } else {
                descriptor_send(d, "Delete cancelled.\r\n");
            }
            show_redit_extra_menu(d);
            return true;
        }

        case CONN_REDIT_EXTRA_DELETE_ALL_CONFIRM: {
            if (strcasecmp(line, "yes") == 0) {
                room_repo_extra_delete_all(d->redit_work.vnum);
                descriptor_send(d, "All extra descriptions deleted.\r\n");
            } else {
                descriptor_send(d, "Delete cancelled.\r\n");
            }
            show_redit_extra_menu(d);
            return true;
        }

        case CONN_REDIT_CLEAR_CONFIRM: {
            if (strcasecmp(line, "yes") == 0) {
                redit_clear(d);
                descriptor_send(d, "Room blanked in the editor (Save to persist).\r\n");
            } else {
                descriptor_send(d, "Clear cancelled.\r\n");
            }
            show_redit_menu(d);
            return true;
        }

        case CONN_REDIT_QUIT_CONFIRM: {
            char c = (char)toupper((unsigned char)line[0]);
            if (c == 'S') {
                redit_save(d);
                redit_leave(d);
            } else if (c == 'D') {
                redit_leave(d);
            } else {
                show_redit_menu(d);
            }
            return true;
        }

        case CONN_EDPLAYER_MENU: {
            being_t *w = &d->edplayer_work;
            if (isdigit((unsigned char)line[0])) {
                switch (atoi(line)) {
                    case 1:
                        descriptor_send(d, "\r\nEnter new level (blank to cancel): ");
                        d->state = CONN_EDPLAYER_LEVEL;
                        break;
                    case 2:
                        descriptor_send(d, "\r\nEnter new experience (blank to cancel): ");
                        d->state = CONN_EDPLAYER_XP;
                        break;
                    case 3:
                        descriptor_send(d, "\r\nEnter HP and Max HP, e.g. \"25 25\" (blank to cancel): ");
                        d->state = CONN_EDPLAYER_HP;
                        break;
                    case 4: {
                        char msg[256];
                        snprintf(msg, sizeof(msg),
                            "\r\n-- Attributes for %s -- <attr> <value> (str/dex/con/int/wis/cha,\r\n"
                            "1-%d), or 'done' to return: --\r\n"
                            "  Str %d  Dex %d  Con %d  Int %d  Wis %d  Cha %d\r\n] ",
                            w->base.name, ATTR_MAX, w->attrs.strength, w->attrs.dexterity,
                            w->attrs.constitution, w->attrs.intelligence, w->attrs.wisdom,
                            w->attrs.charisma);
                        descriptor_send(d, msg);
                        d->state = CONN_EDPLAYER_ATTRS;
                        break;
                    }
                    case 5:
                        descriptor_send(d, "\r\nEnter gender: male, female, or neuter (blank to cancel): ");
                        d->state = CONN_EDPLAYER_GENDER;
                        break;
                    case 6:
                        descriptor_send(d, "\r\nEnter new title, or 'none' to clear (blank to cancel): ");
                        d->state = CONN_EDPLAYER_TITLE;
                        break;
                    case 7:
                        descriptor_send(d, "\r\nEnter new load-room vnum (blank to cancel): ");
                        d->state = CONN_EDPLAYER_LOADROOM;
                        break;
                    case 8:
                        descriptor_send(d, "\r\nEnter handedness: left or right (blank to cancel): ");
                        d->state = CONN_EDPLAYER_HANDED;
                        break;
                    case 9: {
                        char msg[160];
                        snprintf(msg, sizeof(msg),
                            "\r\nEnter class: mage, cleric, warrior, thief, druid, or monk "
                            "(blank to cancel): ");
                        descriptor_send(d, msg);
                        d->state = CONN_EDPLAYER_CLASS;
                        break;
                    }
                    case 0: {
                        char msg[160];
                        snprintf(msg, sizeof(msg),
                            "\r\nEnter race: human, elf, ogre, dwarf, hobbit, or gnome "
                            "(blank to cancel): ");
                        descriptor_send(d, msg);
                        d->state = CONN_EDPLAYER_RACE;
                        break;
                    }
                    default:
                        descriptor_send(d, "Pick a menu number (0-9), or S/Q.\r\n");
                        show_edplayer_menu(d);
                        break;
                }
                return true;
            }
            switch ((char)toupper((unsigned char)line[0])) {
                case 'S':
                    edplayer_save(d);
                    show_edplayer_menu(d);
                    break;
                case 'Q':
                    if (d->edplayer_dirty) {
                        descriptor_send(d,
                            "\r\nYou have unsaved changes. (S)ave, (D)iscard, (C)ancel: ");
                        d->state = CONN_EDPLAYER_QUIT_CONFIRM;
                    } else {
                        edplayer_leave(d);
                    }
                    break;
                default:
                    descriptor_send(d, "Pick a menu number (0-9), or S/Q.\r\n");
                    show_edplayer_menu(d);
                    break;
            }
            return true;
        }

        case CONN_EDPLAYER_LEVEL: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= MORTAL_LEVEL_MIN && v <= IMMORTAL_LEVEL_MAX) {
                    d->edplayer_work.progress.level = (int)v;
                    d->edplayer_dirty = true;
                } else {
                    char msg[96];
                    snprintf(msg, sizeof(msg), "Level must be between %d and %d.\r\n",
                             MORTAL_LEVEL_MIN, IMMORTAL_LEVEL_MAX);
                    descriptor_send(d, msg);
                }
            }
            show_edplayer_menu(d);
            return true;
        }

        case CONN_EDPLAYER_XP: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= 0) {
                    d->edplayer_work.progress.experience = v;
                    d->edplayer_dirty = true;
                } else {
                    descriptor_send(d, "Experience must be a non-negative number.\r\n");
                }
            }
            show_edplayer_menu(d);
            return true;
        }

        case CONN_EDPLAYER_HP: {
            if (line[0]) {
                int hp = 0, max_hp = 0;
                if (sscanf(line, "%d %d", &hp, &max_hp) == 2 && hp >= 0 && max_hp >= 1 && hp <= max_hp) {
                    d->edplayer_work.progress.hp = hp;
                    d->edplayer_work.progress.max_hp = max_hp;
                    d->edplayer_dirty = true;
                } else {
                    descriptor_send(d, "Usage: <hp> <max hp>, with 0 <= hp <= max hp and max hp >= 1.\r\n");
                }
            }
            show_edplayer_menu(d);
            return true;
        }

        case CONN_EDPLAYER_ATTRS: {
            if (strcasecmp(line, "done") == 0) {
                show_edplayer_menu(d);
                return true;
            }
            char tok[32];
            int value = 0;
            if (sscanf(line, "%31s %d", tok, &value) != 2) {
                descriptor_send(d, "Usage: <attr> <value>, or 'done'.\r\n");
            } else {
                int *field = attrs_field(&d->edplayer_work.attrs, tok);
                if (!field) {
                    descriptor_send(d, "Unknown attribute. Try: str, dex, con, int, wis, cha.\r\n");
                } else if (value < 1 || value > ATTR_MAX) {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "Value must be between 1 and %d.\r\n", ATTR_MAX);
                    descriptor_send(d, msg);
                } else {
                    *field = value;
                    d->edplayer_dirty = true;
                }
            }
            char msg[256];
            being_t *w = &d->edplayer_work;
            snprintf(msg, sizeof(msg),
                "  Str %d  Dex %d  Con %d  Int %d  Wis %d  Cha %d\r\n] ",
                w->attrs.strength, w->attrs.dexterity, w->attrs.constitution,
                w->attrs.intelligence, w->attrs.wisdom, w->attrs.charisma);
            descriptor_send(d, msg);
            return true;
        }

        case CONN_EDPLAYER_GENDER: {
            if (line[0]) {
                if (strcasecmp(line, "male") == 0 || strcasecmp(line, "m") == 0) {
                    d->edplayer_work.gender = GENDER_MALE;
                    d->edplayer_dirty = true;
                } else if (strcasecmp(line, "female") == 0 || strcasecmp(line, "f") == 0) {
                    d->edplayer_work.gender = GENDER_FEMALE;
                    d->edplayer_dirty = true;
                } else if (strcasecmp(line, "neuter") == 0 || strcasecmp(line, "n") == 0) {
                    d->edplayer_work.gender = GENDER_NEUTER;
                    d->edplayer_dirty = true;
                } else {
                    descriptor_send(d, "Usage: male, female, or neuter.\r\n");
                }
            }
            show_edplayer_menu(d);
            return true;
        }

        case CONN_EDPLAYER_TITLE: {
            if (line[0]) {
                if (strcasecmp(line, "none") == 0)
                    d->edplayer_work.title[0] = '\0';
                else
                    snprintf(d->edplayer_work.title, sizeof(d->edplayer_work.title), "%s", line);
                d->edplayer_dirty = true;
            }
            show_edplayer_menu(d);
            return true;
        }

        case CONN_EDPLAYER_LOADROOM: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= 0) {
                    d->edplayer_load_room = (int)v;
                    d->edplayer_dirty = true;
                } else {
                    descriptor_send(d, "Load room must be a non-negative vnum.\r\n");
                }
            }
            show_edplayer_menu(d);
            return true;
        }

        case CONN_EDPLAYER_HANDED: {
            if (line[0]) {
                if (strcasecmp(line, "left") == 0 || strcasecmp(line, "l") == 0) {
                    d->edplayer_work.handed_right = 0;
                    d->edplayer_dirty = true;
                } else if (strcasecmp(line, "right") == 0 || strcasecmp(line, "r") == 0) {
                    d->edplayer_work.handed_right = 1;
                    d->edplayer_dirty = true;
                } else {
                    descriptor_send(d, "Usage: left or right.\r\n");
                }
            }
            show_edplayer_menu(d);
            return true;
        }

        case CONN_EDPLAYER_CLASS: {
            if (line[0]) {
                static const char *const NAMES[CLASS_COUNT] =
                    { "mage", "cleric", "warrior", "thief", "druid", "monk" };
                int found = -1;
                for (int i = 0; i < CLASS_COUNT; i++) {
                    if (strncasecmp(NAMES[i], line, strlen(line)) == 0) {
                        found = i;
                        break;
                    }
                }
                if (found >= 0) {
                    d->edplayer_work.char_class = (player_class_t)found;
                    d->edplayer_dirty = true;
                } else {
                    descriptor_send(d, "Usage: mage, cleric, warrior, thief, druid, or monk.\r\n");
                }
            }
            show_edplayer_menu(d);
            return true;
        }

        case CONN_EDPLAYER_RACE: {
            if (line[0]) {
                static const char *const NAMES[RACE_COUNT] =
                    { "human", "elf", "ogre", "dwarf", "hobbit", "gnome" };
                int found = -1;
                for (int i = 0; i < RACE_COUNT; i++) {
                    if (strncasecmp(NAMES[i], line, strlen(line)) == 0) {
                        found = i;
                        break;
                    }
                }
                if (found >= 0) {
                    d->edplayer_work.race = (player_race_t)found;
                    d->edplayer_dirty = true;
                } else {
                    descriptor_send(d, "Usage: human, elf, ogre, dwarf, hobbit, or gnome.\r\n");
                }
            }
            show_edplayer_menu(d);
            return true;
        }

        case CONN_EDPLAYER_QUIT_CONFIRM: {
            char c = (char)toupper((unsigned char)line[0]);
            if (c == 'S') {
                edplayer_save(d);
                edplayer_leave(d);
            } else if (c == 'D') {
                edplayer_leave(d);
            } else {
                show_edplayer_menu(d);
            }
            return true;
        }

        case CONN_EDZONE_MENU: {
            if (isdigit((unsigned char)line[0])) {
                switch (atoi(line)) {
                    case 1:
                        descriptor_send(d, "\r\nEnter new name (blank to cancel): ");
                        d->state = CONN_EDZONE_NAME;
                        break;
                    case 2:
                        descriptor_send(d, "\r\nEnabled? yes or no (blank to cancel): ");
                        d->state = CONN_EDZONE_ENABLED;
                        break;
                    case 3:
                        descriptor_send(d, "\r\nEnter new lifespan in minutes (blank to cancel): ");
                        d->state = CONN_EDZONE_LIFESPAN;
                        break;
                    case 4:
                        descriptor_send(d, "\r\nEnter bottom and top vnum, e.g. \"100 199\" (blank to cancel): ");
                        d->state = CONN_EDZONE_RANGE;
                        break;
                    case 5:
                        descriptor_send(d, "\r\nEnter a builder name to assign/unassign (blank to cancel): ");
                        d->state = CONN_EDZONE_BUILDER;
                        break;
                    default:
                        descriptor_send(d, "Pick a menu number (1-5), or R/S/Q.\r\n");
                        show_edzone_menu(d);
                        break;
                }
                return true;
            }
            switch ((char)toupper((unsigned char)line[0])) {
                case 'R': {
                    int mobs = 0, objs = 0;
                    zone_reset_now(d->edzone_work.zone_nr, &mobs, &objs);
                    char msg[96];
                    snprintf(msg, sizeof(msg), "Zone reset: %d mobs, %d objects loaded.\r\n", mobs, objs);
                    descriptor_send(d, msg);
                    show_edzone_menu(d);
                    break;
                }
                case 'S':
                    edzone_save(d);
                    show_edzone_menu(d);
                    break;
                case 'Q':
                    if (d->edzone_dirty) {
                        descriptor_send(d,
                            "\r\nYou have unsaved changes. (S)ave, (D)iscard, (C)ancel: ");
                        d->state = CONN_EDZONE_QUIT_CONFIRM;
                    } else {
                        edzone_leave(d);
                    }
                    break;
                default:
                    descriptor_send(d, "Pick a menu number (1-5), or R/S/Q.\r\n");
                    show_edzone_menu(d);
                    break;
            }
            return true;
        }

        case CONN_EDZONE_NAME: {
            if (line[0]) {
                snprintf(d->edzone_work.name, sizeof(d->edzone_work.name), "%s", line);
                d->edzone_dirty = true;
            }
            show_edzone_menu(d);
            return true;
        }

        case CONN_EDZONE_ENABLED: {
            if (line[0]) {
                bool is_no = strcasecmp(line, "n") == 0 || strcasecmp(line, "no") == 0;
                bool is_yes = strcasecmp(line, "y") == 0 || strcasecmp(line, "yes") == 0;
                if (is_yes || is_no) {
                    d->edzone_work.enabled = is_yes;
                    d->edzone_dirty = true;
                } else {
                    descriptor_send(d, "Please answer yes or no.\r\n");
                }
            }
            show_edzone_menu(d);
            return true;
        }

        case CONN_EDZONE_LIFESPAN: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v > 0) {
                    d->edzone_work.lifespan = (int)v;
                    d->edzone_dirty = true;
                } else {
                    descriptor_send(d, "Lifespan must be a positive number of minutes.\r\n");
                }
            }
            show_edzone_menu(d);
            return true;
        }

        case CONN_EDZONE_RANGE: {
            if (line[0]) {
                int bottom, top;
                if (sscanf(line, "%d %d", &bottom, &top) == 2 && bottom <= top) {
                    d->edzone_work.bottom = bottom;
                    d->edzone_work.top = top;
                    d->edzone_dirty = true;
                } else {
                    descriptor_send(d, "Enter two numbers, bottom then top, bottom <= top.\r\n");
                }
            }
            show_edzone_menu(d);
            return true;
        }

        case CONN_EDZONE_BUILDER: {
            if (line[0]) {
                long player_id = player_id_for_name(line);
                if (player_id < 0) {
                    descriptor_send(d, "No such character.\r\n");
                } else if (zone_repo_is_assigned(d->edzone_work.zone_nr, player_id)) {
                    zone_repo_unassign(d->edzone_work.zone_nr, player_id);
                    descriptor_send(d, "Un-assigned.\r\n");
                } else {
                    zone_repo_assign(d->edzone_work.zone_nr, player_id);
                    descriptor_send(d, "Assigned.\r\n");
                }
            }
            show_edzone_menu(d);
            return true;
        }

        case CONN_EDZONE_QUIT_CONFIRM: {
            char c = (char)toupper((unsigned char)line[0]);
            if (c == 'S') {
                edzone_save(d);
                edzone_leave(d);
            } else if (c == 'D') {
                edzone_leave(d);
            } else {
                show_edzone_menu(d);
            }
            return true;
        }

        case CONN_OEDIT_MENU: {
            if (isdigit((unsigned char)line[0])) {
                switch (atoi(line)) {
                    case 1:
                        descriptor_send(d, "\r\nEnter new name (blank to cancel): ");
                        d->state = CONN_OEDIT_NAME;
                        break;
                    case 2:
                        descriptor_send(d, "\r\nEnter new short description (blank to cancel): ");
                        d->state = CONN_OEDIT_SHORT_DESC;
                        break;
                    case 3:
                        show_oedit_type_picker(d);
                        break;
                    case 4:
                        descriptor_send(d, "\r\nEnter new long description (blank to cancel): ");
                        d->state = CONN_OEDIT_LONG_DESC;
                        break;
                    case 5:
                        descriptor_send(d, "\r\nEnter new weight (blank to cancel): ");
                        d->state = CONN_OEDIT_WEIGHT;
                        break;
                    case 6:
                        descriptor_send(d, "\r\nEnter new volume (blank to cancel): ");
                        d->state = CONN_OEDIT_VOLUME;
                        break;
                    case 7:
                        show_oedit_action_flags(d);
                        break;
                    case 8:
                        show_oedit_wear_flags(d);
                        break;
                    case 9:
                        descriptor_send(d, "\r\nEnter new cost/value (blank to cancel): ");
                        d->state = CONN_OEDIT_PRICE;
                        break;
                    case 10:
                        descriptor_send(d, "\r\nEnter four values, e.g. \"0 0 0 0\" (blank to cancel): ");
                        d->state = CONN_OEDIT_VALUES;
                        break;
                    case 11:
                        descriptor_send(d, "\r\nEnter new decay time, -1 for never (blank to cancel): ");
                        d->state = CONN_OEDIT_DECAY;
                        break;
                    case 12:
                        descriptor_send(d, "\r\nEnter new max struct points (blank to cancel): ");
                        d->state = CONN_OEDIT_MAX_STRUCT;
                        break;
                    case 13:
                        descriptor_send(d, "\r\nEnter new struct points (blank to cancel): ");
                        d->state = CONN_OEDIT_CUR_STRUCT;
                        break;
                    case 14:
                        descriptor_send(d, "\r\nEnter new material number (blank to cancel): ");
                        d->state = CONN_OEDIT_MATERIAL;
                        break;
                    case 15:
                        descriptor_send(d, "\r\nCan be seen? yes or no (blank to cancel): ");
                        d->state = CONN_OEDIT_CAN_BE_SEEN;
                        break;
                    case 16:
                        descriptor_send(d, "\r\nEnter new special proc number (blank to cancel): ");
                        d->state = CONN_OEDIT_SPEC_PROC;
                        break;
                    case 17:
                        descriptor_send(d, "\r\nEnter new max exist, 0 for uncapped (blank to cancel): ");
                        d->state = CONN_OEDIT_MAX_EXIST;
                        break;
                    case 18:
                        show_oedit_anti_race_flags(d);
                        break;
                    default:
                        descriptor_send(d, "Pick a menu number (1-18), or S/Q.\r\n");
                        show_oedit_menu(d);
                        break;
                }
                return true;
            }
            switch ((char)toupper((unsigned char)line[0])) {
                case 'S':
                    oedit_save(d);
                    show_oedit_menu(d);
                    break;
                case 'Q':
                    if (d->oedit_dirty) {
                        descriptor_send(d,
                            "\r\nYou have unsaved changes. (S)ave, (D)iscard, (C)ancel: ");
                        d->state = CONN_OEDIT_QUIT_CONFIRM;
                    } else {
                        oedit_leave(d);
                    }
                    break;
                default:
                    descriptor_send(d, "Pick a menu number (1-18), or S/Q.\r\n");
                    show_oedit_menu(d);
                    break;
            }
            return true;
        }

        case CONN_OEDIT_NAME: {
            if (line[0]) {
                snprintf(d->oedit_work.name, sizeof(d->oedit_work.name), "%s", line);
                d->oedit_dirty = true;
            }
            show_oedit_menu(d);
            return true;
        }

        case CONN_OEDIT_SHORT_DESC: {
            if (line[0]) {
                snprintf(d->oedit_work.short_descr, sizeof(d->oedit_work.short_descr), "%s", line);
                d->oedit_dirty = true;
            }
            show_oedit_menu(d);
            return true;
        }

        case CONN_OEDIT_LONG_DESC: {
            if (line[0]) {
                snprintf(d->oedit_work.long_descr, sizeof(d->oedit_work.long_descr), "%s", line);
                d->oedit_dirty = true;
            }
            show_oedit_menu(d);
            return true;
        }

        case CONN_OEDIT_TYPE: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= 0 && strcmp(obj_type_name((int)v), "?") != 0) {
                    d->oedit_work.type = (int)v;
                    d->oedit_dirty = true;
                    show_oedit_menu(d);
                } else {
                    descriptor_send(d, "Pick a type number from the list, or blank to cancel.\r\n");
                    show_oedit_type_picker(d);
                }
            } else {
                show_oedit_menu(d);
            }
            return true;
        }

        case CONN_OEDIT_WEIGHT: {
            if (line[0]) {
                char *end;
                double v = strtod(line, &end);
                if (end != line && v >= 0) {
                    d->oedit_work.weight = v;
                    d->oedit_dirty = true;
                } else {
                    descriptor_send(d, "Weight must be a non-negative number.\r\n");
                }
            }
            show_oedit_menu(d);
            return true;
        }

        case CONN_OEDIT_VOLUME: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= 0) {
                    d->oedit_work.volume = (int)v;
                    d->oedit_dirty = true;
                } else {
                    descriptor_send(d, "Volume must be a non-negative number.\r\n");
                }
            }
            show_oedit_menu(d);
            return true;
        }

        case CONN_OEDIT_ACTION_FLAGS: {
            if (line[0]) {
                char *end;
                long bit = strtol(line, &end, 10);
                if (end != line && bit >= 0 && bit < obj_action_flag_count()) {
                    d->oedit_work.action_flag ^= (1 << bit);
                    d->oedit_dirty = true;
                    show_oedit_action_flags(d);
                } else {
                    descriptor_send(d, "Pick a flag number, or blank to return.\r\n");
                    show_oedit_action_flags(d);
                }
            } else {
                show_oedit_menu(d);
            }
            return true;
        }

        case CONN_OEDIT_WEAR_FLAGS: {
            if (line[0]) {
                char *end;
                long bit = strtol(line, &end, 10);
                if (end != line && bit >= 0 && bit < obj_wear_flag_count()) {
                    d->oedit_work.wear_flag ^= (1 << bit);
                    d->oedit_dirty = true;
                    show_oedit_wear_flags(d);
                } else {
                    descriptor_send(d, "Pick a flag number, or blank to return.\r\n");
                    show_oedit_wear_flags(d);
                }
            } else {
                show_oedit_menu(d);
            }
            return true;
        }

        case CONN_OEDIT_ANTI_RACE_FLAGS: {
            if (line[0]) {
                char *end;
                long bit = strtol(line, &end, 10);
                if (end != line && bit >= 0 && bit < obj_anti_race_flag_count()) {
                    d->oedit_work.anti_race_flag ^= (1 << bit);
                    d->oedit_dirty = true;
                    show_oedit_anti_race_flags(d);
                } else {
                    descriptor_send(d, "Pick a flag number, or blank to return.\r\n");
                    show_oedit_anti_race_flags(d);
                }
            } else {
                show_oedit_menu(d);
            }
            return true;
        }

        case CONN_OEDIT_PRICE: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= 0) {
                    d->oedit_work.price = (int)v;
                    d->oedit_dirty = true;
                } else {
                    descriptor_send(d, "Cost must be a non-negative number.\r\n");
                }
            }
            show_oedit_menu(d);
            return true;
        }

        case CONN_OEDIT_VALUES: {
            if (line[0]) {
                int v0, v1, v2, v3;
                if (sscanf(line, "%d %d %d %d", &v0, &v1, &v2, &v3) == 4) {
                    d->oedit_work.val[0] = v0;
                    d->oedit_work.val[1] = v1;
                    d->oedit_work.val[2] = v2;
                    d->oedit_work.val[3] = v3;
                    d->oedit_dirty = true;
                } else {
                    descriptor_send(d, "Enter four numbers, e.g. \"0 0 0 0\".\r\n");
                }
            }
            show_oedit_menu(d);
            return true;
        }

        case CONN_OEDIT_DECAY: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= -1) {
                    d->oedit_work.decay_time = (int)v;
                    d->oedit_dirty = true;
                } else {
                    descriptor_send(d, "Decay time must be -1 (never) or a non-negative tick count.\r\n");
                }
            }
            show_oedit_menu(d);
            return true;
        }

        case CONN_OEDIT_MAX_STRUCT: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= 0) {
                    d->oedit_work.max_struct = (int)v;
                    d->oedit_dirty = true;
                } else {
                    descriptor_send(d, "Max struct points must be a non-negative number.\r\n");
                }
            }
            show_oedit_menu(d);
            return true;
        }

        case CONN_OEDIT_CUR_STRUCT: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= 0) {
                    d->oedit_work.cur_struct = (int)v;
                    d->oedit_dirty = true;
                } else {
                    descriptor_send(d, "Struct points must be a non-negative number.\r\n");
                }
            }
            show_oedit_menu(d);
            return true;
        }

        case CONN_OEDIT_MATERIAL: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= 0) {
                    d->oedit_work.material = (int)v;
                    d->oedit_dirty = true;
                } else {
                    descriptor_send(d, "Material must be a non-negative number.\r\n");
                }
            }
            show_oedit_menu(d);
            return true;
        }

        case CONN_OEDIT_CAN_BE_SEEN: {
            if (line[0]) {
                bool is_no = strcasecmp(line, "n") == 0 || strcasecmp(line, "no") == 0;
                bool is_yes = strcasecmp(line, "y") == 0 || strcasecmp(line, "yes") == 0;
                if (is_yes || is_no) {
                    d->oedit_work.can_be_seen = is_yes;
                    d->oedit_dirty = true;
                } else {
                    descriptor_send(d, "Please answer yes or no.\r\n");
                }
            }
            show_oedit_menu(d);
            return true;
        }

        case CONN_OEDIT_SPEC_PROC: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= 0) {
                    d->oedit_work.spec_proc = (int)v;
                    d->oedit_dirty = true;
                } else {
                    descriptor_send(d, "Special proc must be a non-negative number.\r\n");
                }
            }
            show_oedit_menu(d);
            return true;
        }

        case CONN_OEDIT_MAX_EXIST: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= 0) {
                    d->oedit_work.max_exist = (int)v;
                    d->oedit_dirty = true;
                } else {
                    descriptor_send(d, "Max exist must be 0 (uncapped) or a positive number.\r\n");
                }
            }
            show_oedit_menu(d);
            return true;
        }

        case CONN_OEDIT_QUIT_CONFIRM: {
            char c = (char)toupper((unsigned char)line[0]);
            if (c == 'S') {
                oedit_save(d);
                oedit_leave(d);
            } else if (c == 'D') {
                oedit_leave(d);
            } else {
                show_oedit_menu(d);
            }
            return true;
        }

        case CONN_MEDIT_MENU: {
            if (isdigit((unsigned char)line[0])) {
                switch (atoi(line)) {
                    case 1:
                        descriptor_send(d, "\r\nEnter new name (blank to cancel): ");
                        d->state = CONN_MEDIT_NAME;
                        break;
                    case 2:
                        descriptor_send(d, "\r\nEnter new short description (blank to cancel): ");
                        d->state = CONN_MEDIT_SHORT_DESC;
                        break;
                    case 3:
                        descriptor_send(d, "\r\nEnter new long description (blank to cancel): ");
                        d->state = CONN_MEDIT_LONG_DESC;
                        break;
                    case 4:
                        descriptor_send(d, "\r\nEnter new description (blank to cancel): ");
                        d->state = CONN_MEDIT_DESCRIPTION;
                        break;
                    case 5:
                        descriptor_send(d, "\r\nEnter new action flags bitmask (blank to cancel): ");
                        d->state = CONN_MEDIT_ACTIONS;
                        break;
                    case 6:
                        descriptor_send(d, "\r\nEnter new affect flags bitmask (blank to cancel): ");
                        d->state = CONN_MEDIT_AFFECTS;
                        break;
                    case 7:
                        descriptor_send(d, "\r\nEnter new number of attacks, e.g. 1.0 (blank to cancel): ");
                        d->state = CONN_MEDIT_ATTACKS;
                        break;
                    case 8:
                        descriptor_send(d, "\r\nEnter new level (blank to cancel): ");
                        d->state = CONN_MEDIT_LEVEL;
                        break;
                    case 9:
                        descriptor_send(d, "\r\nEnter new hitroll (blank to cancel): ");
                        d->state = CONN_MEDIT_HITROLL;
                        break;
                    case 10:
                        descriptor_send(d, "\r\nEnter new armor level (blank to cancel): ");
                        d->state = CONN_MEDIT_ARMOR;
                        break;
                    case 11:
                        descriptor_send(d, "\r\nEnter new HP level (blank to cancel): ");
                        d->state = CONN_MEDIT_HPLEVEL;
                        break;
                    case 12:
                        descriptor_send(d, "\r\nEnter damage level and precision, e.g. \"2.5 3\" (blank to cancel): ");
                        d->state = CONN_MEDIT_DAMAGE;
                        break;
                    case 13:
                        descriptor_send(d, "\r\nEnter new gold (blank to cancel): ");
                        d->state = CONN_MEDIT_GOLD;
                        break;
                    case 14:
                        descriptor_send(d, "\r\nEnter new race number (blank to cancel): ");
                        d->state = CONN_MEDIT_RACE;
                        break;
                    case 15:
                        descriptor_send(d, "\r\nEnter new sex, e.g. \"male\" (or 0 neuter, 1 male, 2 female; blank to cancel): ");
                        d->state = CONN_MEDIT_SEX;
                        break;
                    case 16:
                        descriptor_send(d, "\r\nEnter new max exist, 0 for uncapped (blank to cancel): ");
                        d->state = CONN_MEDIT_MAX_EXIST;
                        break;
                    case 17:
                        descriptor_send(d, "\r\nEnter new default position, e.g. \"standing\" (blank to cancel): ");
                        d->state = CONN_MEDIT_DEF_POSITION;
                        break;
                    case 18:
                        descriptor_send(d, "\r\nEnter new class bitmask (blank to cancel): ");
                        d->state = CONN_MEDIT_CLASS;
                        break;
                    case 19:
                        descriptor_send(d, "\r\nEnter height and weight, e.g. \"72 180\" (blank to cancel): ");
                        d->state = CONN_MEDIT_SIZE;
                        break;
                    case 20:
                        descriptor_send(d, "\r\nEnter new vision bonus (blank to cancel): ");
                        d->state = CONN_MEDIT_VISION;
                        break;
                    case 21:
                        descriptor_send(d, "\r\nCan be seen? yes or no (blank to cancel): ");
                        d->state = CONN_MEDIT_CAN_BE_SEEN;
                        break;
                    case 22:
                        descriptor_send(d, "\r\nEnter new skin type number (blank to cancel): ");
                        d->state = CONN_MEDIT_SKIN;
                        break;
                    case 23:
                        descriptor_send(d, "\r\nEnter new alignment, -1 evil, 0 unaligned, 1 good (blank to cancel): ");
                        d->state = CONN_MEDIT_ALIGN;
                        break;
                    default:
                        descriptor_send(d, "Pick a menu number (1-23), or S/Q.\r\n");
                        show_medit_menu(d);
                        break;
                }
                return true;
            }
            switch ((char)toupper((unsigned char)line[0])) {
                case 'S':
                    medit_save(d);
                    show_medit_menu(d);
                    break;
                case 'Q':
                    if (d->medit_dirty) {
                        descriptor_send(d,
                            "\r\nYou have unsaved changes. (S)ave, (D)iscard, (C)ancel: ");
                        d->state = CONN_MEDIT_QUIT_CONFIRM;
                    } else {
                        medit_leave(d);
                    }
                    break;
                default:
                    descriptor_send(d, "Pick a menu number (1-23), or S/Q.\r\n");
                    show_medit_menu(d);
                    break;
            }
            return true;
        }

        case CONN_MEDIT_NAME: {
            if (line[0]) {
                snprintf(d->medit_work.name, sizeof(d->medit_work.name), "%s", line);
                d->medit_dirty = true;
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_SHORT_DESC: {
            if (line[0]) {
                snprintf(d->medit_work.short_descr, sizeof(d->medit_work.short_descr), "%s", line);
                d->medit_dirty = true;
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_LONG_DESC: {
            if (line[0]) {
                snprintf(d->medit_work.long_descr, sizeof(d->medit_work.long_descr), "%s", line);
                d->medit_dirty = true;
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_DESCRIPTION: {
            if (line[0]) {
                snprintf(d->medit_work.description, sizeof(d->medit_work.description), "%s", line);
                d->medit_dirty = true;
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_ACTIONS: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= 0) {
                    d->medit_work.actions = (int)v;
                    d->medit_dirty = true;
                } else {
                    descriptor_send(d, "Action flags must be a non-negative number.\r\n");
                }
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_AFFECTS: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= 0) {
                    d->medit_work.affects = (int)v;
                    d->medit_dirty = true;
                } else {
                    descriptor_send(d, "Affect flags must be a non-negative number.\r\n");
                }
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_ATTACKS: {
            if (line[0]) {
                char *end;
                double v = strtod(line, &end);
                if (end != line && v >= 0) {
                    d->medit_work.attacks = v;
                    d->medit_dirty = true;
                } else {
                    descriptor_send(d, "Attacks must be a non-negative number.\r\n");
                }
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_LEVEL: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= 0) {
                    d->medit_work.level = (int)v;
                    d->medit_dirty = true;
                } else {
                    descriptor_send(d, "Level must be a non-negative number.\r\n");
                }
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_HITROLL: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line) {
                    d->medit_work.tohit = (int)v;
                    d->medit_dirty = true;
                } else {
                    descriptor_send(d, "Hitroll must be a number.\r\n");
                }
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_ARMOR: {
            if (line[0]) {
                char *end;
                double v = strtod(line, &end);
                if (end != line) {
                    d->medit_work.ac = v;
                    d->medit_dirty = true;
                } else {
                    descriptor_send(d, "Armor level must be a number.\r\n");
                }
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_HPLEVEL: {
            if (line[0]) {
                char *end;
                double v = strtod(line, &end);
                if (end != line) {
                    d->medit_work.hpbonus = v;
                    d->medit_dirty = true;
                } else {
                    descriptor_send(d, "HP level must be a number.\r\n");
                }
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_DAMAGE: {
            if (line[0]) {
                double lvl;
                int prec;
                if (sscanf(line, "%lf %d", &lvl, &prec) == 2) {
                    d->medit_work.damage_level = lvl;
                    d->medit_work.damage_precision = prec;
                    d->medit_dirty = true;
                } else {
                    descriptor_send(d, "Enter two numbers, e.g. \"2.5 3\".\r\n");
                }
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_GOLD: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= 0) {
                    d->medit_work.gold = (int)v;
                    d->medit_dirty = true;
                } else {
                    descriptor_send(d, "Gold must be a non-negative number.\r\n");
                }
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_RACE: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= 0) {
                    d->medit_work.race = (int)v;
                    d->medit_dirty = true;
                } else {
                    descriptor_send(d, "Race must be a non-negative number.\r\n");
                }
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_SEX: {
            if (line[0]) {
                bool matched = true;
                if (strcasecmp(line, "neuter") == 0) d->medit_work.sex = 0;
                else if (strcasecmp(line, "male") == 0) d->medit_work.sex = 1;
                else if (strcasecmp(line, "female") == 0) d->medit_work.sex = 2;
                else {
                    char *end;
                    long v = strtol(line, &end, 10);
                    if (end != line && v >= 0 && v <= 2)
                        d->medit_work.sex = (int)v;
                    else
                        matched = false;
                }
                if (matched)
                    d->medit_dirty = true;
                else
                    descriptor_send(d, "Sex must be \"neuter\", \"male\", \"female\" (or 0/1/2).\r\n");
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_MAX_EXIST: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= 0) {
                    d->medit_work.max_exist = (int)v;
                    d->medit_dirty = true;
                } else {
                    descriptor_send(d, "Max exist must be 0 (uncapped) or a positive number.\r\n");
                }
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_DEF_POSITION: {
            if (line[0]) {
                position_t pos;
                if (position_from_name(line, &pos)) {
                    d->medit_work.def_position = (int)pos;
                    d->medit_dirty = true;
                } else {
                    descriptor_send(d, "Not a recognized position -- try \"standing\", \"sitting\", etc.\r\n");
                }
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_CLASS: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= 0) {
                    d->medit_work.class_mask = (int)v;
                    d->medit_dirty = true;
                } else {
                    descriptor_send(d, "Class bitmask must be a non-negative number.\r\n");
                }
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_SIZE: {
            if (line[0]) {
                int h, w;
                if (sscanf(line, "%d %d", &h, &w) == 2 && h >= 0 && w >= 0) {
                    d->medit_work.height = h;
                    d->medit_work.weight = w;
                    d->medit_dirty = true;
                } else {
                    descriptor_send(d, "Enter two non-negative numbers, e.g. \"72 180\".\r\n");
                }
            }
            show_medit_menu(d);
            return true;
        }


        case CONN_MEDIT_VISION: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line) {
                    d->medit_work.vision = (int)v;
                    d->medit_dirty = true;
                } else {
                    descriptor_send(d, "Vision must be a number.\r\n");
                }
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_CAN_BE_SEEN: {
            if (line[0]) {
                bool is_no = strcasecmp(line, "n") == 0 || strcasecmp(line, "no") == 0;
                bool is_yes = strcasecmp(line, "y") == 0 || strcasecmp(line, "yes") == 0;
                if (is_yes || is_no) {
                    d->medit_work.can_be_seen = is_yes;
                    d->medit_dirty = true;
                } else {
                    descriptor_send(d, "Please answer yes or no.\r\n");
                }
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_SKIN: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= 0) {
                    d->medit_work.skin = (int)v;
                    d->medit_dirty = true;
                } else {
                    descriptor_send(d, "Skin type must be a non-negative number.\r\n");
                }
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_ALIGN: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line && v >= -1 && v <= 1) {
                    d->medit_work.align = (int)v;
                    d->medit_dirty = true;
                } else {
                    descriptor_send(d, "Alignment must be -1 (evil), 0 (unaligned), or 1 (good).\r\n");
                }
            }
            show_medit_menu(d);
            return true;
        }

        case CONN_MEDIT_QUIT_CONFIRM: {
            char c = (char)toupper((unsigned char)line[0]);
            if (c == 'S') {
                medit_save(d);
                medit_leave(d);
            } else if (c == 'D') {
                medit_leave(d);
            } else {
                show_medit_menu(d);
            }
            return true;
        }

        case CONN_EDSUIT_LIST: {
            if (!line[0]) {
                descriptor_send(d, "Done editing that suit -- your changes are saved.\r\n");
                d->state = CONN_PLAYING;
                return true;
            }
            if (isdigit((unsigned char)line[0])) {
                int vnums[SUIT_MAX_ITEMS], qtys[SUIT_MAX_ITEMS];
                int n = suit_repo_load_items_qty(d->edsuit_id, vnums, qtys, SUIT_MAX_ITEMS);
                int idx = atoi(line) - 1;
                if (idx < 0 || idx >= n) {
                    descriptor_send(d, "Pick an item number from the list, A, C, R, D, X, or blank.\r\n");
                    show_edsuit_list(d);
                    return true;
                }
                d->edsuit_item_vnum = vnums[idx];
                show_edsuit_item(d);
                return true;
            }
            switch ((char)toupper((unsigned char)line[0])) {
                case 'A':
                    descriptor_send(d, "\r\nEnter the obj vnum to add (blank to cancel): ");
                    d->state = CONN_EDSUIT_ADD_VNUM;
                    break;
                case 'C':
                    descriptor_send(d,
                        "\r\nEnter a class number (0=Mage 1=Cleric 2=Warrior 3=Thief 4=Druid "
                        "5=Monk), or \"any\" to clear (blank to cancel): ");
                    d->state = CONN_EDSUIT_CLASS;
                    break;
                case 'R':
                    descriptor_send(d,
                        "\r\nEnter a race number (0=Human 1=Elf 2=Ogre 3=Dwarf 4=Hobbit "
                        "5=Gnome), or \"any\" to clear (blank to cancel): ");
                    d->state = CONN_EDSUIT_RACE;
                    break;
                case 'D':
                    descriptor_send(d, "\r\nEnter new description (blank to cancel): ");
                    d->state = CONN_EDSUIT_DESC;
                    break;
                case 'X':
                    descriptor_send(d, "Delete this ENTIRE suit? This cannot be undone. (yes/no): ");
                    d->state = CONN_EDSUIT_DELETE_CONFIRM;
                    break;
                default:
                    descriptor_send(d, "Pick an item number from the list, A, C, R, D, X, or blank.\r\n");
                    show_edsuit_list(d);
                    break;
            }
            return true;
        }

        case CONN_EDSUIT_ITEM: {
            if (!line[0]) {
                show_edsuit_list(d);
                return true;
            }
            if (line[1] == '\0' && line[0] == '1') {
                descriptor_send(d, "\r\nEnter new quantity, 1 or more (blank to cancel): ");
                d->state = CONN_EDSUIT_ITEM_QTY;
                return true;
            }
            if (toupper((unsigned char)line[0]) == 'D' && line[1] == '\0') {
                descriptor_send(d, "Delete this item from the suit? (yes/no): ");
                d->state = CONN_EDSUIT_ITEM_DELETE_CONFIRM;
                return true;
            }
            descriptor_send(d, "Pick 1, D, or blank to go back.\r\n");
            show_edsuit_item(d);
            return true;
        }

        case CONN_EDSUIT_ITEM_QTY: {
            if (line[0]) {
                char *end;
                long qty = strtol(line, &end, 10);
                if (end != line && qty >= 1) {
                    suit_repo_set_item_qty(d->edsuit_id, d->edsuit_item_vnum, (int)qty);
                } else {
                    descriptor_send(d, "Quantity must be a whole number, 1 or more.\r\n");
                }
            }
            show_edsuit_item(d);
            return true;
        }

        case CONN_EDSUIT_ITEM_DELETE_CONFIRM: {
            if (strcasecmp(line, "yes") == 0) {
                suit_repo_delete_item(d->edsuit_id, d->edsuit_item_vnum);
                descriptor_send(d, "Item removed from the suit.\r\n");
                show_edsuit_list(d);
            } else {
                descriptor_send(d, "Cancelled.\r\n");
                show_edsuit_item(d);
            }
            return true;
        }

        case CONN_EDSUIT_DELETE_CONFIRM: {
            if (strcasecmp(line, "yes") == 0) {
                suit_repo_delete(d->edsuit_id);
                descriptor_send(d, "Suit deleted.\r\n");
                d->state = CONN_PLAYING;
            } else {
                descriptor_send(d, "Cancelled.\r\n");
                show_edsuit_list(d);
            }
            return true;
        }

        case CONN_EDSUIT_ADD_VNUM: {
            if (!line[0]) {
                show_edsuit_list(d);
                return true;
            }
            char *end;
            long vnum = strtol(line, &end, 10);
            obj_proto_t proto;
            if (end == line || vnum <= 0 || !obj_proto_load((int)vnum, &proto)) {
                descriptor_send(d, "Not a recognized obj vnum -- check `stat obj`/`vnum obj` first.\r\n");
                show_edsuit_list(d);
                return true;
            }
            d->edsuit_add_vnum = (int)vnum;
            char msg[320];
            snprintf(msg, sizeof(msg), "\r\nAdding %s (vnum %ld) -- enter quantity, blank for 1: ",
                     proto.short_descr, vnum);
            descriptor_send(d, msg);
            d->state = CONN_EDSUIT_ADD_QTY;
            return true;
        }

        case CONN_EDSUIT_ADD_QTY: {
            int qty = 1;
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end == line || v < 1) {
                    descriptor_send(d, "Quantity must be a whole number, 1 or more -- item not added.\r\n");
                    show_edsuit_list(d);
                    return true;
                }
                qty = (int)v;
            }
            if (suit_repo_add_item(d->edsuit_id, d->edsuit_add_vnum, qty))
                descriptor_send(d, "Item added.\r\n");
            else
                descriptor_send(d, "Failed to add that item -- check the vnum is real.\r\n");
            show_edsuit_list(d);
            return true;
        }

        case CONN_EDSUIT_CLASS: {
            if (!line[0]) {
                show_edsuit_list(d);
                return true;
            }
            if (strcasecmp(line, "any") == 0) {
                suit_repo_set_class(d->edsuit_id, -1);
            } else {
                char *end;
                long c = strtol(line, &end, 10);
                if (end == line || c < 0 || c >= CLASS_COUNT) {
                    descriptor_send(d, "Not a recognized class number.\r\n");
                    show_edsuit_list(d);
                    return true;
                }
                suit_repo_set_class(d->edsuit_id, (int)c);
            }
            show_edsuit_list(d);
            return true;
        }

        case CONN_EDSUIT_RACE: {
            if (!line[0]) {
                show_edsuit_list(d);
                return true;
            }
            if (strcasecmp(line, "any") == 0) {
                suit_repo_set_race(d->edsuit_id, -1);
            } else {
                char *end;
                long r = strtol(line, &end, 10);
                if (end == line || r < 0 || r >= RACE_COUNT) {
                    descriptor_send(d, "Not a recognized race number.\r\n");
                    show_edsuit_list(d);
                    return true;
                }
                suit_repo_set_race(d->edsuit_id, (int)r);
            }
            show_edsuit_list(d);
            return true;
        }

        case CONN_EDSUIT_DESC: {
            if (line[0])
                suit_repo_set_description(d->edsuit_id, line);
            show_edsuit_list(d);
            return true;
        }

        case CONN_BALANCE_MENU: {
            if (isdigit((unsigned char)line[0])) {
                switch (atoi(line)) {
                    case 1:
                        descriptor_send(d, "\r\nEnter new HP multiplier, e.g. 1.25 (blank to cancel): ");
                        d->state = CONN_BALANCE_HP_MULT;
                        break;
                    case 2:
                        descriptor_send(d, "\r\nEnter new damage multiplier, e.g. 0.9 (blank to cancel): ");
                        d->state = CONN_BALANCE_DMG_MULT;
                        break;
                    case 3:
                        descriptor_send(d, "\r\nEnter new to-hit modifier, e.g. -5 or 10 (blank to cancel): ");
                        d->state = CONN_BALANCE_TOHIT_MOD;
                        break;
                    case 4:
                        descriptor_send(d, "\r\nEnter new AC modifier, e.g. -5 or 10 (blank to cancel): ");
                        d->state = CONN_BALANCE_AC_MOD;
                        break;
                    default:
                        descriptor_send(d, "Pick a menu number (1-4), or S/Q.\r\n");
                        show_balance_menu(d);
                        break;
                }
                return true;
            }
            switch ((char)toupper((unsigned char)line[0])) {
                case 'S':
                    balance_save(d);
                    show_balance_menu(d);
                    break;
                case 'Q':
                    if (d->balance_dirty) {
                        descriptor_send(d,
                            "\r\nYou have unsaved changes. (S)ave, (D)iscard, (C)ancel: ");
                        d->state = CONN_BALANCE_QUIT_CONFIRM;
                    } else {
                        balance_leave(d);
                    }
                    break;
                default:
                    descriptor_send(d, "Pick a menu number (1-4), or S/Q.\r\n");
                    show_balance_menu(d);
                    break;
            }
            return true;
        }

        case CONN_BALANCE_HP_MULT: {
            if (line[0]) {
                char *end;
                double v = strtod(line, &end);
                if (end != line && v > 0) {
                    d->balance_work.hp_mult = (float)v;
                    d->balance_dirty = true;
                } else {
                    descriptor_send(d, "HP multiplier must be a positive number.\r\n");
                }
            }
            show_balance_menu(d);
            return true;
        }

        case CONN_BALANCE_DMG_MULT: {
            if (line[0]) {
                char *end;
                double v = strtod(line, &end);
                if (end != line && v > 0) {
                    d->balance_work.dmg_mult = (float)v;
                    d->balance_dirty = true;
                } else {
                    descriptor_send(d, "Damage multiplier must be a positive number.\r\n");
                }
            }
            show_balance_menu(d);
            return true;
        }

        case CONN_BALANCE_TOHIT_MOD: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line) {
                    d->balance_work.tohit_mod = (int)v;
                    d->balance_dirty = true;
                } else {
                    descriptor_send(d, "To-hit modifier must be a whole number.\r\n");
                }
            }
            show_balance_menu(d);
            return true;
        }

        case CONN_BALANCE_AC_MOD: {
            if (line[0]) {
                char *end;
                long v = strtol(line, &end, 10);
                if (end != line) {
                    d->balance_work.ac_mod = (int)v;
                    d->balance_dirty = true;
                } else {
                    descriptor_send(d, "AC modifier must be a whole number.\r\n");
                }
            }
            show_balance_menu(d);
            return true;
        }

        case CONN_BALANCE_QUIT_CONFIRM: {
            char c = (char)toupper((unsigned char)line[0]);
            if (c == 'S') {
                balance_save(d);
                balance_leave(d);
            } else if (c == 'D') {
                balance_leave(d);
            } else {
                show_balance_menu(d);
            }
            return true;
        }

        case CONN_EDACCOUNT_MENU: {
            if (isdigit((unsigned char)line[0])) {
                switch (atoi(line)) {
                    case 1:
                        descriptor_send(d, "\r\nEnter new account name (blank to cancel): ");
                        d->state = CONN_EDACCOUNT_RENAME;
                        break;
                    case 2:
                        descriptor_send(d, "\r\nEnter new password (blank to cancel): ");
                        d->state = CONN_EDACCOUNT_PASSWORD;
                        break;
                    default:
                        descriptor_send(d, "Pick a menu number (1-2), or Q.\r\n");
                        show_edaccount_menu(d);
                        break;
                }
                return true;
            }
            if (toupper((unsigned char)line[0]) == 'Q') {
                edaccount_leave(d);
            } else {
                descriptor_send(d, "Pick a menu number (1-2), or Q.\r\n");
                show_edaccount_menu(d);
            }
            return true;
        }

        case CONN_EDACCOUNT_RENAME: {
            if (line[0]) {
                if (account_set_name(d->edaccount_id, line))
                    descriptor_send(d, "Account renamed.\r\n");
                else
                    descriptor_send(d, "Rename failed -- that name may already be taken.\r\n");
            }
            show_edaccount_menu(d);
            return true;
        }

        case CONN_EDACCOUNT_PASSWORD: {
            if (line[0]) {
                if (strlen(line) < 3) {
                    descriptor_send(d, "Password must be at least 3 characters.\r\n");
                } else if (account_set_password(d->edaccount_id, line)) {
                    descriptor_send(d, "Password reset.\r\n");
                } else {
                    descriptor_send(d, "Password reset failed.\r\n");
                }
            }
            show_edaccount_menu(d);
            return true;
        }

        case CONN_EDSOCIAL_LIST: {
            if (!line[0]) {
                edsocial_leave(d);
                return true;
            }
            if (strcasecmp(line, "new") == 0) {
                descriptor_send(d, "\r\nEnter the new social's name (blank to cancel): ");
                d->state = CONN_EDSOCIAL_NEW_NAME;
                return true;
            }
            social_t s;
            if (social_repo_get(line, &s)) {
                snprintf(d->edsocial_name, sizeof(d->edsocial_name), "%s", s.name);
                show_edsocial_item(d);
            } else {
                char msg[80];
                snprintf(msg, sizeof(msg), "No social named '%.32s'.\r\n", line);
                descriptor_send(d, msg);
                show_edsocial_list(d);
            }
            return true;
        }

        case CONN_EDSOCIAL_NEW_NAME: {
            if (!line[0]) {
                descriptor_send(d, "Cancelled.\r\n");
                show_edsocial_list(d);
                return true;
            }
            if (strlen(line) >= SOCIAL_NAME_LEN) {
                descriptor_send(d, "That name is too long.\r\n");
                show_edsocial_list(d);
                return true;
            }
            social_t existing;
            if (social_repo_get(line, &existing)) {
                descriptor_send(d, "A social by that name already exists.\r\n");
                show_edsocial_list(d);
                return true;
            }
            social_t s;
            memset(&s, 0, sizeof(s));
            snprintf(s.name, sizeof(s.name), "%s", line);
            if (social_repo_save(&s)) {
                social_cache_load();
                snprintf(d->edsocial_name, sizeof(d->edsocial_name), "%s", s.name);
                descriptor_send(d, "Social created -- fill in its fields below.\r\n");
                show_edsocial_item(d);
            } else {
                descriptor_send(d, "Create failed.\r\n");
                show_edsocial_list(d);
            }
            return true;
        }

        case CONN_EDSOCIAL_ITEM: {
            if (!line[0]) {
                show_edsocial_list(d);
                return true;
            }
            if (line[1] == '\0' && isdigit((unsigned char)line[0])) {
                int field = line[0] - '0';
                if (field >= 1 && field <= EDSOCIAL_FIELD_COUNT) {
                    social_t s;
                    if (!social_repo_get(d->edsocial_name, &s)) {
                        descriptor_send(d, "That social no longer exists.\r\n");
                        show_edsocial_list(d);
                        return true;
                    }
                    char msg[SOCIAL_TEXT_LEN + 96];
                    snprintf(msg, sizeof(msg),
                        "\r\nCurrent (%s): %s\r\nEnter new text (blank to cancel):\r\n> ",
                        EDSOCIAL_FIELD_LABELS[field - 1], edsocial_field_ptr(&s, field));
                    descriptor_send(d, msg);
                    d->edsocial_field = field;
                    d->state = CONN_EDSOCIAL_FIELD;
                    return true;
                }
            }
            char c = (char)toupper((unsigned char)line[0]);
            if (line[1] == '\0' && c == 'H') {
                social_t s;
                if (social_repo_get(d->edsocial_name, &s)) {
                    s.hide = !s.hide;
                    social_repo_save(&s);
                    social_cache_load();
                }
                show_edsocial_item(d);
                return true;
            }
            if (line[1] == '\0' && c == 'P') {
                social_t s;
                if (social_repo_get(d->edsocial_name, &s)) {
                    char msg[112];
                    snprintf(msg, sizeof(msg),
                        "\r\nCurrent minimum position: %s\r\nEnter a new one (e.g. standing, "
                        "sleeping), blank to cancel:\r\n> ",
                        position_name((position_t)s.min_position));
                    descriptor_send(d, msg);
                    d->edsocial_field = 0;
                    d->state = CONN_EDSOCIAL_FIELD;
                } else {
                    descriptor_send(d, "That social no longer exists.\r\n");
                    show_edsocial_list(d);
                }
                return true;
            }
            if (line[1] == '\0' && c == 'R') {
                char msg[SOCIAL_NAME_LEN + 64];
                snprintf(msg, sizeof(msg),
                    "\r\nCurrent name: %s\r\nEnter new name (blank to cancel): ", d->edsocial_name);
                descriptor_send(d, msg);
                d->state = CONN_EDSOCIAL_RENAME;
                return true;
            }
            if (line[1] == '\0' && c == 'D') {
                char msg[SOCIAL_NAME_LEN + 64];
                snprintf(msg, sizeof(msg),
                    "\r\nReally delete the social \"%s\"? (yes/no): ", d->edsocial_name);
                descriptor_send(d, msg);
                d->state = CONN_EDSOCIAL_DELETE_CONFIRM;
                return true;
            }
            descriptor_send(d, "Pick 1-8, H, P, R, D, or blank to return.\r\n");
            show_edsocial_item(d);
            return true;
        }

        case CONN_EDSOCIAL_FIELD: {
            if (!line[0]) {
                descriptor_send(d, "Cancelled.\r\n");
                show_edsocial_item(d);
                return true;
            }
            social_t s;
            if (!social_repo_get(d->edsocial_name, &s)) {
                descriptor_send(d, "That social no longer exists.\r\n");
                show_edsocial_list(d);
                return true;
            }
            if (d->edsocial_field == 0) {
                position_t pos;
                if (!position_from_name(line, &pos)) {
                    descriptor_send(d, "Not a recognized (or ambiguous) position name.\r\n");
                    show_edsocial_item(d);
                    return true;
                }
                s.min_position = (int)pos;
            } else {
                char *field = edsocial_field_ptr(&s, d->edsocial_field);
                snprintf(field, SOCIAL_TEXT_LEN, "%s", line);
            }
            if (social_repo_save(&s)) {
                social_cache_load();
                descriptor_send(d, "Saved.\r\n");
            } else {
                descriptor_send(d, "Save failed.\r\n");
            }
            show_edsocial_item(d);
            return true;
        }

        case CONN_EDSOCIAL_RENAME: {
            if (!line[0]) {
                descriptor_send(d, "Cancelled.\r\n");
                show_edsocial_item(d);
                return true;
            }
            if (strlen(line) >= SOCIAL_NAME_LEN) {
                descriptor_send(d, "That name is too long.\r\n");
                show_edsocial_item(d);
                return true;
            }
            if (social_repo_rename(d->edsocial_name, line)) {
                snprintf(d->edsocial_name, sizeof(d->edsocial_name), "%s", line);
                social_cache_load();
                descriptor_send(d, "Renamed.\r\n");
            } else {
                descriptor_send(d, "Rename failed -- that name may already be used.\r\n");
            }
            show_edsocial_item(d);
            return true;
        }

        case CONN_EDSOCIAL_DELETE_CONFIRM: {
            if (strcasecmp(line, "yes") == 0) {
                social_repo_delete(d->edsocial_name);
                social_cache_load();
                descriptor_send(d, "Social deleted.\r\n");
            } else {
                descriptor_send(d, "Delete cancelled.\r\n");
            }
            show_edsocial_list(d);
            return true;
        }

        case CONN_TRIGEDIT_LIST: {
            if (!line[0]) {
                d->state = CONN_PLAYING;
                descriptor_send(d, "Leaving the trigger editor.\r\n");
                descriptor_editor_exit_notice(d);
                return true;
            }
            if (strcasecmp(line, "a") == 0) {
                char msg[160];
                snprintf(msg, sizeof(msg),
                    "\r\nTrigger types for %s: %s\r\nEnter type (blank to cancel): ",
                    d->trigedit_target_type, trigedit_valid_types(d->trigedit_target_type));
                descriptor_send(d, msg);
                d->state = CONN_TRIGEDIT_NEW_TYPE;
                return true;
            }
            char *end;
            long idx = strtol(line, &end, 10);
            if (end != line && *end == '\0' && idx >= 1) {
                trigger_t trigs[32];
                int n = trigger_repo_list_for(d->trigedit_target_type, d->trigedit_target_vnum, trigs, 32);
                if (idx <= n) {
                    d->trig_edit_id = trigs[idx - 1].id;
                    show_trigedit_item(d);
                    return true;
                }
            }
            descriptor_send(d, "Pick a trigger number, A to add, or blank to quit.\r\n");
            show_trigedit_list(d);
            return true;
        }

        case CONN_TRIGEDIT_ITEM: {
            if (!line[0]) {
                show_trigedit_list(d);
                return true;
            }
            char c = (char)toupper((unsigned char)line[0]);
            if (line[1] == '\0' && c == '1') {
                trigger_t t;
                if (!trigger_repo_get(d->trig_edit_id, &t)) {
                    descriptor_send(d, "That trigger no longer exists.\r\n");
                    show_trigedit_list(d);
                    return true;
                }
                char msg[TRIGGER_MATCH_LEN + 96];
                snprintf(msg, sizeof(msg),
                    "\r\nCurrent: %s\r\nEnter new match text (blank to cancel): ",
                    t.match_text[0] ? t.match_text : "(none)");
                descriptor_send(d, msg);
                d->state = CONN_TRIGEDIT_MATCH;
                return true;
            }
            if (line[1] == '\0' && c == '2') {
                trigger_t t;
                if (!trigger_repo_get(d->trig_edit_id, &t)) {
                    descriptor_send(d, "That trigger no longer exists.\r\n");
                    show_trigedit_list(d);
                    return true;
                }
                char msg[96];
                snprintf(msg, sizeof(msg),
                    "\r\nCurrent: %d%%\r\nEnter new chance percent, 1-100 (blank to cancel): ",
                    t.chance_pct);
                descriptor_send(d, msg);
                d->state = CONN_TRIGEDIT_CHANCE;
                return true;
            }
            if (line[1] == '\0' && c == '3') {
                trigger_t t;
                if (!trigger_repo_get(d->trig_edit_id, &t)) {
                    descriptor_send(d, "That trigger no longer exists.\r\n");
                    show_trigedit_list(d);
                    return true;
                }
                snprintf(d->trig_target_type, sizeof(d->trig_target_type), "%s", t.target_type);
                d->trig_target_vnum = t.target_vnum;
                snprintf(d->trig_trigger_type, sizeof(d->trig_trigger_type), "%s", t.trigger_type);
                snprintf(d->trig_match_text, sizeof(d->trig_match_text), "%s", t.match_text);
                d->trig_chance_pct = t.chance_pct;
                d->trig_edit_id = t.id; /* > 0 -- save handler updates, not inserts */
                trigedit_arm_script_editor(d, t.script);
                return true;
            }
            if (line[1] == '\0' && c == 'D') {
                char msg[64];
                snprintf(msg, sizeof(msg), "\r\nReally delete trigger #%ld? (yes/no): ", d->trig_edit_id);
                descriptor_send(d, msg);
                d->state = CONN_TRIGEDIT_DELETE_CONFIRM;
                return true;
            }
            descriptor_send(d, "Pick 1, 2, 3, D, or blank to return.\r\n");
            show_trigedit_item(d);
            return true;
        }

        case CONN_TRIGEDIT_MATCH: {
            if (line[0]) {
                descriptor_send(d, trigger_repo_update_match(d->trig_edit_id, line)
                    ? "Match text saved.\r\n" : "Saving the match text failed.\r\n");
            }
            show_trigedit_item(d);
            return true;
        }

        case CONN_TRIGEDIT_CHANCE: {
            if (line[0]) {
                char *end;
                long pct = strtol(line, &end, 10);
                if (end != line && *end == '\0' && pct >= 1 && pct <= 100) {
                    descriptor_send(d, trigger_repo_update_chance(d->trig_edit_id, (int)pct)
                        ? "Chance percent saved.\r\n" : "Saving the chance percent failed.\r\n");
                } else {
                    descriptor_send(d, "Chance percent must be a number from 1 to 100.\r\n");
                }
            }
            show_trigedit_item(d);
            return true;
        }

        case CONN_TRIGEDIT_DELETE_CONFIRM: {
            if (strcasecmp(line, "yes") == 0) {
                if (trigger_repo_delete(d->trig_edit_id))
                    descriptor_send(d, "Trigger deleted.\r\n");
                else
                    descriptor_send(d, "Delete failed -- it may already be gone.\r\n");
                show_trigedit_list(d);
            } else {
                descriptor_send(d, "Cancelled.\r\n");
                show_trigedit_item(d);
            }
            return true;
        }

        case CONN_TRIGEDIT_NEW_TYPE: {
            if (!line[0]) {
                descriptor_send(d, "Cancelled.\r\n");
                show_trigedit_list(d);
                return true;
            }
            char type[16];
            snprintf(type, sizeof(type), "%.15s", line);
            for (char *p = type; *p; p++)
                *p = (char)tolower((unsigned char)*p);
            if (!trigedit_type_valid(d->trigedit_target_type, type)) {
                char msg[160];
                snprintf(msg, sizeof(msg), "Not a valid type for %s. Valid: %s\r\n",
                         d->trigedit_target_type, trigedit_valid_types(d->trigedit_target_type));
                descriptor_send(d, msg);
                show_trigedit_list(d);
                return true;
            }
            snprintf(d->trig_target_type, sizeof(d->trig_target_type), "%s", d->trigedit_target_type);
            d->trig_target_vnum = d->trigedit_target_vnum;
            snprintf(d->trig_trigger_type, sizeof(d->trig_trigger_type), "%s", type);
            d->trig_match_text[0] = '\0';
            d->trig_chance_pct = 25; /* default, matches the old TRIGGER_DEFAULT_CHANCE_PCT */
            d->trig_edit_id = 0;     /* 0 -- save handler inserts a new row */

            if (strcasecmp(type, "speech") == 0) {
                descriptor_send(d, "\r\nEnter the speech keyword to match (required): ");
                d->state = CONN_TRIGEDIT_NEW_MATCH;
            } else if (strcasecmp(type, "random") == 0) {
                descriptor_send(d, "\r\nEnter the percent chance per tick, 1-100 (blank for 25): ");
                d->state = CONN_TRIGEDIT_NEW_CHANCE;
            } else {
                trigedit_arm_script_editor(d, NULL);
            }
            return true;
        }

        case CONN_TRIGEDIT_NEW_MATCH: {
            if (!line[0]) {
                descriptor_send(d, "A speech trigger needs a keyword. Cancelled.\r\n");
                show_trigedit_list(d);
                return true;
            }
            snprintf(d->trig_match_text, sizeof(d->trig_match_text), "%s", line);
            trigedit_arm_script_editor(d, NULL);
            return true;
        }

        case CONN_TRIGEDIT_NEW_CHANCE: {
            if (line[0]) {
                char *end;
                long pct = strtol(line, &end, 10);
                if (end == line || *end != '\0' || pct < 1 || pct > 100) {
                    descriptor_send(d, "Chance percent must be a number from 1 to 100. Cancelled.\r\n");
                    show_trigedit_list(d);
                    return true;
                }
                d->trig_chance_pct = (int)pct;
            }
            trigedit_arm_script_editor(d, NULL);
            return true;
        }

        case CONN_TRIGEDIT_SCRIPT: {
            editor_action_t act = editor_feed(d, line);
            if (act == EDITOR_SAVE) {
                const char *who = d->character ? d->character->base.name : "";
                bool ok = d->trig_edit_id > 0
                    ? trigger_repo_update_script(d->trig_edit_id, d->edit_buf)
                    : trigger_repo_add(who, d->trig_target_type, d->trig_target_vnum,
                                      d->trig_trigger_type, d->trig_match_text,
                                      d->trig_chance_pct, d->edit_buf);
                descriptor_send(d, ok ? "Trigger saved.\r\n" : "Saving the trigger failed.\r\n");
                d->edit_kind = EDIT_NONE;
                show_trigedit_list(d);
            } else if (act == EDITOR_ABORT) {
                d->edit_kind = EDIT_NONE;
                descriptor_send(d, "Edit aborted -- nothing changed.\r\n");
                show_trigedit_list(d);
            }
            return true;
        }

        case CONN_PLAYING: {
            /* Output pager (news): while a page is pending, a line advances
             * (ENTER) or stops (Q) instead of running a command. */
            if (d->page_len > 0) {
                if (line[0] == 'q' || line[0] == 'Q') {
                    d->page_len = 0;
                    d->page_pos = 0;
                    descriptor_send(d, "\r\n");
                } else {
                    descriptor_page_next(d);
                }
                return true;
            }

            /* The shared line editor (hedit topics + addnews stories -- room
             * descriptions edit through the redit menu's CONN_REDIT_DESC
             * state) swallows every line while active; commands don't work
             * until it closes with "." (save) or "~" (abort). */
            if (d->edit_kind != EDIT_NONE) {
                editor_action_t act = editor_feed(d, line);
                if (act == EDITOR_SAVE) {
                    const char *who = d->character ? d->character->base.name : "";
                    if (d->edit_kind == EDIT_NEWS || d->edit_kind == EDIT_WIZNEWS) {
                        bool wiz = (d->edit_kind == EDIT_WIZNEWS);
                        if (news_repo_upsert(wiz, who, d->news_title, d->edit_buf))
                            descriptor_send(d, wiz ? "Immortal news posted.\r\n"
                                                   : "News posted.\r\n");
                        else
                            descriptor_send(d, "Posting failed.\r\n");
                    } else if (d->edit_kind == EDIT_RULES) {
                        if (rules_repo_upsert(d->rule_num, d->news_title, d->edit_buf, who)) {
                            char msg[64];
                            snprintf(msg, sizeof(msg), "Rule %d saved.\r\n", d->rule_num);
                            descriptor_send(d, msg);
                        } else {
                            descriptor_send(d, "Saving the rule failed.\r\n");
                        }
                    } else {
                        char final_body[HELP_BODY_MAX + sizeof(d->edit_related) + 16];
                        if (d->edit_related[0])
                            snprintf(final_body, sizeof(final_body), "%s\nRelated: %s",
                                     d->edit_buf, d->edit_related);
                        else
                            snprintf(final_body, sizeof(final_body), "%s", d->edit_buf);

                        if (help_topic_save(d->edit_topic, final_body, who)) {
                            char msg[96];
                            snprintf(msg, sizeof(msg), "Help topic '%s' saved.\r\n",
                                     d->edit_topic);
                            descriptor_send(d, msg);
                        } else {
                            descriptor_send(d, "Saving failed -- topic unchanged.\r\n");
                        }
                    }
                    d->edit_kind = EDIT_NONE;
                } else if (act == EDITOR_ABORT) {
                    d->edit_kind = EDIT_NONE;
                    descriptor_send(d, "Edit aborted -- nothing changed.\r\n");
                }
                if (d->edit_kind == EDIT_NONE) /* just left the editor */
                    descriptor_editor_exit_notice(d);
                return true;
            }

            /* `snoop` mirror: the snooped player's own typed command line,
             * prefixed "% " (classic DikuMUD/Sneezy convention) so the
             * watcher can tell it apart from the target's own output. Sent
             * before dispatch so command order matches what actually
             * happened. */
            if (d->snooped_by) {
                char echo[600];
                snprintf(echo, sizeof(echo), "%% %s\r\n", line);
                descriptor_send(d->snooped_by, echo);
            }

            /* No explicit prompt here anymore (Session 21): the game
             * loop's prompter sends one "> " per iteration to any playing,
             * non-editing connection that received output -- covering both
             * command replies and asynchronous output (says, combat
             * rounds, broadcasts) uniformly. */
            return cmd_dispatch(d, line);
        }

        case CONN_CLOSED:
        default:
            return false;
    }
}
