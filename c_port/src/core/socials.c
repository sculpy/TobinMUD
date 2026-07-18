/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "socials.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "descriptor.h"
#include "obj.h"
#include "room.h"
#include "thing.h"

/* One social's messages. The %s slots are filled, in order:
 *   self          -- (none)
 *   others        -- actor name
 *   self_targ     -- target name
 *   targ          -- actor name
 *   others_targ   -- actor name, then target name
 * A NULL targeted-form set means the social can't be aimed at anyone. */
typedef struct {
    const char *name;
    const char *self;
    const char *others;
    const char *self_targ;
    const char *targ;
    const char *others_targ;
} social_t;

static const social_t SOCIALS[] = {
    {"smile", "You smile.\r\n", "%s smiles.\r\n",
     "You smile at %s.\r\n", "%s smiles at you.\r\n", "%s smiles at %s.\r\n"},
    {"grin", "You grin evilly.\r\n", "%s grins evilly.\r\n",
     "You grin at %s.\r\n", "%s grins at you.\r\n", "%s grins at %s.\r\n"},
    {"laugh", "You laugh out loud.\r\n", "%s laughs out loud.\r\n",
     "You laugh at %s.\r\n", "%s laughs at you.\r\n", "%s laughs at %s.\r\n"},
    {"nod", "You nod.\r\n", "%s nods.\r\n",
     "You nod at %s.\r\n", "%s nods at you.\r\n", "%s nods at %s.\r\n"},
    /* "shake"/"poke"/"comfort" (Session 43, standing gender-pronoun habit):
     * the bare "their"/"they"/"themselves" text below (here and at "poke"
     * and "comfort" further down) is a fallback for `social_try()`'s shared
     * %s-name-only formatting -- each is overridden with a real gendered
     * pronoun there before display, same convention as descriptor.c's
     * link-loss line and cmd_position.c's `stand`. Left here (rather than
     * deleted) so social_names()/this table stay the single source of
     * truth for every social's text shape. */
    {"shake", "You shake your head.\r\n", "%s shakes their head.\r\n",
     "You shake your head at %s.\r\n", "%s shakes their head at you.\r\n",
     "%s shakes their head at %s.\r\n"},
    {"wave", "You wave.\r\n", "%s waves.\r\n",
     "You wave at %s.\r\n", "%s waves at you.\r\n", "%s waves at %s.\r\n"},
    {"bow", "You bow deeply.\r\n", "%s bows deeply.\r\n",
     "You bow before %s.\r\n", "%s bows before you.\r\n", "%s bows before %s.\r\n"},
    {"wink", "You wink.\r\n", "%s winks.\r\n",
     "You wink at %s.\r\n", "%s winks at you.\r\n", "%s winks at %s.\r\n"},
    {"grovel", "You grovel in the dirt.\r\n", "%s grovels in the dirt.\r\n",
     "You grovel before %s.\r\n", "%s grovels before you.\r\n",
     "%s grovels before %s.\r\n"},
    {"shrug", "You shrug.\r\n", "%s shrugs.\r\n",
     "You shrug at %s.\r\n", "%s shrugs at you.\r\n", "%s shrugs at %s.\r\n"},
    {"cheer", "You cheer wildly!\r\n", "%s cheers wildly!\r\n",
     "You cheer for %s!\r\n", "%s cheers for you!\r\n", "%s cheers for %s!\r\n"},
    {"cackle", "You cackle gleefully.\r\n", "%s cackles gleefully.\r\n",
     "You cackle at %s.\r\n", "%s cackles at you.\r\n", "%s cackles at %s.\r\n"},
    {"poke", "You poke yourself. Ow.\r\n", "%s pokes themselves.\r\n", /* "others" overridden, see "shake" comment above */
     "You poke %s in the ribs.\r\n", "%s pokes you in the ribs.\r\n",
     "%s pokes %s in the ribs.\r\n"},
    {"comfort", "You need some comforting yourself.\r\n", "%s looks like they need comforting.\r\n",
     "You comfort %s.\r\n", "%s comforts you.\r\n", "%s comforts %s.\r\n"}, /* "others" overridden, see "shake" comment above */
    {"thank", "You thank everyone.\r\n", "%s thanks everyone.\r\n",
     "You thank %s heartily.\r\n", "%s thanks you heartily.\r\n",
     "%s thanks %s heartily.\r\n"},
    /* With no target, point around randomly (user spec). The held-item form
     * ("...points at you with his <item>") lands with the object system. */
    {"point", "You point around randomly.\r\n", "%s points around randomly.\r\n",
     "You point at %s.\r\n", "%s points at you.\r\n", "%s points at %s.\r\n"},
};
#define NUM_SOCIALS (sizeof(SOCIALS) / sizeof(SOCIALS[0]))

void social_names(char *out, size_t size) {
    size_t n = 0;
    out[0] = '\0';
    for (size_t i = 0; i < NUM_SOCIALS; i++)
        n += (size_t)snprintf(out + n, size > n ? size - n : 0, "%s%s",
                              i ? ", " : "", SOCIALS[i].name);
}

/* Finds a PLAYING character in `ch`'s room whose name matches `arg` by
 * case-insensitive prefix (excludes `ch`). */
static being_t *find_room_pc(being_t *ch, const char *arg) {
    room_t *r = ch->base.roomp;
    if (!r)
        return NULL;
    size_t len = strlen(arg);
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC || t == &ch->base)
            continue;
        being_t *other = (being_t *)t;
        if (other->desc && strncasecmp(other->base.name, arg, len) == 0)
            return other;
    }
    return NULL;
}

