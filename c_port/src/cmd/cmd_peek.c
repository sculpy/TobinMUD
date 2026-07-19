/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "being.h"
#include "obj.h"
#include "room.h"
#include "skill.h"
#include "thing.h"

/* short_descr/name are stored lowercase-first by convention; capitalize
 * only when starting a whole message. Same duplicated-per-file helper as
 * cmd_look.c/cmd_object.c's own copies (color-tag-skipping). */
static const char *cap_first(const char *label, char *buf, size_t bufsz) {
    snprintf(buf, bufsz, "%s", label);
    size_t i = 0;
    while (buf[i] == '<' && buf[i + 1] != '\0' && buf[i + 2] == '>')
        i += 3;
    if (buf[i])
        buf[i] = (char)toupper((unsigned char)buf[i]);
    return buf;
}

/* "Loose" = carried but not worn or held (same definition cmd_object.c's
 * `inventory` command uses) -- duplicated here rather than shared since
 * it's file-local there too. */
static bool is_loose(const being_t *b, const obj_t *o) {
    if (b->held[0] == o || b->held[1] == o)
        return false;
    for (int i = 0; i < LIMB_COUNT; i++)
        if (b->equipment[i] == o)
            return false;
    return true;
}

/* `peek <target>` (Thief skill, TODO.md "Thief 'peek' skill" -- user: "a
 * thief skill could be added to attempt a peak at the targets inventory").
 * Distinct from `look <target>`'s worn-equipment display (Session 43+): this
 * tries to see what someone is CARRYING (loose inventory, not worn/held), a
 * new skill gated by `being_knows_skill()` and rolled the same way trap
 * mechanics are (cmd_trap.c) -- immortals always succeed, a fumble is
 * presumably detectable (the target gets an on-guard notice) rather than
 * silently failing, same "attempt made, effect not guaranteed" spirit as
 * everything else in the per-skill proficiency system. A clean success
 * stays undetected. */
bool cmd_peek(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    if (!being_knows_skill(ch, "peek")) {
        descriptor_send(d, "Command not found, maybe submit an idea if you believe TobinMUD should have it.\r\n");
        return true;
    }

    char raw[64];
    if (sscanf(args, "%63s", raw) != 1) {
        descriptor_send(d, "Usage: peek <target>\r\n");
        return true;
    }
    const char *tok;
    int ordinal = thing_parse_ordinal(raw, &tok);
    size_t len = strlen(tok);

    being_t *tgt = NULL;
    int seen = 0;
    for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC && t->kind != THING_MOB)
            continue;
        if (t == &ch->base)
            continue;
        if (thing_name_matches(t->name, tok, len)) {
            seen++;
            if (seen == ordinal) {
                tgt = (being_t *)t;
                break;
            }
        }
    }
    if (!tgt) {
        descriptor_send(d, "You don't see them here.\r\n");
        return true;
    }

    bool imm = being_is_immortal(ch);
    const skill_def_t *sk = skill_find(ch->char_class, "peek", imm);
    bool success = imm || !sk || skill_roll_success(skill_learn_from_doing(ch, sk));

    if (!success) {
        descriptor_send(d, "You fail to get a good look at what they're carrying.\r\n");
        if (tgt->desc)
            descriptor_notify(tgt->desc, "You have a feeling someone just tried to peek at what you're carrying.\r\n");
        return true;
    }

    const char *display = tgt->base.name;
    if (tgt->base.kind == THING_MOB && tgt->base.short_descr[0])
        display = tgt->base.short_descr;

    char out[2048];
    char capbuf[128];
    int n = snprintf(out, sizeof(out), "You covertly peek at what %s is carrying:\r\n",
                      display);
    bool any = false;
    for (thing_t *t = tgt->base.stuff_head; t && (size_t)n < sizeof(out); t = t->stuff_next) {
        if (t->kind != THING_OBJ)
            continue;
        obj_t *o = (obj_t *)t;
        if (!is_loose(tgt, o))
            continue;
        any = true;
        const char *label = o->base.short_descr[0] ? o->base.short_descr : o->base.name;
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  %s\r\n",
                      cap_first(label, capbuf, sizeof(capbuf)));
    }
    if (!any && (size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  Nothing.\r\n");

    descriptor_page_start(d, out, 0);
    return true;
}
