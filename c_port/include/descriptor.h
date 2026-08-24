/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_DESCRIPTOR_H
#define TOBIN_DESCRIPTOR_H

#include <stdbool.h>

#include "account.h"
#include "balance_repo.h"
#include "being.h"
#include "help_repo.h"
#include "mob_repo.h"
#include "obj_repo.h"
#include "player_repo.h"
#include "room.h"
#include "room_repo.h"
#include "shop_repo.h"
#include "social_repo.h"
#include "suit_repo.h"
#include "trigger_repo.h"
#include "zone_repo.h"

/* C replacement for the Descriptor class (sys/connect.h/.cc) -- still a
 * deliberately minimal subset of the original nanny() login state machine's
 * ~15 CON_* states (MOTD paging, wizlock, delete-character-with-typed-
 * password-reconfirm, ...), but this now covers real multi-character
 * account ownership: an account menu (play/create/delete) and a point-buy
 * attribute dialog for new characters. See c_port/STATUS.md for what's
 * still deferred. */

typedef enum {
    CONN_GET_ACCOUNT_NAME,
    CONN_CONFIRM_NEW_ACCOUNT, /* unrecognized name at login -- confirm before creating */
    CONN_GET_PASSWORD,
    CONN_GET_NEW_PASSWORD,
    CONN_CONFIRM_PASSWORD,
    CONN_GET_COLOR_PREF, /* new-account color on/off prompt */
    CONN_GET_TIMEZONE, /* new-account real-time-zone-offset prompt */
    CONN_GET_EMAIL, /* new-account optional email prompt (user, 2026-08-08) */
    CONN_ACCOUNT_MENU,
    CONN_CHAR_CREATE_NAME,
    CONN_CHAR_CREATE_RACE,      /* after name: pick one of 6 races */
    CONN_CHAR_CREATE_TERRITORY, /* after race: pick a homeland (Territory/Homeland audit item) */
    CONN_CHAR_CREATE_CLASS,     /* after territory: pick one of 6 classes */
    CONN_CHAR_CREATE_ATTRS,     /* after class: point-buy, race/territory/class bonuses folded in on "done" */
    CONN_CHAR_CREATE_ATTR_AMOUNT, /* numbered-pick (1-6) sub-prompt: "how much?" for one attribute */
    /* Second boxed menu (user wireframe, 2026-07-26), shown after attrs:
     * handedness/gender/alignment/appearance all live here now, each its
     * own numbered sub-menu -- "done" here (not a bare alignment number)
     * is what actually creates the character. Replaces the old standalone
     * CONN_CHAR_CREATE_ALIGNMENT screen (alignment is now option 3 here,
     * defaulting to neutral rather than always forcing a choice). */
    CONN_CHAR_CREATE_OPTIONS,
    CONN_CHAR_CREATE_OPT_HAND,
    CONN_CHAR_CREATE_OPT_GENDER,
    CONN_CHAR_CREATE_OPT_ALIGN,
    CONN_CHAR_CREATE_OPT_APPEARANCE,
    /* Bare D/delete (no target) at the account menu -- shows the numbered
     * character list and waits for a number/name pick before moving on to
     * CONN_CHAR_DELETE_CONFIRM (user, 2026-07-26: "when deleting a
     * character, the player should be presented a list of his characters
     * so he could choose properly"). A dedicated state rather than reusing
     * CONN_ACCOUNT_MENU because a bare number THERE already means "connect
     * to that character" -- this needs its own distinct meaning for the
     * exact same input. */
    CONN_CHAR_DELETE_PICK,
    CONN_CHAR_DELETE_CONFIRM,
    CONN_CHAR_DELETE_PASSWORD, /* typed YES accepted; now re-verify the account password */
    CONN_ACCOUNT_DELETE_CONFIRM,
    CONN_ACCOUNT_DELETE_PASSWORD, /* typed YES accepted; now re-verify the account password */
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
    /* Extra Descriptions ("look <keyword>" hidden detail, the roomextra
     * table) -- UNLIKE the rest of redit above, these commit to the DB
     * immediately rather than deferring to (S)ave; see room_repo.h's
     * comment on room_repo_extra_save() et al. for why. */
    CONN_REDIT_EXTRA_MENU,
    CONN_REDIT_EXTRA_ITEM,
    CONN_REDIT_EXTRA_KEYWORDS,
    CONN_REDIT_EXTRA_DESC,
    CONN_REDIT_EXTRA_DELETE_CONFIRM,
    CONN_REDIT_EXTRA_DELETE_ALL_CONFIRM,
    CONN_REDIT_CLEAR_CONFIRM,
    CONN_REDIT_QUIT_CONFIRM,
    /* Menu-driven player editor (edplayer) -- same working-copy shape as
     * redit, but the working copy is a snapshot loaded via
     * player_load_admin() (players aren't kept resident in memory like
     * rooms are), not a live pointer: (S)ave writes the snapshot back to
     * the DB tables (and syncs the live being_t too, if that player
     * happens to be online right now), (Q)uit just discards it. See
     * descriptor_edplayer_begin() and the CONN_EDPLAYER_* cases in
     * descriptor.c. */
    CONN_EDPLAYER_MENU,
    CONN_EDPLAYER_LEVEL,
    CONN_EDPLAYER_XP,
    CONN_EDPLAYER_HP,
    CONN_EDPLAYER_ATTRS,
    CONN_EDPLAYER_GENDER,
    CONN_EDPLAYER_TITLE,
    CONN_EDPLAYER_LOADROOM,
    CONN_EDPLAYER_HANDED,
    CONN_EDPLAYER_CLASS,
    CONN_EDPLAYER_RACE,
    CONN_EDPLAYER_QUIT_CONFIRM,
    /* Menu-driven zone editor (edzone, Session 43) -- same snapshot-working-
     * copy shape as edplayer (a zone isn't kept resident in memory like a
     * room is): (S)ave writes the scalar properties back; assigning/un-
     * assigning a builder applies immediately (not deferred to Save, same
     * as the old one-shot `zone assign`) since membership is an atomic
     * toggle, not something you'd want to "cancel". See
     * descriptor_edzone_begin() and the CONN_EDZONE_* cases in
     * descriptor.c. */
    CONN_EDZONE_MENU,
    CONN_EDZONE_NAME,
    CONN_EDZONE_ENABLED,
    CONN_EDZONE_LIFESPAN,
    CONN_EDZONE_RANGE,
    CONN_EDZONE_BUILDER,
    CONN_EDZONE_QUIT_CONFIRM,
    /* Menu-driven shop editor (sedit / `edit shop`, 58+) -- same snapshot-
     * working-copy shape as edzone (a shop isn't kept resident in memory):
     * targeted by the immortal's OWN current room (shop_repo_find_by_room()),
     * not a shop_nr argument, so there's no separate "no such shop" name/
     * number to type -- an immortal just walks into the shop room first.
     * (S)ave writes every scalar column (pricing, keeper, room, the six
     * canned messages, stable/repair/bank flags) back at once; the
     * accepted-item-types submenu (CONN_EDSHOP_SHOPTYPE and its ADD/REMOVE
     * children) applies each add/remove immediately instead, same
     * "membership is an atomic toggle" precedent as CONN_EDZONE_BUILDER.
     * See descriptor_edshop_begin() and the CONN_EDSHOP_* cases in
     * descriptor.c. */
    CONN_EDSHOP_MENU,
    CONN_EDSHOP_PROFIT_BUY,
    CONN_EDSHOP_PROFIT_SELL,
    CONN_EDSHOP_KEEPER,
    CONN_EDSHOP_ROOM,
    CONN_EDSHOP_NO_SUCH_ITEM1,
    CONN_EDSHOP_NO_SUCH_ITEM2,
    CONN_EDSHOP_DO_NOT_BUY,
    CONN_EDSHOP_MISSING_CASH1,
    CONN_EDSHOP_MESSAGE_BUY,
    CONN_EDSHOP_MESSAGE_SELL,
    CONN_EDSHOP_FLAGS,
    CONN_EDSHOP_SHOPTYPE,
    CONN_EDSHOP_SHOPTYPE_ADD,
    CONN_EDSHOP_SHOPTYPE_REMOVE,
    CONN_EDSHOP_QUIT_CONFIRM,
    /* Menu-driven class/race balance editor (`balance class|race <name>`,
     * user 2026-07-12: "a balance command (60) where you take args:
     * balance <class|race> that is menu driven to adjust balance
     * numbers/modifiers that will apply gamewide to the class or race
     * you just balanced"). Same working-copy shape as edzone: the
     * current class_balance/race_balance row (balance_repo.h) is copied
     * into d->balance_work on entry, field edits mutate the copy only,
     * (S)ave writes it back to the DB AND the live in-memory cache
     * (balance.h's class_balance_set()/race_balance_set() -- NOT
     * class_balance_save()/race_balance_save() directly, which would
     * leave the cache stale). See cmd_balance.c and
     * descriptor_balance_begin(). */
    CONN_BALANCE_MENU,
    CONN_BALANCE_HP_MULT,
    CONN_BALANCE_DMG_MULT,
    CONN_BALANCE_TOHIT_MOD,
    CONN_BALANCE_AC_MOD,
    CONN_BALANCE_EDIT,
    CONN_BALANCE_QUIT_CONFIRM,
    /* Menu-driven account editor (edaccount, TODO.md: "rename, password
     * reset, list chars"). Unlike edplayer/edzone/balance above, NOT a
     * working-copy-plus-Save editor -- every action here (rename, reset)
     * is a single atomic DB write applied the moment it's confirmed, same
     * reasoning as CONN_EDZONE_BUILDER's immediate apply above: there's
     * no in-between state worth letting someone "cancel" out of. See
     * descriptor_edaccount_begin() and the CONN_EDACCOUNT_* cases in
     * descriptor.c. */
    CONN_EDACCOUNT_MENU,
    CONN_EDACCOUNT_RENAME,
    CONN_EDACCOUNT_PASSWORD,
    /* Menu-driven social editor (edsocial, 55+, TODO.md's "Socials -> DB +
     * full Sneezy set + edsocial" item). Same "commits immediately, no
     * working copy" shape as the Extra Descriptions submenu (CONN_REDIT_
     * EXTRA_*) and edaccount above -- a social is a small, fully-
     * independent DB row (social_repo.h), so there's nothing worth
     * buffering; every edit calls social_cache_load() right after so the
     * change is live for the NEXT `smile`/whatever without a restart.
     * CONN_EDSOCIAL_LIST shows every social name (unpaged, same precedent
     * as show_redit_extra_menu()'s small list -- see descriptor.c) and
     * accepts a name (jump to its detail view), "new" (create one), or
     * blank (quit back to CONN_PLAYING). CONN_EDSOCIAL_ITEM is one
     * social's detail view: 8 numbered message fields plus H/P/R/D --
     * picking a message number or P enters CONN_EDSOCIAL_FIELD, a single
     * generic "current value, type new one-line value" prompt
     * (d->edsocial_field says which). H toggles `hide` (the upstream
     * act()'s per-recipient invisibility gate, sys/comm.cc -- "don't show
     * this message to someone who can't currently see the actor"; inert
     * in Tobin today since there's no invisibility system yet, but a real
     * upstream field worth exposing for editing). */
    CONN_EDSOCIAL_LIST,
    CONN_EDSOCIAL_ITEM,
    CONN_EDSOCIAL_FIELD,
    CONN_EDSOCIAL_NEW_NAME,
    CONN_EDSOCIAL_RENAME,
    CONN_EDSOCIAL_DELETE_CONFIRM,
    /* Menu-driven object-prototype editor (`edit object <vnum>`/oedit,
     * TODO.md's "NEXT UP" item -- Sneezy -> Tobin feature audit's builder-
     * tools-OLC gap). Same snapshot-working-copy shape as edzone/edplayer
     * (a prototype row isn't kept resident like a room): loaded via
     * obj_proto_load() on entry, field edits mutate d->oedit_work only,
     * (S)ave writes the whole row back via obj_proto_save(). EDIT-ONLY,
     * same scope boundary `edroom` draws around rooms -- there is no
     * "create a new object vnum" path here (a separate concern, like
     * `dig` is separate from `edit room`). Field numbering/order/labels
     * follow the real upstream's own `update_obj_menu()`
     * (misc/create_objs.cc) for familiarity, EXCEPT it renumbers
     * sequentially (1..17) rather than preserving the original's 1-8,
     * 10-21 gaps -- Tobin's other editors (edzone, edplayer) don't
     * preserve upstream slot numbers either. Three of the original's 21
     * fields are a disclosed, deliberate scope gap, not an oversight: an
     * item's `action_desc` (real upstream menu slot 9, labeled "Unused"
     * with NO case in the original's own dispatcher either -- genuinely
     * not exposed there, not merely a Tobin omission), "Extra
     * Description" (slot 15/19, needs an objextra-style per-object extra-
     * desc table Tobin doesn't have yet, unlike rooms' roomextra), and
     * "Applys" (slot 17, the objaffect stat/AC bonus rows -- its own
     * related-table submenu, like Extra Description; also real-upstream
     * wizpower-gated at 53+, a granular permission system Tobin doesn't
     * have, so not replicated here as a separate gate either). See
     * descriptor_oedit_begin() and the CONN_OEDIT_* cases in
     * descriptor.c. */
    CONN_OEDIT_MENU,
    CONN_OEDIT_NAME,
    CONN_OEDIT_SHORT_DESC,
    CONN_OEDIT_TYPE,
    CONN_OEDIT_LONG_DESC,
    CONN_OEDIT_WEIGHT,
    CONN_OEDIT_VOLUME,
    CONN_OEDIT_ACTION_FLAGS,
    CONN_OEDIT_WEAR_FLAGS,
    CONN_OEDIT_PRICE,
    CONN_OEDIT_VALUES,
    CONN_OEDIT_DECAY,
    CONN_OEDIT_MAX_STRUCT,
    CONN_OEDIT_CUR_STRUCT,
    CONN_OEDIT_MATERIAL,
    CONN_OEDIT_CAN_BE_SEEN,
    CONN_OEDIT_SPEC_PROC,
    CONN_OEDIT_MAX_EXIST,
    CONN_OEDIT_ANTI_RACE_FLAGS, /* Object anti-race flags, TODO.md priority
                                   item, 2026-08-02 -- Tobin-only, no
                                   upstream field, own submenu since
                                   action_flag's 32 bits are already fully
                                   assigned (see obj.h). */
    CONN_OEDIT_QUIT_CONFIRM,
    /* Menu-driven trigger manager (`edit trigger <room|mob|obj> <vnum>`,
     * 2026-07-25 redesign -- user: "should go into a menu driven editor
     * where you choose type with an option to delete the trigger inside
     * the menu"). Replaces the old one-shot `edit trigger <target_type>
     * <vnum> <trigger_type> [match_text|chance]` command line entirely --
     * every field is now reachable from inside the menu instead of having
     * to be front-loaded as arguments. Same "commits immediately, no
     * working copy" shape as CONN_EDSOCIAL_* (a trigger row is small and
     * fully independent, nothing worth buffering) EXCEPT the script body
     * itself, which still goes through the shared line editor (edit_kind
     * EDIT_TRIGGER) -- trig_edit_id (0 = creating new, >0 = updating that
     * existing row) tells its save handler which to do.
     * CONN_TRIGEDIT_LIST shows every trigger already on this target
     * (trigedit_target_type/vnum) and accepts a list number (jump to its
     * detail view), "A" (add a new one), or blank (quit). CONN_TRIGEDIT_
     * ITEM is one trigger's detail view: match text, chance percent,
     * "edit script" (opens the shared line editor), and delete.
     * CONN_TRIGEDIT_NEW_TYPE/_NEW_MATCH/_NEW_CHANCE walk a new trigger's
     * header fields (trigger_type, then match_text OR chance_pct
     * depending on which that type actually uses) before landing in the
     * same script editor. See descriptor_trigedit_begin() and the
     * CONN_TRIGEDIT_* cases in descriptor.c. */
    CONN_TRIGEDIT_LIST,
    CONN_TRIGEDIT_ITEM,
    CONN_TRIGEDIT_MATCH,
    CONN_TRIGEDIT_CHANCE,
    CONN_TRIGEDIT_DELETE_CONFIRM,
    CONN_TRIGEDIT_NEW_TYPE,
    CONN_TRIGEDIT_NEW_MATCH,
    CONN_TRIGEDIT_NEW_CHANCE,
    /* The script body itself, reached from CONN_TRIGEDIT_ITEM (option 3)
     * or after a new trigger's header fields are filled in. Unlike every
     * other CONN_TRIGEDIT_* state, this one owns editor_feed() directly
     * in its own switch case (descriptor.c) rather than relying on the
     * generic "if (d->edit_kind != EDIT_NONE)" interception inside
     * CONN_PLAYING -- that generic path only ever fires when d->state IS
     * CONN_PLAYING, which is true for every OTHER shared-line-editor use
     * (hedit, addnews, the old one-shot edit-trigger flow) but NOT here,
     * since the menu never leaves the CONN_TRIGEDIT_* range. Bug found
     * live (2026-07-25, user reproduced it immediately after this
     * redesign shipped): without its own case, a line typed into the
     * script body (e.g. "wait 20") fell through to whatever
     * CONN_TRIGEDIT_* state d->state still held (CONN_TRIGEDIT_NEW_CHANCE
     * in the reported repro) and got misparsed there instead of reaching
     * the editor at all. Same fix shape CONN_REDIT_DESC already uses for
     * exactly this reason (room descriptions, reached from CONN_REDIT_MENU). */
    CONN_TRIGEDIT_SCRIPT,
    /* Menu-driven mob-prototype editor (`edit mob <vnum>`/medit,
     * 2026-07-25 -- the last builder-tools-OLC gap, TODO.md). Same
     * snapshot-working-copy shape as CONN_OEDIT_* (a prototype row isn't
     * kept resident like a room is): loaded via mob_proto_load() on
     * entry, field edits mutate d->medit_work only, (S)ave writes the
     * whole row back via mob_proto_save(). EDIT-ONLY, same scope boundary
     * `edit object`/`edit room` draw -- no "create a new mob vnum" path
     * here. Field numbering/order/labels follow the real upstream's own
     * `send_mob_menu()` (misc/create_mobs.cc, 30 fields) as closely as
     * Tobin's `mob` table allows, renumbered sequentially and with
     * naturally-paired values (faction+percent, damage level+precision,
     * height+weight) combined onto one prompt each to keep the menu a
     * manageable size -- same renumbering precedent CONN_OEDIT_* already
     * set. Two real upstream fields are a disclosed gap, not an
     * oversight: "Immunities" (Tobin's `mob` table has no immunity/
     * resistance column to write to) and the real menu's own slot 15,
     * labeled "unused" with no case in ITS OWN dispatcher either. See
     * descriptor_medit_begin() and the CONN_MEDIT_* cases in
     * descriptor.c. */
    CONN_MEDIT_MENU,
    CONN_MEDIT_NAME,
    CONN_MEDIT_SHORT_DESC,
    CONN_MEDIT_LONG_DESC,
    CONN_MEDIT_DESCRIPTION,
    CONN_MEDIT_ACTIONS,
    CONN_MEDIT_AFFECTS,
    CONN_MEDIT_ATTACKS,
    CONN_MEDIT_LEVEL,
    CONN_MEDIT_HITROLL,
    CONN_MEDIT_ARMOR,
    CONN_MEDIT_HPLEVEL,
    CONN_MEDIT_DAMAGE,
    CONN_MEDIT_GOLD,
    CONN_MEDIT_RACE,
    CONN_MEDIT_SEX,
    CONN_MEDIT_MAX_EXIST,
    CONN_MEDIT_DEF_POSITION,
    CONN_MEDIT_CLASS,
    CONN_MEDIT_SIZE,
    CONN_MEDIT_VISION,
    CONN_MEDIT_CAN_BE_SEEN,
    CONN_MEDIT_SKIN,
    CONN_MEDIT_ALIGN,
    CONN_MEDIT_QUIT_CONFIRM,
    /* Menu-driven loadsuit editor (`edit suit [name]`, TODO.md priority
     * item, 2026-08-02) -- structurally closer to CONN_TRIGEDIT_* (a
     * variable-length list of child rows under one parent) than
     * CONN_OEDIT_/CONN_MEDIT_'s "many scalar fields, batched Save"
     * shape, so it follows THAT precedent instead: every field commits
     * immediately via suit_repo.h's CRUD functions, no working copy, no
     * separate Save step. Addressed by list position in what's SENT to
     * the player, but by obj_vnum (suit_item's own natural key, not a
     * synthetic row id) in what the state machine actually tracks --
     * same "no raw db ids in the UI" spirit CONN_TRIGEDIT_* already
     * established. See descriptor_edsuit_begin() and the CONN_EDSUIT_*
     * cases in descriptor.c. */
    CONN_EDSUIT_LIST,
    CONN_EDSUIT_ITEM,
    CONN_EDSUIT_ITEM_QTY,
    CONN_EDSUIT_ITEM_DELETE_CONFIRM,
    CONN_EDSUIT_ADD_VNUM,
    CONN_EDSUIT_ADD_QTY,
    CONN_EDSUIT_CLASS,
    CONN_EDSUIT_RACE, /* Race restriction (user, 2026-08-03: "add a column for
                          race next to class") -- mirrors CONN_EDSUIT_CLASS. */
    CONN_EDSUIT_DESC,
    CONN_EDSUIT_DELETE_CONFIRM, /* whole-suit delete (user, 2026-08-02:
                                   "a way to delete a suit needs to be
                                   implemented") -- distinct from
                                   CONN_EDSUIT_ITEM_DELETE_CONFIRM,
                                   which only removes one item */
    CONN_PLAYING,
    CONN_CLOSED
} conn_state_t;

