/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

#include "db.h"
#include "log.h"

/* `mailinglist`: implementor-only (level 60, same top tier as `shutdown`/
 * `copyover`). Dumps every account's opted-in email address (account.email
 * non-empty -- see cmd_email.c/descriptor.c's CONN_GET_EMAIL, user
 * 2026-08-08) to a plain text file under logs/, one address per line, for
 * pasting into a real email client's BCC field or importing as a mailing
 * list (user, 2026-08-08: "generate a mailing list that writes to a file
 * on the server that can be used in an email client to send mass email").
 * Never emails anyone itself -- this is an export only, no SMTP in Tobin. */
bool cmd_mailinglist(descriptor_t *d, const char *args) {
    (void)args;

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db) {
        descriptor_send(d, "Could not reach the database.\r\n");
        return true;
    }

    if (!db_query(db, "select email from account where email != '' order by email")) {
        db_close(db);
        descriptor_send(d, "Query failed.\r\n");
        return true;
    }

    mkdir(LOG_DIR, 0755); /* EEXIST is fine, same pattern log.c uses */

    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);
    char stamp[32];
    strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tm_buf);

    char path[LOG_PATH_MAX];
    snprintf(path, sizeof(path), "%s/mailinglist_%s.txt", LOG_DIR, stamp);

    FILE *f = fopen(path, "w");
    if (!f) {
        db_close(db);
        descriptor_send(d, "Could not open the output file for writing.\r\n");
        return true;
    }

    int count = 0;
    while (db_fetch_row(db)) {
        const char *email = db_get(db, "email");
        if (email && email[0]) {
            fprintf(f, "%s\n", email);
            count++;
        }
    }

    fclose(f);
    db_close(db);

    char msg[256];
    snprintf(msg, sizeof(msg),
             "Wrote %d opted-in email address%s to %s\r\n",
             count, count == 1 ? "" : "es", path);
    descriptor_send(d, msg);
    return true;
}
