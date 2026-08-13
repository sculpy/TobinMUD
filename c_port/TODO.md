# Tobin — TODO

Last updated: 2026-07-07. Companion to STATUS.md, which holds the full
session log, decisions, and history — **this file tracks only what's NEXT.**
Completed items are pruned from here as they land (find them in STATUS.md).

All in-game editors are menu-driven, like character creation — see the
[[editors-menu-driven]] memory. The user provides a wireframe for each.
Editor commands are unified under **`edit <noun> [args]`** (user
2026-07-11, superseding the old separate `ed<noun>` verbs from
2026-07-05): `edit room` (rooms), `edit zone` (zones), `edit help`
(help), `edit news` (news), `edit wiznews` (wiznews), `edit player`
(players); future `edit object`/`edit mob`/`edit account`. Read-only
viewers keep plain names (`news`, `wiznews`).

## PC-race perk system + menu-driven `balance` rework (user 2026-08-10: "do it all")

Turns the Sneezy RACE_* per-race data (hp/mana/move/food/drink mods,
IMMUNE_* resistances, infravision, talents) into real, tunable PC-race
perks, delivered through the existing data-driven `race_balance` table so
every value stays live-editable via `balance`. Decisions taken (per the
design discussion): net-zero across a race's whole identity (upside paired
with a drawback, not per-subsystem), Human keeps a signature "adaptable"
flex perk, magnitudes scaled down from Sneezy (all tunable live anyway).
See docs/RACE_STATS.md (stat layer) + docs/RACE_PERKS.md (this).

Extended balance_mod_t / class_balance+race_balance columns (class rows
keep the race-only perks neutral):

- Tier 0 numeric: mana_mult, move_mult (scale max mana / max move, mirror
  the existing hp_mult), food_mult, drink_mult (hunger/thirst decay rate).

- Tier 1 resistances (0-100% each, race only): resist_poison, resist_charm,
  resist_sleep, resist_paralysis, resist_energy, resist_heat, resist_cold.

- Tier 2 senses (race only): infravision (0/1 innate dark-vision).

- Tier 3 talent (race only): one enum per race (Ogre brawler, Hobbit
  woodland stealth, Gnome innate detect-magic, Human adaptable skill-gain).

- [x] **Phase 1 — data model + seed + menu rework + Tier 0/Tier 2 apply.**
  
      Schema migration (balance.sql) + balance_mod_t/balance_repo for all new
      columns; seed scaled Sneezy values for the 6 races; apply mana_mult
      (being_calc_max_mana), move_mult (being_calc_max_vit), food/drink_mult
      (vitals.c decay), infravision (room_is_dark_for). Rework the `balance`
      editor into fully self-documenting menu screens: per-field label,
      one-line "what it does", neutral baseline, current value, and a
      live-effect line, split into Combat / Vitals / Resistances / Senses /
      Talent sections (race) vs Combat only (class). Smoke test.

- [x] **Phase 2 — Tier 1 resistance application.** being_race_resist(type)
  
      helper; apply at the existing hooks (charm/sleep via the affect save,
      poison via drink/sip). Elemental types (heat/cold/energy/paralysis)
      stored + editable now, applied where/when such damage sources exist
      (disclosed-pending, shown as such in the menu). Smoke test.

- [x] **Phase 3 — Tier 3 talents.** Wire the concrete talents: Gnome innate
  
      detect-magic, Hobbit sneak/hide/search bonus, Ogre brawling-skill
      bonus, Human across-the-board learn-by-doing bonus. Smoke test +
      docs/RACE_PERKS.md finalized.

## User batch 2026-08-10 — done same session
- [x] **All-classes immortals** -- "make all immortals all classes upon
      promotion to immortal and update current immortal characters to be
      all classes." New `CLASS_ALL` sentinel; `promote` sets it on
      crossing into immortal range; existing immortals backfilled live.
      See STATUS.md Session 148.
- [x] **`practice <class>` browses any class's skill listing** -- "morts
      should be able to see what skill is offered in classes they are
      not, so this works for immorts and morts alike" / "let prac class
      also count for prac basic." See STATUS.md Session 148.
- [x] **Login log line** -- "[PIO] <N> has entered the game in room
      <vnum>. [host], just before load room fires." See STATUS.md
      Session 148.

- [x] **Add news to tobinmud website** -- user, 2026-08-10: show the
      in-game `news` feed on the public site, each entry carrying its
      date. Likely mirrors `web/generate_help_json.sh`'s existing
      pattern (news.sql/`news` table -> a generated page under
      `~/TobinMUD/web/`) -- not yet scoped further.

## Sneezy spell-component system, disclosed follow-up from the container-stacking task (2026-08-08)

- [ ] sneezymud-master/code/code/obj/obj_component.cc (~2500 lines) is a full material-spell-component subsystem: TComponent::willMerge()/doMerge() merge same-vnum components together (weighted-average decay time, summed cost/charges) up to a 100-charge cap; a large `component_placement` table spawns specific components (e.g. "cheval") into specific rooms under specific weather/time-of-day windows for later collection. Tobin has no equivalent at all -- not attempted as part of the 2026-08-08 "containers should stack" fix (that was a display-grouping gap, this is a whole missing gameplay system). Real follow-up if picked up: would need its own scoping pass (which spells actually need material components in Tobin's current spell roster, whether the room/weather spawn-table approach is wanted or a simpler always-available-for-purchase model fits Tobin better).

## Open follow-ups, logged 2026-08-04

- [ ] **Examine how spells are formed in real SneezyMUD and port the approach here** -- user, 2026-08-04, no further detail yet. Likely means reviewing `sneezymud-master/code/code/misc/spell_info.cc`'s discArray[]/spell-casting architecture (already the source used for the spell/skill audit's roster data) for structural/mechanical patterns Tobin's `cmd_cast.c`/`cmd_pray.c` keyword-dispatch approach doesn't currently capture -- scope not yet defined.

