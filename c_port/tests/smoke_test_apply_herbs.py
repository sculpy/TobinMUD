#!/usr/bin/env python3
"""Smoke test for the Druid `apply` (apply herbs) nature-heal (cmd_apply.c,
Tier-2 port; Sneezy discArray[SKILL_APPLY_HERBS]).

  1. Applying to a target already at full health is refused.
  2. A mortal who doesn't know the skill (Mage) is refused.
  3. An immortal (always succeeds) applies herbs to a wounded target and
     actually restores its hit points (set to 15/200, healed above 15).
  4. A Druid who knows the skill passes the class/skill gate (makes a real
     attempt -- a poultice pressed or a poultice crumbled -- never the
     "don't know" refusal).

Immortal loads the scene and uses `set ... hp` to wound the patient live.

    python3 tests/smoke_test_apply_herbs.py [host] [port]
"""
import re
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_apply_herbs", host, port)

_sfx = "".join(chr(ord("a") + (int(time.time() * 1000000) // 26 ** i) % 26) for i in range(6))
pw = "applyp12"
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


def hp_from_score(text):
    m = re.search(r"HP:\s*(\d+)/(\d+)", text, re.I)
    return int(m.group(1)) if m else None


imm = f"Apim{_sfx}"
make_char(imm, "5")  # Druid immortal
sql("UPDATE player_progress SET level=60, true_level=60 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")
si = login(imm)
cmd(si, f"goto {ROOM}")
cmd(si, "purge")

# 1. self at full health -> refused
cmd(si, f"set {imm} hp 50 50")
out = cmd(si, "apply herbs")
check("already in perfect health" in out.lower(),
      "applying to a fully-healthy target is refused")

# 2. a Mage doesn't know the skill
mage = f"Apmg{_sfx}"
make_char(mage, "1")
sm = login(mage)
cmd(si, f"transfer {mage} {ROOM}")
out = cmd(sm, "apply herbs")
check("don't know how to apply healing herbs" in out.lower(),
      "a non-Druid mortal cannot apply herbs")
sm.close()

# 3. immortal heals a wounded patient -- HP actually rises
patient = f"Appt{_sfx}"
make_char(patient, "1")
sp = login(patient)
cmd(si, f"transfer {patient} {ROOM}")
cmd(si, f"set {patient} hp 15 200")
before = hp_from_score(cmd(sp, "score"))
out = cmd(si, f"apply herbs {patient}")
check("poultice of healing herbs" in out.lower(),
      "an immortal successfully applies herbs to the wounded patient")
after = hp_from_score(cmd(sp, "score"))
check(before is not None and before <= 20 and after is not None and after > before,
      f"the patient's HP rose after herbs (before={before}, after={after})")
sp.close()

# 4. a real Druid passes the skill gate
dru = f"Apdr{_sfx}"
make_char(dru, "5")  # Druid
sql("UPDATE player_progress SET level=20, basic_disc_pct=100 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{dru}');")
sd = login(dru)
cmd(si, f"transfer {dru} {ROOM}")
cmd(si, f"set {dru} hp 30 200")
out = cmd(sd, "apply herbs").lower()
check("don't know" not in out and ("poultice" in out),
      "a Druid passes the skill gate and makes a real apply attempt")
sd.close()

cmd(si, "purge")
si.close()

announce_done("smoke_test_apply_herbs", host, port)
print("PASS: smoke_test_apply_herbs")
