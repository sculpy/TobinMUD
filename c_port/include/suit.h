#ifndef TOBIN_SUIT_H
#define TOBIN_SUIT_H

struct being;

/* Newbie equipment suits (user 2026-07-26) -- see suit_repo.h for the DB
 * lookup layer this sits on top of. suit_grant() is the one shared piece
 * of logic behind all three real features: automatic issue at character
 * creation (player_repo.c), the `loadsuit` immortal command
 * (cmd_loadsuit.c), and the Grimhaven Welfare Department social worker's
 * gear reissue (cmd_say.c). */

/* Creates every item in `suit_id` and gives them to `ch`, loose in
 * inventory -- deliberately not auto-equipped/auto-wielded (user
 * 2026-07-26: "they can hold the items themselves, just load into
 * inventory"). Silently does nothing for an invalid suit_id (-1) or a
 * NULL `ch`. Returns how many items were actually created and given (0
 * if the suit has no items, or doesn't exist). Saves inventory once at
 * the end if anything was granted, not once per item. */
int suit_grant(struct being *ch, int suit_id);

#endif
