/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "db.h"
#include "mob_ai.h"
#include "obj.h"
#include "player_repo.h"
#include "room.h"

/* `stat obj|mob|room <vnum>` (Sneezy port, user 2026-07-12: "add stat
 * command so an immortal of level 55+ can see everything about the mob
 * obj or room with a vnum argument"). Unlike `vnum` (cmd_vnum.c), which
 * searches by name/keyword and shows one summary line per match, `stat`
 * takes an exact vnum and dumps EVERY column of that one row -- a
 * generic "<column>: <value>" listing built from `db_col_count()`/
 * `db_col_name()` (new, db.c) rather than hardcoding each table's ~20-40
 * field names by hand, so it can never silently go stale as columns are
 * added later. A room additionally lists its exits (roomexit table); an
 * object additionally lists any objaffect rows (hitroll/damroll/AC
 * modifiers etc).
 *
 * Follow-up (user 2026-07-12): raw numbers weren't good enough for
 * everything -- bit-flag columns (obj's `wear_flag`, mob's `actions`)
 * and enum-ish columns (mob's `class`/`race`) are decoded to readable
 * text instead of the generic dump for those specific columns
 * (SKIP_COLS below), and mob's `faction`/`fact_perc` are dropped
 * entirely ("we will not support factions"). mob's twelve raw attribute
 * columns are also trimmed down to just the six Tobin actually models
 * (str/con/dex/intel/wis/cha) -- bra/agi/foc/per/kar/spe are real
 * Sneezy columns with real seeded values, but nothing in Tobin reads
 * them, so showing them as if they mattered would be misleading.
 *
 * Second follow-up (user 2026-07-12: "in stat room ... dir, cond, and door
 * should be words not numbers"): the room's exit listing decodes
 * `direction` via DIR_NAMES, `type` (door type) via door_type_name(), and
 * `condition_flag` via exit_cond_names() -- the same helpers redit's
 * "goto exit" submenu already uses (descriptor.c), reused here rather
 * than re-deriving the tables.
 *
 * Third follow-up (user 2026-07-12: "stat obj, get names for type,
 * action_flag" / "stat room get names for room_flags, sector"): obj's
 * `type` (the original's itemTypeT) and `action_flag` (the original's
 * extraFlags bitmask) decode via new obj_type_name()/
 * obj_action_flag_names() (obj.c); room's `sector` and `room_flag`
 * decode via the already-existing sector_name()/room_flag_names()
 * (room.c) -- those two already existed for `look`/`redit` and just
 * weren't wired into `stat` yet.
 *
 * Fourth follow-up (user 2026-07-12: "stat player <name> to stat a
 * player"): players aren't a single vnum-keyed table like obj/mob/room --
 * they're a name-keyed row in `player`, plus one-to-one rows in
 * `player_progress` and `player_attrs` -- so stat_player() below looks the
 * name up via the existing player_id_for_name() (player_repo.c) and dumps
 * all three tables as separate sections, same generic dump_row() as
 * everywhere else. Reads straight from the DB, not the live in-memory
 * being_t, matching how `stat mob`/`stat room` already show the DB
 * prototype rather than any particular spawned instance's live state --
 * an online player's most current data is on disk anyway via player_save()
 * at their last quit/death, or default values pre-first-save. */

/* Column names dump_row() should skip entirely -- either because this
 * file prints its own decoded line for them instead, or (faction/
 * fact_perc, and the six unused mob attributes) because Tobin doesn't
 * support them at all. */
