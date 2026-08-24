/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_MONK_QUEST_H
#define TOBIN_MONK_QUEST_H
#include "being.h"
#include "obj.h"
#include "room.h"
/* Monk sash quest chain (white belt -> yellow -> purple -> blue -> green
 * -> red -> black), reverse-engineered from SneezyMUD's `mobresponses`
 * script table + misc/gaining.cc/damage.cc/discipline.cc and ported here
 * as hand-written C dispatch, NOT a generic scripting-DSL port -- Tobin's
 * existing `trigger` system (docs/TRIGGER_SCRIPTING.md) was considered
 * but its `global` variables are world-wide, not per-character, so it
 * can't hold quest-chain state; seven quest stages don't justify
 * inventing a second, per-player scripting layer just to reuse it.
 * Mob/room/item vnums all come from already-seeded SneezyMUD zone data
 * (verified present in tobin's mob/obj/room tables before writing any of
 * this).
 *
 * Progress is a bitfield (being_t.monk_quest_flags, persisted via
 * player_set_monk_quest()) plus a separate 0-5 leper kill counter
 * (monk_purple_kills) for the purple stage. Deliberate deviations from
 * the original, documented in STATUS.md's decisions table:
 *   - Blue sash: no `dissect` subsystem exists in Tobin, so the proof
 *     item (dog collar) drops as guaranteed corpse loot on the shark kill
 *     itself (monk_quest_on_mob_kill()), not via a separate dissect step.
 *   - Green sash: the original's 3-room fall sequence (balcony, hanging
 *     room, mid-air teleport, landing, gated on last-move-direction)
 *     collapses to a single room-enter check (must be riding the elephant
 *     when entering the balcony's hanging room) that teleports straight
 *     to the landing room -- no last-direction tracking subsystem exists
 *     to gate a multi-hop fall.
 *   - Red sash: the original's 4 weapon-prof skills (slash/blunt/pierce/
 *     ranged) don't map 1:1 onto Tobin's SKILL_TIER_COMBAT roster (which
 *     has barehand instead of ranged -- no ranged-weapon skill exists
 *     yet) -- uses barehand/slash/blunt/pierce proficiency instead, which
 *     actually fits a MONK sash quest better than a ranged skill would.
 *   - Black sash was an unfinished stub upstream ("This quest isn't ready
 *     yet."). Original content here: "Trials of the Unbroken Sash" --
 *     master all four combat-tier proficiencies to 50 (double red's
 *     threshold) AND defeat "the wandering monk" (mob vnum 6464, an
 *     existing level-53 solo-appropriate mob) in single combat. Awards
 *     the black sash object (vnum 6796 -- already present in seeded world
 *     data as a [quest_object], just never wired to any quest logic
 *     upstream). */
