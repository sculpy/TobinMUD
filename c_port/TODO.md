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

## User batch 2026-08-08 — logged, working these now
- [x] Searchable help files on the website + nginx web root pointed at
      /home/mud/TobinMUD/web -- done 2026-08-08 (Session 140). The
      nginx `root` already pointed there, but the site was silently
      404ing for everyone -- see STATUS.md Session 140 for the real
      root cause (a `/home/mud` permission block, then an SELinux
      label block, both now fixed). New web/help.html (client-side
      search) + web/generate_help_json.sh (cron-refreshed JSON dump
      of help_topic).

- [x] Combat skills should improve by doing (skill-gain-through-use), not just training -- done 2026-08-08, expanded by the user mid-session to 'all skills/spells should be learn by doing, linked to use and player stats, use sneezymud as inspiration for all.' The 5 weapon/barehand proficiency skills (slash/blunt/pierce/barehand proficiency, all 6 classes) were the one real gap: they auto-tracked combat_disc_pct 1:1 (2026-08-03 fix) instead of gaining from actual use, because nothing called skill_learn_from_doing() on them (no weapon-type distinction existed to gate a per-swing roll). Fixed by reusing weapon_verb()'s existing slice/chop/bludgeon/stab/pierce/hit classification (combat.c, already used for hit-message flavor and the Warrior specialization bonus) to pick which proficiency skill a given swing exercises -- combat_strike() now calls skill_learn_from_doing() for the matching skill every round, for every class (PCs only). skill_proficiency()/skill_learn_from_doing() (skill.c) had their SKILL_TIER_COMBAT special-casing removed entirely -- they now go through the exact same headroom+Wisdom-scaled learn-by-doing path (skill_ceiling(), cooldown, gain-chance curve) as every other skill tier; combat_disc_pct is still this tier's ceiling (unchanged relationship), just no longer the live value. Every other skill/spell already routed through skill_learn_from_doing() (47 call sites across cmd_*.c/combat.c going into this session) -- this was the one un-grindable exception. 'ranged proficiency' remains a disclosed, separate gap: no throw/bow attack command exists at all yet (weapon_verb() has no ranged bucket), so there's nothing to hang a per-shot hook on until ranged combat itself is built -- that's a real 'implement ranged combat' feature, tracked separately, not a learn-by-doing wiring gap. tests/smoke_test_proficiency_autotrack.py rewritten (its 5 checks used to assert the OLD auto-track-with-no-combat-needed behavior, which is now gone by design) -- new 14-check version confirms: roster/lock unchanged, 0% at 100% discipline with zero real combat (no more auto-ceiling), a real sword swing raises ONLY slash proficiency to its 1% floor (other 3 stay 0, confirming weapon-type targeting), and a proficiency already at its discipline ceiling can't be pushed past it by further combat. All 14 checks pass live. Also installed cronie + enabled crond (were missing entirely on this droplet) and registered the pre-existing watchdog.sh in mud's crontab ('* * * * *') per user request -- the documented auto-restart-on-crash safety net was never actually wired up.
- [x] Client status bar is one tick behind actual game state -- done 2026-08-08. Root cause: regen_tick_run() (src/core/regen.c) called being_notify_vitals_changed() BEFORE applying that tick's own being_heal()/being_heal_vit()/being_heal_mana(), so every GMCP/MSDP Char.Vitals push carried the PREVIOUS tick's numbers -- client gauge bar was always one tick stale by construction, not a client-side rendering bug. Fixed by moving the notify call to the end of both the fighting and resting branches, after all heals for that tick are applied. Also fixed tests/smoke_test_regen.py's HP regex, which still expected the old 'HP: NN (NN Max' score format (score was reworked to 'HP: NN/NN' at some point) and didn't strip ANSI color codes -- pre-existing stale test, unrelated to this bug, fixed in passing.
- [x] Client music playback is sporadic; exit+relog sometimes fixes it -- done 2026-08-08 (client v0.4.24). Root cause: scan_msp_and_forward() (client/src/win32/main.c) only recognized a '!!MUSIC('/'!!SOUND(' MSP marker once all 8 prefix bytes had arrived in the same on_text() chunk -- if a marker landed right at a TCP packet boundary with fewer than 8 bytes in the tail (e.g. server flushed after '!!MUS'), the code fell through to its plain-text path and silently forwarded/dropped the fragment instead of buffering it for the next read, losing that music cue entirely. Purely a function of how the OS happened to chunk that particular send, hence sporadic and network-timing-dependent -- exit+relog just landed on different chunking by chance, not a real fix. Fixed by checking, when fewer than 8 bytes remain in the buffer, whether what IS there still matches a valid prefix of either marker string; if so it's held in sound_scan_buf (existing carry-over mechanism, same as the already-handled 'saw !!MUSIC( but no close paren yet' case) instead of falling through. Rebuilt, bumped to v0.4.24, MSI redeployed to /usr/share/nginx/html/tobinclient/ (version.txt updated) so the client auto-updater picks it up.
- [x] Examine sneezymud-master/lib/races — DONE 2026-08-10: race_stat_bonus() now derived from the Sneezy 12-stat table via a documented fold (docs/RACE_STATS.md), net-zero, +smoke_test_race_stats.py. hp/mana/immunity data left unimported (no PC perk system).
- [x] Examine sneezymud-master/lib/tipsfile — DONE 2026-08-10: imported 10 adapted, Tobin-verified tips into db/tobin/tip.sql (16 -> 26); Sneezy-world-specific/duplicate lines dropped.
- [x] Account menu: prompt for email address (privacy message: never shared, MUD-communications only), save to account DB -- done 2026-08-08. account.email already existed in the schema (inherited from Sneezy's own account.sql, unused until now) -- no migration needed. New CONN_GET_EMAIL connection state (descriptor.c) prompts right after the timezone step, for BOTH the timezone-set and timezone-skip branches (the second one was missed on the first pass -- blank at the timezone prompt used to jump straight to CONN_ACCOUNT_MENU, skipping the new email prompt entirely; caught live via smoke_test_email.py and fixed same session). Blank opts out silently (user follow-up, 2026-08-08: 'allow someone to opt out of providing email') -- account.email just stays empty, no nagging. A minimal '@' + '.'-after-the-'@' check rejects obvious typos with a retry; real validation is out of scope. New self-service `email [<address>|clear]` command (cmd_email.c, mortal-level) mirrors `time`/`color`'s own view-or-set shape for changing it later. New account_set_email()/account.email field (account_repo.c/account.h). tests/smoke_test_email.py (16 checks) passes live.
- [x] Containers: objects should stack and components merge (see Sneezy for inspiration) -- done 2026-08-08. `inventory` (cmd_object.c) and the room floor listing (group_room_items(), cmd_look.c) already grouped identical items into a single "(xN)" line; container contents were the one place still listing every item on its own line -- `look <container>`'s "It contains:" and the immortal-only carried-inventory view's one-level-nested container listing (both cmd_look.c). New shared render_grouped_contents() (cmd_look.c) closes both gaps with the exact same "identical rendered line -> one entry, count it" technique the other two spots already use -- no separate vnum-equality check needed, so it naturally covers "components merge" too (ephemeral same-label items, e.g. Planting's fruit/hide/meat, group the same way real prototype items do). Checked Sneezy's own obj_component.cc for inspiration first -- it's a much bigger, disclosed-separate system (TComponent::willMerge()/doMerge(), spell-material charges, decay-time weighted averaging, room/weather-gated spawn placement tables for ~2500 lines) tied to spell components specifically, not general item stacking; scoped this fix to the display-grouping gap that actually existed, same precedent the 2026-07-26 inventory-stacking work already set, not the full Sneezy subsystem -- logged as a real, much larger follow-up below if the user wants the actual spell-component system ported later. tests/smoke_test_container_stack.py (3 checks) passes live.
- [x] Fully implement egotrip from Sneezy -- done 2026-08-08, see the
      'Egotrip expansion + new `force` command' section below for
      details. **Follow-up done same day** (user: "bypass xp loss on an
      egotrip hit" / "make egotrip usable on mobs too"): new `damn
      <target>` instakill subcommand (zero XP cost, immortal-PC-winner
      rule already made this automatic); blast/damn/disease/crit now
      also reach a mob in the caller's own room, not just world-wide
      PCs -- see STATUS.md Session 139.
- [x] Implement trophy system from Sneezy -- done 2026-08-08, see the 'Trophy system' section below for details.
- [x] Implement missing skills from docs/Spell Assignments.xlsx; take
      everything usable from sneezy code/code/disc. **Batch A done
      2026-08-08** (cook/whittle skill-gated + learn-by-doing;
      new defense/praying/casting/swim skills -- see STATUS.md Session
      138). **Batch B done 2026-08-08** (bandage -- new bleeding-limb
      mechanic + treatment command; hiking -- reduces move cost -- see
      STATUS.md Session 141). **Batch C done 2026-08-10** -- the five
      real subsystem/design gaps Session 141 had disclosed as open are
      now all built (see STATUS.md Session 147): sharpen/smooth on a new
      mutable weapon sharpness stat; alcoholism on a new drunk stat fed
      by liquids.c's own real per-liquid drunk values, with upstream's
      real pass-out formula; the Deikhan mounted-combat trio (charge,
      calm mount, advanced riding), granted to every class since Tobin
      has no Deikhan class; the Ranger beast-charm pair, wired into the
      existing charmed-pet system (the "blocked on task 35" note was
      stale -- that system has been built for a while, and upstream's
      own versions of both skills are stubs anyway); and the five
      Shaman spells folded onto Druid, three of them carrying real new
      affect types with combat/movement hooks rather than flavor text.
      Also fixed a real production bug found while testing: an older
      generic self-ward branch matched the substring "shield" inside
      "shield of mists", making that spell dead code.
- [x] Implement what's useful from sneezy code/code/misc/ai_* -- reviewed 2026-08-08, superseded, no port needed. All 5 files (ai_commands.cc/ai_reactions.cc/ai_responses.cc, ~7700 lines combined) are Sneezy's mob-reacts-to-player-social system -- one hardcoded TMonster::ai<Verb>() method per emote (hug/dance/poke/applaud/laugh/...), plus a keyword-triggered mob dialogue-response dispatcher (ai_responses.cc). This is the exact same job Tobin's own `edit trigger` system already does, by a prior deliberate architecture choice (spec-procs/hardcoded reactions skipped in favor of data-driven triggers -- see the Feature Audit artifact's 'spec procs' note). No smaller portable subset exists -- it's one monolithic, already-superseded design, not a set of independent AI behaviors. ai_utility.cc's mobAI() (mood-stat simulation: susp/greed/anger/malice) and ai_assignskills.cc (skill-point allocation at mob creation) are the two non-social files, also not attempted -- both tied deeply to Sneezy's own TMonster mood fields Tobin's being_t has no equivalent of, and no user request exists for mob mood simulation specifically.
- [x] Implement a command separator in the game -- done 2026-08-08. `;` splits one typed line into several commands run in order ("north;look;inventory"), the classic Diku/Sneezy convention. Real conflict found before implementing: `;` at the start of a line was already a one-character shorthand for `wiznet` (cmd_table.c, same idea as `'` for say) -- asked the user, who chose to drop the wiznet shorthand outright ("use ';' anyway, drop the wiznet shorthand" / "if you need the ; wizards can use alias to substitute"). Split happens in descriptor.c's CONN_PLAYING dispatch point (via strtok_r on ';', each trimmed piece re-entering cmd_dispatch() fresh) -- so a leading `'` say-shorthand still works per-segment ("'hi;'bye" says "hi" then says "bye"), and if any chained segment ends the connection (e.g. `quit!` mid-line) the rest of the line is abandoned. No escaping for a literal `;` inside a message -- same accepted limitation every Diku-family MUD's separator has. help_topic `wiznet` rewritten (no longer claims the retired shorthand, points at the new `separator` topic); new `separator` help_topic added. tests/smoke_test_separator.py (8 checks) passes live.
- [x] Fix skills/spells that are stubbed out/no-ops — make them work exactly as coded in Sneezy. Done 2026-08-09 (Session 143) for the "Learn-by-doing roster audit" list below: pick lock, shoulder throw, set trap (container), retreat, counter steal, close quarters fighting, groundfighting, Oomlat Philosophy, power move, voplat, dufali, snofalte, knot all implemented; Garmul's tail (the skill) verified already working via the pre-existing spell branch. Disclosed-skipped (real subsystem gaps, not attempted): sling shot/stunning arrow (no ranged-weapon subsystem), two-handed specialization (no per-weapon two-handed data field), set trap (arrow/mine/grenade) (ammo-quiver/room-floor/thrown-explosive mechanics Tobin has no subsystem for). See STATUS.md Session 143 for the full breakdown. tests/smoke_test_missing_skills_task1.py (13 checks) passes live.

## Races + tips audits, 2026-08-08 (research only, no code changed)

- [x] Races audit: sneezymud-master/lib/races is 127 flat-text RACE_<NAME> files with 12 raw base attributes (STR BRA CON DEX AGI INT WIS FOC PER CHA KAR SPE, Human baseline 105) plus hpMod/moveMod/manaMod/IMMUNE_*/AFF_* flags -- different scale/shape than Tobin's own system (6-race hardcoded enum in being.c, race_stat_bonus() small +/-2..4 deltas off a zero-sum baseline, plus a DB-backed race_balance table (db/tobin/balance.sql) of hp_mult/dmg_mult/tohit_mod/ac_mod, currently all neutral). Not a literal import target, but confirmed: Tobin's existing delta SIGNS already match Sneezy's direction for all 6 shared races (Elf DEX/INT up CON down, Ogre STR up INT/CHA down, Gnome INT up STR/CON down, etc.). Concrete next step if picked up: (1) Dwarf's CON is Sneezy's single biggest per-race outlier (+50 over baseline) -- Tobin's current Dwarf CON bonus (+4) could go up to better match; (2) Sneezy's hpMod/moveMod/manaMod per race (Dwarf: hp+1/move-20/mana-1, Ogre: hp+4/move+40, Elf: move+25/mana+1, Gnome: move-35/mana+2, Hobbit: move+40) map directly into race_balance's currently-neutral columns as real starting values instead of 1.0/1.0/0/0 for everyone. IMMUNE/AFF per-race resistances (e.g. Dwarf poison/charm/sleep) aren't modeled in Tobin at all -- a possible future feature, not this task's scope.
- [x] Tipsfile audit: sneezymud-master/lib/tipsfile is 48 plain tips (2 comment lines), Sneezy-color-coded, vs Tobin's own 12 seeded rows in the tip table (tips_repo.c, plain prose, no color markup, no seed SQL file found -- rows look hand-inserted). Roughly 15-18 of the 48 are clean/directly usable after stripping color markup (equipment/inventory/limbs/toggle/consider/say -- all confirmed to exist as Tobin commands); ~20 need editing (mostly goto-a-sneezy-shop tips pointing at Sneezy-only NPCs/shops, plus Grimhaven->Tobin City, the already-established rename); ~8-10 are not applicable (SneezyMUD-branded Discord links, unverified help topics). Not yet imported -- next step if picked up is drafting the ~15-18 clean ones as new tip rows plus editing the ~20 goto-reference ones to real Tobin shop/NPC equivalents.

## Mailing list export + shutdown/copyover -now, 2026-08-08

- [x] Level-60 `mailinglist` command (cmd_mailinglist.c): exports every account with a non-empty (opted-in) email to logs/mailinglist_<timestamp>.txt, one address per line, for pasting into a real email client's BCC field or importing as a mailing list -- no SMTP in Tobin, export only, user's own words: "writes to a file on the server that can be used in an email client to send mass email." Depends on the email-opt-in feature directly above.
- [x] `shutdown` already had a settable-seconds timer and a level gate (SHUTDOWN_MIN_LEVEL=60, already >= the requested "59+") from an earlier 2026-07-17 session -- the only new work was a `-now` synonym for `shutdown 0` (cmd_shutdown.c), for a consistent word across both commands.
- [x] `copyover -now` (cmd_copyover.c): copyover always had a hardcoded, non-optional 5-second warning+sleep before rebooting in place -- `-now` skips straight to the recovery-file-write + exec, same everything-else. Used to deploy this very session's own combat.c/skill.c changes live with zero dropped connections.

## Copyover timed countdown + message polish, 2026-08-08 (follow-up)

- [x] `copyover <seconds>` (user follow-up, same day: "i didnt want a 5 second timer, i wanted a timer argument so we can copyover in 300 seconds or whatever time frame we want ... announcing every minute the countdown has changed until 5 seconds then a count every second"). The `-now` work above only added an immediate-execute path; copyover itself was still hardcoded to a flat 5-second sleep() with no seconds argument at all. Refactored into a real timed countdown: new `copyover.h`/`src/core/copyover_schedule.c` (new file, mirrors shutdown.c's existing pulse-driven design almost exactly) ticks via `pulse_register(10, copyover_pulse_tick)` (registered in main.c) so the game keeps running normally for the whole countdown -- only the final `copyover_execute()` moment itself (recovery-file write + exec, moved out of cmd_copyover.c's old inline body into its own callable function) is a blocking instant, same as before, just deferred to the countdown's actual zero point instead of always preceded by a hardcoded freeze. Same curated milestone list as shutdown.c (3600/1800/900/600/300/120/60/30/20/15/10/5/4/3/2/1 seconds) -- kept as its own copy rather than shared, same reasoning as the rest of this file's "small duplication over premature abstraction" precedent.
- [x] Both `shutdown`/`copyover`'s player-facing countdown messages now render duration as minutes+seconds ("5 minutes", "1 minute 30 seconds") instead of raw seconds (user: "not everyone will know what 300 second means") -- a `format_seconds()` helper (duplicated in shutdown.c and copyover_schedule.c) handles the rendering; `log_info()` calls stay in plain seconds throughout, unaffected (user: "in the messages only, we will know issuing the command").
- [x] `abort` accepted as a synonym for `cancel` on both `shutdown` and `copyover` (user: "did you add an abort argument to copyover/shutdown?" -- it only had `cancel` before).
- [x] Cancellation messages now name who cancelled it (user: "add a name to the message: Jesus has canceled a shutdown ... for both copyover and shutdown") -- `shutdown_cancel()`/`copyover_cancel()` both gained an `initiator` parameter; broadcasts "<name> has cancelled the scheduled shutdown/copyover." instead of the old nameless "The scheduled shutdown has been cancelled."
- [x] Milestone broadcast wording changed from "The MUD will copyover/shut down in..." to "TobinMUD will copyover/shut down in..." (user, matching the actual product name).
- Fixed in passing: cmd_copyover.c had picked up a redundant `log_info("Copyover scheduled by...")` call duplicating the one already inside `copyover_schedule()` itself (caught via doubled log lines during live testing) -- removed, matching cmd_shutdown.c's own single-log-site convention.
- New `tests/smoke_test_copyover_timer.py` (9 checks: long-countdown scheduling broadcasts immediately and renders minutes; the game stays responsive to ordinary commands while a long countdown is pending, not frozen; `cancel`/`abort` both work and name the canceller; a non-round duration renders "N minute(s) M second(s)"; a short countdown reaches its unattended 5-second milestone broadcast) passes live. One real-world lesson from getting there: an earlier manual repro got killed by an external `timeout` wrapper before its own cleanup `copyover abort` could run, leaving a real countdown live on the production server -- it fired unattended 90 seconds later and copyover'd cleanly with a real player connected throughout (exactly copyover's whole purpose), which is itself a strong live confirmation the feature works correctly end-to-end, but the lesson stands: never wrap a script that schedules a live countdown in an external kill-timer without a guaranteed cancel in its cleanup path.


## Sneezy spell-component system, disclosed follow-up from the container-stacking task (2026-08-08)

- [x] **DONE 2026-08-10.** Per-spell spell-component system ported (src/core/spell_component.c). The portable core landed: each material spell now needs its OWN reagent (obj.val2 binding, imported verbatim from Sneezy -- 198 bindings indexed at boot), same-vnum components MERGE (doMerge: charges cap 100, charge-weighted decay), and caster MOBS load reagents for spells castable at level+2 nested in their spellbag. Immortals (NOHASSLE) may cast with any component. The Sneezy component_placement world-spawn table was deliberately NOT ported (hundreds of Sneezy-vnum-specific room/weather rows). See STATUS.md Session 151 + smoke_test_spell_component_binding.py. ORIGINAL SCOPING NOTE follows:
- [ ] sneezymud-master/code/code/obj/obj_component.cc (~2500 lines) is a full material-spell-component subsystem: TComponent::willMerge()/doMerge() merge same-vnum components together (weighted-average decay time, summed cost/charges) up to a 100-charge cap; a large `component_placement` table spawns specific components (e.g. "cheval") into specific rooms under specific weather/time-of-day windows for later collection. Tobin has no equivalent at all -- not attempted as part of the 2026-08-08 "containers should stack" fix (that was a display-grouping gap, this is a whole missing gameplay system). Real follow-up if picked up: would need its own scoping pass (which spells actually need material components in Tobin's current spell roster, whether the room/weather spawn-table approach is wanted or a simpler always-available-for-purchase model fits Tobin better).

## Egotrip expansion + new `force` command, 2026-08-08

- [x] "Implement egotrip from Sneezy" -- egotrip previously only had `blast` (2026-07-12; the other 12 Sneezy subcommands were disclosed as skipped since their systems -- disease, garble, portal objects, mob hate/aggro, a numbered crit table -- didn't exist in Tobin at the time). A real disease system has landed since then, so this pass ports every subcommand that now maps onto something real: `disease <target> <disease>` (cold/dysentery/flu/pneumonia/leprosy/gangrene/plague/scurvy, reusing being_apply_affect()+AFFECT_DISEASE_*, same mechanism cmd_drink.c's puddle-disease roll already uses, just a deliberate curse duration -- 5x cmd_drink.c's own accidental-infection durations); `cleanse` (removes every disease + poison affect from every connected being, world-wide, via affect_is_disease()+being_remove_affect()); `stupidity` (AFFECT_STUPIDITY on every connected mortal, same penalty formula cmd_cast.c's own stupidity branch uses); `wander` (forces every eligible mob in the caller's room to attempt its wander move right now -- mob_try_wander() (mob_ai.c) gained a `force` parameter that skips its per-tick RNG gate but keeps every other legitimate one -- charm/sentinel/fighting/position -- exposed via a new public mob_ai_force_wander()); `crit <target>` (forces a random MINOR limb sever, reusing Tobin's own "limb hits 0% HP" crit mechanic (combat_egotrip_crit(), new public wrapper around the existing static combat_sever_limb()) instead of Sneezy's missing ~100-entry numbered crit-effect table -- deliberately MINOR limbs only, never a killing blow, since a major-limb sever normally routes through combat_defeat(), which has its own subtle preconditions tied to being called from within combat_process_run()'s own iteration; user confirmed this scope-down explicitly). Still disclosed as not ported, no reusable system exists: `deity` (no spec-proc global-pulse hook), `bless` (Sneezy's version is 17 different named-god flavor blessings, no generic "flat blessing" affect exists to reuse), `portal` (no persistent walk-through portal object type, only the instant one-shot `ethereal gate` cast), `hate` (Tobin's aggro is alignment-based, no per-mob targeted-hate list). `teleport` is likewise not duplicated here -- already fully covered by the separate, pre-existing `transfer <name> [vnum]` command. Updated the `egotrip` help_topic (both live and the seed file) to match; `egotrip`'s cmd_table.c description updated too. tests/smoke_test_egotrip.py (15 checks) passes live.
- [x] New `force <target> <command>` (user follow-up, same day: "need a force command thats 55+ that force <target> [command] mobs or players can be forced") -- classic Diku/Sneezy immortal command with no dedicated Sneezy source of its own to port, just the standard shape (new FORCE_MIN_LEVEL=55, cmd_internal.h). A PC target is found world-wide (same g_descriptors scan egotrip's blast/disease/crit already use) and the command runs on their OWN real descriptor -- exactly as if they'd typed it, so their own command feedback goes to their own screen, not the immortal's (verified live: forcing a target to `score` showed the score output on the TARGET's connection). A mob target is looked up in the caller's own room only (combat_find_room_target(), same helper kill/disarm already share) -- Tobin has no world-wide mob-by-name index, a disclosed narrower scope than Sneezy's own world-wide lookup. A mob has no real descriptor, so a throwaway one is heap-allocated (descriptor_t is ~150KB -- a 128KB page buffer alone -- so deliberately NOT stack-allocated), zeroed, pointed at the mob, dispatched through, and freed; fd=-1 makes any stray descriptor_send() a safe no-op. No per-command filtering needed: cmd_dispatch()'s own min_level gate already checks the ACTING being's level once forced, so a forced mob or lowbie player can't reach an immortal-only command through `force` any more than by typing it directly. Same true-rank immortal-vs-immortal guard cmd_kill.c's own instakill check uses (can't force an equal-or-higher-ranked immortal peer). New `force` help_topic (live + seed file). Covered by the same smoke_test_egotrip.py above.

## Trophy system, 2026-08-08

- [x] "Implement trophy system from Sneezy" -- ported SneezyMUD's TTrophy
      class (cmd/cmd_trophy.cc/.h): a per-player, per-mob-vnum decaying
      kill count that shrinks the XP a repeatedly-farmed mob is worth,
      nudging players toward variety instead of parking on one easy
      spawn. New `player_trophy` table (tobin_migrations.sql, one row per
      player per mob vnum) + trophy_repo.h/.c (skill_repo.c's own
      get/add-upsert pattern) + trophy.h/.c (the actual formula/logic) +
      new `trophy [name]` player command (cmd_trophy.c).
      trophy_exp_mod(vnum, count) is an exact port of Sneezy's own
      getExpModVal(): full reward for the first 8 kills (`free_kills`),
      then steps down 0.5/14 (`step_mod`/`num_steps`) per kill past that,
      floored at 0.3 (`min_mod`) -- normalized by the mob's `max_exist`
      (Tobin's analog of Sneezy's `numberLoad`) when that's a real
      positive cap, so a mob with many concurrent spawns decays more
      slowly per-kill than a unique one. Wired into combat.c in two
      places: combat_award_hit_xp() reads each recipient's OWN trophy
      count for the victim's vnum and multiplies their per-hit XP share
      by the modifier (before the existing <1-floor clamp), and
      combat_defeat() calls trophy_record_kill() once per real mob kill
      (gated on `!loser_is_pc` -- `slain` is only decapitation flavor
      text, NOT "did they actually die", both branches of that function
      are real kills, so the increment had to sit above that branch, not
      inside `if (slain)`). Decay is pulse-driven (trophy_pulse_tick(),
      main.c, ~60s, matching every other slow-tick system's cadence) --
      0.25 off every row, floor-at-zero guarded by the SQL itself
      (`where count > 0.25`). Real, disclosed simplification from
      Sneezy's own decay: ONE global `update player_trophy` query per
      tick regardless of who's online, instead of Sneezy's own
      procTrophyDecay walking character_list and issuing one query PER
      ONLINE PC -- simpler and cheaper on this box's memory-constrained
      MariaDB (same OOM history combat_award_hit_xp()'s own practice-
      point-save comment already notes), and the DB is the only
      persistent store here anyway (no in-memory TTrophy cache to keep
      in sync the way Sneezy's TBeing::trophy instance does).
      Two more disclosed scope-downs, both noted in trophy.h's own doc
      comment: no `trophy wipe()` (character deletion already cascades
      every player-keyed table the same way, nothing trophy-specific
      needed) and no zone-grouped kill-percentage browsing in the
      `trophy` command -- Sneezy's own doTrophy() walks every zone
      counting `doesLoad` mobs to print "you've killed N% of this zone",
      which needs a full mob-census-per-zone repo function Tobin doesn't
      have; this port keeps just the functionally important half, a flat
      per-mob XP-modifier listing, optionally filtered by name
      substring (`trophy rat`). New `trophy` help_topic (live + seed
      file). tests/smoke_test_trophy.py (15 checks, including a live
      relative-XP comparison proving the modifier is actually APPLIED to
      combat XP, not just displayed) passes live.

## Open follow-ups, logged 2026-08-05

- [x] **Client v0.4.23 + server: sound pack fully remapped after user's real 2026-08-07 upload landed in the wrong directory** -- done 2026-08-07. What actually happened: the user's new/reorganized 43-file sound set landed root-owned in `client/installer/windows/sounds/` (a build-staging copy, not the canonical source), while `client/sounds/` itself kept the OLD 24-file set until a later sync step deleted the old-named files there too (adventure1/atlasaudio/audiodollar-adventure/melodygodzilla/motivational/nastelborn/hit/hit2-4/slash/thief/thief2/spell_fireball all gone). Net result: the real, final file set (43 files, confirmed via `ls`) renamed/reorganized the whole taxonomy -- hit.wav-hit4.wav -> barehand1-4.wav (+ a genuinely new barehand5.wav), thief.wav/thief2.wav -> stab4.wav/stab5.wav (folded into a new shared weapon-type pool, +3 genuinely new stab1-3.wav), slash.wav -> slash8.wav (+5 new slash1/4-7.wav), spell_fireball.wav -> spell3.wav, the 6 old fight-music tracks -> music1-5.wav (one dropped, melodygodzilla had no surviving name) + 4 genuinely new tracks (music6-9.wav), and two entirely new categories: casting1-6.wav (spell-cast moment, not impact) and staff1.wav (blunt weapons). backstab.wav/cleric.wav/monk1-4.wav/spell.wav/spell2.wav kept their names unchanged.
  - **chown**'d the stray root-owned files back to mud:mud, merged them into `client/sounds/` (the canonical source), then deleted the now-empty `client/installer/windows/sounds/` staging copy per user request (it's regenerated automatically every build via `cp -r ../../sounds .`, no reason to keep two copies in the repo).
  - **`pick_hit_sound()` (combat.c) remapped**: Cleric/Monk/Mage keep class-based pools (cleric/monk1-4/spell+spell2+spell3); Thief's old dedicated class pool (thief/thief2/backstab) is REMOVED -- Thief now falls through to weapon-verb pools like Warrior, since thief.wav/thief2.wav were folded into the shared stab pool by the rename itself. New `stab` weapon-verb branch (dagger/knife, `weapon_verb()` already returns "stab" for these -- previously fell through to generic) using stab1-5.wav + backstab.wav (6 files); new `bludgeon` branch (mace/hammer/club/staff) using staff1.wav; slash pool expanded to 8 files; generic/barehand pool now 5 files.
  - **`combat_music_tick()`'s TRACKS[]** expanded from 6 to the full 9 music1-9.wav files.
  - **New: casting sound** -- `cmd_cast.c`'s `task_cast()` now plays a random casting1-6.wav at the moment of casting (same beat as the existing 3-line flavor text), distinct from the spell-IMPACT sound in pick_hit_sound().
  - **tobinmud.wxs regenerated**: the old Component/ComponentRef list named each sound file explicitly and would have failed to build outright once 12 referenced files no longer existed -- rebuilt programmatically from the current 43-file directory listing, reusing existing GUIDs for the 10 unchanged filenames and generating fresh ones for the 33 new/renamed files. Verified as well-formed XML before building, and the resulting MSI built clean.

- [x] **Client v0.4.22: real fix for "music continues after fight ends"** -- done 2026-08-07. Server-side code review (combat_defeat/combat_drown_pc/combat_fall_kill_pc/cmd_flee.c/combat_music_tick) found no bug, and scripted repro attempts failed -- both correctly predicted this was actually client-side. Confirmed live via the user's own tobinmud_debug.log: MCI was failing to open "tobinmusic" at all (`error 259: driver cannot recognize the specified command parameter`), so fight music was actually playing through the PlaySoundA(SND_LOOP) fallback (added in v0.4.13 for exactly this kind of MCI failure), not the real MCI device. The `!!MUSIC(Off)` handler only ever tried `mciSendStringA("close tobinmusic")` -- a complete no-op when there was never an MCI device open to close -- so the looping PlaySoundA fallback had nothing left to stop it. Fixed: the Off handler now also calls `PlaySoundA(NULL, NULL, 0)` unconditionally (harmless no-op when MCI succeeded and PlaySoundA was never used, actually silences it when it was). Residual risk, not yet addressed: the SAME log shows the hit-sound MCI open also failing (`error 296`) on this user's machine, meaning both channels are currently falling back to PlaySoundA there -- which means they CAN still interrupt each other again (the original pre-v0.4.13 bug), since PlaySoundA only supports one sound at a time system-wide. MCI itself appears to be broadly unreliable on this particular machine/audio-driver combination; a real fix would mean either debugging why MCI's waveaudio driver rejects these files specifically, or replacing MCI with a proper multi-stream backend (e.g. raw `waveOutOpen()` with separate device handles, or DirectSound/XAudio2) -- bigger scope, not attempted this pass.

- [x] **Client v0.4.21 + server: 5 of the 7-item client batch shipped** -- done 2026-08-06/07. Items 2, 3, 4, 6, 7, 8 (see the batch item above) are done:
  - **Click-to-focus (2):** `OutputSubclassProc` on the scrollback RichEdit -- lets its own click handling run first (so click-drag selection for copying still works), then hands focus back to the input box on WM_LBUTTONDOWN/UP.
  - **Gauge strip moved to the bottom (3):** WM_SIZE now lays out the HP/Mana/Move gauges just above the input line instead of between the menu and scrollback; output fills from the top.
  - **Color lost / toggle does nothing (4):** real root cause was my own v0.4.15 fix -- `EM_SETTEXTMODE(TM_PLAINTEXT)` silently restricts the whole RichEdit control to ONE uniform character format, killing per-line ANSI color runs (the toggle had nothing left to act on). Reverted TM_PLAINTEXT; the real \r\n-doubling fix now lives in `scan_msp_and_forward()` instead, collapsing \r\n to a bare \r (RichEdit's native paragraph separator) before the text ever reaches the control -- doesn't touch character formatting, so color keeps working. Client-side change only, server-side `color_enabled`/`toggle color`/`account.color_pref` reviewed and are unrelated, working as designed.
  - **Mob long-description blank lines (6):** NOT a client bug -- found via HEX() that 5097/5156 mob.description rows and 35 obj.long_desc rows had reversed line endings (`\n\r` instead of `\r\n`) from the original seed import; fixed with a one-time SQL REPLACE() data correction (separate TODO.md entry above has the full detail).
  - **Status bar per-tick (7):** `regen_tick_run()` (regen.c) now calls `being_notify_vitals_changed()` once per tick for every connected PC, before healing -- previously the gauge only ever updated when combat.c pushed it on actual damage taken, so it sat frozen while resting/regenerating outside a fight.
  - **Sound/music rotation (8):** per-class hit-sound randomization (`pick_hit_sound()`) already existed from an earlier pass -- only the "never repeat the same music track twice in a row" half was missing; added `descriptor_t.last_music_track` (re-rolls until it differs from the last pick, skipped on the very first fight via `-1` init).
  - **Found/fixed along the way:** the sound files for cleric/monk1-4/backstab/slash2-3/spell/spell2 were stuck root-owned in `client/installer/windows/sounds/` from an earlier root WinSCP upload -- every client build since v0.4.13 silently failed to refresh those specific files (wixl doesn't error on a stale file, it just packages whatever bytes are already there). `chown`'d back to `mud:mud`, re-copied, verified byte-for-byte against source before repackaging v0.4.21.
  - **Still open from this batch:** item 1 (in-client trigger/alias GUI editors, currently plain-text-file + Reload Triggers only) and item 5 (music continuing after a fight ends -- reviewed `combat_defeat()`/`combat_drown_pc()`/`combat_fall_kill_pc()`/`cmd_flee.c`, all correctly clear `fighting = NULL` for both sides, and `combat_music_tick()`'s on/off logic is sound; could not reproduce live via scripted multi-round fights either -- no bug found yet, needs real-world repro detail if it persists after this batch's other client fixes).

- [x] **Real data bug found: mob/object descriptions had REVERSED line endings (`\n\r` instead of `\r\n`)** -- done 2026-08-06, user: "look at mob long description inserts blank lines in output". Not a client parsing bug -- confirmed via HEX() that 5097/5156 mob.description rows (98.9%) and 35 obj.long_desc rows had `\n\r` (LF-then-CR) sequences instead of the standard `\r\n`, almost certainly from a reversed byte order during the original seed-data import; room.description (0 affected) and most of obj was clean, so this was scoped to mob/object text specifically. A bare `\n` followed by a `\r` renders as two separate line-break-ish things in a row to a RichEdit-based client (the client's own CRLF-collapse pass, added the same session for the line-doubling bug, only recognizes `\r\n` in the CORRECT order) -- exactly the reported extra blank line. Fixed with a one-time SQL data correction: `REPLACE(col, CONCAT(CHAR(10),CHAR(13)), CONCAT(CHAR(13),CHAR(10)))` against mob.description, obj.long_desc, and obj.action_desc (2 rows). Verified 0 remaining reversed pairs across all three afterward. Not yet swept: other text-bearing columns (player.description, social/skill_help tables) -- those specific table/column names errored out during triage and weren't chased further; worth a quick follow-up sweep if a similar blank-line report comes in from a different screen.

- [x] **Client batch, logged 2026-08-06 (user, end of session):** 8 items to work through (header originally undercounted as "7"). Items 2-8 done -- see the "Open follow-ups, logged 2026-08-05" entries above (v0.4.21/v0.4.22/v0.4.23: click-to-focus, gauge-strip move, color/toggle fix, mob-description blank lines -- real cause was reversed DB line endings, not the client, per-tick status bar, sound pack remap, and the real music-after-fight fix). All 8 items are now done:
  1. Trigger and alias editors in the client -- **done 2026-08-10, client v0.4.25.** File > Edit Triggers... / Edit Aliases... each open a real editor window (the same hand-built-window pattern Preferences already used) with a grid of the current entries plus Add / Update Selected / Delete Selected / Save. Saving writes back in the exact same tab-delimited format, so the files stay hand-editable, and reloads the live tables -- the existing Reload menu items are kept for the hand-edit workflow. Editing works against a private staging copy rather than the live tables, since the socket-poll timer keeps matching lines behind the open window. Scope-down: comment lines round-trip only when they sit above the first real entry.

- [x] **Client v0.4.20: real fix for "title bar still says T"** -- done 2026-08-06, user provided a screenshot confirming it was literal text "T", not the icon, and had already ruled out a stale window (confirmed exiting fully via File menu each time). Real cause: both WndProcs (main window and Preferences window) fell through to the plain `DefWindowProc()` for unhandled messages, which resolves to the ANSI `DefWindowProcA` since neither UNICODE nor _UNICODE is defined anywhere in this build -- but both windows are registered via `RegisterClassW` (genuine Unicode windows). CreateWindowW sets the initial caption via an internal WM_SETTEXT carrying a wide-string pointer; letting that fall through to the ANSI DefWindowProcA misreads it byte-by-byte -- 'T' is 0x54,0x00 in UTF-16LE, and that trailing zero byte reads as an immediate ANSI string terminator, so only the first character ever survives. Classic, well-documented Win32 Unicode/ANSI window mismatch. Fix: both `DefWindowProc(...)` calls changed to `DefWindowProcW(...)`. Confirmed via screenshot-driven diagnosis, not guesswork -- prior theories (stale window, icon-vs-text confusion) were both explicitly ruled out by the user before this was found.

- [x] **Client v0.4.16-0.4.19: box-drawing alignment / font face saga** -- done 2026-08-06, user: "the right side of the box is misaligned because of font face choice" (Consolas's box-drawing glyphs weren't guaranteed the same advance width as its regular glyphs on every system). v0.4.16 switched to Lucida Console (real fixed-width box-drawing, ships with Windows) -- this WORKED. User then explicitly requested "CHANGE THE FONT TO BITSTREAM VERA SANS MONO" (v0.4.17); flagged up front that it doesn't ship with Windows, confirmed via InstalledFontCollection it wasn't present on the test machine. Rather than leave it broken, bundled the real font (Fedora's bitstream-vera-sans-mono-fonts package) into fonts\ next to the exe (same pattern as sounds\) and load it privately at startup via AddFontResourceExW(..., FR_PRIVATE, ...) -- no admin rights, no per-user registry/relogin timing, works every launch (v0.4.18, new wxs Directory/Component + Feature ref). User then reported "account menu is the same" (still misaligned) even with the font confirmed loaded (debug log: "bundled font loaded (FR_PRIVATE)"). Verified the server's own ASCII art is byte-perfect (every account-menu line is exactly 19 chars between borders, matching the border width exactly) -- ruled out a server-side content bug entirely. Real cause: confirmed via fontTools that VeraMono.ttf has only 256 glyphs total (basic Latin/Latin-1) -- ZERO coverage of the U+2500 box-drawing block TobinMUD's menus/banners use throughout, forcing Windows to silently substitute a different fallback font for just those characters, which doesn't share Vera's cell width. Switched to DejaVu Sans Mono (v0.4.19) -- Vera's actively-maintained, visually-near-identical descendant, confirmed via the same fontTools check to have full coverage (3300+ glyphs, real U+2550/2551/2554/2557/255A/255D glyphs) -- bundled via `dejavu-sans-mono-fonts` the same way. Also fixed along the way (v0.4.15, this same investigation): the REAL underlying "extra half line of space...for every line" bug -- an earlier v0.4.14 pass wrongly diagnosed it as Msftedit.dll's default paragraph spacing (PARAFORMAT2 dySpaceAfter) and saw zero improvement when tested; the actual cause was the RichEdit output control never being put into TM_PLAINTEXT mode, so it used Msftedit's RTF/TM_RICHTEXT paragraph model, which renders an incoming `\r\n` as a paragraph end PLUS a separate line break inside the new paragraph -- doubling every single line, since every server line ends with `\r\n`. Fixed with `EM_SETTEXTMODE, TM_PLAINTEXT | TM_MULTICODEPAGE` right after control creation (must precede any text, per MSDN). The v0.4.14 PARAFORMAT2 zeroing was re-added in v0.4.15 once the real cause was fixed and could actually be judged -- it does contribute a smaller residual improvement on top of the TM_PLAINTEXT fix, not on its own. Also separately fixed (v0.4.13, same investigation): hit sound stopping combat music, root-caused to PlaySoundA() and an MCI-opened wave device both reaching for the same default wave-out resource -- gave the hit sound its own MCI alias ("tobinhit") alongside music's ("tobinmusic") instead of PlaySoundA. Also under investigation, not yet resolved: user reports "the windows title bar still says T" -- CLIENT_TITLE_BASE / set_status_title() logic reviewed and cannot produce literal "T" from any code path found; user confirmed they fully exit via File menu each time (ruling out the "stale window" theory that explained every other report tonight); waiting on a screenshot to actually see what's rendering before guessing further.

- [x] **Client v0.4.15: real fix for the extra-line-spacing bug (v0.4.14 was a wrong diagnosis)** -- done 2026-08-06. v0.4.14 shipped a PARAFORMAT2 dySpaceAfter=0 change on the theory that Msftedit.dll's default paragraph style was adding space after every line; user confirmed via debug log they'd actually updated to 0.4.14 (and tried 10pt vs 16pt) and got "same" -- no change at all, which is exactly what you'd expect if paragraph spacing was never the real cause. Real root cause: the RichEdit output control was never put into TM_PLAINTEXT mode, so it defaults to Msftedit.dll's RTF/TM_RICHTEXT paragraph model -- that model does NOT treat an incoming `\r\n` pair as one line break the way a plain Edit control does; it renders `\r` as a paragraph end and the trailing `\n` as an ADDITIONAL break inside the new paragraph, visually doubling every single line server output ends with (every line does, since it's all `\r\n`-terminated). Fix: added `EM_SETTEXTMODE, TM_PLAINTEXT | TM_MULTICODEPAGE` immediately after CreateWindowExW for the output control (must happen before ANY text is set, per MSDN -- placed before EM_SETBKGNDCOLOR). Character-level color formatting (per-ANSI-run SCF_SELECTION calls in append_output()) is unaffected -- TM_PLAINTEXT only changes paragraph/line-break handling, not font/color runs. The v0.4.14 PARAFORMAT2 change was reverted (harmless but inert once the real cause is fixed, no reason to keep dead code). Version bumped 0.4.14->0.4.15, clean zero-warning rebuild, repackaged and published.

- [x] **Client v0.4.14: extra half-line spacing between every output line** -- done 2026-08-06, user: "line spacing is wrong" -> "theres like an extra half line of space in the box display" -> "for every line" (running at font size 16, "so i can read"). Not a font-metric issue -- Msftedit.dll (real RichEdit control, not the old RICHED32) defaults new paragraphs to its own "Normal style" spacing, adding visible space AFTER every paragraph on top of the font's natural line height; since every incoming MUD line ends its own paragraph (\r\n), that showed up as extra gap on every single line, scaling with font size (worse at 16pt than the 10pt default). Fix: apply_font() (client/src/win32/main.c) now also sets a PARAFORMAT2 with dySpaceBefore=0/dySpaceAfter=0/bLineSpacingRule=0 (single) via EM_SETPARAFORMAT, applied over the full existing text the same way CHARFORMAT2 already was -- reads like a real terminal again instead of a word processor. Version bumped 0.4.13->0.4.14, clean zero-warning rebuild, repackaged and published same as 0.4.13.

- [x] **Client v0.4.13: hit sound no longer stops combat music** -- done 2026-08-06, user: "playing music and sounds at the same time is flaky" -> "music plays, but as soon as a hit occurs the music stops" -> "i want the hits to play over the music". Root-caused via the client's own tobinmud_debug.log (%TEMP%\tobinmud_debug.log): no "MCI failed" line ever appeared, ruling out the v0.4.5-era MCI-failure fallback (which drops music back to a PlaySoundA loop, recreating the exact symptom). Real cause: the v0.4.5 fix put MUSIC on its own MCI device alias ("tobinmusic") but left the one-shot hit sound on PlaySoundA -- Windows documents that PlaySound() can preempt/stop audio already playing through an open MCI wave device, so despite looking like two independent APIs, they still contend for the same default wave-out resource. Fix: play_msp() (client/src/win32/main.c) now opens the hit sound through its OWN MCI alias ("tobinhit") instead of PlaySoundA, so both channels go through the mechanism already proven not to fail on this machine -- PlaySoundA is kept only as a last-resort fallback if MCI itself refuses to open the hit file, logging the real MCI error code same as the music path. Version bumped 0.4.12->0.4.13 (main.c + tobinmud.wxs), cross-compiled clean (zero warnings) via mingw64, repackaged as MSI, published to nginx (version.txt bumped) so the client's existing self-updater picks it up on next launch.

- [x] **Next spell/skill ports -- DONE 2026-08-10.** User's curated list was defined/completed in an Excel sheet (their words: "i defined the list in an excel sheet which are done"); this curation gate is now closed. Original text follows for history: **waiting on user's hand-curated list from the roster artifact** -- user, 2026-08-06: "i'll go through the roster and outline exactly what skills/spells need to be ported and for whom." Two living Claude Artifacts track port/feature status against real SneezyMUD (both owned by the user, updatable in-place from any session via the `Artifact` tool's `url` param -- never create a fresh one for these, or it forks a new URL): **Feature Audit** -- https://claude.ai/code/artifact/6ac41b28-f8e6-4c70-8124-d1d0073395b1 -- 55 player-facing systems marked Done/Partial/Missing/N/A, updated 2026-08-06 after an 11-row correction pass (bash/kick/disarm, magic items, sign language, territory, mount, transformation, crafting, pet/charm, drug tracking, OLC, and monster AI were all already Done but still showed Missing/Partial from a stale snapshot). **Spell & Skill Roster** -- https://claude.ai/code/artifact/2f96f0e8-4185-4890-9aa8-faa16dd43e0d -- all 471 real SneezyMUD spell/skill entries from spell_info.cc's discArray[], each with a `have` flag against Tobin's skill.c; updated 2026-08-06 after fixing 41 false negatives caused by naming drift (Tobin's one generic `"repair"` vs Sneezy's per-class names, apostrophes/plurals, outright renames like Slit->throatslit) -- the flag is a heuristic, verify against real skill.c before trusting an old read of either page. Do not port roster items ahead of the user's list on your own initiative -- this is a deliberate curation step they reserved for themselves, not delegated work. When the list arrives (likely pasted here or appended below), port each entry the same way this session's mana/HP/vitality work was done: pull the real formula/constants from sneezymud-master's own source on the droplet rather than inventing behavior. Trait system and Faction system are NOT part of this list regardless of what the roster shows -- user has explicitly rejected building either (see the 2026-08-06 entry above).

- [x] **Reconcile the feature-backlog list against actual codebase state (most were already done)** -- closed 2026-08-10 (bookkeeping only, no code was ever required; the two "remaining gaps" it names are both rejected outright, see the end of this entry). -- user, 2026-08-06, pasted a 15-item feature list from the Sneezy->Tobin Feature Audit artifact ("Skill-based combat (bash, kick, disarm, parry) / Offensive spell system / Magic items (scrolls, wands, staves) / OOC channels (commune, etc.) / Sign language / cross-race comprehension / Death processing (XP loss, resurrection) / Builder tools (OLC) / Mount / riding system / Monster AI & behavior / Transformation (polymorph/disguise/shapeshift) / Crafting & extraction (skin/butcher/brew/repair) / Pet / charm (followers) / Quest system / Drug tracking (addiction/withdrawal) / Special procedures (spec procs)") and noted "some have already been done". A full re-verification pass (reading actual source, not trusting file existence) found the audit was stale on 11 of its 17 flagged rows -- see the Feature Audit artifact's 2026-08-06 changelog for full detail. Real remaining gaps from this list: only the **Trait system** (advantages/disadvantages -- no data structure or command exists anywhere, just creation-screen flavor text) and **Faction system** (clans/caravans -- explicitly declared out of scope in cmd_table.c's own comments). Everything else on the pasted list is either fully Done (bash/kick/disarm, magic items via `use`, sign language, mount/riding, transformation/polymorph, crafting & extraction, pet/charm, drug tracking, and now builder tools/OLC and monster AI too) or a confirmed-still-real Partial (offensive spell breadth, OOC channels, death processing/resurrection, quest system content) -- not a gap someone forgot to build. Spec procs are intentionally not ported at all (Tobin chose the in-game `edit trigger` system instead, one real spec_proc was ported directly into mob_ai.c). No code changes needed for this item itself -- it's a bookkeeping correction. Trait system and Faction system: user, 2026-08-06, "we dont want traits or factions" -- explicitly rejected, not just low-priority `Leave`. Do not build either without the user reversing this decision by name.

- [x] **HP/Vitality formula ported exactly from real SneezyMUD** -- done 2026-08-06, user: "level 3 and i have 28 max hit" / "too low" / "hp formula for new players needs balancing", then explicit numbers ("Warrior 8.5, Cleric/Thief 5.6 (8.0*35/50), Mage/Druid/Monk 5.25 (7.5*35/50)") and "i want it exactly like sneezy" / "mana calculations vitality calculations, all exactly like sneezy" / "sneezy had good balance, no sense reinventing the wheel". class_hp_per_level() now uses the real per-class rates above (Druid has no real-Sneezy analog -- it's Tobin's own Ranger/Shaman-lineage stand-in, see mob_class_mask_to_tobin() -- grouped with Mage/Monk's 5.25). being_calc_max_hp() is now a direct port of real TPerson::hitLimit(): (baseHp()=21 + ageHpMod()=16 [both constants -- real aging is disabled upstream too, pinned to a constant "35 year old" result] + classHpPerLevel()*level) * getConHpModifier(). New plot_value() in being.c is a direct port of real plotValue<T,V>() (misc/extern.h) -- the actual two-branch power-curve (power=1.4) upstream uses to turn CON into a smooth 0.8..1.25 multiplier, replacing the old flat linear con_bonus approximation. Tobin's own `balance` command multiplier is folded into the class-level term, on top of the real formula, not replacing it. being_calc_max_vit() is now a direct port of real getMaxMove()/moveLimit(): 100 + 15 + level + plotStat(CON,3,18,13) [+ Iron Legs*2 for a Monk who knows it], same plot_value() curve reused with real Sneezy's own 3/18/13 bounds. Real upstream's race move-mod, gear/affect move bonus, and asthmatic-quest-bit halving have no Tobin equivalent yet and are simply absent, disclosed rather than faked. A level-3 Mage at average CON now comes to ~52 HP (was 28) -- Warrior's own real-Sneezy anchors of ~25 HP at level 1 and ~500 at level 50 land close (~45 and ~457 at average CON here) given the stat-scale remapping (Tobin's point-buy ATTR_BASE=120/ATTR_MAX=250 vs real Sneezy's raw 5-205 CON range) is necessarily approximate. Needed a `pow()` call, so CMakeLists.txt now links libm explicitly (was previously an implicit transitive link, and started failing to link once being.c actually called pow() directly).

- [x] **Real mana pool for Mage, ported from real SneezyMUD** -- done 2026-08-06, user: "add mana to prompt"/"add mana to score"/"implement it just like sneezy"/"meditate isnt a spell"/"meditate sits a character down and meditates back to his max mana"/"wizardry should also gain automatically from casting". New progress_t.mana/max_mana (being.h + player_progress migration); being_calc_max_mana() is a direct port of TPerson::manaLimit() (`100 + mana-skill-proficiency*3`), Mage only -- Druid deliberately excluded, real Sneezy spends Lifeforce there instead of mana (Tobin's own resource_pool_label() already called it that before this session), and Lifeforce itself is a separate, still-unbuilt resource. New spell_mana.c/.h: a 95-entry name->cost table scraped from every real spellInfo() call in sneezymud-master's spell_info.cc (its own MANA_<n> argument), not invented numbers -- gust really costs 10, inferno really costs 54. cmd_cast.c spends it (Mage-only, pre-roll, immortals exempt), refuses an unaffordable cast outright, and now trains `wizardry`/`mana` proficiency on every cast attempt (previously only the specific spell being cast ever trained). score/prompt (new PROMPT_FLAG_MANA) show it for a Mage instead of a hardcoded 0. `meditate` was redesigned mid-session after user pushback: briefly wired as `cast meditate` (wrong -- real Sneezy's meditate is passive, not something you cast), rebuilt as a standalone command (cmd_meditate.c) mirroring `yoginsa` exactly -- auto-sits you, the existing meditate_tick_run() background task (now resource-aware: Mana for Mage, HP/Vitality for anyone else) does the real work. Two real bugs caught live: the mana_mode check originally excluded immortals (an immortal Mage's meditate silently used the wrong resource), and regen.c's separate pre-existing "fully rested, stand up" check only ever looked at HP/Vitality, so it fired on a Mage's very FIRST meditate tick (HP/Vit almost always already full) before the mana tick could do anything -- fixed by skipping that check entirely while `meditating`. `cast meditate` still redirects with a one-line message rather than silently breaking muscle memory. New tests/smoke_test_mana.py (14 checks) -- along the way, confirmed a real gotcha: a live session's being_t loads once at login, so a mid-session SQL edit to player_progress needs a reconnect to actually take effect.

- [x] **flee should show which direction you fled** -- done 2026-08-06. cmd_flee.c now names the actual direction via DIR_NAMES[dir] (room.h), same lookup cmd_move.c uses -- "<y>You flee north, head over heels!<z>" instead of the old generic message.



- [x] **`kick` should be a way to start a fight** -- done 2026-08-08. `kick <target>` now opens a fight from scratch (cmd_attack.c's own target-lookup + fighting-pointer-swap shape) when not already fighting; bare `kick` still hits the current opponent as before. tests/smoke_test_kick_starts_fight.py (3 checks) passes live.

- [x] **Log player rents, including rent cost** -- DONE 2026-08-10: level^3 rent tax (Sneezy charge_rent_tax port, rent.c), wallet-then-bank payment, free <=L5, tunable via balance rent; logged with cost. See smoke_test_rent_cost.py. -- user, 2026-08-05: "add a log for a player renting" / "along with rent cost." `cmd_rent.c` exists (offline-regen mechanic) but doesn't appear to game_log() anything currently -- needs a LOG_* entry recording who rented and what it cost them, matching the existing character-delete/quit logging convention (player_repo.c/cmd_quit.c). Needs checking whether `rent` currently charges anything at all (a real gold cost may not exist yet, given TODO.md's Money system is still task 29/unbuilt).

- [x] **TobinMUD Client: input focus/repeat-last-command, preferences (window/font size), titlebar version string, File menu** -- done 2026-08-05, shipped as v0.4.0 (`client/src/win32/main.c`). Input box regains focus on every WM_ACTIVATE (window (re)activation), not just at startup. Hitting Enter on an empty input line resends `g_app.last_line` (the last real command actually sent) instead of a no-op. New Preferences window (hand-built popup, not a resource-file dialog -- this project's CMake has no RC/windres step wired up, see CMakeLists.txt) lets font point size and window width/height be changed live (`apply_font()` restyles already-visible scrollback text too, not just new lines) and persists them to `prefs.ini` next to the exe via the standard Get/WritePrivateProfileString Win32 APIs. Titlebar is now always `CLIENT_TITLE_BASE` ("TobinMUD Client v" + CLIENT_VERSION, derived via a widen-string-literal macro so it can never drift out of sync with the version constant) plus the existing live GMCP status suffix (`set_status_title()`) -- previously a GMCP update would overwrite the base title text entirely, only ever showing HP/room info with no version. New File menu (Connect/Reconnect/Disconnect/Preferences/Exit) and Help menu (About) via a real Win32 menu bar. Version bumped to 0.4.0 in both CLIENT_VERSION and tobinmud.wxs, rebuilt (zero warnings), repackaged, and published to the auto-update host -- every running client picks it up on next launch.

- [x] **Mage/Druid mobs should carry spellbags with real components** -- done 2026-08-05. Vnums 321/322/323 already existed as real spellbag prototypes (small/medium/large). Split the 1-60 level range into even thirds for bag size (1-20/21-40/41-60) since there was no existing bag-size tier ladder to mirror the way Cleric holy symbols have one. The load-on-spawn mechanism already existed too -- `being_grant_class_casting_supplies()` (being.c), called from `being_create_mob()` (Session 92), previously granted a single flat generic "pouch of spell components" (vnum 965881) to every Mage/Druid mob regardless of level; reworked it to spawn a level-sized spellbag with one random real component (user follow-up: "put components in for spells the mob would know at his level, at random") picked from the real seed-data pool (`obj` rows matching `%component mage%`, ~130 candidates) and nested inside via `thing_move_to()`. No per-spell reagent mapping exists in the seed data to bind a specific component to a specific spell, so "for spells it would know" is necessarily cosmetic only -- same as the pre-existing Cleric-symbol tiering's own disclosed limitation, since cmd_cast.c's find_keyword_item() gate only checks for the "component" keyword generically, not a specific item. Applies to every mob spawn path (zone resets, `load mob`, etc.) since it hooks being_create_mob() itself. Verified live: `tests/smoke_test_mob_casting_supplies.py` updated to loot a spawned Mage/Druid mob's corpse, pull the spellbag out, and check its contents against the real seed-data reagent list (the old test's "component" substring check only coincidentally worked because the old flat item's own flavor text happened to say "spell components" -- the new randomly-picked reagents' flavor text, e.g. "a star sapphire", never does). Also had to switch the test's own immortal tester from Mage to Warrior class to avoid an unrelated ambiguous-match collision with the tester's own newbie-gear spellbag. Clean rebuild, zero warnings.

## Open follow-ups, logged 2026-08-04

- [x] **Spell/prayer component requirements should appear in `help <spell>`** -- done 2026-08-05. The mechanism already existed: `skill_help.sql` (redesigned 2026-07-22, see its own header comment) already generates a `Requires:` footer line on every spell/prayer help topic naming the real component/holy-symbol keyword requirement, and this was already live in the DB. The actual gap was staleness, not a missing feature -- diffed skill.c's full roster against `help_topic` and found 20 names with no topic at all; of those, 6 are real cast/pray-reachable spells/prayers added to skill.c after the one-shot generator last ran (`protection from earth`, `inferno`, `stupidity` in cmd_cast.c; `sterilize`, `summon swarm` in cmd_pray.c; `animal companion` in cmd_cast.c) -- the other 14 are passive/physical skills (weapon specializations, evaluate, toughness, etc.) that were never cast/pray-gated in the first place and correctly need no such line. Added hand-authored topics for the 6 real gaps in the same format/conventions as the generated rows (Approx. Level/Classes from skill.c, Discipline % from sneezymud-master's real `discArray[]` START_ values where an upstream entry exists -- `summon swarm`/`animal companion` are Tobin-original mechanics with no upstream spellInfo to cite, so their Discipline line is correctly omitted, same precedent the generator itself used). Verified live against the DB (all 6 rows present with the correct Requires/Usage text).
- [x] **Inventory item lost after a SECOND quit/relog cycle** -- **not a bug, root-caused 2026-08-05.** Reproduced live with a sandbox immortal + a `load obj`-conjured trinket: the `player_inventory` DB row for the trinket never moved or vanished across two full raw-disconnect/reconnect cycles -- confirmed by querying the table directly after every step. What actually happens: `load obj`'s own `player_inventory_save()` call saves it as the FIRST row (lowest id) since it's the most-recently-added thing in the live `stuff_head` chain (head-inserted) at that moment. `player_inventory_load()` processes saved rows in ascending id order and re-attaches each via `thing_move_to()`, which also head-inserts -- so the lowest-id row (the trinket) ends up processed FIRST and therefore pushed to the very END of the rebuilt list after every single relog, regardless of cycle count. On an immortal with a bulky starting-gear loadout (20+ items), that's past the pager's first screen (`[ ENTER for more, Q to stop ]`) -- paging through confirmed the trinket was there the whole time, just on page 2. The "survives one cycle, gone after two" framing was almost certainly the tester not paging far enough on the second check, not a second-cycle-specific defect: the sink-to-bottom behavior is identical after cycle 1 and cycle 2. Not a data-loss bug -- no code change made. (Separately confirmed while investigating: an immortal's literal `quit!` command -- as opposed to a raw disconnect -- PURGES all carried/worn items outright by design, per CLAUDE.md's 2026-07-26 rule; that's expected and unrelated to this report, which used raw disconnects.) If this resurfaces, check the pager first before assuming loss.
- [x] **Cast/pray messaging: 3 lines per task step** -- done 2026-08-05. User clarified: "use sneezymuds casting task for example." Real source is sneezymud-master/code/code/misc/spelltask.cc's TBeing::sendCastingMessages(), which shows a gesture line and a verbal line (each mirrored to the room in third person) plus a "you begin to feel your spell taking form"/"prayer being answered" completion line -- once per ROUND of the original's real multi-round casting task. Tobin's cast/pray still resolve instantly (no multi-round task engine -- see the "examine spell architecture" item below for that separate, larger question), so ported the STYLE, not the full architecture: new shared `spell_flavor_show()` (`src/core/spell_flavor.c`/`.h`), called once at the top of both `task_cast()` (cmd_cast.c) and `task_pray()` (cmd_pray.c) -- applies uniformly to the whole roster. Shows one random gesture line + one random verbal line (each self + room-mirrored, using a gendered possessive pronoun per house style, not "their") + a fixed completion line, text adapted from the real spelltask.cc lines, kept separate for Mage/Druid vs Cleric flavor matching the original's own split. Only fires after every real gate (component/symbol/level/discipline) already passes. Verified live (`cast gust`, `pray sterilize`) and via new `tests/smoke_test_cast_pray_flavor.py` (gated and ungated cast/pray). Clean rebuild, zero warnings, deployed via copyover.
- [ ] **Examine how spells are formed in real SneezyMUD and port the approach here** -- user, 2026-08-04, no further detail yet. Likely means reviewing `sneezymud-master/code/code/misc/spell_info.cc`'s discArray[]/spell-casting architecture (already the source used for the spell/skill audit's roster data) for structural/mechanical patterns Tobin's `cmd_cast.c`/`cmd_pray.c` keyword-dispatch approach doesn't currently capture -- scope not yet defined.

- [x] **Copyover hangs a bit when restoring from a reboot** -- user, 2026-08-04: noticed live during this session's own copyover deploys, then "I believe this issue is what was eating machine resources." **Fixed, same session.** Root cause found: `copyover_recover()` (game_loop.c) restores every loose room mob/object from the copyover dump by calling `being_create_mob()`/`obj_create_from_proto()` per instance, and neither `mob_proto_load()` nor `obj_proto_load()` (mob_repo.c/obj_repo.c) cached anything -- every single instance re-ran a full DB SELECT for its prototype, even when hundreds of instances shared the same handful of vnums. World population had grown unboundedly across this session's own copyovers (3185 -> 3786 mob restores logged within about 2 hours, mostly leftover test-loaded mobs nothing ever purged), so the DB-query cost climbed every single copyover. Fix: an opt-in per-vnum prototype cache in mob_repo.c/obj_repo.c (`mob_proto_cache_begin()`/`_end()`, `obj_proto_cache_begin()`/`_end()`), OFF by default so `mob_proto_load()`/`obj_proto_load()` behave exactly as before everywhere else -- activated only around `copyover_recover()`'s own restore loop (game_loop.c), where it's provably safe (boot-time, single-threaded, nothing can be concurrently medit/oedit-saving or raw-SQL-editing a prototype). Verified live: two consecutive copyovers post-fix both completed their mob/object restore in ~1 second (log timestamp granularity) despite a still-growing population (3735, then 3786 mobs restored) -- previously that phase alone measured ~2 seconds and was climbing with population size. Confirmed the cache's scoping doesn't break live prototype edits: `tests/smoke_test_material.py` (which raw-SQL-edits `obj.material` mid-test and expects the very next spawned instance to reflect it, no restart) still passes clean. `tests/smoke_test_combat.py` and `tests/smoke_test_specproc_tuskgoring.py` also rerun clean post-copyover.
  - [x] **Fixed:** cleaned up the leftover-mob/room accumulation, and added two new admin commands so this doesn't need a one-off script next time. `purge <low>-<high>` (cmd_purge.c, 59+) clears loose mobs/objects from every currently-loaded room in a vnum range -- ran once room-by-room via a script first (670/670 sandbox rooms had something to purge), which is what motivated the real command. `zone reclaim <low>-<high>` (cmd_zone.c, 59+, user 2026-08-04) goes further: permanently deletes room/obj/mob prototype rows (plus their roomexit/roomextra/objaffect/objextra/obj_magic/mob_extra/mob_imm/mobresponses satellite rows) and any zone_reset/trigger rows referencing that range, refusing if any OTHER connected player is standing in an in-range room. Ran `zone reclaim 900000-1010000` against the real leftover test-sandbox range: 665 rooms, 513 objs, 321 mobs, 5 triggers deleted, live player connection undisturbed throughout. New tests/smoke_test_purge_range.py and tests/smoke_test_zone_reclaim.py pass live.
- [x] **Player PK should neither gain nor lose experience** -- user, 2026-08-04: player-vs-player kills currently affect XP like normal combat; PK outcomes should be XP-neutral for both sides ("They should still be able to loot gold. They just shouldn't get experience for killing another player."). **Fixed, same session.** Two spots in combat.c: `combat_award_hit_xp()` now returns immediately when `victim->base.kind == THING_PC` (no per-hit XP credit for landing blows on another player), and `combat_defeat()`'s death-XP-loss block now gates on `winner->base.kind != THING_PC` (used to just /10-reduce the loss for a PC winner, now skips it entirely). The PvP gold-to-corpse path (separate code, same function) is untouched -- looting a PK kill's gold still works exactly as before. New `tests/smoke_test_pk_xp.py` (6 checks) verifies both sides' `experience` column is unchanged across a PK kill; passes live. Deployed via copyover (temp level-60 `Deploybot` test account, deleted after use -- a real player connection was live throughout).
  - [x] **Fixed:** `tests/smoke_test_pk_gold.py` now sends `toggle autoloot` for the winner before attacking, matching how the corpse-gold-loot message actually fires. Passes live again.
- [x] **Fixed:** `get all.<target>` was verified live and already works correctly (case-insensitive per-keyword-prefix match against room-floor items, e.g. `get all.gizmo` matches "a trinket gizmo"). `remove all.target` was the real gap -- it genuinely didn't exist at all before this session (user's "not implemented at all, command fails" pinned it down) -- now implemented (`remove_all_worn()`, cmd_object.c) with the same `all`/`all.<name>` convention. Both verified live/via smoke test.
  - [x] **Fixed, user follow-up "get from containers?":** `get all.<name> <container>` now works too -- the bare room-floor-only `get all.<name>` couldn't reach into a container sitting in the room. cmd_object.c's `get all <container>` (which already emptied a whole container unconditionally) now accepts the same `<name>` filter; bare `get all <container>` (no filter) still empties everything, unaffected.
  - [x] **Fixed, user follow-up "and drop all.target":** `drop all.<name>` added (cmd_object.c), same convention as bare `drop all`.
  - [x] **Fixed, user follow-up "now we can add room reclaim for just rooms obj reclaim for objs mob reclaim for mobs trigger reclaim etc":** split `zone reclaim`'s all-at-once sweep into narrower per-type forms under each editor's own `edit <noun>` entry point -- `edit room reclaim <low>-<high>`, `edit object reclaim <low>-<high>`, `edit mob reclaim <low>-<high>`, `edit trigger reclaim <low>-<high>` (all 59+, cmd_edroom.c/cmd_edobject.c/cmd_edmobile.c/cmd_edtrigger.c), each deleting only its own table (room reclaim also refuses if another player is standing in-range, same as `zone reclaim`; the immortal issuing the command can stand there themselves). New tests/smoke_test_get_all_container.py, tests/smoke_test_drop_all_target.py, tests/smoke_test_edit_reclaim.py all pass live.

## User batch 2026-08-04 — done

- [x] **Crit hit messages reproduced from Sneezy** — done, Session 128.
      Scoped down after checking the real upstream source
      (`crit_combat.cc`, ~2800 lines): upstream's crit mechanic is a
      whole separate crit-chance-roll/broken-bone system
      (`critBlunt()`/`critSlash()`/`critPierce()`) Tobin doesn't have and
      isn't porting -- Tobin's own crit trigger stays "a limb's HP
      crosses to 0%" (Session 42). What upstream's own wording DID give:
      weapon-category-flavored severing phrases instead of one hardcoded
      "is severed clean off!" for every weapon. `combat.c`'s
      `sever_verb_phrase()` now picks a phrase (sliced/hacked/crushed/
      punctured/skewered/torn, drawn from upstream's own critBlunt/
      critSlash/critPierce wording) keyed off the same `weapon_verb()`
      category buckets `combat_strike()`'s per-hit message already
      computes. Existing `tests/smoke_test_crit.py` (19 checks, generic/
      bare-hand path) passes live; the weapon-flavored path was verified
      by code review + a manual live check rather than a new committed
      test (ran out of budget debugging an ad hoc sandbox-room/wield
      setup issue in a throwaway verification script -- not a gap in the
      feature itself, `sever_verb_phrase()` is a plain string switch).

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
- [x] Inferno — **Fixed 2026-08-05:** real upstream discArray[SPELL_INFERNO] (Mage, DISC_FIRE). New skill.c roster entry (level 21, CLASS tier) + cmd_cast.c branch: a real instant strike (combat_apply_skill_damage(), same shape every offensive spell here uses -- Tobin has no fire-DoT resource to port the original's periodic burn into, disclosed gap).
- [x] Protection from earth — **Fixed 2026-08-05:** real upstream discArray[SPELL_PROTECTION_FROM_EARTH] (Mage, DISC_EARTH, TAR_AREA). New skill.c roster entry (level 8, CLASS tier) + cmd_cast.c branch: real room-wide AFFECT_SANCTUARY ward, same room-walk-loop shape as sorcerer's globe (Tobin has no elemental damage-type system to give a literal earth-resistance, same disclosed scope-cut as every other elemental-flavored spell this audit has ported).

#### Cleric (1)

- [x] Sterilize — **Fixed 2026-08-05:** real upstream discArray[SPELL_STERILIZE] cures a PART_INFECTED limb flag; Tobin already has a matching real affect, AFFECT_DISEASE_INFECTION. New skill.c roster entry (level 6, CLASS tier) + cmd_pray.c branch: real targeted cure, same shape as clot/remove curse.

#### Druid (6) — no Ranger class in Tobin

- [ ] Creeping doom
- [x] Living vines -- **Done 2026-08-10 (Batch C, Session 148 aftermath) -- see STATUS.md Session 147.** Roots + easier-to-hit debuff, outdoors only (folded onto Druid).
- [ ] Root control
- [ ] Shapeshift
- [ ] Sticks to snakes
- [ ] Stormy skies

#### Shaman (44) — no Shaman class in Tobin (Druid reused only 6 of these)

- [ ] Aqualung
- [ ] Aquatic blast
- [ ] Boiling blood
- [ ] Celerite
- [ ] Chase spirits
- [ ] Cheval
- [ ] Chrism
- [ ] Clarity
- [ ] Cleanse
- [ ] Control undead
- [ ] Coronary
- [ ] Create diamond golem
- [ ] Create iron golem
- [ ] Create rock golem
- [ ] Create wood golem
- [ ] Dancing bones
- [ ] Death wave
- [ ] Detect shadow
- [ ] Distort
- [ ] Djalla's protection
- [ ] Embalm
- [ ] Enliven
- [ ] Enthrall demon
- [ ] Enthrall ghast
- [ ] Enthrall ghoul
- [ ] Enthrall spectre
- [x] Flatulence -- **Done 2026-08-10 (Batch C, Session 148 aftermath) -- see STATUS.md Session 147.** Room-wide damage sparing the caster's group (Druid).
- [ ] Healing grasp
- [ ] Hypnosis
- [ ] Intimidate
- [ ] Legba's guidance
- [ ] Lich touch
- [ ] Life leech
- [x] Raze -- **Done 2026-08-10 (Batch C, Session 148 aftermath) -- see STATUS.md Session 147.** Hardest-hitting single strike on the Druid roster, refuses on an immortal.
- [ ] Resurrection
- [ ] Romble
- [ ] Sense presence
- [ ] Shadow walk
- [x] Shield of mists -- **Done 2026-08-10 (Batch C, Session 148 aftermath) -- see STATUS.md Session 147.** A real defensive to-hit debuff on the caster (Druid).
- [ ] Soul twister
- [ ] Squish
- [x] Thornflesh -- **Done 2026-08-10 (Batch C, Session 148 aftermath) -- see STATUS.md Session 147.** Self buff that damages anyone landing a melee hit on you (Druid).
- [ ] Vampiric touch
- [ ] Voodoo

### Skill port status — 105 of 232 SneezyMUD skills not yet in Tobin

#### Warrior (5)

- [x] Advanced blacksmithing — **Fixed 2026-08-05:** new roster entry; cmd_repair.c skips the depreciation increment on a successful repair when known (real, working difference from base `repair`).
- [x] Blacksmithing — **Re-triaged 2026-08-05:** this IS Tobin's existing `repair` skill (Warrior) under its real-upstream name -- already fully covered, not a separate gap.
- [x] Bloodlust — **Fixed 2026-08-05:** new roster entry; combat.c grants a passive hitroll/damroll bonus scaling with proficiency, same shape as the weapon-specialization bonus it sits next to (real upstream's own stacking-buff mechanic has no matching Tobin infrastructure -- disclosed scope-cut).
- [x] Debride — **Fixed 2026-08-05:** new roster entry + new `debride <item>` command (cmd_repair.c) -- reduces an item's depreciation by 1, the real inverse of what `repair` increases (real upstream's ITEM_RUSTY-flag mechanic has no Tobin equivalent -- disclosed scope-cut, same-value reuse of the depreciation field instead of doing nothing).
- [x] Stomp — **Fixed 2026-08-05:** new roster entry + new `stomp` command (cmd_stomp.c), same shape as `kick` -- a real leg-targeted attack (real upstream's own berserk-auto-proc-pool integration has no Tobin equivalent -- disclosed scope-cut, same "own standalone command" precedent as kick).

#### Cleric (1)

- [x] Cleric repair — **Fixed 2026-08-05:** new `repair` roster row for Cleric -- reuses the exact same command/mechanic Warrior's `repair` already has (cmd_repair.c gates by skill name + caster's own class, so a same-named row for another class needs no code changes).

#### Monk (2)

- [x] Oomlat Philosophy — **Fixed 2026-08-05:** new roster entry; being_total_ac() (being.c) grants a real passive AC bonus scaling with skill_proficiency(), same ratio as real upstream's own armor-scaling formula (combat.cc).
- [x] Monk repair — **Fixed 2026-08-05:** new `repair` roster row for Monk, same reuse as Cleric repair above.

#### Thief (2)

- [x] Swindle — **Fixed 2026-08-05:** new roster entry; cmd_shop.c's buy/sell price formulas apply a real 0-10% haggling discount/markup scaling with skill_proficiency(), same ratio as real upstream's own getSwindleBonus().
- [x] Thief repair — **Fixed 2026-08-05:** new `repair` roster row for Thief, same reuse as Cleric/Monk repair above.

#### Ranger (6) — no Ranger class in Tobin

- [ ] Apply herbs
- [x] Beast charm -- **Done 2026-08-10 (Batch C, Session 148 aftermath) -- see STATUS.md Session 147.** Wired into the charmed-pet system (Druid); upstream's own version is a stub.
- [ ] Beast summon
- [x] Befriend beast -- **Done 2026-08-10 (Batch C, Session 148 aftermath) -- see STATUS.md Session 147.** Wired into the charmed-pet system (Druid); upstream's own version is a stub.
- [ ] Transfix
- [ ] Transform limb

#### Deikhan (24) — no Deikhan (paladin) class in Tobin

- [ ] 2h specialization (Deikhan variant)
- [x] Advanced riding -- **Done 2026-08-10 (Batch C, Session 148 aftermath) -- see STATUS.md Session 147.** Improves mount success and charge damage; every class, no Deikhan class needed.
- [ ] Aura of absolution
- [ ] Aura of might
- [ ] Aura of regeneration
- [ ] Aura of the guardian
- [ ] Aura of vengeance
- [ ] Bash (Deikhan variant)
- [x] Calm mount -- **Done 2026-08-10 (Batch C, Session 148 aftermath) -- see STATUS.md Session 147.** Unseat-resist on a landed hit; every class.
- [x] Charge -- **Done 2026-08-10 (Batch C, Session 148 aftermath) -- see STATUS.md Session 147.** Bonus-damage + knockdown opening strike, mounted only; every class.
- [ ] Chivalry
- [ ] Deikhan repair
- [ ] Divine grace
- [ ] Divine rescue
- [ ] Guardians light
- [ ] Lay hands
- [ ] Orient mount
- [ ] Ride domestic
- [ ] Ride exotic
- [ ] Ride non-domestic
- [ ] Ride winged
- [ ] Shock cavalry
- [ ] Smite
- [ ] Train mount

#### Generic / cross-class (50)

- [x] Advanced defense -- **Done 2026-08-10 (Session 150):** new roster entry (all 6 classes), real passive wired into combat.c, learn-by-doing; verified live.
- [x] Advanced offense -- **Done 2026-08-10 (Session 150):** new roster entry (all 6 classes), real passive wired into combat.c, learn-by-doing; verified live.
- [x] Alcoholism -- **Done 2026-08-10 (Batch C, Session 148 aftermath) -- see STATUS.md Session 147.** New drunk stat from real per-liquid values, to-hit penalty + pass-out, sobers over time.
- [ ] Avian (language)
- [x] Bandage -- **Fixed 2026-08-08 (Session 141):** new bleeding-limb mechanic (transient per-limb `bleeding` flag, chipped each vitals tick) + `bandage [target]` command (cmd_bandage.c) that consumes a bandage item to clear it and heal a little.
- [x] Barehand specialization — **Fixed 2026-08-04:** all 5 weapon specializations (slash/blunt/pierce/ranged/barehand) now implemented (skill.c/combat.c/cmd_skills.c) -- Warrior-only, level 1, auto-known with no guildmaster visit needed, individually learned by doing (Class-tier, uncapped ceiling) as a real passive hit/damage bonus scaling with proficiency, with an extra bump at exactly 100%. Kept genuinely distinct from Tobin's pre-existing "*_proficiency" Combat-tier skills (found live while building this -- those just mirror the shared combat_disc_pct with no individual tracking; specialization is a separate, individually-grindable layer on top). New tests/smoke_test_weapon_spec.py passes live.
- [x] Blunt specialization
- [ ] Bullycroak (language)
- [ ] Climbing
- [ ] Common (language)
- [x] Cook -- **Fixed 2026-08-08 (Session 138):** the existing recipe system had no skill gate at all; now requires `being_knows_skill(ch, "cook")` (all 6 classes, Combat tier).
- [x] Defense -- **Fixed 2026-08-08 (Session 138):** new roster entry (all 6 classes, level 1, Combat tier); combat_strike() grants a passive cross-class to-hit-modifier reduction, the first passive defensive skill every class gets.
- [ ] Dissect
- [ ] Divine (fortune-telling; distinct from Mage's "divination" spell)
- [ ] Encamp
- [x] Evaluate -- **Fixed 2026-08-05:** new roster entry (all 6 classes, generic) + new `evaluate <item>` command (cmd_evaluate.c) -- real upstream (cmd_compare.cc) gates a `compare` command Tobin never had; ported as its own new command instead, tiered by proficiency: a fuzzed price guess always, condition at 30%+, exact price + material tier at 60%+.
- [x] Fast heal -- **Done 2026-08-10 (Session 150):** new roster entry (all 6 classes); real upstream SKILL_FAST_HEAL has no traced regen formula in this source snapshot, ported as a passive rest-healing bonus scaling with proficiency (regen.c), disclosed name-driven.
- [ ] Fast load
- [ ] Fish burble (language)
- [ ] Fishing
- [ ] Fishlore
- [x] Focused avoidance -- **Fixed 2026-08-05:** new roster entry (all 6 classes, generic, Advanced tier); combat.c grants a real passive to-hit-modifier reduction against the defender scaling with proficiency, same insertion point/shape as the existing `oomlat` AC bonus (real upstream's own agility-scaled dodge check has no matching Tobin infrastructure -- disclosed scope-cut).
- [ ] Gnoll jargon (language)
- [ ] Gutter cant (language)
- [x] Hiking -- **Fixed 2026-08-08 (Session 141):** new roster entry; reduces cmd_move.c's terrain movement cost up to 50% at full proficiency, stacking with flying/mounted discounts.
- [x] Inevitability -- **Done 2026-08-10 (Session 150):** new roster entry (all 6 classes); real upstream is a repeatedly-activated stacking +hitroll buff (cap +50), ported as a flat passive to-hit bonus scaling with proficiency (combat.c) -- same disclosed scope-cut toughness/bloodlust used.
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
- [x] Offense -- **Done 2026-08-10 (Session 150):** new roster entry (all 6 classes), real passive wired into combat.c, learn-by-doing; verified live.
- [x] Pierce specialization
- [x] Ranged specialization
- [ ] Read magic
- [ ] Seekwater
- [x] Sharpen -- **Done 2026-08-10 (Batch C, Session 148 aftermath) -- see STATUS.md Session 147.** New mutable weapon sharpness stat, whetstone-gated, feeds the combat condition bonus.
- [ ] Skinning (generic, non-Druid classes)
- [x] Slash specialization
- [x] Smooth -- **Done 2026-08-10 (Batch C, Session 148 aftermath) -- see STATUS.md Session 147.** Blunt-weapon counterpart to sharpen (file-gated); also fixed blunt weapons getting no condition bonus at all.
- [x] Swim -- **Fixed 2026-08-08 (Session 138):** new roster entry (all 6 classes, Combat tier); mitigates Tobin's unconditional underwater drowning damage, up to 50% off at full proficiency.
- [x] Tactics -- **Done 2026-08-10 (Session 150):** new roster entry (all 6 classes). Real upstream carries NO traced mechanical effect (only a learnable/displayed stat); ported as a small learn-by-doing to-hit nudge (combat.c), disclosed name-driven, same shape as wizardry/casting/praying.
- [x] Toughness -- **Fixed 2026-08-05:** new roster entry (all 6 classes, generic, Advanced tier); combat.c grants a real passive damage-reduction percentage (up to 20% at 100% proficiency) applied after Sanctuary (real upstream's own per-hit stacking-immunity buff has no matching Tobin infrastructure -- disclosed scope-cut, same "flat passive instead of true stacking" shape `bloodlust` already used).
- [ ] Troglodyte pidgin (language)
- [ ] Trollish (language)
- [x] Whittle -- **Fixed 2026-08-08 (Session 138):** the existing recipe system had no skill gate at all; now requires `being_knows_skill(ch, "whittle")` (all 6 classes, Combat tier).
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

- [x] bank sub-verbs (balance/deposit/withdraw) now prefix-match as abbreviations (cmd_bank.c), matching the treasury/allocate convention.
- Always use SneezyMUD code as implementation guidance where available.
- Complete the current task fully before moving to the next.
- Continue progressing rapidly through the backlog without waiting for additional instructions unless blocked by missing requirements.
- Document all completed changes, database updates, VNUM allocations, and implementation notes.
- Help me conserve tokens. Go absolutly silent unless acknowledging an instruction, or giving a brief report on an item just finished. Otherwise, silence.
