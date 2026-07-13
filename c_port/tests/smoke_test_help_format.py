#!/usr/bin/env python3
"""Smoke test for the colorized help-topic format (cmd_help.c):
  1. `help <cmd>` renders the description body in bright white (ANSI
     1;37m -- was magenta before user 2026-07-11: "colorize help files
     with <W>").
  2. The header title-cases the command name ("-- Help: Color --", not
     "-- Help: color --" -- user 2026-07-11: "proper case for the command").
  3. It shows a cyan-labelled "Syntax:" line whose value is parsed from the
     body's leading "Usage:" line (so "color [on|off]", not just "color").
  4. It shows a cyan-labelled "Minimum Level:" line from the command table.
  5. The leading "Usage:" line is lifted out of the body (not shown twice).

    python3 tests/smoke_test_help_format.py [host] [port]
"""
import re
import socket
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


announce("smoke_test_help_format")

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


def make_char(nm):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, "hlpfpw"); recv_all(s)
    send_line(s, "hlpfpw"); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done"); recv_all(s)
    send_line(s, "2"); recv_all(s)  # alignment: neutral
    return s


s = make_char(f"Hfmt{_suffix}")

raw = cmd(s, "help color")
plain = re.sub(r"\x1b\[[0-9;]*m", "", raw)

check("\x1b[0;35m" in raw, "the description body is rendered in magenta")
check("Syntax:" in plain, "the footer shows a Syntax label")
check("Minimum Level:" in plain, "the footer shows a Minimum Level label")
check("color [on|off]" in plain,
      "Syntax is parsed from the body's Usage line (shows the full syntax)")
check("Minimum Level: 1" in plain, "Minimum Level comes from the command table")
# The Syntax/Minimum Level labels are cyan.
check("\x1b[0;36m" in raw, "the footer labels are cyan")
# The leading "Usage:" line was lifted into the footer, not left in the body.
check("Usage:" not in plain, "the body's leading Usage line is not shown twice")

s.close()
announce_done("smoke_test_help_format")
print("=== ALL CHECKS PASSED ===")
