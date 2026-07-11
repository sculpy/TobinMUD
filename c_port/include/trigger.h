/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_TRIGGER_H
#define TOBIN_TRIGGER_H

#include "trigger_repo.h"

struct being;
struct room;

/* Runs every action line in `trig->script` (one action per line, newline-
 * separated -- same convention the shared line editor already writes,
 * descriptor.c's editor_feed()) against the given context. `actor` is the
 * being who caused the event (may be NULL for a pure-ambient "random"
 * trigger with no specific instigator); `room` is where it's happening
 * (may be NULL only if `actor` is also NULL, i.e. never in practice --
 * every real firing has a room). `self_name` is the mob/obj's own
 * capitalized display name, used by the `emote` action (NULL for room
 * triggers, which have no single "self" to speak as -- `emote` falls back
 * to "Something" in that case).
 *
 * Fixed action vocabulary (one per script line, verb then rest-of-line
 * argument) -- deliberately small, not a general-purpose scripting
 * language (see trigger.sql's header comment for why):
 *   echo <text>      -- sent to `actor` only
 *   echoroom <text>  -- sent to everyone else in `room` (actor excluded)
 *   emote <text>     -- "<self_name> <text>" sent to everyone in `room`
 *                        (actor included -- it's the mob/room "speaking")
 *   teleport <vnum>  -- moves `actor` to room <vnum> (no-op if no actor)
 *   give <vnum>      -- spawns object <vnum> into `actor`'s inventory
 *   damage <n>       -- deals <n> damage to `actor`, clamped so it can
 *                        never drop them below 1 HP (no death-outside-
 *                        combat handling exists yet, same limitation
 *                        `drink`'s poison already accepted)
 *   log <text>       -- LOG_SILENT game log entry (audit/debug, never
 *                        broadcast live)
 * Unrecognized verbs are silently skipped (typo-tolerant, matching the
 * spirit of a builder-facing tool over a strict compiler). */
void trigger_run(const trigger_t *trig, struct being *actor, struct room *room,
                 const char *self_name);

/* Ages the world's `random` triggers by one tick: rolls `chance_pct` for
 * every room's "random" room trigger and every mob's "random" mob trigger,
 * running any that hit. Pulse-registered in main.c at the same ~60s
 * cadence as mob_ai_tick()/obj_pool_decay_tick(); also forced by `aitick`
 * for deterministic testing (same precedent as those two). */
void trigger_random_tick(long pulse_num);

#endif
