/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_SPELL_FLAVOR_H
#define TOBIN_SPELL_FLAVOR_H

#include <stdbool.h>

#include "being.h"
#include "descriptor.h"

/* Shows a caster/pray-er three flavor lines for a cast/pray attempt --
 * a random gesture line, a random verbal line (each also echoed, in a
 * third-person form, to the rest of the room), and a fixed completion
 * line seen only by the caster -- modeled on real SneezyMUD's
 * TBeing::sendCastingMessages() (spelltask.cc), which shows the same
 * three categories once per ROUND of its real multi-round casting
 * task. Tobin's cast/pray commands resolve instantly (no multi-round
 * task engine exists), so this ports the STYLE (the "3 lines" a user
 * asked for, 2026-08-04/08-05) as a single one-shot flourish before
 * the spell's real effect, not the full per-round distraction/
 * interrupt architecture -- see TODO.md's "examine spell architecture"
 * item for that larger, still-open question. Called once, at the top
 * of task_cast()/task_pray(), for every successful spell/prayer alike
 * (uniform across the whole roster, same "one rule" precedent the
 * component/symbol requirement already uses). */
void spell_flavor_show(descriptor_t *d, being_t *ch, bool is_prayer);

#endif
