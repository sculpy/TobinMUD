/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd.h"

#include <ctype.h>
#include <string.h>

#include "being.h"
#include "cmd_internal.h"
#include "socials.h"

/* First-word command dispatch with DikuMUD-style abbreviation matching:
 * any non-empty prefix of a command's name dispatches to it (e.g. "sc" or
 * "sco" both reach "score", same as "l" reaches "look") -- replacing the
 * original's much larger cmd/ directory of command-table machinery. Add
 * an entry here + a cmd_<name>.c as each command gets ported -- see
 * c_port/STATUS.md. Keep names distinct enough that no two commands share
 * a meaningful prefix, since the FIRST match in this list wins.
 *
 * `quit` is deliberately NOT in this table -- it's excluded from
 * abbreviation matching entirely and requires the exact, full literal
 * "quit!" (see cmd_dispatch below), so a mistyped or abbreviated command
 * can never accidentally leave the character.
 *
 * Table-wide ordering rule (user 2026-07-12: "place immortal commands
 * lower in the list of commands, that way the immortals are less likely
 * to make mistakes when working on the game"): every MORTAL_LEVEL_MIN
 * command below is grouped together FIRST, and every immortal-tier
 * command (anything above MORTAL_LEVEL_MIN) comes after, as a second
 * block -- so any abbreviation an immortal types that's ambiguous
 * between an everyday mortal action and a rarer, more consequential
 * immortal one always resolves to the mortal action first (the "set"
 * vs "settrap" and "get" vs "goto" bugs fixed earlier this session were
 * both exactly this class of mistake). Relative order WITHIN each block
 * is unchanged from before -- every existing per-entry abbreviation
 * comment below still describes a same-tier neighbor unless it says
 * otherwise. One deliberate exception: `settrap`/`disarmtrap` (mortal
 * Thief skills) stay grouped with `set`/`setsev` in the immortal block
 * rather than moving to the mortal block -- `settrap` is a literal
 * prefix of "set", so if it moved ahead of `set`/`setsev` the general
 * mortal-first rule would silently reintroduce the exact "set" ->
 * "settrap" collision this session already fixed once. */
