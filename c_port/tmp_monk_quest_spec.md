# Monk Sash Quest Chain — SneezyMUD Reference Spec

Reverse-engineered from `sneezymud-master` (untouched upstream C++ reference)
for porting into `c_port` (Tobin). Two design docs exist under
`sneezymud-master/web/quests/` (`Monk Green Quest.doc`, `Monk Purple Quest.doc`,
both by Peel, 1998) — those are summarized here too so this file is the single
source of truth for the whole chain. Everything else was reverse-engineered
from `misc/toggle.h`, `misc/gaining.cc`, `misc/discipline.cc`, `misc/damage.cc`,
`misc/movement.cc`, `misc/physics.cc`, `misc/riding.cc`, `obj/obj_base_corpse.cc`,
`cmd/cmd_dissect.cc`, `spec/spec_mobs.cc`, and — the key discovery — the
**`mobresponses` DB table** (`db/sneezy/mobresponses.sql`), a per-mob-vnum
scripted dialogue DSL (`say`/`give`/`package` trigger blocks with
`checktoggle`/`checkuntoggle`/`toggle`/`untoggle`/`resize`/`load`/`link`
actions) that is where ALL of the guildmaster/NPC quest dialogue actually
lives — none of it is hardcoded in the C++ spec-proc functions.

## Guildmaster dialogue framework

