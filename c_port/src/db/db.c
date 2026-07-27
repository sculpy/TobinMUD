/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "db.h"

#include <mysql.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "config.h"
#include "log.h"

/* One pooled connection per db_type_t, lazily connected and ping-refreshed,
 * mirroring TDatabaseConnection in the original database.cc. */
static MYSQL *g_pool[DB_MAX];

struct db_conn {
    MYSQL *db;
    MYSQL_RES *res;
    MYSQL_ROW row;
    long row_count;
    char col_names[DB_MAX_COLS][DB_MAX_COL_NAME];
    unsigned int col_count;
};

/* Maps a db_type_t to the configured schema name for that database
 * (the immortal/admin DB vs. the main tobin DB). */
static const char *db_name_for(db_type_t type) {
    const config_t *cfg = config_get();
    return (type == DB_IMMORTAL) ? cfg->db_name_immortal : cfg->db_name_tobin;
}

/* Returns the pooled MYSQL handle for type, (re)connecting first if it has
 * never been opened or the existing connection has gone stale (mysql_ping
 * fails). Callers never see raw connect/reconnect logic -- db_open() just
 * asks the pool for a handle. */
static MYSQL *pool_get(db_type_t type) {
    if (type < 0 || type >= DB_MAX)
        return NULL;

    if (!g_pool[type] || mysql_ping(g_pool[type])) {
        const config_t *cfg = config_get();
        const char *dbname = db_name_for(type);

        log_info("Connecting to database '%s'.", dbname);
        if (!g_pool[type])
            g_pool[type] = mysql_init(NULL);

        if (!mysql_real_connect(g_pool[type], cfg->db_host, cfg->db_user,
                                 cfg->db_pass, dbname, 0, NULL, 0)) {
            log_error("Could not connect to database '%s': %s", dbname,
                       mysql_error(g_pool[type]));
            return NULL;
        }
    }
    return g_pool[type];
}

/* Allocates a per-caller db_conn_t (result-set cursor state) wrapping the
 * shared pooled connection for type. Must be paired with db_close(). */
db_conn_t *db_open(db_type_t type) {
    db_conn_t *conn = calloc(1, sizeof(*conn));
    if (!conn)
        return NULL;
    conn->db = pool_get(type);
    return conn;
}

/* Frees a db_conn_t and any pending result set. Does NOT close the
 * underlying pooled MYSQL connection -- that lives in g_pool and is reused
 * by the next db_open() for the same db_type_t. */
void db_close(db_conn_t *conn) {
    if (!conn)
        return;
    if (conn->res)
        mysql_free_result(conn->res);
    free(conn);
}

/* Closes every pooled connection and tears down the MySQL client library.
 * Called once at process exit, not per-request. */
void db_shutdown(void) {
    for (int i = 0; i < DB_MAX; i++) {
        if (g_pool[i]) {
            mysql_close(g_pool[i]);
            g_pool[i] = NULL;
        }
    }
    mysql_thread_end();
    mysql_library_end();
}

/* Bounded, truncation-safe append used while building the query buffer. */
static bool buf_append(char *buf, size_t bufsz, size_t *len, const char *s) {
    size_t slen = strlen(s);
    if (*len + slen + 1 > bufsz)
        return false;
    memcpy(buf + *len, s, slen);
    *len += slen;
    buf[*len] = '\0';
    return true;
}

/* Core of the whole db layer: builds and runs a SQL statement from a
 * printf-style template and stashes any result set on conn for the
 * db_fetch_row()/db_get() family to walk afterward. Every *_repo.c function
 * in this codebase goes through this one call. Format specifiers are
 * deliberately narrow to keep query building safe:
 *   %r - raw string, inserted verbatim (table/column names, literal SQL)
 *   %s - string value, escaped via mysql_real_escape_string before insertion
 *   %i - int, formatted decimal
 *   %f - double, formatted decimal
 *   %% - literal percent
 * On success, any prior result on conn is freed and replaced with the new
 * one (if the statement produced one) and conn->row_count is updated. */
