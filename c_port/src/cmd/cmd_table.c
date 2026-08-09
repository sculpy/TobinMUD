/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "alias_repo.h"
#include "being.h"
#include "cmd_internal.h"
#include "socials.h"

/* First-word command dispatch with DikuMUD-style abbreviation matching:
 * any non-empty prefix of a command's name dispatches to it (e.g. "sc" or
 * "sco" both reach "score", same as "l" reaches "look") -- replacing the
 * original's much larger cmd/ directory of command-table machinery. Add
 * an entry here + a cmd_<name>.c as each command gets ported -- see
 * c_port/STATUS.md.
 *
 * `quit` is deliberately NOT in this table -- it's excluded from
 * abbreviation matching entirely and requires the exact, full literal
 * "quit!" (see cmd_dispatch below), so a mistyped or abbreviated command
 * can never accidentally leave the character.
 *
 * =====================================================================
 * ORDERING RULES -- THIS TABLE'S ORDER IS ITS SEMANTICS.
 * Read this before moving any line.
 *
 * cmd_dispatch() dispatches to the FIRST entry the caller's level lets
 * them see whose name STARTS WITH what they typed. So an entry's position
 * decides which command owns every abbreviation it shares with another,
 * and moving a line silently rewires player muscle memory.
 *
 * SNEEZY-PRECEDENCE ORDER (Phase 2, TODO.md priority item, user 2026-08-02:
 * "do the reorder"). Superseded the earlier tier-then-alphabetical scheme
 * (see STATUS.md/git history for that era's own rationale) in favor of
 * matching the REAL upstream SneezyMUD's abbreviation precedence exactly:
 *
 *   - Every Tobin command with a real upstream equivalent (matched by
 *     exact name against SneezyMUD's own `commandArray[]`, misc/parse.cc)
 *     is ordered by that command's raw `CMD_*` enum declaration order
 *     (misc/parse.h) -- `searchForCommandNum()` (parse.cc) walks that
 *     array in exactly that order and returns the first `is_abbrev()`
 *     match, so the enum's declaration order (NOT buildCommandArray()'s
 *     own assignment-statement order, which does NOT match it -- e.g.
 *     CMD_TROPHY is assigned right after CMD_SCORE textually but is
 *     declared ~500 entries later) IS Sneezy's real precedence.
 *   - Every Tobin-only command (no real Sneezy equivalent -- new
 *     features, renamed/collapsed editors, etc.) keeps its ORIGINAL
 *     relative position, anchored immediately before whichever matched
 *     command originally followed it in this table. This keeps
 *     Tobin-invented commands sitting near whatever thematic context
 *     they were hand-placed in, rather than being scattered to wherever
 *     a purely mechanical merge would drop them.
 *   - EXCEPTION, mechanically necessary: if a Tobin-only name is a
 *     STRICT PREFIX of another command's name (e.g. `set` / `settrap`,
 *     `quest` / `questdef`), the shorter name must stay ahead of the
 *     longer one no matter what the two rules above would otherwise
 *     produce -- otherwise typing a command's own COMPLETE, EXACT name
 *     could fail to reach itself, which no reorder should ever do. Two
 *     such pairs existed in the raw merge and were manually pulled back
 *     into the required order: `settrap` moved to directly follow `set`,
 *     `questdef` to directly follow `quest`.
 *
 * A mortal player is completely unaffected by any of this: the min_level
 * gate below skips every entry above the caller's level BEFORE the name
 * check even runs, so an immortal-only command can never occupy any of a
 * mortal's abbreviation space regardless of table position. The
 * cross-tier interleaving this reorder introduces (immortal-tier entries
 * with a low Sneezy index now sitting ahead of low-Sneezy-index mortal
 * entries) therefore only affects an IMMORTAL typing an ambiguous
 * abbreviation while both readings are visible to them -- a real, deliberate
 * trade against the earlier 2026-07-12 "immortal commands sort lower so
 * immortals don't fumble into one by accident" directive, made explicitly
 * in favor of matching Sneezy's real precedence per the 2026-08-02
 * instruction. Every exact, full command name still always reaches
 * itself regardless of level (verified mechanically, see below) -- only
 * SHORT AMBIGUOUS ABBREVIATIONS shifted, which is the entire, intended
 * point of this reorder.
 *
 * Per-entry "SWAP: X before Y so ..." comments surviving below from the
 * PRE-reorder tier-alphabetical era are HISTORICAL ONLY -- they describe
 * reasoning from an ordering scheme this table no longer uses, and the
 * neighbors they reference may no longer even be adjacent. Do not trust
 * them for current dispatch behavior; they're kept only so the original
 * per-command design intent isn't lost. Trust a live prefix-resolution
 * diff instead (same tool, updated methodology below).
 *
 * VERIFY REORDERS MECHANICALLY, NOT BY EYE. `tests/tools/cmd_abbrev_check.py`
 * resolves every prefix of every command at every level against an old and
 * a new copy of this file and diffs the two. The 2026-08-02 Sneezy-precedence
 * reorder was verified two ways: (1) zero commands fail to resolve to
 * THEMSELVES when typed in full, at every level they're visible (a hard
 * invariant, not a judgment call); (2) every remaining abbreviation change
 * was reviewed by hand and found to be the intended, expected consequence
 * of matching Sneezy's real precedence, not an accidental regression.
 * =====================================================================
 *
 * SNEEZYMUD COMMAND-PARSER AUDIT (TODO.md priority item, user 2026-07-30:
 * "Rewrite the command parser to match SneezyMUD exactly. Import
 * equivalent commands, comment out unsupported Sneezy commands, and
 * align command naming with the Tobin codebase"). Full scope check
 * against the real upstream source first: SneezyMUD's actual command
 * table (`buildCommandArray()`, misc/parse.cc) registers 568 named
 * commands -- an order of magnitude more than this table's ~210. Landed
 * in two phases:
 * (1) 2026-08-02 (Session 106): A COMPLETE accounting of every Sneezy
 *     command name against this table (below) -- genuinely missing
 *     entries are listed, grouped, and left commented out rather than
 *     silently dropped (the user's own literal instruction), not
 *     actually wired up here.
 * (2) 2026-08-02 (this pass, user: "do the reorder"): this table's own
 *     ORDER now matches Sneezy's real abbreviation precedence for every
 *     command the two systems share -- see the ORDERING RULES block
 *     above for the full methodology and the mortal-safety analysis.
 * A handful of confirmed like-for-like renames noted inline where Tobin
 * already has the real equivalent under different wording (e.g. Sneezy's
 * "feign death" / "redit"+"medit"+"oedit"+"fedit" -> Tobin's
 * `feigndeath` / unified `edit <noun>`) -- NOT given their own table
 * entry, since they already work under Tobin's own name; noted so a
 * future session doesn't re-flag them as gaps. Everything else in the
 * "genuinely not yet ported" lists below was NOT individually verified
 * against Tobin's full skill/social/help surface with full precision
 * (262 names is too large to hand-check one by one in a single pass) --
 * grouped by rough theme with a best-effort category note. A future
 * session narrowing any one group into real cmd_*.c work should
 * re-verify each name isn't already covered before starting.
 *
 * Already covered under different Tobin naming (verified, not gaps):
 *   redit, medit, oedit, fedit  -> unified `edit <noun>` (cmd_edit.c)
 *   "feign death" (two words)   -> `feigndeath` (cmd_*, level-25 batch)
 *   trigger                     -> `edit trigger` (menu-driven)
 *   quit / quit!                -> handled specially, excluded from this
 *                                  table by design (see file-top comment)
 *
 * Genuinely not yet ported, grouped (commented out per user instruction --
 * NOT wired to any handler):
 *   -- Punctuation say/emote shortcuts (Sneezy: ' = say, , = emote,
 *      : = emote) -- Tobin has no punctuation-prefixed command syntax:
 *      ', ,, :
 *   -- Immortal-only admin/dev tooling likely irrelevant to Tobin's own
 *      DB-only, no-zonefile architecture (bload/gload/rload/rsave are a
 *      zonefile save/load model Tobin doesn't have; cutlink/cutlog/
 *      loglist/hostlog/traceroute/clientmessage/testcode/testfight/
 *      bruttest are original dev-debug tooling):
 *      bload, gload, rload, rsave, cutlink, cutlog, loglist, hostlog,
 *      traceroute, clientmessage, clients, testcode, testfight, bruttest,
 *      access, adjust, checklog, dfold, fold, viewoutput, buildhelp,
 *      whozone, zones, world, deathcheck, gamestats
 *   -- Systems Tobin deliberately doesn't model (mail, factions --
 *      cmd_stat.c's own comment: "we will not support factions" -- meta/
 *      OOC extras like donate/vote/television/poop, wizlock):
 *      mail, email, findemail, newmember, recruit, disband, makeleader,
 *      factions, donate, vote, television, poop, wizlock, tithe, bet,
 *      bid, deal, distribute, divine, gain, deposit, withdraw, store,
 *      value
 *   -- Real gameplay gaps worth a future look (skills/spells-as-verbs,
 *      combat maneuvers, crafting, movement variants) -- genuinely
 *      absent, not just renamed:
 *      bandage, barkskin, bonebreak, breathe, brew, camp, capture,
 *      charge, charm, climb, combine, conceal, cover, crawl, cudgel,
 *      defenestrate, dissection, dodge, doorbash, drag, earthmaw,
 *      enter, feral, fish, flag, fly, focus, force, fortify, glance,
 *      grab, guard, harness, hide, innate, invisible, join, juggle,
 *      land, "lay-hands", leap, lift, loot, mend, "mend limb", "mindfocus",
 *      "mindthrust", operate, order, orient, outfit, parry (already
 *      passive, no command), pass, penance, pick, "poison-weapon",
 *      "psiblast", "psidrain", "psycrush", protect, pull, push, quaff,
 *      "quivering palm", raise, receive, recharge, recite, release,
 *      rename, replace, reset, resize, restore, restring, retrain,
 *      rituals, saddle, scribe, search, "seekwater", sharpen, "shoulder
 *      throw", shuffle, skulk, slay, slit, smite, sooth, spy, stab,
 *      stomp, summon, take, tan, taste, tie, timeshift, toast, transfix,
 *      transform, trap, turn, twist, unharness, unsaddle, untie,
 *      whittle
 *   -- Player-facing informational/utility commands worth a look:
 *      abort, afk, ask, at, attribute, attune, aura, boot, break,
 *      call, change, check, clear, clone, cls, comment, commands,
 *      compare, credits, description, descend, disengage, drive,
 *      echo, emote, evaluate, feign death, history, ideas, info,
 *      insult, "highfive", "kwave", leave, levels, low, lower, map,
 *      medit, meditate, message, motd, move, nojunk, noop, noshout,
 *      office, paint, pass, play, post, powers, "pracinfo", prayers,
 *      preen, press, "psay", "pshout", "ptell", quaff, reboo, reboot,
 *      recruit, remember, "rememberplayer", report, request, resize,
 *      rmember, run, "seekwater", send, shoot, shuffle, sky, smooth,
 *      sort, spells, spy, stay, store, tasks, throw, tithe, toast,
 *      track, trophy, typos, unsaddle, visible, where, whittle,
 *      wizlist, world, zones
 *   -- Ambiguous/likely-duplicate/typo entries in Sneezy's own source
 *      (not confirmed real, listed for completeness only):
 *      as, sooth (vs. "soothe"?), reboo/shutdow (truncated?)
 * ===================================================================== */
