/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef CMD_INTERNAL_H
#define CMD_INTERNAL_H

#include <stdbool.h>

#include "descriptor.h"

/* Internal wiring between cmd_table.c and each cmd_*.c handler -- not part
 * of the public include/ API surface. Each handler returns false to
 * request the connection be closed (only cmd_quit does this), true
 * otherwise. */

bool cmd_look(descriptor_t *d, const char *args);
bool cmd_who(descriptor_t *d, const char *args);
bool cmd_score(descriptor_t *d, const char *args);
bool cmd_level(descriptor_t *d, const char *args);
bool cmd_ride(descriptor_t *d, const char *args);
bool cmd_dismount(descriptor_t *d, const char *args);
bool cmd_bash(descriptor_t *d, const char *args);
bool cmd_bodyslam(descriptor_t *d, const char *args);
bool cmd_kick(descriptor_t *d, const char *args);
bool cmd_stomp(descriptor_t *d, const char *args);
bool cmd_evaluate(descriptor_t *d, const char *args);
bool cmd_disarm(descriptor_t *d, const char *args);
bool cmd_trip(descriptor_t *d, const char *args);
bool cmd_rescue(descriptor_t *d, const char *args);
bool cmd_taunt(descriptor_t *d, const char *args);
bool cmd_backstab(descriptor_t *d, const char *args);
bool cmd_steal(descriptor_t *d, const char *args);
bool cmd_sneak(descriptor_t *d, const char *args);
bool cmd_grapple(descriptor_t *d, const char *args);
bool cmd_whirlwind(descriptor_t *d, const char *args);
bool cmd_kneestrike(descriptor_t *d, const char *args);
bool cmd_switchopp(descriptor_t *d, const char *args);
bool cmd_tranceblades(descriptor_t *d, const char *args);
bool cmd_stabbing(descriptor_t *d, const char *args);
bool cmd_subterfuge(descriptor_t *d, const char *args);
bool cmd_chop(descriptor_t *d, const char *args);
bool cmd_hurl(descriptor_t *d, const char *args);
bool cmd_feigndeath(descriptor_t *d, const char *args);
bool cmd_berserk(descriptor_t *d, const char *args);
bool cmd_rally(descriptor_t *d, const char *args);
bool cmd_garrotte(descriptor_t *d, const char *args);
bool cmd_throatslit(descriptor_t *d, const char *args);
bool cmd_yoginsa(descriptor_t *d, const char *args);
bool cmd_meditate(descriptor_t *d, const char *args);
bool cmd_chi(descriptor_t *d, const char *args);
bool cmd_shove(descriptor_t *d, const char *args);
bool cmd_materialize(descriptor_t *d, const char *args);
bool cmd_disguise(descriptor_t *d, const char *args);
bool cmd_repair(descriptor_t *d, const char *args);
bool cmd_debride(descriptor_t *d, const char *args);
bool cmd_submit(descriptor_t *d, const char *args);
bool cmd_retrieve(descriptor_t *d, const char *args);
bool cmd_tickets(descriptor_t *d, const char *args);
bool cmd_bank(descriptor_t *d, const char *args);
bool cmd_treasury(descriptor_t *d, const char *args);
bool cmd_skills(descriptor_t *d, const char *args);
bool cmd_practice(descriptor_t *d, const char *args);
bool cmd_continue(descriptor_t *d, const char *args);
bool cmd_balance(descriptor_t *d, const char *args);
bool cmd_settrap(descriptor_t *d, const char *args);
bool cmd_disarmtrap(descriptor_t *d, const char *args);
bool cmd_peek(descriptor_t *d, const char *args);
bool cmd_dig(descriptor_t *d, const char *args);
bool cmd_affects(descriptor_t *d, const char *args);
bool cmd_alias(descriptor_t *d, const char *args);
bool cmd_quit(descriptor_t *d, const char *args);
bool cmd_color(descriptor_t *d, const char *args);
bool cmd_attack(descriptor_t *d, const char *args);
bool cmd_kill(descriptor_t *d, const char *args);
bool cmd_hit(descriptor_t *d, const char *args);
bool cmd_hurtlimb(descriptor_t *d, const char *args);
bool cmd_restore(descriptor_t *d, const char *args);
bool cmd_aitick(descriptor_t *d, const char *args);
bool cmd_flee(descriptor_t *d, const char *args);
bool cmd_headbutt(descriptor_t *d, const char *args);
bool cmd_spin(descriptor_t *d, const char *args);
bool cmd_springleap(descriptor_t *d, const char *args);
bool cmd_slam(descriptor_t *d, const char *args);
bool cmd_deathstroke(descriptor_t *d, const char *args);
bool cmd_say(descriptor_t *d, const char *args);
bool cmd_shout(descriptor_t *d, const char *args);
bool cmd_sign(descriptor_t *d, const char *args);
bool cmd_limbs(descriptor_t *d, const char *args);
bool cmd_help(descriptor_t *d, const char *args);
bool cmd_wizhelp(descriptor_t *d, const char *args);
bool cmd_goto(descriptor_t *d, const char *args);
bool cmd_promote(descriptor_t *d, const char *args);
bool cmd_hedit(descriptor_t *d, const char *args);
bool cmd_copyover(descriptor_t *d, const char *args);
bool cmd_north(descriptor_t *d, const char *args);
bool cmd_east(descriptor_t *d, const char *args);
bool cmd_south(descriptor_t *d, const char *args);
bool cmd_west(descriptor_t *d, const char *args);
bool cmd_up(descriptor_t *d, const char *args);
bool cmd_down(descriptor_t *d, const char *args);
bool cmd_northeast(descriptor_t *d, const char *args);
bool cmd_northwest(descriptor_t *d, const char *args);
bool cmd_southeast(descriptor_t *d, const char *args);
bool cmd_southwest(descriptor_t *d, const char *args);
bool cmd_edroom(descriptor_t *d, const char *args);
bool cmd_edit(descriptor_t *d, const char *args);
bool cmd_log(descriptor_t *d, const char *args);
bool cmd_exits(descriptor_t *d, const char *args);
bool cmd_loadroom(descriptor_t *d, const char *args);
bool cmd_mortal(descriptor_t *d, const char *args);
bool cmd_immort(descriptor_t *d, const char *args);
bool cmd_prompt(descriptor_t *d, const char *args);
bool cmd_time(descriptor_t *d, const char *args);
bool cmd_uptime(descriptor_t *d, const char *args);
bool cmd_edsuit(descriptor_t *d, const char *args);
bool cmd_title(descriptor_t *d, const char *args);
bool cmd_save(descriptor_t *d, const char *args);
bool cmd_rent(descriptor_t *d, const char *args);
bool cmd_toggle(descriptor_t *d, const char *args);
bool cmd_gametog(descriptor_t *d, const char *args);
bool cmd_snoop(descriptor_t *d, const char *args);
bool cmd_exec(descriptor_t *d, const char *args);
bool cmd_bug(descriptor_t *d, const char *args);
bool cmd_delbug(descriptor_t *d, const char *args);
bool cmd_edbug(descriptor_t *d, const char *args);
bool cmd_idea(descriptor_t *d, const char *args);
bool cmd_delidea(descriptor_t *d, const char *args);
bool cmd_typo(descriptor_t *d, const char *args);
bool cmd_deltypo(descriptor_t *d, const char *args);
bool cmd_test(descriptor_t *d, const char *args);
bool cmd_newbie(descriptor_t *d, const char *args);
bool cmd_rules(descriptor_t *d, const char *args);
bool cmd_edrules(descriptor_t *d, const char *args);
bool cmd_users(descriptor_t *d, const char *args);
bool cmd_news(descriptor_t *d, const char *args);
bool cmd_addnews(descriptor_t *d, const char *args);
bool cmd_stand(descriptor_t *d, const char *args);
bool cmd_sit(descriptor_t *d, const char *args);
bool cmd_rest(descriptor_t *d, const char *args);
bool cmd_sleep(descriptor_t *d, const char *args);
bool cmd_smoke(descriptor_t *d, const char *args);
bool cmd_wake(descriptor_t *d, const char *args);
bool cmd_catchup(descriptor_t *d, const char *args);
bool cmd_catleap(descriptor_t *d, const char *args);
bool cmd_cast(descriptor_t *d, const char *args);
bool cmd_pray(descriptor_t *d, const char *args);
bool cmd_wiznews(descriptor_t *d, const char *args);
bool cmd_wipe(descriptor_t *d, const char *args);
bool cmd_edwiznews(descriptor_t *d, const char *args);
bool cmd_socials(descriptor_t *d, const char *args);
bool cmd_wiznet(descriptor_t *d, const char *args);
bool cmd_system(descriptor_t *d, const char *args);
bool cmd_shutdown(descriptor_t *d, const char *args);
bool cmd_mudstats(descriptor_t *d, const char *args);
bool cmd_multiplay(descriptor_t *d, const char *args);
bool cmd_setsev(descriptor_t *d, const char *args);
bool cmd_edplayer(descriptor_t *d, const char *args);
bool cmd_edaccount(descriptor_t *d, const char *args);
bool cmd_edsocial(descriptor_t *d, const char *args);
bool cmd_set(descriptor_t *d, const char *args);
bool cmd_open(descriptor_t *d, const char *args);
bool cmd_close(descriptor_t *d, const char *args);
bool cmd_lock(descriptor_t *d, const char *args);
bool cmd_unlock(descriptor_t *d, const char *args);
bool cmd_ignore(descriptor_t *d, const char *args);
bool cmd_unignore(descriptor_t *d, const char *args);
bool cmd_possess(descriptor_t *d, const char *args);
bool cmd_return(descriptor_t *d, const char *args);
bool cmd_follow(descriptor_t *d, const char *args);
bool cmd_stop(descriptor_t *d, const char *args);
bool cmd_group(descriptor_t *d, const char *args);
bool cmd_dismiss(descriptor_t *d, const char *args);
bool cmd_plant(descriptor_t *d, const char *args);
bool cmd_skin(descriptor_t *d, const char *args);
bool cmd_butcher(descriptor_t *d, const char *args);
bool cmd_forage(descriptor_t *d, const char *args);
bool cmd_split(descriptor_t *d, const char *args);
bool cmd_get(descriptor_t *d, const char *args);
bool cmd_put(descriptor_t *d, const char *args);
bool cmd_drop(descriptor_t *d, const char *args);
bool cmd_inventory(descriptor_t *d, const char *args);
bool cmd_wear(descriptor_t *d, const char *args);
bool cmd_hold(descriptor_t *d, const char *args);
bool cmd_wield(descriptor_t *d, const char *args);
bool cmd_switch(descriptor_t *d, const char *args);
bool cmd_list(descriptor_t *d, const char *args);
bool cmd_buy(descriptor_t *d, const char *args);
bool cmd_sell(descriptor_t *d, const char *args);
bool cmd_remove(descriptor_t *d, const char *args);
bool cmd_give(descriptor_t *d, const char *args);
bool cmd_use(descriptor_t *d, const char *args);
bool cmd_equipment(descriptor_t *d, const char *args);
bool cmd_load(descriptor_t *d, const char *args);
bool cmd_loadsuit(descriptor_t *d, const char *args);
bool cmd_purge(descriptor_t *d, const char *args);
bool cmd_transfer(descriptor_t *d, const char *args);
bool cmd_pee(descriptor_t *d, const char *args);
bool cmd_drink(descriptor_t *d, const char *args);
bool cmd_pour(descriptor_t *d, const char *args);
bool cmd_fill(descriptor_t *d, const char *args);
bool cmd_cook(descriptor_t *d, const char *args);
bool cmd_whittle(descriptor_t *d, const char *args);
bool cmd_sacrifice(descriptor_t *d, const char *args);
bool cmd_eat(descriptor_t *d, const char *args);
bool cmd_junk(descriptor_t *d, const char *args);
bool cmd_identify(descriptor_t *d, const char *args);
bool cmd_quest(descriptor_t *d, const char *args);
bool cmd_questdef(descriptor_t *d, const char *args);
bool cmd_weather(descriptor_t *d, const char *args);
bool cmd_bamfin(descriptor_t *d, const char *args);
bool cmd_bamfout(descriptor_t *d, const char *args);
bool cmd_poofin(descriptor_t *d, const char *args);
bool cmd_poofout(descriptor_t *d, const char *args);
bool cmd_scan(descriptor_t *d, const char *args);
bool cmd_vnum(descriptor_t *d, const char *args);
bool cmd_zone(descriptor_t *d, const char *args);
bool cmd_edzone(descriptor_t *d, const char *args);
bool cmd_edobject(descriptor_t *d, const char *args);
bool cmd_edmobile(descriptor_t *d, const char *args);
bool cmd_zonefile(descriptor_t *d, const char *args);
bool cmd_edtrigger(descriptor_t *d, const char *args);
bool cmd_consider(descriptor_t *d, const char *args);
bool cmd_egotrip(descriptor_t *d, const char *args);
bool cmd_stat(descriptor_t *d, const char *args);
bool cmd_stats(descriptor_t *d, const char *args);
bool cmd_gtell(descriptor_t *d, const char *args);
bool cmd_assist(descriptor_t *d, const char *args);
bool cmd_examine(descriptor_t *d, const char *args);
bool cmd_sip(descriptor_t *d, const char *args);
bool cmd_show(descriptor_t *d, const char *args);
bool cmd_tell(descriptor_t *d, const char *args);
/* Shared tell/reply delivery logic -- see cmd_tell.c's own doc comment. */
void tell_deliver(descriptor_t *d, being_t *target, const char *msg_text);
bool cmd_reply(descriptor_t *d, const char *args);
bool cmd_mute(descriptor_t *d, const char *args);
bool cmd_unmute(descriptor_t *d, const char *args);
bool cmd_whisper(descriptor_t *d, const char *args);
bool cmd_read(descriptor_t *d, const char *args);
bool cmd_write(descriptor_t *d, const char *args);
bool cmd_light(descriptor_t *d, const char *args);
bool cmd_extinguish(descriptor_t *d, const char *args);
bool cmd_refuel(descriptor_t *d, const char *args);
bool cmd_tips(descriptor_t *d, const char *args);
bool cmd_tipedit(descriptor_t *d, const char *args);