`GenericGuildMaster` (misc/gaining.cc:2298, registered in spec/spec_mobs.cc
for "monk guildmaster" and every other class's guildmaster) ONLY handles
`CMD_GAIN` — the practice-training / `gain reset` flow. It does **not**
process quest dialogue at all.

Quest dialogue ("say white belt", "give bandage to Huang'lo", etc.) is driven
by a separate, generic script interpreter that reads `mobresponses.vnum ->
response` (keyed by mob vnum) and matches the player's command against
`say { "keyword"; ... }` / `give { "objvnum"; ... }` / `package { "id"; ... }`
blocks. Each block is a flat sequence of guard clauses (`checktoggle N` /
`checkuntoggle N` — abort the block if the quest-bit is/isn't set) followed by
actions:
- `say`/`tell %n`/`tovict`/`tonotvict`/`toroom` — dialogue/room messages (`%n`
  = player name substitution)
- emotes: `smile`, `nod`, `frown`, `cry`, `sigh`, `beam`, `cheer`, `wave`,
  `blush`, `wink`, `pat`, `thank %n`, etc.
- `toggle N` / `untoggle N` — set/clear a quest-bit on the player
- `flag %n` / `unflag %n` — solo-quest flag (blocks grouping during the
  quest, matches the "Flags solo" language in the design docs)
- `resize N` — load and resize/give quest item vnum N to the player (used
  for sash/belt awards)
- `load N` — load an object into the room/mob's inventory
- `give item %n` — hand an already-loaded object to the player
- `link say <keyword>` / `link package <id>` — chain to another block's
  dialogue as a "did you know" pointer (used so e.g. saying "purple sash"
  mid-quest re-shows the "package" progress-reminder block, or "elephant"/
  "shark"/"leper" to Huang'lo redirects to the matching quest's advice block)

Blocks are matched top-to-bottom, first block whose guards all pass wins.
Multiple `say { "same keyword"; checktoggle X; ... }` blocks for the same
keyword implement a state machine over the toggle chain (one block per
quest stage). **This is the framework the c_port needs to replicate** (or a
purpose-built equivalent) — a generic quest-bit system on the player plus a
small per-mob scripted-dialogue table, since 7 quest stages all route
through it.

Full raw source dumps used for this doc:
- mob 207 (Nimble Monk, L15 guildmaster), 223 (Hermit, L40 guildmaster),
  385 (Huang'lo, ex-trainer/bodyguard), 12509 (Castaway, L50 guildmaster) —
  `db/sneezy/mobresponses.sql`

---

## Quest chain overview (misc/gaining.cc eligibility gating)

| # | Sash | Min level | Guildmaster mob | Eligible toggle | Owned/Has toggle |
|---|------|-----------|------------------|------------------|-------------------|
| 1 | White belt | 2 | 207 (Nimble Monk) | `TOG_ELIGIBLE_MONK_WHITE`=132 | `TOG_HAS_MONK_WHITE`=134 |
| 2 | Yellow sash | 5 | 207 | `TOG_ELIGIBLE_MONK_YELLOW`=135 | `TOG_HAS_MONK_YELLOW`=137 |
| 3 | Purple sash | 15 | 207 | `TOG_MONK_PURPLE_ELIGIBLE`=138 | `TOG_MONK_PURPLE_OWNED`=145 |
| 4 | Blue sash | 25 | 223 (Hermit) | `TOG_ELIGIBLE_MONK_BLUE`=146 | `TOG_HAS_MONK_BLUE`=150 |
| 5 | Green sash | 35 | 223 | `TOG_MONK_GREEN_ELIGIBLE`=151 | `TOG_MONK_GREEN_OWNED`=156 |
| 6 | Red sash | 45 | 12509 (Castaway) | `TOG_MONK_RED_ELIGIBLE`=81 | `TOG_HAS_MONK_RED`=84 |
| 7 | Black sash | 50 | 12509 | `TOG_MONK_BLACK_ELIGIBLE`=215 | `TOG_MONK_BLACK_OWNED`=218 |

Note toggle numbers are NOT contiguous/ordered across quests (red sash
reuses low numbers 81-84, black sash uses 215-218, white/yellow are 132-137)
— just carry these as named constants, don't rely on ordering.

Eligibility for each stage requires: previous stage `_HAS_`/`_OWNED` bit set,
current stage's own toggles all clear, and level threshold reached. On level
gain, the guildmaster (`gm->doSay(...)`) announces eligibility and sets the
`_ELIGIBLE` bit — this partly duplicates what the mobresponses script does on
first "say <sash> quest" but the eligibility bit itself is set here, not by
the script.

---

## 1. White Belt (L2)

**Design:** trivially simple starter quest — bring a bandage to Huang'lo, he
sews/imbues it into a belt.

Toggles: `TOG_ELIGIBLE_MONK_WHITE`=132, `TOG_STARTED_MONK_WHITE`=133,
`TOG_HAS_MONK_WHITE`=134.

Flow:
1. Player levels to 2 → guildmaster (207) sets 132, announces eligibility.
2. Player says **"white belt"** to 207 (132 set) → 207 tells them to seek
   Huang'lo (385) in "the tower of the palace in the northeast section of
   the city"; unsets 132, sets 133.
3. Player says **"white belt"** to Huang'lo (385, 133 set) → Huang'lo asks
   for a **bandage** (obj vnum 9, "a bandage", sold by "Taloc").
4. Player `give bandage` to Huang'lo (385, obj vnum 9, guard 133) → Huang'lo
   sews it into a **white cloth belt** (obj vnum 6790, `[quest_object]`,
   resized/given to player), unsets 133, sets 134 (`TOG_HAS_MONK_WHITE`).
5. Repeat visits to either NPC after 134 is set just acknowledge completion.

No mob-kill or item-hunt beyond buying one bandage from a vendor. No skill
award (skill awards start at the green-sash stage with catfall).

---

## 2. Yellow Sash (L5)

**Design:** fetch-quest — bring tobacco ash to guildmaster 207.

Toggles: `TOG_ELIGIBLE_MONK_YELLOW`=135, `TOG_FINISHED_MONK_YELLOW`=136 (used
as "started" here despite the name — set once the player has heard the
quest text but before turning in the item; actual completion goes straight
to 137), `TOG_HAS_MONK_YELLOW`=137.

Flow:
1. Requires `TOG_HAS_MONK_WHITE` (134) already set, level 5 → 207 sets 135.
2. Player says **"yellow sash"** to 207 (135+134 set) → 207 wants **tobacco
   ash**, potted around a small tree he's growing; suggests asking Huang'lo.
   (No explicit `toggle`/`untoggle` action fires here in the script — the
   guard stays open, i.e. re-saying "yellow sash" just repeats the same
   text; toggle 136 in gating table above is referenced by the "already
   begun" guard but nothing in 207's script sets it — likely dead/vestigial,
   confirm before porting whether anything else sets 136.)
3. Player says **"ash"** to Huang'lo (385, guard 135) → hint: "check out
   some of the bars and restaurants of The World" for ash from smokers.
4. Player `give ashtray` (obj vnum 3319, "a metal ashtray") to 207 (guard
   135) → unsets 135, `resize 6791` (yellow sash), sets 137
   (`TOG_HAS_MONK_YELLOW`).

Item required: metal ashtray, vnum 3319 (a room prop somewhere in a bar,
not looted from a kill — player picks it up). Award: yellow sash obj vnum
6791.

---

## 3. Purple Sash (L15) — matches `Monk Purple Quest.doc`

Toggles 138–145 (see table in that doc / STATUS quoted below). Guildmaster
207, extra flavor line from Huang'lo (385) on keyword "leper" (guard 139,
just a cheer/encouragement, no mechanic).

Flow: eligible at L15 with `TOG_HAS_MONK_YELLOW` (137) → say "purple sash"
(137+138) gets the leper-colony rant → say **"I am ready to slaughter
lepers"** (137+138) unsets 138, sets 139 (`_STARTED`), flags player solo →
kill **5 lepers** (mob vnum 6602, found in "the Dungeon under Grimhaven";
kill increments 140→141→142→143→144 one at a time, tracked in
`misc/damage.cc:1368-1382` on the killing blow) → say "leper" to 207 (guard
144) unsets 144, sets 145 (`_OWNED`), unflags solo, `resize 6792` (purple
sash), guildmaster gives a farewell speech (207 stops training beyond this
point — matches "next guildmaster" hint text pointing to 223).

Re-saying "purple sash" while `139..144` set links to a generic "package
111" progress reminder (`"Come back to me when you have done so"` idiom via
`link package 111`).

Known design quirks (from the .doc, still true from the mobresponses
script): killing blow doesn't need to be solo/1v1, just needs to land the
kill (damage.cc checks quest bit on the killer at time of death, not combat
mode); losing/giving away the yellow sash mid-quest would presumably break
eligibility re-checks since gating re-derives from `TOG_HAS_MONK_YELLOW`.

---

## 4. Blue Sash (L25)

**Design:** revenge quest — kill a tiger shark, retrieve a dog collar from
its corpse (via `dissect`), bring it to guildmaster 223 (the Hermit).

Toggles: `TOG_ELIGIBLE_MONK_BLUE`=146, `TOG_STARTED_MONK_BLUE`=147,
`TOG_MONK_KILLED_SHARK`=148, `TOG_FINISHED_MONK_BLUE`=149 (declared but
unused in the traced code — the flow goes 147→give-item resolves directly
to 150; likely vestigial, same pattern as yellow's 136), `TOG_HAS_MONK_BLUE`
=150.

Flow:
1. Requires `TOG_MONK_PURPLE_OWNED` (145), L25 → 223 sets 146.
2. Say **"blue sash"** to 223 (146+145) → hermit's backstory: his dog was
   eaten by a tiger shark while fetching a stick at the beach; asks for
   proof of revenge; "Say 'I will help you guildmaster' when ready."
3. Say **"I will help you guildmaster"** (146+145) → unsets 146, sets 147
   (`_STARTED`), flags solo. Warns "Mind the whirlpool."
4. Say "blue sash" to Huang'lo (385, guard 147) → hint to scour the oceans,
   bring boat/food/water, avoid a whirlpool.
5. Kill a **tiger shark** (mob vnum 12413, `Mob::TIGER_SHARK` constant in
   `misc/low.h:387`) then **`dissect`** its corpse
   (`cmd/cmd_dissect.cc` + `obj/obj_base_corpse.cc:240-256`): normally
   dissect can fail based on `SKILL_DISSECT` roll, but with
   `TOG_STARTED_MONK_BLUE` set the failure-roll is bypassed entirely and the
   corpse-vnum switch (`obj_base_corpse.cc:246`) special-cases
   `Mob::TIGER_SHARK` to guarantee-yield `Obj::MONK_QUEST_DOG_COLLAR` (obj
   vnum 12468, "a golden chain dog collar with a ruby pendant") at 100%
   amount, and sets `TOG_MONK_KILLED_SHARK` (148) on the dissecting player.
6. `give` the dog collar (obj vnum 12468) to 223 (guard 148) → hermit is
   moved, unflags solo, unsets 148 and 147, `resize 6793` (blue sash),
   sets 150 (`TOG_HAS_MONK_BLUE`).

Special mechanic to preserve: the shark-dissect success/amount bypass is
gated purely on `TOG_STARTED_MONK_BLUE` — i.e. only players with the quest
active get guaranteed loot; other players dissecting a tiger shark corpse
use the normal skill-roll dissect path (and would NOT get the collar,
since the vnum-switch entry itself is only reachable... actually re-check:
the switch case fires regardless of quest bit per `obj_base_corpse.cc:246`,
but only sets num/amount `if (ch && ch->hasQuestBit(...))` — so a
non-questing player dissecting a tiger shark just falls through to no
special yield).

---

## 5. Green Sash (L35) — matches `Monk Green Quest.doc`

Guildmaster 223 (same hermit as blue). Toggles 151–156 + 161 (catfall) as
already documented in the .doc; mobresponses confirms the dialogue and adds:
- Elaborate "falling animals" science-fair framing (elephant/lion/cat land
  on feet; giraffe/rhino/wolf don't) as the hermit's stated motive.
- Huang'lo (385) hint on keyword "elephant" (guard 152): find one in the
  Veldt (south of Grimhaven, then the east/southeast road split), directions
  to Cimea/Cloud City are NOT known to Huang'lo ("you are on your own").
- Completion (guard 154, matches doc's toggle 154 "fallen") awards catfall
  (`toggle 161` = `TOG_HAS_CATFALL`) AND unlocks a **teaser for the next
  (unimplemented in original) monk item-quest**: keyword **"another
  matter"** (guard 161) — hermit wants a pet cat in Grimhaven imbued with
  `fly`, brought to him to research "catleaping". This is a real, live
  dialogue branch in the script but has **no corresponding toggle-consuming
  payoff anywhere else in the traced code** — appears to be an
  unfinished/abandoned side quest hook. Treat as flavor-only unless we want
  to build it out ourselves (out of scope for the sash chain itself).

Rooms (from `room.sql`): 11074 "The Empress' Balcony" (Cimea) → down exit →
11089 "Hanging From a Balcony" (the fall trigger room, matches doc's
"11089") → teleports to 11090 "High Up in the Clouds" → lands in 10020
"A Blank Rock Wall" ("narrow path winding around the side of the mountain
... comes to an abrupt end" — the landing room, matches doc's vnum 10020).

Award: green sash obj vnum 6794, catfall skill (toggle 161).

---

## 6. Red Sash (L45)

**Design:** pure skill-training gate, no NPCs/mobs/items beyond the
guildmaster conversation — first quest handled by 12509 (the Castaway, L50
GM, found per 223's riddle "lost at sea... a certain Island... a clue that
blows your mind").

Toggles: `TOG_MONK_RED_ELIGIBLE`=81, `TOG_STARTED_MONK_RED`=82,
`TOG_FINISHED_MONK_RED`=83, `TOG_HAS_MONK_RED`=84.

Flow:
1. Requires `TOG_MONK_GREEN_OWNED` (156), L45 → sets 81.
2. Say **"red sash"** to 12509 (81+156) → speech about needing to
   understand all combat styles; names the 4 required skills; unsets 81,
   sets 82 (`_STARTED`). (Re-saying while 82 set repeats the same speech,
   guarded on 82+!83.)
3. **Train** (via normal `learnFromDoing`/practice-in-combat mechanics, NOT
   a guildmaster `practice` purchase) all 4 of: `SKILL_SLASH_PROF`,
   `SKILL_BLUNT_PROF`, `SKILL_PIERCE_PROF`, `SKILL_RANGED_PROF` to
   **natural skill value ≥ 20 each**. Checked in
   `misc/discipline.cc:3050-3062` every time any skill increases via
   `learnFromDoing`, gated on `hasQuestBit(TOG_STARTED_MONK_RED) &&
   !hasQuestBit(TOG_FINISHED_MONK_RED)`; on all 4 reaching threshold
   simultaneously it print a special message and sets
   `TOG_FINISHED_MONK_RED` (83) directly (bypassing NPC interaction —
   this is the one stage where the state transition happens outside the
   mobresponses script).
4. Say "red sash" to 12509 (guard 83+82) → congratulates, `resize 6795`
   (red sash), sets 84 (`TOG_HAS_MONK_RED`).

No item, no mob kill — purely a skill-threshold gate reached through normal
combat use of those 4 weapon-proficiency skills.

---

## 7. Black Sash (L50)

**Was never implemented upstream.** `mobresponses` for mob 12509 has only:

```
say { "black sash";
    say This quest isn't ready yet.;
}
```

Toggles exist and are wired into the eligibility chain in
`misc/gaining.cc` (`TOG_MONK_BLACK_ELIGIBLE`=215, `TOG_MONK_BLACK_STARTED`=
216, `TOG_MONK_BLACK_FINISHED`=217, `TOG_MONK_BLACK_OWNED`=218 — all defined
in `misc/toggle.h`), and the eligibility-announcement fires normally at L50
once `TOG_HAS_MONK_RED` is set, but there is no quest content anywhere in
the codebase, world data, or mobresponses table beyond that one stub line.
No black-sash object vnum was found either (6790-6795 cover white through
red; nothing analogous exists for black — confirm no reserved vnum before
picking one).

**Decision needed for the port:** either (a) preserve the stub faithfully
(black sash "isn't ready yet" — matches original, zero new design work), or
(b) design original content for it (a deliberate deviation, needs its own
design pass and should be flagged in STATUS.md's decisions table per house
rules). Recommend (a) for an initial port pass, with (b) as a possible
later enhancement — flag this explicitly to the user rather than silently
picking one.

---

## Items reference (obj.sql)

| Vnum | Keywords | Short desc | Role |
|------|----------|-------------|------|
| 9 | bandage band aid | a bandage | white belt turn-in |
| 3319 | ashtray metal tray | a metal ashtray | yellow sash turn-in |
| 6790 | belt white monk [quest_object] | a white belt | white belt award |
| 6791 | sash belt yellow monk [quest_object] | a yellow sash | yellow sash award |
| 6792 | sash belt purple monk [quest_object] | a purple sash | purple sash award |
| 6793 | sash belt blue monk [quest_object] | a blue sash | blue sash award |
| 6794 | sash belt green monk [quest_object] | a green sash | green sash award |
| 6795 | sash belt red monk [quest_object] | a red sash | red sash award |
| 12468 | collar dog golden chain ruby pendant | a golden chain dog collar with a ruby pendant | blue sash proof item (shark dissect) |

## Mobs reference (mob.sql)

| Vnum | Keywords | Short desc | Role |
|------|----------|-------------|------|
| 207 | guildmaster monk nimble level15 | the monk guildmaster | L15 GM — white/yellow/purple |
| 223 | hermit monk guildmaster level40 | the old hermit | L40 GM — blue/green |
| 385 | Huang'lo bodyguard | Huang'lo | advice-only NPC, all 5 early quests |
| 12509 | castaway guildmaster monk level50 | the castaway | L50 GM — red/black(stub) |
| 6602 | leper | a leper | purple sash kill target (x5) |
| 8525 | elephant | an elephant | green sash mount |
| 12413 | shark tiger | a tiger shark | blue sash kill+dissect target |

## Rooms reference (room.sql) — green sash cliff sequence

| Vnum | Name | Role |
|------|------|------|
| 11074 | The Empress' Balcony | Cimea, entry point (down exit) |
| 11089 | Hanging From a Balcony | fall-trigger room while mounted on elephant |
| 11090 | High Up in the Clouds | teleport-to room mid-fall |
| 10020 | A Blank Rock Wall | landing room, sets "fallen" toggle |

---

## Open questions / things I could not fully resolve

1. **Toggle 136 (yellow) and 149 (blue)** are declared/referenced by "already
   in progress" guard clauses but no traced code path actually sets them —
   likely dead bits from an earlier design iteration; the live scripts skip
   straight from `_ELIGIBLE` to `_HAS`/`_OWNED` with only one "started"-ish
   toggle actually exercised. Worth a final grep sweep of `spec_procs.cc`
   for any generic engine that might set these implicitly (I didn't find
   one, but the mobresponses interpreter itself lives in compiled C++ I
   did not locate a source file for — see item 3 below).
2. **RESOLVED:** the interpreter is `misc/ai_responses.cc`
   (`TMonster::loadResponses`, `checkResponses`/`checkResponsesReal`,
   `modifiedDoCommand`). Confirmed semantics: within a block, actions run
   top-to-bottom in order; `CMD_RESP_CHECKTOG`/`CHECKUNTOG` return
   `RET_STOP_PARSING` on guard failure, which aborts the REST of that block
   — any actions already executed earlier in the same block before the
   failing guard stay applied (not transactional/rolled back). All the
   quests documented above put their guards first, so this doesn't matter
   in practice for this feature, but worth carrying into the port's engine
   design (sequential side effects, not atomic). `toggle`/`untoggle` also
   validate the toggle is legal for the mob's own vnum
   (`TogIndex[value].togmob`) when `!= Mob::ANY`, and `resize` validates the
   target object vnum resolves and calls `resize_personalize_object` (loads
   + gives + auto-resizes gear to the recipient, used for all sash/belt
   awards).
3. **"another matter" / catleaping teaser** (green sash epilogue) has no
   payoff anywhere traced — confirmed abandoned, not a quest to port.
4. Did not verify vnum availability for a hypothetical black-sash item —
   only confirmed 6790-6795 exist and nothing obviously analogous for
   black.
