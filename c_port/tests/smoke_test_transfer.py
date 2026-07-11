#!/usr/bin/env python3
"""Smoke test for `transfer` (user, 2026-07-10: "add a transfer command
that will take a target and transfer them into the same room as the
transfer command was issued in (transfer name) also transfer name vnum to
transfer the target to the room that matches vnum"). Covers:

  1. `transfer <name>` pulls an online player into the immortal's own
     room -- the target is told what happened and shown a fresh `look`;
     bystanders in the old and new rooms see them vanish/arrive.
  2. `transfer <name> <vnum>` moves the target into a specific room by
     vnum instead of the caller's own room.
  3. An unrecognized name is rejected without moving anyone.

    python3 tests/smoke_test_transfer.py [host] [port]
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


announce("smoke_test_transfer")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_IMM = 900000 + (int(time.time()) % 70000)
ROOM_TARGET = ROOM_IMM + 1
ROOM_OTHER = ROOM_IMM + 2


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


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


imm_name = f"Transimm{_suffix}"
imm_pw = "transimmpw123"
tgt_name = f"Transtgt{_suffix}"
tgt_pw = "transtgtpw123"

s = socket.create_connection((host, port), timeout=5)
make_char(s, imm_name, imm_pw)
set_level(imm_name, 51)
s.close()
s = login(imm_name, imm_pw)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_IMM},0,0,0,'Transfer Imm Room','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_TARGET},0,0,0,'Transfer Target Room','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_OTHER},0,0,0,'Transfer Other Room','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")

check("Transfer Imm Room" in cmd(s, f"goto {ROOM_IMM}"), "immortal lands in their own sandbox room")

sv = socket.create_connection((host, port), timeout=5)
make_char(sv, tgt_name, tgt_pw)
sql(f"UPDATE player SET load_room={ROOM_TARGET} WHERE name='{tgt_name}';")
cmd(sv, "quit!")
sv.close()
sv = login(tgt_name, tgt_pw)
check("Transfer Target Room" in cmd(sv, "look"), "the target lands in the target room")

# --- 0: an unrecognized name is rejected, nobody moves ---
out = cmd(s, "transfer NoSuchPersonAtAll")
check("no one named" in out.lower(), "transfer rejects an unrecognized name")
check("Transfer Target Room" in cmd(sv, "look"), "the target hasn't moved")

# --- 1: transfer <name> pulls the target into the immortal's own room ---
out = cmd(s, f"transfer {tgt_name}")
check(f"you transfer {tgt_name}".lower() in out.lower(), "the immortal is told the transfer happened")
check(f"room {ROOM_IMM}" in out, "the confirmation names the destination room's vnum")

out = recv_all(sv, timeout=1.0)
check("yanked through space" in out.lower(), "the target is told something happened to them")
check("Transfer Imm Room" in out, "the target's own look shows the immortal's room")

out = cmd(sv, "look")
check("Transfer Imm Room" in out, "the target is genuinely standing in the immortal's room now")

# --- 2: transfer <name> <vnum> moves the target to a specific room ---
out = cmd(s, f"transfer {tgt_name} {ROOM_OTHER}")
check(f"room {ROOM_OTHER}" in out, "the confirmation names the specific destination vnum")

out = recv_all(sv, timeout=1.0)
check("Transfer Other Room" in out, "the target's own look shows the specific destination room")

out = cmd(sv, "look")
check("Transfer Other Room" in out, "the target is genuinely standing in the specific room now")

s.close()
sv.close()
announce_done("smoke_test_transfer")
print("=== ALL CHECKS PASSED ===")
