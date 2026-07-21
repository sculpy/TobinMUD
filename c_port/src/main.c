/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "affect.h"
#include "balance.h"
#include "combat.h"
#include "config.h"
#include "crash_handler.h"
#include "db.h"
#include "descriptor.h"
#include "game_loop.h"
#include "bank.h"
#include "gametime.h"
#include "heartbeat.h"
#include "mob_ai.h"
#include "multiplay.h"
#include "log.h"
#include "obj.h"
#include "practice.h"
#include "pulse.h"
#include "regen.h"
#include "shutdown.h"
#include "socials.h"
#include "tips_repo.h"
#include "trigger.h"
#include "vitals.h"
#include "wait_tick.h"
#include "weather.h"
#include "world.h"
#include "zone.h"

static char g_binary_path[PATH_MAX];

/* Reports the full path to the running server binary, so `copyover`
 * (a reboot that keeps everyone connected) knows exactly which file to
 * re-launch. */
const char *tobin_binary_path(void) {
    return g_binary_path[0] ? g_binary_path : "/proc/self/exe";
}

/* The program's starting point: sets up logging and the database
 * connection, restores any saved game state, then hands off to the
 * main game loop that keeps the server running. */
int main(int argc, char **argv) {
    /* Resolve our own path NOW (cwd never changes) so copyover can exec
     * the file at this path -- picking up a rebuilt binary -- rather than
     * /proc/self/exe, which pins the possibly-stale inode we booted from.
     * Copyover successors are exec'd with the full path as argv[0], so the
     * chain keeps resolving across generations. */
    if (argc >= 1 && !realpath(argv[0], g_binary_path))
        g_binary_path[0] = '\0';

    /* --copyover <file>: we are the exec()'d successor of a `copyover`
     * command (cmd_copyover.c) -- adopt the recovery file's sockets
     * instead of opening fresh ones. */
    const char *copyover_file = NULL;
    if (argc >= 3 && strcmp(argv[1], "--copyover") == 0)
        copyover_file = argv[2];

    const config_t *cfg = config_get();

    /* Open the timestamped game log (logs/<datetime>.game.log) before
     * anything logs. A copyover successor lands here too, so every server
     * generation naturally starts its own file. */
    log_open();
    crash_handler_install();

    srand((unsigned int)time(NULL));

    /* Fail fast: confirm the DB is reachable before opening the listening
     * socket, so connectivity problems are obvious at boot rather than
     * discovered mid-session. */
    db_conn_t *probe = db_open(DB_TOBIN);
    if (!probe || !db_query(probe, "select 1")) {
        log_error("Could not reach the '%s' database at %s. Set TOBIN_DB_HOST/"
                   "TOBIN_DB_USER/TOBIN_DB_PASS/TOBIN_DB_NAME and try again.",
                   cfg->db_name_tobin, cfg->db_host);
        db_close(probe);
        return EXIT_FAILURE;
    }
    db_close(probe);
    log_info("Database connection OK.");

    multiplay_load(); /* restore the persisted multiplay game flag */
    gametime_load();  /* restore the persisted game clock */
    weather_load();   /* restore the persisted world weather state */
    balance_cache_load(); /* class/race balance modifiers (cmd_balance.c) */
    wisdom_practice_load(); /* wisdom->practice-points scalar (practice.c) */
    social_cache_load(); /* socials (emotes) -- checked on nearly every unmatched
                             player command, see socials.h for why this is cached */

    /* Zones Part 2 (Session 43): populate rooms from the zone_reset data
     * migrated in Part 1. Runs unconditionally here -- for both a cold
     * boot and a copyover-resumed process alike, since neither preserves
     * room/mob/object state (see zone.h). */
    zone_boot_all();

    pulse_register(1, wait_tick_run);
    pulse_register(10, shutdown_pulse_tick);     /* ~1s: pending `shutdown <seconds>` countdown */
    pulse_register(COMBAT_ROUND_PULSES, combat_process_run);
    pulse_register(COMBAT_ROUND_PULSES, affect_tick_run); /* counts down active buffs/debuffs every round */
    pulse_register(REGEN_PULSES, regen_tick_run);
    pulse_register(100, descriptor_held_expire); /* ~10s: expire held msgs past TTL */
    pulse_register(120, descriptor_keepalive);   /* ~12s: telnet NOP anti-idle (aggressive, survives tight NAT windows) */
    pulse_register(600, descriptor_idle_timeout);/* ~60s: idle-out mortals (immortals immune) */
    pulse_register(600, zone_process_run);       /* ~60s: age zones by 1 minute, top up any that hit their lifespan */
    pulse_register(600, gametime_tick);          /* ~60s: advance the game clock 15 mud-minutes */
    pulse_register(600, bank_interest_tick);     /* ~60s: apply bank interest once per in-game day */
    pulse_register(600, heartbeat_tick);         /* ~60s: real-time half-hour blank-line tick */
    pulse_register(600, mob_ai_tick);            /* ~60s: mob wander/scavenge (mob.actions bits) */
    pulse_register(600, obj_pool_decay_tick);    /* ~60s: ground puddles shrink, then vanish */
    pulse_register(600, obj_light_burn_tick);    /* ~60s: lit lights burn down, then go out */
    pulse_register(600, obj_decay_tick);         /* ~60s: room-floor decay timers (corpses, ...) */
    pulse_register(VITALS_PULSES, vitals_tick_run); /* ~60s: hunger/thirst drain + starvation */
    pulse_register(WEATHER_PULSES, weather_tick_run); /* ~60s: world weather transitions */
    pulse_register(600, trigger_random_tick);    /* ~60s: mob/room "random" scripted triggers */
    pulse_register(10, trigger_pending_tick);    /* ~1s: resume `wait`-paused trigger scripts */
    pulse_register(6000, tips_pulse_tick);       /* ~10min: echo a random tip to newbie-flagged players */
    pulse_register(600, linkdead_purge_tick);    /* ~60s: force-save + destroy any PC linkdead 5+ minutes */

    int rc = game_loop_run(cfg->telnet_port, copyover_file);

    db_shutdown();
    return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
