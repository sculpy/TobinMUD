/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

#include "log.h"

/* `test` (58+): shows the name of whatever smoke test is currently running,
 * per the loopback-only `@test <name>` / `@test done <name>` hook
 * (descriptor.c's handle_line()) -- user: "add a test command that will
 * list whatever smoke test is currently running". */
bool cmd_test(descriptor_t *d, const char *args) {
    (void)args;

    const char *name = log_test_current_name();
    char msg[160];
    if (name[0])
        snprintf(msg, sizeof(msg), "Currently running: %s\r\n", name);
    else
        snprintf(msg, sizeof(msg), "No smoke test is currently running.\r\n");
    descriptor_send(d, msg);
    return true;
}
