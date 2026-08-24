/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "player_repo.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>

#include "affect_repo.h"
#include "db.h"
#include "drug_repo.h"
#include "log.h"
#include "obj_repo.h"
#include "suit.h"
#include "suit_repo.h"

/* Flat offline-regen rate for a character returning from `rent`
 * (cmd_rent.c) -- 1 HP per this many real seconds elapsed since renting,
 * capped at max_hp. Deliberately simple (not the same curve as the
 * online regen_tick_run()/regen_amount() in regen.c, which factor in
 * CON and position) since a rented-out character has no position to
 * read and this only needs to feel like real "time passed, you
 * recovered" progress, not a precisely balanced number. */
#define RENT_REGEN_SECONDS_PER_HP 5

/* Default landing room for freshly created characters -- vnum 100 ("Center Square")
 * exists in the seed data (db/tobin/room.sql) and has a real description,
 * unlike vnum 0 ("The Void"). Revisit once zone-aware character creation
 * (starting city selection etc) is ported. */
#define DEFAULT_LOAD_ROOM DEFAULT_LOAD_ROOM_MORTAL

/* Standing safeguard (user request, 2026-07-07): the real character named
 * Jesus is always level 60, full stop. Added after the player_attrs.sql/
 * player_progress.sql DROP-TABLE bug (see STATUS.md) silently reset nearly
 * every player's level -- rather than rely on a one-off manual SQL fix,
 * self-heals on every load. A no-op once already correct. */
static void ensure_jesus_level(being_t *b) {
    if (b && strcasecmp(b->base.name, "Jesus") == 0 && b->progress.level != 60) {
        b->progress.level = 60;
        player_progress_save(b->player_id, &b->progress);
    }
}

/* Full character load for actually entering the game: creates a being_t
 * from the player row, then pulls in attrs, progress, drugs, affects, and
 * inventory from their respective repos, applies the rent-regen healing
 * and limb full-heal fixups, and self-heals the Jesus-is-always-60
 * safeguard. This is the "real" load path -- see player_load_admin() below
 * for the lighter snapshot used by admin tools that must not attach live
 * inventory. */
