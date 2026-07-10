/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_GAMETIME_H
#define TOBIN_GAMETIME_H

#include <stdbool.h>
#include <stddef.h>

/* Time/day/date system (Session 43, ported from Sneezy's GameTime class,
 * sys/gametime.{h,cc}): a 28-day month, 12-month year, tracked in 15-
 * mud-minute increments -- same calendar shape, same weekday formula
 * ((28*month + day + 1) % 7), same noon/midnight/month/year rollover
 * announcements. Session-only (in-memory, like other ephemeral state in
 * this port) -- starts fresh at boot rather than resuming a persisted
 * clock; Tobin has no save-game-time table and this doesn't need one for
 * a first cut.
 *
 * Deliberately dropped from the original: the weather-driven sunrise/
 * sunset/moon-phase tracking (Tobin has no weather system yet -- same
 * reason `$$g`'s weather-prefix was dropped, see room.h) and the
 * account-level personal real-time-zone-offset sub-feature of Sneezy's
 * `time` command (a real feature, just not part of "the day/date
 * system" itself -- can be added later if wanted).
 *
 * Ticks on a pulse (gametime_tick(), main.c) rather than the original's
 * real-clock-seconds-per-mud-hour formula -- simpler, and reuses the same
 * ~60s cadence zone_process_run() already established, advancing 15 mud-
 * minutes per tick (1 mud-hour per ~4 real minutes, 1 mud-day per ~96
 * real minutes). */

/* Pulse callback (main.c): advances the clock by 15 mud-minutes, handling
 * hour/day/month/year rollover and the associated world announcements
 * (noon, midnight, new month, new year) via descriptor_notify() (so
 * nobody mid-edit is interrupted -- see the Session 43 editors-quiet
 * audit). */
void gametime_tick(long pulse_num);

int gametime_hour(void);    /* 0-23 */
int gametime_minute(void);  /* 0,15,30,45 */
int gametime_day(void);     /* 0-27 (0-indexed internally; display as +1) */
int gametime_month(void);   /* 0-11 */
int gametime_year(void);    /* starts at 1 */

/* 0 (Sunday) - 6 (Saturday), Sneezy's exact formula. */
int gametime_weekday(void);

/* "March", "Sunday", etc. NULL-safe bounds (out of range -> "?"). */
const char *gametime_month_name(int month);
const char *gametime_weekday_name(int weekday);

/* "3:45 PM" -- hour/minute formatted 12-hour with AM/PM, matching
 * GameTime::hmtAsString(). Writes into buf, returns buf. */
const char *gametime_clock_string(int hour, int minute, char *buf, size_t bufsz);

/* Simple fixed day/night split (6 AM - 8 PM is "day") -- a placeholder for
 * the original's sunrise/sunset-driven version, which needs the weather
 * system this port doesn't have yet. */
bool gametime_is_daytime(void);

#endif
