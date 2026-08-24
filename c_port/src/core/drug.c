/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "drug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "being.h"
#include "descriptor.h"

/* ~4 real minutes per mud-hour (gametime.h's own documented ratio) --
 * lets every withdrawal-onset "N game hours" from the real upstream
 * convert once, here, into a plain real-time duration, same convention
 * `player.birth_time`/held-message TTLs already use rather than needing
 * a cumulative mud-hour counter anywhere. */
#define REAL_SECONDS_PER_MUD_HOUR 240

/* Half a mud-hour (real upstream's own "duration = ... / 2" convention),
 * expressed as a tick count (~60s/tick, main.c) rather than wall-clock
 * seconds so `aitick` can force it deterministically. */
#define EFFECT_DURATION_TICKS 2 /* ~2 real minutes at the real ~60s cadence */

typedef struct {
    const char *name;
    int withdrawal_hours;   /* real upstream "withdrawal onset" */
    double addiction_rate;  /* real upstream "addiction threshold", doses/hour */
} drug_data_t;

/* Real per-drug thresholds, ported verbatim from the upstream doc's own
 * table (docs/systems/informational/drug-tracking.md). */
static const drug_data_t DRUGS[DRUG_COUNT] = {
    [DRUG_PIPEWEED]  = { "pipeweed",  24, 2.0 },
    [DRUG_OPIUM]     = { "opium",      6, 0.5 },
    [DRUG_POT]       = { "pot",       12, 1.0 },
    [DRUG_FROGSLIME] = { "frogslime", 18, 0.8 },
};

/* Display name for a drug_type_t (DRUGS[] above) -- returns "" for an
 * out-of-range value. */
const char *drug_name(drug_type_t type) {
    if (type < 0 || type >= DRUG_COUNT)
        return "";
    return DRUGS[type].name;
}

/* Reverses whatever stat deltas are currently recorded in `deltas`
 * (either a dose's `applied[]` or withdrawal's own
 * `withdrawal_applied[]`), then zeroes it out. Shared by dose expiry,
 * dose consolidation (a fresh dose replaces rather than stacks, same
 * rule the original's findDrugAffect()/reapplyDrugAffect() enforce),
 * and withdrawal recovery. */
static void reverse_deltas(being_t *b, int *deltas) {
    b->attrs.strength -= deltas[0];
    b->attrs.dexterity -= deltas[1];
    b->attrs.constitution -= deltas[2];
    b->attrs.intelligence -= deltas[3];
    b->attrs.wisdom -= deltas[4];
    b->attrs.charisma -= deltas[5];
    memset(deltas, 0, sizeof(int) * 6);
}

/* Records `str..cha` into `deltas` (so reverse_deltas() above can undo
 * exactly this later) and applies them to `b`'s attrs immediately --
 * the write half of the apply/reverse pair every drug effect and
 * withdrawal penalty in this file goes through. */
static void apply_deltas(being_t *b, int *deltas, int str, int dex, int con, int intl, int wis, int cha) {
    deltas[0] = str;
    deltas[1] = dex;
    deltas[2] = con;
    deltas[3] = intl;
    deltas[4] = wis;
    deltas[5] = cha;
    b->attrs.strength += str;
    b->attrs.dexterity += dex;
    b->attrs.constitution += con;
    b->attrs.intelligence += intl;
    b->attrs.wisdom += wis;
    b->attrs.charisma += cha;
}

/* Applies one dose of `type` to `b` -- consolidates onto any already-
 * active dose of the same drug (reverse then reapply, never stacks),
 * tracks usage stats for apply_withdrawal() below to work from later,
 * and applies this drug's own per-type stat effect (see the switch's
 * per-case comments for how each departs from or matches the real
 * upstream effect). Returns the flavor message to show the smoker. */
