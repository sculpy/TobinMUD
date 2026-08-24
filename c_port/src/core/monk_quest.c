/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "monk_quest.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "descriptor.h"
#include "obj_repo.h"
#include "player_repo.h"
#include "skill.h"
#include "thing.h"
#include "world.h"
/* Implements the 6 entry points declared in monk_quest.h -- see that
 * file's top comment for the full design rationale, deviations, and
 * verified vnum list. This file is the hand-written C dispatch table
 * for the 7-stage sash chain (white/yellow/purple/blue/green/red/black),
 * driven entirely off `being_t.monk_quest_flags` + `monk_purple_kills`.
 *
 * One further deviation beyond monk_quest.h's own three (documented in
 * STATUS.md's decisions table): the original's `give` turn-ins for the
 * white belt (bandage) and yellow sash (ashtray) both go to Huang'lo/207
 * respectively in different NPCs (bandage -> Huang'lo, ashtray -> 207);
 * monk_quest.h's own doc comment on monk_quest_on_give() already
 * consolidates BOTH item turn-ins onto guildmaster 207 alone (Huang'lo
 * stays a pure advice NPC, never a give target) -- that consolidation was
 * this port's own prior design decision (see monk_quest.h), carried
 * through here rather than re-litigated.
 */
/* Best-effort message to b's connection, if any -- mirrors combat.c's/
 * fall.c's own static tell() helper (not shared across files). */
static void tell(being_t *b, const char *fmt, ...) {
    if (!b || !b->desc)
        return;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    descriptor_notify(b->desc, buf);
}
static bool ci_has(const char *hay, const char *needle) {
    size_t nl = strlen(needle);
    if (nl == 0 || !hay)
        return false;
    for (; *hay; hay++)
        if (strncasecmp(hay, needle, nl) == 0)
            return true;
    return false;
}
static bool is_monk_pc(const being_t *b) {
    return b && b->base.kind == THING_PC && b->char_class == CLASS_MONK;
}
static being_t *find_mob_in_room(room_t *r, int vnum) {
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind == THING_MOB && t->id == vnum)
            return (being_t *)t;
    }
    return NULL;
}
/* Loads+awards quest item `vnum` straight into `ch`'s inventory (sash/
 * belt awards -- the port's equivalent of the original's `resize`
 * script action, minus the auto-resize-to-fit step, which has no
 * analog in Tobin's simpler obj model). PCs only; saves inventory. */
static void award_item(being_t *ch, int vnum, const char *label) {
    obj_t *o = obj_create_from_proto(vnum);
    if (!o)
        return;
    thing_move_to(&o->base, &ch->base);
    if (ch->base.kind == THING_PC)
        player_inventory_save(ch->player_id, ch);
    tell(ch, "<W>*** You have earned the %s! ***<z>\r\n", label);
    /* Game-wide [INFO] announcement, same pattern as combat.c's PC-death
     * taunts (g_descriptors walk, cyan [INFO] tag, earner excluded --
     * they already got the line above). Sash earning is comparatively
     * rare and a genuine milestone, unlike routine mob kills, so this
     * fires every time rather than being random-flavor/best-effort. */
    char announce[192];
    snprintf(announce, sizeof(announce), "\r\n<c>[INFO]<z> %s has earned the %s!\r\n",
             being_display_name(ch), label);
    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (!it->character || it->character == ch)
            continue;
        descriptor_notify(it, announce);
    }
}
static void set_flags(being_t *b, unsigned int add, unsigned int clear) {
    b->monk_quest_flags = (b->monk_quest_flags & ~clear) | add;
    monk_quest_save(b);
}
/* Awards the black sash the moment BOTH trial bits are set, regardless
 * of which one (skill-mastery or the wandering-monk kill) landed last --
 * called from both monk_quest_check_combat_mastery() and
 * monk_quest_on_mob_kill(). No NPC interaction required for this final
 * step, same auto-completion precedent as red sash's skill-threshold
 * transition (see monk_quest_check_combat_mastery()'s own comment). */
