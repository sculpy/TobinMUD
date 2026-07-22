/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "material.h"
#include "obj.h"
#include "obj_repo.h"
#include "thing.h"

/* Same case-insensitive per-keyword prefix match cmd_drink.c/cmd_sip.c/
 * cmd_eat.c each keep their own local copy of (cmd_object.c's own
 * obj_name_matches() is static to that file). */
static bool keyword_matches(const char *keywords, const char *tok) {
    size_t tok_len = strlen(tok);
    if (tok_len == 0)
        return false;
    const char *p = keywords;
    while (*p) {
        while (*p == ' ')
            p++;
        const char *start = p;
        while (*p && *p != ' ')
            p++;
        size_t wlen = (size_t)(p - start);
        if (wlen >= tok_len && strncasecmp(start, tok, tok_len) == 0)
            return true;
    }
    return false;
}

/* `identify <item>` (Sneezy → Tobin feature audit, "Object manipulation
 * depth"). Checked Sneezy's own `identify` spell help first: "informative
 * spell for yielding more intelligence about a creature or object,"
 * accuracy scaling with caster skill. Implemented as a plain command, not
 * a spell -- same tier as `examine`/`consider` (other info-reveal commands
 * that aren't spell-gated), and Tobin's val[] payload is small/finite
 * enough that "accuracy scaling with skill" has nothing real to scale
 * (there's no hidden/fuzzy data to reveal partially). Deliberately does
 * NOT show `stat`'s full prototype dump (raw obj.type name, actual/action
 * flags, DB-level detail) -- that stays the immortal-only deep-dive;
 * identify shows the same practical, category-specific val[] breakdown a
 * player actually needs to decide whether to use/wear/wield something,
 * translated to plain language (obj.h's val[] doc comment). Structure/
 * material aren't shown -- that data isn't meaningfully populated yet
 * (separate, still-open "Object maintenance" audit item). Searches
 * carried inventory only, same scope as `eat`/`drop`/`junk` (not the room
 * floor or a target's own inventory). */
bool cmd_identify(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Identify what?\r\n");
        return true;
    }
    const char *tok;
    int ordinal = thing_parse_ordinal(raw, &tok);

    obj_t *o = NULL;
    int seen = 0;
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        if (!keyword_matches(t->name, tok))
            continue;
        seen++;
        if (seen != ordinal)
            continue;
        o = (obj_t *)t;
        break;
    }

    if (!o) {
        descriptor_send(d, "You aren't carrying that.\r\n");
        return true;
    }

    const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
    char out[512];
    int n = snprintf(out, sizeof(out),
                      "You identify %s:\r\n"
                      "  Category:  %s\r\n"
                      "  Weight:    %.1f\r\n"
                      "  Material:  %s\r\n",
                      label, obj_category_name(o->category), o->weight,
                      material_tier_name(material_tier_for_id(o->material)));
    const char *cond = obj_condition_word(o);
    if (cond && (size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  Condition: %s\r\n", cond);

    switch (o->category) {
        case OBJ_CAT_WEAPON: {
            /* val[0]/val[1] are NOT real dice despite obj.h's own val[]
             * doc comment claiming "damage dice count/sides" -- verified
             * against the real seeded data (e.g. vnum 175 "dart simple
             * wooden" carries val0=4626, val1=2073, nonsense as dice) and
             * against combat.c's actual damage formula, which never reads
             * either field -- real damage is a flat
             * 1 + str_bonus + rand()%6 + weapon_damroll, weapon_damroll
             * coming from the objaffect companion table
             * (obj_load_combat_mods()), not val[]. Showing only what's
             * mechanically real. */
            int hitroll = 0, damroll = 0;
            obj_load_combat_mods(o->vnum, &hitroll, &damroll);
            if (hitroll || damroll)
                n += snprintf(out + n, sizeof(out) - (size_t)n,
                               "  Bonus:     +%d to-hit, +%d damage\r\n", hitroll, damroll);
            else
                n += snprintf(out + n, sizeof(out) - (size_t)n,
                               "  Bonus:     none\r\n");
            break;
        }
        case OBJ_CAT_ARMOR:
            n += snprintf(out + n, sizeof(out) - (size_t)n,
                           "  Armor:     %d AC\r\n", obj_armor_ac(o));
            break;
        case OBJ_CAT_CONTAINER:
            n += snprintf(out + n, sizeof(out) - (size_t)n,
                           "  Capacity:  %d lbs%s\r\n", o->val[0],
                           (o->val[1] & CONT_CLOSEABLE) ? " (closeable)" : "");
            break;
        case OBJ_CAT_DRINK:
            n += snprintf(out + n, sizeof(out) - (size_t)n,
                           "  Liquid:    %d/%d units remaining\r\n", o->val[1], o->val[0]);
            break;
        case OBJ_CAT_FOOD:
            n += snprintf(out + n, sizeof(out) - (size_t)n,
                           "  Nutrition: restores %d hunger when eaten\r\n", o->val[0]);
            break;
        case OBJ_CAT_MAGIC_DEVICE:
            /* val[0] ("charges") is unreliable raw import data, not real
             * game state -- verified against the real seed (a plausible
             * 5/5 on potions, but a nonsense 25650 on one scroll) and
             * against the code: nothing reads it yet, since there's no
             * `use`/`zap`/`quaff` command to spend a charge with (the
             * still-open "Magic items" audit item builds that). Honest
             * about the gap rather than printing a number that might be
             * garbage. */
            n += snprintf(out + n, sizeof(out) - (size_t)n,
                           "  It's a magic item -- not yet usable (no scroll/wand/staff system exists).\r\n");
            break;
        case OBJ_CAT_KEY:
            n += snprintf(out + n, sizeof(out) - (size_t)n,
                           "  It's a key -- matched by whatever lock names its own vnum.\r\n");
            break;
        default:
            break;
    }

    descriptor_send(d, out);
    return true;
}
