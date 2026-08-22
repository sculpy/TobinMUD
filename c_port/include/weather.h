/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_WEATHER_H
#define TOBIN_WEATHER_H

/* C replacement for the shape of Sneezy's Weather class (misc/weather.h/
 * .cc) -- Sneezy → Tobin feature audit, "Weather & light levels". Trimmed
 * hard: the original runs a real barometric-pressure random walk driving
 * sky-state transitions, a 0-31 moon phase cycle, per-room "wetness"
 * tracking, and season-aware sunrise/sunset calculation. Tobin has none
 * of the climate-zone/moon-cycle content that would make most of that
 * meaningful, so this keeps just the visible, atmospheric core: a single
 * WORLD-WIDE sky state (not per-room -- Tobin has no distinct weather
 * regions), advanced by a simple weighted transition table instead of a
 * pressure simulation, with a world-wide announcement on every actual
 * state change (same broadcast-to-everyone-regardless-of-location
 * precedent gametime.c's own noon/midnight announcements already use,
 * rather than building per-room outdoor-only delivery). The "light
 * levels" half of this audit item lives in being.c
 * (being_has_active_light()) and cmd_look.c/cmd_exits.c's darkness gate,
 * not here -- day/night itself was already tracked by gametime.c's
 * existing gametime_is_daytime() before this session touched anything. */

typedef enum {
    WEATHER_CLEAR,
    WEATHER_CLOUDY,
    WEATHER_RAINY,
    WEATHER_STORMY,
} weather_t;

/* Display name ("clear", "cloudy", "rainy", "stormy"). */
const char *weather_name(weather_t w);

/* The current world-wide sky state. */
weather_t weather_current(void);

/* A short flavor hint about where the weather might be headed -- NOT a
 * real forecast (the pressure-trend math behind Sneezy's own "theoretical
 * prediction" is exactly the simulation depth this port trims), just a
 * fixed phrase per current state so `weather` (cmd_weather.c) has
 * something to say beyond the bare current condition, matching the
 * original's help text describing both halves. */
const char *weather_forecast_hint(void);

/* Temperature shift (on sector_heat()'s scale) the current sky applies to
 * OUTDOOR ambient heat for the heat subsystem (room.h): clear +5 (sun),
 * cloudy 0, rainy -10, stormy -20. A labelled Tobin-original coupling --
 * Sneezy's own weather never fed its (also-unused) TerrainInfo heat. */
int weather_heat_delta(weather_t w);

/* Restores the persisted sky state from game_config (same key/value
 * table + convention gametime.c already uses) at boot. A missing row
 * (fresh DB) defaults to WEATHER_CLEAR. */
void weather_load(void);

/* Rolls a chance to transition the sky state (weighted by the current
 * state, see weather.c's own transition table) and, on an actual change,
 * persists it and announces a flavor message to everyone connected.
 * Register with pulse_register(WEATHER_PULSES, weather_tick_run) from
 * main.c. */
void weather_tick_run(long pulse_num);

/* WEATHER_PULSES = 600 (~60s at 100ms/pulse) -- same "once a minute"
 * cadence as zone aging/gametime/mob AI/vitals (main.c). */
#define WEATHER_PULSES 600

#endif
