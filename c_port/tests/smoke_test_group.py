#!/usr/bin/env python3
"""Smoke test for `follow`/`stop`/`group`/`split` (Sneezy â†’ Tobin feature
audit, "Group / party system"). Scoped down from the original's per-
player configurable money-share factor and leader-succession algorithm --
see being.h's `master`/`followers`/`grouped` field comment. Covers:
  1. `follow` alone does NOT grant group benefits (matches the original).
  2. `group <name>` (leader-only) grants it; `group` lists members.
  3. A non-leader can't `group`/`split`.
  4. XP on a kill splits between grouped, in-room members (level-weighted)
     instead of going entirely to the winner.
  5. `split <amount>` divides gold evenly among present grouped members.
  6. `stop` breaks the follow relationship and drops group status.

    python3 tests/smoke_test_group.py [host] [port]
"""
import re
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


announce("smoke_test_group")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)
MOB_VNUM = ROOM + 1


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


def proper(name):
    return name[:1].upper() + name[1:].lower()


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def gold_of(name):
    return int(query(f"SELECT gold FROM player_progress WHERE player_id="
                      f"(SELECT id FROM player WHERE name='{name}');"))


def xp_of(name):
    return int(query(f"SELECT experience FROM player_progress WHERE player_id="
                      f"(SELECT id FROM player WHERE name='{name}');"))


def set_gold(name, amount):
    sql(f"UPDATE player_progress SET gold={amount} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


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
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


leader_name, leader_pw = f"Grpleadb{_suffix}", "grpleadpw1234"
foll_name, foll_pw = f"Grpfollb{_suffix}", "grpfollpw1234"
imm_name, imm_pw = f"Grpimmb{_suffix}", "grpimmpw1234"

sA = make_char(leader_name, leader_pw); sA.close()
sB = make_char(foll_name, foll_pw); sB.close()
si = make_char(imm_name, imm_pw); si.close()
set_level(imm_name, 51)

sA = relog(leader_name, leader_pw)
sB = relog(foll_name, foll_pw)
si = relog(imm_name, imm_pw)

# --- 1: follow alone grants nothing ---
out = cmd(sB, f"follow {leader_name}")
check(f"You now follow {proper(leader_name)}" in out, "follow confirms")
out = cmd(sA, "group")
check("not grouped in yet" in out, "a followed-but-not-grouped member shows as such")

# --- 2: group <name> grants it; group lists members ---
out = cmd(sA, f"group {foll_name}")
check(f"You group in {proper(foll_name)}" in out, "group <name> confirms")
out = cmd(sA, "group")
check("not grouped in yet" not in out, "both leader and follower now show grouped")
check(proper(foll_name) in out, "group listing includes the follower")

# --- 3: a non-leader can't group/split ---
check("Only the group leader" in cmd(sB, f"group {leader_name}"), "a follower can't run group <name>")
check("Only the group leader" in cmd(sB, "split 100"), "a follower can't run split")

# --- bootstrap a sandbox room + tanky mob for the XP/gold checks ---
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Group Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
cols = {
    "vnum": MOB_VNUM, "name": "'grpdummy'", "short_desc": "'a group test dummy'",
    "long_desc": "'A group test dummy stands here.'", "description": "'desc'",
    "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
    "letter": "'A'", "attacks": 1.0,
    "class": 0, "level": 1, "tohit": 0, "ac": 0, "hpbonus": 0,
    "damage_level": 0, "damage_precision": 0, "gold": 0, "race": 0,
    "weight": 0, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
    "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
    "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
    "spec_proc": 0, "skin": 0, "vision": 0, "can_be_seen": 1,
    "max_exist": 1,
}
col_names = ",".join(cols.keys())
col_values = ",".join(str(v) for v in cols.values())
sql(f"INSERT INTO mob ({col_names}) VALUES ({col_values});")

# `goto` is immortal-only and doesn't drag followers along -- move the
# leader and follower into the sandbox via load_room + relog instead.
# `quit!` (not a raw close) is required first -- a raw close leaves a
# linkdead body at its CURRENT room, and reconnecting resumes there,
# ignoring load_room entirely (enter_world() only consults load_room when
# no linkdead body exists to resume into instead).
sql(f"UPDATE player SET load_room={ROOM} WHERE name IN ('{leader_name}','{foll_name}');")
cmd(sA, "quit!"); cmd(sB, "quit!")
sA.close(); sB.close()
# a big HP buffer so the (weak-hitting-on-average, but not zero-damage)
# sandbox mob can't win the fight before the leader does -- legitimate
# test setup, not the mechanism under test.
set_hp(leader_name, 300, 300)
sA = relog(leader_name, leader_pw)
sB = relog(foll_name, foll_pw)
# group state is in-memory only -- re-follow/re-group after relogging.
cmd(sB, f"follow {leader_name}")
cmd(sA, f"group {foll_name}")

set_gold(leader_name, 0)
set_gold(foll_name, 0)
xp_before_leader = xp_of(leader_name)
xp_before_foll = xp_of(foll_name)

cmd(si, f"goto {ROOM}")
cmd(si, f"load mob {MOB_VNUM}")

# --- 4: XP splits between grouped, in-room members ---
out = cmd(sA, "attack grpdummy")
check("You attack" in out, "leader engages the mob")
slain = False
for _ in range(20):
    out = cmd(sA, "score")
    if "slain" in out.lower() or "have defeated" in out.lower() or xp_of(leader_name) > xp_before_leader:
        slain = True
        break
    time.sleep(1.5)

check(xp_of(leader_name) > xp_before_leader, "the leader gained XP from the kill")
check(xp_of(foll_name) > xp_before_foll, "the grouped, in-room follower ALSO gained XP from the same kill")

# --- 5: split divides gold evenly ---
# set_gold() only touches the DB row -- the leader's in-memory progress
# won't see it without a relog (same load_room lesson as above). And it
# must run AFTER quit!, not before: quit! itself calls player_save()
# (descriptor_leave_to_menu(), descriptor.c), which would otherwise
# overwrite our direct SQL update with the leader's stale in-memory gold
# (1, left over from the kill's loot drop) the instant they quit.
# The follower never relogs here, so their gold is checked by DELTA, not
# absolute value -- phase 4's kill already granted them a small random
# share of the mob's gold drop (also group-split, since they were
# grouped for that kill too), which a same-value check would miss.
foll_gold_before_split = gold_of(foll_name)
cmd(sA, "quit!")
sA.close()
set_gold(leader_name, 100)
sA = relog(leader_name, leader_pw)
cmd(sB, f"follow {leader_name}")
cmd(sA, f"group {foll_name}")
out = cmd(sA, "split 100")
check("50 each" in out, "split reports an even 50/50 division between 2 present members")
check(gold_of(foll_name) - foll_gold_before_split == 50, "the follower received their even split share")

# --- 6: stop breaks the relationship ---
out = cmd(sB, "stop")
check(f"You stop following {proper(leader_name)}" in out, "stop confirms")
check("Only the group leader" not in cmd(sB, "group"), "after stopping, group no longer refuses (no master anymore)")

sA.close()
sB.close()
si.close()
announce_done("smoke_test_group")
print("=== ALL CHECKS PASSED ===")