static bool is_skipped_column(const char *table, const char *col) {
    if (strcmp(table, "obj") == 0) {
        return strcasecmp(col, "wear_flag") == 0 || strcasecmp(col, "type") == 0
            || strcasecmp(col, "action_flag") == 0;
    }
    if (strcmp(table, "mob") == 0) {
        static const char *const skip[] = {
            "actions", "class", "race", "faction", "fact_perc",
            "bra", "agi", "foc", "per", "kar", "spe",
        };
        for (size_t i = 0; i < sizeof(skip) / sizeof(skip[0]); i++)
            if (strcasecmp(col, skip[i]) == 0)
                return true;
        return false;
    }
    if (strcmp(table, "room") == 0) {
        return strcasecmp(col, "room_flag") == 0 || strcasecmp(col, "sector") == 0;
    }
    if (strcmp(table, "player") == 0) {
        return strcasecmp(col, "class") == 0 || strcasecmp(col, "race") == 0
            || strcasecmp(col, "gender") == 0;
    }
    return false;
}

static void dump_row(char *out, size_t out_sz, size_t *n, db_conn_t *db, const char *table) {
    unsigned int cols = db_col_count(db);
    for (unsigned int i = 0; i < cols && *n < out_sz; i++) {
        const char *col = db_col_name(db, i);
        if (is_skipped_column(table, col))
            continue;
        *n += (size_t)snprintf(out + *n, out_sz - *n, "  %-16s %s\r\n",
                               col, db_get_idx(db, i));
    }
}

/* `stat player <name>` (user 2026-07-12: "stat player <name> to stat a
 * player"): name-keyed instead of vnum-keyed, and spread across three
 * tables instead of one -- see the header comment's fourth follow-up. */
static bool stat_player(descriptor_t *d, const char *name) {
    long pid = player_id_for_name(name);
    if (pid < 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "No such player '%s'.\r\n", name);
        descriptor_send(d, msg);
        return true;
    }

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db) {
        descriptor_send(d, "The database is unavailable.\r\n");
        return true;
    }

    char out[8192];
    size_t n = 0;
    n += (size_t)snprintf(out + n, sizeof(out) - n, "\r\n<c>-- Player %s --<z>\r\n", name);

    if (db_query(db, "select * from player where id=%i", (int)pid) && db_fetch_row(db)) {
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-16s %s\r\n", "class",
                              class_name((player_class_t)atoi(db_get(db, "class"))));
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-16s %s\r\n", "race",
                              race_name((player_race_t)atoi(db_get(db, "race"))));
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-16s %s\r\n", "gender",
                              gender_name((gender_t)atoi(db_get(db, "gender"))));
        dump_row(out, sizeof(out), &n, db, "player");
    }

    n += (size_t)snprintf(out + n, sizeof(out) - n, "<c>-- Progress --<z>\r\n");
    if (db_query(db, "select * from player_progress where player_id=%i", (int)pid) && db_fetch_row(db)) {
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-16s %s\r\n", "alignment_tier",
                              alignment_word(atoi(db_get(db, "alignment"))));
        dump_row(out, sizeof(out), &n, db, "player_progress");
    } else {
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  (never saved)\r\n");
    }

    n += (size_t)snprintf(out + n, sizeof(out) - n, "<c>-- Attributes --<z>\r\n");
    if (db_query(db, "select * from player_attrs where player_id=%i", (int)pid) && db_fetch_row(db)) {
        dump_row(out, sizeof(out), &n, db, "player_attrs");
    } else {
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  (never saved)\r\n");
    }

    db_close(db);
    descriptor_send(d, out);
    return true;
}

