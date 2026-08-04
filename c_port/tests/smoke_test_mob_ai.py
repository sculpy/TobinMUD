#!/usr/bin/env python3
"""Smoke test for mob AI wander/scavenge (Session 43 continued, user: "in
pulse, make sure that mob actions click and mobs that can wander will do
so, look at mob ai from sneezy" / "i want cleaner mobs to clean up
randomly, i believe this is also in mob ai"). Reads `mob.actions` (the
original's ACT_* bitmask) and drives mob_ai_tick() (mob_ai.c), pulse-
registered at the same ~60s cadence as gametime_tick()/zone_process_run().

That real cadence is far too slow to wait on in an automated test (the
wander/scavenge chances are only 20%/25% per tick), so this uses the new
immortal-only `aitick [count]` debug command (cmd_aitick.c) to force many
ticks synchronously instead -- `aitick 30` gives a ~99.9% chance of at
least one wander/scavenge firing, deterministic enough for a smoke test
without waiting on real time at all.

  1. A mob WITHOUT ACT_SENTINEL wanders out of its room through its one
     exit within a forced batch of ticks.
  2. A mob WITH ACT_SENTINEL (bit 1, value 2) never wanders, even after
     the same forced batch.
  3. A mob with ACT_SCAVENGER (bit 2, value 4) cleans up a loose
     OBJ_CAT_TRASH item in its room within a forced batch of ticks.

    python3 tests/smoke_test_mob_ai.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_mob_ai", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_A = 900000 + (int(time.time()) % 70000)
ROOM_B = ROOM_A + 1
MOB_SENTINEL = ROOM_A + 2
MOB_WANDER = ROOM_A + 3
MOB_CLEANER = ROOM_A + 4
TRASH_OBJ = ROOM_A + 5

ACT_SENTINEL = 2
ACT_SCAVENGER = 4
TYPE_TRASH = 13


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(sock, name, pw):
    recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "y"); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, "new"); recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)  # race: human (zero stat modifier)
    send_line(sock, "1"); recv_all(sock)  # territory: urban
    send_line(sock, "1"); recv_all(sock)  # class: mage
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)  # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def make_mob(vnum, keyword, actions):
    sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
        f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
        f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
        f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
        f"VALUES ({vnum},'{keyword}','a {keyword}','A {keyword} stands here.',"
        f"'A stuffed practice dummy.',{actions},0,0,0,'A',1.0,0,1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
        f"10,10,1,0,0,0,1,1);")


imm_name = f"Mobaiimm{_suffix}"
imm_pw = "mobaiimmpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_A},0,0,0,'MobAI Room A','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_B},0,0,0,'MobAI Room B','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO roomexit (vnum, direction, name, description, type, "
    f"condition_flag, lock_difficulty, weight, key_num, destination) "
    f"VALUES ({ROOM_A}, 0, '', '', 0, 0, 0, 0, 0, {ROOM_B});")
sql(f"INSERT INTO roomexit (vnum, direction, name, description, type, "
    f"condition_flag, lock_difficulty, weight, key_num, destination) "
    f"VALUES ({ROOM_B}, 2, '', '', 0, 0, 0, 0, 0, {ROOM_A});")

check("MobAI Room A" in cmd(s, f"goto {ROOM_A}"), "goto lands in the SQL-bootstrapped room A")

# --- 1 & 2: wander vs. sentinel ---
make_mob(MOB_SENTINEL, f"sentineldummy{_suffix}", ACT_SENTINEL)
make_mob(MOB_WANDER, f"wanderdummy{_suffix}", 0)
check("You conjure" in cmd(s, f"load mob {MOB_SENTINEL}"), "sentinel dummy loaded")
check("You conjure" in cmd(s, f"load mob {MOB_WANDER}"), "wander dummy loaded")

out = cmd(s, "look")
check(f"sentineldummy{_suffix}" in out.lower(), "sentinel dummy visible in room A before ticking")
check(f"wanderdummy{_suffix}" in out.lower(), "wander dummy visible in room A before ticking")

out = cmd(s, "aitick 30")
check("Ran 30 mob AI tick" in out, "aitick forces 30 ticks")
# Room A and B are directly connected to each other and nowhere else, so a
# wandering mob bounces back and forth between just the two -- an even
# number of moves lands it right back in room A by chance, so checking
# its FINAL room isn't reliable. The "walks to the"/"walks in from the"
# echoes in the aitick response itself prove it moved at all (2026-07-11:
# these replaced the old, buggy "leaves"/"arrives" wording that printed
# the mob's raw keyword list instead of its short_descr -- see
# mob_ai.c's cap_first() fix), which is what we actually want to verify.
check("walks to the" in out.lower() or "walks in from the" in out.lower(),
      "the non-sentinel mob's wander produced at least one move echo")
check(f"a sentineldummy{_suffix} walks" not in out.lower(),
      "the sentinel-flagged mob never produced a wander echo")

out = cmd(s, "look")
check(f"sentineldummy{_suffix}" in out.lower(),
      "the sentinel-flagged mob is still in room A after 30 forced ticks")

cmd(s, f"goto {ROOM_A}")

# --- 3: scavenger cleans up trash ---
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({TRASH_OBJ},'trashbit','a scrap of trash','A scrap of trash is lying here.',"
    f"{TYPE_TRASH},1,1);")
make_mob(MOB_CLEANER, f"cleanerdummy{_suffix}", ACT_SCAVENGER)
check("You conjure" in cmd(s, f"load obj {TRASH_OBJ}"), "trash object loaded")
check("You conjure" in cmd(s, f"load mob {MOB_CLEANER}"), "cleaner dummy loaded")

out = cmd(s, "look")
check("trash" in out.lower(), "the trash is visible before ticking")

out = cmd(s, "aitick 30")
check("Ran 30 mob AI tick" in out, "aitick forces 30 more ticks")

out = cmd(s, "look")
check("trash" not in out.lower(), "the scavenger mob cleaned up the trash within 30 forced ticks")

s.close()
announce_done("smoke_test_mob_ai", host, port)
print("=== ALL CHECKS PASSED ===")
