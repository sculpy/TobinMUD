#ifndef TOBIN_DRUG_H
#define TOBIN_DRUG_H

#include <stdbool.h>

struct being;

/* Drug tracking (Sneezy -> Tobin feature audit, "drug tracking").
 * Checked the real upstream first (docs/systems/informational/
 * drug-tracking.md, obj/obj_component.h's TDrug/TDrugContainer split):
 * consumption applies a real, temporary stat effect, tracked for
 * addiction (lifetime average consumption rate) and withdrawal (real
 * penalty once you've gone too long without a dose). The original's
 * stat effects lean on BRA/AGI/FOC/SPE/PER/KAR -- six attributes
 * Tobin's simplified 6-stat system (STR/DEX/CON/INT/WIS/CHA) doesn't
 * have at all (a known, already-documented gap, see obj.h's objaffect
 * doc comment) -- so every drug's effect is a deliberate REMAP onto
 * Tobin's real stats, not a literal port:
 *   SPE (speed)   -> DEX
 *   FOC (focus)   -> INT
 *   KAR (karma)   -> WIS
 *   (STR/CON/CHA already exist verbatim)
 * Opium's original effect is explicitly documented upstream as BUGGY
 * (checks one stat, sets another) -- deliberately NOT ported as a bug;
 * a clean, internally-consistent penalty is used instead. Frogslime's
 * real GARBLE (speech-scrambling) effect isn't ported -- Tobin has no
 * garble/drunk-speech mechanic anywhere yet (a bigger, separate lift);
 * kept as a flavor message + a real chance of being knocked out
 * (POSITION_SLEEPING) instead, an honest, disclosed scope cut.
 *
 * Storage: `first_use`/`last_use` are real wall-clock `time(NULL)`
 * values (same convention `player.birth_time`/`held[].when` already
 * use), not the original's own mud-calendar time_info_data -- disc
 * thresholds (originally in mud-hours) are converted once, here, using
 * gametime.h's own documented ~4-real-minutes-per-mud-hour ratio, so
 * "6 game hours" becomes a real, comparable wall-clock duration without
 * needing a cumulative mud-hour counter anywhere (directly testable: a
 * test can SQL-set a fake `last_use` far in the past, then force one
 * `aitick` to recompute withdrawal against it, no real waiting needed).
 * An active dose's OWN effect window is a real tick countdown instead
 * (`effect_ticks_left`), not wall-clock, specifically so THAT part is
 * also forceable via `aitick` -- see its own field comment. Deliberately does NOT
 * port the original's own documented `current_consumed` bug (a
 * session-only counter that persists to DB and reloads stale) --
 * Tobin only ever tracks `total_consumed` (lifetime) and `last_use`,
 * which is all withdrawal/addiction math actually needs. */
typedef enum {
    DRUG_PIPEWEED,
    DRUG_OPIUM,
    DRUG_POT,
    DRUG_FROGSLIME,
    DRUG_COUNT
} drug_type_t;

/* Per-drug, per-being runtime state -- being_t.drugs[DRUG_COUNT].
 * Not persisted directly; player_drug_repo.h mirrors first_use/
 * last_use/total_consumed to the DB, loaded on login/reconnect.
 * `effect_ticks_left`/`applied[]` are deliberately NOT persisted
 * (matching `progress.hp`'s own "in-memory only, not worth saving a
 * transient buff across a reconnect" precedent) -- a fresh session
 * simply starts with no active dose in effect, same as any other
 * timed affect. */
typedef struct {
    long first_use;          /* real time(NULL), 0 = never used */
    long last_use;            /* real time(NULL) */
    long total_consumed;      /* lifetime dose count */
    int effect_ticks_left;    /* counts down once per drug_tick_run() call --
                                * same countdown-not-wall-clock convention
                                * obj_decay_tick()'s CORPSE_DECAY_TICKS uses,
                                * so `aitick` can force a dose to expire
                                * deterministically in a test instead of a
                                * real ~2-minute wait; 0 = no active dose */
    int applied[6];           /* STR/DEX/CON/INT/WIS/CHA deltas currently in effect, for clean reversal */
    int withdrawal_applied[6]; /* separate from `applied[]` -- withdrawal and
                                 * an active dose are independent, both can
                                 * apply/reverse on their own schedule */
} drug_state_t;

/* Real short name ("pipeweed"), used by `smoke` and item-keyword
 * matching. */
const char *drug_name(drug_type_t type);

/* Consumes one dose: reverses any still-active previous dose for this
 * drug first (same "consolidate rather than stack" rule the original's
 * findDrugAffect()/reapplyDrugAffect() enforce), applies this drug's
 * real stat deltas (race-checked for Pipeweed's Hobbit bonus), updates
 * first_use/last_use/total_consumed, and sets a fresh effect duration.
 * Returns the flavor message to show the smoker (caller sends it). */
const char *drug_smoke(struct being *b, drug_type_t type);

/* Pulse callback (~60s, main.c; also forced by `aitick`, cmd_aitick.c):
 * decrements any active dose's `effect_ticks_left`, reversing its stat
 * deltas once it hits 0, and recomputes/applies withdrawal penalties
 * for anyone overdue past their drug's real threshold. */
void drug_tick_run(long pulse_num);

#endif
