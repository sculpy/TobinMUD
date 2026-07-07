#!/usr/bin/env python3
"""Smoke test for socials (socials.c, cmd_socials.c + the dispatch hook).

  1. An untargeted social shows a self message and a room message.
  2. A targeted social shows self / target / room messages.
  3. `socials` lists the available verbs.
  4. Aiming at someone not present is rejected.

    python3 tests/smoke_test_socials.py [host] [port]
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


announce("smoke_test_socials")

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
    for step in (nm, "socpw", "socpw", "new", nm, "done"):
        send_line(s, step); recv_all(s)
    return s


# Two fresh mortals both land in Center Square (100) -- same room.
nameA, nameB = f"Soca{_suffix}", f"Socb{_suffix}"
sA = make_char(nameA)
sB = make_char(nameB)
recv_all(sA); recv_all(sB)   # drain arrival notices

# 1: untargeted social
out = cmd(sA, "smile")
check("You smile." in out, "an untargeted social shows the self message")
check("smiles" in recv_all(sB), "the room sees the untargeted social")

# 2: targeted social
out = cmd(sA, f"smile {nameB}")
check(f"You smile at {nameB}." in out, "a targeted social names the target for the actor")
check("smiles at you" in recv_all(sB), "the target sees the aimed social")

# 3: list
out = cmd(sA, "socials")
check("smile" in out and "wave" in out and "bow" in out, "socials lists the verbs")

# 4: absent target
check("aren't here" in cmd(sA, f"smile Nobody{_suffix}"),
      "aiming a social at someone not present is rejected")

# 5: abbreviation -- a prefix resolves to the closest social (poi -> point).
check("point around randomly" in cmd(sA, "poi"),
      "'poi' abbreviates to the point social")
check("point around randomly" in cmd(sA, "poin"),
      "'poin' abbreviates to the point social")
check("smile" in cmd(sA, "smi"), "'smi' abbreviates to smile")

sA.close(); sB.close()
announce_done("smoke_test_socials")
print("=== ALL CHECKS PASSED ===")
