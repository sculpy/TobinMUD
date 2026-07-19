#!/usr/bin/env python3
"""Smoke test for blood pools from limb damage (user, 2026-07-11: "goes
with limb damage and bleeding", said right after the pools/pee request).
A limb crossing into a bad-enough injury tier (limb_status_text()
non-NULL, <20% HP -- the same tier-crossing guard combat.c already uses
for its injury-tier tell() messages) now also drops a "pool of blood" via
obj_create_pool() (obj.h/obj.c, shared with `pee`) in the room, and
echoes "Blood pools around X!" to everyone there. Deterministic via the
immortal-only `hurtlimb <target> <limb> <hp>` debug command (same
precedent as smoke_test_limbs.py), rather than waiting on combat RNG.

    python3 tests/smoke_test_bleeding.py [host] [port]
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


announce("smoke_test_bleeding")

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


imm_name = f"Bleedimm{_suffix}"
imm_pw = "bleedimmpw123"
victim_name = f"Bleedvic{_suffix}"
victim_pw = "bleedvicpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Bleed Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
check("Bleed Sandbox" in cmd(s, f"goto {ROOM}"), "goto lands in the SQL-bootstrapped sandbox room")

sv = socket.create_connection((host, port), timeout=5)
make_char(sv, victim_name, victim_pw)
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{victim_name}';")
cmd(sv, "quit!")
sv.close()
sv = login(victim_name, victim_pw)
check("Bleed Sandbox" in cmd(sv, "look"), "the victim lands directly in the sandbox room")

out = cmd(s, "look")
check("blood" not in out.lower(), "no blood pool exists yet before any injury")

# hp=2 against the LIMB_MIN_MAX_HP=15 floor -> 13%, inside the "hurt rather
# badly" (<20%) tier -- same trick smoke_test_limbs.py uses.
out = cmd(s, f"hurtlimb {victim_name} leftarm 2")
check("Limb HP set" in out, "hurtlimb confirms (not a decapitation)")
check(f"Blood pools around {victim_name}!" in out,
      "the immortal is told a blood pool appeared")

out_victim = recv_all(sv, timeout=1.0)
check(f"Blood pools around {victim_name}!" in out_victim,
      "the victim also sees the blood pool announcement")

out = cmd(s, "look")
check("a puddle of blood" in out.lower(), "a fresh (size-1) blood pool reads as 'a puddle of blood'")

# A second tier crossing (destroyed, 0%) GROWS the same blood pool instead
# of dropping a separate object (user, 2026-07-11: "pools should grow in
# size if multiple puddles of the same type are created in a room").
out = cmd(s, f"hurtlimb {victim_name} leftarm 0")
check(f"Blood pools around {victim_name}!" in out,
      "a second injury-tier crossing grows the blood pool again")
out = cmd(s, "look")
check("a pool of blood" in out.lower(), "the blood pool grew from a puddle to a pool")
check(out.lower().count("blood") == 1, "still just one blood pool object, grown in place, not a second one")

s.close()
sv.close()
announce_done("smoke_test_bleeding")
print("=== ALL CHECKS PASSED ===")