/* cmd_look.c's own `look <name>` resolver -- shared with `examine`
 * (cmd_examine.c), which Sneezy documents as a plain synonym for
 * "look at" ("Examine is synonymous with 'look at'"). */
bool look_at_target(descriptor_t *d, const char *args);

/* `hedit`'s gate (user-specified): level 56+, i.e. senior "God"-tier
 * immortals and up, not every 51+ immortal. */
#define HELP_EDIT_MIN_LEVEL 56

/* `copyover` reboots the server binary in place -- the most consequential
 * command there is, so it's gated at Administrator (59) and up. */
#define COPYOVER_MIN_LEVEL 59

/* `exec` runs shell commands on the host box -- Implementor-only (60). */
#define EXEC_MIN_LEVEL 60

/* `shutdown` ends the whole process -- more consequential than `copyover`
 * (which keeps every connection alive across the reboot), so it's gated a
 * tier above that, at Implementor (60), same as `exec`. */
#define SHUTDOWN_MIN_LEVEL 60

/* `delbug` removes a handled bug report -- Administrator (59) and up. */
#define DELBUG_MIN_LEVEL 59

/* `edbug` resolves a bug report in place (keeping it, unlike `delbug`) --
 * same tier as `delbug`, TODO.md-planned. */
#define EDBUG_MIN_LEVEL 59