/* DESC_LINE_MAX is the REAL bottleneck for "my editor paste got cut off"
 * (user 2026-07-28: pasting a room description truncated ~1/3 of the
 * way in) -- drain_lines() (descriptor.c) silently drops any character
 * past this many bytes on a single incoming line, with no warning, long
 * before a paste ever reaches an editor's own accumulation buffer
 * (edit_buf, HELP_BODY_MAX/ROOM_DESCRIPTION_MAX below -- those were
 * already generous). A pasted multi-sentence paragraph routinely arrives
 * as one long line if the client doesn't hard-wrap it. Quadrupled
 * (256->1024) alongside DESC_RAW_BUF (which must stay comfortably above
 * DESC_LINE_MAX -- it's the UNPARSED-bytes-across-reads buffer a long
 * line has to fit inside before its terminating newline arrives). */
#define DESC_RAW_BUF 4096
#define DESC_LINE_MAX 1024
/* Out-of-band subnegotiation payload accumulator (GMCP/MSDP/MSP) -- a
 * real message (e.g. GMCP's JSON) is small; this is generously sized
 * for headroom, not tuned to a real observed max like DESC_LINE_MAX
 * above. Payload bytes past this cap are silently truncated rather
 * than growing the buffer or disconnecting -- a malformed/oversized
 * inbound subnegotiation is not worth tearing down the connection
 * over (matches DESC_RAW_BUF's own "abuse protection: drop it" shape
 * in descriptor_process_input()). */
