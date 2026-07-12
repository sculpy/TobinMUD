#!/usr/bin/env python3
"""Smoke test for quit! dropping possessions (user 2026-07-12: "after
rent goes in quitting the game will drop all possessions on the ground
where the quit command was executed, gold included"). Covers:

  1. quit! while carrying an item drops it on the room floor and tells
     both the quitter and any onlookers.
  2. The item is gone from the quitter's inventory on reconnect.
  3. quit! with nothing carried does NOT print a spill message (no
     empty-handed false positive).

Gold itself isn't covered -- there is no Money system yet (TODO.md task
29), so there's nothing to drop.

    python3 tests/smoke_test_quit_drop.py [host] [port]
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


announce("smoke_test_quit_drop")

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time() * 1000000) % 90000)
OBJ = ROOM + 1


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


def make_char(sock, name, pw):
    recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "y"); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, "new"); recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)  # race: human
    send_line(sock, "1"); recv_all(sock)  # class: mage
    send_line(sock, "2"); recv_all(sock)  # alignment: neutral


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


pw = "quitdroppw123"

imm_name = f"Quitdimm{_suffix}"
s_imm = socket.create_connection((host, port), timeout=5)
make_char(s_imm, imm_name, pw)
s_imm.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
s_imm = login(imm_name, pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Quitdrop Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
check("Quitdrop Sandbox" in cmd(s_imm, f"goto {ROOM}"), "goto lands in the sandbox room")

sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({OBJ},'trinket silver','a small silver trinket',"
    f"'A small silver trinket is lying here.',12,1,1);")

mort_name = f"Quitdmor{_suffix}"
s_mort = socket.create_connection((host, port), timeout=5)
make_char(s_mort, mort_name, pw)
cmd(s_mort, "quit!")  # a real quit!, not a raw close -- see the load_room comment below
s_mort.close()
# A raw close leaves the character linkdead in its CURRENT room, and a
# linkdead body's room wins over player.load_room on reconnect (see
# enter_world(), descriptor.c) -- only a real quit! (fully detached, no
# linkdead body left behind) lets this SQL edit actually take effect.
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{mort_name}';")
s_mort = login(mort_name, pw)
check("Quitdrop Sandbox" in cmd(s_mort, "look"), "the mortal lands in the sandbox room")

witness_name = f"Quitdwit{_suffix}"
s_wit = socket.create_connection((host, port), timeout=5)
make_char(s_wit, witness_name, pw)
cmd(s_wit, "quit!")
s_wit.close()
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{witness_name}';")
s_wit = login(witness_name, pw)
recv_all(s_wit, timeout=0.3)

check("You conjure" in cmd(s_imm, f"load obj {OBJ}"), "the trinket is loaded")
check("you get" in cmd(s_mort, "get trinket").lower(), "the mortal picks up the trinket")
out = cmd(s_mort, "inventory")
check("trinket" in out.lower(), "the trinket is in the mortal's inventory before quitting")

# --- 1/2: quit! drops the carried item on the floor ---
out = cmd(s_mort, "quit!")
check("Your belongings spill onto the ground" in out, "the quitter is told their belongings spilled")
witness_out = recv_all(s_wit, timeout=0.5)
check(f"{mort_name}'s belongings spill onto the ground!" in witness_out,
      "onlookers are told the quitter's belongings spilled")

out = cmd(s_imm, "look")
check("trinket" in out.lower(), "the trinket is now lying on the sandbox floor")

s_mort = login(mort_name, pw)
out = cmd(s_mort, "inventory")
check("trinket" not in out.lower(), "the trinket is gone from the mortal's inventory on reconnect")

# --- 3: quit! with nothing carried says nothing about spilling ---
out = cmd(s_mort, "quit!")
check("belongings spill" not in out, "quitting empty-handed does not claim anything spilled")

s_imm.close()
s_wit.close()
announce_done("smoke_test_quit_drop")
print("=== ALL CHECKS PASSED ===")
