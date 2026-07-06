#!/usr/bin/env python3
"""Smoke test for the menu-driven room builder (edroom) -- the CONN_REDIT_*
working-copy editor, formatted like Sneezy's update_room_menu (numbered
fields) with door-type/condition editing on exits.

Covered:
  1. Gate: a mortal typing `edroom` gets Huh?! (hidden below BUILD_MIN_LEVEL).
  2. `edroom`/`edroom <vnum>` opens the numbered menu for the current/named room.
  3. Fields 1-7: Name, Description (line editor + /clear), Flags toggle,
     Sector Type, Exits, Max Capacity, Room Height -- each changes the
     working copy and shows in the re-rendered menu, marking it unsaved.
  4. Exits submenu: pick a direction -> per-exit menu (Target / Door type /
     Conditions / Remove). Target to a not-yet-existing vnum + a door type +
     a condition; (S)ave auto-creates the target, fixes the reverse exit,
     and persists dest+type+condition. Movement walks the new link.
  5. (Q)uit + (D)iscard leaves the DB untouched.
  6. (C)lear room out blanks the room AND its exits (+ neighbour reverse).

All editing happens in SQL-bootstrapped sandbox rooms at high vnums
(900000+); the seeded world is never touched.

    python3 tests/smoke_test_edroom.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
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


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    return recv_all(sock, timeout)


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def sql(stmt):
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


def query(stmt):
    return subprocess.run(["mariadb", "-N", "sneezy", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


name = f"Menued{_suffix}"
pw = "edroompw"

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "new"); recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "done"); recv_all(s)

check("Huh?!" in cmd(s, "edroom"), "a mortal typing edroom gets Huh?! (hidden)")

set_level(name, 51)
s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")   # strip <c>/<z> tags so menu text is clean for matching

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({BASE},0,0,0,'Sandbox Origin','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
check("Sandbox Origin" in cmd(s, f"goto {BASE}"),
      "goto lands in the SQL-bootstrapped sandbox room")

# --- 2: numbered menu opens (Sneezy layout) ---
out = cmd(s, "edroom")
check(f"Number: {BASE}" in out and "Menu:" in out and "1) Name" in out
      and "6) Max Capacity" in out and "[edroom]" in out,
      "edroom opens the Sneezy-style numbered menu")

# --- 3: fields 1,4,3,6,7 (name, sector, flags, capacity, height) ---
cmd(s, "1")                                        # name
out = cmd(s, "The Menu Workshop")
check("Room Name: The Menu Workshop" in out and "unsaved changes" in out,
      "1) Name sets the name and marks unsaved")

cmd(s, "4")                                        # sector
out = cmd(s, "3")                                  # arctic road
check("[ ARCTIC ROAD ]" in out and "(arctic road)" not in out,
      "4) Sector Type shows the all-caps enum name in a [ ] bracket, no number")

cmd(s, "3")                                        # flags
cmd(s, "3")                                        # toggle indoors (bit 3)
out = cmd(s, "")                                   # back to menu
check("[ INDOORS ]" in out, "3) Flags toggles INDOORS on, each flag bracketed")

cmd(s, "6")                                        # capacity
out = cmd(s, "25")
check("Max Capacity: 25" in out, "6) Max Capacity persists into the menu")

cmd(s, "7")                                        # height
out = cmd(s, "12")
check("Room Height: 12" in out, "7) Room Height persists into the menu")

# --- 3d: description (2) with /clear ---
out = cmd(s, "2")
check("Editing description" in out, "2) Description opens the line editor")
cmd(s, "junk that will be cleared")
check("Buffer cleared" in cmd(s, "/clear"), "/clear wipes the description buffer")
cmd(s, "A tidy menu-built room.")
out = cmd(s, ".")
check("A tidy menu-built room" in out and "junk that will be cleared" not in out,
      "2)+/clear replaces the description")

# --- 4: exits submenu -> per-exit menu -> target/door/condition ---
out = cmd(s, "5")                                  # exits list
check("choose a direction" in out, "5) Exits opens the direction list")
out = cmd(s, "north")                              # -> per-exit menu
check("Exit north" in out and "1) Target room" in out and "2) Door type" in out,
      "picking a direction opens the per-exit menu")
cmd(s, "1")                                        # target
out = cmd(s, str(BASE + 1))
check("created on save" in out and "Target: " + str(BASE + 1) in out,
      "1) Target sets a not-yet-existing target (warned)")
cmd(s, "2")                                        # door type
out = cmd(s, "1")                                  # Door
check("Door: Door" in out, "2) Door type sets the door to 'Door'")
cmd(s, "3")                                        # conditions
cmd(s, "0")                                        # toggle Closed (bit 0)
out = cmd(s, "")                                   # back to per-exit menu
check("Cond: Closed" in out, "3) Conditions toggles 'Closed' on")
cmd(s, "")                                         # back to exits list
out = cmd(s, "")                                   # back to main menu
check(f"north->{BASE + 1}/Door" in out, "the menu summarizes the exit with its door")

out = cmd(s, "S")
check("Room saved" in out, "S) Save persists the working copy")

# DB verification
row = query(f"SELECT name, sector, room_flag, capacity, height FROM room WHERE vnum={BASE};").split("\t")
check(row[0] == "The Menu Workshop" and row[1] == "3" and row[2] == "8"
      and row[3] == "25" and row[4].strip() == "12",
      "name/sector/flags(8)/capacity(25)/height(12) persisted")
ex = query(f"SELECT destination, type, condition_flag FROM roomexit WHERE vnum={BASE} AND direction=0;").split("\t")
check(ex[0] == str(BASE + 1) and ex[1] == "1" and ex[2].strip() == "1",
      "north exit persisted with door type 1 (Door) and condition 1 (Closed)")
check(query(f"SELECT COUNT(*) FROM room WHERE vnum={BASE + 1};").strip() == "1",
      "the missing exit target was auto-created on save")
check(query(f"SELECT destination FROM roomexit WHERE vnum={BASE + 1} AND direction=2;").strip()
      == str(BASE), "the reverse (south) exit was fixed automatically")

check("Leaving the room editor" in cmd(s, "Q"), "Q) leaves cleanly when saved")
# The north exit's door was set to the Closed condition above (door
# mechanics, Session 31) -- open it before walking through, matching how
# a real player would have to.
check("You open the door to the north" in cmd(s, "open north"),
      "the door saved as Closed can be opened before walking through")
check("An unfinished room" in cmd(s, "north"), "walking north lands in the auto-created room")
check("The Menu Workshop" in cmd(s, "south"), "walking south returns to the workshop")

# --- 5: quit-discard leaves the DB untouched ---
cmd(s, "edroom")
cmd(s, "1")
out = cmd(s, "This name must not persist")
check("unsaved changes" in out, "an edit marks the session dirty")
out = cmd(s, "Q")
check("(S)ave, (D)iscard, (C)ancel" in out, "Q with unsaved changes prompts")
check("Leaving the room editor" in cmd(s, "D"), "D)iscard leaves without saving")
check(query(f"SELECT name FROM room WHERE vnum={BASE};").strip() == "The Menu Workshop",
      "the discarded name change never reached the DB")

# --- 6: clear room out blanks room + exits (+ neighbour reverse) ---
cmd(s, f"edroom {BASE + 1}")
out = cmd(s, "C")
check("(yes/no)" in out, "C) asks for confirmation")
check("blanked" in cmd(s, "yes"), "C) blanks the working copy after confirmation")
cmd(s, "S")
cmd(s, "Q")
check(query(f"SELECT name FROM room WHERE vnum={BASE + 1};").strip() == "An unfinished room",
      "cleared room is blanked in the DB")
check(query(f"SELECT COUNT(*) FROM roomexit WHERE vnum={BASE + 1};").strip() == "0",
      "cleared room's own exits are gone")
check(query(f"SELECT COUNT(*) FROM roomexit WHERE vnum={BASE} AND direction=0;").strip() == "0",
      "the neighbour's reverse exit (BASE north) was removed too")

set_level(name, 1)
s.close()
print("=== ALL CHECKS PASSED ===")
