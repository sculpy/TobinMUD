/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_DESCRIPTOR_H
#define TOBIN_DESCRIPTOR_H

#include <stdbool.h>

#include "account.h"
#include "being.h"
#include "help_repo.h"
#include "player_repo.h"
#include "room.h"

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
    CONN_CONFIRM_PASSWORD,
    CONN_GET_COLOR_PREF, /* new-account color on/off prompt */
    CONN_ACCOUNT_MENU,
    CONN_CHAR_CREATE_NAME,
    CONN_CHAR_CREATE_ATTRS,
    CONN_CHAR_DELETE_CONFIRM,
    /* Menu-driven room builder (redit) -- a working-copy editor: field
     * edits mutate d->redit_work only; (S)ave applies it to the live room +
     * DB, (Q)uit can discard. See descriptor_redit_begin() and the
     * CONN_REDIT_* cases in descriptor.c. */
    CONN_REDIT_MENU,
    CONN_REDIT_NAME,
    CONN_REDIT_TERRAIN,
    CONN_REDIT_FLAGS,
    CONN_REDIT_CAPACITY,
    CONN_REDIT_HEIGHT,
    CONN_REDIT_EXITS,
    CONN_REDIT_EXIT_MENU,
    CONN_REDIT_EXIT_TARGET,
    CONN_REDIT_EXIT_DOORTYPE,
    CONN_REDIT_EXIT_CONDITIONS,
    CONN_REDIT_DESC,
    CONN_REDIT_CLEAR_CONFIRM,
    CONN_REDIT_QUIT_CONFIRM,
    CONN_PLAYING,
    CONN_CLOSED
} conn_state_t;

#define DESC_RAW_BUF 1024
#define DESC_LINE_MAX 256

/* Held-message buffer (people in an editor aren't interrupted by game
 * messages -- they save up here for `catchup`, and expire after the TTL). */
#define HELD_MSG_MAX 64
#define HELD_MSG_LEN 256
#define HELD_MSG_TTL 300 /* seconds -- 5 minutes */

typedef struct descriptor {
    int fd;
    conn_state_t state;

    /* Peer address, captured at accept() (or inherited across a copyover
     * via the recovery file). Appears in log lines only -- immortal eyes
     * (the log command's gate); never in player-visible room messages. */
    char ip[46];

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
    char new_password[64]; /* CONN_CONFIRM_PASSWORD scratch, zeroed after use */
    account_t account;

    /* Per-connection color preference (see colorstring.h) -- deliberately
     * a single toggle, not the original's ~10-category bitmask, and not
     * DB-persisted (revisit once there's an account-settings concept). */
    bool color_enabled;

    /* Set by descriptor_send() whenever output goes to this connection;
     * the game loop turns it into exactly one fresh "> " prompt per
     * iteration for playing, non-editing connections -- so asynchronous
     * output (says, combat rounds, arrivals, broadcasts) always leaves
     * the player with a prompt (Session 21, user requirement). Command
     * replies no longer append their own prompt; this is the single
     * prompt authority. */
    bool needs_prompt;

    /* CONN_ACCOUNT_MENU scratch: character list cached for this visit to
     * the menu, refreshed every time the menu is (re-)shown. */
    char char_list[MAX_CHARS_PER_ACCOUNT][PLAYER_NAME_LEN];
    int char_levels[MAX_CHARS_PER_ACCOUNT];
    int char_count;

    /* CONN_CHAR_CREATE_NAME / CONN_CHAR_CREATE_ATTRS scratch. */
    char new_char_name[PLAYER_NAME_LEN];
    attrs_t new_char_attrs;
    int new_char_handed; /* 1 right (default), 0 left */
    gender_t new_char_gender; /* GENDER_NEUTER default */
    char new_char_appearance[BEING_APPEARANCE_LEN]; /* empty default */

    /* CONN_CHAR_DELETE_CONFIRM scratch. */
    char delete_char_name[PLAYER_NAME_LEN];

    /* Shared line-editor state (the classic DikuMUD string editor: "."
     * alone saves, "~" alone aborts, anything else appends). cmd_hedit.c
     * arms it for help topics, cmd_edit.c for room descriptions;
     * descriptor.c's CONN_PLAYING case intercepts every line while
     * edit_kind != EDIT_NONE and routes the save by kind. */
    enum { EDIT_NONE = 0, EDIT_HELP_TOPIC, EDIT_ROOM_DESC, EDIT_NEWS,
           EDIT_WIZNEWS } edit_kind;
    char edit_topic[HELP_TOPIC_NAME_LEN]; /* EDIT_HELP_TOPIC target */
    int edit_room_vnum;                   /* EDIT_ROOM_DESC target */
    char news_title[128];                 /* EDIT_NEWS headline (addnews) */
    char edit_buf[HELP_BODY_MAX];         /* == ROOM_DESCRIPTION_MAX, see room.h */
    int edit_len;

    /* Output pager (the `news` command): long output is buffered here and
     * released one page (page_size lines) at a time. While page_len > 0 the
     * CONN_PLAYING input handler consumes each line to advance (ENTER) or
     * stop (Q) instead of running a command. */
    char page_buf[16384];
    size_t page_len;
    size_t page_pos;
    int page_size;

    /* Menu-driven room builder working copy (CONN_REDIT_*). The live room is
     * copied here on entry; field edits mutate this copy only; (S)ave writes
     * it back to the live room + DB. Never write the whole struct back over
     * the live room -- only its scalar fields + exits are ever applied, so
     * the live room's base pointers stay intact. */
    room_t redit_work;
    bool redit_dirty;    /* unsaved field changes since entry / last save */
    int redit_exit_dir;  /* CONN_REDIT_EXIT_TARGET: direction being set */

    being_t *character;

    /* Time (epoch seconds) of the last input line -- who shows "(idle)" after
     * 5 minutes with no input; any command resets it. */
    long last_active;

    /* Held messages: while this connection is in an editor, asynchronous game
     * messages (says, combat, arrivals, broadcasts) are buffered here instead
     * of interrupting the editor. Reviewed with `catchup`; anything older than
     * HELD_MSG_TTL is dropped by the descriptor_held_expire() pulse. */
    struct { long when; char text[HELD_MSG_LEN]; } held[HELD_MSG_MAX];
    int held_count;

    struct descriptor *next; /* intrusive list of all active descriptors */
} descriptor_t;

