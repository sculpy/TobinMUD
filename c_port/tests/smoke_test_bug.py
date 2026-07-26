#!/usr/bin/env python3
"""Smoke test for `bug` / `delbug` (cmd_bug.c, bug_repo.c):
  1. A mortal files a bug with `bug <text>` and gets a thank-you.
  2. Bare `bug` from a mortal shows usage; from an immortal lists reports.
  3. The filed report shows the submitter and appears in the immortal list.
  4. `delbug <id>` (59+) removes it; a mortal can't see delbug (Command not found).

    python3 tests/smoke_test_bug.py [host] [port]
"""
import re
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


announce("smoke_test_bug")

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


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def sql(stmt):
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


def make_char(nm):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, "bugpw"); recv_all(s)
    send_line(s, "bugpw"); recv_all(s)
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
    send_line(r, "bugpw"); recv_all(r)
    send_line(r, "1"); recv_all(r)
    return r


# --- mortal files a bug ---
nameM = f"Bugm{_suffix}"
sm = make_char(nameM)
marker = f"widget{_suffix}"
check("filed" in strip(cmd(sm, f"bug the {marker} is broken")).lower(),
      "a mortal can file a bug and is thanked")
check("Usage: bug" in strip(cmd(sm, "bug")), "bare bug from a mortal shows usage")
check("Command not found" in cmd(sm, "delbug 1"), "a mortal cannot see delbug (Command not found)")
sm.close()

# --- immortal lists + deletes ---
nameI = f"Bugi{_suffix}"
make_char(nameI).close()
sql(f"UPDATE player_progress SET level=59 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{nameI}');")
si = relogin(nameI)

out = strip(cmd(si, "bug"))
check("Bug reports" in out, "immortal bare bug shows the report list header")
check(marker in out and nameM.capitalize() in out,
      "the report lists its text and the submitter's name")

# find the id for our marker and delete it
m = re.search(rf"#(\d+)[^\n]*{re.escape(marker)}", out)
check(m is not None, "the report carries a numeric id")
bug_id = m.group(1)
check(f"#{bug_id} deleted" in strip(cmd(si, f"delbug {bug_id}")),
      "delbug removes the report by id")
check(marker not in strip(cmd(si, "bug")), "the deleted report is gone from the list")
check("No bug report has that number" in strip(cmd(si, f"delbug {bug_id}")),
      "deleting a nonexistent bug is rejected")

si.close()
announce_done("smoke_test_bug")
print("=== ALL CHECKS PASSED ===")
