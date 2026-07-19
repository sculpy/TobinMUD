#!/usr/bin/env python3
"""Smoke test for `purge` (user, 2026-07-09/2026-07-10: "add a purge
command that is 51+ that will purge the contents of a room, add a
linkdead argument that a 58+ god can purge the game of all linkdead
characters"). Scoped down from the original SneezyMUD's full purge (see
lib/help/_immortal/purge in the bundled reference tree) to just the two
requested forms:

  1. Bare `purge` (51+): clears the current room's mobs and objects, but
     never a PC standing in it.
  2. `purge linkdead` is refused below level 58, even for a 51+ builder
     who can do the bare form.
  3. `purge linkdead` (58+): force-removes every linkdead PC in the game,
     but leaves a live (still-connected) character in the very same room
     alone.

    python3 tests/smoke_test_purge.py [host] [port]
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


announce("smoke_test_purge")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)
MOB = ROOM + 1
OBJ = ROOM + 2

WEAR_TAKE = 1


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
    send_line(sock, "1"); recv_all(sock)  # race: human (zero stat modifier)
    send_line(sock, "1"); recv_all(sock)  # class: mage
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "2"); recv_all(sock)  # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


imm_name = f"Purgeimm{_suffix}"
imm_pw = "purgeimmpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
s.close()
set_level(imm_name, 51)
s = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Purge Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Purge Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

# --- 1: bare purge (51+) clears mobs/objects, but never a PC in the room ---
sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
    f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
    f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
    f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
    f"VALUES ({MOB},'purgedummy{_suffix}','a purge test dummy','A purge test dummy stands here.',"
    f"'A stuffed practice dummy.',0,0,0,0,'A',1.0,0,1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
    f"10,10,1,0,0,0,1,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({OBJ},'trinket','a small trinket','A small trinket is lying here.',12,{WEAR_TAKE},1);")

check("You conjure" in cmd(s, f"load mob {MOB}"), "the test dummy mob is loaded")
check("You conjure" in cmd(s, f"load obj {OBJ}"), "the test trinket object is loaded")
out = cmd(s, "look")
check(f"purge test dummy" in out.lower(), "the dummy is visible before purging")
check("trinket is lying here" in out.lower(), "the trinket is visible before purging")

out = cmd(s, "purge")
check("2 thing" in out, "purge reports removing both the mob and the object")

out = cmd(s, "look")
check("purge test dummy" not in out.lower(), "the dummy is gone after a bare purge")
check("trinket" not in out.lower(), "the trinket is gone after a bare purge")
check("Purge Sandbox" in out, "the room itself (and the immortal in it) survives the purge")

# --- 2: purge linkdead is refused below level 58, even for this 51+ builder ---
out = cmd(s, "purge linkdead")
check("Command not found" in out, "purge linkdead is refused for a level-51 builder (needs 58+)")

# --- 3: promote to 58, put a linkdead AND a live character in the room,
# confirm only the linkdead one is removed ---
cmd(s, "quit!")
s.close()
set_level(imm_name, 58)
s = login(imm_name, imm_pw)
check("Purge Sandbox" in cmd(s, f"goto {ROOM}"), "re-enter the sandbox room as a level-58 immortal")

victim_name = f"Purgevic{_suffix}"
victim_pw = "purgevicpw123"
sv = socket.create_connection((host, port), timeout=5)
make_char(sv, victim_name, victim_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{victim_name}';")
cmd(sv, "quit!")
sv.close()
sv = login(victim_name, victim_pw)
check("Purge Sandbox" in cmd(sv, "look"), "the victim lands directly in the sandbox room")

live_name = f"Purgelive{_suffix}"
live_pw = "purgelivepw123"
sl = socket.create_connection((host, port), timeout=5)
make_char(sl, live_name, live_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{live_name}';")
cmd(sl, "quit!")
sl.close()
sl = login(live_name, live_pw)
check("Purge Sandbox" in cmd(sl, "look"), "the live character also lands in the sandbox room")

out = cmd(s, "look")
check(victim_name.lower() in out.lower(), "the victim is visible before going linkdead")
check(live_name.lower() in out.lower(), "the live character is visible too")

sv.close()  # abrupt close, not quit! -- leaves the victim linkdead in place
time.sleep(0.5)

out = cmd(s, "purge linkdead")
check("Purged" in out, "purge linkdead (58+) succeeds")
import re as _re
m = _re.search(r"Purged (\d+) linkdead", out)
check(m is not None and int(m.group(1)) >= 1, "purge linkdead reports removing at least the one linkdead victim")

out = cmd(s, "look")
check(victim_name.lower() not in out.lower(), "the linkdead victim is gone after purge linkdead")
check(live_name.lower() in out.lower(), "the still-connected live character is untouched by purge linkdead")

s.close()
sl.close()
announce_done("smoke_test_purge")
print("=== ALL CHECKS PASSED ===")
