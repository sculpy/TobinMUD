#!/usr/bin/env python3
"""Smoke test for `edbug <id> [note]` (TODO.md-planned): resolve a filed
bug report in place instead of only being able to `delbug` (delete) it,
so the submitter can be told it was fixed.

  1. `edbug` is hidden from a mortal.
  2. `edbug <id> <note>` marks the bug resolved and the still-online
     submitter gets a live notice with the note.
  3. A resolved bug no longer shows up in the bare `bug` listing, but the
     report itself still exists (not deleted -- `delbug` still works on it
     afterward, proving the row survived).
  4. `edbug`-ing an already-resolved id is refused.

    python3 tests/smoke_test_edbug.py [host] [port]
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


announce("smoke_test_edbug")

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


reporter_name = f"Bugrep{_suffix}"
reporter_pw = "bugreppw123"
imm_name = f"Edbugimm{_suffix}"
imm_pw = "edbugimmpw123"

reporter_text = f"the door in room {_suffix} won't open"

# --- the reporter files a bug and STAYS online (color off, idle) ---
sr = socket.create_connection((host, port), timeout=5)
make_char(sr, reporter_name, reporter_pw)
out = cmd(sr, f"bug {reporter_text}")
check("has been filed" in out, "the reporter files a bug report")

# --- a 51 immortal can't see edbug ---
si = socket.create_connection((host, port), timeout=5)
make_char(si, imm_name, imm_pw)
set_level(imm_name, 51)
si.close()
si = login(imm_name, imm_pw)
check("Command not found" in cmd(si, "edbug 1 fixed it"), "edbug is hidden below level 59")

# --- promote to 59 and find the bug's real id ---
set_level(imm_name, 59)
si.close()
si = login(imm_name, imm_pw)

out = cmd(si, "bug")
bug_id = None
for line in out.splitlines():
    if reporter_text in line:
        bug_id = line.split("#", 1)[1].split()[0]
        break
check(bug_id is not None, "the filed bug is found in the outstanding list")

note = "Fixed in the next patch, thanks!"
out = cmd(si, f"edbug {bug_id} {note}")
check("marked resolved" in out, "edbug confirms the bug is resolved")

# --- the still-online reporter gets a live notice ---
notice = recv_all(sr, timeout=1.0)
check(note in notice and reporter_text in notice,
      "the online reporter receives a live notice with the note")

# --- resolved bug no longer shows in the outstanding list ---
out = cmd(si, "bug")
check(reporter_text not in out, "the resolved bug no longer appears in the outstanding bug list")

# --- edbug-ing it again is refused ---
out = cmd(si, f"edbug {bug_id} again")
check("already resolved" in out.lower(), "edbug refuses an already-resolved bug id")

# --- the row still exists (not deleted) -- delbug can still remove it ---
out = cmd(si, f"delbug {bug_id}")
check("deleted" in out.lower(), "the resolved bug's row still existed -- delbug can remove it")

sr.close()
si.close()
announce_done("smoke_test_edbug")
print("=== ALL CHECKS PASSED ===")