/* Stage-state bits, player.monk_quest_flags. */
#define MQ_WHITE_ELIGIBLE     (1u << 0)
#define MQ_WHITE_STARTED      (1u << 1)
#define MQ_WHITE_HAS          (1u << 2)
#define MQ_YELLOW_ELIGIBLE    (1u << 3)
#define MQ_YELLOW_HAS         (1u << 4)
#define MQ_PURPLE_ELIGIBLE    (1u << 5)
#define MQ_PURPLE_STARTED     (1u << 6)
#define MQ_PURPLE_HAS         (1u << 7)
#define MQ_BLUE_ELIGIBLE      (1u << 8)
#define MQ_BLUE_STARTED       (1u << 9)
#define MQ_BLUE_KILLED_SHARK  (1u << 10)
#define MQ_BLUE_HAS           (1u << 11)
#define MQ_GREEN_ELIGIBLE     (1u << 12)
#define MQ_GREEN_STARTED      (1u << 13)
#define MQ_GREEN_HAS          (1u << 14)
#define MQ_RED_ELIGIBLE       (1u << 15)
#define MQ_RED_STARTED        (1u << 16)
#define MQ_RED_FINISHED       (1u << 17)
#define MQ_RED_HAS            (1u << 18)
#define MQ_BLACK_ELIGIBLE     (1u << 19)
#define MQ_BLACK_STARTED      (1u << 20)
#define MQ_BLACK_TRIAL_SKILL  (1u << 21)
#define MQ_BLACK_TRIAL_COMBAT (1u << 22)
#define MQ_BLACK_HAS          (1u << 23)
/* Quest-giver mob vnums (verified present in tobin's mob table). */
#define MONK_MOB_GUILDMASTER_207   207   /* white/yellow/purple */
#define MONK_MOB_GUILDMASTER_223   223   /* blue/green */
#define MONK_MOB_HUANGLO           385   /* advice-only, all early stages */
#define MONK_MOB_GUILDMASTER_12509 12509 /* red/black */
#define MONK_MOB_LEPER             6602  /* purple sash kill target x5 */
#define MONK_MOB_ELEPHANT          8525  /* green sash mount */
#define MONK_MOB_TIGER_SHARK       12413 /* blue sash kill+loot target */
#define MONK_MOB_WANDERING_MONK    6464  /* black sash trial-by-combat */
/* Quest item vnums (verified present in tobin's obj table). */
#define MONK_OBJ_BANDAGE       9
#define MONK_OBJ_ASHTRAY       3319
#define MONK_OBJ_WHITE_BELT    6790
#define MONK_OBJ_YELLOW_SASH   6791
#define MONK_OBJ_PURPLE_SASH   6792
#define MONK_OBJ_BLUE_SASH     6793
#define MONK_OBJ_GREEN_SASH    6794
#define MONK_OBJ_RED_SASH      6795
#define MONK_OBJ_BLACK_SASH    6796
#define MONK_OBJ_DOG_COLLAR    12468
/* Green sash cliff-jump rooms (verified present in tobin's room table). */
#define MONK_ROOM_BALCONY_FALL   11089 /* "Hanging From a Balcony" */
#define MONK_ROOM_LANDING        10020 /* "A Blank Rock Wall" */
/* Called from combat.c whenever `m` gains one or more levels -- announces
 * eligibility for the next sash stage exactly like the original's
 * gaining.cc chain (each stage requires the previous stage's has/owned
 * bit, a clear slate on its own bits, and the level threshold). No-op for
 * non-Monks. */
void monk_quest_on_levelup(being_t *m);
/* Called from cmd_say.c for every `say` in a room, after the normal
 * broadcast -- dispatches to whichever quest-giver mob (207/223/385/
 * 12509) is present and matches `said` against that mob's current-stage
 * keyword. No-op if no such mob is in `r`. */
void monk_quest_on_say(being_t *speaker, room_t *r, const char *said);
/* Called from cmd_object.c's cmd_give(), after `item` has already been
 * moved into `vict`'s inventory -- checks white/yellow (207) and blue
 * (223) item turn-ins; consumes the item (obj_destroy()) and advances
 * state on a match. No-op otherwise (item stays with `vict` normally). */
void monk_quest_on_give(being_t *ch, being_t *vict, obj_t *item);
/* Called from combat.c's combat_defeat() once `winner` (a PC) has just
 * killed `loser_vnum` and `corpse` exists -- purple sash's leper kill
 * counter, blue sash's guaranteed dog-collar drop, and black sash's
 * wandering-monk trial. No-op for vnums none of those. */
void monk_quest_on_mob_kill(being_t *winner, int loser_vnum, obj_t *corpse);
/* Called from combat.c right after a barehand/slash/blunt/pierce
 * proficiency skill-learn attempt for `ch` -- checks red sash's (all 4
 * >=20) and black sash's (all 4 >=50) skill-mastery trials. */
void monk_quest_check_combat_mastery(being_t *ch);
/* Called from cmd_move.c's run_room_and_greet_triggers(), right as `ch`
 * arrives in `to` -- green sash's elephant cliff-jump (see the file-top
 * comment's documented simplification). No-op unless `to` is the fall
 * room and `ch` is both green-sash-started and mounted on the elephant. */
void monk_quest_on_room_enter(being_t *ch, room_t *to);
/* Persists `b`'s current monk_quest_flags/monk_purple_kills (PCs only,
 * no-op otherwise). Small wrapper so every mutation site doesn't repeat
 * the player_id/kind guard. */
void monk_quest_save(being_t *b);
#endif
