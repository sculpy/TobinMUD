/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
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
#include "net.h"
#include "obj_repo.h"
#include "player_repo.h"
#include "multiplay.h"
#include "news_repo.h"
#include "room_repo.h"
#include "rules_repo.h"
#include "world.h"
#include "zone.h"

descriptor_t *g_descriptors = NULL;

/* Minimal telnet negotiation (RFC 854): ask the client to let us do the
 * echoing and to send character-at-a-time instead of buffering a whole
 * line client-side before sending it. */
enum { TN_IAC = 255, TN_WILL = 251, TN_WONT = 252, TN_DO = 253, TN_DONT = 254,
       TN_SB = 250, TN_SE = 240, TN_ECHO = 1, TN_SGA = 3 };

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
    socket_write(fd, (const char *)negotiate, sizeof(negotiate));

    /* The TobinMUD banner (user-supplied art, Session 21). The <_> in the
     * O of "Tobin" is an unrecognized color tag and passes through
     * literally by design. */
    descriptor_send(d,
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
        || (d->state >= CONN_BALANCE_MENU && d->state <= CONN_BALANCE_QUIT_CONFIRM)
        || d->page_len > 0; /* mid-pager -- same "no interruptions" treatment */
}

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

void descriptor_notify(descriptor_t *d, const char *msg) {
    if (descriptor_in_editor(d))
        descriptor_hold(d, msg);
    else
        descriptor_send(d, msg);
}

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

void descriptor_keepalive(long pulse_num) {
    (void)pulse_num;
    /* IAC NOP -- a telnet no-op. Keeps the TCP connection warm so idle
     * players aren't dropped by NAT/router timeouts; clients ignore it.
     * Sent aggressively (see the pulse interval in main.c) to survive short
     * NAT/router idle windows. */
    static const unsigned char nop[2] = {255, 241};
    for (descriptor_t *d = g_descriptors; d; d = d->next)
        socket_write(d->fd, (const char *)nop, sizeof(nop));
}

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

void descriptor_destroy(descriptor_t *d) {
    if (!d)
        return;

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
         * save, an accepted, narrow trade-off. */
        d->character->desc = NULL;
    }

    /* Unhook any live `snoop` relationship in either direction so neither
     * side is left pointing at a descriptor about to be freed. */
    if (d->snoop_target)
        d->snoop_target->snooped_by = NULL;
    if (d->snooped_by)
        d->snooped_by->snoop_target = NULL;

    close(d->fd);
    free(d);
}

const char *descriptor_display_host(const descriptor_t *d) {
    return d->hostname[0] ? d->hostname : d->ip;
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
        socket_write(d->fd, msg, len); /* best-effort fallback */
        return;
    }
    size_t clen = colorstring_translate(msg, colored, color_cap, d->color_enabled);

    char *normalized = malloc(clen * 2 + 1);
    if (!normalized) {
        socket_write(d->fd, colored, clen); /* best-effort fallback */
        free(colored);
        return;
    }

    size_t out = 0;
    for (size_t i = 0; i < clen; i++) {
        if (colored[i] == '\n' && (i == 0 || colored[i - 1] != '\r'))
            normalized[out++] = '\r';
        normalized[out++] = colored[i];
    }

    socket_write(d->fd, normalized, out);

    /* `snoop` mirror: whatever this descriptor sees, its watcher (if any)
     * sees too -- the exact same bytes already rendered for THIS
     * descriptor's own color preference (matching a real snoop: you're
     * watching their literal screen, not re-rendering for your own). A
     * direct socket_write, not a recursive descriptor_send() call, so a
     * chain/cycle of snoops can never recurse.
     *
     * A "% " marker (same literal prefix the typed-command mirror below
     * already uses) is written ahead of every mirrored output chunk too
     * (user 2026-07-11: "add a special prompt to messages sent in snoop
     * (%) snooped content") -- before this, only the target's typed
     * commands were marked; their OWN output was mirrored completely
     * unmarked, indistinguishable from the snooper's own screen. */
    if (d->snooped_by) {
        static const char marker[] = "% ";
        socket_write(d->snooped_by->fd, marker, strlen(marker));
        socket_write(d->snooped_by->fd, normalized, out);
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

void descriptor_page_start(descriptor_t *d, const char *text, int page_size) {
    d->page_size = page_size > 0 ? page_size : 20;
    snprintf(d->page_buf, sizeof(d->page_buf), "%s", text);
    d->page_len = strlen(d->page_buf);
    d->page_pos = 0;
    descriptor_page_next(d);
}

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
            socket_write(d->fd, "\r\n", 2);
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
                    socket_write(d->fd, "\b \b", 3);
            }
            continue;
        }

        if (b < 32)
            continue;

        if (d->line_len + 1 < DESC_LINE_MAX) {
            d->line[d->line_len++] = (char)b;
            if (!is_password_state(d->state)) {
                char echo[2] = { (char)b, '\0' };
                socket_write(d->fd, echo, 1);
            }
        }
    }
    return true;
}

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

