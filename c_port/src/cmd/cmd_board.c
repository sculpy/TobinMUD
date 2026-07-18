/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "board_repo.h"
#include "obj.h"
#include "thing.h"

/* `read`/`write` -- bulletin boards (Sneezy port, user 2026-07-18: "we
 * need to make bulletin boards function, read and write commands, from
 * sneezy"). Real seeded ITEM_BOARD objects (obj.type=24, e.g. "board
 * bulletin galek brightmoon") sit in real rooms across the world; the
 * `board_message` table (already-imported upstream schema, FK'd to
 * obj.vnum) holds their posts.
 *
 * `read` (no board object needed to specify -- same "the thing in this
 * room" convention cmd_shop.c's shopkeeper lookup uses) with no argument
 * lists every live post on the board standing here; `read <#>` shows one
 * in full. `write <subject> <message>` posts directly to it -- a
 * deliberate simplification of the original's two-step "write a note,
 * then post the note" flow (see board_repo.h's header comment): Tobin has
 * no separate writable-note-object system yet, so this just inserts the
 * message straight into board_message, same net result with one command
 * instead of two. Per-board minimum level (the real seeded `obj.val0`,
 * e.g. 52 for "board bulletin wizard immortal") gates BOTH commands, with
 * no immortal bypass -- matches the original's own boardHandler, which
 * checks getBoardLevel() > ch->GetMaxLevel() unconditionally. Faction-
 * gated boards (Brotherhood/Serpent/Logrus in the original) are skipped
 * entirely -- Tobin has no faction system ("we will not support
 * factions", cmd_stat.c). */

/* Same keyword-abbreviation matching spirit as cmd_drink.c's own local
 * copy -- duplicated rather than shared, same precedent as that file's
 * own comment on cap_first(). */
static bool keyword_matches(const char *keywords, const char *tok) {
    size_t tok_len = strlen(tok);
    if (tok_len == 0)
        return false;
    const char *p = keywords;
    while (*p) {
        while (*p == ' ')
            p++;
        const char *start = p;
        while (*p && *p != ' ')
            p++;
        size_t wlen = (size_t)(p - start);
        if (wlen >= tok_len && strncasecmp(start, tok, tok_len) == 0)
            return true;
    }
    return false;
}

/* Finds a board (OBJ_CAT_WRITTEN + keyworded "board", same marker every
 * real seeded ITEM_BOARD object's `name` column carries) in `ch`'s
 * current room -- `filter` (if non-NULL) additionally narrows to one
 * whose own keywords match it (e.g. "wiz" for "the wiz board"), same
 * substring convention as cmd_drink.c's puddle lookup. Returns the first
 * match either way. */
static obj_t *find_board(const being_t *ch, const char *filter) {
    if (!ch->base.roomp)
        return NULL;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (o->category != OBJ_CAT_WRITTEN || !keyword_matches(o->base.name, "board"))
            continue;
        if (filter && !keyword_matches(o->base.name, filter))
            continue;
        return o;
    }
    return NULL;
}

/* The Nth board (room order) -- "read 2.board" / "write 2.board ..."
 * (user 2026-07-18: "make it true as part of everything that can exist"),
 * the same ordinal convention `look`/`kill`/`get` use everywhere else,
 * offered here alongside (not instead of) "at <name>" -- either
 * disambiguates a multi-board room, whichever's easier to type. */
static obj_t *find_board_ordinal(const being_t *ch, int ordinal) {
    if (!ch->base.roomp)
        return NULL;
    int seen = 0;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (o->category != OBJ_CAT_WRITTEN || !keyword_matches(o->base.name, "board"))
            continue;
        seen++;
        if (seen == ordinal)
            return o;
    }
    return NULL;
}

/* How many DISTINCT boards are in `ch`'s current room -- some rooms (a
 * builder office with both a Wizard board and a plain bulletin board, see
 * user 2026-07-18 bug report) have more than one, so a bare `read`/`write`
 * with no board named is only safe to resolve automatically when there's
 * exactly one candidate. */
static int count_boards(const being_t *ch) {
    if (!ch->base.roomp)
        return 0;
    int n = 0;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (o->category == OBJ_CAT_WRITTEN && keyword_matches(o->base.name, "board"))
            n++;
    }
    return n;
}

static bool is_all_digits(const char *s) {
    if (!*s)
        return false;
    for (; *s; s++)
        if (!isdigit((unsigned char)*s))
            return false;
    return true;
}

static const char *board_label(const obj_t *board) {
    return board->base.short_descr[0] ? board->base.short_descr : board->base.name;
}

/* `read [<#>]` / `write <subject> <message>` normally, or when the room
 * has more than one board (user 2026-07-18 bug report: a builder office
 * with both a Wizard board and a plain bulletin board silently read/
 * posted to whichever one happened to be first in the room, not
 * necessarily the one meant) either `read 2.board [<#>]` / `write
 * 2.board <subject> <message>` (the same "N.keyword" ordinal convention
 * `look`/`kill`/`get` use everywhere else -- user 2026-07-18: "make it
 * true as part of everything that can exist") or `read at <boardname>
 * [<#>]` / `write at <boardname> <subject> <message>`, whichever's
 * easier to type. Both forms are only ever RECOGNIZED when there's real
 * ambiguity to resolve -- with just one board present, an ordinary post
 * whose subject happens to be the word "at" (or start with a number)
 * still works fine, since there's nothing to disambiguate and the
 * ordinary path never inspects the first word at all.
 *
 * Returns NULL (having already sent an error) if no board could be
 * resolved. `*rest` is left pointing at whatever comes after the
 * disambiguation prefix, or unchanged (still `args`) if there wasn't one. */
