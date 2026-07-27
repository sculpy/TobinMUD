/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "socials.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "descriptor.h"
#include "obj.h"
#include "room.h"
#include "thing.h"

/* In-memory cache -- see socials.h's header comment for why social_try()
 * can't hit the DB per call. Headroom above the ~155 real rows so a
 * builder growing the set in-game (edsocial) doesn't need a code change. */
#define SOCIAL_CACHE_MAX 256
static social_t g_cache[SOCIAL_CACHE_MAX];
static int g_cache_count = 0;

/* Loads the full social table from the DB into g_cache at boot (or on
 * reload) so social_try()'s per-command lookups never hit the DB. */
void social_cache_load(void) {
    g_cache_count = social_repo_load_all(g_cache, SOCIAL_CACHE_MAX);
}

/* Number of socials currently loaded in the cache. */
int social_cache_count(void) {
    return g_cache_count;
}

/* Cached social by cache index (not by name/verb), or NULL if out of range
 * -- used by callers that iterate the whole set, e.g. an admin listing. */
const social_t *social_cache_at(int index) {
    if (index < 0 || index >= g_cache_count)
        return NULL;
    return &g_cache[index];
}

/* Case-insensitive prefix match against the cache, same abbreviation rule
 * as the command table -- first (alphabetical) match wins. */
static const social_t *social_find(const char *verb) {
    size_t vlen = strlen(verb);
    if (vlen == 0)
        return NULL;
    for (int i = 0; i < g_cache_count; i++)
        if (strncasecmp(g_cache[i].name, verb, vlen) == 0)
            return &g_cache[i];
    return NULL;
}

/* Writes a comma-separated list of every social's verb into `out` -- backs
 * the `socials` command's full listing. */
void social_names(char *out, size_t size) {
    size_t n = 0;
    out[0] = '\0';
    for (int i = 0; i < g_cache_count; i++)
        n += (size_t)snprintf(out + n, size > n ? size - n : 0, "%s%s",
                              i ? ", " : "", g_cache[i].name);
}

/* Finds a PLAYING character in `ch`'s room whose name matches `arg` by
 * case-insensitive prefix -- INCLUDES `ch` itself, since social_try()'s
 * self-target branch (tgt == ch) depends on self-targeting resolving to a
 * hit here rather than falling through to not_found. */
static being_t *find_room_pc(being_t *ch, const char *arg) {
    room_t *r = ch->base.roomp;
    if (!r)
        return NULL;
    size_t len = strlen(arg);
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC)
            continue;
        being_t *other = (being_t *)t;
        if (other->desc && strncasecmp(other->base.name, arg, len) == 0)
            return other;
    }
    return NULL;
}

/* `point`'s held-item form (Tobin-original, TOBIN_EXTRAS in
 * db/import-socials.py) -- "X points at you with his/her/its <primary-hand
 * item>". NULL if empty-handed, in which case `point` falls back to its
 * plain DB-templated wording exactly like every other social. Strips a
 * leading "a "/"an "/"the " -- short_descr already carries its own
 * article, and the message templates below supply their own
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

/* Expands the upstream $-token grammar ($n/$N/$P/$s/$S/$e/$E/$m/$M --
 * verified against the real loader/act(), see db/import-socials.py's
 * header comment) in `tmpl` for `actor` (and `target`, NULL for a
 * no-target form) into `out`. $n/$N/$P are capitalized only when they're
 * the very first character of the template (matches the original's
 * position-1 rule, sys/comm.cc's act() -- every ported message starts
 * with either "You " or "$n ", so this only ever fires there). Tobin has
 * no invisibility system yet, so unlike the original there's no "someone"/
 * "it" fallback for an unseen actor/target -- always the real name/
 * pronoun (documented gap, revisit if invisibility lands). A target-
 * pronoun/name token with no `target` (shouldn't happen in well-formed
 * templates, since no-target forms don't reference $N/$M/etc, but real
 * upstream data occasionally does something unexpected) falls back to a
 * neutral "them"/"their"/"they" rather than crashing. */