#define DESC_SUBNEG_BUF 2048

/* Real assigned telnet option numbers for GMCP/MSDP/MSP (TobinMUD
 * Client project, 2026-08-05) -- exposed here (not just as a private
 * enum inside descriptor.c) because callers outside descriptor.c need
 * them too, as the `opt` argument to descriptor_send_subneg()
 * (gmcp.c/msdp.c's builders don't know their own option number --
 * that's telnet-layer framing, not GMCP/MSDP payload content). Same
 * values every MSDP-aware client already expects, not Tobin-invented. */
#define TOBIN_TN_GMCP 201
#define TOBIN_TN_MSDP 69
#define TOBIN_TN_MSP 90

/* Outgoing-data backlog (see descriptor_write()/descriptor_flush_output()
 * in descriptor.c). Bytes that don't fit in one write() -- a full or
 * partial socket send-buffer, common under bursty output like several
 * new connections landing in the same select() tick -- wait here for the
 * game loop to retry once the socket is writable again, instead of being
 * silently dropped. */
#define DESC_OUT_BUF 65536

/* Held-message buffer (people in an editor aren't interrupted by game
 * messages -- they save up here for `catchup`, and expire after the TTL). */
#define HELD_MSG_MAX 64
#define HELD_MSG_LEN 256
#define HELD_MSG_TTL 300 /* seconds -- 5 minutes */