bool cmd_stat(descriptor_t *d, const char *args) {
    char cat[16] = "";
    char arg2[PLAYER_NAME_LEN] = "";
    if (sscanf(args, "%15s %63s", cat, arg2) != 2) {
        descriptor_send(d, "Usage: stat <obj|mob|room> <vnum> | stat player <name>\r\n");
        return true;
    }

    size_t clen = strlen(cat);
    if (strncasecmp(cat, "player", clen) == 0)
        return stat_player(d, arg2);

    int vnum = atoi(arg2);
    const char *table, *label;
    if (strncasecmp(cat, "object", clen) == 0) {
        table = "obj"; label = "Object";
    } else if (strncasecmp(cat, "mobile", clen) == 0) {
        table = "mob"; label = "Mobile";
    } else if (strncasecmp(cat, "room", clen) == 0) {
        table = "room"; label = "Room";
    } else {
        descriptor_send(d, "Usage: stat <obj|mob|room> <vnum> | stat player <name>\r\n");
        return true;
    }

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db) {
        descriptor_send(d, "The database is unavailable.\r\n");
        return true;
    }

    char out[8192];
    size_t n = 0;
    if (!db_query(db, "select * from %s where vnum=%i", table, vnum) || !db_fetch_row(db)) {
        n = (size_t)snprintf(out, sizeof(out), "No such %s vnum %d.\r\n", table, vnum);
        descriptor_send(d, out);
        db_close(db);
        return true;
    }

    n += (size_t)snprintf(out + n, sizeof(out) - n, "\r\n<c>-- %s %d --<z>\r\n", label, vnum);

    if (strcmp(table, "obj") == 0) {
        char flagbuf[700];
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-16s %s\r\n", "type",
                              obj_type_name(atoi(db_get(db, "type"))));
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-16s %s\r\n", "wear_flag",
                              obj_wear_flag_names(atoi(db_get(db, "wear_flag")), flagbuf, sizeof(flagbuf)));
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-16s %s\r\n", "action_flag",
                              obj_action_flag_names(atoi(db_get(db, "action_flag")), flagbuf, sizeof(flagbuf)));
    } else if (strcmp(table, "room") == 0) {
        char flagbuf[256];
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-16s %s\r\n", "sector",
                              sector_name(atoi(db_get(db, "sector"))));
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-16s %s\r\n", "room_flag",
                              room_flag_names(atoi(db_get(db, "room_flag")), flagbuf, sizeof(flagbuf)));
    } else if (strcmp(table, "mob") == 0) {
        char flagbuf[512];
        char labelbuf[64];
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-16s %s\r\n", "actions",
                              mob_action_names(atoi(db_get(db, "actions")), flagbuf, sizeof(flagbuf)));
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-16s %s\r\n", "class",
                              mob_class_label(atoi(db_get(db, "class")), labelbuf, sizeof(labelbuf)));
        n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-16s %s\r\n", "race",
                              mob_race_name(atoi(db_get(db, "race"))));
    }

    dump_row(out, sizeof(out), &n, db, table);

    if (strcmp(table, "obj") == 0) {
        if (db_query(db, "select type,mod1,mod2 from objaffect where vnum=%i", vnum)
            && db_has_results(db)) {
            n += (size_t)snprintf(out + n, sizeof(out) - n, "<c>-- Affects --<z>\r\n");
            while (db_fetch_row(db) && n < sizeof(out)) {
                n += (size_t)snprintf(out + n, sizeof(out) - n, "  type=%s mod1=%s mod2=%s\r\n",
                                      db_get(db, "type"), db_get(db, "mod1"), db_get(db, "mod2"));
            }
        }
    } else if (strcmp(table, "room") == 0) {
        if (db_query(db, "select direction,destination,type,condition_flag,name from roomexit where vnum=%i", vnum)
            && db_has_results(db)) {
            n += (size_t)snprintf(out + n, sizeof(out) - n, "<c>-- Exits --<z>\r\n");
            while (db_fetch_row(db) && n < sizeof(out)) {
                char condbuf[128];
                int dir = atoi(db_get(db, "direction"));
                const char *dirname = (dir >= 0 && dir < ROOM_NUM_EXITS) ? DIR_NAMES[dir] : "unknown";
                const char *doorname = door_type_name(atoi(db_get(db, "type")));
                exit_cond_names(atoi(db_get(db, "condition_flag")), condbuf, sizeof(condbuf));
                n += (size_t)snprintf(out + n, sizeof(out) - n,
                                      "  dir=%s -> %s  door=%s  cond=%s  name=%s\r\n",
                                      dirname, db_get(db, "destination"),
                                      doorname, condbuf, db_get(db, "name"));
            }
        }
    }

    db_close(db);
    descriptor_send(d, out);
    return true;
}
