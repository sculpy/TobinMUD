/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "bank.h"

#include "db.h"
#include "gametime.h"
#include "log.h"

/* -1 sentinel forces the very first tick after boot to compute (not
 * apply) a baseline rather than crediting interest for whatever partial
 * day was already in progress when the server came up. */
static long last_seen_day = -1;

/* Runs on a timer (see main.c): once per in-game day (detected by
 * comparing a packed year/month/day value against last_seen_day, not a
 * fixed pulse count -- gametime can run at a different rate than real
 * time), credits BANK_INTEREST_RATE interest to every player's bank_gold
 * directly in the DB. The -1 sentinel above means boot always sets a
 * baseline on its first call instead of crediting for a partial day. */
void bank_interest_tick(long pulse_num) {
    (void)pulse_num;

    long today = (long)gametime_year() * 12 * 28 + (long)gametime_month() * 28 + (long)gametime_day();
    if (last_seen_day < 0) {
        last_seen_day = today;
        return;
    }
    if (today == last_seen_day)
        return;
    last_seen_day = today;

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return;

    /* FLOOR() so a balance under 200 gold (0.5% < 1) simply doesn't grow
     * that day rather than rounding up to a free gold coin. */
    if (!db_query(db,
                  "update player_progress set bank_gold = bank_gold + "
                  "FLOOR(bank_gold * %f) where bank_gold > 0",
                  BANK_INTEREST_RATE))
        log_error("bank_interest_tick: daily interest UPDATE failed");

    db_close(db);
}
