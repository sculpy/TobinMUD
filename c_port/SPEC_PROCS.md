# Special-procedure port — tracking

User directive (2026-08-03): "lets port over all special procedures from
sneezy" / "and apply them to the mobs/objs they modify" / scope call
"everything, in original file order." This file tracks that project
across sessions since it's far too large for TODO.md or one sitting —
**325 distinct spec-proc functions** in the original engine
(`sneezymud-master/code/code/spec/*.cc`), Tobin has **3** so far (all
pre-dating this project, hand-written ad hoc: `SPEC_PROC_DOCTOR`,
`SPEC_PROC_LAMPLIGHTER`, `SPEC_PROC_NEWBIE_EQUIPPER` — see
`c_port/include/mob_ai.h`).

**Order = alphabetical `spec/` directory listing** (58 files) — the only
defensible "file order" since the original has no other canonical
build/registration sequence visible from outside the engine.

**Framework**: a real id → function dispatch table is being built in
`mob_ai.c`/`mob_ai.h` (mirroring the original's `mob_specials[]` +
`TMonster::checkSpec`), rather than continuing the old one-off
special-cased-`if` pattern — that pattern doesn't scale to 322 more.
Hook points wired in so far: pulse (`mob_ai_tick`), speech
(`cmd_say.c`'s existing dispatch). More hook points (combat-adjacent,
room-entry, `buy`/`list`) get added as procs that need them come up.

**Not every original proc is portable as-is** — many depend on
subsystems Tobin doesn't have yet (factions, disease, pets, a real
disc/discipline system, horse-finding/pathfinding, rumor files, a
commodity-trading system). Those get logged as **BLOCKED** with the
missing prerequisite named, not silently skipped or faked.

## Progress

