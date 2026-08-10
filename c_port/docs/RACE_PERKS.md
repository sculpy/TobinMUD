# PC-race perk system

Companion to [RACE_STATS.md](RACE_STATS.md) (the attribute layer). Where
that covers the one-time creation stat bonus, this covers the ongoing
per-race **perks** — mana/move/upkeep, resistances, senses, and talents —
all derived+scaled from the SneezyMUD `RACE_*` files and all **live-tunable**
through the `balance` command (no restart, no recompile).

## Design decisions

- **Net-zero across the whole race**, not per subsystem. A race pairs upside
  (more HP, a resistance, infravision) with drawbacks elsewhere (eats more,
  low mana/move), the way Sneezy balances its own races.
- **Human is the baseline** — every mult 1.0, no resistances — but keeps one
  signature *Adaptable* talent so it isn't strictly dominated.
- **Magnitudes scaled down** from Sneezy's raw numbers, and tunable live, so
  nothing here is locked in.

## Storage & tuning

All perks live on the `race_balance` table (and the parallel `class_balance`,
where the race-only columns stay neutral) — cached in memory at startup,
edited via `balance race <name>` / `balance class <name>`. The editor is
fully menu-driven with a labelled, unit-annotated screen so it is always
clear what a number means and what is neutral. See `db/tobin/balance.sql`,
`src/db/balance_repo.c`, `src/core/balance.c`, and `descriptor.c`'s
`CONN_BALANCE_*` state machine.

## The perks (seeded values)

`x` = multiplier (1.00 neutral). Resistances = % chance to shrug off.

| Race | HP | Mana | Move | Food | Drink | Resists | Infra | Talent |
|---|---|---|---|---|---|---|---|---|
| Human | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | — | no | Adaptable |
| Elf | 1.00 | 1.05 | 1.13 | 1.50 | 0.50 | charm 33, sleep 33, paralysis 20 | no | — |
| Ogre | 1.10 | 1.00 | 1.20 | 1.25 | 0.75 | poison 17, heat 13, cold 13 | no | Brawler |
| Dwarf | 1.03 | 0.95 | 0.90 | 0.75 | 1.25 | poison 20, charm 38, sleep 38, energy 5 | yes | — |
| Hobbit | 1.00 | 1.00 | 1.20 | 0.50 | 1.30 | poison 20 | yes | Woodland Stealth |
| Gnome | 1.00 | 1.10 | 0.83 | 0.80 | 1.20 | energy 15 | yes | Detect Magic |

Food/Drink are **decay-rate** multipliers: <1 = the race gets hungry/thirsty
more slowly. Fractional rates decay probabilistically per tick (`upkeep_decay`,
vitals.c) so e.g. 0.75 drains one point about three ticks in four.

## Application status

- **Tier 0 (applied):** HP (`being_calc_max_hp`), mana (`being_calc_max_mana`),
  move (`being_calc_max_vit`), food/drink decay (`vitals.c`).
- **Tier 2 (applied):** infravision — exempts the race from `room_is_dark_for()`,
  i.e. innate dark-vision with no light or spell.
- **Tier 1 (Phase 2):** resistances applied at the effect-roll sites that exist
  (charm/sleep via the affect save, poison via drink/sip). Elemental types
  (energy/heat/cold/paralysis) are stored + tunable now and take effect where
  such damage sources are added — disclosed-pending.
- **Tier 3 (Phase 3):** talents — Gnome innate detect-magic, Hobbit
  sneak/hide/search bonus, Ogre brawling-skill bonus, Human across-the-board
  learn-by-doing bonus.

## Not imported

Height/weight/age, move verbs, body type, and per-race quest-item tables from
the Sneezy files are flavour/systems Tobin doesn't model per PC race. Talents
beyond the four above are cosmetic labels until a matching system exists.
