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
- [ ] **Boxed ASCII-art menu rework, all character-facing menus** — user
      gave the exact account-menu before/after and said to apply the same
      boxed style everywhere. Old:
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
- [ ] **Split victim's gold among the group on kill** — "To the victor go
      the spoils!" User: "also upon death get all gold from the victim and
      split it between all group members if groupped." Blocked on/pairs
      naturally with the not-yet-built group/party system (see "Bigger
      systems" below) for the "if grouped" split; solo case is simple.
- [ ] **Meaningful limb damage** — a decapitated limb currently still
      shows ~100% in places; individual limb hits should visibly matter.
      User: "make limb damage mean something. if you have a limb
      decapitated it shouldnt be at 100% limb health. make individual limb
      hits actually hurt." (Related bug already fixed today in this same
      session: [[player_repo.c]]'s `player_load()` was resetting every
      reconnecting character's limbs to level-1-sized fractions regardless
      of real max_hp -- fixed by calling `being_limbs_full_heal()` after
      `player_progress_load()`. This TODO item is the broader "limb % is
      informative and hits feel weighty" pass, not just that bug.)
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
- [ ] **Expand `prompt` toggles** — add mana, piety, vitality, gold, etc
      to the existing `prompt` command's toggle set. User: "expand prompt
      command toggles to include mana, piety, vitality, gold, etc."
      BLOCKED on those stats existing at all first: `being_t`/`progress_t`
      currently has no mana pool, piety stat, or vitality stat (prayer/
      casting is component-consumption-based, task 44, not mana-based),
      and no gold/currency field (task 29, "Money system", still
      pending). `prompt hp` (the one toggle that exists today,
      `cmd_prompt.c`/`game_loop.c`) is a clean, small template to extend
      once each underlying stat is real -- but adding the toggles before
      the stats themselves would just be dead bitmask flags.
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
- [ ] **Thief "peek" skill (attempt to see someone's carried inventory)**
      — user (same message as the look-equipment request above): "a
      thief skill could be added to attempt a peak at the targets
      inventory." Distinct from the (now-done) worn-equipment display
      above -- this is a new skill/command that tries to see what
      someone is CARRYING (not worn), with some chance of success/
      detection, gated by `being_knows_skill()` same as trap mechanics.
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
- [ ] **Alphabetize each `cmd_table.c` tier block — GO-AHEAD GIVEN
      2026-07-13, no further confirmation needed.** User: "sort by
      alphabet first then level lowest to highest" ... "leave important
      commands at the top." The stall last time was a real conflict:
      naive alphabetizing breaks the movement-must-be-first invariant
      (single-letter `n`/`e`/`s`/`w`/`u`/`d` abbreviations depend on
      movement sitting earliest in the table so nothing else can shadow
      them) and would also re-open the `set`/`settrap` and `get`/`goto`
      collisions this session just fixed by ordering. Resolve it as:
      keep movement (and any other pair the table already documents as
      deliberately non-alphabetical, e.g. `set` before `settrap`, `get`
      before `goto`, `wiznews` before `wiznet`) pinned at the top of its
      tier as documented exceptions, then alphabetize everything else
      within each of the two tiers (mortal, then immortal) around them.
      After editing: rebuild, deploy, re-run
      `smoke_test_immortal_cmds.py` (has the `g`→get/`s`→settrap-style
      abbreviation checks) plus a couple of movement smoke tests before
      calling it done — this exact class of collision bit the table
      reorder twice already this session, don't skip verification here.
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
- [ ] **Practice system redesign — GO-AHEAD GIVEN 2026-07-13, design is
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
      6. The 6 new per-class Combat guildmaster mobs/rooms — **check in
         with the user before creating this world content**, per the
         explicit commitment already made; how Basic/Advanced mobs get
         identified by role (existing level-51/level-100 mobs, by
         mob.level threshold or a keyword) also needs a decision before
         `cmd_practice.c`/`cmd_goto.c` can match them reliably.
      7. Help topics (`practice`, `goto`, `skills`, `balance`) + wiznews
         entry once shipped, plus new/extended smoke tests covering
         practice-point earning/spending, the three-discipline gate, the
         three `goto` forms, and `balance wisdom`.
- [ ] **Message boards + related commands** — port from Sneezy. User:
      "implement message boards and related commands from sneezy."

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
- [ ] **Mid-fight persistence** — HP and limb HP are only saved at defeat; a
      mid-fight disconnect reloads at last-saved values.
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
