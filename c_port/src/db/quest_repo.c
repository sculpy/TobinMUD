/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "quest_repo.h"

#include <stdio.h>
#include <stdlib.h>

#include "db.h"

/* Returns a player's current stage in a quest (0 if not started/no row). */
int quest_repo_get_stage(long player_id, const char *quest_name) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int stage = 0;
    if (db_query(db, "select stage from player_quest where player_id=%i and quest_name='%s'",
                 (int)player_id, quest_name)
        && db_fetch_row(db))
        stage = atoi(db_get(db, "stage"));

    db_close(db);
    return stage;
}

/* Sets a player's stage in a quest. A stage <= 0 deletes the row entirely
 * (treated as "not started") rather than storing a zero/negative value. */
bool quest_repo_set_stage(long player_id, const char *quest_name, int stage) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok;
    if (stage <= 0) {
        ok = db_query(db, "delete from player_quest where player_id=%i and quest_name='%s'",
                      (int)player_id, quest_name);
    } else {
        ok = db_query(db,
            "insert into player_quest (player_id, quest_name, stage) values (%i, '%s', %i) "
            "on duplicate key update stage=%i",
            (int)player_id, quest_name, stage, stage);
    }

    db_close(db);
    return ok;
}

/* Lists every quest a player has actually started (stage > 0), name-sorted,
 * up to max entries -- backs a player's quest-progress listing. */
int quest_repo_list_player(long player_id, quest_entry_t *out, int max) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    int n = 0;
    if (db_query(db,
                "select quest_name, stage from player_quest where player_id=%i and stage > 0 "
                "order by quest_name",
                (int)player_id)) {
        while (n < max && db_fetch_row(db)) {
            snprintf(out[n].name, QUEST_NAME_LEN, "%s", db_get(db, "quest_name"));
            out[n].stage = atoi(db_get(db, "stage"));
            n++;
        }
    }

    db_close(db);
    return n;
}

/* Looks up the static description text for a given quest/stage pair from
 * quest_def (the quest's authored content, as opposed to a player's
 * per-player progress). */
bool quest_repo_def_get(const char *quest_name, int stage, char *buf, size_t bufsz) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select description from quest_def where quest_name='%s' and stage=%i",
                 quest_name, stage)
        && db_fetch_row(db)) {
        snprintf(buf, bufsz, "%s", db_get(db, "description"));
        found = true;
    }

    db_close(db);
    return found;
}

/* Creates or updates a quest/stage's description text in quest_def. */
bool quest_repo_def_set(const char *quest_name, int stage, const char *description) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into quest_def (quest_name, stage, description) values ('%s', %i, '%s') "
        "on duplicate key update description='%s'",
        quest_name, stage, description, description);

    db_close(db);
    return ok;
}
/* Per-race quest reward items (Sneezy -> Tobin feature audit -- see the
 * `quest_item`/`player_quest_item_claimed` doc comment in
 * tobin_migrations.sql for why this is a Tobin-original addition, not a
 * port). Returns the obj_vnum an immortal set for (quest_name, stage,
 * race) via `questitem`, or -1 if none is defined for that exact triple
 * (no fallback to a different race -- an unset combination just has no
 * reward, same "absence means nothing" convention as quest_def). */
int quest_repo_reward_item(const char *quest_name, int stage, player_race_t race) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;
    int vnum = -1;
    if (db_query(db,
                "select obj_vnum from quest_item where quest_name='%s' and stage=%i and race=%i",
                quest_name, stage, (int)race)
        && db_fetch_row(db))
        vnum = atoi(db_get(db, "obj_vnum"));
    db_close(db);
    return vnum;
}
/* Creates or replaces the reward item for (quest_name, stage, race). */
bool quest_repo_reward_set(const char *quest_name, int stage, player_race_t race, int obj_vnum) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db,
        "insert into quest_item (quest_name, stage, race, obj_vnum) values ('%s', %i, %i, %i) "
        "on duplicate key update obj_vnum=%i",
        quest_name, stage, (int)race, obj_vnum, obj_vnum);
    db_close(db);
    return ok;
}
/* True if player_id has already been handed their (quest_name, stage)
 * reward item -- `quest claim` (cmd_quest.c) checks this first so
 * re-running it can't duplicate the item. */
bool quest_repo_reward_claimed(long player_id, const char *quest_name, int stage) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool claimed = false;
    if (db_query(db,
                "select 1 from player_quest_item_claimed where player_id=%i and quest_name='%s' and stage=%i",
                (int)player_id, quest_name, stage)
        && db_fetch_row(db))
        claimed = true;
    db_close(db);
    return claimed;
}
/* Records that player_id has now claimed their (quest_name, stage)
 * reward -- called right after the object is actually granted
 * (cmd_quest.c), never before, so a DB failure here can't mark a claim
 * that never actually happened. */
bool quest_repo_reward_mark_claimed(long player_id, const char *quest_name, int stage) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db,
        "insert into player_quest_item_claimed (player_id, quest_name, stage) values (%i, '%s', %i) "
        "on duplicate key update claimed_at=claimed_at",
        (int)player_id, quest_name, stage);
    db_close(db);
    return ok;
}
