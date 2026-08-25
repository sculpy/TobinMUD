/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "dragon_route_repo.h"

#include <stdio.h>
#include <stdlib.h>

#include "db.h"

void dragon_route_repo_list(int from_room, dragon_route_t *out, int max, int *count) {
    *count = 0;

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return;

    if (db_query(db,
                "select to_room, dest_name, fee from dragon_route "
                "where from_room=%i order by dest_name limit %i",
                from_room, max)) {
        while (*count < max && db_fetch_row(db)) {
            out[*count].to_room = atoi(db_get(db, "to_room"));
            snprintf(out[*count].dest_name, DRAGON_ROUTE_NAME_LEN, "%s", db_get(db, "dest_name"));
            out[*count].fee = atoi(db_get(db, "fee"));
            (*count)++;
        }
    }

    db_close(db);
}
