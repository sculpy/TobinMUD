#!/usr/bin/env python3
"""Smoke test for `shout` (user, 2026-07-11: "add a shout channel, use
sneezy for implementation ideas"). Modeled on the original's sendShout()
(misc/talk.cc): unlike `say`, a shout reaches everyone playing anywhere in
the game, not just the shouter's room.

  1. An empty shout is rejected.
  2. A shout reaches a listener in a completely different room.
  3. A sleeping listener does not hear a shout.
  4. `toggle noshout` opts a mortal out of hearing shouts.
  5. An immortal's shout is heard even by someone with noshout on.

    python3 tests/smoke_test_shout.py [host] [port]
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


announce("smoke_test_shout")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_A = 900000 + (int(time.time()) % 60000)
ROOM_B = ROOM_A + 1


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


sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_A},0,0,0,'Shout Room A','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_B},0,0,0,'Shout Room B','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")

shouter_name = f"Shoutmort{_suffix}"
shouter_pw = "shoutmortpw123"
s = socket.create_connection((host, port), timeout=5)
make_char(s, shouter_name, shouter_pw)
sql(f"UPDATE player SET load_room={ROOM_A} WHERE name='{shouter_name}';")
cmd(s, "quit!")
s.close()
s = login(shouter_name, shouter_pw)
check("Shout Room A" in cmd(s, "look"), "the shouter lands in room A")

# --- 1: an empty shout is rejected ---
out = cmd(s, "shout")
check("WHAT do you want to shout" in out, "an empty shout is rejected")

# --- 2: a shout reaches a listener in a different room ---
listener_name = f"Shoutlist{_suffix}"
listener_pw = "shoutlistpw123"
sl = socket.create_connection((host, port), timeout=5)
make_char(sl, listener_name, listener_pw)
sql(f"UPDATE player SET load_room={ROOM_B} WHERE name='{listener_name}';")
cmd(sl, "quit!")
sl.close()
sl = login(listener_name, listener_pw)
check("Shout Room B" in cmd(sl, "look"), "the listener lands in room B, a different room")

out = cmd(s, "shout Can anyone hear me out there?")
check('You shout, "Can anyone hear me out there?"' in out, "the shouter sees their own shout")
heard = recv_all(sl, timeout=1.0)
check(f'{shouter_name} shouts, "Can anyone hear me out there?"' in heard,
      "the listener in a different room hears the shout")

# --- 3: a sleeping listener does not hear a shout ---
cmd(sl, "sleep")
recv_all(sl, timeout=0.3)
cmd(s, "shout This should not wake anyone.")
heard_asleep = recv_all(sl, timeout=1.0)
check("This should not wake anyone." not in heard_asleep, "a sleeping listener does not hear the shout")
cmd(sl, "wake")
recv_all(sl, timeout=0.3)

# --- 4: toggle noshout opts a mortal out ---
out = cmd(sl, "toggle noshout")
check("noshout is now" in out, "toggle noshout confirms")
cmd(s, "shout This should be muted for the opted-out listener.")
heard_muted = recv_all(sl, timeout=1.0)
check("This should be muted for the opted-out listener." not in heard_muted,
      "a mortal with noshout on does not hear a mortal's shout")

# --- 5: an immortal's shout always gets through, even with noshout on ---
imm_name = f"Shoutimm{_suffix}"
imm_pw = "shoutimmpw123"
si = socket.create_connection((host, port), timeout=5)
make_char(si, imm_name, imm_pw)
set_level(imm_name, 51)
si.close()
si = login(imm_name, imm_pw)

cmd(si, "shout This is an announcement from on high.")
heard_imm = recv_all(sl, timeout=1.0)
check("This is an announcement from on high." in heard_imm,
      "an immortal's shout is heard even by a listener with noshout on")

s.close()
sl.close()
si.close()
announce_done("smoke_test_shout")
print("=== ALL CHECKS PASSED ===")
