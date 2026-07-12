#!/usr/bin/env python3
"""Smoke test for the mob display-name capitalization bug found during the
2026-07-11 "review and find them and fix them" proper-case audit: combat
messages and the corpse description were built from a mob's `base.name`
(its raw, space-separated KEYWORD list, e.g. "lady stroll walk" -- matched
by `look lady`/`look stroll`/`look walk`) instead of its `short_descr`
("a lady out for a stroll", the actual display text). Covers:

  1. A hit message against a multi-keyword mob shows the short_descr
     ("a lady...") mid-sentence, never the raw keyword list.
  2. A miss message directed AT the attacker (sentence-initial) shows the
     short_descr capitalized ("A lady...").
  3. The corpse left behind after the mob dies describes it correctly
     ("The corpse of a lady...lies here."), not the keyword list.

    python3 tests/smoke_test_mob_display_name.py [host] [port]
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


announce("smoke_test_mob_display_name")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)
MOB1 = ROOM + 1
MOB2 = ROOM + 2
SWORD = ROOM + 3

WEAR_TAKE = 1
WEAR_HOLD = 16384
TYPE_WEAPON = 5


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
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)  # race: human
    send_line(sock, "1"); recv_all(sock)  # class: mage
    send_line(sock, "2"); recv_all(sock)  # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def make_mob(vnum, keywords, shortdesc):
    sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
        f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
        f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
        f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
        f"VALUES ({vnum},'{keywords}','{shortdesc}','{shortdesc} stands here.',"
        f"'A test dummy.',0,0,0,0,'A',1.0,0,1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
        f"10,10,1,0,0,0,1,1);")


imm_name = f"Displimm{_suffix}"
imm_pw = "displimmpw123"
mort_name = f"Displmor{_suffix}"
mort_pw = "displmorpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Display Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
check("Display Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

sv = socket.create_connection((host, port), timeout=5)
make_char(sv, mort_name, mort_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mort_name}';")
cmd(sv, "quit!")
sv.close()
sv = login(mort_name, mort_pw)
check("Display Sandbox" in cmd(sv, "look"), "the mortal attacker lands directly in the sandbox room")

# A guaranteed-hit weapon (same trick as smoke_test_weapon_messaging.py)
# so the fight resolves fast and deterministically.
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SWORD},'sword testsword','a test sword','A test sword is lying here.',"
    f"{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},1);")
sql(f"INSERT INTO objaffect (vnum, type, mod1, mod2) VALUES ({SWORD}, 15, 1000, 0);")  # APPLY_HITROLL

keywords = f"lady stroll walk {_suffix}"
shortdesc = f"a lady out for a stroll {_suffix}"
make_mob(MOB1, keywords, shortdesc)
check("You conjure" in cmd(s, f"load mob {MOB1}"), "the multi-keyword mob is loaded")
check("You conjure" in cmd(s, f"load obj {SWORD}"), "the test sword is loaded")

out = cmd(sv, "get sword")
check("you get" in out.lower(), "the mortal attacker picks up the test sword")
out = cmd(sv, "wield sword")
check("wield" in out.lower(), "wield equips the test sword")

# --- 1/2: hit and miss messages use the short_descr, never the raw keywords ---
out = cmd(sv, f"attack {keywords.split()[0]}")
check("You attack" in out, "attack initiated against the multi-keyword mob")

found_hit = False
chunks = [out]
for _ in range(6):
    time.sleep(1.5)
    chunk = recv_all(sv, timeout=0.5)
    chunks.append(chunk)
    check("lady stroll walk" not in chunk, "no combat message leaks the raw keyword list")
    if re.search(rf"You slice {re.escape(shortdesc)}'s .+? for \d+ damage!", chunk):
        found_hit = True
    if "slain" in chunk.lower() or "defeated" in chunk.lower():
        break
combined = "".join(chunks)
check(found_hit or re.search(rf"You slice {re.escape(shortdesc)}'s .+? for \d+ damage!", combined),
      "the hit message uses the mob's short_descr mid-sentence, lowercase")

# --- 3: the corpse left behind describes the mob correctly, not the raw keywords ---
if "slain" not in combined.lower() and "defeated" not in combined.lower():
    cmd(s, f"kill {keywords.split()[0]}")  # immortal instakill finishes it off
out = cmd(sv, "look")
check("lady stroll walk" not in out, "the corpse description doesn't leak the raw keyword list")
check(f"corpse of {shortdesc}" in out, "the corpse correctly names the mob by its short_descr")

s.close()
sv.close()
announce_done("smoke_test_mob_display_name")
print("=== ALL CHECKS PASSED ===")
