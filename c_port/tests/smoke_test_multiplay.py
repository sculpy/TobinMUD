#!/usr/bin/env python3
"""Smoke test for the multiplay gate.

  1. Default off: a mortal account's second connected character is refused.
  2. A 59+ immortal can `multiplay on`, after which the second mortal
     character connects.
  3. `multiplay` is hidden from mortals.

    python3 tests/smoke_test_multiplay.py [host] [port]
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


announce("smoke_test_multiplay")

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


acct = f"Mpacct{_suffix}"
pw = "mppw"
char1, char2 = f"Mpone{_suffix}", f"Mptwo{_suffix}"

# --- Build an account with two mortal characters ---
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, acct); recv_all(s)
send_line(s, "y"); recv_all(s)        # confirm new account creation
send_line(s, pw); recv_all(s)
send_line(s, pw); recv_all(s)         # confirm password
send_line(s, "new"); recv_all(s)
send_line(s, char1); recv_all(s)
send_line(s, "done"); recv_all(s)     # playing char1
send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
send_line(s, "1"); recv_all(s)  # class: mage
send_line(s, "2"); recv_all(s)  # alignment: neutral
send_line(s, "quit!"); recv_all(s)    # -> account menu
send_line(s, "new"); recv_all(s)
send_line(s, char2); recv_all(s)
send_line(s, "done"); recv_all(s)     # playing char2
send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
send_line(s, "1"); recv_all(s)  # class: mage
send_line(s, "2"); recv_all(s)  # alignment: neutral
send_line(s, "quit!"); recv_all(s)    # -> account menu
s.close()


def login(char_index):
    r = socket.create_connection((host, port), timeout=5)
    recv_all(r)
    send_line(r, acct); recv_all(r)
    send_line(r, pw); recv_all(r)
    return r, cmd(r, str(char_index))


# --- Default off: second character refused ---
s1, out1 = login(1)
check("Welcome" in out1 or "Center Square" in out1, "char1 connects normally")
s2, out2 = login(2)
check("multiplaying is not allowed" in out2,
      "with multiplay off, the account's second character is refused")
s2.close()

# --- A 59 immortal turns multiplay on ---
immname = f"Mpimm{_suffix}"
si = socket.create_connection((host, port), timeout=5)
recv_all(si)
for step in (immname, "y", "mppw", "mppw", "new", immname, "done", "1", "1", "2"):
    send_line(si, step); recv_all(si)
si.close()
subprocess.run(["mariadb", "sneezy", "-e",
                f"UPDATE player_progress SET level=59 WHERE player_id="
                f"(SELECT id FROM player WHERE name='{immname}');"], check=True)
si = socket.create_connection((host, port), timeout=5)
recv_all(si)
send_line(si, immname); recv_all(si)
send_line(si, "mppw"); recv_all(si)
send_line(si, "1"); recv_all(si)
check("now ON" in cmd(si, "multiplay on"), "a 59 immortal turns multiplay on")

# --- Now the second character connects ---
s2b, out2b = login(2)
check("Welcome" in out2b or "Center Square" in out2b,
      "with multiplay on, the second character connects")

# --- gate: mortals can't multiplay ---
sm = socket.create_connection((host, port), timeout=5)
recv_all(sm)
mort = f"Mpmort{_suffix}"
for step in (mort, "y", "mppw", "mppw", "new", mort, "done", "1", "1", "2"):
    send_line(sm, step); recv_all(sm)
check("Huh?!" in cmd(sm, "multiplay off"), "multiplay is hidden from mortals")

# restore default off
cmd(si, "multiplay off")

s1.close(); s2b.close(); si.close(); sm.close()
announce_done("smoke_test_multiplay")
print("=== ALL CHECKS PASSED ===")
