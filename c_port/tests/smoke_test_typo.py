#!/usr/bin/env python3
"""Smoke test for `typo` / `deltypo` (cmd_typo.c, typo_repo.c) -- a direct
mirror of `bug`/`idea` (smoke_test_bug.py/smoke_test_idea.py), same shape,
different table (user, 2026-08-02: "add a typo command in the same way"):
  1. A mortal files a typo report with `typo <text>` and gets a thank-you.
  2. Bare `typo` from a mortal shows usage; from an immortal lists reports.
  3. The filed report shows the submitter, its room vnum, and appears in
     the immortal list.
  4. `deltypo <id>` (59+) removes it; a mortal can't see deltypo (Command
     not found).

    python3 tests/smoke_test_typo.py [host] [port]
"""
import re
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


announce("smoke_test_typo")

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
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def make_char(nm):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, "typopw"); recv_all(s)
    send_line(s, "typopw"); recv_all(s)
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
    send_line(r, "typopw"); recv_all(r)
    send_line(r, "1"); recv_all(r)
    return r


# --- mortal files a typo report ---
nameM = f"Typom{_suffix}"
sm = make_char(nameM)
marker = f"widget{_suffix}"
check("filed" in strip(cmd(sm, f"typo the {marker} description has a typo")).lower(),
      "a mortal can file a typo report and is thanked")
check("Usage: typo" in strip(cmd(sm, "typo")), "bare typo from a mortal shows usage")
check("Command not found" in cmd(sm, "deltypo 1"), "a mortal cannot see deltypo (Command not found)")
sm.close()

# --- immortal lists + deletes ---
nameI = f"Typoi{_suffix}"
make_char(nameI).close()
sql(f"UPDATE player_progress SET level=59 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{nameI}');")
si = relogin(nameI)

out = strip(cmd(si, "typo"))
check("Typo reports" in out, "immortal bare typo shows the report list header")
check(marker in out and nameM.capitalize() in out,
      "the report lists its text and the submitter's name")
check("(room 100)" in out, "the report shows the room vnum the submitter was standing in")

# find the id for our marker and delete it
m = re.search(rf"#(\d+)[^\n]*{re.escape(marker)}", out)
check(m is not None, "the report carries a numeric id")
typo_id = m.group(1)
check(f"#{typo_id} deleted" in strip(cmd(si, f"deltypo {typo_id}")),
      "deltypo removes the report by id")
check(marker not in strip(cmd(si, "typo")), "the deleted report is gone from the list")
check("No typo report has that number" in strip(cmd(si, f"deltypo {typo_id}")),
      "deleting a nonexistent typo report is rejected")

si.close()
announce_done("smoke_test_typo")
print("=== ALL CHECKS PASSED ===")
