/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "tell_history_repo.h"

#include <stdio.h>
#include <stdlib.h>

#include "db.h"

/* Inserts the row, then deletes anything past the cap for that recipient
 * (oldest `sent_at`/`id` first) -- same "insert then trim" shape as
 * player_inventory's own cap enforcement elsewhere in this codebase. */
bool tell_history_add(long from_player_id, long to_player_id, const char *message) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into tell_history (from_player_id, to_player_id, message) values (%i, %i, '%s')",
        (int)from_player_id, (int)to_player_id, message ? message : "");

    if (ok) {
        db_query(db,
            "delete from tell_history where to_player_id=%i and id not in "
            "(select id from (select id from tell_history where to_player_id=%i "
            "order by sent_at desc, id desc limit %i) as keep)",
            (int)to_player_id, (int)to_player_id, TELL_HISTORY_CAP_PER_RECIPIENT);
    }

    db_close(db);
    return ok;
}
