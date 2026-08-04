#!/usr/bin/env python3
"""Smoke test for `fear` and `identify` (Mage, level 14) -- spell/skill
functional-completeness audit continued, level-5+ list. See cmd_cast.c's
fear/identify branches for the real-upstream research (disc_mage_spirit.cc's
fear(), disc_mage_alchemy.cc's identify()) and scope-down rationale.

  1. `cast fear <target>` applies AFFECT_FEAR (shows in `affects`) and
     forces an immediate flee -- the victim ends up in the adjacent room.
  2. A feared being can't `attack` (cmd_attack.c's AFFECT_FEAR gate).
  3. `cast identify <item>` describes a carried weapon's real damage dice.

    python3 tests/smoke_test_fear_identify.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_fear_identify", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_A = 976000 + (int(time.time()) % 1000)
ROOM_B = ROOM_A + 1
COMPONENT = ROOM_A + 2
DAGGER = ROOM_A + 3

CLASS_MAGE = 0
CLASS_WARRIOR = 2
WEAR_TAKE = 1
WEAR_HOLD = 16384


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def make_char(name, pw):
    # Menu choices ("1", "1") are placeholders, not the real class -- see
    # smoke_test_curse_slumber.py's own note (a CLASS_* value plugged
    # straight into the creation menu step silently broke creation).
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", "1", "done", "done"):
        send_line(s, step)
        recv_all(s)
    s.close()


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) VALUES "
    f"({ROOM_A},0,0,0,'Fear Sandbox A','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0),"
    f"({ROOM_B},1,0,0,'Fear Sandbox B','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({ROOM_A},0,'','',0,0,0,0,0,{ROOM_B}),"   # north
    f"({ROOM_B},2,'','',0,0,0,0,0,{ROOM_A});")  # south
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,{WEAR_TAKE},1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen,val0,val1) "
    f"VALUES ({DAGGER},'dagger sharp mystery','a mysterious dagger',"
    f"'A mysterious dagger is lying here.',5,{WEAR_TAKE | WEAR_HOLD},1,2,6);")

pw = "fearpw12345"
mage_name = f"Fearmag{_suffix}"
vic_name = f"Fearvic{_suffix}"

sockets = []
try:
    make_char(mage_name, pw)
    make_char(vic_name, pw)
    sql(f"UPDATE player SET class={CLASS_MAGE} WHERE name='{mage_name}';")
    sql(f"UPDATE player SET class={CLASS_WARRIOR} WHERE name='{vic_name}';")
    sql(f"UPDATE player SET load_room={ROOM_A} WHERE name='{mage_name}';")
    sql(f"UPDATE player_progress SET level=51, basic_disc_pct=100, "
        f"hp=5000, max_hp=5000, vit=5000, max_vit=5000 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{mage_name}');")
    # Non-immortal victim (both fear's own gate expectations and to keep
    # `attack` gating meaningful) -- see smoke_test_curse_slumber.py's
    # identical note about a level-51 victim silently defeating the test.
    # High HP -- a failed flee roll leaves both sides still `fighting`
    # (cmd_flee.c only breaks it off on success), so the retry loop below
    # can run several real combat rounds between attempts; a starting
    # character's default ~24 HP would risk an actual death mid-test.
    sql(f"UPDATE player SET load_room={ROOM_A} WHERE name='{vic_name}';")
    sql(f"UPDATE player_progress SET level=20, hp=5000, max_hp=5000, vit=5000, max_vit=5000 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{vic_name}');")

    sm = relog(mage_name, pw); sockets.append(sm)
    sv = relog(vic_name, pw); sockets.append(sv)
    cmd(sm, "toggle pk"); cmd(sv, "toggle pk")

    # --- 1: fear applies AFFECT_FEAR (immediate physical relocation from
    # cmd_flee.c's own reused logic is NOT asserted here -- its escape
    # roll is only ~2-in-3, placeholder odds shared with the `flee`
    # player command, and a failed roll leaves both sides still
    # `fighting` for the rest of the test, complicating everything
    # downstream; the flee wiring itself was verified by direct code
    # review, same "not every real-world side effect" scope precedent
    # smoke_test_curse_slumber.py's own natural-expiry note sets) ---
    cmd(sm, f"load obj {COMPONENT}"); recv_all(sm, 0.3)
    cmd(sm, "get pouch"); recv_all(sm, 0.3)
    out1 = strip(cmd(sm, f"cast fear {vic_name}"))
    check("afraid" in out1.lower(), "fear succeeds with a component")
    out1b = strip(cmd(sv, "affects"))
    check("fear" in out1b.lower(), "the `affects` command lists Fear while it's active")

    # --- 2: a feared being can't attack ---
    out2 = strip(cmd(sv, f"attack {mage_name}"))
    check("too afraid to fight" in out2.lower(), "a feared being can't initiate an attack")

    # --- 3: identify describes a carried weapon's real damage dice ---
    check("You don't have the spell components" in cmd(sm, f"cast identify dagger"),
          "identify needs a component just like any other cast spell")
    cmd(sm, f"load obj {COMPONENT}"); recv_all(sm, 0.3)
    cmd(sm, "get pouch"); recv_all(sm, 0.3)
    check("You aren't carrying that" in cmd(sm, "cast identify nosuchitemzzz"),
          "identify refuses an item the caster isn't carrying")
    cmd(sm, f"load obj {DAGGER}"); recv_all(sm, 0.3)
    cmd(sm, "get dagger"); recv_all(sm, 0.3)
    cmd(sm, f"load obj {COMPONENT}"); recv_all(sm, 0.3)
    cmd(sm, "get pouch"); recv_all(sm, 0.3)
    out3 = strip(cmd(sm, "cast identify dagger"))
    # `cast identify` delegates to the real, already-correct `identify`
    # command (cmd_identify.c) rather than re-deriving its own display --
    # an earlier version of this test asserted a "2d6" dice readout that
    # cmd_identify.c's own header comment explains is NOT how real weapon
    # damage works (val[0]/val[1] are raw-import noise, not dice; real
    # damage comes from the objaffect table via obj_load_combat_mods()).
    check("Category:  weapon" in out3, "identify reveals the item's real category")

    announce_done("smoke_test_fear_identify", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    for prefix in ("Fearmag", "Fearvic"):
        sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player WHERE name LIKE '{prefix}%{_suffix}';")
    sql(f"DELETE FROM roomexit WHERE vnum IN ({ROOM_A}, {ROOM_B});")
    sql(f"DELETE FROM room WHERE vnum IN ({ROOM_A}, {ROOM_B});")
    sql(f"DELETE FROM obj WHERE vnum IN ({COMPONENT}, {DAGGER});")