being_t *player_load(const char *name, long account_id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return NULL;

    being_t *b = NULL;
    if (db_query(db, "select id, name, account_id, load_room, handed, prompt_flags, "
                      "title, gender, appearance, pflags, monk_quest_flags, monk_purple_kills, poofin, poofout, bamfin, bamfout, "
                      "class, race, territory, height, weight, start_age from player "
                      "where name='%s' and account_id=%i",
                 name, (int)account_id)
        && db_fetch_row(db)) {
        long player_id = atol(db_get(db, "id"));
        b = being_create_pc(db_get(db, "name"), account_id, player_id);
        if (b) {
            b->handed_right = atoi(db_get(db, "handed"));
            b->prompt_flags = atoi(db_get(db, "prompt_flags"));
            snprintf(b->title, sizeof(b->title), "%s", db_get(db, "title"));
            b->gender = (gender_t)atoi(db_get(db, "gender"));
            snprintf(b->appearance, sizeof(b->appearance), "%s", db_get(db, "appearance"));
            b->pflags = atoi(db_get(db, "pflags"));
            b->monk_quest_flags = (unsigned int)strtoul(db_get(db, "monk_quest_flags"), NULL, 10);
            b->monk_purple_kills = (unsigned char)atoi(db_get(db, "monk_purple_kills"));
            snprintf(b->poofin, sizeof(b->poofin), "%s", db_get(db, "poofin"));
            snprintf(b->poofout, sizeof(b->poofout), "%s", db_get(db, "poofout"));
            snprintf(b->bamfin, sizeof(b->bamfin), "%s", db_get(db, "bamfin"));
            snprintf(b->bamfout, sizeof(b->bamfout), "%s", db_get(db, "bamfout"));
            b->char_class = (player_class_t)atoi(db_get(db, "class"));
            b->race = (player_race_t)atoi(db_get(db, "race"));
            b->territory = (player_territory_t)atoi(db_get(db, "territory"));
            b->height = atoi(db_get(db, "height"));
            b->weight = atoi(db_get(db, "weight"));
            b->start_age = atoi(db_get(db, "start_age"));
            /* Body type (Sneezy -> Tobin feature audit) -- kept consistent with player_create() so a character logged in before this migration also gets the real per-race lookup, not just being_create_pc()'s hardcoded default. */
            b->body_type = race_body_type(b->race);
        }
    }

    db_close(db);

    if (b) {
        player_attrs_load(b->player_id, &b->attrs); /* falls back to ATTR_BASE defaults if missing */
        player_progress_load(b->player_id, &b->progress); /* falls back to being_create_pc()'s defaults if missing */
        drug_repo_load_all(b->player_id, b->drugs); /* rows with no history leave the calloc'd all-zero defaults alone */
        affect_repo_load_all(b->player_id, b->affects); /* bookkeeping only -- see affect_repo.h, attrs above already carries any baked-in modifier */
        /* Returning from `rent` (cmd_rent.c): heal for the real time spent
         * rented out, then clear the marker so this only fires once. */
        if (b->progress.rented_at > 0) {
            long elapsed = (long)time(NULL) - b->progress.rented_at;
            if (elapsed > 0) {
                int healed = (int)(elapsed / RENT_REGEN_SECONDS_PER_HP);
                b->progress.hp += healed;
                if (b->progress.hp > b->progress.max_hp)
                    b->progress.hp = b->progress.max_hp;
            }
            b->progress.rented_at = 0;
            player_progress_save(b->player_id, &b->progress);
        }
        /* being_create_pc() already sized limbs off level-1 defaults before
         * the real (possibly much higher) level/max_hp landed just above --
         * without this, every reconnect for a leveled character leaves limbs
         * stuck at level-1-sized fractions, making them trivially
         * decapitatable regardless of real max_hp (found 2026-07-12 while
         * testing the affects system). */
        being_limbs_full_heal(b);
        /* Same staleness class as the mana fix just below, for max_hp/
         * max_vit: both are also stored-verbatim values that only ever
         * get refreshed by whatever code path happens to touch them
         * (e.g. leveling up) -- a character whose row predates a
         * formula change (like the 2026-08-06 real-SneezyMUD HP/
         * Vitality rework) keeps showing the OLD ceiling forever
         * otherwise. CEILING ONLY, never auto-heals: current hp/vit
         * are clamped DOWN if the recompute ever lowers the max below
         * what was stored, but never bumped up to a newly-higher max --
         * relogging must not become a free full-heal exploit. */
        b->progress.max_hp = being_calc_max_hp(b);
        if (b->progress.hp > b->progress.max_hp)
            b->progress.hp = b->progress.max_hp;
        b->progress.max_vit = being_calc_max_vit(b);
        if (b->progress.vit > b->progress.max_vit)
            b->progress.vit = b->progress.max_vit;
        /* Same "stored value can go stale, recompute on login" fix as
         * being_limbs_full_heal() just above -- user, 2026-08-06:
         * "mana isnt updating, meditated to full still reads 0".
         * player_progress_load() (called above) trusts whatever
         * max_mana was last SAVED to the DB verbatim; a character whose
         * row predates the real mana pool (or was otherwise never
         * saved with a real value) stays stuck at max_mana<=0 forever,
         * since being_heal_mana() treats that as "no mana pool at all"
         * and silently no-ops -- meditate genuinely was restoring
         * nothing, not failing to report a real gain. being_calc_max_
         * mana() is cheap and deterministic (class + current "mana"
         * skill proficiency only), so recomputing it on every login is
         * always safe and self-healing, matching the limb-heal
         * precedent right above. Clamp current mana down too, in the
         * (currently impossible, since proficiency only ever grows)
         * case a recompute ever lowers the ceiling below what was
         * stored. */
        b->progress.max_mana = being_calc_max_mana(b);
        if (b->progress.mana > b->progress.max_mana)
            b->progress.mana = b->progress.max_mana;
        b->progress.max_piety = being_calc_max_piety(b);
        if (b->progress.piety > b->progress.max_piety)
            b->progress.piety = b->progress.max_piety;
        player_inventory_load(b->player_id, b); /* recreates carried/worn/held instances, see obj_repo.h */
        ensure_jesus_level(b);
    }

    return b;
}

