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

## PRIORITY — pick this up next (user 2026-07-20)

- [ ] **Linkdead auto-purge (5 minutes, discard-only)** — found live,
      2026-07-20: a linkdead PC's `being_t` stays fully resident in its
      room forever (`desc == NULL`, never destroyed except on that same
      account's reconnect or a `purge linkdead`/process restart) --
      after months of smoke-test runs (every test that `s.close()`s a
      raw socket instead of `quit`ing leaves one behind), Center Square
      alone had 70+ linkdead bodies, which looks like the proximate
      cause of a real slowdown/hang hit while testing this session (see
      STATUS.md's Session 51 write-up). Verified against the original:
      `misc/periodic.cc` already has exactly this mechanic -- a per-tick
      linkdead timer, `nukeLdead()` once it crosses 15 minutes (mortals)
      or 60 (immortals) -- force-saves, THEN strips/frees their
      equipment and destroys the being. **User-directed deviation**: flat
      5 minutes for everyone (not the original's 15/60 split) --
      document this as a deliberate deviation in STATUS.md's decisions
      table when it lands.
      **Open design question, needs a decision before/while
      implementing**: save-then-destroy (user's original phrasing,
      matches the original's `nukeLdead()`) vs discard-only (matches
      Tobin's OWN existing precedent -- `descriptor_destroy()`'s comment
      and `world_purge_linkdead()`, both deliberately avoid an eager
      save because it could clobber a fresher DB-side edit made while
      linkdead, e.g. an admin `set`/`promote`). Leaning discard-only for
      consistency with the existing precedent, but this is a real
      data-integrity tradeoff worth re-confirming with the user before
      picking, not just defaulting silently.
      **Implementation sketch**: a new pulse (`main.c`, alongside
      `descriptor_idle_timeout`/`zone_process_run` etc.) that walks
      every linkdead PC (`world_for_each_room()`-style iteration,
      `base.kind == THING_PC && desc == NULL`) via a new `world.h`
      primitive (parallel to `world_purge_linkdead()`/
      `world_count_linkdead()`), tracks a per-being elapsed-linkdead
      timer (new `being_t` field, incremented each pulse the same way
      `progress.rented_at`-style timestamps already work elsewhere --
      or simpler, just stamp `being_t` with the wall-clock time
      `desc` was cleared in `descriptor_destroy()` and compare against
      `time(NULL)` each pulse, no counter needed), and once past the
      5-minute mark, force-destroys it (`being_destroy()`, already
      exists) -- optionally save first per the design question above.
      Needs a smoke test (spin up a character, close the raw socket
      without `quit`, wait past the threshold or fast-forward the
      pulse counter the way other pulse-driven tests already do,
      confirm the being is gone from the room) and a wiznews entry
      (immortal-facing world-management change) -- likely a `news`
      entry too, since it changes what a player finds on reconnecting
      after a long disconnect (resumes fresh from their last real save/
      rent instead of picking the old body back up).

## Sneezy → Tobin feature audit (user 2026-07-19)

User audited the Sneezy → Tobin Feature Audit artifact (published
2026-07-11, refreshed 2026-07-19) and asked to finish the remaining
Missing/Partial rows: skill-based combat, offensive spell breadth, magic
items, object maintenance, group/party, ignore lists, OOC channels, sign
language, death processing (XP loss/resurrection), vital statistics,
builder tools OLC (oedit/medit), weather/light, terrain movement cost,
water/drowning/flight, mount/riding, monster AI pursuit, transformation,
money system v2, crafting & extraction, material properties, object
manipulation depth, switch/puppet, pet/charm, quest system, drug
tracking. Ground rules set via AskUserQuestion before starting: oedit/
medit designed without a wireframe (following redit/edzone conventions);
a real Vitality stat gets added to unblock terrain cost; the large
economy/material systems (banking, crafting, materials) get a Tobin-scale
slice, not Sneezy's full original depth. Sneezy source (bundled
`sneezymud-master/`, especially `docs/systems/`) checked for real
implementation inspiration before each one, not guessed at.

- [x] **Ignore lists** — done 2026-07-19. Checked Sneezy's own
      `communication-system.md` doc first: the original's `ignoreList`
      blocks by descriptor/name/whole-account across say/tell/whisper/
      shout/grouptell/emote/socials, with silent-to-sender failure.
      Scoped to what Tobin actually has and what "unwanted communication"
      really means here: a flat per-character name list (new
      `player_ignore` table, `ignore_repo.h`/`.c`), checked only by
      `tell`/`whisper` (the two direct-message channels that exist) --
      broadcast channels (say/socials) would need a per-listener filter
      loop at every room-echo call site, a much bigger change for less
      value than blocking the channels people actually get unwanted
      DMs through. `ignore [<name>]` (bare lists, up to 30 entries)/
      `unignore <name>` (cmd_ignore.c). A blocked sender's `tell`/
      `whisper` still reports success on their own screen -- matches the
      original's own documented "fails silently" behavior exactly, not a
      Tobin invention. `cmd_table.c` entries verified with
      `tests/tools/cmd_abbrev_check.py` (zero collisions). New
      `tests/smoke_test_ignore.py` (13 checks).
- [x] **Switch / return (puppet a mob)** — done 2026-07-19. Checked
      Sneezy's own `23-snoop-switch.md` doc first: admin switch is a raw
      descriptor-pointer swap (no stat transfer, no transformation
      message -- that's the SEPARATE spell-driven polymorph flavor, which
      Tobin doesn't have yet, tracked as its own "Transformation" audit
      item). Named `possess` rather than Sneezy's own `switch` -- that
      word is already taken in Tobin (swap held items, cmd_object.c).
      `possess <mob>` (59+, cmd_possess.c)/`return`: new
      `descriptor_t.possess_original` holds the immortal's own PC while
      `character` points at the puppet -- the original goes temporarily
      `desc == NULL`, the exact same shape a real link-drop already
      leaves a body in, so no new "state" concept was needed to represent
      it. Two real bugs caught before shipping, not just guessed at from
      reading the doc: (1) `cmd_dispatch()`'s min_level gate reads
      `d->character->progress.level` -- while possessing, that's the
      MOB's seeded level (often 1), which made `return` itself invisible
      and would have permanently stranded an immortal inside whatever
      they possessed. Fixed by gating on `d->possess_original`'s level
      when set. (2) A disconnect while possessing was about to run the
      normal link-drop handling (room announce, log, linkdead parking)
      against the PUPPET instead of the immortal -- fixed by restoring
      `d->character` to the original FIRST, at the very top of
      `descriptor_destroy()`. `quit!` while possessing is refused outright
      (would try to `player_save()`/drop-items a being with no real
      player row) -- `return` first. Known, accepted gap: internal
      same-level checks inside OTHER admin commands (e.g. `snoop`'s
      target-level comparison) weren't individually audited for the
      "acting immortal's real level, not the puppet's" distinction --
      only the two paths above (dispatch gate, disconnect) were provably
      capable of permanently stranding someone, so those got fixed;
      combining `possess` with a same-level check inside another command
      is an unexplored edge case. New `tests/smoke_test_possess.py` (9
      checks, including the disconnect-while-possessing recovery).
- [x] **OOC channels (commune, etc.)** — done 2026-07-19. Checked Sneezy's
      own `communication-system.md` doc first: `wiznet` (`;<message>`
      shorthand) already covers the original's "immortal broadcast" half
      of `doCommune()` exactly -- every online immortal, sender included.
      The one real documented gap was the `@<level>` prefix, which
      narrows delivery to gods at or above that level (`commune @60
      <message>`); added the equivalent to `wiznet` (`wiznet @<level>
      <message>`, composes with the `;` shorthand). Newbie channel
      already existed too. Deliberately did NOT add a separate redundant
      "commune" command -- the 2026-07-11 audit's own note ("leave
      further channel sprawl unless a specific need shows up") still
      applies; the actual gap was the targeting feature, not a whole new
      channel. New `tests/smoke_test_wiznet.py` (4 checks).
- [x] **Group / party system** — done 2026-07-19. Checked Sneezy's own
      `09-group-party.md` doc first, then scoped it down deliberately (all
      documented inline in being.h's field comment): no leader-succession
      algorithm (the group simply dissolves if the leader leaves/dies,
      rather than the original's two-pass `reformGroup()`); no per-player
      configurable 1-10 `group share` money factor -- gold splits EVENLY
      instead; XP shares are level-weighted (a simplification of the
      original's `mob_exp()`-based share); no quest-flag
      (`PLR_SOLOQUEST`/`PLR_GRPQUEST`) or charm/mount interaction checks;
      `follow` only refuses a DIRECT circular pair (`target->master ==
      ch`), not a full chain walk, since the model doesn't support deep
      nesting anyway. New `being_t` fields `master`/`grouped`/
      `followers[GROUP_MAX_FOLLOWERS]` (live in-memory only, same rule as
      `fighting` -- EXCEPT a raw disconnect deliberately does NOT clear
      them, so a group survives a member briefly dropping link, matching
      the original; only `being_destroy()` calls `being_leave_group()`).
      `follow <name>`/`stop`/`group [<name>|all]` (leader-only to add)/
      `split <amount>` (leader-only, divides evenly among present grouped
      members, refuses if the per-share would round to 0) --
      `cmd_group.c`. `combat.c`'s new `group_recipients()` helper feeds
      three existing blocks (XP award, PK gold-steal, mob-gold-drop) so a
      grouped, in-room, non-immortal winner's kill benefits the whole
      party instead of just the striker, while preserving the exact
      pre-existing solo-winner math/messages when `grouped` is false
      (regression-verified clean against `smoke_test_pk_gold.py` and
      `smoke_test_levelup_hp.py`). Two real test-infrastructure lessons
      hit and documented in `smoke_test_group.py`'s own comments: (1) a
      direct SQL `UPDATE` on an already-connected character's gold/HP/
      room isn't seen by their in-memory copy until a relog, AND (2)
      `quit!` itself calls `player_save()` (`descriptor_leave_to_menu()`),
      which will silently overwrite a just-applied direct SQL update with
      stale in-memory data if the SQL update runs BEFORE the `quit!`
      instead of after. New `tests/smoke_test_group.py` (14 checks, all 6
      phases: follow-alone-grants-nothing, group-grants-it, non-leader-
      refused, XP-splits-on-a-kill, gold-splits-evenly-via-`split`, `stop`
      breaks it).
- [x] **Vital statistics (hunger/thirst/age)** — done 2026-07-19. Checked
      Sneezy's own `vital-statistics.md` doc first, then scoped it down
      hard (documented inline in being.h's progress_t field comment):
      rescaled the original's cryptic 0-24 `condTypeT` units (FULL/THIRST)
      to a plain 0-100 "percent full/hydrated"; dropped PEE/POOP waste
      products and DRUNK entirely (out of scope for this item); flat drain
      rate instead of the original's terrain-weighted two-stage-random
      `foodNDrink()` (terrain factors belong to the still-open "Terrain
      movement cost" item, not this one). Age: user, AskUserQuestion
      2026-07-19 -- track + display only, explicitly NOT the original's
      full `graf()`-interpolated age-based stat-curve system (6 stats,
      human-equivalent age conversion, opt-in quest bit, vampire
      exemption) -- real machinery for a mostly-cosmetic payoff on a small
      MUD. New `progress_t.hunger`/`thirst` (-1 = immortal-immune,
      `vitals_tick_run()`/`vitals.c` skips immortals outright rather than
      ever storing -1) and `.birth_time` (unix timestamp, set once in
      `being_create_pc()`). Drains 1 hunger + 1 thirst per ~60s tick
      (`VITALS_PULSES`, same "once a minute" cadence as zone aging/
      gametime/mob AI); starvation (hunger OR thirst at 0) costs 1 HP per
      tick, floored at 1 -- same "never lethal outside real combat"
      precedent `cmd_sip.c`'s poison roll already established, since real
      death-by-starvation is explicitly deferred to whenever the still-open
      "Death processing" item builds a real non-combat death path. New
      `eat <food>` (`cmd_eat.c`) restores hunger by the FOOD object's own
      `val[0]` (already well-populated 1-24 in the real seed) and fully
      consumes the object in one bite -- sidesteps a real upstream data gap
      (every seeded FOOD's `val[1]`, "current units", is uniformly 0) rather
      than migrating it, since a single-bite model needs it not at all.
      `drink` (`cmd_drink.c`) and `sip` (`cmd_sip.c`) now also raise thirst
      (a fountain fully refills it, a puddle/sip only partly) -- `sip`'s own
      comment previously said Tobin had no meter for it to move; now it
      does. `score` (`cmd_score.c`) shows Hunger/Thirst as descriptive
      words (`being_hunger_word()`/`being_thirst_word()`, same bucketing
      style as `being_health_word()`) plus a real-elapsed-time Age line --
      immortals see "Immune" regardless of their stored value. `aitick`
      (`cmd_aitick.c`) now also forces `vitals_tick_run()`, same "force the
      real ~60s cadence deterministically" precedent already used for pool
      decay/light burn, so a test doesn't need to wait on real time. New
      `tests/smoke_test_vitals.py` (13 checks: fresh-character defaults,
      eat/drink restoring the right amounts, starvation costing HP and
      flooring at 1, immortal immunity) — uses tolerant ranges rather than
      exact equality on a couple of checks since a real background tick can
      in principle land mid-test (caught live once while writing this).
- [x] **Death processing (XP loss, resurrection)** — done 2026-07-19.
      Checked Sneezy's own `death-processing.md` doc first: real death
      there is a whole pipeline (`die()` -> `rawKill()` -> `makeCorpse()`)
      built around genuine permadeath (the character record itself is
      deleted) with a corpse and a resurrection SPELL to revive it. Asked
      the user before building anything (AskUserQuestion, since Tobin's
      PC "death" was already NOT permadeath -- `combat_defeat()`
      half-heals HP, fully heals limbs, and ejects the loser to the
      account menu, same soft-respawn design as before this session):
      answer was XP loss only -- "resurrection" is already covered by the
      existing relog flow, so no corpse or spell system was built. New
      logic in `combat_defeat()` (`combat.c`), inside the existing
      `loser_is_pc` block: docks XP on death using `min(20% of current
      XP, XP banked past the current level's own threshold)` -- adapted
      from Sneezy's own `min(20%, 25 * mob_exp(level))` formula, but the
      cap here needs no separate mob-XP curve since it's just
      `progress_xp_for_level()` (already existed for leveling) -- and
      means a death can never de-level anyone, only eat into progress
      toward the next level. PvP (a PC winner, i.e. a mutual `toggle pk`
      duel) divides the result by 10, same reduction Sneezy applies; a
      MOB winner (the ordinary "died to a monster" case) gets the full
      penalty. Immortals never lose XP (already past the mortal ladder,
      same precedent as the winner-XP-gain block just below it). New
      `tests/smoke_test_death_xploss.py` (6 checks: PvE death loses
      exactly 20% with ample banked XP, a tiny-banked-XP death is capped
      at exactly the level threshold without de-leveling, and a PvP death
      loses 1/10th what the same banked XP would in PvE) -- regression-
      verified clean against `smoke_test_pk_gold.py`,
      `smoke_test_levelup_hp.py`, and `smoke_test_group.py`, all three of
      which also exercise `combat_defeat()`.
- [x] **Object manipulation depth (sacrifice, junk, identify)** — done
      2026-07-19. Checked Sneezy's own help topics/source first. `sacrifice`
      turned out to be entirely a Shaman-class skill tied to a `lifeforce`
      resource and totems held in the caster's hand -- Tobin has no Shaman
      class, no lifeforce stat, no totems, so it doesn't map onto anything
      that exists here. Asked the user (AskUserQuestion) rather than either
      inventing a whole lifeforce/totem/Shaman system or silently
      reinterpreting the verb into something the original never meant;
      answer was to skip it outright. `junk <item>` (`cmd_object.c`, next
      to `drop`) is a straight port of the original's own command (not
      skill-gated there either): destroys a carried, loose item for good,
      same scope as `drop` (worn/held items need removing first).
      `identify <item>` (new `cmd_identify.c`) is a plain command, not a
      spell -- same tier as `examine`/`consider`; shows a carried item's
      real category-specific stats (weapon hit/dam bonus, armor AC,
      container capacity, drink units, food nutrition), NOT `stat`''s full
      immortal-only prototype dump. Building it surfaced two real
      DB/doc-comment mismatches, fixed by showing only what''s
      mechanically real rather than trusting obj.h''s val[] doc comment
      blindly: (1) a weapon''s val[0]/val[1] are NOT damage dice despite
      the doc comment claiming so -- verified against the real seed (e.g.
      vnum 175 carries val0=4626, val1=2073, nonsense as dice) and against
      combat.c''s actual formula, which never reads either field, so
      identify shows the real hitroll/damroll bonus (`obj_load_combat_mods()`)
      instead; (2) a magic item''s val[0] "charges" is likewise unreliable
      (a plausible 5 on potions, a nonsense 25650 on one scroll) and unread
      by any code yet (no `use`/`zap`/`quaff` command exists -- that''s the
      still-open "Magic items" audit item) -- identify says so honestly
      instead of printing a number that might be garbage. Also found and
      fixed a real latent bug while here: `eat` (previous audit item)
      called `obj_destroy()` on the consumed food but never
      `player_inventory_save()` afterward, same gap `junk` would have had
      -- without it, a reconnect before any OTHER inventory-touching
      action would resurrect the "destroyed" item from its still-persisted
      DB row (matches `cmd_shop.c`''s own sell path, which already got
      this right). Both commands now save inventory immediately after
      destroying. New `tests/smoke_test_objmanip.py` (10 checks, including
      a regression check for the resurrection bug on both `junk` and
      `eat`).
- [x] **Quest system** — done 2026-07-19. Checked Sneezy's own
      `quest-system.md` doc first: the real system is a fixed 454-bit
      array (`toggles[]`) tied entirely to hand-authored content that
      doesn't exist in Tobin (specific quests like "Avenger"/"Silverclaw"/
      "Holy Devastator", named NPCs, spec-procedure dialogue trees) --
      porting the bit array itself would just be 454 meaningless numbers
      with nothing behind them. Asked the user (AskUserQuestion) rather
      than either inventing bespoke Tobin quest content or a whole
      conditional-branching layer for Tobin's trigger-script language
      (wait/echo/echoroom/emote/say/teleport/give/damage/log, which has no
      "if" at all yet) just to drive it -- answer: infrastructure only.
      Ported the SHAPE of the original instead of its bit numbers: a
      player's progress through a named quest, tracked as a small integer
      stage, visible only where an immortal has written a description for
      that exact (quest, stage) pair -- same "only bits with a help file
      are visible" rule, DB rows instead of files (matches help_topic/
      news/wiznews convention). New `player_quest` (player's current
      stage per quest touched) / `quest_def` (immortal-authored
      description per stage) tables, `quest_repo.h`/`.c`. `quest
      [<name>]` (mortal, `cmd_quest.c`): bare form lists quest names with
      a written description for the player's current stage; `quest <name>`
      pages the full description. `questdef <name> <stage> <text>`
      (builder tier, `cmd_questdef.c`, no menu editor -- same "no in-game
      editor for it yet" precedent as several other content types) writes/
      replaces a stage's description. Advancing a player's stage is a new
      `quest` field on the existing `set <name> <field> <value>` (`set
      <player> quest <name> <stage>`, cmd_set.c) -- manual, since there's
      no scripting hook yet to automate it; stage 0 clears the quest
      entirely. New `tests/smoke_test_quest.py` (9 checks: invisible
      without a description, visible once one exists, hidden again when
      advancing to an undescribed stage, cleared at stage 0) --
      regression-verified against `smoke_test_set.py`/
      `smoke_test_alignment.py`, which also exercise `cmd_set.c`.
- [x] **Weather & light levels** — done 2026-07-19. Checked Sneezy's own
      `misc/weather.h`/`.cc` first: a real barometric-pressure random
      walk driving sky-state transitions, a 0-31 moon-phase cycle,
      per-room "wetness" tracking, and season-aware sunrise/sunset --
      trimmed hard to a single WORLD-WIDE sky state (clear/cloudy/rainy/
      stormy, not per-room -- Tobin has no distinct climate zones to make
      that meaningful) advanced by a simple weighted transition table
      instead of a pressure simulation. `weather` (mortal, new
      `cmd_weather.c`) shows the current condition, a fixed flavor "hint"
      per state (not a real forecast -- that's exactly the simulation
      depth trimmed), and day/night. New `weather.h`/`.c`,
      `pulse_register(WEATHER_PULSES, weather_tick_run)` in main.c, same
      ~60s "once a minute" cadence as gametime/mob AI/vitals, persisted
      via `game_config` (same table/convention `gametime.c` already uses,
      no new table needed); world-wide announcement on every actual state
      change, same broadcast-to-everyone precedent gametime.c's own noon/
      midnight announcements already use. The "light levels" half is the
      more concrete payoff: `gametime_is_daytime()` already existed
      (used by the lamplighter mob, mob_ai.c) but nothing actually gated
      visibility on it. New `being_has_active_light()` (any lit
      OBJ_CAT_LIGHT anywhere in a being's carried/worn/held chain, same
      scope as `obj_light_burn_tick()`'s own burn-down) and
      `room_is_dark_for()` (being.c) -- a room is dark for a non-immortal
      looker with no active light unless ROOM_FLAG_ALWAYS_LIT,
      ROOM_FLAG_INDOORS, or plain daylight. Wired into both `look` (bare
      room description only, not `look <target>` -- a deliberate, smaller
      scope) and `exits` (gating only one of the two would let a player
      just route around the restriction with the other) -- this is what
      makes carried light sources (cmd_light.c) matter for the first
      time. `aitick` now also forces `gametime_tick()`/`weather_tick_run()`,
      same deterministic-testing precedent as every other ~60s tick this
      session added. New `tests/smoke_test_weather.py` (8 checks) --
      building it surfaced a real cross-test hazard: darkness is brand
      new, so ANY pre-existing test with a plain (room_flag=0, not
      ALWAYS-LIT/INDOORS) sandbox room that calls `look`/`exits` on it can
      now intermittently fail depending on the shared server's current
      in-game time of day. Confirmed and fixed one real casualty
      (`smoke_test_look_equipment.py`, now flagged ALWAYS-LIT); a ~54-file
      grep found many more tests using the same unflagged-sandbox-room
      pattern, most of which don't actually touch look/exits and aren't
      at risk, but a full audit was spun off as a follow-up task rather
      than scope-creeping this session further. `smoke_test_weather.py`
      itself restores a neutral daytime hour when it finishes, so it
      doesn't leave the shared preview/production clock stuck at night
      for whatever runs next.
- [x] **Monster AI & behavior (pursuit)** — done 2026-07-19. Checked
      Sneezy's own `docs/systems/critical/14-monster-ai-behavior.md`
      first: a huge opinion/emotion model (suspicion/greed/malice/anger
      0-100 attributes, categorical + individual hate/fear memory lists,
      faction territorial combat, per-class combat AI dispatch, scripted
      DB-driven mob dialogue) plus a real multi-room `hunt()` pursuit
      state machine (persistence/distance counters, `dirTrack()`
      pathfinding, vision/concealment checks, cleric teleport-pursuit).
      None of that infrastructure exists in Tobin, and building it is way
      out of scope for one audit item — scoped the "(pursuit)"
      parenthetical down to exactly what it names: a single-room,
      immediate, probabilistic "does an aggressive mob follow a fleeing
      player into the next room" reaction, no persistence, no cross-tick
      hunting state, no real hunt. New `mob_ai_try_pursue()` (mob_ai.c/
      .h) — if the mob left behind by a successful `flee` has
      `ACT_AGGRESSIVE`, a flat 50% chance to immediately follow into the
      destination room and re-engage before the fleeing player's own
      `look` renders (so a successful chase is already standing there in
      the room description, not a surprise arrival after the fact).
      Wired into `cmd_flee.c` right after combat breaks off; only fires
      for a mob foe (a PC opponent from PK combat never gives chase).
      New `tests/smoke_test_pursuit.py` (2 checks: an ACT_AGGRESSIVE mob
      eventually chases across repeated flee attempts; a mob without the
      flag never pursues, deterministically) — retries across up to 30
      attempts since both flee's own ~2/3 escape chance and pursuit's
      50% chance are probabilistic, same "N attempts" precedent as
      `smoke_test_drink.py`. Verified variance is genuine (not always
      landing on the first attempt) across 5 runs (1, 1, 1, 3, 4 tried)
      before shipping, ruling out an always-succeeds bug in either roll.
      Regression-checked against `smoke_test_flee.py`, `smoke_test_mob_ai.py`,
      `smoke_test_group.py` (also exercise flee/combat/mob AI).
- [x] **Vitality stat + Terrain movement cost** — done 2026-07-19. Neither
      half exists in the original either -- its "moves" resource (misc/
      limits.cc's `getMaxMove()`/`moveGain()`) is CON-and-level-derived
      but otherwise unnamed; Vitality is a Tobin-original name for the
      same role, per the ground rule set before this session's audit
      started ("a real Vitality stat gets added to unblock terrain
      cost"), closing the long-orphaned "Depends on Vitality" TODO.md
      fragment (see "Terrain movement cost" above, now resolved).
      New `progress_t.vit`/`max_vit` (being.h) — `being_calc_max_vit()`:
      50 base + CON bonus + 2/level, same placeholder-formula shape as
      `being_calc_max_hp()`. Shown in `score`'s new Vitality line and
      now toggleable into the prompt (`prompt vit`/`PROMPT_FLAG_VIT`,
      previously blocked alongside mana/piety per cmd_prompt.c's own
      doc comment). Regenerates on the exact same weight-by-position
      regen tick as HP (regen.c) — the one concrete behavior the
      orphaned fragment specified. New `player_progress.vit`/`max_vit`
      DB columns.
      Terrain cost: the original's `TerrainInfo[MAX_SECTOR_TYPES]`
      (misc/constants.cc) hand-tunes a movement cost for every one of
      61 sectors; Tobin has no per-sector content to justify that
      granularity, so new `sector_move_cost()` (room.c) buckets all 61
      into 6 cost tiers by name-substring instead, reusing the exact
      grouping precedent `sector_color()` already established (roads/
      cities/plains cheapest, lava/solid rock/fire priciest). `north`/
      `east`/etc (cmd_move.c) now charge the average of the source and
      destination sector's cost — same average-of-two-sectors rule the
      original's `rawMove()` uses — refusing the move outright ("You
      are too exhausted to go that way") rather than letting vit go
      negative, same hard-gate shape the door/fighting/position checks
      already there use. Persisted immediately on every charged move
      (`player_progress_save()`), same "don't lose it to a disconnect"
      precedent `cmd_eat.c`/`cmd_drink.c` already established. No swim/
      fly/mount modifiers -- those stay blocked on the still-open
      "Water, drowning, flight" and "Mount / riding" audit items; water
      sectors are just an expensive-but-walkable tier for now.
      Immortals are exempt, same reasoning as hunger/thirst immunity.
      New `tests/smoke_test_vitality_terrain.py` (10 checks). Building
      it surfaced a real timing gotcha: `regen_tick_run()` heals vit
      roughly every 5 real seconds but ONLY in memory (it never itself
      calls `player_progress_save()`), so a naive DB-read-based check
      can flake against a stray regen tick a live `score` read doesn't
      -- fixed by reading vit live via `score` with a short 0.3s
      timeout instead of the DB for every timing-sensitive check, and
      loosening the checks that can tolerate a stray tick (regen only
      ever adds) from `==` to `>=`. Regression-checked against
      `smoke_test_exits_display.py`, `smoke_test_look_equipment.py`,
      `smoke_test_weather.py`, `smoke_test_vitals.py`,
      `smoke_test_group.py`, `smoke_test_quest.py` (also exercise
      movement/rooms/player_progress). Also fixed a stale, unrelated
      assertion found while regression-testing: `smoke_test_parser_display.py`'s
      "'qu' must NOT reach quit" check predated the `quest` command and
      needed updating to match "qu" now legitimately reaching quest
      instead (quit itself remains genuinely unreachable via any
      abbreviation, verified directly).
- [x] **Water, drowning, flight** — done 2026-07-19. Checked Sneezy's own
      movement-terrain-navigation doc first: AFF_SWIM halves/quarters
      water-sector movement cost, a procCharDrowning scheduler deals
      1d10 every 3.6 real seconds to anyone underwater without
      AFF_WATERBREATH (genuinely lethal via reconcileDamage()), and
      AFF_FLYING quarters movement cost and bypasses the whole drowning
      chain. Ported as two new spells that were already listed
      unimplemented in `skill.c` before this session: `cast gills of
      flesh` (Mage, level 9) grants a new AFFECT_WATERBREATH,
      `cast levitate` (Mage, level 11) grants a new AFFECT_FLYING (both
      plain timed buffs, same shape as the existing AFFECT_SANCTUARY --
      `affect.h`/`affect.c`). New `sector_is_underwater()` (room.c)
      distinguishes true UNDERWATER sectors from merely-wet surface
      water (OCEAN/RIVER SURFACE/ICEFLOW, still just an expensive-but-
      walkable tier, no drowning risk). `vitals_tick_run()` (vitals.c)
      now rolls the SAME 1d10 against anyone underwater without
      AFFECT_WATERBREATH/AFFECT_FLYING, just on Tobin's own slower ~60s
      cadence instead of the original's 3.6s one -- already far gentler
      in practice without softening the roll itself. `cmd_move.c`
      quarters `sector_move_cost()`'s charge while AFFECT_FLYING is
      active, same discount the original's flight math uses.
      Per AskUserQuestion, drowning is genuinely LETHAL, unlike
      hunger/thirst/poison's non-lethal floor-at-1 convention -- new
      `combat_drown_pc()` (combat.c) handles the death (half-heal reset,
      limb heal, full-rate XP loss since there's no PvP consent to halve
      it for, a corpse with the victim's belongings, eject to the
      account menu) since there's no `winner` to reuse `combat_defeat()`
      as-is for an environmental death. New
      `tests/smoke_test_water_drowning_flight.py` (13 checks) -- a
      spell's proficiency starts at a 1% floor and climbs with a 30s
      cooldown per gain-check (skill.c), far too slow to reliably self-
      cast live in a test, so mortal test characters have their
      `player_skill` row seeded directly to 100% first, same "seed DB
      state instead of grinding it live" precedent
      `smoke_test_vitals.py`'s `set_hp()`/`set_level()` already use.
      Also hit (and documented) a real gotcha: `quit!`/relog wipes an
      in-memory affect entirely (affects aren't DB-persisted) -- tests
      that need to both cast a buff AND relocate move by real walking
      instead of relogging, so the just-granted affect survives the
      trip. Regression-checked against `smoke_test_vitals.py`,
      `smoke_test_castpray.py`, `smoke_test_immortal_castpray.py`,
      `smoke_test_death_xploss.py`, `smoke_test_pk_gold.py`,
      `smoke_test_group.py`, `smoke_test_vitality_terrain.py` (also
      exercise vitals ticks, casting, and non-combat/combat death).

## Buildable now (no blocked dependencies)

Self-contained — no need for the object/mob systems. Keep working through
these; each ships with a smoke test + (if player-facing) a news entry.

### User batch 2026-07-19 (evening) — logged, not yet started

- [x] **Weather shouldn't affect INDOORS rooms** — done 2026-07-19. Root
      cause: `weather_announce()` (weather.c) notified EVERY connected
      character on a sky-state change regardless of room — someone
      standing inside a building would still see "Clouds begin to
      gather overhead"/"It begins to rain" despite being unable to see
      the sky at all. Fixed with the exact same `ROOM_FLAG_INDOORS`
      check `room_is_dark_for()` (being.c) already uses for the
      darkness half of this same "Weather & light levels" audit item —
      a weather-change announcement is exactly the kind of sky-
      visibility-dependent content that check exists for. Deliberately
      NO `ALWAYS_LIT` exemption here (unlike darkness) — a torchlit
      indoor room is still indoors, it just isn't dark; the two flags
      answer different questions. New
      `tests/smoke_test_weather_indoors.py` (3 checks) — forces real
      weather-state changes (probabilistic, same "force + poll" shape
      `smoke_test_weather.py` already uses) and confirms an outdoor
      character receives the announcement on the SAME tick an indoor
      one does not. Regression-checked against `smoke_test_weather.py`
      (also exercises weather ticks).
- [x] **`toggle tips` (mute the tips channel)** — done 2026-07-19. User:
      "tips channel should be a toggle to shut it off or turn it on
      again." Before this, the only way to silence the periodic
      pulse-driven tip echo (`tips_repo.c`'s `tips_pulse_tick()`) was
      `toggle newbie`, which also drops the player off the newbie help
      channel entirely (`cmd_newbie.c`) — a much bigger side effect than
      "stop showing me tips." Added a dedicated `PLR_NOTIPS` pflags bit
      (being.h, deliberately its own bit rather than reusing `PLR_NEWBIE`)
      and a `toggle tips` switch (cmd_toggle.c, "Communication" category,
      default on). `tips_pulse_tick()` now requires `PLR_NEWBIE` set AND
      `PLR_NOTIPS` clear, so tips still only ever reach newbie-flagged
      connections. New `tests/smoke_test_toggle_tips.py` (8 checks) —
      confirms the switch lists/flips/persists correctly and is genuinely
      independent of `toggle newbie`. Regression-checked against
      `smoke_test_toggle.py`.
- [x] **`level` command** — done 2026-07-19. User: "a level command that
      will display when your due for a gain in level, You have X
      experience and need X experience to level." Turned out the exact
      helper this needed already existed: `progress_xp_for_level()`
      (being.c), the same total-XP-to-reach-a-level curve
      `progress_add_xp()` levels a player up against, so `level` can
      never drift out of sync with a real level-up. New `cmd_level.c`:
      "You have X experience and need Y more experience to level." A
      mortal already at `MORTAL_LEVEL_MAX` (50) gets a distinct
      "already at the maximum mortal level" message instead of a
      nonsensical/negative need number; an immortal gets "don't gain
      levels through experience" (leveling past 50 is `promote`, not
      XP). New `tests/smoke_test_level.py` (8 checks) covers a fresh
      level-1 character, a partial-XP grant shrinking the need number
      by exactly the granted amount, the level-50 case, and the
      immortal case.
- [x] **Prompt: experience/mana/piety/vitality toggles + `prompt all`** —
      done 2026-07-19 (partial: mana/piety remain blocked, see below).
      `cmd_prompt.c`'s toggle set expanded beyond hp/gold/vit (vit
      shipped earlier) to also cover `exp` (current experience) and
      `expneed` (experience still needed to reach the next level, reusing
      `progress_xp_for_level()` -- the same curve `level`, cmd_level.c,
      shows -- clamped at 0 rather than negative for an immortal or a
      mortal already at `MORTAL_LEVEL_MAX`). New `prompt all` turns ON
      every currently-available toggle at once (always sets, never a
      toggle itself -- "give me everything" has one obvious meaning,
      unlike per-stat off which already has its own command). Mana/piety
      STILL blocked -- those resources genuinely don't exist anywhere in
      Tobin yet, unrelated to this item's scope. New checks added to the
      existing `tests/smoke_test_parser_display.py` prompt section (15
      checks covering exp/expneed/all) rather than a new file, matching
      where the hp/vit toggle tests already lived. Verified live on both
      preview and production, 2 clean full runs each -- one earlier run
      hit a pre-existing, unrelated flake (a telnet keepalive IAC NOP
      occasionally racing the test's strict `.endswith(">")` check on
      totally different lines each time, e.g. plain `look`/`score`/an
      unknown command -- none of which this change touched); confirmed
      not a regression by reproducing it on both untouched command paths
      across multiple runs.
- [x] **Animal races shouldn't carry wealth** — done 2026-07-19. User:
      "animal races should not have wealth, that doesnt make sense."
      Tobin's own 6 PLAYER races (Human/Elf/Ogre/Dwarf/Hobbit/Gnome)
      turned out to have nothing literally "animal" among them, so this
      wasn't a `progress.gold` gate on a PC race at all.
      AskUserQuestion-confirmed with the user: gate the existing mob
      gold-drop-on-kill (combat.c's combat_defeat()) by the mob's
      upstream `mob.race` column being a mundane real-world creature
      (RODENT, FELINE, BEAR, DEER, BIRD, FISH, SNAKE, INSECT, ...)
      instead — previously wholly deferred/display-only (only read
      directly from the DB for `stat`). `mob.race` is now loaded into
      `mob_proto_t` (mob_repo.c/h) and copied onto a new
      `being_t.mob_race` at spawn time (being_create_mob()); new
      `mob_race_is_animal()` (being.c, a 46-case lookup against the
      existing 127-entry MOB_RACE_NAMES[] table) deliberately excludes
      fantastical/sapient races (DRAGON, ORC, GOBLIN, UNDEAD, DEMON,
      ...) and plants/oozes/elementals (TREE, VEGGIE, MOSS, SLIME,
      ELEMENT) — the user asked about ANIMAL races specifically. XP is
      unaffected; only the wallet-stat gold drop is gated. New
      `tests/smoke_test_animal_no_gold.py` (6 checks) — a RODENT-race
      mob awards 0 gold (but still awards XP), an otherwise-identical
      NORACE mob still awards gold as before. Regression-checked against
      `smoke_test_pk_gold.py` and `smoke_test_death_xploss.py` (both
      also exercise combat_defeat()'s reward paths).
- [x] **Fix placeholder spell help files** — already resolved, confirmed
      2026-07-19. User found `help haste` still showing the literal
      generator placeholder text ("Description of what the spell is
      intended to do.") instead of a real description. Turned out this
      was already fixed the DAY BEFORE the report, by commit `ca55246`
      ("Add engage alias and real per-skill/spell help topics",
      2026-07-18) — that commit generated real, distinct
      `help_topic` rows for all 271 unique skill/spell names straight
      out of `skill.c`'s own roster (see that commit's own TODO.md
      entry for the full story). The user's report was most likely
      based on a stale client view from before that fix landed.
      Re-verified live 2026-07-19: `select body from help_topic where
      name='haste'` (and `fireball`, `cure poison`, `bless`) all show
      real per-spell descriptions, a broad DB search for the literal
      placeholder phrase ("intended to do") across all 424 help_topic
      rows returns zero matches, and `smoke_test_help_topics.py` (20
      checks) passes clean against production. No code change needed
      -- closing this out as confirmed-already-fixed rather than
      redoing completed work.

### User batch 2026-07-19 (later evening) — logged, not yet started

- [x] **Immortals see inventory when looking at a mob/player** — done
      2026-07-19. User: "immortals can see inventory when looking at a
      mob or player and can also see the contents of any container they
      carry." `look <target>` (cmd_look.c) now appends a carried-
      inventory listing after the equipment listing, immortal viewers
      only — worn/held items are excluded (already shown by the
      equipment section above it; reuses the same `is_loose()` check
      cmd_object.c's own `inventory` command uses, duplicated locally
      per this file's existing `cap_first()` duplication precedent), and
      an open container among the loose items gets one extra level
      shown inline ("It contains:"-equivalent), matching `look
      <container>`'s own single-level convention rather than a full
      recursive dump. A closed carried container shows "(closed)"
      instead. Looking at yourself reads "You are carrying:"; looking
      at someone else reads "<Name> is carrying:". Mortals see no
      change at all. New `tests/smoke_test_look_inventory.py` (10
      checks). Regression-checked against `smoke_test_look_equipment.py`.
- [x] **Wiznews pager freezes the MUD** — done 2026-07-19, fixed same-day
      as reported given "freezes the mud" severity. Root cause:
      `descriptor_page_start()` (descriptor.c) copies its whole source
      string into a FIXED `page_buf[16384]` via a bounded snprintf --
      silently TRUNCATING anything longer, no matter how big the
      caller's own source buffer was. `cmd_news.c`/`cmd_wiznews.c`
      already build up to a 101000-byte `full` string (a *previous*, real
      fix -- their own comment documents an earlier overflow at
      15000/16000 bytes) -- but that fix never reached `page_buf`, so
      once either feed grew past ~16KB (both have, after this session's
      own many wiznews posts) it silently cut off MID-SENTENCE with no
      indication anything was missing. Confirmed via live reproduction
      (not just static reading) that this is a severe multi-second-per-
      page STALL misread as an "elapsed time" artifact of the test
      harness's own timeouts on first pass -- the real, confirmed bug is
      the silent truncation itself: content just vanishes mid-word, the
      pager thinks it reached the end, and the reader is left staring at
      a broken, seemingly-stuck page that reads exactly like "the mud
      froze" even though the connection itself is fine underneath.
      Fixed by sizing `page_buf` to 131072 bytes, comfortably clearing
      both callers' 101000-byte ceiling with real margin. New
      `tests/smoke_test_wiznews_pager.py` (11 checks) -- deliberately
      does NOT reuse `smoke_test_news.py`'s existing (but currently
      broken for an unrelated, pre-existing reason -- old real headlines
      scrolling out of the 40-item recent-news window as the feed grows
      over many sessions, confirmed identical on the untouched
      production binary) "whole feed is shown" check; instead seeds its
      own large, uniquely-marked synthetic wiznews entries sized to
      cumulatively exceed the old cap, confirms every one -- including
      each entry's own LAST character -- survives paging intact, and
      confirms the fix actually matters by verifying this exact test
      genuinely fails against the old page_buf size (reproduced live on
      production before deploying the fix there). Regression-checked
      against `smoke_test_wiznews.py`, `smoke_test_news_followups.py`,
      `smoke_test_skills.py` (also exercise the shared pager).

### User batch 2026-07-17 — queued after Money/Shops, working these next

User's own list, cross-checked against existing entries below (most were
already tracked — pointers, not duplicates):

- `wipe` command + a real (non-hardcoded) master password — done
  2026-07-18, see **`wipe` (59+)** and **`wipe` master password** entries
  above.
- `dig`, `edaccount`, Typed logs, Tips system, PK opt-in flag, Diseases —
  all already tracked (their own entries above).
- News edit/delete, redit extra descriptions, personalized immortal log
  messages — News edit/delete is **News follow-ups**; extra descs is
  **redit Extra Descriptions**; personalized log messages already partly
  shipped under **Typed logs (LOG_GAME + personalized)** (LOG_JESUS-style
  per-immortal routing) — a duplicate stale **Typed logs** entry further
  down still lists `log search`-by-type as unbuilt, which is real/accurate,
  not stale.
- Meaningful limb damage, Thief "peek" skill — already tracked (their own
  entries above).
- [x] **Zone `A` opcode (random-room roll) — fixes 1119+ dead mob spawns**
      — done. Found chasing why the trigger just attached to mob 149
      (see the item above) never fired for a real player: the mob was
      never spawning through the normal zone reset system AT ALL, in
      ANY zone. Root-caused against the real SneezyMUD C++ source
      (`sneezymud-master/code/code/sys/db.cc`): the upstream zonefile
      opcode `A <lo> <hi>` rolls a random room number in that inclusive
      range and stores it as the zone's current "random room"
      (`ZONE_ROOM_RANDOM = -99` in the original, `db.h`); any following
      `M`/`O` command whose own room arg is that same `-99` sentinel
      uses the rolled room instead of a literal vnum, for the rest of
      that reset pass. Tobin's `zone_execute()` (`src/core/zone.c`)
      only ever implemented six opcodes (M/O/E/G/P/D) -- `A` fell into
      the same silent "unhandled opcode" bucket as a dozen others,
      which meant its own `-99` sentinel was never substituted with
      anything, and `zone_get_room(-99)` always failed. Confirmed via
      direct query: **1119 `M` rows and 25 `O` rows across the whole DB**
      use `-99` as their room -- these are wandering/ambient mobs by
      theme ("grimhaven youth", "scarred tomcat", "filthy dog", "ugly
      crow", ...), never a fixed spot, exactly matching the mechanic's
      purpose. New `zone_cmd_random_room()` (rolls arg1..arg2, retries
      up to 10x on an invalid candidate, matching the original's
      shape) + `zone_resolve_target_room()` (substitutes the current
      random room for the `-99` sentinel, used-as-is otherwise) wired
      into `M`/`O`'s existing handlers. Verified live: preview's boot
      mob/object count jumped from 649/587 to 769/652 with this fix in
      (120 more mobs, 65 more objects now successfully spawning
      world-wide); scanned the full 101-244 room range used by mob
      149/148's own `A` rolls and found them landing in real, different
      random rooms (105, 119, 138, 211) across repeated resets, exactly
      as designed. Zone-related smoke tests
      (edzone/zone_identity/zonefile/zones) all still pass --
      `smoke_test_zones.py`'s one failure (room 200 expected "Farm
      House", is actually "Inside the City Gates") is confirmed
      pre-existing/stale, unrelated to this change.
- [x] **Zone reset: world-wide `max_exist` gate (urgent follow-up to the
      `A` opcode fix above)** — done. User: "i used scan and got 50-60
      mobs listed." Real regression the `A` fix exposed, same day: a
      ZONE_ROOM_RANDOM (-99) mob lands in a DIFFERENT room on every
      reset, so `zone_cmd_load_mob()`'s existing per-room cap
      (`zone_count_in_room()` -- "is this ROOM already full of this
      vnum") was trivially satisfied every single time, since the mob
      is (almost) never in the same room twice. Confirmed live in the
      log: zone 100's periodic reset alone added ~190 fresh mob
      instances on top of whatever already existed, on EVERY firing --
      unbounded growth, not a one-time bump. The original engine gates
      this with a world-wide `mob_index[vnum].getNumber() >=
      max_exist` check (`sys/db.cc`); ported as `zone_world_count()` +
      a check in `zone_cmd_load_mob()`/`zone_cmd_load_obj_ground()`
      before creating anything (not after, unlike `load`'s existing
      warn-only version, cmd_load.c -- a zone reset REFUSES over cap,
      it doesn't just nag). Verified live: forced zone 100 to reset 3
      times in a row after the fix landed, then did a full room-by-room
      scan of its own 101-244 random range -- mob 149 (max_exist=8)
      sat at exactly 8 live instances, not climbing. A production
      restart was needed alongside the code fix, not instead of it --
      nothing persists live mob/object state, so a fresh boot is what
      actually clears the bloat the running process had already
      accumulated; the code fix is what stops it from recurring.
- [x] **Trigger `wait`/`say` actions** — done. User (2026-07-19, pasted a
      Monty-Python-esque market-vendor script -- "wait 1 / say Larks'
      tongues. / wait 1 / say Wrens' livers. / ..."): "i want to put this
      script on a mob." The trigger vocabulary had no `say` (only `emote`,
      which doesn't render "says, '...'") and no pause primitive at all --
      a script ran start-to-finish in one synchronous pass
      (trigger_run()). Confirmed with the user before building (this
      breaks the system's own documented "deliberately small, not a
      general-purpose language" design intent, trigger.sql) rather than
      picking a direction unprompted. `say <text>` renders "<Name> says,
      '<text>'" to the room, same shape as `emote`. `wait <seconds>`
      (1-3600, clamped) pauses everything AFTER that line and schedules a
      real continuation (trigger_pending_tick(), ~1s pulse cadence,
      main.c) -- the paused actor is deliberately NOT preserved across the
      pause (may be long gone by the time it resumes), only room/self are,
      safely RE-DERIVED fresh at resume time from the trigger's own
      target_type/target_vnum rather than holding a raw pointer across an
      unbounded real-time gap. `aitick` (cmd_aitick.c) forces pending waits
      along too (`trigger_pending_force_all()`), for deterministic
      testing. Caught two real bugs building this: (1) the new pulse
      registration silently exceeded `MAX_PULSE_PROCESSES` (16, pulse.c)
      and got dropped with only a log line, no boot failure -- `wait`
      would have just never fired in production; bumped to 24. (2)
      `aitick`'s original ordering ran the pending-force step in the SAME
      loop iteration as the random-trigger fire that might just have
      scheduled a NEW wait, immediately resolving it and collapsing the
      pause into a no-op within a single `aitick 1` call -- reordered so
      each iteration resolves what was ALREADY pending before creating
      anything new. `tests/smoke_test_trigger_wait.py` -- its own first
      version used `wait 1`, which raced against the server's real ~1s
      background pulse and sometimes resolved before the test's own
      "hasn't fired yet" check ran; fixed by using `wait 3600` so only the
      explicit `aitick` force can resolve it.
- [x] **Split gold on kill** — SOLO case done 2026-07-19 (see the fuller
      "Split victim's gold among the group on kill" entry below for the
      writeup); the group-split half remains blocked on the not-yet-built
      group/party system, tracked there.
- Expand `prompt` toggles, Boxed-menu rework for the remaining editors —
  already tracked (their own entries above).
- Mid-fight persistence (HP only saves at defeat — a real crash-loss risk)
  — already tracked: **Mid-fight persistence** entry below.

### User batch 2026-07-17 (continued) — shutdown, shop numbering, branding

- [x] **`shutdown` command** — done 2026-07-17. User: "write a shutdown
      command to kill the mud kindly along with a time function that will
      shutdown in <X> seconds", then "shutdown should be level 60 only",
      then "shutdown should display a countdown from 5 seconds until
      shutdown to everyone" ("shutdown in 5, 4, 3, 2, 1, shutdown"). Bare
      `shutdown` counts down from 5 seconds (the new default); `shutdown
      <seconds>` counts down from any number instead; `shutdown cancel`
      aborts a countdown in progress (not explicitly asked for, added as a
      minimal safety valve). Implementor-only (60). Every connected
      character is saved before the process actually exits ("kindly" --
      `player_save()`, same call `save` uses). Ticks via the pulse
      scheduler at 1-second granularity (`shutdown_pulse_tick()`,
      shutdown.c) rather than blocking the game loop the way `copyover`'s
      5-second `sleep()` does, so a long countdown doesn't freeze anyone's
      play. `game_loop_request_shutdown()` (game_loop.h/.c) reuses the
      exact same clean-exit path SIGINT already took. Help topic added.
- [x] **Numbered shop `list` + `buy <#>`** — done 2026-07-17. User: "should
      also number the list of items in a shop so a player can buy #".
      `list` now shows each item's 1-based position; `buy 3` buys that
      exact item, same as typing its name -- both index into the exact
      same stable `shop_repo_producing()` order, so the number always
      means the same item whether or not `list` was run first.
- [x] **`<h>`/`<H>` tag + SneezyMUD branding cleanup** — done 2026-07-17.
      User noticed a literal `<h>` in a book's title and asked what it
      meant. Turned out to be a real, never-ported original-game feature
      (sys/colorstring.cc's `colorString()`): NOT a color code at all, a
      name-template substitution -- `<h>` inserts `MUD_NAME`
      ("SneezyMUD" in the original), `<H>` a versioned variant. Ported as
      literal-text substitution in `colorstring_translate()`
      (colorstring.c), both aliasing "TobinMUD" (no separate
      versioned-name concept here). Then, user: "SneezyMUD should be
      TobinMUD, replace all instances of SneezyMUD with TobinMUD in the
      database" -- surveyed every text column across the seed DB and
      fixed the 4 unambiguous rows (a book's name/short_desc/long_desc,
      two room descriptions, one item's extra-description, plus the
      book's own search keyword). Deliberately left 2 `wiznews`
      dev-changelog entries untouched (user confirmed) -- they correctly
      name SneezyMUD as the original codebase this port is based on
      ("unlike SneezyMUD's old spec proc system"), not a branding
      artifact. DB-content fix applied live only, same as the earlier
      talens→gold fix -- not captured in tobin_migrations.sql, so a
      from-scratch fresh install would need it re-applied by hand.

### User batch 2026-07-11 (continued) — working these next

- [x] **`practice <discipline>` now shows the skill listing anywhere**
      — done 2026-07-17. User reported testing `practice combat` away
      from a guildmaster: "You don't see a Combat guildmaster of your
      discipline here" -- then: "this command should display the skill
      listing for that discipline along with percentage of proficiency
      unless in front of a guildmaster then the offer to practice still
      applies along with the percentage of each seperate skill/spell
      proficiency." Reworked `cmd_practice.c`'s `<discipline>` form to
      split on whether an explicit count was given: `practice combat`
      (bare, no count) now ALWAYS shows that one discipline's skill/spell
      listing with each accessible skill's own individual proficiency
      (new `practice_show_discipline()`, reuses `skill_proficiency()`
      from the same-session per-skill-proficiency work), with NO
      guildmaster required -- if one happens to be present, a training
      reminder is appended. Only `practice combat <count>` (an EXPLICIT
      count) still spends practice points, and that form is unchanged --
      still requires the matching guildmaster present. This is a real
      behavior change from before (bare `practice combat` used to
      silently spend exactly 1 point); the new split is deliberate --
      checking status shouldn't cost a scarce resource. Help topic +
      wiznews entry updated to document the split clearly.
- [x] **`set` grew practices/basic/combat/advanced fields** — done
      2026-07-17. User: "need the ability for the set command to adjust
      practices and any other stat you can think of, we'll get in the
      habit of updating set with new items as we go." Added 4 new
      `apply_field()` cases to `cmd_set.c`: `practices`/`practicepoints`
      (progress.practice_points, >=0), `basic`/`combat`/`advanced`
      (the three discipline percentages, 0-100, matching `practice
      <discipline>`'s own vocabulary). No new online-sync code needed --
      the existing sync loop already copies the whole `progress_t`
      struct wholesale. **Standing habit going forward**: `set` should
      grow a matching field whenever a new player-facing stat gets
      added (like this session's practice_points/*_disc_pct did before
      this item, and skill proficiency conceivably could later -- though
      per-skill fields don't fit `set`'s one-name-one-value shape as
      cleanly, so that may need its own subcommand if ever added).
- [x] **Per-skill proficiency (Sneezy-style learn-by-doing)** — done
      2026-07-17. User: "when typing skill or any other item that has
      long output, pass it to pagination" (see the pagination item below)
      then, separately: "also as far as skills/spells are concerned, they
      should gain access to a skill by practicing, but the actual gain in
      proficiency should be gained as in sneezy." Researched Sneezy's real
      mechanic first (`code/code/misc/skills.cc`/`discipline.cc`): each
      skill/spell has its OWN 0-100% proficiency, separate from the
      coarse discipline-tier ACCESS gate (Tobin's `*_disc_pct`, unchanged)
      -- proficiency climbs via `learnFromDoing()` on every attempt (win
      or lose), the gain chance shrinks as it nears a ceiling set by the
      discipline percentage (Wisdom softens the curve), and it then gates
      a real d100 success roll. Confirmed scope with the user via
      AskUserQuestion before building (matching the practice-redesign
      precedent for multi-part features): real success roll (not just
      cosmetic tracking), hooked into `cast`/`pray` plus every
      already-mechanically-wired skill (`settrap`/`disarmtrap`, dual
      wield; sanctuary comes free since it's invoked via `pray`).
      New `player_skill` table (player_id, skill_name, pct, last_gain_at
      -- one row per player per skill actually attempted; unattempted =
      0%). New `skill_repo.h`/`.c` (DB access) + `skill.c` additions:
      `skill_find()` (exact-name lookup, for callers that already know
      the name rather than parsing player input), `skill_proficiency()`
      (read-only getter, for display), `skill_learn_from_doing()` (the
      gain-check + increment, returns the resulting pct), `skill_roll_success()`
      (d100 vs pct). Formula is a Tobin-simplified port: integer exponent
      (1/2/3 by Wisdom tier around ATTR_BASE) instead of Sneezy's
      continuous float exponent via `pow()`, avoiding a new libm
      dependency (matching practice.c's own no-`math.h` precedent from
      earlier this session) -- `chance = 1000 * headroom^power`, floored
      at 15/1000, first-ever attempt floors at 1%. A flat 30s anti-grind
      cooldown replaces Sneezy's two-tier 30s/3min. `cmd_cast.c`/
      `cmd_pray.c`: after the existing component/symbol gate, roll
      proficiency (immortals always succeed); the material is still
      consumed either way, a failure just fizzles ("You fumble the
      casting/prayer..."). `cmd_trap.c`: `settrap`/`disarmtrap` roll the
      same way -- a fumble wastes the attempt (door stays un/re-trapped)
      but still nudges proficiency; a fumbled disarm deliberately does
      NOT spring the trap on the disarmer (kept non-punishing, v1 scope).
      `combat.c`'s dual wield check: learn-by-doing only, no roll (it's a
      passive stance already gated by the existing binary
      `being_knows_skill()` check, unaffected) -- PCs only, mobs have no
      practice-points system to hang this on. `cmd_skills.c` shows each
      known skill's proficiency in brackets (e.g. "bash [34%]"). Help
      topics (`cast`/`pray`/`skills`/`settrap`/`disarmtrap`) + wiznews
      entry updated. **Not yet done**: no dedicated smoke test written
      this session (time-boxed to shipping the mechanic + docs); the
      full sweep before the eventual repo push will need one.
- [x] **Pagination for long-output commands (`skills`/`bug`/`idea`/`rules`)**
      — done 2026-07-17. User: "also when typing skill or any other item
      that has long output, pass it to pagination." `skills` (now showing
      three discipline percentages plus every tier, worse for immortals
      who see all 6 classes at once), `bug`, `idea`, and `rules` all
      built an unpaginated buffer and dumped it in one `descriptor_send()`
      -- same bug class `news`/`wiznews` already had fixed earlier this
      session. All four now go through the existing shared pager
      (`descriptor_page_start()`, `descriptor.c`) instead: one screen at
      a time, ENTER for more, Q to stop. `skills`' immortal branch was
      also restructured to accumulate all 6 classes into one buffer and
      page it once, instead of sending each class's block immediately
      per-iteration (which bypassed pagination entirely for that path).
- [x] **Room-listing stacking (`(xN)`)** — done. User: "in look at room,
      object stacking and mob stacking. for 2 gremlins you would see
      A gremlin is standing here. (x2)." `cmd_look.c`'s room loop used to
      print one line per THING; now `render_room_item()` renders each
      thing's line first, and `group_room_items()` groups identical
      RENDERED STRINGS together (rather than needing a separate vnum-
      equality check) before printing -- two mobs of the same vnum, or
      two objects with the same long_desc, naturally stack; PCs (always
      unique names) and visually-distinct objects are naturally
      unaffected. A group of 2+ gets a trailing " (xN)"; a lone item gets
      none. Stacking happens within each of the existing two fixture/
      non-fixture passes independently. `tests/smoke_test_room_stacking.py`
      covers 3-mob and 3-object stacking, a lone mob showing no suffix,
      and two different mobs never merging.
- [x] **Fix blank-tick catchup bug** — done. User: "catchup: -- What you
      missed while editing -- / -- end of held messages -- / no message.
      could that be caused by the blank prompt we are sending? also,
      lets remove the \r\n from the end of the prompt." Root cause:
      `heartbeat_tick()`'s half-hourly blank-line "tick" (Session:
      "send a blank line ... so a tick becomes apparent") used
      `descriptor_notify()`, which HOLDS it for catchup if the recipient
      is mid-editor -- a held "\r\n" replays as an invisible blank line,
      making catchup look empty. Fixed: skip descriptors currently in
      an editor entirely (a blank tick has nothing worth catching up
      on); for everyone else, send "" instead of "\r\n" (the game loop's
      prompter already opens every prompt with its own "\r\n\r\n", so
      the tick's own newline was just doubling the blank line) --
      `descriptor_send()` still marks `needs_prompt` on an empty string,
      so the visible-tick effect is unchanged for non-editing players.
- [x] **`balance` command (gamewide class/race modifiers, 60+)** — done.
      User: "a balance command (60) where you take args: balance
      <class|race> that is menu driven to adjust balance
      numbers/modifiers that will apply gamewide to the class or race
      you just balanced." Chose 4 concrete, already-real-formula-backed
      modifiers per class/race (not a per-skill system -- nothing like
      that existed to plug into): HP multiplier (being_calc_max_hp()),
      damage multiplier and to-hit modifier (combat_strike()), AC
      modifier (being_total_ac()). New `class_balance`/`race_balance`
      tables (db/sneezy/balance.sql), one row per player_class_t/
      player_race_t value, seeded neutral (1.0/1.0/0/0) -- an untouched
      class/race behaves exactly as before. New `balance_repo.h`/
      `balance_repo.c` (raw DB access) + `balance.h`/`balance.c` (an
      in-memory cache, loaded once at boot via `balance_cache_load()`
      in main.c, so combat's hot path never hits the DB per swing;
      `class_balance_set()`/`race_balance_set()` write through to both
      DB and cache on Save). `mob_class_known` guildmaster mobs (task
      47) get the class modifiers too (mobs have no race). New
      `balance class|race <name>` command (Implementor-only,
      `BALANCE_MIN_LEVEL 60`) opens a menu-driven editor -- same
      working-copy/Save/Quit-with-unsaved-changes-prompt shape as
      `edzone`/`edplayer` (CONN_BALANCE_* in descriptor.c, added to
      `descriptor_in_editor()`'s catchup-hold range same as every
      other menu editor must). `tests/smoke_test_balance.py` covers the
      60+ level gate, the neutral starting values, editing+dirty-
      marking+Save, the change actually raising a fresh Warrior's max
      HP by the exact expected amount, and Quit-with-unsaved-changes
      offering Save/Discard/Cancel (with Discard verified to never hit
      the DB). Hit two test-authoring bugs along the way: character
      names with digits in them ("Bal51xxxx") are rejected by character
      creation (names must be letters only) -- silently derailing every
      subsequent step in the test until traced back; and a raw SQL
      reset of the balance tables between test runs fixes the DB row
      but leaves the *already-running* server's in-memory cache stale
      -- fixed by resetting through the real `balance`/Save command
      path instead, which updates both.
- [x] **Affects system (buffs/debuffs/status)** — done (self-assigned
      backlog item, sequenced right after trap mechanics per user
      2026-07-11's "...then weapon depth, trap mechanics" continuation).
      New general-purpose infrastructure: `active_affect_t { affect_type_t
      type; int rounds_left; }`, a fixed `MAX_ACTIVE_AFFECTS=4` array per
      being (`affect.h`/`affect.c`), ticked down every combat round
      (`affect_tick_run()`, registered at `COMBAT_ROUND_PULSES` alongside
      `combat_process_run`) independent of whether the being is currently
      fighting, so a buff wears off outside combat too. Proven with one
      real flagship effect rather than built speculatively: the Cleric
      spell "sanctuary" ("a strong aura that reduces incoming damage",
      previously falling into the generic "isn't implemented yet"
      placeholder branch same as every other spell) now actually applies
      `AFFECT_SANCTUARY` for 12 rounds and halves whatever damage the
      target takes in `combat_strike()`, applied last so it discounts the
      fully-modified hit. New `affects` command lists what's active and
      how many rounds are left, or "(none)". `tests/smoke_test_affects.py`
      covers: a fresh character's `affects` is empty; `pray sanctuary`
      applies it and `affects` shows a positive round count; a 20-sample
      statistical comparison of incoming damage with/without Sanctuary
      active; and natural expiry ("Your Sanctuary wears off.", `affects`
      empty again). Found and fixed a genuine, previously-unknown
      production bug while writing this test: `player_load()`
      (`player_repo.c`) called `being_create_pc()` (which sizes limb HP
      off level-1 defaults) and then overwrote `progress.level`/`max_hp`
      from the DB, but never re-synced limb HP to match -- so EVERY
      reconnect for a character above level 1 left their limbs stuck at
      level-1-sized fractions, trivially decapitatable regardless of real
      max_hp. Fixed by calling `being_limbs_full_heal()` again right after
      `player_progress_load()`. Three test-authoring snags along the way:
      (1) a raw socket close reconnects to the still-linkdead live
      `being_t` (skipping `player_load()` entirely, so the fix above never
      triggers) -- must `quit!` first to actually free the character, same
      as `smoke_test_armor.py`'s established pattern; (2) mutual combat
      (`combat_process_run()` strikes both directions every round) means
      either combatant can die first at random -- fixed by giving both
      sides a huge `set_hp()` (reusing `smoke_test_weapon_depth.py`'s
      helper) so 20+ rounds of real damage doesn't risk death, and giving
      the attacker a huge `set_dex()` gap so the target's automatic
      retaliation essentially never lands; (3) Sanctuary's 12-round
      duration can expire mid-sampling (20 hits can take longer than 12
      rounds to land) -- its "wears off" message would otherwise be
      silently consumed by the sampling loop's own polling before a later,
      dedicated check ever saw it, so `average_incoming()` now returns the
      raw text it saw too, for the expiry check to also scan.
- [x] **Spell/skill affects expansion** — done 2026-07-18. User:
      "implement spell/skill affects and write help files for each
      including what symbol/component/commodity is needed to cast/pray.
      Make each work from sneezy code." Expanded `cast`/`pray`'s
      keyword-pattern effect dispatch (cmd_cast.c/cmd_pray.c) well
      beyond the original heal/damage/Sanctuary-only v1, reusing THIS
      session's own disease/poison/affect work rather than hand-building
      ~30 bespoke spell mechanics: (1) exact-name "cure poison"/"cure
      disease" now genuinely remove `AFFECT_POISON`/any active disease
      (self for `cast`, self-or-a-named-target for `pray` — the same
      target-resolution `pray heal <target>` already had); (2) a much
      wider armor/shield/resistance-flavored keyword family (stone skin,
      barkskin, self-wards, armor, bless, plasma mirror, reflective
      shield, ...) all apply the same `AFFECT_SANCTUARY` ward "sanctuary"
      itself uses; (3) Cleric's own real roster entries "poison" and
      "disease"/"infect" now genuinely inflict `AFFECT_POISON`/a random
      disease (`affect_random_disease()`, new in affect.h/.c) on
      `ch->fighting`, mirroring the pre-existing damage branch's
      targeting/messaging conventions exactly. New
      `tests/smoke_test_cure_and_inflict.py` covers all of the above
      live (13 checks) using a fresh Cleric+Warrior pair and the mutual-
      combat-survival trick from `smoke_test_affects.py` (huge
      `set_hp()`/`set_dex()`, plain `hit` not `kill`/`attack`).
      Deliberately still out of scope, same as v1: mana costs (no mana
      pool exists in Tobin at all), an elemental-resistance/damage-type
      model (so damage-flavored spells stay generic, not typed), and
      anything needing a subsystem Tobin doesn't have yet at all
      (teleport/summon/portal/polymorph/invisibility-flavored spells) —
      those still fall through to the honest "nothing happens yet"
      placeholder. The "help files for each spell" half of the request:
      FIRST attempt judged writing ~300 individual `help_topic` rows
      (one per spell) inconsistent with how this codebase's help system
      is used elsewhere, and substituted an inline note in
      `skills`/`practice` instead — user corrected this 2026-07-18:
      "also no spell helpfiles? that was my instruction. help files for
      all skills/spells along with required components spell
      components should be listed in the footer before related" — that
      was the actual ask, not a suggestion to reinterpret. Redone
      properly: a one-shot Python script parsed skill.c's `SKILLS[]`
      array directly (name/class/tier/level/description, so it can't
      hand-transcription-drift from the real roster) and generated
      **271 real `help_topic` rows**, one per unique skill/spell name
      (`db/sneezy/skill_help.sql`; 25 names are shared across more than
      one class — mostly identical weapon-proficiency rows, plus a
      handful of same-named Cleric/Druid entries like "cure poison" —
      merged into one topic listing every class/tier/level that has it,
      rather than colliding on `help_topic`'s name primary key). Each
      topic names what `cast`/`pray` actually needs (a spell component,
      a holy symbol, the real command it already routes through instead
      for Thief's door-trap skills, or an honest "not yet wired to a
      command" for the many still-placeholder Warrior/Thief/Monk
      physical skills). New `Requires:` trailing-directive convention
      added to `cmd_help.c` (`extract_trailing_directive()`, generalized
      from the existing `Related:` line) renders this as its own
      cyan-labelled footer line, positioned before `Related:` as asked.
      "disease" deliberately excluded from the generated set — it
      collided with a pre-existing general-mechanic topic (drink a
      puddle → catch a disease), hand-merged into a rewrite of that
      topic instead (also fixed its stale "4 diseases" claim while at
      it, and noted the Cleric prayer CAN land on an immortal opponent
      unlike the puddle roll). Found and fixed a real, previously-latent
      bug while wiring this up: `help <topic>` only ever looked up the
      FIRST whitespace token of its argument, so multi-word topics
      resolved via a `LIKE 'firstword%' ORDER BY name LIMIT 1` fallback
      — harmless while few topics shared a first word, but "cure poison"
      silently landed on "cure blindness" once several generated
      "cure ___" topics existed. Fixed by trying the full trimmed
      argument as an exact (or prefix) topic name FIRST, falling back to
      the original first-token behavior only when that fails — verified
      not to regress the pre-existing two-word `edit <noun>` special
      case, alias resolution (ne/nw/se/sw/'), or ordinary single-word
      abbreviations. `skills`/`practice <discipline>`'s inline per-class
      reagent note stays too (a real convenience, just not a substitute
      for the actual help files). `tests/smoke_test_help_topics.py`
      extended with checks for exact multi-word resolution, the
      `Requires:` footer's content on a couple of representative topics,
      and a regression check on `edit room`. Live-testing note: an IMMORTAL
      attacker's `kill`/`attack` always instakills any target in one
      command regardless of stats (`cmd_kill.c`'s `combat_instakill()`,
      by design, confirmed by reading the source after two dead-end
      live-test attempts against a rat AND a level-127 mob) — `hit`
      (real multi-round combat, "never instakill" even for immortals)
      is the correct command whenever a test needs a target to survive.
- [x] **`engage` command** — done 2026-07-18. User: "add an engage
      command that alias for hit." Full alias, same one-handler-two-
      table-rows pattern `attack`/`kill` already use (`cmd_table.c`):
      `{ "engage", cmd_hit, ... }`. No abbreviation conflict (nothing
      else starts with "en"; single-letter "e" is already claimed by
      the pinned movement head, east). `help engage`/`help hit` each
      point at the other as an alias. Covered by
      `tests/smoke_test_help_topics.py`.
- [x] **Skill/spell level curve rescaled to 1-25-50** — done 2026-07-18.
      User, reacting to a help topic showing "Mage (Class, level 68)":
      "im not sure what that means. they should have all skills/spells
      by level 50 splitting the discs in a scaled manner. by the time
      they complete basic and gained 100% proficiency they should be
      level 25, same for combat disc, advanced should go from 25 to 50."
      A real, previously-unnoticed problem: `IMMORTAL_LEVEL_MIN` is 51,
      so ANY `min_level` above 50 in `skill.c`'s roster (dozens did --
      up to 99) was permanently unreachable by a mortal, immortal-bypass
      only. Rescaled every entry's `min_level` with a one-shot Python
      script (parsed skill.c's `SKILLS[]` directly, same script family
      as the help-topic generator): grouped by (class, tier), computed
      each group's own current min/max, linearly remapped into the
      target envelope per tier (`SKILL_TIER_COMBAT`/`SKILL_TIER_CLASS`
      -> [1,25], `SKILL_TIER_ADVANCED` -> [25,50]) independently per
      class (so a narrow-spread class doesn't get artificially
      stretched by a wide one), rounding to the nearest integer and
      preserving relative order/ties. 233 of 305 entries changed value;
      re-verified per-group min/max landed exactly on the target
      envelope for every one of the 18 (class, tier) groups.
      `skill_help.sql` (271 generated help topics, see the entry above)
      necessarily regenerated afterward to match -- caught and fixed a
      latent bug in that file's own apply pattern while doing so: its
      `ON DUPLICATE KEY UPDATE name=name` is a deliberate no-op (by
      design, to protect in-game `hedit` edits from a reseed), which
      meant simply re-running the file after regenerating its content
      silently did NOTHING to already-existing rows -- fixed for this
      one-time need with an explicit `DELETE ... WHERE updated_by='seed'
      AND name IN (...)` before re-applying (help_topic.sql's normal
      convention is per-topic `UPDATE ... WHERE name=X` instead, which
      doesn't have this problem; skill_help.sql's machine-generated bulk
      nature made that impractical here). Fixed two pre-existing, unrelated
      test assumptions this rescale exposed rather than caused: (1)
      `smoke_test_immortal_castpray.py`'s single un-paged `skills` read
      never actually covered Mage's now-105-entry roster (the real pager
      caps at 20 lines; this test happened to work before only because
      nothing had pushed Mage's listing past that boundary yet) -- fixed
      by paging through with blank-line continuations; (2)
      `smoke_test_affects.py`'s Cleric fixture set
      `basic_disc_pct`/`advanced_disc_pct` but never `combat_disc_pct`,
      which `sanctuary`'s Advanced-tier gate has required all along
      (live-confirmed via direct DB query, unrelated to today's level
      change) -- fixed by setting all three, plus bumped its `set_hp` up
      (2000 -> 8000) after live-observing an occasional death by limb
      severance during its long combat-sampling window (Tobin's death
      check is per-limb, not the aggregate HP pool, so a big total-HP
      cushion is a probabilistic safety margin, not a guarantee).
- [x] **Spell components have real charges; holy symbols genuinely
      decay** — done 2026-07-18. User: "how long does each component
      last? should be getting 10 casts out of each component and the
      symbols should decay as in sneezy." Previously every component/
      symbol was single-use regardless (destroyed on every `cast`/`pray`
      attempt, success or fail). Researched the original source directly
      (misc/obj_component.h/.cc, misc/discipline.cc's
      `requireHolySym()`) rather than guessing: SneezyMUD's `TComponent`
      genuinely has a `charges` counter ("use up one charge... else
      discard it as worthless"), and `TSymbol` genuinely has a
      `strength`/`max_strength` pool that decays a variable amount per
      prayer (scaled by the caster's effective spell level SQUARED in
      the original, further multiplied if badly overpowering the
      symbol's own rated level) and can outright shatter mid-prayer if
      overstressed.
      Ported the real SHAPE of both mechanics, not the exact formulas
      (Tobin has no per-symbol "level" rating to make the original's
      overpower multiplier meaningful, and inheriting its raw numbers
      wholesale would flatly contradict "10 casts" above -- see below):
      `obj.h`'s val[]/val[1] (previously unused/decorative for these two
      keyword-identified item types) now hold current/max charges
      (component) or current/max strength (symbol). A component spends
      exactly 1 charge per `cast` ATTEMPT, destroyed only once the last
      charge is spent (`cmd_cast.c`'s new `consume_component()`); a
      symbol loses a random 1-2 strength per `pray`/`continue` attempt,
      shattering only once strength runs out (`cmd_pray.c`/
      `cmd_continue.c`'s new, duplicated `consume_symbol()` -- same
      "small helper duplicated per command file" convention this
      codebase already uses throughout). `tobin_migrations.sql` seeds
      every real component/symbol row to 10/10 -- UNCONDITIONALLY, not
      guarded on val0=0/val1=0 like a normal idempotent migration,
      because many holy symbol rows turned out to already carry huge
      leftover val0/val1 from the upstream import (up to 1.8 MILLION,
      val2 uniformly -1, val3 uniformly 0 -- plausibly the real
      upstream TSymbol strength values under the level-squared formula,
      but meaningless at Tobin's much smaller scale and directly
      contradicting the user's explicit "10 casts" spec, so reset
      rather than inherited). New `tests/smoke_test_component_charges.py`
      confirms live: a component survives EXACTLY 10 casts then reports
      "is used up" and is gone; an 11th attempt correctly finds nothing;
      a symbol survives more than one prayer (unlike the old single-use
      behavior) and eventually reports "shatters from the stress of the
      prayer." Regression-verified against `smoke_test_castpray.py`
      (single-cast consumption still works when val0/val1 default to 0,
      via a "treat an uncharged/legacy item as 1 fallback charge"
      clause in both new helpers, rather than refusing outright).
- [x] **Trap mechanics (door traps)** — done (self-assigned backlog
      item, sequenced right after weapon depth per user 2026-07-11:
      "...then weapon depth, trap mechanics"). Wired the Thief's
      long-defined-but-unused "set trap (door)"/"disarm trap"/"detect
      trap" skills (skill.c) up to `EXIT_COND_TRAPPED` -- a new bit
      (room.h) alongside a room exit condition bit ("Trapped") that
      already existed, named, builder-editable via redit's toggle
      submenu, but had literally no behavior before this. New
      `settrap <direction>`/`disarmtrap <direction>` commands
      (cmd_trap.c): `settrap` requires the door be closed (rig an open
      door doesn't make sense) and not already trapped; both are
      gated on the caller actually knowing the corresponding skill
      (`being_knows_skill()`, task 11's helper) rather than a level-
      table entry, so a non-Thief (or a too-low-level/unpracticed
      Thief) gets the same "Huh?!" as an unknown command. `cmd_move.c`'s
      `do_move()` is where a trap actually matters: walking through a
      trapped door springs it (random-limb damage, one-shot -- the bit
      clears in memory AND the DB) unless the mover knows "detect
      trap" (Combat tier, always known), in which case they spot it
      and step around -- leaving it rigged for whoever comes next,
      since avoiding a trap shouldn't consume it (only actually
      springing should). v1 scope: door traps only (skill.c also lists
      arrow/container/mine/grenade trap variants, deferred -- door
      traps were the one variant with ready-made infrastructure to
      hook into, matching the AC-from-weight/sharpness-from-verb
      precedent of picking one concrete mechanic per backlog item
      rather than building every variant speculatively). Trap damage
      is a placeholder `being_hurt_limb()` call, same "honest, not
      wired into full combat_defeat()" scope as spell damage
      elsewhere. `tests/smoke_test_trap.py` covers: settrap's three
      refusals (open door, no door, already trapped); settrap/
      disarmtrap hidden from someone who doesn't know the skill; a
      Warrior springing a rigged door and the trap clearing afterward;
      a Thief detecting and avoiding one, confirmed still rigged
      afterward; and disarmtrap clearing a still-rigged door. Two
      test-authoring snags: `transfer` needs its target ONLINE (must
      reconnect before transferring, not after); and re-closing a door
      via raw SQL doesn't take effect on the ALREADY-LOADED live
      room_t (same "SQL doesn't refresh live in-memory state" lesson
      as task 47's level-setting bug) -- fixed by using the real
      `close` command instead.
- [x] **Weapon depth: sharpness + dual wield** — done (self-assigned
      backlog item, sequenced after AC/to-hit per user 2026-07-11:
      "Armor & Protection (AC) go in next, complete the to-hit/defense
      formula depth, and then weapon depth, trap mechanics"). Two flat
      modifiers folded into `combat_strike()`'s existing damage roll:
      (1) sharpness -- any weapon whose `weapon_verb()` classification
      isn't the blunt "bludgeon" (slice/chop/stab/pierce) gets +1
      damage, reusing the verb bucket already computed for messaging
      rather than adding a new weapon property; (2) dual wield -- the
      Warrior/Thief "dual wield" skill (skill.c's roster, already
      described as "passively reduces the damage penalty for your
      off-hand weapon" but never wired up) now actually does that: the
      usual off-hand -1 becomes a plain 0 for anyone who knows it,
      checked via new `being_knows_skill()` (skill.h/skill.c -- level +
      discipline-percentage gate, immortals always know everything,
      reusable for future skill-mechanic work). `tests/smoke_test_weapon_depth.py`
      covers both: a fresh Warrior's `skills` already lists dual wield
      (Combat tier) while a Cleric's roster has no such entry at all
      (fast, deterministic), and a sharp dagger's average damage over
      30 real hits beats bare hands by a clear margin (statistical,
      immortal attacker so the mortal 1.2s post-swing cooldown doesn't
      apply). Iterating on this test surfaced two real bugs along the
      way (both described below): `hit`/`kill` only INITIATE a fight,
      the pulse scheduler resolves the actual rounds asynchronously
      every 1.2s, so repeatedly re-sending the command doesn't work
      the way repeatedly polling for the async result does; and a
      training dummy fighting back can decapitate an attacker whose
      limb HP never scaled with a (real or SQL-boosted) higher max HP.
- [x] **Fix: leveling up never raised max HP or limb HP** — done (real
      bug, not a testing artifact, found while building the weapon-depth
      test above). `progress_add_xp()` (being.c) only ever bumped
      `level` -- since it works on a bare `progress_t` with no access
      to attrs/kind, it never called `being_calc_max_hp()` or
      `being_limbs_full_heal()`, so a player's max HP (and every limb's
      own HP cap) stayed frozen at their level-1 starting values
      forever, no matter how high they leveled -- leaving even a very
      high-level character exactly as vulnerable to a lucky limb-crit/
      decapitation as a brand new one. Fixed at the one call site
      (combat.c's `combat_defeat()`, which already has the full
      `being_t` winner): on `levels_gained > 0`, recompute
      `winner->progress.max_hp` and fully heal (`being_limbs_full_heal()`),
      a full-HP restore as part of the level-up reward. New
      `tests/smoke_test_levelup_hp.py`: records a fresh Warrior's
      starting max HP, kills 8 very weak mobs (enough XP to reach
      level 2), confirms max HP increased afterward. Two more test-
      authoring bugs found and fixed while building this one: `kill`
      has the same async-combat-round timing as `hit` (see above); and
      a raw SQL `load_room` edit on a player who's still linkdead from
      an earlier abrupt disconnect (socket closed without `quit!`)
      gets ignored -- `enter_world()`'s linkdead-resume path takes
      priority over `load_room` -- fixed by using the `transfer`
      command instead of relying on `load_room` at all.
- [x] **`continue` command + targeted heals + breaking holy symbols** —
      done. User: "add a continue command so clerics that heal <target>
      can continue automatically until the target is fully healed or
      thier holy symbol breaks (holy symbols should use the same logic
      as components for mages and druids)." Three changes: (1)
      `cmd_pray.c`'s heal-type prayers ("heal light" etc) can now target
      someone else in the room ("pray heal light <target>") instead of
      only the caster -- new `find_spell_and_target()` tries the whole
      `args` string against the spell roster first (self-heal, fully
      backward compatible with every existing test/usage); only if that
      fails does it peel off the last word as an optional target and
      retry the remainder as the (possibly abbreviated, possibly multi-
      word) spell name, so "heal lig joe" and "heal light joe" both
      resolve correctly. (2) Holy symbols are now consumed via
      `obj_destroy()` on every successful pray, exactly like a
      component -- no longer the permanent keepsake originally shipped;
      updated `tests/smoke_test_castpray.py`'s assertion (and its
      Mage/Cleric test characters' level-setting, which had the same
      "SQL-after-connect never reaches the live being_t" bug the practice
      test hit) to match. (3) New `cmd_continue.c`: `continue` repeats
      the caster's most recent heal-type prayer (`being_t.last_heal_target`/
      `last_heal_spell`, set by cmd_pray.c, cleared by being_destroy()
      if the target goes away) on the same target, once per holy symbol
      on hand, ALL within one command call ("continue automatically"),
      stopping the instant the target leaves the room, is fully healed,
      or the caster runs out of holy symbols ("their holy symbol
      breaks") -- capped at 50 rounds as a pure safety valve, not a
      gameplay limit. Deliberately does not re-check class/level/
      discipline-percentage each round (already validated when the
      original prayer set the state; `continue` only replays its
      effect + consumes its resource). `tests/smoke_test_continue.py`
      covers self-pray being unaffected, targeted pray healing someone
      else, `continue` with nothing to continue being refused, and
      `continue` running the loop to one of its two stopping conditions
      and then being refused again afterward.
- [x] **`practice` command + guildmaster-gated discipline percentages**
      — done. User: "add the practice command so players have to visit a
      guildmaster to gain skills based upon percentage of discipline
      learned. cant get to advanced disc until basic disc is at least
      95% complete." A player's Basic (SKILL_TIER_CLASS) and Advanced
      (SKILL_TIER_ADVANCED) discipline are each tracked as a single
      0-100 aggregate percentage (new `player_progress.basic_disc_pct`/
      `advanced_disc_pct` columns, `progress_t` fields) -- deliberately
      NOT per-skill, matching the user's own wording ("percentage of
      discipline learned") and avoiding a new per-player-per-skill table
      before the roster has real bespoke mechanics anyway. `cmd_cast.c`/
      `cmd_pray.c` now additionally require `basic_disc_pct > 0` for a
      Class-tier spell and `basic_disc_pct >= 95 && advanced_disc_pct > 0`
      for an Advanced-tier one (bypassed for immortals, same spirit as
      task 45's class/level bypass); `cmd_skills.c` shows both
      percentages and marks locked entries with the specific reason
      (level vs. discipline). New `cmd_practice.c`: `practice` (status)
      / `practice basic` / `practice advanced` (+10% per use, capped at
      100, no resource cost yet -- same "no economy to hang it on" v1
      scope as cast/pray's missing mana) -- requires a "guildmaster"-
      keyworded mob of the player's own class in the room. Wiring a
      guildmaster's class required finally loading the previously-
      wholly-deferred `mob.class` column (confirmed via the live DB to
      be a BITMASK -- 1 mage/2 cleric/4 warrior/8 thief/16 shaman/32
      deikhan/64 monk/128 ranger/256 other, verified against the seeded
      guildmaster mobs vnum 200-229): `mob_repo.c`/`mob_proto_t` gained
      `class_mask`, and `being_create_mob()` maps the single-class bits
      with a real Tobin equivalent (ranger -> Druid, matching the
      Druid roster's own Ranger lineage; shaman/deikhan/other stay
      unmapped, no Tobin class fits) into the mob's `char_class` +
      new `mob_class_known` flag. `tests/smoke_test_practice.py` covers
      no-guildmaster refusal, wrong-class-guildmaster refusal, the 0%-
      Basic prayer refusal, `practice basic` raising the percentage and
      unlocking the prayer, `practice advanced` being refused below 95%
      Basic and working above it, the Advanced-tier prayer unlocking
      only once Advanced percentage is nonzero, and `skills`'s
      percentage display. (Hit one test-authoring bug along the way:
      the test originally set the Cleric's level to 90 to sidestep an
      unrelated level gate, which crossed IMMORTAL_LEVEL_MIN=51 and
      silently bypassed the very discipline gate under test -- fixed by
      using level 40 instead, and setting it before the final
      login/reconnect rather than after, since a live being_t doesn't
      pick up a raw SQL level change without a fresh load.)
- [x] **Immortals bypass class restrictions on skills/spells** — done.
      User: "immortals can use any skill or spell in game, no class
      restrictions." `cmd_cast.c`/`cmd_pray.c`'s class gate
      (`ch->char_class != CLASS_MAGE/CLASS_DRUID` / `!= CLASS_CLERIC`) and
      each command's `find_spell()` (previously hard-restricted to the
      caller's own class) now both short-circuit for
      `being_is_immortal(ch)` -- an immortal's `find_spell()` searches
      every class's roster, and the spell's `min_level` check is skipped
      too (an immortal shouldn't be locked out of a level-99 spell they're
      testing). The component/holy-symbol item requirement is
      deliberately NOT bypassed -- that's an item gate, not a class
      restriction, same spirit as `cast`/`pray`'s existing design.
      `cmd_skills.c` got the matching discoverability change: an immortal's
      `skills` now prints every class's full 3-tier roster under a
      `=== <Class> ===` heading (all shown as known, sidestepping the
      level-dimming) instead of just their own class. New
      `tests/smoke_test_immortal_castpray.py` covers an immortal Warrior
      reaching and successfully casting a Mage spell and praying a Cleric
      spell (still needing the component/symbol item each time), an
      immortal's `skills` output containing other classes' sections, and
      a regression check that a same-class mortal Warrior is still
      refused entirely.
- [x] **`cast`/`pray` with component/holy-symbol requirements** — done
      (v1 scope). User: "clerics should require a holy symbol to pray
      successfully, druids and mages should require components to cast
      with, so implement task_pray task_cast etc." New `cmd_cast.c`
      (Mage/Druid, `cast <spell>`) and `cmd_pray.c` (Cleric,
      `pray <spell>`) -- both gated on class, then the spell's
      `min_level` (looked up in skill.c's roster, restricted to
      non-Combat-tier entries), then the required item: `cast` needs any
      carried/worn/held object keyworded "component" (consumed on
      success); `pray` needs one keyworded "symbol" (NOT consumed --
      a holy symbol is a keepsake, unlike a material component). Each
      command's core logic lives in a `task_cast()`/`task_pray()`
      function, matching the literal naming the user asked for. **v1
      scope, not full spell mechanics**: Tobin has no mana/resource pool
      yet (a prerequisite this backlog doesn't have built), and the full
      ~150-entry roster doesn't each have a bespoke effect -- a spell
      whose one-line description mentions "heal"/"cure" heals the
      caster, one mentioning damage-flavored words (bolt/blast/strike/
      etc) damages the caster's current fight opponent if any, everything
      else casts/prays successfully (consuming its component/needing its
      symbol) but says its effect "isn't implemented yet" -- honest about
      what's real vs. placeholder rather than silently doing nothing.
      Real per-spell mechanics remain a follow-up (the "Offensive spell
      system"/"Affects system" backlog items). `tests/smoke_test_castpray.py`
      covers class gating, unknown-spell rejection, the component-vs-
      symbol requirement gate, and that a component is consumed while a
      holy symbol isn't.
- [x] **Druid skill/spell roster** — done. User: "go with the druid
      spells/skills" (confirming the proposal). Custom blend (not a
      direct Sneezy class port): Ranger's real non-stub nature/animal
      skills (barkskin, beast soother, feral wrath, sky spirit, tree
      walk), a subset of Cleric's heal/utility ladder, and several of
      Shaman's working damage spells renamed/reflavored to a nature
      theme (entangling roots/thorn barrage/sunscald/storm call/wave
      crash/withering touch/wild agony/nature's wrath/wildfire/leeching
      vine, from root_control/distort/blood_boil/stormy_skies/
      aquatic_blast/lich_touch/soul_twist/deathwave/raze/vampiric_touch
      respectively). Shaman's totem/golem/undead-thrall/possession lines
      deliberately excluded -- poor thematic fit regardless of renaming.
      Added to `tests/smoke_test_skills.py`'s per-class loop.
- [x] **Combat capitalization audit (mob display names)** — done. User:
      "some areas that should be proper case still arent, review and find
      them and fix them." Delegated a research pass first rather than
      guessing; confirmed two related bug classes beyond the trigger.c/
      cmd_object.c fixes already made this session: (1) three more
      color-tag-skip-before-capitalizing gaps (combat.c's death-trigger
      firing, cmd_move.c's greet-trigger firing, cmd_say.c's speech-
      trigger firing -- each had its own un-guarded `toupper(capbuf[0])`);
      (2) a bigger one the audit flagged as worth a separate look: nearly
      every combat message (miss/hit/limb-status/death/corpse-description)
      read a mob's `base.name` directly -- for a mob that's the raw
      space-separated KEYWORD list ("lady stroll walk", matched by `look
      lady`/`look stroll`/`look walk`), never a display string, producing
      exactly that garbled text in combat output and -- worse -- baked
      permanently into a killed mob's corpse description. New
      `being_display_name()`/`being_display_name_cap()` (being.c) pick
      short_descr for a mob / base.name for a PC, lowercase-mid-sentence
      or capitalized-sentence-initial; replaced every affected read in
      combat.c, cmd_attack.c, and (the earlier-flagged raw-`base.name`
      bug class) mob_ai.c's `mob_try_scavenge()`/`mob_try_aggress()`
      (only `mob_try_wander()` had been fixed previously). New
      `tests/smoke_test_mob_display_name.py` fights a mob with a
      deliberately multi-keyword name and checks every combat message and
      the resulting corpse description.
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
      **Extended 2026-07-18** (user: "make look board, look 2.board to
      look at second board... make it true as part of everything that
      can exist, l mob, l 2.mob, kill 2.mob, etc."): `look`/`examine` had
      been the one real gap left -- `look_at_target()` (cmd_look.c) only
      ever matched the first PC/mob/object, no ordinal support at all.
      Now uses the same `thing_parse_ordinal()` primitive as everywhere
      else, for both the being search and the object search (room floor,
      then carried/worn/held). Also newly wired into `drink`/`sip`
      (cmd_drink.c/cmd_sip.c, "2.puddle" when more than one pool/fountain
      matches), `open`/`close`'s container lookup (cmd_open.c), `show`'s
      item token (cmd_show.c), `sell` (cmd_shop.c, "2.sword" among loose
      carried items), and `read`/`write`'s board disambiguation
      (cmd_board.c, "read 2.board" alongside the existing "read at
      <name>" form). `buy` was deliberately left alone -- it already has
      its own numbered `buy <#>` (from `list`'s own numbering), a
      cleaner disambiguator than an ordinal would add on top of a
      catalog that can't actually contain duplicate-keyword entries.
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
- [x] **Sweep `help_topic.sql` for redundant level-gate phrasing** — done
      2026-07-17. Follow-up to the help-format reformat (`snoop`'s body
      was the original worked example). Checked `cmd_help.c` first: the
      cyan `Minimum Level:` footer only appears when the topic's `name`
      EXACTLY matches a `cmd_table.c` entry (`strcasecmp` against
      `resolved`) -- so a lead-in like "Administrator (59+) only:" is
      truly redundant ONLY for topics with a real, single-level command
      entry. Stripped it from 15: `balance`, `copyover`, `delbug`,
      `delidea`, `edbug`, `egotrip`, `exec`, `gametog`, `load`, `purge`
      (bare-purge sentence only), `set`, `stat`, `vnum`, `zone`,
      `zonefile`. Deliberately LEFT ALONE: `administration` (the numbers
      are the topic's actual subject -- explaining the level ladder --
      not boilerplate), `edit`/`trigger`/`edit player`/`edit rules` (none
      of these four have an exact-name `cmd_table.c` match -- "edit"
      dispatches sub-nouns at DIFFERENT levels internally, and "edit
      player"/"edit rules"/"trigger" as topic names never match "edit"'s
      own table entry -- so no footer ever shows for them, and their
      inline level text is the ONLY place that information appears).
      `purge`'s `linkdead` sub-form's "(58+)" was also kept -- the base
      `purge` command's own table entry is level 51, so the footer alone
      doesn't cover the elevated sub-case. Applied directly to the live
      DB via targeted `REPLACE()` (verified per-row before and after),
      then mirrored into `help_topic.sql`'s AUTHORITATIVE source line for
      each (several topics have a later `UPDATE ... WHERE updated_by =
      'seed'` overriding their original `INSERT` -- fixed whichever one
      actually wins). Side finding while at it: `copyover`'s LIVE topic
      turned out to be a completely different, much older placeholder
      body ("The copyover command will allow an higher level immortal
      reboot the mud...") -- the seed file's polished `Usage:`/footer-
      style text existed on disk but had never actually reached the DB
      (an existing row + the seed's `ON DUPLICATE KEY UPDATE name=name`
      no-op silently skipped it). Pushed the seed's version live too.
      **Not investigated further, flagging as a possible follow-up**:
      this same "seed file has the intended text, but an existing DB row
      + a no-op ON DUPLICATE KEY guard means it was never actually
      applied" pattern could affect OTHER topics beyond `copyover` --
      would need a systematic live-vs-seed diff across all ~140 topics
      to find out, which is a bigger task than this session's scope.
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
- [x] **Account menu: hide the character list until `C`, boxed style** —
      done 2026-07-17, merged with the "Boxed ASCII-art menu rework" item
      below (same feature, described twice). User supplied the exact
      before/after wireframe, then later "i created 3 text files for the
      new menus that contain ascii art. use those to create new menu
      output" -- `box1.txt` (a blank double-line `╔═╗║╚╝` frame) for the
      letter-menu container.
      New `descriptor_t.char_list_shown` (descriptor.h): false by default
      (calloc'd), true only after a bare `C` reveals the list for the
      REST of that menu visit -- every sub-flow return (cancelled
      creation, cancelled/failed deletion, ...) naturally preserves
      whatever state was already set just by never touching the flag,
      except `descriptor_leave_to_menu()` (quit!-while-playing, combat
      defeat, `rent`), which explicitly resets it to false since that's a
      genuinely FRESH arrival at the menu, not a same-visit return.
      New `send_boxed_menu()`/`visible_len()` (descriptor.c): a reusable
      double-line-box renderer, auto-sized to its widest line, that
      measures width around `<X>` color tags (zero screen columns either
      way, color on or off) rather than counting them -- so it's not tied
      to box1.txt's own fixed 82x22 dimensions (which is really a style
      swatch/glyph reference, not a literal template; a giant empty box
      around 5 short lines would look absurd). `show_account_menu()`:
      hidden state shows the 5-line boxed letter-menu (`C`/`N`/`D`/`X`/`Q`,
      bright-cyan letters); revealed state (originally a plain "-- Your
      players --" numbered list, see the follow-up below) ends "Choose a
      number to connect that player to the game: " (matches the user's
      exact wireframe wording) instead of the old repeated C/N/D/X/Q
      footer. `C <number|name>` (already-known target) still connects
      directly, unchanged -- confirmed the sensible default rather than
      re-asking, given the note already leaned that way. A single-
      character account still auto-connects on bare `C` without ever
      needing the reveal.
      7 existing smoke tests referenced the old literal text ("Your
      characters", "C [number|name]", "Connect which one?") and needed
      updating to match: `smoke_test_kill.py` (the slain target's
      single-character account now only proves it via a live reconnect,
      not by scraping its name out of the menu text -- a single-char
      account never lists names as text at all now), `smoke_test_combat.py`,
      `smoke_test_crit.py`, `smoke_test_quit.py`, `smoke_test_quit_creation.py`,
      `smoke_test_quit_menu.py`, `smoke_test_rent.py` (all switched to
      checking for "Connect Player"/"(none yet)" instead),
      `smoke_test_menu_letters.py` (the dedicated menu-letters test --
      updated its 3 literal-text assertions to match the new box/reveal
      wording). Verified live against the running server, all 8 pass.
      **Follow-up, same session**: user: "place the character list in
      connection inside a box, colorize the number list with <C>" -- the
      revealed listing (was a plain "-- Your players --\n  1. Name
      [Level]" list) is now ALSO boxed via `send_boxed_menu()`, one line
      per character, its number colorized `<C>N.<z>` bright-cyan; the
      "-- Your players --" heading is gone (redundant once it's visibly
      a menu box). Then: "do the same for character creation menu" --
      `show_race_screen()`/`show_class_screen()`/`show_alignment_screen()`
      (the three numbered-choice screens between name entry and playing)
      got the same treatment: each option's full multi-line description
      is now one `send_boxed_menu()` call (auto-sized to its widest
      line, ~70-75 chars given the existing prose), with each leading
      "N)" recolored `<C>N)<z>`. The point-buy attribute screen
      (`show_attr_screen()`) was left alone -- it's a live-updating
      table/pool display, not a numbered-choice menu, a different shape
      that doesn't fit this same box-per-option pattern. No smoke test
      referenced the exact old option-list text (only "Choose a race"
      substring, unaffected), so no test updates needed; verified live
      (`smoke_test_menu_letters.py`, `smoke_test_quit_creation.py`) plus
      manual preview of all three screens on an isolated test port.
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
- [x] **General output pagination (20-line threshold)** — done 2026-07-17.
      Turned out `descriptor_page_start(d, text, 0)` (descriptor.c) was
      ALREADY the reusable helper asked for: it's a safe drop-in
      replacement for `descriptor_send()` -- text under the ~20-line
      `page_size` default goes out in one shot with zero pager UI (no
      "ENTER for more" line, identical to plain `descriptor_send()`),
      only genuinely-long text arms the pager. No new function needed,
      just retrofitting call sites. Surveyed every `cmd_*.c` for a final,
      single-buffer reply whose length is realistically unbounded and
      switched its closing `descriptor_send(d, out)` to
      `descriptor_page_start(d, out, 0)`: `cmd_help.c`'s `send_columns()`
      (covers both `help` and `wizhelp`'s command listings -- the
      clearest case, since the command table only grows), `cmd_who.c`,
      `cmd_users.c` (both scale with concurrent connections), `cmd_stat.c`
      (both `stat_player()` and `cmd_stat()` -- generic DB-column dumps,
      already routinely 20-40 lines for a real mob/room), `cmd_object.c`'s
      `cmd_inventory()` (unbounded carried-item count), `cmd_look.c`'s
      container-contents branch in `look_at_target()` (unbounded, but NOT
      the bare per-move room look -- deliberately left untouched, a
      pager UI interrupting normal movement flow would be a real UX
      regression, not a fix), `cmd_log.c`'s `log list`, `cmd_edtrigger.c`'s
      `edit trigger list`. Left alone as genuinely bounded (checked each
      against its real cap, never realistically &gt;20 lines):
      `cmd_equipment`/`cmd_limbs` (`LIMB_COUNT`=13), `cmd_affects.c`
      (`MAX_ACTIVE_AFFECTS`=4), `cmd_score.c`. `cmd_log.c`'s `log <n>`/
      `log search` are a known gap -- genuinely unbounded (up to
      `LOG_TAIL_MAX`=100 / `LOG_MATCH_MAX`=20 lines) but send each ring-
      buffer line via its OWN `descriptor_send()` call in a loop rather
      than one accumulated buffer, so retrofitting needs restructuring
      first (build one string, or extend the pager to accept a line
      array) -- left as a follow-up, not done this pass.
      3 smoke tests broke from real pagination now actually kicking in
      (`help`'s command list, `stat`'s DB-column dumps) and needed a
      page-draining helper added to their `cmd()`/send+recv wrapper:
      `smoke_test_help.py`, `smoke_test_help_content.py`,
      `smoke_test_stat.py` (same drain-until-no-"ENTER for more" pattern
      `smoke_test_skills.py` already used from the earlier per-command
      pagination work). ~20 other tests touching `who`/`users`/
      `inventory`/etc. were spot-checked live and unaffected (their
      output stays under the threshold in normal test conditions, where
      pagination is correctly a no-op).
- [x] **Point-buy attribute screen boxed too** — done 2026-07-17. Follow-up
      to the account-menu/creation-screen boxing above: user: "allocate
      attribute menu should be boxed in the same way." `show_attr_screen()`
      (descriptor.c) was deliberately left out of the earlier pass (it's a
      live-updating table, not a numbered-choice menu, a different shape)
      but boxes cleanly regardless -- commands list + current
      attribute/handedness/gender/appearance values, all inside one
      `send_boxed_menu()` box, header ("-- Allocate attributes for X --")
      and the bare "> " prompt outside it, matching every other creation
      screen. Verified live: initial values render correctly, and an
      adjustment (`str 20`) redisplays the box with the changed value and
      recalculated points-remaining.
- [x] **`pee <liquid>`** — done 2026-07-17. User: "pee should be able to
      pee liquid types, pee defaults to pee, pee <arg> tries to find a
      matching liquid type and leave a puddle of that liquid type."
      `cmd_pee.c`: new `PEE_LIQUIDS[]` catalog (pee/water/wine/beer/acid,
      each with its own `obj_grow_pool()` keywords), prefix-matched
      against `<arg>` same as every other command's abbreviation
      convention; bare `pee` still defaults to plain pee, unchanged.
      An unrecognized type is refused with the valid list rather than
      silently falling back. A DIFFERENT liquid starts its own separate
      puddle in the room (obj_grow_pool() only merges into a puddle whose
      keywords already match the requested type) -- verified live:
      `pee water` twice grew one water pool, `pee` afterward left a
      SEPARATE pee puddle alongside it, both visible in `look`. No
      broader "Liquids" system exists yet (still a separate, unbuilt
      TODO item below) -- this is a small fixed catalog scoped to `pee`
      itself, easy to extend with more entries later. `pool_noun_color()`
      (obj.c) already falls back to plain white for any noun besides
      blood/pee, so the three new liquids needed no color-table changes
      unless a future pass wants each its own color. Help topic and
      command-table one-liner updated to mention the new form.
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
- [~] **Socials → DB + full Sneezy set + `edsocial` (55+)** — DB-port half
      done 2026-07-20: socials moved from the compiled table to a `social`
      DB table (`social_repo.h/.c`), full ~155-verb set ported from
      `sneezymud-master/lib/actions` via `db/import-socials.py` (position-code
      translation verified against the original's `mapFileToPos()`, `$`-token
      grammar verified against `comm.cc`'s `act()`), targeting yourself now
      gets its own dedicated message instead of repeating the no-target one,
      `socials` list is paged (4-column). `smoke_test_socials.py`. **Still
      open:** `edsocial` (55+, menu-driven editor) to edit socials in game —
      not started.

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
- [x] **`wipe` (59+)** — done 2026-07-18. `wipe <name> <password>` /
      `wipe account <name> <password>`, ported from the original's
      `doWipe()`. Only strictly-lower-level targets (or, for an account,
      every character on it) can be targeted -- naturally blocks
      self-wipe too. Every player-scoped table cascades on delete
      (player_progress, player_attrs, player_inventory, and ~20 more --
      confirmed via an FK survey), so `player_delete()`/`account_delete()`
      alone is genuinely everything, no manual per-table cleanup. An
      online target is disconnected, belongings drop to the room floor,
      then the being is fully removed (not left linkdead). Logged to
      `[GAME]` (visible to online immortals), closing the "Account-delete
      logging lands with `wipe`" note above.
- [x] **`;` wiznet shorthand** — done 2026-07-05: `;<msg>` broadcasts to
      immortals (cmd_dispatch special-case, like `'` for say).
- [x] **`alias` command** — done 2026-07-17. New `account_alias` table
      (`account_id`, `tier`, `name`, `expansion`, tobin_migrations.sql) +
      `alias_repo.h`/`.c` (get/set/remove/list, following the same shape
      as this session's `skill_repo`/`balance_repo`). `cmd_alias.c`: bare
      `alias` lists (paginated -- `descriptor_page_start`, not boxed,
      matching every other in-game list command; boxes are for the pre-
      login menu screens only), `alias <name>` shows one, `alias <name>
      <expansion>` sets/overwrites, `alias remove <name>` deletes. Capped
      at `ALIAS_MAX_PER_TIER` (20) per account per tier -- editing an
      EXISTING alias never counts against the cap, only adding a
      genuinely new name does. Expansion wired into `cmd_dispatch()`
      (cmd_table.c), checked right after the hardcoded `quit!` special-
      case (so `quit!` itself can never be shadowed) and before the wait-
      state gate: looks up `verb` against the caller's account+tier
      (`being_is_immortal()` picks 'mortal' vs 'immortal'), and on a hit,
      re-dispatches ONCE on `expansion + " " + args` -- an alias is never
      itself re-expanded (only the resulting real command's own verb can
      match next), so a two-alias naming cycle can't loop. Command-table
      entry placed alphabetically between `attack`/`affects` (an already-
      swapped pair protecting bare "a") and `bug` -- no abbreviation
      collisions. Verified live: set/list/show/overwrite/remove all work;
      `k self` (aliased to `kill`) correctly routes through the real
      `kill` command's own target parsing; promoting the SAME account to
      immortal and reconnecting shows an EMPTY alias list and the mortal
      alias does NOT expand -- tier scoping confirmed working both ways.
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
- [x] **Holdable items** — already shipped: `hold`/`wield`/`switch`
      commands (cmd_object.c) -- entry pruned 2026-07-17, was stale
      (marked "BLOCKED on Objects/2C" long after Objects landed and these
      shipped in a later session).
- [x] **`point` social: reference the held item** — done 2026-07-18.
      `point`/`point <target>` now reads `ch->held[0]` (socials.c) and
      substitutes it into the message ("You point around with your
      <item>.", "X points at you with his/her/its <item>.") whenever
      something's actually held; falls back to the original plain
      random-point wording exactly as before when empty-handed.

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
- [x] **`wipe` master password** — done 2026-07-18, alongside **`wipe`
      (59+)** above: `TOBIN_WIPE_PASSWORD` (config.c), read once from the
      environment at process start, no fallback value anywhere in the
      source (the original hardcoded a literal "ole'chicken" --
      misc/immortal.cc). `wipe` refuses outright if it's unset.

### User batch 2026-07-05 (night) — BLOCKED on Objects (Phase 2C)

- [x] **Money system** — done 2026-07-17. Shipped scoped-down from the
      original spec: GOLD-COIN-ONLY wallet stat (`player_progress.gold`,
      same shape as practice_points — not a pickupable/lootable object).
      Commodities (ingots/nuggets/shards) deliberately NOT built — out of
      scope per the Shops entry below's own simpler spec. Mobs drop gold
      on defeat (`combat.c`'s `combat_defeat()`, level-scaled). Legacy
      seeded "pile of talens" treasure objects (16 vnums, ITEM_MONEY/
      `OBJ_CAT_MONEY`) had their flavor text renamed talens→gold for
      consistency. **Follow-up done same day**: picking one up now
      auto-credits its `val[0]` coin amount to the wallet and destroys the
      object (`pick_up_money()`, cmd_object.c) — wired into all three
      `get` pickup paths (room floor, `get <item> <container>`, `get all
      <container>`), so these never actually sit in inventory. User,
      2026-07-17: "once you pick up an object that contains gold it
      should increase your wealth and get rid of the obj in inventory."
- [x] **Components and commodities** — done 2026-07-18 (user: "implement
      components and commodities again from sneezy"). Turned out to need
      no new code: `ITEM_COMPONENT`/`RAW_MATERIAL`/`RAW_ORGANIC` (obj.type
      30/42/50) already collapse into a working generic category
      (`OBJ_CAT_OTHER`, obj.c), 380 real objects of these types already
      exist in the upstream seed, and real `shoptype` rows already declare
      which shops buy each category — get/drop/inventory/buy/sell all
      already worked generically. The actual gap: every one of these
      objects' upstream zone_reset placements sat in a disabled or
      missing zone, unreachable, and the shops flavored as component/
      commodity dealers (Camron's Components, Katherine's/Brightmoon's,
      Logrus, Xanesla, Amber, Tuvar's hides-and-herbs) had empty
      `shopproducing` catalogs — nothing to actually buy.
      `tobin_migrations.sql` stocks those six already-live shops with a
      curated sample of the game's own real component/raw-material/
      organic vnums (no fabricated content). **Real bug fixed as a
      side effect**: `cast`'s existing component-consumption requirement
      (see "`cast`/`pray` with component/holy-symbol requirements" above)
      had no reachable source anywhere in the world for the "component"-
      keyworded item it needs — mages/druids could never actually cast
      until Camron's Components had something to sell. Verified live:
      buying a component at Camron's and casting a spell consumes it.
      The original's full merge-stack/decay/alchemy component system
      (`obj_component.cc`, 2471 lines) remains out of scope — these are
      plain, individually-vnum'd objects, same as everything else sold
      in Tobin's shops.
- [ ] **Liquids** — drinkable liquids; pouring one out pools on the ground
      (from Sneezy). Needs objects/containers.
- [ ] **`fill`** — fill a container from a liquid pool. Needs liquids+objects.
- [x] **`switch`** — already shipped alongside `hold`/`wield`
      (cmd_object.c) -- entry pruned 2026-07-17, was a stale duplicate.
- [x] **`examine`** — already shipped as a synonym for `look <target>`
      (cmd_examine.c, `look_at_target()` shared with cmd_look.c) -- entry
      pruned 2026-07-19, was stale. Keyword extra-descriptions (redit's own
      side of this) remain a separate, still-open item -- see "redit Extra
      Descriptions" below.

- [x] **Druid class** — already shipped as one of the 6 selectable classes
      (see `show_class_screen()`, descriptor.c) -- entry pruned 2026-07-17,
      was stale (still marked open after Classes landed in a later
      session).
- [x] **Vitality stat + Terrain movement cost** — done 2026-07-19, closing
      the orphaned "Vitality"/"Depends on Vitality" fragment that used to
      sit here (its own bullet marker and title had gone missing from the
      file at some point; confirmed via grep 2026-07-17 that no such stat
      existed yet). See the full writeup under "Sneezy → Tobin feature
      audit" below for the Vitality stat and terrain-cost details --
      `player_progress.vit`/`max_vit`, regen tick, score/prompt display,
      `sector_move_cost()`'s 6-tier bucketing, and `cmd_move.c`'s
      average-of-two-sectors charge, same shape this fragment described.
- [x] **Socials/actions** — done 2026-07-05: 15 socials (smile/nod/wave/bow/
      cheer/poke/...) in `socials.c`, checked in dispatch after the command
      table; untargeted + targeted forms; `socials` lists them. Room echoes
      go through `descriptor_notify` (held for editors). More can be added to
      the table; a DB-backed/editable social set (`edsocial`?) is future work.
- [x] **Health strings** — done 2026-07-05: `being_health_word()` maps HP%
      to a word (near death ... perfect); shown in `score`'s HP line.
      Optional follow-up: also show it in the prompt (prompt-flag system).
- [x] **PK opt-in flag** — done 2026-07-18. `toggle pk` (player.pflags
      bit `PLR_PK_OPTIN`, being.h). Gated in `combat_find_room_target()`
      (combat.c) rather than at the command layer, so `attack`/`kill`/
      `hit` all get it for free -- a PC target that hasn't opted in (or
      whose attacker hasn't) is simply invisible to targeting, same as a
      linkdead PC already is. Mob targets are always fine; either side
      being immortal bypasses the gate entirely (instakill and the
      existing immortal-vs-immortal guard, cmd_kill.c, are unaffected --
      this only governs mortal-vs-mortal). Default off (opt IN, not
      out).
- [x] **Tips system** — done 2026-07-18. DB-backed (`tip` table,
      tips_repo.h/.c, seeded with a real starter set) like news/help, but
      kept one-liner-flat like `bug`/`idea` rather than a full menu
      editor -- a tip is one short sentence, not long-form titled
      content. `tips` (mortal) shows one random tip on demand;
      `tipedit <text>`/`tipedit list`/`tipedit delete <id>` (53+) manage
      the pool. `tips_pulse_tick()` (~10min cadence) echoes a random tip
      to every connected player currently on the newbie channel
      (`PLR_NEWBIE`, already existed) -- "per-player newbie toggle" reuses
      that flag rather than adding a second one.
- [x] **Typed logs** — already done, clarified/closed 2026-07-19, was
      stale. Both halves already shipped, just never checked off: `log.h`'s
      `log_type_t` taxonomy (SILENT/GAME/PIO/COMBAT/BUG/IDEA/DB/EDIT/
      JESUS/TEST) has been in real use throughout the codebase for a long
      time (`game_log(<type>, ...)` calls everywhere; per-type on/off via
      `setsev`), and every line `game_log()` writes to the file is ALREADY
      tagged verbatim as `[TYPE] message` (`descriptor.c`'s `game_log()`:
      `log_info("[%s] %s", log_type_name(type), msg)`), regardless of
      whether it's echoed live -- confirmed against a real log file.
      `log search`'s existing case-insensitive substring match (cmd_log.c)
      therefore already filters by type today: `log search [combat]`
      matches only COMBAT-tagged lines. No dedicated `log search
      type <type>` syntax was added on top of that -- the substring form
      already covers it with no functional gap.
- [x] **`dig`** — done 2026-07-18. `dig <direction>` (BUILD_MIN_LEVEL): if
      there's no exit that way yet, creates a new room, wires this room's
      exit to it and its own exit back (REV_DIR), then walks the digger
      through exactly like a normal move (`cmd_dispatch()` of the
      direction word itself, so every bit of `do_move()`'s own logic --
      arrival triggers, poofin/poofout, the fighting/position gates --
      applies unchanged). The next-free-vnum strategy: the new room lands
      within the CURRENT room's own zone range (one query,
      `room_repo_next_free_vnum()`, not a per-vnum exists() loop), gated
      by the same `zone_can_edit()` ownership check `edroom`/`edtrigger`
      already enforce -- refuses on an unzoned room (no range to place
      from) or a zone whose range is already full.
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
- [x] **`edit account`** (accounts) — done 2026-07-18, as `edit account
      <name>` (Administrator, 58+, matching edplayer's tier) -- unified
      under the `edit <noun>` dispatcher from the start, never a
      standalone `edaccount` verb (user, 2026-07-11:
      "unify all ed* commands into one edit command"). Menu-driven, but
      unlike edplayer/edzone/balance NOT a working-copy-plus-Save editor
      -- rename and password reset each commit immediately (same
      reasoning as edzone's builder-assignment toggle), since there's no
      real "cancel" state worth staging for either. Lists every character
      on the account with its level. New `account_load_by_id()`/
      `account_set_name()`/`account_set_password()` (account_repo.c),
      `CONN_EDACCOUNT_*` states (descriptor.c). No self-service
      equivalent -- same as promote/edplayer, a player asks an immortal.
- [x] **wiznews** — done 2026-07-05: an immortal-only (51+) news channel like
      `news`; `edwiznews` posts items that concern immortals. Parallel to
      news/ednews.
- [x] **ed* rename** — done 2026-07-05: redit→edroom, hedit→edhelp,
      addnews→ednews (command names, help topics, tests, editor prompts).
- [x] **Diseases** — done 2026-07-18, then expanded same day (user: "may as
      well include all disease now, from sneezy along with affects for
      players and NPCs"). ALL 26 diseases from the upstream
      `diseaseTypeT` roster (misc/disease.h) -- Cold, Flu, Frostbite,
      Bleeding, Infection, Herpes, Broken Bone, Numbed Limb, Voicebox,
      Eyeball, Lung, Stomach Wound, Internal Bleeding, Leprosy, Plague,
      Suffocation, Food Poisoning, Drowning, Garrotte, Syphilis, Bruised,
      Scurvy, Dysentery, Pneumonia, Gangrene, Extreme Pain (`affect_type_t`,
      affect.h) -- immortals immune. Caught from drinking a pool
      (`cmd_drink.c`, 15% chance on the puddle-drinking branch, one of the
      26 picked at random with its own duration). Ticks via the existing
      `affect_tick_run()` pulse: every 10th round-left tick drains HP
      (per-type, `DISEASE_HP_DRAIN[]`) with a "flares up" message, clamped
      to a minimum of 1 HP, until it wears off naturally or is cured (see
      Hospital below). Deliberately NOT a port of each disease's own
      upstream spec_proc mechanic (disease.cc is ~2000 lines of bespoke
      per-disease effects like blindness/muteness/limping) -- same v1
      scope as the original 4: a name, a duration, a periodic HP drain,
      a hospital cure price (`affect_cure_price()`). Poison (drinking,
      30% chance, independent roll from disease) is its own `AFFECT_POISON`
      rather than folded into the disease list, since the original keeps
      DISEASE_POISON separate too, and Tobin's `drink` already had its own
      poison roll predating the disease work -- converted from a one-shot
      instant hit into a proper timed affect (user 2026-07-18 bug report:
      "i peed and drank acid, said it poisoned me... did a score and no
      message stating that i was poisoned" -- poison had no persistent
      state to show; now it does, in `affects`, same as any disease).
      **Now ticks for mobs too, not just connected players**
      (`world_for_each_mob()`, same iteration primitive mob_ai.c's own
      pulse uses) -- a mob has no descriptor to send a first-person
      message to, so its own tick/expiry is echoed to the room in third
      person instead ("The rat's Cold flares up."/"...wears off."). Not
      DB-persisted (like all affects) -- lost on reconnect.
- [x] **News follow-ups** — done: edit/delete existing news in-game (addnews
      only creates); show unseen news at login (per-player last-seen).
      `edit news <existing headline>` now preloads the current body
      ("existing text below", same convention as `edit help`) and
      overwrites it in place on save (`news_repo_upsert`, was a straight
      INSERT that failed on the duplicate title); `edit news delete
      <headline>` removes an item outright. `edit wiznews` got the
      identical treatment (same underlying gap, same shared
      EDIT_NEWS/EDIT_WIZNEWS save path in descriptor.c). New
      `player.news_last_seen_id` column (tobin_migrations.sql) tracks the
      highest news.id each player has read; login shows a one-line
      "There is new news! Type 'news' to catch up." notice (no count/id
      ever shown -- house rule) when something's posted since they last
      ran `news`; `news` bumps the bookmark. `tests/smoke_test_news_
      followups.py`.
- [x] **redit Extra Descriptions** — mortal-facing half done 2026-07-19:
      `look <keyword>` now reveals a room's extra description
      (`room_repo_extra_desc()`, `room_repo.c`, wired into
      `look_at_target()`, `cmd_look.c`) -- the `roomextra` table already
      carried 8,861 real seeded rows (vnum, space-separated keyword list,
      description) with zero Tobin code reading it before this; verified
      live against real seeded content (room 3's "calendar" extra desc).
      Same case-insensitive per-word prefix matching convention as every
      other obj/mob keyword match in this codebase, checked only after a
      real PC/mob/object match misses. New `tests/smoke_test_extra_desc.py`
      (5 checks: reveal, case-insensitivity, prefix matching, a genuine
      miss still falls through to "You don't see that here.", room-scoped
      not global).
      **Builder-facing half done (work, this session):** `edit room`'s
      menu 8 opens a new Extra Descriptions submenu (`CONN_REDIT_EXTRA_*`,
      `descriptor.c`) -- list, add (keywords then description via the
      shared line editor), rename keywords, edit description, delete one,
      delete ALL (Sneezy redit items 6 & 10). UNLIKE the rest of `edit
      room`, these commit to the DB immediately rather than deferring to
      the working copy's Save -- extras were never modeled in `room_t` to
      begin with (the mortal-facing lookup above already hits the DB
      fresh every time), so there was no in-memory state to defer from;
      see `room_repo.h`'s comment on `room_repo_extra_save()` for the
      full reasoning. New repo functions `room_repo_extra_list/_get/
      _save/_rename/_delete/_delete_all()`. A rename that collides with a
      different entry's exact keyword string is refused cleanly (relies
      on `roomextra`'s own `(vnum, name)` primary key -- no extra
      pre-check needed). New `tests/smoke_test_redit_extradesc.py` (add/
      rename/edit/delete/delete-all, a colliding rename, cancel-add and
      abort-mid-add leaving no row behind, and an end-to-end check that
      `look <keyword>` immediately sees what redit just authored).
- [x] **Door mechanics** — done 2026-07-06: `open`/`close <direction>`
      (`cmd_open.c`), movement blocking on a closed door (`cmd_move.c`:
      "The door is closed."), and secret exits hidden from `look`'s
      Obvious-exits line and `exits` (still walkable if you know the
      direction). New `EXIT_COND_CLOSED`/`_LOCKED`/`_SECRET` bit constants
      in `room.h`. Door/condition state is per-exit, NOT mirrored to the
      reverse exit -- matches how `edroom`'s own auto-created reverse
      exits already work (independent door state per direction), not an
      oversight. `open` refuses a Locked door; `unlock`/`lock` shipped
      later (2026-07-19, see "Keys unlocking doors" below). `smoke_test_
      doors.py` + 3 new help topics (`open`, `close`, updated `exits`).
- [x] **Positions polish** — done (Session 43): a defender who isn't
      standing (sitting/resting/sleeping/any lower rung) takes a flat
      +15 hit-roll bonus against them in `combat_strike()` -- attacking
      only auto-stands the ATTACKER (cmd_attack.c), so this stays in
      effect for as long as the defender chooses to stay down.
- [x] **Personalized immortal log messages (57+)** — clarified/closed
      2026-07-19, was based on a misread of the original engine. Checked
      the real SneezyMUD source before touching anything: `LOG_JESUS`/
      `LOG_PEEL` (misc/log.h) are each a named DEVELOPER's own personal
      SCRATCH DEBUG CHANNEL -- whoever's actively chasing a bug drops ad-
      hoc `vlogf(LOG_JESUS, "...")`/`vlogf(LOG_PEEL, "...")` calls into
      whatever code they're debugging that session, visible ONLY to that
      one named immortal (gated by character name), so their in-progress
      noise never spams every other immortal's screen -- then the calls
      get pulled once the bug's fixed. `LOG_LOW` is unrelated (a generic
      mob-data-integrity warning severity, not personalized at all --
      this TODO's own "inspiration" list conflated it with the other two).
      Tobin already has the real infrastructure for this (`LOG_JESUS` in
      `log.h`, name-gated in `cmd_setsev.c`'s toggle list) -- there's
      nothing further to build; it's a standing CONVENTION for whoever's
      debugging to use ad-hoc, not a discrete feature with a fixed set of
      messages. No `LOG_PEEL` equivalent added -- no second named
      immortal exists in this game to gate it to.
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
- [x] **Make `smoke_test_limbs.py`/`smoke_test_limbs_cmd.py`
      deterministic** — already done in an earlier session (both fully
      migrated to `hurtlimb <target> <limb> <hp>` for every injury-tier
      assertion; `smoke_test_limbs.py`'s Part 2 still uses real combat,
      but only to check that a landed hit names a limb at all, which the
      file's own header comment explains is reliable within a few rounds
      and was never the flaky part) -- just never checked off. Confirmed
      2026-07-19 by running both live. Found and fixed one real, unrelated
      staleness while doing so: `smoke_test_limbs.py`'s Part 2 PvP setup
      predated the PK opt-in feature, same `toggle pk` fix already applied
      to `smoke_test_combat.py`.
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
- [x] **Mob AI: wandering + mob actions** — done, deployed, and live (its
      pulse registration -- `pulse_register(600, mob_ai_tick)` -- has been
      confirmed wired in `main.c` all session; entry corrected 2026-07-17,
      was stale ("not yet deployed" from a snapshot taken mid-deploy that
      was never updated once it actually landed). User: "in pulse, make
      sure that mob actions click and mobs that can wander will do so,
      look at mob ai from sneezy". New `mob.actions` field wired all the way
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
- [x] **Drink/sip commands** — already shipped (cmd_drink.c/cmd_sip.c) --
      entry pruned 2026-07-19, was stale. Landed via a different angle than
      this entry originally scoped: not container liquid-type/capacity
      modeling, but drinking/sipping directly from a ground puddle (`pee`'s
      puddles, `combat.c`'s blood pools), with real poison (30% chance,
      `sip` lower-risk than `drink`) and a 26-disease infection roll (15%
      chance) -- see `help disease`. No thirst/hunger stat exists or is
      consumed by either command; `nutrition` remains vestigial. Drinking
      from a proper container (fill/pour, `OBJ_CAT_DRINK` capacity
      modeling) remains unbuilt -- see **Liquids**/**`fill`** below, which
      are still genuinely open, not superseded by this.
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

### User batch 2026-07-12 — logged, not yet started

- [x] **`egotrip` command** — done, scoped down. User: "add egotrip
      command from sneezy." The original (cmd_egotrip.cc) is a 13-
      subcommand immortal toy-box (deity/bless/blast/damn/hate/cleanse/
      wander/stupidity/crit/portal/teleport/disease/garble) built almost
      entirely on systems Tobin hasn't built yet: a disease system, a
      garble/speech-distortion system, mob AI hate/aggro tracking (task
      27), stat-modifying affects beyond Sanctuary's flat damage-halving
      (task 13), and free-standing portal objects. Rather than stub out
      twelve dead branches, ported the one subcommand that maps cleanly
      onto what already exists: `egotrip blast <target>` (60+, matching
      `balance`'s tier), a non-lethal bolt of lightning that halves the
      target's current HP (floored at 1 -- per the original's own
      comment, "this just nails um, but shouldn't actually kill them").
      Any other/missing subcommand shows a usage line that's honest
      about the scope-down rather than silently no-opping.
      `tests/smoke_test_egotrip.py` covers the mortal-can't-reach-it
      gate, the scoped usage line, and two successive blasts roughly
      halving HP each time (loosened to a tolerance band after finding
      the periodic HP-regen tick (`REGEN_PULSES`) nudges the exact
      before/after numbers by a point or two between reads).
- [x] **`stat` command (Implementor 55+)** — done. User: "add stat
      command so an immortal of level 55+ can see everything about the
      mob obj or room with a vnum argument (Ex.: stat obj 101) from
      sneezy." Rather than hardcode each of `obj`/`mob`/`room`'s ~20-40
      column names by hand (and risk silently going stale whenever a
      column gets added), `stat` dumps every column of the row
      generically via two new small accessors added to `db.c`/`db.h`
      (`db_col_count()`/`db_col_name()`, alongside the already-existing
      `db_get_idx()`) -- "<column>: <value>" per line, driven entirely
      by whatever the query actually returned. Distinct from the
      existing `vnum` command (cmd_vnum.c), which searches BY NAME and
      shows one summary line per match -- `stat` takes an exact vnum and
      shows everything about that one row. Also appends what a bare
      column dump alone would miss: a room's exits (`roomexit` table)
      and an object's hitroll/damroll/AC-style affects (`objaffect`
      table). `tests/smoke_test_stat.py` covers the 55+ gate, all three
      categories (obj/mob/room) each showing their own header plus a
      couple of known column values, the extra exits/affects sections,
      and a nonexistent vnum reporting plainly instead of an empty dump.
      `smoke_test_vnum.py` re-run clean (same `db.c` file touched, but
      only additive new functions).
      Follow-up (user 2026-07-12, three rounds): "in stat action flags
      and wear flags should be readable, not numbers"; "same with stat
      mob actions should be readable flags, not numbers, and faction
      line shouldnt exist. we will not support factions"; "class should
      report class text, not class number, same for race, and only
      report the stats that are in use on tobin, you are reporting
      stats on 12 character stats". Added `obj_wear_flag_names()`
      (obj.c/obj.h), `mob_action_names()` (mob_ai.c/mob_ai.h),
      `mob_class_label()` and a newly-ported 127-entry `mob_race_name()`
      table (being.c/being.h, straight from Sneezy's real monster-race
      list, misc/race.cc) -- mob.class/mob.race are Sneezy's own
      encodings, entirely separate from Tobin's player `player_class_t`/
      6-entry `player_race_t`. `cmd_stat.c` now prints decoded
      wear_flag/actions/class/race lines up front and skips those raw
      columns (plus faction/fact_perc and the six mob attribute columns
      Tobin never reads -- bra/agi/foc/per/kar/spe) from the generic
      dump. Second follow-up (same day): "in stat room ... dir, cond,
      and door should be words not numbers" -- room exits now decode
      `direction` via the existing `DIR_NAMES` table, `type` via
      `door_type_name()`, and `condition_flag` via `exit_cond_names()`
      (all three already existed for redit's own exit editor,
      descriptor.c -- reused rather than re-derived). `smoke_test_stat.py`
      updated with assertions for all of the above; 32/32 checks pass.
      Third follow-up (user 2026-07-12): "stat obj, get names for type,
      action_flag" / "stat room get names for room_flags, sector". Obj's
      `type` (the original's itemTypeT, 77 entries) and `action_flag`
      (the original's extraFlags bitmask, 32 bits) decode via new
      `obj_type_name()`/`obj_action_flag_names()` (obj.c/obj.h). Room's
      `sector` and `room_flag` decode via the already-existing
      `sector_name()`/`room_flag_names()` (room.c) -- those two already
      existed for `look`/`redit` and just weren't wired into `stat` yet.
      `smoke_test_stat.py` extended again; 34/34 checks pass.
      Fourth follow-up (user 2026-07-12): "stat player <name> to stat a
      player". Players aren't a single vnum-keyed table like obj/mob/
      room -- a name-keyed row in `player`, plus one-to-one rows in
      `player_progress`/`player_attrs` -- so a new `stat_player()`
      helper (cmd_stat.c) looks the name up via the already-existing
      `player_id_for_name()` (player_repo.c) and dumps all three tables
      as separate sections via the same generic `dump_row()`, decoded
      the same way (class/race/gender as words via the already-existing
      `class_name()`/`race_name()`/`gender_name()`, plus an
      `alignment_tier` line via `alignment_word()` alongside the raw
      alignment number). Reads straight from the DB, matching how `stat
      mob`/`stat room` already show the prototype rather than any one
      spawned instance's live state. A nonexistent name reports plainly
      ("No such player '<name>'.") rather than a blank dump, matching
      the existing nonexistent-vnum behavior. Caught and fixed a real
      bug while wiring this up: `db_query()`'s format parser (db.c) only
      supports `%s`/`%i`/`%f`/`%r`, not real printf's `%li` -- the first
      draft used `%li` for the player_id (a `long`) and every player-
      table/player_progress/player_attrs lookup silently returned zero
      rows. Fixed by casting to `int` and using `%i`, matching the
      convention already used elsewhere in the codebase for other `long`
      ID columns (e.g. `player_repo.c`'s account_id lookups).
      `smoke_test_stat.py` extended with a sixth section covering the
      header, decoded class/race/gender, the Progress section showing a
      persisted level, the alignment tier line, the Attributes section,
      and the nonexistent-name case; all checks pass. `help_topic.sql`'s
      `stat` entry updated to document the new form.
- [x] **`goto guildmaster` (mortal-usable)** — done. User: "add a goto
      class function that mortals can do to help find thier
      guildmasters." `cmd_goto.c`'s table-level gate lowered from
      immortal to mortal, but only the literal `goto guildmaster` form
      is reachable by a mortal -- `goto <vnum>` and `goto <player>`
      still refuse a mortal caller outright, same message as before
      ("Command not found..."). `goto guildmaster` uses a new
      world-wide `world_for_each_mob()` scan (staged through file-scope
      statics, same pattern as `trigger.c`'s random-trigger tick, since
      the callback takes no userdata pointer) to find any mob keyworded
      "guildmaster" whose `mob_class_known`/`char_class` matches the
      caller's own class, then reuses the same bamfout/bamfin/look
      teleport tail as the vnum/player forms. Kept as its own small copy
      rather than sharing `cmd_practice.c`'s existing `find_guildmaster()`
      (that one is deliberately scoped to "in this room only"; this one
      is deliberately world-wide -- sharing would need a mode flag for
      one caller each). `tests/smoke_test_goto_guildmaster.py` covers a
      mortal Mage reaching a real seeded guildmaster, a mortal still
      being refused the vnum/player forms, and an immortal still able to
      use the vnum form. A fourth "no seeded guildmaster anywhere"
      negative case was drafted but dropped -- a live DB check showed
      every one of Sneezy's class bits already has a seeded guildmaster
      somewhere in the world, so every one of Tobin's 6 real classes has
      a real match and there's no safe way to construct a true "nobody
      trains this" scenario without mutating real seed content; the
      refusal path itself is a simple early-return, low risk, and was
      manually verified during development. `stat`/`administration`
      help topics unaffected; `goto`'s own help topic still needs a
      follow-up pass to mention the new mortal form.
      Follow-up (user 2026-07-12): "goto guildmaster should give them
      directions, not transfer. also add a goto rent, goto surplus for
      now with goto expanding for mortals." Redesigned from a teleport
      into a walking-direction list: a new `goto_bfs()` (cmd_goto.c)
      breadth-first-searches outward from the caller's current room over
      real room exits until it reaches a room satisfying a caller-
      supplied predicate, then reports the shortest direction sequence
      instead of moving anyone. `world_get_room()` (world.c) is a linear
      scan over a linked list of ~20,500 real rooms -- far too slow to
      call once per BFS edge -- so `goto_bfs()` builds its own flat
      open-addressed vnum->room_t* hash table once per call (via the
      existing `world_for_each_room()` bare-callback iterator) and
      searches against that instead, O(1)-average per edge. `goto
      guildmaster`'s predicate now checks each BFS-visited room
      individually for a matching guildmaster (closest one wins, unlike
      the old world-wide "any match" scan). Two new fixed-room landmarks
      share the same direction-giving path: `goto rent` (room 557, The
      Roaring Lion Inn) and `goto surplus` (room 563, Surplus -- both
      vnums user-specified 2026-07-12). Standing exactly in the target
      room reports "You're already there" instead of a meaningless
      zero-hop direction list. All three landmark forms are reachable by
      immortals too, not just mortals -- checked before the immortal-
      only gate, same as before; only the raw vnum/player-name forms
      stay immortal-only. `smoke_test_goto_guildmaster.py` rewritten:
      confirms `goto guildmaster` no longer teleports (room unchanged
      before/after) and reports a direction list instead, `goto rent`/
      `goto surplus` do the same, and the "already there" message fires
      once an immortal is actually parked in room 557/563 via the
      still-working vnum teleport. `goto`'s help topic and command-table
      description updated to describe all three landmark forms.
      Follow-up conflicts found and fixed (user 2026-07-12: "leave it
      alone and resolve conflicts as they occur", re: help-system work):
      two smoke tests used `goto` as their stock example of an
      "immortal-only command" for leak-testing purposes
      (`smoke_test_help.py`'s wizhelp/help listing checks,
      `smoke_test_help_topics.py`'s "help <topic> leaks nothing to a
      mortal" check) -- both stale now that `goto`'s landmark forms are
      genuinely mortal-visible. Swapped both to `transfer` (still
      IMMORTAL_LEVEL_MIN, unaffected) as the example instead; both files
      re-run clean.
- [x] **Character creation: descriptive race/class text instead of raw
      numbers** — done. User: "char creation, dont tell the player
      number bonuses, tell them this class X or this race X. be
      descriptive so they can imagine the rest." `descriptor.c`'s
      `show_race_screen()`/`show_class_screen()` rewritten from raw
      stat-delta lines (e.g. "+2 Dex, +2 Int, -4 Con") to a short
      evocative sentence per race/class, in the same direction as its
      real stat shift (an Elf really is quick and clever, an Ogre really
      is strong and thick, etc) without showing the exact numbers. The
      real mechanics (`race_stat_bonus()`/`class_stat_bonus()`,
      being.c) are completely unchanged -- display-only. Buffers bumped
      900->1600 bytes for the longer prose.
- [x] **Character creation: race/class before attribute point-buy** —
      done. User: "also, selection of race and class should go before
      picking attributes." `descriptor.h`'s `CONN_CHAR_CREATE_*` state
      order and `descriptor.c`'s case-block order both changed from
      NAME->ATTRS->RACE->CLASS->ALIGNMENT to
      NAME->RACE->CLASS->ATTRS->ALIGNMENT. Race/class choices are now
      just recorded at selection time; `race_stat_bonus()`/
      `class_stat_bonus()` are applied later, in the ATTRS "done"
      handler, once point-buy is finished -- keeps `attrs_allocated()`
      measuring pure point-buy spend rather than race/class deltas
      leaking into the net-pool calculation. This touched every smoke
      test that creates a character (~90 files): wrote a one-shot
      migration script to swap the `done`/race/class send-line order in
      each file's `make_char()`-style helper, then hand-fixed ~13
      outlier files the script's pattern didn't cover (tuple-form
      `for step in (...)` creation, single-statement `cmd()` calls
      instead of `send_line`+`recv_all` pairs, and one file
      (`smoke_test_gender.py`) where the script's naive "peel back
      preceding attr-command lines" logic broke an `if gender:`/`if
      appearance:` guard's indentation -- fixed by hand). All 124 test
      files verified to `py_compile` cleanly afterward; a broad sample
      re-run clean.
- [x] **Fixed: `set` command dispatch collided with `settrap`** — found
      while re-testing the char-creation reorder above (unrelated to
      it): `smoke_test_alignment.py`'s `set <name> alignment 500` was
      landing on `settrap`'s "Usage: settrap <direction>" instead.
      Root cause: `cmd_table.c` resolves commands by first-prefix-match
      in table order, and `settrap` (which "set" is a literal prefix
      of) appeared earlier in the table than the actual `set` entry, so
      the exact 3-letter command "set" was shadowed by the longer,
      earlier "settrap" instead of matching its own exact entry. This is
      the same class of bug the file already had one guard comment
      for (`set` vs `setsev`) but not this second collision. Fixed by
      moving `set`+`setsev` immediately before `settrap` in the table,
      same "shorter exact match must come first" convention already
      established. Wrote a one-off script checking every command name
      against every earlier command name for this exact shadowing
      pattern across the whole table -- no other collisions found.
      `smoke_test_alignment.py`/`smoke_test_set.py`/`smoke_test_trap.py`
      all re-run clean.
- [x] **Fixed: 20 more test files missed by the char-creation-reorder
      migration** — found while re-testing after the goto redesign
      below: `smoke_test_help.py` hung mid-character-creation because
      its `make_player()` helper (not `make_char()` -- a second, unrelated
      naming convention the earlier migration script never searched for)
      still sent `done` before race/class. Audited ALL 124 test files by
      pattern rather than by function name this time (a "done" send
      immediately followed by a `# race` comment, in any statement-
      pairing style) and found 20 total still broken: 14 using a bare
      `send_line(s, "done")` + `recv_all(s)` on separate lines with a
      `make_player()` helper (`smoke_test_color.py`, `_combat.py`,
      `_copyover.py`, `_help.py`, `_immortal_cmds.py`, `_kill.py`,
      `_level_titles.py`, `_logs.py`, `_notify.py`, `_regen.py`, `_say.py`,
      `_sector_color.py`, `_target_abbrev.py`, `_telnet_iac.py`), 4 more
      tuple-form `for step in (..., "done", "1", "1", "2")` creations
      (`smoke_test_held.py`, `_mudstats.py`, `_multiplay.py` x2), and 2
      more separate-line stragglers missed by the original 8-file
      hand-fix pass (`smoke_test_help_topics.py`,
      `_mortal_toggle.py`). All 20 fixed by hand, same swap as
      everywhere else (race/class before done). Re-audited with the same
      pattern-based script afterward: 0 remaining. All 124 files verified
      to `py_compile` cleanly; a 19-file regression batch covering every
      touched file re-run clean.
- [x] **`open door <direction>` / bare `open door`** — done. User: "open
      dootr doesnt work, did we add that to todo file?" Root cause: the
      original Sneezy `open`/`close` (`lib/help/open`) documents `open
      door <direction>` as the primary phrasing ("open door north",
      "open door east", ...) -- Tobin's port (`cmd_open.c`) had only
      ever implemented the bare `open <direction>` half of that, so the
      natural "open door"/"open door north" phrasing silently fell
      through to "you don't see that here." Fixed by trying a leading
      "door" token as a fallback ONLY once the first token fails to
      parse as a direction outright -- "door" and "down" share a prefix
      ("do" matches both), so checking direction-parsing first means
      `open d`/`open do` still mean down exactly as before, and the new
      "door" handling can never shadow it. A bare `open door` with no
      direction opens the room's one door if it has exactly one
      (matching the original's own "try to determine what you mean"
      disambiguation for an ambiguous target); with zero or multiple
      doors it asks for a direction instead of guessing.
      `smoke_test_doors.py` extended with `open door north`/`close door
      north` and bare `open door`/`close door` cases. `open`/`close`
      help topics updated to document the new phrasing.
- [x] **Immortal commands moved lower in the command table** — done.
      User: "place immortal commands lower in the list of commands, that
      way the immortals are less likely to make mistakes when working on
      the game." `cmd_table.c`'s `COMMANDS[]` (113 entries) reorganized
      into two blocks: every `MORTAL_LEVEL_MIN` command first, every
      immortal-tier command (anything above) second -- relative order
      *within* each block is unchanged from before. Since dispatch
      resolves an abbreviation to the FIRST matching entry in table
      order, this means any short abbreviation an immortal types that's
      ambiguous between an everyday mortal action and a rarer, more
      consequential immortal one now always resolves to the mortal
      action -- exactly the class of mistake the `set`/`settrap` and
      `get`/`goto` bugs fixed earlier this session both were, now
      prevented structurally instead of one collision at a time.
      One deliberate exception, called out in a new table-wide comment
      at the top of the file: `settrap`/`disarmtrap` (mortal Thief
      skills) stay grouped with `set`/`setsev` in the immortal block
      rather than moving to the mortal block, since `settrap` is a
      literal prefix of "set" -- moving it ahead of `set`/`setsev` would
      have silently reintroduced that exact bug. `hurtlimb`/`aitick`
      (two immortal debug tools that used to sit in the middle of the
      mortal combat block, between kill and hit, for no documented
      reason) moved down too. Wrote a one-off collision-checker script
      (checks every command name against every EARLIER command name for
      "exact match shadowed by a longer earlier prefix", the same class
      of bug as set/settrap) and ran it against the new order -- 0
      collisions found. Corrected two pre-existing stale comments found
      while touching this file (unrelated to the reorder itself): the
      `inventory`/`immort` "bare i reaches immort" claim was already
      backwards before any of today's changes (inventory was already
      registered earlier in the table and always won "i"; immort's real
      shortest abbreviation was already "im", not "i"). Full regression
      batch covering abbreviation-sensitive tests (`smoke_test_
      immortal_cmds.py`, `_alignment.py`, `_set.py`, `_trap.py`,
      `_menu_letters.py`, `_help.py`, plus a broad general sweep) re-run
      clean.
- [x] **Toggle listing split into categories** — done. User: "split the
      toggle listing into categories: Preferences, Prompt,
      Communication." `cmd_toggle.c`'s `toggle_t` gained a `category`
      field (personal toggles only; `gametog`'s global switches keep
      their single flat list, unaffected). Assignment: color/nospam/
      autoloot -> Preferences, hp -> Prompt, newbie/noshout ->
      Communication. `toggle_dispatch()`'s listing path now iterates
      `CATEGORIES[]` in that order, printing a `<y>Category:<z>` header
      only for categories with at least one visible toggle (an empty
      category is skipped rather than printing a bare header). Existing
      `smoke_test_toggle.py`/`smoke_test_gametog.py`/
      `smoke_test_autoloot.py`/`smoke_test_nospam.py` re-run clean --
      row format (`name`, on/off, description) is unchanged, only the
      grouping/headers are new.
- [x] **Fixed: five more "Huh?!" gates missed by the friendlier-message
      sweep** — found while re-testing `stat`'s follow-ups (a Warrior
      attempting `cast`/`pray` unexpectedly got the OLD "Huh?!" instead
      of "Command not found..."). The original fix (see "Replace 'Huh?!'
      with a friendlier unknown-command message" above) only covered
      five spots (the dispatcher, `cmd_immort`, and `cmd_edit.c`'s four
      level-gates); it turns out `cmd_cast.c`'s class gate, `cmd_pray.c`'s
      class gate, `cmd_purge.c`'s `purge linkdead` level gate, and both
      of `cmd_trap.c`'s skill gates (`settrap`/`disarmtrap`) each had
      their own hardcoded "Huh?!" that was never touched. All five now
      send the same "Command not found, maybe submit an idea..." text.
      `smoke_test_castpray.py` (was failing on this) and
      `smoke_test_trap.py` (needed one stale assertion updated to match
      the separate, already-shipped "damage numbers hidden from
      mortals" behavior -- unrelated to this fix, just surfaced by the
      same re-test) both pass clean now; `smoke_test_purge.py` re-run
      clean too.
- [ ] **Boxed ASCII-art menu rework, remaining editors** — the account
      menu itself is DONE (see the merged entry above); this entry now
      covers just the "audit every other menu-driven screen" half:
      `edit room`/`edit zone`/`edit player`/`edit zone`/`balance`'s
      menu screens (per [[editors-menu-driven]] memory) haven't been
      touched, and `send_boxed_menu()`/`visible_len()` (descriptor.c) are
      ready to reuse for them. Original spec, for reference. Old:
      ```
      -- Your characters --
        1. Jesus (Implementor)
        2. Testdummy (Level 1)
        3. Willy (Level 1)

        C [number|name] -- connect a character
        N               -- create a new character
        D <name>        -- delete a character
        X               -- delete this ENTIRE ACCOUNT
        Q               -- quit the game
      (Letters work in either case; a bare number still connects too.)
      ```
      New:
      ```
      ╔════════════════════╗
      ║ <C>C<z>  Connect Player ║
      ║ <C>N<z>  New Player     ║
      ║ <C>D<z>  Delete Player  ║
      ║ <C>X<z>  Delete Account ║
      ║ <C>Q<z>  Quit Game      ║
      ╚════════════════════╝
      ```
      `C` then opens a numbered submenu of that account's characters:
      ```
      -- Your players --
        1. Jesus 			[Implementor]
        2. Testdummy 		[Level 1]
        3. Willy 			[Level 1]
      Choose a number to connect that player to the game:
      ```
      "make all character facing menus in this fashion" -- audit every
      other menu-driven screen (editors, `edit` menus, etc, per
      [[editors-menu-driven]] memory) for the same boxed treatment.
- [x] **Login banner: keep-gate art above the TobinMUD logo** — done
      2026-07-17. Same "3 text files ... use those to create new menu
      output" request as the account-menu box above; user confirmed via
      AskUserQuestion that `castle1.txt`/`keep1.txt` map to the login/
      welcome banner (vs. `box1.txt` for letter-menus). First version
      opened with BOTH pieces (a distant castle-skyline silhouette, then
      the keep's gate) above the existing "TobinMUD" text logo -- user
      cut the skyline right after seeing it live: "remove the castle art
      from the connection screen, its too long displaying 2 seperate
      ascii art pieces." Final: `keep1.txt`'s gate, then the logo,
      unchanged otherwise (`descriptor_create()`, descriptor.c).
- [x] **Autoloot toggle** — done. User: "add an autoloot toggle where a
      player upon opponent death automatically loots all from the
      corpse." New `PLR_AUTOLOOT` player-flag bit (being.h) plumbed into
      the existing `toggle` menu (`toggle autoloot`, cmd_toggle.c, same
      pattern as nospam/noshout) and checked in `combat.c`'s
      `combat_defeat()` right after the corpse is populated -- fires for
      a normal defeat, a decapitation, AND an immortal's `kill` instakill
      alike, since all three already funnel through that one function.
      Moves every item straight from the corpse into the winner's
      inventory and confirms with "You automatically loot <loser>'s
      corpse." `tests/smoke_test_autoloot.py` covers the toggle itself
      appearing in the menu, an autoloot-on kill landing the gear in the
      winner's inventory, and autoloot-off leaving it sitting in the
      corpse instead. Two test-authoring snags: character names with
      digits ("Autolootv1"/"v2") are silently rejected (letters-only
      rule, hit repeatedly earlier this session too) -- renamed letters-
      only; and a time.time()-second-resolution suffix collided with a
      leftover account from a debug rerun seconds earlier -- switched to
      millisecond resolution.
- [~] **Split victim's gold among the group on kill** — SOLO case done
      2026-07-19; the "if grouped" split remains genuinely blocked. "To
      the victor go the spoils!" User: "also upon death get all gold from
      the victim and split it between all group members if groupped."
      `combat_defeat()` (combat.c): a PC loser's entire `progress.gold`
      transfers to a non-immortal PC winner, right alongside the existing
      HP-reset-to-half/limb-heal handling on defeat -- reuses the same
      `player_progress_save()` calls already made there (loser's via the
      existing call right after; winner's via the XP block's own save
      just below, since gold and XP land on the same struct before that
      save fires -- no second DB write added). Same non-immortal-winner
      gate as the pre-existing mob-gold-drop path, so an immortal's
      instakill never triggers this. PK combat already requires both
      sides to have opted in (`toggle pk`), so there's no non-consensual
      gold-loss path here -- this only ever fires with mutual consent.
      Group split remains blocked on the not-yet-built group/party system
      (see "Bigger systems" below) -- when that lands, this same block is
      the place to divide `stolen` across the killer's group instead of
      crediting it all to one winner. New `tests/smoke_test_pk_gold.py`.
- [x] **Meaningful limb damage** — done 2026-07-19, after auditing what
      already existed against the user's original ask: "make limb damage
      mean something. if you have a limb decapitated it shouldnt be at 100%
      limb health. make individual limb hits actually hurt." Turned out
      most of this had already landed in earlier sessions and just never
      got checked off: per-limb hit weighting (a bigger target like the
      torso gets hit far more than a finger), escalating injury-tier
      messages in both combat and `score` ("hurt rather badly" /
      "needs medical attention" / "destroyed"), a destroyed limb (0% HP)
      already penalized its OWNER's own hit chance (`DESTROYED_LIMB_HIT_
      PENALTY`, combat.c), and Hospital (limb repair, 2026-07-18) already
      gives a real mid-game cure. What was genuinely missing, and is the
      actual change this session: `combat_strike()` only checked the
      ATTACKER's own destroyed limb before -- a destroyed limb didn't make
      its owner any easier to HIT, only worse at hitting back. Added the
      mirror check on the defender side (same flat, non-stacking amount,
      for symmetry) -- a badly maimed combatant is now genuinely more
      vulnerable, not just less dangerous. On the other half of the
      complaint: a decapitated limb DOES correctly read 0% while the fight
      is still ongoing (confirmed via `hurtlimb`/`limbs`/`score`, already
      solid) -- it only reads back at 100% AFTER the fight ends, because
      combat defeat has always fully healed every limb as part of its
      "revived at half HP" recovery (`being_limbs_full_heal()`), the same
      as HP itself. That's deliberate soft-respawn behavior (PC death
      isn't permadeath in this engine), not the bug it looked like --
      documented explicitly in `being_has_destroyed_limb()`'s doc comment
      (being.h) and combat.c's own comment so it doesn't get mistaken for
      one again. No dedicated smoke test added for the new defender-side
      penalty specifically -- like every other single hit-roll modifier in
      this formula (AC, position, weapon bonus), it's inherently
      probabilistic (the formula's own guaranteed-hit/miss-zone design
      means no single roll can ever be made fully deterministic), and a
      statistically-meaningful sample would cost minutes of real combat-
      round pacing for one modifier's marginal coverage; verified instead
      via a live spot-check plus the existing combat/limb regression
      suite passing clean.
- [ ] **`smoke_test_limbs_cmd.py` intermittent flake, real and unrelated to
      this session's limb work** — found 2026-07-19 while regression-
      testing the defender-vulnerability change above (that change doesn't
      touch this test's code path at all -- `hurtlimb`/`limbs`, no
      `combat_strike()` involved). Fails maybe 1 run in 3-4, always at the
      same assertion ("the injured limb... shows its exact percentage
      (13%)"), even running in total isolation (no concurrent test load).
      `being_limbs_full_heal()`'s per-limb `share` calc has no randomness
      (confirmed by reading it) and level-1 baseline-stat characters
      should get an identical, deterministic 13% every time -- root cause
      not yet found. Re-running always passes clean on retry, so this
      wasn't chased further this session; flagging for whoever picks it up
      next rather than silently living with an occasional false failure.
- [x] **Global "Grimhaven" → "Tobin City" text replace** — done. User:
      "search the entire database and replace any instances of
      'Grimhaven' with 'Tobin City'." Scanned every varchar/text column
      across the whole `sneezy` schema (a small script iterating
      `information_schema.columns`) rather than guessing which tables
      might contain it -- found matches in 18 columns across `corporation`,
      `mobresponses`, `objextra`, `obj`, `roomextra`, `room`,
      `ship_destinations`, `zone`, `zone_reset`, and `mob`. Plain
      `REPLACE()` missed lowercase/mixed-case variants ("grimhaven",
      "Grimhaven Bank", etc) since it's byte-exact regardless of column
      collation; switched to `REGEXP_REPLACE(col, 'grimhaven', 'Tobin
      City')`, which the column's case-insensitive collation makes match
      any casing in one pass. Also fixed the one local seed file with the
      same text (`db/sneezy/zone_reset.sql`, builder-facing comments
      only) so a future re-seed doesn't reintroduce it.
- [x] **Zone list: builder-assignment column** — already done. User:
      "add a column to the zone list displaying what builder is assigned
      to that zone." Turns out `zone list` (`cmd_zone.c`) already had a
      "Builders" column since Session 43 (`zone_repo_load_owner_names()`),
      built for the near-identical earlier request "dont forget a zone
      list so we can see whats been assigned and to whom" -- no code
      change needed, just confirmed it's live and matches the ask.
- [x] **Expand `prompt` toggles** — partially done 2026-07-18: `prompt
      gold` joins `prompt hp` (`PROMPT_FLAG_GOLD`, being.h/cmd_prompt.c/
      game_loop.c), unblocked now that the Money system shipped
      (`progress_t.gold`). Both flags render together when both are on
      ("HP: 25 Gold: 40 > "). mana/piety/vitality remain genuinely
      blocked -- `being_t`/`progress_t` still has no mana pool, piety
      stat, or vitality stat (prayer/casting is component-consumption-
      based, not mana-based) -- add their own toggles once/if those
      stats ever get built.
- [x] **Port Sneezy commands: consider, examine, sip, show, tell,
      whisper** — done. User: "port the sneezy commands consider and
      examine and sip and show and tell and whisper." Each scoped down
      from the original where Tobin lacks the supporting system (see
      each new `cmd_*.c`'s own header comment for exactly what was kept
      vs dropped): `examine` is a thin wrapper around `cmd_look.c`'s own
      `look_at_target()` (now exposed non-static) -- Sneezy's help text
      says plainly "Examine is synonymous with 'look at'". `consider`
      drops the original's trophy-tracked kill counts, per-lore-skill
      creature identification, and HP/AC/attack-count estimates (none of
      that infra exists) but keeps `consider self`'s AC-based equipment
      verdict (`being_total_ac()`), the immortal/mortal-PC flavor
      refusals, and the plain level-difference verdict ladder for mobs.
      `sip` reuses `drink`'s exact puddle/fountain targets (cmd_drink.c)
      with a much lower poison chance and "taste" flavored messaging,
      per Sneezy's own help text ("less risk of damage... does not fill
      you up as much"). `show <item> <person>` is the ordinary social
      meaning of the word (a message only, item stays put) rather than
      Sneezy's actual `show.cc` -- which turned out to be a sprawling
      immortal admin utility (room/zone listings) already covered by
      this backlog's own `stat`/`zone list` items, not what a player
      means by "show". `tell` (global reach, same lookup-by-name-prefix
      pattern as `transfer`) and `whisper` (same-room only, bystanders
      see a content-free "X whispers something to Y" notice) match
      Sneezy's help text exactly, minus the blind/darkness visibility
      check (not built yet). `tests/smoke_test_sneezy_ports.py` covers
      all six. Three test-authoring snags: forgetting to actually `load
      obj` the seeded fountain before `sip`-ing it; mixing up which
      side of `consider <PC>` gets the big-ego line vs the generic
      refusal; and discovering that `goto` never persists to
      `player.load_room`, so a `quit!`+reconnect (used elsewhere to force
      a fresh DB load) drops an immortal back at their ORIGINAL load
      room, not wherever `goto` last put them -- sidestepped by giving
      the test mob the same level as the already-immortal tester instead
      of changing the tester's level.
- [x] **Strip damage numbers from combat messages** — done. User: "You
      stab a messenger from the goblins's left finger for 4 damage!,
      dont report damage. messages should read You stab a messenger from
      the goblins's left finger." Six message sites touched
      (`combat.c`'s melee strike, `cmd_cast.c`'s offensive spell,
      `cmd_pray.c`'s offensive prayer, `cmd_move.c`'s door trap). Rather
      than remove the number outright, gated it on `being_is_immortal()`
      per VIEWER independently (same pattern as the existing nospam
      toggle) -- a plain mortal never sees the raw number, but an
      immortal still does, useful for balancing/testing. Found afterward
      that SIX existing smoke tests (`smoke_test_weapon_depth.py`,
      `smoke_test_bleeding.py`, `smoke_test_limbs.py`,
      `smoke_test_mob_display_name.py`, `smoke_test_weapon_messaging.py`,
      `smoke_test_affects.py`) parse damage numbers out of combat text to
      verify their own mechanics -- all six turned out to already use an
      IMMORTAL attacker/viewer, so the immortal-keeps-the-number design
      meant most needed no changes at all. Three did: `smoke_test_limbs.py`
      and `smoke_test_mob_display_name.py` had a genuinely MORTAL
      attacker/viewer for the specific check that broke, so their regex
      was loosened to match the new number-free mortal wording instead;
      `smoke_test_weapon_messaging.py`'s attacker was mortal by name but
      needed to actually observe damage (verifying a damroll bonus), so
      it was promoted to immortal instead (matching this file's own `s`
      variable's pattern) -- which then exposed two more pre-existing,
      unrelated bugs in the same file: `attack`/`kill` instakill for
      immortals (needed `hit` instead, the "always real combat" command),
      and a regex bug where `\w+'s` could never match a multi-word mob
      short_descr like "a weapon test dummy's" (fixed to `.+?'s`).
- [x] **Limb-specific decapitation difficulty + major-limb instadeath** —
      done. User: "some limbs are harder to decapitate, and should be
      instadeath if it is a major body part. decapitating a neck should
      also remove the head. head neck waist body are all major limbs.
      this should be based on the likelihood that a limb could be
      damaged vs decapitated. see sneezy code for inspiration." Two
      real Sneezy ports, not Tobin inventions:
      (1) `pick_weighted_limb()` (`combat.c`) replaces the old flat
      `rand() % LIMB_COUNT` with weights lifted directly from Sneezy's
      own humanoid `slot_chance[]` table (`misc/body.cc`) -- the torso
      (26) and back/waist-equivalent (10→mapped to waist=5 here) get hit
      far more than a finger (1), so a bigger limb is also, correctly,
      "harder to decapitate" in practice: it absorbs more of the random
      hit distribution before its own (still separately-sized) HP share
      runs out.
      (2) `is_major_limb()` generalizes what used to be a `LIMB_HEAD`-
      only instadeath check to all four limbs the user named (head,
      neck, waist, body) -- `combat_strike()` and the `hurtlimb` debug
      path (`combat_debug_set_limb_hp()`) both route through it now.
      Destroying the neck additionally zeroes and severs the head in
      the same swing (`combat_sever_limb()`'s new one-level recursion,
      "the head has nothing left to hang onto"), matching "decapitating
      a neck should also remove the head" literally.
      `tests/smoke_test_limb_severity.py` covers: a non-major limb (an
      arm) staying survivable; all four major limbs individually
      reporting instant death via `hurtlimb`; the neck test also
      reporting the head coming off. Existing `smoke_test_limbs.py` and
      `smoke_test_weapon_depth.py` re-run clean (both already used an
      immortal attacker, so the weighted pick didn't change what they
      measure).
- [x] **Immortals take zero damage in combat** — done, adapted. User:
      "an immortal character shouldnt be damaged by hits in a fight, see
      engage code from sneezy." Real Sneezy's own rule
      (`setCharFighting()`/`setVictFighting()`, misc/combat.cc) actually
      refuses a PC from ever INITIATING an attack on an immortal PC in
      the first place -- not ported as literally as that, since it would
      break the existing `hit` command's whole purpose (an immortal
      sparring in real combat for testing, which task 11/13's own smoke
      tests already rely on). Instead, `combat_strike()` now zeroes
      damage against an immortal DEFENDER as the very last step (after
      Sanctuary and every other modifier) -- landing a hit on an
      immortal still works exactly as before, verb/messaging and all, it
      just always deals 0. As flagged when this was logged:
      `smoke_test_affects.py` needed rework, since its Cleric target
      (whose incoming damage the test measures) was immortal --
      switched to an ordinary MORTAL Cleric with `basic_disc_pct`/
      `advanced_disc_pct` set directly via SQL to satisfy sanctuary's
      Advanced-tier gate without immortal status. That in turn ran into
      the damage-numbers-hidden-from-mortals change (also this session):
      a mortal Cleric never sees a damage number on their OWN incoming-
      hit messages either, so `average_incoming()`/`damages_from()` were
      switched to read the immortal ATTACKER's own outgoing "You hit
      X's arm for N damage!" confirmation line instead (same pattern
      `smoke_test_weapon_depth.py` already used, for the same reason).
- [x] **`look <person>` shows worn equipment** — done. User: "when you
      look at someone you should also see what equipment thier wearing."
      Rather than duplicate `cmd_equipment()`'s (cmd_object.c) rendering
      logic, pulled the shared part out into a new
      `being_render_equipment()` (being.c/being.h) -- same label-column
      formatting, genitalia skip, and primary/secondary-hand ordering,
      now used by both the self-view `equipment` command and
      `look_at_target()`'s PC/mob branch (cmd_look.c). Header line reads
      "You are using:" for looking at yourself, "<Name> is using:" for
      anyone else (capitalized if it's a mob's lowercase short_descr,
      same convention as the room-listing lines). `tests/
      smoke_test_look_equipment.py` covers a fresh, unequipped target
      showing all "nothing" slots, an item appearing in the right slot
      after being worn, and confirms the self-view `equipment` command's
      own output is unchanged by the shared refactor. Found and fixed a
      wider, unrelated regression while testing this: the 2026-07-12
      "Huh?!" → "Command not found..." message change (an earlier item
      in this same session) had left 36 existing smoke test files still
      checking for the literal old "Huh?!" string -- fixed with a
      mechanical find-replace across all of them (both assertion strings
      and doc-comment mentions), verified against `smoke_test_objects.py`
      (full pass) plus `smoke_test_quit.py`/`smoke_test_bug.py` spot
      checks. **Deferred, not part of this item**: the Thief "peek at
      inventory" skill (a separate, real new mechanic -- attempted,
      chance-based, presumably detectable) is still open, tracked below.
- [x] **Thief "peek" skill (attempt to see someone's carried inventory)**
      — done. User (same message as the look-equipment request above): "a
      thief skill could be added to attempt a peak at the targets
      inventory." Distinct from the worn-equipment display -- new skill/
      command that tries to see what someone is CARRYING (loose
      inventory, not worn), gated by `being_knows_skill()` and rolled
      the same way trap mechanics are (cmd_trap.c): immortals always
      succeed, a fumble is detectable (the target gets an on-guard
      notice), a clean success stays silent. New "peek" entry in
      skill.c's SKILLS[] roster (Thief, Combat tier, level 1, right
      after "steal" -- thematically paired). New `cmd_peek.c` +
      `peek <target>` in cmd_table.c, placed right after `pee` (not
      alphabetically near pray/practice) so an immortal typing bare
      "pee" keeps meaning the pee command, not peek -- see the comment
      there. `help peek` topic added directly to help_topic.sql (not
      skill_help.sql, since the skill and command share the literal
      name "peek" and skill_help's generated rows would collide on
      that key). `tests/smoke_test_peek.py` -- caught a real gate
      detail while writing it: `being_knows_skill()` requires
      `combat_disc_pct > 0` for a Combat-tier skill, not just the
      level threshold, so a fresh level-1 character needs to actually
      practice once before "peek" (or any Combat skill) is usable
      (same discipline gate smoke_test_affects.py already had to seed
      for an Advanced-tier skill).
- [x] **Make `rent` work (Sneezy port)** — done. User (2026-07-12): "make
      rent work from sneezy." Per Sneezy's own help text: `rent` stores
      your items and cleanly ends your session (regenerating HP while
      "rented out"), the RECOMMENDED way to leave the game -- "simply
      dropping link is risky." A new `rent` command (`cmd_rent.c`,
      mortal-usable) refuses while fighting, stamps a new
      `progress.rented_at` column (unix timestamp, migration in
      `tobin_migrations.sql`) with the current time, announces to the
      room, and calls the same `descriptor_leave_to_menu()` quit! already
      uses -- which now auto-saves via `player_save()` (see above), so
      `rent` doesn't need its own separate save call. Since Tobin's
      inventory already persists across ANY clean session end (not just
      `rent`), the real thing `rent` adds is the offline HP regen:
      `player_load()` (`player_repo.c`) checks `rented_at` on every
      login, heals a flat 1 HP per 5 real seconds elapsed (capped at
      max_hp -- deliberately simpler than the online `regen_tick_run()`
      curve, since a rented-out character has no position/CON context to
      read), then clears the marker so it only fires once. Deliberately
      NOT ported: per-item storage cost (blocked on the not-yet-built
      Money system, task 29) and the inn/home-room restriction (blocked
      on an undecided room-flag scheme -- available anywhere for now);
      NPC follower/pet storage-across-rent is blocked on the not-yet-
      built Pet/charm system (task 35). No mana system exists yet, so
      "regenerating mana" from Sneezy's help text doesn't apply.
      `tests/smoke_test_rent.py` covers the fighting refusal, the
      confirmation/room-announcement/clean-session-end, and the offline
      healing (an hour back-dated `rented_at` heals a beaten-down mortal
      all the way to max_hp on reconnect, and clears the marker
      afterward). `smoke_test_quit.py`/`smoke_test_save.py` re-run clean.
- [x] **Replace "Huh?!" with a friendlier unknown-command message** —
      done. User: "for failed commands that dont exist dont reply Huh?!
      reply with 'Command not found, maybe submit an idea if you believe
      TobinMUD should have it.'" Turned out to be sent from FIVE separate
      spots, not just the dispatcher's main no-match fallback
      (`cmd_table.c`): `cmd_mortal.c`'s `cmd_immort` (a real mortal typing
      the hidden `immort` toggle back gets the same message as an unknown
      command, by design) and four level-gate checks inside `cmd_edit.c`
      (`edit player/help/news/wiznews` refusing a too-low immortal, each
      deliberately matching the dispatcher's own wording per that file's
      own comment "same 'Huh?!' wording the command table itself would
      have given"). All five updated together so none silently fell out
      of sync with the other four.
- [x] **`goto` redesign for mortals** — done. User: "goto guildmaster
      should give them directions, not transfer. also add a goto rent,
      goto surplus for now with goto expanding for mortals." Mortal-
      visible now; gives walking directions via a new BFS pathfinder
      instead of teleporting (`goto guildmaster`/`goto rent`/`goto
      surplus`). Immortals keep instant teleport for every other form.
- [x] **`stat player <name>`** — done. User: "stat player <name> to stat
      a player." Stats an offline/online player; fixed a `%li`-format
      bug in `db_query()` along the way (it only supports `%s/%i/%f/%r`).
- [x] **`open door <direction>` syntax** — done. User: "open dootr
      doesnt work, did we add that to todo file?" Was missing entirely
      (bare-direction form only).
- [x] **`cmd_table.c` mortal-first/immortal-second reorder** — done.
      User: "place immortal commands lower in the list of commands, that
      way the immortals are less likely to make mistakes." Full table
      reorder; found/fixed a `set`/`settrap` and a `get`/`goto`
      abbreviation collision along the way. Further alphabetizing each
      tier block was requested, then explicitly halted mid-edit by the
      user ("STOP... wait for the user to tell you how to proceed").
- [x] **Alphabetize each `cmd_table.c` tier block** — done 2026-07-17
      (GO-AHEAD was given 2026-07-13). User: "sort by alphabet first then
      level lowest to highest" ... "leave important commands at the top."
      Shipped as a **pure refactor: zero abbreviation changes**, which is
      better than the "pin the documented exceptions and let the rest
      change" plan this entry originally sketched — see below.
      **The insight:** the table's order IS its semantics (`cmd_dispatch()`
      takes the FIRST entry the caller can see whose name starts with the
      typed verb), so "alphabetize it" really means "find the
      alphabetically-smallest order that still resolves every abbreviation
      the way it does today" — a topological sort. Derived the precedence
      edges mechanically (for every prefix at every level, today's winner
      must keep preceding the other matches), then Kahn's algorithm with an
      alphabetical min-heap tiebreak = the lexicographically smallest legal
      order. Result: only **16 of 74** mortal entries and **1 of 39**
      immortal ones must sit out of strict A-to-Z, each a local swap of an
      adjacent pair, each marked inline with the abbreviation it protects
      (`say` before `save` for "sa"; `attack` before `affects` for "a";
      `close` before `cast`/`catchup` for "c"; `hit` before `help` for
      "h"; `pray` before `practice` for "p"; `wiznews` before
      `wizhelp`/`wiznet` for "wiz"). Everything else is plain alphabetical.
      **Verification** (this class of collision bit the reorder twice
      before, so it was NOT done by eye): a script resolves all 432
      prefixes at all 8 distinct levels against both the old and new tables
      and diffs them — reported **zero differences**. Clean build, zero
      warnings. Smoke: `immortal_cmds`, `doors`, `exits_display`,
      `room_stacking`, `alignment` (the test that originally caught
      `set`→`settrap`), `goto_guildmaster`, `limbs_cmd`, `mortal_toggle`,
      `news`, `save`, `socials`, `trap`, `wiznews`, `practice`, `combat`.
      The tooling is kept at `tests/tools/cmd_abbrev_check.py` — **run it
      against any future reorder of this table.**
      **Also fixed (pre-existing, found by A/B):** `smoke_test_immortal_cmds.py`
      asserted `users` prints the raw IP `127.0.0.1`, but
      `descriptor_display_host()` shows the reverse-DNS hostname once the
      off-thread lookup lands (loopback → "localhost"; live game log
      confirms `[localhost]`). The test predated hostname resolution and was
      racing the resolver; it now accepts either. Verified pre-existing by
      rebuilding the ORIGINAL table and reproducing the identical failure —
      not a regression from this change.
- [x] **Player help content pass** — done. User: "player help files get
      priority" / "help playing remove the phrase 'Sneezy always warned
      about'". Removed that phrase; fixed the hand-authored `classes`
      topic (typos, missing Druid); two stale "goto is immortal-only"
      test assumptions swapped to `transfer` instead, per "leave [the
      help architecture] alone and resolve conflicts as they occur."
- [x] **Sweep-failure triage after the char-creation reorder** — done.
      A 101-passed/23-failed sweep was triaged file-by-file; 9 were real
      stale-step-order regressions in test scripts (fixed), 14 were
      pre-existing unrelated test bugs or DB seed drift, none were
      product-code regressions. Full breakdown in STATUS.md. Found (but
      did not fix, out of scope) an architectural pulse-timing bug in
      `game_loop.c` — flagged as background task `task_2c2e0409`.
- [x] **Pulse scheduler timing bug (`game_loop.c`)** — done (background
      task `task_2c2e0409`, follow-up to the item above). The pulse
      counter advanced once per main-loop iteration, and `select()`
      returns immediately whenever a socket has data ready -- so under
      concurrent connection traffic the loop (and every `pulse_register()`
      system: HP regen, combat rounds, the game clock, mob AI, zone
      aging, puddle decay) could fire far more often than its constant's
      real-time meaning implied. Fixed by gating pulse advancement on
      real elapsed wall-clock time (`clock_gettime(CLOCK_MONOTONIC)`)
      instead of loop iterations, with a bounded catch-up (`
      MAX_PULSE_CATCHUP`) so a genuine stall (slow query, debugger pause)
      doesn't queue an unbounded burst either. Verified via
      `smoke_test_trigger_seed.py` (the flaky "damage 2" check that
      originally surfaced this now passes clean), plus
      `smoke_test_combat.py`/`smoke_test_zones.py`/`smoke_test_gametime.py`/
      `smoke_test_multiplay.py`.
- [x] **`who` reports active links / linkdead / total player count** — done
      2026-07-17. User: "who should report player count (active links) and
      linkdeads in a total player count" -- raised while diagnosing the
      connection-handling bug below, to make linkdead accumulation directly
      observable instead of inferred. New `world_count_linkdead()`
      (world.h/world.c), a read-only twin of the existing
      `world_purge_linkdead()` (same room-walk, just doesn't destroy
      anything). `who`'s footer now always shows the GLOBAL count
      (`<c>N active links, M linkdead, N+M total players.<z>`), regardless
      of any filter (`who imm`/`who mort`/`who <name>`) applied above it --
      this is a server-health stat, not a scoped listing. Verified live:
      a fresh post-restart instance currently shows 0 linkdead, which
      rules OUT "accumulated orphaned linkdead bodies" as the connection
      bug's cause, at least as the state stands right now.
- [x] **RESOLVED 2026-07-17 (same day, continued session): this was NEVER a
      server bug. It was a false positive caused by every diagnostic/catch
      script in this investigation (including this session's own) using a
      "still running after N=15-25 seconds -> STUCK" heuristic that was too
      tight for the test helper's actual, correct, worst-case runtime.**
      `smoke_test_kill.py`'s `recv_all(sock, timeout)` helper ALWAYS blocks
      for the full `timeout` on every single call -- it only returns early
      on EOF/connection-close, never early just because a complete reply
      arrived (it can't tell "done" from "server is about to send more").
      `make_player()` calls it ~9 times per character created (name, y,
      pw, pw-confirm, "new", name, race, class, done, alignment), and the
      full script creates 5 characters (2 in Part 1, 3 in Part 2) --
      roughly 45+ seconds of individually-correct, unavoidable blocking
      even with ZERO bugs anywhere. Every "hang" reproduced and gdb-
      captured throughout this investigation (including a fresh capture
      this session showing `d->out_len == 0`, `raw_pos == raw_len` -- no
      buffered/dropped bytes anywhere, socket-level TCP receive buffer
      genuinely empty) was the script still legitimately running, not
      stuck. **Proof**: re-ran the exact same reproduction with a 90s
      timeout instead of 25s -- completed in 72.8 real seconds with every
      check passing except one unrelated minor assertion (see below); no
      hang, no stall, no server-side anomaly of any kind.
      **Lesson for future sessions**: when a test script "hangs," check
      its own blocking-call budget (timeout × call count) against however
      long you're willing to wait BEFORE suspecting the server. A `kill
      -0 $PID` check after N seconds only proves "still running," not
      "stuck" -- these are very different for a script built on
      always-block-the-full-timeout helpers like `recv_all()` here.
      **One genuine, minor, unrelated finding from the 90s run**: the very
      last check (`the unsolicited broadcast still leaves the bystander at
      a prompt`, `smoke_test_kill.py` line ~195) failed -- `outObs`'s
      trailing prompt didn't arrive within `recv_until()`'s 5s deadline
      even though the death-taunt broadcast itself did. Center Square (the
      test's room) had accumulated a couple dozen `(linkdead)` bodies from
      this session's own repeated test runs by that point -- plausible
      that room-broadcast iteration over that many entries pushed the
      trailing-prompt emission past 5s under that specific clutter, though
      this wasn't confirmed. Low priority; likely resolves itself once
      test-generated linkdead debris is cleaned out of the DB (hundreds of
      `Kill*`-named test characters have accumulated across today's
      sessions -- a housekeeping pass, not a code fix, is probably all
      this needs). Not investigated further this session.
      **Real, unrelated improvement made while chasing this (kept
      regardless of the false-positive finding above)**: `socket_write()`
      (`src/net/socket.c`) was a single raw `write()` with no retry and no
      partial-write handling -- on `EAGAIN`/`EWOULDBLOCK` it silently
      dropped the data entirely, and every call site ignored a short
      write's return value too. This was a real, if apparently never-yet-
      triggered-in-practice, data-loss bug. Replaced with a proper
      per-descriptor output backlog: new `descriptor_write()`/
      `descriptor_flush_output()` (`descriptor.h`/`descriptor.c`) queue
      whatever a `write()` call doesn't finish into a new `out_buf`
      (`DESC_OUT_BUF` = 64KB) on the descriptor, and `game_loop.c` now
      also watches `writefds` for any descriptor with backlog and retries
      via `descriptor_flush_output()` each iteration. Every prior
      `socket_write()` call site (telnet negotiate, NOP keepalive,
      character echo/backspace, `descriptor_send()`, the snoop mirror, the
      prompt writer) now goes through `descriptor_write()`. `cmd_copyover.c`
      flushes every descriptor's backlog right before `execl()` too (a
      backlog lives only in this process's heap and would otherwise vanish
      across the exec). Verified: clean build, live-deployed to the Home
      VM production server via a hard kill+restart (user: "when you need a
      reboot, just hard boot as i may not be here to copyover"), binary
      md5 confirmed live.
      Historical investigation trail preserved below for context (repro
      attempts, ruled-out theories, tooling notes) -- none of it pointed
      at a real bug in the end, but the methodology notes (gdb without
      sudo, A/B pristine-build testing, etc.) remain useful technique
      references.
- [x] (historical, see RESOLVED entry above) **investigation trail for the
      "connection-handling bug"** -- originally discovered 2026-07-17 while
      triaging sweep failures for a suspected regression from that
      session's other work. **Confirmed NOT a regression**:
      reproduces identically against a completely pristine build of commit
      `74ead6d` (the last commit before ANY of 2026-07-17's changes),
      built fresh in `/tmp/ab_pristine` on a separate port (4001) sharing
      the same DB, isolated from the live server entirely. Symptoms:
      - A connected session (already past login, mid-test-script) simply
        stops responding to further commands -- `recv()` blocks
        indefinitely, well past any test's own bounded retry/timeout
        logic (ruling out the test scripts themselves).
      - The server process itself stays alive and healthy throughout --
        confirmed via `top` (near-idle CPU, no busy-loop), continued zone
        resets firing on schedule, MySQL `SHOW PROCESSLIST` showing only
        idle `Sleep` connections (no lock/deadlock), and a stable file
        descriptor count (`/proc/<pid>/fd`, nowhere near any limit).
      - Reproduces under BOTH concurrent-load patterns observed so far:
        3 near-simultaneous NEW connections (`smoke_test_kill.py`'s
        Imm/Tgt/Obs, `smoke_test_weapon_depth.py`'s bare-handed combat
        against a mob), and just 2 ALREADY-CONNECTED sessions under
        general server activity (`smoke_test_practice.py`'s cleric+
        immortal pair, `smoke_test_skills.py`'s sequential make_char()
        loop). No single common trigger pinned down yet -- looks load/
        timing-sensitive rather than tied to one specific command.
      - When it happens, typically TWO OR MORE connections "lose their
        link" in the log at the exact same timestamp, and any newly
        INITIATED connection around that same moment never completes
        character creation either (its socket opens at the OS level --
        logged "New connection" -- but the app-level accept/login flow
        never proceeds for it).
      - Affects at minimum: `smoke_test_affects.py`, `smoke_test_continue.py`,
        `smoke_test_immortal_castpray.py`, `smoke_test_kill.py`,
        `smoke_test_logging.py`, `smoke_test_ordinal_target.py`,
        `smoke_test_practice.py`, `smoke_test_skills.py`,
        `smoke_test_trap.py`, `smoke_test_weapon_depth.py`,
        `smoke_test_weapon_messaging.py` (11 of the 2026-07-17 sweep's
        113-passed/11-failed run) -- likely more under the right timing,
        this just happens to be the set that hit it during one sweep run.
      - **Next steps for whoever picks this up**: this smells like a
        `select()`/descriptor-handling bug in `game_loop.c` or
        `descriptor.c` -- something that can leave a connection's read
        state stuck (not polled, or polled but never actually serviced)
        once enough OTHER connections/activity are in flight at once.
        Worth checking: is `maxfd` tracked correctly as connections churn
        (a stale/wrong `maxfd` after several opens+closes could exclude a
        newer fd from the `select()` set entirely)? Does anything hold a
        connection's descriptor in a state where the main loop's dispatch
        skips it (an editor-mode flag left set, a pager state left
        pending, a wait-state counter that never decrements)? A live
        reproduction with `strace -f -p <pid>` attached at the moment of
        stall, or add temporary trace logging around `select()`'s fd_set
        construction, would likely nail it faster than guessing further.
      - This is now the PRIMARY known blocker for a fully clean sweep --
        every other 2026-07-17 sweep failure has a specific, understood
        cause (see the two entries below).
      - **Follow-up investigation session (2026-07-17, continued)**: set
        up a real git checkout on the Home VM (see SYNC.md) and did a full
        clean server restart specifically to chase this further. Findings:
        - `game_loop.c`'s `select()` loop recomputes `maxfd` fresh every
          iteration (not cached) -- ruled OUT the "stale maxfd excludes a
          new fd" theory.
        - Sockets are confirmed non-blocking (`O_NONBLOCK` set in
          `socket.c`); `descriptor_process_input()` correctly handles
          `EAGAIN`/`EWOULDBLOCK`.
        - **Live lead, not yet confirmed or ruled out**: `db.c`'s
          `db_query()` calls `mysql_query()` synchronously with NO
          timeout configured anywhere (`pool_get()` never calls
          `mysql_options()` for `MYSQL_OPT_READ_TIMEOUT`/`_WRITE_TIMEOUT`/
          `_CONNECT_TIMEOUT`). Since this is a single-threaded event loop,
          ANY query that blocks server-side (e.g. an InnoDB row-lock wait,
          default 50s timeout) would freeze EVERY connection for that
          whole duration. Attempted to catch this live with `gdb -p <pid>
          -batch -ex 'thread apply all bt'` at the exact moment of a
          reproduced hang -- blocked by `sudo` requiring a password
          non-interactively (same `dnf` gotcha noted elsewhere in this
          file); never got a live backtrace. **Next session should either
          get passwordless sudo for gdb (a scoped `visudo` drop-in, same
          idea as the `dnf` one already suggested in SYNC.md) or run gdb
          as a NON-root ptrace of a same-UID process (should work without
          sudo at all if `/proc/sys/kernel/yama/ptrace_scope` isn't
          hardened -- try `gdb -p <pid>` as plain `mud` first, before
          reaching for sudo).**
        - Built a `who` command upgrade (see the entry above) specifically
          to make one hypothesis (accumulated linkdead bodies degrading
          room-list operations) directly observable rather than inferred.
          **Ruled OUT** for the current server lifetime: a freshly
          restarted, actively-tested instance showed 0 linkdead at a point
          where the hang had already been reproduced multiple times since
          the restart -- so linkdead accumulation is not (solely)
          responsible, at least not on a short timescale.
        - Reproduced the hang inconsistently even right after a clean
          restart: the FIRST full run of `smoke_test_kill.py` post-restart
          passed completely ("ALL CHECKS PASSED"), but 3 immediate
          back-to-back re-runs all hung at the same early point (right
          after Part 1). This rules out "purely a long-uptime degradation"
          as the sole explanation too -- something about RAPID repeated
          runs specifically seems to matter, not just elapsed server
          uptime. Possibly relevant: rapid-fire test runs create many
          near-simultaneous DB writes (SQL from the raw `mariadb` CLI in
          test helpers, interleaved with the server's own queries) --
          worth checking `SHOW ENGINE INNODB STATUS` for lock waits
          DURING an active hang (not after, which is what was checked this
          session -- by the time `SHOW PROCESSLIST` was checked, whatever
          was blocking had already resolved).
        - Also confirmed some of THIS session's own ad-hoc diagnostic
          scripts (not the real test suite) produced a red herring: a
          quick script using a NUMERIC name suffix hit the pre-documented
          "names must be letters only" rejection, which cascaded into a
          confusing "reconnect shows an empty account menu" symptom that
          looked like the real bug but was purely a scripting mistake, not
          a server issue. The REAL test suite (`smoke_test_kill.py` etc.)
          already uses the correct letters-only suffix and was never
          affected by this -- noted here only so a future session doesn't
          waste time rediscovering the same false lead.
        - **BREAKTHROUGH, still not fully solved: got a clean, isolated,
          gdb-confirmed backtrace of the server at the exact moment a hang
          was reproduced (`gdb -p <pid> -batch -ex 'thread apply all bt
          full'` -- works fine WITHOUT sudo, same-UID ptrace, no need for
          the passwordless-sudo workaround speculated above).** The result
          rules out a server-side hang ENTIRELY: the main thread is
          sitting in a plain, healthy `select()` call
          (`game_loop.c:144`), `ready=0`, correctly watching exactly the
          real live fds (`readfds` bitmask decodes to fds 5/6/11/12 --
          listen socket + the 3 actually-connected sockets, confirmed
          against both `/proc/<pid>/fd/` and `ss -tnp | grep 4000` at the
          same instant, with no mismatch). No stuck DB call, no infinite
          loop, no dropped/orphaned descriptor still open but unwatched --
          every hypothesis chased earlier this session (stale `maxfd`,
          blocking `mysql_query()` with no timeout, orphaned linkdead fds)
          is now DEFINITIVELY ruled out by direct observation, not just
          reasoning.
          **This flips the investigation: the server is innocent and
          idle, so the bug must be in what the CLIENT is waiting for that
          never arrives** -- either the server already sent a valid
          response that the test script's own parsing/state-tracking
          fails to recognize (causing it to keep waiting on a `recv()`
          that will genuinely never satisfy its check), or a genuine
          telnet-protocol-level edge case (an IAC negotiation sequence,
          a color-code edge case, something `drain_lines()` mishandles
          for one specific input shape) that leaves the CLIENT confused
          about what it already received, even though the SERVER did its
          job correctly. Next session: capture the RAW BYTES the client
          actually received right before it stalls (not just the
          decoded/printed text) and diff that against what a working run
          received at the equivalent point -- the answer is almost
          certainly hiding in a byte-level mismatch, not a logic bug in
          either the server's command dispatch or its select() loop,
          both of which are now confirmed healthy.
        - Added `crash_handler.c`/`.h` (user, once this investigation
          confirmed the server ISN'T crashing during this specific bug:
          "if the game detects an oncoming crash, add debug info to a
          separate log file... or do a core dump with the datetime
          attached"). Confirmed Fedora's `systemd-coredump` is already
          active and `ulimit -c` is unlimited on the Home VM -- a REAL
          crash already gets a fully timestamped, retrievable core dump
          for free via `coredumpctl list`/`coredumpctl dump <pid>`, no
          changes needed there. Added a lightweight supplementary
          handler (SIGSEGV/SIGABRT/SIGFPE/SIGBUS/SIGILL) that writes a
          quick app-level marker (timestamp, signal, uptime, active
          connection count) to `logs/crashes/crash-<timestamp>-pid<pid>.log`
          before re-raising the signal with default disposition, so the
          OS's own core dump still happens -- this just adds context a
          raw core dump doesn't carry (how many players were connected).
          Doesn't apply to the bug investigated above (confirmed NOT a
          crash), but is real defensive infrastructure for whatever
          crash, if any, comes up in the future.
- [x] **`smoke_test_practice.py`/`smoke_test_skills.py` rewritten for the
      practice-system redesign** — done 2026-07-17, partially verified at
      the time (see the RESOLVED "connection-handling bug" entry above --
      both runs were flagged as hitting that issue partway through, but
      it's now understood there was no real hang; they just hadn't been
      given enough wall-clock time to finish. Every check that ran before
      the timeout passed either way, validating the rewrite logic itself
      -- worth a follow-up run with a generous timeout to confirm full
      completion, not yet re-verified end-to-end this session).
      `smoke_test_skills.py`: Combat tier is no longer "innate" (this
      session's redesign made it a real discipline gated by
      `combat_disc_pct`, same as Basic/Advanced) -- a fresh 0%-everywhere
      character now sees every skill locked-by-discipline, breaking the
      old assumption that a level-1 skill like "bash" shows known by
      default. Fixed by maxing out all three disciplines via SQL right
      after character creation (`set_disciplines()`), reconnecting once so
      the live `being_t` actually picks it up (SQL never reaches an
      already-connected session's in-memory state, a lesson repeated
      throughout this session's other test fixes). Also added
      `cmd_paged()`: `skills`' new pagination (see the pagination TODO
      entry) means a single class's full 3-tier roster can exceed the
      pager's 20-line default page, so the test now drains every page by
      sending blank lines until no "ENTER for more" prompt remains.
      `smoke_test_practice.py`: full rewrite, not a patch -- the entire
      flat-+10%-per-use, no-guildmaster-required-refused, Combat-doesn't-
      exist model it tested is gone. Covers the actual current behavior:
      bare `practice` works anywhere showing 0%/0 points; `practice
      <discipline>` (no count) shows that discipline's listing anywhere,
      no guildmaster needed; `practice <discipline> <count>` is the only
      form that spends and DOES need the matching guildmaster (refused
      with none present, refused at the wrong class's); grants points via
      `set <name> practices <n>` (dogfooding this session's own new `set`
      field); spending raises the percentage and stops at 100%
      ("already mastered" on a re-spend); Advanced refused until Basic
      AND Combat both hit 100% (`set <name> combat 100` again for live-
      sync, not raw SQL); `practice <yourclass>` as a Basic synonym; and
      -- the main new mechanic -- per-skill proficiency actually gating
      `pray` success: a spell SQL-forced to 100% proficiency
      (`player_skill` table) succeeds reliably, one left at the natural
      1% first-attempt floor fumbles in a clear majority of a small
      statistical sample (8 attempts, expects >=5 fumbles). Found and
      fixed one bug in my own first draft while writing this: a raw SQL
      `combat_disc_pct` update while the cleric stayed connected
      wouldn't reach the live session either -- switched to `set <name>
      combat 100` for the same live-sync reason as skills.py above.
- [x] **Practice system redesign — GO-AHEAD GIVEN 2026-07-13, design is
      fully locked, implement without asking for further design input.**
      (multi-part, not yet implemented) — user: "practice needs to work
      differently. a
      player that levels gets 6-8 practices per level gain (calculated
      by wisdom as a modifier) and spends those practices at their
      guildmaster. they can split their practices among combat skills
      and basic until they have learned 100% in both disciplines before
      they can move on to advanced. The advanced guild master, the basic
      guildmaster, and the combat guildmaster need to be different mobs
      located in different places. the combat guildmaster can be shared
      with all classes, the basic and advanced ones must be separate.
      the goto for mortals should be modified to go to each guildmaster;
      goto guildmaster goes to basic guildmaster, goto combat goes to
      the shared combat trainer, and goto advanced should state that no
      one knows where the advanced trainer is." Refined via follow-up
      into: practice points on level-up = `random(6,8) +
      round(wisdom_bonus * wisdom_practice_modifier)` (wisdom_bonus =
      floor((wisdom-120)/10), modifier defaults 1 via new `game_config`
      row + `balance wisdom` subcommand); each point spends for a random
      1-2% discipline gain; three disciplines (Basic/Combat/Advanced,
      Combat is new); Advanced unlocks only once Basic AND Combat both
      hit 100%. Guildmasters: Basic = existing level-51 mobs (unchanged),
      Advanced = existing level-100 mobs (unchanged), Combat = **6 NEW
      per-class mobs** (reversed from "shared" after user confirmed
      6-per-class is simpler in code — "would it be easier to just have
      6 combat trainers, one for each class?"). `goto combat` checks the
      caller's class and routes to that class's own trainer (same
      mechanism `goto guildmaster` already uses); `goto advanced` always
      refuses with flavor text, no pathfinding. NOT YET IMPLEMENTED —
      implementation order:
      1. DB schema: `ALTER TABLE player_progress ADD COLUMN IF NOT EXISTS
         combat_disc_pct ...` + `practice_points ...`; a `game_config` row
         for `wisdom_practice_modifier` (default `'1'`), following
         `multiplay.c`'s exact load/cache/set pattern (`src/core/`).
      2. Level-up hook: `progress_add_xp()` (`src/core/being.c:647`) is
         the actual level-incrementing function -- it loops `p->level++`
         per level crossed and returns `levels_gained`, but only touches
         a bare `progress_t` (no attrs/class access). Its one caller,
         `combat_defeat()` in `src/core/combat.c` (around line 466-488,
         right where it already does the post-level-up full-heal/"You
         feel more experienced!" reward using the full `being_t winner`),
         is where to add the practice-points award -- loop
         `levels_gained` times, each awarding `random(6,8) +
         round(wisdom_bonus * wisdom_practice_modifier)` where
         `wisdom_bonus = floor((winner->attrs.wisdom - ATTR_BASE) / 10)`.
      3. `cmd_balance.c`: add a `balance wisdom [<value>]` subcommand
         (view with no arg, set with an arg) -- a direct scalar
         read/write, not the existing menu-driven
         `descriptor_balance_begin()` machinery (that's for 4-field
         class/race records).
      4. `cmd_practice.c`: full rewrite -- practice-points-as-spendable-
         resource instead of unlimited flat-step visits, three
         disciplines each with their own guildmaster-type match, random
         1-2%-per-point spend, Advanced gated on Basic==100 AND
         Combat==100. **Syntax** (user 2026-07-13): `practice <discipline>
         [<#>]` -- e.g. `practice combat 7` spends 7 points on Combat in
         one command instead of making the player type `practice combat`
         seven separate times; bare `practice <discipline>` (no count)
         still spends exactly 1, same as today. Spend loop must stop
         early and report how many points actually landed if the player
         runs out of points or the discipline hits 100% partway through
         a requested `<#>` (don't silently no-op the whole batch, and
         don't let a discipline overshoot 100% mid-batch either).
      5. `cmd_goto.c`: repurpose `goto guildmaster`→Basic (existing BFS
         logic, unchanged target type), add `goto combat`→per-class
         Combat trainer (class-aware routing, same mechanism as Basic),
         add `goto advanced`→always refuses, no pathfinding attempted.
      6. Combat guildmasters — **RESOLVED 2026-07-17, no new world content
         needed.** The check-in happened and both open questions are now
         decided by the user:
         - **Reuse the existing level-80 guildmaster tier** as the Combat
           trainers, rather than creating 6 new mobs/rooms. Investigating
           the seed data turned up a THIRD guildmaster tier the locked
           design never accounted for: Sneezy ships 51/80/100 sets
           (keyworded `level15`/`level40`/`level50`), each one-per-class
           and each already placed in a distinct room. The level-80 set
           covers exactly Tobin's 6 classes and already sits scattered
           away from the city guild offices — which is precisely the
           "different mobs located in different places" requirement.
           Combat trainers, mob → room: 216 mage → 14435 In a Pipeweed
           Patch; 217 cleric → 11309 Tiny Alcove; 218 warrior → 7800
           Watchtower On The Southern Wall; 219 thief → 11472 Inside the
           Pile; 223 monk → 7803 Large Chamber in the Rock; 220
           ranger→Druid → 23293 Tree Spanning a Stream in Arden Forest.
           (Masks 16/32 = shaman/deikhan stay unmapped by
           `mob_class_mask_to_tobin()`, so those tier-mates are invisible
           to Tobin — harmless.)
         - **Identify role by `mob.level`: 51 = Basic, 80 = Combat, 100 =
           Advanced.** Zero DB edits, the levels are already distinct in
           the seed. `find_guildmaster()` takes a tier argument and
           matches keyword "guildmaster" + class + level. Known tradeoff
           the user accepted: it's an implicit convention, so a
           guildmaster added at some other level matches nothing — keep
           the tier levels named as constants in one place, and if that
           ever bites, switch to explicit `basic`/`combat`/`advanced`
           keywords (~18 UPDATE rows).
      7. Help topics (`practice`, `goto`, `skills`, `balance`) + wiznews
         entry once shipped, plus new/extended smoke tests covering
         practice-point earning/spending, the three-discipline gate, the
         three `goto` forms, and `balance wisdom`.
      **Shipped 2026-07-17, live and verified.** Three follow-up
      refinements landed post-ship from in-game testing (user, same
      session): (a) bare `practice` originally required a guildmaster in
      the room just to show your own percentages/points -- fixed to
      always show your own status regardless of location, only adding a
      guildmaster's training-prompt line when one happens to be present;
      (b) that training-prompt line originally suggested all three
      disciplines regardless of which guildmaster tier was actually
      standing there -- fixed to suggest only the one that guildmaster
      teaches (`find_guildmaster()`'s strict tier match already refused
      the others correctly, only the flavor text was misleading); (c)
      `practice <yourclassname>` (e.g. `practice warrior`) now works as a
      synonym for `practice basic`, matching how `skills` already labels
      that tier by class name ("Warrior Skills", not "Basic Skills");
      (d) new `goto <classname>` (e.g. `goto thief`) gives directions to
      that NAMED class's own Basic guildmaster, not just the caller's own
      class -- checked last in the landmark chain, so it never shadows
      the fixed landmarks above it.
- [x] **Message boards + related commands** — done 2026-07-18. User:
      "implement message boards and related commands from sneezy" ->
      "we need to make bulletin boards function, read and write commands,
      from sneezy". Real seeded ITEM_BOARD objects (obj.type=24, e.g.
      "board bulletin galek brightmoon") and the real upstream
      `board_message` table (already-imported schema, FK'd to obj.vnum)
      back two new commands: `read` (no arg lists a board's live posts;
      `read <#>` shows one in full) and `write <subject> <message>`
      (posts directly). Per-board minimum level is the real seeded
      `obj.val0` (e.g. 52 for "board bulletin wizard immortal"), gating
      both commands with no immortal bypass, matching the original's own
      boardHandler. `read at <name>`/`write at <name> ...` disambiguates
      when a room has more than one board (found live during testing --
      a builder office has both a Wizard board and a plain bulletin
      board); with only one board present the "at" prefix is never even
      inspected, so an ordinary subject that happens to start with the
      word "at" still posts fine. Deliberately scoped DOWN from the
      original's two-step "write a note object, then post the note" flow
      -- Tobin has no separate writable-note-object system, so `write`
      inserts straight to the board instead; faction-gated boards
      (Brotherhood/Serpent/Logrus) are skipped entirely (no faction
      system); pulling a post back off the board (`get <#>` upstream) is
      not included -- board_repo.h's `date_removed` column is ready for
      it whenever that's wanted.

## Small near-term gameplay follow-ups

- [x] **`quit!` drops all possessions on the ground, gold included** —
      done. User (2026-07-12): "after rent goes in quitting the game
      will drop all possessions on the ground where the quit command
      was executed, gold included." Now that `rent` (above) exists as
      the safe way to leave with belongings intact, plain `quit!`
      (`cmd_quit.c`) is the risky option Sneezy's own `rent` help text
      warns about: everything carried/worn/held (same unified
      `stuff_head` chain combat_defeat()'s corpse population already
      walks) spills onto the floor of the current room the moment you
      quit, both the quitter and the room are told, and it's saved via
      the existing `player_inventory_save()` -- matching what `drop`
      already does, so it inherits the same accepted limitation (a
      loose room object isn't otherwise persisted across a restart).
      Gold specifically is NOT covered -- there is no `gold`/Money field
      on `being_t` at all yet (task 29 is still not built), so there's
      nothing to drop; noted honestly rather than faked.
      `tests/smoke_test_quit_drop.py` covers the drop + both messages +
      the item leaving inventory on reconnect, and confirms an
      empty-handed quit says nothing about spilling.
      `smoke_test_quit.py`/`smoke_test_save.py`/`smoke_test_rent.py`
      re-run clean (shared `cmd_quit.c` touched).
- [x] **Catch up on help file entries** — done. User (2026-07-12):
      "catch up on the help file entries", then, as a direct follow-up:
      "i want very detailed help files. Especially wiz* help files. i
      want it so a first time player of this game will feel comfortable
      playing because he knows where to find game play information and
      administration detailed so new immortals can know what commands
      do and why we use them." A command-table-vs-`help_topic`-table
      diff found 23 real commands with NO help entry at all (`stat`,
      `save`, `rent`, `cast`, `pray`, `practice`, `skills`, `affects`,
      `consider`, `continue`, `examine`, `show`, `sip`, `tell`,
      `whisper`, `balance`, `egotrip`, `settrap`, `disarmtrap`,
      `hurtlimb`, `aitick`, `immort`, `test`) -- all written with real
      detail, not one-liners. Two new prose-only orientation topics were
      also added (`db/sneezy/help_topic.sql`): `playing` (first-time
      player: look/movement, character info, talking, fighting, class/
      skills/practice, rent-vs-quit!, reporting problems) and
      `administration` (new immortal: the full 51-60 level ladder
      explained by WHY each tier sits where it does, building, running
      the playerbase, and -- the user's specific ask -- why each debug
      tool exists: `hurtlimb`/`aitick`/`stat`/`balance`/`egotrip`/`test`
      all exist because real combat/world-ticks/decay are too slow and
      random to test against directly). Both are the most discoverable
      spots possible: `help`'s and `wizhelp`'s own bare-no-argument
      footers (`cmd_help.c`) now point at them, and a brand-new
      character sees a one-time "type help playing" nudge right at
      creation (`descriptor.c`, shown once, never again on a later
      relog). Found and fixed a real, previously-undiscovered bug while
      deploying: the `edroom`/`edzone`/`edplayer`/`edhelp`/`ednews`/
      `edwiznews`/`edrules` -> `edit <noun>` rename migration (Session
      21) was never actually idempotent -- the top-level seed INSERT
      kept silently re-creating a fresh row under each OLD name on every
      subsequent deploy (nothing left under that name to no-op against
      once the first run renamed it away), so the rename UPDATE then
      collided with the already-renamed row and aborted the whole file
      partway through on any second deploy. Fixed with a guarded DELETE
      for each of the 7 pairs (only when the correctly-renamed row
      already exists, so a genuinely fresh database still renames
      normally); verified idempotent by running the file twice in a
      row. Also discovered `copyover` already had a help topic --
      hand-edited in-game by a real immortal account (`updated_by`
      != 'seed'), never in this seed file at all -- correctly left
      alone by the existing `ON DUPLICATE KEY UPDATE name=name` guard
      rather than clobbered. `tests/smoke_test_help_content.py` covers
      the creation nudge (once, not on relog), both new orientation
      topics, `help`/`wizhelp`'s footers, and a representative sample of
      the newly-added topics (mortal-visible and immortal-visible
      alike). This is a first substantial pass, not a claim that every
      pre-existing terse topic has been rewritten -- see the SQL file's
      own comment; further deepening can continue incrementally.
- [x] **XP on kill** — done (Session 43): `combat_defeat()` awards
      `loser->progress.level * 50` XP (placeholder formula, same precedent
      as other placeholder combat/growth numbers) via the already-existing
      `progress_add_xp()`, and saves it. Only for a non-immortal PC winner
      -- covers a normal defeat and a decapitation, but not an immortal's
      `cmd_kill` instakill (that winner is always an immortal, who doesn't
      need XP).
- [x] **Mid-fight persistence** — done 2026-07-19. HP was only saved at
      defeat/quit (`descriptor_leave_to_menu()`); a mid-fight disconnect
      (crash, or a losing player quietly pulling the plug) reloaded at
      whatever HP was last saved BEFORE the fight even started, silently
      undoing all damage taken -- a real crash-loss risk, and a soft
      exploit. `combat_process_run()` (combat.c) now calls the existing
      `player_progress_save()` for both PC participants after every round
      the fight is still ongoing (not just at defeat) -- reuses the exact
      same call the defeat/gold-drop paths already made, not a new
      mechanism. Limb HP is explicitly OUT of scope here -- it isn't
      persisted at all yet, by ANY path, defeat included (see
      player_repo.c's own note under "`player_save()` + a `save` command"
      above); that's the separate, still-open "Meaningful limb damage" item.
      New `tests/smoke_test_mid_fight_persist.py`: fights a deliberately
      tanky sandbox mob, waits for real damage, then hard-closes the
      socket (no `quit!`, simulating a crash) and reconnects -- HP landed
      exactly where it was the instant before the disconnect, not reset to
      the pre-fight max.
- [x] **`player_save()` + a `save` command** (user request, 2026-07-07) —
      done. A single `player_save(player_id, being_t*)` (`player_repo.c`/
      `.h`) persists attrs, progress (level/xp/hp/etc), and inventory in
      one call -- mirrors the original's real `TBeing::doSave()`
      (`cmd/cmd_save.cc`), a genuine port, not a Tobin invention. A new
      player-invokable `save` command (`cmd_save.c`) calls it on demand
      ("Saved." / "Save failed -- the database is unavailable."). Limb HP
      deliberately isn't included -- it's never persisted at all,
      recalculated from strength on every fresh load (see being.c).
      Follow-up (user, 2026-07-12): **quit and death should auto-save**.
      Confirmed this was a real gap, not just theoretical: neither
      `cmd_quit.c`'s `descriptor_leave_to_menu()` nor `being_destroy()`
      persisted anything before freeing the character, so state changed
      since the last scattered save call (attrs, inventory, HP) was
      silently lost. Fixed by calling `player_save()` from
      `descriptor_leave_to_menu()` right before `being_destroy()` frees
      the character -- covers both `quit!` (`cmd_quit.c`) and a PC's
      combat defeat (`combat_defeat()`'s loser-has-a-desc path,
      `combat.c`), since both already route through that one function.
      `tests/smoke_test_save.py` uses mid-fight HP loss as the test
      signal, since it's the one state change in the game deliberately
      NOT saved the instant it happens (see "Mid-fight persistence"
      above): confirms damage sits unsaved in memory until `save` (or a
      `quit!` with no explicit save first) writes it to
      `player_progress.hp`. `smoke_test_quit.py`/`smoke_test_multiplay.py`
      re-run clean (shared `descriptor.c`/`cmd_table.c` touched, but only
      additive).
      **Test-methodology fallout**: this broke a widespread smoke-test
      convention -- roughly a dozen files raise a test character's level
      via a raw `UPDATE player_progress` while still connected, then use
      `quit!`+reconnect to force a fresh DB load (since a live connection
      never re-reads the DB on its own). `quit!` now saves the live
      (stale, pre-SQL-edit) in-memory progress right before destroying
      the character, silently clobbering the SQL edit. Fixed by
      reordering every affected call site to run `set_level()` (or
      equivalent) AFTER `quit!`+close, while the character is fully
      offline, rather than before: `smoke_test_stat.py`,
      `smoke_test_balance.py`, `smoke_test_castpray.py`,
      `smoke_test_continue.py`, `smoke_test_egotrip.py`,
      `smoke_test_limb_severity.py`, `smoke_test_pee.py`,
      `smoke_test_practice.py`, `smoke_test_purge.py`,
      `smoke_test_sneezy_ports.py`, `smoke_test_weapon_messaging.py`.
      Not a gameplay regression -- real admin paths (`promote`, `set`)
      already update the live in-memory character AND the DB together
      (see `cmd_promote.c`'s explicit comment on this), so this only
      ever bit the raw-SQL test convention. A plain disconnected socket
      close (no `quit!`) was already safe and remains so --
      `descriptor_destroy()`'s link-drop path deliberately never
      auto-saves, for exactly this reason (see its own comment).

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
- [x] **Mob AI / aggression** — `ACT_AGGRESSIVE` shipped (see the
      "Mobile_Attitude"/alignment entry earlier in this file,
      `mob_ai_tick()` in mob_ai.c, `tests/smoke_test_alignment.py`) --
      entry pruned 2026-07-17, was a stale duplicate claiming it was
      "completely unused" after it had already landed. Scoped down from
      Sneezy's full attitude/opinion system (still real future work, see
      that entry) but a working aggro tick exists today, mobs are no
      longer reactive-only.
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
- [x] **Keys unlocking doors** — done 2026-07-19. New `lock`/`unlock
      <direction>` and `lock`/`unlock <container>` (cmd_lock.c). Researched
      the real matching rule against the bundled SneezyMUD C++ source
      (`has_key()`/`keyCheck()`, misc/movement.cc) before writing anything:
      a key is identified by its own OBJ VNUM, not any val[] field on the
      key -- this TODO's own original wording ("val[0]") turned out to be
      wrong, confirmed against real seeded key rows (val0/val1 both 0 on
      every one). A door's `roomexit.key_num` / a container's `val[2]`
      names the vnum a carried object must have; `room_t` gained an
      `exit_key[]` field to actually carry key_num through (it was selected
      into nothing before this and silently discarded). Both must be closed
      first before `lock` works; `unlock` only needs the key, doesn't care
      about open/closed. 1,141 real seeded doors and dozens of seeded
      containers already carry working key data -- immediately testable
      against real content. New `tests/smoke_test_keys.py` (23 checks,
      SQL-bootstrapped sandbox rooms/objects, same pattern as
      `smoke_test_doors.py`/`smoke_test_containers.py`). `cmd_table.c`
      entries verified with `tests/tools/cmd_abbrev_check.py` (zero
      abbreviation collisions among existing commands; also had to patch
      the tool itself -- it was missing 3 level macros added by later
      sessions and errored out before it could even run). Also corrected
      two stale comments this surfaced: obj.h's KEY val[0] doc (see above)
      and cmd_open.c's "a real lock/unlock command needs a key... deferred"
      note.
- [x] **Shops + money** — done 2026-07-17. `list`/`buy`/`sell` shipped
      against the real seeded shop economy (264 `shop` rows, `shoptype`
      buy-categories, `shopproducing` catalogs — not the keeper's carried
      items, which the seed data never actually stocks). Each shop keeps
      its own authentic flavor text (talens→gold bulk-renamed, 263 rows).
      **Shop editor NOT built** — real gap, still open. Immortals can't
      create/modify shops in-game; only read-only consumption of the
      pre-existing seeded tables exists.
- [x] **Player-state logging** — done. get/drop already logged
      (`LOG_SILENT`, cmd_object.c, predates this item). Added the other
      half: `title`, `prompt`, `poofin`/`poofout`, `bamfin`/`bamfout` each
      now log a `LOG_SILENT` line on set/clear, so `log search <name>`
      surfaces a player's vanity-customization history alongside their
      get/drop activity. `color` deliberately left out -- it's an
      account-level connection preference (toggled far more casually,
      closer to a terminal setting than "a player's story"), not a
      pfile change. `tests/smoke_test_pfile_logging.py`.
- [ ] **Body types** — `body.h` body-type concept (creatures have different
      limb sets). Pairs with mobs + limbs.

## Bigger systems (need design / a decision)

- [x] **Hospital (limb repair)** — done 2026-07-18: reuses the real seeded
      doctor shops (6 of them -- Tobin City, Amber, Logrus, Brightmoon, a
      field medic, Xanesla -- identified via `mob.spec_proc=48`,
      `shop_repo_is_hospital()`, no hardcoded vnum list) and the ROOM_HOSPITAL
      room flag (`goto hospital` finds the nearest one). `list`/`buy` at a
      hospital shop (`cmd_shop.c`) special-case into an ailment menu instead
      of the normal item catalog: every damaged limb and active disease
      (now also poison, and all 26 diseases -- see Diseases above) is
      priced and numbered, `buy <#>` cures it instantly (full limb heal or
      `being_remove_affect()`) and deducts gold. Instant, not timed/queued.
      Bugfix same day: the doctor's own line ("the Tobin City Doctor looks
      you over:") wasn't capitalized at the start of the sentence for any
      mob whose short_desc begins lowercase (a mid-sentence keyword
      convention, e.g. "the Tobin City Doctor") -- now runs through
      `being_display_name_cap()` (already existed, just wasn't used here)
      wherever a shopkeeper's name opens a message, including the plain
      `list`'s "<keeper> offers:" line too.
- [x] **Light refuel + the lamp-lighting boy** — done 2026-07-18 (user:
      "light refuel and the lamp lighting boy code need to be
      implemented, from sneezy"). Real seeded OBJ_CAT_LIGHT objects
      (lampposts, torches, lanterns) and ITEM_FUEL objects (sold at
      Lumor's Illuminations, room 550) already existed but had no command
      layer at all -- new `light`/`extinguish`/`refuel <light> <fuel>
      [held|room]` (cmd_light.c), matching the original's obj_light.cc/
      obj_fuel.cc split: refueling rejects a lit lamp ("might explode"),
      an already-full one, or an unrefuelable one (val[1]<0, e.g. a
      torch). Found and fixed along the way: obj.h's own val[] doc
      comment for LIGHT was WRONG (claimed val[0]=is-lit; the real
      seeded data is val[0]=radius, val[1]=max burn, val[2]=current burn,
      val[3]=is-lit) -- never noticed before since nothing read it.
      New `obj_light_burn_tick()` (obj.c, ~60s pulse like pool decay)
      drains 1 unit from every LIT light's burn -- room-floor, carried by
      a connected player, AND carried by a mob (world_for_each_obj()
      alone only covers room floors), extinguishing it at 0, silently
      (same "no message" precedent as pool decay). Room floor listings
      now tag a lit OBJ_CAT_LIGHT item "(lit)" (cmd_look.c) -- otherwise
      invisible state with no way to tell.
      Lamplighter: the original's "Lamp-Lighter" spec-proc (spec_mobs.cc,
      real seeded on mob vnum 99 "a lamp-lighting boy"/Grimhaven and 1303
      "an eager page"/Brightmoon) walks a hardcoded scripted patrol route
      between named lampposts (misc/paths.h). Tobin's version drops the
      patrol entirely (no path-following primitive exists) -- a mob whose
      `mob.spec_proc` (now cached on `being_t.mob_spec_proc` at spawn,
      mob_repo.h, so mob_ai.c's per-tick check needs no DB round trip)
      equals `SPEC_PROC_LAMPLIGHTER` just lights/extinguishes any
      "lamppost"-keyworded light already in its OWN current room each AI
      tick, gated on `gametime_is_daytime()`, auto-refueling to full each
      time it lights one -- same "infinite fuel supply for the NPC" the
      original's `lampLightStuff()` does. A lamplighter that never shares
      a room with a lamppost simply does nothing; honest scope-down, not
      faked patrol coverage. Verified live: a fresh lamppost (seeded
      already-lit) + the mob + `aitick` correctly extinguished it with
      the room message "... reaches up and extinguishes ... for the day."
- [x] **Classes** — already shipped: 6 classes (mage/cleric/warrior/thief/
      druid/monk) chosen at creation (`show_class_screen()`, descriptor.c),
      shown in score/who, with `class_stat_bonus()` (being.c) applying
      affinities on top of point-buy attributes -- entries pruned
      2026-07-17, were stale.
- [x] **Races** — already shipped: 6 curated player races (human/elf/ogre/
      dwarf/hobbit/gnome, `show_race_screen()`) with `race_stat_bonus()`
      (being.c), plus an open mob-race list not limited to those 6 (e.g.
      "GOBLIN", confirmed via `smoke_test_stat.py`'s mob race check).
- [x] **Game balance layer (60 ONLY)** — shipped as the `balance` command
      (not `gameedit`, but the same intent): `balance class|race <name>`,
      a menu-driven, DB-persisted, Implementor-only live tuner for 4
      gamewide class/race modifiers (HP/damage multiplier, to-hit/AC
      modifier), applied with no restart needed (`balance_repo.h`/
      `balance.c`, `tests/smoke_test_balance.py`).
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
- [x] **STATUS.md's "Module port status" table is stale** — audited and
      fixed 2026-07-19. Real count: 109 `cmd_*.c` handler files / 152
      registered verbs (was 11 at the old count). Retired the "N/66
      ported" framing entirely -- a straight filename cross-check against
      the original's 66-file `cmd/` dir found only 17 direct matches
      (reimplemented/simplified, not 1:1 ports); the other ~92 Tobin
      command files are new-to-Tobin *relative to `cmd/` specifically*,
      including several (`cmd_attack.c`/`cmd_kill.c`/`cmd_hit.c`/
      `cmd_flee.c`/`cmd_move.c`) that port functionality the original
      kept outside `cmd/` entirely (`fight.cc`, `act.movement.cc`). Full
      breakdown, including the 42 original files still unmatched by name
      (mostly combat maneuvers + the `low`/shop subsystem), now lives in
      STATUS.md's Module port status table.
- [x] **`stat obj|mob|room <name>` silently statted vnum 0** — fixed
      2026-07-18 (user: `stat o phos` dumped a blank "Object 0" instead of
      finding the vial of red phosphorus). `stat` only ever atoi()'d its
      vnum argument -- a non-numeric name silently became 0 rather than
      erroring, and vnum 0 happens to be a real (empty/placeholder) row.
      Now checks whether the argument is all-digits first; if not, it's
      resolved via a `name like '%...%'` search (same substring
      convention `vnum`/`obj_find_vnum_by_name` already use), erroring
      cleanly if nothing matches instead of ever falling through to atoi().
- [x] **`load` should bypass max_exist, not silently ignore it** — done
      2026-07-18 (user: "when a immortal loads an obj or mob... max exist
      should be bypassed with a warning to clean up after the immort is
      done... if he goes over max exist"). `load` never enforced a
      world-wide instance cap in the first place (zone.c's own comment
      already documents this as a deliberate simplification -- only
      per-room zone-reset caps are tracked) -- so there was nothing to
      "bypass". What it does now: after loading, counts every live
      instance of that vnum anywhere in the world
      (`world_for_each_mob()`/`world_for_each_obj()` + the real seeded
      `max_exist` column, now read into `obj_proto_t`/`mob_proto_t`) and
      warns (never refuses) the immortal if they've pushed it over that
      prototype's own limit, so they know to clean up. Every ALREADY-
      loaded instance genuinely was (and remains) an independent live
      object/mob, never a "prototype" reference -- `obj_create_from_proto()`/
      `being_create_mob()` always built a real, separate instance each
      call; the confusing `stat o phos` symptom above was `stat`'s own
      bug, not a `load` one.

## Reference material (Sneezy enums, provided 2026-07-04)

Upstream enums the user pasted, staged for the features above (kept in this
conversation and in `sneezymud-master`): `positionTypeT` (done), `prompt_mesg`
(health strings), `classInfo` (classes), `body_flags` (limb conditions),
`wearSlotT` (limbs), `heraldcodes`/`heraldcolors` (immortal color/heraldry),
`doorTypeT`/`doorIntentT`/`doorUniqueT` + `exit_bits` (door mechanics).

## Deferred decisions (blocked on choosing, not on code)

- [ ] Which ~8-10 `disc/` disciplines to keep; which 1-2 `task/` professions.
- [x] Hospital mechanic for destroyed limbs — done 2026-07-18, see above.
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