extern descriptor_t *g_descriptors;

descriptor_t *descriptor_create(int fd);

/* Rebuilds a playing descriptor around an fd inherited across a copyover
 * exec() (see cmd_copyover.c / game_loop.c): reloads the account and
 * character from the DB, places the character back in room_vnum, and
 * resumes CONN_PLAYING with a "copyover complete" message. No telnet
 * re-negotiation -- the client negotiated on its original connect and the
 * socket never closed. Returns NULL (and closes fd) if the account or
 * character can't be reloaded. */
descriptor_t *descriptor_copyover_adopt(int fd, long account_id, int room_vnum,
                                        bool color_enabled, const char *peer_ip,
                                        const char *char_name,
                                        const char *account_name);

/* Unlinks from g_descriptors, closes the socket, frees the character (if
 * any) and the descriptor itself. */
void descriptor_destroy(descriptor_t *d);

/* Reads whatever is currently available on the socket (non-blocking),
 * extracts complete lines, and feeds each through the login state machine
 * or command dispatcher. Returns false if the connection should be torn
 * down (EOF, error, or a command that requested disconnect). */
bool descriptor_process_input(descriptor_t *d);

void descriptor_send(descriptor_t *d, const char *msg);

/* Sends `text` a page (`page_size` lines, <=0 for a default) at a time, with
 * a "[ ENTER for more, Q to stop ]" prompt between pages; short text goes out
 * in one shot. While a page is pending, the CONN_PLAYING handler routes input
 * to the pager (ENTER = next page, Q = stop). Used by the `news` command. */
void descriptor_page_start(descriptor_t *d, const char *text, int page_size);

/* Opens the menu-driven room builder on room `vnum` (loading it if not
 * already in the world), copies it into the descriptor's working copy, and
 * shows the redit menu (entering CONN_REDIT_MENU). Returns false if no such
 * room exists. Caller (cmd_edit.c) owns the level gate. */
bool descriptor_redit_begin(descriptor_t *d, int vnum);

/* Sends `msg` to every connected player in room `r` except `except` (may
 * be NULL to include everyone). Shared by movement, quit/link-drop, and
 * combat announcements. Recipients in an editor have it held, not sent. */
void descriptor_room_echo(struct room *r, being_t *except, const char *msg);

/* Delivers an asynchronous game message: sent immediately if the connection
 * is free, or held (buffered for `catchup`) if it is in an editor. Async
 * senders (room echoes, combat, broadcasts) use this instead of
 * descriptor_send so nobody's editing is interrupted. */
void descriptor_notify(descriptor_t *d, const char *msg);

/* Pulse callback: drops each connection's held messages older than
 * HELD_MSG_TTL. Registered in main.c. */
void descriptor_held_expire(long pulse_num);

/* Pulse callback: sends a telnet NOP to every connection so idle players
 * aren't dropped by NAT/router idle timeouts. Invisible to the client.
 * Registered in main.c. */
void descriptor_keepalive(long pulse_num);

/* Seconds a playing MORTAL may idle before being disconnected. Immortals are
 * exempt (never idle-dropped). 30 minutes. */
#define IDLE_DISCONNECT_SECS 1800

/* Pulse callback: disconnects mortals idle past IDLE_DISCONNECT_SECS; leaves
 * immortals connected. Registered in main.c. */
void descriptor_idle_timeout(long pulse_num);

/* Unloads the current character (freed, removed from its room) and returns
 * this connection to the account menu -- used by the `quit!` command while
 * playing. Does NOT close the socket; see cmd_quit.c (playing) vs. the
 * CONN_ACCOUNT_MENU `quit!` handling in descriptor.c (actually disconnects). */
void descriptor_leave_to_menu(descriptor_t *d);

#endif
