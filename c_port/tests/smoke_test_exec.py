#!/usr/bin/env python3
"""Smoke test for `exec` (cmd_exec.c), the level-60 host shell:
  1. A mortal can't see it (Command not found) -- it's hidden below level 60.
  2. An Implementor (60) runs `exec echo <marker>` and sees the marker.
  3. A blocklisted command (e.g. `rm -rf /tmp/x`) is refused, not run.

    python3 tests/smoke_test_exec.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

def announce(test_name, host=host, port=port):
    """Tell the running MUD which smoke test is executing: emits a [TEST]
    log line (visible to online immortals and in the day's log file) via
    the loopback-only `@test` server hook. Best-effort -- never fails the
    test. Self-contained (own socket, doesn't depend on this file's other
    helpers) so it can run at any point in the script."""
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
    """Companion to announce() -- emits a [TEST] "finished" log line. Call
    once at the very end of a smoke test, just before it reports success."""
    announce(f"done {test_name}", host, port)


announce("smoke_test_exec")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def recv_all(sock, timeout=1.5):
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


def cmd(sock, line, timeout=1.5):
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
    send_line(s, "execpw"); recv_all(s)
    send_line(s, "execpw"); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    return s


def relogin(nm):
    r = socket.create_connection((host, port), timeout=5)
    recv_all(r)
    send_line(r, nm); recv_all(r)
    send_line(r, "execpw"); recv_all(r)
    send_line(r, "1"); recv_all(r)
    return r


# mortal: exec is hidden
nameM = f"Exem{_suffix}"
sm = make_char(nameM)
check("Command not found" in cmd(sm, "exec echo hi"), "a mortal cannot see exec (Command not found)")
sm.close()

# Implementor (60)
nameI = f"Exei{_suffix}"
make_char(nameI).close()
sql(f"UPDATE player_progress SET level=60 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{nameI}');")
s = relogin(nameI)

marker = f"tobinexec{_suffix}"
out = cmd(s, f"exec echo {marker}")
check(marker in out, "exec echo shows the command output")

out = cmd(s, "exec rm -rf /tmp/doesnotexist")
check("Refused" in out or "blocklist" in out, "a blocklisted command is refused")

s.close()
announce_done("smoke_test_exec")
print("=== ALL CHECKS PASSED ===")
