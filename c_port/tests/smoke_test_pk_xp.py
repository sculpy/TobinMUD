#!/usr/bin/env python3
"""Smoke test for PK being XP-neutral (TODO.md, user 2026-08-04: "Player PK
should neither gain nor lose experience"). combat.c's combat_award_hit_xp()
skips any PC victim entirely, and combat_defeat()'s death-XP-loss block now
gates on `winner->base.kind != THING_PC` -- so a PK kill should leave BOTH
sides' `experience` column untouched.

Flow: A and B both opt into PK; B is set to 1 HP so the first landed hit
finishes the fight, keeping this deterministic. Records both sides'
experience before the fight, attacks, then re-reads experience via direct
DB read (B is ejected to the account menu on defeat) and checks neither
value moved.

    python3 tests/smoke_test_pk_xp.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_pk_xp", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def xp_of(name):
    return int(query(f"SELECT experience FROM player_progress WHERE player_id="
                      f"(SELECT id FROM player WHERE name='{name}');"))


def set_xp(name, amount):
    sql(f"UPDATE player_progress SET experience={amount} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_hp(name, hp, max_hp):
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={max_hp} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    create_character(s, name, send_line, recv_all)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


nameA = f"Xpwin{_suffix}"
nameB = f"Xplos{_suffix}"
pw = "pkxptestpw1234"

sA = make_char(nameA, pw); sA.close()
sB = make_char(nameB, pw); sB.close()

set_xp(nameA, 1000)
set_xp(nameB, 1000)
set_hp(nameB, 1, 1)  # any landed hit finishes B -- keeps this deterministic

xp_a_before = xp_of(nameA)
xp_b_before = xp_of(nameB)

sA = relog(nameA, pw)
sB = relog(nameB, pw)

step_out_a = cmd(sA, "toggle pk")
check("pk" in step_out_a.lower(), "A opts into PK")
step_out_b = cmd(sB, "toggle pk")
check("pk" in step_out_b.lower(), "B opts into PK")

out = cmd(sA, f"attack {nameB}")
resolved = False
for _ in range(8):
    if "slain" in out.lower() or "defeated" in out.lower():
        resolved = True
        break
    out += recv_all(sA, 1.5)

check(resolved, "the fight resolved (B, at 1 HP, went down to the first landed hit)")
check("experience" not in out.lower(), "A's screen shows no XP-gain message for the PK kill")

sA.close()
sB.close()
time.sleep(0.5)  # let the server-side saves land

check(xp_of(nameA) == xp_a_before, "A (the PK winner) gained no experience")
check(xp_of(nameB) == xp_b_before, "B (the PK loser) lost no experience")

announce_done("smoke_test_pk_xp", host, port)
print("=== ALL CHECKS PASSED ===")
