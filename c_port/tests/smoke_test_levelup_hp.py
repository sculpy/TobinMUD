#!/usr/bin/env python3
"""Smoke test for the level-up max-HP/limb-HP fix (found 2026-07-12
while testing weapon depth): `progress_add_xp()` used to only bump
`level` -- it never recomputed `max_hp` or healed limbs up to match,
so a leveled-up character stayed exactly as fragile as a level-1 one
forever. Fixed in combat.c's combat_defeat(): on `levels_gained > 0`,
recompute max_hp (being_calc_max_hp()) and fully heal (being_limbs_
full_heal()) the winner. Covers:

  1. A fresh mortal character's starting max_hp is recorded.
  2. Killing enough very weak mobs to level up from 1 to 2 raises
     max_hp above the recorded starting value.

    python3 tests/smoke_test_levelup_hp.py [host] [port]
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


announce("smoke_test_levelup_hp")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time() * 1000) % 70000)
MOB_BASE = ROOM + 10


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


def set_dex(name, dex):
    sql(f"UPDATE player_attrs SET dexterity={dex} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, class_choice); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    cmd(s, "color off")
    return s


def make_weak_mob(vnum, keyword):
    # hpbonus=0, level=1 -> max_hp = 20 + level*5 = 25, dies in a hit
    # or two so leveling up (8 kills at 50xp each = 400xp for level 2)
    # doesn't take long or expose the PC to much counter-attack risk.
    cols = {
        "vnum": vnum, "name": f"'{keyword}'", "short_desc": f"'a {keyword}'",
        "long_desc": f"'A {keyword} stands here.'", "description": "'desc'",
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


def max_hp_from_score(out):
    m = re.search(r"HP:\s*(\d+) \((\d+) Max", out)
    check(m is not None, "score shows an HP: current (max Max.) pair")
    return int(m.group(2))


pw = "levelupmoghp123"

imm_name = f"Lvlimm{_suffix}"
imm_pw = "lvlimmpw123"
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, "y"); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "new"); recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
send_line(s_imm, "done"); recv_all(s_imm)
send_line(s_imm, "done"); recv_all(s_imm)
sql(f"UPDATE player_progress SET level=51 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
s_imm.close()
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Levelup Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Levelup Sandbox" in cmd(s_imm, f"goto {ROOM}"), "goto lands in the sandbox room")

warrior_name = f"Lvlwar{_suffix}"
sw = make_char(warrior_name, pw, "3")
set_dex(warrior_name, 900)  # high hit chance, minimizes real-combat round count/exposure
sw.close()
sw = socket.create_connection((host, port), timeout=5)
recv_all(sw)
send_line(sw, warrior_name); recv_all(sw)
send_line(sw, pw); recv_all(sw)
send_line(sw, "1"); recv_all(sw)
cmd(sw, "color off")

# `transfer` (not a load_room SQL edit) pulls the Warrior into the
# immortal's own sandbox room -- more reliable than relying on the
# player's persisted load_room taking effect on the next login.
out = cmd(s_imm, f"transfer {warrior_name}")
check("Levelup Sandbox" in cmd(sw, "look"), "the Warrior is in the sandbox room after transfer")

out = cmd(sw, "score")
start_max_hp = max_hp_from_score(out)
check(start_max_hp > 0, f"recorded the fresh Warrior's starting max HP ({start_max_hp})")

mob_word = f"weakling{_suffix}"
kills = 0
needed_xp = 400  # progress_xp_for_level(2) = 2*2*100
xp_per_kill = 50  # loser level (1) * 50
kills_needed = -(-needed_xp // xp_per_kill)  # ceil division -> 8

for i in range(kills_needed):
    vnum = MOB_BASE + i
    make_weak_mob(vnum, mob_word)
    check("You conjure" in cmd(s_imm, f"load mob {vnum}"), f"weakling #{i+1} is loaded")
    # `kill` (like `hit`/`attack`) only INITIATES a fight -- the actual
    # strikes resolve asynchronously, once every COMBAT_ROUND_PULSES
    # (1.2s), via the server's own pulse scheduler. Issue it ONCE, then
    # just listen for the round-by-round messages rather than
    # re-sending `kill` (which mostly just re-attacks/resets the wait
    # without producing an immediate strike of its own).
    out = cmd(sw, f"kill {mob_word}")
    tries = 0
    while "You have slain" not in out and "You have defeated" not in out and tries < 20:
        tries += 1
        out += recv_all(sw, 1.5)
    check("You have slain" in out or "You have defeated" in out,
          f"the Warrior killed weakling #{i+1}")
    kills += 1

out = cmd(sw, "score")
end_max_hp = max_hp_from_score(out)
check(end_max_hp > start_max_hp,
      f"leveling up raised max HP ({start_max_hp} -> {end_max_hp})")

s_imm.close()
sw.close()
announce_done("smoke_test_levelup_hp")
print("=== ALL CHECKS PASSED ===")
