#!/usr/bin/env python3
"""Smoke test for Zones Part 2 (Session 43, user: "zonefiles are not
loading? i dont see anything in rooms or mobs wandering around"). Covers:
  1. A REAL, currently-enabled seeded zone actually populated a room with
     its mob at boot -- this is the exact bug reported: the zone_reset
     data existed since Session 38 but nothing ever executed it.
  2. The `zone reset <zone>` builder command runs a zone's reset commands
     on demand: M (load mob), E (equip an item onto it), G (give it a
     carried container), and P (place an item inside that container) --
     verified indirectly by killing the mob and checking its corpse
     contains everything, reusing the already-tested corpse-on-death
     feature as the inspection tool (mobs have no `stat`/equipment-
     display command yet).
  3. D (set a door's state) -- closes and locks a sandbox exit.
  4. An unhandled opcode (Y, not in the v1 subset) doesn't break the rest
     of the zone's reset -- it's skipped, not fatal.

    python3 tests/smoke_test_zones.py [host] [port]
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


announce("smoke_test_zones")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 60000)
ROOM2 = ROOM + 1
ZONE = 90000 + (int(time.time()) % 9000)
MOB = ROOM + 2
HELMET = ROOM + 3
BAG = ROOM + 4
GEM = ROOM + 5


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


def obj_insert(vnum, name, short_desc, long_desc, item_type, wear_flag):
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
        f"VALUES ({vnum},'{name}','{short_desc}','{long_desc}',{item_type},{wear_flag},1);")


imm_name = f"Zonetest{_suffix}"
imm_pw = "zonetestpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)

# --- 1: a REAL seeded zone (0, "Void") already populated a real room at
#     boot -- room 200 ("Inside the Farm House") gets mob 210 (a city
#     gatekeeper) via a real 'M' command. This is the exact bug reported. ---
out = cmd(s, "goto 200")
check("Farm House" in out, "goto reaches the real seeded room")
out = cmd(s, "look")
check("gatekeeper" in out.lower(), "a REAL zone's 'M' command actually populated this room at boot")

# --- bootstrap a sandbox zone + room + fixtures for the rest ---
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Zone Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM2},0,0,0,'Zone Sandbox Beyond','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({ROOM},0,'','',1,0,0,0,0,{ROOM2});")  # north: a real door, currently open

sql(f"INSERT INTO zone (zone_nr,zone_name,zone_enabled,bottom,top,reset_mode,lifespan,age,util_flag) "
    f"VALUES ({ZONE},'Zone Test Sandbox',1,{ROOM},{ROOM2},2,999999,0,0);")

sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
    f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
    f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
    f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
    f"VALUES ({MOB},'zonedummy{_suffix}','a zone test dummy','A zone test dummy stands here.',"
    f"'A stuffed practice dummy.',0,0,0,0,'A',1.0,0,1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
    f"10,10,1,0,0,0,1,1);")
obj_insert(HELMET, "helmet", "a helmet", "A helmet lies here.", 9, 16)   # WEAR_HEAD
obj_insert(BAG, "bag", "a bag", "A bag lies here.", 15, 1)               # ITEM_CHEST -> container
obj_insert(GEM, "gem", "a gem", "A gem lies here.", 43, 1)               # ITEM_GEMSTONE

# M: load the dummy (if_flag=0, starts a fresh chain); E/G/P: equip the
# helmet, give the bag, put the gem in the bag -- if_flag=1 on all three,
# matching how the REAL seeded data always chains them (100% of real E/G/P
# rows use if_flag=1, confirmed against the live zone_reset table) --
# skipped if the preceding load failed, but here M succeeds so the whole
# chain should fire. Y: an UNHANDLED opcode, should be skipped harmlessly.
# D: close+lock the sandbox door (if_flag=0, independent of the mob chain).
sql(f"INSERT INTO zone_reset (zone_nr,cmd_no,command,if_flag,arg1,arg2,arg3,arg4,comment) VALUES "
    f"({ZONE},0,'M',0,{MOB},1,{ROOM},0,''),"
    f"({ZONE},1,'E',1,{HELMET},1,0,0,''),"
    f"({ZONE},2,'G',1,{BAG},1,0,0,''),"
    f"({ZONE},3,'P',1,{GEM},1,0,0,''),"
    f"({ZONE},4,'Y',0,1,1,1,1,'unhandled opcode -- should be skipped, not fatal'),"
    f"({ZONE},5,'D',0,{ROOM},0,2,0,'');")

# --- 2/3/4: force the sandbox zone's reset now ---
out = cmd(s, f"zone reset {ZONE}")
check("1 mobs, 3 objects" in out.lower(),
      "zone reset loaded exactly 1 mob (M) and 3 objects (E helmet + G bag + P gem) -- "
      "the unhandled Y opcode contributed nothing but didn't break the rest")

out = cmd(s, f"goto {ROOM}")
check("Zone Sandbox" in out, "goto reaches the sandbox room")
out = cmd(s, "look")
check("zone test dummy is here" in out.lower(), "M loaded the sandbox mob")

out = cmd(s, "north")
check("the door is closed" in out.lower(), "D closed the sandbox door -- movement is blocked")
out = cmd(s, "open north")
check("it's locked" in out.lower(), "D also locked it, not just closed (distinct from merely closed)")

# --- kill the dummy, inspect its corpse to confirm E/G/P actually worked ---
out = cmd(s, f"kill zonedummy{_suffix}")
check("slain" in out.lower(), "the immortal instakills the zone-loaded mob")

outRoom = cmd(s, "look")
check("the corpse of a zone test dummy lies here" in outRoom.lower(),
      "the mob's corpse appears (Session 43 corpse feature, reused here as an inspection tool)")

out = cmd(s, "get helmet corpse")
check("you get" in out.lower(), "E equipped the helmet on the mob -- it's in the corpse now")

out = cmd(s, "get bag corpse")
check("you get" in out.lower(), "G gave the mob a bag -- it's in the corpse now")

out = cmd(s, "get gem bag")
check("you get" in out.lower(), "P placed the gem INSIDE the bag (nesting survived the corpse transfer)")

s.close()
announce_done("smoke_test_zones")
print("=== ALL CHECKS PASSED ===")
