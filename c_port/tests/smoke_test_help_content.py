#!/usr/bin/env python3
"""Smoke test for the detailed help catch-up (user 2026-07-12: "catch up
on the help file entries... i want very detailed help files. Especially
wiz* help files... so a first time player of this game will feel
comfortable playing... and administration detailed so new immortals can
know what commands do and why we use them"). Covers:

  1. A brand-new character creation shows the one-time "help playing"
     nudge (and a later relog does NOT repeat it).
  2. `help playing` and `help administration` (both immortal-gated
     reads -- administration only shown to an actual immortal) render
     real content.
  3. Bare `help`/`wizhelp` footers point at the two new topics.
  4. A representative sample of the newly-added command topics
     (previously missing entirely) resolve to real content instead of
     "No help available".

    python3 tests/smoke_test_help_content.py [host] [port]
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


announce("smoke_test_help_content")

_suffix = "".join(chr(ord("a") + (int(time.time() * 1000) // 26**i) % 26) for i in range(4))


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
    # Normalized to bare \n so a needle string spanning the server's own
    # word-wrap line breaks (\r\n) can still match with a plain \n.
    return b"".join(chunks).decode(errors="replace").replace("\r\n", "\n")


def send_line(sock, line):
    sock.sendall((line + "\r\n").encode())


def cmd(sock, line, timeout=1.0, max_pages=15):
    """Drains a paginated reply too (help/wizhelp now page past ~20 lines,
    2026-07-17 general-pagination sweep) -- a no-op for anything short."""
    send_line(sock, line)
    out = recv_all(sock, timeout)
    pages = 0
    while "ENTER" in out and "more" in out and pages < max_pages:
        send_line(sock, "")
        out += recv_all(sock, timeout)
        pages += 1
    return out


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def sql(stmt):
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


pw = "helpcontentpw123"

# --- 1: the one-time creation nudge, and it does not repeat on relog ---
mort_name = f"Helpcmor{_suffix}"
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, mort_name); recv_all(s)
send_line(s, "y"); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "new"); recv_all(s)
send_line(s, mort_name); recv_all(s)
send_line(s, "1"); recv_all(s)
send_line(s, "1"); recv_all(s)
send_line(s, "done"); recv_all(s)
out = cmd(s, "2")  # alignment: neutral -- finishes creation, enters the world
check("New to TobinMUD? Type 'help playing'" in out, "a brand-new character sees the one-time playing nudge")
cmd(s, "quit!")
s.close()

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, mort_name); recv_all(s)
out = cmd(s, pw)
send_line(s, "1")
out += recv_all(s)
check("New to TobinMUD?" not in out, "a later relog does not repeat the nudge")
cmd(s, "color off")

# --- 2/4: help playing + a sample of the newly-added command topics ---
out = cmd(s, "help playing")
check("LOOK AROUND" in out and "LEAVING THE GAME" in out, "help playing renders the full overview")

for topic, needle in (
    ("save", "reassurance of doing it yourself"),
    ("rent", "RECOMMENDED way to leave"),
    ("cast", "spell component"),
    ("pray", "holy symbol"),
    ("practice", "guildmaster"),
    ("skills", "Combat, <Class> Skills"),
    ("consider", "plain-English verdict"),
    ("tell", "no need\nto share a room"),
    ("whisper", "conversation happened"),
):
    out = cmd(s, f"help {topic}")
    check(needle in out, f"help {topic} renders real content, not a stale/missing topic")

# --- 3: help's own footer points at playing ---
out = cmd(s, "help")
check("help playing" in out, "bare help's footer points at help playing")

# --- 2/3 for wizhelp/administration: needs an actual immortal ---
cmd(s, "quit!")  # not a raw close -- see the quit!-ordering lesson elsewhere
s.close()
sql(f"UPDATE player_progress SET level=60 WHERE player_id="  # 60: sees every gated topic below (stat 55+, copyover/balance 60+)
    f"(SELECT id FROM player WHERE name='{mort_name}');")
s2 = socket.create_connection((host, port), timeout=5)
recv_all(s2)
send_line(s2, mort_name); recv_all(s2)
send_line(s2, pw); recv_all(s2)
send_line(s2, "1"); recv_all(s2)
cmd(s2, "color off")

out = cmd(s2, "help administration")
check("THE LADDER" in out and "THE DANGEROUS ONES" in out, "help administration renders the full immortal guide")
out = cmd(s2, "wizhelp")
check("help administration" in out, "wizhelp's own footer points at help administration")

for topic, needle in (
    ("stat", "decoded to\nreadable text"),
    ("balance", "gamewide\ncombat modifiers"),
    ("hurtlimb", "bypassing real\ncombat entirely"),
    ("aitick", "collapses real time"),
    # copyover deliberately excluded: it turned out to already carry a
    # hand-edited topic (in-game `edit help`, updated_by != 'seed') --
    # the seed file's ON DUPLICATE KEY UPDATE name=name correctly left
    # it alone rather than clobbering a real immortal's own edit.
):
    out = cmd(s2, f"help {topic}")
    check(needle in out, f"help {topic} renders real content, not a stale/missing topic")

out = cmd(s2, "help copyover")
check("reboot" in out.lower(), "help copyover still renders real content (a pre-existing hand-edited topic)")

s.close()
s2.close()
announce_done("smoke_test_help_content")
print("=== ALL CHECKS PASSED ===")
