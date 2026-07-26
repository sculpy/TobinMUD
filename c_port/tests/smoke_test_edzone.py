#!/usr/bin/env python3
"""Smoke test for the menu-driven zone editor (Session 43, user: "make an
edzone command to have a menu driven editor function like edroom etc").
Covers:
  1. edzone opens the menu; editing name/enabled/lifespan/vnum range marks
     it dirty (shown in the menu) but doesn't touch the DB until Save.
  2. Save persists the scalar properties.
  3. Assigning a builder from the menu (item 5) applies immediately (not
     deferred to Save), and shows up in the menu's builder list; selecting
     the same name again un-assigns them.
  4. R) forces a reset right now from inside the editor.
  5. Quit with unsaved changes prompts Save/Discard/Cancel; Discard
     actually discards (DB unchanged).
  6. A level 51-54 builder not assigned to the zone is refused, same as
     edroom (zone_can_edit() is shared).

    python3 tests/smoke_test_edzone.py [host] [port]
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


announce("smoke_test_edzone")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ZONE = 90000 + (int(time.time()) % 9000)
ROOM = 900000 + (int(time.time()) % 60000)


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
    out = subprocess.run(["mariadb", "sneezy", "-N", "-e", stmt],
                          check=True, capture_output=True, text=True)
    return out.stdout.strip()


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


senior_name = f"Edzs{_suffix}"
senior_pw = "edzspw123"
helper_name = f"Edzh{_suffix}"
helper_pw = "edzhpw123"

sql(f"INSERT INTO zone (zone_nr,zone_name,zone_enabled,bottom,top,reset_mode,lifespan,age,util_flag) "
    f"VALUES ({ZONE},'Edzone Sandbox',1,{ROOM},{ROOM},2,45,0,0);")

s = socket.create_connection((host, port), timeout=5)
make_char(s, senior_name, senior_pw)
set_level(senior_name, 56)
s.close()
s = login(senior_name, senior_pw)

s2 = socket.create_connection((host, port), timeout=5)
make_char(s2, helper_name, helper_pw)
set_level(helper_name, 52)
s2.close()
s2 = login(helper_name, helper_pw)

# --- 6: an unassigned 51-54 builder is refused, same rule as edroom ---
out = cmd(s2, f"edit zone {ZONE}")
check("you aren't assigned to that zone" in out.lower(), "an unassigned builder is refused edzone too")

# --- 1: 55+ opens the menu; edits mark it dirty, no DB write yet ---
out = cmd(s, f"edit zone {ZONE}")
check("Editing zone:" in out and "Edzone Sandbox" in out, "edzone opens the menu")

out = cmd(s, "1")
check("Enter new name" in out, "menu item 1 prompts for a new name")
out = cmd(s, "Edzone Renamed")
check("unsaved changes" in out.lower(), "the rename marks the working copy dirty")
check(query(f"SELECT zone_name FROM zone WHERE zone_nr={ZONE};") == "Edzone Sandbox",
      "the DB is untouched before Save")

cmd(s, "3")
out = cmd(s, "90")
check("Lifespan (minutes): 90" in out, "lifespan updated in the working copy")

cmd(s, "4")
out = cmd(s, f"{ROOM} {ROOM + 5}")
check(f"Vnum range: {ROOM}-{ROOM + 5}" in out, "vnum range updated in the working copy")

# --- 2: Save persists ---
out = cmd(s, "S")
check("Zone saved" in out, "Save reports success")
check(query(f"SELECT zone_name, lifespan, bottom, top FROM zone WHERE zone_nr={ZONE};")
      == f"Edzone Renamed\t90\t{ROOM}\t{ROOM + 5}",
      "Save actually persisted name/lifespan/range to the DB")

# --- 3: assigning a builder from the menu applies immediately ---
out = cmd(s, "5")
check("Enter a builder name" in out, "menu item 5 prompts for a builder name")
out = cmd(s, helper_name)
check("Assigned." in out, "assigning from the menu confirms")
check(f"Assigned builders: {helper_name}" in out, "the menu redisplay lists the newly-assigned builder")
check(query(f"SELECT COUNT(*) FROM zone_owner WHERE zone_nr={ZONE} AND player_id="
            f"(SELECT id FROM player WHERE name='{helper_name}');") == "1",
      "the assignment landed in zone_owner immediately, not deferred to Save")

out = cmd(s, "5")
out = cmd(s, helper_name)
check("Un-assigned." in out, "selecting the same name again un-assigns them")
check(query(f"SELECT COUNT(*) FROM zone_owner WHERE zone_nr={ZONE} AND player_id="
            f"(SELECT id FROM player WHERE name='{helper_name}');") == "0",
      "the un-assignment landed immediately too")

# --- 4: R) forces a reset right now ---
out = cmd(s, "R")
check("Zone reset:" in out, "R triggers an immediate reset from inside the editor")

# --- 5: Quit with unsaved changes -- Discard actually discards ---
cmd(s, "1")
cmd(s, "Should Not Persist")
out = cmd(s, "Q")
check("unsaved changes" in out.lower(), "Quit with unsaved changes prompts to save/discard/cancel")
out = cmd(s, "D")
check("Leaving the zone editor" in out, "Discard leaves the editor")
check(query(f"SELECT zone_name FROM zone WHERE zone_nr={ZONE};") == "Edzone Renamed",
      "Discard did NOT persist the last unsaved rename")

s.close()
s2.close()
announce_done("smoke_test_edzone")
print("=== ALL CHECKS PASSED ===")