being_t *player_create(const char *name, long account_id, const attrs_t *attrs,
                       int handed_right, gender_t gender, const char *appearance,
                       player_class_t char_class, player_race_t race, int alignment,
                       player_territory_t territory) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return NULL;

    db_query(db, "select 1 from player where name='%s'", name);
    if (db_fetch_row(db)) {
        log_error("player_create: name '%s' already taken", name);
        db_close(db);
        return NULL;
    }

    db_query(db, "select count(*) as n from player where account_id=%i", (int)account_id);
    if (db_fetch_row(db) && atoi(db_get(db, "n")) >= MAX_CHARS_PER_ACCOUNT) {
        log_error("player_create: account %ld already has %d characters", account_id, MAX_CHARS_PER_ACCOUNT);
        db_close(db);
        return NULL;
    }

    /* Height/weight/age (Sneezy -> Tobin feature audit -- docs/RACE_STATS.md's/
     * RACE_PERKS.md's "Not imported" list): rolled ONCE here, off the
     * chosen race's (and gender's, for height/weight) own SneezyMUD dice
     * formula (race_roll_height()/race_roll_weight()/race_roll_age(),
     * being.c) -- same "roll once at creation, persist forever" precedent
     * as everything else in this function. */
    int rolled_height = race_roll_height(race, gender);
    int rolled_weight = race_roll_weight(race, gender);
    int rolled_start_age = race_roll_age(race);
    bool ok = db_query(db,
        "insert into player (name, talens, account_id, load_room, nutrition, handed, gender, appearance, class, race, territory, height, weight, start_age) "
        "values ('%s', 0, %i, %i, 100, %i, %i, '%s', %i, %i, %i, %i, %i, %i)",
        name, (int)account_id, DEFAULT_LOAD_ROOM, handed_right ? 1 : 0,
        (int)gender, appearance ? appearance : "", (int)char_class, (int)race, (int)territory,
        rolled_height, rolled_weight, rolled_start_age);

    being_t *b = NULL;
    if (ok) {
        long player_id = db_last_insert_id(db);
        b = being_create_pc(name, account_id, player_id);
        if (b) {
            b->handed_right = handed_right ? 1 : 0;
            b->gender = gender;
            b->char_class = char_class;
            b->race = race;
            b->territory = territory;
            b->height = rolled_height;
            b->weight = rolled_weight;
            b->start_age = rolled_start_age;
            /* Body type (Sneezy -> Tobin feature audit): race_body_type()
             * replaces being_create_pc()'s bare BODY_HUMANOID hardcode
             * now that the real race is known -- see being.h's
             * race_body_type() doc comment for why every current PC race
             * resolves the same way anyway. */
            b->body_type = race_body_type(race);
            b->pflags = PLR_NEWBIE; /* new players start on the newbie channel */
            snprintf(b->appearance, sizeof(b->appearance), "%s", appearance ? appearance : "");
            if (attrs)
                b->attrs = *attrs;
            b->progress.alignment = alignment;
            /* being_create_pc() already computed max_hp/limb HP, but from
             * default ATTR_BASE attrs and char_class 0 (CLASS_MAGE) --
             * BEFORE the real race/class-adjusted attrs and chosen class
             * were set just above. Recompute now that both are final, or a
             * Warrior's real HP bonus (etc.) would never actually apply. */
            b->progress.max_hp = being_calc_max_hp(b);
            b->progress.hp = b->progress.max_hp;
            being_limbs_full_heal(b);
            /* Same recompute-after-real-attrs fix as max_hp just above --
             * being_create_pc()'s max_vit was sized off default attrs too. */
            b->progress.max_vit = being_calc_max_vit(b);
            b->progress.vit = b->progress.max_vit;
            b->progress.max_mana = being_calc_max_mana(b);
            b->progress.mana = b->progress.max_mana;
            b->progress.max_piety = being_calc_max_piety(b);
            b->progress.piety = b->progress.max_piety;
            player_attrs_save(player_id, &b->attrs);
            player_progress_save(player_id, &b->progress);
            /* Newbie equipment suit (user 2026-07-26): "load on the
             * character when connecting for the first time" -- this IS
             * that first load, the one-time actual character-creation
             * path (not every login). Silently does nothing if no suit
             * is defined for this class yet (suit.sql seeds all 6). */
            int suit_id = suit_repo_find_for_class((int)char_class);
            if (suit_id >= 0)
                suit_grant(b, suit_id);
            /* Newbie equipment system expansion (TODO.md priority item,
             * 2026-08-02): a SECOND, independent suit grant by race --
             * every character now also gets their race's own 11-piece
             * starting armor set + racial weapon + shared training
             * shield (newbie_gear_race.sql), on top of whatever their
             * class suit already gave them. */
            int race_suit_id = suit_repo_find_for_race((int)race);
            if (race_suit_id >= 0)
                suit_grant(b, race_suit_id);
        }
    }

    db_close(db);
    return b;
}

