#!/usr/bin/env python3
"""Smoke test for body positions (cmd_position.c + gates in move/look/score/
attack, regen weighting).

  1. A fresh character is Standing; score shows the position.
  2. sit / rest / sleep / stand / wake transition and echo, and score tracks.
  3. You can't move unless standing; you can't see while asleep.
  4. In a fight, position reads Fighting and you can't sit.

    python3 tests/smoke_test_positions.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_positions", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def make_char(nm):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, "pospw"); recv_all(s)
    send_line(s, "pospw"); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    return s


nameA = f"Posa{_suffix}"
s = make_char(nameA)

out = cmd(s, "score")
check("Standing" in out, "a fresh character's score shows Standing")
check("(perfect)" in out, "score shows the health word (perfect at full HP)")

# transitions
check("sit down" in cmd(s, "sit"), "sit sits you down")
check("Sitting" in cmd(s, "score"), "score shows Sitting")
check("no position to move" in cmd(s, "north"), "you can't walk while sitting")

check("rest" in cmd(s, "rest"), "rest works")
check("Resting" in cmd(s, "score"), "score shows Resting")

check("sleep" in cmd(s, "sleep"), "sleep works")
check("Sleeping" in cmd(s, "score"), "score shows Sleeping")
check("fast asleep" in cmd(s, "look"), "you can't see while asleep")

check("wake" in cmd(s, "wake"), "wake wakes you (to resting)")
check("clamber to your feet" in cmd(s, "stand"), "stand stands you up")
check("Standing" in cmd(s, "score"), "score shows Standing again")

s.close()

# --- fighting derives the position and blocks sitting ---
# Two fighters with huge HP so a combat round can't end the fight before we
# check (rounds fire on a global pulse -- see STATUS.md).
def relogin(nm):
    r = socket.create_connection((host, port), timeout=5)
    recv_all(r)
    send_line(r, nm); recv_all(r)
    send_line(r, "pospw"); recv_all(r)
    send_line(r, "1"); recv_all(r)      # connect the only character
    return r


nameC, nameD = f"Posc{_suffix}", f"Posd{_suffix}"
make_char(nameC).close()
make_char(nameD).close()
for nm in (nameC, nameD):
    sql(f"UPDATE player_progress SET hp=9999, max_hp=9999 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{nm}');")

sC = relogin(nameC)
sD = relogin(nameD)                     # both land in Center Square (100)
recv_all(sC); recv_all(sD)

# PK opt-in (TODO.md's "PK opt-in flag") postdates this test's original
# PC-vs-PC setup -- both sides must toggle it on before attack/kill/hit
# can even find each other (combat.c's combat_find_room_target()), same
# stale-test class Session 48 already fixed in smoke_test_combat.py.
cmd(sC, "toggle pk")
cmd(sD, "toggle pk")

out = cmd(sC, f"attack {nameD}")
check("You attack" in out, "attack lands (a fight starts)")


def poll(sock, line, needle, tries=8):
    """Retry a command until `needle` shows -- the fight persists (HP 9999),
    so this rides out combat-round output / recv-window timing."""
    for _ in range(tries):
        if needle in cmd(sock, line):
            return True
    return False


check(poll(sC, "score", "Fighting"), "score reads Fighting during a fight")
check(poll(sC, "sit", "finish this fight first"), "you can't sit while fighting")

sC.close(); sD.close()
announce_done("smoke_test_positions", host, port)
print("=== ALL CHECKS PASSED ===")
