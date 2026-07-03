#!/usr/bin/env python3
"""Smoke test for the room builder (`edit`, port of Sneezy's doEdit) and
the movement commands:
  1. Gate: mortals and 51s get Huh?! for `edit`; 56 works.
  2. Bare `edit` shows the room summary (name/number/sector/exits).
  3. `edit name`, `edit sector_type`, and `edit description` (line editor)
     change the live room AND persist across a re-look/DB reload.
  4. `edit exit <dir> <toroom>` auto-creates a missing target room and
     fixes the reverse exit; movement (north/south) walks the new link
     both ways; look shows "Obvious exits"; `edit exit <dir> -1` deletes.
  5. Movement letters: 'n' walks north; walking into a wall says so.

All editing happens in freshly created sandbox rooms at high vnums --
the seeded world (rooms 0/1/...) is never modified.

    python3 tests/smoke_test_redit.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
# Sandbox vnums: far above the seed's range, varied per run.
BASE = 900000 + (int(time.time()) % 90000)


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


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def sql(stmt):
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id=(SELECT id FROM player WHERE name='{name}');")


name = f"Builder{_suffix}"
pw = "reditpw"
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name)
recv_all(s)
send_line(s, pw)
recv_all(s)
send_line(s, "new")
recv_all(s)
send_line(s, name)
recv_all(s)
send_line(s, "done")
recv_all(s)

# A mortal's look shows the plain room name -- no builder header.
send_line(s, "look")
out = recv_all(s)
check("Imperia" in out and "[1]" not in out and "[sector" not in out,
      "a mortal's look has no vnum/sector/flags header")

# --- Part 1: the gate (51+ as of the Tier-3 follow-up) ---
send_line(s, "redit")
check("Huh?!" in recv_all(s), "a mortal typing redit gets Huh?! (hidden)")

set_level(name, 51)
s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name)
recv_all(s)
send_line(s, pw)
recv_all(s)
send_line(s, "1")
recv_all(s)

# --- Sandbox bootstrap: create the first room via SQL, goto it ---
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({BASE},0,0,0,'Sandbox Origin','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
send_line(s, f"goto {BASE}")
out = recv_all(s)
check("Sandbox Origin" in out, "goto lands in the SQL-bootstrapped sandbox room")
check(f"[{BASE}]" in out and "[sector 0]" in out and "[flags 0]" in out,
      "an immortal's look shows the [vnum] name [sector] [flags] header")

# --- Part 2: bare edit shows the summary ---
send_line(s, "redit")
out = recv_all(s)
check("Room Name: Sandbox Origin" in out and f"Number: {BASE}" in out,
      "bare redit shows the room summary (Sneezy's info block)")
check("NONE" in out, "the summary shows no exits yet")

# --- Part 3: name, sector, description ---
send_line(s, f"redit name The Builder's Workshop")
out = recv_all(s)
check("New Room Title: The Builder's Workshop" in out,
      "redit name confirms with the original's 'New Room Title' message")

send_line(s, "redit sector_type 3")
out = recv_all(s)
check("Sector type set" in out, "redit sector_type sets the sector")
send_line(s, "redit sector_type")
out = recv_all(s)
check("Current sector type: 3" in out, "redit sector_type with no arg shows the value")

send_line(s, "redit description")
out = recv_all(s)
check("Editing room" in out, "redit description opens the line editor")
send_line(s, "Sawdust and fresh vnums hang in the air.")
recv_all(s)
send_line(s, ".")
out = recv_all(s)
check("description saved" in out, "'.' saves the description")
send_line(s, "look")
out = recv_all(s)
check("Sawdust and fresh vnums" in out, "look shows the new description immediately")

# --- Part 4: exits -- auto-create, reverse fix, walk, delete ---
send_line(s, f"redit exit north {BASE + 1}")
out = recv_all(s)
check("Exit room does not exist. Creating room" in out,
      "a missing exit target is auto-created (original behavior)")
check("Fixing opposite directions" in out and "Making new exit back" in out,
      "the reverse exit is fixed automatically (original behavior)")

send_line(s, "look")
out = recv_all(s)
check("Obvious exits: north" in out, "look now shows the new exit")

send_line(s, "north")
out = recv_all(s)
check("An unfinished room" in out, "walking north lands in the auto-created room")
send_line(s, "redit")
out = recv_all(s)
check(f"south->{BASE}" in out, "the auto-created room's reverse (south) exit points home")

send_line(s, "s")
out = recv_all(s)
check("The Builder's Workshop" in out, "'s' (single letter) walks back south")

send_line(s, "east")
out = recv_all(s)
check("You can't go that way." in out, "walking into a wall is rejected")

send_line(s, f"redit exit north -1")
out = recv_all(s)
check("Deleting exit." in out, "redit exit <dir> -1 deletes the exit")
send_line(s, "north")
out = recv_all(s)
check("You can't go that way." in out, "the deleted exit no longer works")

# --- Part 4b: diagonals (Session 21, all ten directions) ---
send_line(s, f"redit exit northeast {BASE + 2}")
out = recv_all(s)
check("Creating room" in out and "Making new exit back" in out,
      "a diagonal exit auto-creates its room and reverse (southwest) exit")
send_line(s, "ne")
out = recv_all(s)
check("An unfinished room" in out, "'ne' walks northeast into the new room")
send_line(s, "sw")
out = recv_all(s)
check("The Builder's Workshop" in out, "'sw' walks the reverse southwest exit home")

# `exits` (Tier 3) lists directions with destination names.
send_line(s, "exits")
out = recv_all(s)
check("northeast" in out and "An unfinished room" in out,
      "exits lists the diagonal exit with its destination's name")
outW = recv_all(s, timeout=0.3)  # drain

# --- Part 5: persistence across a DB reload (fresh login re-reads rooms) ---
# The room registry caches in memory; verify the DB rows directly instead.
out = subprocess.run(
    ["mariadb", "-N", "sneezy", "-e",
     f"SELECT name, sector FROM room WHERE vnum={BASE}; "
     f"SELECT COUNT(*) FROM roomexit WHERE vnum={BASE} AND direction=0; "
     f"SELECT destination FROM roomexit WHERE vnum={BASE + 1} AND direction=2;"],
    check=True, capture_output=True, text=True).stdout
check("The Builder's Workshop" in out and "3" in out,
      "name and sector persisted to the DB")
check("0" in out.splitlines()[1], "the deleted north exit is gone from the DB")
check(str(BASE) in out.splitlines()[2], "the reverse exit row persisted in the DB")

# hygiene: back to mortal
set_level(name, 1)
s.close()
print("=== ALL CHECKS PASSED ===")