static void social_expand(const char *tmpl, const being_t *actor, const being_t *target,
                          char *out, size_t outsz) {
    size_t o = 0;
    bool first = true;
    for (const char *p = tmpl; *p && o + 1 < outsz;) {
        if (*p == '$' && p[1] != '\0') {
            const char *sub = NULL;
            bool nameish = false;
            switch (p[1]) {
                case 'n': sub = actor->base.name; nameish = true; break;
                case 'N': case 'P': sub = target ? target->base.name : "them"; nameish = true; break;
                case 's': sub = gender_possess(actor->gender); break;
                case 'S': sub = target ? gender_possess(target->gender) : "their"; break;
                case 'e': sub = gender_subject(actor->gender); break;
                case 'E': sub = target ? gender_subject(target->gender) : "they"; break;
                case 'm': sub = gender_object(actor->gender); break;
                case 'M': sub = target ? gender_object(target->gender) : "them"; break;
                default: break;
            }
            if (sub) {
                bool cap = first && nameish;
                for (const char *s = sub; *s && o + 1 < outsz; s++)
                    out[o++] = (cap && s == sub) ? (char)toupper((unsigned char)*s) : *s;
                p += 2;
                first = false;
                continue;
            }
        }
        out[o++] = *p++;
        first = false;
    }
    out[o < outsz ? o : outsz - 1] = '\0';
}

/* Expands `tmpl` and sends it to `d`'s own screen, appending \r\n. A blank
 * `tmpl` (a handful of real upstream entries have gaps -- e.g. comfort's
 * others_no_arg -- see the importer's header comment) sends nothing,
 * rather than replaying what's likely an accidental empty line upstream. */
static void social_send(descriptor_t *d, const char *tmpl, const being_t *actor, const being_t *target) {
    if (!tmpl[0])
        return;
    char buf[384];
    social_expand(tmpl, actor, target, buf, sizeof(buf) - 2);
    size_t len = strlen(buf);
    buf[len] = '\r'; buf[len + 1] = '\n'; buf[len + 2] = '\0';
    descriptor_send(d, buf);
}

/* Same, but via descriptor_notify (dropped, not held, if the recipient
 * is mid-editor -- a social/emote is ambient RP flavor, not real
 * communication) -- for anyone who isn't the acting player themselves. */
static void social_notify(descriptor_t *d, const char *tmpl, const being_t *actor, const being_t *target) {
    if (!tmpl[0])
        return;
    char buf[384];
    social_expand(tmpl, actor, target, buf, sizeof(buf) - 2);
    size_t len = strlen(buf);
    buf[len] = '\r'; buf[len + 1] = '\n'; buf[len + 2] = '\0';
    descriptor_notify(d, buf);
}

static void social_room_echo(room_t *r, being_t *exclude, const char *tmpl,
                             const being_t *actor, const being_t *target) {
    if (!tmpl[0])
        return;
    char buf[384];
    social_expand(tmpl, actor, target, buf, sizeof(buf) - 2);
    size_t len = strlen(buf);
    buf[len] = '\r'; buf[len + 1] = '\n'; buf[len + 2] = '\0';
    descriptor_room_echo(r, exclude, buf);
}

/* Plays a social's no-target room message on behalf of `actor` without
 * going through a descriptor/command line -- e.g. for mob AI or scripted
 * triggers that want a being to "emote" a stock social. Only echoes to the
 * room (no self message, since there may be no descriptor to send it to).
 * Returns false if the verb isn't a known social or the actor can't act
 * right now. */
bool social_perform_for(being_t *actor, const char *verb) {
    const social_t *soc = social_find(verb);
    if (!soc || !actor || !actor->base.roomp)
        return false;
    if ((int)actor->position < soc->min_position)
        return false;

    social_room_echo(actor->base.roomp, NULL, soc->others_no_arg, actor, NULL);
    return true;
}