/* `delidea` removes a handled idea -- Administrator (59) and up. */
#define DELIDEA_MIN_LEVEL 59

/* `deltypo` removes a handled typo report -- Administrator (59) and up,
 * same tier as delbug/delidea. */
#define DELTYPO_MIN_LEVEL 59

/* `test` shows the currently-running smoke test -- 58+ (user spec). */
#define TEST_MIN_LEVEL 58

/* `edrules` writes the numbered game rules -- Administrator (59) and up. */
#define EDRULES_MIN_LEVEL 59

/* `tipedit` adds/lists/deletes tips (TODO.md-planned tier). */
#define TIPEDIT_MIN_LEVEL 53

/* `redit` (the room builder): 51+ -- every immortal builds (user spec,
 * Session 21; future oedit/medit/zedit land at 51 too). Help editing
 * (hedit) stays at its own higher tier. */
#define BUILD_MIN_LEVEL 51

/* `zoneassign`: 55+ only (user spec, Session 43) -- must match
 * ZONE_UNRESTRICTED_LEVEL in zone.c (the level at which a builder is no
 * longer restricted to their assigned zones). */
#define ZONE_ASSIGN_MIN_LEVEL 55

/* `log` (read/search/list the game log files): 54+; `log rotate` alone is
 * isolated to 59+ (both user-specified, Tier 3). */
