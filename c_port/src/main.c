#include <stdlib.h>
#include <time.h>

#include "combat.h"
#include "config.h"
#include "db.h"
#include "game_loop.h"
#include "log.h"
#include "pulse.h"
#include "regen.h"
#include "wait_tick.h"

int main(void) {
    const config_t *cfg = config_get();

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

    pulse_register(1, wait_tick_run);
    pulse_register(COMBAT_ROUND_PULSES, combat_process_run);
    pulse_register(REGEN_PULSES, regen_tick_run);

    int rc = game_loop_run(cfg->telnet_port);

    db_shutdown();
    return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