/* Main entry point for the social command dispatcher: resolves `verb`
 * against the cache and, if found, plays out its no-arg, self/other-target,
 * or (for the "point" special case) held-item message forms to the actor,
 * target, and room. Returns false if `verb` isn't a recognized social at
 * all (so the caller can fall through to "huh?"); returns true for every
 * other outcome, including position-blocked or target-not-found, since
 * those cases already sent their own message. */
bool social_try(descriptor_t *d, const char *verb, const char *args) {
    const social_t *soc = social_find(verb);
    if (!soc)
        return false;

    being_t *ch = d->character;
    if (!ch || !ch->base.roomp) {
        descriptor_send(d, "You can't do that here.\r\n");
        return true;
    }
    if ((int)ch->position < soc->min_position) {
        /* Generic wording -- unlike cmd_move.c's movement-specific gate, a
         * social's min_position can be almost any threshold (RESTING,
         * FIGHTING, STANDING, ...), so no single "try standing up" phrase
         * fits them all. */
        descriptor_send(d, "You can't do that right now.\r\n");
        return true;
    }

    /* "point"'s held-item form is Tobin-original (not upstream) and
     * involves a THIRD entity -- the item -- outside the actor/target
     * $-token grammar, so it's handled as a full bypass of the generic
     * templates, same architecture the pre-DB-port code already used. */
    bool is_point = strcmp(soc->name, "point") == 0;
    const char *point_item = is_point ? point_item_label(ch) : NULL;
    char buf[384];

    if (!args || !args[0]) {
        if (point_item) {
            snprintf(buf, sizeof(buf), "You point around with your %s.\r\n", point_item);
            descriptor_send(d, buf);
            snprintf(buf, sizeof(buf), "%s points around with %s %s.\r\n",
                     ch->base.name, gender_possess(ch->gender), point_item);
            descriptor_room_echo(ch->base.roomp, ch, buf);
            return true;
        }
        social_send(d, soc->self_no_arg, ch, NULL);
        social_room_echo(ch->base.roomp, ch, soc->others_no_arg, ch, NULL);
        return true;
    }

    /* Targeted form. */
    char tok[64];
    sscanf(args, "%63s", tok);
    being_t *tgt = find_room_pc(ch, tok);
    if (!tgt) {
        social_send(d, soc->not_found, ch, NULL);
        return true;
    }
    if (tgt == ch) {
        social_send(d, soc->self_auto, ch, ch);
        social_room_echo(ch->base.roomp, ch, soc->others_auto, ch, ch);
        return true;
    }

    if (point_item) {
        snprintf(buf, sizeof(buf), "You point at %s with your %s.\r\n", tgt->base.name, point_item);
        descriptor_send(d, buf);
        if (tgt->desc) {
            snprintf(buf, sizeof(buf), "%s points at you with %s %s.\r\n",
                     ch->base.name, gender_possess(ch->gender), point_item);
            descriptor_notify(tgt->desc, buf);
        }
        room_t *r = ch->base.roomp;
        for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
            if (t->kind != THING_PC || t == &ch->base || t == &tgt->base)
                continue;
            being_t *o = (being_t *)t;
            if (o->desc) {
                snprintf(buf, sizeof(buf), "%s points at %s with %s %s.\r\n",
                         ch->base.name, tgt->base.name, gender_possess(ch->gender), point_item);
                descriptor_notify(o->desc, buf);
            }
        }
        return true;
    }

    social_send(d, soc->self_found, ch, tgt);
    if (tgt->desc)
        social_notify(tgt->desc, soc->vict_found, ch, tgt);
    room_t *r = ch->base.roomp;
    for (thing_t *t = r->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind != THING_PC || t == &ch->base || t == &tgt->base)
            continue;
        being_t *o = (being_t *)t;
        if (o->desc)
            social_notify(o->desc, soc->others_found, ch, tgt);
    }
    return true;
}
