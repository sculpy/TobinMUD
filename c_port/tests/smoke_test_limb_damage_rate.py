#!/usr/bin/env python3
"""Smoke test for the blood/limb-damage generation rate reduction
(TODO.md priority item, user 2026-07-30: "Reduce blood and limb-damage
generation rates by 50%"). Verifies that a limb's implied HP loss over a
real combat exchange is roughly HALF of the defender's overall HP loss
over that same exchange -- not a 1:1 ratio like before.

Aggregates over several real combat rounds (not a single hit) to smooth
out RNG noise (which limb gets hit, roll variance), then compares total
implied limb HP lost (percentage delta * each limb's own max_hp, itself
a deterministic share of overall max_hp -- being.c's own
being_limbs_full_heal()) against total overall HP lost, expecting a
ratio near 0.5 with a generous tolerance band.

    python3 tests/smoke_test_limb_damage_rate.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

LIMB_NAMES = [
    "head", "neck", "back", "left arm", "right arm", "left wrist", "right wrist",
    "left hand", "right hand", "left finger", "right finger",
    "body", "waist", "genitalia", "right leg", "left leg", "left foot", "right foot",
]
LIMB_MIN_MAX_HP = 15


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, ""); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, "1"); recv_all(s)  # homeland: urban (territory, forced step since 2026-08-03)
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    return s


def make_single(prefix, level=None):
    name, pw = f"{prefix}{_suffix}", f"{prefix}pw12345"
    s0 = make_char(name, pw)
    cmd(s0, "quit!"); s0.close()
    if level is not None:
        sql(f"UPDATE player_progress SET level={level} WHERE player_id="
            f"(SELECT id FROM player WHERE name='{name}');")
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s, 1.0)
    cmd(s, "color off")
    cmd(s, "toggle pk")
    return name, s


def read_hp(sock):
    out = cmd(sock, "score", 1.0)
    m = re.search(r"HP:\s*(-?\d+)\s*\((\d+) Max\.\)", out)
    return int(m.group(1)), int(m.group(2))


def read_limb_pcts(sock):
    # limbs shows a health WORD first now (user, 2026-08-03: "limbs command,
    # list health words not %"), with the exact percentage kept alongside
    # in parentheses -- e.g. "head          perfect     (100%)" -- this
    # test needs the real number for its damage-ratio math, not just which
    # of 10 coarse word tiers a limb falls in.
    out = cmd(sock, "limbs", 1.0)
    pcts = {}
    for name in LIMB_NAMES:
        m = re.search(re.escape(name) + r"\s+\S+\s+\(\s*(\d+)%\)", out)
        if m:
            pcts[name] = int(m.group(1))
    return pcts


print("=== Limb-Damage/Blood Generation Rate Test ===\n")

nameA, sA = make_single("Ratea", level=20)
nameB, sB = make_single("Rateb", level=20)

hp_before, max_hp = read_hp(sB)
limb_max_hp = max(LIMB_MIN_MAX_HP, max_hp // len(LIMB_NAMES))
pcts_before = read_limb_pcts(sB)

send_line(sA, f"attack {nameB}")
recv_all(sA, 0.5)
recv_all(sB, 0.3)

# 12 rounds (~14.5s of real combat) -- generous enough to accumulate
# several landed hits despite miss-chance RNG.
for _ in range(12):
    time.sleep(1.3)
    recv_all(sA, 0.3)
    recv_all(sB, 0.3)

hp_after, _ = read_hp(sB)
pcts_after = read_limb_pcts(sB)

cmd(sA, "flee"); recv_all(sA, 0.5)
recv_all(sB, 0.3)

overall_lost = hp_before - hp_after
check(overall_lost > 0, "the defender actually took real damage over the exchange")

limb_hp_lost = 0
for name in LIMB_NAMES:
    if name in pcts_before and name in pcts_after:
        pct_delta = max(0, pcts_before[name] - pcts_after[name])
        limb_hp_lost += pct_delta * limb_max_hp // 100

print(f"overall HP lost: {overall_lost}, implied total limb HP lost: {limb_hp_lost}")
ratio = limb_hp_lost / overall_lost
check(0.25 <= ratio <= 0.75,
      f"limb HP lost is roughly HALF overall HP lost (ratio={ratio:.2f}, expected ~0.5, not ~1.0)")

sA.close(); sB.close()

print("\n=== ALL CHECKS PASSED ===")
