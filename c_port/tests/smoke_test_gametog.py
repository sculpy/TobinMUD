#!/usr/bin/env python3
"""Smoke test for `gametog` (TODO.md-planned split of `toggle`, done
2026-07-11): global game-wide switches (currently just multiplay) moved
out of the mortal-facing `toggle` command into their own 58+ command.

  1. `gametog` is hidden from a 51-57 immortal (Command not found).
  2. A 58+ immortal sees and can flip the multiplay game toggle via
     `gametog`.
  3. `toggle` no longer lists OR accepts "multiplay" at all -- not even
     for a 58+ immortal -- since it moved out entirely.

    python3 tests/smoke_test_gametog.py [host] [port]
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


announce("smoke_test_gametog")

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


def set_level(name, level):
    subprocess.run(["mariadb", "sneezy", "-e",
                    f"UPDATE player_progress SET level={level} WHERE player_id="
                    f"(SELECT id FROM player WHERE name='{name}');"], check=True)


name = f"Gtog{_suffix}"
pw = "gtogpw123"

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "y"); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "new"); recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
send_line(s, "1"); recv_all(s)  # class: mage
send_line(s, "done"); recv_all(s)
send_line(s, "done"); recv_all(s)  # alignment: neutral
cmd(s, "color off")

# --- a 51 immortal (below 58) is gated out of gametog ---
set_level(name, 51)
s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")
check("Command not found" in cmd(s, "gametog"), "gametog is hidden below level 58")

# --- toggle no longer shows or accepts multiplay at all, even at 51 ---
out = cmd(s, "toggle")
check("multiplay" not in out.lower(), "toggle no longer lists multiplay")
out = cmd(s, "toggle multiplay")
check("No such toggle" in out, "toggle no longer accepts 'multiplay' by name")

# --- promote to 58 and use gametog ---
set_level(name, 58)
s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

out = cmd(s, "gametog")
check("Game Toggles" in out and "multiplay" in out.lower(),
      "a 58+ immortal sees the multiplay game toggle via gametog")

out = cmd(s, "gametog multiplay")
check("multiplay is now" in out.lower(), "a 58+ immortal can flip multiplay via gametog")
cmd(s, "gametog multiplay")  # flip back to restore the default

s.close()
announce_done("smoke_test_gametog")
print("=== ALL CHECKS PASSED ===")