static const cmd_entry_t COMMANDS[] = {

    /* ==================== MORTAL-VISIBLE COMMANDS ==================== */

    /* -- Pinned movement head (rule 2); not alphabetized. -------------- */
    /* Cardinals own the single letters n/e/s/w/u/d. */
    { "north",   cmd_north,   "Walk north.",                                        MORTAL_LEVEL_MIN },
    { "east",    cmd_east,    "Walk east.",                                         MORTAL_LEVEL_MIN },
    { "south",   cmd_south,   "Walk south.",                                        MORTAL_LEVEL_MIN },
    { "west",    cmd_west,    "Walk west.",                                         MORTAL_LEVEL_MIN },
    { "up",      cmd_up,      "Walk up.",                                           MORTAL_LEVEL_MIN },
    { "down",    cmd_down,    "Walk down.",                                         MORTAL_LEVEL_MIN },
    /* Diagonals AFTER the cardinals so "n"/"s" stay north/south -- the long
     * names need "northe"/"southw"-length prefixes to clear them. The
     * two-letter forms are NOT prefixes of the long names ("ne" vs
     * "no-rtheast"), so they get explicit alias rows, the classic Diku
     * arrangement. "se"/"sw" sit above the set-family and switch in
     * matching order but shadow nothing: those need "set"/"swi" anyway. */
    { "northeast", cmd_northeast, "Walk northeast.",                                MORTAL_LEVEL_MIN },
    { "northwest", cmd_northwest, "Walk northwest.",                                MORTAL_LEVEL_MIN },
    { "southeast", cmd_southeast, "Walk southeast.",                                MORTAL_LEVEL_MIN },
    { "southwest", cmd_southwest, "Walk southwest.",                                MORTAL_LEVEL_MIN },
    { "ne",        cmd_northeast, NULL,                                             MORTAL_LEVEL_MIN },
    { "nw",        cmd_northwest, NULL,                                             MORTAL_LEVEL_MIN },
    { "se",        cmd_southeast, NULL,                                             MORTAL_LEVEL_MIN },
    { "sw",        cmd_southwest, NULL,                                             MORTAL_LEVEL_MIN },
    { "drink",   cmd_drink,   "Drink from a puddle, fountain, or carried container (drink <target>).", MORTAL_LEVEL_MIN },
    /* "ea" is already claimed by "east" (pinned movement head, elsewhere in
     * this table) -- eat needs the full "eat" to reach it, same spirit as
     * the drop/drink swap just above. */
    { "eat",     cmd_eat,     "Eat a carried food item (eat <food>).",              MORTAL_LEVEL_MIN },
    /* Body-slot items only since the hold/wield split (user 2026-07-09) --
     * a holdable item refuses `wear` and points at whichever of hold/wield
     * applies. */
    { "wear",    cmd_wear,    "Put on a carried item's body slot (wear <item>).",   MORTAL_LEVEL_MIN },
    /* Weapons only; a non-weapon refuses wield and points to hold. */
    { "wield",   cmd_wield,   "Wield a weapon in a free hand (wield <item>).",      MORTAL_LEVEL_MIN },
    /* SWAP: look before limbs, so "l" looks; limbs needs "li". */
    { "look",    cmd_look,    "Look around the room you're in.",                    MORTAL_LEVEL_MIN },
    /* SWAP: score before scan, so "sc"/"sco" still reach score; scan needs
     * "sca". */
    { "score",   cmd_score,   "Show your character's stats, level, and HP.",        MORTAL_LEVEL_MIN },
    /* Sneezy's own real command table assigns CMD_TROPHY right after
     * CMD_SCORE textually (see this file's own Phase 2 reorder notes,
     * 2026-08-02) -- kept adjacent here too; no real "tr"-prefix
     * ambiguity risk (transfer/trip/treasury/trance all diverge by the
     * 3rd letter). */
    { "trophy",  cmd_trophy,  "See how the XP you earn from repeat kills has decayed (trophy [name]).", MORTAL_LEVEL_MIN },
    /* shout before show is already alphabetical, but it is load-bearing:
     * "sho" is ambiguous and shout wins it -- "show" must be typed in
     * full. */
    { "shout",   cmd_shout,   "Shout something to everyone in the game (shout <msg>).", MORTAL_LEVEL_MIN },
    { "tell",    cmd_tell,    "Send a private message to anyone playing (tell <name> <message>).", MORTAL_LEVEL_MIN },
    /* SWAP: inventory before idea/immort, so bare "i" is an inventory
     * check -- always the more useful outcome than a no-op immort attempt.
     * idea needs "id", immort needs "im". */
    { "inventory", cmd_inventory, "List what you're carrying.",                     MORTAL_LEVEL_MIN },
    /* "g" must reach get, not goto -- alphabetical already delivers that
     * (get < goto), but it is load-bearing, not incidental: goto's landmark
     * forms (guildmaster/rent/surplus) are mortal-visible, so goto would
     * otherwise shadow get's single letter for everyone. Immortals wanting
     * goto's vnum/player teleport still have "go" -- nothing else starts
     * with "go". */
    { "get",     cmd_get,     "Pick up an item, or take one from a container (get <item> [container]).", MORTAL_LEVEL_MIN },
    /* SWAP: say before save, so "sa" still speaks; save needs "sav".
     * ("s" is south -- movement head.) */
    { "say",     cmd_say,     "Say something to everyone in the room.",             MORTAL_LEVEL_MIN },
    { "group",   cmd_group,   "List/manage your group (group [<name>|all]) -- leader only to add.", MORTAL_LEVEL_MIN },
    { "put",     cmd_put,     "Put a carried item into a container (put <item> <container>).", MORTAL_LEVEL_MIN },
    { "help",    cmd_help,    "List available commands.",                           MORTAL_LEVEL_MIN },
    /* SWAP: who before whisper, so "wh" lists players; whisper needs
     * "whi". */
    { "who",     cmd_who,     "List everyone currently playing.",                   MORTAL_LEVEL_MIN },
    { "stand",   cmd_stand,   "Stand up.",                                          MORTAL_LEVEL_MIN },
    /* SWAP: sit before sip, so "si" sits; sip must be typed in full. */
    { "sit",     cmd_sit,     "Sit down.",                                          MORTAL_LEVEL_MIN },
    /* SWAP: rest before remove/rent, so "r"/"re" still rest; remove needs
     * "rem", rent needs "ren", rules needs "ru". */
    { "rest",    cmd_rest,    "Sit down and rest (heals faster).",                  MORTAL_LEVEL_MIN },
    { "skills",  cmd_skills,  "List your class's skills/spells, known and locked.", MORTAL_LEVEL_MIN },
    { "sleep",   cmd_sleep,   "Lie down and sleep (heals fastest).",                MORTAL_LEVEL_MIN },
    /* "w"/"we" are west (movement head); wake needs "wa", wear needs "wea",
     * wield needs "wi". */
    { "wake",    cmd_wake,    "Wake up from sleep.",                                MORTAL_LEVEL_MIN },
    { "test",    cmd_test,    "Show the currently-running smoke test, if any.",     TEST_MIN_LEVEL },
    { "tipedit", cmd_tipedit, "Add/list/delete tips (tipedit <text>|list|delete <id>).", TIPEDIT_MIN_LEVEL },
    { "transfer", cmd_transfer, "Teleport someone to you, or to a room (transfer <name> [vnum]).", IMMORTAL_LEVEL_MIN },
    { "force",   cmd_force,   "Make a player or mob run a command as themselves (force <target> <command>).", FORCE_MIN_LEVEL },
    { "mudstats", cmd_mudstats, "Show basic statistics about the game world.",      MORTAL_LEVEL_MIN },
    /* SWAP: news before newbie, so "new" still means news; newbie needs
     * "newb". ("n"/"ne" are movement -- head above.) */
    { "news",    cmd_news,    "Read the latest game news (news [10|20|50|100]).",   MORTAL_LEVEL_MIN },
    { "equipment", cmd_equipment, "List what you're wearing and holding.",          MORTAL_LEVEL_MIN },
    { "buy",     cmd_buy,     "Buy an item from a shopkeeper (buy <item>|<#> -- see list).", MORTAL_LEVEL_MIN },
    { "sell",    cmd_sell,    "Sell a carried item to a shopkeeper (sell <item>).", MORTAL_LEVEL_MIN },
    { "list",    cmd_list,    "List a shopkeeper's wares, if you're at a shop.",    MORTAL_LEVEL_MIN },
    /* SWAP: drop before drink -- "dr" is far likelier to mean drop;
     * drink needs "dri". */
    { "drop",    cmd_drop,    "Put down a carried item (drop <item>).",             MORTAL_LEVEL_MIN },
    { "goto",    cmd_goto,    "Directions to your guildmaster/the inn/the surplus store (goto guildmaster|rent|surplus), or teleport to a room by vnum (immortal).", MORTAL_LEVEL_MIN },
    /* "wea" is already claimed by "wear" (sits first) -- weather needs
     * "weat" to diverge from it, same spirit as the drop/drink swap
     * elsewhere in this table. */
    { "weather", cmd_weather, "Check the current sky and whether it's day or night.", MORTAL_LEVEL_MIN },
    { "rules",   cmd_rules,   "Read the game rules (rules, or rules <number>).",    MORTAL_LEVEL_MIN },
    /* Bulletin boards (user 2026-07-18). Placed after the whole r-block
     * above (not in strict alpha order, which would put it before "rest")
     * so "r"/"re" keep meaning rest, per that block's own SWAP -- "read"/
     * "rea" stay unambiguous regardless, since nothing else starts "rea". */
    { "read",    cmd_read,    "Read the messages on a bulletin board (read [<#>]).", MORTAL_LEVEL_MIN },
    /* Liquids (user 2026-07-26): "pou" reaches this fine -- nothing else in
     * the table needs a prefix that short ("poofin"/"poofout" already need
     * "poof", "possess" needs "pos", both longer than "pou" would ever
     * collide with). */
    { "pour",    cmd_pour,    "Empty a drink container onto the ground, or into another one (pour <container> [<container2>]).", MORTAL_LEVEL_MIN },
    { "remove",  cmd_remove,  "Take off a worn or held item (remove <item>).",      MORTAL_LEVEL_MIN },
    { "save",    cmd_save,    "Save your character now.",                           MORTAL_LEVEL_MIN },
    /* SWAP: hit before help, so "h" stays a combat verb; help needs "he",
     * hold needs "ho". */
    { "hit",     cmd_hit,     "Attack a player or mobile via real combat, even for immortals (never instakill).", MORTAL_LEVEL_MIN },
    /* SWAP: exits before examine, so "ex" lists exits; examine needs "exa".
     * ("e" is east -- movement head.) */
    { "exits",   cmd_exits,   "List this room's exits and where they lead.",        MORTAL_LEVEL_MIN },
    { "give",    cmd_give,    "Hand a carried item, or gold, to someone else here (give <item>|<amount> gold <person>).", MORTAL_LEVEL_MIN },
    { "stat",    cmd_stat,    "See every field of an obj/mob/room prototype, or a player (stat <obj|mob|room> <vnum> | stat player <name>).", STAT_MIN_LEVEL },
    { "tickets", cmd_tickets, "List your pending claim tickets at a repair shop.",  MORTAL_LEVEL_MIN },
    { "time",    cmd_time,    "Show the current mud clock, weekday, and date.",     MORTAL_LEVEL_MIN },
    { "gametog", cmd_gametog, "View or flip global game-wide switches (58+).",      GAMETOG_MIN_LEVEL },
    { "hurtlimb", cmd_hurtlimb, "Debug: set a target's limb HP directly (hurtlimb <target> <limb> <hp>).", IMMORTAL_LEVEL_MIN },
    { "restore", cmd_restore, "Fully heal an online target and clear their spell affects (restore <target>).", IMMORTAL_LEVEL_MIN },
    /* "load" is a full prefix of "loadroom", so it MUST stay ahead of it
     * and wins every shared abbreviation up to the exact word "load" --
     * loadroom needs "loadr" (5 letters). Alphabetical order delivers this
     * for free, but it is load-bearing, not incidental. Replaces the old
     * separate mload/oload (user 2026-07-09: one command, category as the
     * first argument). No zone-reset system executes yet, so a room-floor
     * object/mob placed this way doesn't survive a restart. */
    { "load",    cmd_load,    "Spawn a mob or object prototype into your room (load <mob|obj> <vnum|name>).", BUILD_MIN_LEVEL },
    { "poofin",  cmd_poofin,  "Set your custom walking arrival message (poofin [msg]).",   IMMORTAL_LEVEL_MIN },
    { "poofout", cmd_poofout, "Set your custom walking departure message (poofout [msg]).", IMMORTAL_LEVEL_MIN },
    { "possess", cmd_possess, "Puppet a mob's body (possess <mob>; `return` to come back).", POSSESS_MIN_LEVEL },
    { "promote", cmd_promote, "Set a player's level (up to your own).",             PROMOTE_MIN_LEVEL },
    /* Bare `purge` clears the room; `purge linkdead` (58+, checked inside
     * cmd_purge itself) sweeps the whole game. */
    { "purge",   cmd_purge,   "Clear this room's mobs/objects, purge linkdead (58+), or purge a vnum range (59+).", PURGE_MIN_LEVEL },
    /* "shu" is already unambiguous ("sh"/"sho" still resolve to shout,
     * above) -- no collision to guard against. Implementor-only (60),
     * same tier as `exec`: ends the whole process. */
    { "shutdown", cmd_shutdown, "End the game gracefully, now or in N seconds (shutdown [seconds|-now|cancel|abort]).", SHUTDOWN_MIN_LEVEL },
    { "whisper", cmd_whisper, "Send a private message to someone in the room (whisper <name> <message>).", MORTAL_LEVEL_MIN },
    /* Whittle profession (TODO.md "Deferred decisions" -- the second
     * `task/` profession alongside cook). Placed after "who"/"whisper"
     * so "wh" still abbreviates to who, matching cook's own precedent of
     * living wherever it was added rather than at a strict Sneezy-order
     * slot. */
    { "whittle", cmd_whittle, "Carve a wooden item from carried wood logs (whittle <item>).", MORTAL_LEVEL_MIN },
    /* Mortal-level: the pager (e.g. `news`) holds messages for anyone, not
     * just immortals mid-editor, so anyone can have something to catch up
     * on. */
    { "catchup", cmd_catchup, "Replay game messages missed while editing or paging.", MORTAL_LEVEL_MIN },
    /* Shares the "cat" prefix with catchup above -- catchup (listed
     * first) keeps "cat"/"catc", catleap needs "catl" to disambiguate;
     * both are 4+ characters when actually typed in practice, so this
     * doesn't cost either command a real-world abbreviation. */
    { "catleap", cmd_catleap, "Leap and glide a direction, out of combat (Monk, catleap <direction>).", MORTAL_LEVEL_MIN },
    { "cast",    cmd_cast,    "Cast a spell (Mage/Druid) -- requires a component.", MORTAL_LEVEL_MIN },
    { "sip",     cmd_sip,     "Taste a bit of a puddle, fountain, or carried container, low risk (sip <target>).", MORTAL_LEVEL_MIN },
    { "snoop",   cmd_snoop,   "Watch what a lower-level player sees and types.",    SNOOP_MIN_LEVEL },
    { "follow",  cmd_follow,  "Start following someone (follow <name>); `stop` to break it.", MORTAL_LEVEL_MIN },
    { "rent",    cmd_rent,    "Store your belongings and leave the game safely.",   MORTAL_LEVEL_MIN },
    { "open",    cmd_open,    "Open a door (open <direction>).",                    MORTAL_LEVEL_MIN },
    /* SWAP: close before cast/catchup/color, so "c" still closes doors.
     * SWAP: catchup before cast, so "ca" reaches catchup; cast needs "cas".
     * color needs "co". (Older comments here claimed color owned "c" -- it
     * never did once close was added ahead of it.) */
    { "close",   cmd_close,   "Close a door (close <direction>).",                  MORTAL_LEVEL_MIN },
    { "lock",    cmd_lock,    "Lock a closed door or container (lock <direction|container>).", MORTAL_LEVEL_MIN },
    { "typo",    cmd_typo,    "Report a typo/text problem (typo <text>); immortals list them.", MORTAL_LEVEL_MIN },
    { "unignore", cmd_unignore, "Stop blocking someone's tells/whispers (unignore <name>).", MORTAL_LEVEL_MIN },
    { "unlock",  cmd_unlock,  "Unlock a locked door or container (unlock <direction|container>).", MORTAL_LEVEL_MIN },
    { "flee",    cmd_flee,    "Try to escape a fight through a random exit.",       MORTAL_LEVEL_MIN },
    /* Bulletin boards (user 2026-07-18) -- "wr" is already unambiguous
     * (nothing else starts "wr"). */
    { "write",   cmd_write,   "Post a message on a bulletin board (write <subject> <message>).", MORTAL_LEVEL_MIN },
    /* Non-weapon holdables only; a weapon refuses hold and points to wield. */
    { "hold",    cmd_hold,    "Hold a non-weapon item in a free hand (hold <item>).", MORTAL_LEVEL_MIN },
    /* Same audit batch, same clustering precedent as backstab above --
     * none of these four collide with an existing short abbreviation
     * ("sn"/"gr"/"be"/"ra" are all otherwise unclaimed). */
    { "sneak",   cmd_sneak,   "Toggle moving quietly, suppressing your own arrival/departure notices (Thief/Warrior).", MORTAL_LEVEL_MIN },
    /* Spell/skill functional-completeness audit (2026-07-27): the first
     * batch of level-1 roster entries that previously had NO handler at
     * all (fell through to "Command not found"). Grouped here with bash
     * rather than split to each one's strict alphabetical slot, same
     * "skill-combat commands cluster near bash" precedent bash's own
     * placement above bank/bug already establishes. "back"+ is
     * unambiguous against bash/bank/bug/butcher at 4 chars; rescue and
     * trip are listed in their own alphabetical R/T comments below since
     * they DO collide with existing short abbreviations there. */
    { "backstab", cmd_backstab, "A devastating opening sneak attack (Thief, backstab <target>; only works to start a fight).", MORTAL_LEVEL_MIN },
    /* Spell/skill functional-completeness audit (2026-07-27). "ste" is
     * unambiguous against stand's "sta" and stop's "sto" at 3 chars, so
     * no ordering conflict with either despite the shared "st" prefix. */
    { "steal",   cmd_steal,   "Steal gold or an item from someone (Thief, steal gold|<item> <target>).", MORTAL_LEVEL_MIN },
    /* Skill-based combat (Sneezy → Tobin feature audit, Warrior). Extra
     * action layered on the automatic per-round exchange -- see
     * cmd_bash.c's own header comment. */
    { "bash",    cmd_bash,    "Bash your opponent, knocking them down (Warrior, must be fighting them).", MORTAL_LEVEL_MIN },
    /* Spell/skill functional-completeness audit (2026-07-27): "tr" is free
     * for mortals (transfer/treasury below are IMMORTAL_LEVEL_MIN, so the
     * dispatch loop's level filter skips them for a mortal caller
     * entirely -- see cmd_dispatch()'s own min_level-skip comment). */
    { "trip",    cmd_trip,    "Knock your opponent to the ground (Warrior, must be fighting them).", MORTAL_LEVEL_MIN },
    /* Spell/skill functional-completeness audit (2026-07-27). "res" alone
     * still reaches "rest" (this block's own SWAP note above) -- rescue
     * needs "resc"+ typed to disambiguate, an acceptable cost for a
     * deliberate, planned action rather than a reflexive one (same
     * "disarm needs a longer prefix than disarmtrap expected" precedent
     * cmd_disarm.c's own table comment sets). */
    { "rescue",  cmd_rescue,  "Swap places with an ally in combat, pulling their attacker onto you (Warrior, rescue <ally>).", MORTAL_LEVEL_MIN },
    /* Skill-based combat (Sneezy → Tobin feature audit, Thief/Monk). */
    { "kick",    cmd_kick,    "Kick for bonus damage -- kick <target> also starts a fight (Warrior/Thief/Monk).", MORTAL_LEVEL_MIN },
    { "stomp",   cmd_stomp,   "A crushing stomp attack (Warrior, must be fighting them).", MORTAL_LEVEL_MIN },
    { "evaluate", cmd_evaluate, "Appraise an item's worth, condition, and material (evaluate <item>).", MORTAL_LEVEL_MIN },
    /* SWAP: pray before practice, so "p"/"pr" stay a Cleric's spell verb;
     * practice needs "prac", prompt needs "pro", put needs "pu". */
    { "pray",    cmd_pray,    "Pray for a spell (Cleric) -- requires a holy symbol.", MORTAL_LEVEL_MIN },
    { "quest",   cmd_quest,   "See your current quests (quest [<name>]).",          MORTAL_LEVEL_MIN },
    { "questdef", cmd_questdef, "Write/replace a quest stage's description (questdef <name> <stage> <text>).", BUILD_MIN_LEVEL },
    { "uptime",  cmd_uptime,  "Show how long the server has been running since the last boot/copyover.", MORTAL_LEVEL_MIN },
    /* Must precede "users" (below, immortal tier) in table order -- both
     * start with "use", and dispatch's strncmp scan takes the first match,
     * so typing "use" exactly has to hit this entry, not fall through to
     * "users" for an immortal typing it in full. */
    { "use",     cmd_use,     "Use a scroll, wand, or staff (use <item> [target]).", MORTAL_LEVEL_MIN },
    { "dismiss", cmd_dismiss, "Release a charmed pet early, before its bond fades on its own.", MORTAL_LEVEL_MIN },
    { "pee",     cmd_pee,     "Leave a puddle on the floor (pee <liquid> for a specific type).", IMMORTAL_LEVEL_MIN },
    /* Spell/skill functional-completeness audit continued, level 22. */
    { "taunt",   cmd_taunt,   "Provoke a mob into attacking you instead of whoever it's fighting (Warrior, taunt <target>).", MORTAL_LEVEL_MIN },
    { "wiznet",  cmd_wiznet,  "Broadcast to online immortals (wiznet [@<level>] <msg>).", IMMORTAL_LEVEL_MIN },
    { "consider", cmd_consider, "Size up a fight before you start one (consider <target>|self).", MORTAL_LEVEL_MIN },
    /* MORTAL_LEVEL_MIN, not immortal-only (2026-07-26, Transformation):
     * `cast polymorph` sets the exact same d->possess_original swap
     * `possess` does, so an ordinary mage needs to be able to run
     * `return` to end it early -- `possess` itself stays immortal-only
     * (POSSESS_MIN_LEVEL above), only this shared "come back" half is
     * now reachable by anyone. A mortal typing `return` while not
     * possessing/polymorphed into anything just gets cmd_return's own
     * "You aren't possessing anything." -- harmless, same shape as
     * `stop` showing up for someone not currently following anyone. */
    { "return",  cmd_return,  "Come back to your own body after `possess`ing or polymorphing.", MORTAL_LEVEL_MIN },
    { "submit",  cmd_submit,  "Hand a damaged item to a repair shop for a claim ticket (submit <item>).", MORTAL_LEVEL_MIN },
    /* "sw" is southwest's alias (movement head); switch needs "swi". */
    { "switch",  cmd_switch,  "Swap what's in your primary and secondary hold.",    MORTAL_LEVEL_MIN },
    { "treasury", cmd_treasury, "See how much gold the crown has collected in sales tax.", IMMORTAL_LEVEL_MIN },
    { "users",   cmd_users,   "List all connections with IPs and states.",          USERS_MIN_LEVEL },
    /* Hidden from mortals entirely (Tier 3): players only ever see help for
     * what they can use. */
    { "wizhelp", cmd_wizhelp, "List immortal-only commands.",                       IMMORTAL_LEVEL_MIN },
    { "extinguish", cmd_extinguish, "Put out a light source (extinguish <item> [held|room]).", MORTAL_LEVEL_MIN },

    /* ==================== IMMORTAL-ONLY COMMANDS ==================== */
    /* Alphabetical throughout, with one swap (wiznews/wizhelp, below).
     * Two MORTAL_LEVEL_MIN Thief skills live down here rather than in the
     * mortal tier -- `settrap` and `disarmtrap`; see settrap's note. */

    { "aitick",  cmd_aitick,  "Debug: force N mob AI ticks right now (aitick [count]).", IMMORTAL_LEVEL_MIN },
    { "balance", cmd_balance, "Adjust gamewide class/race balance modifiers (balance class|race <name>).", BALANCE_MIN_LEVEL },
    /* Must follow "stat" above -- cmd_dispatch()'s "first name that
     * STARTS WITH what was typed" rule means "stats" listed first would
     * hijack the plain "stat" abbreviation. */
    { "stats",   cmd_stats,   "Aggregate world statistics: rooms/mobiles/objects/accounts/characters, live from the DB.", STAT_MIN_LEVEL },
    { "system",  cmd_system,  "Broadcast an atmosphere line to everyone.",          IMMORTAL_LEVEL_MIN },
    { "edbug",   cmd_edbug,   "Resolve a bug report in place (edbug <id> [note]).", EDBUG_MIN_LEVEL },
    /* Unified editor dispatcher (user 2026-07-11: "unify all ed* commands
     * into one edit command"): `edit room [vnum]`, `edit zone <num>`,
     * `edit player <name>`, `edit help <name>`, `edit news`, `edit
     * wiznews`, `edit rules <n> <title>` all replace their old standalone
     * ed* verbs. Gated at BUILD_MIN_LEVEL (the lowest of any sub-editor);
     * nouns needing more (player 58+, help/news/wiznews 56+, rules 59+)
     * check that internally, in cmd_edit.c. */
    { "edit",    cmd_edit,    "Edit a room/zone/object/player/account/help/news/wiznews/rules/trigger (edit <noun> ...).", BUILD_MIN_LEVEL },
    { "tips",    cmd_tips,    "Show a random gameplay tip.",                        MORTAL_LEVEL_MIN },
    { "title",   cmd_title,   "Set the title shown after your name in who.",        MORTAL_LEVEL_MIN },
    { "assist",  cmd_assist,  "Join a groupmate's fight, attacking whoever they're fighting (assist <groupmate>).", MORTAL_LEVEL_MIN },
    { "refuel",  cmd_refuel,  "Refuel a light source from a fuel item (refuel <light> <fuel> [held|room]).", MORTAL_LEVEL_MIN },
    { "show",    cmd_show,    "Show a carried item to someone in the room (show <item> <person>).", MORTAL_LEVEL_MIN },
    /* Level-5+ list, spell/skill functional-completeness audit
     * (2026-07-27): first Warrior entry off that list. No existing
     * "bo*" command -- unclaimed abbreviation. */
    { "bodyslam", cmd_bodyslam, "Lift and slam your opponent down for real damage (Warrior, bodyslam <target>).", MORTAL_LEVEL_MIN },
    /* "s"/"so" are south (movement head); socials needs "soc". */
    { "socials", cmd_socials, "List the socials you can use (smile, wave, ...).",   MORTAL_LEVEL_MIN },
    { "spin",    cmd_spin,    "A spinning grapple-style strike that needs a free hand (Warrior, spin <target>).", MORTAL_LEVEL_MIN },
    { "switchopponents", cmd_switchopp, "Switch which opponent you're fighting (switchopponents <target>) -- not `switch`, which swaps your held items.", MORTAL_LEVEL_MIN },
    { "trance", cmd_tranceblades, "Enter a defensive trance of blades, sharpening your reflexes.", MORTAL_LEVEL_MIN },
    { "delbug",  cmd_delbug,  "Delete a handled bug report by id.",                 DELBUG_MIN_LEVEL },
    { "delidea", cmd_delidea, "Delete a handled idea by id.",                       DELIDEA_MIN_LEVEL },
    { "deltypo", cmd_deltypo, "Delete a handled typo report by id.",                DELTYPO_MIN_LEVEL },
    /* Skill-based combat (Sneezy → Tobin feature audit) -- listed BEFORE
     * disarmtrap on purpose, taking over the shared "di"/"dis" short
     * abbreviation from it: a mid-fight command benefits far more from a
     * quick-typed abbreviation than a deliberate, planned trap-disarming
     * utility action does. Exact "disarm" (6 chars) matches THIS entry,
     * not disarmtrap -- strncmp() prefix matching means "disarmtrap"
     * would also match a 6-char "disarm" query, so table ORDER is what
     * decides the tie; disarmtrap now needs "disarmt"+ (or its full
     * name) to reach specifically. */
    { "disarm",  cmd_disarm,  "Attempt to knock the weapon from your opponent's grip (must be fighting them).", MORTAL_LEVEL_MIN },
    /* Liquids (user 2026-07-26): "fi" is unambiguous -- nothing else in the
     * table starts with "f" and "i" (flee/follow/forage all diverge at the
     * 2nd letter already). */
    { "fill",    cmd_fill,    "Fill a container from a fountain or puddle (fill <container>).", MORTAL_LEVEL_MIN },
    /* "sh"/"sho" are already claimed by shout above -- "shov" is
     * shove's own shortest unambiguous abbreviation, still short in
     * practice. Placed AFTER shout/show/shutdown, not near berserk/
     * rally (its fellow level-1-audit Warrior skills) -- table order
     * decides abbreviation ownership here, and shout's own comment
     * above already depends on staying first at "sho". */
    { "shove",   cmd_shove,   "Push an opponent through an exit, knocking them off balance (Warrior, shove <target> <direction>).", MORTAL_LEVEL_MIN },
    { "level",   cmd_level,   "Show your experience and how much more you need to level.", MORTAL_LEVEL_MIN },
    { "scan",    cmd_scan,    "Peer several rooms down each exit (scan [dir|name]).", MORTAL_LEVEL_MIN },
    { "toggle",  cmd_toggle,  "View or flip on/off switches (color, hp, ...).",     MORTAL_LEVEL_MIN },
    { "gtell",   cmd_gtell,   "Send a message to everyone in your group, wherever they are (gtell <message>, or the 'gt' abbreviation).", MORTAL_LEVEL_MIN },
    { "loadroom", cmd_loadroom, "Set the room your character logs in at.",          IMMORTAL_LEVEL_MIN },
    { "loadsuit", cmd_loadsuit, "Load a named equipment suit onto yourself or a target (loadsuit <suit name> [target]).", LOADSUIT_MIN_LEVEL },
    { "log",     cmd_log,     "Read, search, list, or rotate the game logs.",       LOG_MIN_LEVEL },
    /* "wip" is already unambiguous ("wi" is shared with wield/wiznews/
     * wizhelp/wiznet, all resolved by their own longer prefixes). The
     * most destructive command short of `shutdown` -- permanently erases
     * a character or account. */
    { "wipe",    cmd_wipe,    "Permanently erase a character or account (wipe <name>|account <name> <password>).", WIPE_MIN_LEVEL },
    { "stop",    cmd_stop,    "Stop following whoever you're following.",           MORTAL_LEVEL_MIN },
    { "split",   cmd_split,   "Split gold evenly among your grouped members present (split <amount>).", MORTAL_LEVEL_MIN },
    /* "bamfin"/"bamfout" set `goto`'s custom teleport messages; "poofin"/
     * "poofout" (their old name, before a same-session rename to "bamf*"
     * and back) set the WALKING move messages -- see cmd_bamf.c/cmd_poof.c.
     * "bamfi"/"bamfo" and "poofi"/"poofo" are each other's shortest safe
     * abbreviations. */
    { "bamfin",  cmd_bamfin,  "Set your custom `goto` arrival message (bamfin [msg]).",    IMMORTAL_LEVEL_MIN },
    { "bamfout", cmd_bamfout, "Set your custom `goto` departure message (bamfout [msg]).", IMMORTAL_LEVEL_MIN },
    { "copyover", cmd_copyover, "Reboot the server in place; nobody is disconnected (copyover [seconds|-now|cancel|abort]).", COPYOVER_MIN_LEVEL },
    { "deathstroke", cmd_deathstroke, "A heavy, finishing-style attack against a single target (Warrior, deathstroke <target>).", MORTAL_LEVEL_MIN },
    /* Must come AFTER sleep -- "sl" is already sleep's abbreviation;
     * slam is only reachable via its own "sla"+ prefix. */
    { "slam",    cmd_slam,    "A heavy shield-and-body slam for considerable extra damage (Warrior, slam <target>).", MORTAL_LEVEL_MIN },
    { "whirlwind", cmd_whirlwind, "A spinning attack that can strike every mob in the room (Warrior, must be fighting).", MORTAL_LEVEL_MIN },
    { "rally",   cmd_rally,   "Let out a battlecry that boosts nearby allies' combat prowess (Warrior).", MORTAL_LEVEL_MIN },
    /* Placed after look/limbs/list (not strict alphabetical, which would
     * put it before limbs) so "l"/"li" keep meaning look/limbs exactly as
     * before -- "lig" is already unambiguous either way. */
    { "light",   cmd_light,   "Light a light source (light <item> [held|room]).",  MORTAL_LEVEL_MIN },
    /* Mortal Thief skill (gated internally by being_knows_skill(), same
     * pattern as settrap/disarmtrap), but placed here rather than
     * alphabetically near pray/practice/prompt: "peek" starts with "pee",
     * and cmd_dispatch() resolves a shared abbreviation to whichever entry
     * comes FIRST -- an immortal (who, unlike a mortal, can reach both)
     * typing bare "pee" must keep meaning the pee command above, not this
     * one. Mortals never see "pee" at all, so this costs them nothing;
     * "peek" itself still works fine typed in full either way. */
    { "peek",    cmd_peek,    "Attempt to see what someone is carrying, without their knowledge (Thief, peek <target>).", MORTAL_LEVEL_MIN },
    { "color",   cmd_color,   "Toggle ANSI color rendering on or off.",             MORTAL_LEVEL_MIN },
    { "email",   cmd_email,   "View or set your account email address (never shared -- MUD communications only).", MORTAL_LEVEL_MIN },
    { "mailinglist", cmd_mailinglist, "Export every opted-in account email address to a file in logs/ for mass-email use.", 60 },
    /* Must come AFTER hit/help/hold above -- "h"/"he"/"ho" are already
     * spoken for by those three (matching is first-match-in-table-order
     * by prefix, cmd_dispatch() below), so headbutt is only reachable via
     * its own full 4-letter "head" prefix, which nothing else in the
     * table starts with. */
    { "headbutt", cmd_headbutt, "Slam your head into an opponent for real damage (Warrior, headbutt <target>).", MORTAL_LEVEL_MIN },
    { "stabbing", cmd_stabbing, "A piercing melee attack against your opponent (Thief, must be fighting).", MORTAL_LEVEL_MIN },
    { "subterfuge", cmd_subterfuge, "Redirect your mob opponent's aggression onto someone else (subterfuge <target>).", MORTAL_LEVEL_MIN },
    { "examine", cmd_examine, "Look at something in detail -- a synonym for look <target>.", MORTAL_LEVEL_MIN },
    { "grapple", cmd_grapple, "Grab and hold your opponent, locking you both down a while (Warrior, must be fighting them).", MORTAL_LEVEL_MIN },
    { "springleap", cmd_springleap, "Spring instantly from sitting or resting to standing (Monk).", MORTAL_LEVEL_MIN },
    /* Placed AFTER sit/sip (above), never before -- "si" is deliberately
     * reserved for sit (see its own comment); "sig" is already
     * unambiguous (no other command starts with it). */
    { "sign",    cmd_sign,    "Communicate silently with hand signals (sign <message>).", MORTAL_LEVEL_MIN },
    { "vnum",    cmd_vnum,    "List vnums of rooms/objs/mobs by name (vnum <room|obj|mob> <pat>).", BUILD_MIN_LEVEL },
    /* SWAP: wiznews before wizhelp/wiznet -- the immortal tier's only
     * non-alphabetical placement. "wiz" reaches wiznews; wizhelp needs
     * "wizh" and wiznet must be typed in full ("wizne" is ambiguous with
     * wiznews, which wins it). Posting to either channel is `edit wiznews`,
     * folded into the edit dispatcher above. */
    { "wiznews", cmd_wiznews, "Read the immortal news channel.",                    IMMORTAL_LEVEL_MIN },
    { "repair",  cmd_repair,  "Mend a damaged item yourself (Warrior, repair <item>).", MORTAL_LEVEL_MIN },
    { "debride", cmd_debride, "Undo some of an item's accumulated wear (Warrior, debride <item>).", MORTAL_LEVEL_MIN },
    /* Full spell/skill/prayer roster import (user 2026-07-26): "sac" is
     * unambiguous -- say already owns "sa", save "sav". */
    { "sacrifice", cmd_sacrifice, "Ritually sacrifice a corpse to the loa (Druid, sacrifice <corpse>).", MORTAL_LEVEL_MIN },
    { "prompt",  cmd_prompt,  "Customize your prompt (prompt hp|gold).",            MORTAL_LEVEL_MIN },
    { "affects", cmd_affects, "List your currently active buffs/debuffs.",          MORTAL_LEVEL_MIN },
    { "alias",   cmd_alias,   "Manage your command aliases (alias [<name> [<expansion>]] | alias remove <name>).", MORTAL_LEVEL_MIN },
    { "mount",   cmd_ride,    "Mount a rideable creature -- alias of ride.",        MORTAL_LEVEL_MIN },
    { "dismount", cmd_dismount, "Get off your mount.",                             MORTAL_LEVEL_MIN },
    /* Mortality toggle, other half: `mortal` sets aside true immortal rank
     * to walk the world as an ordinary player for testing -- `immort`
     * (mortal tier, above) reclaims it. Registered at IMMORTAL_LEVEL_MIN;
     * gates further on the STORED true level internally -- see
     * cmd_mortal.c. */
    { "mortal",  cmd_mortal,  "Walk the world as a mortal (immort to return).",     IMMORTAL_LEVEL_MIN },
    { "multiplay", cmd_multiplay, "Toggle whether mortals may multiplay (59+).",    MULTIPLAY_MIN_LEVEL },
    /* Full names only recommended -- "mu" alone is already ambiguous
     * between mudstats/multiplay/mute, first table match wins. */
    { "mute",    cmd_mute,    "Block a misbehaving player's tell/shout (mute <player>).", IMMORTAL_LEVEL_MIN },
    { "unmute",  cmd_unmute,  "Lift a mute (unmute <player>).",                     IMMORTAL_LEVEL_MIN },
    /* Mount/riding system (Sneezy → Tobin feature audit). Listed AFTER
     * "mortal" above on purpose -- "mortal" already owns the "mo"
     * abbreviation for immortals (both are reachable to them, and
     * "mortal" is the established muscle-memory one); "mount" still
     * works fully spelled, or via "mou". "ride" is the primary name
     * (no prefix conflict, nothing else starts "ri"); "mount" is a full
     * alias of it, same alias-row precedent as "engage"/hit above. */
    { "ride",    cmd_ride,    "Mount a rideable creature (ride <target>).",         MORTAL_LEVEL_MIN },

    /* -- Everything else, alphabetical (rule 3). ---------------------- */

    /* attack/kill are FULL aliases (user spec): one handler, both instakill
     * for immortals, both normal combat for mortals. Alphabetical order
     * splits the pair -- `kill` is further down, under K. */
    /* SWAP: attack before affects, so "a" still attacks mid-fight rather
     * than printing a buff list. affects needs "af". */
    { "attack",  cmd_kill,    "Attack a player or mobile (instant slay for immortals).", MORTAL_LEVEL_MIN },
    { "kill",    cmd_kill,    "Attack a player or mobile (instant slay for immortals).", MORTAL_LEVEL_MIN },
    { "limbs",   cmd_limbs,   "Show the current health of all your limbs.",         MORTAL_LEVEL_MIN },
    { "practice", cmd_practice, "Train your Basic/Advanced discipline with a guildmaster (practice basic|advanced).", MORTAL_LEVEL_MIN },
    /* "set" must stay ahead of "setsev"/"settrap" -- all three start with
     * "set" and the first match wins, so the exact command "set" would
     * otherwise dispatch into one of the others (found via
     * smoke_test_alignment.py: `set <name> alignment 500` was landing on
     * settrap's "Usage: settrap <direction>"). Alphabetical order delivers
     * set < setsev < settrap for free, but it is load-bearing.
     *
     * Note the level gate interacts here: at 58+ "set" reaches cmd_set; a
     * 51-57 immortal can't see set (58) so "set" reaches setsev; a mortal
     * sees neither and "set" reaches settrap. */
    { "set",     cmd_set,     "Set one field on a player (set <name> <field> <value>).", SET_MIN_LEVEL },
    /* A MORTAL Thief skill that lives in the immortal tier on purpose:
     * "set" is a literal prefix of "settrap", so if settrap moved up into
     * the mortal tier (where its level says it belongs) the tier rule would
     * put it ahead of `set` and silently reintroduce the exact collision
     * described above. disarmtrap keeps it company for symmetry. */
    { "settrap", cmd_settrap, "Rig a trap on a closed door (Thief, settrap <direction>).", MORTAL_LEVEL_MIN },
    { "setsev",  cmd_setsev,  "View or flip which log types echo to you.",          IMMORTAL_LEVEL_MIN },
    { "feigndeath", cmd_feigndeath, "Play dead to avoid an aggressive mob's attention.", MORTAL_LEVEL_MIN },
    { "berserk", cmd_berserk, "Fly into a berserk rage -- unparryable, but you can't be rescued either (Warrior).", MORTAL_LEVEL_MIN },
    { "bandage", cmd_bandage, "Treat a bleeding limb using a carried bandage (bandage [target]).", MORTAL_LEVEL_MIN },
    { "exec",    cmd_exec,    "Run a shell command on the host box (Implementor).", EXEC_MIN_LEVEL },
    { "kneestrike", cmd_kneestrike, "A knee strike against your opponent (Warrior, must be fighting, can't be crawling).", MORTAL_LEVEL_MIN },
    /* Mortal Thief skill (see settrap's note); needs "disarmt"+ now that
     * combat `disarm` (above) owns the shared "di"/"dis" abbreviation. */
    { "disarmtrap", cmd_disarmtrap, "Safely remove a trap from a door (Thief, disarmtrap <direction>).", MORTAL_LEVEL_MIN },
    /* Needs "disg"+ to reach -- "disarm"/"disarmtrap" above already own
     * the shared "di"/"dis" prefix, same abbreviation-ownership shape as
     * disarmtrap's own note just above. */
    { "disguise", cmd_disguise, "Alter your apparent identity (Thief, toggle).", MORTAL_LEVEL_MIN },
    { "garrotte", cmd_garrotte, "Strangle an unaware victim with a cord (Thief, garrotte <target>; only works to start a fight).", MORTAL_LEVEL_MIN },
    { "hurl", cmd_hurl, "Throw your mob opponent bodily out of the room (Monk, must be fighting).", MORTAL_LEVEL_MIN },
    { "reply",   cmd_reply,   "Tell whoever last told you (reply <message>).",      MORTAL_LEVEL_MIN },
    /* Mortality toggle: `immort` reclaims true immortal rank after `mortal`
     * (immortal tier, below) sets it aside. Registered at MORTAL level
     * itself (NULL help = unlisted) so a real mortal typing it still
     * matches the entry, but the handler gates on the STORED true level and
     * does nothing useful for them. */
    { "immort",  cmd_immort,  NULL,                                                 MORTAL_LEVEL_MIN },
    { "junk",    cmd_junk,    "Destroy a carried item for good, no chance of recovery (junk <item>).", MORTAL_LEVEL_MIN },
    /* Crafting & extraction (Sneezy -> Tobin feature audit, Druid). Placed
     * after "follow" so "fo" keeps reaching the far more frequently typed
     * `follow` -- "forage" needs "for" to disambiguate. */
    { "forage",  cmd_forage,  "Gather a bit of wild food from the terrain (Druid).", MORTAL_LEVEL_MIN },
    { "chop", cmd_chop, "An edge-of-hand strike against your opponent (Monk, must be fighting).", MORTAL_LEVEL_MIN },
    /* No existing "ma*" command -- unclaimed abbreviation. */
    { "materialize", cmd_materialize, "Conjure a named item out of thin air, for a price (Mage, materialize <item>).", MORTAL_LEVEL_MIN },
    /* Money system v2. Placed AFTER "bash" (not strict alphabetical
     * order, same "ret"/"retu" precedent as retrieve/return) so the
     * far-more-frequently-typed combat skill keeps ownership of the "ba"
     * abbreviation -- "bank" only needs "ban" to disambiguate. */
    { "bank",    cmd_bank,    "Deposit/withdraw at a bank, or check your balance (bank [deposit|withdraw <amt>]).", MORTAL_LEVEL_MIN },
    { "bug",     cmd_bug,     "Report a bug (bug <text>); immortals list them.",    MORTAL_LEVEL_MIN },
    /* Crafting & extraction (Sneezy -> Tobin feature audit, Druid). */
    { "butcher", cmd_butcher, "Carve meat from a slain animal's corpse (Druid).",   MORTAL_LEVEL_MIN },
    /* Crafting & extraction (Sneezy -> Tobin feature audit, Druid). "ski"
     * is unambiguous -- no collision with "skills" ("skil"). */
    { "skin",    cmd_skin,    "Strip a hide from a slain animal's corpse (Druid).", MORTAL_LEVEL_MIN },
    /* No existing "ch*" command -- unclaimed abbreviation, confirmed via
     * cmd_abbrev_check.py before landing. */
    { "chi",     cmd_chi,     "Unleash a chi-powered strike against a foe (Monk, chi [<target>]; defaults to your current opponent).", MORTAL_LEVEL_MIN },
    /* Full alias of hit (user 2026-07-18: "add an engage command that
     * alias for hit"), same one-handler-two-table-rows pattern as
     * attack/kill above. No abbreviation conflict: nothing else in the
     * table starts with "en", and single-letter "e" is already claimed
     * by the pinned movement head (east). */
    { "engage",  cmd_hit,     "Attack a player or mobile via real combat, even for immortals (never instakill).", MORTAL_LEVEL_MIN },
    { "throatslit", cmd_throatslit, "A lethal opening throat-slitting attack (Thief, throatslit <target>; only works to start a fight).", MORTAL_LEVEL_MIN },
    { "yoginsa", cmd_yoginsa, "Meditate to recover HP and Vitality faster (Monk, sits you down automatically).", MORTAL_LEVEL_MIN },
    { "meditate", cmd_meditate, "Meditate to recover Mana faster (Mage/Druid, sits you down automatically).", MORTAL_LEVEL_MIN },
    /* SWAP: continue before consider, so "con" keeps repeating a heal
     * rather than sizing up a fight; consider needs "cons". */
    { "continue", cmd_continue, "Repeat your last heal-type prayer until the target is healed or your holy symbols run out.", MORTAL_LEVEL_MIN },
    { "egotrip", cmd_egotrip, "Immortal toy-box (blast/damn/disease/cleanse/stupidity/wander/crit -- see HELP EGOTRIP).", EGOTRIP_MIN_LEVEL },
    { "dig",     cmd_dig,     "Dig a new room in the current direction, if none exists yet (dig <direction>).", BUILD_MIN_LEVEL },
    { "smoke",   cmd_smoke,   "Smoke a carried drug item (smoke <item>).",          MORTAL_LEVEL_MIN },
    /* "zone" is a full prefix of "zonefile", so it MUST stay ahead of it --
     * a bare "zone" abbreviation must match the shorter entry first.
     * Alphabetical delivers this for free, but it is load-bearing. */
    { "zone",    cmd_zone,    "zone reset <zone>, or zone assign <zone> <bottom> <top> <builder> (55+).", BUILD_MIN_LEVEL },
    { "zonefile", cmd_zonefile, "zonefile create <zone> -- snapshot the zone's current live mobs/objects into its reset data.", BUILD_MIN_LEVEL },
    /* Two unrelated mechanics, same command name -- see cmd_plant.c's own
     * doc comment. `plant <seeds>` (anyone, outdoors) vs. `plant <item>
     * <victim>` (Thief skill, gated internally). No prefix collision:
     * nothing else in this table starts "pl". */
    { "plant",   cmd_plant,   "Sow seeds (plant <seeds>), or secretly slip an item onto someone (Thief, plant <item> <victim>).", MORTAL_LEVEL_MIN },
    /* Cook profession (user 2026-07-26): "coo" is unambiguous -- color
     * already needs "co" (comment above), so "cook" needs one more letter. */
    { "cook",    cmd_cook,    "Cook a known recipe from carried/nearby ingredients (cook <recipe>).", MORTAL_LEVEL_MIN },
    { "newbie",  cmd_newbie,  "Chat on the newbie help channel (newbie <msg>).",    MORTAL_LEVEL_MIN },
    { "idea",    cmd_idea,    "Suggest a feature (idea <text>); immortals list them.", MORTAL_LEVEL_MIN },
    /* "ide" is already claimed by idea (shorter, sits first) -- identify
     * needs "iden" to diverge from it, same spirit as the drop/drink swap
     * elsewhere in this table. */
    { "identify", cmd_identify, "Reveal an item's real stats (identify <item>).",     MORTAL_LEVEL_MIN },
    { "ignore",  cmd_ignore,  "Block tells/whispers from someone (ignore [<name>]).", MORTAL_LEVEL_MIN },
    /* Placed right after "return" (not in strict alpha order among the
     * mortal r-block above) so "ret"/"retu" keep reaching the far more
     * frequently-typed `return` -- `retrieve` (a deliberate,
     * ticket-number-driven action) is meant to be typed in full anyway. */
    { "retrieve", cmd_retrieve, "Pay for and collect a repaired item (retrieve <ticket #>).", MORTAL_LEVEL_MIN },
};
#define NUM_COMMANDS (sizeof(COMMANDS) / sizeof(COMMANDS[0]))