## Spell/skill/spec-proc coverage audit — logged 2026-08-04

User 2026-08-04: "unimplemented sneezy spells and skills should all be
listed in the to do list, as well as all the special procs" / "I also
want all skills and spells to have effect to actually do something."
Spec-procs were already tracked in [SPEC_PROCS.md](SPEC_PROCS.md); the
three checklists below are new, from a fresh audit (Explore agent,
Session 128) comparing `sneezymud-master/code/code/misc/spells.h`'s
enum + `spell_info.cc`'s `discArray[]` (463 named entries) against
Tobin's own `skill.c`'s `SKILLS[]` roster (283 unique names). Most of
the "missing" totals are classes Tobin doesn't implement at all
(Deikhan/Ranger/Shaman/Psionicist -- a class-scope decision, not
per-spell oversight); Druid absorbed a reflavored subset of Ranger/
Shaman. Not yet triaged into session-sized work items -- this is the
raw checklist, pick from it as capacity allows.

### Spell port status — 56 of 231 SneezyMUD spells not yet in Tobin

#### Mage (3)

- [ ] Death mist — **re-triaged 2026-08-05:** real upstream discArray[SPELL_DEATH_MIST] is actually DISC_SHAMAN, not DISC_MAGE (TODO's own audit mis-attributed it) -- Tobin has no Shaman class, so this is a class-scope-out item like the rest of the Shaman list below, not a Mage gap.

#### Druid (9) — no Ranger class in Tobin

- [ ] Creeping doom
- [ ] Root control
- [ ] Shapeshift
- [ ] Sticks to snakes
- [ ] Stormy skies
- [ ] Apply herbs
- [ ] Beast summon
- [ ] Transfix
- [ ] Transform limb

#### Generic / cross-class (50)

- [ ] Avian (language)
- [ ] Bullycroak (language)
- [ ] Climbing
- [ ] Common (language)
- [ ] Dissect
- [ ] Divine (fortune-telling; distinct from Mage's "divination" spell)
- [ ] Encamp
- [ ] Fast load
- [ ] Fish burble (language)
- [ ] Fishing
- [ ] Fishlore
- [ ] Gnoll jargon (language)
- [ ] Gutter cant (language)
- [ ] Know animal (creature lore)
- [ ] Know demon
- [ ] Know giantkin
- [ ] Know other
- [ ] Know people
- [ ] Know reptile
- [ ] Know undead
- [ ] Know veggie
- [ ] Lumberjack
- [ ] Mend
- [ ] Read magic
- [ ] Seekwater
- [ ] Skinning (generic, non-Druid classes)
- [ ] Troglodyte pidgin (language)
- [ ] Trollish (language)
- [ ] Turn undead (command `turn` -- no display-name string upstream, real and missing regardless)

### Spell/skill stub audit — roster-listed but mechanically no-op

`cmd_cast.c` (Mage/Druid) and `cmd_pray.c` (Cleric) dispatch each spell by
matching its own `name`/`desc` string against a fixed set of keyword
branches; anything matching none of them falls through to a literal
"nothing happens yet" placeholder. These are castable/practicable TODAY
(pass class+level+component gating, appear in `skills`/`practice`) but
hit that fallback -- their help text promises an effect that never fires.
A secondary bug found along the way: the damage-keyword check is a
literal substring match on "damage" (not "damag(e|es|ing)"), so several
spells whose own description says "damages"/"damaging" ALSO silently
miss the damage branch -- flagged inline below.

**2026-08-09 update:** `skill_def_t.desc` (skill.c's roster array) is now
a dead placeholder ("See help `X` for help.") for all 416 entries -- the
real descriptive prose this whole audit's keyword branches were matching
against moved to the `help_topic` table at some point after the audit
above was written, silently breaking every `ci_contains(sk->desc, ...)`
branch (any spell without its own bespoke `strcasecmp(sk->name, ...)`
branch, e.g. `gust`, fell through to the placeholder). Fixed by having
`cmd_cast_resolve_effect()`/`task_pray()` look up the live `help_topic`
body for the spell's own name (`help_topic_load_exact()`, falling back
to `sk->desc` if a spell has no help topic yet) and matching keywords
against THAT instead -- the help bodies still carry the same prose the
keyword list below was always written against, so no branch logic
changed, just its text source. 37 Mage/Druid + 16 Cleric roster entries
were silently regressed this way and are working again (gust, mystic
darts, chain lightning, meteor swarm, ice storm, tornado, tsunami,
blizzard, plasma mirror, energy drain, protection from energy, levitate,
gills of flesh, harm light/serious/critical/harm, call lightning,
sanctuary, bless, armor, spontaneous combust, and more -- full list in
the fix commit). Did NOT restructure the roster into a per-spell effect-
category enum (the task's "Option B") -- the closed keyword set was
small enough that fixing the lookup's data SOURCE was the surgical fix,
not a reason to redesign the dispatch mechanism.

## Spell/skill functional-completeness audit (2026-07-27)

User asked whether every spell/skill in the roster (`skill.c`, ~230
entries) is actually implemented with a real handler, real affects/
durations where warranted, and a cooldown where warranted. Audit findings
(full report given in chat, not duplicated here):

- **Placeholder-only spells** (cast/pray fall through to "...but nothing
  happens yet"): blindness, slumber, fear, curse, paralyze/paralyze limb,
  invisibility, teleport, summon, word of recall, telepathy, dispel
  magic/invisible, identify, materialize, farlook, scribe, bind, silence.
  Root cause: `include/affect.h`'s `AFFECT_*` enum has no blind/sleep/
  paralyze/fear/curse/invisible entry to even apply -- needs the enum
  extended before any of these can get a real per-spell effect (same
  "new subsystem needed first" shape the Stupidity affect took, see the
  roster-import section above).
- **Skill roster entries with literally no handler** (fall through to
  "Command not found"), sorted by min_level ascending -- this is the
  active work list, lowest level first per the user's instruction:
  - Level 1: **backstab** (Thief) -- done 2026-07-27. **rescue**
    (Warrior) -- done 2026-07-27. **trip** (Warrior) -- done 2026-07-27.
    New `cmd_backstab.c`/`cmd_rescue.c`/`cmd_trip.c`, registered in
    `cmd_table.c`/`cmd_internal.h`, same one-`skill_roll_success()`-roll
    shape as bash/kick/disarm. `tests/smoke_test_skillcombat2.py` (7
    checks) passes live against the rebuilt+restarted dev server.
    **steal** (Thief) -- done 2026-07-27. `steal gold <target>` /
    `steal <item> <target>`, reusing cmd_plant.c's Thief reverse-
    pickpocket gate/chance shape in reverse (mutual `toggle pk` consent,
    level-gap-scaled chance). New `cmd_steal.c`. `tests/smoke_test_steal.py`
    (8 checks) passes live.
    **sneak** (Thief) -- done 2026-07-27. Plain toggle (new `being_t.
    sneaking` field, live in-memory only like `fighting`) that suppresses
    your own arrival/departure room echo in cmd_move.c while moving;
    broken outright the instant `fighting` gets set (cmd_attack.c/
    cmd_backstab.c both clear it). Does NOT hide you from a stationary
    room's person-listing -- that stays `hide`'s separate, higher-level
    (31) job, not touched this pass. New `cmd_sneak.c`.
    **grapple** (Warrior) -- done 2026-07-27. Reuses `being_set_wait()`
    for the "restricting what they can do" part instead of a new
    restrain flag -- a successful grapple locks up BOTH combatants (not
    just the defender, unlike bash/trip's knockdown) for 3 rounds. New
    `cmd_grapple.c`.
    **berserk** (Warrior) -- done 2026-07-27. New plain flag/timer
    `AFFECT_BERSERK` (affect.h/affect.c, 8 rounds): combat.c's parry
    check now skips itself entirely against a berserking attacker, and
    cmd_rescue.c refuses to let anyone rescue a berserking ally --
    both roster-described effects wired at their natural call sites
    rather than a new subsystem. New `cmd_berserk.c`.
    **rally** (Warrior) -- done 2026-07-27. New stat-modifying
    `AFFECT_RALLY` (+STRENGTH, standing in for "combat prowess" --
    Tobin has no separate hitroll/damroll stat), reusing the
    `being_apply_stat_affect()`/`affect_stat_target()` machinery the
    Stupidity affect already built. Applies to every other PC/mob in the
    room except the rallier's own current opponent (no team/faction
    concept to test against instead) for 8 rounds. New `cmd_rally.c`.
    `tests/smoke_test_skillcombat3.py` (12 checks covering all four)
    passes live.
    **garrotte** (Thief) -- done 2026-07-27. Real upstream needs a
    dedicated TOOL_GARROTTE item (wears down, snaps after N uses) --
    scoped down to a bare-handed opener (same "only works before either
    side is fighting" shape as backstab) that applies
    `AFFECT_DISEASE_GARROTTE` (already modeled in Tobin's affect system;
    duration 90 matches cmd_drink.c's own table for the same disease)
    instead of a one-time damage number. New `cmd_garrotte.c`.
    **throatslit** (Thief) -- done 2026-07-27. Real upstream needs a
    wielded piercing/slicing weapon and a separate willKill() instant-
    death roll -- scoped down to the same opener shape as backstab, just
    hitting harder (x6 vs x4) instead of a separate kill check. New
    `cmd_throatslit.c`. `tests/smoke_test_thiefmurder.py` (6 checks)
    passes live.
    Remaining Level 1 entries -- Monk basic-stance cluster (yoginsa,
    jirin, kubo, chi, oomlat, catfall, catleap). **Research done
    2026-07-27** (checked the real upstream source directly, not
    guessed): these are NOT one shape at all --
    - **yoginsa** IS a real player-invoked ability, `task_yoginsa()`
      (`disc/disc_monk_meditation.cc:9`): a background multi-tick task
      (must be resting/sitting, re-checks every 4 pulses) that restores
      HP/Move/mana on a skill roll each tick, plus chained secondary
      rolls at higher proficiency (self-salve at 20+, cure poison 35+,
      sterilize 50+, cure disease 60+ -- all gated on a SEPARATE skill,
      "wohlin meditation", that isn't even in Tobin's roster). Most
      implementable of the seven, but Tobin has no existing "start a
      background self-heal task and let it run for N ticks" pattern to
      reuse (planting.c's `planting_ticks_left` is the closest shape,
      single-purpose for seed-growing) -- would need a new small task
      mechanism, not a one-shot skill_roll_success() roll like
      backstab/trip/etc.
    - **jirin**, **kubo**, **oomlat** are NOT commands at all -- they're
      PASSIVE combat-math modifiers checked directly inside the
      original's `misc/combat.cc`, the same "no cmd_*.cc file, wired
      straight into the hit/damage pipeline" shape as Tobin's own
      `parry` (combat.c:450). Specifically: **oomlat** is a passive AC
      bonus scaled by skill value (`armor * skill/250`, combat.cc:2787);
      **kubo** is a passive bonus appearing in several combat
      calculations (`3 * skill`/`3.0 * skill/10`, combat.cc:1928,1933,
      5981,5985 -- exact effect not fully traced, looked like a
      dodge/damage-reduction factor); **jirin** is checked via a
      `bSuccess()` roll inside `disc_monk_mind_body.cc` (a passive
      reactive save, not traced to its exact effect either). Wiring
      these in properly means finding and modifying Tobin's own
      to-hit/AC/damage formulas in `combat.c`, not writing a new
      `cmd_*.c` file -- a different, riskier kind of change than every
      other skill in this audit, and worth a deliberate look at
      `combat.c`'s current formulas first rather than guessing at the
      exact multiplier to port.
    - **catfall**/**catleap**: spell_info.cc registers `catfall` with a
      `TOG_HAS_CATFALL` toggle bit (`toggle.h:161`) but no other file in
      the whole codebase actually CHECKS that bit -- no fall-damage
      function references it anywhere greppable. Either fall damage
      lives in a file this search missed, or catfall was already
      vestigial/unfinished in the real upstream. `catleap` has no
      dedicated code at all beyond its spell_info.cc registration.
      Before porting either, worth confirming Tobin even HAS a fall-
      damage mechanic to hook a reduction into (unclear from this pass).
    - **chi**: registered in spell_info.cc (`misc/spell_info.cc:2340ish`,
      STAT_INT, a mana-recharge flavor message "You feel your inner chi
      recharge") but no task_chi()/doChi() function was found alongside
      yoginsa's real implementation -- likely folds into the same
      meditation task as yoginsa (chi refills mana the way yoginsa
      refills HP/Move) rather than being a fully separate mechanic.
      Worth confirming by reading disc_monk_meditation.cc in full (only
      partially read this pass) before scoping.
      **yoginsa** (Monk) -- done 2026-07-27. Single-action heal (not the
      real background multi-tick task -- Tobin has no generic "start a
      self-task" mechanism to reuse), heals HP + Vitality (Tobin's two
      resources; no separate mana pool exists to touch), gated on
      sitting/resting and not fighting. New `cmd_yoginsa.c`.
      `tests/smoke_test_yoginsa.py` (4 checks) passes live.
      **chi** (Monk) -- done 2026-07-27. The "folds into yoginsa's
      meditation task" guess above was wrong -- reading
      disc_monk_meditation.cc in full found no chi involvement at all;
      `misc/being.cc`'s `TBeing::doChi()`/`chiSelf()`/`chiTarget()`/
      `chiRoom()`/`chiObject()` is a real, separate, much bigger skill:
      primarily an OFFENSIVE chi-blast against a target, plus a
      self-buff (mana refill + temp cold immunity), a room-wide AOE
      attack, and an object-targeted effect -- skill.c's own pre-existing
      flavor text ("a mana-based healing touch") was simply wrong, fixed
      to match. Scoped down to `chi [<target>]` only (single-target
      attack, WIS-scaled damage, usable whether or not already fighting,
      defaults to your current opponent like the real `doChi()`) --
      self/room/object all key off mana, which Tobin doesn't have at all
      (casting/praying is component-consumption-based instead), so
      there's nothing to refill/spend/gate a cooldown on for those three;
      cut, same disclosed-scope-cut spirit as drug.c's opium/frogslime.
      Reuses `combat_find_room_target()` (PK-consent + linkdead exclusion
      already built in, same as `attack`) rather than a new target-
      resolution path. New `cmd_chi.c`. `tests/smoke_test_chi.py` (7
      checks) passes live.
      **jirin/kubo/oomlat** -- done 2026-07-27. Wired directly into
      `combat_strike()` (not new commands): jirin is a passive dodge roll,
      same shape as the existing `parry` check -- initially gated on the
      attacker being unarmed, corrected once the fuller peel-sneezymud
      reference clone (not the originally-bundled sneezymud-master/)
      turned up the real `monkDodge()`: it's a general anti-hit defense
      ("a replacement for Monk's lack of AC" per its own comment), checked
      against ANY incoming hit regardless of the attacker's weapon; kubo
      adds an unarmed to-hit+damage bonus (fills the same slot a weapon's
      hitroll/damroll would); oomlat adds an unarmed AC-style to-hit-
      denial bonus. All three proficiency-scaled, ~12 points at 100%
      (comparable to existing modifiers like `NON_STANDING_HIT_BONUS`).
      `tests/smoke_test_monkpassives.py` (3 checks) passes live.
      **catfall/catleap** -- done 2026-07-27. User: "the goal is to remain
      as close as possible to the original" -- built the real fall-damage
      mechanic (`fall.c`, ported from `TBeing::checkFalling()`) rather than
      cutting catfall for lack of one. New `sector_is_fall()`/`sector_is_
      water()` (room.c, same substring-bucketing convention as `room_can_
      plant()`) mark ATMOSPHERE/MAKE FLY sectors as open air; `fall_check()`
      (called from cmd_move.c after any successful move) drops a being
      through consecutive DIR_DOWN-linked fall sectors, landing tier
      decided by how many rooms were fallen through against two
      thresholds (num1 = max survivable depth, 10 with catfall else 5;
      num2 = num1-2) -- an agility-style DEX roll can land clean below
      num2, a CON-scaled roll decides a crushing landing vs. death between
      num2 and num1, and it's unconditionally fatal beyond num1 (new
      `combat_fall_kill_pc()`, an environmental death mirroring
      `combat_drown_pc()`). catfall halves damage; water landings soften
      it further. Real primitives Tobin doesn't have were adapted, not
      skipped: `getConShock()`/`isAgile()` -> flat CON/DEX-scaled percentage
      rolls; `break_bone()` -> splitting the landing damage across both
      legs via `being_hurt_limb()` (a leg-specific `being_hurt_limb()` call
      on top of the normal damage would have double-counted it -- caught
      live, since that function deducts from overall HP too, not just the
      limb). catleap's own real function (`doLeap()`) was found only in
      the fuller peel-sneezymud clone too -- ported as `cmd_catleap.c`:
      refuses while fighting or standing on open air, spends 15 Vitality
      (Move's Tobin equivalent), grants a brief `AFFECT_FLYING` then
      dispatches the real typed direction through `cmd_dispatch()` rather
      than reimplementing movement, reusing do_move()'s own cost/
      messaging/fall-check logic for free. `tests/smoke_test_catfall.py`
      (8 checks, including a statistical catfall-halves-damage comparison
      that needed bumping from 10 to 25 samples live to reliably separate
      from background HP-regen noise) passes live.
  - Level 5+ (sorted ascending): **cintai** (Monk, 5) -- done 2026-07-27.
    Wired into `combat_strike()` directly (not a command), same as
    jirin/kubo/oomlat. skill.c's own roster text called it "A passive
    to-hit bonus while unarmed" -- wrong per the real source
    (`attackRound()`, misc/combat.cc, fuller peel-sneezymud reference
    clone): a GENERAL to-hit bonus used for every attack regardless of
    weapon, alongside level scaling and a mounted Chivalry bonus. Ported
    the real formula directly (`(skillValue/20.0)*3.0`, flat 0-15 at
    full proficiency) rather than approximating it. `tests/smoke_test_
    cintai.py` (1 check, statistical) passes live -- needed a wider,
    more central dex-mismatch calibration than jirin/kubo/oomlat's own
    test uses; their clamped-edge (~6% baseline) design produced hit
    counts too low/noisy for cintai's smaller effect size (one run came
    back backwards, 15 vs 18, before recalibrating).
    **shove** (Warrior, 6) -- done 2026-07-27. Real upstream
    (disc/disc_dueling.cc's `doShove()`/`shove()`/`throwChar()`,
    fuller peel-sneezymud clone): refuses while either side is
    fighting, spends Move, DEX/level-scaled roll, and on success
    physically pushes the victim through a real exit into the adjacent
    room -- on FAILURE it starts a fight instead of just fizzling, a
    deliberate real-game design (ported as-is). Spends 8 Vitality
    (the middle of the real 5-10 roll). Deliberately NOT ported: the
    real version's entire mount-vs-mount dismounting branch (refused
    outright instead) and the counter-move skill interaction (Tobin's
    own "counter move" roster entry has no handler yet either). New
    `cmd_shove.c`. `tests/smoke_test_shove.py` (6 checks) passes live.
    **materialize** (Mage, 6) -- done 2026-07-27. Real upstream
    (disc/disc_alchemy.cc's `materialize()`/`castMaterialize()`): pay a
    flat 100 gold, search the object PROTOTYPE table for a name match
    cheap enough to conjure, then a skill roll decides whether it
    actually manifests -- gold is spent either way (a gamble, not a
    guaranteed purchase). Reused `obj_find_vnum_by_name()`/`obj_proto_
    load()` (already backing `load obj <name>`) for the prototype
    search. Simplified from the real version's 1-10 copies scaled by
    price and its equip-into-a-free-hand logic, down to a flat one
    copy into inventory (same "conjured items land in inventory"
    precedent `load obj` already established for immortals). New
    `cmd_materialize.c`. Two real test bugs caught while verifying, not
    C bugs: (1) the search string's word order didn't match the seeded
    item's name -- `obj_find_vnum_by_name()` is a literal substring
    match, not per-keyword; (2) an SQL gold UPDATE issued after the
    test character was already logged in never reached the live
    session (same "SQL-then-relog, never SQL-then-quit!" trap
    documented in smoke_test_skillcombat3.py). `tests/smoke_test_
    materialize.py` (9 checks) passes live.
    **bodyslam** (Warrior, 10) -- done 2026-07-27. Real upstream
    (cmd/cmd_bodyslam.cc's `canBodyslam()`/`bodyslam()`/`bodyslamHit()`/
    `bodyslamMiss()`, fuller peel-sneezymud clone): a heavy function --
    three miss types (DEX-avoid, STR-fails-to-lift, Monk countermove),
    a carry-weight comparison, proficiency-gated held-item rules, a
    `trySpringleap()` follow-up chain, mount dismounting. Scoped down
    to the same "Tobin-scale slice" pattern as bash/kick/disarm: one
    `skill_roll_success()` roll (no armor%/weight-comparison factor --
    Tobin doesn't model carry capacity robustly enough to gate on it),
    reused `combat_apply_skill_damage()`'s STR-flavored placeholder
    formula scaled x2. Success knocks the victim down (POSITION_SITTING)
    and deals damage; failure knocks the ATTACKER down instead (cheap
    stand-in for the real crashLanding()/three-way-miss branching, no
    springleap since Tobin doesn't have it). 15 Vitality. Deliberately
    NOT ported: mount dismounting (refused outright) and the
    proficiency-gated held-item restriction. New `cmd_bodyslam.c`.
    Same dead self-target-branch bug this audit keeps re-hitting
    (`combat_find_room_target()` already excludes self) caught and
    fixed before commit. `tests/smoke_test_bodyslam.py` (5 checks)
    passes live -- one real test bug caught: check 4 originally reused
    the already-100%-seeded attacker instead of a fresh 0%-proficiency
    one, so the failure path was never actually exercised.
    **curse** (Cleric, 13) + **slumber** (Mage, 13) -- done 2026-07-27,
    the first items needing the AFFECT_* enum extension flagged above.
    Real upstream curse (misc/magicutils.cc's genericCurse()) is a
    hitroll penalty plus a worsened paralysis-immunity penalty; Tobin
    has neither a separate hitroll stat nor a paralysis affect, so it
    lands as a level-scaled DEXTERITY penalty (new AFFECT_CURSE,
    affect.h) since combat_strike()'s to-hit roll is driven directly off
    DEXTERITY -- same stat-modifying-affect shape as AFFECT_STUPIDITY/
    AFFECT_RALLY. The "curses...an object" variant (an item that can't
    be removed) has no Tobin equivalent, dropped. Real upstream slumber
    (disc_mage_spirit.cc's slumber()/rawSleep()) puts the victim into
    POSITION_SLEEPING for a timed duration with its own separate
    luck-save resist roll, an optional Sleep Tag Staff branch, and a
    crit-fail-hits-the-caster-instead branch; scoped to the core effect
    (new AFFECT_SLEEP, special-cased in affect.c's tick_being_affects()
    to auto-wake the target on expiry instead of the generic "wears
    off" message, same dissolve/revert shape as AFFECT_CHARMED/
    AFFECT_POLYMORPH) -- the outer cast-proficiency roll already stands
    in for the real version's own success/luck-save pair, so no second
    resist roll. Neither the Sleep Tag Staff nor the crit-fail branch
    ported. `tests/smoke_test_curse_slumber.py` (8 checks) passes live
    -- one real test bug caught: `make_char()` was passed a raw CLASS_*
    value as the character-creation MENU choice (they don't share
    numbering), which silently broke Mage creation; fixed by using
    placeholder menu choices and setting the real class via a direct
    SQL UPDATE afterward, same pattern shove/bodyslam's own tests use.
    **fear** (Mage, 14) + **identify** (Mage, 14) -- done 2026-07-27.
    Real upstream fear (disc_mage_spirit.cc's fear()) forces an
    immediate flee, then a lingering affect keeps compelling the victim
    to keep running. New `AFFECT_FEAR` (plain flag/timer, no stat
    modifier) checked by `cmd_attack.c` (a feared being can't initiate
    an attack); the immediate flee reuses `cmd_flee.c`'s own logic
    directly on the victim's descriptor (PC victims only -- a mob has
    no descriptor to flee through). Not ported: the isLucky
    resist-and-fizzle branch (the outer cast-proficiency roll already
    stands in) and the crit-fail-fears-the-caster branch. Real upstream
    identify (disc_mage_alchemy.cc's identify()) TARGETS AN OBJECT, not
    a being -- every other spell in this roster resolves a being target
    via `combat_find_room_target()`, so identify is handled entirely
    separately in `cmd_cast.c`. Found live while building it: Tobin
    ALREADY has a real, correct, general-purpose `identify` command
    (`cmd_identify.c`, from an earlier "Object manipulation depth" audit
    pass) -- deliberately built as a plain, ungated command rather than
    a spell, since Tobin's val[] payload has nothing real for "accuracy
    scales with skill" to scale. A first pass here duplicated that
    display logic from scratch and got it factually wrong (used
    val[0]/val[1] as literal weapon damage dice -- cmd_identify.c's own
    header comment explains why that's not how real weapon damage
    works, verified against real seeded data). Fixed by delegating to
    the real, already-correct command instead of re-deriving it --
    `cast identify` just adds the spell-specific component gate on top
    of the same logic every player can already reach via the bare
    `identify` command. `tests/smoke_test_fear_identify.py` (6 checks)
    passes live -- fear's own immediate-flee physical relocation is NOT
    asserted (cmd_flee.c's escape roll is only ~2-in-3, and a failed
    roll leaves both sides `fighting` for the rest of the test with no
    clean reset), verified by code review instead, matching curse/
    slumber's own natural-expiry precedent for not asserting every
    real-world side effect.
    **headbutt** (Warrior, 15) -- done 2026-07-27. Real upstream
    (cmd/cmd_headbutt.cc's canHeadbutt()/headbutt()/headbuttHit()/
    headbuttMiss()) is relative-HEIGHT-driven: picks a different body
    region to strike (foot/leg/crotch/body/throat/jaw/skull) depending
    on how the attacker's height compares to the victim's, and refuses
    outright if the attacker is more than 25% shorter. Tobin has no
    height stat, so the whole region-selection mechanic has no faithful
    port -- scoped to the same "Tobin-scale slice" shape as chi/
    bodyslam: one `skill_roll_success()` roll striking LIMB_HEAD
    specifically, reusing the STR-flavored placeholder damage formula.
    No knockdown (the real version doesn't knock anyone down either).
    6 Vitality, matching the real version's own Move cost directly. New
    `cmd_headbutt.c`, registered as `headbutt` (needs the full 4-letter
    prefix -- "h"/"he"/"ho" are already claimed by hit/help/hold).
    `tests/smoke_test_headbutt.py` (4 checks) passes live.
    **telepathy** (Mage, 16) -- done 2026-07-27. Real upstream
    (disc_mage_spirit.cc's telepathy()) reaches every connected
    character in the WORLD with a free-text message. Intercepted in
    `cmd_cast.c` BEFORE the generic `find_spell_and_target()` parse --
    that helper only ever captures a single trailing word as a
    "target" (every other spell in this roster targets a being or, for
    identify, one named item), which would mangle a multi-word message
    into a bogus failed spell-name lookup. Deliberately does NOT skip a
    sleeping listener or honor the `noshout` toggle the way `shout`
    (cmd_shout.c) does -- telepathy is mind-to-mind, not sound, a
    disclosed difference from shout's own scope, not a missed check.
    Not ported: garble/drunk-speech distortion (no such mechanic
    exists) and the 5-Move cost. `tests/smoke_test_telepathy.py`
    (3 checks) passes live.
    **invisibility** + **dispel invisible** (Mage, 17) -- done
    2026-07-28. New `AFFECT_INVISIBLE` (affect.h): a plain flag/timer
    affect, same shape as AFFECT_SANCTUARY/AFFECT_BERSERK -- no armor
    bonus (real upstream's own -40 APPLY_ARMOR) and no crit-success/
    crit-fail branches (same "no crit branch ported" precedent as
    fear/slumber). Checked at `combat_find_room_target()` (untargetable
    by name) and `cmd_look.c`'s room listing (doesn't show), both gated
    on `!being_is_immortal(viewer)` so an immortal sees/targets right
    through it -- no `detect invisibility` counter-check exists yet
    (its own separate, higher roster entry). Object-target invisibility
    (roster's "yourself or an OBJECT") not ported -- Tobin's object
    `INVISIBLE` action-flag (obj.c) is display-only today, nothing in
    `cmd_look.c` hides a flagged object from the room listing yet.
    `tests/smoke_test_invisibility.py` (10 checks) passes live.
    **spin** (Warrior, 17) -- done 2026-07-28. Real upstream
    (cmd/cmd_spin.cc's `canSpin()`/`spin()`/`spinHit()`/`spinMiss()`) is
    another heavy function: a flying-victim difficulty roll (flavor only
    -- still lets the spin proceed either way), a graduated held-item
    restriction that eases with proficiency, a Monk counter-move defense
    plus a separate focused-avoidance defense roll, and on a hit either
    `knockOffMount()` or `crashLanding()` depending on mount state.
    Scoped down, same "Tobin-scale slice" shape as bodyslam/headbutt: one
    `skill_roll_success()` roll (no countermove/focused-avoidance defense
    rolls), reusing `combat_apply_skill_damage()`'s STR-flavored formula
    at bash's baseline (not bodyslam's x2). Two checks DID port cheaply
    since Tobin already has the underlying mechanic: refuses a flying
    target outright unless already fighting you (`being_has_affect(target,
    AFFECT_FLYING)`), and requires the primary hand empty (`ch->held[0]`),
    matching skill.c's own roster flavor "needs a free hand" more simply
    than the real graduated one/two-hand easing. Same knockdown-on-hit/
    knockdown-on-miss shape bodyslam uses as a stand-in for
    `crashLanding()`. 6 Vitality, matching the real 6-Move `SPIN_COST`
    directly. Not ported: mount dismounting (refused outright while
    either side is mounted, same scope cut bodyslam/shove already made)
    and the proficiency-graduated held-item easing. New `cmd_spin.c`.
    `tests/smoke_test_spin.py` (8 checks) passes live; the flying-target
    refusal is verified by code review only (no DB-persisted affect row
    to seed without a live Mage `levitate` cast, same "not every side
    effect needs a live assertion" precedent as fear/slumber).
    **teleport** (Mage, 19) -- done. Self or an offensive cast on a
    room-local target, both sent to a genuinely RANDOM room (real
    upstream, disc_mage_sorcery.cc's `teleport()`/`genericTeleport()` --
    skill.c's old roster text "random or chosen location" was an
    inaccurate guess, corrected to "random location"). Real destination
    exclusions (DEATH/PRIVATE/HAVE-TO-WALK rooms) and a caster-room
    NO-ESCAPE refusal ported via new `room_repo_random_teleport_vnum()`
    and four new `ROOM_FLAG_*` bits. Not ported: the `isLucky`
    resist-and-fizzle roll and the critical-failure caster-flung branch.
    **summon** (Cleric, 19) -- done. World-wide online-character search
    (reusing `transfer`'s lookup) pulls a named target into the caster's
    room; refuses an immortal target outright (no caster-immortal
    exception, matching real upstream exactly) and requires mutual
    `toggle pk` consent for a mortal target (`combat_pk_allowed()`). Not
    ported: the real `isNotPowerful()` discipline-tier power-gap gate
    (no clean Tobin equivalent) and the real arena/have-to-walk/
    fall-sector location refusals. New `cmd_pray.c` branch.
    `tests/smoke_test_teleport_summon.py` (10 checks) -- verified correct
    via isolated single-character manual testing; the test's own
    multi-character batch setup intermittently trips a separate engine
    bug (see "Small near-term gameplay follow-ups" section) not yet
    understood, so the automated test itself isn't clean end-to-end.
    **springleap** (Monk, 20) -- done, ported faithfully (real upstream,
    disc/disc_monk_leverage.cc, needed no scope cut at all): refuses
    unless resting or sitting, one roll, stands you up on success.
    **slam** (Warrior, 20) -- done. Real upstream (cmd/cmd_slam.cc) has
    no stun at all -- skill.c's old roster text ("extra damage and a
    stun") was another inaccurate guess, corrected. Real damage formula
    scales to a level-tiered percentage of the victim's max HP (not
    ported, no clean Tobin equivalent); scoped to the shared STR-flavored
    placeholder at x2.5, the heaviest scale used so far.
    **deathstroke** (Warrior, 20) -- done. Requires a wielded weapon
    (`combat_wielded_weapon()`, promoted from static to shared via
    combat.h), x3 damage (heaviest yet). Not ported: the real self-lockout
    timer, armor-penalty/hitroll-buff affects (no generic arbitrary-
    modifier system), and the victim's own counterattack-if-they-also-
    know-deathstroke chain.
    **riposte** (Warrior, 20, passive) -- done. No `cmd_riposte.c` at
    all, matching real upstream (`parry` has no player command either) --
    a successful parry has a 50%-then-skill-roll chance to set a new
    transient `being_t.riposte_ready` flag, consumed at the top of that
    being's own next `combat_strike()` call to force that swing to land
    (the simplest faithful analog of real upstream's literal bonus
    attack, "fx++" in `hit()`, inside Tobin's fixed one-strike-per-side
    shape).
    **dispel magic** (Mage, 20) -- done, but a DELIBERATE DEVIATION from
    the real mechanic, disclosed in cmd_cast.c's own comment: real
    upstream (disc_mage_alchemy.cc) is entirely OBJECT-targeted (strips
    an item's enchantment bonuses), which Tobin has nothing to port onto
    (objaffect rows are permanent DB data, not a runtime dispellable
    state). Implemented instead as a being-targeted "strip every active
    affect" (self or a named target) -- useful both offensively and as a
    one-shot cure-everything, unlike the single-purpose cure spells.
    `tests/smoke_test_level20_warrior_mage.py` (8 checks) passes live for
    all five items above (riposte verified by code review only -- no
    deterministic way to force a parry+riposte proc).
    **blindness** (Cleric, 21) -- done. New `AFFECT_BLIND` (affect.h):
    a plain flag/timer checked at two of the real upstream's many gate
    points (cmd_look.c blocks the room description AND `look <target>`
    entirely, matching real cmd_look.cc almost verbatim; combat_strike()
    adds a flat to-hit penalty when the ATTACKER is blinded, a disclosed
    approximation since the real to-hit effect isn't one single traced
    formula). Not ported: TRUE_SIGHT/CLARITY immunity (neither exists in
    Tobin) and the isNotPowerful() power-gap gate.
    **word of recall** (Cleric 21, also Druid 50 advanced -- same name,
    same branch handles both) -- done. Self or a named target relocates
    to `DEFAULT_LOAD_ROOM_MORTAL` (room 100, Center Square) -- a real
    match for real upstream's own "no hometown set" fallback (`Room::
    CS`), not a guess; Tobin has no per-player recall-point concept to
    port the normal case. Refuses to recall FROM an ARENA/NO-ESCAPE
    room, refuses an immortal target, breaks the target's current fight.
    Not ported: the real "murderer" (AFFECT_PLAYERKILL) refusal -- no
    such system in Tobin -- and the critical-failure random-fling branch.
    `tests/smoke_test_blindness_recall.py` (6 checks) -- the NO-ESCAPE
    room check is intermittently affected by the same not-yet-root-
    caused room-placement bug documented above (a test character can
    occasionally land in the wrong room on login); the underlying
    ROOM_FLAG_NO_ESCAPE gate itself reuses the identical pattern already
    verified working for `teleport`'s own version of this check.
    Re-ran with debug prints (2026-07-28) to rule out a `goto`/timing
    problem specifically: confirmed the immortal helper's `goto` into
    the NO-ESCAPE room succeeds every time (both the goto output and a
    follow-up `look` show it landing there correctly) -- the failure is
    squarely the *other* character (the actual caster) landing in
    Center Square instead of its own `load_room` on login, same bug,
    not a new one. No fix attempted here; out of scope for this batch.
    **taunt** (Warrior, 22) -- done 2026-07-28. Real upstream
    (disc_warrior_brawling.cc's doTaunt()) only works against a mob
    already fighting YOU, and its whole effect is a temporary debuff to
    that mob's "wimp switch" AI score (misc/ai_reactions.cc) -- making it
    less likely to swap off you mid-fight. Tobin has no multi-attacker
    mob-AI target-switching subsystem at all (`fighting` is a strict
    mutual one-to-one pointer pair, being.h), so that mechanic doesn't
    apply. Ported instead as a direct aggro PULL matching the roster's
    own plain description ("Provoke a target into focusing their
    aggression on you") -- same fighting-pointer-swap shape as
    cmd_rescue.c, but framed the opposite way: steals a MOB's attention
    off whoever/whatever it's currently fighting (ally or stranger) onto
    the taunter, with no requirement the taunter was already involved.
    Restricted to mob targets (aggro is a mob-AI concept; redirecting a
    PC's fight without consent would be an unrequested PvP mechanic).
    New `cmd_taunt.c`.
    **paralyze limb** (Cleric, 22) -- done 2026-07-28. Real upstream
    (disc_cleric_afflictions.cc's paralyzeLimb()) picks a random limb and
    sets a PERMANENT PART_PARALYZED flag on it (can't wield/wear on an
    arm, can't walk right on a leg) until a separate "restore limb" spell
    (Cleric, 25, not yet ported) cures it. Tobin's limb_state_t is just
    hp/max_hp, no per-limb status-flag system -- ported as a disclosed
    approximation: drives a randomly-picked SAFE limb's hp straight to 0,
    reusing the exact "destroyed" state combat already leaves a limb in
    (same to-hit penalty, same score/limbs display) via
    combat_debug_set_limb_hp() (combat.c) rather than duplicating it.
    Deliberately restricted to arms/hands/legs/feet, never the four MAJOR
    limbs (head/neck/waist/body, combat.c's is_major_limb()) whose
    destruction is instant death -- would otherwise make a "paralyze"
    spell an accidental save-or-die, which real upstream's own
    pickRandomLimb() also avoids. No natural regen exists for a destroyed
    limb (regen.c), so this is "permanent until cured" here too, even
    without restore limb existing yet to actually cure it.
    `tests/smoke_test_taunt_paralyzelimb.py` (12 checks) passes live --
    hit the SAME room-placement-on-login flakiness documented above
    during development (one of the four test characters landed in Center
    Square instead of its sandbox room on a couple of runs); worked
    around in the test itself with `goto`/`transfer` to force everyone
    into the sandbox deterministically rather than depending on
    `load_room`, since that's the same pre-existing bug, not a new one.
    Not yet started: whirlwind/kneestrike/farlook/scribe/bind (25), hide
    (31), paralyze (33), quivering palm (42), silence (48).
- **Buff spells that conflate distinct effects**: sanctuary/armor/bless/
  stone skin/barkskin/protection-from-* all currently reuse the identical
  `AFFECT_SANCTUARY` buff rather than each having its own effect --
  functional (not a stub), but a disclosed simplification worth a
  separate pass once the placeholder-spell affects above get their own
  enum entries.

Self-contained — no need for the object/mob systems. Keep working through
these; each ships with a smoke test + (if player-facing) a news entry.

## Standing rules (learned)

- No new full sweeps. Full sweep takes 5+ hours to complete. Targeted testing only.
- Every player-facing change gets a `news.sql` entry (no numbers). See CLAUDE.md.
- Every new `db/tobin/*.sql` file MUST use `CREATE TABLE IF NOT EXISTS`,
  never an unconditional `DROP TABLE IF EXISTS` + `CREATE TABLE` — the
  latter silently wipes live data every time `apply-tobin-schema.sh` re-runs
  it (which it always does; that script re-applies every file, every time).
  Burned us once for real (Session 36: `player_attrs.sql`/
  `player_progress.sql` wiped ~1338 players' progress this way).
- Always use SneezyMUD code as implementation guidance where available.
- Complete the current task fully before moving to the next.
- Continue progressing rapidly through the backlog without waiting for additional instructions unless blocked by missing requirements.
- Document all completed changes, database updates, VNUM allocations, and implementation notes.
- Help me conserve tokens. Go absolutly silent unless acknowledging an instruction, or giving a brief report on an item just finished. Otherwise, silence.
