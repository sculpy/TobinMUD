#!/usr/bin/env python3
"""Smoke test for pager held-messages + the colorized MORE prompt (Session
43 continued, user: "silence all messaging like youve done for the
editors, but for pagination. also colorize the [ ENTER for more, Q to
stop ] line like my example"). Covers:
  1. The MORE prompt is colorized (cyan brackets, bright-cyan ENTER/Q) and
     sits on its own line.
  2. A player mid-pager (reading `news` a page at a time) receives nothing
     from a roommate's `say` -- same "no interruptions" treatment as the
     editors (descriptor_in_editor() now also covers page_len > 0).
  3. Once the pager is drained, `catchup` -- widened from immortal-only to
     mortal-level for exactly this reason -- replays the held message.

    python3 tests/smoke_test_pager_held.py [host] [port]
"""
import socket
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


announce("smoke_test_pager_held")

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


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, ""); recv_all(s)   # color default (Y)
    send_line(s, ""); recv_all(s)   # timezone default (blank)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    cmd(s, "1")  # race: human (zero stat modifier)
    cmd(s, "1")  # class: mage
    cmd(s, "done")
    cmd(s, "2")  # alignment: neutral
    return s


nameA = f"Pgha{_suffix}"
nameB = f"Pghb{_suffix}"
pw = "pagerheldpw123"

A = make_char(nameA, pw)
B = make_char(nameB, pw)
# A is left with color ON (default) -- checking #1 needs the raw ANSI codes.

# --- 1: colorized MORE prompt, small page size so it definitely triggers ---
out = cmd(A, "news 1")
check("\r\n\x1b[0;36m[ \x1b[1;36mENTER\x1b[0;36m for more, \x1b[1;36mQ\x1b[0;36m to stop ]\x1b[0m" in out,
      "the MORE prompt is colorized (cyan brackets, bright-cyan ENTER/Q) on its own line")

# --- 2: A is mid-pager; B's `say` must not reach A ---
cmd(B, "say pager should hold this")
time.sleep(0.3)
leaked = recv_all(A, 0.5)
check("pager should hold this" not in leaked,
      "a message sent while a player is mid-pager does not interrupt them")

# --- 3: drain the pager, then catchup (mortal-level) replays it ---
guard = 0
while "ENTER" in out and guard < 200:
    out = cmd(A, "")
    guard += 1
check(guard < 200, "the pager actually drained (didn't loop forever)")

out = cmd(A, "catchup")
check("pager should hold this" in out,
      "catchup (now mortal-level) replays the message held during pagination")
check("haven't missed anything" in cmd(A, "catchup"),
      "catchup is empty after reading")

A.close(); B.close()
announce_done("smoke_test_pager_held")
print("=== ALL CHECKS PASSED ===")
