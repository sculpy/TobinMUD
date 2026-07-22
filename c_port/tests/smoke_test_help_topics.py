#!/usr/bin/env python3
"""Smoke test for DB-backed help topics and the `edit help` editor
(formerly the standalone `edhelp`, folded into the unified `edit <noun>`
dispatcher, 2026-07-11):
  1. `help <command>` shows the seeded topic body (exact and by prefix);
     an unknown topic is rejected; a mortal asking for an immortal-only
     command's topic gets "no help" (no existence leak).
  2. `edit help` is invisible to mortals AND to ordinary 51-55 immortals --
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


announce("smoke_test_help_topics")

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
pw = "edhelptestpw"
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
send_line(s, "2"); recv_all(s)  # alignment: neutral

# --- Part 1: reading topics as a mortal ---
send_line(s, "help say")
out = recv_all(s)
check("-- Help: Say --" in out and "apostrophe shorthand" in out,
      "help <command> shows the seeded topic body")

send_line(s, "help sco")
out = recv_all(s)
check("-- Help: Score --" in out,
      "a topic prefix ('sco') resolves to the topic (score)")

send_line(s, "help zzznotopic")
out = recv_all(s)
check("No help available" in out, "an unknown topic is rejected")

# Alias resolution (Session 21): short forms land on the canonical topic.
send_line(s, "help nw")
out = recv_all(s)
check("-- Help: Northwest --" in out, "help nw resolves to the northwest topic")
send_line(s, "help '")
out = recv_all(s)
check("-- Help: Say --" in out, "help ' resolves to the say topic")

send_line(s, "help transfer")
out = recv_all(s)
check("No help available" in out,
      "a mortal asking about an immortal-only command's topic gets no leak")

# --- Part 1b: multi-word topic resolution (user 2026-07-18 fix -- `help`
# used to only ever look up the FIRST token, so "help cure poison" landed
# on "cure blindness" instead, once skill_help.sql's generated topics gave
# "cure" more than one same-prefixed match). Also covers the new `engage`
# alias (full alias of `hit`, same one-handler-two-table-rows pattern as
# attack/kill) and a couple of the generated skill/spell topics'
# `Requires:` footer (new trailing-directive convention, cmd_help.c). ---
send_line(s, "help cure poison")
out = recv_all(s)
check("-- Help: Cure poison --" in out,
      "help <multi-word skill name> resolves exactly, not to another same-prefixed topic")
check("Requires:" in out and "component" in out and "holy symbol" in out,
      "the cure poison topic's Requires footer lists both cast's component and pray's symbol")

send_line(s, "help gust")
out = recv_all(s)
check("-- Help: Gust --" in out and "Requires:" in out and "a rabbit's foot on a silver chain" in out,
      "a Mage-only spell topic's Requires footer names gust's real specific component "
      "(post-follow-up wording -- the generic 'keyworded component' phrasing this "
      "assertion used to check for was superseded by the same-session real-component-"
      "mapping follow-up, STATUS.md's Session 60 write-up; this assertion was stale)")

send_line(s, "help edit room")
out = recv_all(s)
check("-- Help: Edit room --" in out,
      "the pre-existing two-word 'edit <noun>' special case still resolves correctly (regression check)")

send_line(s, "help engage")
out = recv_all(s)
check("-- Help: Engage --" in out and "alias: hit" in out,
      "help engage shows the alias topic with a real Syntax/Level footer (a genuine command, not a skill)")

send_line(s, "engage")
out = recv_all(s)
check("Attack whom?" in out, "bare `engage` dispatches through the same handler as `hit`")

# --- Part 2: edit help gating ---
send_line(s, "edit help whatever")
out = recv_all(s)
check("Command not found" in out, "a mortal typing edit help gets Command not found (hidden)")

set_level(name, 51)
s.close()
s = login(name, pw)
send_line(s, "edit help whatever")
out = recv_all(s)
check("Command not found" in out, "a level-51 immortal still can't use edit help (gate is 56)")
send_line(s, "help goto")
out = recv_all(s)
check("-- Help: Goto --" in out, "the 51 immortal CAN read the goto topic now")

set_level(name, 56)
s.close()
s = login(name, pw)

# --- Part 3: the editor, end to end ---
topic = f"lore{_suffix}"
send_line(s, f"edit help {topic}")
out = recv_all(s)
check("new topic" in out, "edit help on a new topic says it's new")

send_line(s, "The world of Tobin was carved from")
recv_all(s)
send_line(s, "the bones of an older world.")
recv_all(s)
send_line(s, "/s")
out = recv_all(s)
check(f"'{topic}' saved" in out, "/s saves the topic")

send_line(s, f"help {topic}")
out = recv_all(s)
check("carved from" in out and "older world" in out,
      "help <topic> shows the freshly saved body")

send_line(s, f"edit help {topic}")
out = recv_all(s)
check("existing text below" in out and "carved from" in out,
      "re-editing preloads and shows the existing text")

send_line(s, "THIS LINE SHOULD BE DISCARDED")
recv_all(s)
send_line(s, "/a")
out = recv_all(s)
check("aborted" in out.lower(), "/a aborts the edit")

send_line(s, f"help {topic}")
out = recv_all(s)
check("SHOULD BE DISCARDED" not in out and "carved from" in out,
      "the aborted line was not saved; the original body survives")

# hygiene: demote back to mortal so this fixture doesn't linger as a 56
set_level(name, 1)
s.close()
announce_done("smoke_test_help_topics")
print("=== ALL CHECKS PASSED ===")
