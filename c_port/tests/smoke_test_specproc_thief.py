#!/usr/bin/env python3
"""Smoke test for SPEC_THIEF (spec_mobs.cc's `thief`), another proc
ported under the spec-proc project (SPEC_PROCS.md). Id 4 lives in the
REAL `mob_specials[]` registration array (spec_mobs.cc), not the sparse
named-constant list in spec_mobs.h. Real seeded mobs already carry
`spec_proc=4` (vnum 602 "thief" among others), confirming this is
genuine importable content, not fabricated for the test.

Ported behavior: on each AI pulse, an awake, standing, non-fighting
thief mob has a 1-in-26 chance to silently pickpocket a random loose
(not worn/held) item from a non-immortal, non-fighting PC in its room --
ported from upstream's own `rob_blind()` helper (mob_ai.c's
mob_spec_thief_pulse()). Genuinely blind: no message to anyone, success
or failure, matching upstream. Uses `aitick <count>` (cmd_aitick.c) to
force enough pulses that the 1-in-26 * 1-in-5 combined odds land
reliably instead of waiting on the real ~60s cadence.

Covers:
  1. A thief mob (vnum 602) eventually steals a loose carried item from
     a PC in its room, forced via many `aitick` pulses.
  2. A worn item survives -- only loose inventory is ever taken (checked
     by leaving a worn item on the victim throughout).
  3. An ordinary mob with no matching spec_proc never steals, even after
     the same number of forced pulses (no false-positive firing).

    python3 tests/smoke_test_specproc_thief.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


announce("smoke_test_specproc_thief", host, port)


def make_char(name, pw, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", str(class_num), "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "color off")
    return s


imm_name, imm_pw = f"Sth{_suffix}", "sthpw1234567"
vic_name, vic_pw = f"Sthvic{_suffix}", "sthvicpw1234"
ROOM = 954000 + (int(time.time()) % 40000)
TRINKET1 = ROOM + 1
HELM = ROOM + 2

WEAR_TAKE = 1
WEAR_HEAD = 16
TYPE_ARMOR = 9
TYPE_MISC = 12  # matches OBJ_CAT_OTHER's raw seeded itemTypeT bucket, same as other tests' "trinket" props

s1 = make_char(imm_name, imm_pw, 3)  # Warrior (level 51+ needed for `load`)
cmd(s1, "quit!"); s1.close()
sql(f"UPDATE player_progress SET level=59 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Thief Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{imm_name}';")
s1 = relog(imm_name, imm_pw)

sv = make_char(vic_name, vic_pw, 3)
cmd(sv, "quit!"); sv.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{vic_name}';")
sv = relog(vic_name, vic_pw)
check("Thief Sandbox" in cmd(sv, "look"), "the victim lands in the sandbox room")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({TRINKET1},'trinket','a small trinket','A small trinket lies here.',{TYPE_MISC},{WEAR_TAKE},1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({HELM},'helm','a plain helm','A plain helm lies here.',{TYPE_ARMOR},{WEAR_TAKE | WEAR_HEAD},1);")

check("You conjure" in cmd(s1, f"load obj {TRINKET1}"), "a loose trinket is loaded")
cmd(s1, "give trinket " + vic_name)
check("You conjure" in cmd(s1, f"load obj {HELM}"), "a wearable helm is loaded")
cmd(s1, "give helm " + vic_name)

# --- 1/2: a real thief mob (vnum 602, spec_proc=4) steals the loose
# trinket but never the worn helm, across many forced pulses ---
cmd(sv, "wear helm")
out = cmd(sv, "inventory")
check("trinket" in out.lower(), "the victim is carrying the loose trinket before any pulses")

check("You conjure" in cmd(s1, "load mob 602"), "a real thief mob is loaded")

stolen = False
for _ in range(40):
    cmd(s1, "aitick 50")
    out = cmd(sv, "inventory", 0.3)
    if "trinket" not in out.lower():
        stolen = True
        break
check(stolen, "the thief mob eventually pickpockets the loose trinket")

out = cmd(sv, "equipment")
check("helm" in out.lower(), "the worn helm survives -- only loose inventory is ever taken")

# --- 3: an ordinary mob (no matching spec_proc) never steals ---
cmd(s1, "purge thief")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({TRINKET1 + 100},'trinket','a small trinket','A small trinket lies here.',12,1,1);")
check("You conjure" in cmd(s1, f"load obj {TRINKET1 + 100}"), "a fresh loose trinket is loaded")
cmd(s1, "give trinket " + vic_name)
check("You conjure" in cmd(s1, "load mob 10"), "an ordinary mob (vnum 10, no spec_proc) is loaded")
for _ in range(10):
    cmd(s1, "aitick 50")
out = cmd(sv, "inventory")
check("trinket" in out.lower(), "an ordinary mob never steals the trinket, even after many pulses")

s1.close()
sv.close()

announce_done("smoke_test_specproc_thief", host, port)
print("=== ALL CHECKS PASSED ===")
