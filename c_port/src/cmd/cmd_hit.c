/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

/* `hit`: always the normal multi-round combat process (cmd_attack.c),
 * regardless of the caller's level -- an immortal typing `hit` actually
 * fights something instead of `kill`/`attack`'s instant slay. `kill` and
 * `attack` are unchanged (still instakill for immortals); this is purely
 * an additional way in, for when an immortal wants a real fight. cmd_attack
 * itself never special-cased immortals to begin with, so this is a thin
 * passthrough, not new combat logic. */
bool cmd_hit(descriptor_t *d, const char *args) {
    return cmd_attack(d, args);
}
