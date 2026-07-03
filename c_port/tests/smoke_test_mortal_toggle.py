#!/usr/bin/env python3
"""Smoke test for the mortal/immortal toggle (Session 21):
  1. A real mortal typing `immort` gets "Huh?!" -- nothing leaks.
  2. An immortal (56 via SQL) uses `mortal`: score shows Level 50 (no
     rank title), immortal commands (goto) are gone, and wait-states
     apply again.
  3. Mortality persists: quit! to the menu and re-enter -- still mortal.
  4. `immort` restores the exact stored rank; goto works again.

    python3 tests/smoke_test_mortal_toggle.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
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


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


name = f"Togimm{_suffix}"
pw = "togglepw"
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name)
recv_all(s)
send_line(s, pw)
recv_all(s)
send_line(s, "new")
recv_all(s)
send_line(s, name)
recv_all(s)
send_line(s, "done")
recv_all(s)

# --- Part 1: a real mortal gets nothing from immort (or mortal) ---
send_line(s, "immort")
check("Huh?!" in recv_all(s), "a real mortal typing immort gets Huh?! (no leak)")
send_line(s, "mortal")
check("Huh?!" in recv_all(s), "a real mortal typing mortal gets Huh?! (51+ command)")

subprocess.run(
    ["mariadb", "sneezy", "-e",
     f"UPDATE player_progress SET level=56 WHERE player_id=(SELECT id FROM player WHERE name='{name}');"],
    check=True,
)
send_line(s, "quit!")
recv_all(s)
send_line(s, "1")
recv_all(s)

# --- Part 2: become mortal ---
send_line(s, "mortal")
out = recv_all(s)
check("walk the world as a mortal" in out, "mortal confirms the descent")

send_line(s, "score")
out = recv_all(s)
check("Level:" in out and "50" in out and "Immortal" not in out and "God" not in out,
      "score shows a plain level 50, no rank title")

send_line(s, "goto 0")
out = recv_all(s)
check("Huh?!" in out, "immortal commands are out of reach while mortal")

# --- Part 3: mortality persists across quit!/re-enter ---
send_line(s, "quit!")
recv_all(s)
send_line(s, "1")
recv_all(s)
send_line(s, "goto 0")
out = recv_all(s)
check("Huh?!" in out, "still mortal after a relog (true_level persisted)")

# --- Part 4: immort restores the stored rank ---
send_line(s, "immort")
out = recv_all(s)
check("divinity floods back" in out and "56" in out,
      "immort restores the exact stored rank (56)")
send_line(s, "goto 0")
out = recv_all(s)
check("The Void" in out, "immortal commands work again after immort")

send_line(s, "immort")
out = recv_all(s)
check("Huh?!" in out, "immort while already immortal acts like an unknown command")

# hygiene
subprocess.run(
    ["mariadb", "sneezy", "-e",
     f"UPDATE player_progress SET level=1, true_level=0 WHERE player_id=(SELECT id FROM player WHERE name='{name}');"],
    check=True,
)
s.close()
print("=== ALL CHECKS PASSED ===")
