#!/usr/bin/env python3
"""Smoke test for the Druid `cast sticks to snakes` spell (cmd_cast.c
branch, Tier-2 port; Sneezy sticksToSnakes()).

  1. With no target (and not fighting), the cast is refused.
  2. Casting at a creature conjures a level-scaled rattlesnake (immortal
     caster -> the top "gigantic" tier, vnum 7859) from a yellow mist and
     springs it at the victim; the snake is now present in the room.
  3. A non-Druid mortal (Mage) can't cast it (no yellow mist appears).

    python3 tests/smoke_test_sticks_to_snakes.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_sticks_to_snakes", host, port)

_sfx = "".join(chr(ord("a") + (int(time.time() * 1000000) // 26 ** i) % 26) for i in range(6))
pw = "stickp12"
ROOM = 1200


def make_char(name, char_class):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s, 1.0)
    for step in [name, "y", pw, pw, "new", name, "1", "1", char_class, "done", "done"]:
        send_line(s, step); recv_all(s, 0.7)
    s.close()


def login(name):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s, 1.0)
    send_line(s, name); recv_all(s, 0.7)
    send_line(s, pw); recv_all(s, 0.7)
    send_line(s, "1"); recv_all(s, 0.7)
    cmd(s, "color off")
    return s


def cast_slow(s, spell):
    """A cast stages over 2-3 rounds (~3-4s) before its effect resolves and
    the caster is locked out for that whole delay -- send it, then drain the
    socket long enough to capture the delayed resolution message."""
    send_line(s, spell)
    return recv_all(s, 6.0)


imm = f"Stim{_sfx}"
make_char(imm, "5")  # Druid immortal
sql("UPDATE player_progress SET level=60, true_level=60 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")
si = login(imm)
cmd(si, f"goto {ROOM}")
cmd(si, "purge")

# 1. no target -> refused (and no pet created, so test 2's cap stays free)
out = cast_slow(si, "cast sticks to snakes")
check("upon whom" in out.lower(),
      "casting sticks to snakes with no target is refused")

# 2. cast at a loaded creature -> gigantic rattlesnake springs from a mist
cmd(si, "load mob 104")  # a small cat -- the victim
out = cast_slow(si, "cast sticks to snakes cat")
check("yellow mist" in out.lower() and "springs at" in out.lower(),
      "sticks to snakes conjures a serpent and springs it at the victim")
look = cmd(si, "look")
check("rattlesnake" in look.lower() and "gigantic" in look.lower(),
      "the conjured snake (level-60 caster -> gigantic tier, vnum 7859) is in the room")

cmd(si, "purge")
si.close()

# 3. a Mage can't cast a Druid spell
mage = f"Stmg{_sfx}"
make_char(mage, "1")
sm = login(mage)
out = cast_slow(sm, "cast sticks to snakes").lower()
check("yellow mist" not in out,
      "a non-Druid mortal cannot conjure snakes")
sm.close()

announce_done("smoke_test_sticks_to_snakes", host, port)
print("PASS: smoke_test_sticks_to_snakes")
