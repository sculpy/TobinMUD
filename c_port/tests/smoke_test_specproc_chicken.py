#!/usr/bin/env python3
"""Smoke test for the first proc ported under the "port all special
procedures from sneezy" project (SPEC_PROCS.md, user 2026-08-03).

SPEC_CHICKEN (id 8, spec_mobs.cc's `chicken`) is a rare per-pulse chance
(1-in-5000, kept verbatim from the real upstream) for a mob carrying
that spec_proc to lay an egg (obj vnum 2376, already seeded) into its
own room, with a "$n lays an egg." room echo. Chosen as the first proc
to port because it's fully self-contained -- no faction/disease/pet/
pathfinding subsystem dependency, unlike most of spec_mobs.cc (see
SPEC_PROCS.md's blocker notes) -- and proves the new generic dispatch
framework (mob_ai.c's mob_spec_dispatch_pulse()) end to end: DB-seeded
spec_proc id -> dispatch table -> real gameplay effect.

Since the odds are a genuine 1-in-5000 per AI tick and kept verbatim
(not re-tuned for a test), this loads MANY chicken mobs into one room
and forces MANY AI ticks via the immortal `aitick` debug command
(cmd_aitick.c) to make at least one egg statistically inevitable
(100 mobs x 500 forced ticks = 50,000 rolls, expected ~10 eggs,
P(zero eggs) = (4999/5000)^50000 ~ e^-10, effectively never) rather
than trying to force the RNG directly (no test hook for that exists,
nor should one -- the odds are real upstream behavior).

Covers:
  1. A chicken mob (spec_proc 8) loaded into a room eventually lays an
     egg under forced AI ticks -- a "$n lays an egg." message appears.
  2. The egg is a REAL room object afterward (`look` shows "a chicken
     egg" on the floor), not just a flavor message with nothing to
     back it.
  3. A mob WITHOUT spec_proc 8 in a separate control room never lays
     an egg under the same forced-tick budget (dispatch is correctly
     scoped to the seeded id, not firing for every mob).

    python3 tests/smoke_test_specproc_chicken.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time() * 1000) % 60000)
CONTROL_ROOM = ROOM + 1
CHICKEN_VNUM = ROOM + 2
CONTROL_VNUM = ROOM + 3


announce("smoke_test_specproc_chicken", host, port)


def make_char(name, pw, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", str(class_num), "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


imm_name, imm_pw = f"Chickimm{_suffix}", "chickimmpw123"
imm = make_char(imm_name, imm_pw, 1)
cmd(imm, "quit!")
imm.close()
sql(f"UPDATE player_progress SET level=59 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
imm = socket.create_connection((host, port), timeout=5)
recv_all(imm)
for step in (imm_name, imm_pw, "1"):
    send_line(imm, step)
    recv_all(imm)
cmd(imm, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Chicken Coop Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({CONTROL_ROOM},0,0,0,'Control Room Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

base_cols = {
    "name": "'chickentest'", "short_desc": "'a test chicken'",
    "long_desc": "'A test chicken pecks around here.'", "description": "'desc'",
    "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
    "letter": "'A'", "attacks": 1.0,
    "class": 0, "level": 1, "tohit": 0, "ac": 0, "hpbonus": 0,
    "damage_level": 0, "damage_precision": 0, "gold": 0, "race": 0,
    "weight": 0, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
    "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
    "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
    "skin": 0, "vision": 0, "can_be_seen": 1,
    "max_exist": 500,
}

chicken_cols = dict(base_cols)
chicken_cols["vnum"] = CHICKEN_VNUM
chicken_cols["spec_proc"] = 8
col_names = ",".join(chicken_cols.keys())
col_values = ",".join(str(v) for v in chicken_cols.values())
sql(f"INSERT INTO mob ({col_names}) VALUES ({col_values});")

control_cols = dict(base_cols)
control_cols["vnum"] = CONTROL_VNUM
control_cols["name"] = "'controltest'"
control_cols["short_desc"] = "'a test control mob'"
control_cols["spec_proc"] = 0
col_names = ",".join(control_cols.keys())
col_values = ",".join(str(v) for v in control_cols.values())
sql(f"INSERT INTO mob ({col_names}) VALUES ({col_values});")

check("Chicken Coop Sandbox" in cmd(imm, f"goto {ROOM}"), "goto lands in the chicken sandbox")
for _ in range(100):
    cmd(imm, f"load mob {CHICKEN_VNUM}", timeout=0.3)

cmd(imm, f"goto {CONTROL_ROOM}")
cmd(imm, f"load mob {CONTROL_VNUM}", timeout=0.3)

# --- 3 (control room first, cheap): a non-chicken mob never lays an egg ---
control_out = ""
for _ in range(5):
    control_out += cmd(imm, "aitick 100", timeout=3.0)
check("lays an egg" not in control_out.lower(),
      "a mob WITHOUT spec_proc 8 never lays an egg under the same forced-tick budget")
control_look = cmd(imm, "look")
check("chicken egg" not in control_look.lower(),
      "no chicken egg object appears in the control room")

# --- 1/2: back in the chicken room, force ticks until an egg is statistically inevitable ---
cmd(imm, f"goto {ROOM}")
seen_egg_msg = False
out = ""
for _ in range(5):
    out += cmd(imm, "aitick 100", timeout=3.0)
    if "lays an egg" in out.lower():
        seen_egg_msg = True
        break

check(seen_egg_msg,
      "at least one chicken mob laid an egg ('$n lays an egg.') under a 50,000-roll forced-tick budget")

look_out = cmd(imm, "look")
check("chicken egg" in look_out.lower(),
      "a real chicken egg object is now on the sandbox room's floor")

imm.close()

announce_done("smoke_test_specproc_chicken", host, port)
print("=== ALL CHECKS PASSED ===")
