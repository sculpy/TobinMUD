#!/usr/bin/env python3
"""Smoke test for `pee` (user, 2026-07-11: "add pools and the pee command
for 51"). A flavor command, immortal-only (51+): leaves a non-takeable
puddle (obj_create_pool(), obj.c) on the floor of the caller's room.

  1. Below level 51, `pee` is refused.
  2. At 51+, `pee` succeeds, tells the caller, and leaves a puddle visible
     in `look` -- and to a bystander in the same room.
  3. The puddle cannot be picked up (`get puddle` fails).
  4. Multiple uses leave multiple puddles (no merging/dedup attempted).

    python3 tests/smoke_test_pee.py [host] [port]
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


announce("smoke_test_pee")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 900000 + (int(time.time()) % 70000)


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
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)  # race: human (zero stat modifier)
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


imm_name = f"Peeimm{_suffix}"
imm_pw = "peeimmpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
s.close()
s = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Pee Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")

# --- 1: below 51, pee is refused ---
out = cmd(s, "pee")
check("Command not found" in out, "pee is refused below level 51")

# --- 2: at 51+, pee succeeds and leaves a puddle ---
set_level(imm_name, 51)
cmd(s, "quit!")
s.close()
s = login(imm_name, imm_pw)
check("Pee Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

bystander_name = f"Peewitness{_suffix}"
bystander_pw = "peewitnesspw123"
sb = socket.create_connection((host, port), timeout=5)
make_char(sb, bystander_name, bystander_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{bystander_name}';")
cmd(sb, "quit!")
sb.close()
sb = login(bystander_name, bystander_pw)
check("Pee Sandbox" in cmd(sb, "look"), "the bystander lands in the sandbox room")

out = cmd(s, "pee")
check("You relieve yourself" in out, "pee tells the caller what happened")

witness_out = recv_all(sb, timeout=0.5)
check(imm_name.lower() in witness_out.lower(), "the bystander is told who did it")

out = cmd(s, "look")
check("puddle of pee" in out.lower(), "the puddle is visible to the immortal via look")
out = cmd(sb, "look")
check("puddle of pee" in out.lower(), "the puddle is visible to the bystander via look")

# --- 3: the puddle can't be picked up ---
out = cmd(s, "get puddle")
check("puddle" not in cmd(s, "inventory").lower(), "the puddle is not in inventory after a get attempt")

# --- 4: a second use GROWS the same puddle into a bigger pool instead of
# adding a separate one (user, 2026-07-11: "pools should grow in size if
# multiple puddles of the same type are created in a room") ---
cmd(s, "pee")
out = cmd(s, "look")
check("a pool of pee" in out.lower(), "a second pee grows the puddle into 'a pool of pee'")
check("a puddle of pee" not in out.lower(),
      "the size-1 wording is gone -- confirms it grew in place rather than a second puddle sitting alongside it")
check(out.lower().count("a pool of pee") == 1,
      "still just one pee pool object, grown in place, not a second one")

s.close()
sb.close()
announce_done("smoke_test_pee")
print("=== ALL CHECKS PASSED ===")
