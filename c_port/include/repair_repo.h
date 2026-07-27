/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_REPAIR_REPO_H
#define TOBIN_REPAIR_REPO_H

#include <stdbool.h>

/* DB access for the `repair_ticket` table (Object maintenance tasks 3-4,
 * Sneezy → Tobin feature audit, "full system" scope) -- the repair-shop
 * economy's own claim-check, `submit`/`retrieve` (cmd_repair.c). Unlike
 * the real upstream (misc/repair.cc), which serializes the actual damaged
 * object into a per-repairman file on disk, a ticket here is just a DB
 * row: the item is destroyed on submit and reconstructed fresh from its
 * prototype vnum on retrieval (obj_create_from_proto(), same "vnum +
 * saved state" shape player_inventory already uses for every carried
 * item, not a serialized object blob). */

#define REPAIR_TICKET_LABEL_LEN 255
#define REPAIR_TICKET_MONOGRAM_LEN 64

typedef struct {
    int id;
    long player_id;
    int shop_nr;
    int obj_vnum;
    char item_label[REPAIR_TICKET_LABEL_LEN + 1];
    int orig_max_struct;
    int depreciation_before;
    char monogram[REPAIR_TICKET_MONOGRAM_LEN + 1];
    int price;
} repair_ticket_t;

/* Creates a new ticket, returns its id (>0), or -1 on failure. */
int repair_ticket_create(long player_id, int shop_nr, int obj_vnum, const char *item_label,
                          int orig_max_struct, int depreciation_before, const char *monogram,
                          int price);

/* Fills `out` from ticket `id`, but ONLY if it belongs to `player_id` and
 * was submitted to shop `shop_nr` -- `retrieve` can't be used to pull a
 * ticket that isn't yours or that was left at a different shop. Returns
 * false (out untouched) if no such row matches all three. */
bool repair_ticket_find(int id, long player_id, int shop_nr, repair_ticket_t *out);

/* Removes ticket `id` -- called once `retrieve` has successfully paid out
 * and handed the reconstructed item back. */
bool repair_ticket_delete(int id);

/* Fills `out` (capacity `max`) with every ticket `player_id` has waiting
 * at shop `shop_nr`, newest first. Used by `tickets` to list them without
 * needing to guess a ticket number. Returns the count written. */
int repair_ticket_list_for_player(long player_id, int shop_nr, repair_ticket_t *out, int max);

#endif
