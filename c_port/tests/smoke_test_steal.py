#!/usr/bin/env python3
"""Smoke test for `steal` (spell/skill functional-completeness audit,
2026-07-27: Thief roster entry, skill.c level 1). See cmd_steal.c's own
header comment for the full scope-down rationale (reuses cmd_plant.c's
do_thief_plant() gate/chance shape in reverse).

    python3 tests/smoke_test_steal.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_steal", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

CLASS_THIEF = 3
CLASS_WARRIOR = 2

WEAR_TAKE = 1
TYPE_WEAPON = 5


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def set_class(name, cls):
    sql(f"UPDATE player SET class={cls} WHERE name='{name}';")


def set_combat_disc(name, pct):
    sql(f"UPDATE player_progress SET combat_disc_pct={pct} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_basic_disc(name, pct):
    sql(f"UPDATE player_progress SET basic_disc_pct={pct} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_gold(name, gold):
    sql(f"UPDATE player_progress SET gold={gold} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_room(name, room):
    sql(f"UPDATE player SET load_room={room} WHERE name='{name}';")


def seed_proficiency(name, skill_name, pct):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{name}'), '{skill_name}', {pct}, {int(time.time())}) "
        f"ON DUPLICATE KEY UPDATE pct={pct};")


def get_gold(name):
    out = subprocess.run(["mariadb", "tobin", "-N", "-e",
                           f"SELECT gold FROM player_progress WHERE player_id="
                           f"(SELECT id FROM player WHERE name='{name}');"],
                          check=True, capture_output=True, text=True)
    return int(out.stdout.strip())


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)  # class placeholder
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def make_single(prefix, cls, level=None, room=None, gold=None):
    name, pw = f"{prefix}{_suffix}", f"{prefix}pw12345"
    s0 = make_char(name, pw)
    cmd(s0, "quit!"); s0.close()
    set_class(name, cls)
    if level is not None:
        sql(f"UPDATE player_progress SET level={level} WHERE player_id="
            f"(SELECT id FROM player WHERE name='{name}');")
    if room is not None:
        set_room(name, room)
    if gold is not None:
        set_gold(name, gold)
    set_combat_disc(name, 100)
    set_basic_disc(name, 100)
    s = relog(name, pw)
    cmd(s, "toggle pk")
    return name, s


ROOM = 971000 + (int(time.time()) % 20000)
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Steal Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

# =================== 1. steal gold, success ===================
nameA, sA = make_single("Stlg", CLASS_THIEF, level=10, room=ROOM, gold=0)
nameB, sB = make_single("Stlgv", CLASS_WARRIOR, level=10, room=ROOM, gold=500)
seed_proficiency(nameA, "steal", 100)

out = strip(cmd(sA, f"steal gold {nameB}"))
check("lift" in out.lower() and "gold" in out.lower(), "100%-chance steal gold succeeds")
new_gold_a = get_gold(nameA)
new_gold_b = get_gold(nameB)
check(new_gold_a > 0, "thief's gold increased after a successful steal")
check(new_gold_b < 500, "victim's gold decreased after a successful steal")
check(new_gold_a + new_gold_b == 500, "gold conserved across the transfer (nothing created/destroyed)")
sA.close(); sB.close()

# =================== 2. steal gold, forced failure (negative level gap) ===================
nameC, sC = make_single("Stlgz", CLASS_THIEF, level=1, room=ROOM, gold=0)
nameD, sD = make_single("Stlgzv", CLASS_WARRIOR, level=50, room=ROOM, gold=500)
seed_proficiency(nameC, "steal", 0)

out = strip(cmd(sC, f"steal gold {nameD}"))
check("come up empty-handed" in out.lower(), "very-low-chance steal gold fails")
check(get_gold(nameD) == 500, "victim keeps their gold after a failed steal")
sC.close(); sD.close()

# =================== 3. steal <item>, success ===================
nameE, sE = make_single("Stli", CLASS_THIEF, level=10, room=ROOM)
nameF, sF = make_single("Stliv", CLASS_WARRIOR, level=10, room=ROOM)
seed_proficiency(nameE, "steal", 100)

imm_name, imm_pw = f"Stliimm{_suffix}", "stliimmpw1"
si0 = make_char(imm_name, imm_pw)
cmd(si0, "quit!"); si0.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)
cmd(si, f"goto {ROOM}")

WEAPON_VNUM = ROOM + 1
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,weight,can_be_seen) "
    f"VALUES ({WEAPON_VNUM},'trinket','a small trinket','A small trinket is lying here.',"
    f"{TYPE_WEAPON},{WEAR_TAKE},1,1);")
cmd(si, f"load obj {WEAPON_VNUM}")
cmd(si, "drop trinket")
cmd(sF, "get trinket")
si.close()

out = strip(cmd(sE, f"steal trinket {nameF}"))
check("lift" in out.lower() and "trinket" in out.lower(), "100%-chance steal item succeeds")
out_inv = strip(cmd(sE, "inventory"))
check("trinket" in out_inv.lower(), "the stolen item lands in the thief's inventory")
sE.close(); sF.close()

announce_done("smoke_test_steal", host, port)
print("=== ALL CHECKS PASSED ===")
