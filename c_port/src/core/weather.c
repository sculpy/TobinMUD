/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "weather.h"

#include <stdlib.h>

#include "db.h"
#include "descriptor.h"
#include "room.h"

static weather_t g_weather = WEATHER_CLEAR;

/* Lowercase display name for a weather_t value (e.g. "stormy"). */
const char *weather_name(weather_t w) {
    switch (w) {
        case WEATHER_CLEAR:  return "clear";
        case WEATHER_CLOUDY: return "cloudy";
        case WEATHER_RAINY:  return "rainy";
        case WEATHER_STORMY: return "stormy";
        default:             return "unknown";
    }
}

/* The current gamewide weather state. */
weather_t weather_current(void) {
    return g_weather;
}

/* Flavor-text hint at what the current weather might do next, e.g. for a
 * `weather` command or a ranger/druid forecasting skill. */
const char *weather_forecast_hint(void) {
    switch (g_weather) {
        case WEATHER_CLEAR:  return "It looks like it should stay clear for a while.";
        case WEATHER_CLOUDY: return "It looks like it could rain before long.";
        case WEATHER_RAINY:  return "The rain could ease up, or it could worsen into a real storm.";
        case WEATHER_STORMY: return "This storm can't last forever -- it should ease up soon.";
        default:              return "";
    }
}

/* Loads the persisted weather state from game_config at boot, leaving the
 * WEATHER_CLEAR default in place if the row is missing, the DB is
 * unreachable, or the stored value is out of range. */
void weather_load(void) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return;
    if (db_query(db, "select value from game_config where name='weather_state'")
        && db_fetch_row(db)) {
        int v = atoi(db_get(db, "value"));
        if (v >= WEATHER_CLEAR && v <= WEATHER_STORMY)
            g_weather = (weather_t)v;
    }
    db_close(db);
}

/* Persists the current weather state to game_config (upsert) so it
 * survives a reboot instead of always resetting to clear. */
static void weather_save(void) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return;
    db_query(db, "insert into game_config (name, value) values ('weather_state', '%i') "
                 "on duplicate key update value=values(value)",
             (int)g_weather);
    db_close(db);
}

/* Same "broadcast to everyone connected, held for anyone mid-edit"
 * convention as gametime.c's own gametime_announce() -- duplicated
 * locally rather than shared, matching this codebase's established
 * precedent for small single-purpose helpers (e.g. the several local
 * copies of keyword_matches() across cmd_*.c).
 *
 * User bug report (2026-07-19): "weather should not affect rooms that
 * are flagged indoors" -- this used to notify EVERY connected
 * character regardless of room, so someone standing inside a building
 * would still see "Clouds begin to gather overhead"/"It begins to
 * rain" despite being unable to see the sky at all. Same
 * ROOM_FLAG_INDOORS check `room_is_dark_for()` (being.c) already uses
 * for the darkness half of this same audit item -- a weather-change
 * announcement is exactly the kind of sky-visibility-dependent content
 * that check exists for. No ALWAYS_LIT exemption here (unlike
 * darkness) -- a torchlit indoor room is still indoors, it just isn't
 * DARK; the two flags answer different questions. */
static void weather_announce(const char *msg) {
    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (!it->character)
            continue;
        room_t *r = it->character->base.roomp;
        if (r && (r->room_flag & ROOM_FLAG_INDOORS))
            continue;
        descriptor_notify(it, msg);
    }
}

/* Weighted transition table (percent chance per tick of moving to each
 * NEXT state, remainder stays put) -- a simple Markov chain standing in
 * for the original's real barometric-pressure simulation. Biased to favor
 * clear/mild weather (matches most real climates spending more time calm
 * than stormy) while still allowing a full clear-to-storm escalation over
 * several ticks. */
static weather_t weather_roll_next(weather_t current) {
    int roll = rand() % 100;
    switch (current) {
        case WEATHER_CLEAR:
            return roll < 15 ? WEATHER_CLOUDY : WEATHER_CLEAR;
        case WEATHER_CLOUDY:
            if (roll < 30) return WEATHER_CLEAR;
            if (roll < 50) return WEATHER_RAINY;
            return WEATHER_CLOUDY;
        case WEATHER_RAINY:
            if (roll < 25) return WEATHER_CLOUDY;
            if (roll < 40) return WEATHER_STORMY;
            return WEATHER_RAINY;
        case WEATHER_STORMY:
            return roll < 50 ? WEATHER_RAINY : WEATHER_STORMY;
        default:
            return WEATHER_CLEAR;
    }
}

/* The room-broadcast line for a weather transition from `from` to `to`
 * (e.g. "It begins to rain."), or NULL for a transition with no message
 * (shouldn't normally happen since weather_tick_run() only calls this when
 * the state actually changed, but every from/to pair isn't necessarily
 * covered explicitly). */
static const char *weather_change_message(weather_t from, weather_t to) {
    if (to == WEATHER_CLOUDY && from == WEATHER_CLEAR)
        return "\r\n<c>Clouds begin to gather overhead.<z>\r\n";
    if (to == WEATHER_CLEAR)
        return "\r\n<C>The clouds part and the sky clears up.<z>\r\n";
    if (to == WEATHER_RAINY && from == WEATHER_CLOUDY)
        return "\r\n<b>It begins to rain.<z>\r\n";
    if (to == WEATHER_CLOUDY && from == WEATHER_RAINY)
        return "\r\n<c>The rain tapers off, leaving the sky overcast.<z>\r\n";
    if (to == WEATHER_STORMY)
        return "\r\n<B>The rain intensifies into a full storm!<z>\r\n";
    if (to == WEATHER_RAINY && from == WEATHER_STORMY)
        return "\r\n<b>The storm eases back into steady rain.<z>\r\n";
    return NULL;
}

/* Periodic hook (registered with the pulse scheduler) that rolls the next
 * weather state via weather_roll_next(), and if it actually changed,
 * persists it and announces the transition to everyone outdoors. */
void weather_tick_run(long pulse_num) {
    (void)pulse_num;
    weather_t next = weather_roll_next(g_weather);
    if (next == g_weather)
        return;

    const char *msg = weather_change_message(g_weather, next);
    g_weather = next;
    weather_save();
    if (msg)
        weather_announce(msg);
}
