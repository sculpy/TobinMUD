#!/usr/bin/env python3
"""Smoke test for per-race quest-item tables (Sneezy -> Tobin feature
audit -- disclosed NOT a port, see tobin_migrations.sql's `quest_item`
doc comment/quest_repo.h: SneezyMUD's real race data and quest system
carry no per-race item table at all, this is a Tobin-original addition).
Exercises the full loop against the seeded "heirloom" demo quest
(zz_quest_item_race.sql): `set <char> quest heirloom 1` (immortal) puts a
character on the quest, `quest claim heirloom` grants THEIR race's own
reward vnum, a second claim is refused (already claimed), and a
DIFFERENT race gets a DIFFERENT vnum for the same quest+stage.
    python3 tests/smoke_test_race_quest_items.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, cmd, check, announce, announce_done
from mud_creation import create_character
host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)
def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()
def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)
announce("smoke_test_race_quest_items", host, port)
suf = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(5))
# Expected reward vnums, straight from zz_quest_item_race.sql (reusing
# each race's own starting racial weapon vnum -- zz_newbie_gear_race.sql).
EXPECT_VNUM = {0: 36996, 1: 36998, 2: 37001, 3: 36997, 4: 37000, 5: 36999}
def new_char(name, pw, race_pick):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw):
        send_line(s, step)
        recv_all(s)
    create_character(s, name, send_line, recv_all, race=str(race_pick))
    return s
# Human (race 1 at creation -> race 0) and Ogre (race 3 at creation -> race 2).
human = new_char(f"Rqih{suf}", "rqipw12345", 1)
send_line(human, "quit!")
recv_all(human)
human.close()
ogre = new_char(f"Rqio{suf}", "rqipw12345", 3)
send_line(ogre, "quit!")
recv_all(ogre)
ogre.close()
imm_name = f"Rqii{suf}"
imm_pw = "rqipw12345"
imm = new_char(imm_name, imm_pw, 1)
send_line(imm, "quit!")
recv_all(imm)
imm.close()
sql(f"UPDATE player_progress SET level=59 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
imm = socket.create_connection((host, port), timeout=5)
recv_all(imm)
for step in (imm_name, imm_pw, "1"):
    send_line(imm, step)
    recv_all(imm)
out = strip(cmd(imm, f"set {f'Rqih{suf}'} quest heirloom 1"))
check("quest" in out.lower() or "stage" in out.lower() or "set" in out.lower(),
      f"immortal sets Human's quest stage: {out!r}")
out = strip(cmd(imm, f"set {f'Rqio{suf}'} quest heirloom 1"))
check("quest" in out.lower() or "stage" in out.lower() or "set" in out.lower(),
      f"immortal sets Ogre's quest stage: {out!r}")
send_line(imm, "quit!")
imm.close()
def claim(name, pw, expect_vnum_race):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, pw)
    recv_all(s)
    send_line(s, "1")
    recv_all(s)
    out = strip(cmd(s, "quest claim heirloom"))
    check("claim your reward" in out.lower(), f"{name} claims the heirloom: {out!r}")
    inv = query(
        f"SELECT COUNT(*) FROM player_inventory pi JOIN player p ON p.id=pi.player_id "
        f"WHERE p.name='{name}' AND pi.vnum={EXPECT_VNUM[expect_vnum_race]};")
    check(inv != "0", f"{name} really has vnum {EXPECT_VNUM[expect_vnum_race]} in inventory (count={inv})")
    out2 = strip(cmd(s, "quest claim heirloom"))
    check("already claimed" in out2.lower(), f"{name} can't double-claim: {out2!r}")
    send_line(s, "quit!")
    s.close()
claim(f"Rqih{suf}", "rqipw12345", 0)  # Human -> vnum 36996
claim(f"Rqio{suf}", "rqipw12345", 2)  # Ogre -> vnum 37001
check(EXPECT_VNUM[0] != EXPECT_VNUM[2], "Human and Ogre really do get DIFFERENT reward vnums")
sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
sql(f"DELETE FROM player WHERE name='{imm_name}';")
announce_done("smoke_test_race_quest_items", host, port)
print("=== ALL CHECKS PASSED ===")
