#!/usr/bin/env python3
"""Smoke test for the immortal-only carried-inventory view in `look
<target>` (cmd_look.c, user 2026-07-19: "immortals can see inventory
when looking at a mob or player and can also see the contents of any
container they carry").

Covers:
  1. A mortal looking at another PC sees the equipment listing but NOT a
     carried-inventory section (unchanged mortal behavior).
  2. An immortal looking at that same PC sees "<name> is carrying:" with
     their loose items, and one level into an open container's contents.
  3. A closed carried container shows "(closed)" instead of its contents.
  4. An immortal looking at THEMSELVES sees "You are carrying:".

    python3 tests/smoke_test_look_inventory.py [host] [port]
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


announce("smoke_test_look_inventory")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 960000 + (int(time.time()) % 20000)
ROOM = BASE
GEM = BASE + 1       # loose trinket
CAP = BASE + 2        # worn item -- should NOT show up in the carrying section
BAG = BASE + 3         # carried, open container
TRINKET = BASE + 4      # sits inside the open bag
CHEST = BASE + 5         # carried, closed container

WEAR_TAKE = 1
WEAR_BODY = 8
TYPE_TRINKET = 5
TYPE_BAG = 27
TYPE_CHEST = 15
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


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def sql(stmt):
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


def obj_insert(vnum, name, short_desc, long_desc, item_type, wear_flag,
               val0=0, val1=0, weight=0):
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,"
        f"val0,val1,weight,can_be_seen) VALUES ({vnum},'{name}','{short_desc}',"
        f"'{long_desc}',{item_type},{wear_flag},{val0},{val1},{weight},1);")


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


sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Look Inventory Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
obj_insert(GEM, "gem", "a shiny gem", "A shiny gem is lying here.", TYPE_TRINKET, WEAR_TAKE, weight=1)
obj_insert(CAP, "cap", "a leather cap", "A leather cap is lying here.", TYPE_TRINKET, WEAR_TAKE | WEAR_BODY, weight=1)
obj_insert(BAG, "bag", "a small bag", "A small bag is lying here.", TYPE_BAG, WEAR_TAKE, val0=10, val1=0, weight=1)
obj_insert(TRINKET, "trinket", "a tiny trinket", "A tiny trinket is lying here.", TYPE_TRINKET, WEAR_TAKE, weight=1)
obj_insert(CHEST, "strongbox", "a locked strongbox", "A locked strongbox is lying here.",
           TYPE_CHEST, WEAR_TAKE, val0=10, val1=CONT_CLOSEABLE | CONT_CLOSED, weight=5)

imm_name, imm_pw = f"Lkinvimm{_suffix}", "lkinvimmpw1"
si = make_char(imm_name, imm_pw)
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)
cmd(si, f"goto {ROOM}")

nameA, pwA = f"Lkinva{_suffix}", "lkinvapw123"
sA = make_char(nameA, pwA)
cmd(sA, "quit!"); sA.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{nameA}';")
sA = relog(nameA, pwA)

nameB, pwB = f"Lkinvb{_suffix}", "lkinvbpw123"
sB = make_char(nameB, pwB)
cmd(sB, "quit!"); sB.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{nameB}';")
sB = relog(nameB, pwB)

# --- A picks up a loose gem, wears a cap, carries an open bag with a
# trinket inside, and carries a closed strongbox. ---
cmd(si, f"load obj {GEM}")
cmd(sA, "get gem")
cmd(si, f"load obj {CAP}")
cmd(sA, "get cap")
cmd(sA, "wear cap")
cmd(si, f"load obj {BAG}")
cmd(sA, "get bag")
cmd(si, f"load obj {TRINKET}")
cmd(sA, "get trinket")
cmd(sA, "put trinket bag")
cmd(si, f"load obj {CHEST}")
cmd(sA, "get strongbox")

# --- 1: a mortal looking at A sees equipment but no carrying section ---
out = strip(cmd(sB, f"look {nameA}"))
check("is using" in out.lower() and "leather cap" in out.lower(),
      "a mortal viewer sees A's equipment")
check("is carrying" not in out.lower(),
      "a mortal viewer does NOT see A's carried inventory")

# --- 2: an immortal looking at A sees the carrying section ---
out = strip(cmd(si, f"look {nameA}"))
check(f"{nameA} is carrying" in out or f"{nameA.lower()} is carrying" in out.lower(),
      "an immortal viewer sees A's carrying header")
check("shiny gem" in out.lower(), "the loose gem shows up in the carrying list")
check("small bag" in out.lower(), "the open bag itself shows up in the carrying list")
check("tiny trinket" in out.lower(), "the bag's own contents show up one level deep")
check("leather cap" not in out.lower().split("is carrying")[1],
      "the worn cap is NOT duplicated into the carrying section")

# --- 3: a closed carried container shows (closed), not its contents ---
check("locked strongbox" in out.lower(), "the closed strongbox itself shows up in the carrying list")
check("(closed)" in out, "a closed carried container shows (closed) instead of contents")

# --- 4: an immortal looking at themselves (by their own name -- look_at_
# target() matches any PC/mob name in the room, self included) ---
cmd(si, f"load obj {GEM}")
cmd(si, "get gem")
out = strip(cmd(si, f"look {imm_name}"))
check("You are carrying" in out, "an immortal looking at themselves sees 'You are carrying'")

cmd(sA, "quit!"); sA.close()
cmd(sB, "quit!"); sB.close()
si.close()
announce_done("smoke_test_look_inventory")
print("=== ALL CHECKS PASSED ===")
