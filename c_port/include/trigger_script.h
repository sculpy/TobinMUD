/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_TRIGGER_SCRIPT_H
#define TOBIN_TRIGGER_SCRIPT_H

#include <stdbool.h>
#include <stddef.h>

#include "trigger_repo.h"

struct being;
struct room;

/* DG Scripts-style interpreter core (user 2026-07-25: "use the DG_* source
 * files to revamp triggers" -- full language port, scoped to the LANGUAGE
 * only; the authoring flow stays `edit trigger ...`, per "stick to the edit
 * unification", not a separate dg_olc.c-style editor). Ported from tbaMUD's
 * dg_scripts.c/dg_variables.c: %var% substitution, if/elseif/else/end,
 * while/done, switch/case/default/done (real fallthrough, matching DG),
 * break, set/unset/eval/global. NOT ported: DG's full command sets
 * (dg_mobcmd.c/dg_objcmd.c/dg_wldcmd.c -- hundreds of mob/obj/room-specific
 * script commands), remote/context/attach (multi-script targeting), and
 * dg_olc.c (script authoring stays the existing `edit trigger` flow, one
 * script per trigger row, not DG's separate numbered-script-per-vnum
 * model). Tobin's existing action vocabulary (echo/echoroom/emote/say/
 * teleport/give/damage/log/wait) is unchanged in meaning, just now
 * %var%-substituted and usable inside if/while bodies. */

#define TRIG_VAR_MAX 16
#define TRIG_VAR_NAME_LEN 32
#define TRIG_VAR_VALUE_LEN 128
#define TRIG_LINES_MAX 64
#define TRIG_LINE_LEN 256

typedef struct {
    char name[TRIG_VAR_NAME_LEN];
    char value[TRIG_VAR_VALUE_LEN];
} trig_var_t;

/* One script run's full variable scope + context. Persists across a `wait`
 * pause (trigger.c snapshots vars[]/var_count into the pending continuation
 * and restores them verbatim on resume) -- unlike the pre-revamp `wait`,
 * which dropped all script state and only re-derived room/self_name fresh.
 * `actor`/`room` are still NOT safely held across a real-time pause (may
 * have disconnected/moved/died) -- trigger.c re-derives those at resume
 * time, same as before; only the variable scope itself survives. */
typedef struct {
    trig_var_t vars[TRIG_VAR_MAX];
    int var_count;
    struct being *actor;
    struct room *room;
    const char *self_name; /* mob/room's own capitalized display name, or NULL */
    const char *arg;       /* %arg% -- the matched keyword/command tail, or NULL */
} trig_ctx_t;

/* Splits `script_text` into `lines_out` (up to TRIG_LINES_MAX pointers into
 * `buf`, which must be at least strlen(script_text)+1 bytes and stays owned
 * by the caller for as long as lines_out is used). Returns the line count.
 * Both the initial run and a `wait` resume call this on the SAME original
 * script text, so line indices (and therefore a saved resume_pc) stay
 * stable across the pause. */
int trig_script_split(const char *script_text, char *buf, size_t bufsz,
                      char *lines_out[TRIG_LINES_MAX]);

typedef enum {
    TRIG_EXEC_DONE, /* ran to the end (or hit an error) -- nothing pending */
    TRIG_EXEC_WAIT, /* hit a `wait` line -- out_resume_pc/out_wait_secs are set */
} trig_exec_result_t;

/* Executes `lines[0..nlines)` starting at `start_pc`, mutating `ctx`'s
 * variable scope as `set`/`unset`/`eval`/`global` lines run. On
 * TRIG_EXEC_WAIT, `*out_resume_pc` is the line to resume at and
 * `*out_wait_secs` is the (already-clamped 1-3600) wait duration; the
 * caller is responsible for scheduling the continuation (trigger.c). */
trig_exec_result_t trig_script_exec(trig_ctx_t *ctx, char *const lines[], int nlines,
                                    int start_pc, int *out_resume_pc, int *out_wait_secs);

/* Runs one line of the fixed action vocabulary (echo/echoroom/emote/say/
 * teleport/give/damage/log) -- implemented in trigger.c, which owns the
 * actual side effects (being.h/room.h/obj_repo.h context this file
 * deliberately doesn't pull in). `arg` has already been %var%-substituted.
 * Unrecognized verbs are silently skipped, same typo-tolerant convention
 * as everywhere else in this language. */
void trigger_dispatch_action(trig_ctx_t *ctx, const char *verb, const char *arg);

/* Persisted global variables (DG's `global`/`context` scope, simplified to
 * a single flat world-wide key/value store -- no per-context namespacing,
 * since Tobin's triggers don't have DG's numbered-script-instance model to
 * namespace against). Backed by `trigger_global_var` (trigger_global_var.sql).
 * `trigger_global_get` returns false if the key has never been set. */
bool trigger_global_get(const char *name, char *out, size_t outsz);
bool trigger_global_set(const char *name, const char *value);

#endif
