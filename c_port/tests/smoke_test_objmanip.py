#!/usr/bin/env python3
"""Smoke test for `junk`/`identify` (Sneezy â†’ Tobin feature audit, "Object
manipulation depth"). `sacrifice` was deliberately skipped -- user,
AskUserQuestion 2026-07-19: it's entirely a Shaman-class skill tied to a
`lifeforce` resource and totems in the original, none of which exist in
Tobin, so there was no honest way to keep the verb without inventing
machinery nothing else needs. Covers:
  1. `identify` on a real weapon shows its real hitroll/damroll bonus, not
     the object's own val[0]/val[1] (verified NOT real dice -- see
     cmd_identify.c's own comment -- so must never appear as "NdM").
  2. `identify` on real food shows its nutrition value.
  3. `junk` destroys a carried item and it's really gone (not just
     detached from the live in-memory list) -- reconnecting doesn't
     resurrect it. This is also a regression check for a real bug caught
     while building this: obj_destroy() alone doesn't update the
     PERSISTED inventory row, so a reconnect before any other
     inventory-touching action would otherwise bring a "destroyed" item
     back from the dead. `eat` (previous audit item) had the same gap;
     fixed alongside this one.

    python3 tests/smoke_test_objmanip.py [host] [port]
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


announce("smoke_test_objmanip")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 930000 + (int(time.time()) % 70000)
WEAPON_VNUM = 300  # "sword long simple" -- real seeded WEAPON
FOOD_VNUM = 405    # "steak beef marinated" -- real seeded FOOD, val0=24


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


imm_name, imm_pw = f"Omimmb{_suffix}", "omimmpw1234"
mort_name, mort_pw = f"Ommortb{_suffix}", "ommortpw1234"

si = make_char(imm_name, imm_pw); si.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
sA = make_char(mort_name, mort_pw)
cmd(sA, "quit!")
sA.close()

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Objmanip Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mort_name}';")

si = relog(imm_name, imm_pw)
sA = relog(mort_name, mort_pw)
cmd(si, f"goto {ROOM}")
cmd(si, f"load obj {WEAPON_VNUM}")
cmd(si, "drop sword")
cmd(si, f"load obj {FOOD_VNUM}")
cmd(si, "drop steak")
cmd(sA, "get sword")
cmd(sA, "get steak")

# --- 1: identify a weapon shows the real bonus, never the fake val0/val1 dice ---
out = cmd(sA, "identify sword")
check("Category:  weapon" in out, "identify shows the weapon's category")
check("7710d1586" not in out and "d1586" not in out,
      "identify never shows val0/val1 as fake damage dice (verified not real dice)")
check("Bonus:" in out, "identify shows a real Bonus: line for the weapon")

# --- 2: identify food shows its real nutrition value ---
out = cmd(sA, "identify steak")
check("Category:  food" in out, "identify shows the food's category")
check("restores 24 hunger" in out, "identify shows the steak's real val0 nutrition value (24)")

# --- 3: junk really destroys an item -- survives a reconnect, doesn't resurrect ---
out = cmd(sA, "junk steak")
check("You junk" in out, "junk confirms")
out = cmd(sA, "identify steak")
check("aren't carrying" in out, "the junked steak is immediately gone from inventory")

# A raw close (not `quit!`, which would itself force a fresh inventory
# save and mask the bug) -- reconnecting must NOT bring the junked item
# back. Regression check for the missing player_inventory_save() this
# session found and fixed in both `junk` and `eat`.
sA.close()
sA = relog(mort_name, mort_pw)
out = cmd(sA, "identify steak")
check("aren't carrying" in out, "the junked steak did not resurrect after a reconnect")

# Same regression check for `eat` (previous audit item, same missing-save
# bug, fixed alongside this one).
out = cmd(sA, "eat sword")
check("aren't carrying" in out or "You eat" not in out, "a sword can't be eaten (not FOOD-category)")
cmd(si, f"load obj {FOOD_VNUM}")
cmd(si, "drop steak")
cmd(sA, "get steak")
cmd(sA, "eat steak")
sA.close()
sA = relog(mort_name, mort_pw)
out = cmd(sA, "identify steak")
check("aren't carrying" in out, "an eaten steak did not resurrect after a reconnect either")

sA.close()
si.close()
announce_done("smoke_test_objmanip")
print("=== ALL CHECKS PASSED ===")
