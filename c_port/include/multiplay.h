/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_MULTIPLAY_H
#define TOBIN_MULTIPLAY_H

#include <stdbool.h>

/* Global multiplay flag (persisted in game_config). When off (default),
 * a mortal account may have only one character in the game at a time;
 * immortals are always exempt. Toggled by the 59+ `multiplay` command. */
bool multiplay_allowed(void);
void multiplay_set(bool on); /* updates the cached flag + the DB */
void multiplay_load(void);   /* loads the cached flag from the DB at boot */

#endif