static void maybe_award_black_sash(being_t *ch) {
    if ((ch->monk_quest_flags & MQ_BLACK_TRIAL_SKILL)
        && (ch->monk_quest_flags & MQ_BLACK_TRIAL_COMBAT)
        && !(ch->monk_quest_flags & MQ_BLACK_HAS)) {
        award_item(ch, MONK_OBJ_BLACK_SASH, "black sash");
        set_flags(ch, MQ_BLACK_HAS, MQ_BLACK_STARTED);
        tell(ch, "<W>You have completed the Trials of the Unbroken Sash!<z>\r\n"
                 "<W>A black sash settles across your waist.<z>\r\n");
    }
}
void monk_quest_on_levelup(being_t *m) {
    if (!is_monk_pc(m))
        return;
    unsigned int f = m->monk_quest_flags;
    int lvl = m->progress.level;
    if (lvl >= 2 && !(f & (MQ_WHITE_ELIGIBLE | MQ_WHITE_STARTED | MQ_WHITE_HAS))) {
        set_flags(m, MQ_WHITE_ELIGIBLE, 0);
        tell(m, "<c>The monk guildmaster nods approvingly -- ask about a 'white belt' the next time you see him.<z>\r\n");
    }
    if (lvl >= 5 && (f & MQ_WHITE_HAS) && !(f & (MQ_YELLOW_ELIGIBLE | MQ_YELLOW_HAS))) {
        set_flags(m, MQ_YELLOW_ELIGIBLE, 0);
        tell(m, "<c>You feel ready to prove yourself further -- ask the guildmaster about a 'yellow sash'.<z>\r\n");
    }
    if (lvl >= 15 && (f & MQ_YELLOW_HAS) && !(f & (MQ_PURPLE_ELIGIBLE | MQ_PURPLE_STARTED | MQ_PURPLE_HAS))) {
        set_flags(m, MQ_PURPLE_ELIGIBLE, 0);
        tell(m, "<c>The guildmaster senses your growing discipline -- ask him about a 'purple sash'.<z>\r\n");
    }
    if (lvl >= 25 && (f & MQ_PURPLE_HAS)
        && !(f & (MQ_BLUE_ELIGIBLE | MQ_BLUE_STARTED | MQ_BLUE_KILLED_SHARK | MQ_BLUE_HAS))) {
        set_flags(m, MQ_BLUE_ELIGIBLE, 0);
        tell(m, "<c>Word reaches you of an old hermit who trains monks in a blue sash -- seek him out.<z>\r\n");
    }
    if (lvl >= 35 && (f & MQ_BLUE_HAS) && !(f & (MQ_GREEN_ELIGIBLE | MQ_GREEN_STARTED | MQ_GREEN_HAS))) {
        set_flags(m, MQ_GREEN_ELIGIBLE, 0);
        tell(m, "<c>The hermit has another test in mind for you -- ask him about a 'green sash'.<z>\r\n");
    }
    if (lvl >= 45 && (f & MQ_GREEN_HAS)
        && !(f & (MQ_RED_ELIGIBLE | MQ_RED_STARTED | MQ_RED_FINISHED | MQ_RED_HAS))) {
        set_flags(m, MQ_RED_ELIGIBLE, 0);
        tell(m, "<c>You are ready to seek out a castaway guildmaster who trains monks in a red sash.<z>\r\n");
    }
    if (lvl >= 50 && (f & MQ_RED_HAS)
        && !(f & (MQ_BLACK_ELIGIBLE | MQ_BLACK_STARTED | MQ_BLACK_TRIAL_SKILL
                  | MQ_BLACK_TRIAL_COMBAT | MQ_BLACK_HAS))) {
        set_flags(m, MQ_BLACK_ELIGIBLE, 0);
        tell(m, "<c>The castaway eyes you with new respect -- ask him about a 'black sash', if you dare.<z>\r\n");
    }
}
void monk_quest_on_say(being_t *speaker, room_t *r, const char *said) {
    if (!is_monk_pc(speaker) || !r || !said)
        return;
    unsigned int f = speaker->monk_quest_flags;
    being_t *gm207 = find_mob_in_room(r, MONK_MOB_GUILDMASTER_207);
    being_t *huanglo = find_mob_in_room(r, MONK_MOB_HUANGLO);
    being_t *gm223 = find_mob_in_room(r, MONK_MOB_GUILDMASTER_223);
    being_t *gm12509 = find_mob_in_room(r, MONK_MOB_GUILDMASTER_12509);
    /* --- Guildmaster 207: white / yellow / purple --- */
    if (gm207) {
        if (ci_has(said, "white belt")) {
            if (f & MQ_WHITE_ELIGIBLE) {
                set_flags(speaker, MQ_WHITE_STARTED, MQ_WHITE_ELIGIBLE);
                tell(speaker, "The monk guildmaster says, 'Seek out Huang'lo in the tower of the palace, "
                              "northeast of the city, and bring him a bandage.'\r\n");
            } else if (f & MQ_WHITE_HAS) {
                tell(speaker, "The monk guildmaster nods at your white belt. 'Wear it with pride.'\r\n");
            }
        } else if (ci_has(said, "yellow sash")) {
            if ((f & MQ_YELLOW_ELIGIBLE) && (f & MQ_WHITE_HAS)) {
                tell(speaker, "The monk guildmaster says, 'Bring me a metal ashtray, full of tobacco ash -- "
                              "Huang'lo may know where to find one.'\r\n");
            } else if (f & MQ_YELLOW_HAS) {
                tell(speaker, "The monk guildmaster nods at your yellow sash.\r\n");
            }
        } else if (ci_has(said, "purple sash")) {
            if ((f & MQ_PURPLE_ELIGIBLE) && (f & MQ_YELLOW_HAS)) {
                tell(speaker, "The monk guildmaster's face darkens. 'Lepers infest the Dungeon under Grimhaven, "
                              "spreading their rot. Say \"I am ready to slaughter lepers\" when you are prepared "
                              "to end five of them.'\r\n");
            } else if (f & MQ_PURPLE_STARTED) {
                tell(speaker, "The monk guildmaster says, 'Come back to me once you have slain five lepers.' "
                              "(%u/5 so far)\r\n", speaker->monk_purple_kills);
            } else if (f & MQ_PURPLE_HAS) {
                tell(speaker, "The monk guildmaster says, 'You have gone beyond what I can teach -- seek the old "
                              "hermit for your next trial.'\r\n");
            }
        } else if (ci_has(said, "i am ready to slaughter lepers")) {
            if ((f & MQ_PURPLE_ELIGIBLE) && (f & MQ_YELLOW_HAS)) {
                speaker->monk_purple_kills = 0;
                set_flags(speaker, MQ_PURPLE_STARTED, MQ_PURPLE_ELIGIBLE);
                tell(speaker, "The monk guildmaster nods grimly. 'Five lepers, then. Go.'\r\n");
            }
        } else if (ci_has(said, "leper")) {
            if ((f & MQ_PURPLE_STARTED) && speaker->monk_purple_kills >= 5) {
                award_item(speaker, MONK_OBJ_PURPLE_SASH, "purple sash");
                set_flags(speaker, MQ_PURPLE_HAS, MQ_PURPLE_STARTED);
                tell(speaker, "The monk guildmaster ties a <p>purple sash<z> around your waist. 'You have done well -- "
                              "I have taught you all I know. Seek the old hermit for what comes next.'\r\n");
            } else if (f & MQ_PURPLE_STARTED) {
                tell(speaker, "The monk guildmaster says, 'Come back to me once you have slain five lepers.' "
                              "(%u/5 so far)\r\n", speaker->monk_purple_kills);
            }
        }
    }
    /* --- Huang'lo: advice-only, all early stages --- */
    if (huanglo) {
        if (ci_has(said, "white belt") && (f & MQ_WHITE_STARTED)) {
            tell(speaker, "Huang'lo says, 'A bandage will do nicely -- Taloc sells them.'\r\n");
        } else if (ci_has(said, "ash") && (f & MQ_YELLOW_ELIGIBLE)) {
            tell(speaker, "Huang'lo says, 'Check the bars and restaurants of The World -- smokers leave ash "
                          "in their trays.'\r\n");
        } else if (ci_has(said, "leper") && (f & MQ_PURPLE_STARTED)) {
            tell(speaker, "Huang'lo cheers you on. 'Show those lepers no mercy!'\r\n");
        } else if ((ci_has(said, "blue sash") || ci_has(said, "shark")) && (f & MQ_BLUE_STARTED)) {
            tell(speaker, "Huang'lo says, 'Scour the oceans for a tiger shark. Bring a boat, food, and water -- "
                          "and mind the whirlpool.'\r\n");
        } else if (ci_has(said, "elephant") && (f & (MQ_GREEN_ELIGIBLE | MQ_GREEN_STARTED))) {
            tell(speaker, "Huang'lo says, 'Elephants roam the Veldt, south of Grimhaven. Past that, Cimea and "
                          "Cloud City -- you are on your own.'\r\n");
        }
    }
    /* --- Guildmaster 223: blue / green --- */
    if (gm223) {
        if (ci_has(said, "blue sash")) {
            if ((f & MQ_BLUE_ELIGIBLE) && (f & MQ_PURPLE_HAS)) {
                tell(speaker, "The old hermit's eyes grow distant. 'My dog was eaten by a tiger shark while "
                              "fetching a stick at the beach. Say \"I will help you guildmaster\" when you are "
                              "ready to avenge him.'\r\n");
            } else if (f & MQ_BLUE_STARTED) {
                tell(speaker, "The hermit says, 'Find the shark. Mind the whirlpool.'\r\n");
            } else if (f & MQ_BLUE_HAS) {
                tell(speaker, "The hermit nods at your blue sash, eyes still a little misty.\r\n");
            }
        } else if (ci_has(said, "i will help you guildmaster")) {
            if (f & MQ_BLUE_ELIGIBLE) {
                set_flags(speaker, MQ_BLUE_STARTED, MQ_BLUE_ELIGIBLE);
                tell(speaker, "The hermit says, 'Thank you. Mind the whirlpool.'\r\n");
            }
        } else if (ci_has(said, "green sash")) {
            if ((f & MQ_GREEN_ELIGIBLE) && (f & MQ_BLUE_HAS)) {
                set_flags(speaker, MQ_GREEN_STARTED, MQ_GREEN_ELIGIBLE);
                tell(speaker, "The hermit muses, 'Some animals land on their feet -- elephants, lions, cats. "
                              "Others do not. Find an elephant in the Veldt, ride it up to the Empress' Balcony "
                              "in Cimea, and jump.'\r\n");
            } else if (f & MQ_GREEN_STARTED) {
                tell(speaker, "The hermit says, 'Find your elephant, and jump.'\r\n");
            } else if (f & MQ_GREEN_HAS) {
                tell(speaker, "The hermit smiles. 'You landed on your feet, just like I hoped. There is "
                              "\"another matter\" I've been meaning to look into, if you're curious.'\r\n");
            }
        }
    }
    /* --- Guildmaster 12509: red / black --- */
    if (gm12509) {
        if (ci_has(said, "red sash")) {
            if ((f & MQ_RED_ELIGIBLE) && (f & MQ_GREEN_HAS)) {
                set_flags(speaker, MQ_RED_STARTED, MQ_RED_ELIGIBLE);
                tell(speaker, "The castaway says, 'A true monk understands every style of combat. Train your "
                              "barehand, slash, blunt, and pierce proficiencies, each to twenty, and return to me.'\r\n");
            } else if ((f & MQ_RED_STARTED) && !(f & MQ_RED_FINISHED)) {
                tell(speaker, "The castaway says, 'Keep training -- barehand, slash, blunt, pierce, all to twenty.'\r\n");
            } else if (f & MQ_RED_FINISHED) {
                award_item(speaker, MONK_OBJ_RED_SASH, "red sash");
                set_flags(speaker, MQ_RED_HAS, MQ_RED_STARTED | MQ_RED_FINISHED);
                tell(speaker, "The castaway ties a <r>red sash<z> around your waist. 'Well fought.'\r\n");
            } else if (f & MQ_RED_HAS) {
                tell(speaker, "The castaway nods at your red sash.\r\n");
            }
        } else if (ci_has(said, "black sash")) {
            if ((f & MQ_BLACK_ELIGIBLE) && (f & MQ_RED_HAS)) {
                set_flags(speaker, MQ_BLACK_STARTED, MQ_BLACK_ELIGIBLE);
                tell(speaker, "The castaway's expression turns grave. 'The Trials of the Unbroken Sash. Master "
                              "barehand, slash, blunt, and pierce, each to fifty -- and defeat the wandering monk "
                              "in single combat. Few succeed.'\r\n");
            } else if (f & MQ_BLACK_STARTED) {
                bool sk = (f & MQ_BLACK_TRIAL_SKILL) != 0;
                bool cb = (f & MQ_BLACK_TRIAL_COMBAT) != 0;
                tell(speaker, "The castaway says, 'You have %s the skill trial, and %s the wandering monk.'\r\n",
                     sk ? "completed" : "not yet completed", cb ? "defeated" : "not yet defeated");
            } else if (f & MQ_BLACK_HAS) {
                tell(speaker, "The castaway bows to you, master of the Unbroken Sash.\r\n");
            }
        }
    }
}
void monk_quest_on_give(being_t *ch, being_t *vict, obj_t *item) {
    if (!ch || !vict || !item || vict->base.kind != THING_MOB)
        return;
    if (!is_monk_pc(ch))
        return;
    unsigned int f = ch->monk_quest_flags;
    if (vict->base.id == MONK_MOB_GUILDMASTER_207) {
        if (item->vnum == MONK_OBJ_BANDAGE && (f & MQ_WHITE_STARTED)) {
            obj_destroy(item);
            award_item(ch, MONK_OBJ_WHITE_BELT, "white belt");
            set_flags(ch, MQ_WHITE_HAS, MQ_WHITE_STARTED);
            tell(ch, "The monk guildmaster sews the bandage into a <W>white belt<z> and hands it to you.\r\n");
        } else if (item->vnum == MONK_OBJ_ASHTRAY && (f & MQ_YELLOW_ELIGIBLE) && (f & MQ_WHITE_HAS)) {
            obj_destroy(item);
            award_item(ch, MONK_OBJ_YELLOW_SASH, "yellow sash");
            set_flags(ch, MQ_YELLOW_HAS, MQ_YELLOW_ELIGIBLE);
            tell(ch, "The monk guildmaster empties the ashtray, satisfied, and hands you a <Y>yellow sash<z>.\r\n");
        }
    } else if (vict->base.id == MONK_MOB_GUILDMASTER_223) {
        if (item->vnum == MONK_OBJ_DOG_COLLAR && (f & MQ_BLUE_KILLED_SHARK)) {
            obj_destroy(item);
            award_item(ch, MONK_OBJ_BLUE_SASH, "blue sash");
            set_flags(ch, MQ_BLUE_HAS, MQ_BLUE_STARTED | MQ_BLUE_KILLED_SHARK);
            tell(ch, "The old hermit's hands tremble as he takes the collar. 'Thank you.' He hands you a "
                     "<b>blue sash<z>.\r\n");
        }
    }
}
void monk_quest_on_mob_kill(being_t *winner, int loser_vnum, obj_t *corpse) {
    if (!is_monk_pc(winner))
        return;
    unsigned int f = winner->monk_quest_flags;
    if (loser_vnum == MONK_MOB_LEPER && (f & MQ_PURPLE_STARTED)) {
        if (winner->monk_purple_kills < 5) {
            winner->monk_purple_kills++;
            monk_quest_save(winner);
            if (winner->monk_purple_kills >= 5)
                tell(winner, "<p>You have slain your fifth leper. Return to the monk guildmaster.<z>\r\n");
            else
                tell(winner, "<p>Leper slain (%u/5).<z>\r\n", winner->monk_purple_kills);
        }
    } else if (loser_vnum == MONK_MOB_TIGER_SHARK && (f & MQ_BLUE_STARTED)
               && !(f & MQ_BLUE_KILLED_SHARK)) {
        set_flags(winner, MQ_BLUE_KILLED_SHARK, 0);
        if (corpse) {
            obj_t *collar = obj_create_from_proto(MONK_OBJ_DOG_COLLAR);
            if (collar)
                thing_move_to(&collar->base, &corpse->base);
        }
        tell(winner, "<b>Something glints in the shark's remains -- a golden dog collar.<z>\r\n");
    } else if (loser_vnum == MONK_MOB_WANDERING_MONK && (f & MQ_BLACK_STARTED)
               && !(f & MQ_BLACK_TRIAL_COMBAT)) {
        set_flags(winner, MQ_BLACK_TRIAL_COMBAT, 0);
        tell(winner, "<W>You have bested the wandering monk in single combat!<z>\r\n");
        maybe_award_black_sash(winner);
    }
}
void monk_quest_check_combat_mastery(being_t *ch) {
    if (!is_monk_pc(ch))
        return;
    unsigned int f = ch->monk_quest_flags;
    bool red_pending = (f & MQ_RED_STARTED) && !(f & MQ_RED_FINISHED);
    bool black_pending = (f & MQ_BLACK_STARTED) && !(f & MQ_BLACK_TRIAL_SKILL);
    if (!red_pending && !black_pending)
        return;
    static const char *const PROF_NAMES[4] = {
        "barehand proficiency", "slash proficiency", "blunt proficiency", "pierce proficiency"
    };
    int worst = 999;
    for (int i = 0; i < 4; i++) {
        const skill_def_t *sk = skill_find(ch->char_class, PROF_NAMES[i], false);
        int pct = sk ? skill_proficiency(ch, sk) : 0;
        if (pct < worst)
            worst = pct;
    }
    if (red_pending && worst >= 20) {
        set_flags(ch, MQ_RED_FINISHED, 0);
        tell(ch, "<r>You have mastered all four combat styles -- the castaway guildmaster will want to hear "
                 "about this.<z>\r\n");
    }
    if (black_pending && worst >= 50) {
        set_flags(ch, MQ_BLACK_TRIAL_SKILL, 0);
        tell(ch, "<W>You have mastered all four combat styles to a legendary degree.<z>\r\n");
        maybe_award_black_sash(ch);
    }
}
void monk_quest_on_room_enter(being_t *ch, room_t *to) {
    if (!is_monk_pc(ch) || !to)
        return;
    if (to->vnum != MONK_ROOM_BALCONY_FALL)
        return;
    if (!(ch->monk_quest_flags & MQ_GREEN_STARTED))
        return;
    if (!ch->mount || ch->mount->base.kind != THING_MOB || ch->mount->base.id != MONK_MOB_ELEPHANT)
        return;
    room_t *landing = world_get_room(MONK_ROOM_LANDING);
    if (!landing)
        return;
    tell(ch, "<c>Your elephant leaps from the balcony -- the wind roars past you as you plummet, then land "
             "cleanly on all fours, high on a mountainside.<z>\r\n");
    being_t *mount = ch->mount;
    thing_set_room(&ch->base, landing);
    if (mount)
        thing_set_room(&mount->base, landing);
    award_item(ch, MONK_OBJ_GREEN_SASH, "green sash");
    set_flags(ch, MQ_GREEN_HAS, MQ_GREEN_STARTED);
    /* The original awards the `catfall` skill here via a toggle-driven
     * skill grant; Tobin has no such mechanism (skill.h: a skill is known
     * purely by class + level, no practice-point/toggle economy). Catfall
     * is already a level-25 Monk skill in skill.c's roster -- well below
     * green sash's own level-35 gate -- so every player reaching this
     * point already has it. Documented deviation, not a missing feature. */
    tell(ch, "<g>You land like a cat!<z>\r\n");
}
void monk_quest_save(being_t *b) {
    if (!b || b->base.kind != THING_PC || b->player_id <= 0)
        return;
    player_set_monk_quest(b->player_id, b->monk_quest_flags, b->monk_purple_kills);
}
