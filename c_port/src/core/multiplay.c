/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "multiplay.h"

#include <strings.h>

#include "db.h"

static bool g_multiplay_allowed = false;

bool multiplay_allowed(void) {
    return g_multiplay_allowed;
}

void multiplay_load(void) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return;
    if (db_query(db, "select value from game_config where name='multiplay'")
        && db_fetch_row(db)) {
        const char *v = db_get(db, "value");
        g_multiplay_allowed = (v && strcasecmp(v, "on") == 0);
    }
    db_close(db);
}

void multiplay_set(bool on) {
    g_multiplay_allowed = on;
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return;
    db_query(db, "insert into game_config (name, value) values ('multiplay', '%s') "
                 "on duplicate key update value='%s'",
             on ? "on" : "off", on ? "on" : "off");
    db_close(db);
}
