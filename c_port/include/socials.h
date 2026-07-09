/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_SOCIALS_H
#define TOBIN_SOCIALS_H

#include <stdbool.h>

struct descriptor;

/* Socials (emotes): smile, nod, wave, ... A trimmed port of the original's
 * lib/actions -- each is a verb with a fixed set of messages. Checked by
 * cmd_dispatch() AFTER the command table (classic DikuMUD ordering), so a
 * social never shadows a real command. Returns true if `verb` was a social
 * (and it was handled), false to let the caller print "Huh?!". */
bool social_try(struct descriptor *d, const char *verb, const char *args);

/* Comma-separated list of the available social verbs, for the `socials`
 * command. */
#include <stddef.h>
void social_names(char *out, size_t size);

#endif
