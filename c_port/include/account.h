#ifndef TOBIN_ACCOUNT_H
#define TOBIN_ACCOUNT_H

#include <stdbool.h>

/* C replacement for the login-relevant slice of misc/account.{h,cc}'s
 * TAccount, backed by the `account` table (db/sneezy/account.sql). */

typedef struct {
    long account_id;
    char name[80];
    char passwd[256]; /* crypt() hash, e.g. "$6$..." */
} account_t;

/* Case-insensitive by name, mirrors the original's `where name=lower(...)`.
 * Returns true and fills *out on success, false if no such account. */
bool account_load(const char *name, account_t *out);

/* Creates a new account row with a freshly salted crypt() hash of
 * plain_password. Fails if the name is already taken. */
bool account_create(const char *name, const char *plain_password, account_t *out);

/* Verifies a plaintext password against the account's stored crypt() hash. */
bool account_verify_password(const account_t *acct, const char *plain_password);

#endif
