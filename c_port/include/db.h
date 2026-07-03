#ifndef TOBIN_DB_H
#define TOBIN_DB_H

#include <stdbool.h>

/* Plain-C replacement for sys/database.{h,cc}'s TDatabase.  TDatabase was
 * already just RAII + a vtable wrapped around the plain mariadb/mysql.h C
 * API (see TDatabasePimpl in database.cc) -- this is a direct 1:1 port of
 * that behavior, not a redesign.
 *
 * Usage mirrors the original:
 *
 *   db_conn_t *db = db_open(DB_TOBIN);
 *   db_query(db, "select vnum, short_desc from obj where weight<%f and "
 *                "name like '%%%s%%' and vnum>%i", weight, name, vnum);
 *   while (db_fetch_row(db)) {
 *       printf("%s\n", db_get(db, "short_desc"));
 *   }
 *   db_close(db);
 *
 * query() supports the same printf-ish mini-language as the original:
 *   %s - escaped string   %r - raw/unescaped string   %i - int
 *   %f - double            %% - literal percent
 */

typedef enum {
    DB_TOBIN,   /* the underlying MariaDB database is still named "sneezy" on
                 * disk (db/ wasn't renamed, see STATUS.md) -- this enum tag
                 * is just this port's internal name for it */
    DB_IMMORTAL,
    DB_MAX
} db_type_t;

#define DB_MAX_COLS 64
#define DB_MAX_COL_NAME 64

typedef struct db_conn db_conn_t;

db_conn_t *db_open(db_type_t type);
void db_close(db_conn_t *conn);

bool db_query(db_conn_t *conn, const char *fmt, ...);
bool db_fetch_row(db_conn_t *conn);

const char *db_get(db_conn_t *conn, const char *col_name);
const char *db_get_idx(db_conn_t *conn, unsigned int idx);

bool db_has_results(db_conn_t *conn);
long db_row_count(db_conn_t *conn);
long db_last_insert_id(db_conn_t *conn);
unsigned long db_escape_string(db_conn_t *conn, char *to, const char *from, unsigned long length);

bool db_begin(db_conn_t *conn);
bool db_commit(db_conn_t *conn);
bool db_rollback(db_conn_t *conn);

/* Call once at process exit to close pooled connections cleanly. */
void db_shutdown(void);

#endif
