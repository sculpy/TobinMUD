#!/usr/bin/env python3
"""Smoke test for gender + appearance at character creation (descriptor.c,
being.c, player_repo.c, cmd_score.c, cmd_look.c):
  1. `gender male` + `appearance <text>` on the creation screen take effect;
     score shows "Sex: male" and the appearance line.
  2. Both persist across a reconnect (player.gender / player.appearance).
  3. `look <player>` shows another player's appearance.
  4. Looking at a player with no appearance gives a gender-aware
     "nothing special about <him/her/it>" line.

    python3 tests/smoke_test_gender.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_gender", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def make_char(nm, gender=None, appearance=None):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, "genpw"); recv_all(s)
    send_line(s, "genpw"); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, nm); recv_all(s)      # now on the race screen
    send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done"); recv_all(s)  # attrs done -> the options menu (hand/gender/align/appearance)
    if gender:
        send_line(s, "2"); recv_all(s)  # options menu -> gender sub-menu
        choice = {"male": "1", "female": "2", "neuter": "3"}[gender]
        send_line(s, choice); recv_all(s)  # picks gender, back at the options menu
    if appearance:
        send_line(s, "4"); recv_all(s)  # options menu -> appearance sub-menu
        send_line(s, appearance); recv_all(s)  # sets it, back at the options menu
    send_line(s, "done"); recv_all(s)  # finishes creation
    return s


def relogin(nm):
    r = socket.create_connection((host, port), timeout=5)
    recv_all(r)
    send_line(r, nm); recv_all(r)
    send_line(r, "genpw"); recv_all(r)
    send_line(r, "1"); recv_all(r)
    return r


nameA = f"Gena{_suffix}"
appearA = "a towering scarred warrior"
sA = make_char(nameA, gender="male", appearance=appearA)

out = cmd(sA, "score")
check("Sex: male" in out, "score shows the chosen gender (male)")
check(appearA in out, "score shows the chosen appearance")

# Persistence across reconnect.
sA.close()
sA = relogin(nameA)
out = cmd(sA, "score")
check("Sex: male" in out and appearA in out,
      "gender and appearance persist across a reconnect")

# A neuter character with no appearance, in the same start room (Center Square).
nameB = f"Genb{_suffix}"
sB = make_char(nameB)  # defaults: neuter, no appearance
recv_all(sA)  # drain B's arrival echo

# A looks at B (neuter, no appearance) -> gender-aware nothing-special line.
out = cmd(sA, f"look {nameB[:4]}")
check("nothing special about it" in out,
      "looking at a neuter player with no appearance uses 'it'")

# B looks at A -> sees A's appearance.
out = cmd(sB, f"look {nameA[:4]}")
check(appearA in out, "look <player> shows that player's appearance")

# Looking at nobody -- and nothing (Phase 2C widened this to also search
# objects, so the wording changed from "anyone" to the more general "that").
check("don't see that" in cmd(sA, "look nosuchperson"),
      "looking at an absent name is rejected")

sA.close(); sB.close()
announce_done("smoke_test_gender", host, port)
print("=== ALL CHECKS PASSED ===")
