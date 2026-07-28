/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
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
    else if (strcmp(which, "account") == 0)
        ok = db_query(db, "select count(*) as c from account");
    else if (strcmp(which, "player") == 0)
        ok = db_query(db, "select count(*) as c from player");
    else
        ok = db_query(db, "select count(*) as c from obj");
    long n = (ok && db_fetch_row(db)) ? atol(db_get(db, "c")) : -1;
    db_close(db);
    return n;
}

/* Shells out to count non-blank lines across every .c/.h file under src/
 * and include/ (relative to the game process's own working directory, same
 * assumption cmd_exec.c's popen() usage already relies on). Not cached --
 * cheap enough to run fresh on every `mudstats` call, and the source tree
 * only changes between deploys anyway. Returns -1 if the shell-out itself
 * fails to start; a malformed `wc` line is reported as 0 rather than
 * silently hidden. */
static long mud_count_loc(void) {
    FILE *fp = popen("find src include -name '*.c' -o -name '*.h' 2>/dev/null "
                      "| xargs wc -l 2>/dev/null | tail -1", "r");
    if (!fp)
        return -1;
    long total = 0;
    if (fscanf(fp, "%ld", &total) != 1)
        total = 0;
    pclose(fp);
    return total;
}

/* `mudstats`: a quick overview of the world's content -- rooms, mobs,
 * objects, accounts, and characters defined in the database, plus the
 * codebase's own current size. Expandable as more systems land. */
bool cmd_mudstats(descriptor_t *d, const char *args) {
    (void)args;

    char out[768];
    snprintf(out, sizeof(out),
             "\r\n<c>=== TobinMUD Statistics ===<z>\r\n"
             "There are %ld rooms in the game.\r\n"
             "There are %ld mobs (NPCs) in the game.\r\n"
             "There are %ld objects in the game.\r\n"
             "There are %ld accounts registered.\r\n"
             "There are %ld player characters.\r\n"
             "The codebase is %ld lines of C code.\r\n",
             mud_count("room"), mud_count("mob"), mud_count("obj"),
             mud_count("account"), mud_count("player"), mud_count_loc());
    descriptor_send(d, out);
    return true;
}
