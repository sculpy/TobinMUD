#!/usr/bin/env python3
"""Smoke test for mob pursuit (Sneezy → Tobin feature audit, "Monster AI &
behavior (pursuit)"). Checked Sneezy's own 14-monster-ai-behavior.md doc
first: the real system is a whole emotional/opinion model with a
persistence/distance-based multi-room hunt() -- scoped hard down to the
one concrete, named gap: an ACT_AGGRESSIVE mob a player successfully flees
from gets one immediate, single-room chance to follow and resume the
fight (mob_ai_try_pursue(), mob_ai.c), not a real multi-room hunt. Covers:
  1. An ACT_AGGRESSIVE mob eventually gives chase across repeated flee
     attempts (both flee's own ~2/3 escape chance and pursuit's own 50%
     chance are probabilistic, so this retries rather than asserting on
     any single attempt -- same "N attempts, some chance each" precedent
     already used elsewhere in this suite, e.g. smoke_test_drink.py).
  2. A mob WITHOUT ACT_AGGRESSIVE never pursues, deterministically (no
     RNG involved -- mob_ai_try_pursue()'s own flag check short-circuits
     before any chance roll).

    python3 tests/smoke_test_pursuit.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


def announce(test_name, host=host, port=port):
    try:
        s = socket.create_connection((host, port), timeout=3)
        s.settimeout(0.5)
        try:
            while s.recv(4096):
                pass
        except socket.timeout:
            pass
        s.sendall(f"@test {test_name}\r\n".encode())
        s.settimeout(0.5)
        try:
            while s.recv(4096):
                pass
        except socket.timeout:
            pass
        s.close()
    except OSError:
        pass


def announce_done(test_name, host=host, port=port):
    announce(f"done {test_name}", host, port)


announce("smoke_test_pursuit")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_A = 960000 + (int(time.time()) % 30000)
ROOM_B = ROOM_A + 1
MOB_AGGRO_VNUM = ROOM_A + 2
MOB_PASSIVE_VNUM = ROOM_A + 3
ACT_AGGRESSIVE = 1 << 5


def recv_all(sock, timeout=1.0):
    sock.settimeout(timeout)
    chunks = []
    try:
        while True:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
    except socket.timeout:
        pass
    return b"".join(chunks).decode(errors="replace")


def send_line(sock, line):
    sock.sendall((line + "\r\n").encode())


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    return recv_all(sock, timeout)


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def sql(stmt):
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


def set_hp(name, hp, max_hp):
    sql(f"UPDATE player_progress SET hp={hp}, max_hp={max_hp} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


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
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done"); recv_all(s)
    send_line(s, "2"); recv_all(s)  # alignment: neutral
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


imm_name, imm_pw = f"Prsimmb{_suffix}", "prsimmpw1234"
mort_name, mort_pw = f"Prsmortb{_suffix}", "prsmortpw1234"

si = make_char(imm_name, imm_pw); si.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
sA = make_char(mort_name, mort_pw)
cmd(sA, "quit!")
sA.close()

# A has exactly one exit (north -> B); B has exactly one exit (south -> A)
# -- deterministic flee direction, so the test can track which room the
# player is in without guessing (flee picks a random VALID exit, but with
# only one choice there's nothing to randomize).
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_A},0,0,0,'Pursuit Sandbox A','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_B},0,0,0,'Pursuit Sandbox B','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) "
    f"VALUES ({ROOM_A},0,'','',0,0,0,0,0,{ROOM_B});")  # 0 = north
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) "
    f"VALUES ({ROOM_B},2,'','',0,0,0,0,0,{ROOM_A});")  # 2 = south

cols_common = {
    "short_desc": "'a pursuit test dummy'", "long_desc": "'A pursuit test dummy stands here.'",
    "description": "'desc'", "affects": 0, "faction": 0, "fact_perc": 0, "letter": "'A'", "attacks": 1.0,
    "class": 0, "level": 1, "tohit": 0, "ac": 0, "hpbonus": 0,
    "damage_level": 0, "damage_precision": 0, "gold": 0, "race": 0,
    "weight": 0, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
    "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
    "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
    "spec_proc": 0, "skin": 0, "vision": 0, "can_be_seen": 1, "max_exist": 3,
}


def mob_cols(vnum, name, actions):
    cols = {"vnum": vnum, "name": f"'{name}'", "actions": actions, **cols_common}
    col_names = ",".join(cols.keys())
    col_values = ",".join(str(v) for v in cols.values())
    return f"INSERT INTO mob ({col_names}) VALUES ({col_values});"


sql(mob_cols(MOB_AGGRO_VNUM, "dummy", ACT_AGGRESSIVE))
sql(mob_cols(MOB_PASSIVE_VNUM, "calmdummy", 0))

sql(f"UPDATE player SET load_room={ROOM_A} WHERE name='{mort_name}';")
si = relog(imm_name, imm_pw)
sA = relog(mort_name, mort_pw)
set_hp(mort_name, 300, 300)  # survive plenty of missed-flee rounds
cmd(sA, "quit!")
sA.close()
sA = relog(mort_name, mort_pw)

cmd(si, f"goto {ROOM_A}")

# --- 1: an ACT_AGGRESSIVE mob eventually gives chase ---
cmd(si, f"load mob {MOB_AGGRO_VNUM}")
pursued = False
need_attack = True
for attempt in range(30):
    if need_attack:
        cmd(sA, "attack dummy")
        need_attack = False
    out = cmd(sA, "flee")
    if "chases you down" in out:
        pursued = True
        break
    if "You flee head over heels" in out:
        cmd(sA, "south")  # back from B to A, where the mob stayed behind
        need_attack = True
    # else: failed to escape this round -- still fighting in the same room, flee again

check(pursued, f"an ACT_AGGRESSIVE mob eventually chased across repeated flee attempts ({attempt + 1} tried)")

# --- 2: a mob without ACT_AGGRESSIVE never pursues (deterministic, no RNG) ---
cmd(sA, "quit!")
sA.close()
sql(f"UPDATE player SET load_room={ROOM_A} WHERE name='{mort_name}';")
sA = relog(mort_name, mort_pw)
set_hp(mort_name, 300, 300)
cmd(sA, "quit!")
sA.close()
sA = relog(mort_name, mort_pw)

cmd(si, f"load mob {MOB_PASSIVE_VNUM}")
never_pursued = True
for attempt in range(10):
    cmd(sA, "attack calmdummy")
    out = cmd(sA, "flee")
    if "chases you down" in out:
        never_pursued = False
        break
    if "You flee head over heels" in out:
        cmd(sA, "south")
        cmd(sA, "attack calmdummy")

check(never_pursued, "a mob without ACT_AGGRESSIVE never pursues, regardless of attempts")

sA.close()
si.close()
announce_done("smoke_test_pursuit")
print("=== ALL CHECKS PASSED ===")
