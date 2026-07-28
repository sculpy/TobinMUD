#!/usr/bin/env python3
"""Smoke test for death XP loss (Sneezy â†’ Tobin feature audit, "Death
processing (XP loss, resurrection)"). User, AskUserQuestion 2026-07-19:
XP loss only -- Tobin's PC "death" was already NOT permadeath (half-heal +
eject to menu, combat.c's combat_defeat()), so there's no corpse to build a
resurrection spell around; "resurrection" is already covered by the
existing soft-respawn/relog flow. Formula (combat.c): min(20% of current
XP, XP banked past the current level's own threshold) -- the second term
means a death can never de-level anyone. PvP (a PC winner) divides the
result by 10; a MOB winner (the ordinary "died to a monster" case) doesn't.
Covers:
  1. PvE death with plenty of banked XP: loses exactly 20%.
  2. PvE death with only a little banked XP: capped so it can't de-level.
  3. PvP death (`toggle pk`) with plenty of banked XP: loses 20% / 10.

    python3 tests/smoke_test_death_xploss.py [host] [port]
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


announce("smoke_test_death_xploss")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 920000 + (int(time.time()) % 70000)
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


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def xp_of(name):
    return int(query(f"SELECT experience FROM player_progress WHERE player_id="
                      f"(SELECT id FROM player WHERE name='{name}');"))


def level_of(name):
    return int(query(f"SELECT level FROM player_progress WHERE player_id="
                      f"(SELECT id FROM player WHERE name='{name}');"))


def set_level_xp(name, level, experience):
    sql(f"UPDATE player_progress SET level={level}, experience={experience} WHERE player_id="
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
    send_line(s, "1"); recv_all(s)  # class: warrior (better to-hit for the PvP fight)
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


imm_name, imm_pw = f"Xpimmb{_suffix}", "xpimmpw1234"
si = make_char(imm_name, imm_pw); si.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)

# A tanky, hard-hitting sandbox mob (unlike the "weak" template used
# elsewhere this session) -- high tohit/damage so it reliably lands the
# killing blow on a 1-HP PC within a few rounds, and enough HP that the
# PC's own attack can't finish it off first. Deterministic mob-wins setup,
# same spirit as smoke_test_pk_gold.py's "loser at 1 HP" trick.
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Death XP Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
cols = {
    "vnum": MOB_VNUM, "name": "'xpdummy'", "short_desc": "'an xp-loss test dummy'",
    "long_desc": "'An xp-loss test dummy stands here.'", "description": "'desc'",
    "actions": 0, "affects": 0, "faction": 0, "fact_perc": 0,
    "letter": "'A'", "attacks": 1.0,
    "class": 0, "level": 5, "tohit": 80, "ac": 0, "hpbonus": 8,
    "damage_level": 15, "damage_precision": 5, "gold": 0, "race": 0,
    "weight": 0, "height": 0, "str": 0, "bra": 0, "con": 0, "dex": 0,
    "agi": 0, "intel": 0, "wis": 0, "foc": 0, "per": 0, "cha": 0,
    "kar": 0, "spe": 0, "pos": 10, "def_position": 10, "sex": 1,
    "spec_proc": 0, "skin": 0, "vision": 0, "can_be_seen": 1,
    "max_exist": 3,
}
col_names = ",".join(cols.keys())
col_values = ",".join(str(v) for v in cols.values())
sql(f"INSERT INTO mob ({col_names}) VALUES ({col_values});")


def fight_mob_and_check(pc_sock, pc_name):
    """Issues `attack xpdummy` and polls until the PC dies (ejected to the
    account menu -- its own screen shows the account-menu prompt)."""
    cmd(si, f"load mob {MOB_VNUM}")
    out = cmd(pc_sock, "attack xpdummy")
    for _ in range(10):
        if "menu" in out.lower() or "DEAD" in out or "slain" in out.lower() or "defeated" in out.lower():
            break
        out += recv_all(pc_sock, 1.5)
    return out


# --- 1: PvE death with plenty of banked XP loses exactly 20% ---
# `quit!` (not a raw close) before the load_room move takes effect -- a raw
# close leaves a linkdead body at its CURRENT room, which reconnecting
# resumes into regardless of load_room (same lesson smoke_test_group.py's
# own comments document).
nameA, pwA = f"Xpvea{_suffix}", "xpveapw1234"
sA = make_char(nameA, pwA)
cmd(sA, "quit!")
sA.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{nameA}';")
set_level_xp(nameA, 5, 4326)  # xp_for_level(5)=3326 (real upstream table, wired 2026-07-28), so 1000 banked past it
set_hp(nameA, 1, 1)
sA = relog(nameA, pwA)
cmd(si, f"goto {ROOM}")
out = fight_mob_and_check(sA, nameA)
check("You lose" in out and "experience" in out, "PvE death shows the XP-loss message")
check(xp_of(nameA) == 3461, "PvE death with ample banked XP loses exactly 20% (4326 -> 3461)")
check(level_of(nameA) == 5, "the PvE death did not change level")

# --- 2: PvE death with only a little banked XP is capped, never de-levels ---
nameB, pwB = f"Xpveb{_suffix}", "xpvebpw1234"
sB = make_char(nameB, pwB)
cmd(sB, "quit!")
sB.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{nameB}';")
set_level_xp(nameB, 5, 3376)  # only 50 banked past the level-5 threshold (3326)
set_hp(nameB, 1, 1)
sB = relog(nameB, pwB)
out = fight_mob_and_check(sB, nameB)
check(xp_of(nameB) == 3326, "a tiny banked-XP death is capped at exactly the level-5 threshold, not below")
check(level_of(nameB) == 5, "the capped death did not de-level the character")

# --- 3: PvP death (mutual toggle pk) with ample banked XP loses 20% / 10 ---
nameC, pwC = f"Xppkwin{_suffix}", "xppkwinpw1234"
nameD, pwD = f"Xppklos{_suffix}", "xppklospw1234"
sC = make_char(nameC, pwC); sC.close()
sD = make_char(nameD, pwD); sD.close()
set_level_xp(nameD, 5, 4326)  # same banked amount as scenario 1, for a clean 20%-vs-2% comparison
set_hp(nameD, 1, 1)
sC = relog(nameC, pwC)
sD = relog(nameD, pwD)
cmd(sC, "toggle pk")
cmd(sD, "toggle pk")
out = cmd(sC, f"attack {nameD}")
for _ in range(10):
    if "slain" in out.lower() or "defeated" in out.lower():
        break
    out += recv_all(sC, 1.5)
check(xp_of(nameD) == 4240, "PvP death loses 1/10th what the same banked XP would lose in PvE (4326 -> 4240)")

sA.close(); sB.close(); sC.close(); sD.close(); si.close()
announce_done("smoke_test_death_xploss")
print("=== ALL CHECKS PASSED ===")