static obj_t *resolve_board(descriptor_t *d, being_t *ch, const char *args, const char **rest,
                             const char *verb) {
    *rest = args;

    int count = count_boards(ch);
    if (count == 0) {
        char msg[64];
        snprintf(msg, sizeof(msg), "There's no board here to %s.\r\n", verb);
        descriptor_send(d, msg);
        return NULL;
    }
    if (count == 1)
        return find_board(ch, NULL);

    char first[16] = "";
    int consumed = 0;
    sscanf(args, "%15s %n", first, &consumed);

    const char *ord_rest;
    int ordinal = thing_parse_ordinal(first, &ord_rest);
    if (ordinal > 1) {
        obj_t *board = find_board_ordinal(ch, ordinal);
        if (!board) {
            descriptor_send(d, "There aren't that many boards here.\r\n");
            return NULL;
        }
        *rest = args + consumed;
        return board;
    }

    if (strcasecmp(first, "at") != 0) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "There's more than one board here -- %s 2.board or %s at <board name> ...\r\n",
                 verb, verb);
        descriptor_send(d, msg);
        return NULL;
    }

    char name[BOARD_SUBJECT_LEN] = "";
    int consumed2 = 0;
    sscanf(args + consumed, "%79s %n", name, &consumed2);
    if (!name[0]) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Usage: %s at <board name> ...\r\n", verb);
        descriptor_send(d, msg);
        return NULL;
    }
    obj_t *board = find_board(ch, name);
    if (!board) {
        descriptor_send(d, "There's no board like that here.\r\n");
        return NULL;
    }
    *rest = args + consumed + consumed2;
    return board;
}

bool cmd_read(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    const char *rest;
    obj_t *board = resolve_board(d, ch, args, &rest, "read");
    if (!board)
        return true;

    if (ch->progress.level < board->val[0]) {
        char msg[160];
        snprintf(msg, sizeof(msg), "You are too lowly to read %s.\r\n", board_label(board));
        descriptor_send(d, msg);
        return true;
    }

    char tok[32] = "";
    sscanf(rest, "%31s", tok);

    if (!*tok) {
        board_post_summary_t posts[BOARD_LIST_MAX];
        int n = board_repo_list(board->vnum, posts, BOARD_LIST_MAX);

        char out[8192];
        size_t len = 0;
        len += (size_t)snprintf(out + len, sizeof(out) - len, "\r\n<c>-- %s --<z>\r\n", board_label(board));
        len += (size_t)snprintf(out + len, sizeof(out) - len,
                                "You can <g>write <subject> <message><z>, or <g>read <#><z> below.\r\n\r\n");
        if (n == 0) {
            len += (size_t)snprintf(out + len, sizeof(out) - len, "  (no messages posted yet)\r\n");
        } else {
            for (int i = 0; i < n && len < sizeof(out); i++)
                len += (size_t)snprintf(out + len, sizeof(out) - len, "  %2d) [%s] %s (%s)\r\n",
                                        posts[i].post_num, posts[i].date_posted, posts[i].subject, posts[i].author);
        }
        descriptor_page_start(d, out, 0);
        return true;
    }

    if (!is_all_digits(tok)) {
        descriptor_send(d, "Usage: read [<#>]\r\n");
        return true;
    }

    int post_num = atoi(tok);
    char subject[BOARD_SUBJECT_LEN], author[BOARD_AUTHOR_LEN], body[BOARD_POST_LEN], date[BOARD_DATE_LEN];
    if (post_num < 1
        || !board_repo_read(board->vnum, post_num, subject, sizeof(subject), author, sizeof(author),
                             body, sizeof(body), date, sizeof(date))) {
        descriptor_send(d, "That message exists only in your imagination...\r\n");
        return true;
    }

    char out[BOARD_POST_LEN + 256];
    snprintf(out, sizeof(out), "\r\nMessage %d : [%s] %s (%s)\r\n\r\n%s\r\n\r\nEnd of message %d.\r\n",
             post_num, date, subject, author, body, post_num);
    descriptor_page_start(d, out, 0);
    return true;
}

bool cmd_write(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    const char *rest;
    obj_t *board = resolve_board(d, ch, args, &rest, "write");
    if (!board)
        return true;

    if (ch->progress.level < board->val[0]) {
        char msg[160];
        snprintf(msg, sizeof(msg), "You are too lowly to use %s.\r\n", board_label(board));
        descriptor_send(d, msg);
        return true;
    }

    char subject[BOARD_SUBJECT_LEN] = "";
    int consumed = 0;
    if (sscanf(rest, "%79s %n", subject, &consumed) < 1 || !subject[0]) {
        descriptor_send(d, "Usage: write <subject> <message>\r\n");
        return true;
    }
    const char *body = rest + consumed;
    while (*body == ' ')
        body++;
    if (!*body) {
        descriptor_send(d, "Usage: write <subject> <message>\r\n");
        return true;
    }
    if (strcasecmp(subject, "board") == 0) {
        descriptor_send(d, "You may not use 'board' as a subject. Please use another.\r\n");
        return true;
    }

    if (!board_repo_post(board->vnum, subject, ch->base.name, body)) {
        descriptor_send(d, "Something went wrong posting your message.\r\n");
        return true;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "You post your message on %s.\r\n", board_label(board));
    descriptor_send(d, msg);
    snprintf(msg, sizeof(msg), "%s writes a message on %s.\r\n", ch->base.name, board_label(board));
    descriptor_room_echo(ch->base.roomp, ch, msg);
    return true;
}
