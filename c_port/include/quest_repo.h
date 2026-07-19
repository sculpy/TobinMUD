/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_QUEST_REPO_H
#define TOBIN_QUEST_REPO_H

#include <stdbool.h>
#include <stddef.h>

/* DB access for `player_quest`/`quest_def` (db/sneezy/tobin_migrations.sql)
 * -- Sneezy → Tobin feature audit, "Quest system". User, AskUserQuestion
 * 2026-07-19: infrastructure only. Sneezy's real quest system is a fixed
 * 454-bit array (`toggles[]`) tied entirely to hand-authored content that
 * doesn't exist in Tobin -- specific quests ("Avenger", "Silverclaw",
 * "Holy Devastator"), named NPCs, dialogue trees driven by spec
 * procedures. Porting the bit array itself would just be 454 meaningless
 * numbers with nothing behind them. What actually generalizes is the
 * SHAPE of the system: a player's progress through a named thing, tracked
 * as a small integer, made visible only where an immortal has written a
 * description for that exact step -- so that's what this ports, using a
 * human-readable quest name + stage instead of a raw bit index.
 *
 * Deliberately does NOT include any conditional trigger-script hooks
 * ("if player has stage 2, say X") -- Tobin's trigger language
 * (wait/echo/echoroom/emote/say/teleport/give/damage/log, trigger.c) has
 * no branching at all yet, so there's no way to drive quest logic
 * automatically even with this storage in place. For now, advancing a
 * player's stage is a manual immortal action (`set <player> quest <name>
 * <stage>`, cmd_set.c) -- real automated quest-giving mobs are a future
 * session's work once conditional scripting exists. */

#define QUEST_NAME_LEN 64

typedef struct {
    char name[QUEST_NAME_LEN];
    int stage;
} quest_entry_t;

/* player_id's current stage for `quest_name`, or 0 if they've never
 * touched it (no row = stage 0, same "absence means zero" convention as
 * the original's unset bits). */
int quest_repo_get_stage(long player_id, const char *quest_name);

/* Sets player_id's stage for `quest_name`. `stage <= 0` deletes the row
 * entirely (matches setting every one of the original's bits for a quest
 * back to 0 -- "not started"), rather than persisting a redundant
 * stage-0 row. Returns false only on a real DB error. */
bool quest_repo_set_stage(long player_id, const char *quest_name, int stage);

/* Fills `out` (up to `max` entries) with every quest player_id has a
 * nonzero stage in, alphabetically by name. Returns the count written --
 * NOT filtered by quest_def existence (cmd_quest.c's `quest` command does
 * that filtering itself, matching the original's "only bits with a help
 * file are visible" rule at the display layer, not the storage layer). */
int quest_repo_list_player(long player_id, quest_entry_t *out, int max);

/* The immortal-authored description for (quest_name, stage), written into
 * `buf` (size `bufsz`). Returns false if no such description exists --
 * this IS the "only visible if it has a help file" gate from the
 * original, just a DB lookup instead of a filesystem check. */
bool quest_repo_def_get(const char *quest_name, int stage, char *buf, size_t bufsz);

/* Creates or replaces the description for (quest_name, stage). Immortal-
 * only (`questdef`, cmd_questdef.c) -- no menu editor, same "no in-game
 * editor for it yet" precedent as several other content types in this
 * codebase; a builder edits an existing description by just running
 * `questdef` again with the corrected text. */
bool quest_repo_def_set(const char *quest_name, int stage, const char *description);

#endif
