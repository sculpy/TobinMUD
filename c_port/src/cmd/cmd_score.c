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

    char out[1024];
    int n = snprintf(out, sizeof(out),
             "\r\n-- %s --\r\n"
             "  Level:         %s\r\n"
             "  Experience:    %ld\r\n"
             "  HP:            %d/%d\r\n"
             "  Strength:      %d\r\n"
             "  Dexterity:     %d\r\n"
             "  Constitution:  %d\r\n"
             "  Intelligence:  %d\r\n"
             "  Wisdom:        %d\r\n"
             "  Charisma:      %d\r\n"
             "  Handedness:    %s\r\n",
             d->character->base.name,
             level_field,
             p->experience, p->hp, p->max_hp,
             a->strength, a->dexterity, a->constitution, a->intelligence, a->wisdom, a->charisma,
             d->character->handed_right ? "right" : "left");
    if (n < 0)
        n = 0;

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
