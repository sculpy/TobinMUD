/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "shop_repo.h"

#include <stdio.h>
#include <stdlib.h>

#include "db.h"
#include "mob_repo.h"
#include "obj.h"

bool shop_repo_find_by_room(int room_vnum, shop_t *out) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db,
                "select shop_nr, profit_buy, profit_sell, keeper, in_room, "
                "no_such_item1, no_such_item2, do_not_buy, missing_cash1, "
                "message_buy, message_sell from shop where in_room=%i limit 1",
                room_vnum)
        && db_fetch_row(db)) {
        out->shop_nr = atoi(db_get(db, "shop_nr"));
        out->profit_buy = atof(db_get(db, "profit_buy"));
        out->profit_sell = atof(db_get(db, "profit_sell"));
        out->keeper = atoi(db_get(db, "keeper"));
        out->in_room = atoi(db_get(db, "in_room"));
        snprintf(out->no_such_item1, SHOP_MSG_LEN, "%s", db_get(db, "no_such_item1"));
        snprintf(out->no_such_item2, SHOP_MSG_LEN, "%s", db_get(db, "no_such_item2"));
        snprintf(out->do_not_buy, SHOP_MSG_LEN, "%s", db_get(db, "do_not_buy"));
        snprintf(out->missing_cash1, SHOP_MSG_LEN, "%s", db_get(db, "missing_cash1"));
        snprintf(out->message_buy, SHOP_MSG_LEN, "%s", db_get(db, "message_buy"));
        snprintf(out->message_sell, SHOP_MSG_LEN, "%s", db_get(db, "message_sell"));
        found = true;
    }

    db_close(db);
    return found;
}

bool shop_repo_buys_category(int shop_nr, int category) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool buys = false;
    if (db_query(db, "select type from shoptype where shop_nr=%i", shop_nr)) {
        while (db_fetch_row(db)) {
            int raw_type = atoi(db_get(db, "type"));
            if ((int)category_for_item_type(raw_type) == category) {
                buys = true;
                break;
            }
        }
    }

    db_close(db);
    return buys;
}

bool shop_repo_is_hospital(int shop_nr) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    int keeper = -1;
    if (db_query(db, "select keeper from shop where shop_nr=%i", shop_nr) && db_fetch_row(db))
        keeper = atoi(db_get(db, "keeper"));

    db_close(db);
    return keeper >= 0 && mob_repo_get_spec_proc(keeper) == SPEC_PROC_DOCTOR;
}

void shop_repo_producing(int shop_nr, int *out, int max, int *count) {
    *count = 0;

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return;

    if (db_query(db, "select producing from shopproducing where shop_nr=%i order by producing limit %i",
                shop_nr, max)) {
        while (*count < max && db_fetch_row(db)) {
            out[*count] = atoi(db_get(db, "producing"));
            (*count)++;
        }
    }

    db_close(db);
}