static int attrs_allocated(const attrs_t *a) {
    return (a->strength - ATTR_BASE) + (a->dexterity - ATTR_BASE) + (a->constitution - ATTR_BASE) +
           (a->intelligence - ATTR_BASE) + (a->wisdom - ATTR_BASE) + (a->charisma - ATTR_BASE);
}

/* Refreshes d->char_list from the DB and prints the account menu. */
static void show_account_menu(descriptor_t *d) {
    player_list_by_account(d->account.account_id, d->char_list, d->char_levels,
                           MAX_CHARS_PER_ACCOUNT, &d->char_count);

    char out[2048];
    int n = snprintf(out, sizeof(out), "\r\n-- Your characters --\r\n");
    if (d->char_count == 0) {
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  (none yet)\r\n");
    } else {
        for (int i = 0; i < d->char_count && (size_t)n < sizeof(out); i++) {
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
                n += snprintf(out + n, sizeof(out) - (size_t)n, "  %d. %s (%s)%s\r\n",
                              i + 1, d->char_list[i], title,
                              online ? " (connected)" : "");
            else
                n += snprintf(out + n, sizeof(out) - (size_t)n, "  %d. %s (Level %d)%s\r\n",
                              i + 1, d->char_list[i], d->char_levels[i],
                              online ? " (connected)" : "");
        }
    }
    if ((size_t)n < sizeof(out)) {
        snprintf(out + n, sizeof(out) - (size_t)n,
                 "\r\n  C [number|name] -- connect a character\r\n"
                 "  N               -- create a new character\r\n"
                 "  D <name>        -- delete a character\r\n"
                 "  X               -- delete this ENTIRE ACCOUNT\r\n"
                 "  Q               -- quit the game\r\n"
                 "(Letters work in either case; a bare number still connects too.)\r\n\r\n> ");
    }
    descriptor_send(d, out);
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
    show_account_menu(d);
}

/* Prints the current point-buy allocation and remaining pool. */
static void show_attr_screen(descriptor_t *d) {
    int remaining = ATTR_POOL - attrs_allocated(&d->new_char_attrs);

    const char *appear = d->new_char_appearance[0] ? d->new_char_appearance : "(none set)";

    /* Sized for the fixed menu text plus a full-length appearance (up to
     * BEING_APPEARANCE_LEN, bumped Session 43 continued for the mob-
     * description truncation fix -- new_char_appearance shares that
     * buffer size) with generous headroom so gcc's -Wformat-truncation
     * worst-case estimate (which sums every %s field's own declared
     * bound, not just the actual data) can prove it always fits. */
    char out[BEING_APPEARANCE_LEN + 2048];
    snprintf(out, sizeof(out),
             "\r\n-- Allocate attributes for %s --\r\n"
             "Every attribute starts at %d. Raise or lower any attribute by up to\r\n"
             "%d in either direction -- lowering one frees up room to raise another.\r\n"
             "Net pool: %d points. Commands:\r\n"
             "  str/dex/con/int/wis/cha <amount>   set that attribute's adjustment,\r\n"
             "                                      e.g. \"str 30\" or \"wis -20\"\r\n"
             "  hand left|right                    choose your primary hand (default right)\r\n"
             "  gender male|female|neuter          choose your gender (default neuter)\r\n"
             "  appearance <text>                  describe how you look to others\r\n"
             "  reset                              clear all adjustments\r\n"
             "  done                                finish and create the character\r\n"
             "  quit!                               cancel and return to the character menu\r\n\r\n"
             "  Strength:      %3d\r\n"
             "  Dexterity:     %3d\r\n"
             "  Constitution:  %3d\r\n"
             "  Intelligence:  %3d\r\n"
             "  Wisdom:        %3d\r\n"
             "  Charisma:      %3d\r\n"
             "  Handedness:    %s\r\n"
             "  Gender:        %s\r\n"
             "  Appearance:    %s\r\n"
             "Points remaining: %d\r\n\r\n> ",
             d->new_char_name, ATTR_BASE, ATTR_DELTA_CAP, ATTR_POOL,
             d->new_char_attrs.strength, d->new_char_attrs.dexterity, d->new_char_attrs.constitution,
             d->new_char_attrs.intelligence, d->new_char_attrs.wisdom, d->new_char_attrs.charisma,
             d->new_char_handed ? "right" : "left",
             gender_name(d->new_char_gender), appear,
             remaining);
    descriptor_send(d, out);
}