/* Permanently deletes a player character, scoped to the owning account so
 * one account can't delete another's character by name collision. */
bool player_delete(const char *name, long account_id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    db_query(db, "select 1 from player where name='%s' and account_id=%i", name, (int)account_id);
    if (!db_fetch_row(db)) {
        db_close(db);
        return false;
    }

    /* player_attrs is removed automatically via ON DELETE CASCADE. */
    bool ok = db_query(db, "delete from player where name='%s' and account_id=%i", name, (int)account_id);

    db_close(db);
    return ok;
}

/* Lists an account's characters (name + level, defaulting to level 1 if
 * no progress row yet) for the character-selection menu at login. */
bool player_list_by_account(long account_id, char names[][PLAYER_NAME_LEN], int levels[], int max, int *count) {
    *count = 0;

    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
                       "select p.name, coalesce(pp.level, 1) as level"
                       " from player p left join player_progress pp on pp.player_id = p.id"
                       " where p.account_id=%i order by coalesce(pp.level, 1) desc, p.name limit %i",
                       (int)account_id, max);
    if (ok) {
        while (*count < max && db_fetch_row(db)) {
            snprintf(names[*count], PLAYER_NAME_LEN, "%s", db_get(db, "name"));
            if (levels)
                levels[*count] = atoi(db_get(db, "level"));
            (*count)++;
        }
    }

    db_close(db);
    return ok;
}

/* Fetches just a character's saved load_room vnum, without loading the
 * whole being -- used by the login flow to know where to place the player
 * before the full player_load() runs. Returns -1 if not found. */
int player_load_room(const char *name, long account_id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;

    int room_vnum = -1;
    if (db_query(db, "select load_room from player where name='%s' and account_id=%i",
                 name, (int)account_id)
        && db_fetch_row(db)) {
        room_vnum = atoi(db_get(db, "load_room"));
    }

    db_close(db);
    return room_vnum;
}

/* True if any character (in any account) already has this name -- player
 * names are globally unique, so character creation checks this before
 * allowing a new name. */
bool player_name_exists(const char *name) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool found = db_query(db, "select id from player where name='%s'", name)
                 && db_fetch_row(db);
    db_close(db);
    return found;
}

/* Resolves a character name to its player id. Returns -1 if not found. */
long player_id_for_name(const char *name) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;
    long id = -1;
    if (db_query(db, "select id from player where name='%s'", name) && db_fetch_row(db))
        id = atol(db_get(db, "id"));
    db_close(db);
    return id;
}

/* Resolves a character name to its owning account id. Returns -1 if not
 * found. */
long player_account_id_for_name(const char *name) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return -1;
    long account_id = -1;
    if (db_query(db, "select account_id from player where name='%s'", name) && db_fetch_row(db))
        account_id = atol(db_get(db, "account_id"));
    db_close(db);
    return account_id;
}

/* Updates where a character logs back in next time (e.g. after `rent`,
 * a recall, or a death respawn). */
bool player_set_load_room(const char *name, long account_id, int vnum) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db, "update player set load_room=%i where name='%s' and account_id=%i",
                       vnum, name, (int)account_id);

    db_close(db);
    return ok;
}

/* Sets (or clears, if title is empty) a character's displayed title. */
bool player_set_title(const char *name, long account_id, const char *title) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    /* An empty title clears the column (SQL NULL); db_query's %s escapes the
     * free-form title text, so no injection through player-supplied strings. */
    bool ok;
    if (title && title[0])
        ok = db_query(db, "update player set title='%s' where name='%s' and account_id=%i",
                      title, name, (int)account_id);
    else
        ok = db_query(db, "update player set title=NULL where name='%s' and account_id=%i",
                      name, (int)account_id);

    db_close(db);
    return ok;
}

/* Sets (or clears) a character's custom poofin message (shown to the room
 * when the immortal appears via goto/teleport). Same NULL-clear-on-empty
 * pattern as player_set_title() above. */
