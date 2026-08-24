/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "repair_repo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db.h"

int repair_ticket_create(long player_id, int shop_nr, int obj_vnum, const char *item_label,
                          int orig_max_struct, int depreciation_before, const char *monogram,
                          int price) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;

    bool ok = db_query(db,
        "insert into repair_ticket (player_id, shop_nr, obj_vnum, item_label, orig_max_struct, "
        "depreciation_before, monogram, price) values (%i, %i, %i, '%s', %i, %i, '%s', %i)",
        (int)player_id, shop_nr, obj_vnum, item_label, orig_max_struct, depreciation_before,
        monogram ? monogram : "", price);

    int id = ok ? (int)db_last_insert_id(db) : -1;
    db_close(db);
    return id;
}

/* Shared row-to-struct mapper for repair_ticket_find() and
 * repair_ticket_list_for_player() -- both select * from repair_ticket and
 * need the same column set copied out, so this avoids duplicating it. */
static void fill_ticket(db_conn_t *db, repair_ticket_t *out) {
    out->id = atoi(db_get(db, "id"));
    out->player_id = atol(db_get(db, "player_id"));
    out->shop_nr = atoi(db_get(db, "shop_nr"));
    out->obj_vnum = atoi(db_get(db, "obj_vnum"));
    snprintf(out->item_label, sizeof(out->item_label), "%s", db_get(db, "item_label"));
    out->orig_max_struct = atoi(db_get(db, "orig_max_struct"));
    out->depreciation_before = atoi(db_get(db, "depreciation_before"));
    snprintf(out->monogram, sizeof(out->monogram), "%s", db_get(db, "monogram"));
    out->price = atoi(db_get(db, "price"));
}

/* Looks up a single repair ticket by id, scoped to the claiming player and
 * shop -- the scoping prevents a player from redeeming/picking up a ticket
 * that isn't theirs or belongs to a different repair shop. */
bool repair_ticket_find(int id, long player_id, int shop_nr, repair_ticket_t *out) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select * from repair_ticket where id=%i and player_id=%i and shop_nr=%i",
                 id, (int)player_id, shop_nr) && db_fetch_row(db)) {
        fill_ticket(db, out);
        found = true;
    }

    db_close(db);
    return found;
}

/* Removes a repair ticket, e.g. once the repaired item has been picked up. */
bool repair_ticket_delete(int id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "delete from repair_ticket where id=%i", id);
    db_close(db);
    return ok;
}

/* Lists a player's outstanding repair tickets at a given shop, newest
 * first, up to max entries -- backs the repair shop's "list" display. */
int repair_ticket_list_for_player(long player_id, int shop_nr, repair_ticket_t *out, int max) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int count = 0;
    if (db_query(db, "select * from repair_ticket where player_id=%i and shop_nr=%i order by id desc",
                 (int)player_id, shop_nr)) {
        while (count < max && db_fetch_row(db)) {
            fill_ticket(db, &out[count]);
            count++;
        }
    }

    db_close(db);
    return count;
}
