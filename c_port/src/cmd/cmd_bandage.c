/*******************************************************************
 * TobinMUD ver. 0.5 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "cmd_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "being.h"
#include "descriptor.h"
#include "obj.h"
#include "skill.h"
#include "thing.h"

/* Real upstream raw obj type for a bandage item (obj.c's own
 * OBJ_CAT_TOOL comment table: "38 ITEM_BANDAGE") -- no symbolic constant
 * exists yet since nothing referenced this type by number before now,
 * same "give it a name the first time code actually needs one" pattern
 * OBJ_PLANT_RAW_TYPE (obj_plant.h) already set. */
#define OBJ_BANDAGE_RAW_TYPE 38

/* `bandage [target]` (docs/Spell Assignments.xlsx gap audit, batch B,
 * 2026-08-08 -- "Implement missing skills from docs/Spell Assignments.xlsx").
 * Real upstream (cmd_egotrip.cc's cousin, disc_basic_adventuring.cc's
 * doBandage()) auto-finds a bleeding limb (PART_BLEEDING) on the target,
 * a mechanic Tobin never had until this session -- limb_state_t gained a
 * transient `bleeding` flag (being.h), set the moment a limb crosses into
 * limb_status_text()'s bad tier (combat.c, same tier-crossing guard the
 * blood-pool spawn already uses) and chipped by vitals_tick_impl()
 * (vitals.c) every ~60s until treated. This command is the treatment:
 * requires a carried bandage item (OBJ_BANDAGE_RAW_TYPE) and the
 * `bandage` skill; on a successful roll, clears the bleeding flag, heals
 * a small fixed amount, and consumes the bandage. On failure the
 * bandage is NOT consumed (deliberate scope-down from the real
 * doBandage(), which always consumes -- kept simple/less punishing
 * since there's no multi-bandage-combining mechanic here to soften a
 * failed attempt the way the original's `band_num` count does).
 * PC-only target, matching the real doBandage()'s own explicit
 * "Bandage non-players has been temporarily disabled" restriction --
 * Tobin has no equivalent "someday" plan for mobs here either, so this
 * just keeps that restriction rather than half-porting a mob path. No
 * specific-limb argument (the original's optional second word) --
 * auto-find only, a real scope-down kept small on purpose. */
bool cmd_bandage(descriptor_t *d, const char *args) {
    being_t *ch = d->character;
    if (!ch)
        return true;

    if (!being_is_immortal(ch) && !being_knows_skill(ch, "bandage")) {
        descriptor_send(d, "You don't know how to bandage wounds.\r\n");
        return true;
    }

    char tok[64] = "";
    sscanf(args, "%63s", tok);

    being_t *target = ch;
    if (tok[0]) {
        being_t *found = NULL;
        if (ch->base.roomp) {
            size_t len = strlen(tok);
            for (thing_t *t = ch->base.roomp->base.stuff_head; t; t = t->stuff_next) {
                if (t->kind == THING_PC && thing_name_matches(t->name, tok, len)) {
                    found = (being_t *)t;
                    break;
                }
            }
        }
        if (!found) {
            descriptor_send(d, "Bandage non-players is not supported. Nobody by that name is here.\r\n");
            return true;
        }
        target = found;
    }

    int bleeding_limb = -1;
    for (int i = 0; i < LIMB_COUNT; i++) {
        if (target->limbs[i].bleeding) {
            bleeding_limb = i;
            break;
        }
    }
    if (bleeding_limb < 0) {
        descriptor_send(d, target == ch ? "You have no bleeding wounds.\r\n" : "They have no bleeding wounds.\r\n");
        return true;
    }

    obj_t *bandage_obj = NULL;
    for (thing_t *t = ch->base.stuff_head; t; t = t->stuff_next) {
        if (t->kind == THING_OBJ && ((obj_t *)t)->raw_type == OBJ_BANDAGE_RAW_TYPE) {
            bandage_obj = (obj_t *)t;
            break;
        }
    }
    if (!bandage_obj) {
        descriptor_send(d, "You don't have a bandage to use.\r\n");
        return true;
    }

    const skill_def_t *sk = skill_find(ch->char_class, "bandage", false);
    bool success = being_is_immortal(ch) || (sk && skill_roll_success(skill_learn_from_doing(ch, sk)));

    if (!success) {
        descriptor_send(d, "You fumble with the bandage and fail to stop the bleeding.\r\n");
        if (target != ch && target->desc) {
            char fail_msg[160];
            snprintf(fail_msg, sizeof(fail_msg), "%s fumbles with a bandage, failing to help you.\r\n",
                     being_display_name(ch));
            descriptor_notify(target->desc, fail_msg);
        }
        return true;
    }

    target->limbs[bleeding_limb].bleeding = false;
    int heal = 5 + rand() % 6;
    target->progress.hp += heal;
    if (target->progress.hp > target->progress.max_hp)
        target->progress.hp = target->progress.max_hp;

    obj_destroy(bandage_obj);

    char msg[200];
    snprintf(msg, sizeof(msg), "You bandage %s %s, stopping the bleeding.\r\n",
             target == ch ? "your" : "their", limb_name((limb_t)bleeding_limb));
    descriptor_send(d, msg);
    if (target != ch && target->desc) {
        snprintf(msg, sizeof(msg), "%s bandages your %s, stopping the bleeding.\r\n",
                 being_display_name(ch), limb_name((limb_t)bleeding_limb));
        descriptor_notify(target->desc, msg);
    }
    return true;
}
