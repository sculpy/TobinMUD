/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/

#ifndef TOBIN_ACCOUNT_H
#define TOBIN_ACCOUNT_H

#include <stdbool.h>

/* C replacement for the login-relevant slice of misc/account.{h,cc}'s
 * TAccount, backed by the `account` table (db/sneezy/account.sql). */

typedef struct {
    long account_id;
    char name[80];
    char passwd[256]; /* crypt() hash, e.g. "$6$..." */
    bool color_pref;  /* ANSI color on/off, chosen at account creation */
    int time_adjust;  /* hours offset from server time (Eastern), chosen at
                        * account creation; see cmd_time.c */
} account_t;

/* Case-insensitive by name, mirrors the original's `where name=lower(...)`.
 * Returns true and fills *out on success, false if no such account. */
bool account_load(const char *name, account_t *out);

/* Same as account_load(), but by account_id -- needed once a caller only
 * has the id, e.g. after account_set_name() changes the name a prior
 * lookup was keyed on (edaccount, cmd_edaccount.c/descriptor.c). */
bool account_load_by_id(long account_id, account_t *out);

/* Creates a new account row with a freshly salted crypt() hash of
 * plain_password. Fails if the name is already taken. */
bool account_create(const char *name, const char *plain_password, account_t *out);

/* Verifies a plaintext password against the account's stored crypt() hash. */
bool account_verify_password(const account_t *acct, const char *plain_password);

/* Persists the ANSI color preference (account.color_pref). Backs the color
 * prompt at account creation and the `color on|off` command. */
bool account_set_color(long account_id, bool color_on);

/* Persists the real-time-zone offset (account.time_adjust, hours relative to
 * the server's own Eastern clock). Backs the creation prompt and the
 * `time <difference>` command. */
bool account_set_timezone(long account_id, int hours);

/* Renames the account (edaccount, admin-only -- a player never renames
 * their own account, no self-service equivalent exists). Fails if
 * new_name is already taken by a DIFFERENT account (case-insensitive,
 * same collation the login lookup uses) or if account_id doesn't exist. */
bool account_set_name(long account_id, const char *new_name);

/* Resets the account's password to a freshly salted hash of
 * plain_password (edaccount, admin-only -- same crypt() scheme
 * account_create() uses for a brand new account). */
bool account_set_password(long account_id, const char *plain_password);

/* Deletes the account row outright. `player.account_id` carries an ON
 * DELETE CASCADE FK to `account`, so every character on the account (and,
 * transitively, their player_attrs/player_progress/player_inventory rows)
 * is removed along with it -- no separate per-character cleanup needed. */
bool account_delete(long account_id);

#endif
