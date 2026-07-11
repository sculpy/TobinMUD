/*******************************************************************
 * TobinMUD ver. 0.1 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>

bool cmd_score(descriptor_t *d, const char *args) {
    (void)args;

    if (!d->character) {
        descriptor_send(d, "You are nowhere.\r\n");
        return true;
    }
    const attrs_t *a = &d->character->attrs;
    const progress_t *p = &d->character->progress;

    char level_field[24];
    const char *title = being_level_title(p->level);
    if (title)
        snprintf(level_field, sizeof(level_field), "%s", title);
    else
        snprintf(level_field, sizeof(level_field), "%d", p->level);

    const char *pos = d->character->fighting ? "Fighting"
                                             : position_name(d->character->position);
    /* Tint the Level/rank field by immortal rank tier (mortals: no color) --
     * the name itself stays uncolored (user spec). */
    const char *col = being_rank_color(p->level);
    const char *reset = col[0] ? "<z>" : "";

    char out[1536];
    int n = snprintf(out, sizeof(out),
             "  Name:          %s\t  Level:         %s%s%s\r\n"
             "  Experience:    %ld\r\n"
             "  HP:            %d/%d (%s)\r\n"
             "  Position:      %s\r\n"
             "  Characteristics:\r\n"
			 "  Strength:      %d\t  Intelligence:  %d\r\n"
             "  Dexterity:     %d\t  Wisdom:        %d\r\n"
             "  Constitution:  %d\t  Charisma:      %d\r\n"
             "  You are %s handed.  Gender: %s\r\n"
             "  Alignment:     %s\r\n",
             d->character->base.name,
			 col, level_field, reset,
             p->experience, p->hp, p->max_hp, being_health_word(d->character), pos,
             a->strength, a->intelligence, a->dexterity, a->wisdom, a->constitution, a->charisma,
             d->character->handed_right ? "right" : "left",
             gender_name(d->character->gender),
             alignment_word(p->alignment));
    if (n < 0)
        n = 0;

    /* Appearance, if the player set one at creation. */
    if (d->character->appearance[0] && (size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n,
                      "  Appearance:    %s\r\n", d->character->appearance);

    /* A limb only shows up here at all once it's hurt (< 20% health, per
     * limb_status_text()) -- a fully healthy character has no Limbs
     * section. Same wording combat announces mid-fight, so a player who
     * reads about an injury there sees the identical phrase here. */
    char injuries[512];
    int inj_n = 0;
    for (int i = 0; i < LIMB_COUNT && (size_t)inj_n < sizeof(injuries); i++) {
        int pct = being_limb_pct(d->character, (limb_t)i);
        const char *status = limb_status_text(pct);
        if (status)
            inj_n += snprintf(injuries + inj_n, sizeof(injuries) - (size_t)inj_n,
                              "  Your %s %s! (%d%%)\r\n", limb_name((limb_t)i), status, pct);
    }

    if (inj_n > 0 && (size_t)n < sizeof(out))
        n += snprintf(out + n, sizeof(out) - (size_t)n, "  Limbs:\r\n%s", injuries);

    descriptor_send(d, out);
    return true;
}
