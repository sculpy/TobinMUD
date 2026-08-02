#!/usr/bin/env python3
"""Smoke test for `wear all` (TODO.md priority item, user 2026-08-02).

Covers:
  1. `wear all` equips every wearable item carried, across several distinct
     body slots, in one command.
  2. A carried item with no wearable slot (a held-only weapon) is silently
     skipped -- no error spam, and it stays in inventory/unheld.
  3. `wear all` with nothing wearable to wear reports "nothing to wear"
     rather than silence.

    python3 tests/smoke_test_wear_all.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 910000 + (int(time.time()) % 60000)
HEAD_ITEM = ROOM + 1
BODY_ITEM = ROOM + 2
FEET_ITEM = ROOM + 3
SWORD_ITEM = ROOM + 4

WEAR_TAKE = 1
WEAR_BODY = 8
WEAR_HEAD = 16
WEAR_FEET = 64
WEAR_HOLD = 16384
TYPE_ARMOR = 9
TYPE_WEAPON = 5


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


announce("smoke_test_wear_all")


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
    send_line(sock, "1"); recv_all(sock)  # race: human
    send_line(sock, "1"); recv_all(sock)  # class: mage
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)  # alignment: neutral


imm_name = f"Wearimm{_suffix}"
imm_pw = "wearimmpw123"
mort_name = f"Wearmor{_suffix}"
mort_pw = "wearmorpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, imm_name); recv_all(s)
send_line(s, imm_pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Wear-All Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Wear-All Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

sv = socket.create_connection((host, port), timeout=5)
make_char(sv, mort_name, mort_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mort_name}';")
cmd(sv, "quit!")
sv.close()
sv = socket.create_connection((host, port), timeout=5)
recv_all(sv)
send_line(sv, mort_name); recv_all(sv)
send_line(sv, mort_pw); recv_all(sv)
send_line(sv, "1"); recv_all(sv)
cmd(sv, "color off")
check("Wear-All Sandbox" in cmd(sv, "look"), "the mortal lands directly in the sandbox room")

# --- nothing to wear yet ---
out = cmd(sv, "wear all")
check("nothing to wear" in out.lower(), "wear all with an empty inventory reports nothing to wear")

# --- load three wearable items (head/body/feet) plus one held-only sword ---
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,weight,val0,can_be_seen) "
    f"VALUES ({HEAD_ITEM},'iron helm','an iron helm','An iron helm is lying here.\\n',"
    f"{TYPE_ARMOR},{WEAR_TAKE | WEAR_HEAD},5,0,1),"
    f"({BODY_ITEM},'chain shirt','a chain shirt','A chain shirt is lying here.\\n',"
    f"{TYPE_ARMOR},{WEAR_TAKE | WEAR_BODY},10,0,1),"
    f"({FEET_ITEM},'leather boots','a pair of leather boots','A pair of leather boots lie here.\\n',"
    f"{TYPE_ARMOR},{WEAR_TAKE | WEAR_FEET},4,0,1),"
    f"({SWORD_ITEM},'iron sword','an iron sword','An iron sword is lying here.\\n',"
    f"{TYPE_WEAPON},{WEAR_TAKE | WEAR_HOLD},6,0,1);")

for vnum in (HEAD_ITEM, BODY_ITEM, FEET_ITEM, SWORD_ITEM):
    check("You conjure" in cmd(s, f"load obj {vnum}"), f"item {vnum} is loaded")
    cmd(s, "drop all")

for word in ("helm", "shirt", "boots", "sword"):
    out = cmd(sv, f"get {word}")
    check("you get" in out.lower(), f"the mortal picks up the {word}")

# --- wear all: three armor pieces equip, the sword (held-only) is skipped ---
out = cmd(sv, "wear all")
check("you wear" in out.lower() and "helm" in out.lower(), "wear all equips the helm")
check("shirt" in out.lower(), "wear all equips the chain shirt")
check("boots" in out.lower(), "wear all equips the boots")
check("sword" not in out.lower(), "wear all silently skips the held-only sword")

out = cmd(sv, "equipment")
check("helm" in out.lower() and "shirt" in out.lower() and "boots" in out.lower(),
      "equipment listing confirms all three pieces are actually worn")

out = cmd(sv, "inventory")
check("sword" in out.lower(), "the skipped sword is still sitting in inventory, untouched")

s.close()
sv.close()
announce_done("smoke_test_wear_all")
print("=== ALL CHECKS PASSED ===")
