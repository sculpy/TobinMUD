/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"

/* Counts rows in one of the content tables (literal queries -- table names
 * can't be parameterized). Returns -1 on error. */
static long mud_count(const char *which) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;
    bool ok;
    if (strcmp(which, "room") == 0)
        ok = db_query(db, "select count(*) as c from room");
    else if (strcmp(which, "mob") == 0)
        ok = db_query(db, "select count(*) as c from mob");
    else
        ok = db_query(db, "select count(*) as c from obj");
    long n = (ok && db_fetch_row(db)) ? atol(db_get(db, "c")) : -1;
    db_close(db);
    return n;
}

/* `mudstats`: a quick overview of the world's content -- rooms, mobs, and
 * objects defined in the database. Expandable as more systems land. */
bool cmd_mudstats(descriptor_t *d, const char *args) {
    (void)args;

    char out[512];
    snprintf(out, sizeof(out),
             "\r\n<c>=== TobinMUD Statistics ===<z>\r\n"
             "There are %ld rooms in the game.\r\n"
             "There are %ld mobs (NPCs) in the game.\r\n"
             "There are %ld objects in the game.\r\n",
             mud_count("room"), mud_count("mob"), mud_count("obj"));
    descriptor_send(d, out);
    return true;
}
