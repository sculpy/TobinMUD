/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_CONFIG_H
#define TOBIN_CONFIG_H

typedef struct {
    const char *db_host;
    const char *db_user;
    const char *db_pass;
    const char *db_name_tobin;
    const char *db_name_immortal;
    int telnet_port;
    const char *wipe_password;
    int linkdead_purge_seconds;
} config_t;

/* Loaded once from environment variables on first call:
 *   TOBIN_DB_HOST  (default "localhost")
 *   TOBIN_DB_USER  (default NULL -- let the mysql client library decide)
 *   TOBIN_DB_PASS  (default NULL)
 *   TOBIN_DB_NAME           (default "tobin" -- renamed from "sneezy", see
 *                             STATUS.md)
 *   TOBIN_DB_NAME_IMMORTAL  (default "immortal")
 *   TOBIN_PORT     (default 4000)
 *   TOBIN_WIPE_PASSWORD  (default NULL -- `wipe` (cmd_wipe.c) refuses
 *                          outright if unset, rather than falling back to
 *                          any password baked into the source. The
 *                          original hardcoded a literal string
 *                          ("ole'chicken", misc/immortal.cc) -- user,
 *                          2026-07-17: "a real (non-hardcoded) master
 *                          password". Runtime-configurable like the DB
 *                          creds above, not stored anywhere in git.)
 *   TOBIN_LINKDEAD_PURGE_SECONDS  (default 300 -- the flat 5-minute
 *                          threshold world.c's linkdead_purge_tick() uses
 *                          before force-saving and destroying a linkdead
 *                          PC. Runtime-configurable so a smoke test can
 *                          run it against a short threshold instead of
 *                          waiting out the real 5 minutes.)
 */
const config_t *config_get(void);

#endif
