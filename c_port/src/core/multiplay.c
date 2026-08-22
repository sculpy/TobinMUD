/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "multiplay.h"

#include <strings.h>

#include "db.h"

static bool g_multiplay_allowed = false;

/* Whether logging in a second character from the same account is
 * currently allowed -- the runtime flag toggled by multiplay_set()
 * below, checked at login time. */
bool multiplay_allowed(void) {
    return g_multiplay_allowed;
}

/* Restores the multiplay flag from the game_config DB row at boot --
 * defaults to false (whatever g_multiplay_allowed was initialized to)
 * if no row exists yet or the value isn't literally "on". */
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

/* Immortal-facing toggle: updates the in-memory flag immediately and
 * persists it to game_config so the setting survives a restart --
 * the write side of multiplay_load(). */
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
