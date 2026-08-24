/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "commodity.h"

#include <stdlib.h>

#include "db.h"

/* Headroom above the current seeded count (182 as of 2026-08-24: 27
 * RAW_MATERIAL + 9 GEMSTONE + 146 RAW_ORGANIC) -- a future builder
 * seeding more commodity prototypes just fills more of this array,
 * nothing else needs to change. */
#define COMMODITY_CACHE_MAX 512

typedef struct {
    int vnum;
    int price;
    int material;
} commodity_entry_t;

static commodity_entry_t g_commodities[COMMODITY_CACHE_MAX];
static int g_commodity_count = 0;

void commodity_cache_load(void) {
    g_commodity_count = 0;

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return;

    /* type 42/43/50 = RAW_MATERIAL/GEMSTONE/RAW_ORGANIC (upstream's
     * itemTypeT, kept verbatim in obj.type -- see obj.c's
     * category_for_item_type()). Sorted price-descending so
     * commodity_pick_for_wealth() can just take the first affordable
     * hit. price=0 rows excluded (unseeded/placeholder prototypes,
     * not real tradeable commodities). */
    if (db_query(db,
            "select vnum, price, material from obj where type in (42,43,50) "
            "and price > 0 order by price desc")) {
        while (db_fetch_row(db) && g_commodity_count < COMMODITY_CACHE_MAX) {
            g_commodities[g_commodity_count].vnum = atoi(db_get(db, "vnum"));
            g_commodities[g_commodity_count].price = atoi(db_get(db, "price"));
            g_commodities[g_commodity_count].material = atoi(db_get(db, "material"));
            g_commodity_count++;
        }
    }

    db_close(db);
}

bool commodity_pick_for_wealth(int wealth_budget, int *out_vnum, int *out_price) {
    if (wealth_budget <= 0)
        return false;

    for (int i = 0; i < g_commodity_count; i++) {
        if (g_commodities[i].price <= wealth_budget) {
            *out_vnum = g_commodities[i].vnum;
            *out_price = g_commodities[i].price;
            return true;
        }
    }

    return false;
}
