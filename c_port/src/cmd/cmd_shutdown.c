/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "being.h"
#include "shutdown.h"

/* Bare `shutdown` counts down from here, same 5-second warning window
 * copyover already gives everyone (user, 2026-07-17: "shutdown should
 * display a countdown from 5 seconds until shutdown to everyone" --
 * "shutdown in 5, 4, 3, 2, 1, shutdown"). An explicit `shutdown <seconds>`
 * overrides it. */
#define DEFAULT_SHUTDOWN_SECONDS 5

/* `shutdown [seconds|cancel]` -- the Implementor-only "kindly" kill switch
 * (user, 2026-07-17: "write a shutdown command to kill the mud kindly
 * along with a time function that will shutdown in <X> seconds"). Bare
 * `shutdown` counts down from DEFAULT_SHUTDOWN_SECONDS; `shutdown
 * <seconds>` counts down from a given number instead -- either way,
 * without blocking anyone's play in the meantime (see shutdown.c, whose
 * per-second broadcast covers the final "5, 4, 3, 2, 1" stretch of any
 * countdown, long or short). `shutdown cancel` aborts a countdown already
 * in progress. All the actual work lives in shutdown.c so it can be
 * driven by the pulse scheduler. */
bool cmd_shutdown(descriptor_t *d, const char *args) {
    char tok[32] = "";
    sscanf(args, "%31s", tok);

    const char *initiator = (d->character && d->character->base.name[0])
                                 ? d->character->base.name
                                 : "An immortal";

    if (tok[0] && (strcasecmp(tok, "cancel") == 0 || strcasecmp(tok, "abort") == 0)) {
        if (shutdown_cancel(initiator))
            descriptor_send(d, "Pending shutdown cancelled.\r\n");
        else
            descriptor_send(d, "No shutdown is pending.\r\n");
        return true;
    }

    int seconds = DEFAULT_SHUTDOWN_SECONDS;
    /* `-now` (user, 2026-08-08, alongside copyover's own -now): an
     * explicit synonym for "shutdown 0" -- same immediate path, just a
     * clearer word for it at the keyboard. */
    if (tok[0] && strcasecmp(tok, "-now") == 0) {
        seconds = 0;
        tok[0] = '\0';
    }
    if (tok[0]) {
        bool all_digits = true;
        for (const char *p = tok; *p; p++) {
            if (!isdigit((unsigned char)*p)) {
                all_digits = false;
                break;
            }
        }
        if (!all_digits) {
            descriptor_send(d, "Usage: shutdown [seconds|-now|cancel|abort]\r\n");
            return true;
        }
        seconds = atoi(tok);
    }

    /* seconds == 0 (only reachable via an explicit "shutdown 0"):
     * shutdown_schedule() has already saved everyone and broadcast the
     * farewell line (which reaches `d` too) by the time it returns --
     * nothing more to send here. */
    shutdown_schedule(seconds, initiator);
    if (seconds > 0)
        descriptor_send(d, "Shutdown scheduled.\r\n");
    return true;
}
