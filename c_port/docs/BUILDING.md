# Building content in TobinMUD

A working guide for immortal builders: how to make rooms, objects, mobs,
scripted triggers, and how to make it all survive a reboot via zonefiles.
This documents what actually exists today, not an aspirational design —
see the caveats in each section for what's still missing.

All commands below require at least `BUILD_MIN_LEVEL` (51). A builder can
only touch a zone they're assigned to (`zone_can_edit()`); level 55+ can
touch any zone. `edit zone <n>` (below) is how an implementor assigns a
builder to a zone.

## Contents

- [Rooms](#rooms)
- [Objects](#objects)
- [Mobs](#mobs)
- [Triggers (scripted behavior)](#triggers-scripted-behavior)
- [Zonefiles (making placement permanent)](#zonefiles-making-placement-permanent)
- [Zones themselves](#zones-themselves)
- [Putting it together: a worked example](#putting-it-together-a-worked-example)

## Rooms

Rooms are the one piece of content with a full in-game creation *and*
editing story — no SQL required.

**`dig <direction>`** — builder-walk creation. If there's no exit that way
yet, this creates a brand-new room, wires this room's exit to it and the
new room's reverse exit back, then walks you into it (so it plays exactly
like a normal move — arrival triggers, poofin/poofout, everything). The
new room's vnum comes from the next free number in **your current room's
own zone range** — you can't dig into someone else's zone, or an unzoned
room (it has no range to allocate from).

**`edit room [vnum]`** (bare form edits your current room) — menu-driven
editor, same shape as character creation:

```
Menu:
   1) Name              2) Description
   3) Flags             4) Sector Type
   5) Exits             6) Max Capacity
   7) Room Height

   C) Clear room out    S) Save    Q) Quit
```

- **1) Name** / **2) Description** — free text (description supports the
  shared line editor conventions — reflow, etc.).
- **3) Flags** / **4) Sector Type** — pick from a numbered list; sector
  type also drives the room's ambient color in `look`.
- **5) Exits** — add/remove a direction, optionally attach a door (with a
  door type) and set its condition bits (closed/locked/secret/trapped).
  Door state is **per-exit**, not mirrored to the reverse side — matches
  how `dig`'s own auto-created reverse exit already behaves.
- **6) Max Capacity** / **7) Room Height** — plain numbers.
- **S) Save** commits everything to the `room` table immediately (no
  separate "publish" step); **Q** without saving discards your edits.

## Objects

**There is no in-game object creator yet** (no `oedit`/`edobject` — see
TODO.md). A brand-new object *prototype* has to be inserted directly into
the `obj` table via SQL. Once a prototype exists, everything else about
placing and using it is in-game.

### Creating a prototype (SQL)

```sql
INSERT INTO obj (vnum, name, short_desc, long_desc, action_desc,
                 type, wear_flag, val0, val1, val2, val3,
                 weight, price, can_be_seen)
VALUES (123456, 'sharp dagger dagger', 'a sharp dagger',
        'A sharp dagger is lying here.', '',
        5, 17, 2, 4, 0, 0,
        1.0, 50, 1);
```

Column notes (see `include/obj.h` for the full picture):

- **`name`** — space-separated keywords used for matching (`get dagger`,
  `wear dagger` all match here); **not** what players see.
- **`short_desc`** — the player-facing label, lowercase-first by
  convention ("a sharp dagger") — commands capitalize it themselves at a
  sentence start.
- **`long_desc`** — the full ground-listing sentence shown by `look`
  ("A sharp dagger is lying here."). If left empty, `short_desc` is used
  instead (capitalized).
- **`type`** — the upstream item-type number; this is what actually
  determines the object's *category* (weapon/armor/container/...) via
  `category_for_item_type()` (`src/core/obj.c`). The common ones:

  | type | category | type | category |
  |-----:|----------|-----:|----------|
  | 1 | LIGHT | 17 | DRINK |
  | 5 | WEAPON | 18 | KEY |
  | 9 or 11 | ARMOR | 19 | FOOD |
  | 15 or 27 | CONTAINER | 20 | MONEY |

  (Full table in `ITEM_TYPE_CATEGORY[]`, `src/core/obj.c`.)
- **`wear_flag`** — a bitmask, stored verbatim in the upstream layout:
  `TAKE=1, FINGERS=2, NECK=4, BODY=8, HEAD=16, LEGS=32, FEET=64,
  HANDS=128, ARMS=256, BACK=1024, WAIST=2048, WRISTS=4096, HOLD=16384,
  THROW=32768`. A weapon you want `wield`-able needs `TAKE|HOLD` = 16385.
  An un-pickupable fixture (a fountain, a statue) gets `0`.
- **`val0`–`val3`** — meaning depends on `type`'s category; see the big
  comment block on `obj_t` in `include/obj.h` for every category's
  layout (weapon = dice count/sides, container = weight cap/flags/key
  vnum, drink/food = max/current units, key = the vnum it unlocks, ...).
