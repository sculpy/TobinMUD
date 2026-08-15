#!/usr/bin/env python3
"""Smoke test for the Cleric `turn` (turn undead) command (cmd_turn.c,
spell/skill audit "Turn undead"). Ported from Sneezy's TBeing::doTurn().

An immortal loads three mobs -- an undead (Yorick), a demon (a vrock), and
an animal (a cat) -- then:

  1. An immortal turn on the undead succeeds (immortals bypass the roll):
     the creature recoils, seared and terrified.
  2. Turning a non-minion (the cat) is refused: "not a minion of darkness".
  3. A non-Cleric mortal (Mage) doesn't know the skill.
  4. A Cleric who knows the skill actually makes the attempt (a real turn
     outcome -- sear on success or a faltered will on failure -- never the
     "don't know" refusal), confirming the class/skill gate passes.

Mortals can't goto/load, so the immortal loads the mobs and `transfer`s
the mortals in.

    python3 tests/smoke_test_turn_undead.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_turn_undead", host, port)

_sfx = "".join(chr(ord("a") + (int(time.time() * 1000000) // 26 ** i) % 26) for i in range(6))
pw = "turnpw12"
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


imm = f"Tuim{_sfx}"
make_char(imm, "2")  # Cleric immortal (any class -- immortals are all-class)
sql("UPDATE player_progress SET level=60, true_level=60 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")
si = login(imm)
cmd(si, f"goto {ROOM}")
cmd(si, "purge")
cmd(si, "load mob 3107")  # Yorick the Undead -- UNDEAD
cmd(si, "load mob 104")   # a small cat -- animal

# 1. turning a non-minion is refused (done first, before any fight starts --
#    once the immortal is fighting, `turn` targets the current opponent and
#    ignores its argument).
out = cmd(si, "turn cat")
check("not a minion of darkness" in out.lower(),
      "turning a mundane animal is refused (not undead/demon)")

# 2. immortal turn on the undead succeeds (and opens the fight)
out = cmd(si, "turn yorick")
check("recoil" in out.lower() and "terrified" in out.lower(),
      "an immortal turn sears and routs the undead (success path)")
cmd(si, "flee"); cmd(si, "flee")  # drop out of the fight the turn started

# 3. a Mage doesn't know the skill
mage = f"Tumg{_sfx}"
make_char(mage, "1")
sm = login(mage)
cmd(si, f"transfer {mage} {ROOM}")
out = cmd(sm, "turn yorick")
check("don't know how to turn" in out.lower(),
      "a non-Cleric mortal cannot turn undead")
sm.close()

# 4. a Cleric who knows the skill makes a real attempt
cler = f"Tucl{_sfx}"
make_char(cler, "2")  # Cleric
sql("UPDATE player_progress SET level=10, basic_disc_pct=100 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{cler}');")
sc = login(cler)
cmd(si, f"transfer {cler} {ROOM}")
out = cmd(sc, "turn yorick").lower()
check(("recoil" in out or "falter" in out) and "don't know" not in out,
      "a Cleric passes the skill gate and makes a real turn attempt")
sc.close()

cmd(si, "flee"); cmd(si, "purge")
si.close()

announce_done("smoke_test_turn_undead", host, port)
print("PASS: smoke_test_turn_undead")
