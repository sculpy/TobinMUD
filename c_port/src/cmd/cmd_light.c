/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "obj.h"
#include "room.h"
#include "thing.h"

/* `light`/`extinguish`/`refuel` (Sneezy port, user 2026-07-18: "light
 * refuel and the lamp lighting boy code need to be implemented, from
 * sneezy"). Real seeded OBJ_CAT_LIGHT objects (lampposts, torches,
 * lanterns) and ITEM_FUEL objects ("a small brick of solid fuel", sold
 * at Lumor's Illuminations, room 550) already exist and are already
 * reachable -- this is the missing command layer on top, matching the
 * original's obj_light.cc/obj_fuel.cc split: `TLight::refuelMeLight()`
 * just re-dispatches to `TFuel::refuelMeFuel()`, which does the actual
 * transfer and rejects a lit lamp ("might explode") or an unrefuelable
 * one (`maxBurn < 0`, e.g. a torch). See obj.h's val[] comment for the
 * exact field layout (val[0]=radius, val[1]=max burn, val[2]=current
 * burn, val[3]=is lit for LIGHT; val[0]=current fuel, val[1]=max fuel
 * for FUEL).
 *
 * Deliberately NOT ported: darkness/visibility (an unlit room has no
 * gameplay effect on `look` in Tobin yet -- a separate, much bigger
 * system; this is flavor + a real resource sink for now, same "v1 scope"
 * precedent as everything else this session). Burn-down while lit is a
 * pulse tick (obj_light_burn_tick(), obj.c), same cadence as pool decay. */

#define LIGHT_UNREFUELABLE (-1)

/* Same keyword-abbreviation matching spirit as cmd_drink.c's own local
 * copy -- duplicated rather than shared, same established precedent. */
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

/* Ordinal-aware object search over one chain, filtered to OBJ_CAT_LIGHT --
 * same "N.keyword" convention as cmd_look.c's find_obj_here(). */
static obj_t *find_light_in_chain(thing_t *chain, const char *tok, size_t len, int ordinal) {
    int seen = 0;
    for (thing_t *t = chain; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (o->category != OBJ_CAT_LIGHT || !thing_name_matches(t->name, tok, len))
            continue;
        seen++;
        if (seen == ordinal)
            return o;
    }
    return NULL;
}

/* `<item> [held|room]` -- an explicit scope word (same syntax the
 * original's `light`/`extinguish`/`refuel` all share) picks carried/worn
 * items only, or the room floor only; omitted, tries carried/worn first
 * then the room floor, same order cmd_look.c's object search uses. */
static obj_t *find_light_target(being_t *ch, const char *raw, const char *scope) {
    const char *tok;
    int ordinal = thing_parse_ordinal(raw, &tok);
    size_t len = strlen(tok);

    size_t slen = scope ? strlen(scope) : 0;
    bool room_only = slen && strncasecmp(scope, "room", slen) == 0;
    bool held_only = slen && strncasecmp(scope, "held", slen) == 0;

    obj_t *o = NULL;
    if (!room_only)
        o = find_light_in_chain(ch->base.stuff_head, tok, len, ordinal);
    if (!o && !held_only && ch->base.roomp)
        o = find_light_in_chain(ch->base.roomp->base.stuff_head, tok, len, ordinal);
    return o;
}

/* Display name for a light/fuel item in player-facing messages -- prefers
 * short_descr, falling back to the raw keyword name. */
static const char *light_label(const obj_t *o) {
    return o->base.short_descr[0] ? o->base.short_descr : o->base.name;
}

/* Same duplication precedent as cmd_object.c's own cap_first() -- skips
 * a leading inline color tag ("<o>a torch<1>") before capitalizing, for
 * a label that opens a sentence instead of following "You ..."/"...
 * refuel " mid-sentence. */
static const char *cap_first(const char *label, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%s", label);
    size_t i = 0;
    while (buf[i] == '<' && buf[i + 1] != '\0' && buf[i + 2] == '>')
        i += 3;
    if (buf[i])
        buf[i] = (char)toupper((unsigned char)buf[i]);
    return buf;
}

/* `light <item> [held|room]` -- lights an unlit OBJ_CAT_LIGHT item that
 * still has fuel remaining (val[2] > 0). See the file's top comment for
 * the val[] field layout and the original's obj_light.cc this mirrors. */
