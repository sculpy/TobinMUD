/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef SPELL_COMPONENT_H
#define SPELL_COMPONENT_H

#include <stdbool.h>

typedef struct obj obj_t;
typedef struct being being_t;
typedef struct thing thing_t;

/* Per-spell spell-component system, ported from real SneezyMUD's
 * obj_component.cc (TComponent / CompIndex / findComponent / doMerge).
 *
 * Real Sneezy binds each material spell to a SPECIFIC reagent object: an
 * ITEM_COMPONENT (obj.type==30) carries the spell it is a component FOR in
 * its val2 (a "file spell number"), and a caster must be holding the right
 * reagent for the exact spell they are casting -- not just any generic
 * pouch. That binding data was imported into Tobin's obj table verbatim
 * (every type-30 row still has val2=file-spell-number, val3=usage flags),
 * so this module reuses the real data rather than inventing bindings.
 *
 * COMP_FILEMAP (spell_component_data.h, generated from Sneezy's own
 * mapFileToSpellnum()) decodes val2 -> Tobin spell name. At boot,
 * spell_component_init() indexes every type-30 obj by the spell it binds
 * to, so we can (a) find the RIGHT carried component for a spell,
 * (b) name the component a caster is missing, and (c) load level-
 * appropriate components onto caster mobs.
 *
 * Backward compatibility: a spell with no bound component anywhere in the
 * world (e.g. a Tobin-original spell, or one whose reagent was never
 * seeded) falls back to the old generic "any item keyworded component"
 * behavior at the cast site, so nothing that cast before this system
 * still refuses now. See cmd_cast.c. */

/* Highest total charges a merged component stack may hold (real
 * TComponent::willMerge()'s own 100-charge cap). */
#define SPELL_COMP_MERGE_CAP 100

/* Ticks of decay given to a freshly spawned/looted component so a dropped
 * reagent eventually ages out of the world (real components decay; Tobin's
 * obj_decay_tick() only counts down while an object sits on a room floor,
 * so a carried/bagged reagent never expires -- deliberately player-
 * friendly, same "decay only ticks in rooms" rule the rest of the codebase
 * already follows). Starting-gear/reset content overrides this to -1. */
#define SPELL_COMP_DECAY_TICKS 300

/* Boot-time: build the spell-name -> component-vnum index from the live obj
 * table (type=30 rows). Safe to call once, after the DB is up. */
void spell_component_init(void);

/* True iff `o` is an ITEM_COMPONENT (raw upstream type 30). */
bool obj_is_spell_component(const obj_t *o);

/* The Tobin spell name this component is bound to (via COMP_FILEMAP on its
 * val[2]), or NULL if it is not a bound component (val[2] unmapped/-1). */
const char *spell_for_component(const obj_t *o);

/* Representative component vnum bound to `spell` (the lowest vnum, matching
 * Sneezy's own comp_num pick), or 0 if no seeded component binds to it. */
int spell_bound_component_vnum(const char *spell);

/* short_descr of the representative component for `spell`, for a "you need
 * <X>" refusal message, or NULL if the spell has no bound component. */
const char *spell_bound_component_name(const char *spell);

/* The caster's own carried component for `spell` with charges left, or NULL.
 * Searches carried/worn/held plus one level into containers (spellbags),
 * a strict superset of the old top-level-only find_keyword_item() scope. */
obj_t *spell_component_find_for(being_t *ch, const char *spell);

/* Fold every same-vnum component sitting directly alongside `o` in its
 * parent into `o` (summed charges capped at SPELL_COMP_MERGE_CAP, charge-
 * weighted-average decay time) -- real TComponent::doMerge(). Safe no-op
 * for a non-component or a lone component. Returns how many siblings were
 * merged away (and destroyed). */
int spell_component_merge_siblings(obj_t *o);

/* Load up to `max_items` distinct level-appropriate components into `dest`
 * for caster mob `mob`: one per spell the mob's class can cast at or below
 * its level + 2 ("saving up for future spells", user 2026-08-10) that has a
 * seeded component. Returns the number placed. */
int spell_component_grant_caster(being_t *mob, thing_t *dest, int max_items);

#endif /* SPELL_COMPONENT_H */
