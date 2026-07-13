#!/usr/bin/env python3
"""Smoke test for `news` / `ednews` (cmd_news.c, cmd_ednews.c, the pager in
descriptor.c, news_repo.c, news.sql).

  1. `news` shows the whole feed, newest first, a PAGE AT A TIME (ENTER for
     more, Q to stop). No digits anywhere in the news (user rule).
  2. `news <n>` sets the page size (a small page paginates; a big page shows
     more per page).
  3. `ednews` (level 56+) posts a headline + story that `news` then shows.
  4. help topics describe both commands.

    python3 tests/smoke_test_news.py [host] [port]
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


announce("smoke_test_news")

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


def news_read(sock, arg=""):
    """Returns (first_page, full_feed) and fully drains the pager so the next
    command isn't swallowed by a pending 'more' prompt."""
    first = cmd(sock, f"news {arg}" if arg else "news")
    full = first
    resp = first
    guard = 0
    while "ENTER for more" in resp and guard < 60:
        resp = cmd(sock, "")
        full += resp
        guard += 1
    return first, full


def set_level(name, level):
    subprocess.run(["mariadb", "sneezy", "-e",
                    f"UPDATE player_progress SET level={level} WHERE player_id="
                    f"(SELECT id FROM player WHERE name='{name}');"], check=True)


name = f"Newsy{_suffix}"
pw = "newspw"

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
send_line(s, "2"); recv_all(s)  # alignment: neutral
cmd(s, "color off")   # strip <c>/<z> so ANSI escapes don't add digits

_, full = news_read(s)
check("TobinMUD News" in full, "a mortal can read the news")
check("The Room Builder Reborn" in full and "Doorways Arrive" in full,
      "the whole feed is shown after paging through")
idx_new = full.find("News Grows a Voice")     # inserted latest -> newest
idx_old = full.find("The Room Builder Reborn")
check(idx_new != -1 and idx_old != -1 and idx_new < idx_old,
      "news is ordered newest-first")

news_body = full.split("TobinMUD News", 1)[1]
digits = [c for c in news_body if c.isdigit()]
check(not digits, f"news items contain no digits (found: {digits[:5]})")

# page-size option: a small page paginates; a big page shows more per page.
small_first, _ = news_read(s, "10")
big_first, _ = news_read(s, "200")
check("ENTER for more" in small_first, "a small page size paginates (more prompt shown)")
check(len(big_first.splitlines()) > len(small_first.splitlines()),
      "a bigger page size shows more lines per page")

check("page at a time" in cmd(s, "help news"), "help news describes pagination")

# edit news gate: a mortal can't (hidden).
check("Command not found" in cmd(s, "edit news Nope"), "edit news is hidden from mortals")

# --- promote to 56 and post a news item ---
set_level(name, 56)
s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

headline = f"Fresh Off The Press {_suffix}"
out = cmd(s, f"edit news {headline}")
check("Writing news" in out, "edit news opens the story editor with the headline")
cmd(s, "Something new is happening in the realm today.")
check("News posted" in cmd(s, "/s"), "edit news saves the story")
_, full = news_read(s)
check(headline in full, "the posted item shows up in news, newest first")

check("post a news item" in cmd(s, "help edit news"), "help edit news describes the command")

set_level(name, 1)

# Clean up the item this run posted -- otherwise repeated runs pile up news
# entries and eventually push the real (older) headlines off the news
# display's most-recent window, which fails the "whole feed is shown" check.
subprocess.run(["mariadb", "sneezy", "-e",
                f"DELETE FROM news WHERE title = 'Fresh Off The Press {_suffix}';"],
               check=True)

s.close()
announce_done("smoke_test_news")
print("=== ALL CHECKS PASSED ===")