#define LOG_MIN_LEVEL 54
#define LOG_ROTATE_MIN_LEVEL 59

/* `promote`: 58+ (user-specified, Tier 3 -- was 51+). */
#define PROMOTE_MIN_LEVEL 58

/* `users` (connection roster with IPs): 58+, user-specified. */
#define USERS_MIN_LEVEL 58

/* `addnews` (post a news item): 56+, user-specified. */
#define ADDNEWS_MIN_LEVEL 56

/* `loadsuit` (load a named equipment suit onto someone): 56+, user-
 * specified ("the new loadsuit immortal 56+ command"), same tier as
 * addnews/help-edit -- a bigger lever than the builder-tier `load`. */
#define LOADSUIT_MIN_LEVEL 56

/* `multiplay` (toggle the mortal-multiplay game flag): 59+, user-specified. */
#define MULTIPLAY_MIN_LEVEL 59

/* `gametog` (global game-wide toggles, split out of `toggle`): 58+,
 * TODO.md-planned. */
#define GAMETOG_MIN_LEVEL 58

/* `balance` (gamewide class/race HP/damage/to-hit/AC modifiers) --
 * Implementor-only (60), user-specified: "a balance command (60)". */
#define BALANCE_MIN_LEVEL 60

/* `egotrip` (immortal toy-box, Sneezy port): Implementor-only (60),
 * matching `balance`'s tier -- "should be used seldomly" per the
 * original's own help text. */