static const cmd_entry_t COMMANDS[] = {
    /* ==================== MORTAL-VISIBLE COMMANDS ==================== */

    /* Movement first, like classic Diku command tables: the single letters
     * n/e/s/w/u/d must always mean movement, so these outrank say ("s"),
     * who ("w"), and everything else in abbreviation matching. */
    { "north",   cmd_north,   "Walk north.",                                        MORTAL_LEVEL_MIN },
    { "east",    cmd_east,    "Walk east.",                                         MORTAL_LEVEL_MIN },
    { "south",   cmd_south,   "Walk south.",                                        MORTAL_LEVEL_MIN },
    { "west",    cmd_west,    "Walk west.",                                         MORTAL_LEVEL_MIN },
    { "up",      cmd_up,      "Walk up.",                                           MORTAL_LEVEL_MIN },
    { "down",    cmd_down,    "Walk down.",                                         MORTAL_LEVEL_MIN },
    /* Diagonals AFTER the cardinals so "n"/"s" stay north/south. The
     * two-letter forms are NOT prefixes of the long names ("ne" vs
     * "no-rtheast"), so they get explicit alias rows -- the classic Diku
     * arrangement. "se"/"sw" sit above say/score in matching order but
     * don't collide ("sa"/"sc" still reach those). */
    { "northeast", cmd_northeast, "Walk northeast.",                                MORTAL_LEVEL_MIN },
    { "northwest", cmd_northwest, "Walk northwest.",                                MORTAL_LEVEL_MIN },
    { "southeast", cmd_southeast, "Walk southeast.",                                MORTAL_LEVEL_MIN },
    { "southwest", cmd_southwest, "Walk southwest.",                                MORTAL_LEVEL_MIN },
    { "ne",        cmd_northeast, NULL,                                             MORTAL_LEVEL_MIN },
    { "nw",        cmd_northwest, NULL,                                             MORTAL_LEVEL_MIN },
    { "se",        cmd_southeast, NULL,                                             MORTAL_LEVEL_MIN },
    { "sw",        cmd_southwest, NULL,                                             MORTAL_LEVEL_MIN },
    { "look",    cmd_look,    "Look around the room you're in.",                    MORTAL_LEVEL_MIN },
    /* "e" is east; "ex"+ reaches exits, so "exa"+ is examine's shortest
     * safe abbreviation. A plain synonym for `look <target>`. */
    { "exits",   cmd_exits,   "List this room's exits and where they lead.",        MORTAL_LEVEL_MIN },
    { "examine", cmd_examine, "Look at something in detail -- a synonym for look <target>.", MORTAL_LEVEL_MIN },
    /* "o" is unused, so "open" gets it; "c" is color's, "cl"+ reaches close. */
    { "open",    cmd_open,    "Open a door (open <direction>).",                    MORTAL_LEVEL_MIN },
    { "close",   cmd_close,   "Close a door (close <direction>).",                  MORTAL_LEVEL_MIN },
    { "who",     cmd_who,     "List everyone currently playing.",                   MORTAL_LEVEL_MIN },
    { "score",   cmd_score,   "Show your character's stats, level, and HP.",        MORTAL_LEVEL_MIN },
    { "skills",  cmd_skills,  "List your class's skills/spells, known and locked.", MORTAL_LEVEL_MIN },
    /* after score, so "sc"/"sco" still reach score; "sca"/"scan" -> scan. */
    { "scan",    cmd_scan,    "Peer several rooms down each exit (scan [dir|name]).", MORTAL_LEVEL_MIN },
    { "color",   cmd_color,   "Toggle ANSI color rendering on or off.",             MORTAL_LEVEL_MIN },
    /* attack and kill are FULL aliases (user spec): one handler, both
     * instakill for immortals, both normal combat for mortals. (hurtlimb/
     * aitick, two immortal debug tools that used to sit right here, moved
     * down to the immortal block -- see the table-wide note above.) */
    { "attack",  cmd_kill,    "Attack a player or mobile (instant slay for immortals).", MORTAL_LEVEL_MIN },
    { "kill",    cmd_kill,    "Attack a player or mobile (instant slay for immortals).", MORTAL_LEVEL_MIN },
    { "hit",     cmd_hit,     "Attack a player or mobile via real combat, even for immortals (never instakill).", MORTAL_LEVEL_MIN },
    { "flee",    cmd_flee,    "Try to escape a fight through a random exit.",        MORTAL_LEVEL_MIN },
    { "say",     cmd_say,     "Say something to everyone in the room.",             MORTAL_LEVEL_MIN },
    /* No "sh"-prefixed collision yet -- typed in full is safe either way. */
    { "shout",   cmd_shout,   "Shout something to everyone in the game (shout <msg>).", MORTAL_LEVEL_MIN },
    /* "show" and "shout" diverge at the 4th letter ("show" vs "shou"),
     * but "sho" alone is still ambiguous and "shout" (just above) wins
     * it -- "show" typed in full is show's shortest safe abbreviation. */
    { "show",    cmd_show,    "Show a carried item to someone in the room (show <item> <person>).", MORTAL_LEVEL_MIN },
    /* "te" also reaches "test" (immortal block, further down) -- "tel"+ is
     * tell's shortest safe abbreviation regardless, since this whole
     * mortal block is checked before any immortal command anyway. */
    { "tell",    cmd_tell,    "Send a private message to anyone playing (tell <name> <message>).", MORTAL_LEVEL_MIN },
    /* "wh" also reaches "who" (above, wins the 2-letter abbreviation) --
     * "whi"+ is whisper's shortest safe abbreviation. */
    { "whisper", cmd_whisper, "Send a private message to someone in the room (whisper <name> <message>).", MORTAL_LEVEL_MIN },
    /* Positions. Prefix notes: "s"=south; "sa"=say, "sc"=score, "se/sw"=
     * diagonals; "si"=sit, "sl"=sleep, "st"=stand; "r"=rest (no other r
     * command); "w"=west, so "wa"=wake. */
    { "stand",   cmd_stand,   "Stand up.",                                          MORTAL_LEVEL_MIN },
    { "sit",     cmd_sit,     "Sit down.",                                          MORTAL_LEVEL_MIN },
    { "rest",    cmd_rest,    "Sit down and rest (heals faster).",                  MORTAL_LEVEL_MIN },
    { "sleep",   cmd_sleep,   "Lie down and sleep (heals fastest).",                MORTAL_LEVEL_MIN },
    { "wake",    cmd_wake,    "Wake up from sleep.",                                MORTAL_LEVEL_MIN },
    /* "s"/"so" are south (movement); "soc"+ reaches socials. */
    { "socials", cmd_socials, "List the socials you can use (smile, wave, ...).",   MORTAL_LEVEL_MIN },
    /* "m"/"mo" reach mortal (immortal block, further down); "mu"+ reaches
     * mudstats -- unaffected by that, mudstats is mortal so it's checked
     * first regardless. */
    { "mudstats", cmd_mudstats, "Show basic statistics about the game world.",       MORTAL_LEVEL_MIN },
    /* "c"/"co" reach color/copyover; "cat"+ reaches catchup. Mortal-level:
     * the pager (e.g. `news`) holds messages for anyone, not just
     * immortals mid-editor, so anyone can be left with something to
     * catch up on. */
    { "catchup", cmd_catchup, "Replay game messages missed while editing or paging.", MORTAL_LEVEL_MIN },
    { "cast",    cmd_cast,    "Cast a spell (Mage/Druid) -- requires a component.", MORTAL_LEVEL_MIN },
    { "pray",    cmd_pray,    "Pray for a spell (Cleric) -- requires a holy symbol.", MORTAL_LEVEL_MIN },
    { "practice", cmd_practice, "Train your Basic/Advanced discipline with a guildmaster (practice basic|advanced).", MORTAL_LEVEL_MIN },
    { "continue", cmd_continue, "Repeat your last heal-type prayer until the target is healed or your holy symbols run out.", MORTAL_LEVEL_MIN },
    /* "con" already reaches "continue" (just above, wins any 3-letter
     * abbreviation) -- "cons"+ is consider's shortest safe abbreviation. */
    { "consider", cmd_consider, "Size up a fight before you start one (consider <target>|self).", MORTAL_LEVEL_MIN },
    /* settrap/disarmtrap are NOT here -- see the table-wide note at the
     * top of this file. They're mortal Thief skills, but "settrap" is a
     * literal prefix of "set" (an immortal command), so they stay grouped
     * with set/setsev in the immortal block below to keep that exact-name
     * collision fixed. */
    { "affects", cmd_affects, "List your currently active buffs/debuffs.", MORTAL_LEVEL_MIN },
    /* "n"/"ne"/"nw" are movement (above); "new"/"news" reach news, "newb"+
     * reaches newbie (must stay AFTER news so "new" still means news). */
    { "news",    cmd_news,    "Read the latest game news (news [10|20|50|100]).",   MORTAL_LEVEL_MIN },
    { "newbie",  cmd_newbie,  "Chat on the newbie help channel (newbie <msg>).",    MORTAL_LEVEL_MIN },
    { "rules",   cmd_rules,   "Read the game rules (rules, or rules <number>).",     MORTAL_LEVEL_MIN },
    /* Posting news/rules is now `edit news`/`edit rules` (immortal block,
     * further down). */
    { "limbs",   cmd_limbs,   "Show the current health of all your limbs.",         MORTAL_LEVEL_MIN },
    { "help",    cmd_help,    "List available commands.",                           MORTAL_LEVEL_MIN },
    /* "g" must reach "get", not "goto" -- get is deliberately ahead of
     * goto (same precedent as set/settrap): goto used to be immortal-only,
     * so "g" only ever meant get for mortals anyway and this ordering
     * didn't matter; now that goto's landmark forms (guildmaster/rent/
     * surplus) are mortal-visible too, "g" would otherwise ambiguously
     * shadow get's single-letter abbreviation for everyone. Immortals who
     * want goto's vnum/player teleport form still have "go" (2 letters) --
     * nothing else in the table starts with "go", so that's unambiguous
     * regardless of ordering. */
    { "get",     cmd_get,     "Pick up an item, or take one from a container (get <item> [container]).", MORTAL_LEVEL_MIN },
    { "goto",    cmd_goto,    "Directions to your guildmaster/the inn/the surplus store (goto guildmaster|rent|surplus), or teleport to a room by vnum (immortal).", MORTAL_LEVEL_MIN },
    /* "d"/"do" are down (movement, above); "drop" needs "dr" minimum. */
    { "drop",    cmd_drop,    "Put down a carried item (drop <item>).",            MORTAL_LEVEL_MIN },
    /* "put" typed in full; "p"/"pu" reach earlier p-commands first. */
    { "put",     cmd_put,     "Put a carried item into a container (put <item> <container>).", MORTAL_LEVEL_MIN },
    /* "dr" already reaches "drop" (registered first, wins the 2-letter
     * abbreviation) -- "dri"+ is drink's shortest safe abbreviation. */
    { "drink",   cmd_drink,   "Drink from a puddle on the ground (drink <puddle>).", MORTAL_LEVEL_MIN },
    /* "si" already reaches "sit" (registered with the other positions,
     * above) -- "sip" typed in full is safe either way. */
    { "sip",     cmd_sip,     "Taste a bit of a puddle or fountain, low risk (sip <liquid>).", MORTAL_LEVEL_MIN },
    /* Bare "i" reaches inventory itself -- it's registered before
     * "immort" (further down), so "immort" actually needs "im" minimum,
     * not "i" (a pre-existing inaccuracy in this comment, corrected while
     * touching this file for the 2026-07-12 table-wide reorder -- doesn't
     * change any real behavior, since a real mortal typing "i" getting
     * their inventory instead of a no-op immort attempt was always the
     * more useful outcome anyway). */
    { "inventory", cmd_inventory, "List what you're carrying.",                    MORTAL_LEVEL_MIN },
    /* "eq" doesn't collide with anything ('east' only owns "ea..."). */
    { "equipment", cmd_equipment, "List what you're wearing and holding.",         MORTAL_LEVEL_MIN },
    /* "we" already reaches "west" (movement, above) -- "wear" needs "wea".
     * Body-slot items only as of the hold/wield split below (user
     * 2026-07-09) -- a holdable item now refuses `wear` with a pointer to
     * whichever of hold/wield actually applies. */
    { "wear",    cmd_wear,    "Put on a carried item's body slot (wear <item>).",  MORTAL_LEVEL_MIN },
    /* "h" already reaches "help" (above) -- "hold" needs "ho". Non-weapon
     * holdables only; a weapon refuses hold and points to wield. */
    { "hold",    cmd_hold,    "Hold a non-weapon item in a free hand (hold <item>).", MORTAL_LEVEL_MIN },
    /* "wield" needs "wie" (to clear "wear"/"west"). Weapons only; a
     * non-weapon refuses wield and points to hold. */
    { "wield",   cmd_wield,   "Wield a weapon in a free hand (wield <item>).",     MORTAL_LEVEL_MIN },
    /* "sw" is already southwest's own alias (above) -- "switch" needs "swi". */
    { "switch",  cmd_switch,  "Swap what's in your primary and secondary hold.",   MORTAL_LEVEL_MIN },
    /* "re" already reaches "rest" (above) -- "remove" needs "rem". */
    { "remove",  cmd_remove,  "Take off a worn or held item (remove <item>).",     MORTAL_LEVEL_MIN },
    /* "p"/"pr"/"pro" reach prompt; "prom"+ reaches promote (immortal
     * block, further down). */
    { "prompt",  cmd_prompt,  "Customize your prompt (prompt hp).",                 MORTAL_LEVEL_MIN },
    { "time",    cmd_time,    "Show the current mud clock, weekday, and date.",     MORTAL_LEVEL_MIN },
    { "title",   cmd_title,   "Set the title shown after your name in who.",        MORTAL_LEVEL_MIN },
    { "save",    cmd_save,    "Save your character now.",                           MORTAL_LEVEL_MIN },
    { "rent",    cmd_rent,    "Store your belongings and leave the game safely.",   MORTAL_LEVEL_MIN },
    { "toggle",  cmd_toggle,  "View or flip on/off switches (color, hp, ...).",     MORTAL_LEVEL_MIN },
    { "bug",     cmd_bug,     "Report a bug (bug <text>); immortals list them.",    MORTAL_LEVEL_MIN },
    { "idea",    cmd_idea,    "Suggest a feature (idea <text>); immortals list them.", MORTAL_LEVEL_MIN },
    /* Mortality toggle: "immort" reclaims true immortal rank after
     * `mortal` (immortal block, further down) sets it aside -- registered
     * at MORTAL level itself (NULL help = unlisted) so a real mortal
     * typing it still matches the entry, but the handler gates on the
     * STORED true level internally and does nothing useful for them.
     * Registered last in the mortal block, after both other "i"-prefixed
     * commands (inventory, idea), so "im" is its actual shortest safe
     * abbreviation -- bare "i" reaches inventory (see its own note). */
    { "immort",  cmd_immort,  NULL,                                                 MORTAL_LEVEL_MIN },

    /* ==================== IMMORTAL-ONLY COMMANDS ==================== */

    /* Two immortal debug tools, moved down from the combat block above
     * (user 2026-07-12 table-wide reorder) -- no abbreviation collision
     * with anything, mortal or immortal, so their exact position within
     * this block doesn't matter. */
    { "hurtlimb", cmd_hurtlimb, "Debug: set a target's limb HP directly (hurtlimb <target> <limb> <hp>).", IMMORTAL_LEVEL_MIN },
    { "aitick",  cmd_aitick,  "Debug: force N mob AI ticks right now (aitick [count]).", IMMORTAL_LEVEL_MIN },
    { "multiplay", cmd_multiplay, "Toggle whether mortals may multiplay (59+).",      MULTIPLAY_MIN_LEVEL },
    { "balance", cmd_balance, "Adjust gamewide class/race balance modifiers (balance class|race <name>).", BALANCE_MIN_LEVEL },
    { "egotrip", cmd_egotrip, "Immortal toy-box -- only 'blast <target>' is implemented.", EGOTRIP_MIN_LEVEL },
    /* "set" must come BEFORE "settrap" here -- both start with "set", and
     * first match in table order wins, so typing the exact command "set"
     * would otherwise dispatch into settrap instead (discovered via
     * smoke_test_alignment.py: `set <name> alignment 500` was landing on
     * settrap's "Usage: settrap <direction>" instead). setsev tags along
     * right after, per the pre-existing set-vs-setsev ordering rule
     * below. settrap/disarmtrap are mortal Thief skills that would
     * otherwise belong in the mortal block above (see the table-wide note
     * at the top of this file) but stay right here instead, specifically
     * so this fix can't be undone by the general mortal-first reorder. */
    { "set",     cmd_set,     "Set one field on a player (set <name> <field> <value>).", SET_MIN_LEVEL },
    { "setsev",  cmd_setsev,  "View or flip which log types echo to you.",          IMMORTAL_LEVEL_MIN },
    { "settrap", cmd_settrap, "Rig a trap on a closed door (Thief, settrap <direction>).", MORTAL_LEVEL_MIN },
    { "disarmtrap", cmd_disarmtrap, "Safely remove a trap from a door (Thief, disarmtrap <direction>).", MORTAL_LEVEL_MIN },
    /* Immortal news channel. "wizh"->wizhelp, "wizn"->wiznews. Posting is
     * now `edit wiznews` (folded into the unified edit dispatcher below). */
    { "wiznews", cmd_wiznews, "Read the immortal news channel.",                     IMMORTAL_LEVEL_MIN },
    /* "wizne" is ambiguous with wiznews (above, so it wins); "wiznet" full. */
    { "wiznet",  cmd_wiznet,  "Broadcast a message to all online immortals.",        IMMORTAL_LEVEL_MIN },
    { "system",  cmd_system,  "Broadcast an atmosphere line to everyone.",           IMMORTAL_LEVEL_MIN },
    /* Hidden from mortals entirely (Tier 3): players only ever see help
     * for what they can use. */
    { "wizhelp", cmd_wizhelp, "List immortal-only commands.",                       IMMORTAL_LEVEL_MIN },
    { "transfer", cmd_transfer, "Teleport someone to you, or to a room (transfer <name> [vnum]).", IMMORTAL_LEVEL_MIN },
    /* "load" is itself a full prefix of "loadroom", so it MUST sit before
     * loadroom and wins every shared abbreviation up to and including the
     * exact word "load" -- loadroom needs "loadr"+ (5 letters) to reach,
     * not "loa". Replaces the old separate mload/oload commands (user
     * 2026-07-09: one command, category as the first argument). No
     * zone-reset system executing yet, so a room-floor object/mob placed
     * this way doesn't survive a restart. */
    { "load",    cmd_load,    "Spawn a mob or object prototype into your room (load <mob|obj> <vnum|name>).", BUILD_MIN_LEVEL },
    { "loadroom", cmd_loadroom, "Set the room your character logs in at.",          IMMORTAL_LEVEL_MIN },
    /* "purge" would otherwise share "pu" with "put" (mortal block, above)
     * -- moot now, "put" is checked first regardless since it's mortal --
     * "pur"+ is purge's shortest safe abbreviation. Bare `purge` clears
     * the room; `purge linkdead` (58+, checked inside cmd_purge itself)
     * sweeps the whole game. */
    { "purge",   cmd_purge,   "Clear this room's mobs/objects, or purge linkdead (58+).", PURGE_MIN_LEVEL },
    /* Flavor command -- no abbreviation collision with any existing "pe"+
     * command, so it's registered typed in full. */
    { "pee",     cmd_pee,     "Leave a puddle on the floor.",                       IMMORTAL_LEVEL_MIN },
    { "vnum",    cmd_vnum,    "List vnums of rooms/objs/mobs by name (vnum <room|obj|mob> <pat>).", BUILD_MIN_LEVEL },
    /* "stat" typed in full is safe either way (nothing else immortal-tier
     * starts with "stat"). */
    { "stat",    cmd_stat,    "See every field of an obj/mob/room prototype, or a player (stat <obj|mob|room> <vnum> | stat player <name>).", STAT_MIN_LEVEL },
    { "zone",    cmd_zone,    "zone reset <zone>, or zone assign <zone> <bottom> <top> <builder> (55+).", BUILD_MIN_LEVEL },
    /* Must stay AFTER "zone" above -- a bare "zone" abbreviation must match
     * the shorter "zone" entry first (see cmd_dispatch()'s prefix-match
     * loop; "zonefile" itself also starts with "zone" and would otherwise
     * shadow it if checked first). */
    { "zonefile", cmd_zonefile, "zonefile create <zone> -- snapshot the zone's current live mobs/objects into its reset data.", BUILD_MIN_LEVEL },
    { "promote", cmd_promote, "Set a player's level (up to your own).",             PROMOTE_MIN_LEVEL },
    /* Editing a player is now `edit player <name>` (folded into the
     * unified edit dispatcher below). */
    /* "bamfin"/"bamfout" now set `goto`'s custom teleport messages;
     * "poofin"/"poofout" (their old name, before a same-session rename to
     * "bamfin"/"bamfout" and then back) set the WALKING move messages --
     * see cmd_bamf.c/cmd_poof.c's doc comments. "bamfi"+/"bamfo"+ and
     * "poofi"+/"poofo"+ are each other's shortest safe abbreviations. */
    { "bamfin",  cmd_bamfin,  "Set your custom `goto` arrival message (bamfin [msg]).",    IMMORTAL_LEVEL_MIN },
    { "bamfout", cmd_bamfout, "Set your custom `goto` departure message (bamfout [msg]).", IMMORTAL_LEVEL_MIN },
    { "poofin",  cmd_poofin,  "Set your custom walking arrival message (poofin [msg]).",   IMMORTAL_LEVEL_MIN },
    { "poofout", cmd_poofout, "Set your custom walking departure message (poofout [msg]).", IMMORTAL_LEVEL_MIN },
    { "gametog", cmd_gametog, "View or flip global game-wide switches (58+).",      GAMETOG_MIN_LEVEL },
    { "snoop",   cmd_snoop,   "Watch what a lower-level player sees and types.",    SNOOP_MIN_LEVEL },
    { "test",    cmd_test,    "Show the currently-running smoke test, if any.",     TEST_MIN_LEVEL },
    /* "c"/"co" reach color (mortal block, above, checked first regardless)
     * -- "cop"+ is copyover's shortest safe abbreviation either way. */
    { "copyover", cmd_copyover, "Reboot the server in place; nobody is disconnected.", COPYOVER_MIN_LEVEL },
    { "exec",    cmd_exec,    "Run a shell command on the host box (Implementor).", EXEC_MIN_LEVEL },
    { "delbug",  cmd_delbug,  "Delete a handled bug report by id.",                 DELBUG_MIN_LEVEL },
    { "edbug",   cmd_edbug,   "Resolve a bug report in place (edbug <id> [note]).", EDBUG_MIN_LEVEL },
    { "delidea", cmd_delidea, "Delete a handled idea by id.",                       DELIDEA_MIN_LEVEL },
    /* Unified editor dispatcher (user, 2026-07-11: "unify all ed* commands
     * into one edit command"): `edit room [vnum]`, `edit zone <num>`,
     * `edit player <name>`, `edit help <name>`, `edit news`, `edit
     * wiznews`, `edit rules <n> <title>` all replace their old standalone
     * ed* verbs. Gated at BUILD_MIN_LEVEL (the lowest of any sub-editor)
     * -- nouns needing more (player 58+, help/news/wiznews 56+, rules
     * 59+) check that internally, in cmd_edit.c. */
    { "edit",    cmd_edit,    "Edit a room/zone/player/help/news/wiznews/rules/trigger (edit <noun> ...).", BUILD_MIN_LEVEL },
    /* "log" needs its three letters ("l" look, "li" limbs, "lo" look --
     * all mortal block, above, checked first regardless). */
    { "log",     cmd_log,     "Read, search, list, or rotate the game logs.",       LOG_MIN_LEVEL },
    { "users",   cmd_users,   "List all connections with IPs and states.",          USERS_MIN_LEVEL },
    /* Mortality toggle, other half: `mortal` sets aside true immortal rank
     * to walk the world as an ordinary player for testing -- `immort`
     * (mortal block, above) reclaims it. Registered at IMMORTAL_LEVEL_MIN
     * itself; gates further on the STORED true level internally -- see
     * cmd_mortal.c. */
    { "mortal",  cmd_mortal,  "Walk the world as a mortal (immort to return).",     IMMORTAL_LEVEL_MIN },
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
     * refused, matching the original's commandInfo::minLevel dispatch gate. */
    int level = d->character ? d->character->progress.level : MORTAL_LEVEL_MIN;
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
