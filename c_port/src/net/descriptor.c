#include "descriptor.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "cmd.h"
#include "colorstring.h"
#include "log.h"
#include "net.h"
#include "player_repo.h"
#include "room_repo.h"
#include "world.h"

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
    d->next = g_descriptors;
    g_descriptors = d;

    unsigned char negotiate[] = {
        TN_IAC, TN_WILL, TN_ECHO,
        TN_IAC, TN_WILL, TN_SGA,
        TN_IAC, TN_DO, TN_SGA,
    };
    socket_write(fd, (const char *)negotiate, sizeof(negotiate));

    descriptor_send(d, "Welcome to Tobin (C port, walking skeleton).\r\nAccount name: ");
    return d;
}

void descriptor_destroy(descriptor_t *d) {
    if (!d)
        return;

    descriptor_t **cur = &g_descriptors;
    while (*cur && *cur != d)
        cur = &(*cur)->next;
    if (*cur)
        *cur = d->next;

    if (d->character)
        being_destroy(d->character);

    close(d->fd);
    free(d);
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
    free(normalized);
    free(colored);
}

static bool is_password_state(conn_state_t s) {
    return s == CONN_GET_PASSWORD || s == CONN_GET_NEW_PASSWORD;
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

/* Maps an attribute token ("str", "strength", ...) to the matching field
 * in `a`, or NULL for an unrecognized token. */
static int *attr_field(attrs_t *a, const char *tok) {
    if (strcasecmp(tok, "str") == 0 || strcasecmp(tok, "strength") == 0) return &a->strength;
    if (strcasecmp(tok, "dex") == 0 || strcasecmp(tok, "dexterity") == 0) return &a->dexterity;
    if (strcasecmp(tok, "con") == 0 || strcasecmp(tok, "constitution") == 0) return &a->constitution;
    if (strcasecmp(tok, "int") == 0 || strcasecmp(tok, "intelligence") == 0) return &a->intelligence;
    if (strcasecmp(tok, "wis") == 0 || strcasecmp(tok, "wisdom") == 0) return &a->wisdom;
    if (strcasecmp(tok, "cha") == 0 || strcasecmp(tok, "charisma") == 0) return &a->charisma;
    return NULL;
}

static int attrs_allocated(const attrs_t *a) {
    return (a->strength - ATTR_BASE) + (a->dexterity - ATTR_BASE) + (a->constitution - ATTR_BASE) +
           (a->intelligence - ATTR_BASE) + (a->wisdom - ATTR_BASE) + (a->charisma - ATTR_BASE);
}

/* Refreshes d->char_list from the DB and prints the account menu. */
static void show_account_menu(descriptor_t *d) {
    player_list_by_account(d->account.account_id, d->char_list, MAX_CHARS_PER_ACCOUNT, &d->char_count);

    char out[2048];
    int n = snprintf(out, sizeof(out), "\r\n-- Your characters --\r\n");
    if (d->char_count == 0) {
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  (none yet)\r\n");
    } else {
        for (int i = 0; i < d->char_count && (size_t)n < sizeof(out); i++)
            n += snprintf(out + n, sizeof(out) - (size_t)n, "  %d. %s\r\n", i + 1, d->char_list[i]);
    }
    if ((size_t)n < sizeof(out)) {
        snprintf(out + n, sizeof(out) - (size_t)n,
                 "\r\nType a number to play, 'new' to create a character%s,\r\n"
                 "or 'quit!' to disconnect.\r\n\r\n> ",
                 d->char_count > 0 ? ", 'delete <name>' to remove one" : "");
    }
    descriptor_send(d, out);
}

/* Unloads the current character and returns to the account menu, without
 * closing the connection. Used by `quit!` while playing (see cmd_quit.c);
 * the account menu's own `quit!` (see the CONN_ACCOUNT_MENU case below)
 * closes the connection instead. */
void descriptor_leave_to_menu(descriptor_t *d) {
    if (d->character) {
        being_destroy(d->character); /* also removes it from its room */
        d->character = NULL;
    }
    d->state = CONN_ACCOUNT_MENU;
    show_account_menu(d);
}

/* Prints the current point-buy allocation and remaining pool. */
static void show_attr_screen(descriptor_t *d) {
    int remaining = ATTR_POOL - attrs_allocated(&d->new_char_attrs);

    char out[1024];
    snprintf(out, sizeof(out),
             "\r\n-- Allocate attributes for %s --\r\n"
             "Every attribute starts at %d. Raise or lower any attribute by up to\r\n"
             "%d in either direction -- lowering one frees up room to raise another.\r\n"
             "Net pool: %d points. Commands:\r\n"
             "  str/dex/con/int/wis/cha <amount>   set that attribute's adjustment,\r\n"
             "                                      e.g. \"str 30\" or \"wis -20\"\r\n"
             "  reset                              clear all adjustments\r\n"
             "  done                                finish and create the character\r\n"
             "  quit!                               cancel and return to the character menu\r\n\r\n"
             "  Strength:      %3d\r\n"
             "  Dexterity:     %3d\r\n"
             "  Constitution:  %3d\r\n"
             "  Intelligence:  %3d\r\n"
             "  Wisdom:        %3d\r\n"
             "  Charisma:      %3d\r\n"
             "Points remaining: %d\r\n\r\n> ",
             d->new_char_name, ATTR_BASE, ATTR_DELTA_CAP, ATTR_POOL,
             d->new_char_attrs.strength, d->new_char_attrs.dexterity, d->new_char_attrs.constitution,
             d->new_char_attrs.intelligence, d->new_char_attrs.wisdom, d->new_char_attrs.charisma,
             remaining);
    descriptor_send(d, out);
}

/* Shared finish-up for both "play an existing character" and "just
 * finished creating one": place them in their load room and start play. */
static void enter_world(descriptor_t *d, being_t *b) {
    b->desc = d;
    d->character = b;

    int room_vnum = player_load_room(b->base.name, d->account.account_id);
    if (room_vnum < 0)
        room_vnum = 1;

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
    snprintf(welcome, sizeof(welcome), "Welcome, %s!\r\n", b->base.name);
    descriptor_send(d, welcome);

    d->state = CONN_PLAYING;
    cmd_dispatch(d, "look");
    descriptor_send(d, "\r\n> ");
}

static bool handle_line(descriptor_t *d, const char *line) {
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
                descriptor_send(d, "New account. Choose a password (3+ characters): ");
                d->state = CONN_GET_NEW_PASSWORD;
            }
            return true;
        }

        case CONN_GET_PASSWORD: {
            if (!account_verify_password(&d->account, line)) {
                descriptor_send(d, "Incorrect password.\r\nAccount name: ");
                d->state = CONN_GET_ACCOUNT_NAME;
                return true;
            }
            d->state = CONN_ACCOUNT_MENU;
            show_account_menu(d);
            return true;
        }

        case CONN_GET_NEW_PASSWORD: {
            if (strlen(line) < 3) {
                descriptor_send(d, "Too short -- choose a password (3+ characters): ");
                return true;
            }
            if (!account_create(d->account_name, line, &d->account)) {
                descriptor_send(d, "Could not create that account.\r\nAccount name: ");
                d->state = CONN_GET_ACCOUNT_NAME;
                return true;
            }
            d->state = CONN_ACCOUNT_MENU;
            show_account_menu(d);
            return true;
        }

        case CONN_ACCOUNT_MENU: {
            if (!line[0]) {
                show_account_menu(d);
                return true;
            }

            if (strcasecmp(line, "new") == 0) {
                descriptor_send(d, "New character name (or 'quit!' to cancel): ");
                d->state = CONN_CHAR_CREATE_NAME;
                return true;
            }

            if (strcasecmp(line, "quit!") == 0) {
                descriptor_send(d, "Goodbye!\r\n");
                return false; /* actually disconnect -- unlike `quit!` while playing */
            }

            if (strncasecmp(line, "delete", 6) == 0 && (line[6] == '\0' || line[6] == ' ')) {
                const char *target = line + 6;
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

            descriptor_send(d, "Huh? Type a number, 'new', 'delete <name>', or 'quit!'.\r\n");
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
            snprintf(d->new_char_name, sizeof(d->new_char_name), "%s", line);
            being_normalize_name(d->new_char_name);
            d->new_char_attrs = (attrs_t){ ATTR_BASE, ATTR_BASE, ATTR_BASE, ATTR_BASE, ATTR_BASE, ATTR_BASE };
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

            if (strcasecmp(line, "done") == 0) {
                being_t *b = player_create(d->new_char_name, d->account.account_id, &d->new_char_attrs);
                if (!b) {
                    descriptor_send(d, "Could not create that character (name may already be taken).\r\n");
                    d->state = CONN_ACCOUNT_MENU;
                    show_account_menu(d);
                    return true;
                }
                enter_world(d, b);
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

            int *field = attr_field(&d->new_char_attrs, tok);
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

        case CONN_CHAR_DELETE_CONFIRM: {
            if (strcmp(line, "YES") == 0) {
                if (player_delete(d->delete_char_name, d->account.account_id))
                    descriptor_send(d, "Character deleted.\r\n");
                else
                    descriptor_send(d, "Could not delete that character.\r\n");
            } else {
                descriptor_send(d, "Cancelled.\r\n");
            }
            d->state = CONN_ACCOUNT_MENU;
            show_account_menu(d);
            return true;
        }

        case CONN_PLAYING: {
            bool keep_going = cmd_dispatch(d, line);
            /* `quit!` transitions away from CONN_PLAYING (to the account
             * menu, which sends its own trailing prompt) -- only add ours
             * if the command left us still playing, to avoid a double
             * prompt. */
            if (keep_going && d->state == CONN_PLAYING)
                descriptor_send(d, "\r\n> ");
            return keep_going;
        }

        case CONN_CLOSED:
        default:
            return false;
    }
}
