/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>

#include <stdlib.h>
#include <string.h>

#include "affect.h"
#include "being.h"
#include "cmd.h"
#include "combat.h"
#include "fall.h"
#include "mob_ai.h"
#include "player_repo.h"
#include "room.h"
#include "room_repo.h"
#include "skill.h"
#include "thing.h"
#include "trigger.h"
#include "world.h"

/* Fires `to`'s room "enter" triggers, then every mob-in-`to`'s "greet"
 * triggers, for `ch` just having walked in (user, 2026-07-11: "implement
 * mob object and room scripting ... interaction with mobs objs and room
 * via scripts"). Called after the arrival broadcast so any trigger flavor
 * text reads as following "X has arrived", not before it. */
static void run_room_and_greet_triggers(being_t *ch, room_t *to) {
    trigger_t trigs[8];
    int n = trigger_repo_load_for("room", to->vnum, "enter", trigs, 8);
    for (int i = 0; i < n; i++)
        trigger_run(&trigs[i], ch, to, NULL);

    for (thing_t *t = to->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_MOB)
            continue;
        being_t *mob = (being_t *)t;
        trigger_t mtrigs[8];
        int mn = trigger_repo_load_for("mob", mob->base.id, "greet", mtrigs, 8);
        if (mn == 0)
            continue;
        /* short_descr may start with a color tag -- skip it before
         * capitalizing, same bug class fixed elsewhere (cmd_look.c/
         * cmd_object.c/cmd_scan.c/mob_ai.c/trigger.c/combat.c). */
        char capbuf[128];
        snprintf(capbuf, sizeof(capbuf), "%s", mob->base.short_descr);
        size_t ci = 0;
        while (capbuf[ci] == '<' && capbuf[ci + 1] != '\0' && capbuf[ci + 2] == '>')
            ci += 3;
        if (capbuf[ci])
            capbuf[ci] = (char)toupper((unsigned char)capbuf[ci]);
        for (int i = 0; i < mn; i++)
            trigger_run(&mtrigs[i], ch, to, capbuf[0] ? capbuf : NULL);
    }
}

/* Substitutes `$d` (a direction word) and `$p` (the mover's gender_possess()
 * pronoun) into a poofin/poofout template -- see cmd_poof.c's doc comment
 * for the token contract. Used by do_move() below. */
static void apply_poof_tokens(const char *tmpl, const char *dir_word, gender_t gender,
                               char *out, size_t outsz) {
    size_t oi = 0;
    for (const char *p = tmpl; *p && oi + 1 < outsz; p++) {
        if (p[0] == '$' && p[1] == 'd') {
            oi += (size_t)snprintf(out + oi, outsz - oi, "%s", dir_word);
            p++;
        } else if (p[0] == '$' && p[1] == 'p') {
            oi += (size_t)snprintf(out + oi, outsz - oi, "%s", gender_possess(gender));
            p++;
        } else {
            out[oi++] = *p;
        }
    }
    out[oi < outsz ? oi : outsz - 1] = '\0';
}

/* north/east/south/west/up/down -- the first movement commands in the
 * port. Directions are the original dirTypeT's first six slots (see
 * room.h); like classic Diku (and the original's command table), these sit
 * at the very top of COMMANDS[] so the single letters n/e/s/w/u/d always
 * mean movement ("s" is south, not say; "w" is west, not who). */

/* Drags every standing, non-fighting follower of `leader` who was still
 * physically in `from` along through the same exit `leader` just took --
 * "proper leader/follower movement" (TODO.md priority item, user
 * 2026-07-30). Checked SneezyMUD's own moveGroup() (misc/movement.cc)
 * first: it's gated on plain `follow` (master/followers[]) alone, NOT on
 * `grouped`/AFF_GROUP -- matching Tobin's own existing split (`follow`
 * establishes the relationship, `group` only shares XP/gold, see
 * cmd_group.c's file-top comment) -- so this reads master/followers[]
 * directly and never touches `grouped`. Recurses into each follower's
 * OWN followers (a chain, not just one level), same as the real
 * moveGroup(). Skips: not physically in `from` anymore (wandered off, or
 * a linkdead follower who never moves at all since their position never
 * changes them out of `from` -- checked, doesn't cause a stuck-forever
 * bug because `from` is a snapshot, not re-read after teleporting), mid-
 * fight (matching the leader's OWN "no walking out of a fight" refusal),
 * not standing/mounted (matching the leader's OWN position gate exactly,
 * stricter than Sneezy's own `>= POSITION_CRAWLING` -- consistent with
 * what Tobin already requires of the PRIMARY mover). Deliberately no vit
 * cost charged to a dragged-along follower (a disclosed scope-cut --
 * Sneezy's own version only ever spends a Shaman leader's lifeforce
 * here, nothing follower-side either) and no door/trap/terrain
 * re-checks (the leader already cleared all of those for this exit).
 * `descriptor_dispatch()`-free by design: mob followers can't exist
 * (only a PC can type `follow`; see cmd_follow.c), so every entry here
 * always has a `desc`, but a raw `cmd_dispatch(desc, "look")` is used
 * instead of assuming any particular caller-facing helper. */
