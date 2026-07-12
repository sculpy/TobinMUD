/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "player_repo.h"
#include "thing.h"

/* `practice` (user 2026-07-12: "add the practice command so players have
 * to visit a guildmaster to gain skills based upon percentage of
 * discipline learned. cant get to advanced disc until basic disc is at
 * least 95% complete"). A player's Basic (SKILL_TIER_CLASS) and Advanced
 * (SKILL_TIER_ADVANCED) discipline are each tracked as a single 0-100
 * aggregate percentage (progress_t.basic_disc_pct/advanced_disc_pct) --
 * NOT a per-skill percentage. That coarser scope is deliberate: the user's
 * own wording ("percentage of discipline LEARNED... basic disc... advanced
 * disc") describes progress through the discipline as a whole, and it
 * avoids needing a new per-player-per-skill table before any of the
 * ~230-entry roster has real bespoke mechanics anyway (see cmd_cast.c's
 * task_cast() header comment for the same "don't over-build ahead of the
 * roster" reasoning).
 *
 * A "guildmaster" is a mob in the caller's room keyworded "guildmaster"
 * whose mob.class (mob_repo.c's class_mask, mapped in being_create_mob())
 * matches the caller's own class -- a guildmaster of a different class
 * can't train you (Basic/Advanced are each specific to your own
 * discipline). No practice-session resource cost yet (no
 * mana/stamina-pool precedent to hang it on, same v1 scope as cast/pray's
 * missing mana cost) -- each successful `practice` raises the relevant
 * percentage by a flat step, capped at 100. */

#define PRACTICE_STEP 10

static being_t *find_guildmaster(const being_t *ch) {
    if (!ch->base.roomp)
        return NULL;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_MOB)
            continue;
        being_t *mob = (being_t *)t;
        if (!mob->mob_class_known || mob->char_class != ch->char_class)
            continue;
        if (thing_name_matches(t->name, "guildmaster", strlen("guildmaster")))
            return mob;
    }
    return NULL;
}

bool cmd_practice(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    while (*args == ' ')
        args++;

    being_t *gm = find_guildmaster(ch);
    if (!gm) {
        descriptor_send(d, "You don't see a guildmaster of your discipline here.\r\n");
        return true;
    }

    if (!*args) {
        char gmname[128];
        being_display_name_cap(gm, gmname, sizeof(gmname));
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "%s says, \"Basic discipline: %d%%.  Advanced discipline: %d%%%s\"\r\n"
                 "Type 'practice basic' or 'practice advanced' to train.\r\n",
                 gmname, ch->progress.basic_disc_pct, ch->progress.advanced_disc_pct,
                 ch->progress.basic_disc_pct < 95 ? " (locked until Basic reaches 95%)" : "");
        descriptor_send(d, msg);
        return true;
    }

    char word[32];
    snprintf(word, sizeof(word), "%s", args);
    char *sp = strchr(word, ' ');
    if (sp)
        *sp = '\0';
    size_t wlen = strlen(word);

    bool advanced = wlen && strncasecmp(word, "advanced", wlen) == 0;
    bool basic = !advanced && wlen && strncasecmp(word, "basic", wlen) == 0;
    if (!advanced && !basic) {
        descriptor_send(d, "Practice what? Try 'practice basic' or 'practice advanced'.\r\n");
        return true;
    }

    if (advanced) {
        if (ch->progress.basic_disc_pct < 95) {
            descriptor_send(d, "The guildmaster shakes their head. \"Master your Basic discipline first -- 95% or better.\"\r\n");
            return true;
        }
        if (ch->progress.advanced_disc_pct >= 100) {
            descriptor_send(d, "You have already mastered your Advanced discipline.\r\n");
            return true;
        }
        ch->progress.advanced_disc_pct += PRACTICE_STEP;
        if (ch->progress.advanced_disc_pct > 100)
            ch->progress.advanced_disc_pct = 100;
        char msg[160];
        snprintf(msg, sizeof(msg), "You practice with the guildmaster. Advanced discipline: %d%%.\r\n",
                 ch->progress.advanced_disc_pct);
        descriptor_send(d, msg);
    } else {
        if (ch->progress.basic_disc_pct >= 100) {
            descriptor_send(d, "You have already mastered your Basic discipline.\r\n");
            return true;
        }
        ch->progress.basic_disc_pct += PRACTICE_STEP;
        if (ch->progress.basic_disc_pct > 100)
            ch->progress.basic_disc_pct = 100;
        char msg[160];
        snprintf(msg, sizeof(msg), "You practice with the guildmaster. Basic discipline: %d%%.\r\n",
                 ch->progress.basic_disc_pct);
        descriptor_send(d, msg);
    }

    player_progress_save(ch->player_id, &ch->progress);
    return true;
}
