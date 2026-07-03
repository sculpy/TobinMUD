#ifndef TOBIN_DESCRIPTOR_H
#define TOBIN_DESCRIPTOR_H

#include <stdbool.h>

#include "account.h"
#include "being.h"
#include "help_repo.h"
#include "player_repo.h"

/* C replacement for the Descriptor class (sys/connect.h/.cc) -- still a
 * deliberately minimal subset of the original nanny() login state machine's
 * ~15 CON_* states (MOTD paging, wizlock, delete-character-with-typed-
 * password-reconfirm, ...), but this now covers real multi-character
 * account ownership: an account menu (play/create/delete) and a point-buy
 * attribute dialog for new characters. See c_port/STATUS.md for what's
 * still deferred. */

typedef enum {
    CONN_GET_ACCOUNT_NAME,
    CONN_GET_PASSWORD,
    CONN_GET_NEW_PASSWORD,
    CONN_ACCOUNT_MENU,
    CONN_CHAR_CREATE_NAME,
    CONN_CHAR_CREATE_ATTRS,
    CONN_CHAR_DELETE_CONFIRM,
    CONN_PLAYING,
    CONN_CLOSED
} conn_state_t;

#define DESC_RAW_BUF 1024
#define DESC_LINE_MAX 256

typedef struct descriptor {
    int fd;
    conn_state_t state;

    /* raw telnet byte buffer, persists across process_input() calls since
     * reads are non-blocking and a line may arrive split across packets */
    unsigned char raw[DESC_RAW_BUF];
    int raw_len;
    int raw_pos;
    /* An IAC SB ... IAC SE subnegotiation may arrive split across reads;
     * the scan state persists here so leftover payload bytes are never
     * mistaken for typed input on the next read. */
    bool in_subneg;
    unsigned char subneg_prev;
    int line_len; /* in-progress line being typed, before Enter */
    char line[DESC_LINE_MAX];

    char account_name[80];
    account_t account;

    /* Per-connection color preference (see colorstring.h) -- deliberately
     * a single toggle, not the original's ~10-category bitmask, and not
     * DB-persisted (revisit once there's an account-settings concept). */
    bool color_enabled;

    /* CONN_ACCOUNT_MENU scratch: character list cached for this visit to
     * the menu, refreshed every time the menu is (re-)shown. */
    char char_list[MAX_CHARS_PER_ACCOUNT][PLAYER_NAME_LEN];
    int char_levels[MAX_CHARS_PER_ACCOUNT];
    int char_count;

    /* CONN_CHAR_CREATE_NAME / CONN_CHAR_CREATE_ATTRS scratch. */
    char new_char_name[PLAYER_NAME_LEN];
    attrs_t new_char_attrs;

    /* CONN_CHAR_DELETE_CONFIRM scratch. */
    char delete_char_name[PLAYER_NAME_LEN];

    /* `hedit` line-editor state (cmd_hedit.c starts it; descriptor.c's
     * CONN_PLAYING case intercepts every line while editing_help is set:
     * "." saves, "~" aborts, anything else is appended). */
    bool editing_help;
    char help_edit_topic[HELP_TOPIC_NAME_LEN];
    char help_edit_buf[HELP_BODY_MAX];
    int help_edit_len;

    being_t *character;

    struct descriptor *next; /* intrusive list of all active descriptors */
} descriptor_t;

extern descriptor_t *g_descriptors;

descriptor_t *descriptor_create(int fd);

/* Unlinks from g_descriptors, closes the socket, frees the character (if
 * any) and the descriptor itself. */
void descriptor_destroy(descriptor_t *d);

/* Reads whatever is currently available on the socket (non-blocking),
 * extracts complete lines, and feeds each through the login state machine
 * or command dispatcher. Returns false if the connection should be torn
 * down (EOF, error, or a command that requested disconnect). */
bool descriptor_process_input(descriptor_t *d);

void descriptor_send(descriptor_t *d, const char *msg);

/* Unloads the current character (freed, removed from its room) and returns
 * this connection to the account menu -- used by the `quit!` command while
 * playing. Does NOT close the socket; see cmd_quit.c (playing) vs. the
 * CONN_ACCOUNT_MENU `quit!` handling in descriptor.c (actually disconnects). */
void descriptor_leave_to_menu(descriptor_t *d);

#endif
