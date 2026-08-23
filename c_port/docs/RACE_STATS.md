# Race stat bonuses — the SneezyMUD conversion

Tobin's six playable races (Human, Elf, Ogre, Dwarf, Hobbit, Gnome) each
apply a small, **net-zero** attribute bonus once at character creation, on
top of the class bonus and the (Tobin-original) territory/homeland bonus.

Those race numbers are **derived from SneezyMUD's own per-race stat table**
(`sneezymud-master/lib/races/RACE_*`), not invented. This document is the
conversion of record — regenerate the numbers from it if the scale is ever
retuned.

> Correction: an earlier note in `include/being.h` claimed "SneezyMUD's
> race table wasn't found carrying attribute bonuses." That was wrong. The
> `RACE_*` files carry a full 12-stat weighting per race (plus hp/mana/move
> mods, immunities, infravision, and talents — see "Not imported" below).

## 1. Fold the 12 Sneezy stats into Tobin's 6

Sneezy rates each race on 12 stats as a percentage where **100 = neutral**
(a plain Human is a flat 105 across the board). Tobin has 6 stats. The fold
(per user, 2026-08-10):

| Tobin stat | Sneezy source(s) | rule |
|---|---|---|
| STR | STR | direct |
| CON | CON, **BRA** (brawn) | average |
| DEX | DEX, **AGI** (agility), **SPE** (speed) | average |
| INT | INT, **FOC** (focus) | average (focus split) |
| WIS | WIS, **FOC** (focus) | average (focus split) |
| CHA | CHA, **PER** (perception), **KAR** (karma) | average |

Focus feeds **both** INT and WIS. Averaging (not summing) keeps each folded
Tobin stat on the same 100=neutral scale.

## 2. Centre each race to net zero

Subtract the race's own mean-across-the-6 from each stat, so every race is a
pure set of relative strengths and weaknesses that sums to zero. Human
(all 105) trivially centres to all-zero — the deliberate baseline.

## 3. Scale: 1 Tobin point per 10% of deviation

Divide the centred deviation by 10 and round to the nearest integer. Where
rounding leaves a race off by ±1, one stat is nudged by 1 (the one with the
largest rounding residual) to restore an exact net zero. This lands every
race in a modest ±6 band — a clearer identity than the ±4 class bonus, still
well inside the point-buy range (base 120, ±30/stat).

## Worked result

Folded Tobin-stat percentages (before centring):

| Race | STR | DEX | CON | INT | WIS | CHA |
|---|---|---|---|---|---|---|
| Human | 105 | 105 | 105 | 105 | 105 | 105 |
| Elf | 80 | 125 | 45 | 137.5 | 150 | 86.7 |
| Ogre | 165 | 80 | 140 | 80 | 80 | 96.7 |
| Dwarf | 130 | 75 | 142.5 | 92.5 | 100 | 96.7 |
| Hobbit | 55 | 155 | 67.5 | 81.5 | 81.5 | 115 |
| Gnome | 77 | 73.3 | 82.5 | 150 | 137.5 | 111.7 |

Final applied bonuses (centred, ÷10, net-zero) — this is what
`race_stat_bonus()` implements:

| Race | STR | DEX | CON | INT | WIS | CHA | signature |
|---|---|---|---|---|---|---|---|
| Human | 0 | 0 | 0 | 0 | 0 | 0 | versatile baseline |
| Elf | −2 | +2 | −6 | +3 | +5 | −2 | wise & keen, frail |
| Ogre | +6 | −3 | +3 | −2 | −3 | −1 | brute strength |
| Dwarf | +2 | −3 | +4 | −1 | −1 | −1 | tough & strong |
| Hobbit | −4 | +6 | −2 | −1 | −1 | +2 | nimble & likeable |
| Gnome | −3 | −3 | −2 | +4 | +3 | +1 | clever tinkerer |

Every row sums to 0. Verified live by `tests/smoke_test_race_stats.py`,
which creates one character per race and checks the stored attributes equal
`120 + class + race + territory` for each stat.

## Not imported (disclosed scope)

The `RACE_*` files carry more than attributes; these are **not** folded into
the bonus above, for want of a Tobin system to hang them on:

- **hpMod / manaMod / moveMod** — could later seed the tunable `race_balance`
  table (`balance` command): Ogre `hpMod 4` and Dwarf `hpMod 1` are the only
  non-zero HP mods among the six. Left neutral for now.
- **Immunities** (poison/charm/sleep/energy/heat/cold/paralysis), **AFF_INFRAVISION**,
  and **talents** (e.g. Hobbit sneak/hide, Gnome detect-magic) — no PC-race
  perk system exists yet.
- ~~Height/weight dice, age, move verbs, body type~~ — **implemented**
  (2026-08-23, `src/core/race_flavor.c`): height/weight/starting age are
  rolled once at creation off each race's own RACE_* dice and shown on
  `score`; move-in/move-out verbs (`race_move_verb_in()`/
  `race_move_verb_out()`) replace the flat "has arrived"/"exits" default
  in `cmd_move.c` when no poofin/poofout is set; body type is now a real
  per-race lookup (`race_body_type()`) rather than a hardcode, though
  every one of Tobin's 6 playable races' own RACE_* file says `body
  Humanoid`, so all six resolve the same either way. See RACE_FLAVOR.md.
  (Corrects this list's earlier claim that food/drink mods aren't
  modeled either — they are, and already were: race_balance's food/drink
  decay-rate multipliers, RACE_PERKS.md above, Tier 0 applied.)

Territory/homeland is a **Tobin-original add-on**, unrelated to this Sneezy
conversion — it stacks as its own net-zero layer (see `territory_stat_bonus()`).
