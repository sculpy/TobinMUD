/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "practice.h"

#include <stdio.h>
#include <stdlib.h>

#include "db.h"

/* Gamewide wisdom->practice-points scalar, cached like multiplay.c's flag
 * so the level-up hot path never hits the DB. Default 1.0. */
static double g_wisdom_practice_modifier = 1.0;

/* Current gamewide wisdom->practice-points scalar (cached value, no DB hit). */
double wisdom_practice_modifier(void) {
    return g_wisdom_practice_modifier;
}

/* Loads g_wisdom_practice_modifier from the game_config table at boot,
 * leaving the 1.0 default in place if the row is missing or the DB is
 * unreachable. */
void wisdom_practice_load(void) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return;
    if (db_query(db, "select value from game_config where name='wisdom_practice_modifier'")
        && db_fetch_row(db)) {
        const char *v = db_get(db, "value");
        if (v && *v)
            g_wisdom_practice_modifier = atof(v);
    }
    db_close(db);
}

/* Updates the cached wisdom-practice modifier and persists it to
 * game_config (upsert) so an immortal's `set` change survives a reboot. */
void wisdom_practice_modifier_set(double value) {
    g_wisdom_practice_modifier = value;
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return;
    /* db_query has no %f that round-trips cleanly for arbitrary doubles, so
     * format the value ourselves and pass it as %s. */
    char buf[32];
    snprintf(buf, sizeof(buf), "%g", value);
    db_query(db, "insert into game_config (name, value) values ('wisdom_practice_modifier', '%s') "
                 "on duplicate key update value='%s'",
             buf, buf);
    db_close(db);
}

/* random(6,8) + round(wisdom_bonus * modifier), floored at 0. Uses rand()
 * once, so it must be called once per level gained. No libm dependency
 * (this codebase links no math library) -- floor division and round-half-
 * away-from-zero are both done with plain integer/double arithmetic. */
int practice_points_for_level(const being_t *ch) {
    int base = 6 + (rand() % 3); /* 6, 7, or 8 */

    int diff = ch->attrs.wisdom - ATTR_BASE;
    int wisdom_bonus = diff / 10;
    if (diff % 10 != 0 && diff < 0)
        wisdom_bonus--; /* floor division: C truncates toward zero, we want floor */

    double scaled = wisdom_bonus * g_wisdom_practice_modifier;
    int rounded = (int)(scaled >= 0.0 ? scaled + 0.5 : scaled - 0.5);

    int award = base + rounded;
    return award < 0 ? 0 : award;
}