/* Read-only accessor for the static COMMANDS table above -- lets other
 * files (e.g. `help`/`wizhelp` building their command lists) iterate the
 * real dispatch table instead of keeping their own separate copy. */
const cmd_entry_t *cmd_table_entries(int *count) {
    *count = (int)NUM_COMMANDS;
    return COMMANDS;
}

/* Top-level entry point for every line a connected socket sends while
 * playing: strips a stray leading `@`, expands the `'`/`;` one-character
 * shorthands for say/wiznet, handles the un-abbreviatable "quit!"
 * literal, expands account-level aliases (recursing once on the
 * expansion), enforces the wait-state lag gate, then walks COMMANDS in
 * table order looking for the first entry both visible at the caller's
 * level (see the ORDERING RULES block above the table -- position IS
 * meaning here) and a prefix-match for what they typed. Falls back to
 * socials, then to "Command not found" if nothing matched. */
bool cmd_dispatch(descriptor_t *d, const char *line) {
    while (*line == ' ')
        line++;
    if (!*line)
        return true;

    /* `@set ...` (Session 43, TODO) -- a leading `@` isn't a command of its
     * own (no `@`-anything system is planned), just a habit some players
     * type before `set`. Unlike the `'` shortcut below (which replaces a
     * single character with a whole hardcoded verb, since the real verb
     * never appears), the real verb already follows the `@` here, so this
     * is a plain strip-and-fall-through into the normal parse below rather
     * than a hardcoded alias -- harmlessly covers any other stray leading
     * `@` too, not just `@set`. */
    if (*line == '@') {
        line++;
        while (*line == ' ')
            line++;
        if (!*line)
            return true;
    }

    char verb[32];
    const char *args;

    /* `'` is a one-character shorthand for `say`, with no space required
     * before the message ("'hello" says "hello", not an empty message
     * with args "hello") -- mirrors the original's special-case for this
     * in TBeing::parseCommand() (misc/parse.cc), handled before the
     * normal whitespace-delimited verb split below so it isn't mangled by
     * that split (which would otherwise treat "'hello" as one malformed
     * verb token). */
    if (*line == '\'') {
        strcpy(verb, "say");
        args = line + 1;
        while (*args == ' ')
            args++;
    } else {
        size_t i = 0;
        while (line[i] && line[i] != ' ' && i + 1 < sizeof(verb)) {
            verb[i] = (char)tolower((unsigned char)line[i]);
            i++;
        }
        verb[i] = '\0';

        args = line + i;
        while (*args == ' ')
            args++;
    }

    if (strcmp(verb, "quit!") == 0)
        return cmd_quit(d, args);

    /* Alias expansion (user 2026-07-17: "players define their own
     * aliases... stored on the account and shared across that account's
     * characters", scoped by tier). Checked after the quit! special-case
     * so "quit!" itself can never be shadowed by an alias -- it stays the
     * one guaranteed, un-redefinable escape hatch. Expands ONCE against
     * the caller's account+tier, then falls straight through to normal
     * dispatch on the EXPANDED line -- an alias can never itself be
     * re-expanded (only a real command's own verb can appear next), so a
     * two-alias naming cycle can't loop. */
    if (d->character) {
        char expansion[ALIAS_EXPANSION_LEN];
        const char *tier = being_is_immortal(d->character) ? "immortal" : "mortal";
        if (alias_repo_find(d->account.account_id, tier, verb, expansion, sizeof(expansion))) {
            char expanded[ALIAS_EXPANSION_LEN + 256];
            if (*args)
                snprintf(expanded, sizeof(expanded), "%s %s", expansion, args);
            else
                snprintf(expanded, sizeof(expanded), "%s", expansion);
            return cmd_dispatch(d, expanded);
        }
    }

    /* Wait-state gate (see pulse.h / being_get_wait()): a laggy mortal
     * can't issue any further command until their wait clears. Immortals
     * always read 0 here, so this is a no-op for them. Checked after the
     * quit! special-case so a laggy player can never get stuck unable to
     * leave. `wake` is also exempt (user, 2026-08-06: fell asleep with
     * leftover combat/skill lag still ticking down and couldn't `wake`
     * until it cleared on its own) -- the sleeping gate just below this
     * one already promises "a sleeping mortal can issue exactly one
     * verb, wake, and nothing else," and residual wait-state from
     * whatever they were doing right before falling asleep shouldn't be
     * able to override that and leave them stuck asleep a while longer. */
    if (d->character && being_get_wait(d->character) > 0 && strcmp(verb, "wake") != 0) {
        descriptor_send(d, "You are still recovering!\r\n");
        return true;
    }

    /* Sleeping gate (user 2026-08-03: "nothing is doable except wake" --
     * previously only a handful of individual commands (`look`, `attack`,
     * ...) checked POSITION_SLEEPING themselves, so most of the command
     * table was silently still usable while fast asleep). Checked here,
     * centrally, same shape as the wait-state gate just above -- a
     * sleeping mortal can issue exactly one verb, `wake`, and nothing
     * else. Immortals are fully exempt (same "no restrictions" convention
     * `being_get_wait()` already applies to lag) even if their own
     * position happens to be POSITION_SLEEPING. */
    if (d->character && d->character->position == POSITION_SLEEPING
        && !being_is_immortal(d->character) && strcmp(verb, "wake") != 0) {
        descriptor_send(d, "You can't do anything -- you're fast asleep!\r\n");
        return true;
    }

    /* min_level enforcement (Phase 2A): commands above the caller's level
     * are skipped during matching entirely, so a mortal typing "goto" gets
     * the same "Huh?!" as any nonexistent command (and "g" can never
     * abbreviate to it) -- immortal commands are invisible, not merely
     * refused, matching the original's commandInfo::minLevel dispatch gate.
     * While `possess`ing a mob (cmd_possess.c), `d->character` is the
     * PUPPET, whose seeded level could be anything (often 1) -- gate on
     * the immortal's own real level instead, or `return` itself (51+)
     * could become invisible and strand them in the mob permanently. */
    being_t *level_source = d->possess_original ? d->possess_original : d->character;
    int level = level_source ? level_source->progress.level : MORTAL_LEVEL_MIN;
    size_t verb_len = strlen(verb);
    for (size_t k = 0; k < NUM_COMMANDS; k++) {
        if (COMMANDS[k].min_level > level)
            continue;
        if (strncmp(COMMANDS[k].name, verb, verb_len) == 0)
            return COMMANDS[k].fn(d, args);
    }

    /* Socials (smile/nod/wave/...) are checked after the command table, so a
     * real command always wins -- classic DikuMUD ordering. */
    if (social_try(d, verb, args))
        return true;

    descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
    return true;
}
