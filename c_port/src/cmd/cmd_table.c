#include "cmd.h"

#include <ctype.h>
#include <string.h>

#include "being.h"
#include "cmd_internal.h"

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
    { "ne",        cmd_northeast, "Walk northeast (alias).",                        MORTAL_LEVEL_MIN },
    { "nw",        cmd_northwest, "Walk northwest (alias).",                        MORTAL_LEVEL_MIN },
    { "se",        cmd_southeast, "Walk southeast (alias).",                        MORTAL_LEVEL_MIN },
    { "sw",        cmd_southwest, "Walk southwest (alias).",                        MORTAL_LEVEL_MIN },
    { "look",    cmd_look,    "Look around the room you're in.",                    MORTAL_LEVEL_MIN },
    /* "e" is east; "ex"+ reaches exits. */
    { "exits",   cmd_exits,   "List this room's exits and where they lead.",        MORTAL_LEVEL_MIN },
    { "who",     cmd_who,     "List everyone currently playing.",                   MORTAL_LEVEL_MIN },
    { "score",   cmd_score,   "Show your character's stats, level, and HP.",        MORTAL_LEVEL_MIN },
    { "color",   cmd_color,   "Toggle ANSI color rendering on or off.",             MORTAL_LEVEL_MIN },
    { "attack",  cmd_attack,  "Attack another player in the room.",                 MORTAL_LEVEL_MIN },
    { "kill",    cmd_kill,    "Attack another player (instant for immortals).",     MORTAL_LEVEL_MIN },
    { "say",     cmd_say,     "Say something to everyone in the room.",             MORTAL_LEVEL_MIN },
    { "limbs",   cmd_limbs,   "Show the current health of all your limbs.",         MORTAL_LEVEL_MIN },
    { "help",    cmd_help,    "List available commands.",                           MORTAL_LEVEL_MIN },
    /* Hidden from mortals entirely (Tier 3): players only ever see help
     * for what they can use. */
    { "wizhelp", cmd_wizhelp, "List immortal-only commands.",                       IMMORTAL_LEVEL_MIN },
    { "goto",    cmd_goto,    "Teleport to a room by vnum.",                        IMMORTAL_LEVEL_MIN },
    /* "l" look, "li" limbs, "lo" look, "loa"+ loadroom, "log" log. */
    { "loadroom", cmd_loadroom, "Set the room your character logs in at.",          IMMORTAL_LEVEL_MIN },
    { "promote", cmd_promote, "Set a player's level (up to your own).",             PROMOTE_MIN_LEVEL },
    /* NOTE: must stay after "help" in this table -- "h"/"he"/"hel" should
     * abbreviate to help (first match wins), "hed"+ reaches hedit. Same
     * deal for copyover after color: "c"/"co" reach color, "cop"+ this. */
    { "hedit",   cmd_hedit,   "Edit a help topic in the line editor.",              HELP_EDIT_MIN_LEVEL },
    { "copyover", cmd_copyover, "Reboot the server in place; nobody is disconnected.", COPYOVER_MIN_LEVEL },
    /* "log" needs its three letters ("l" look, "li" limbs, "lo" look). */
    { "redit",   cmd_edit,    "Edit the room you are standing in (builder).",       BUILD_MIN_LEVEL },
    { "log",     cmd_log,     "Read, search, list, or rotate the game logs.",       LOG_MIN_LEVEL },
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

    descriptor_send(d, "Huh?!\r\n");
    return true;
}