/* `point`'s held-item form (TODO.md: "the original's item-referencing
 * form -- 'X points at you with his/her/its <primary-hand item>' --
 * doesn't [exist]"). NULL if empty-handed, in which case `point` falls
 * back to its plain random-point wording exactly as before. Strips a
 * leading "a "/"an "/"the " -- short_descr already carries its own
 * article ("a torch"), and the message templates below supply their own
 * "your"/"his"/"her"/"its", so leaving both in would read "with your a
 * torch" (just advances past the article in-place, no copy needed). */
static const char *point_item_label(const being_t *ch) {
    if (!ch->held[0])
        return NULL;
    const char *label = ch->held[0]->base.short_descr[0]
                             ? ch->held[0]->base.short_descr
                             : ch->held[0]->base.name;
    /* Skip a leading inline color tag ("<o>a torch<1>", real seeded
     * content) before checking for the article -- same tag shape
     * cap_first() (cmd_look.c/cmd_object.c) already skips. */
    while (label[0] == '<' && label[1] != '\0' && label[2] == '>')
        label += 3;
    if (strncasecmp(label, "a ", 2) == 0)
        return label + 2;
    if (strncasecmp(label, "an ", 3) == 0)
        return label + 3;
    if (strncasecmp(label, "the ", 4) == 0)
        return label + 4;
    return label;
}

bool social_try(descriptor_t *d, const char *verb, const char *args) {
    /* Abbreviation matching, same rule as the command table (cmd_table.c):
     * any non-empty prefix resolves to the FIRST social it matches, so
     * "poi"/"poin" reach "point". Commands are tried before socials in
     * cmd_dispatch, so a real command still always wins. */
    const social_t *soc = NULL;
    size_t vlen = strlen(verb);
    for (size_t i = 0; i < NUM_SOCIALS; i++) {
        if (vlen > 0 && strncasecmp(SOCIALS[i].name, verb, vlen) == 0) {
            soc = &SOCIALS[i];
            break;
        }
    }
    if (!soc)
        return false;

    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You can't do that here.\r\n");
        return true;
    }

    char buf[256];
    const char *point_item = strcmp(soc->name, "point") == 0 ? point_item_label(ch) : NULL;

    if (!args || !args[0]) {
        if (point_item) {
            snprintf(buf, sizeof(buf), "You point around with your %s.\r\n", point_item);
            descriptor_send(d, buf);
        } else {
            descriptor_send(d, soc->self);
        }
        if (strcmp(soc->name, "shake") == 0)
            snprintf(buf, sizeof(buf), "%s shakes %s head.\r\n", ch->base.name, gender_possess(ch->gender));
        else if (strcmp(soc->name, "comfort") == 0)
            snprintf(buf, sizeof(buf), "%s looks like %s could use some comforting.\r\n",
                     ch->base.name, gender_subject(ch->gender));
        else if (strcmp(soc->name, "poke") == 0)
            snprintf(buf, sizeof(buf), "%s pokes %s.\r\n", ch->base.name, gender_reflexive(ch->gender));
        else if (point_item)
            snprintf(buf, sizeof(buf), "%s points around with %s %s.\r\n",
                     ch->base.name, gender_possess(ch->gender), point_item);
        else
            snprintf(buf, sizeof(buf), soc->others, ch->base.name);
        descriptor_room_echo(ch->base.roomp, ch, buf);
        return true;
    }

    /* Targeted form. */
    char tok[64];
    sscanf(args, "%63s", tok);
    being_t *tgt = find_room_pc(ch, tok);
    if (!tgt) {
        descriptor_send(d, "They aren't here.\r\n");
        return true;
    }
    if (tgt == ch) {
        descriptor_send(d, soc->self);
        return true;
    }

    if (point_item)
        snprintf(buf, sizeof(buf), "You point at %s with your %s.\r\n", tgt->base.name, point_item);
    else
        snprintf(buf, sizeof(buf), soc->self_targ, tgt->base.name);
    descriptor_send(d, buf);
    if (tgt->desc) {
        if (strcmp(soc->name, "shake") == 0)
            snprintf(buf, sizeof(buf), "%s shakes %s head at you.\r\n", ch->base.name, gender_possess(ch->gender));
        else if (point_item)
            snprintf(buf, sizeof(buf), "%s points at you with %s %s.\r\n",
                     ch->base.name, gender_possess(ch->gender), point_item);
        else
            snprintf(buf, sizeof(buf), soc->targ, ch->base.name);
        descriptor_notify(tgt->desc, buf); /* held if the target is editing */
    }
    /* Everyone else in the room. */
    room_t *r = ch->base.roomp;
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC || t == &ch->base || t == &tgt->base)
            continue;
        being_t *o = (being_t *)t;
        if (o->desc) {
            if (strcmp(soc->name, "shake") == 0)
                snprintf(buf, sizeof(buf), "%s shakes %s head at %s.\r\n",
                         ch->base.name, gender_possess(ch->gender), tgt->base.name);
            else if (point_item)
                snprintf(buf, sizeof(buf), "%s points at %s with %s %s.\r\n",
                         ch->base.name, tgt->base.name, gender_possess(ch->gender), point_item);
            else
                snprintf(buf, sizeof(buf), soc->others_targ, ch->base.name, tgt->base.name);
            descriptor_notify(o->desc, buf);
        }
    }
    return true;
}
