#!/usr/bin/env python3
"""Smoke test for "News follow-ups" (user 2026-07-17 batch: "edit/delete
existing news in-game (addnews only creates); show unseen news at login
(per-player last-seen)").

  1. `edit news <headline>` re-run under an EXISTING headline preloads the
     existing body ("existing text below") and overwrites it in place on
     save (news_repo_upsert), instead of failing as a duplicate title.
  2. `edit news delete <headline>` removes an item outright.
  3. `edit wiznews` gets the identical treatment (same underlying gap, same
     shared EDIT_NEWS/EDIT_WIZNEWS save path in descriptor.c).
  4. A player who hasn't read `news` since something new was posted sees a
     one-line, number-free notice at login; running `news` clears it.

    python3 tests/smoke_test_news_followups.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, check, sql, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_news_followups", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def recv_all(sock, timeout=1.0, idle_gap=0.3):
    """Idle-gap-vs-deadline: waits up to `timeout` total, but returns as soon
    as `idle_gap` passes with nothing new (doesn't always burn the full
    timeout on an instant reply)."""
    sock.settimeout(idle_gap)
    chunks = []
    deadline = time.time() + timeout
    try:
        while time.time() < deadline:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
    except socket.timeout:
        pass
    return b"".join(chunks).decode(errors="replace")


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    return recv_all(sock, timeout)


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def make_char(name, pw, race="1", cls="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, race); recv_all(s)
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, cls); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    cmd(s, "color off")
    return s


def reconnect(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    out = recv_all(s)
    send_line(s, name); out += recv_all(s)
    send_line(s, pw); out += recv_all(s)
    out += cmd(s, "1")
    cmd(s, "color off")
    return s, out


imm_name, imm_pw = f"Nfimm{_suffix}", "nfimmpw123"
mo_name, mo_pw = f"Nfmo{_suffix}", "nfmopw123"

s_imm = make_char(imm_name, imm_pw)
s_imm.close()
set_level(imm_name, 56)
s_imm, _ = reconnect(imm_name, imm_pw)

headline = f"Followups Landed {_suffix}"
wiz_headline = f"Immortal Followups {_suffix}"

# --- 1. edit news: create, then re-edit in place (no duplicate-title error) ---
out = cmd(s_imm, f"edit news {headline}")
check("(new)" in out, "edit news on a fresh headline offers a blank story")
cmd(s_imm, "Original story text.")
check("News posted" in cmd(s_imm, "/s"), "edit news saves a new item")

out = cmd(s_imm, f"edit news {headline}")
check("existing text below" in out and "Original story text." in out,
      "re-editing an existing headline preloads its current body")
check("Posting failed" not in cmd(s_imm, "/s"),
      "re-saving the same headline overwrites in place instead of failing as a duplicate")

out = cmd(s_imm, "news 100")
full = out
guard = 0
while "ENTER for more" in full and guard < 60:
    full += cmd(s_imm, "")
    guard += 1
check(full.count(headline) == 1,
      "editing an existing item does not create a second entry with the same headline")

# --- 2. edit news delete ---
check("News item deleted" in cmd(s_imm, f"edit news delete {headline}"),
      "edit news delete removes an existing item")
check("No news item has that exact headline" in cmd(s_imm, f"edit news delete {headline}"),
      "deleting an already-gone headline reports cleanly instead of crashing")

# --- 3. edit wiznews gets the same treatment ---
out = cmd(s_imm, f"edit wiznews {wiz_headline}")
cmd(s_imm, "Immortal-only story text.")
check("Immortal news posted" in cmd(s_imm, "/s"), "edit wiznews saves a new item")
out = cmd(s_imm, f"edit wiznews {wiz_headline}")
check("existing text below" in out and "Immortal-only story text." in out,
      "re-editing an existing wiznews headline preloads its current body")
cmd(s_imm, "/s")
check("Immortal news item deleted" in cmd(s_imm, f"edit wiznews delete {wiz_headline}"),
      "edit wiznews delete removes an existing item")

# --- 4. unseen-news notice at login ---
s_mo = make_char(mo_name, mo_pw)
s_mo.close()
s_mo, out = reconnect(mo_name, mo_pw)
check("There is new news" in out,
      "a brand-new player who has never read news sees the unseen-news notice at login "
      "(real news items already exist from prior sessions)")
digits = [c for c in out.split("There is new news", 1)[1].split("\r\n", 1)[0] if c.isdigit()]
check(not digits, f"the login notice itself contains no numbers (found: {digits[:5]})")

cmd(s_mo, "news 200")  # read the feed -> marks caught-up
s_mo.close()
s_mo, out = reconnect(mo_name, mo_pw)
check("There is new news" not in out,
      "the notice is gone after reading news, with nothing new posted since")

notice_headline = f"Notice Check {_suffix}"
out = cmd(s_imm, f"edit news {notice_headline}")
cmd(s_imm, "A brand new announcement.")
cmd(s_imm, "/s")
s_mo.close()
s_mo, out = reconnect(mo_name, mo_pw)
check("There is new news" in out,
      "posting a fresh item after the player caught up brings the notice back")

set_level(imm_name, 1)
sql(f"DELETE FROM news WHERE title IN ('{headline}', '{notice_headline}');")
sql(f"DELETE FROM wiznews WHERE title = '{wiz_headline}';")

s_imm.close()
s_mo.close()
announce_done("smoke_test_news_followups", host, port)
print("=== ALL CHECKS PASSED ===")
