/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
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
} config_t;

/* Loaded once from environment variables on first call:
 *   TOBIN_DB_HOST  (default "localhost")
 *   TOBIN_DB_USER  (default NULL -- let the mysql client library decide)
 *   TOBIN_DB_PASS  (default NULL)
 *   TOBIN_DB_NAME           (default "sneezy" -- the underlying MariaDB
 *                             database is still named "sneezy" on disk;
 *                             db/ wasn't renamed, see STATUS.md)
 *   TOBIN_DB_NAME_IMMORTAL  (default "immortal")
 *   TOBIN_PORT     (default 4000)
 */
const config_t *config_get(void);

#endif
