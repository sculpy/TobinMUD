#!/usr/bin/env python3
"""Smoke test for the Whittle profession (Sneezy -> Tobin feature audit,
TODO.md "Deferred decisions" -- task_whittle.h/.cc, scoped down: no bows/
arrows, single-action instead of the original's multi-tick task; see
whittle.h's own doc comment).

  1. `whittle` alone lists the known items.
  2. Whittling with no weapon wielded is refused.
  3. Whittling with a weapon but no wood is refused.
  4. A real recipe (toothpick) succeeds once enough carried wood-log
     weight is present, and the wood is actually consumed (gone from
     inventory afterward), and the result lands in inventory.
  5. An unknown item name is refused.

    python3 tests/smoke_test_whittle.py [host] [port]
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


announce("smoke_test_whittle")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 974000 + (int(time.time()) % 20000)
POPLAR_LOG_VNUM = 79   # real seeded raw wood log, MAT_WOOD, weight 49
STAFF_VNUM = 177       # wooden training staff, OBJ_CAT_WEAPON


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
    raw = recv_all(sock, timeout)
    return raw.split("\r\n", 1)[1] if "\r\n" in raw else raw


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def cmd_paged(sock, line, timeout=1.0):
    """Like cmd(), but drains a paginated ("[ENTER for more]") response
    fully before returning -- a long `inventory` listing can trip the
    pager, and a later command sent while a page is still pending gets
    silently swallowed as a "show more" keystroke instead of dispatched."""
    out = cmd(sock, line, timeout)
    full = out
    while "enter" in out.lower() and "more" in out.lower():
        out = cmd(sock, "", timeout)
        full += out
    return full


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def make_char(name, pw, class_choice="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", class_choice, "done", "done"):
        send_line(s, step)
        recv_all(s)
    s.close()


sql(f"DELETE FROM room WHERE vnum={ROOM};")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Whittle Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

imm_name = f"Whitimm{_suffix}"
imm_pw = "whitimmpw1234"
make_char(imm_name, imm_pw)
sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, imm_name); recv_all(s)
send_line(s, imm_pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")
cmd(s, f"goto {ROOM}")

out = cmd(s, "whittle")
check("toothpick" in out.lower(), "`whittle` with no argument lists the known items")

out = cmd(s, "whittle toothpick")
check("weapon wielded" in out.lower(), "whittling with no weapon wielded is refused")

cmd(s, f"load obj {STAFF_VNUM}")
cmd(s, "wield staff")
out = cmd(s, "whittle toothpick")
check("don't have enough wood" in out.lower(), "whittling with a weapon but no wood is refused")

cmd(s, f"load obj {POPLAR_LOG_VNUM}")
out = cmd(s, "whittle toothpick")
check("you whittle a toothpick" in out.lower(), "toothpick succeeds once wood is carried")

out = cmd_paged(s, "inventory")
lines = [l.strip().lower() for l in out.splitlines()]
check(not any("poplar log" in l for l in lines), "the wood log was consumed")
check(any(l.startswith("a toothpick") or l == "toothpick" for l in lines),
      "the finished toothpick is now in inventory")

out = cmd(s, "whittle bogus item")
check("no idea how to whittle" in out.lower(), "an unknown item name is refused")

announce_done("smoke_test_whittle")
print("=== ALL CHECKS PASSED ===")
