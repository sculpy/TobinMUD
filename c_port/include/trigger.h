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
 * capitalized display name, used by the `emote`/`say` actions (NULL for
 * room triggers, which have no single "self" to speak as -- they fall
 * back to "Something" in that case).
 *
 * Full DG Scripts-style language as of 2026-07-25 (user: "use the DG_*
 * source files to revamp triggers") -- see trigger_script.h for the
 * interpreter core (%var% substitution, if/elseif/else/end, while/done,
 * switch/case/default/done, break, set/unset/eval/global). This file keeps
 * only the fixed ACTION vocabulary (one per script line, verb then
 * rest-of-line argument, already %var%-substituted by the time it reaches
 * trigger_dispatch_action() in trigger.c):
 *   echo <text>      -- sent to `actor` only
 *   echoroom <text>  -- sent to everyone else in `room` (actor excluded)
 *   emote <text>     -- "<self_name> <text>" sent to everyone in `room`
 *                        (actor included -- it's the mob/room "speaking")
 *   say <text>       -- "<self_name> says, '<text>'" sent to everyone in
 *                        `room` (actor included, same as emote)
 *   teleport <vnum>  -- moves `actor` to room <vnum> (no-op if no actor)
 *   give <vnum>      -- spawns object <vnum> into `actor`'s inventory
 *   damage <n>       -- deals <n> damage to `actor`, clamped so it can
 *                        never drop them below 1 HP (no death-outside-
 *                        combat handling exists yet, same limitation
 *                        `drink`'s poison already accepted)
 *   log <text>       -- LOG_SILENT game log entry (audit/debug, never
 *                        broadcast live)
 *   wait <seconds>   -- pauses the REST of this script (everything after
 *                        this line) for <seconds> real seconds (1-3600,
 *                        clamped), then resumes it -- e.g. a market-vendor
 *                        mob crying out one line at a time. The pause
 *                        survives past this trigger_run() call returning
 *                        (see trigger_pending_tick() below). As of the
 *                        2026-07-25 DG revamp, the full `set`/`eval`/
 *                        `global` variable scope AND the resume point
 *                        inside any `while` loop survive the pause intact
 *                        (trigger.c snapshots trig_ctx_t); only `actor`
 *                        itself is still NOT preserved (may have
 *                        disconnected/died/moved away by the time it
 *                        resumes) -- only `say`/`emote`/`echoroom`/`log`
 *                        lines make sense after a `wait`, since those only
 *                        need `room`/`self_name`, both safely re-derived
 *                        at resume time from the trigger's own
 *                        target_type/target_vnum (`echo`/`teleport`/
 *                        `give`/`damage`, which need a live `actor`,
 *                        silently no-op if placed after a `wait`, same as
 *                        if actor were NULL for any other reason).
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

/* Resumes any `wait`-paused trigger scripts whose time has come. Pulse-
 * registered in main.c at a ~1s cadence (matching `wait`'s whole-seconds
 * granularity) -- see trigger_run()'s `wait` doc above for what does and
 * doesn't survive the pause. A mob/room that's gone by resume time (purged,
 * moved, room unloaded) just silently drops that continuation -- no crash,
 * no error, matching every other trigger action's "actor missing -> no-op"
 * convention. */
void trigger_pending_tick(long pulse_num);

/* Testing/debug hook (`aitick`, cmd_aitick.c): runs EVERY currently-pending
 * `wait` continuation right now, regardless of how much real time is left
 * on its clock -- forcing trigger_pending_tick() itself to fire early would
 * need a fake pulse number far enough past whatever real g_now_pulse the
 * live game loop last saw, which risks corrupting the base a real `wait`
 * scheduled in the same window would resume from. This sidesteps that
 * entirely by not touching the pulse clock at all. */
void trigger_pending_force_all(void);

#endif
