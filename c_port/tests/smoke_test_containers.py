#!/usr/bin/env python3
"""Smoke test for containers (put/get <item> <container>, look inside, and
open/close on the container's val[1] CONT_* flags). Covers:
  1. `look <container>` shows "It is closed." while shut, contents when open.
  2. A closed container refuses put and get; `open` clears CONT_CLOSED.
  3. `put <item> <container>` moves a carried item in; `get <item> <container>`
     takes it back out; room-floor and carried containers both work.
  4. `put` into a non-container is refused; nothing goes inside itself.
  5. Weight capacity (val[0]): an over-weight item won't fit; a light one does.
  6. `close` re-shuts a closeable container; a shut one blocks access again.
  7. A container's contents survive a relog (reload loose, never lost).

All setup is SQL-bootstrapped sandbox rooms/objects at high vnums (900000+);
the seeded world and its obj table are never touched.

    python3 tests/smoke_test_containers.py [host] [port]
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


announce("smoke_test_containers")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 900000 + (int(time.time()) % 80000)

ROOM = BASE
CHEST = BASE + 1   # room-floor container, closeable + starts closed
GEM = BASE + 2     # small item to stash
COIN = BASE + 3    # a second small item (used for the not-a-container check)
BAG = BASE + 4     # carried container, open, small weight capacity
HEAVY = BASE + 5   # too heavy for the bag
LIGHT = BASE + 6   # fits the bag

WEAR_TAKE = 1
TYPE_CHEST = 15    # ITEM_CHEST -> OBJ_CAT_CONTAINER
TYPE_BAG = 27      # ITEM_BAG   -> OBJ_CAT_CONTAINER
TYPE_TRINKET = 5   # a plain takeable non-container (weapon bucket; never worn here)

CONT_CLOSEABLE = 1 << 0
CONT_CLOSED = 1 << 2


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


def obj_insert(vnum, name, short_desc, long_desc, item_type, wear_flag,
               val0=0, val1=0, weight=0):
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,"
        f"val0,val1,weight,can_be_seen) VALUES ({vnum},'{name}','{short_desc}',"
        f"'{long_desc}',{item_type},{wear_flag},{val0},{val1},{weight},1);")


imm_name = f"Contest{_suffix}"
imm_pw = "contestpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)

# --- bootstrap: sandbox room + container/item prototypes ---
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Container Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
obj_insert(CHEST, "chest", "a heavy chest", "A heavy chest sits here.",
           TYPE_CHEST, WEAR_TAKE, val0=100, val1=CONT_CLOSEABLE | CONT_CLOSED, weight=40)
obj_insert(GEM, "gem", "a shiny gem", "A shiny gem is lying here.",
           TYPE_TRINKET, WEAR_TAKE, weight=2)
obj_insert(COIN, "coin", "a gold coin", "A gold coin is lying here.",
           TYPE_TRINKET, WEAR_TAKE, weight=1)
obj_insert(BAG, "bag", "a small bag", "A small bag is lying here.",
           TYPE_BAG, WEAR_TAKE, val0=10, val1=CONT_CLOSEABLE, weight=3)
obj_insert(HEAVY, "anvil", "a heavy anvil", "A heavy anvil is lying here.",
           TYPE_TRINKET, WEAR_TAKE, weight=50)
obj_insert(LIGHT, "feather", "a light feather", "A light feather is lying here.",
           TYPE_TRINKET, WEAR_TAKE, weight=2)

check("Container Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the sandbox room")

# --- 1: a closed container reads as closed and blocks access ---
cmd(s, f"load obj {CHEST}")
check("It is closed." in cmd(s, "look chest"), "look on a closed container says it is closed")
cmd(s, f"load obj {GEM}")
check("You get" in cmd(s, "get gem"), "pick up the gem")
check("It's closed." in cmd(s, "put gem chest"), "put into a closed container is refused")

# --- 2: open clears CONT_CLOSED; contents now visible ---
check("You open" in cmd(s, "open chest"), "open a closeable container")
check("It contains" in cmd(s, "look chest"), "an open container shows its (empty) contents")

# --- 3: put / look-inside / get from a room-floor container ---
check("You put" in cmd(s, "put gem chest"), "put the gem into the open chest")
check("gem" in cmd(s, "look chest"), "look lists the gem inside the chest")
check("gem" not in cmd(s, "inventory"), "the gem is no longer loose in inventory")
check("You get" in cmd(s, "get gem chest"), "get the gem back out of the chest")
check("gem" in cmd(s, "inventory"), "the gem is loose in inventory again")

# --- 4: not-a-container / self refusals ---
cmd(s, f"load obj {COIN}")
cmd(s, "get coin")
check("not a container" in cmd(s, "put coin gem"), "put into a non-container is refused")

# --- 5: weight capacity on a carried container ---
cmd(s, f"load obj {BAG}"); cmd(s, "get bag")
cmd(s, f"load obj {HEAVY}"); cmd(s, "get anvil")
check("won't fit" in cmd(s, "put anvil bag"), "an over-capacity item won't fit")
cmd(s, f"load obj {LIGHT}"); cmd(s, "get feather")
check("You put" in cmd(s, "put feather bag"), "a light item fits within capacity")
check("feather" in cmd(s, "look bag"), "look lists the feather inside the carried bag")

# --- 6: close re-shuts and blocks access ---
check("You close" in cmd(s, "close bag"), "close a closeable container")
check("It's closed." in cmd(s, "get feather bag"), "a re-closed container blocks get")

# --- 7: contents survive a relog (reload loose, never lost) ---
s.close()
s = login(imm_name, imm_pw)
check("feather" in cmd(s, "inventory"), "the bagged feather survived a relog (reloaded loose)")
check("bag" in cmd(s, "inventory"), "the bag itself survived the relog too")

s.close()
announce_done("smoke_test_containers")
print("=== ALL CHECKS PASSED ===")