bool player_set_poofin(const char *name, long account_id, const char *msg) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok;
    if (msg && msg[0])
        ok = db_query(db, "update player set poofin='%s' where name='%s' and account_id=%i",
                      msg, name, (int)account_id);
    else
        ok = db_query(db, "update player set poofin=NULL where name='%s' and account_id=%i",
                      name, (int)account_id);

    db_close(db);
    return ok;
}

/* Sets (or clears) a character's custom poofout message (shown to the room
 * when the immortal leaves via goto/teleport). */
bool player_set_poofout(const char *name, long account_id, const char *msg) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok;
    if (msg && msg[0])
        ok = db_query(db, "update player set poofout='%s' where name='%s' and account_id=%i",
                      msg, name, (int)account_id);
    else
        ok = db_query(db, "update player set poofout=NULL where name='%s' and account_id=%i",
                      name, (int)account_id);

    db_close(db);
    return ok;
}

/* Sets (or clears) a character's custom bamfin message (shown when they
 * arrive via a `bamf`-style summon/recall effect). */
bool player_set_bamfin(const char *name, long account_id, const char *msg) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok;
    if (msg && msg[0])
        ok = db_query(db, "update player set bamfin='%s' where name='%s' and account_id=%i",
                      msg, name, (int)account_id);
    else
        ok = db_query(db, "update player set bamfin=NULL where name='%s' and account_id=%i",
                      name, (int)account_id);

    db_close(db);
    return ok;
}

/* Sets (or clears) a character's custom bamfout message (shown when they
 * depart via a `bamf`-style summon/recall effect). */
bool player_set_bamfout(const char *name, long account_id, const char *msg) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok;
    if (msg && msg[0])
        ok = db_query(db, "update player set bamfout='%s' where name='%s' and account_id=%i",
                      msg, name, (int)account_id);
    else
        ok = db_query(db, "update player set bamfout=NULL where name='%s' and account_id=%i",
                      name, (int)account_id);

    db_close(db);
    return ok;
}

/* Loads a character's six core stats from player_attrs. Returns false if
 * no row exists (player_load() falls back to ATTR_BASE defaults in that
 * case). */
bool player_attrs_load(long player_id, attrs_t *out) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select strength, dexterity, constitution, intelligence, wisdom, charisma "
                      "from player_attrs where player_id=%i",
                 (int)player_id)
        && db_fetch_row(db)) {
        out->strength = atoi(db_get(db, "strength"));
        out->dexterity = atoi(db_get(db, "dexterity"));
        out->constitution = atoi(db_get(db, "constitution"));
        out->intelligence = atoi(db_get(db, "intelligence"));
        out->wisdom = atoi(db_get(db, "wisdom"));
        out->charisma = atoi(db_get(db, "charisma"));
        found = true;
    }

    db_close(db);
    return found;
}

/* Upserts a character's six core stats. */
bool player_attrs_save(long player_id, const attrs_t *attrs) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into player_attrs (player_id, strength, dexterity, constitution, "
        "intelligence, wisdom, charisma) values (%i, %i, %i, %i, %i, %i, %i) "
        "on duplicate key update strength=%i, dexterity=%i, constitution=%i, "
        "intelligence=%i, wisdom=%i, charisma=%i",
        (int)player_id, attrs->strength, attrs->dexterity, attrs->constitution,
        attrs->intelligence, attrs->wisdom, attrs->charisma,
        attrs->strength, attrs->dexterity, attrs->constitution,
        attrs->intelligence, attrs->wisdom, attrs->charisma);

    db_close(db);
    return ok;
}

/* Loads a character's level/xp/hp/gold/hunger-thirst/etc progress fields
 * from player_progress. Returns false if no row exists (player_load()
 * falls back to being_create_pc()'s defaults in that case). */
