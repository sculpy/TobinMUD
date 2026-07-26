/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
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
 * and moving a line silently rewires player muscle memory. Three rules,
 * applied in this order:
 *
 *   1. TIER, lowest level first (user 2026-07-12: "place immortal
 *      commands lower in the list of commands, that way the immortals are
 *      less likely to make mistakes when working on the game"). Every
 *      MORTAL_LEVEL_MIN command comes first, then every immortal-tier
 *      one, so an ambiguous abbreviation always resolves to the everyday
 *      mortal action instead of the rarer, more consequential immortal
 *      one. ("set" vs "settrap" and "get" vs "goto" were both exactly
 *      this mistake.)
 *
 *   2. MOVEMENT HEAD, pinned at the top of the mortal tier. The single
 *      letters n/e/s/w/u/d must always mean movement, so they outrank
 *      say ("s"), who ("w"), and everything else no matter how it's
 *      spelled.
 *
 *   3. ALPHABETICAL within each tier (user 2026-07-13: "sort by alphabet
 *      first then level lowest to highest" ... "leave important commands
 *      at the top") -- EXCEPT where strict alphabetical order would hand
 *      a shared abbreviation to the wrong command. Every exception is a
 *      local swap of an adjacent pair and is marked inline below with the
 *      abbreviation it protects (e.g. `say` sits just before `save` so
 *      "sa" still says). There are 16 such swaps in the mortal tier and 1
 *      in the immortal tier; every other entry is in plain alphabetical
 *      order.
 *
 * VERIFY REORDERS MECHANICALLY, NOT BY EYE. The 2026-07-13 pass that
 * alphabetized this table was driven from a script that resolves every
 * prefix of every command at every level against both the old and the new
 * order and diffs the two -- it landed with ZERO abbreviation changes.
 * Do the same for any future move. The per-entry prose here had drifted
 * out of sync with the real behavior twice before that pass (it claimed
 * "c" reached `color` and "h" reached `help`, when `close` and `hit` had
 * quietly taken both), so trust a prefix-resolution diff over any comment
 * -- including these.
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

    /* -- Everything else, alphabetical (rule 3). ---------------------- */

    /* attack/kill are FULL aliases (user spec): one handler, both instakill
     * for immortals, both normal combat for mortals. Alphabetical order
     * splits the pair -- `kill` is further down, under K. */
    /* SWAP: attack before affects, so "a" still attacks mid-fight rather
     * than printing a buff list. affects needs "af". */
    { "attack",  cmd_kill,    "Attack a player or mobile (instant slay for immortals).", MORTAL_LEVEL_MIN },
    { "affects", cmd_affects, "List your currently active buffs/debuffs.",          MORTAL_LEVEL_MIN },
    { "alias",   cmd_alias,   "Manage your command aliases (alias [<name> [<expansion>]] | alias remove <name>).", MORTAL_LEVEL_MIN },
    /* Skill-based combat (Sneezy → Tobin feature audit, Warrior). Extra
     * action layered on the automatic per-round exchange -- see
     * cmd_bash.c's own header comment. */
    { "bash",    cmd_bash,    "Bash your opponent, knocking them down (Warrior, must be fighting them).", MORTAL_LEVEL_MIN },
    /* Money system v2. Placed AFTER "bash" (not strict alphabetical
     * order, same "ret"/"retu" precedent as retrieve/return) so the
     * far-more-frequently-typed combat skill keeps ownership of the "ba"
     * abbreviation -- "bank" only needs "ban" to disambiguate. */
    { "bank",    cmd_bank,    "Deposit/withdraw at a bank, or check your balance (bank [deposit|withdraw <amt>]).", MORTAL_LEVEL_MIN },
    { "bug",     cmd_bug,     "Report a bug (bug <text>); immortals list them.",    MORTAL_LEVEL_MIN },
    /* Crafting & extraction (Sneezy -> Tobin feature audit, Druid). */
    { "butcher", cmd_butcher, "Carve meat from a slain animal's corpse (Druid).",   MORTAL_LEVEL_MIN },
    { "buy",     cmd_buy,     "Buy an item from a shopkeeper (buy <item>|<#> -- see list).", MORTAL_LEVEL_MIN },
    /* SWAP: close before cast/catchup/color, so "c" still closes doors.
     * SWAP: catchup before cast, so "ca" reaches catchup; cast needs "cas".
     * color needs "co". (Older comments here claimed color owned "c" -- it
     * never did once close was added ahead of it.) */
    { "close",   cmd_close,   "Close a door (close <direction>).",                  MORTAL_LEVEL_MIN },
    /* Mortal-level: the pager (e.g. `news`) holds messages for anyone, not
     * just immortals mid-editor, so anyone can have something to catch up
     * on. */
    { "catchup", cmd_catchup, "Replay game messages missed while editing or paging.", MORTAL_LEVEL_MIN },
    { "cast",    cmd_cast,    "Cast a spell (Mage/Druid) -- requires a component.", MORTAL_LEVEL_MIN },
    { "color",   cmd_color,   "Toggle ANSI color rendering on or off.",             MORTAL_LEVEL_MIN },
    /* Cook profession (user 2026-07-26): "coo" is unambiguous -- color
     * already needs "co" (comment above), so "cook" needs one more letter. */
    { "cook",    cmd_cook,    "Cook a known recipe from carried/nearby ingredients (cook <recipe>).", MORTAL_LEVEL_MIN },
    /* SWAP: continue before consider, so "con" keeps repeating a heal
     * rather than sizing up a fight; consider needs "cons". */
    { "continue", cmd_continue, "Repeat your last heal-type prayer until the target is healed or your holy symbols run out.", MORTAL_LEVEL_MIN },
    { "consider", cmd_consider, "Size up a fight before you start one (consider <target>|self).", MORTAL_LEVEL_MIN },
    /* SWAP: drop before drink -- "dr" is far likelier to mean drop;
     * drink needs "dri". */
    { "drop",    cmd_drop,    "Put down a carried item (drop <item>).",             MORTAL_LEVEL_MIN },
    { "drink",   cmd_drink,   "Drink from a puddle, fountain, or carried container (drink <target>).", MORTAL_LEVEL_MIN },
    /* "ea" is already claimed by "east" (pinned movement head, elsewhere in
     * this table) -- eat needs the full "eat" to reach it, same spirit as
     * the drop/drink swap just above. */
    { "eat",     cmd_eat,     "Eat a carried food item (eat <food>).",              MORTAL_LEVEL_MIN },
    /* Full alias of hit (user 2026-07-18: "add an engage command that
     * alias for hit"), same one-handler-two-table-rows pattern as
     * attack/kill above. No abbreviation conflict: nothing else in the
     * table starts with "en", and single-letter "e" is already claimed
     * by the pinned movement head (east). */
    { "engage",  cmd_hit,     "Attack a player or mobile via real combat, even for immortals (never instakill).", MORTAL_LEVEL_MIN },
    { "equipment", cmd_equipment, "List what you're wearing and holding.",          MORTAL_LEVEL_MIN },
    /* SWAP: exits before examine, so "ex" lists exits; examine needs "exa".
     * ("e" is east -- movement head.) */
    { "exits",   cmd_exits,   "List this room's exits and where they lead.",        MORTAL_LEVEL_MIN },
    { "examine", cmd_examine, "Look at something in detail -- a synonym for look <target>.", MORTAL_LEVEL_MIN },
    { "extinguish", cmd_extinguish, "Put out a light source (extinguish <item> [held|room]).", MORTAL_LEVEL_MIN },
    /* Liquids (user 2026-07-26): "fi" is unambiguous -- nothing else in the
     * table starts with "f" and "i" (flee/follow/forage all diverge at the
     * 2nd letter already). */
    { "fill",    cmd_fill,    "Fill a container from a fountain or puddle (fill <container>).", MORTAL_LEVEL_MIN },
    { "flee",    cmd_flee,    "Try to escape a fight through a random exit.",       MORTAL_LEVEL_MIN },
    { "follow",  cmd_follow,  "Start following someone (follow <name>); `stop` to break it.", MORTAL_LEVEL_MIN },
    /* Crafting & extraction (Sneezy -> Tobin feature audit, Druid). Placed
     * after "follow" so "fo" keeps reaching the far more frequently typed
     * `follow` -- "forage" needs "for" to disambiguate. */
    { "forage",  cmd_forage,  "Gather a bit of wild food from the terrain (Druid).", MORTAL_LEVEL_MIN },
    /* "g" must reach get, not goto -- alphabetical already delivers that
     * (get < goto), but it is load-bearing, not incidental: goto's landmark
     * forms (guildmaster/rent/surplus) are mortal-visible, so goto would
     * otherwise shadow get's single letter for everyone. Immortals wanting
     * goto's vnum/player teleport still have "go" -- nothing else starts
     * with "go". */
    { "get",     cmd_get,     "Pick up an item, or take one from a container (get <item> [container]).", MORTAL_LEVEL_MIN },
    { "goto",    cmd_goto,    "Directions to your guildmaster/the inn/the surplus store (goto guildmaster|rent|surplus), or teleport to a room by vnum (immortal).", MORTAL_LEVEL_MIN },
    { "group",   cmd_group,   "List/manage your group (group [<name>|all]) -- leader only to add.", MORTAL_LEVEL_MIN },
    /* SWAP: hit before help, so "h" stays a combat verb; help needs "he",
     * hold needs "ho". */
    { "hit",     cmd_hit,     "Attack a player or mobile via real combat, even for immortals (never instakill).", MORTAL_LEVEL_MIN },
    { "help",    cmd_help,    "List available commands.",                           MORTAL_LEVEL_MIN },
    /* Non-weapon holdables only; a weapon refuses hold and points to wield. */
    { "hold",    cmd_hold,    "Hold a non-weapon item in a free hand (hold <item>).", MORTAL_LEVEL_MIN },
    /* SWAP: inventory before idea/immort, so bare "i" is an inventory
     * check -- always the more useful outcome than a no-op immort attempt.
     * idea needs "id", immort needs "im". */
    { "inventory", cmd_inventory, "List what you're carrying.",                     MORTAL_LEVEL_MIN },
    { "idea",    cmd_idea,    "Suggest a feature (idea <text>); immortals list them.", MORTAL_LEVEL_MIN },
    /* "ide" is already claimed by idea (shorter, sits first) -- identify
     * needs "iden" to diverge from it, same spirit as the drop/drink swap
     * elsewhere in this table. */
    { "identify", cmd_identify, "Reveal an item's real stats (identify <item>).",     MORTAL_LEVEL_MIN },
    { "ignore",  cmd_ignore,  "Block tells/whispers from someone (ignore [<name>]).", MORTAL_LEVEL_MIN },
    /* Mortality toggle: `immort` reclaims true immortal rank after `mortal`
     * (immortal tier, below) sets it aside. Registered at MORTAL level
     * itself (NULL help = unlisted) so a real mortal typing it still
     * matches the entry, but the handler gates on the STORED true level and
     * does nothing useful for them. */
    { "immort",  cmd_immort,  NULL,                                                 MORTAL_LEVEL_MIN },
    { "junk",    cmd_junk,    "Destroy a carried item for good, no chance of recovery (junk <item>).", MORTAL_LEVEL_MIN },
    /* Skill-based combat (Sneezy → Tobin feature audit, Thief/Monk). */
    { "kick",    cmd_kick,    "Kick your opponent for bonus damage (Thief/Monk, must be fighting them).", MORTAL_LEVEL_MIN },
    { "kill",    cmd_kill,    "Attack a player or mobile (instant slay for immortals).", MORTAL_LEVEL_MIN },
    /* SWAP: look before limbs, so "l" looks; limbs needs "li". */
    { "look",    cmd_look,    "Look around the room you're in.",                    MORTAL_LEVEL_MIN },
    { "limbs",   cmd_limbs,   "Show the current health of all your limbs.",         MORTAL_LEVEL_MIN },
    { "list",    cmd_list,    "List a shopkeeper's wares, if you're at a shop.",    MORTAL_LEVEL_MIN },
    /* Placed after look/limbs/list (not strict alphabetical, which would
     * put it before limbs) so "l"/"li" keep meaning look/limbs exactly as
     * before -- "lig" is already unambiguous either way. */
    { "light",   cmd_light,   "Light a light source (light <item> [held|room]).",  MORTAL_LEVEL_MIN },
    { "lock",    cmd_lock,    "Lock a closed door or container (lock <direction|container>).", MORTAL_LEVEL_MIN },
    { "mudstats", cmd_mudstats, "Show basic statistics about the game world.",      MORTAL_LEVEL_MIN },
    /* SWAP: news before newbie, so "new" still means news; newbie needs
     * "newb". ("n"/"ne" are movement -- head above.) */
    { "news",    cmd_news,    "Read the latest game news (news [10|20|50|100]).",   MORTAL_LEVEL_MIN },
    { "newbie",  cmd_newbie,  "Chat on the newbie help channel (newbie <msg>).",    MORTAL_LEVEL_MIN },
    { "open",    cmd_open,    "Open a door (open <direction>).",                    MORTAL_LEVEL_MIN },
    /* SWAP: pray before practice, so "p"/"pr" stay a Cleric's spell verb;
     * practice needs "prac", prompt needs "pro", put needs "pu". */
    { "pray",    cmd_pray,    "Pray for a spell (Cleric) -- requires a holy symbol.", MORTAL_LEVEL_MIN },
    { "practice", cmd_practice, "Train your Basic/Advanced discipline with a guildmaster (practice basic|advanced).", MORTAL_LEVEL_MIN },
    { "prompt",  cmd_prompt,  "Customize your prompt (prompt hp|gold).",            MORTAL_LEVEL_MIN },
    { "put",     cmd_put,     "Put a carried item into a container (put <item> <container>).", MORTAL_LEVEL_MIN },
    { "quest",   cmd_quest,   "See your current quests (quest [<name>]).",          MORTAL_LEVEL_MIN },
    /* SWAP: rest before remove/rent, so "r"/"re" still rest; remove needs
     * "rem", rent needs "ren", rules needs "ru". */
    { "rest",    cmd_rest,    "Sit down and rest (heals faster).",                  MORTAL_LEVEL_MIN },
    { "remove",  cmd_remove,  "Take off a worn or held item (remove <item>).",      MORTAL_LEVEL_MIN },
    { "rent",    cmd_rent,    "Store your belongings and leave the game safely.",   MORTAL_LEVEL_MIN },
    { "repair",  cmd_repair,  "Mend a damaged item yourself (Warrior, repair <item>).", MORTAL_LEVEL_MIN },
    { "rules",   cmd_rules,   "Read the game rules (rules, or rules <number>).",    MORTAL_LEVEL_MIN },
    /* Bulletin boards (user 2026-07-18). Placed after the whole r-block
     * above (not in strict alpha order, which would put it before "rest")
     * so "r"/"re" keep meaning rest, per that block's own SWAP -- "read"/
     * "rea" stay unambiguous regardless, since nothing else starts "rea". */
    { "read",    cmd_read,    "Read the messages on a bulletin board (read [<#>]).", MORTAL_LEVEL_MIN },
    { "refuel",  cmd_refuel,  "Refuel a light source from a fuel item (refuel <light> <fuel> [held|room]).", MORTAL_LEVEL_MIN },
    /* SWAP: say before save, so "sa" still speaks; save needs "sav".
     * ("s" is south -- movement head.) */
    { "say",     cmd_say,     "Say something to everyone in the room.",             MORTAL_LEVEL_MIN },
    { "save",    cmd_save,    "Save your character now.",                           MORTAL_LEVEL_MIN },
    /* SWAP: score before scan, so "sc"/"sco" still reach score; scan needs
     * "sca". */
    { "score",   cmd_score,   "Show your character's stats, level, and HP.",        MORTAL_LEVEL_MIN },
    { "level",   cmd_level,   "Show your experience and how much more you need to level.", MORTAL_LEVEL_MIN },
    { "scan",    cmd_scan,    "Peer several rooms down each exit (scan [dir|name]).", MORTAL_LEVEL_MIN },
    { "sell",    cmd_sell,    "Sell a carried item to a shopkeeper (sell <item>).", MORTAL_LEVEL_MIN },
    /* shout before show is already alphabetical, but it is load-bearing:
     * "sho" is ambiguous and shout wins it -- "show" must be typed in
     * full. */
    { "shout",   cmd_shout,   "Shout something to everyone in the game (shout <msg>).", MORTAL_LEVEL_MIN },
    { "show",    cmd_show,    "Show a carried item to someone in the room (show <item> <person>).", MORTAL_LEVEL_MIN },
    /* "shu" is already unambiguous ("sh"/"sho" still resolve to shout,
     * above) -- no collision to guard against. Implementor-only (60),
     * same tier as `exec`: ends the whole process. */
    { "shutdown", cmd_shutdown, "End the game gracefully, now or in N seconds (shutdown [seconds|cancel]).", SHUTDOWN_MIN_LEVEL },
    /* SWAP: sit before sip, so "si" sits; sip must be typed in full. */
    { "sit",     cmd_sit,     "Sit down.",                                          MORTAL_LEVEL_MIN },
    { "sip",     cmd_sip,     "Taste a bit of a puddle, fountain, or carried container, low risk (sip <target>).", MORTAL_LEVEL_MIN },
    /* Placed AFTER sit/sip (above), never before -- "si" is deliberately
     * reserved for sit (see its own comment); "sig" is already
     * unambiguous (no other command starts with it). */
    { "sign",    cmd_sign,    "Communicate silently with hand signals (sign <message>).", MORTAL_LEVEL_MIN },
    /* Crafting & extraction (Sneezy -> Tobin feature audit, Druid). "ski"
     * is unambiguous -- no collision with "skills" ("skil"). */
    { "skin",    cmd_skin,    "Strip a hide from a slain animal's corpse (Druid).", MORTAL_LEVEL_MIN },
    { "skills",  cmd_skills,  "List your class's skills/spells, known and locked.", MORTAL_LEVEL_MIN },
    { "sleep",   cmd_sleep,   "Lie down and sleep (heals fastest).",                MORTAL_LEVEL_MIN },
    { "smoke",   cmd_smoke,   "Smoke a carried drug item (smoke <item>).",          MORTAL_LEVEL_MIN },
    /* "s"/"so" are south (movement head); socials needs "soc". */
    { "socials", cmd_socials, "List the socials you can use (smile, wave, ...).",   MORTAL_LEVEL_MIN },
    { "split",   cmd_split,   "Split gold evenly among your grouped members present (split <amount>).", MORTAL_LEVEL_MIN },
    { "stand",   cmd_stand,   "Stand up.",                                          MORTAL_LEVEL_MIN },
    { "stop",    cmd_stop,    "Stop following whoever you're following.",           MORTAL_LEVEL_MIN },
    { "submit",  cmd_submit,  "Hand a damaged item to a repair shop for a claim ticket (submit <item>).", MORTAL_LEVEL_MIN },
    /* "sw" is southwest's alias (movement head); switch needs "swi". */
    { "switch",  cmd_switch,  "Swap what's in your primary and secondary hold.",    MORTAL_LEVEL_MIN },
    { "tell",    cmd_tell,    "Send a private message to anyone playing (tell <name> <message>).", MORTAL_LEVEL_MIN },
    { "tickets", cmd_tickets, "List your pending claim tickets at a repair shop.",  MORTAL_LEVEL_MIN },
    { "time",    cmd_time,    "Show the current mud clock, weekday, and date.",     MORTAL_LEVEL_MIN },
    { "tips",    cmd_tips,    "Show a random gameplay tip.",                        MORTAL_LEVEL_MIN },
    { "title",   cmd_title,   "Set the title shown after your name in who.",        MORTAL_LEVEL_MIN },
    { "toggle",  cmd_toggle,  "View or flip on/off switches (color, hp, ...).",     MORTAL_LEVEL_MIN },
    { "unignore", cmd_unignore, "Stop blocking someone's tells/whispers (unignore <name>).", MORTAL_LEVEL_MIN },
    { "unlock",  cmd_unlock,  "Unlock a locked door or container (unlock <direction|container>).", MORTAL_LEVEL_MIN },
    /* Must precede "users" (below, immortal tier) in table order -- both
     * start with "use", and dispatch's strncmp scan takes the first match,
     * so typing "use" exactly has to hit this entry, not fall through to
     * "users" for an immortal typing it in full. */
    { "use",     cmd_use,     "Use a scroll, wand, or staff (use <item> [target]).", MORTAL_LEVEL_MIN },
    /* "w"/"we" are west (movement head); wake needs "wa", wear needs "wea",
     * wield needs "wi". */
    { "wake",    cmd_wake,    "Wake up from sleep.",                                MORTAL_LEVEL_MIN },
    /* Body-slot items only since the hold/wield split (user 2026-07-09) --
     * a holdable item refuses `wear` and points at whichever of hold/wield
     * applies. */
    { "wear",    cmd_wear,    "Put on a carried item's body slot (wear <item>).",   MORTAL_LEVEL_MIN },
    /* "wea" is already claimed by "wear" (sits first) -- weather needs
     * "weat" to diverge from it, same spirit as the drop/drink swap
     * elsewhere in this table. */
    { "weather", cmd_weather, "Check the current sky and whether it's day or night.", MORTAL_LEVEL_MIN },
    /* SWAP: who before whisper, so "wh" lists players; whisper needs
     * "whi". */
    { "who",     cmd_who,     "List everyone currently playing.",                   MORTAL_LEVEL_MIN },
    { "whisper", cmd_whisper, "Send a private message to someone in the room (whisper <name> <message>).", MORTAL_LEVEL_MIN },
    /* Weapons only; a non-weapon refuses wield and points to hold. */
    { "wield",   cmd_wield,   "Wield a weapon in a free hand (wield <item>).",      MORTAL_LEVEL_MIN },
    /* "wip" is already unambiguous ("wi" is shared with wield/wiznews/
     * wizhelp/wiznet, all resolved by their own longer prefixes). The
     * most destructive command short of `shutdown` -- permanently erases
     * a character or account. */
    { "wipe",    cmd_wipe,    "Permanently erase a character or account (wipe <name>|account <name> <password>).", WIPE_MIN_LEVEL },
    /* Bulletin boards (user 2026-07-18) -- "wr" is already unambiguous
     * (nothing else starts "wr"). */
    { "write",   cmd_write,   "Post a message on a bulletin board (write <subject> <message>).", MORTAL_LEVEL_MIN },

    /* ==================== IMMORTAL-ONLY COMMANDS ==================== */
    /* Alphabetical throughout, with one swap (wiznews/wizhelp, below).
     * Two MORTAL_LEVEL_MIN Thief skills live down here rather than in the
     * mortal tier -- `settrap` and `disarmtrap`; see settrap's note. */

    { "aitick",  cmd_aitick,  "Debug: force N mob AI ticks right now (aitick [count]).", IMMORTAL_LEVEL_MIN },
    { "balance", cmd_balance, "Adjust gamewide class/race balance modifiers (balance class|race <name>).", BALANCE_MIN_LEVEL },
    /* "bamfin"/"bamfout" set `goto`'s custom teleport messages; "poofin"/
     * "poofout" (their old name, before a same-session rename to "bamf*"
     * and back) set the WALKING move messages -- see cmd_bamf.c/cmd_poof.c.
     * "bamfi"/"bamfo" and "poofi"/"poofo" are each other's shortest safe
     * abbreviations. */
    { "bamfin",  cmd_bamfin,  "Set your custom `goto` arrival message (bamfin [msg]).",    IMMORTAL_LEVEL_MIN },
    { "bamfout", cmd_bamfout, "Set your custom `goto` departure message (bamfout [msg]).", IMMORTAL_LEVEL_MIN },
    { "copyover", cmd_copyover, "Reboot the server in place; nobody is disconnected.", COPYOVER_MIN_LEVEL },
    { "delbug",  cmd_delbug,  "Delete a handled bug report by id.",                 DELBUG_MIN_LEVEL },
    { "delidea", cmd_delidea, "Delete a handled idea by id.",                       DELIDEA_MIN_LEVEL },
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
    /* Mortal Thief skill (see settrap's note); needs "disarmt"+ now that
     * combat `disarm` (above) owns the shared "di"/"dis" abbreviation. */
    { "disarmtrap", cmd_disarmtrap, "Safely remove a trap from a door (Thief, disarmtrap <direction>).", MORTAL_LEVEL_MIN },
    /* Needs "disg"+ to reach -- "disarm"/"disarmtrap" above already own
     * the shared "di"/"dis" prefix, same abbreviation-ownership shape as
     * disarmtrap's own note just above. */
    { "disguise", cmd_disguise, "Alter your apparent identity (Thief, toggle).", MORTAL_LEVEL_MIN },
    { "dig",     cmd_dig,     "Dig a new room in the current direction, if none exists yet (dig <direction>).", BUILD_MIN_LEVEL },
    { "edbug",   cmd_edbug,   "Resolve a bug report in place (edbug <id> [note]).", EDBUG_MIN_LEVEL },
    /* Unified editor dispatcher (user 2026-07-11: "unify all ed* commands
     * into one edit command"): `edit room [vnum]`, `edit zone <num>`,
     * `edit player <name>`, `edit help <name>`, `edit news`, `edit
     * wiznews`, `edit rules <n> <title>` all replace their old standalone
     * ed* verbs. Gated at BUILD_MIN_LEVEL (the lowest of any sub-editor);
     * nouns needing more (player 58+, help/news/wiznews 56+, rules 59+)
     * check that internally, in cmd_edit.c. */
    { "edit",    cmd_edit,    "Edit a room/zone/object/player/account/help/news/wiznews/rules/trigger (edit <noun> ...).", BUILD_MIN_LEVEL },
    { "egotrip", cmd_egotrip, "Immortal toy-box -- only 'blast <target>' is implemented.", EGOTRIP_MIN_LEVEL },
    { "exec",    cmd_exec,    "Run a shell command on the host box (Implementor).", EXEC_MIN_LEVEL },
    { "gametog", cmd_gametog, "View or flip global game-wide switches (58+).",      GAMETOG_MIN_LEVEL },
    { "hurtlimb", cmd_hurtlimb, "Debug: set a target's limb HP directly (hurtlimb <target> <limb> <hp>).", IMMORTAL_LEVEL_MIN },
    /* "load" is a full prefix of "loadroom", so it MUST stay ahead of it
     * and wins every shared abbreviation up to the exact word "load" --
     * loadroom needs "loadr" (5 letters). Alphabetical order delivers this
     * for free, but it is load-bearing, not incidental. Replaces the old
     * separate mload/oload (user 2026-07-09: one command, category as the
     * first argument). No zone-reset system executes yet, so a room-floor
     * object/mob placed this way doesn't survive a restart. */
    { "load",    cmd_load,    "Spawn a mob or object prototype into your room (load <mob|obj> <vnum|name>).", BUILD_MIN_LEVEL },
    { "loadroom", cmd_loadroom, "Set the room your character logs in at.",          IMMORTAL_LEVEL_MIN },
    { "log",     cmd_log,     "Read, search, list, or rotate the game logs.",       LOG_MIN_LEVEL },
    /* Mortality toggle, other half: `mortal` sets aside true immortal rank
     * to walk the world as an ordinary player for testing -- `immort`
     * (mortal tier, above) reclaims it. Registered at IMMORTAL_LEVEL_MIN;
     * gates further on the STORED true level internally -- see
     * cmd_mortal.c. */
    { "mortal",  cmd_mortal,  "Walk the world as a mortal (immort to return).",     IMMORTAL_LEVEL_MIN },
    { "multiplay", cmd_multiplay, "Toggle whether mortals may multiplay (59+).",    MULTIPLAY_MIN_LEVEL },
    /* Mount/riding system (Sneezy → Tobin feature audit). Listed AFTER
     * "mortal" above on purpose -- "mortal" already owns the "mo"
     * abbreviation for immortals (both are reachable to them, and
     * "mortal" is the established muscle-memory one); "mount" still
     * works fully spelled, or via "mou". "ride" is the primary name
     * (no prefix conflict, nothing else starts "ri"); "mount" is a full
     * alias of it, same alias-row precedent as "engage"/hit above. */
    { "ride",    cmd_ride,    "Mount a rideable creature (ride <target>).",         MORTAL_LEVEL_MIN },
    { "mount",   cmd_ride,    "Mount a rideable creature -- alias of ride.",        MORTAL_LEVEL_MIN },
    { "dismount", cmd_dismount, "Get off your mount.",                             MORTAL_LEVEL_MIN },
    { "dismiss", cmd_dismiss, "Release a charmed pet early, before its bond fades on its own.", MORTAL_LEVEL_MIN },
    { "pee",     cmd_pee,     "Leave a puddle on the floor (pee <liquid> for a specific type).", IMMORTAL_LEVEL_MIN },
    /* Mortal Thief skill (gated internally by being_knows_skill(), same
     * pattern as settrap/disarmtrap), but placed here rather than
     * alphabetically near pray/practice/prompt: "peek" starts with "pee",
     * and cmd_dispatch() resolves a shared abbreviation to whichever entry
     * comes FIRST -- an immortal (who, unlike a mortal, can reach both)
     * typing bare "pee" must keep meaning the pee command above, not this
     * one. Mortals never see "pee" at all, so this costs them nothing;
     * "peek" itself still works fine typed in full either way. */
    { "peek",    cmd_peek,    "Attempt to see what someone is carrying, without their knowledge (Thief, peek <target>).", MORTAL_LEVEL_MIN },
    /* Two unrelated mechanics, same command name -- see cmd_plant.c's own
     * doc comment. `plant <seeds>` (anyone, outdoors) vs. `plant <item>
     * <victim>` (Thief skill, gated internally). No prefix collision:
     * nothing else in this table starts "pl". */
    { "plant",   cmd_plant,   "Sow seeds (plant <seeds>), or secretly slip an item onto someone (Thief, plant <item> <victim>).", MORTAL_LEVEL_MIN },
    { "poofin",  cmd_poofin,  "Set your custom walking arrival message (poofin [msg]).",   IMMORTAL_LEVEL_MIN },
    { "poofout", cmd_poofout, "Set your custom walking departure message (poofout [msg]).", IMMORTAL_LEVEL_MIN },
    { "possess", cmd_possess, "Puppet a mob's body (possess <mob>; `return` to come back).", POSSESS_MIN_LEVEL },
    { "promote", cmd_promote, "Set a player's level (up to your own).",             PROMOTE_MIN_LEVEL },
    { "questdef", cmd_questdef, "Write/replace a quest stage's description (questdef <name> <stage> <text>).", BUILD_MIN_LEVEL },
    /* Bare `purge` clears the room; `purge linkdead` (58+, checked inside
     * cmd_purge itself) sweeps the whole game. */
    { "purge",   cmd_purge,   "Clear this room's mobs/objects, or purge linkdead (58+).", PURGE_MIN_LEVEL },
    /* Liquids (user 2026-07-26): "pou" reaches this fine -- nothing else in
     * the table needs a prefix that short ("poofin"/"poofout" already need
     * "poof", "possess" needs "pos", both longer than "pou" would ever
     * collide with). */
    { "pour",    cmd_pour,    "Empty a drink container onto the ground (pour <container>).", MORTAL_LEVEL_MIN },
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
    /* Placed right after "return" (not in strict alpha order among the
     * mortal r-block above) so "ret"/"retu" keep reaching the far more
     * frequently-typed `return` -- `retrieve` (a deliberate,
     * ticket-number-driven action) is meant to be typed in full anyway. */
    { "retrieve", cmd_retrieve, "Pay for and collect a repaired item (retrieve <ticket #>).", MORTAL_LEVEL_MIN },
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
    { "setsev",  cmd_setsev,  "View or flip which log types echo to you.",          IMMORTAL_LEVEL_MIN },
    /* A MORTAL Thief skill that lives in the immortal tier on purpose:
     * "set" is a literal prefix of "settrap", so if settrap moved up into
     * the mortal tier (where its level says it belongs) the tier rule would
     * put it ahead of `set` and silently reintroduce the exact collision
     * described above. disarmtrap keeps it company for symmetry. */
    { "settrap", cmd_settrap, "Rig a trap on a closed door (Thief, settrap <direction>).", MORTAL_LEVEL_MIN },
    { "snoop",   cmd_snoop,   "Watch what a lower-level player sees and types.",    SNOOP_MIN_LEVEL },
    { "stat",    cmd_stat,    "See every field of an obj/mob/room prototype, or a player (stat <obj|mob|room> <vnum> | stat player <name>).", STAT_MIN_LEVEL },
    { "system",  cmd_system,  "Broadcast an atmosphere line to everyone.",          IMMORTAL_LEVEL_MIN },
    { "test",    cmd_test,    "Show the currently-running smoke test, if any.",     TEST_MIN_LEVEL },
    { "tipedit", cmd_tipedit, "Add/list/delete tips (tipedit <text>|list|delete <id>).", TIPEDIT_MIN_LEVEL },
    { "transfer", cmd_transfer, "Teleport someone to you, or to a room (transfer <name> [vnum]).", IMMORTAL_LEVEL_MIN },
    { "treasury", cmd_treasury, "See how much gold the crown has collected in sales tax.", IMMORTAL_LEVEL_MIN },
    { "users",   cmd_users,   "List all connections with IPs and states.",          USERS_MIN_LEVEL },
    { "vnum",    cmd_vnum,    "List vnums of rooms/objs/mobs by name (vnum <room|obj|mob> <pat>).", BUILD_MIN_LEVEL },
    /* SWAP: wiznews before wizhelp/wiznet -- the immortal tier's only
     * non-alphabetical placement. "wiz" reaches wiznews; wizhelp needs
     * "wizh" and wiznet must be typed in full ("wizne" is ambiguous with
     * wiznews, which wins it). Posting to either channel is `edit wiznews`,
     * folded into the edit dispatcher above. */
    { "wiznews", cmd_wiznews, "Read the immortal news channel.",                    IMMORTAL_LEVEL_MIN },
    /* Hidden from mortals entirely (Tier 3): players only ever see help for
     * what they can use. */
    { "wizhelp", cmd_wizhelp, "List immortal-only commands.",                       IMMORTAL_LEVEL_MIN },
    { "wiznet",  cmd_wiznet,  "Broadcast to online immortals (wiznet [@<level>] <msg>).", IMMORTAL_LEVEL_MIN },
    /* "zone" is a full prefix of "zonefile", so it MUST stay ahead of it --
     * a bare "zone" abbreviation must match the shorter entry first.
     * Alphabetical delivers this for free, but it is load-bearing. */
    { "zone",    cmd_zone,    "zone reset <zone>, or zone assign <zone> <bottom> <top> <builder> (55+).", BUILD_MIN_LEVEL },
    { "zonefile", cmd_zonefile, "zonefile create <zone> -- snapshot the zone's current live mobs/objects into its reset data.", BUILD_MIN_LEVEL },
};
#define NUM_COMMANDS (sizeof(COMMANDS) / sizeof(COMMANDS[0]))

const cmd_entry_t *cmd_table_entries(int *count) {
    *count = (int)NUM_COMMANDS;
    return COMMANDS;
}

bool cmd_dispatch(descriptor_t *d, const char *line) {
    while (*line == ' ')
        line++;
    if (!*line)
        return true;

    /* `@set ...` (Session 43, TODO) -- a leading `@` isn't a command of its
     * own (no `@`-anything system is planned), just a habit some players
     * type before `set`. Unlike the `'`/`;` shortcuts below (which replace
     * a single character with a whole hardcoded verb, since the real verb
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
    } else if (*line == ';') {
        /* `;` is the one-character shorthand for `wiznet` (same idea as `'`
         * for say). Immortal-only like wiznet itself -- a mortal typing it
         * just falls through to the "Huh?!" path below. */
        strcpy(verb, "wiznet");
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
     * leave. */
    if (d->character && being_get_wait(d->character) > 0) {
        descriptor_send(d, "You are still recovering!\r\n");
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
