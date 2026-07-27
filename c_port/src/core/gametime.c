/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "gametime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"
#include "descriptor.h"

#define DAYS_PER_MONTH 28
#define MONTHS_PER_YEAR 12

static const char *const MONTH_NAMES[MONTHS_PER_YEAR] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December",
};

static const char *const WEEKDAY_NAMES[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday",
};

typedef struct {
    int minute;
    int hour;
    int day;
    int month;
    int year;
} game_time_t;

static game_time_t g_time = { 0, 8, 0, 0, 1 }; /* starts 8:00 AM, day 1, year 1 */

int gametime_hour(void)   { return g_time.hour; }
int gametime_minute(void) { return g_time.minute; }
int gametime_day(void)    { return g_time.day; }
int gametime_month(void)  { return g_time.month; }
int gametime_year(void)   { return g_time.year; }

/* Same `game_config` key/value pattern as multiplay.c -- one row per
 * field, upserted every tick. */
static void gametime_save(void) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return;
    db_query(db, "insert into game_config (name, value) values "
                 "('gametime_hour', '%i'), ('gametime_minute', '%i'), "
                 "('gametime_day', '%i'), ('gametime_month', '%i'), "
                 "('gametime_year', '%i') "
                 "on duplicate key update value=values(value)",
             g_time.hour, g_time.minute, g_time.day, g_time.month, g_time.year);
    db_close(db);
}

/* Restores g_time from the game_config rows gametime_save() writes --
 * called once at boot so the in-game clock resumes where it left off
 * instead of always restarting at day 1, 8:00 AM. Any field with no
 * saved row simply keeps its g_time initializer default. */
void gametime_load(void) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return;
    if (db_query(db, "select name, value from game_config where name like 'gametime_%%'")) {
        while (db_fetch_row(db)) {
            const char *name = db_get(db, "name");
            int value = atoi(db_get(db, "value"));
            if (strcmp(name, "gametime_hour") == 0)        g_time.hour = value;
            else if (strcmp(name, "gametime_minute") == 0) g_time.minute = value;
            else if (strcmp(name, "gametime_day") == 0)    g_time.day = value;
            else if (strcmp(name, "gametime_month") == 0)  g_time.month = value;
            else if (strcmp(name, "gametime_year") == 0)   g_time.year = value;
        }
    }
    db_close(db);
}

/* Current in-game weekday index (0=Sunday..6=Saturday), derived
 * arithmetically from month/day rather than tracked as its own field --
 * feeds gametime_weekday_name() below for display. */
int gametime_weekday(void) {
    return ((DAYS_PER_MONTH * g_time.month) + g_time.day + 1) % 7;
}

/* Display name for a 0-based month index (MONTH_NAMES[] above) --
 * returns "?" for an out-of-range value. */
const char *gametime_month_name(int month) {
    if (month < 0 || month >= MONTHS_PER_YEAR)
        return "?";
    return MONTH_NAMES[month];
}

/* Display name for a 0-based weekday index (WEEKDAY_NAMES[] above,
 * matching gametime_weekday()'s numbering) -- returns "?" for an
 * out-of-range value. */
const char *gametime_weekday_name(int weekday) {
    if (weekday < 0 || weekday >= 7)
        return "?";
    return WEEKDAY_NAMES[weekday];
}

/* Formats `hour`:`minute` (24h) into a 12-hour "H:MM AM/PM" clock
 * string in `buf` -- used wherever the in-game time needs a
 * human-readable display (e.g. the `time` command). */
const char *gametime_clock_string(int hour, int minute, char *buf, size_t bufsz) {
    int h12 = hour % 12;
    if (h12 == 0)
        h12 = 12;
    snprintf(buf, bufsz, "%d:%02d %s", h12, minute, hour >= 12 ? "PM" : "AM");
    return buf;
}

/* Whether the in-game clock currently reads as daytime (6:00 AM through
 * 7:59 PM) -- feeds room_is_dark_for() (being.c) so darkness/light
 * mechanics track the in-game clock, not just room flags. */
bool gametime_is_daytime(void) {
    return g_time.hour >= 6 && g_time.hour < 20;
}

/* Sends `msg` to every playing connection, held (not lost) for anyone
 * mid-edit -- same descriptor_notify() convention as the death taunt and
 * every other world-wide broadcast (combat.c). */
static void gametime_announce(const char *msg) {
    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (!it->character)
            continue;
        descriptor_notify(it, msg);
    }
}

/* Runs on a timer (see main.c): advances the in-game clock by 15
 * minutes per call, cascading minute -> hour -> day -> month -> year as
 * each unit rolls over, announcing noon/midnight/New Year to every
 * connected player along the way (gametime_announce() above), and
 * persisting the new time (gametime_save()) after every call so a
 * server restart doesn't lose progress. */
void gametime_tick(long pulse_num) {
    (void)pulse_num;

    g_time.minute += 15;
    if (g_time.minute < 60) {
        gametime_save();
        return;
    }
    g_time.minute = 0;
    g_time.hour++;

    if (g_time.hour == 12)
        gametime_announce("\r\n<Y>It is noon.<z>\r\n");

    if (g_time.hour < 24) {
        gametime_save();
        return;
    }
    gametime_announce("\r\n<k>It is midnight.<z>\r\n");
    g_time.hour = 0;
    g_time.day++;

    if (g_time.day < DAYS_PER_MONTH) {
        gametime_save();
        return;
    }
    g_time.day = 0;
    g_time.month++;

    if (g_time.month < MONTHS_PER_YEAR) {
        gametime_save();
        return;
    }
    g_time.month = 0;
    g_time.year++;
    char msg[128];
    snprintf(msg, sizeof(msg), "\r\n<Y>Happy New Year! It is now the Year %d.<z>\r\n", g_time.year);
    gametime_announce(msg);
    gametime_save();
}