typedef struct descriptor {
    int fd;
    conn_state_t state;
    /* Canonical COMMANDS[] entry name cmd_dispatch() matched for the line
     * currently being processed (e.g. "edit" or "redit") -- lets a handler
     * shared between `edit <noun>` and its standalone verb print a Usage
     * line matching what was actually typed. Session 182. */
    char last_verb[16];

    /* Duplicate-character-session takeover (TODO.md priority item, user
     * 2026-07-30): set true when enter_world() reclaims this connection's
     * character for a NEW login elsewhere. Deliberately NOT destroyed
     * synchronously at that point -- enter_world() runs from inside
     * descriptor_process_input(d2) for some OTHER descriptor d2, itself
     * called from game_loop_run()'s per-descriptor while loop, which
     * caches `next = d->next` before processing each entry precisely so
     * that `d` can safely destroy ITSELF mid-iteration; freeing an
     * unrelated descriptor's memory the same way corrupts THAT cached
     * `next` if it happens to alias the freed node, a real SIGSEGV caught
     * live (game_loop.c, `next` dangling after a same-tick
     * descriptor_destroy() of a different descriptor). game_loop_run()'s
     * own loop checks this flag and safely destroys the descriptor at
     * the top of ITS OWN iteration instead, the same "cache next first"
     * pattern that already makes self-destruction safe. */
    bool pending_destroy;

    /* Peer address, captured at accept() (or inherited across a copyover
     * via the recovery file). Appears in log lines only -- immortal eyes
     * (the log command's gate); never in player-visible room messages. */
    char ip[46];

    /* Reverse-DNS hostname for `ip` (user 2026-07-11: "in messages and
     * logs where IP address is displayed, make it a hostname dns lookup
     * instead"). Resolved asynchronously (hostname_resolve.c) so a slow
     * lookup never blocks the accept()/game loop -- empty until the
     * background resolver reports back, or forever if the lookup fails.
     * Display code should always go through descriptor_display_host(),
     * which falls back to the raw `ip` while this is empty. */
    char hostname[64];

    /* raw telnet byte buffer, persists across process_input() calls since
     * reads are non-blocking and a line may arrive split across packets */
    unsigned char raw[DESC_RAW_BUF];
    int raw_len;
    int raw_pos;

    /* Output backlog -- see DESC_OUT_BUF above. Empty (out_len == 0) the
     * overwhelming majority of the time; game_loop.c only watches this fd
     * for writability while it's non-empty. */
    char out_buf[DESC_OUT_BUF];
    size_t out_len;
    /* An IAC SB ... IAC SE subnegotiation may arrive split across reads;
     * the scan state persists here so leftover payload bytes are never
     * mistaken for typed input on the next read. */
    bool in_subneg;
    unsigned char subneg_prev;
    /* GMCP/MSDP/MSP (TobinMUD Client project, 2026-08-05): real
     * WILL/WONT/DO/DONT option-state tracking -- the first the server
     * has ever done; previously those bytes were read and discarded
     * with no memory of the outcome (see descriptor.c's history). Set
     * when the client answers our on-connect `IAC WILL <opt>` offers
     * (descriptor_create()) with `IAC DO <opt>`; cleared on `IAC DONT
     * <opt>`. Gates both outbound pushes (descriptor_send_subneg()
     * callers check these first) and MSP's in-band `!!SOUND(...)`
     * markers (descriptor_send_msp_sound()). */
    bool opt_gmcp;
    bool opt_msdp;
    bool opt_msp;
    /* Whether combat_music_tick() (combat.c) currently believes this
     * descriptor has fight music playing -- lets that periodic tick
     * detect the fighting->not-fighting transition without needing a
     * hook at every one of the 30+ scattered `->fighting = ...` combat-
     * entry sites across the codebase. */
    bool music_playing;
    int last_music_track; /* index into combat_music_tick()'s TRACKS[] --
                            * -1 until the first pick, so the next-track
                            * roll can exclude whatever played last and
                            * never repeat back-to-back (user, 2026-08-06). */
    /* Real subnegotiation payload capture, replacing the old discard-
     * only swallow loop. `subneg_have_opt` is false immediately after
     * `IAC SB` until the very next payload byte arrives (that byte IS
     * the option id, not payload -- captured into `subneg_opt` and
     * consumed, never appended to `subneg_buf`). Everything after that
     * up to the `IAC SE` terminator accumulates in `subneg_buf`,
     * capped at DESC_SUBNEG_BUF (see its own comment on truncation).
     * All of this persists across process_input() calls exactly like
     * `in_subneg`/`subneg_prev` already did, for the same "a real
     * subnegotiation can arrive split across two TCP reads" reason. */
    bool subneg_have_opt;
    unsigned char subneg_opt;
    unsigned char subneg_buf[DESC_SUBNEG_BUF];
    int subneg_len;
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

    /* Whether the character list has been revealed this visit to the menu
     * (user: "hide the character list until C"). False on every FRESH
     * arrival at the menu (login, quit!-while-playing, a cancelled
     * creation/deletion, ...); a bare `C` flips it true for the rest of
     * this visit, so a later redisplay (after a typo, say) doesn't hide it
     * again. `C <number|name>` (an already-known target) still connects
     * directly without ever needing to reveal the list. */
    bool char_list_shown;

    /* CONN_CHAR_CREATE_NAME / CONN_CHAR_CREATE_ATTRS / CONN_CHAR_CREATE_RACE /
     * CONN_CHAR_CREATE_CLASS / CONN_CHAR_CREATE_OPTIONS scratch. */
    char new_char_name[PLAYER_NAME_LEN];
    attrs_t new_char_attrs;
    int new_char_attr_pick; /* 1-6, which attribute a numbered pick (CONN_CHAR_CREATE_ATTRS)
                                is waiting on an amount for (CONN_CHAR_CREATE_ATTR_AMOUNT) */
    int new_char_handed; /* 1 right (default), 0 left */
    gender_t new_char_gender; /* GENDER_NEUTER default */
    char new_char_appearance[BEING_APPEARANCE_LEN]; /* empty default */
    player_race_t new_char_race;   /* chosen in CONN_CHAR_CREATE_RACE */
    player_territory_t new_char_territory; /* chosen in CONN_CHAR_CREATE_TERRITORY */
    player_class_t new_char_class; /* chosen in CONN_CHAR_CREATE_CLASS */
    int new_char_alignment;        /* chosen in CONN_CHAR_CREATE_OPTIONS' alignment
                                       sub-menu: -500/0/500, defaults 0 (neutral) */

    /* CONN_CHAR_DELETE_CONFIRM scratch. */
    char delete_char_name[PLAYER_NAME_LEN];

    /* Shared line-editor state (the classic DikuMUD string editor: "."
     * alone saves, "~" alone aborts, anything else appends). cmd_hedit.c
     * arms it for help topics, cmd_edit.c for room descriptions;
     * descriptor.c's CONN_PLAYING case intercepts every line while
     * edit_kind != EDIT_NONE and routes the save by kind. */
    enum { EDIT_NONE = 0, EDIT_HELP_TOPIC, EDIT_ROOM_DESC, EDIT_NEWS,
           EDIT_WIZNEWS, EDIT_RULES, EDIT_TRIGGER } edit_kind;
    char edit_topic[HELP_TOPIC_NAME_LEN]; /* EDIT_HELP_TOPIC target */
    int edit_room_vnum;                   /* EDIT_ROOM_DESC target */
    char news_title[128];                 /* EDIT_NEWS headline (addnews); reused as EDIT_RULES title */
    int rule_num;                         /* EDIT_RULES target rule number (edrules) */
    char edit_buf[HELP_BODY_MAX];         /* == ROOM_DESCRIPTION_MAX, see room.h */
    int edit_len;
    /* EDIT_HELP_TOPIC only: the topic's "Related: ..." footer, set with
     * the editor's `/r <topics>` command instead of typing a literal
     * "Related:" line into the body (user 2026-07-11: "in the help editor
     * we should be able to set related topics in there"). Preloaded from
     * an existing topic's trailing Related line, if it has one; appended
     * back onto edit_buf on save (see descriptor.c's EDIT_HELP_TOPIC save
     * branch). Empty means no Related footer. */
    char edit_related[128];

    /* EDIT_TRIGGER scratch, shared by the CONN_TRIGEDIT_* menu (2026-07-25
     * redesign) -- these header fields are captured before dropping into
     * the shared line editor for the script body itself (edit_buf,
     * above); the save handler uses trig_edit_id to decide whether to
     * INSERT a new trigger row (0) or UPDATE an existing one's script
     * (>0, see trigger_repo_update_script()). */
    char trig_target_type[8];
    int trig_target_vnum;
    char trig_trigger_type[16];
    char trig_match_text[TRIGGER_MATCH_LEN];
    int trig_chance_pct;
    long trig_edit_id;

    /* CONN_TRIGEDIT_LIST/_ITEM/etc. scratch -- which target the menu is
     * open on (trigedit_target_type/vnum, set once at descriptor_
     * trigedit_begin()) and which trigger's detail view is current
     * (trig_edit_id, reused above -- same field, since the item view and
     * the script editor's save-routing need the exact same "which row"
     * value). */
    char trigedit_target_type[8];
    int trigedit_target_vnum;

    /* CONN_EDSUIT_* scratch (menu-driven loadsuit editor, TODO.md priority
     * item, 2026-08-02) -- edsuit_id is set once at descriptor_
     * edsuit_begin() and stays fixed for the whole editing session;
     * edsuit_item_vnum tracks which item's detail view is current
     * (CONN_EDSUIT_ITEM/_ITEM_QTY/_ITEM_DELETE_CONFIRM); edsuit_add_vnum
     * carries the new item's vnum from CONN_EDSUIT_ADD_VNUM across to
     * CONN_EDSUIT_ADD_QTY. No working-copy struct needed -- every field
     * commits immediately via suit_repo.h, same as CONN_TRIGEDIT_*. */
    int edsuit_id;
    int edsuit_item_vnum;
    int edsuit_add_vnum;

    /* Output pager (the `news` command): long output is buffered here and
     * released one page (page_size lines) at a time. While page_len > 0 the
     * CONN_PLAYING input handler consumes each line to advance (ENTER) or
     * stop (Q) instead of running a command.
     *
     * Bug found 2026-07-19 (user: "wiznews bug with the pager/long output
     * that freezes the mud"): descriptor_page_start() copies its whole
     * source string into this buffer via a bounded snprintf -- silently
     * TRUNCATING anything longer, no matter how big the caller's own
     * source buffer was. cmd_news.c/cmd_wiznews.c already size their own
     * `full` buffer at 101000 bytes (a previous, real fix for a growing
     * feed that used to overflow at 15000/16000) -- but that fix never
     * reached here, so a feed long enough to fill this 16384-byte cap
     * still got cut off mid-sentence, silently, with no indication to the
     * reader that anything was missing (not a true infinite hang, but a
     * broken, seemingly-stuck-mid-page experience easily read as "the mud
     * froze"). Sized to comfortably clear cmd_news.c/cmd_wiznews.c's own
     * 101000-byte ceiling with real margin for future growth, same
     * "size for the feed's own growth, not just today's content"
     * reasoning that produced 101000 in the first place. */
    char page_buf[131072];
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

    /* Extra Descriptions submenu scratch state (CONN_REDIT_EXTRA_*). Commits
     * immediately, not part of the working-copy/dirty model above -- see
     * room_repo.h's room_repo_extra_save() comment. redit_extra_name holds
     * the roomextra.name (primary key) of the item CONN_REDIT_EXTRA_ITEM/
     * _DESC/_DELETE_CONFIRM are currently acting on; empty means "adding a
     * new one" while in CONN_REDIT_EXTRA_KEYWORDS. */
    char redit_extra_name[ROOM_EXTRA_NAME_LEN];
    bool redit_extra_is_new; /* CONN_REDIT_EXTRA_DESC: true if this description
                                 editor was entered via "Add new" (aborting
                                 should return to the list -- the row was never
                                 saved) rather than "edit description" on an
                                 already-existing item (aborting should return
                                 to that item's detail view instead). */

    /* Menu-driven player editor working copy (CONN_EDPLAYER_*). Unlike
     * redit_work this is a DB snapshot (player_load_admin()), not a live
     * pointer -- see the CONN_EDPLAYER_MENU enum comment. edplayer_work's
     * own account_id/player_id fields identify which row (S)ave writes
     * back to; edplayer_load_room holds player.load_room, which isn't part
     * of being_t. */
    being_t edplayer_work;
    int edplayer_load_room;
    bool edplayer_dirty;

    /* Menu-driven zone editor working copy (CONN_EDZONE_*, Session 43).
     * DB snapshot (zone_repo_load_one()), same shape as edplayer_work.
     * Builder assignment is NOT part of the working copy -- it applies
     * immediately against zone_owner (an atomic toggle, see the
     * CONN_EDZONE_MENU enum comment), so only the scalar properties
     * (name/enabled/lifespan/bottom/top) participate in dirty/Save. */
    zone_t edzone_work;
    bool edzone_dirty;

    /* Menu-driven shop editor working copy (CONN_EDSHOP_*). DB snapshot
     * (shop_repo_find_by_room()), same shape as edzone_work -- a shop row
     * isn't kept resident like a room is. Accepted-item-types (shoptype)
     * membership is NOT part of the working copy -- same "applies
     * immediately, atomic toggle" precedent as edzone_work's builder
     * assignment above -- so only the scalar shop_t columns participate
     * in dirty/Save. */
    shop_t edshop_work;
    bool edshop_dirty;

    /* Menu-driven object-prototype editor working copy (CONN_OEDIT_*).
     * DB snapshot (obj_proto_load()), same shape as edzone_work -- a
     * prototype row isn't kept resident like a room is. */
    obj_proto_t oedit_work;
    bool oedit_dirty;

    /* Menu-driven mob-prototype editor working copy (CONN_MEDIT_*,
     * `edit mob <vnum>`, 2026-07-25 -- the last builder-tools-OLC gap).
     * DB snapshot (mob_proto_load()), same shape as oedit_work above --
     * medit_vnum is threaded separately since mob_proto_save() takes it
     * as its own argument rather than storing vnum inside mob_proto_t. */
    int medit_vnum;
    mob_proto_t medit_work;
    bool medit_dirty;

    /* Menu-driven balance editor working copy (CONN_BALANCE_*, user
     * 2026-07-12). balance_is_class picks which table (true =
     * class_balance, false = race_balance); balance_index is the
     * player_class_t/player_race_t value within it. */
    bool balance_is_class;
    int balance_index;
    balance_mod_t balance_work;
    bool balance_dirty;
    int balance_field;  /* which menu field CONN_BALANCE_EDIT is editing (1-17) */

    /* Menu-driven account editor (CONN_EDACCOUNT_*, TODO.md: "rename,
     * password reset, list chars"). Just the id, not a working-copy
     * struct -- every action commits immediately (see the CONN_EDACCOUNT_
     * MENU enum comment), so the menu re-reads the row fresh
     * (account_load_by_id()) rather than caching a copy that could go
     * stale the moment a rename lands. */
    long edaccount_id;

    /* Menu-driven social editor (CONN_EDSOCIAL_*, see the enum comment
     * above). Just the name, not a working-copy struct -- every action
     * commits immediately, so the item menu re-reads the row fresh
     * (social_repo_get()) rather than caching a copy that could go stale
     * the moment a field edit lands. edsocial_field is which of the 8
     * message fields (1-8) CONN_EDSOCIAL_FIELD is currently editing;
     * 0 means "editing the minimum position instead" (a name, not a
     * message, so it shares the same generic single-line prompt state
     * without needing its own separate one). */
    char edsocial_name[SOCIAL_NAME_LEN];
    int edsocial_field;

    being_t *character;

    /* `possess`/`return` (60+, cmd_possess.c -- Sneezy → Tobin feature
     * audit, "Switch / return (puppet a mob)"): while possessing a mob,
     * `character` points at the mob and this holds the immortal's own PC,
     * temporarily desc==NULL (same "linkdead" shape a real disconnect
     * already leaves a body in -- no new state needed to represent it).
     * NULL when not possessing anything. Sneezy's own admin switch (not
     * the spell-driven polymorph flavor, which Tobin doesn't have yet)
     * does no stat transfer and no visual message -- this mirrors that
     * exactly: a raw descriptor-pointer swap, nothing else. */
    being_t *possess_original;

    /* Time (epoch seconds) of the last input line -- who shows "(idle)" after
     * 5 minutes with no input; any command resets it. */
    long last_active;

    /* `reply` (cmd_reply.c, 2026-07-26 docs/systems review -- original's
     * `desc->last_teller`): who most recently `tell`'d this descriptor,
     * so `reply <message>` doesn't need the name retyped. Live descriptor
     * state only, same as the original -- NOT persisted (empty after a
     * fresh reconnect, matching what `tellhistory` alone can't recover:
     * the original's own `last_teller` is exactly this transient). */
    char last_teller[PLAYER_NAME_LEN];

    /* `toggle notell`'s exception (PLR_NOTELL, being.h -- original's
     * `desc->last_told`): who this descriptor most recently `tell`'d.
     * Checked on the RECIPIENT side of a blocked tell -- if the sender's
     * name matches the recipient's OWN last_told, the tell gets through
     * even with PLR_NOTELL set, so a conversation you started yourself
     * still gets a reply. Live descriptor state only, same as
     * last_teller above. */
    char last_told[PLAYER_NAME_LEN];

    /* Held messages: while this connection is in an editor, asynchronous game
     * messages (says, combat, arrivals, broadcasts) are buffered here instead
     * of interrupting the editor. Reviewed with `catchup`; anything older than
     * HELD_MSG_TTL is dropped by the descriptor_held_expire() pulse. */
    struct { long when; char text[HELD_MSG_LEN]; } held[HELD_MSG_MAX];
    int held_count;

    /* `snoop` (59+, cmd_snoop.c): one outgoing snoop per descriptor at a
     * time. `snoop_target` is who I'm watching (NULL if none); `snooped_by`
     * is who's watching ME (NULL if nobody). descriptor_send() mirrors
     * everything sent to a snooped descriptor over to its watcher, and the
     * CONN_PLAYING input handler mirrors the snooped player's own typed
     * lines too (prefixed "% "), matching classic DikuMUD/Sneezy snoop.
     * Both pointers are unhooked in descriptor_destroy() so neither side
     * ever ends up pointing at a freed descriptor. */
    struct descriptor *snoop_target;
    struct descriptor *snooped_by;

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

/* Sends a raw out-of-band subnegotiation packet: `IAC SB <opt> <payload>
 * IAC SE`. Bypasses descriptor_send()'s color-tag translation and CRLF
 * normalization entirely -- both assume printable client-displayed
 * text, and would corrupt GMCP's JSON or MSDP's binary VAR/VAL framing.
 * Any literal 0xFF (TN_IAC) byte inside `payload` is doubled (IAC IAC),
 * the standard telnet escape, so a client's own IAC-aware parser never
 * mistakes payload content for the terminator. Caller is responsible
 * for checking the matching `d->opt_gmcp`/`opt_msdp` flag first -- this
 * function sends unconditionally. */
void descriptor_send_subneg(descriptor_t *d, unsigned char opt, const unsigned char *payload, size_t len);

/* MSP (2026-08-05): NOT a subnegotiation -- a plain in-band text marker
 * (`!!SOUND(file V=vol)\r\n`) understood by an MSP-aware client once it
 * has ack'd `IAC DO MSP` (d->opt_msp). Routed through descriptor_send()
 * like any other text (color/CRLF pipeline is harmless here -- the
 * marker contains no '<X>' tags or bare '\n'). No-op if the descriptor
 * never opted in. `volume` is clamped to MSP's real 0-100 range. */
void descriptor_send_msp_sound(descriptor_t *d, const char *filename, int volume);

/* Combat music (user, 2026-08-05: "random fight music that will stop
 * when the fight is over") -- real MSP `!!MUSIC(...)` (NOT `!!SOUND`,
 * a separate marker in the real MSP spec, meant exactly for looping
 * background audio a client can be told to stop). `descriptor_send_
 * msp_music()` loops (L=-1); `descriptor_send_msp_music_off()` sends
 * the literal `!!MUSIC(Off)` stop marker. See combat_music_tick()
 * (combat.h) for what actually calls these. */
void descriptor_send_msp_music(descriptor_t *d, const char *filename);
void descriptor_send_msp_music_off(descriptor_t *d);

/* Queues already-formatted bytes for `d`, replacing a bare socket_write().
 * Tries an immediate write() first (the common case never touches
 * out_buf); whatever doesn't go out -- a partial write or EAGAIN -- is
 * buffered for descriptor_flush_output() to retry once the socket is
 * writable again, instead of being silently discarded. */
void descriptor_write(descriptor_t *d, const char *data, size_t len);

/* Retries whatever's buffered in d->out_buf once game_loop.c's select()
 * reports the fd writable. Returns false on a hard write error (the
 * connection is dead) -- caller should descriptor_destroy(), same
 * contract as descriptor_process_input(). */
bool descriptor_flush_output(descriptor_t *d);

/* The resolved reverse-DNS hostname for this connection, or the raw IP if
 * the lookup hasn't finished (or failed) yet -- every log line and
 * immortal-facing display (`users`, link-drop/connect logging) should go
 * through this instead of reading `ip` directly (user 2026-07-11: "in
 * messages and logs where IP address is displayed, make it a hostname
 * dns lookup instead"; see hostname_resolve.h). */
const char *descriptor_display_host(const descriptor_t *d);

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

/* Opens the menu-driven player editor on the player named `name` (any
 * account, per player_load_admin()), copies their persisted fields into
 * the descriptor's working copy, and shows the edplayer menu (entering
 * CONN_EDPLAYER_MENU). Returns false if no such player exists. Caller
 * (cmd_pedit.c) owns the level gate. */
bool descriptor_edplayer_begin(descriptor_t *d, const char *name);

/* Opens the menu-driven zone editor on zone `zone_nr`, copies its DB row
 * into the descriptor's working copy, and shows the edzone menu (entering
 * CONN_EDZONE_MENU). Returns false if no such zone exists. Caller
 * (cmd_edzone.c) owns the level gate + zone_can_edit() ownership check. */
bool descriptor_edzone_begin(descriptor_t *d, int zone_nr);

/* Opens the menu-driven shop editor on the shop operating out of room
 * `room_vnum` (the immortal's own current room -- sedit takes no target
 * argument), copies its DB row into the descriptor's working copy, and
 * shows the edshop menu (entering CONN_EDSHOP_MENU). Returns false if
 * room_vnum has no shop. Caller (cmd_sedit.c) owns the level gate. */
bool descriptor_edshop_begin(descriptor_t *d, int room_vnum);

/* Opens the menu-driven object-prototype editor on `vnum`, copies its DB
 * row into the descriptor's working copy, and shows the oedit menu
 * (entering CONN_OEDIT_MENU). Returns false if no such prototype exists.
 * Caller (cmd_edobject.c) owns the level gate. */
bool descriptor_oedit_begin(descriptor_t *d, int vnum);

/* Opens the menu-driven mob-prototype editor on `vnum`, copies its DB row
 * into the descriptor's working copy, and shows the medit menu (entering
 * CONN_MEDIT_MENU). Returns false if no such prototype exists. Caller
 * (cmd_edmobile.c) owns the level gate. */
bool descriptor_medit_begin(descriptor_t *d, int vnum);

/* Opens the menu-driven balance editor on class `cls` (if `is_class`) or
 * race `race_val` (if not), copies its current class_balance/race_balance
 * row into the descriptor's working copy, and shows the balance menu
 * (entering CONN_BALANCE_MENU). `cls`/`race_val` is the SAME int either
 * way -- caller (cmd_balance.c) passes whichever is meaningful; the
 * other is ignored. Caller owns the level gate. */
bool descriptor_balance_begin(descriptor_t *d, bool is_class, int index);

/* Opens the menu-driven account editor on the account named `name`
 * (rename, password reset, list its characters -- TODO.md), entering
 * CONN_EDACCOUNT_MENU. Returns false if no such account exists. Caller
 * (cmd_accedit.c) owns the level gate. */
bool descriptor_edaccount_begin(descriptor_t *d, const char *name);

/* Opens the menu-driven social editor. If `name` is non-empty and matches
 * an existing social exactly (case-insensitive), jumps straight to that
 * social's detail view (CONN_EDSOCIAL_ITEM); otherwise (empty, or no
 * match) shows the full list (CONN_EDSOCIAL_LIST) where a name can be
 * typed to edit it or "new" to create one. Always succeeds (there's
 * always at least the list to show) -- unlike the other _begin()
 * functions, no bool return; nothing here is a "no such X" failure case
 * the caller (cmd_edit.c) needs to report. */
void descriptor_edsocial_begin(descriptor_t *d, const char *name);

/* Opens the menu-driven trigger manager on `target_type`/`target_vnum`
 * (`edit trigger <room|mob|obj> <vnum>`, 2026-07-25 redesign), entering
 * CONN_TRIGEDIT_LIST. Always succeeds (an empty trigger list is a valid,
 * normal state -- there's nothing to fail on). Caller (cmd_edtrigger.c)
 * owns the level gate and the room-target zone_can_edit() check. */
void descriptor_trigedit_begin(descriptor_t *d, const char *target_type, int target_vnum);

/* Opens the menu-driven loadsuit editor on `suit_id` (`edit suit
 * [name]`, TODO.md priority item, 2026-08-02), entering CONN_EDSUIT_LIST.
 * Always succeeds (an empty item list is a valid, normal state -- same
 * as an empty trigger list above). Caller (cmd_suitedit.c) owns the level
 * gate and confirms `suit_id` is real (or has just created it) first. */
void descriptor_edsuit_begin(descriptor_t *d, int suit_id);

/* Sends `msg` to every connected player in room `r` except `except` (may
 * be NULL to include everyone). Shared by movement, quit/link-drop, and
 * combat announcements. Recipients in an editor have it held, not sent. */
void descriptor_room_echo(struct room *r, being_t *except, const char *msg);

/* Delivers an ambient/theme game message (room echoes, combat, mob AI,
 * weather, object actions, ...): sent immediately if the connection is
 * free, or silently DROPPED (not held/replayed) if it's in an editor --
 * `catchup` only replays real communication (user 2026-07-26: "catchup
 * command should only record communications not theme messages"), see
 * descriptor_notify_comm() below for that. Async senders that aren't
 * genuine player-to-player/immortal communication use this instead of
 * descriptor_send so nobody's editing is interrupted. */
void descriptor_notify(descriptor_t *d, const char *msg);

/* Delivers a real communication message (tell, say, shout, whisper,
 * wiznet, the `system` broadcast, the newbie channel, direct group
 * notices): sent immediately if the connection is free, or held
 * (buffered for `catchup`) if it's in an editor. Use this, not the
 * plain descriptor_notify() above, for anything that's genuinely
 * player-to-player/immortal communication rather than ambient flavor. */
void descriptor_notify_comm(descriptor_t *d, const char *msg);

/* True while `d` is inside any editor (shared line editor or a menu-driven
 * one -- redit/edplayer/edzone). Exported so command handlers that
 * enumerate online characters (who, promote, set, copyover) can tell a
 * genuinely-offline connection apart from one that's just mid-edit, rather
 * than relying on `state == CONN_PLAYING` (which excludes every editor
 * sub-state and so undercounts who's actually online). */
bool descriptor_in_editor(const descriptor_t *d);

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

/* Seconds idle before a connection is considered "idle" for display
 * purposes -- `who`'s "(idle)" tag (cmd_who.c) and the PLR_AFK auto-notice
 * on an incoming tell (cmd_tell.c/cmd_reply.c, being.h's PLR_AFK comment)
 * both use this same 5-minute threshold. Separate from
 * IDLE_DISCONNECT_SECS on purpose -- "looks idle" and "gets disconnected"
 * are very different thresholds. */
#define IDLE_DISPLAY_SECS 300

/* Pulse callback: disconnects mortals idle past IDLE_DISCONNECT_SECS; leaves
 * immortals connected. Registered in main.c. */
void descriptor_idle_timeout(long pulse_num);

/* Unloads the current character (freed, removed from its room) and returns
 * this connection to the account menu -- used by the `quit!` command while
 * playing. Does NOT close the socket; see cmd_quit.c (playing) vs. the
 * CONN_ACCOUNT_MENU `quit!` handling in descriptor.c (actually disconnects). */
void descriptor_leave_to_menu(descriptor_t *d);

#endif
