#!/usr/bin/env python3
"""Smoke test for telnet IAC handling in descriptor.c's input parser:
  1. A complete IAC SB ... IAC SE subnegotiation (as Mudlet/MUSHclient send
     for TTYPE/NAWS) is swallowed whole -- its payload never leaks into the
     line buffer as typed input.
  2. The same subnegotiation SPLIT ACROSS TWO TCP SENDS (the historical bug:
     the parser used to lose the "inside SB" state between reads, so the
     second half was typed into the player's line) is also swallowed whole.
  3. A split WILL/WONT/DO/DONT triple and a lone trailing IAC still resume
     correctly (regression guard for the already-working rewind paths).

    python3 tests/smoke_test_telnet_iac.py [host] [port]
"""
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


announce("smoke_test_telnet_iac")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

IAC, SB, SE, WILL, DO = 255, 250, 240, 251, 253
NAWS, TTYPE = 31, 24


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


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def make_player(tag):
    name = f"Iac{tag}{_suffix}"
    pw = "iactestpw123"
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "y")
    recv_all(s)
    send_line(s, pw)
    recv_all(s)
    send_line(s, pw)  # confirm password (Session 21)
    recv_all(s)
    send_line(s, "new")
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done")
    recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    return s, name


s, name = make_player("A")

# --- Part 1: complete subnegotiation in one send, then a real command ---
naws = bytes([IAC, SB, NAWS, 0, 80, 0, 24, IAC, SE])
s.sendall(naws + b"score\r\n")
out = recv_all(s)
check(name.capitalize() in out or "Level" in out,
      "a complete IAC SB..SE followed by 'score' still runs score normally")
check("Huh?" not in out, "no leaked subnegotiation bytes produced an unknown command")

# --- Part 2: subnegotiation split across two sends (the historical bug) ---
ttype_payload = bytes([IAC, SB, TTYPE, 0]) + b"xterm-256color"
s.sendall(ttype_payload[:7])   # first half: opener + part of the payload
time.sleep(0.4)                # force a separate read() on the server
s.sendall(ttype_payload[7:] + bytes([IAC, SE]) + b"score\r\n")
out = recv_all(s)
check("Huh?" not in out and "xterm" not in out,
      "a subnegotiation split across two reads leaks nothing into the line buffer")
check("Level" in out, "'score' typed right after the split subnegotiation still works")

# --- Part 3: split WILL triple and lone trailing IAC still resume ---
s.sendall(bytes([IAC]))
time.sleep(0.4)
s.sendall(bytes([WILL, TTYPE]) + b"score\r\n")
out = recv_all(s)
check("Level" in out and "Huh?" not in out,
      "an IAC WILL split after the lone IAC byte is still consumed cleanly")

s.sendall(bytes([IAC, DO]))
time.sleep(0.4)
s.sendall(bytes([TTYPE]) + b"score\r\n")
out = recv_all(s)
check("Level" in out and "Huh?" not in out,
      "an IAC DO split before the option byte is still consumed cleanly")

s.close()
announce_done("smoke_test_telnet_iac")
print("=== ALL CHECKS PASSED ===")
