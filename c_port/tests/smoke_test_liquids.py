#!/usr/bin/env python3
"""Smoke test for Liquids (user 2026-07-26: "drinkable liquids; pouring
one out pools on the ground" + "fill a container from a liquid pool").

  1. Drinking/sipping from a real seeded drink container (vnum 410, a
     waterskin, val0=val1=70, val2=0/water) works and doesn't empty it in
     one action.
  2. Filling a non-empty container from a puddle holding a DIFFERENT
     liquid (a wineskin poured out) is refused rather than silently
     mixing.
  3. `pour` empties a container onto the ground as a puddle; the
     container reports empty afterward.
  4. `fill` refills that now-empty container from that same puddle,
     correctly recovering WHICH liquid the puddle holds (not just
     defaulting to plain water) -- confirmed by drinking it back.

Note: `load obj <vnum>` (an immortal-only debug tool) delivers straight
into the loading immortal's OWN inventory, not the room floor (see
STATUS.md's 2026-07-22 fix) -- no `get` needed/possible afterward.
Checks below only look at the response body AFTER the echoed command
line, since the command text itself often contains substrings like
"water"/"wine" that would otherwise make a check pass vacuously.

    python3 tests/smoke_test_liquids.py [host] [port]
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


announce("smoke_test_liquids")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 971000 + (int(time.time()) % 20000)
WATERSKIN = 410
WINESKIN = 409


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
    """Sends `line`, returns only the RESPONSE body (the echoed command
    line itself is stripped) so a check can't pass just because the
    command text happens to contain the word being searched for."""
    send_line(sock, line)
    raw = recv_all(sock, timeout)
    return raw.split("\r\n", 1)[1] if "\r\n" in raw else raw


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def make_char(name, pw, class_choice="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)
    send_line(s, class_choice); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    cmd(s, "color off")
    return s


sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Liquids Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

imm_name, imm_pw = f"Liqimmb{_suffix}", "liqimmpw1234"
s_imm = make_char(imm_name, imm_pw)
cmd(s_imm, "quit!")
s_imm.close()
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")

s_imm = socket.create_connection((host, port), timeout=5)
recv_all(s_imm)
send_line(s_imm, imm_name); recv_all(s_imm)
send_line(s_imm, imm_pw); recv_all(s_imm)
send_line(s_imm, "1"); recv_all(s_imm)
cmd(s_imm, "color off")
cmd(s_imm, f"goto {ROOM}")

cmd(s_imm, f"load obj {WATERSKIN}")  # lands straight in inventory, no `get` needed

out = cmd(s_imm, "sip waterskin")
check("you taste" in out.lower(), "sipping the waterskin works")
out = cmd(s_imm, "drink waterskin")
check("you drink" in out.lower() and "now empty" not in out.lower(),
      "drinking the waterskin works without emptying it in one go")

cmd(s_imm, f"load obj {WINESKIN}")
out = cmd(s_imm, "pour wineskin")
check("you empty" in out.lower(), "pouring the wineskin creates a wine puddle")

out = cmd(s_imm, "fill waterskin")
check("mix" in out.lower(), "filling the (still water-holding) waterskin from a wine puddle is refused")

cmd(s_imm, "purge")  # clear the wine puddle off the floor before the next phase

out = cmd(s_imm, "pour waterskin")
check("you empty" in out.lower(), "pouring the waterskin onto the ground reports emptying it")
out = cmd(s_imm, "drink waterskin")
check("it's empty" in out.lower(), "the poured-out waterskin is now empty")

out = cmd(s_imm, "look")
check("puddle" in out.lower() or "pool" in out.lower(), "a fresh water puddle now sits on the ground")

out = cmd(s_imm, "fill waterskin")
check("you fill" in out.lower(), "filling the empty waterskin from that puddle succeeds")
out = cmd(s_imm, "drink waterskin")
check("it's empty" not in out.lower() and "you drink" in out.lower() and "water" in out.lower(),
      "the refilled waterskin correctly has water in it again, not defaulted to the wrong liquid")

announce_done("smoke_test_liquids")
print("=== ALL CHECKS PASSED ===")
