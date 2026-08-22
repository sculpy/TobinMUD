/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
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

/* Looks up an account by login name (case-insensitive) and fills out.
 * Returns false if no such account exists. */
bool account_load(const char *name, account_t *out) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = false;
    if (db_query(db, "select account_id, name, passwd, color_pref, time_adjust, email from account where name=lower('%s')", name)
        && db_fetch_row(db)) {
        out->account_id = atol(db_get(db, "account_id"));
        snprintf(out->name, sizeof(out->name), "%s", db_get(db, "name"));
        snprintf(out->passwd, sizeof(out->passwd), "%s", db_get(db, "passwd"));
        out->color_pref = atoi(db_get(db, "color_pref")) != 0;
        out->time_adjust = atoi(db_get(db, "time_adjust"));
        snprintf(out->email, sizeof(out->email), "%s", db_get(db, "email"));
        ok = true;
    }

    db_close(db);
    return ok;
}

/* Same as account_load() but keyed by account_id instead of name -- used
 * once a caller already has the id (e.g. from a loaded player row) and
 * doesn't want a second name lookup. */
bool account_load_by_id(long account_id, account_t *out) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = false;
    if (db_query(db, "select account_id, name, passwd, color_pref, time_adjust, email from account where account_id=%i", (int)account_id)
        && db_fetch_row(db)) {
        out->account_id = atol(db_get(db, "account_id"));
        snprintf(out->name, sizeof(out->name), "%s", db_get(db, "name"));
        snprintf(out->passwd, sizeof(out->passwd), "%s", db_get(db, "passwd"));
        out->color_pref = atoi(db_get(db, "color_pref")) != 0;
        out->time_adjust = atoi(db_get(db, "time_adjust"));
        snprintf(out->email, sizeof(out->email), "%s", db_get(db, "email"));
        ok = true;
    }

    db_close(db);
    return ok;
}

/* Creates a brand-new account row: rejects a duplicate name, hashes
 * plain_password with a fresh random salt (see the deviation note on
 * make_salt() above), and fills out with the new account's id/name/hash. */
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
        out->email[0] = '\0'; /* empty (opted out) by default; the creation prompt may set it */
    }

    db_close(db);
    return ok;
}

/* Checks plain_password against acct's stored crypt hash by re-crypting
 * with the hash's own salt and comparing. Pure in-memory check, no DB
 * access. */
bool account_verify_password(const account_t *acct, const char *plain_password) {
    if (!acct->passwd[0])
        return false;
    char *hash = crypt(plain_password, acct->passwd);
    return hash && strcmp(hash, acct->passwd) == 0;
}

/* Updates an account's ANSI color preference. */
bool account_set_color(long account_id, bool color_on) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "update account set color_pref=%i where account_id=%i",
                       color_on ? 1 : 0, (int)account_id);

    db_close(db);
    return ok;
}

/* Updates an account's email address. An empty string is a valid,
 * deliberate opt-out, not an error (user, 2026-08-08: "allow someone to
 * opt out of providing email"). */
bool account_set_email(long account_id, const char *email) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "update account set email='%s' where account_id=%i",
                       email, (int)account_id);

    db_close(db);
    return ok;
}

/* Updates an account's timezone offset (hours from server time). */
bool account_set_timezone(long account_id, int hours) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "update account set time_adjust=%i where account_id=%i",
                       hours, (int)account_id);

    db_close(db);
    return ok;
}

/* Renames an account, rejecting the change if new_name is already taken by
 * a different account. */
bool account_set_name(long account_id, const char *new_name) {
    account_t existing;
    if (account_load(new_name, &existing) && existing.account_id != account_id) {
        log_error("account_set_name: name '%s' already taken", new_name);
        return false;
    }

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "update account set name=lower('%s') where account_id=%i",
                        new_name, (int)account_id);

    db_close(db);
    return ok;
}

/* Changes an account's password: hashes plain_password with a fresh random
 * salt and stores it. */
bool account_set_password(long account_id, const char *plain_password) {
    char salt[32];
    make_salt(salt, sizeof(salt));
    char *hash = crypt(plain_password, salt);
    if (!hash) {
        log_error("account_set_password: crypt() failed");
        return false;
    }

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "update account set passwd='%s' where account_id=%i",
                        hash, (int)account_id);

    db_close(db);
    return ok;
}

/* Permanently removes an account row. Does not cascade-delete the
 * account's players -- callers are responsible for that. */
bool account_delete(long account_id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "delete from account where account_id=%i", (int)account_id);

    db_close(db);
    return ok;
}