static void move_followers_along(being_t *leader, room_t *from, room_t *to) {
    for (int i = 0; i < GROUP_MAX_FOLLOWERS; i++) {
        being_t *f = leader->followers[i];
        if (!f || f->base.roomp != from)
            continue;
        if (f->fighting)
            continue;
        if (f->position != POSITION_STANDING && f->position != POSITION_MOUNTED)
            continue;

        char msg[128];
        snprintf(msg, sizeof(msg), "You follow %s.\r\n", being_display_name(leader));
        if (f->desc)
            descriptor_send(f->desc, msg);

        thing_set_room(&f->base, to);

        char arrive[96];
        snprintf(arrive, sizeof(arrive), "%s has arrived.\r\n", f->base.name);
        if (!f->sneaking)
            descriptor_room_echo(to, f, arrive);

        if (f->desc)
            cmd_dispatch(f->desc, "look");

        move_followers_along(f, from, to);
    }
}

static bool do_move(descriptor_t *d, int dir) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (ch->fighting) {
        /* Same rule as the original's doMove: no walking out of a fight. */
        descriptor_send(d, "No way! You are fighting for your life!\r\n");
        return true;
    }
    if (ch->position != POSITION_STANDING && ch->position != POSITION_MOUNTED) {
        /* Must be on your feet (or in the saddle) to travel (original
         * doMove position gate; POSITION_MOUNTED exempted, Mount/riding
         * system, Sneezy → Tobin feature audit). */
        descriptor_send(d, "You are in no position to move -- try standing up first.\r\n");
        return true;
    }
    /* `feign death` (level-25 audit batch) -- moving breaks the act. */
    ch->feigning = false;
    if (being_has_affect(ch, AFFECT_BIND)) {
        /* `bind` (Mage, level 25, level-25 audit batch) -- see
         * AFFECT_BIND's own doc comment (affect.h). */
        descriptor_send(d, "You're stuck fast in a mass of sticky webbing!\r\n");
        return true;
    }
    if (being_has_affect(ch, AFFECT_LIVING_VINES)) {
        /* `living vines` (Shaman/Druid audit batch C, 2026-08-09) -- see
         * AFFECT_LIVING_VINES's own doc comment (affect.h). */
        descriptor_send(d, "Living vines have wrapped tight around your legs -- you can't move!\r\n");
        return true;
    }

    room_t *from = ch->base.roomp;
    int dest = from->exits[dir];
    room_t *to = NULL;
    if (dest >= 0) {
        to = world_get_room(dest);
        if (!to) {
            to = room_repo_load(dest);
            if (to)
                world_register_room(to);
        }
    }
    if (!to) {
        descriptor_send(d, "You can't go that way.\r\n");
        return true;
    }
    if (from->exit_door[dir] != 0 && (from->exit_cond[dir] & EXIT_COND_CLOSED)) {
        descriptor_send(d, "The door is closed.\r\n");
        return true;
    }

    /* PRIVATE room (ROOM_FLAG_PRIVATE, user 2026-08-16 "room flags ...
     * intended effects from sneezy") -- a private room holds at most two
     * players (real upstream ROOM_PRIVATE); a third is refused entry.
     * Immortals ignore the cap (same "no restrictions" spirit as their
     * other gate bypasses). Counts real players only -- a mob sharing the
     * room (shopkeeper, guard) shouldn't lock players out. */
    if (!being_is_immortal(ch) && (to->room_flag & ROOM_FLAG_PRIVATE)) {
        int players = 0;
        for (thing_t *t = to->base.stuff_head; t; t = t->stuff_next)
            if (t->kind == THING_PC)
                players++;
        if (players >= 2) {
            descriptor_send(d, "That room is private -- there's no room for you right now.\r\n");
            return true;
        }
    }

    /* Terrain movement cost (Sneezy → Tobin feature audit, "Vitality
     * stat + Terrain movement cost"): average of the source and
     * destination sector's cost, same average-of-two-sectors rule the
     * original's rawMove() uses. A hard gate, not a soft slowdown --
     * refused outright rather than let vit go negative, same shape as
     * the door/fighting/position checks above. Immortals are exempt,
     * same reasoning as hunger/thirst immunity (being.h). */
    if (!being_is_immortal(ch)) {
        int cost = (sector_move_cost(from->sector) + sector_move_cost(to->sector) + 1) / 2;
        /* `hiking` (docs/Spell Assignments.xlsx gap audit, 2026-08-08) --
         * up to 50% off terrain cost at full proficiency, same shape
         * `swim` uses for drowning damage. Applied to the base terrain
         * cost before the flying/mounted discounts below so all three
         * compose naturally (e.g. a hiking-trained rider gets both).
         * Trains on every real move, win or lose. */
        if (being_knows_skill(ch, "hiking")) {
            const skill_def_t *hiking_sk = skill_find(ch->char_class, "hiking", false);
            if (hiking_sk) {
                int hiking_prof = skill_learn_from_doing(ch, hiking_sk);
                cost -= cost * (hiking_prof / 2) / 100;
                if (cost < 1)
                    cost = 1;
            }
        }
        /* `climbing` (missing-skill audit, generic/cross-class,
         * 2026-08-10): real upstream (physics.cc's climb()) is a
         * pass-or-fall check when moving through vertical terrain.
         * Dropping/refusing a live player on a failed roll is punitive on
         * a production server, so -- same shape as `hiking` just above --
         * climbing grants an ADDITIONAL cost reduction that applies only
         * when the source or destination is a mountain/climbing sector,
         * the exact vertical terrain the skill is about. Stacks with
         * hiking. Disclosed scope-cut from the original's fall gate. */
        {
            const char *fn = sector_name(from->sector);
            const char *tn = sector_name(to->sector);
            bool vertical = strstr(fn, "MOUNTAIN") || strstr(fn, "CLIMBING")
                         || strstr(tn, "MOUNTAIN") || strstr(tn, "CLIMBING");
            if (vertical && being_knows_skill(ch, "climbing")) {
                const skill_def_t *climb_sk = skill_find(ch->char_class, "climbing", false);
                if (climb_sk) {
                    int climb_prof = skill_learn_from_doing(ch, climb_sk);
                    cost -= cost * (climb_prof / 2) / 100;
                    if (cost < 1)
                        cost = 1;
                }
            }
        }
        /* Flying quarters the charge (Sneezy → Tobin feature audit,
         * "Water, drowning, flight") -- same discount the original's
         * canFly()-gated movement math applies, rounded up so it's
         * never free (min 1). */
        if (being_has_affect(ch, AFFECT_FLYING)) {
            cost = (cost + 3) / 4;
            if (cost < 1)
                cost = 1;
        } else if (ch->position == POSITION_MOUNTED) {
            /* Mounted discount (Mount / riding system, Sneezy → Tobin
             * feature audit) -- half cost, rounded up so it's never
             * free, same shape as the flying discount just above. Drawn
             * from the RIDER's own vit pool (Tobin mobs don't track vit
             * at all -- being_create_mob() never calls
             * being_calc_max_vit() -- so there's no mount-side pool to
             * draw from instead, unlike Sneezy's real system). */
            cost = (cost + 1) / 2;
            if (cost < 1)
                cost = 1;
        }
        if (ch->progress.vit < cost) {
            descriptor_send(d, "You are too exhausted to go that way.\r\n");
            return true;
        }
        being_spend_vit(ch, cost);
        /* Persist immediately, not just at quit -- same "don't lose a
         * real stat change to a disconnect" reasoning as cmd_eat.c/
         * cmd_drink.c's own player_progress_save() calls. Only a real
         * PC has a player_progress row; a possessed mob (cmd_possess.c)
         * reaches do_move() too but must never try to save one. */
        if (ch->base.kind == THING_PC)
            player_progress_save(ch->player_id, &ch->progress);
    }

    /* Trap mechanics (user 2026-07-11, sequenced after weapon depth): a
     * Thief's "detect trap" skill spots and safely steps around a
     * trapped door, leaving it rigged for the next person -- stepping
     * around it doesn't spring it. Everyone else springs it: one-shot,
     * the trap is gone (both in memory and the DB) once it actually
     * goes off, matching a real trap being a single rigged mechanism,
     * not a renewable hazard. */
    if (from->exit_door[dir] != 0 && (from->exit_cond[dir] & EXIT_COND_TRAPPED)) {
        if (being_knows_skill(ch, "detect trap")) {
            descriptor_send(d, "You spot a trap rigged to the door and carefully step around it.\r\n");
        } else {
            int dmg = 5 + rand() % 10;
            /* LIMB_REAL_COUNT, not LIMB_COUNT (Limbs -> wearSlotT,
             * 2026-07-26) -- excludes the mob-only, always-inactive EX_*
             * placeholder slots from ever being randomly hit. */
            limb_t limb = (limb_t)(rand() % LIMB_REAL_COUNT);
            int limb_hp_before = ch->limbs[limb].hp;
            being_hurt_limb(ch, limb, dmg);
            char trap_msg[160];
            /* Damage numbers (user 2026-07-12, follow-up "take out the
             * damage number and use it to describe how hard the hit
             * was"): same describe_dam() treatment as combat.c's melee
             * messages, shown to every viewer now, not just immortals. */
            snprintf(trap_msg, sizeof(trap_msg),
                     "A trap rigged to the door springs! It catches your %s %s!\r\n",
                     limb_name(limb), describe_dam(dmg, limb_hp_before, NULL));
            descriptor_send(d, trap_msg);
            from->exit_cond[dir] &= ~EXIT_COND_TRAPPED;
            room_repo_save_exit(from->vnum, dir, from->exits[dir], from->exit_door[dir], from->exit_cond[dir]);
        }
    }

    /* "exits to the north" for compass directions (user-specified
     * phrasing), "exits upward/downward" where "to the up" won't parse. */
    static const char *const EXIT_PHRASES[ROOM_NUM_EXITS] = {
        "exits to the north", "exits to the east", "exits to the south",
        "exits to the west", "exits upward", "exits downward",
        "exits to the northeast", "exits to the northwest",
        "exits to the southeast", "exits to the southwest",
    };
    char msg[256];
    char body[BEING_BAMF_LEN + 32];
    if (ch->poofout[0]) {
        apply_poof_tokens(ch->poofout, DIR_NAMES[dir], ch->gender, body, sizeof(body));
        snprintf(msg, sizeof(msg), "%s %s.\r\n", ch->base.name, body);
    } else {
        snprintf(msg, sizeof(msg), "%s %s.\r\n", ch->base.name, EXIT_PHRASES[dir]);
    }
    if (!ch->sneaking)
        descriptor_room_echo(from, ch, msg);

    being_break_hiding(ch);
    thing_set_room(&ch->base, to);

    /* A mounted rider's mount comes along for the ride -- otherwise the
     * horse gets left behind the moment its rider walks anywhere (Mount
     * / riding system, Sneezy → Tobin feature audit). EXCEPT into an
     * indoor room -- same real precedent Sneezy's own goDirection() uses,
     * a horse doesn't fit through a doorway -- where the rider dismounts
     * instead and the mount simply stays behind in `from`, never moved.
     * No separate room echo for the mount tagging along; the rider's own
     * arrival/departure messages already cover the normal case. */
    if (ch->mount && (to->room_flag & ROOM_FLAG_INDOORS)) {
        being_t *mount = ch->mount;
        ch->mount = NULL;
        mount->rider = NULL;
        ch->position = POSITION_STANDING;
        mount->position = POSITION_STANDING;
        char dismount_msg[128];
        snprintf(dismount_msg, sizeof(dismount_msg),
                 "You duck through the doorway and have to dismount, leaving %s behind.\r\n",
                 being_display_name(mount));
        descriptor_send(d, dismount_msg);
    } else if (ch->mount) {
        thing_set_room(&ch->mount->base, to);
    }

    /* Pet/charm (Sneezy → Tobin feature audit): a charmed pet follows its
     * master room-to-room, same "comes along for the ride" precedent as
     * the mount block above -- unlike a mount, no indoor exception (a
     * pet isn't too big for a doorway). Silent on the pet's own arrival/
     * departure (no separate room echo either side) -- the master's own
     * move messages already cover it, same choice human followers now
     * make too (move_followers_along() below, called after the leader's
     * own arrival echo so the room reads "leader arrives" then "follower
     * follows" in natural order, not the other way around). */
    being_t *pet = being_find_charmed_pet(ch);
    if (pet)
        thing_set_room(&pet->base, to);

    if (ch->poofin[0]) {
        apply_poof_tokens(ch->poofin, DIR_NAMES[REV_DIR[dir]], ch->gender, body, sizeof(body));
        snprintf(msg, sizeof(msg), "%s %s.\r\n", ch->base.name, body);
    } else {
        snprintf(msg, sizeof(msg), "%s has arrived.\r\n", ch->base.name);
    }
    if (!ch->sneaking)
        descriptor_room_echo(to, ch, msg);

    mob_ai_greet_newbie_equipper(ch, to);

    /* Death-trap room (ROOM_FLAG_DEATH, user 2026-08-16 "room flags ...
     * intended effects from sneezy") -- a mortal who steps in is slain on
     * the spot (real upstream ROOM_DEATH), losing XP and dropping a corpse
     * with their gear, then ejected to the account menu. Checked right
     * after arrival (so the room sees them arrive, then die) and before
     * followers are pulled in, so followers don't get dragged to their
     * death too. combat_death_room_kill_pc() frees d->character via
     * descriptor_leave_to_menu(), same "check d->character after" guard as
     * the fatal fall_check() below. Immortals pass unharmed. */
    if (!being_is_immortal(ch) && ch->base.kind == THING_PC
        && (to->room_flag & ROOM_FLAG_DEATH)) {
        combat_death_room_kill_pc(ch);
        if (!d->character)
            return true;
    }

    /* Leader/follower movement (TODO.md priority item, user 2026-07-30) --
     * see move_followers_along()'s own doc comment above do_move() for
     * the full design. */
    move_followers_along(ch, from, to);

    run_room_and_greet_triggers(ch, to);

    /* Falling (Sneezy → Tobin feature audit, "catfall/catleap") -- see
     * fall.c's own doc comment. May destroy `ch` on a fatal landing
     * (combat_fall_kill_pc() -> descriptor_leave_to_menu(), same "d->
     * character freed and set NULL" shape combat_defeat()'s own loser-
     * ejection path already uses) -- checked before the final `look`,
     * same guard vitals_tick_run() uses after a fatal drowning check. */
    fall_check(ch);
    if (!d->character)
        return true;

    /* Tobin-original heat subsystem (user 2026-08-17): a cosmetic comfort
     * cue when a mortal steps into an outdoor temperature-extreme sector --
     * the STRESS band below the DAMAGE thresholds vitals.c chips at. Fires
     * on entry only (not per drain tick), sheltered indoors. See
     * sector_heat() (room.c) for why this layer is a labelled invention. */
    if (d->character && !being_is_immortal(ch) && ch->base.kind == THING_PC
        && !(to->room_flag & ROOM_FLAG_INDOORS)) {
        int amb = room_ambient_heat(to);
        if (amb >= HEAT_STRESS_HOT)
        descriptor_send(d, "<o>The heat here is oppressive; you break into a sweat.<z>\r\n");
        else if (amb <= HEAT_STRESS_COLD)
        descriptor_send(d, "<c>The air here is bitterly cold; you shiver.<z>\r\n");
    }

    return cmd_dispatch(d, "look");
}

bool cmd_north(descriptor_t *d, const char *args) { (void)args; return do_move(d, 0); }
bool cmd_east(descriptor_t *d, const char *args)  { (void)args; return do_move(d, 1); }
bool cmd_south(descriptor_t *d, const char *args) { (void)args; return do_move(d, 2); }
bool cmd_west(descriptor_t *d, const char *args)  { (void)args; return do_move(d, 3); }
bool cmd_up(descriptor_t *d, const char *args)    { (void)args; return do_move(d, 4); }
bool cmd_down(descriptor_t *d, const char *args)  { (void)args; return do_move(d, 5); }
bool cmd_northeast(descriptor_t *d, const char *args) { (void)args; return do_move(d, 6); }
bool cmd_northwest(descriptor_t *d, const char *args) { (void)args; return do_move(d, 7); }
bool cmd_southeast(descriptor_t *d, const char *args) { (void)args; return do_move(d, 8); }
bool cmd_southwest(descriptor_t *d, const char *args) { (void)args; return do_move(d, 9); }
