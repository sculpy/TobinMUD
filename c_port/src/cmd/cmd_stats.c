/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>

#include "db.h"
#include "descriptor.h"
#include "world.h"

/* `stats` (TODO.md priority item, user 2026-07-30: "persisted game
 * statistics (rooms, mobs, objects, accounts, characters) so values
 * survive reboot/copyover and display correctly"). Checked SneezyMUD's
 * own `info numbers` (misc/immortal.cc) for the closest real precedent --
 * a similar immortal-facing aggregate counts panel, though the real
 * source counts live in-memory linked lists (`character_list`, a
 * per-process `AccountStats::player_num` static), which is exactly the
 * kind of state that does NOT survive a reboot/copyover, the user's
 * whole complaint. Tobin has no boot-time world load at all (rooms/mobs/
 * objects load lazily as visited, see world.h's own doc comment) and no
 * persistent in-memory counters to go stale -- so every count here is a
 * live `SELECT COUNT(*)` straight from the DB tables that actually own
 * the data, which is correct by construction immediately after ANY
 * reboot or copyover with zero extra persistence machinery needed. Only
 * the currently-online figures (players connected, rooms/mobs/objects
 * currently loaded into memory) are live in-process state, clearly
 * labeled apart from the persisted DB totals. */
bool cmd_stats(descriptor_t *d, const char *args) {
    (void)args;

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db) {
        descriptor_send(d, "The database is unavailable.\r\n");
        return true;
    }

    long total_rooms = 0, total_mobs = 0, total_objs = 0;
    long total_accounts = 0, total_players = 0;

    if (db_query(db, "select count(*) as n from room") && db_fetch_row(db))
        total_rooms = atol(db_get(db, "n"));
    if (db_query(db, "select count(*) as n from mob") && db_fetch_row(db))
        total_mobs = atol(db_get(db, "n"));
    if (db_query(db, "select count(*) as n from obj") && db_fetch_row(db))
        total_objs = atol(db_get(db, "n"));
    if (db_query(db, "select count(*) as n from account") && db_fetch_row(db))
        total_accounts = atol(db_get(db, "n"));
    if (db_query(db, "select count(*) as n from player") && db_fetch_row(db))
        total_players = atol(db_get(db, "n"));

    db_close(db);

    int online = 0;
    for (descriptor_t *it = g_descriptors; it; it = it->next)
        if (it->character)
            online++;

    char out[512];
    size_t n = 0;
    n += (size_t)snprintf(out + n, sizeof(out) - n, "\r\n<c>-- World statistics --<z>\r\n");
    n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-20s %ld\r\n", "Rooms (seeded):", total_rooms);
    n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-20s %ld\r\n", "Mobiles (seeded):", total_mobs);
    n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-20s %ld\r\n", "Objects (seeded):", total_objs);
    n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-20s %ld\r\n", "Accounts:", total_accounts);
    n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-20s %ld\r\n", "Characters:", total_players);
    n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-20s %d\r\n", "Currently online:", online);
    n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-20s %d\r\n", "Rooms in memory:", world_count_loaded_rooms());
    n += (size_t)snprintf(out + n, sizeof(out) - n, "  %-20s %d\r\n", "Linkdead bodies:", world_count_linkdead());

    descriptor_send(d, out);
    return true;
}
