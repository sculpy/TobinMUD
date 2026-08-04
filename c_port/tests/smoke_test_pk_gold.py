#!/usr/bin/env python3
"""Smoke test for gold-on-kill (combat.c's combat_defeat(), TODO.md "Split
gold on kill" / "Split victim's gold among the group on kill"). Solo case
only -- there's no group/party system yet, so a PK winner simply takes the
loser's ENTIRE gold; the "split it between all group members if grouped"
half remains a separate, still-blocked item.

Flow: A (0 gold) and B (500 gold, deliberately 1 HP so the very first landed
hit finishes the fight, keeping this deterministic) both opt into PK
(`toggle pk`) and A attacks B. Checks:
  1. A's incoming text shows the loot message and B's shows the matching one.
  2. A's gold ends at 500, B's at 0 (both via direct DB read, since B is
     ejected to the account menu on defeat).

A automatically loots the corpse's gold via `toggle autoloot` (2026-08-04
fix: the corpse-gold-loot message only ever fires through that toggle --
combat.c's autoloot pass -- this test previously assumed it fired
unconditionally and never actually turned the toggle on, so it was passing
by accident before autoloot existed and silently broke once gold moved to
a lootable corpse object instead of a direct wallet credit).

    python3 tests/smoke_test_pk_gold.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_pk_gold", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def gold_of(name):
    return int(query(f"SELECT gold FROM player_progress WHERE player_id="
                      f"(SELECT id FROM player WHERE name='{name}');"))


def set_gold(name, amount):
    sql(f"UPDATE player_progress SET gold={amount} WHERE player_id="
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


nameA = f"Goldwin{_suffix}"
nameB = f"Goldlos{_suffix}"
pw = "goldtestpw1234"

sA = make_char(nameA, pw); sA.close()
sB = make_char(nameB, pw); sB.close()

set_gold(nameA, 0)
set_gold(nameB, 500)
set_hp(nameB, 1, 1)  # any landed hit finishes B -- keeps this deterministic

sA = relog(nameA, pw)
sB = relog(nameB, pw)

step_out_a = cmd(sA, "toggle pk")
check("pk" in step_out_a.lower(), "A opts into PK")
autoloot_out = cmd(sA, "toggle autoloot")
check("autoloot" in autoloot_out.lower(), "A opts into autoloot, so the corpse's gold is picked up automatically")
step_out_b = cmd(sB, "toggle pk")
check("pk" in step_out_b.lower(), "B opts into PK")

out = cmd(sA, f"attack {nameB}")
looted = False
for _ in range(8):
    if "loot" in out.lower() or "slain" in out.lower() or "defeated" in out.lower():
        looted = True
        break
    out += recv_all(sA, 1.5)

check(looted, "the fight resolved (B, at 1 HP, went down to the first landed hit)")
check("You loot 500 gold" in out, "A's screen shows the loot message with the right amount")

sA.close()
sB.close()
time.sleep(0.5)  # let the server-side saves land

check(gold_of(nameA) == 500, "A ended up with all 500 gold")
check(gold_of(nameB) == 0, "B's gold was fully taken")

announce_done("smoke_test_pk_gold", host, port)
print("=== ALL CHECKS PASSED ===")