bool player_progress_load(long player_id, progress_t *out) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool found = false;
    if (db_query(db, "select level, experience, hp, max_hp, true_level, alignment, "
                      "basic_disc_pct, advanced_disc_pct, combat_disc_pct, practice_points, "
                      "rented_at, gold, bank_gold, hunger, thirst, drunk, birth_time, vit, max_vit, "
                      "mana, max_mana, piety, max_piety "
                      "from player_progress where player_id=%i",
                 (int)player_id)
        && db_fetch_row(db)) {
        out->level = atoi(db_get(db, "level"));
        out->experience = atol(db_get(db, "experience"));
        out->hp = atoi(db_get(db, "hp"));
        out->max_hp = atoi(db_get(db, "max_hp"));
        out->true_level = atoi(db_get(db, "true_level"));
        out->alignment = atoi(db_get(db, "alignment"));
        out->basic_disc_pct = atoi(db_get(db, "basic_disc_pct"));
        out->advanced_disc_pct = atoi(db_get(db, "advanced_disc_pct"));
        out->combat_disc_pct = atoi(db_get(db, "combat_disc_pct"));
        out->practice_points = atoi(db_get(db, "practice_points"));
        out->rented_at = atol(db_get(db, "rented_at"));
        out->gold = atoi(db_get(db, "gold"));
        out->bank_gold = atoi(db_get(db, "bank_gold"));
        out->hunger = atoi(db_get(db, "hunger"));
        out->thirst = atoi(db_get(db, "thirst"));
        out->drunk = atoi(db_get(db, "drunk"));
        out->birth_time = atol(db_get(db, "birth_time"));
        out->vit = atoi(db_get(db, "vit"));
        out->max_vit = atoi(db_get(db, "max_vit"));
        out->mana = atoi(db_get(db, "mana"));
        out->max_mana = atoi(db_get(db, "max_mana"));
        out->piety = atoi(db_get(db, "piety"));
        out->max_piety = atoi(db_get(db, "max_piety"));
        found = true;
    }

    db_close(db);
    return found;
}

/* Upserts a character's full progress row (level, xp, hp, gold, hunger/
 * thirst, etc). The main persistence point for anything that changes
 * during play -- called from player_save() and various one-off updates
 * like the rent-regen heal in player_load() above. */
bool player_progress_save(long player_id, const progress_t *progress) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    bool ok = db_query(db,
        "insert into player_progress (player_id, level, experience, hp, max_hp, true_level, alignment, "
        "basic_disc_pct, advanced_disc_pct, combat_disc_pct, practice_points, rented_at, gold, bank_gold, "
        "hunger, thirst, drunk, birth_time, vit, max_vit, mana, max_mana, piety, max_piety) "
        "values (%i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i, %i) "
        "on duplicate key update level=%i, experience=%i, hp=%i, max_hp=%i, true_level=%i, alignment=%i, "
        "basic_disc_pct=%i, advanced_disc_pct=%i, combat_disc_pct=%i, practice_points=%i, rented_at=%i, gold=%i, bank_gold=%i, "
        "hunger=%i, thirst=%i, drunk=%i, birth_time=%i, vit=%i, max_vit=%i, mana=%i, max_mana=%i, piety=%i, max_piety=%i",
        (int)player_id, progress->level, (int)progress->experience, progress->hp, progress->max_hp, progress->true_level, progress->alignment,
        progress->basic_disc_pct, progress->advanced_disc_pct, progress->combat_disc_pct, progress->practice_points, (int)progress->rented_at, progress->gold, progress->bank_gold,
        progress->hunger, progress->thirst, progress->drunk, (int)progress->birth_time, progress->vit, progress->max_vit, progress->mana, progress->max_mana, progress->piety, progress->max_piety,
        progress->level, (int)progress->experience, progress->hp, progress->max_hp, progress->true_level, progress->alignment,
        progress->basic_disc_pct, progress->advanced_disc_pct, progress->combat_disc_pct, progress->practice_points, (int)progress->rented_at, progress->gold, progress->bank_gold,
        progress->hunger, progress->thirst, progress->drunk, (int)progress->birth_time, progress->vit, progress->max_vit, progress->mana, progress->max_mana, progress->piety, progress->max_piety);

    db_close(db);
    return ok;
}

/* Lighter-weight load for admin tools (edplayer, cmd_set) that need a
 * being_t snapshot of any character by name (not scoped to an account, and
 * without the caller needing to already know the account_id). Deliberately
 * skips player_inventory_load() -- see the comment below for why attaching
 * live inventory objects to a throwaway snapshot is unsafe. Not the login
 * path; use player_load() for that. */
