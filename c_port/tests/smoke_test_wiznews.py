#!/usr/bin/env python3
"""Smoke test for the immortal news channel (cmd_wiznews.c, cmd_edwiznews.c,
news_repo.c wiz path, wiznews.sql).

  1. `wiznews` is hidden from mortals (level 51+).
  2. An immortal reads wiznews (paged, newest first) and sees the seed item.
  3. `edwiznews` (56+) posts an item that wiznews then shows.
  4. Immortal news stays OUT of the public `news` feed (separate channels).

    python3 tests/smoke_test_wiznews.py [host] [port]
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


announce("smoke_test_wiznews")

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


def read_all(sock, command):
    """Page through a news-style feed, returning the full text, draining the pager."""
    first = cmd(sock, command)
    full = first
    resp = first
    guard = 0
    while "ENTER for more" in resp and guard < 60:
        resp = cmd(sock, "")
        full += resp
        guard += 1
    return full


name = f"Wizn{_suffix}"
pw = "wiznpw"

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "new"); recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "done"); recv_all(s)

check("Huh?!" in cmd(s, "wiznews"), "wiznews is hidden from mortals (51+)")
check("Huh?!" in cmd(s, "edwiznews Nope"), "edwiznews is hidden from mortals")

# Promote to 56 (can read AND post) and reconnect.
set_level(name, 56)
s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

wn = read_all(s, "wiznews")
check("Immortal News" in wn, "an immortal reads the wiznews channel")
check("Immortal News Arrives" in wn, "the seeded wiznews item is shown")

# Post an immortal-only item.
headline = f"Staff Meeting {_suffix}"
out = cmd(s, f"edwiznews {headline}")
check("Writing immortal news" in out, "edwiznews opens the story editor")
cmd(s, "The council convenes at the appointed hour.")
check("Immortal news posted" in cmd(s, "/s"), "edwiznews saves the item")

wn = read_all(s, "wiznews")
check(headline in wn, "the posted item shows up in wiznews")

# It must NOT leak into the public news feed.
pub = read_all(s, "news")
check(headline not in pub, "immortal news does not appear in the public news feed")

check("immortals'' news channel".replace("''", "'") in cmd(s, "help wiznews")
      or "immortal" in cmd(s, "help wiznews").lower(),
      "help wiznews describes the channel")

set_level(name, 1)

# Clean up the item this run posted -- otherwise repeated runs pile up
# wiznews entries and eventually push the seeded item off the display's
# most-recent window (same fix as smoke_test_news.py).
subprocess.run(["mariadb", "sneezy", "-e",
                f"DELETE FROM wiznews WHERE title = '{headline}';"],
               check=True)

s.close()
announce_done("smoke_test_wiznews")
print("=== ALL CHECKS PASSED ===")
