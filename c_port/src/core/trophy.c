/*******************************************************************
 * TobinMUD ver. 0.7 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#include "trophy.h"

#include "mob_repo.h"
#include "trophy_repo.h"

/* Exact port of TTrophy::getExpModVal() (cmd_trophy.cc) -- see trophy.h
 * for the constants' meaning. */
double trophy_exp_mod(int mob_vnum, double count) {
    const double min_mod = 0.3;
    const double max_mod = 1.0;
    const double free_kills = 8.0;
    const double step_mod = 0.5;
    const double num_steps = 14.0;

    mob_proto_t proto;
    if (mob_proto_load(mob_vnum, &proto) && proto.max_exist > 0)
        count /= proto.max_exist;

    double t1 = count - free_kills;
    double t2 = step_mod / num_steps;
    double t3 = t1 * t2;
    double t4 = max_mod - t3;
    double t5 = t4 > min_mod ? t4 : min_mod;
    if (t5 > max_mod)
        t5 = max_mod;
    return t5;
}

const char *trophy_exp_mod_descr(double mod) {
    if (mod >= 1.0)
        return "<y>full<z>";
    if (mod >= 0.90)
        return "<o>much<z>";
    if (mod >= 0.80)
        return "a fair amount of";
    if (mod >= 0.70)
        return "<w>some<z>";
    return "<k>little<z>";
}

void trophy_record_kill(being_t *winner, int mob_vnum) {
    if (!winner || winner->base.kind != THING_PC || being_is_immortal(winner))
        return;
    if (mob_vnum <= 0)
        return;
    trophy_repo_add_count(winner->player_id, mob_vnum, 1.0);
}

void trophy_pulse_tick(long pulse_num) {
    (void)pulse_num;
    trophy_repo_decay_all(0.25);
}
