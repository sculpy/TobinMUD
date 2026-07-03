#!/usr/bin/env python3
"""Smoke test for DB-backed help topics and the `hedit` editor:
  1. `help <command>` shows the seeded topic body (exact and by prefix);
     an unknown topic is rejected; a mortal asking for an immortal-only
     command's topic gets "no help" (no existence leak).
  2. `hedit` is invisible to mortals AND to ordinary 51-55 immortals --
     its gate is level 56+ (user-specified).
  3. The editor works end-to-end: create a topic, type lines, '.' saves,
     `help <topic>` then shows it; re-edit preloads the existing text;
     '~' aborts without saving changes.

    python3 tests/smoke_test_help_topics.py [host] [port]
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


def set_level(name, level):
    subprocess.run(
        ["mariadb", "sneezy", "-e",
         f"UPDATE player_progress SET level={level} WHERE player_id=(SELECT id FROM player WHERE name='{name}');"],
        check=True,
    )


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, pw)
    recv_all(s)
    send_line(s, "1")
    recv_all(s)
    return s


name = f"Helpedit{_suffix}"
pw = "hedittestpw"
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

# --- Part 1: reading topics as a mortal ---
send_line(s, "help say")
out = recv_all(s)
check("-- Help: say --" in out and "apostrophe shorthand" in out,
      "help <command> shows the seeded topic body")

send_line(s, "help sc")
out = recv_all(s)
check("-- Help: score --" in out, "a topic prefix ('sc') resolves to the topic (score)")

send_line(s, "help zzznotopic")
out = recv_all(s)
check("No help available" in out, "an unknown topic is rejected")

# Alias resolution (Session 21): short forms land on the canonical topic.
send_line(s, "help nw")
out = recv_all(s)
check("-- Help: northwest --" in out, "help nw resolves to the northwest topic")
send_line(s, "help '")
out = recv_all(s)
check("-- Help: say --" in out, "help ' resolves to the say topic")

send_line(s, "help goto")
out = recv_all(s)
check("No help available" in out,
      "a mortal asking about an immortal-only command's topic gets no leak")

# --- Part 2: hedit gating ---
send_line(s, "hedit whatever")
out = recv_all(s)
check("Huh?!" in out, "a mortal typing hedit gets Huh?! (hidden)")

set_level(name, 51)
s.close()
s = login(name, pw)
send_line(s, "hedit whatever")
out = recv_all(s)
check("Huh?!" in out, "a level-51 immortal still can't use hedit (gate is 56)")
send_line(s, "help goto")
out = recv_all(s)
check("-- Help: goto --" in out, "the 51 immortal CAN read the goto topic now")

set_level(name, 56)
s.close()
s = login(name, pw)

# --- Part 3: the editor, end to end ---
topic = f"lore{_suffix}"
send_line(s, f"hedit {topic}")
out = recv_all(s)
check("new topic" in out, "hedit on a new topic says it's new")

send_line(s, "The world of Tobin was carved from")
recv_all(s)
send_line(s, "the bones of an older world.")
recv_all(s)
send_line(s, ".")
out = recv_all(s)
check(f"'{topic}' saved" in out, "'.' saves the topic")

send_line(s, f"help {topic}")
out = recv_all(s)
check("carved from" in out and "older world" in out,
      "help <topic> shows the freshly saved body")

send_line(s, f"hedit {topic}")
out = recv_all(s)
check("existing text below" in out and "carved from" in out,
      "re-editing preloads and shows the existing text")

send_line(s, "THIS LINE SHOULD BE DISCARDED")
recv_all(s)
send_line(s, "~")
out = recv_all(s)
check("aborted" in out.lower(), "'~' aborts the edit")

send_line(s, f"help {topic}")
out = recv_all(s)
check("SHOULD BE DISCARDED" not in out and "carved from" in out,
      "the aborted line was not saved; the original body survives")

# hygiene: demote back to mortal so this fixture doesn't linger as a 56
set_level(name, 1)
s.close()
print("=== ALL CHECKS PASSED ===")
