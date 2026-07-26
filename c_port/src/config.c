/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "config.h"

#include <stdlib.h>

static config_t g_config;
static int g_loaded = 0;

static const char *env_or(const char *name, const char *dflt) {
    const char *v = getenv(name);
    return (v && *v) ? v : dflt;
}

const config_t *config_get(void) {
    if (!g_loaded) {
        g_config.db_host = env_or("TOBIN_DB_HOST", "localhost");
        g_config.db_user = getenv("TOBIN_DB_USER");
        g_config.db_pass = getenv("TOBIN_DB_PASS");
        g_config.db_name_tobin = env_or("TOBIN_DB_NAME", "tobin");
        g_config.db_name_immortal = env_or("TOBIN_DB_NAME_IMMORTAL", "immortal");

        const char *port_str = getenv("TOBIN_PORT");
        g_config.telnet_port = port_str ? atoi(port_str) : 4000;
        if (g_config.telnet_port <= 0 || g_config.telnet_port > 65535)
            g_config.telnet_port = 4000;

        g_config.wipe_password = getenv("TOBIN_WIPE_PASSWORD");

        const char *linkdead_str = getenv("TOBIN_LINKDEAD_PURGE_SECONDS");
        g_config.linkdead_purge_seconds = linkdead_str ? atoi(linkdead_str) : 300;
        if (g_config.linkdead_purge_seconds <= 0)
            g_config.linkdead_purge_seconds = 300;

        g_loaded = 1;
    }
    return &g_config;
}