- Framework (dispatch table + pulse hook): **built** (Session 124,
  `mob_spec_dispatch_pulse()` in mob_ai.c, id -> function switch keyed
  on `being_t.mob_spec_proc`, mirrors the original's `mob_specials[]`).
  A second hook, "given item/given coins" (`mob_ai_notify_given_item()`/
  `mob_ai_notify_given_coins()`, called from cmd_object.c's `give`), was
  added Session 127 for `SPEC_BEGGAR`. Movement/room-entry/buy-list hooks
  still get added as procs needing them come up.
- Files fully done: 0 / 58
- Functions ported: 3 / 325 (pre-existing: DOCTOR/LAMPLIGHTER/
  NEWBIE_EQUIPPER) + 4 new this project (`SPEC_CHICKEN` Session 124,
  `SPEC_BEGGAR` Session 127, `SPEC_REPLICANT` -- ported in the droplet's
  own parallel work, found already in mob_ai.c and only now reflected
  here, no session number recorded -- and `SPEC_THIEF` Session 128).
  Along the way, found and fixed a real live data bug: mob vnum 601 (the
  seeded beggar, `SPEC_BEGGAR`'s own test subject) had its `spec_proc`
  overwritten to 71 (`SPEC_REPLICANT`'s id) in the live DB, almost
  certainly from an earlier ad hoc verification of replicant that
  mutated a real seeded mob instead of a scratch one -- restored to 17,
  `smoke_test_specproc_beggar.py` re-confirmed passing.

## IMPORTANT correction #2 (found 2026-08-03, Session 127): real ids come
from the `mob_specials[]`/`objSpecials[]` ARRAY, not the sparse named-
constant list

Correction #1 below (still true) found that not every function is
independently assignable. This is a SEPARATE, equally important
correction in the other direction: `spec_mobs.h`'s `const int SPEC_X = N`
list only names the ids referenced BY NAME elsewhere in the C++ source
(e.g. by another spec-proc calling one directly) -- it is NOT the
complete list of real, assignable ids. The actual source of truth is the
`mob_specials[NUM_MOB_SPECIALS + 1] = { ... }` array itself (spec_mobs.cc,
~7206), where **array position IS the id**. Confirmed by finding `thief`,
`beggar`, `dagger_thrower`, `hobbitEmissary`, `replicant`, `TicketGuy`,
and others at real array positions despite having NO named constant in
spec_mobs.h -- several of these were WRONGLY marked `[-]` (ambient/dead)
earlier in this same file before this correction. `objSpecials[]`
(spec_objs.cc, ~7630) has the same shape for object procs -- 164 real
entries vs. only 10 named constants in spec_objs.h.
**Practical impact**: before marking any proc `[-]` (no real id) going
forward, check its actual array POSITION in `mob_specials[]`/
`objSpecials[]`, not just whether spec_mobs.h/spec_objs.h names it. The
`[-]` calls made in the checklist below under correction #1, before this
array was found, need re-verification against the array -- flagged where
already caught (`beggar` re-verified `[x]`, done), not yet swept for the
rest.

## IMPORTANT correction #1 (found 2026-08-03, Session 127): not every function
in a `spec/*.cc` file is an independently-assignable spec-proc

`spec_mobs.h` has the authoritative, complete list of real `SPEC_*` ids
(`NUM_MOB_SPECIALS = 222`, ids up to 222, NOT all used/contiguous) — only a
function with a matching `const int SPEC_WHATEVER = N;` entry there can
actually be assigned to a mob via `mob.spec_proc` and dispatched through
`checkSpec()`/`mob_specials[]`. Several functions physically living in
`spec_mobs.cc` (and presumably other `spec_*.cc` files too — unverified
yet) have NO matching id: `insulter`, `librarian`, `siren`, `thief`,
`Summoner`, `replicant`, `StatTeller`, `throwChar` (x2), `ThrowerMob`,
`Tyrannosaurus_swallower`, `frostGiant`, `dagger_thrower`, `beggar`,
`TicketGuy`, `hobbitEmissary`, `banshee`, `corpseMuncher`,
`grimhavenHooker` among others found so far. These are either (a) ambient
AI-reaction helpers called unconditionally for many/all monsters from
elsewhere in the AI code (same shape as Tobin's own
`mob_try_align_flavor()`/`mob_try_wander()`, which aren't spec-gated
either), or (b) genuinely dead/unwired code from an earlier version of
the engine. **The original 325-function count for this project was a
raw grep of function definitions across `spec/*.cc` and is therefore an
OVER-count of true assignable spec-procs** — the real "port one function,
wire one `SPEC_*` id" unit of work only applies to functions with a real
header entry. Functions without one get `[-]` (ambient helper / dead code,
not an assignable spec-proc) rather than `[ ]`/`[B]` in the checklists
below. This needs re-auditing file by file as each is actually worked, not
retroactively for all 58 right now.

**New blocker class found**: several real, ID-having procs
(`payToll`/`SPEC_TOLL_TAKER=79`, `horse`/`SPEC_HORSE=16`) need hook points
Tobin has NONE of yet — a pre-movement block hook (intercept/refuse a
directional move before it executes, not just react after) and a
per-combat-round mob "extra action" hook (`CMD_MOB_COMBAT`, a mob's own
special move during its own fight, not just the passive wander/scavenge/
aggress the existing pulse hook covers). Both are real, disclosed gaps,
not shortcuts to route around — log as `[B]` with the missing hook named
when hit, same as the missing-subsystem blockers (faction/disease/pet/
pathfinding/commodity) already documented below.

## File-by-file checklist (alphabetical, `spec/` directory order)

Legend: `[ ]` not started · `[~]` in progress · `[x]` done · `[B]` blocked
(reason noted) · `[-]` deliberately skipped (dead/unregistered in the
original, or a helper function, not a real registered spec-proc)

- [~] `spec_mobs.cc` (63 funcs total, but see the correction above — only
      functions with a real `spec_mobs.h` id are actually portable
      spec-procs; most of the 63 are ambient helpers, `[-]`).
      Real-id functions found and their status:
      - `SPEC_CHICKEN=8` (`chicken`) — `[x]` done, Session 124.
      - `SPEC_TORMENTOR=6` (`tormentor`) — `[B]` blocked: the real body
        does nothing observable except return TRUE/FALSE gating whether a
        non-immortal PC's command is suppressed; `checkSpec()`'s own
        comment reveals its actual purpose is a punishment-room mechanic
        (it's the ONE spec still active while a mob sits in Sneezy's
        `Room::HELL`, every other spec is skipped there) — Tobin has no
        equivalent prison/punishment-room concept to hang this on.
      - `SPEC_TOLL_TAKER=79` (`payToll`) — `[B]` blocked: needs a
        pre-movement block hook (see above) AND is hardcoded to specific
        Sneezy room vnums (1024, 36060) with quest-bit/item-vnum checks
        tied to that world's zone data, unverified whether/how those map
        onto Tobin's own imported rooms.
      - `SPEC_HORSE=16` (`horse`) — `[B]` blocked: needs the
        `CMD_MOB_COMBAT` per-round mob-action hook (see above) plus a
        "poop" condition mechanic Tobin doesn't have (droppable, but a
        clearly skippable flavor detail once the hook exists).
      - `SPEC_CARAVAN=43` (`caravan`) — `[B]` blocked, faction system
        (`FactionInfo[].caravan_flags`, `dest_fact`).
      - `SPEC_LAMPBOY=96` (`lamp_lighter`) — `[x]` already covered, same
        id as Tobin's own pre-existing `SPEC_PROC_LAMPLIGHTER=96`.
      - `SPEC_SHARPENER=40` (`sharpener`) — `[B]` blocked: needs THREE
        hooks Tobin doesn't have (`CMD_WHISPER` dispatch to spec-procs,
        a "peaceful room" violence-interrupt hook, generic-created/
        generic-destroyed lifecycle hooks for a mob's own per-instance
        job state) plus shop-keeper infrastructure this specific proc
        assumes.
      - `SPEC_SCARED_KID=160` (`scaredKid`) — `[B]` blocked: needs a
        faster "quick pulse" hook (`CMD_GENERIC_QUICK_PULSE`, distinct
        from the existing ~60s `CMD_GENERIC_PULSE`) AND is hardcoded to
        one specific Sneezy jungle zone's room vnums (27472-27488) with
        a scripted per-room flee path — not reusable content even if the
        hook existed.
      - `SPEC_AGGRO_FOLLOWER=210` (`aggroFollower`) — `[B]`/likely `[-]`:
        hardcoded to FOUR specific real player account names from the
        live original Sneezy game ("trav"/"scout"/"laren"/"ekeron") plus
        specific zone-vnum exclusion ranges and a group/follower system
        Tobin's own group model doesn't fully match — not meaningfully
        portable as generic behavior.
      - `SPEC_BANK_GUARD=155` (`bankGuard`) — `[B]` blocked: needs a
        persistent hunt/hate state machine (`setHunting`/`addHated`,
        same missing primitive as the Four Horsemen family) AND is
        hardcoded to one specific bank zone's room vnums.
      - `SPEC_ARCHER=184` (`archer`, in its own `spec_mobs_archer.cc`
        file but checked here since it was the next real-id candidate
        found) — `[B]` blocked: needs a whole bow/arrow/quiver ranged-
        weapon subsystem Tobin doesn't have (confirmed absent already,
        Whittle profession's own scope-cut note), plus cross-room
        targeting through multiple exits and a hunt state machine.
      - `SPEC_ADVENTURER=168` (`adventurer`) — `[B]` blocked on a NEW
        finding: it attacks any non-grouped mob in its room (mob-vs-mob
        combat), but `combat_process_run()` (combat.c) only ever
        iterates `g_descriptors` (connected PLAYER connections) each
        round -- a mob has no descriptor, so two mobs' `fighting`
        pointers being set at each other would never actually resolve a
        single round. **Mob-vs-mob combat has no resolution loop in
        Tobin at all currently** -- a real, disclosed gap affecting any
        future proc that wants a mob to fight another mob, not just
        this one.
      - **Summary after checking every real-id candidate found in
        `spec_mobs.cc` so far**: every one is blocked on one of six
        missing subsystems/primitives -- faction (`factionFaery`/
        `rumorMonger`/`caravan`), disease (`cold_giver`/`frostbiter`/
        `leper`/`flu_giver`), pet (`petVeterinarian`/`pet_keeper`/
        `stable_man`), hunting/pathfinding (`death`/`war`/`famine`/
        `pestilence`/`bankGuard`), a punishment-room concept
        (`tormentor`), a pre-movement block hook (`payToll`), a
        per-round mob combat-action hook (`horse`), a whole ranged-
        weapon subsystem (`archer`), or mob-vs-mob combat resolution
        (`adventurer`) -- plus several hardcoded to specific Sneezy zone
        content unlikely to map onto Tobin's world (`scaredKid`,
        `aggroFollower`). **No further quick "chicken-style" wins are
        likely left in this file** without first building one of those
        subsystems -- a real scope decision for whoever picks this up
        next, not something to force through piecemeal.
      - Not yet checked against real ids: `SPEC_BOUNTY_HUNTER=11`,
        `SPEC_CITYGUARD=14`,
        `SPEC_PET_KEEPER=18`/`SPEC_STABLE_MAN=19` (pet system, likely
        `[B]`), `SPEC_HORSE_FAMINE/WAR/DEATH/PESTILENCE=20/21/44/45` (Four
        Horsemen family, pathfinding-blocked per the existing note below),
        `SPEC_DOCTOR=48` (Tobin already has its OWN pre-existing
        `SPEC_PROC_DOCTOR` — verify it's the same concept before treating
        as a gap), `SPEC_SHARPENER=40`, `SPEC_LAMPBOY=96` (Tobin already
        has its own pre-existing `SPEC_PROC_LAMPLIGHTER` — same
        already-covered check needed), `SPEC_FACTION_FAERY=103`/
        `SPEC_RUMORMONGER=114` (faction/rumor-blocked per below),
        `SPEC_ATTUNER=150`, `SPEC_FISHTRACKER=154` (fishing-blocked),
        `SPEC_BANK_GUARD=155`, `SPEC_SCARED_KID=160`,
        `SPEC_CORPORATE_ASSISTANT=161`, `SPEC_DIVMAN=163`,
        `SPEC_PLANTER=165`, `SPEC_BMARCHER=166`, `SPEC_ADVENTURER=168`,
        `SPEC_KAVOD_BARMAID=171`, `SPEC_COMMOD_MAKER=175` (commodity-
        blocked), `SPEC_HOLDEM_PLAYER=178`, `SPEC_POSTMAN=179`,
        `SPEC_FIREMAN=182`, `SPEC_ARCHER=184`, `SPEC_FLASK_PEDDLER=185`,
        `SPEC_LIMB_DISPO=186`, `SPEC_STAT_SURG=187`,
        `SPEC_LOAN_SHARK=189`, `SPEC_PROPERTY_CLERK=191`,
        `SPEC_BANKER=192`, `SPEC_TAXMAN=195`, `SPEC_SIGNMAKER=200`,
        `SPEC_LEPER_HUNTER=202` (disease-blocked), `SPEC_AUCTIONEER=203`,
        `SPEC_LOAN_MANAGER=204`, `SPEC_HERO_FAERIE=206`,
        `SPEC_AGGRO_FOLLOWER=210`, `SPEC_COMMOD_TRADER=216` (commodity-
        blocked), `SPEC_PET_VETERINARIAN=218` (pet-blocked).
      - `SPEC_BEGGAR=17` (`beggar`) — **`[x]` DONE, Session 127.**
        WRONGLY listed `[-]` below before correction #2 (top of this
        file) was found -- it has a real array position (17) in
        `mob_specials[]` despite no named constant in spec_mobs.h.
        Second proc ported under this project: `give`-triggered (new
        "given item"/"given coins" hook, cmd_object.c/mob_ai.c), reacts
        to a mob receiving an item (flat thanks) or coins (6 amount-
        tiered reactions, ported faithfully except two lines' crude
        wording toned down, a disclosed scope note). Real seeded mobs
        already carry `spec_proc=17` (vnum 601 "a beggar" among others).
        `tests/smoke_test_specproc_beggar.py` (8 checks, including
        confirming a mob with NO matching spec_proc stays silent) passes
        live.
      - Ambient helpers confirmed `[-]` (no real array position in
        `mob_specials[]`, called unconditionally elsewhere or otherwise
        not independently assignable -- re-verify each against the
        array per correction #2 if reused as evidence later, only
        `beggar` above has been re-checked so far): `move_thing_forshop`,
        `kick_mobs_from_shop`, `CallForGuard`, `petPriceL`, `shopWhisper`,
        `insulter`, `librarian`, `siren`, `Summoner`, `StatTeller`,
        `throwChar` (x2), `ThrowerMob`, `Tyrannosaurus_swallower`,
        `frostGiant`, `banshee`, `corpseMuncher`, `grimhavenHooker`.
        **Re-verified via the array and found to actually be real** (id
        in parens), previously wrongly `[-]`: `thief` (4) -- **`[x]`
        DONE, Session 128**, see below -- `dagger_thrower` (15),
        `hobbitEmissary` (36), `replicant` (71) -- **`[x]` DONE**, ported
        in the droplet's own parallel work (found already implemented in
        mob_ai.c as `mob_spec_replicant_pulse()`, no session number on
        record; on-pulse HP-below-max heal-and-spawn-a-copy, no blockers
        hit) -- `TicketGuy` (51) -- still unstarted `[ ]` (not ported,
        just no longer miscategorized), will need its own blocker/
        portability check same as `beggar`/`thief`/`replicant` got.
      - `SPEC_THIEF=4` (`thief`) — **`[x]` DONE, Session 128.** Fourth
        proc ported under this project. On pulse, an awake standing
        thief mob that isn't fighting has a 1-in-26 chance to silently
        pickpocket a random loose (not worn/held) item from a non-
        immortal, non-fighting PC in its room -- ported from upstream's
        own `rob_blind()` helper (`mob_ai.c`'s `mob_spec_thief_pulse()`).
        Genuinely blind, matching upstream: no message to anyone, on
        success or failure. Real seeded mobs already carry
        `spec_proc=4` (vnum 602 "thief" among others).
        `tests/smoke_test_specproc_thief.py` (10 checks, including
        confirming a worn item is never taken and an ordinary mob with
        no matching spec_proc never fires) passes live.
      - `factionFaery`/`rumorMonger` — `[B]` faction system, rumor data
        files. `findMyHorse`/`randomHunt` (`TMonster::` methods feeding
        the Four Horsemen family above) — `[B]` pathfinding.
        `petVeterinarian`/`pet_keeper`/`stable_man` — `[B]` pet system.
        `cold_giver`/`frostbiter`/`leper`/`flu_giver` — `[B]` disease
        system. `commodMaker`/`fishTracker` — `[B]` commodity trading /
        fishing. `bogus_mob_proc`/`specificDisc` — no matching
        `spec_mobs.h` id found either, `[-]` likely-dead code, same as
        the ambient-helper list above.
      - `newbieEquipper` (the ORIGINAL C++ implementation of what became
        Tobin's own `SPEC_PROC_NEWBIE_EQUIPPER=147`) — `[x]`, already
        done (pre-dates this project, see mob_ai.h).
- [ ] `spec_mobs_archer.cc`
- [ ] `spec_mobs_auctioneer.cc`
- [ ] `spec_mobs_banker.cc`
- [ ] `spec_mobs_bounty_hunter.cc`
- [ ] `spec_mobs_cityguard.cc`
- [ ] `spec_mobs_combat.cc` (26 funcs)
- [ ] `spec_mobs_commod_trader.cc` — likely blocked, commodity system
- [ ] `spec_mobs_coroner.cc`
- [ ] `spec_mobs_corporate_assistant.cc`
- [ ] `spec_mobs_customizers.cc`
- [ ] `spec_mobs_doppleganger.cc`
- [ ] `spec_mobs_dragon.cc` — likely unique boss, low reuse
- [ ] `spec_mobs_earthquake_digger.cc`
- [ ] `spec_mobs_fireman.cc`
- [ ] `spec_mobs_flaskPeddler.cc`
- [ ] `spec_mobs_goring.cc`
- [ ] `spec_mobs_herofaeries.cc`
- [ ] `spec_mobs_holdem_player.cc`
- [ ] `spec_mobs_janitors.cc`
- [ ] `spec_mobs_leper_hunter.cc` — likely blocked, disease system
- [ ] `spec_mobs_limbDispo.cc`
- [ ] `spec_mobs_loanshark.cc`
- [ ] `spec_mobs_loan_manager.cc`
- [ ] `spec_mobs_lottery_redeemer.cc`
- [ ] `spec_mobs_money_train.cc`
- [ ] `spec_mobs_paladin_patrol.cc`
- [ ] `spec_mobs_posse.cc`
- [ ] `spec_mobs_postman.cc`
- [ ] `spec_mobs_property_clerk.cc`
- [ ] `spec_mobs_signmaker.cc`
- [ ] `spec_mobs_statSurgeon.cc`
- [ ] `spec_mobs_tattoo_artist.cc`
- [ ] `spec_mobs_taxman.cc`
- [ ] `spec_mobs_tudy.cc`
- [ ] `spec_mobs_vehicle.cc`
- [ ] `spec_objs.cc` (99 funcs — largest file)
- [ ] `spec_objs_ballot_box.cc`
- [ ] `spec_objs_black_sun.cc` — likely unique item, low reuse
- [ ] `spec_objs_blind.cc`
- [ ] `spec_objs_casting.cc`
- [ ] `spec_objs_deikhan_sword.cc` — unique item
- [ ] `spec_objs_dk_sword.cc` — unique item
- [ ] `spec_objs_graffitiMaker.cc`
- [ ] `spec_objs_graffitiObj.cc`
- [ ] `spec_objs_lightning_rod.cc`
- [ ] `spec_objs_lottery_ticket.cc`
- [ ] `spec_objs_manadrain.cc`
- [ ] `spec_objs_of_many_potions.cc`
- [ ] `spec_objs_poison_cutlass.cc` — unique item
- [ ] `spec_objs_setcasts.cc`
- [ ] `spec_objs_sweeps.cc`
- [ ] `spec_objs_tequila_cutlass.cc` — unique item
- [ ] `spec_objs_thief_quest.cc`
- [ ] `spec_objs_unholy_cutlass.cc` — unique item
- [ ] `spec_objs_weapons.cc` (43 funcs)
- [ ] `spec_rooms.cc` (31 funcs — **no room-spec plumbing exists in
      Tobin yet**: no `spec` column on `room`, no dispatch hook. Needs a
      schema addition before any room proc in this file or
      `spec_rooms_sleeptag.cc` can be ported, not just individual
      function work.)
- [ ] `spec_rooms_sleeptag.cc`
- [ ] `spec_weapons_lightsaber.cc` — unique item

## Notes for whoever picks this up next

- Verify each proc against the real upstream source before porting —
  same house rule as everything else in this codebase (CLAUDE.md).
- A proc that's genuinely one-off/lore-specific (a single named boss or
  unique item) still gets ported if reachable in file order — the user's
  call was "everything," not "the reusable ones." Don't silently
  downgrade scope; if something looks skippable, ask or mark `[B]`
  with a clear reason instead of quietly dropping it.
- Update this file's checklist AND the "Progress" counts at the top
  every session that touches this project, so progress is visible
  without reading STATUS.md's full session log.
- STATUS.md still gets the normal per-session writeup for whatever
  actually lands each session — this file is the index/checklist, not
  a replacement for it.