const char *drug_smoke(being_t *b, drug_type_t type) {
    if (!b || type < 0 || type >= DRUG_COUNT)
        return "Nothing happens.";

    drug_state_t *st = &b->drugs[type];
    reverse_deltas(b, st->applied); /* consolidate, don't stack -- same real-data rule */

    long now = (long)time(NULL);
    if (st->first_use == 0)
        st->first_use = now;
    st->last_use = now;
    st->total_consumed++;
    st->effect_ticks_left = EFFECT_DURATION_TICKS;

    const char *msg;
    switch (type) {
        case DRUG_PIPEWEED:
            /* Real upstream race-specific case: Hobbits get a genuine
             * bonus, every other race gets a multi-stat penalty (SPE,
             * KAR, CHA, FOC -> DEX, WIS, CHA, INT). */
            if (b->race == RACE_HOBBIT) {
                apply_deltas(b, st->applied, 0, 0, 0, 9, 0, 0);
                msg = "You pack a bowl of pipeweed and light up -- your mind sharpens wonderfully.\r\n";
            } else {
                apply_deltas(b, st->applied, 0, -2, 0, -2, -2, -2);
                msg = "You pack a bowl of pipeweed and light up, coughing through a wave of dizziness.\r\n";
            }
            break;
        case DRUG_OPIUM:
            /* The real upstream effect is documented as outright buggy
             * (checks one stat, sets another) -- deliberately NOT
             * ported; a clean, internally-consistent numbing penalty
             * instead. */
            apply_deltas(b, st->applied, 0, -3, 0, 0, -2, -1);
            msg = "You draw deep on the pipe -- a heavy numbness spreads through you.\r\n";
            break;
        case DRUG_POT:
            /* Positive SPE/CHA, negative INT/FOC in the original --
             * both FOC and INT collapse onto Tobin's single INT stat,
             * so that penalty lands as one flat INT hit rather than
             * two separate ones. Flat magnitude (Tobin-scale), not the
             * original's scale-with-consumed-count formula. */
            apply_deltas(b, st->applied, 0, 2, 0, -3, 0, 2);
            msg = "You take a long drag -- you feel quicker and more charming, if a little scattered.\r\n";
            break;
        case DRUG_FROGSLIME:
        default:
            /* No stat effect in the original either (GARBLE + optional
             * SENSE_LIFE + a chance of sleep) -- Tobin has no garble/
             * drunk-speech mechanic to port that into (a separate,
             * bigger lift), so this is flavor + the real sleep chance
             * only, an honest scope cut. */
            if (rand() % 100 < 25) {
                b->position = POSITION_SLEEPING;
                msg = "You lick the frogslime -- your knees buckle and everything goes dark.\r\n";
            } else {
                msg = "You lick the frogslime -- your tongue goes numb and the room tilts sideways.\r\n";
            }
            break;
    }
    return msg;
}

/* Recomputes and re-applies (or clears) withdrawal for one drug on one
 * being, called every tick. Withdrawal severity in the original scales
 * with (average consumption rate) * (hours overdue past onset) --
 * ported directly, just against the real-time thresholds converted
 * above instead of a cumulative mud-hour counter. STR loses severity/2,
 * CON loses severity/3, same real upstream ratio. */
static void apply_withdrawal(being_t *b, drug_type_t type, long now) {
    drug_state_t *st = &b->drugs[type];
    const drug_data_t *dd = &DRUGS[type];

    reverse_deltas(b, st->withdrawal_applied); /* always recompute fresh */

    if (st->total_consumed <= 0 || st->last_use == 0)
        return;

    long hours_since_last = (now - st->last_use) / REAL_SECONDS_PER_MUD_HOUR;
    if (hours_since_last < dd->withdrawal_hours)
        return;

    long hours_since_first = (now - st->first_use) / REAL_SECONDS_PER_MUD_HOUR;
    if (hours_since_first <= 0)
        hours_since_first = 1;
    double rate = (double)st->total_consumed / (double)hours_since_first;
    if (rate < dd->addiction_rate)
        return;

    double severity = rate * (double)(hours_since_last - dd->withdrawal_hours + 1);
    int str_pen = -(int)(severity / 2.0);
    int con_pen = -(int)(severity / 3.0);
    if (str_pen == 0 && con_pen == 0)
        return;

    apply_deltas(b, st->withdrawal_applied, str_pen, 0, con_pen, 0, 0, 0);
    if (b->desc) {
        char msg[128];
        snprintf(msg, sizeof(msg), "<y>You ache for %s -- withdrawal is setting in.<z>\r\n", dd->name);
        descriptor_notify(b->desc, msg);
    }
}

/* Runs on a timer (see main.c): for every connected non-immortal PC and
 * every drug type, counts down that dose's remaining effect (reversing
 * its stat deltas once it expires) and recomputes withdrawal via
 * apply_withdrawal() above. Immortals are skipped outright -- same
 * immunity convention as affect.c's disease/poison ticks. */
void drug_tick_run(long pulse_num) {
    (void)pulse_num;
    long now = (long)time(NULL);
    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        being_t *b = d->character;
        if (!b || being_is_immortal(b))
            continue;
        for (int i = 0; i < DRUG_COUNT; i++) {
            drug_state_t *st = &b->drugs[i];
            if (st->effect_ticks_left > 0) {
                st->effect_ticks_left--;
                if (st->effect_ticks_left == 0)
                    reverse_deltas(b, st->applied);
            }
            apply_withdrawal(b, (drug_type_t)i, now);
        }
    }
}
