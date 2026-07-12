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

## Buildable now (no blocked dependencies)

Self-contained — no need for the object/mob systems. Keep working through
these; each ships with a smoke test + (if player-facing) a news entry.

### User batch 2026-07-11 (continued) — working these next

- [x] **Ordinal targeting (`2.sword`, `3.goblin`)** — done. User: "when
      getting objects or attacking a mob, what happens when there is more
      than one target matching the keyword? mob 2.mob 3.mob etc should
      attack the 1st 2nd and 3rd, same for getting multiple objects, obj
      2.obj 3.obj." Confirmed the gap first: `find_obj()` (cmd_object.c,
      backs get/drop/put/give/wear) and `combat_find_room_target()`
      (combat.c, backs attack/kill) both always returned the FIRST
      keyword match, no way to reach a second/third. New shared
      `thing_parse_ordinal()` (thing.h/thing.c) parses a leading "N."
      prefix (default 1 if absent); wired into `find_obj()`/`find_worn()`
      directly (so every caller gets it for free, zero per-command
      changes needed) and into `combat_find_room_target()` (only when an
      explicit ordinal > 1 is given -- bare "kill clau" keeps its exact-
      name-priority behavior fully unchanged, since "2.clau" only makes
      sense as "count matches in room order", not "prefer an exact
      name"). `tests/smoke_test_ordinal_target.py` covers get 1st/2nd/3rd/
      (4th fails) and kill 1st/2nd/3rd, each reloading a fresh set before
      every check since consuming/killing depletes the pool.
- [x] **Armor Class + completed to-hit/defense formula** — done (user
      2026-07-11: "Armor & protection (AC) go in next, complete the
      to-hit / defense formula depth"). New `obj_armor_ac()` (obj.c) --
      the seeded `obj` table's armor rows are uniformly `val0=0` (no real
      per-item AC was ever populated, confirmed by querying the live DB;
      contrast weapons, whose val0/val1 dice fields ARE populated), so AC
      is derived from the piece's weight instead (`weight * 2`, capped at
      30) -- same "placeholder formula from an available field" precedent
      as the damage formula's STR-ATTR_BASE term. New `being_total_ac()`
      (being.c) sums it across all worn slots; `score` now shows "Armor
      Class: N". `combat_strike()`'s hit-roll formula folds this in
      (subtracted, halved to match the other modifiers' magnitude) and
      also gained Sneezy's "guaranteed hit/miss zones": the modifier
      total (dex diff + weapon hitroll + position bonus + limb penalty -
      AC/2) is now clamped to +/-44 BEFORE adding the d100 base roll,
      so no stat/gear mismatch, however extreme, can make a hit or a
      miss completely impossible (~6% floor either way) -- previously an
      unclamped modifier could in principle guarantee one or the other.
      `tests/smoke_test_armor.py` covers unarmored-vs-armored Armor Class
      display and that removing armor drops it back to 0; existing
      `smoke_test_weapon_messaging.py` confirmed the change didn't break
      ordinary combat.
- [x] **`edit player`: Class and Race fields** — done (user 2026-07-11:
      "player editor needs ability to modify class and race, and should
      be able to set class and race"). Two new menu items (9/0, matching
      the numbered-menu style) alongside the existing 8 fields; new
      `player_set_class_by_name()`/`player_set_race_by_name()` in
      player_repo.c (same not-account-scoped pattern as the gender/
      handedness setters), persisted and synced live to an already-
      connected target on Save, same as every other field. Accepts a
      name (prefix-abbreviatable, same convention as `toggle`) for both.
      Extended `tests/smoke_test_edplayer.py` with the two new fields;
      incidentally exposed a pre-existing test fragility (an exact
      HP-value assertion after a fresh reconnect could occasionally fail
      if background regen ticked HP up by 1 during the now-slightly-
      longer test run) -- loosened that assertion to tolerate natural
      regen instead of chasing the timing itself.
- [x] **Skill/spell roster framework + Warrior/Thief/Monk/Cleric/Mage
      assignment** — done. User: "lets create a list of sneezy features
      that arent implemented in tobin" led to a feature-gap audit
      (Artifact), then "combat and skills first. Skill-based combat.
      assign all warrior skills to warriors in three disciplines: combat,
      warrior skills, advanced warrior skills" -- repeated per class
      (Thief/Monk/Cleric/Mage, each "same as X" or spelled out fresh).
      Researched Sneezy's actual discArray[] (misc/spell_info.cc) per
      class rather than inventing skills -- new `include/skill.h` +
      `src/core/skill.c`: a static `skill_def_t` roster table (~200
      entries), same style as cmd_toggle.c's TOGGLES[], each tagged
      class/tier/min_level/description. Tiers are a simplified 3-way
      split of Sneezy's real sub-discipline structure: `SKILL_TIER_COMBAT`
      (universal fighting basics -- class-specific physical basics for
      Warrior/Thief/Monk, generic weapon-proficiency placeholders for the
      caster classes Cleric/Mage, since they have no melee specialty),
      `SKILL_TIER_CLASS` (the class's always-known core kit, Sneezy's
      `isBasic()` base discipline), `SKILL_TIER_ADVANCED` (Sneezy's
      optional secondary-discipline specializations). New `skills`
      command lists a player's own class's roster across all 3 tiers;
      a skill is "known" purely by character level meeting its threshold
      (no practice-point economy exists in Tobin yet, so nothing needs
      active learning). Excluded confirmed-unimplemented Sneezy
      placeholders (a few cleric/mage spells were dead code in the
      original -- `// not coded` stubs with no real `discArray` entry).
      `tests/smoke_test_skills.py` covers all 5 classes' tier headers,
      the known-vs-locked-by-level display, and a level-up unlocking a
      previously-locked skill. **Not yet done** (tracked as follow-up):
      actual in-combat mechanics for individual skills beyond the roster/
      visibility layer (task queue: flagship proof-of-concept skills,
      then Druid's custom Ranger+Cleric+reworded-Shaman blend, then
      armor/AC, to-hit depth, weapon depth, traps, affects, real spell-
      casting, spell components, magic items, object maintenance -- see
      the session's tracked task list for the full order).
- [x] **Mob/object/room scripting (`edit trigger`)** — done -- deployed
      and verified via standalone smoke test. User: "implement mob object
      and room scripting examine sneezy for ideas -- we want interaction
      with mobs objs and room via scripts." Researched SneezyMUD's actual
      system first (`sneezymud-master/docs/systems/critical/
      10-spec-procs.md`, `code/code/spec/spec_{mobs,objs,rooms}.cc`):
      spec procs are hardcoded C++ functions keyed by a numeric ID --
      flexible, but adding a new one needs a recompile + redeploy, no
      in-game authoring at all. Asked the user via AskUserQuestion which
      direction Tobin should take; they chose the in-game-authorable
      alternative over replicating spec procs or building a full embedded
      language. New `trigger` table (`db/sneezy/trigger.sql`) stores
      target (room/mob/obj + vnum), trigger type, an optional match
      keyword/chance, and a script -- authored via `edit trigger
      <room|mob|obj> <vnum> <trigger_type> [match_text|chance]` (new
      `cmd_edtrigger.c`, folded into the `edit` dispatcher), which drops
      into the same shared line editor `edit news`/`edit rules` already
      use for the script body. Trigger types: room `enter`/`random`; mob
      `greet`/`speech`/`death`/`random`; obj `get`/`wear`. Fixed action
      vocabulary (`trigger.c`'s `trigger_run()`), deliberately small, not
      a general-purpose language: `echo`/`echoroom`/`emote`/`teleport`/
      `give`/`damage`/`log`. Hook points added in `cmd_move.c` (room
      enter + mob greet), `cmd_say.c` (mob speech), `combat.c` (mob
      death, fired before `being_destroy()`), `cmd_object.c` (obj get/
      wear). `random` triggers roll `chance_pct` once per world tick
      (new `trigger_random_tick()`, pulse-registered alongside
      `mob_ai_tick()`/`obj_pool_decay_tick()`; also forced by `aitick` for
      deterministic testing) -- new `world_for_each_room()` iterator
      added alongside the existing mob/obj ones to support room-level
      random triggers. `edit trigger list <type> <vnum>` /
      `edit trigger delete <id>` manage existing triggers. Not a
      SneezyMUD port and not meant to be: no persistent per-trigger state
      (`act_ptr` equivalent), no combat-round hooks, no object-equipped
      hit/miss hooks -- follow-ups if a real need shows up. New
      `tests/smoke_test_trigger.py` covers all seven trigger types plus
      the level gate and list/delete.
- [x] **Seed starter trigger content from SneezyMUD spec procs** — done --
      deployed and verified via standalone smoke test
      (`tests/smoke_test_trigger_seed.py`). User: "and convert what sneezy
      has into a starter set of db data for tobin." New
      `db/sneezy/trigger_seed.sql` reinterprets two spec procs as real
      trigger rows: `insulter` (spec_mobs.cc) -> speech ("hello" ->
      mutters something rude) + random (10%, ambient grumble) triggers on
      the real seeded "dirty refuse hauler" (vnum 33271, already used by
      `smoke_test_look_capitalization.py`); `stickerBush` (spec_objs.cc) ->
      a new takeable "tangle of thorny brambles" prototype (vnum 1000001,
      a deliberately new namespace clear of both real content and the
      900000-970000 ephemeral test-fixture range) with a `get` trigger
      (echo + damage 2). Room-damage-trap procs (`blazingroom`,
      `BankVault`) and portal-gate procs (`SecretPortalDoors`,
      `dayGateRoom`) were deliberately left out -- attaching real damage
      or teleports to an EXISTING, already-traveled room risks disrupting
      live players, which a lightweight demo shouldn't do.
      `corpseMuncher` was also left out: no matching hook exists yet for a
      mob reacting to a corpse object specifically. Idempotent (`WHERE NOT
      EXISTS` guards), safe to re-apply.
- [x] **`shout` channel** — done -- deployed and verified via standalone
      smoke test (`tests/smoke_test_shout.py`, 5 scenarios). User: "add a
      shout channel, use sneezy for implementation ideas" (modeled on
      `sendShout()`/`doShout()` in `misc/talk.cc`). New `cmd_shout.c`:
      reaches every connected+playing character in the game (not just the
      speaker's room), echoing "You shout, ..." to self and "<Name>
      shouts, ..." to everyone else, skipping anyone asleep
      (`position <= POSITION_SLEEPING`) and — unless the shouter is
      immortal — anyone with the new `PLR_NOSHOUT` flag (`being.h`, bit
      value 2, same `player.pflags` column as `PLR_NEWBIE`, no new
      migration). New `noshout` toggle in `cmd_toggle.c`'s `TOGGLES[]`.
      Registered at `MORTAL_LEVEL_MIN`; new `shout` help topic.
- [x] **Test fix: `smoke_test_wiznews.py` pinned to the decade-old seed
      row** — done. After the buffer fix below, the test still failed
      intermittently: it checked for "Immortal News Arrives" (the original
      wiznews.sql seed row), but `wiznews` only shows the 40 most recent
      items and the table keeps growing forever -- worse, every rerun of
      THIS test while debugging posts its own permanent "Staff Meeting
      <suffix>" row via `edit wiznews`, so repeated manual reruns
      accelerated the seed row's rotation past the window. Removed the
      seed-row check; the test already separately proves posting/reading
      works with its own freshly-created item.
- [x] **Bugfix: `news`/`wiznews` were silently truncating** — done.
      Found while diagnosing a `smoke_test_wiznews.py` regression ("the
      seeded wiznews item is shown" started failing): both `cmd_news.c` and
      `cmd_wiznews.c` build their whole 40-item feed into fixed
      `body[15000]`/`full[16000]` stack buffers before handing it to the
      pager. With 8+ new wiznews entries landing THIS session alone, the
      concatenated feed exceeded 15000 bytes and `news_repo_recent()`
      (correctly bounded via `snprintf`) just stopped appending mid-word --
      silently dropping everything older, including the oldest seed item,
      with no error. Since this is an ever-growing changelog by design (one
      entry per player-facing change, forever), a "just big enough for
      today" buffer was always going to get hit again. Both buffers
      enlarged to 100000/101000 -- the pager already chunks display into
      screen-sized pages separately, so there's no reason to keep the
      working buffer tight.
- [x] **Perf fix: `trigger_random_tick` was O(mobs+rooms) DB round trips
      per tick** — done. Found while chasing an `aitick`/sweep regression
      (`smoke_test_mob_ai.py`'s "aitick forces 30 ticks" started failing):
      `trigger_random_tick()` called `trigger_repo_load_for()` -- a live
      query -- for EVERY loaded mob and EVERY loaded room, every tick, even
      though almost none have a "random" trigger row. With the world's
      loaded-room/mob registry (`world_for_each_mob`/`_room`, never
      unloaded once touched) having grown large over a long server uptime,
      `aitick 30` measured at 262s before the fix. New
      `trigger_repo_random_vnums()` (`trigger_repo.c`/`.h`) loads the
      DISTINCT vnums that actually have a "random" trigger ONCE per tick;
      `trigger.c`'s two visitors now skip straight past any mob/room not in
      that small in-memory set, cutting the DB-touching case count from
      O(mobs+rooms) to 2 queries per tick. `aitick 30` now completes well
      within `smoke_test_mob_ai.py`'s 1s-default recv timeout.
- [x] **`get all <container>`** — done -- deployed and verified via
      standalone smoke test (`tests/smoke_test_corpse.py`, extended). User:
      "corpses are supposed to act like containers. get all corpse should
      get all items the player/mob was carrying upon death." `cmd_get.c`
      (`cmd_object.c`) gained a `get all <container>` form alongside the
      existing single-item `get <item> <container>`: sweeps every object
      out of any open container -- corpse, bag, chest -- in one command,
      firing each item's `obj`/`get` trigger and the same per-item log/echo
      the single-item path already does. `get`/`containers` help topics
      updated.
- [x] **Bugfix: `drink` didn't recognize real fountains/drink objects** —
      done -- deployed and verified via standalone smoke test
      (`tests/smoke_test_drink.py`, extended). User bug report: "i just
      tried to drink from a fountain in the game, it failed with You don't
      see that here to drink." `drink` (`cmd_drink.c`) previously ONLY
      matched ground puddles via a "puddle" keyword hack; it now also
      matches any real room object with `category == OBJ_CAT_DRINK`
      (fountains, drink containers -- already-seeded content, e.g. vnum 3
      "a large fountain") by keyword. Clean water, no poison roll, never
      consumed -- liquid-unit depletion (`val[0]`/`val[1]`, `obj.h`'s
      existing DRINK category comment) is a separate, bigger feature and
      out of scope for this fix. `drink` help topic updated.
- [x] **Room look: exits colored by sector, matching the room name** —
      done -- deployed and verified via standalone smoke test
      (`tests/smoke_test_exits_display.py`, extended). User follow-up,
      2026-07-11: "the exit messages in a room should reflect the sector
      type and be colored like name." The `[Exits:]` direction list
      (`cmd_look.c`) was hardcoded green (`<g>`); now uses `bright` -- the
      exact same sector-derived tag the room NAME already renders in --
      so a lava room's exits read bright red, a forest room's bright
      green, and so on, matching instead of clashing with the name above
      it. (Separately: `<W>` for help-topic bodies, added the same
      session, is scoped to `cmd_help.c` only and does NOT touch this --
      confirmed with the user it should stay sector-driven everywhere
      else in the game.)
- [x] **Help topic display reformat** — done -- deployed and verified
      live (`help <topic>` manually checked; `smoke_test_help_topics.py`/
      `smoke_test_help_format.py`/`smoke_test_logs.py` updated and
      passing). User: three related asks in one message --
      (1) "proper case for the command": the `-- Help: <name> --` header
      now title-cases the topic (`cmd_help.c` capitalizes a local copy for
      display only; the stored/looked-up name stays lowercase);
      (2) "Administrator (59+) only: -- take this phrasing out": that
      style of level-gate phrasing baked into body prose is redundant
      with the existing `Minimum Level:` footer -- removed from `snoop`'s
      body (the worked example given); NOT yet swept across every
      historical topic in `help_topic.sql` (see the new TODO entry below);
      (3) "colorize help files with <W>": the body's color changed from
      magenta (`<m>`) to bright white (`<W>`) -- `cmd_help.c` only, every
      other in-game use of color (room names/descriptions, sector tags,
      speech, etc.) is untouched. Also, in the same message: "in the
      /format command in the editor, always indent a paragraph with 2
      spaces" -- `descriptor.c`'s `editor_format()` (the `/f` reflow used
      by every `ed*`/`edit` line editor) now indents each paragraph's
      FIRST line 2 spaces; wrapped continuation lines within the same
      paragraph are not re-indented.
- [ ] **Sweep `help_topic.sql` for redundant level-gate phrasing** —
      follow-up to the help-format reformat above: many EXISTING topic
      bodies still open with phrasing like "Administrator (59+) only:",
      "Builder tool (level 51+):", "Immortal tool (58+):" -- all redundant
      now that the footer always shows `Minimum Level:`. Only `snoop`'s
      body (the user's worked example) has been cleaned up so far. Wide
      but mechanical (dozens of rows); needs its own pass rather than
      being folded into unrelated feature work.
- [x] **Room look: list permanent fixtures (lamppost, fountain, ...)
      first** — done -- deployed and verified via standalone smoke test
      (`tests/smoke_test_look_fixture_order.py`). User: "permanent items
      such as a lamppost or a fountain should be listed first in look room
      code." `cmd_look.c`'s room listing now walks `stuff_head` in two
      passes: non-takeable fixture objects (`!obj_takeable(o->wear_flag)`
      -- fountains, furniture, statuary) first, then everything else
      (ordinary takeable loot, mobs, PCs) in their original order.
      Per-item formatting logic factored into a new `append_room_item()`
      helper so both passes share it verbatim. A corpse is also
      non-takeable-as-a-whole, so it sorts into the fixture group too --
      not exactly "permanent," but harmless (still a reasonable thing to
      surface prominently) and not worth a separate flag for.
- [x] **`snoop` command** — done -- deployed and verified via standalone
      smoke test (`tests/smoke_test_snoop.py`). User: "implement a snoop
      command like sneezy, the command should be 59+ where you cant
      snoop anyone of same or higher level." Modeled on
      `TPerson::doSnoop()` (bundled reference tree, `misc/immortal.cc`):
      `snoop <name>` (59+) mirrors everything a lower-level target sees
      AND everything they type to the snooper in real time, one outgoing
      snoop at a time; refuses a same-or-higher-level target ("You
      failed."), refuses a target already being snooped ("Busy
      already."), and bare `snoop` (no argument, or `snoop <yourself>`)
      stops your own snoop (user follow-up, 2026-07-11: "have it default
      to self without an arg"). New
      `snoop_target`/`snooped_by` descriptor pointers (`descriptor.h`),
      unhooked in `descriptor_destroy()` so neither side is ever left
      pointing at a freed descriptor. The mirroring itself lives in
      `descriptor.c`: `descriptor_send()` mirrors output via a direct
      `socket_write()` (not a recursive `descriptor_send()` call, so a
      mutual/chained snoop can never recurse); the `CONN_PLAYING` input
      handler mirrors the target's own typed lines, prefixed `"% "`
      (classic DikuMUD/Sneezy convention). Covert: the target is never
      told, and it's logged `LOG_SILENT` (file only, matching the
      get/drop precedent for anything that shouldn't tip anyone off live).
- [x] **Unify `ed*` commands into one `edit <noun>` dispatcher** — done --
      deployed and verified via standalone smoke test. User: "unify all
      ed* commands into one edit command that accepts arguments for
      example edit room <vnum>, edit object <vnum>, edit player <name>,
      etc. and keep the level assignments for each function valid."
      Removed `edroom`, `edzone`, `edplayer`, `edhelp`, `ednews`,
      `edwiznews`, `edrules` as standalone command-table entries; all
      seven now route through a single `edit <noun> [args]` command
      (`cmd_edit.c`), forwarding to the exact same unchanged
      implementation functions. Registered at `BUILD_MIN_LEVEL` (51, the
      lowest of any sub-editor); a noun needing more (player 58+,
      help/news/wiznews 56+, rules 59+) checks that internally and
      refuses with the same "Huh?!" a table-level gate would have given
      -- nothing was loosened. `edit room`'s backing function renamed
      `cmd_edit()` -> `cmd_edroom()` (file `cmd_edit.c` -> `cmd_edroom.c`)
      to free up the name for the new dispatcher. Consolidated help
      topic (`help edit`) replaces the old per-command topics and the
      old hardcoded "help edit" live-index-of-ed*-commands special case
      in `cmd_help.c` (now a normal DB-backed topic like any other).
      `object`/`mob` nouns are reserved in the usage text for when those
      editors exist (not wired to anything yet). 13 existing tests
      updated for the new command shape.
- [x] **Mob wander message bug fix** — done -- deployed and verified via
      standalone smoke test. User: "lady stroll walk leaves. is not
      correct it should be A <short desc> <walk type> to the east." Root
      cause: `mob_ai.c`'s wander leave/arrive messages printed
      `m->base.name` directly -- for a mob that's the space-separated
      KEYWORD list (e.g. "lady stroll walk", so you can `look lady`/`look
      stroll`/`look walk`), not a display name, producing exactly the
      garbled text reported. Fixed to use `short_descr` (capitalized) plus
      the real direction of travel/arrival (`DIR_NAMES`/`REV_DIR`, room.h):
      "A lady walks to the east." / "A lady walks in from the west."
- [x] **Immortal custom move messages (`bamfin`/`bamfout`)** — done --
      deployed and verified via standalone smoke test. User: "immorts
      should be able to set their own enter or leave messages. Like Jesus
      drags his cross in from the east. of course gender specific in the
      messaging" (named `poofin`/`poofout` originally, renamed to
      `bamfin`/`bamfout` per user request the same session). New
      `player.bamfin`/`player.bamfout` columns (`tobin_migrations.sql`),
      settable via new `bamfin`/`bamfout <msg>` commands (`cmd_bamf.c`,
      `IMMORTAL_LEVEL_MIN`), mirroring `title`'s set/clear/persist shape.
      `do_move()` (cmd_move.c) substitutes `$d` (the direction word) and
      `$p` (`gender_possess()`, so the same template reads correctly for
      any gender) before showing it in place of the default "exits to the
      <dir>"/"has arrived" wording.
- [x] **Pools grow instead of duplicating + no-newline fix** — done --
      deployed and verified via standalone smoke test. User: "pools
      should grow in size if multiple puddles of the same type are
      created in a room, and no new line after the pee short
      description." `obj_create_pool()` replaced with `obj_grow_pool()`
      (obj.h/obj.c): if a puddle of the same type ("pee"/"blood") already
      exists in the room, it grows a size tier in place ("a puddle of X"
      -> "a pool of X" -> "a large pool of X", tracked in `val[0]`)
      instead of a new object being created. Also fixed the blank-line
      bug: `obj_t.long_descr` was storing a baked-in trailing `\r\n`,
      doubled up with the one `cmd_look.c`'s room-floor listing/`look
      <item>` already append -- removed from `pee`'s and the blood
      pool's long_descr, plus two other pre-existing occurrences of the
      exact same bug (the severed-limb and corpse long_descr in
      combat.c), same root cause. Also colorized (user, 2026-07-11:
      "pee blood x4 should create A large pool of <R>blood<z> is
      here."): the substance noun is wrapped in a color tag that
      escalates with size -- dim (`<r>`/`<y>`) for puddle/pool, bright
      (`<R>`/`<Y>`) once it's a "large pool" -- matching the escalating
      wording tier.

- [x] **Pools decay over time** — done -- deployed and verified via
      standalone smoke test. User: "pools should absorb into the ground
      little by little upon ticks." New `obj_pool_decay_tick()` (obj.c),
      pulse-registered at the same ~60s cadence as `mob_ai_tick()`
      (main.c): every ground puddle shrinks one size tier per tick
      (reversing `obj_grow_pool()`'s growth), and a puddle at the
      smallest tier is destroyed outright on its next tick rather than
      shrinking further -- "little by little" until it's gone. New
      `world_for_each_obj()` (world.h/world.c), the object-iteration
      counterpart to the existing `world_for_each_mob()`. `aitick` (the
      existing mob-AI debug/testing command) now also forces pool decay
      each iteration, so `tests/smoke_test_pool_decay.py` can test it
      deterministically without waiting on the real pulse.

- [x] **`look`'s exits line reformatted + colorized** — done -- deployed
      and verified via standalone smoke test. User: "Obvious exits: north
      east south west southwest change to [Exits:] North East South West
      Southwest and colorize the string appropriatly." `cmd_look.c`'s
      one-line exits summary now reads "[Exits:] North East ..." (cyan
      label, green capitalized direction list) instead of "Obvious exits:
      north east ...". Updated the 4 existing tests that scraped the old
      wording/case (`smoke_test_doors.py`, `smoke_test_linkdead.py`,
      `smoke_test_notify.py`, `smoke_test_scan.py`); new
      `tests/smoke_test_exits_display.py` covers the format directly
      (color off/on, and the "none" dead-end fallback). The dedicated
      `exits` command (`cmd_exits.c`) keeps its own separate, more
      detailed per-direction listing unchanged -- only `look`'s one-line
      summary was in scope.

- [x] **"Related" footer on help topics** — done (user 2026-07-11: "for
      help topics both wizhelp and help add a line at the end for related
      topics: Related: topic topic topic etc"). No new DB column --
      `cmd_help.c` strips a trailing "Related: ..." line out of the body
      (same convention as the existing leading "Usage:" line) and shows
      it as its own cyan-labeled footer, only when present. Populated
      across ~70 existing topics (movement, combat, positions, items,
      communication, admin/builder tools, the whole `edit` family, etc)
      via a guarded `CONCAT`-based migration in help_topic.sql (skips
      topics that already have one, so a re-run never double-appends).
      Follow-up (same session, user: "in the help editor we should be
      able to set related topics in there"): `edit help`'s line editor
      gained a `/r <topics>` command (bare `/r` clears) alongside the
      existing `/s`/`/a`/`/b`/`/f`, storing into a new
      `descriptor_t.edit_related` field instead of requiring the author
      to type a literal "Related:" body line by hand; appended back onto
      the body on save. Re-editing an existing topic strips any stored
      Related line out of the shown body and preloads it into `/r`'s
      state (shown as "Current related topics: ..."), so the round-trip
      never duplicates it.
- [x] **Per-noun `help edit <noun>` topics** — done. `cmd_help.c`'s
      "help <topic>" parsing only reads the FIRST whitespace token, so
      "help edit room" silently collapsed to just "help edit" -- fixed by
      folding "edit" + a following noun into a single two-word lookup key
      ("edit room") before the DB lookup. Also fixed a real pre-existing
      bug found in the process: `edroom`/`edzone`/`edplayer`/`edhelp`/
      `ednews`/`edwiznews`/`edrules` have been dead, unreachable topics
      ever since the ed* commands were unified into `edit <noun>` -- a
      comment in help_topic.sql claimed they'd been deleted but no DELETE
      was ever actually added. Renamed in place to `edit room`/`edit
      zone`/`edit player`/`edit help`/`edit news`/`edit wiznews`/`edit
      rules` (bodies kept, already accurate, each gained a Related line)
      rather than discarded. `edit trigger` intentionally still resolves
      to the existing standalone `trigger` topic (already comprehensive:
      trigger types, the fixed action vocabulary usable inside a script,
      list/delete syntax) rather than a duplicate -- `help trigger` and
      `help edit trigger` both need to keep working. `edit`'s own topic
      gained a Related line listing all 8 nouns.
- [x] **`nospam` toggle (combat)** — done (user 2026-07-11: "add a nospam
      toggle where the games output during fights doesnt show missed
      hits in messages and logs", "take inspiration from sneezy").
      Confirmed Sneezy precedent (`toggle.h:22` `AUTO_NOSPAM = (1 << 0)`,
      checked per-viewer independently in `combat.cc`) -- ported as a new
      `PLR_NOSPAM` bit on `player.pflags` (a per-player DB flag, since
      Tobin's player state already lives there rather than on a transient
      descriptor struct), toggled via `toggle nospam` (cmd_toggle.c,
      same table-driven pattern as `noshout`). `combat.c`'s
      `combat_strike()` miss branch checks each side's own flag before
      sending its "You miss .../ ... misses you!" line -- not log-related
      (Tobin's combat.c never logged misses to begin with). New
      `tests/smoke_test_nospam.py` forces a guaranteed miss via an
      absurdly negative `objaffect` hitroll bonus (same mechanism the
      weapon-messaging test uses in reverse for guaranteed hits) to test
      deterministically instead of waiting on ~50% RNG.
- [x] **Hostname (reverse DNS) instead of raw IP in messages/logs** — done
      (user 2026-07-11: "in messages and logs where IP address is
      displayed, make it a hostname dns lookup instead"). Confirmed
      Sneezy has no real precedent to port (its `desc->host` is just a
      stringified IP under a misleading label -- see research notes this
      entry used to carry). Designed from scratch: new `hostname_resolve.c`
      spawns one detached pthread per accepted connection to run
      `getnameinfo()` (NI_NAMEREQD, so a failed lookup stays empty rather
      than "resolving" back to the same numeric string) -- never inline
      on accept(), which would stall the whole single-threaded select()
      loop on a slow/absent DNS server. Results land in a small fixed-size
      mailbox (`RESOLVE_SLOTS 32`, mutex-guarded) that `hostname_resolve_
      poll()` drains once per game-loop tick, matching each result back to
      its descriptor by fd AND ip together (fd reuse after a fast
      disconnect is the one real race; requiring ip to also match makes a
      mismatch practically impossible). New `descriptor_display_host()`
      (falls back to the raw ip while unresolved or on failure) replaces
      every direct `d->ip` read at a log/display site (`users`, connect/
      reconnect/link-drop PIO logs, character/account deletion logs,
      pee/purge/transfer edit logs, the combat-death log) -- NOT the
      couple of sites that need the real IP regardless (the loopback-only
      `exec` gate check, and the copyover recovery file, which must
      preserve the actual address for reconnection, not a possibly-still-
      unresolved hostname). New `Threads::Threads` link dependency
      (CMakeLists.txt) -- Tobin's first pthread usage.

### User batch 2026-07-11 — working these next

- [x] **Pools + `pee` command (51+)** — done -- deployed and verified via
      standalone smoke test; full sweep pending. User: "add pools and
      the pee command for 51." New
      `obj_create_pool()` (obj.h/obj.c) is a reusable non-takeable ground
      puddle (category `OBJ_CAT_TRASH`, so an `ACT_SCAVENGER` mob eventually
      cleans it up — ties into the existing mob AI scavenge behavior). New
      `pee` command (`cmd_pee.c`, `IMMORTAL_LEVEL_MIN`) is the first user of
      it: leaves a "puddle of pee" on the floor, tells the caller, and
      echoes to the room. No merging/evaporation of puddles over time —
      each use just adds another one, same minimal-scope precedent as
      `purge`/`transfer`. New `tests/smoke_test_pee.py`.
- [x] **Blood pools from limb damage/bleeding** — done -- deployed and
      verified via standalone smoke test; full sweep pending. User: "goes
      with limb damage and bleeding" (said
      right after the pools/pee request). `combat_strike()` (combat.c)
      already announces a limb crossing into a bad-enough tier
      (`limb_status_text()` non-NULL, <20% HP) -- reused that exact
      tier-crossing guard to also drop a "pool of blood" via
      `obj_create_pool()` at the same moment, echoed to the room ("Blood
      pools around X!"). No actual bleed-over-time/DOT mechanic (that's a
      bigger, separate thing) -- just a one-shot flavor pool per tier
      crossing, same minimal scope as `pee`.
- [x] **`drink` from pools, chance of poison** — done -- deployed and
      verified via standalone smoke test; full sweep pending. User: "yu
      should be able tto drink from the pools, chance to get poisoned."
      New `drink <puddle>` command (`cmd_drink.c`, `MORTAL_LEVEL_MIN`)
      finds any ground object tagged with the "puddle" keyword (both the
      pee and blood pools qualify) and lets anyone drink from it -- never
      consumed/removed. 30% chance of a 2-8 HP "poison" hit, clamped so it
      can never drop the drinker below 1 HP (no death-outside-combat
      handling exists yet, so this stays a flavor scare, not a real
      hazard). New `tests/smoke_test_pee.py`, `tests/smoke_test_bleeding.py`,
      `tests/smoke_test_drink.py`.

### User batch 2026-07-10 (continued session) — working these next

- [x] **Confirm before creating a new account at login** — done --
      deployed and verified via standalone smoke test and a clean full
      sweep. User: "in account login, if
      someone types in an account name that doesnt exist, we're assuming
      the want a new account. it should ask: New account, are you sure you
      want to create account <account name>? (y/n) yes creates a new
      account and no prompts for the correct login name." New
      `CONN_CONFIRM_NEW_ACCOUNT` state (descriptor.h/descriptor.c) sits
      between the account-name prompt and password creation. Ripple effect:
      this is a new step in front of EVERY new-account flow, so every
      existing smoke test that creates a fresh account needed a `y` answer
      inserted -- swept across tests/*.py. New
      `tests/smoke_test_account_confirm.py` covers the prompt itself
      (naming, y/n branches, and that "n" truly creates nothing).
- [x] **Delete entire account from the account menu** — done --
      deployed and verified via standalone smoke test and a clean full
      sweep. User: "add a delete option to
      delete account from the account menu, requires user password to
      delete account." New `X` / `delete account` command at the account
      menu, mirroring the existing per-character delete flow one level up
      (type YES, then re-enter the account password). `account_delete()`
      (account.h/account_repo.c) just deletes the `account` row --
      `player.account_id` already carries an `ON DELETE CASCADE` FK, so
      every character on the account (and their attrs/progress/inventory
      rows) goes with it automatically. Disconnects the session afterward
      (the account is gone). New `tests/smoke_test_account_delete.py`.
- [x] **Log messages for player and account deletion** — done: both
      deletions already logged via `log_info()` (file/console only, per
      user: "the messages should just go to game log, not broadcast" --
      NOT `game_log()`, which would also echo live to online immortals).
- [x] **`transfer` command** — done -- deployed and verified via
      standalone smoke test and a clean full sweep. User: "add a transfer
      command that will take a target and transfer them into the same
      room as the transfer command was issued in (transfer name) also
      transfer name vnum to transfer the target to the room tht matches
      vnum." Mirrors the original's `trans` (bundled sneezymud-master
      reference tree, `lib/help/_immortal/transfer`) plus the user's own
      room-vnum variant; scoped to online PCs only (no numbered mob
      syntax like the original's "trans 4.chicken"). Bystanders in both
      the old and new rooms see a "puff of smoke" departure/arrival; the
      target is told what happened and shown a fresh `look`. New
      `tests/smoke_test_transfer.py`.

### User batch 2026-07-09 (home session, post-NewMUD-migration) — working these now

- [x] **Corpse on death (mobs and players alike)** — done (Session 43,
      user: "make it so the corpse of a char loads into the room upon
      death. the corpse should be treated like a container and all
      inventory can be taken off said corpse... mobs and players alike"):
      `combat_defeat()` now creates an ephemeral "corpse of <name>" container
      (`obj_create_ephemeral()`, same primitive as the crit-hit severed
      limbs -- vnum 0, never persisted) and moves everything the loser had
      -- carried, worn, held -- INTO it instead of dropping loose on the
      floor. Not takeable as a whole (`get corpse` alone is refused) and
      never closed/locked, so `get <item> corpse` works immediately with
      no `open` needed. Applies to BOTH a PC's death and a mob's (a mob's
      corpse is empty today since mobs don't carry anything yet, but the
      object itself still appears). `smoke_test_corpse.py` (7 checks).
- [x] **Merge `mload`/`oload` into one `load <mob|obj> <vnum|name>`** — done:
      `cmd_load.c` replaces both; category is abbreviatable down to the bare
      letter (M/O), same as full words. Table-order gotcha (like set/setsev):
      `load` is a prefix of `loadroom`, so `loadroom` now needs `loadr`+ (was
      `loa`+). `R` (ride/follow?) never came up again -- still unimplemented,
      ask if it resurfaces. Old help topics removed + merged; 3 tests updated.
- [x] **Equipment display reformat** — done: right-aligned `label: value`
      columns (14-char field, matching "secondary hold"), replacing the old
      `<label> value` bracket form. Hand slots renamed to **`primary hold`**/
      **`secondary hold`**, now correctly tracking the caller's dominant hand
      (handed_right) instead of a fixed held[0]/held[1] (a latent bug for
      left-handed characters, fixed in passing). **Genitalia removed from
      the listing** -- never actually wearable, just cosmetically listed;
      becomes an object on decapitation instead (crit-hit item below, still
      unbuilt).
- [x] **`hold` vs `wield`, and a `switch` command** — done: `wear` now only
      covers body-slot equipment; a holdable item refuses `wear` and points
      to whichever of `hold` (non-weapons) / `wield` (weapons, gated on
      `obj_t.category == OBJ_CAT_WEAPON`) applies. `switch` swaps
      `held[0]`/`held[1]` in place. Table collisions resolved (documented
      inline): `switch` needs `swi`+, `wield` needs `wie`+, `hold` needs `ho`+.
- [x] **Gender-specific pronouns in ALL mud output** (user 2026-07-09,
      standing habit going forward like the colorize-tastefully rule) —
      uses `gender_subject/object/possess()` (being.c, Session 23), plus a
      new `gender_reflexive()` (himself/herself/itself, Session 43). DONE:
      the link-loss line, `stand`'s room echo (both earlier sessions), and
      now `src/core/socials.c` -- despite the "~15 pairs" estimate, only 3
      of the 16 socials actually used a gender-neutral pronoun once checked
      carefully (`shake`'s "their head" in all 3 echoed forms, `poke`'s
      "themselves", `comfort`'s "they need"): the table keeps its bare
      fallback text (so `social_names()`/the table stay the single source
      of truth for each social's shape) but `social_try()` now overrides
      those 3 specifically with the actor's real pronoun before display.
      New player-facing messages should stay gender-aware going in.
- [x] **Linkdead persistence** (user 2026-07-09) — losing link no longer
      destroys the character: `descriptor_destroy()` detaches (`desc=NULL`)
      instead, leaving the being in its room; `world_find_linkdead_pc()`
      (world.c) finds it on reconnect, `enter_world()` does a fresh DB load
      as always (so a concurrent promotion/edit still applies) but resumes
      it in the linkdead body's room, then discards the old body. Recovers
      via reconnect or process end only (copyover only restores
      descriptor-attached beings). Room listing tags "(linkdead)"; combat
      can't target a linkdead PC at all ("no one can manipulate a linkdead
      char") -- `combat_find_room_target()` skips them. `smoke_test_linkdead.py`
      (8 checks). Fixed 5 existing tests whose abrupt-close-right-after-
      creation pattern now goes linkdead instead of destroying, breaking
      their "SQL-set field takes effect on next login" assumption -- each
      needed an explicit `quit!` first (objects, mobiles, edplayer, set,
      sector_color).
- [x] **World death taunt: PC deaths only** (user 2026-07-09, "should only
      fire when a player dies, skip the mobs unless the mob is the killer")
      — `combat_defeat()` (combat.c) wraps the `[INFO]` broadcast block in
      `if (loser_is_pc)`; a mob's death (Phase 2D) is now silent world-wide,
      while a mob-as-killer still taunts normally (the taunt names the
      loser, not the winner). `smoke_test_mobiles.py` section 5 covers it
      (bystander confirms no `[INFO]` on a mob death).
- [ ] **Account menu: hide the character list until `C`** — currently
      `-- Your characters --` lists every character immediately on reaching
      the account menu. Change so the list is HIDDEN until the player types
      `C` (bare, no number/name yet); typing bare `C` then reveals the list
      and prompts for a number/name (or `N` to create) as a follow-up step.
      `C <number|name>` (already-known target) should probably still connect
      directly without the extra round trip -- confirm with user if that
      one-step form should stay.
- [x] **Port Sneezy's crit-hit system + decapitation object creation** (user
      2026-07-09) — done, scope confirmed with the user first: (1) no
      separate crit-roll -- triggers purely on a limb's HP crossing to 0%
      from ordinary combat damage (combat_strike() in combat.c); (2) ALL
      limbs sever into a lootable ephemeral object ("X's severed <limb>",
      obj_create_ephemeral() in obj.c/obj.h), not just the head -- genitalia
      included, per the user's example; (3) the HEAD specifically is a
      decapitation -- an instant kill routed through the existing
      combat_defeat() "slain" path; (4) PCs only for v1 -- a mob's limb
      reaching 0 HP does nothing extra (mobs still die the plain Phase-2D
      way). Fixed a real balance bug found while scoping this: a level-1
      character's limbs were splitting to 1 HP each (25 max HP / 13 limbs),
      so ANY landed hit already destroyed a limb -- added a `LIMB_MIN_MAX_HP`
      floor (15) in being_limbs_full_heal() so severing/decapitation takes a
      real run of hits even at level 1, not a first-swing coin flip. Added
      an immortal-only debug command `hurtlimb <target> <limb> <hp>`
      (cmd_hurtlimb.c) to test this deterministically instead of waiting on
      combat RNG. `smoke_test_crit.py` (18 checks, including the PCs-only
      scope guarantee).

### User batch 2026-07-07 (home session) — working these now

- [x] **Consistent editor slash-commands** (user 2026-07-07) — done: one set
      keyed to each action's first letter -- `/s` Save, `/a` Abort, `/b` Blank
      (clear), `/f` Format -- centralized in `editor_feed()` (descriptor.c) so
      it covers every ed* editor at once. The old `.`/`~`/`/clear`/`/format`
      keys were removed (user follow-up) -- a bare `.`/`~` is now literal text.
      All editor intro lines + the editor smoke tests updated;
      `smoke_test_editor_format.py` broadened to cover the whole key set.
- [x] **help/wizhelp list size + vnum pagination** — done: help/wizhelp list
      buffer 2048->8192 and name arrays 256->512 (no truncation as commands
      grow); `vnum` now pages the full list (descriptor pager, like `news`)
      instead of stopping at 40, with a 500-row safety cap.

- [x] **Port `scan`** — done 2026-07-07: `cmd_scan.c`, a faithful port of the
      original's `doScan()` (misc/range.cc). Ray-casts up to SCAN_MAX_RANGE (6)
      rooms deep down each exit and reports the players/mobs out there with a
      distance word + direction; `scan <dir>` scans one direction, `scan <name>`
      filters by name, a closed/secret door blocks the line of sight.
      (Skipped the original's move-point cost + blindness gate -- Tobin has
      neither.) Follows exit chains through unloaded rooms via a `roomexit`
      query; occupants come from active (`world_get_room`) rooms only.
      `smoke_test_scan.py`, help topic, news entry ("Cast Your Gaze Afar").
- [x] **`vnum <room|obj|mob> <pattern>`** — done 2026-07-07: `cmd_vnum.c`,
      builder tool (51+). Lists the vnums + names of rooms/objects/mobiles
      whose `name` contains a substring (direct DB_TOBIN query, per
      cmd_mudstats precedent), lowest vnum first, paged a screen at a time
      (descriptor pager, like `news`; 500-row safety cap). Category is
      abbreviatable. `smoke_test_vnum.py`, help topic (no news -- immortal-only,
      same precedent as oload/mload).

### User batch 2026-07-07 (reported during the mobiles session) — working these next

- [x] **`look <object>` doesn't work** — already done in Session 37 (this box
      was just never pruned; verified 2026-07-09). `look_at_target()` falls
      back from PC/mob to a room-floor-then-own-inventory `THING_OBJ` search
      (`find_obj_here()`), showing `long_descr` + a condition line from
      `cur_struct`/`max_struct` (`obj_condition_text()`). Worn/held covered too
      (same `stuff_head` chain). Covered by `smoke_test_objects.py`.
- [x] **`help color`/`help who`: list every color tag + mention `<N>`** —
      done (Session 43, help_topic.sql migration): `help color` now lists
      every `<x>` tag itself (previously only the separate `help colors`
      topic did) and mentions title `<N>`/`<n>` substitution; `help who`
      now mentions that a shown title can use both color tags and `<N>`/
      `<n>`.
- [x] **`bamfin`/`bamfout`** — done, superseded by the "Immortal custom
      move messages" entry above (this stub predates the actual build;
      originally named poofin/poofout, renamed to bamfin/bamfout per user
      request the same session). New `player.bamfin`/`player.bamfout`
      columns wired into `cmd_move.c`'s room-echo calls, with `$d`/`$p`
      direction/pronoun substitution.
- [x] **Colorize copyover messages** — done (Session 43): the 3 player-
      facing reboot lines in `cmd_copyover.c` (5-second warning, mid-
      reborn, please-reconnect) now use `<c>...<z>`, matching the existing
      INFO/`system`-broadcast color convention. The immortal-only error/
      status lines (unavailable, write-failed, exec-failed) stay plain.
- [x] **`@set` currently just falls through to Huh?!** — done (Session 43):
      a leading `@` is now stripped before the normal verb parse in
      `cmd_dispatch()` -- simpler than a hardcoded `'`/`;`-style alias since
      the real verb ("set") already follows the `@`, no need to hardcode a
      target. Covers any other stray leading `@`, not just `@set`.
- [x] **Verify multiplay-off actually gates a second mortal connection**
      — done, verified 2026-07-11: `smoke_test_multiplay.py` already
      covers this exact scenario with two REAL simultaneous connections
      (not mocked) -- reran it live and all 5 checks passed cleanly:
      default-off refusal, a 59+ immortal turning it on, the second
      character then connecting, and `multiplay` staying hidden from
      mortals. `enter_world()`'s gate (`descriptor.c`) is confirmed
      working as designed; no fix was needed.
- [x] **`gametog` (58+)** — done -- deployed and verified via standalone
      smoke test (`tests/smoke_test_gametog.py`). Split `toggle`:
      game-wide switches (`multiplay`, previously living inside the
      unified `toggle` command at 55+) moved to a new `gametog` command
      gated 58+; `toggle` now shows/accepts ONLY the mortal-settable
      personal switches (color, hp, newbie, noshout) -- multiplay isn't
      merely hidden by level anymore, it doesn't exist within `toggle` at
      all. Both share the same `TOGGLES[]` table (already had a `game`
      per-row flag) and dispatch logic, factored into a new
      `toggle_dispatch(d, args, game, header)` helper `cmd_toggle()`/
      `cmd_gametog()` both call. The pre-existing standalone `multiplay
      <on|off>` command (59+, `cmd_multiplay.c`) is untouched -- out of
      scope for this split, a separate (if redundant) entry point.
      `smoke_test_toggle.py` updated for the new "No such toggle" response
      to `toggle multiplay` instead of the old level-gate message.
- [x] **Editors must get ABSOLUTE quiet** — done (Session 43, user: "when
      in the editors, no messages to interrupt, no logs, no output at
      all. thats what catchup is for"). The audit found the REAL bug:
      `descriptor_in_editor()` (descriptor.c) only ever checked the
      `CONN_REDIT_*` range -- `edplayer` and `edzone` were never wired in
      at all, so `descriptor_notify()`'s hold-for-catchup silently never
      applied to them, even though every broadcast call site (game_log,
      death taunt, wiznet, system, newbie) already correctly called
      `descriptor_notify()`. One-line fix in the shared predicate, not a
      per-call-site chase. Also found and fixed the same-root-cause
      pattern elsewhere: `who`/`promote`/`set`/`copyover`/`users` all used
      `state == CONN_PLAYING` as an "is online" proxy, which excludes
      every editor sub-state -- an editing immortal was invisible to
      `who`, got stale live-sync from `set`/`promote`, lost their session
      entirely across `copyover`, and showed as "closing" in `users`. All
      fixed to check `it->character` (or the new range check) instead.
      `smoke_test_held.py` extended to cover edplayer/edzone, not just
      edroom (the gap the old test couldn't have caught).
- [x] **`edbug`** — done -- deployed and verified via standalone smoke
      test (`tests/smoke_test_edbug.py`). One-shot: `edbug <id> [note]`
      (59+, same tier as `delbug`) marks a bug resolved WITHOUT deleting
      it -- new `bug.resolved_at`/`resolution` columns (`bug.sql`,
      `tobin_migrations.sql`) -- so the report stays on file instead of
      vanishing. If the submitter is online right now they get a live
      notice (with the note, if given); either way a resolved report
      drops out of the outstanding `bug` list (`bug_repo_list()` now
      filters `WHERE resolved_at IS NULL`) but the row survives, so
      `delbug` can still remove it later if truly no longer needed.
- [x] **`mlist`/`olist`/`rlist` (builder list commands)** — done, folded
      into the EXISTING `vnum <room|obj|mob> <pattern>` command instead of
      three new near-duplicate ones: `vnum` already listed prototypes by
      name/keyword substring, paginated, builder-gated -- everything the
      three list commands would have needed except vnum/range browsing.
      `cmd_vnum.c` gained `parse_vnum_range()`: `<pattern>` may now also be
      a bare vnum ("vnum obj 1017") or a range ("vnum obj 100-200"),
      switching the query from a name `LIKE` search to `vnum BETWEEN`.
      Verified via standalone smoke test (`tests/smoke_test_vnum.py`,
      extended) against the real seeded fountain (vnum 3).
- [x] **`hit` command (real combat, never instakill)** — done (Session
      43): `cmd_hit.c` is a thin passthrough to `cmd_attack()` (which never
      special-cased immortals to begin with), so an immortal typing `hit`
      gets the normal multi-round combat process instead of `kill`/
      `attack`'s instant slay. Those two are unchanged. New help topic +
      `smoke_test_combat.py` Part 4.
- [ ] **General output pagination (20-line threshold)** — any command
      output longer than 20 lines should paginate automatically (a "more"
      prompt, ENTER for next page / Q to stop -- same UX `news`/`wiznews`
      already have), with one blank line before and after each page.
      Currently only `news`/`wiznews` paginate; this asks for a shared,
      reusable helper so every long output gets it for free, not just
      those two commands.
- [x] **Smoke tests still aren't logging start/finish to the MUD's log** —
      fixed 2026-07-07 (Session 36): the Session 32 `announce()` helper had
      in practice only ever landed in the one file that introduced it
      (`smoke_test_logging.py`); every other test was silently missing it.
      Retrofitted all 56 `tests/smoke_test_*.py` with a self-contained
      `announce()`/`announce_done()` pair; `descriptor.c`'s `@test` hook now
      also recognizes `@test done <name>` and logs `[TEST] finished %s`
      (distinct from `running %s`). See STATUS.md.

### User batch 2026-07-06 (morning queue) — working these next

- [x] **Port `setsev` log severity** — done 2026-07-06: `cmd_setsev.c`, a
      port of `misc/immortal.cc`'s `doSetsev()`. Bare `setsev` lists every
      log type (game/pio/combat/bug/db/edit) with on/off state; `setsev
      <type>` (abbrev ok) flips one, gating `game_log()`'s `[TAG]` echo via
      a new `being_t.severity` bitmask (default: everything on). The
      personalized `jesus` type is hidden from and unsettable by anyone but
      the immortal actually named Jesus, matching the original's per-name
      toggle. Deliberately simplified vs. the original: session-only, not
      persisted (the original's `wizdata` table isn't worth a migration for
      this) -- see the field comment in `being.h`. `smoke_test_setsev.py`
      (help topic added too; no news entry -- immortal-only, same precedent
      as `toggle`/`exec`/`wiznet`).
- [x] **Colorize room name + description by sector** — done 2026-07-06:
      `sector_color()` (room.c) buckets each of the 61 sector types by
      keyword (lava/fire->red, city/road/building->white, mountain/cave/
      solid rock->gray, ocean/river/beach->blue, arctic/atmosphere->cyan,
      desert->yellow, swamp/forest/jungle/grassland/plains/hills->green,
      astral->purple, else->white). `cmd_look.c` wraps the room NAME in the
      bright (uppercase) tag and the DESCRIPTION in the dim (lowercase) one,
      for both the mortal and immortal-builder-header display paths.
      `smoke_test_sector_color.py` (raw-byte ANSI checks, mortal + immortal
      paths); help topic updated.
- [x] **Editor `format` option** — done 2026-07-06: `/format` (matching the
      existing `/clear` slash convention) reflows the shared editor buffer
      (`editor_format()` in descriptor.c) to `EDITOR_FORMAT_WIDTH` (78)
      columns, joining/re-breaking words but preserving blank-line
      paragraph breaks. One shared implementation in `editor_feed()` covers
      every `ed*` editor (edroom's description field, edhelp, ednews,
      edwiznews, edrules) automatically, since they all route through it.
      `smoke_test_editor_format.py` (via edhelp); all editor intro
      messages + help topics mention it now.
- [x] **`set` + `@set` commands** — done 2026-07-06, as two commands: the
      menu-driven `edplayer` (see below) plus a one-shot `set <name> <field>
      <value>` (`cmd_set.c`, same 58+ gate) for quick scriptable single-field
      edits -- user confirmed both were wanted, not one instead of the other.
      `set` covers the same fields as `edplayer` (level/xp/hp/attributes/
      gender/title/loadroom/handed), same admin-wide-by-name reach and
      online-target live sync. The original's 1279-line `@set` covers
      classes/factions/objects/mobs/rooms Tobin doesn't have, so neither
      command attempts that. Note: `set` (exact 3 letters) had to be placed
      BEFORE `setsev` in `cmd_table.c` -- both start with "set", first match
      wins, and `set` needs to win that exact typo/abbreviation. Refactored
      the attribute-name-to-field lookup (`attr_field`, previously `static`
      in `descriptor.c`) into a public `attrs_field()` in `being.c`/`being.h`
      so `edplayer`, character creation, and `set` all share one copy.
      `smoke_test_set.py` (gate, validation, every field, persistence via
      reconnect, online live-sync) + help topic.

### User batch 2026-07-05 (late night, follow-ups #2) — working these next

- [x] **Idle disconnect: immortals immune** — done 2026-07-05 (built, pending
      deploy): "do both" per user. Added `descriptor_idle_timeout` pulse (60s)
      that disconnects playing MORTALS idle > IDLE_DISCONNECT_SECS (30 min)
      with a message; immortals never idle-dropped. Also made the keepalive
      NOP more aggressive (30s -> 12s) to survive tight NAT/router windows.
- [x] **Typed logs (LOG_GAME + personalized)** — done 2026-07-05 (built,
      pending deploy): `log_type_t` enum in log.h (LOG_SILENT, LOG_GAME
      generic, LOG_PIO, LOG_COMBAT, LOG_BUG, LOG_DB, LOG_EDIT, LOG_JESUS) +
      `log_type_name`/`log_type_personal_name`. New `game_log(type, fmt, ...)`
      (in descriptor.c): writes the file line tagged with the type and echoes
      a cyan `<c>[TYPE]<z>` line to non-editing immortals -- except LOG_SILENT
      (file only) and personalized types (LOG_JESUS -> only the immortal named
      Jesus). The link-loss broadcast now goes through `game_log(LOG_PIO,...)`
      (tag `[PIO]` instead of `[LOG]`; notify test updated). Other events
      (quit/delete/bug) can adopt game_log with their type as they land.
- [x] **Title `<N>` substitution** — done 2026-07-05 (built, pending deploy):
      `title_with_name()` in cmd_who.c replaces every `<N>`/`<n>` token with
      the character's name anywhere in the title; when present the title shows
      alone (name embedded), else `Name title` as before. Other color tags
      pass through untouched.
- [x] **Abbreviation → closest command (incl. socials)** — done 2026-07-05
      (built, pending deploy): `social_try` now prefix-matches like the command
      table (first match wins), so `poi`/`poin` -> `point`. Commands are still
      tried before socials, so a real command always wins.
- [x] **`exec` (level 60 only)** — done 2026-07-05: `cmd_exec.c` runs host
      shell commands (Implementor-only). Fenced 3 ways: a blocklist refuses
      dangerous commands (rm/kill/reboot/mkfs/dd/sudo/tobin_c/mariadb/etc.),
      every command runs under `timeout 10` so it can't freeze the game loop,
      and each use is logged (EXEC:/EXEC REFUSED:). Output capped at 8KB.
      `smoke_test_exec.py`.
- [x] **Help format: colorized Syntax/Minimum Level** — done 2026-07-05
      (built, pending deploy): `help <cmd>` now shows the description in
      magenta `<m>...<z>`, then `<c>       Syntax:<z> <syntax>` and
      `<c>Minimum Level:<z> <n>` (right-aligned cyan labels). Syntax is parsed
      out of the body's leading `Usage:` line (fallback: command name); level
      from the command table. Prose topics get just the magenta body.
- [x] **`flee`** — done 2026-07-05 (built, pending deploy): `cmd_flee.c` --
      while fighting, ~2/3 chance to bolt through a random real exit; on
      success both sides stop fighting and you move to a neighbouring room, on
      failure you stay locked in. `smoke_test_flee.py`, help topic.
- [x] **`toggle`** — done 2026-07-05: `cmd_toggle.c` with an extensible
      TOGGLES[] table. Bare `toggle` lists switches + values; `toggle <name>`
      (abbrev ok) flips one. Player toggles (color, hp) affect only you; game
      toggles (multiplay) are hidden from and locked to <55 and flippable by
      55+. New features add a row. `smoke_test_toggle.py`.
- [~] **Colorize displays tastefully (ongoing habit)** — standing guideline
      (saved to memory: tobin-colorize-habit). Applied to new output (toggle,
      flee, exec, help format) using lowercase dim codes; keep doing it.

  NOTE: added `<m>`/`<M>` as magenta aliases for `<p>`/`<P>` in colorstring.c
  (the user's help format uses `<m>`), and made `help <cmd>` render its body
  in magenta with cyan Syntax/Minimum Level labels.

### User batch 2026-07-05 (evening) — working these first

- [x] **wizhelp: usable-only + reformat** — done 2026-07-05: wizhelp already
      filtered to usable commands; removed the `[NN+]` level tag and made it a
      three-column alphabetical list of command names.
- [x] **help / wizhelp in three columns, alphabetical** — done 2026-07-05:
      shared `send_columns()` in cmd_help.c (qsort + 3-col); names only,
      `help <cmd>` for details.
- [x] **Prompt newline** — done 2026-07-05: the game-loop prompt is now
      `\r\n\r\n> ` (a blank line before each prompt). NOTE: this yields two
      blank lines when the preceding output ends in a newline -- confirm one
      is not enough if it looks like too much whitespace.
- [x] **Keepalive** — done 2026-07-05: `descriptor_keepalive` pulse (main.c,
      ~30s) sends an IAC NOP to every connection so idle players aren't dropped
      by NAT/router timeouts. Verified live (NOP received); not in the sweep
      (a 30s timer would slow it).
- [x] **`wiznet`** — done 2026-07-05: immortal-only broadcast to all online
      immortals (`cmd_wiznet.c`). `smoke_test_wizcomm.py`.
- [x] **`system`** — done 2026-07-05: immortal-only global echo -- sender sees
      `system <msg>`, everyone else the bare `<msg>` (`cmd_system.c`).
- [ ] **Socials → DB + full Sneezy set + `edsocial` (55+)** — move socials
      from the compiled table to a DB table; port the full social set from
      `sneezymud-master/lib/actions`; add `edsocial` (55+, menu-driven ed*
      editor) to edit them in game.

### User batch 2026-07-05 (late) — working these next

- [x] **Lose the `[ wiznet ]` prefix** — done 2026-07-05: wiznet shows just
      `<Name>: <msg>` in purple.
- [x] **`mudstats`** — done 2026-07-05: `cmd_mudstats.c` reports room/mob/obj
      counts from the DB. `smoke_test_mudstats.py`.
- [x] **Idle flag** — done 2026-07-05: `descriptor.last_active` (set on each
      input); `who` shows `(idle)` after 5 min, any command clears it.
      `smoke_test_idle.py` (active-not-flagged; the 5-min appearance is
      logic/manual-verified, too slow for the sweep).
- [x] **Log quit/deletes** — quit + link-drop already logged; character delete
      now logs too (2026-07-05). Account-delete logging lands with `wipe`
      (there's no account-delete flow yet).
- [x] **Daily log files + 21-day retention** — done 2026-07-05: one
      `<YYYY-MM-DD>.log` per day, appended across reboots/copyovers; `*.log`
      older than 21 days pruned (by mtime) at each open; `log rotate` now just
      re-opens the day's file. `smoke_test_logs.py` updated.
- [ ] **`wipe` (59+)** — wipe a pfile or an account; requires a password to
      execute; only *lower*-level characters may be targeted (a 59 cannot wipe
      another 59, etc.). Destructive -- confirm + password gate.
- [x] **`;` wiznet shorthand** — done 2026-07-05: `;<msg>` broadcasts to
      immortals (cmd_dispatch special-case, like `'` for say).
- [ ] **`alias` command** — players define their own aliases, stored on the
      ACCOUNT and shared across that account's characters. Scoped by tier: an
      immortal's aliases apply only to their immortal characters; a mortal's
      apply to all mortal characters on the account. Needs a DB table
      (account_id, tier, name, expansion), an `alias` command (add/list/remove),
      and alias expansion in cmd_dispatch before command matching.
- [x] **Immortal color tiers in who/score** — done 2026-07-05:
      `being_rank_color()` (51-53 `<c>`, 54-56 `<C>`, 57-58 `<p>`, 59+ `<P>`)
      tints the name in who and score.
- [x] **`goto <char>`** — done 2026-07-05: goto now accepts a player name and
      teleports to that online being's room (mobs too, once they exist).
- [x] **`help edit`** — done 2026-07-05: dynamic index of `ed*` commands the
      caller can use (auto-updates as editors are added), pointing to each
      one's `help <name>`.
- [x] **Multiplay control** — done 2026-07-05: `multiplay <on|off>` (59+)
      game flag persisted in `game_config`; enter_world refuses a mortal
      account's second connected character when off; immortals exempt.
      `smoke_test_multiplay.py`.
- [ ] **Holdable items + `point` social** (BLOCKED on Objects/2C) — players
      grab/hold items in hand (primary, then secondary); a `point` social
      shows "X points at you with his/her/its <primary-hand item>". Needs the
      object system. (Basic no-arg `point` -> "You point around randomly." is
      buildable now, in the night batch below.)

### User batch 2026-07-05 (night, follow-ups) — working these now

- [x] **who/score: color the bracket, not the name** — done 2026-07-05: the
      rank-tier color now wraps the `[ Implementor ]` bracket in who and the
      `Level:` field in score; the name is uncolored. `smoke_test_level_titles`
      updated to strip ANSI for format checks + assert the bracket (not the
      name) is colored.
- [x] **Help footer: Usage + Level** — done 2026-07-05: viewing `help <cmd>`
      auto-appends `Usage: <name>` and `Level: <min_level>` from the command
      table (cmd_help.c); prose topics with no command entry get no footer.
- [x] **Colored [LOG] tag** — done 2026-07-05: the immortal link-loss
      broadcast now carries a cyan `<c>[LOG]<z>` tag (verified live). Future
      log broadcasts (quit/delete display) route through the same single tag;
      a per-category taxonomy stays with the Typed-logs item.

  NOTE: also fixed `smoke_test_immortal_cmds` — `goto <non-numeric>` is now a
  player lookup ("No one named..."), so the stale "Usage: goto" expectation
  was updated (bare `goto` still shows usage).

### User batch 2026-07-05 (night) — buildable now

- [x] **Player titles + who args** — done 2026-07-05: `title <text>` sets a
      free-form title shown after the name in `who` (`title none` clears it),
      persisted in `player.title` (already an upstream column; `being.title`
      + `player_set_title()` load/save). `who` now takes an argument:
      `who imm[ortals]` / `who mort[als]` scope by rank, any other word is a
      case-insensitive name-substring filter, empty result prints a "No one
      matching" line. `cmd_title.c`, help topics for `title`/`who`,
      `smoke_test_title.py`.
- [x] **Gender + pronouns** — done 2026-07-05 (built, pending deploy): pick
      `gender male|female|neuter` on the creation screen (default neuter),
      stored in `player.gender` (migration). `gender_t` + `gender_name` /
      `gender_subject` (he/she/it) / `gender_object` (him/her/it) /
      `gender_possess` (his/her/its) helpers in being.c; shown on the score
      sheet. Socials/combat can adopt the pronoun helpers as their messages
      grow. `smoke_test_gender.py`.
- [x] **Appearance** — done 2026-07-05 (built, pending deploy): set with
      `appearance <text>` on the creation screen, stored in
      `player.appearance` (migration, varchar(255)). Shown on your own score
      sheet and to others via `look <player>` (a neuter/no-appearance target
      gives a gender-aware "nothing special about him/her/it"). Full `examine`
      stays with the objects batch.
- [x] **Color preference at account creation** — done 2026-07-05: a
      `CONN_GET_COLOR_PREF` step asks on/off during account creation and
      persists it in `account.color_pref` (migration + `account_set_color`);
      the login handshake is backward-compatible (only exact yes/no/blank is
      treated as the answer, else defaults ON and re-dispatches the line).
- [x] **`rules` + `edrules` (59+)** — done 2026-07-05: DB-backed numbered rules
      (like news/help). `rules` lists them, `rules <n>` shows rule n's body in
      magenta, `edrules <n> <title>` (59+) writes one through the shared line
      editor (`EDIT_RULES`). `rules` table + `rules_repo.c`, help topics,
      `smoke_test_rules.py`. Deployed + verified (8/8).
- [x] **Color/name tag help** — done 2026-07-05: `help colors` lists every
      `<x>` color tag with examples.
- [x] **`bug` + `delbug` (59+)** — done 2026-07-05: `bug <text>` files a report
      (stored with submitter + date, echoed to immortals as a typed `[BUG]`
      log); bare `bug` lists reports for immortals (usage for mortals);
      `delbug <id>` (59+) removes one. `bug` table + `bug_repo.c`, help topics,
      `smoke_test_bug.py`.
- [x] **Newbie channel + flag** — done 2026-07-05: `newbie <msg>` is a help
      channel reaching everyone with the `PLR_NEWBIE` flag (new `player.pflags`
      bitmask, default on so newcomers start on it). Toggle off/on with
      `toggle newbie` (persisted); you must be on the channel to speak.
      `cmd_newbie.c`, help topic, `smoke_test_newbie.py`.
- [x] **`point` (no arg)** — done 2026-07-05: basic `point` social ("You point
      around randomly." / "You point at X."). The held-item form ("...with his
      <item>") is objects-blocked above.
- [ ] **`wipe` master password** — the pending `wipe` command's password is a
      compile-time master password (settable in code).

### User batch 2026-07-05 (night) — BLOCKED on Objects (Phase 2C)

- [ ] **Money system** — gold-coin currency + commodities (ingots, nuggets,
      shards of gold/silver/obsidian/...). Repurpose Sneezy's talens/components
      for inspiration. Future: mobs drop them (economy), used in skills
      (repair, spell/prayer fuel). Needs objects.
- [ ] **Liquids** — drinkable liquids; pouring one out pools on the ground
      (from Sneezy). Needs objects/containers.
- [ ] **`fill`** — fill a container from a liquid pool. Needs liquids+objects.
- [ ] **`switch`** — swap primary/secondary held items. Needs holdable items.
- [ ] **`examine`** — look closer at things (extra descriptions). Needs room/
      object extra descriptions (partly objects, partly redit extra-desc item).

### User batch 2026-07-05 (night) — BLOCKED on Classes

- [ ] **Druid class** — add druid to the selectable classes (lands with the
      Classes system).


      the regen tick (weight by position, like HP already does). New
      `player_progress` column; show in score/prompt. Take from Sneezy.
- [ ] **Terrain movement cost** — each sector type modifies the vitality cost
      of moving into it (original `TerrainInfo`). Depends on Vitality.
- [x] **Socials/actions** — done 2026-07-05: 15 socials (smile/nod/wave/bow/
      cheer/poke/...) in `socials.c`, checked in dispatch after the command
      table; untargeted + targeted forms; `socials` lists them. Room echoes
      go through `descriptor_notify` (held for editors). More can be added to
      the table; a DB-backed/editable social set (`edsocial`?) is future work.
- [x] **Health strings** — done 2026-07-05: `being_health_word()` maps HP%
      to a word (near death ... perfect); shown in `score`'s HP line.
      Optional follow-up: also show it in the prompt (prompt-flag system).
- [ ] **PK opt-in flag** — player flag; BOTH players must have opted in for
      attack/kill between players. Toggle command + persistence + combat gate.
- [ ] **Tips system** — `tips` command + periodic tip echoes (pulse-driven),
      per-player newbie toggle, `tipedit` (53+). DB-backed like news/help.
- [ ] **Typed logs** — `log.h` log-type taxonomy; every log line gets a type,
      `log search` can filter by type.
- [ ] **`dig`** — builder-walk: moving into a nonexistent exit auto-creates
      the room + reverse exit (redit's exit machinery already does this).
      Needs a next-free-vnum strategy.
- [x] **`edplayer`** (player files) — done 2026-07-06: menu-driven editor
      (58+, matching `promote`'s tier) for level, experience, HP/max HP,
      attributes, gender, title, load room, and handedness -- an admin
      superset of `promote`. Works on any player by exact name, online or
      offline (`player_load_admin()`, not account-scoped). Unlike `edroom`
      the working copy is a DB snapshot, not a live pointer (players
      aren't kept resident like rooms are) -- (S)ave writes it back to the
      DB and, if the target happens to be online right now, syncs their
      live `being_t` too (no relog needed), matching `promote`'s own
      online-target courtesy. New `player_load_admin()` / `player_set_
      gender_by_name()` / `player_set_handed_by_name()` / `player_set_
      appearance_by_name()` in `player_repo.c`. `smoke_test_edplayer.py`
      (gate, every field, save-persists via reconnect, live sync to an
      already-connected session, and discard-truly-discards) + help topic.
- [ ] **`edaccount`** (accounts) — menu-driven: rename, password reset, list chars.
- [x] **wiznews** — done 2026-07-05: an immortal-only (51+) news channel like
      `news`; `edwiznews` posts items that concern immortals. Parallel to
      news/ednews.
- [x] **ed* rename** — done 2026-07-05: redit→edroom, hedit→edhelp,
      addnews→ednews (command names, help topics, tests, editor prompts).
- [ ] **Diseases** — modest list affecting players (immortals immune);
      pulse-driven affect/tick, cure path TBD (`disease.h` for inspiration).
- [ ] **News follow-ups** — edit/delete existing news in-game (addnews only
      creates); show unseen news at login (per-player last-seen).
- [ ] **redit Extra Descriptions** — keyword extra descs (`roomextra` table
      exists): list/add/edit/delete + delete-all (Sneezy redit items 6 & 10).
- [x] **Door mechanics** — done 2026-07-06: `open`/`close <direction>`
      (`cmd_open.c`), movement blocking on a closed door (`cmd_move.c`:
      "The door is closed."), and secret exits hidden from `look`'s
      Obvious-exits line and `exits` (still walkable if you know the
      direction). New `EXIT_COND_CLOSED`/`_LOCKED`/`_SECRET` bit constants
      in `room.h`. Door/condition state is per-exit, NOT mirrored to the
      reverse exit -- matches how `edroom`'s own auto-created reverse
      exits already work (independent door state per direction), not an
      oversight. `open` refuses a Locked door; unlock/lock commands are
      still deferred (need a key, which needs objects). `smoke_test_
      doors.py` + 3 new help topics (`open`, `close`, updated `exits`).
- [x] **Positions polish** — done (Session 43): a defender who isn't
      standing (sitting/resting/sleeping/any lower rung) takes a flat
      +15 hit-roll bonus against them in `combat_strike()` -- attacking
      only auto-stands the ATTACKER (cmd_attack.c), so this stays in
      effect for as long as the defender chooses to stay down.
- [ ] **Personalized immortal log messages (57+)** — per-immortal flavor on
      log lines (`log.h` LOG_JESUS/LOG_PEEL/LOG_LOW inspiration).
- [x] **`<d>` bold color tag** — done (Session 43, user: "investigate <d>
      and $$g tags from sneezy and implement in tobin"). Sneezy's `<d>`/
      `<D>` is a standalone BOLD toggle (`\033[1m`), distinct from the
      existing R/G/B/... tags (which already bundle bold into their own
      bright/uppercase variant) -- `<d>` stacks bold onto whatever color
      is already active, e.g. `<g><d>bold green<z>`. One-line addition to
      `colorstring.c`'s tag table.
- [x] **`$$g`/`$g` ground-surface token** — done (Session 43, same user
      request). Sneezy's `misc/show.cc` token, substituted in an object's
      description with the room's ground-surface word (`describeGroundType()`,
      misc/create_rooms.cc): "street" (city), "road", "water" (ocean/river),
      "mud" (swamp), "sand" (beach), "floor" (indoors flag), else "ground".
      New `room_ground_type()` (room.h/room.c, same sector-substring-
      bucketing style as `sector_color()`) + `obj_apply_ground_token()`
      (obj.h/obj.c), wired into both `long_descr` display sites in
      cmd_look.c (`look <object>` and the room-floor listing). Dropped the
      original's weather-prefix component ("snow-covered ground", "rain-
      slick street") -- Tobin has no weather system yet. Not present
      anywhere in the currently-migrated obj/objextra data (verified before
      building -- zero real usages), so this is forward-looking
      infrastructure for future hand-authored descriptions, not activating
      existing content.
- [x] **Time/day/date system** — done (Session 43, user: "implement
      time/day/date system from sneezys example"). Ported from Sneezy's
      `GameTime` class (sys/gametime.{h,cc}): 28-day months, 12-month
      years, the same weekday formula `(28*month + day + 1) % 7`, the same
      noon/midnight/new-month/new-year world announcements. New
      `gametime.h`/`gametime.c` + `time` command. Session-only (starts
      fresh at boot, no persisted game-time table). Ticks on a pulse
      (~60s, the same cadence `zone_process_run()` already established)
      advancing 15 mud-minutes per tick, rather than the original's real-
      seconds-per-mud-hour formula. Dropped: the weather-driven sunrise/
      sunset/moon tracking (no weather system) and the personal real-
      time-zone-offset sub-feature of Sneezy's `time <difference>` (a
      separate feature, not part of the day/date system itself). Found
      and fixed a related latent bug while adding this: `pulse_register()`
      silently no-op'd past `MAX_PULSE_PROCESSES` (was 8, exactly filled
      by this addition) -- bumped to 16 and made the overflow case log an
      error instead of vanishing silently.
- [x] **Personal time-zone offset** — done (Session 43 continued, user:
      "is the time based upon time zones? if so, make the mud EST" then
      "in account creation, ask the character to choose a time zone based
      on machine time zone, so for PST set timezone -3, etc"). The mud
      clock itself is fictional (28-day months, pulse-driven) and has no
      real-world timezone; confirmed the VM/MariaDB *are* both already
      America/New_York (EST/EDT), so nothing needed changing there. This
      is the separate real-time-offset sub-feature explicitly deferred
      when the gametime system was added above, now ported from Sneezy's
      `CON_TIME` prompt (sys/connect.cc) and `time <difference>`
      (misc/info.cc doTime()): a new `CONN_GET_TIMEZONE` account-creation
      state (right after the color prompt) asks the offset in hours from
      the server's Eastern clock (e.g. Pacific enters -3), range -23..23,
      blank = 0; persisted to `account.time_adjust` (a pre-existing,
      previously-unused column from the original schema -- no migration
      needed). `time` (bare) now shows a second line, the real-world clock
      shifted by that offset; `time <difference>` re-sets it later. New
      `account_set_timezone()` (account.h/account_repo.c), new
      `tests/smoke_test_timezone.py` (9 checks, including that shifting
      the offset by 2 hours shifts the shown real time by exactly 2
      hours).
- [x] **Pager held-messages + colorized MORE prompt** — done (Session 43
      continued, user: "silence all messaging like youve done for the
      editors, but for pagination. also colorize the [ ENTER for more, Q
      to stop ] line like my example"). `descriptor_in_editor()`
      (descriptor.c) now also returns true while `page_len > 0` (mid-
      pager, e.g. reading `news`), so `descriptor_notify()` holds
      messages for catchup instead of interrupting a paginated read --
      same mechanism as the editors, one extra condition. Since `news` is
      mortal-accessible, `catchup` was widened from immortal-only to
      mortal-level (cmd_table.c) -- otherwise a mortal held mid-pager
      would have no command to retrieve it with. The MORE prompt itself
      is now colorized and on its own line: `\r\n<c>[ <C>ENTER<c> for
      more, <C>Q<c> to stop ]<z>` (was a plain, uncolored trailing
      fragment). New `tests/smoke_test_pager_held.py` (5 checks).
- [x] **Fixed: regular-intensity color tags didn't clear a preceding
      bold** — done (Session 43 continued, user: "colorized pagination is
      incorrect. the intention was to highlight the available
      command/keys in bright. the rest regular"). Root cause in
      `colorstring.c`'s `ansi_for_tag()`: lowercase tags emitted a bare
      `\033[36m`-style code with no intensity reset, and SGR bold (`1`)
      and color are independent parameters that most terminals leave
      stuck on until explicitly cleared -- so `<C>ENTER<c>` (bright, then
      regular) rendered everything bright, since the plain `<c>` never
      actually turned bold off. Every lowercase color tag now leads with
      `0;` (`\033[0;36m`), forcing a full attribute reset before applying
      the color, so a regular tag really is regular regardless of what
      came before. This is a general color-engine fix, not pager-
      specific -- every `<x>` tag in the game benefits. Updated the
      hardcoded expected byte sequences in six existing smoke tests
      (`smoke_test_color.py`, `smoke_test_help_format.py`,
      `smoke_test_notify.py`, `smoke_test_say.py`,
      `smoke_test_sector_color.py`, `smoke_test_pager_held.py`) to match
      the new `0;`-prefixed codes; all still pass except
      `smoke_test_sector_color.py`, which failed for an unrelated reason
      (a stale hardcoded sector expectation for room vnum 100, flagged
      separately, not a regression from this fix).
- [x] **Three `look` bugs, found and fixed together** — done (Session 43
      continued, user reported all three against real seeded mob vnum
      33271, "a dirty refuse hauler"):
      1. **Capitalization sometimes ignored** ("A lamppost is here." /
         "a dirty refuse hauler is here." -- inconsistent). Root cause:
         this mob's `short_desc` is authored with a leading inline color
         tag (`<o>a dirty refuse hauler<1>`), and `cap_first()`
         (cmd_look.c) blindly uppercased byte 0 -- which was `<`, a
         no-op, leaving the real letter untouched. Fixed to skip any
         leading `<X>` tag(s) before capitalizing.
      2. **Wrong name in `look <mob>`** ("You look at man dirty refuse
         hauler." should read "You look at a dirty refuse hauler.").
         Root cause: `look_at_target()` displayed `thing_t.name` (the
         raw keyword-match list, e.g. "man dirty refuse hauler") instead
         of `short_descr` for mobs -- a PC's `name` IS its proper name,
         but a mob's `name` is just matching keywords. Fixed to use
         `short_descr` for mobs (uncapitalized -- it's mid-sentence
         here, not a cap_first() site).
      3. **Truncated long description** ("increase the buffer size so i
         can read the entire string"). Root cause: `BEING_APPEARANCE_LEN`
         was 256, sized for `player.appearance`'s real varchar(255)
         column, but shared with `mob.description` (mediumtext, real
         seeded max ~1200 chars) -- silently cut off mid-sentence on
         load (mob_repo.c's snprintf). Bumped to 2048; PC-authored
         appearance text is unaffected (MariaDB itself truncates on the
         rare overflow past the real 255-char column, same as any other
         varchar overflow -- previously this buffer coincidentally
         enforced that limit earlier, now the DB does). Also bumped two
         downstream buffers (`show_attr_screen` in descriptor.c,
         `look_at_target`'s `out` in cmd_look.c) that hit
         `-Wformat-truncation` once the source could be much longer.
      New `tests/smoke_test_look_capitalization.py` (6 checks, all
      against the real vnum 33271 mob rather than synthetic fixtures,
      since all three bugs only manifest on real authored content).
- [x] **`scan` ignores linkdead characters** — done (Session 43
      continued, user: "scan should ignore linkdead chars").
      `cmd_scan.c`'s room-occupant loop had no linkdead check at all
      (unlike the room-floor `look` listing, which shows a linkdead PC
      tagged "(linkdead)" but still visible, and combat's
      `combat_find_room_target()`, which already excludes them from
      being attacked) -- scan simply never checked. Fixed to skip a PC
      with no live `desc`, same "not a real target" treatment as combat.
      Extended `tests/smoke_test_scan.py` with a 6th check (abrupt
      disconnect -> still-present-but-unscannable).
- [ ] **Make `smoke_test_limbs.py`/`smoke_test_limbs_cmd.py`
      deterministic** — found while chasing an unexpected sweep failure
      (Session 43 continued): both rely on real combat RNG to eventually
      cross a limb status tier within a fixed number of rounds, but with
      `LIMB_MIN_MAX_HP` (15) and damage landing on a random one of 13
      limbs each hit, reliably crossing a tier in the test's round
      budget is statistically marginal -- confirmed via `hurtlimb` that
      the underlying mechanism itself is fine, this is a test-design
      gap. `hurtlimb <target> <limb> <hp>` (cmd_hurtlimb.c, added
      Session 42 for exactly this) already lets a test set a limb's HP
      directly instead of waiting on the dice -- migrate both tests to
      use it instead of a real fight.
- [x] **Get/drop item logging for dispute research** — done (Session 43
      continued, user: "anytime a char gets an item or drops an item i
      want those logged into the game log so we can research disputes
      with log search. these should not be reported via any log type,
      just inserted into the game log"). `LOG_SILENT` (log.h) already
      existed for exactly this -- recorded to the file, never echoed to
      online immortals -- so this needed no new mechanism, just call
      sites: `cmd_object.c`'s `cmd_get()` (both the plain and the
      get-from-container branches) and `cmd_drop()` each now call
      `game_log(LOG_SILENT, "%s gets/drops %s (vnum %d) in room %d", ...)`.
      Verified reachable via `log search` and confirmed a same-room
      online immortal sees nothing from it (the ordinary room-broadcast
      message is separate and still fires normally). Found and fixed a
      duplicated-helper bug while working nearby: `cap_first()`'s
      leading-inline-color-tag fix (from the earlier `look` bugfix) only
      ever landed in cmd_look.c's own copy -- cmd_scan.c and
      cmd_object.c each have their own independent copy of the same
      function, still with the old bug. Fixed both. New
      `tests/smoke_test_getdrop_log.py` (6 checks).
- [ ] **Mob AI: wandering + mob actions** — implemented locally, not yet
      deployed/tested (sweep from an earlier batch this session still
      running, deploy queued right behind it). User: "in pulse, make sure
      that mob actions click and mobs that can wander will do so, look at
      mob ai from sneezy". New `mob.actions` field wired all the way
      through: `mob_proto_t`/`mob_proto_load()` (mob_repo.h/mob_repo.c)
      now loads it, `being_t.mob_actions` (being.h) carries it onto the
      in-world instance (being_create_mob(), being.c). New
      `mob_ai_tick()` (mob_ai.h/mob_ai.c), pulse-registered (main.c) at
      the same ~60s cadence as gametime_tick()/zone_process_run(): a mob
      without `ACT_SENTINEL` (bit 1, value 2 -- confirmed against the
      bundled sneezymud-master reference tree's misc/defs.h), not
      fighting, standing, has a 20%-per-tick chance to walk a random
      valid exit (skips closed doors and ROOM_FLAG_NO_MOB destinations).
      New `world_for_each_mob()` (world.h/world.c) walks every registered
      room's mob list, same pattern as the `purge linkdead` sweep above.
      Simplified vs. the original's mobact.cc: no ACT_STAY_ZONE
      zone-boundary restriction yet (no direct room-to-zone lookup wired
      up for this), no terrain/water/flying/riding/secret-door checks
      (none of those subsystems exist for mobs). Testing a 20%-per-~60s-
      real-tick chance is impractical to wait on in a smoke test (same
      problem as the heartbeat tick), so new immortal-only debug command
      `aitick [count]` (cmd_aitick.c, same precedent as `hurtlimb`) forces
      N ticks synchronously -- `aitick 30` gives ~99.9% odds of firing.
      New `tests/smoke_test_mob_ai.py`.
- [x] **Cleaner mobs clean up randomly** — done, bundled into the mob AI
      item above rather than a separate pulse, per the original plan.
      User: "i want cleaner mobs to clean up randomly, i believe this is
      also in mob ai". `ACT_SCAVENGER` (bit 2, value 4) is checked in the
      same `mob_ai_tick()`: a 25%-per-tick chance to pick up and destroy
      one random loose `OBJ_CAT_TRASH` item in the mob's room. Scoped down
      from the original's ACT_SCAVENGER (picks up ANY loose object,
      including real loot) to trash specifically, matching the user's
      "clean up" framing rather than risking a cleaner mob eating dropped
      gear or a corpse's contents.
- [x] **Weapon-aware combat messaging + hit/dam bonuses** — done --
      deployed and verified via standalone smoke test (also caught
      and fixed a real off-by-one bug in this test's own SQL fixture, and
      discovered `attack`/`kill` instant-slay for immortals via cmd_kill.c,
      which required restructuring the test to attack with a mortal
      character instead) and a clean full sweep. User: "when in combat wielded
      items should modify messaging for example wield sword, you slice
      instead of hit. This should apply to all weapon types and add or
      subtract any hit bonuses placed on the weapon". `combat_wielded_weapon()`
      picks the dominant hand's weapon (falling back to off-hand),
      `weapon_verb()` keyword-buckets its name/short_descr into
      slice/chop/bludgeon/stab/pierce/lash/hit (combat.c, same style as
      `sector_color()`/`room_ground_type()` in room.c). Turns out the
      `objaffect` table (vnum, type, mod1, mod2) already exists in the
      live DB with real seeded data -- confirmed its `type` column against
      the bundled original SneezyMUD source (sneezymud-master/code/code/
      misc/enum.h's `applyTypeT`): 15=APPLY_HITROLL, 16=APPLY_DAMROLL,
      17=APPLY_HITNDAM (both at once); every other type (stat/AC/immunity
      bonuses) is irrelevant here. New `obj_load_combat_mods()`
      (obj_repo.h/obj_repo.c) sums those three types for a vnum;
      `combat_strike()` applies the result to hit_roll/dmg for whichever
      weapon is actually wielded (0/0 for bare hands, a no-op extension of
      the old formula).
- [x] **Persist the game clock across boots** — done (Session 43
      continued, user: "make time save so it continues on from boot to
      boot"). Reused the exact `game_config` key/value pattern
      multiplay.c already established: new `gametime_load()`
      (gametime.c, called from main.c right after `multiplay_load()`)
      restores hour/minute/day/month/year from five `game_config` rows;
      `gametime_tick()` now calls a new `gametime_save()` before every
      return path (there are several, one per rollover stage), so a
      crash or unclean restart never loses more than one tick (~60s) of
      progress. Caught a real bug while writing this: `db_query()`'s
      custom format parser only recognizes `%i` for integers, not
      libc's `%d` -- using `%d` would have silently failed every save
      (falls through to the "bad format specifier" error path). Verified
      end-to-end: set the clock via a real tick, restarted the live
      server, confirmed `time` resumed at the persisted value instead of
      resetting to the 8:00 AM default. New
      `tests/smoke_test_gametime_persist.py` (3 checks, verifies the
      `game_config` row matches `time`'s live output rather than
      requiring an actual server restart mid-test).
- [x] **Half-hour real-time tick (blank line, no message)** — done
      (Session 43 continued, user: "every hour on the half hour send a
      blank line of uinput to the game so a tick becomes apparent to the
      player without any messages"). New `heartbeat.h`/`heartbeat.c`,
      `heartbeat_tick()` registered alongside `gametime_tick()` (main.c,
      same ~60s pulse cadence). Real wall-clock time (`time(NULL)`, NOT
      the fictional mud clock) bucketed into hour-sized windows shifted
      back 30 minutes so the boundary lands on the half hour instead of
      the top of the hour; a static last-fired bucket guards against
      re-firing every pulse within the same window. Sends a bare "\r\n"
      via `descriptor_notify()` (held for anyone mid-editor/pager, same
      as any other broadcast). Verified live with a temporarily
      shortened bucket window (15s instead of 3600s) and faster pulse
      interval: confirmed the blank line actually arrives and does NOT
      re-fire every pulse, then reverted both back to the real values
      before redeploying. New `tests/smoke_test_heartbeat.py` -- the
      real hourly boundary isn't practical to wait for in an automated
      sweep, so this only sanity-checks that a short window doesn't
      flood blank-only bursts; full firing behavior was verified
      manually as above.
- [x] **Mobile_Attitude (mob AI emotional/opinion system)** — done --
      deployed and verified via standalone smoke test + a clean full
      sweep (81 passed, 2 known flakes). User: "class Mobile_Attitude
      in sneezy should be implemented
      into tobin. mobs should react to good vs evil and react
      accordingly". The full original (`sneezymud-master/docs/systems/
      critical/14-monster-ai-behavior.md`, source in misc/monster.cc/.h,
      misc/mobact.cc, misc/opinion.cc) models four 0-100 emotional
      attributes per mob (suspicion/greed/malice/anger, not literally
      "good/evil"), hate/fear opinion bitfields keyed by sex/race/
      individual-char/class/vnum, hunting/pathfinding, faction combat, and
      a full response-script system -- far beyond what Tobin's simplified
      mob model can support in one pass. Scoped down to the identified
      prerequisite (a PC alignment stat -- being.h had none) plus the one
      reaction the user actually described: new `progress_t.alignment`
      (-1000 evil .. +1000 good, 0 neutral default), persisted via a new
      `player_progress.alignment` column; `score` shows it as a word
      (`alignment_word()`, being.c, same bucketing style as
      `being_health_word()`); settable via `set <name> alignment <value>`
      (58+). `mob_ai_tick()` (mob_ai.c) now also reads `ACT_AGGRESSIVE`
      (bit 5, value 32): an aggressive mob picks a fight with a
      non-immortal PC in its room, UNLESS that PC's alignment is >= 350
      (the "good"/"saintly" tiers), mirroring the original's
      karma-vs-mob-disposition aggro() check at a much simpler scale. New
      `tests/smoke_test_alignment.py`.
- [x] **`idea` command (feature requests)** — done (Session 43
      continued, user: "add an idea command so a player can request new
      features, should work the same as reporting a bug also add an
      idea log message"). Direct mirror of the existing `bug`/`delbug`
      pair: `idea <text>` files one (stored with name + date), bare
      `idea` lists outstanding ones for immortals, `delidea <id>` (59+)
      removes a handled one. New `idea` table (db/sneezy/idea.sql,
      copies bug.sql's shape) + `idea_repo.{h,c}` (copies bug_repo's) +
      `cmd_idea.c` (copies cmd_bug.c's), new `LOG_IDEA` value in
      `log_type_t` (log.h, inserted before `LOG_TEST` so
      `LOG_SEVERITY_DEFAULT`'s derived bit width adjusts automatically)
      added to `cmd_setsev.c`'s toggle list. New
      `tests/smoke_test_idea.py` (9 checks, mirrors smoke_test_bug.py).
- [ ] **Drink/sip commands** — user: "add a drink/sip code from
      sneezymud and implement here". From-scratch, not a small addition:
      checked `obj.h` -- `OBJ_CAT_DRINK` exists as a category bucket but
      there's no liquid-type/capacity/current-amount modeling on obj
      instances at all yet, and `being_t` (being.h, checked) has no
      thirst/hunger stat either (the `nutrition` DB column referenced in
      player_repo.c's INSERT is vestigial -- never read or decremented
      anywhere). Needs, roughly: liquid type + capacity + fill-amount
      fields on drink-category objects (obj.h/obj_repo.h), a thirst (and
      maybe hunger, since Sneezy ties both together) stat on being_t,
      `drink`/`sip``/`fill``/`pour` commands (sip = small amount + no
      "full" message, matching the original's distinction), and messages
      for empty-container and over-full-from-drinking-too fast cases.
      Reasonable to scope drink/sip alone first and defer fill/pour.
- [x] **`purge` command (51+, with a 58+ `purge linkdead`)** — done --
      deployed and verified via standalone smoke test and a clean full
      sweep. User: "add a purge command that is
      51+ that will purge the contents of a room, add a linkdead argument
      that a 58+ god can purge the game of all linkdead characters".
      Scoped down from the original SneezyMUD's full purge (bundled
      reference tree, `lib/help/_immortal/purge`: also covers purging a
      single character/object and whole zones) to just the two requested
      forms. Turned out both open questions from the earlier note resolved
      cleanly: (1) bare `purge` (cmd_purge.c) clears mobs AND objects
      (never PCs -- the original's separate, unrequested "purge
      <character>" kick-from-game form is out of scope), matching the
      original help text's own description of the bare form. (2) `world.h`
      already secretly had everything needed for the game-wide linkdead
      sweep -- `world_find_linkdead_pc()` (used on reconnect) already
      walks a `g_rooms` registry of every active room; new
      `world_purge_linkdead()` (world.c) reuses that same walk, destroying
      every `THING_PC` with no live `desc`. Deliberately does NOT save
      first, matching `descriptor_destroy()`'s own documented reasoning
      for linkdead bodies (an eager save could clobber a fresher DB-side
      change) -- it's the same discard that already happens on that
      account's next reconnect or a plain restart, just triggered on
      demand. `purge linkdead`'s 58+ gate is checked inside `cmd_purge()`
      itself (the dispatch table only enforces one floor per command
      name; bare `purge` stays at 51+).
- [x] **`test` command (58+): show the currently-running smoke test** —
      done (Session 43 continued, user: "add a test command that will
      list whatever smoke test is currently running 58+"). The
      `@test <name>` / `@test done <name>` loopback-only hook
      (descriptor.c) already existed but was fire-and-forget (only
      `game_log(LOG_TEST, ...)`, a transient log line) -- added
      `log_test_set_running()`/`log_test_clear_running()`/
      `log_test_current_name()` (log.h/log.c) so the hook now also
      persists the name, and a new `test` command (TEST_MIN_LEVEL=58)
      just prints it ("No smoke test is currently running." if empty).
      Doesn't touch the hook's existing localhost-only security gate.
      New `tests/smoke_test_test_cmd.py` (3 checks) -- relies on
      sweep.sh running tests strictly sequentially so there's no other
      test's announcement to race with.
- [x] **Player classes (6): Mage, Cleric, Warrior, Thief, Druid, Monk** —
      done (user 2026-07-11: "implement classes, 6 player classes... the
      rest of the sneezy classes are for mobs only"). `player_class_t`
      (being.h/being.c), `class_name()`, `class_stat_bonus()` (fixed
      net-zero bonus/penalty on top of point-buy attrs), `class_hp_scale()`
      (feeds `being_calc_max_hp()`: Warrior 1.3, Monk 1.15, Cleric/Druid
      1.0, Thief 0.9, Mage 0.8). No SneezyMUD Druid exists (closest analog
      Shaman, not ported) -- designed fresh, same ±4 pattern as the other
      5. Chosen as a new required step in character creation
      (CONN_CHAR_CREATE_CLASS, descriptor.c) right after race. Persisted
      via `player.class` column; shown in `score` and abbreviated in `who`.
- [x] **Player races (6): Human, Elf, Ogre, Dwarf, Hobbit, Gnome** — done
      (user 2026-07-11: "implement races, 6 player races..."). Same shape
      as classes: `player_race_t`, `race_name()`, `race_stat_bonus()`.
      SneezyMUD's race table has no per-race stat bonuses to port, so this
      is an original design -- Human is a deliberate zero-modifier
      baseline. New required creation step (CONN_CHAR_CREATE_RACE) right
      after point-buy attrs, before class. Persisted via `player.race`;
      shown in `score`.
- [x] **Alignment choice at creation + mob alignment-based aggression** —
      done (user 2026-07-11: "ask player to choose initial alignment so
      good will attack evil and evil will attack good randomly... people
      who are neutral should be taunted by evil and supported by good").
      New CONN_CHAR_CREATE_ALIGNMENT creation step (Good/Neutral/Evil ->
      alignment 500/0/-500). New `mob.align` column (-1 evil, 0 unaligned
      [default, zero behavior change for every pre-existing mob], 1 good),
      loaded into `mob_proto_t`/`being_t.mob_align`. `mob_ai.c`'s
      `mob_try_aggress()` now branches on alignment: unaligned mobs keep
      the original behavior (attack anyone but the sufficiently good);
      aligned mobs only ever fight the opposite alignment, at the same
      25% per-tick chance. New `mob_try_align_flavor()`: a neutral PC
      sharing a room with an aligned aggressive mob gets a 15%-chance
      ambient one-liner instead of combat (good mob nods approvingly, evil
      mob sneers) -- no HP consequence either way.
      Bug caught during manual testing: `being_create_pc()` computed
      max_hp from default (pre-bonus, pre-class) attrs before
      `player_create()` applied the real race/class bonuses and class HP
      scale, so every new character's starting HP was wrong (stale
      calculation, only coincidentally correct for a default Mage/Mage
      match). Fixed by recomputing max_hp/hp and re-healing limbs right
      after race/class/alignment are finalized, before the first save.
      Ripple effect: every test file's character-creation helper answers
      "done" and expected to be playing immediately; now needs 3 more
      scripted responses (race/class/alignment) first. Swept all 94
      affected smoke test files (both the same-line
      `send_line(...,"done"); recv_all(...)` shape and the split
      send/recv-on-separate-lines shape) to insert
      `send_line(VAR,"1")`/`"1"`/`"2"` (Human/Mage/Neutral -- chosen as the
      least attribute-disruptive defaults) right after "done" drains,
      preserving each occurrence's own socket variable name. First
      mechanical pass over-matched and duplicated some insertions (the
      inserted "done" + recv_all pair still matched the same regex on a
      second pass) -- caught and collapsed before deploying. Re-verified
      end-to-end via `smoke_test_alignment.py` (all 16 checks pass) plus a
      handful of other creation-flow-touching tests.
- [x] **Bug: `random_visit_mob()` (trigger.c) didn't skip a leading color
      tag before capitalizing** — done. Same bug class as the
      already-fixed `cap_first()` helpers elsewhere (cmd_look.c etc.): a
      mob whose `short_descr` starts with a color tag (e.g. the seeded
      "dirty refuse hauler", `<o>a dirty refuse hauler<1>`) had its
      *tag's bracket* uppercased instead of the real first letter, so a
      `random`-trigger emote like "grumbles about the state of the
      streets..." rendered lowercase ("a dirty refuse hauler grumbles...")
      instead of "A dirty refuse hauler grumbles...". Fixed to skip
      `<...>` tags first, matching every other `cap_first()` copy.
- [x] **`/f` editor command wording cleanup** — the wiznews entry
      announcing the format-on-save reflow said "The /format command"
      when the actual (and only ever intended) syntax is the one-letter
      `/f`, alongside `/s`/`/a`/`/b` -- every editor (redit, addnews,
      edwiznews, hedit, rules, edtrigger) already consistently displays
      `/f`, so this was a wording-only fix, not a behavior change. Fixed
      the wiznews body text (plus the companion `UPDATE`, since
      `INSERT ... ON DUPLICATE KEY UPDATE title=title` is a no-op on an
      already-seeded row).
- [x] **Confirmed: room vnum/flags are already immortal-only (51+) in
      `look`** — user asked "make sure players level 50 and below only
      see room name and no vnums or flags"; verified `cmd_look.c` already
      gates the `[vnum] Name [sector] [flags]` builder header behind
      `being_is_immortal()` (>= level 51) and mortals only ever see the
      plain room name -- already correct, no change needed.
- [x] **`snoop`'s output mirror gains the same "% " marker its typed-
      command mirror already had** — done (user 2026-07-11: "add a
      special prompt to messages sent in snoop (%) snooped content").
      Before this, only the target's typed commands were prefixed "% ";
      their own output was mirrored completely unmarked (`descriptor.c`'s
      `d->snooped_by` raw `socket_write()`), indistinguishable from the
      snooper's own screen. Now every mirrored chunk, command or output
      alike, gets the same literal "% " prefix. Help topic + wiznews
      updated to match.
- [x] **`zonefile create <zone>`** — new builder tool (user 2026-07-11:
      "zonefile create should create a zone file with the current status
      of the zone and its contents, place an item in a chest, the
      zonefile creates the loading of that chest along with any contents
      in the chest. current placement of mobs etc. you should also be
      able to delete a line from the zone file, rerun zonefile create and
      it fills in the blanks of whats loaded into the zone"). New
      `zone_file_create()` (zone.c/zone.h): scans every room in the
      zone's [bottom,top] vnum range for its CURRENT live mobs and ground
      objects and appends new `zone_reset` rows (`M`/`O`, plus `E`/`G`/`P`
      for a mob's equipped/held/carried items and a G-carried or O-ground
      container's contents, one level deep) -- so the next boot/periodic
      reset recreates exactly what's there now. Idempotent by design: a
      (room, vnum) pair already covered by an EXISTING `M`/`O` row is left
      completely alone (no dupe, no re-touching its children), so
      deleting one row and re-running only fills in what that deletion
      left uncovered, never duplicating what survived. Documented
      limitation inherited from the execution engine itself (zone.c's
      `zone_execute()`): an equipped/held container's contents can't be
      captured, since the 'E' opcode never sets "last object" for a
      following 'P' to attach to -- only G-carried or O-ground containers
      support content capture. New `include/zone_repo.h`'s
      `zone_repo_insert_reset_cmd()` (plain append, caller picks
      `cmd_no`). New `src/cmd/cmd_zonefile.c`, registered right after
      `zone` in `cmd_table.c` (ordering matters -- see that file's
      comment: "zonefile" would otherwise shadow a bare "zone"
      abbreviation in the prefix-match dispatch loop). Same
      `zone_can_edit()` gate as `edzone`/`zone reset`. New help topic
      `zonefile`. New `tests/smoke_test_zonefile.py`: mob + ground chest +
      an item placed inside it, verifies the M/O/P rows land correctly,
      confirms a no-op re-run adds nothing, then deletes the mob's row
      and confirms only that gap gets refilled without touching the
      chest's already-covered rows.
- [x] **`bamfin`/`bamfout` moved to `goto`; the WALKING move-message
      feature they used to name is now `poofin`/`poofout`** — done (user
      2026-07-11: "bamfin|out should modify goto messaging and the
      current bamfin|out should be called something else following the
      in|out syntax"; follow-ups the same session: "<N> should work in
      this as well as $g"; "and $p"). `being_t.bamfin`/`bamfout` (the
      per-move custom-message fields) renamed to `poofin`/`poofout` --
      their ORIGINAL name, before an earlier same-session rename to
      "bamfin"/"bamfout" that this now supersedes -- with matching
      renames throughout: `cmd_bamf.c` -> `cmd_poof.c`
      (`cmd_poofin`/`cmd_poofout`), `player_set_bamfin/out` ->
      `player_set_poofin/out`, `cmd_move.c`'s `apply_bamf_tokens` ->
      `apply_poof_tokens`. DB migration adds `player.poofin`/`poofout`,
      copies over anything already stored in `player.bamfin`/`bamfout`
      (so no immortal's existing custom move message is lost), then
      clears those two columns. Fresh `being_t.bamfin`/`bamfout` fields
      + `player_set_bamfin/out` + a NEW `cmd_bamf.c` (`cmd_bamfin`/
      `cmd_bamfout`) back `goto`'s (cmd_goto.c) own custom teleport
      messages -- broadcast to the room departed (bamfout) and the room
      arrived in (bamfin), the mover's own private "You vanish..." line
      untouched either way. Three tokens (the two follow-up requests):
      `<N>`/`<n>` (the mover's name, may appear anywhere -- same
      convention as a player's `title`, cmd_who.c's `title_with_name()`),
      `$g`/`$$g` (the room's ground-surface word,
      `obj_apply_ground_token()`, already-existing infrastructure reused
      as-is), and `$p` (gender_possess() pronoun, same as poofin/poofout).
      No `$d` for goto -- a teleport has no direction. Renamed
      `tests/smoke_test_bamf.py` -> `tests/smoke_test_poof.py` (mechanical
      bamf->poof rename throughout, still covers the WALKING feature) and
      wrote a fresh `tests/smoke_test_bamf.py` covering `goto`'s
      departure/arrival broadcasts and all three tokens. New help topics
      `bamfin`/`bamfout` (goto); `poofin`/`poofout` help topics carry over
      the old body text (with companion `UPDATE`s renaming the
      already-seeded `bamfin`/`bamfout` rows first, sequenced before the
      fresh inserts to avoid a primary-key collision on redeploy).
- [x] **Character-name rejection reports the specific reason** — done
      (user 2026-07-11, while diagnosing a `smoke_test_bamf.py` failure
      that turned out to be caused by a too-long witness name: "then char
      creation should report when name length is violated?"). The single
      combined message ("Names must be 3 to 15 letters -- no numbers,
      spaces, or symbols.") in `descriptor.c`'s `CONN_CHAR_CREATE_NAME`
      handler is now three distinct checks/messages: too short (<3),
      too long (>15), or contains a non-letter. Updated
      `smoke_test_name_case.py`'s 5-case rejection table to check for the
      matching specific substring per case instead of the old combined text.

## Small near-term gameplay follow-ups

- [x] **XP on kill** — done (Session 43): `combat_defeat()` awards
      `loser->progress.level * 50` XP (placeholder formula, same precedent
      as other placeholder combat/growth numbers) via the already-existing
      `progress_add_xp()`, and saves it. Only for a non-immortal PC winner
      -- covers a normal defeat and a decapitation, but not an immortal's
      `cmd_kill` instakill (that winner is always an immortal, who doesn't
      need XP).
- [ ] **Mid-fight persistence** — HP and limb HP are only saved at defeat; a
      mid-fight disconnect reloads at last-saved values.
- [ ] **`player_save()` + a `save` command** (user request, 2026-07-07) — a
      single function that persists everything about a character in one
      call (progress/attrs/inventory, and by extension HP/limb HP once
      those are added to it), plus a player-invokable `save` command that
      calls it on demand. Mirrors the original's real `TBeing::doSave()`
      (`cmd/cmd_save.cc`) -- a genuine port, not a Tobin invention. This
      would consolidate the currently-scattered save-at-mutation-point
      calls (`player_attrs_save`/`player_progress_save`/
      `player_inventory_save`, each called separately from `set`,
      `cmd_mortal.c`, `combat_defeat()`, and every object command) into one
      place, and directly close the "Mid-fight persistence" gap above.

## Blocked on Objects / Mobs (Phase 2C/2D/2E)

### >>> NEXT UP (work session): `edobject` and/or `edmobile` <<<

Objects (2C) and Mobiles (2D) are BOTH done as of 2026-07-07 (Sessions 34
and 35) -- see STATUS.md's decision rows. Both editors were deliberately
deferred to their own session(s) each (designing a system and its editor
at once serves neither well); the user already said they want wireframes
drafted (not provided) for both, from Sneezy's real menus:
- `edobject`: `create_objs.cc`'s `update_obj_menu` (21 fields), covering
  the real fields now in `obj_t`/the `obj` table: name/short/long/action
  desc, category (was `type`), wear_flag, action_flag, val0-3, weight,
  volume, price, can_be_seen, max_struct/cur_struct, material, decay,
  max_exist.
- `edmobile`: `create_mobs.cc`'s `send_mob_menu` (30 fields), covering the
  `mob` table's real columns -- note Tobin's `being_create_mob()` only
  uses 6 of ~40 columns today (name/short_desc/description/level/hpbonus/
  sex), so this editor's scope decision (edit only what's wired up, vs.
  edit the full row and leave most fields inert until AI/combat-stats work
  lands) is itself worth raising with the user before drafting the
  wireframe.
Same menu-driven working-copy pattern as `edplayer`/`edroom` either way
(see [[editors-menu-driven]]).

- [x] **Objects (2C)** — done 2026-07-07: `obj_t` (16-category collapse,
      not the originally-estimated ~15 -- close enough, see obj.c's
      lookup table comment), DB load (`obj_repo.c` reads the existing
      upstream-seeded `obj` table directly, no new prototype table),
      `oload`, get/drop/inventory/wear/remove/equipment, persistence
      (`player_inventory.sql`, carried/worn/held only), drop-equipment-
      on-death (`combat.c`), equipment wired to the existing 13-limb enum
      (no second enum, per this item's own original constraint).
      `smoke_test_objects.py` + 7 new help topics + a news entry.
- [ ] **`edobject` (oedit)** — object editor (menu-driven, DB prototype
      rows in the existing `obj` table). See NEXT UP note above. Sneezy's
      `update_obj_menu` has 21 fields (STATUS / create_objs.cc).
- [x] **Mobs (2D)** — done 2026-07-07: a mob is just a `being_t` with
      `kind=THING_MOB` (no new struct -- matches the original's own
      `TMonster : TBeing`), DB load (`mob_repo.c` reads the existing
      upstream-seeded `mob` table directly), `mload`, and full combat
      integration (`combat_find_room_target()`/`combat_defeat()` widened
      to handle mobs, permanent removal on defeat -- `combat_process_run()`
      needed zero changes). `attrs`/`max_hp` are level-derived placeholder
      formulas, NOT the mob table's real 12-stat/hpbonus system (see
      STATUS.md). `smoke_test_mobiles.py` + a help topic + refreshed
      `attack`/`kill`/`look` topics + a news entry. Still unlocks the real
      kill-XP economy once `progress_add_xp()` gets wired up (separate
      follow-up, see the small-gameplay-follow-ups list above).
- [ ] **`edmobile` (medit)** — mob editor (menu-driven, DB prototype rows
      in the existing `mob` table). See NEXT UP note above. Sneezy's
      `send_mob_menu` has 30 fields (STATUS / create_mobs.cc).
- [ ] **Mob AI / aggression** — `ACT_AGGRESSIVE` (and the rest of the
      `actions`/`affects` bitmask columns) is completely unused -- mobs
      are reactive-only today (never act until attacked). A real AI/pulse
      tick is a separate, larger follow-up.
- [~] **>>> Zones / zonefiles (2E) <<<** (user 2026-07-07) — in progress.
  - [x] **Part 1: convert zonefiles -> DB** — done 2026-07-07: `db/import-zones.py`
        parses the upstream `lib/zonefiles/*` into `db/sneezy/zone_reset.sql`
        (a new `zone_reset` table: zone_nr, cmd_no, command, if_flag, arg1-4,
        comment). 35,922 reset commands imported (M 11314 -> mob to room,
        O 6625 -> obj to room, E/D/G/P + Sneezy-specific opcodes stored too).
        Sneezy's `?`-conditional (6750x) and one stray `Wrench` skipped.
        Auto-loaded by `apply-tobin-schema.sh`. Data-only so far (no execution).
  - [x] **Part 2: execute resets** — done (Session 43, user: "zonefiles are
        not loading? i dont see anything in rooms or mobs wandering
        around"). New `zone.c`/`zone_repo.c`: covers the highest-value
        opcode subset -- M (load mob), O (load obj on the ground, boot-time
        only -- matches the original exactly), E (equip the last-loaded
        mob -- placement derived from the object's own wear_flag via the
        existing wear_slot_for_flag(), not the original's arg3 slot index,
        which has no Tobin-limb equivalent), G (give the last-loaded mob a
        carried item), P (place an item inside the last-loaded container),
        D (door open/closed/locked). Together ~84% of all real rows. The
        rest (Y/X/Z object "sets", A random-room, V/H/F/T/L/K/C/R/I/J) are
        skipped -- they need subsystems Tobin doesn't have yet (mob AI,
        object sets, loot tables, traps, grouping/charm/mounts); skipping
        one doesn't break the rest of a zone's chain, only `if_flag`-gated
        rows depending on it. **"Wandering" mobs specifically still needs a
        separate mob-movement/AI system** -- this only POPULATES rooms, it
        doesn't move mobs around afterward (see Mob AI/aggression below).
        Runs the FULL reset once at every process start (`zone_boot_all()`,
        main.c) -- both a cold boot AND a copyover-resume, since neither
        preserves room/mob/object state today (only player connection info
        survives a copyover, see cmd_copyover.c -- confirmed by reading it
        before building this, since the user flagged the copyover question
        directly). Then tops up periodically per-zone on its own `lifespan`
        (minutes) via a ~60s pulse tick. New immortal/builder command
        `zonereset <zone>` force-runs a zone's reset on demand (also the
        test hook, since waiting on a real lifespan timer isn't practical
        for a smoke test). Known simplification: no world-wide max_exist
        cap tracking (only a per-room cap, arg2) -- see the original's
        stat_mobs/stat_objs bookkeeping for what that would need.
        `smoke_test_zones.py` (verifies both a REAL seeded zone actually
        populated a real room, and `zonereset`'s M/E/G/P/D/unhandled-Y
        behavior via a sandbox zone).
  - [x] **Zone identity/ownership** — done (Session 43, user: "add
        identity to zones... builder gets assigned a zone then... a
        51-54 wants to edit gets rejected except for those assigned to
        that zone"). New `zone_owner` table (many-to-many: a zone can have
        multiple builders, a builder can own multiple zones). New
        `zone_can_edit()` (zone.h): 55+ edits any zone; a builder (51-54)
        only a zone they're assigned to; a room with NO zone (`room.zone`
        NULL) is unrestricted for everyone, since the boundary is per-
        zone. Wired into `edroom` and `edzone` (below) -- the only content
        editors that exist yet; **apply the same `zone_can_edit()` check
        to edobject/edmobile when those are built.**
  - [x] **Part 3: `edzone`** — done (Session 43, user pivoted from a one-
        shot `zoneassign` command to "make an edzone command to have a
        menu driven editor function like edroom etc"). Menu-driven, same
        snapshot-working-copy shape as `edplayer` (a zone isn't kept
        resident in memory like a room): name/enabled/lifespan/vnum range
        are Save/Quit-gated; assigning/un-assigning a builder (selecting
        an already-assigned name un-assigns them, same toggle as before)
        applies immediately, not deferred to Save; an `R`eset-now action
        force-runs the zone. Gated the same as edroom: 51-54 needs
        `zone_can_edit()` to pass for that zone_nr, 55+ always. Editing
        individual M/O/E/G/P/D reset-command rows is explicitly OUT of
        scope for this pass (user confirmed) -- still a future follow-up.
        Also kept a `zone reset <n>` one-shot shortcut (user: "keep zone
        reset as a quick shortcut") and added `zone list` (user: "dont
        forget a zone list so we can see whats been assigned and to
        whom") -- both in `cmd_zone.c`, paginated. `smoke_test_zone_identity.py`
        + `smoke_test_edzone.py`.
- [x] **Containers holding sub-items** (`put <item> <container>` / `get
      <item> <container>`) — done 2026-07-09 (Session 39, work). `cmd_put` +
      `cmd_get`'s two-arg form move items in/out of a container (carried/worn/
      floor); `look <container>` lists contents when open; `open`/`close` now
      act on containers via `CONT_*` bits in `val[1]`; weight capacity (`val[0]`)
      enforced. `smoke_test_containers.py`, news/wiznews, help topics.
      DEFERRED (see STATUS decisions): carried-container *contents* persistence
      (saved loose to avoid loss, reload un-nested — needs a `player_inventory`
      parent column); lock/unlock+keys (pairs with the doors/keys item below).
      **This unblocks Zones Part 2's `P` opcode** (put obj in container).
- [ ] **Keys unlocking doors** — `unlock`/`lock <direction> with <item>`,
      matching a KEY-category object's val[0] against the exit's key
      requirement. The object system this was blocked on (Session 31) now
      exists -- separate follow-up, not built this pass.
- [ ] **Shops + money** — shopkeeper buy/sell, shop editor, GOLD-COIN-ONLY
      currency (shop tables already in the seed DB). Needs objects.
- [ ] **Player-state logging** — log get/drop + pfile changes so `log search
      <name>` tells a player's story. Needs objects; design a uniform helper.
- [ ] **Body types** — `body.h` body-type concept (creatures have different
      limb sets). Pairs with mobs + limbs.

## Bigger systems (need design / a decision)

- [ ] **Hospital (limb repair)** — user, 2026-07-11: "add hospital code to
      the todo list." Right now a destroyed limb (`being_has_destroyed_limb()`,
      being.h) has no in-game cure -- the only fix is dying and respawning
      (`being_limbs_full_heal()` at combat defeat). A hospital would let a
      living character repair a destroyed/damaged limb mid-game instead.
      Needs design decisions: a physical hospital room/building + a `heal`
      or `repair` command there (vs. an NPC healer to interact with);
      cost (gold? time? risk?); whether it also cures poison (see `drink`
      from pools) or only limb damage; whether it's instant or takes time
      (a queued/timed repair). Not started.
- [ ] **Classes** — warrior/cleric/thief/monk/mage, chosen at creation, shown
      in score/who. Stat affinities (user spec): mage high INT / low STR;
      warrior high CON+STR, dump CHA+WIS; thief high DEX / low STR; cleric
      high WIS / low STR+DEX; monk STR+CON / low CHA. Port Sneezy's class
      tables/formulas onto Tobin's 6-stat set. (Ignore DISC_* for now.)
- [ ] **Races** — curated player list + open mob list; race stat bonuses from
      Sneezy's race tables.
- [ ] **Game balance layer + `gameedit` (60 ONLY)** — race/class PERCENTAGE
      bonuses; a level-60 live tuner in 0.1% increments, exact-word like
      `quit!` (never by typo), DB-persisted. Needs classes+races.
- [ ] **Limbs → `wearSlotT`** — reshape the 13-limb enum toward Sneezy's
      `wearSlotT` (back, wrist, hand vs finger, HOLD, EX_* for mobs). **Open
      question:** keep the Tobin-added `genitalia` slot? include HOLD/EX for
      players or mobs only? Own focused pass; touches combat/score/limbs.
- [ ] **Limbs.h gap review** — weighted hit locations, PART_* flags, etc.
      (overlaps the wearSlotT item).
- [ ] **TobinMUD identity + DB rename** — rename DB `sneezy` → `tobin`
      (init-db.sh, config defaults, docs); rebrand credited as "Derivative of
      SneezyMUD and DikuMUD". Wide but mechanical; coordinate on all boxes.

## Chores / infra

- [x] Create the `mud` user on the work box (db.kullit.com) — done
      2026-07-06, along with a dedicated SSH keypair for it; root access
      to that box is retired (see ENVIRONMENT.md).
- [x] Auto-restart `tobin_c` if it dies — done 2026-07-05: the user added a
      crontab to the `mud` user that checks whether the MUD is running and
      starts it if not. (Deploys still `pkill; sleep 1; restart` fast, before
      any cron tick, so there's no double-launch.)
- [ ] Install a MUD client (Mudlet, for ANSI color) on the Windows machines.
- [ ] **docs/systems review** — read `sneezymud-master/docs/systems` for how
      the original stored things; apply the lessons. RULE: prefer the DB.
- [ ] **Systems documentation** — a doc/systems README for the TobinMUD base.
- [ ] **Function comment headers sweep** — a header comment per function (what
      it's for + cross-refs to what it affects / depends on), then a habit.
- [ ] **STATUS.md's "Module port status" table is stale** — the `cmd/`
      row still says "11/66 ported" (list: look/who/score/quit!/color/
      attack/kill/say/limbs/help/wizhelp); there are 40+ `cmd_*.c` files
      now. Needs a dedicated audit pass, not a quick fix mid-feature.

## Reference material (Sneezy enums, provided 2026-07-04)

Upstream enums the user pasted, staged for the features above (kept in this
conversation and in `sneezymud-master`): `positionTypeT` (done), `prompt_mesg`
(health strings), `classInfo` (classes), `body_flags` (limb conditions),
`wearSlotT` (limbs), `heraldcodes`/`heraldcolors` (immortal color/heraldry),
`doorTypeT`/`doorIntentT`/`doorUniqueT` + `exit_bits` (door mechanics).

## Deferred decisions (blocked on choosing, not on code)

- [ ] Which ~8-10 `disc/` disciplines to keep; which 1-2 `task/` professions.
- [ ] Hospital mechanic for destroyed limbs (only cure now is death/respawn).
- [ ] Whether the destroyed-limb hit penalty scales with count (flat -15 now).
- [x] Immortal-vs-immortal `kill` guard — done (Session 43): `cmd_kill.c`
      refuses to instakill a PC target whose TRUE rank (true_level if set,
      else level -- protects a target who's toggled mortal via `immort`
      too) is equal to or higher than the attacker's.

## Standing rules (learned)

- Superseded 2026-07-10 (user: "interrupt full sweeps to test new code, as
  a habit"): kill an in-progress full sweep to deploy/test new code now,
  don't wait for it. Full sweeps only run right before a repo push (see
  below), so one in flight is disposable. (Old rule: never hot-deploy
  mid-sweep, from an incident where it caused unrelated flakes — no longer
  applies now that sweeps aren't run casually between changes.)
- Every player-facing change gets a `news.sql` entry (no numbers). See CLAUDE.md.
- Every new `db/sneezy/*.sql` file MUST use `CREATE TABLE IF NOT EXISTS`,
  never an unconditional `DROP TABLE IF EXISTS` + `CREATE TABLE` — the
  latter silently wipes live data every time `apply-tobin-schema.sh` re-runs
  it (which it always does; that script re-applies every file, every time).
  Burned us once for real (Session 36: `player_attrs.sql`/
  `player_progress.sql` wiped ~1338 players' progress this way).