being_t *player_load_admin(const char *name, int *out_load_room) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return NULL;

    being_t *b = NULL;
    int load_room = -1;
    if (db_query(db, "select id, name, account_id, load_room, handed, prompt_flags, "
                      "title, gender, appearance, pflags, monk_quest_flags, monk_purple_kills, poofin, poofout, bamfin, bamfout, "
                      "class, race, territory, height, weight, start_age from player "
                      "where name='%s'",
                 name)
        && db_fetch_row(db)) {
        long player_id = atol(db_get(db, "id"));
        long account_id = atol(db_get(db, "account_id"));
        b = being_create_pc(db_get(db, "name"), account_id, player_id);
        if (b) {
            b->handed_right = atoi(db_get(db, "handed"));
            b->prompt_flags = atoi(db_get(db, "prompt_flags"));
            snprintf(b->title, sizeof(b->title), "%s", db_get(db, "title"));
            b->gender = (gender_t)atoi(db_get(db, "gender"));
            snprintf(b->appearance, sizeof(b->appearance), "%s", db_get(db, "appearance"));
            b->pflags = atoi(db_get(db, "pflags"));
            b->monk_quest_flags = (unsigned int)strtoul(db_get(db, "monk_quest_flags"), NULL, 10);
            b->monk_purple_kills = (unsigned char)atoi(db_get(db, "monk_purple_kills"));
            snprintf(b->poofin, sizeof(b->poofin), "%s", db_get(db, "poofin"));
            snprintf(b->poofout, sizeof(b->poofout), "%s", db_get(db, "poofout"));
            snprintf(b->bamfin, sizeof(b->bamfin), "%s", db_get(db, "bamfin"));
            snprintf(b->bamfout, sizeof(b->bamfout), "%s", db_get(db, "bamfout"));
            b->char_class = (player_class_t)atoi(db_get(db, "class"));
            b->race = (player_race_t)atoi(db_get(db, "race"));
            b->territory = (player_territory_t)atoi(db_get(db, "territory"));
            b->height = atoi(db_get(db, "height"));
            b->weight = atoi(db_get(db, "weight"));
            b->start_age = atoi(db_get(db, "start_age"));
            /* Body type (Sneezy -> Tobin feature audit) -- kept consistent with player_create() so a character logged in before this migration also gets the real per-race lookup, not just being_create_pc()'s hardcoded default. */
            b->body_type = race_body_type(b->race);
            load_room = atoi(db_get(db, "load_room"));
        }
    }

    db_close(db);

    if (b) {
        player_attrs_load(b->player_id, &b->attrs);
        player_progress_load(b->player_id, &b->progress);
        ensure_jesus_level(b);
        /* Deliberately NOT player_inventory_load() here: edplayer/cmd_set
         * both do `d->edplayer_work = *loaded;` / mutate-then-destroy on
         * this snapshot (a shallow struct copy) -- an object attached via
         * thing_move_to() would leave a dangling pointer the moment
         * being_destroy(loaded) frees it, since being_destroy() now frees
         * everything in the being's containment chain (see being.c). An
         * admin snapshot has no need to inspect inventory in this pass. */
    }
    if (out_load_room)
        *out_load_room = load_room;

    return b;
}

/* Admin-tool setters (cmd_set) that update a single player column by
 * character name, without needing a loaded being_t or account_id. */
bool player_set_gender_by_name(const char *name, gender_t gender) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db, "update player set gender=%i where name='%s'",
                       (int)gender, name);
    db_close(db);
    return ok;
}

/* Same admin-setter pattern as player_set_gender_by_name() above, for
 * handedness. */
bool player_set_handed_by_name(const char *name, int handed_right) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db, "update player set handed=%i where name='%s'",
                       handed_right ? 1 : 0, name);
    db_close(db);
    return ok;
}

/* Same admin-setter pattern as player_set_gender_by_name() above, for
 * character class. */
bool player_set_class_by_name(const char *name, player_class_t cls) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db, "update player set class=%i where name='%s'",
                       (int)cls, name);
    db_close(db);
    return ok;
}

/* Same admin-setter pattern as player_set_gender_by_name() above, for
 * race. */
bool player_set_race_by_name(const char *name, player_race_t race) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db, "update player set race=%i where name='%s'",
                       (int)race, name);
    db_close(db);
    return ok;
}

/* Same admin-setter pattern as player_set_gender_by_name() above, for
 * the appearance description text. */
bool player_set_appearance_by_name(const char *name, const char *appearance) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db, "update player set appearance='%s' where name='%s'",
                       appearance ? appearance : "", name);
    db_close(db);
    return ok;
}

