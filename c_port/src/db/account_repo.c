/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "account.h"

#include <crypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "db.h"
#include "log.h"

/* NOTE (deviation from the original, recorded in STATUS.md): the original
 * TAccount::write() salts new-account passwords with the account NAME
 * (crypt(arg, account->name.c_str())), which is a weak/guessable DES-crypt
 * salt. This port generates a random SHA-512 crypt salt instead -- a
 * self-contained hardening, not an architectural redesign. */
static void make_salt(char *out, size_t outsz) {
    static const char charset[] =
        "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    snprintf(out, outsz, "$6$");
    size_t pos = strlen(out);
    while (pos + 1 < outsz && pos < 3 + 16) {
        out[pos++] = charset[rand() % (int)(sizeof(charset) - 1)];
    }
    out[pos] = '\0';
}

bool account_load(const char *name, account_t *out) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = false;
    if (db_query(db, "select account_id, name, passwd, color_pref, time_adjust from account where name=lower('%s')", name)
        && db_fetch_row(db)) {
        out->account_id = atol(db_get(db, "account_id"));
        snprintf(out->name, sizeof(out->name), "%s", db_get(db, "name"));
        snprintf(out->passwd, sizeof(out->passwd), "%s", db_get(db, "passwd"));
        out->color_pref = atoi(db_get(db, "color_pref")) != 0;
        out->time_adjust = atoi(db_get(db, "time_adjust"));
        ok = true;
    }

    db_close(db);
    return ok;
}

bool account_create(const char *name, const char *plain_password, account_t *out) {
    account_t existing;
    if (account_load(name, &existing)) {
        log_error("account_create: name '%s' already taken", name);
        return false;
    }

    char salt[32];
    make_salt(salt, sizeof(salt));
    char *hash = crypt(plain_password, salt);
    if (!hash) {
        log_error("account_create: crypt() failed");
        return false;
    }

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into account (multiplay_limit, email, passwd, name, birth, term, "
        "time_adjust, flags, last_logon, color_pref) "
        "values (2, '', '%s', lower('%s'), %i, 0, 0, 0, %i, 1)",
        hash, name, (int)time(NULL), (int)time(NULL));

    if (ok) {
        out->account_id = db_last_insert_id(db);
        snprintf(out->name, sizeof(out->name), "%s", name);
        snprintf(out->passwd, sizeof(out->passwd), "%s", hash);
        out->color_pref = true; /* color on by default; the creation prompt may flip it */
        out->time_adjust = 0; /* server time by default; the creation prompt may set it */
    }

    db_close(db);
    return ok;
}

bool account_verify_password(const account_t *acct, const char *plain_password) {
    if (!acct->passwd[0])
        return false;
    char *hash = crypt(plain_password, acct->passwd);
    return hash && strcmp(hash, acct->passwd) == 0;
}

bool account_set_color(long account_id, bool color_on) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "update account set color_pref=%i where account_id=%i",
                       color_on ? 1 : 0, (int)account_id);

    db_close(db);
    return ok;
}

bool account_set_timezone(long account_id, int hours) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "update account set time_adjust=%i where account_id=%i",
                       hours, (int)account_id);

    db_close(db);
    return ok;
}

bool account_delete(long account_id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "delete from account where account_id=%i", (int)account_id);

    db_close(db);
    return ok;
}