#define EGOTRIP_MIN_LEVEL 60

/* `stat` (see everything about an obj/mob/room prototype by vnum): 55+,
 * user-specified (2026-07-12). */
#define STAT_MIN_LEVEL 55

/* `edsocial` (menu-driven social/emote editor): 55+, per the TODO.md item
 * that spawned it ("edsocial (55+)"). Same tier as `stat` -- both are
 * content-authoring tools a level below the Administrator-only ones
 * (edplayer/edaccount, 58+). */
#define EDSOCIAL_MIN_LEVEL 55

/* `snoop`: 59+, user-specified (2026-07-11). Same-or-higher-level targets
 * are refused inside cmd_snoop.c itself, not by this table gate alone. */
#define SNOOP_MIN_LEVEL 59

/* `possess` (puppet a mob's body -- Sneezy's admin `switch`, POWER_SWITCH):
 * same tier as `snoop`, matching precedent for a powerful admin-only
 * observation/control tool. `return` (come back to your own body) needs
 * no separate gate -- only reachable after a successful possess, which
 * already passed this one. */
#define POSSESS_MIN_LEVEL 59

/* `edplayer`: Administrator (58+), matching `promote`'s tier -- it's an
 * admin superset of promote (TODO.md). */
#define EDPLAYER_MIN_LEVEL 58

/* `edaccount`: Administrator (58+), same tier as edplayer -- both are
 * admin-only account/character management, no self-service equivalent. */
#define EDACCOUNT_MIN_LEVEL 58

/* `wipe`: Administrator (59+), per the TODO's own spec ("wipe a pfile or
 * an account... 59+"). Same-or-higher-level targets are refused inside
 * cmd_wipe.c itself, not by this table gate alone (mirrors `snoop`'s
 * comment above). */
#define WIPE_MIN_LEVEL 59

/* `set`: Administrator (58+), same tier as edplayer -- its one-shot,
 * scriptable sibling (user spec: build both). */
#define SET_MIN_LEVEL 58

/* `purge` (bare -- clears the current room's mobs/objects): 51+, matching
 * `redit`'s builder tier (user spec). `purge linkdead` (force-removes every
 * linkdead PC in the game) is a separate, much higher gate -- checked
 * inside cmd_purge() itself since the dispatch table only enforces one
 * floor per command name. */
#define PURGE_MIN_LEVEL 51
#define PURGE_LINKDEAD_MIN_LEVEL 58
#define PURGE_RANGE_MIN_LEVEL 59
#define ZONE_RECLAIM_MIN_LEVEL 59
#define EDROOM_RECLAIM_MIN_LEVEL 59
#define EDOBJECT_RECLAIM_MIN_LEVEL 59
#define EDMOBILE_RECLAIM_MIN_LEVEL 59
#define EDTRIGGER_RECLAIM_MIN_LEVEL 59

/* One row of cmd_table.c's dispatch table -- shared with cmd_help.c so
 * `help`/`wizhelp` can enumerate it without duplicating the list.
 * min_level is ENFORCED by cmd_dispatch() as of Phase 2A: a command above
 * the caller's level is skipped during matching entirely, so to a mortal
 * an immortal command is indistinguishable from one that doesn't exist
 * ("Huh?!") -- same as the original's commandInfo::minLevel dispatch gate. */
typedef struct {
    const char *name;
    bool (*fn)(descriptor_t *, const char *);
    const char *help;
    int min_level;
} cmd_entry_t;

/* Read-only view of cmd_table.c's COMMANDS[] for `help`/`wizhelp`
 * (cmd_help.c) to iterate. `quit!` is NOT included -- it's deliberately
 * excluded from the dispatch table entirely (see cmd_table.c), so
 * cmd_help.c lists it as a hardcoded extra line instead. */
const cmd_entry_t *cmd_table_entries(int *count);

#endif