/* Admin-tool level setter (cmd_set, "promote"-style edits). Unlike the
 * other player_set_*_by_name() setters above, level lives in
 * player_progress, not the player table, so this upserts a progress row
 * (with the historical 100/100 HP defaults for a player_progress row that
 * somehow doesn't exist yet) instead of a plain UPDATE. */
bool player_set_level_by_name(const char *name, int level) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;

    long player_id = -1;
    if (db_query(db, "select id from player where name='%s'", name) && db_fetch_row(db))
        player_id = atol(db_get(db, "id"));

    if (player_id < 0) {
        db_close(db);
        return false;
    }

    /* A Tobin-created player always has a progress row (player_create seeds
     * one), but tolerate a missing row anyway with the same 100/100 HP
     * defaults the manual promotion SQL has used since Session 11. */
    bool ok = db_query(db,
        "insert into player_progress (player_id, level, experience, hp, max_hp) "
        "values (%i, %i, 0, 100, 100) "
        "on duplicate key update level=%i",
        (int)player_id, level, level);

    db_close(db);
    return ok;
}

/* Updates a player's saved prompt display flags (which pieces of info
 * show up in their combat/status prompt). */
bool player_set_prompt_flags(long player_id, int flags) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db, "update player set prompt_flags=%i where id=%i",
                       flags, (int)player_id);
    db_close(db);
    return ok;
}

/* Returns the highest news id a player has already seen, to compare
 * against news_repo_max_id() and decide whether to flag "new news" at
 * login. 0 if the player has never seen any (or the column is NULL). */
long player_get_news_last_seen(long player_id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    long last_seen = 0;
    if (db_query(db, "select news_last_seen_id from player where id=%i", (int)player_id)
        && db_fetch_row(db)) {
        const char *v = db_get(db, "news_last_seen_id");
        if (v)
            last_seen = atol(v);
    }

    db_close(db);
    return last_seen;
}

/* Records the highest news id a player has now seen, e.g. after reading
 * news at login. */
bool player_set_news_last_seen(long player_id, long news_id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db, "update player set news_last_seen_id=%i where id=%i",
                       (int)news_id, (int)player_id);
    db_close(db);
    return ok;
}

/* Same as player_get_news_last_seen() above, but for the immortal-only
 * wiznews feed. */
long player_get_wiznews_last_seen(long player_id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return 0;

    long last_seen = 0;
    if (db_query(db, "select wiznews_last_seen_id from player where id=%i", (int)player_id)
        && db_fetch_row(db)) {
        const char *v = db_get(db, "wiznews_last_seen_id");
        if (v)
            last_seen = atol(v);
    }

    db_close(db);
    return last_seen;
}

/* Same as player_set_news_last_seen() above, but for wiznews. */
bool player_set_wiznews_last_seen(long player_id, long news_id) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db, "update player set wiznews_last_seen_id=%i where id=%i",
                       (int)news_id, (int)player_id);
    db_close(db);
    return ok;
}

/* Updates a player's saved pflags bitmask (e.g. channel toggles, the
 * newbie flag set at creation). */
bool player_set_pflags(long player_id, int flags) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db, "update player set pflags=%i where id=%i",
                       flags, (int)player_id);
    db_close(db);
    return ok;
}
/* Updates a player's saved monk sash quest-chain progress bits (white
 * belt through black sash) and purple-sash leper kill counter. Called by
 * monk_quest.c whenever a stage bit or the kill counter changes. */
bool player_set_monk_quest(long player_id, unsigned int flags, unsigned char purple_kills) {
    db_conn_t *db = db_open(DB_TOBIN);
    if (!db)
        return false;
    bool ok = db_query(db, "update player set monk_quest_flags=%i, monk_purple_kills=%i where id=%i",
                       (int)flags, (int)purple_kills, (int)player_id);
    db_close(db);
    return ok;
}

/* Top-level "save this character" entry point: persists attrs, progress,
 * inventory, and active affects in one call. Intentionally runs all four
 * saves even if an earlier one fails (accumulating ok via `&&` after the
 * call, not short-circuiting before it) so one failure doesn't skip saving
 * everything else. */
bool player_save(long player_id, const being_t *b) {
    bool ok = player_attrs_save(player_id, &b->attrs);
    ok = player_progress_save(player_id, &b->progress) && ok;
    ok = player_inventory_save(player_id, b) && ok;
    ok = affect_repo_save_all(player_id, b->affects) && ok;
    return ok;
}