/* CONN_CHAR_CREATE_RACE / CONN_CHAR_CREATE_CLASS / CONN_CHAR_CREATE_ALIGNMENT
 * (user 2026-07-11: "implement races, 6 player races" / "implement classes,
 * 6 player classes" / "ask player to choose initial alignment"): three
 * short numbered-choice steps between attribute allocation and actually
 * creating the character. Race and class each apply a fixed stat bonus/
 * penalty on top of the point-buy attrs already chosen (race_stat_bonus()/
 * class_stat_bonus(), being.c) -- shown live so the player sees the actual
 * post-bonus numbers before confirming. */
static void show_race_screen(descriptor_t *d) {
    char out[900];
    snprintf(out, sizeof(out),
             "\r\n-- Choose a race for %s --\r\n"
             "Each race applies a fixed shift on top of the attributes you already\r\n"
             "allocated.\r\n\r\n"
             "  1) Human   -- no change, a versatile baseline\r\n"
             "  2) Elf     -- +2 Dex, +2 Int, -4 Con\r\n"
             "  3) Ogre    -- +4 Str, -2 Int, -2 Cha\r\n"
             "  4) Dwarf   -- +4 Con, -2 Dex, -2 Cha\r\n"
             "  5) Hobbit  -- +4 Dex, -2 Str, -2 Con\r\n"
             "  6) Gnome   -- +4 Int, -2 Str, -2 Con\r\n\r\n"
             "Enter a number (1-6), or 'quit!' to cancel: ",
             d->new_char_name);
    descriptor_send(d, out);
}

static void show_class_screen(descriptor_t *d) {
    char out[900];
    snprintf(out, sizeof(out),
             "\r\n-- Choose a class for %s --\r\n"
             "Each class shifts your attributes further, on top of your race.\r\n\r\n"
             "  1) Mage     -- +4 Int, -4 Str\r\n"
             "  2) Cleric   -- +4 Wis, -2 Str, -2 Dex\r\n"
             "  3) Warrior  -- +3 Con, +3 Str, -3 Cha, -3 Wis\r\n"
             "  4) Thief    -- +4 Dex, -4 Str\r\n"
             "  5) Druid    -- +2 Wis, +2 Con, -4 Int\r\n"
             "  6) Monk     -- +2 Str, +2 Con, -4 Cha\r\n\r\n"
             "Enter a number (1-6), or 'quit!' to cancel: ",
             d->new_char_name);
    descriptor_send(d, out);
}

static void show_alignment_screen(descriptor_t *d) {
    char out[500];
    snprintf(out, sizeof(out),
             "\r\n-- Choose an alignment for %s --\r\n"
             "  1) Good     -- other good-aligned mobs leave you be; evil ones may\r\n"
             "                 target you, and you'll never be picked on by mobs that\r\n"
             "                 favor good\r\n"
             "  2) Neutral  -- no faction leans your way, but you may draw the odd\r\n"
             "                 taunt from evil or word of support from good\r\n"
             "  3) Evil     -- the mirror of Good\r\n\r\n"
             "Enter a number (1-3), or 'quit!' to cancel: ",
             d->new_char_name);
    descriptor_send(d, out);
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
             "   1) Name              2) Description\r\n"
             "   3) Flags             4) Sector Type\r\n"
             "   5) Exits             6) Max Capacity: %d\r\n"
             "   7) Room Height: %d\r\n\r\n"
             "Description:\r\n%s%s"
             "Exits: %s\r\n\r\n"
             "   C) Clear room out    S) Save    Q) Quit\r\n%s[edit room] ",
             w->base.name, w->vnum, sector_name(w->sector),
             room_flag_names(w->room_flag, flagbuf, sizeof(flagbuf)),
             w->capacity, w->height,
             w->description, descnl, exitbuf,
             d->redit_dirty ? "<c>* unsaved changes *<z>\r\n" : "");
    descriptor_send(d, out);
    d->state = CONN_REDIT_MENU;
}

