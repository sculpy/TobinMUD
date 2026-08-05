#!/usr/bin/env python3
"""Smoke test for the level 2/4/5/9 spell/skill stub-audit fixes (user,
2026-08-04: continuing the backlog in ascending level order). Covers:

  1. `pray clot` (Cleric, level 2) -- stops AFFECT_DISEASE_BLEEDING.
  2. `pray salve` (Cleric, level 4) -- heals, via the generic heal branch.
  3. `pray refresh` (Cleric, level 9) -- restores Vitality.
  4. `cast clot` (Druid, level 5) -- same bleeding cure, cast.c dispatcher.

    python3 tests/smoke_test_spell_level2to9.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 940000 + (int(time.time()) % 30000)
SYMBOL = ROOM + 1
COMPONENT = ROOM + 2

WEAR_TAKE = 1


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw):
        send_line(s, step)
        recv_all(s)
    create_character(s, name, send_line, recv_all, char_class=class_choice)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "color off")
    return s


announce("smoke_test_spell_level2to9", host, port)

imm_name, imm_pw = f"Sptwimm{_suffix}", "sptwimmpw123"
s = make_char(imm_name, imm_pw, "1")
cmd(s, "quit!")
s.close()
set_level(imm_name, 51)
s = relog(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Level2to9 Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,val0,val1,can_be_seen) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,{WEAR_TAKE},5,5,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,{WEAR_TAKE},1);")
check("Level2to9" in cmd(s, f"goto {ROOM}"), "the immortal goto's into the sandbox room")

# --- Cleric: clot, salve, refresh ---
cleric_name, cleric_pw = f"Sptwcle{_suffix}", "sptwclepw123"
sc = make_char(cleric_name, cleric_pw, "2")
cmd(sc, "quit!")
sc.close()
set_level(cleric_name, 51)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{cleric_name}';")
sc = relog(cleric_name, cleric_pw)
check("Level2to9" in cmd(sc, "look"), "the cleric lands in the sandbox room")

check("You conjure" in cmd(s, f"load obj {SYMBOL}"), "the holy symbol is loaded")
cmd(s, "drop symbol")
check("you get" in cmd(sc, "get symbol").lower(), "the cleric picks up the holy symbol")

# clot with no bleeding active (the "weren't bleeding" branch) -- proves
# a real outcome message, not the old silent no-op.
out = cmd(sc, "pray clot")
check("weren't bleeding" in out.lower(), "clot with no active bleeding reports cleanly, not a crash/no-op")

out = cmd(sc, "pray salve")
check("nothing happens" not in out.lower() and ("HP" in out or "restored" in out.lower()),
      "salve reports a real heal, not a no-op")

out = cmd(sc, "pray refresh")
check("Vit" in out and "nothing happens" not in out.lower(), "refresh reports a real Vitality restore, not a no-op")

# --- Druid: clot ---
druid_name, druid_pw = f"Sptwdru{_suffix}", "sptwdrupw123"
sd = make_char(druid_name, druid_pw, "5")
cmd(sd, "quit!")
sd.close()
set_level(druid_name, 51)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{druid_name}';")
sd = relog(druid_name, druid_pw)
check("Level2to9" in cmd(sd, "look"), "the druid lands in the sandbox room")

check("You conjure" in cmd(s, f"load obj {COMPONENT}"), "the component is loaded")
cmd(s, "drop pouch")
check("you get" in cmd(sd, "get pouch").lower(), "the druid picks up the component")

out = cmd(sd, "cast clot")
check("weren't bleeding" in out.lower(), "Druid's cast clot reports cleanly too, not a crash/no-op")

sc.close(); sd.close(); s.close()

sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"DELETE FROM obj WHERE vnum IN ({SYMBOL}, {COMPONENT});")

announce_done("smoke_test_spell_level2to9", host, port)
print("=== ALL CHECKS PASSED ===")
