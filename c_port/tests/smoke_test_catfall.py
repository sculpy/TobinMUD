#!/usr/bin/env python3
"""Smoke test for catfall/catleap (spell/skill functional-completeness
audit continued, Monk level 1). See fall.c/cmd_catleap.c's own header
comments for the real-upstream research (checkFalling()/doLeap(), the
latter found only in the fuller peel-sneezymud reference clone, not the
originally-bundled sneezymud-master/) and scope-down rationale.

  1. Moving into an open-air (ATMOSPHERE) sector with a DIR_DOWN chain
     falls through it, landing in the solid room at the bottom.
  2. catfall measurably reduces cumulative fall damage over several
     falls (statistical, low DEX to make the agility save fail most of
     the time so damage reliably registers each attempt).
  3. catleap: refuses while fighting, refuses standing on open air,
     refuses an invalid direction, and a successful leap actually moves
     you (checked via `look`).

    python3 tests/smoke_test_catfall.py [host] [port]
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


announce("smoke_test_catfall")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

CLASS_MONK = 5
SECTOR_CITY = 18
SECTOR_ATMOSPHERE = 31


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


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def hp_from_score(out):
    m = re.search(r"HP:\s+(-?\d+)", out)
    return int(m.group(1)) if m else None


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)
    send_line(s, "1"); recv_all(s)
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


def make_single(prefix, room=None):
    name, pw = f"{prefix}{_suffix}", f"{prefix}pw12345"
    s0 = make_char(name, pw)
    cmd(s0, "quit!"); s0.close()
    sql(f"UPDATE player SET class={CLASS_MONK} WHERE name='{name}';")
    sql(f"UPDATE player_progress SET basic_disc_pct=100, combat_disc_pct=100, "
        f"hp=5000, max_hp=5000, vit=5000, max_vit=5000 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")
    sql(f"UPDATE player_attrs SET dexterity=70 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")
    if room is not None:
        sql(f"UPDATE player SET load_room={room} WHERE name='{name}';")
    s = relog(name, pw)
    return name, s


def seed_proficiency(name, skill_name, pct):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{name}'), '{skill_name}', {pct}, {int(time.time())}) "
        f"ON DUPLICATE KEY UPDATE pct={pct};")


ROOM_TOP = 975000 + (int(time.time()) % 10000)
ROOM_B = ROOM_TOP + 1
ROOM_C = ROOM_TOP + 2
ROOM_D = ROOM_TOP + 3
ROOM_ATMO = ROOM_TOP + 4  # standalone atmosphere room, no floor at all -- catleap refusal check

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) VALUES "
    f"({ROOM_TOP},0,0,0,'Cliff Top','Solid ground at the edge of a cliff.\\n',NULL,1,{SECTOR_CITY},0,0,0,0,0,0,0,0),"
    f"({ROOM_B},0,0,-1,'Open Sky (Upper)','Open air, falling.\\n',NULL,1,{SECTOR_ATMOSPHERE},0,0,0,0,0,0,0,0),"
    f"({ROOM_C},0,0,-2,'Open Sky (Lower)','Open air, still falling.\\n',NULL,1,{SECTOR_ATMOSPHERE},0,0,0,0,0,0,0,0),"
    f"({ROOM_D},0,0,-3,'Canyon Floor','Solid ground at the bottom of the canyon.\\n',NULL,1,{SECTOR_CITY},0,0,0,0,0,0,0,0),"
    f"({ROOM_ATMO},1,0,0,'Open Ledge','A ledge with nothing but open sky underfoot.\\n',NULL,1,{SECTOR_ATMOSPHERE},0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({ROOM_TOP},5,'','',0,0,0,0,0,{ROOM_B}),"   # down
    f"({ROOM_B},5,'','',0,0,0,0,0,{ROOM_C}),"     # down
    f"({ROOM_C},5,'','',0,0,0,0,0,{ROOM_D}),"     # down
    f"({ROOM_D},0,'','',0,0,0,0,0,{ROOM_TOP}),"   # north -- direct way back, skips the fall chain
    f"({ROOM_TOP},1,'','',0,0,0,0,0,{ROOM_ATMO}),"   # east
    f"({ROOM_ATMO},3,'','',0,0,0,0,0,{ROOM_TOP});")  # west -- return path

sockets = []
try:
    # --- 1: falling through 2 atmosphere rooms lands on solid ground ---
    nameA, sA = make_single("Fal", room=ROOM_TOP)
    sockets.append(sA)
    recv_all(sA)
    out = strip(cmd(sA, "down", 1.5))
    check("plunge" in out.lower() or "world spins" in out.lower(), "falling produces the real fall-start message")
    look_out = strip(cmd(sA, "look"))
    check("Canyon Floor" in look_out, "falling through 2 atmosphere rooms lands in the solid room at the bottom")
    sA.close()

    # --- 2: catfall measurably reduces cumulative fall damage ---
    # 10 wasn't enough to reliably separate the two groups against
    # background HP regen noise (verified live 2026-07-27: a 10-attempt
    # run came back 289 vs 279, backwards, despite the mechanic itself
    # working correctly attempt-by-attempt) -- 25 gives the real ~2x
    # per-attempt effect (halved damage) more room to show up in the sum.
    ATTEMPTS = 25

    nameB, sB = make_single("Falc", room=ROOM_TOP)  # control, no catfall
    sockets.append(sB)
    recv_all(sB)
    before_b = hp_from_score(cmd(sB, "score"))
    for _ in range(ATTEMPTS):
        cmd(sB, "down", 1.5)
        cmd(sB, "north", 1.0)  # direct path back to ROOM_TOP, skips the atmosphere chain
    after_b = hp_from_score(cmd(sB, "score"))
    control_loss = before_b - after_b
    sB.close()

    nameC, sC = make_single("Falw", room=ROOM_TOP)  # catfall 100%
    sockets.append(sC)
    seed_proficiency(nameC, "catfall", 100)
    recv_all(sC)
    before_c = hp_from_score(cmd(sC, "score"))
    for _ in range(ATTEMPTS):
        cmd(sC, "down", 1.5)
        cmd(sC, "north", 1.0)
    after_c = hp_from_score(cmd(sC, "score"))
    catfall_loss = before_c - after_c
    sC.close()

    check(catfall_loss < control_loss,
          f"100%-catfall takes less cumulative fall damage over {ATTEMPTS} falls than a "
          f"0%-catfall control ({catfall_loss} vs {control_loss})")

    # --- 3: catleap ---
    nameD, sD = make_single("Ctlp", room=ROOM_TOP)
    sockets.append(sD)
    seed_proficiency(nameD, "catleap", 100)
    recv_all(sD)

    out_baddir = strip(cmd(sD, "catleap sideways"))
    check("can't go that way" in out_baddir.lower(), "catleap refuses an invalid direction")

    cmd(sD, "east")  # walk onto the open ledge normally first
    time.sleep(0.3)
    out_leap_from_atmo = strip(cmd(sD, "catleap west"))
    check("no ground beneath you" in out_leap_from_atmo.lower(),
          "catleap refuses to launch from a room with no floor")
    cmd(sD, "west"); recv_all(sD, 0.3)  # back to solid ground

    out_leap = strip(cmd(sD, "catleap east", 1.5))
    check("leap into the air" in out_leap.lower(), "a successful catleap launches")
    look_after_leap = strip(cmd(sD, "look"))
    check("Open Ledge" in look_after_leap, "a successful catleap actually completes the move")
    cmd(sD, "west"); recv_all(sD, 0.3)

    cmd(sD, "toggle pk")
    nameE, sE = make_single("Ctlpo", room=ROOM_TOP)
    sockets.append(sE)
    cmd(sE, "toggle pk")
    cmd(sD, f"attack {nameE}")
    time.sleep(1.3)
    out_fighting = strip(cmd(sD, "catleap east"))
    check("can't leap away while fighting" in out_fighting.lower(),
          "catleap refuses while fighting")
    sD.close(); sE.close()

    sockets = []
    announce_done("smoke_test_catfall")
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    for prefix in ("Fal", "Falc", "Falw", "Ctlp", "Ctlpo"):
        sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player WHERE name LIKE '{prefix}%{_suffix}';")
    sql(f"DELETE FROM roomexit WHERE vnum IN ({ROOM_TOP}, {ROOM_B}, {ROOM_C}, {ROOM_D}, {ROOM_ATMO});")
    sql(f"DELETE FROM room WHERE vnum IN ({ROOM_TOP}, {ROOM_B}, {ROOM_C}, {ROOM_D}, {ROOM_ATMO});")