static void show_redit_flags(descriptor_t *d) {
    char out[2048];
    size_t n = (size_t)snprintf(out, sizeof(out),
        "\r\nRoom flags for %d -- toggle by number, blank to return:\r\n",
        d->redit_work.vnum);
    for (int b = 0; b < room_flag_count(); b++) {
        n += (size_t)snprintf(out + n, sizeof(out) > n ? sizeof(out) - n : 0,
            "  %2d [%c] %-14s%s", b,
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

static void show_redit_terrain(descriptor_t *d) {
    char out[4096];
    size_t n = (size_t)snprintf(out, sizeof(out),
        "\r\nTerrain for %d (current %d: %s) -- choose a number, blank to return:\r\n",
        d->redit_work.vnum, d->redit_work.sector, sector_name(d->redit_work.sector));
    for (int s = 0; s < MAX_SECTOR_TYPES; s++) {
        n += (size_t)snprintf(out + n, sizeof(out) > n ? sizeof(out) - n : 0,
            "  %2d %-22s%s", s, sector_name(s), (s % 3 == 2) ? "\r\n" : "");
        if (n >= sizeof(out))
            break;
    }
    if (MAX_SECTOR_TYPES % 3 != 0 && n < sizeof(out))
        snprintf(out + n, sizeof(out) - n, "\r\n");
    descriptor_send(d, out);
    descriptor_send(d, "terrain> ");
    d->state = CONN_REDIT_TERRAIN;
}

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
            "  %2d) %-10s %s\r\n", i, DIR_NAMES[i], info);
        if (n >= sizeof(out))
            break;
    }
    descriptor_send(d, out);
    descriptor_send(d, "exit> ");
    d->state = CONN_REDIT_EXITS;
}

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
        "  1) Target room      2) Door type\r\n"
        "  3) Conditions       4) Remove this exit\r\n"
        "  blank) back\r\nexit-%s> ",
        DIR_NAMES[dir], tgt, door_type_name(w->exit_door[dir]),
        exit_cond_names(w->exit_cond[dir], cbuf, sizeof(cbuf)), DIR_NAMES[dir]);
    descriptor_send(d, out);
    d->state = CONN_REDIT_EXIT_MENU;
}

static void show_redit_doortype(descriptor_t *d) {
    int dir = d->redit_exit_dir;
    char out[640];
    size_t n = (size_t)snprintf(out, sizeof(out),
        "\r\nDoor type for %s (current: %s) -- choose a number, blank to keep:\r\n",
        DIR_NAMES[dir], door_type_name(d->redit_work.exit_door[dir]));
    for (int t = 0; t < door_type_count(); t++) {
        n += (size_t)snprintf(out + n, sizeof(out) > n ? sizeof(out) - n : 0,
            "  %2d %-14s%s", t, door_type_name(t), (t % 3 == 2) ? "\r\n" : "");
        if (n >= sizeof(out))
            break;
    }
    if (door_type_count() % 3 != 0 && n < sizeof(out))
        snprintf(out + n, sizeof(out) - n, "\r\n");
    descriptor_send(d, out);
    descriptor_send(d, "door> ");
    d->state = CONN_REDIT_EXIT_DOORTYPE;
}