bool db_query(db_conn_t *conn, const char *fmt, ...) {
    if (!conn || !conn->db) {
        log_error("query failed: no database connection");
        return false;
    }

    enum { QBUF_SIZE = 65536, ESC_SIZE = 65537 };
    char qbuf[QBUF_SIZE];
    char escbuf[ESC_SIZE];
    size_t len = 0;
    qbuf[0] = '\0';

    va_list ap;
    va_start(ap, fmt);

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            char one[2] = { *p, '\0' };
            if (!buf_append(qbuf, sizeof(qbuf), &len, one)) {
                log_error("query - buffer overrun building: %s", fmt);
                va_end(ap);
                return false;
            }
            continue;
        }

        p++;
        switch (*p) {
            case 'r': {
                const char *raw = va_arg(ap, const char *);
                if (!raw) {
                    log_error("null argument for format specifier 'r' in: %s", fmt);
                    va_end(ap);
                    return false;
                }
                if (!buf_append(qbuf, sizeof(qbuf), &len, raw)) {
                    log_error("query - buffer overrun on: %s", raw);
                    va_end(ap);
                    return false;
                }
                break;
            }
            case 's': {
                const char *raw = va_arg(ap, const char *);
                if (!raw) {
                    log_error("null argument for format specifier 's' in: %s", fmt);
                    va_end(ap);
                    return false;
                }
                size_t rawlen = strlen(raw);
                if (rawlen * 2 + 1 > sizeof(escbuf)) {
                    log_error("query - buffer overrun escaping: %s", raw);
                    va_end(ap);
                    return false;
                }
                mysql_real_escape_string(conn->db, escbuf, raw, (unsigned long)rawlen);
                if (!buf_append(qbuf, sizeof(qbuf), &len, escbuf)) {
                    log_error("query - buffer overrun building: %s", fmt);
                    va_end(ap);
                    return false;
                }
                break;
            }
            case 'i': {
                char num[32];
                snprintf(num, sizeof(num), "%d", va_arg(ap, int));
                if (!buf_append(qbuf, sizeof(qbuf), &len, num)) {
                    va_end(ap);
                    return false;
                }
                break;
            }
            case 'f': {
                char num[64];
                snprintf(num, sizeof(num), "%f", va_arg(ap, double));
                if (!buf_append(qbuf, sizeof(qbuf), &len, num)) {
                    va_end(ap);
                    return false;
                }
                break;
            }
            case '%':
                if (!buf_append(qbuf, sizeof(qbuf), &len, "%")) {
                    va_end(ap);
                    return false;
                }
                break;
            default:
                log_error("query - bad format specifier '%c' in: %s", *p, fmt);
                va_end(ap);
                return false;
        }
    }
    va_end(ap);

    if (mysql_query(conn->db, qbuf)) {
        log_error("query failed: %s -- query was: %s", mysql_error(conn->db), qbuf);
        return false;
    }

    MYSQL_RES *restmp = mysql_store_result(conn->db);
    if (restmp) {
        if (conn->res)
            mysql_free_result(conn->res);
        conn->res = restmp;

        conn->col_count = mysql_num_fields(conn->res);
        if (conn->col_count > DB_MAX_COLS)
            conn->col_count = DB_MAX_COLS;
        MYSQL_FIELD *fields = mysql_fetch_fields(conn->res);
        for (unsigned int i = 0; i < conn->col_count; i++)
            snprintf(conn->col_names[i], DB_MAX_COL_NAME, "%s", fields[i].name);
    }

    conn->row_count = (long)mysql_affected_rows(conn->db);
    return true;
}

/* Advances conn's cursor to the next row of the last query's result set.
 * Returns false once rows are exhausted (or if there is no result set),
 * so callers loop `while (db_fetch_row(conn)) { ... }`. */
bool db_fetch_row(db_conn_t *conn) {
    if (!conn || !conn->res)
        return false;
    conn->row = mysql_fetch_row(conn->res);
    return conn->row != NULL;
}

/* Reads the current row's value for col_name by name (case-insensitive).
 * Returns "" (never NULL) for a SQL NULL, a missing column, or no current
 * row, so callers can pass the result straight to string functions. */
const char *db_get(db_conn_t *conn, const char *col_name) {
    if (!conn || !conn->res || !conn->row)
        return "";
    for (unsigned int i = 0; i < conn->col_count; i++) {
        if (strcasecmp(conn->col_names[i], col_name) == 0)
            return conn->row[i] ? conn->row[i] : "";
    }
    log_error("db_get(%s) - invalid column name", col_name);
    return "";
}

/* Same as db_get() but by positional column index instead of name; used
 * where callers iterate all columns (e.g. dumping a whole row). */
const char *db_get_idx(db_conn_t *conn, unsigned int idx) {
    if (!conn || !conn->res || !conn->row || idx >= conn->col_count) {
        log_error("db_get_idx(%u) - invalid column index", idx);
        return "";
    }
    return conn->row[idx] ? conn->row[idx] : "";
}

/* Number of columns in the last query's result set (0 if none). */
unsigned int db_col_count(db_conn_t *conn) {
    return conn ? conn->col_count : 0;
}

/* Column name at positional index idx, for callers that walk columns
 * generically instead of asking for a name they already know. */
const char *db_col_name(db_conn_t *conn, unsigned int idx) {
    if (!conn || idx >= conn->col_count)
        return "";
    return conn->col_names[idx];
}

/* True if the last query returned at least one row; lets callers do a
 * quick existence check without looping db_fetch_row(). */
bool db_has_results(db_conn_t *conn) {
    return conn && conn->res && mysql_num_rows(conn->res) > 0;
}

/* Rows affected by the last query (INSERT/UPDATE/DELETE row count), or -1
 * if conn is invalid. */
long db_row_count(db_conn_t *conn) {
    return conn ? conn->row_count : -1;
}

/* AUTO_INCREMENT id generated by the last INSERT on this connection; 0 if
 * none or conn is invalid. */
long db_last_insert_id(db_conn_t *conn) {
    return (conn && conn->db) ? (long)mysql_insert_id(conn->db) : 0;
}

/* Exposes mysql_real_escape_string() directly for callers that need to
 * build SQL fragments db_query()'s %s specifier can't express. */
unsigned long db_escape_string(db_conn_t *conn, char *to, const char *from, unsigned long length) {
    if (!conn || !conn->db)
        return 0;
    return mysql_real_escape_string(conn->db, to, from, length);
}

/* Starts a transaction on conn. */
bool db_begin(db_conn_t *conn) {
    return db_query(conn, "begin");
}

/* Commits the current transaction on conn. */
bool db_commit(db_conn_t *conn) {
    return db_query(conn, "commit");
}

/* Rolls back the current transaction on conn. */
bool db_rollback(db_conn_t *conn) {
    return db_query(conn, "rollback");
}
