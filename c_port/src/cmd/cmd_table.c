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
 * can never accidentally leave the character. */
static const cmd_entry_t COMMANDS[] = {
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
    /* "e" is east; "ex"+ reaches exits. */
    { "exits",   cmd_exits,   "List this room's exits and where they lead.",        MORTAL_LEVEL_MIN },
    /* "o" is unused, so "open" gets it; "c" is color's, "cl"+ reaches close. */
    { "open",    cmd_open,    "Open a door (open <direction>).",                    MORTAL_LEVEL_MIN },
    { "close",   cmd_close,   "Close a door (close <direction>).",                  MORTAL_LEVEL_MIN },
    { "who",     cmd_who,     "List everyone currently playing.",                   MORTAL_LEVEL_MIN },
    { "score",   cmd_score,   "Show your character's stats, level, and HP.",        MORTAL_LEVEL_MIN },
    /* after score, so "sc"/"sco" still reach score; "sca"/"scan" -> scan. */
    { "scan",    cmd_scan,    "Peer several rooms down each exit (scan [dir|name]).", MORTAL_LEVEL_MIN },
    { "color",   cmd_color,   "Toggle ANSI color rendering on or off.",             MORTAL_LEVEL_MIN },
    /* attack and kill are FULL aliases (user spec): one handler, both
     * instakill for immortals, both normal combat for mortals. */
    { "attack",  cmd_kill,    "Attack a player or mobile (instant slay for immortals).", MORTAL_LEVEL_MIN },
    { "kill",    cmd_kill,    "Attack a player or mobile (instant slay for immortals).", MORTAL_LEVEL_MIN },
    { "flee",    cmd_flee,    "Try to escape a fight through a random exit.",        MORTAL_LEVEL_MIN },
    { "say",     cmd_say,     "Say something to everyone in the room.",             MORTAL_LEVEL_MIN },
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
    /* "m"/"mo" reach mortal; "mu"+ reaches mudstats. */
    { "mudstats", cmd_mudstats, "Show basic statistics about the game world.",       MORTAL_LEVEL_MIN },
    /* "mud"->mudstats, "mul"->multiplay. */
    { "multiplay", cmd_multiplay, "Toggle whether mortals may multiplay (59+).",      MULTIPLAY_MIN_LEVEL },
    /* "c"/"co" reach color/copyover; "cat"+ reaches catchup. Immortal-only:
     * only immortals use editors, so only they ever hold messages. */
    { "catchup", cmd_catchup, "Replay game messages missed while editing.",         IMMORTAL_LEVEL_MIN },
    /* Immortal news channel. "wizh"->wizhelp, "wizn"->wiznews. "edw"->edwiznews. */
    { "wiznews", cmd_wiznews, "Read the immortal news channel.",                     IMMORTAL_LEVEL_MIN },
    { "edwiznews", cmd_edwiznews, "Post immortal news (headline + story).",          ADDNEWS_MIN_LEVEL },
    /* "wizne" is ambiguous with wiznews (above, so it wins); "wiznet" full. */
    { "wiznet",  cmd_wiznet,  "Broadcast a message to all online immortals.",        IMMORTAL_LEVEL_MIN },
    /* "s"/"so" are south/socials; "sy"+ reaches system. */
    { "system",  cmd_system,  "Broadcast an atmosphere line to everyone.",           IMMORTAL_LEVEL_MIN },
    /* "n"/"ne"/"nw" are movement (above); "new"/"news" reach news, "newb"+
     * reaches newbie (must stay AFTER news so "new" still means news). */
    { "news",    cmd_news,    "Read the latest game news (news [10|20|50|100]).",   MORTAL_LEVEL_MIN },
    { "newbie",  cmd_newbie,  "Chat on the newbie help channel (newbie <msg>).",    MORTAL_LEVEL_MIN },
    { "rules",   cmd_rules,   "Read the game rules (rules, or rules <number>).",     MORTAL_LEVEL_MIN },
    /* "a"/"at" reach attack (above); "add"+ reaches addnews. */
    { "ednews",  cmd_addnews, "Post a news item (headline + story).",               ADDNEWS_MIN_LEVEL },
    { "edrules", cmd_edrules, "Write a numbered game rule (edrules <n> <title>).",  EDRULES_MIN_LEVEL },
    { "limbs",   cmd_limbs,   "Show the current health of all your limbs.",         MORTAL_LEVEL_MIN },
    { "help",    cmd_help,    "List available commands.",                           MORTAL_LEVEL_MIN },
    /* Hidden from mortals entirely (Tier 3): players only ever see help
     * for what they can use. */
    { "wizhelp", cmd_wizhelp, "List immortal-only commands.",                       IMMORTAL_LEVEL_MIN },
    { "goto",    cmd_goto,    "Teleport to a room by vnum.",                        IMMORTAL_LEVEL_MIN },
    /* "l" look, "li" limbs, "lo" look, "loa"+ loadroom, "log" log. */
    { "loadroom", cmd_loadroom, "Set the room your character logs in at.",          IMMORTAL_LEVEL_MIN },
    /* Objects (Phase 2C). Placed after "goto" (above) so an immortal's bare
     * "g" still reaches goto first -- "get" only wins "g" for mortals, who
     * never see goto at all (skipped by min_level). */
    { "get",     cmd_get,     "Pick up an item from the room (get <item>).",       MORTAL_LEVEL_MIN },
    /* "d"/"do" are down (movement, above); "drop" needs "dr" minimum. */
    { "drop",    cmd_drop,    "Put down a carried item (drop <item>).",            MORTAL_LEVEL_MIN },
    /* Bare "i" already reaches "immort" (below) for every caller -- this
     * needs at least "in"/"inv". */
    { "inventory", cmd_inventory, "List what you're carrying.",                    MORTAL_LEVEL_MIN },
    /* "eq" doesn't collide with anything ('east' only owns "ea..."). */
    { "equipment", cmd_equipment, "List what you're wearing and holding.",         MORTAL_LEVEL_MIN },
    /* "we" already reaches "west" (movement, above) -- "wear" needs "wea". */
    { "wear",    cmd_wear,    "Put on or wield a carried item (wear <item>).",     MORTAL_LEVEL_MIN },
    /* "re" already reaches "rest" (above) -- "remove" needs "rem". */
    { "remove",  cmd_remove,  "Take off a worn or held item (remove <item>).",     MORTAL_LEVEL_MIN },
    /* "o" is already "open"'s (above) -- "oload" needs "ol". Immortal
     * builder tool, same tier as edroom/goto: no zone-reset system yet, so
     * a room-floor object placed this way doesn't survive a restart. */
    { "oload",   cmd_oload,   "Spawn an object prototype into your room (oload <vnum>).", BUILD_MIN_LEVEL },
    /* "ml" doesn't collide with mudstats/multiplay ("mu") or mortal ("mo"). */
    { "mload",   cmd_mload,   "Spawn a mobile prototype into your room (mload <vnum>).", BUILD_MIN_LEVEL },
    { "vnum",    cmd_vnum,    "List vnums of rooms/objs/mobs by name (vnum <room|obj|mob> <pat>).", BUILD_MIN_LEVEL },
    /* "p"/"pr"/"pro" reach prompt; "prom"+ reaches promote. */
    { "prompt",  cmd_prompt,  "Customize your prompt (prompt hp).",                 MORTAL_LEVEL_MIN },
    { "promote", cmd_promote, "Set a player's level (up to your own).",             PROMOTE_MIN_LEVEL },
    { "edplayer", cmd_edplayer, "Edit a player's level/xp/hp/attrs/gender/title/location.", EDPLAYER_MIN_LEVEL },
    { "title",   cmd_title,   "Set the title shown after your name in who.",        MORTAL_LEVEL_MIN },
    { "toggle",  cmd_toggle,  "View or flip on/off switches (color, hp, ...).",     MORTAL_LEVEL_MIN },
    { "bug",     cmd_bug,     "Report a bug (bug <text>); immortals list them.",    MORTAL_LEVEL_MIN },
    /* NOTE: must stay after "help" in this table -- "h"/"he"/"hel" should
     * abbreviate to help (first match wins), "hed"+ reaches hedit. Same
     * deal for copyover after color: "c"/"co" reach color, "cop"+ this. */
    { "edhelp",  cmd_hedit,   "Edit a help topic in the line editor.",              HELP_EDIT_MIN_LEVEL },
    { "copyover", cmd_copyover, "Reboot the server in place; nobody is disconnected.", COPYOVER_MIN_LEVEL },
    { "exec",    cmd_exec,    "Run a shell command on the host box (Implementor).", EXEC_MIN_LEVEL },
    { "delbug",  cmd_delbug,  "Delete a handled bug report by id.",                 DELBUG_MIN_LEVEL },
    /* "log" needs its three letters ("l" look, "li" limbs, "lo" look). */
    { "edroom",  cmd_edit,    "Edit a room -- the one you're in, or edroom <vnum>.", BUILD_MIN_LEVEL },
    { "log",     cmd_log,     "Read, search, list, or rotate the game logs.",       LOG_MIN_LEVEL },
    /* "se" is already an explicit southeast alias (above). "set" must come
     * BEFORE "setsev" here -- both start with "set", and first match in
     * table order wins, so "set" needs to win the exact 3-letter typo it
     * shares with "setsev"'s own shortest abbreviation. */
    { "set",     cmd_set,     "Set one field on a player (set <name> <field> <value>).", SET_MIN_LEVEL },
    { "setsev",  cmd_setsev,  "View or flip which log types echo to you.",          IMMORTAL_LEVEL_MIN },
    /* "u" is up (movement); "us"+ reaches users. */
    { "users",   cmd_users,   "List all connections with IPs and states.",          USERS_MIN_LEVEL },
    /* Mortality toggle: mortal is a normal 51+ command; immort is
     * registered at MORTAL level (NULL help = unlisted) and gates itself
     * on the STORED true level -- see cmd_mortal.c. "i" reaches immort;
     * "m" reaches mortal (hidden from real mortals by min_level). */
    { "mortal",  cmd_mortal,  "Walk the world as a mortal (immort to return).",     IMMORTAL_LEVEL_MIN },
    { "immort",  cmd_immort,  NULL,                                                 MORTAL_LEVEL_MIN },
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

    descriptor_send(d, "Huh?!\r\n");
    return true;
}