- **`can_be_seen`** — `1` for a normal object; `0` for something meant to
  be invisible/undetectable by ordinary means (no such mechanic reads
  this yet, it's forward-looking).
- **`max_struct`** / **`cur_struct`** — condition/durability. Leave both
  `0` to skip the condition line entirely on `look`; set a real max and
  `look <item>` reports "is in excellent condition" down to "is
  destroyed" based on the current/max ratio.
- **`max_exist`** — world-wide instance cap, same warning-not-enforcement
  behavior as a mob's (see below). `0` = uncapped.

### Placing it in the world

**`load obj <vnum|name>`** — spawns an instance of an *existing*
prototype into your current room. Accepts a vnum, or a case-insensitive
substring match against `obj.name` (lowest-vnum match wins).

This placement is **not persistent by itself** — a room-floor object
placed this way vanishes on the next server restart. See
[Zonefiles](#zonefiles-making-placement-permanent) below for how to make
it stick.

## Mobs

Same story as objects: **no in-game mob creator** (no `medit`/`edmobile`
yet). A new mob prototype is a direct SQL insert into the `mob` table.

### Creating a prototype (SQL)

The upstream `mob` table has ~40 columns, but Tobin's mob loader
(`src/db/mob_repo.c`) only actually **reads** a handful of them — the
rest (race/tohit/ac/damage_level/damage_precision/gold/weight/height,
every one of the 12 attribute stats, letter/pos/def_position/skin/
vision/can_be_seen) are upstream-schema filler that must be present
(the columns are `NOT NULL`) but have **no live effect** — deferred to a
future `edmobile`/AI session (see `include/mob_repo.h`'s header comment).
Give them `0` and move on; don't spend design effort tuning them yet.

```sql
INSERT INTO mob (vnum, name, short_desc, long_desc, description,
                 actions, affects, faction, fact_perc, letter, attacks,
                 class, level, tohit, ac, hpbonus, damage_level,
                 damage_precision, gold, race, weight, height,
                 str, bra, con, dex, agi, intel, wis, foc, per, cha,
                 kar, spe, pos, def_position, sex, spec_proc, skin,
                 vision, can_be_seen, max_exist)
VALUES (149, 'orc skinny', 'a skinny orc',
        'A skinny orc stands here, looking hungry.', 'A rough-looking orc.',
        0, 0, 0, 0, 'A', 1.0,
        0, 5, 0, 0, 100, 0.3,
        0, 20, 0, 0, 0,
        0,0,0,0,0,0,0,0,0,0,
        0,0, 0, 0, 1, 0, 0,
        0, 1, 0);
```

The columns that actually matter — everything else above is inert:

- **`name`** — keyword list, same convention as objects (used to match
  `kill orc`, `look orc`, etc.).
- **`short_desc`** — "a skinny orc"; shown in room listings and combat
  messages ("A skinny orc hits you..."). (`long_desc` is unused for
  mobs — that's an object-only convention.)
- **`description`** — the `look <mob>` closer text (a mob's equivalent
  of a player's `appearance`).
- **`level`** — governs, among other things, whether player-facing skill
  gates treat it as mortal or immortal-tier.
- **`hpbonus`** — added on top of the base HP formula; the practical dial
  for "how much can this thing take" (5000+ for an effectively-unkillable
  training dummy, 0-ish for a normal early mob).
- **`sex`** — drives pronoun selection, same as a player's gender.
- **`actions`** — the upstream `ACT_*` bitmask (mob AI behavior —
  wander/scavenge/aggro, see `mob_ai.c`); `0` = passive, does nothing on
  its own between fights. `ACT_SENTINEL=2, ACT_SCAVENGER=4,
  ACT_STAY_ZONE=64` are the ones actually read.
- **`align`** — a **Tobin-added** column, not upstream: `-1` evil, `0`
  unaligned, `1` good. Drives `mob_ai.c`'s random aggression between
  good/evil mobs and players.
- **`class`** — a **bitmask**, not a single class number: `1` mage, `2`
  cleric, `4` warrior, `8` thief, `16` shaman, `32` deikhan, `64` monk,
  `128` ranger, `256` other. `being_create_mob()` maps a recognizable
  single-class bit to a real Tobin class, which is what makes a mob
  usable as a class-specific guildmaster (see the seeded vnum 200-229
  guildmaster mobs for real examples) and lets the gamewide `balance`
  command's class modifiers apply to it in combat.
- **`max_exist`** — world-wide instance cap. `load`ing past it doesn't
  block you, just warns ("please clean up when you're done"); a zone
  reset's own per-room check *does* enforce it. `0` = uncapped.
- **`spec_proc`** — read by name for one specific purpose today: marking
  a mob as a hospital healer (`shop_repo_is_hospital()`).

### Placing it in the world

**`load mob <vnum|name>`** — same shape as `load obj`: spawns a live
instance into your current room, world-wide-instance-count warning
included, not persistent on its own.

## Triggers (scripted behavior)

The in-game-authorable alternative to hand-coded spec procs — attach a
small script to a room, mob, or object *prototype* and it runs on a
specific event. No recompile needed to add new behavior.

**`edit trigger <room|mob|obj> <vnum> <trigger_type> [match_text|chance]`**

Trigger types by target:

| target | types |
|---|---|
| room | `enter` (someone walks in), `random` (ambient, rolled every ~60s tick) |
| mob | `greet` (someone walks into its room), `speech` (someone says a matching keyword nearby — pass the keyword as `match_text`), `death` (it dies), `random` |
| obj | `get` (picked up), `wear` (worn) |

For a `random` trigger, the trailing argument is the percent chance
rolled per tick (default 25 if omitted).

After the header line, you land in the shared line editor to write the
script — **one action per line**, `/s` saves:

| verb | effect |
|---|---|
| `echo <text>` | sent to the triggering player only |
| `echoroom <text>` | sent to everyone else in the room (triggering player excluded) |
| `emote <text>` | `"<Name> <text>"` to the whole room |
| `say <text>` | `"<Name> says, '<text>'"` to the whole room |
| `teleport <vnum>` | moves the triggering player to that room |
| `give <vnum>` | spawns that object into their inventory |
| `damage <n>` | deals n damage (clamped so it's never fatal on its own) |
| `log <text>` | a silent log entry, never broadcast — audit/debug only |
| `wait <seconds>` | pauses everything **after** this line for that many real seconds (1–3600), then resumes it |

`wait` is what turns a script into a paced little performance instead of
a single burst — e.g. a market vendor calling out one item at a time.
**Whoever triggered the script is not remembered across a `wait`** (they
may be long gone by the time it resumes) — only `say`/`emote`/
`echoroom`/`log` make sense on a line after a `wait`; `echo`/`teleport`/
`give`/`damage` there silently do nothing (no actor to target).

Unrecognized verbs are silently skipped — typo-tolerant by design, this
is a builder tool, not a compiler.

**`edit trigger list <room|mob|obj> <vnum>`** — shows every trigger
attached to that target, with its id.
**`edit trigger delete <id>`** — removes one.

### Testing a trigger without waiting on real time

`random` triggers roll on a real ~60-second world tick, and a `wait`
resolves on a real ~1-second tick — both far too slow to sit and watch
while you're actively building. **`aitick [count]`** (immortal-only debug
tool) forces `count` (default 1, max 100) world ticks to run right now,
synchronously — this also resolves any currently-pending `wait`, so
walking `aitick 1` repeatedly steps a paused script forward one resume at
a time, deterministically.

## Zonefiles (making placement permanent)

Everything placed with `load mob`/`load obj` disappears on the next
restart unless you snapshot it into the zone's own reset script.

**`zonefile create <zone number>`** — scans every room in the zone for
mobs/objects currently sitting there (from `load`, or from you just
walking the zone and dropping things), and adds new rows to that zone's
`zone_reset` script for anything not already represented. Also captures
what a loaded mob has equipped, held, or is carrying (one level into a
carried container's contents).

This is **idempotent and additive, not a full re-snapshot**: if you
delete a row from the zone's reset script by hand and rerun
`zonefile create`, it fills in exactly that gap rather than duplicating
everything else. It never removes a row on its own.

The reset script itself understands six opcodes (everything else from
the original upstream zone format — door state via a `D` opcode aside —
is inert data that Tobin's simplified reset engine ignores):

| opcode | effect |
|---|---|
| `M` | load mob into a room, unless the room already has enough of that vnum |
| `O` | load object onto a room floor (boot-time only — a periodic reset never re-drops ground clutter) |
| `E` | equip/hold an object onto the mob the last `M` loaded |
| `G` | give an object into the last mob's carried inventory |
| `P` | place an object inside the container the last `O`/`G` created |
| `D` | set a door's state on an exit |

You won't normally write these by hand — `zonefile create` generates them
from what you've actually built and placed live.

## Zones themselves

A zone is a vnum range plus reset behavior. **`edit zone <zone number>`**
opens a menu-driven editor: name, enabled flag, reset lifespan, vnum
range, assign/unassign builders to it, force a reset right now. A
builder below level 55 can only `edit zone`/`dig`/`edit trigger` within a
zone they're assigned to here.

## Putting it together: a worked example

Digging a room, populating it with a mob and an object, scripting the
mob, and making it all stick:

```
dig north                          # creates + walks into a new room
edit room                          # 1) name it, 2) describe it, S) save

# (separately, via SQL) insert the prototypes:
#   INSERT INTO mob (...) VALUES (vnum, 'vendor', ...);
#   INSERT INTO obj (...) VALUES (vnum, 'apple', ...);

load mob <vendor vnum>             # spawns the vendor here, live
load obj <apple vnum>              # spawns an apple here too

edit trigger mob <vendor vnum> random 25
  say Fresh apples! Get your fresh apples!
  wait 3
  say Only one gold a bag!
  wait 20
/s

aitick 3                           # sanity-check it fires + resumes

zonefile create <this zone's number>   # makes the vendor + apple permanent
```

After this, the vendor and the apple both respawn on every zone reset —
no further action needed. The trigger was already permanent from the
moment you saved it (`trigger` rows aren't tied to live instances at
all, only to the mob's *vnum*).
