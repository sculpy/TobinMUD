/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_DRAGON_ROUTE_REPO_H
#define TOBIN_DRAGON_ROUTE_REPO_H

/* DB access for the `dragon_route` table (db/tobin/dragon_ride.sql) --
 * backs `fly` (cmd_fly.c). Each row is one directed route: standing in
 * room `from_room` (a dragon roost), `fly <dest_name>` flies the player
 * to `to_room` for `fee` gold. New roosts/routes are pure data -- no
 * code change needed to add one. */

#define DRAGON_ROUTE_MAX 16
#define DRAGON_ROUTE_NAME_LEN 64

typedef struct {
    int to_room;
    char dest_name[DRAGON_ROUTE_NAME_LEN];
    int fee;
} dragon_route_t;

/* Fills `out` (capacity `max`) with every route departing `from_room`,
 * ordered by dest_name. Sets *count to how many were found (0 if
 * `from_room` isn't a roost at all). */
void dragon_route_repo_list(int from_room, dragon_route_t *out, int max, int *count);

#endif
