/*******************************************************************
 * TobinMUD ver. 1.0 - All rights reserved                         *
 * The TobinMUD Development Team                                   *
 *******************************************************************/
#ifndef TOBIN_DAMAGE_H
#define TOBIN_DAMAGE_H

/* Damage-type constants, ported verbatim from the original's
 * misc/damage.h (same names, same values) for future expansion --
 * nothing consumes these yet (Session 21, Tier 3). The negative values
 * are the original's convention: they flow through the same "damage
 * amount" parameters as ordinary positive damage, with negatives acting
 * as type codes for special death messages and effects. */
typedef enum {
    DAMAGE_NORMAL = -2,
    DAMAGE_CAVED_SKULL = -3,
    DAMAGE_BEHEADED = -4,
    DAMAGE_DISEMBOWLED_HR = -5,
    DAMAGE_STOMACH_WOUND = -6,
    DAMAGE_HACKED = -7,
    DAMAGE_IMPALE = -8,
    DAMAGE_STARVATION = -9,
    DAMAGE_FALL = -10,
    DAMAGE_HEMORRHAGE = -11,
    DAMAGE_DROWN = -12,
    DAMAGE_DRAIN = -13,
    DAMAGE_DISRUPTION = -14,
    DAMAGE_SUFFOCATION = -15,
    DAMAGE_RAMMED = -16,
    DAMAGE_WHIRLPOOL = -17,
    DAMAGE_ELECTRIC = -18,
    DAMAGE_ACID = -19,
    DAMAGE_GUST = -20,
    DAMAGE_EATTEN = -21, /* [sic] -- original's spelling, kept for grep parity */
    DAMAGE_KICK_HEAD = -22,
    DAMAGE_KICK_SOLAR = -23,
    DAMAGE_HEADBUTT_THROAT = -24,
    DAMAGE_HEADBUTT_BODY = -25,
    DAMAGE_HEADBUTT_CROTCH = -26,
    DAMAGE_HEADBUTT_LEG = -27,
    DAMAGE_HEADBUTT_FOOT = -28,
    DAMAGE_HEADBUTT_JAW = -29,
    DAMAGE_TRAP_SLEEP = -30,
    DAMAGE_TRAP_TELEPORT = -31,
    DAMAGE_TRAP_FIRE = -32,
    DAMAGE_TRAP_POISON = -33,
    DAMAGE_TRAP_ACID = -34,
    DAMAGE_TRAP_TNT = -35,
    DAMAGE_TRAP_ENERGY = -36,
    DAMAGE_TRAP_BLUNT = -37,
    DAMAGE_TRAP_PIERCE = -38,
    DAMAGE_TRAP_SLASH = -39,
    DAMAGE_TRAP_FROST = -40,
    DAMAGE_TRAP_DISEASE = -41,
    DAMAGE_ARROWS = -42,
    DAMAGE_FIRE = -43,
    DAMAGE_FROST = -44,
    DAMAGE_HEADBUTT_SKULL = -45,
    DAMAGE_COLLISION = -46,
    DAMAGE_KICK_SHIN = -47,
    DAMAGE_KNEESTRIKE_FOOT = -48,
    DAMAGE_KNEESTRIKE_SHIN = -49,
    DAMAGE_KNEESTRIKE_KNEE = -50,
    DAMAGE_KNEESTRIKE_THIGH = -51,
    DAMAGE_KNEESTRIKE_CROTCH = -52,
    DAMAGE_KNEESTRIKE_SOLAR = -53,
    DAMAGE_KNEESTRIKE_CHIN = -54,
    DAMAGE_KNEESTRIKE_FACE = -55,
    DAMAGE_KICK_SIDE = -56,
    DAMAGE_DISEMBOWLED_VR = -57,
    DAMAGE_RIPPED_OUT_HEART = -58,
    DAMAGE_HOLY = -59
} damage_type_t;

#endif
