/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_SOCIALS_H
#define TOBIN_SOCIALS_H

#include <stdbool.h>
#include <stddef.h>

#include "social_repo.h"

struct descriptor;
struct being;

/* Socials (emotes): smile, nod, wave, ... A full port of the original's
 * lib/actions (db/import-socials.py, db/sneezy/social.sql -- 155 verbs),
 * DB-backed but read through an in-memory cache built by
 * social_cache_load() -- social_try() runs on nearly every unmatched
 * player command (checked AFTER the whole command table, classic DikuMUD
 * ordering), so it can't afford a DB round-trip per attempt. Text may
 * contain the upstream $-token grammar ($n/$N/$s/$S/$e/$E/$m/$M),
 * expanded by social_expand() (socials.c) at send time. */

/* (Re)loads the in-memory social cache from the DB. Call once at boot
 * (main.c), and again after any edsocial edit so the change takes effect
 * immediately without a restart. */
void social_cache_load(void);

/* Returns true if `verb` was a social (and it was handled), false to let
 * the caller print "Huh?!". Abbreviation matching: any non-empty prefix
 * resolves to the first cached (alphabetical) social it matches, same
 * rule as the command table. */
bool social_try(struct descriptor *d, const char *verb, const char *args);

/* Performs the exact-or-abbreviated `verb` social AS `actor`, no target,
 * with no self-message (unlike social_try(), `actor` may have no
 * descriptor -- see cmd_say.c's charmed-pet command-obeying, user
 * 2026-07-25: "master says dance pet dances etc"). Only the room-facing
 * `others_no_arg` template is sent, to everyone in actor's room (actor
 * included, since there's no separate "you" line for something with no
 * descriptor to receive it). False if `verb` isn't a known social, or
 * actor's position doesn't meet the social's min_position (same silent-
 * refusal shape as an out-of-position PC would get). */
bool social_perform_for(struct being *actor, const char *verb);

/* Comma-separated list of every cached social's verb, for the `socials`
 * command (paged -- see cmd_socials.c). */
void social_names(char *out, size_t size);

/* The cached list itself, in load order (alphabetical) -- for edsocial's
 * list view, which needs each entry's name individually, not just a
 * flattened comma-joined string. */
int social_cache_count(void);
const social_t *social_cache_at(int index);

#endif
