#include "being.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "descriptor.h"

being_t *being_create_pc(const char *name, long account_id, long player_id) {
    being_t *b = calloc(1, sizeof(*b));
    if (!b)
        return NULL;

    b->base.kind = THING_PC;
    b->base.id = (int)player_id;
    snprintf(b->base.name, sizeof(b->base.name), "%s", name ? name : "");
    snprintf(b->base.short_descr, sizeof(b->base.short_descr), "%s", name ? name : "");
    b->account_id = account_id;
    b->player_id = player_id;
    b->attrs.strength = b->attrs.dexterity = b->attrs.constitution =
        b->attrs.intelligence = b->attrs.wisdom = b->attrs.charisma = ATTR_BASE;

    b->progress.level = MORTAL_LEVEL_MIN;
    b->progress.experience = 0;
    b->progress.max_hp = being_calc_max_hp(b);
    b->progress.hp = b->progress.max_hp;
    being_limbs_full_heal(b);

    /* fighting, last_combat_pulse, wait_pulses are already zeroed by calloc */

    return b;
}

void being_destroy(being_t *b) {
    if (!b)
        return;

    /* Clear any dangling reference from an opponent still fighting us --
     * otherwise the next combat round would dereference freed memory. */
    for (descriptor_t *d = g_descriptors; d; d = d->next) {
        if (d->character && d->character->fighting == b)
            d->character->fighting = NULL;
    }

    thing_remove_from_parent(&b->base);
    free(b);
}

bool being_is_immortal(const being_t *b) {
    return b && b->progress.level >= IMMORTAL_LEVEL_MIN;
}

void being_normalize_name(char *name) {
    if (!name || !name[0])
        return;
    name[0] = (char)toupper((unsigned char)name[0]);
    for (int i = 1; name[i]; i++)
        name[i] = (char)tolower((unsigned char)name[i]);
}

const char *being_level_title(int level) {
    if (level < IMMORTAL_LEVEL_MIN)
        return NULL;
    if (level <= 53)
        return "Immortal";
    if (level <= 57)
        return "God";
    if (level == 58)
        return "Greater God";
    if (level == 59)
        return "Administrator";
    return "Implementor"; /* 60+ */
}

int being_get_wait(const being_t *b) {
    if (!b)
        return 0;
    return being_is_immortal(b) ? 0 : b->wait_pulses;
}

void being_set_wait(being_t *b, int pulses) {
    if (!b || being_is_immortal(b))
        return;
    b->wait_pulses = pulses;
}

int being_calc_max_hp(const being_t *b) {
    if (!b)
        return 20;
    int con_bonus = b->attrs.constitution - ATTR_BASE;
    return 20 + con_bonus + (b->progress.level * 5);
}

static const char *LIMB_NAMES[LIMB_COUNT] = {
    "head", "neck", "left arm", "right arm", "left finger", "right finger",
    "body", "waist", "genitalia", "right leg", "left leg", "left foot", "right foot",
};

const char *limb_name(limb_t limb) {
    if (limb < 0 || limb >= LIMB_COUNT)
        return "body";
    return LIMB_NAMES[limb];
}

void being_limbs_full_heal(being_t *b) {
    if (!b)
        return;
    int share = b->progress.max_hp / LIMB_COUNT;
    if (share < 1)
        share = 1;
    for (int i = 0; i < LIMB_COUNT; i++) {
        b->limbs[i].max_hp = share;
        b->limbs[i].hp = share;
    }
}

void being_hurt_limb(being_t *b, limb_t limb, int dmg) {
    if (!b || dmg <= 0 || limb < 0 || limb >= LIMB_COUNT)
        return;
    b->progress.hp -= dmg;
    b->limbs[limb].hp -= dmg;
    if (b->limbs[limb].hp < 0)
        b->limbs[limb].hp = 0;
}

int being_limb_pct(const being_t *b, limb_t limb) {
    if (!b || limb < 0 || limb >= LIMB_COUNT || b->limbs[limb].max_hp <= 0)
        return 0;
    int pct = (b->limbs[limb].hp * 100) / b->limbs[limb].max_hp;
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    return pct;
}

const char *limb_status_text(int pct) {
    if (pct <= 0)
        return "is destroyed and needs medical attention";
    if (pct < 10)
        return "needs medical attention";
    if (pct < 20)
        return "is hurt rather badly";
    return NULL;
}

bool being_has_destroyed_limb(const being_t *b) {
    if (!b)
        return false;
    for (int i = 0; i < LIMB_COUNT; i++) {
        if (b->limbs[i].hp <= 0)
            return true;
    }
    return false;
}

void being_heal(being_t *b, int amount) {
    if (!b || amount <= 0)
        return;
    b->progress.hp += amount;
    if (b->progress.hp > b->progress.max_hp)
        b->progress.hp = b->progress.max_hp;
    for (int i = 0; i < LIMB_COUNT; i++) {
        b->limbs[i].hp += amount;
        if (b->limbs[i].hp > b->limbs[i].max_hp)
            b->limbs[i].hp = b->limbs[i].max_hp;
    }
}

long progress_xp_for_level(int level) {
    return (long)level * (long)level * 100;
}

int progress_add_xp(progress_t *p, long xp_gain) {
    if (!p || xp_gain <= 0)
        return 0;

    p->experience += xp_gain;

    int levels_gained = 0;
    while (p->level < MORTAL_LEVEL_MAX && p->experience >= progress_xp_for_level(p->level + 1)) {
        p->level++;
        levels_gained++;
    }
    return levels_gained;
}
