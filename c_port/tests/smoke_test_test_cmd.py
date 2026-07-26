#!/usr/bin/env python3
"""Smoke test for the `test` command (58+, cmd_test.c/log.c) -- user: "add
a test command that will list whatever smoke test is currently running
58+". Covers:
  1. Right after this script announces itself via the `@test` hook, `test`
     (as a 58+ immortal) reports this script's own name as running.
  2. A mortal can't see `test` (Command not found).
  3. After `@test done <name>`, `test` reports nothing running.

Relies on sweep.sh running smoke tests strictly sequentially (one Python
process at a time), so there's no other test's announcement to race with.

    python3 tests/smoke_test_test_cmd.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

TEST_NAME = "smoke_test_test_cmd"


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


def make_char(nm):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, "testcmdpw"); recv_all(s)
    send_line(s, "testcmdpw"); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    return s


nameI = f"Tcmdi{_suffix}"
nameM = f"Tcmdm{_suffix}"
make_char(nameI).close()
make_char(nameM).close()
sql(f"UPDATE player_progress SET level=58 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{nameI}');")

announce(TEST_NAME)

si = socket.create_connection((host, port), timeout=5)
recv_all(si)
send_line(si, nameI); recv_all(si)
send_line(si, "testcmdpw"); recv_all(si)
send_line(si, "1"); recv_all(si)

sm = socket.create_connection((host, port), timeout=5)
recv_all(sm)
send_line(sm, nameM); recv_all(sm)
send_line(sm, "testcmdpw"); recv_all(sm)
send_line(sm, "1"); recv_all(sm)

# --- 1: an immortal sees this script's own announced name ---
out = cmd(si, "test")
check(TEST_NAME in out, f"test reports the currently-running smoke test's name (got: {out!r})")

# --- 2: a mortal can't see test ---
check("Command not found" in cmd(sm, "test"), "a mortal cannot see the test command (Command not found)")

# --- 3: after `@test done`, nothing is running ---
announce_done(TEST_NAME)
out = cmd(si, "test")
check("No smoke test is currently running" in out,
      f"test reports nothing running after @test done (got: {out!r})")

si.close()
sm.close()
print("=== ALL CHECKS PASSED ===")
