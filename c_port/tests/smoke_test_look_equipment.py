#!/usr/bin/env python3
"""Smoke test for `look <person>` showing worn equipment (user, 2026-07-12:
"when you look at someone you should also see what equipment thier
wearing"). Shares `being_render_equipment()` (being.c) with the existing
`equipment` command (self-view only) -- this test covers the new
other-viewer path specifically. Covers:

  1. Looking at someone wearing nothing shows all "nothing" slots.
  2. After they wear an item, looking at them shows it in the right slot.
  3. `equipment` (looking at yourself, functionally) still works exactly
     as before -- the shared refactor didn't change self-view output.

    python3 tests/smoke_test_look_equipment.py [host] [port]
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


announce("smoke_test_look_equipment")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time() * 1000000) % 90000)
TUNIC = ROOM + 1

WEAR_TAKE = 1
WEAR_BODY = 8


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
    send_line(s, "1"); recv_all(s)  # race
    send_line(s, "1"); recv_all(s)  # class
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment
    cmd(s, "color off")
    return s


pw = "lookeqpw123"

imm_name = f"Lookeqimm{_suffix}"
s_imm = make_char(imm_name, pw)
cmd(s_imm, "quit!")
s_imm.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    # room_flag=1 (ROOM_FLAG_ALWAYS_LIT, room.h) -- this test isn't about
    # darkness, so it needs to be immune to whatever the world's current
    # day/night state happens to be (Sneezy → Tobin feature audit,
    # "Weather & light levels" wired real darkness into `look`/`exits`
    # after this test was first written).
    f"VALUES ({ROOM},0,0,0,'Look Equipment Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Look Equipment Sandbox" in cmd(s_imm, f"goto {ROOM}"), "goto lands in the sandbox room")

mort_name = f"Lookeqmor{_suffix}"
s_mort = make_char(mort_name, pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mort_name}';")
cmd(s_mort, "quit!")
s_mort.close()
s_mort = socket.create_connection((host, port), timeout=5)
recv_all(s_mort)
send_line(s_mort, mort_name); recv_all(s_mort)
send_line(s_mort, pw); recv_all(s_mort)
send_line(s_mort, "1"); recv_all(s_mort)
cmd(s_mort, "color off")
check("Look Equipment Sandbox" in cmd(s_mort, "look"), "the mortal lands in the sandbox room")

# --- 1: looking at someone wearing nothing shows the full "nothing" slate ---
out = cmd(s_imm, f"look {mort_name}")
check(f"{mort_name} is using:" in out, "the equipment header names the looked-at player")
check("nothing" in out, "an unequipped body slot reads 'nothing'")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({TUNIC},'tunic plain','a plain tunic','A plain tunic is lying here.',"
    f"12,{WEAR_TAKE | WEAR_BODY},1);")
check("You conjure" in cmd(s_imm, f"load obj {TUNIC}"), "the tunic is loaded")
# `load obj` (2026-07-22) drops it straight into the loading immortal's
# OWN inventory, not the room floor -- drop it first so the mortal can
# pick it back up off the ground.
cmd(s_imm, "drop tunic")

out = cmd(s_mort, "get tunic")
check("you get" in out.lower(), "the mortal picks up the tunic")
out = cmd(s_mort, "wear tunic")
check("wear" in out.lower(), "the mortal wears the tunic")

# --- 2: looking at them now shows the tunic in the body slot ---
out = cmd(s_imm, f"look {mort_name}")
check(f"{mort_name} is using:" in out, "the equipment header still names the looked-at player")
check("body:" in out and "tunic" in out, "look now shows the tunic worn on the body")

# --- 3: self-view (`equipment`) is unaffected by the shared refactor ---
out = cmd(s_mort, "equipment")
check("You are using:" in out, "equipment still uses the first-person header for yourself")
check("body:" in out and "tunic" in out, "equipment still shows your own worn tunic")

s_imm.close()
s_mort.close()
announce_done("smoke_test_look_equipment")
print("=== ALL CHECKS PASSED ===")
