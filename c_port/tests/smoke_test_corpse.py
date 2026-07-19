#!/usr/bin/env python3
"""Smoke test for corpses on death (Session 43, user: "make it so the
corpse of a char loads into the room upon death. the corpse should be
treated like a container and all inventory can be taken off said corpse
... mobs and players alike"). Covers:
  1. A PC's death drops a lootable "corpse of <name>" container in the
     room (not loose items) -- everything they were carrying/holding ends
     up INSIDE it, retrievable with `get <item> corpse`.
  2. The corpse itself can't be picked up as a whole (`get corpse` alone
     is refused) -- only its contents are takeable.
  3. A mob's death ALSO leaves a corpse (even though a mob currently
     carries nothing, so it's empty) -- scope explicitly includes mobs,
     unlike the PC-only crit-hit/death-taunt features earlier this session.

    python3 tests/smoke_test_corpse.py [host] [port]
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


announce("smoke_test_corpse")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)
ITEM = ROOM + 1
MOB = ROOM + 2
ITEM2 = ROOM + 3


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


imm_name = f"Corptest{_suffix}"
imm_pw = "corptestpw123"
victim_name = f"Corpvic{_suffix}"
victim_pw = "corpvicpw123"
item_name = f"trinket{_suffix}"
item2_name = f"pouch{_suffix}"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Corpse Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Corpse Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({ITEM},'{item_name}','a {item_name}','A {item_name} lies here.',12,1,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({ITEM2},'{item2_name}','a {item2_name}','A {item2_name} lies here.',12,1,1);")

sv = socket.create_connection((host, port), timeout=5)
make_char(sv, victim_name, victim_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{victim_name}';")
cmd(sv, "quit!")
sv.close()
sv = login(victim_name, victim_pw)
check("Corpse Sandbox" in cmd(sv, "look"), "the victim lands directly in the sandbox room")

check("You conjure" in cmd(s, f"load obj {ITEM}"), "the immortal loads a fixture item into the room")
out = cmd(sv, f"get {item_name}")
check("You get" in out, "the victim picks up the fixture item")
check("You conjure" in cmd(s, f"load obj {ITEM2}"), "the immortal loads a second fixture item into the room")
out = cmd(sv, f"get {item2_name}")
check("You get" in out, "the victim picks up the second fixture item")

# --- 1/2: a PC's death drops a lootable corpse, not loose items ---
out = cmd(s, f"kill {victim_name}")
check("slain" in out.lower(), "the immortal instakills the victim")

outRoom = cmd(s, "look")
check(f"The corpse of {victim_name} lies here." in outRoom, "the victim's corpse drops in the room")
check(f"a {item_name} is here" not in outRoom.lower() and f"a {item_name} lying here" not in outRoom.lower(),
      "the fixture item is NOT loose on the floor -- it's inside the corpse")

out = cmd(s, "get corpse")
check("can't take that" in out.lower(), "the corpse itself can't be picked up as a whole")

out = cmd(s, f"get {item_name} corpse")
check("you get" in out.lower() and "corpse" in out.lower(),
      "the fixture item IS retrievable out of the corpse with `get <item> corpse`")

# --- get all <corpse> (user, 2026-07-11: "corpses are supposed to act
# like containers. get all corpse should get all items the player/mob
# was carrying upon death") -- the second item is still in the corpse. ---
out = cmd(s, "get all corpse")
check(item2_name in out and "corpse" in out.lower(),
      "`get all corpse` sweeps up the remaining item in one go")
out = cmd(s, "inventory")
check(item2_name in out, "the swept-up item lands in the immortal's inventory")

out = cmd(s, "get all corpse")
check("nothing" in out.lower(), "a second `get all corpse` finds the now-empty corpse")

# --- 3: a mob's death ALSO leaves a corpse (mobs and players alike) ---
sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
    f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
    f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
    f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
    f"VALUES ({MOB},'corpsedummy{_suffix}','a corpse test dummy','A corpse test dummy stands here.',"
    f"'A stuffed practice dummy.',0,0,0,0,'A',1.0,0,1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
    f"10,10,1,0,0,0,1,1);")
check("You conjure" in cmd(s, f"load mob {MOB}"), "a fresh mob dummy is loaded for the mob-corpse check")
out = cmd(s, f"kill corpsedummy{_suffix}")
check("slain" in out.lower(), "the immortal instakills the mob")

outRoom = cmd(s, "look")
check("The corpse of a corpse test dummy lies here." in outRoom, "the mob's death ALSO leaves a corpse")

s.close()
sv.close()
announce_done("smoke_test_corpse")
print("=== ALL CHECKS PASSED ===")