bool cmd_light(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch) return true;

    char raw[64] = "", scope[16] = "";
    if (sscanf(args, "%63s %15s", raw, scope) < 1) {
        descriptor_send(d, "Usage: light <item> [held|room]\r\n");
        return true;
    }

    obj_t *o = find_light_target(ch, raw, scope[0] ? scope : NULL);
    if (!o) {
        descriptor_send(d, "You don't have (or see) that here to light.\r\n");
        return true;
    }
    if (o->val[3]) {
        descriptor_send(d, "It's already lit.\r\n");
        return true;
    }
    if (o->val[2] <= 0) {
        descriptor_send(d, "There's no fuel left to light it with.\r\n");
        return true;
    }

    o->val[3] = 1;
    char msg[256];
    snprintf(msg, sizeof(msg), "You light %s.\r\n", light_label(o));
    descriptor_send(d, msg);
    if (ch->base.roomp) {
        snprintf(msg, sizeof(msg), "%s lights %s.\r\n", ch->base.name, light_label(o));
        descriptor_room_echo(ch->base.roomp, ch, msg);
    }
    return true;
}

/* `extinguish <item> [held|room]` -- puts out a currently-lit light item. */
bool cmd_extinguish(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch) return true;

    char raw[64] = "", scope[16] = "";
    if (sscanf(args, "%63s %15s", raw, scope) < 1) {
        descriptor_send(d, "Usage: extinguish <item> [held|room]\r\n");
        return true;
    }

    obj_t *o = find_light_target(ch, raw, scope[0] ? scope : NULL);
    if (!o) {
        descriptor_send(d, "You don't have (or see) that here to extinguish.\r\n");
        return true;
    }
    if (!o->val[3]) {
        descriptor_send(d, "It's not lit.\r\n");
        return true;
    }

    o->val[3] = 0;
    char msg[256];
    snprintf(msg, sizeof(msg), "You extinguish %s.\r\n", light_label(o));
    descriptor_send(d, msg);
    if (ch->base.roomp) {
        snprintf(msg, sizeof(msg), "%s extinguishes %s.\r\n", ch->base.name, light_label(o));
        descriptor_room_echo(ch->base.roomp, ch, msg);
    }
    return true;
}

/* `refuel <light> <fuel> [held|room]` -- tops off an unlit, refuelable
 * light item's val[2] (current burn) from a carried fuel item's val[0]
 * (current fuel), transferring only as much as the light still has room
 * for; the fuel item is destroyed once it's used up. Refuses a lit light
 * ("might explode") same as the original TFuel::refuelMeFuel(). */
bool cmd_refuel(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch) return true;

    char light_raw[64] = "", fuel_raw[64] = "", scope[16] = "";
    int n = sscanf(args, "%63s %63s %15s", light_raw, fuel_raw, scope);
    if (n < 2) {
        descriptor_send(d, "Usage: refuel <light> <fuel> [held|room]\r\n");
        return true;
    }

    obj_t *light = find_light_target(ch, light_raw, scope[0] ? scope : NULL);
    if (!light) {
        descriptor_send(d, "You don't have (or see) that here to refuel.\r\n");
        return true;
    }

    /* Fuel is always searched in your own inventory -- you refuel a lamp
     * FROM a brick you're carrying, same as the original's own
     * get_thing_char_using() scope for the fuel argument. */
    const char *fuel_tok;
    int fuel_ordinal = thing_parse_ordinal(fuel_raw, &fuel_tok);
    obj_t *fuel = NULL;
    int seen = 0;
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (!keyword_matches(o->base.name, "fuel") || !thing_name_matches(t->name, fuel_tok, strlen(fuel_tok)))
            continue;
        seen++;
        if (seen == fuel_ordinal) {
            fuel = o;
            break;
        }
    }
    if (!fuel) {
        descriptor_send(d, "You aren't carrying that fuel.\r\n");
        return true;
    }

    if (light->val[1] == LIGHT_UNREFUELABLE) {
        char capbuf[128], msg[160];
        snprintf(msg, sizeof(msg), "%s can't be refueled.\r\n", cap_first(light_label(light), capbuf, sizeof(capbuf)));
        descriptor_send(d, msg);
        return true;
    }
    if (light->val[3]) {
        descriptor_send(d, "You can't refuel it while it's lit -- it might explode!\r\n");
        return true;
    }
    if (light->val[2] >= light->val[1]) {
        descriptor_send(d, "It's already full.\r\n");
        return true;
    }

    int room_in_light = light->val[1] - light->val[2];
    int transfer = room_in_light < fuel->val[0] ? room_in_light : fuel->val[0];
    light->val[2] += transfer;
    fuel->val[0] -= transfer;

    char msg[256];
    snprintf(msg, sizeof(msg), "You refuel %s with %s.\r\n", light_label(light), light_label(fuel));
    descriptor_send(d, msg);
    if (ch->base.roomp) {
        snprintf(msg, sizeof(msg), "%s refuels %s.\r\n", ch->base.name, light_label(light));
        descriptor_room_echo(ch->base.roomp, ch, msg);
    }

    if (fuel->val[0] <= 0) {
        descriptor_send(d, "The fuel is used up.\r\n");
        obj_destroy(fuel);
    }
    return true;
}
