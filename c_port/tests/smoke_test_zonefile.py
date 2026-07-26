#!/usr/bin/env python3
"""Smoke test for `zonefile create <zone>` (zone.c's zone_file_create()):
snapshots a zone's CURRENT live mobs/objects into new zone_reset rows.

  1. A mob and a ground object sitting in the zone's room each get a new
     'M'/'O' reset row after `zonefile create`.
  2. An item placed inside a ground container gets a 'P' row right after
     the container's 'O' row.
  3. Re-running `zonefile create` with nothing changed adds NO new rows
     (already covered -- idempotent).
  4. Deleting the mob's 'M' row and re-running only fills that one back in
     -- the untouched container's 'O'/'P' rows are NOT duplicated.

All setup is SQL-bootstrapped (zone + room + mob/obj prototypes) at high
vnums/zone numbers (900000+), well clear of the real seeded zones
(max real zone_nr ~850555); the seeded world is never touched.

    python3 tests/smoke_test_zonefile.py [host] [port]
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


announce("smoke_test_zonefile")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 900000 + (int(time.time()) % 80000)

ZONE_NR = BASE
ROOM = BASE
MOB = BASE + 1
CHEST = BASE + 2
ITEM = BASE + 3

WEAR_TAKE = 1
TYPE_CHEST = 15  # ITEM_CHEST -> OBJ_CAT_CONTAINER
TYPE_TRINKET = 5  # plain takeable, never a container
CONT_CLOSEABLE = 1 << 0


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
                           check=True, capture_output=True, text=True).stdout


def reset_row_count(zone_nr, command, room_vnum, item_vnum):
    out = query(f"SELECT COUNT(*) FROM zone_reset WHERE zone_nr={zone_nr} AND "
                f"command='{command}' AND arg3={room_vnum} AND arg1={item_vnum};")
    return int(out.strip())


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
    send_line(sock, "done"); recv_all(sock)  # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


imm_name = f"Zfiletest{_suffix}"
imm_pw = "zfiletestpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 55)  # 55+ -> zone_can_edit() unconditional, no zone_owner row needed
s.close()
s = login(imm_name, imm_pw)

# --- bootstrap: a single-room zone + mob/chest/item prototypes ---
sql(f"INSERT INTO zone (zone_nr, zone_name, zone_enabled, bottom, top, lifespan) "
    f"VALUES ({ZONE_NR}, 'Zonefile Test', 0, {ROOM}, {ROOM}, 30);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Zonefile Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
    f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
    f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
    f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
    f"VALUES ({MOB},'zonefile dummy','a zonefile test dummy','A test dummy stands here.',"
    f"'desc',0,0,0,0,'A',1.0,0,1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,10,10,1,0,0,0,1,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,val0,val1,weight,can_be_seen) "
    f"VALUES ({CHEST},'chest','a test chest','A test chest sits here.',{TYPE_CHEST},{WEAR_TAKE},"
    f"100,{CONT_CLOSEABLE},40,1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,weight,can_be_seen) "
    f"VALUES ({ITEM},'trinket','a small trinket','A small trinket is lying here.',"
    f"{TYPE_TRINKET},{WEAR_TAKE},1,1);")

cmd(s, f"goto {ROOM}")
cmd(s, f"load mob {MOB}")
cmd(s, f"load obj {CHEST}")
cmd(s, f"load obj {ITEM}")
cmd(s, "get trinket")
cmd(s, "put trinket chest")

# --- 1/2: first zonefile create -- mob, chest, and the item inside it.
# The trinket is INSIDE the chest, not a direct room child, so it earns a
# 'P' row (child of the chest's 'O' row), not a top-level 'O' of its own --
# only 1 new object load total (the chest), not 2. ---
out = cmd(s, f"zonefile create {ZONE_NR}", timeout=2.0)
check("1 new mob load" in out and "2 new mob load" not in out,
      "zonefile create reports exactly 1 new mob load")
check("1 new object load" in out and "2 new object load" not in out,
      "zonefile create reports exactly 1 new top-level object load (the chest)")

check(reset_row_count(ZONE_NR, "M", ROOM, MOB) == 1, "an M row was added for the live mob")
check(reset_row_count(ZONE_NR, "O", ROOM, CHEST) == 1, "an O row was added for the ground chest")
p_rows = int(query(f"SELECT COUNT(*) FROM zone_reset WHERE zone_nr={ZONE_NR} "
                    f"AND command='P' AND arg1={ITEM};").strip())
check(p_rows == 1, "a P row was added for the trinket inside the chest")

# --- 3: re-running with nothing changed adds nothing new ---
total_before = int(query(f"SELECT COUNT(*) FROM zone_reset WHERE zone_nr={ZONE_NR};").strip())
out2 = cmd(s, f"zonefile create {ZONE_NR}", timeout=2.0)
check("0 new mob loads, 0 new object loads" in out2,
      "re-running with nothing new loaded reports 0/0")
total_after = int(query(f"SELECT COUNT(*) FROM zone_reset WHERE zone_nr={ZONE_NR};").strip())
check(total_before == total_after, "re-running added no duplicate rows")

# --- 4: delete the mob's M row, rerun -- only that gap gets filled back in ---
sql(f"DELETE FROM zone_reset WHERE zone_nr={ZONE_NR} AND command='M' AND arg3={ROOM} AND arg1={MOB};")
check(reset_row_count(ZONE_NR, "M", ROOM, MOB) == 0, "the mob's M row is really gone")
out3 = cmd(s, f"zonefile create {ZONE_NR}", timeout=2.0)
check("1 new mob load" in out3, "the deleted line's gap is filled back in")
check("0 new object loads" in out3, "the untouched chest/trinket rows are not duplicated")
check(reset_row_count(ZONE_NR, "M", ROOM, MOB) == 1, "exactly one M row exists again for the mob")
check(reset_row_count(ZONE_NR, "O", ROOM, CHEST) == 1, "still exactly one O row for the chest (no dupe)")

cmd(s, "quit!")
s.close()

print("=== ALL CHECKS PASSED ===")
announce_done("smoke_test_zonefile")
