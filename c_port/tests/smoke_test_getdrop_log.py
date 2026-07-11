#!/usr/bin/env python3
"""Smoke test for get/drop dispute logging (Session 43 continued, user:
"anytime a char gets an item or drops an item i want those logged into
the game log so we can research disputes with log search. these should
not be reported via any log type, just inserted into the game log").
Covers:
  1. A `get` writes a [SILENT] line to the day's log file (who, what,
     vnum, room) -- reachable via `log search`.
  2. A `drop` does too.
  3. Neither is echoed to an online immortal (LOG_SILENT never echoes,
     unlike every other log type).

    python3 tests/smoke_test_getdrop_log.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
OBJ_VNUM = 175  # "a simple wooden dart" -- real seeded, takeable


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


announce("smoke_test_getdrop_log")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


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


def make_char(name, pw, level=None):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, ""); recv_all(s)   # color default
    send_line(s, ""); recv_all(s)   # timezone default
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    cmd(s, "done")
    cmd(s, "done")
    if level is not None:
        sql(f"UPDATE player_progress SET level={level} WHERE player_id="
            f"(SELECT id FROM player WHERE name='{name}');")
    s.close()
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    return s


nameA = f"Gdlog{_suffix}"
nameI = f"Gdimm{_suffix}"
pw = "getdroplogpw123"

sA = make_char(nameA, pw, level=52)  # BUILD_MIN_LEVEL, for `load`
cmd(sA, "color off")

sI = make_char(nameI, pw, level=59)  # LOG_MIN_LEVEL, for `log search`
cmd(sI, "color off")
cmd(sI, "goto 1")  # away from A's room -- isolates the LOG_SILENT check
                    # from the ordinary (and entirely expected) room broadcast

cmd(sA, f"load obj {OBJ_VNUM}")
out_get = cmd(sA, "get dart")
check("you get" in out_get.lower(), "the get succeeds")
leaked = recv_all(sI, 0.5)
check("dart" not in leaked.lower(), "an online immortal sees nothing from the get (LOG_SILENT never echoes)")

out_drop = cmd(sA, "drop dart")
check("you drop" in out_drop.lower(), "the drop succeeds")
leaked = recv_all(sI, 0.5)
check("dart" not in leaked.lower(), "an online immortal sees nothing from the drop (LOG_SILENT never echoes)")

out_search = cmd(sI, f"log search {nameA}")
check("gets a simple wooden dart" in out_search and f"(vnum {OBJ_VNUM})" in out_search,
      "log search finds the get, with the item's vnum")
check("drops a simple wooden dart" in out_search and f"(vnum {OBJ_VNUM})" in out_search,
      "log search finds the drop, with the item's vnum")

sA.close()
sI.close()
announce_done("smoke_test_getdrop_log")
print("=== ALL CHECKS PASSED ===")
