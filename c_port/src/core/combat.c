#include "combat.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "descriptor.h"
#include "player_repo.h"
#include "room.h"
#include "thing.h"

/* Best-effort message to b's connection, if any -- no-op for a mob (once
 * mobs exist) or a being whose descriptor already went away. */
static void tell(being_t *b, const char *fmt, ...) {
    if (!b || !b->desc)
        return;
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    descriptor_send(b->desc, buf);
}

/* A destroyed limb (0% HP) penalizes its owner's own offense -- flat,
 * non-stacking (doesn't get worse with more than one destroyed limb),
 * placeholder amount. There's no hospital system yet to repair it mid-game
 * -- see being_has_destroyed_limb()'s doc comment in being.h. */
#define DESTROYED_LIMB_HIT_PENALTY 15

/* Placeholder damage formula, built from the existing 6-attribute set --
 * not the original's weapon/class/skill-driven system (none of that
 * exists yet). Hit chance skews on relative DEX (and is penalized if the
 * attacker has a destroyed limb); damage scales with STR above ATTR_BASE
 * plus a small random component. Each hit lands on a uniformly-random limb
 * (not the original's slotChance()-weighted roll, see being.h) -- damage
 * is applied to both that limb's HP and the defender's overall HP via
 * being_hurt_limb(). Crossing into a worse limb_status_text() tier (see
 * being.h) announces it to both sides. */
static void combat_strike(being_t *attacker, being_t *defender) {
    int hit_roll = (rand() % 100) + (attacker->attrs.dexterity - defender->attrs.dexterity) / 4;
    if (being_has_destroyed_limb(attacker))
        hit_roll -= DESTROYED_LIMB_HIT_PENALTY;
    if (hit_roll < 50) {
        tell(attacker, "You miss %s!\r\n", defender->base.name);
        tell(defender, "%s misses you!\r\n", attacker->base.name);
        return;
    }

    int dmg = 1 + (attacker->attrs.strength - ATTR_BASE) / 4 + (rand() % 6);
    if (dmg < 1)
        dmg = 1;

    limb_t limb = (limb_t)(rand() % LIMB_COUNT);
    int pct_before = being_limb_pct(defender, limb);
    being_hurt_limb(defender, limb, dmg);
    int pct_after = being_limb_pct(defender, limb);

    const char *ln = limb_name(limb);
    tell(attacker, "You hit %s's %s for %d damage!\r\n", defender->base.name, ln, dmg);
    tell(defender, "%s hits your %s for %d damage!\r\n", attacker->base.name, ln, dmg);

    const char *status_before = limb_status_text(pct_before);
    const char *status_after = limb_status_text(pct_after);
    if (status_after && status_after != status_before) {
        tell(attacker, "%s's %s %s!\r\n", defender->base.name, ln, status_after);
        tell(defender, "Your %s %s!\r\n", ln, status_after);
    }
}

/* No permadeath in the sense of the character record being deleted -- the
 * loser's HP is patched up to half max (so their next login isn't stuck at
 * 0) and their limbs fully heal. But losing now genuinely ends the play
 * session: the loser is unloaded and dropped at the account menu (same
 * path `quit!`-while-playing uses, descriptor_leave_to_menu() in
 * descriptor.c) rather than respawning in-place still playing -- they can
 * pick the same character back up from there, or create/play another, or
 * leave. `slain` only picks the flavor of the first message line (normal
 * combat loss vs. an immortal's instant kill via combat_instakill()) --
 * both end the same way. */
static void combat_defeat(being_t *loser, being_t *winner, bool slain) {
    loser->fighting = NULL;
    winner->fighting = NULL;

    loser->progress.hp = loser->progress.max_hp / 2;
    if (loser->progress.hp < 1)
        loser->progress.hp = 1;
    being_limbs_full_heal(loser);
    player_progress_save(loser->player_id, &loser->progress);

    if (slain) {
        tell(winner, "You have slain %s!\r\n", loser->base.name);
        tell(loser, "You have been slain by %s!\r\nYou are DEAD!\r\n", winner->base.name);
    } else {
        tell(winner, "You have defeated %s!\r\n", loser->base.name);
        tell(loser, "You have been defeated by %s!\r\nYou are DEAD!\r\n", winner->base.name);
    }

    /* A death is world news (user requirement): everyone playing -- not
     * just the room -- gets a teasing announcement. Winner and loser are
     * excluded; they already got their own lines above. */
    static const char *const DEATH_TAUNTS[] = {
        "The gods pause their board game to note that %s has been slain by %s.",
        "%s is dead. %s looks insufferably pleased about it.",
        "A distant bell tolls once for %s. %s rang it.",
        "%s's limbs are now a matter of public record, courtesy of %s.",
    };
    char taunt[224];
    int t = rand() % (int)(sizeof(DEATH_TAUNTS) / sizeof(DEATH_TAUNTS[0]));
    /* [INFO] channel prefix (user requirement) -- cyan when color is on,
     * stripped to plain "[INFO]" when off. */
    int n = snprintf(taunt, sizeof(taunt), "\r\n<c>[INFO]<z> ");
    n += snprintf(taunt + n, sizeof(taunt) - (size_t)n, DEATH_TAUNTS[t],
                  loser->base.name, winner->base.name);
    snprintf(taunt + n, sizeof(taunt) - (size_t)n, "\r\n");
    for (descriptor_t *it = g_descriptors; it; it = it->next) {
        if (it->state != CONN_PLAYING || !it->character)
            continue;
        if (it->character == winner || it->character == loser)
            continue;
        descriptor_send(it, taunt);
    }

    if (loser->desc)
        descriptor_leave_to_menu(loser->desc);
}

void combat_process_run(long pulse_num) {
    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        being_t *a = d->character;
        if (!a || !a->fighting)
            continue;
        if (a->last_combat_pulse == pulse_num)
            continue; /* already resolved this round via the other participant */

        being_t *b = a->fighting;
        a->last_combat_pulse = pulse_num;
        b->last_combat_pulse = pulse_num;

        combat_strike(a, b);
        if (b->progress.hp <= 0) {
            combat_defeat(b, a, false);
            continue;
        }

        combat_strike(b, a);
        if (a->progress.hp <= 0)
            combat_defeat(a, b, false);
    }
}

being_t *combat_find_room_target(being_t *self, const char *name) {
    if (!self || !self->base.roomp || !name || !*name)
        return NULL;

    /* Exact name first, so "Clau" always means the player literally named
     * Clau even if a Claudius is also in the room; then fall back to prefix
     * matching ("kill clau" -> Claudius), same abbreviation convention the
     * command parser has used since Session 9 and the original's
     * is_abbrev()-based get_char_room targeting. First prefix match in room
     * order wins. */
    being_t *prefix_match = NULL;
    size_t name_len = strlen(name);
    for (thing_t *t = self->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t == &self->base)
            continue;
        if (t->kind != THING_PC)
            continue;
        if (strcasecmp(t->name, name) == 0)
            return (being_t *)t;
        if (!prefix_match && strncasecmp(t->name, name, name_len) == 0)
            prefix_match = (being_t *)t;
    }
    return prefix_match;
}

void combat_instakill(being_t *attacker, being_t *target) {
    if (!attacker || !target)
        return;

    target->progress.hp = 0;
    for (int i = 0; i < LIMB_COUNT; i++)
        target->limbs[i].hp = 0;

    combat_defeat(target, attacker, true);
}