static void show_redit_conditions(descriptor_t *d) {
    int dir = d->redit_exit_dir;
    int cond = d->redit_work.exit_cond[dir];
    char out[1024];
    size_t n = (size_t)snprintf(out, sizeof(out),
        "\r\nConditions for %s -- toggle by number, blank to return:\r\n",
        DIR_NAMES[dir]);
    for (int b = 0; b < exit_cond_count(); b++) {
        n += (size_t)snprintf(out + n, sizeof(out) > n ? sizeof(out) - n : 0,
            "  %2d [%c] %-18s%s", b, (cond & (1 << b)) ? 'x' : ' ',
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

static void redit_leave(descriptor_t *d) {
    d->state = CONN_PLAYING;
    d->redit_dirty = false;
    descriptor_send(d, "Leaving the room editor.\r\n");
    descriptor_editor_exit_notice(d);
}

bool descriptor_redit_begin(descriptor_t *d, int vnum) {
    room_t *live = redit_room_get(vnum);
    if (!live)
        return false;
    d->redit_work = *live; /* field copy; only scalars + exits are ever used
                              or applied back, never the base pointers. */
    d->redit_dirty = false;
    show_redit_menu(d);
    return true;
}

static void show_edplayer_menu(descriptor_t *d) {
    being_t *w = &d->edplayer_work;
    char out[900];
    snprintf(out, sizeof(out),
             "\r\n<c>Editing player:<z> %s\r\n\r\n"
             "   1) Level: %d              2) Experience: %ld\r\n"
             "   3) HP/Max HP: %d/%d       4) Attributes (str/dex/con/int/wis/cha)\r\n"
             "   5) Gender: %s      6) Title: %s\r\n"
             "   7) Load Room: %d          8) Handedness: %s\r\n"
             "   9) Class: %s       0) Race: %s\r\n\r\n"
             "   S) Save    Q) Quit%s\r\n[edit player] ",
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

static void edplayer_leave(descriptor_t *d) {
    d->state = CONN_PLAYING;
    d->edplayer_dirty = false;
    descriptor_send(d, "Leaving the player editor.\r\n");
    descriptor_editor_exit_notice(d);
}

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
             "   1) Name: %s\r\n"
             "   2) Enabled: %s              3) Lifespan (minutes): %d\r\n"
             "   4) Vnum range: %d-%d\r\n"
             "   5) Assigned builders: %s\r\n\r\n"
             "   R) Reset this zone now\r\n"
             "   S) Save    Q) Quit%s\r\n[edit zone] ",
             w->name, w->zone_nr, w->name, w->enabled ? "yes" : "no", w->lifespan,
             w->bottom, w->top, ownerbuf,
             d->edzone_dirty ? "\r\n   <c>* unsaved changes *<z>" : "");
    descriptor_send(d, out);
    d->state = CONN_EDZONE_MENU;
}

static void edzone_save(descriptor_t *d) {
    if (!zone_repo_save(&d->edzone_work)) {
        descriptor_send(d, "Save failed -- the DB rejected part of it.\r\n");
        return;
    }
    d->edzone_dirty = false;
    descriptor_send(d, "Zone saved.\r\n");
}

static void edzone_leave(descriptor_t *d) {
    d->state = CONN_PLAYING;
    d->edzone_dirty = false;
    descriptor_send(d, "Leaving the zone editor.\r\n");
    descriptor_editor_exit_notice(d);
}

bool descriptor_edzone_begin(descriptor_t *d, int zone_nr) {
    zone_t loaded;
    if (!zone_repo_load_one(zone_nr, &loaded))
        return false;
    d->edzone_work = loaded;
    d->edzone_dirty = false;
    show_edzone_menu(d);
    return true;
}

static void show_balance_menu(descriptor_t *d) {
    const char *kind = d->balance_is_class ? "class" : "race";
    const char *name = d->balance_is_class ? class_name((player_class_t)d->balance_index)
                                            : race_name((player_race_t)d->balance_index);
    balance_mod_t *w = &d->balance_work;
    char out[512];
    snprintf(out, sizeof(out),
             "\r\n<c>Balancing %s:<z> %s\r\n\r\n"
             "   1) HP multiplier: %.2f          2) Damage multiplier: %.2f\r\n"
             "   3) To-hit modifier: %+d         4) AC modifier: %+d\r\n\r\n"
             "   S) Save    Q) Quit%s\r\n[balance %s] ",
             kind, name, (double)w->hp_mult, (double)w->dmg_mult, w->tohit_mod, w->ac_mod,
             d->balance_dirty ? "\r\n   <c>* unsaved changes *<z>" : "", kind);
    descriptor_send(d, out);
    d->state = CONN_BALANCE_MENU;
}

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

static void balance_leave(descriptor_t *d) {
    d->state = CONN_PLAYING;
    d->balance_dirty = false;
    descriptor_send(d, "Leaving the balance editor.\r\n");
    descriptor_editor_exit_notice(d);
}

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
                    } else {
                        descriptor_send(d, d->char_count == 0
                            ? "No characters yet -- N creates one.\r\n"
                            : "Connect which one? C <number or name>\r\n");
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
                if (!*target) {
                    descriptor_send(d, "Delete whom? Usage: delete <name>\r\n");
                    show_account_menu(d);
                    return true;
                }
                int found = 0;
                for (int i = 0; i < d->char_count; i++) {
                    if (strcasecmp(d->char_list[i], target) == 0) { found = 1; break; }
                }
                if (!found) {
                    descriptor_send(d, "No character by that name on this account.\r\n");
                    show_account_menu(d);
                    return true;
                }
                snprintf(d->delete_char_name, sizeof(d->delete_char_name), "%s", target);
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
            d->state = CONN_CHAR_CREATE_ATTRS;
            show_attr_screen(d);
            return true;
        }

        case CONN_CHAR_CREATE_ATTRS: {
            if (strcasecmp(line, "quit!") == 0) {
                descriptor_send(d, "Character creation cancelled.\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }

            /* Handedness choice (Session 21): optional, default right. */
            if (strncasecmp(line, "hand", 4) == 0) {
                char hd[16];
                if (sscanf(line + 4, "%15s", hd) == 1
                    && (strcasecmp(hd, "left") == 0 || strcasecmp(hd, "l") == 0)) {
                    d->new_char_handed = 0;
                } else if (sscanf(line + 4, "%15s", hd) == 1
                           && (strcasecmp(hd, "right") == 0 || strcasecmp(hd, "r") == 0)) {
                    d->new_char_handed = 1;
                } else {
                    descriptor_send(d, "Usage: hand left | hand right\r\n");
                }
                show_attr_screen(d);
                return true;
            }

            /* Gender choice (Session 23): optional, default neuter. */
            if (strncasecmp(line, "gender", 6) == 0) {
                char gd[16];
                if (sscanf(line + 6, "%15s", gd) != 1) {
                    descriptor_send(d, "Usage: gender male | female | neuter\r\n");
                } else if (strcasecmp(gd, "male") == 0 || strcasecmp(gd, "m") == 0) {
                    d->new_char_gender = GENDER_MALE;
                } else if (strcasecmp(gd, "female") == 0 || strcasecmp(gd, "f") == 0) {
                    d->new_char_gender = GENDER_FEMALE;
                } else if (strcasecmp(gd, "neuter") == 0 || strcasecmp(gd, "n") == 0) {
                    d->new_char_gender = GENDER_NEUTER;
                } else {
                    descriptor_send(d, "Usage: gender male | female | neuter\r\n");
                }
                show_attr_screen(d);
                return true;
            }

            /* Appearance (Session 23): free-text self-description, optional. */
            if (strncasecmp(line, "appearance", 10) == 0) {
                const char *p = line + 10;
                while (*p == ' ')
                    p++;
                if (*p)
                    snprintf(d->new_char_appearance, sizeof(d->new_char_appearance), "%s", p);
                else
                    d->new_char_appearance[0] = '\0'; /* bare 'appearance' clears it */
                show_attr_screen(d);
                return true;
            }

            if (strcasecmp(line, "done") == 0) {
                d->new_char_race = RACE_HUMAN;
                d->new_char_class = CLASS_MAGE;
                d->new_char_alignment = 0;
                d->state = CONN_CHAR_CREATE_RACE;
                show_race_screen(d);
                return true;
            }

            if (strcasecmp(line, "reset") == 0) {
                d->new_char_attrs = (attrs_t){ ATTR_BASE, ATTR_BASE, ATTR_BASE, ATTR_BASE, ATTR_BASE, ATTR_BASE };
                show_attr_screen(d);
                return true;
            }

            char tok[32];
            int amount = 0;
            if (sscanf(line, "%31s %d", tok, &amount) != 2) {
                descriptor_send(d, "Usage: <attribute> <amount>, 'reset', 'done', or 'quit!'.\r\n");
                show_attr_screen(d);
                return true;
            }

            int *field = attrs_field(&d->new_char_attrs, tok);
            if (!field) {
                descriptor_send(d, "Unknown attribute. Try: str, dex, con, int, wis, cha.\r\n");
                show_attr_screen(d);
                return true;
            }
            if (amount < -ATTR_DELTA_CAP || amount > ATTR_DELTA_CAP) {
                char msg[96];
                snprintf(msg, sizeof(msg), "Adjustment must be between -%d and +%d.\r\n",
                         ATTR_DELTA_CAP, ATTR_DELTA_CAP);
                descriptor_send(d, msg);
                show_attr_screen(d);
                return true;
            }

            int old_value = *field;
            *field = ATTR_BASE + amount;
            if (attrs_allocated(&d->new_char_attrs) > ATTR_POOL) {
                *field = old_value; /* would overspend the net pool -- reject */
                descriptor_send(d, "Not enough points remaining for that.\r\n");
                show_attr_screen(d);
                return true;
            }

            show_attr_screen(d);
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
            d->new_char_race = (player_race_t)(choice - 1);
            race_stat_bonus(d->new_char_race, &d->new_char_attrs);
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
            d->new_char_class = (player_class_t)(choice - 1);
            class_stat_bonus(d->new_char_class, &d->new_char_attrs);
            d->state = CONN_CHAR_CREATE_ALIGNMENT;
            show_alignment_screen(d);
            return true;
        }

        case CONN_CHAR_CREATE_ALIGNMENT: {
            if (strcasecmp(line, "quit!") == 0) {
                descriptor_send(d, "Character creation cancelled.\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }
            int choice = 0;
            if (sscanf(line, "%d", &choice) != 1 || choice < 1 || choice > 3) {
                descriptor_send(d, "Enter a number from 1 to 3, or 'quit!'.\r\n");
                show_alignment_screen(d);
                return true;
            }
            /* 1 Good -> +500, 2 Neutral -> 0, 3 Evil -> -500 -- solidly in
             * alignment_word()'s "good"/"neutral"/"evil" tiers (>=350/
             * between/<=-350), leaving room to drift toward saintly/demonic
             * through play. */
            static const int ALIGNMENT_CHOICES[3] = { 500, 0, -500 };
            d->new_char_alignment = ALIGNMENT_CHOICES[choice - 1];

            being_t *b = player_create(d->new_char_name, d->account.account_id,
                                       &d->new_char_attrs, d->new_char_handed,
                                       d->new_char_gender, d->new_char_appearance,
                                       d->new_char_class, d->new_char_race,
                                       d->new_char_alignment);
            if (!b) {
                descriptor_send(d, "Could not create that character (name may already be taken).\r\n");
                d->state = CONN_ACCOUNT_MENU;
                show_account_menu(d);
                return true;
            }
            /* One-time nudge (user 2026-07-12: "i want it so a first time
             * player of this game will feel comfortable playing because
             * he knows where to find game play information") -- shown
             * only right here, at genuine character creation, not on
             * every later login (see enter_world()'s "Welcome, X!"),
             * so a veteran never sees it again. */
            descriptor_send(d, "\r\nNew to TobinMUD? Type 'help playing' any time for an overview of the basics.\r\n");
            enter_world(d, b);
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
            } else if (player_delete(d->delete_char_name, d->account.account_id)) {
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
                    default:
                        descriptor_send(d, "Pick a menu number (1-7), or C/S/Q.\r\n");
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
                    descriptor_send(d, "Pick a menu number (1-7), or C/S/Q.\r\n");
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
                        if (news_repo_add(wiz, who, d->news_title, d->edit_buf))
                            descriptor_send(d, wiz ? "Immortal news posted.\r\n"
                                                   : "News posted.\r\n");
                        else
                            descriptor_send(d,
                                "Posting failed (that headline may already exist).\r\n");
                    } else if (d->edit_kind == EDIT_RULES) {
                        if (rules_repo_upsert(d->rule_num, d->news_title, d->edit_buf, who)) {
                            char msg[64];
                            snprintf(msg, sizeof(msg), "Rule %d saved.\r\n", d->rule_num);
                            descriptor_send(d, msg);
                        } else {
                            descriptor_send(d, "Saving the rule failed.\r\n");
                        }
                    } else if (d->edit_kind == EDIT_TRIGGER) {
                        if (trigger_repo_add(who, d->trig_target_type, d->trig_target_vnum,
                                             d->trig_trigger_type, d->trig_match_text,
                                             d->trig_chance_pct, d->edit_buf)) {
                            descriptor_send(d, "Trigger saved.\r\n");
                        } else {
                            descriptor_send(d, "Saving the trigger failed.\r\n");
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
