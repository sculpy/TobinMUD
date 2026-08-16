#!/usr/bin/env python3
"""Smoke test for the three-week news/wiznews archive split (cmd_news.c,
cmd_wiznews.c, news_repo.c's news_repo_recent() age cutoff).

  1. `news` shows an item posted just now but NOT one dated a month back.
  2. `news archived` (alias `news old`) shows the month-old item but NOT the
     brand-new one.
  3. The same split holds for the immortal `wiznews` / `wiznews archived`.

    python3 tests/smoke_test_news_archive.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_news_archive", host, port)

_sfx = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(5))

NEW_NEWS = f"Fresh Bulletin {_sfx}"
OLD_NEWS = f"Ancient Bulletin {_sfx}"
NEW_WIZ = f"Fresh Wiz Memo {_sfx}"
OLD_WIZ = f"Ancient Wiz Memo {_sfx}"


def feed(sock, arg=""):
    """Read a full news/wiznews feed, draining the pager completely so the
    next command isn't swallowed by a pending 'more' prompt."""
    full = cmd(sock, arg.strip())
    resp = full
    guard = 0
    while "ENTER for more" in resp and guard < 80:
        resp = cmd(sock, "")
        full += resp
        guard += 1
    return full


# --- seed one current and one month-old item in each channel ---
sql(f"INSERT INTO news (author,title,body,created_at) VALUES "
    f"('Tester','{NEW_NEWS}','A brand new headline.',NOW());")
sql(f"INSERT INTO news (author,title,body,created_at) VALUES "
    f"('Tester','{OLD_NEWS}','A headline from a month ago.',NOW() - INTERVAL 30 DAY);")
sql(f"INSERT INTO wiznews (author,title,body,created_at) VALUES "
    f"('Tester','{NEW_WIZ}','A brand new immortal memo.',NOW());")
sql(f"INSERT INTO wiznews (author,title,body,created_at) VALUES "
    f"('Tester','{OLD_WIZ}','An immortal memo from a month ago.',NOW() - INTERVAL 30 DAY);")

name = f"Arch{_sfx}"
pw = "archnewspw"

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
for step in (name, "y", pw, pw, "new", name, "1", "1", "1", "done", "done"):
    send_line(s, step); recv_all(s)
cmd(s, "color off")

# 1. live news shows the new item, not the month-old one
live = feed(s, "news")
check(NEW_NEWS in live, "the current news feed shows a just-posted item")
check(OLD_NEWS not in live, "the current news feed hides a month-old item")

# 2. archived news shows the month-old item, not the new one
arch = feed(s, "news archived")
check(OLD_NEWS in arch, "`news archived` shows the month-old item")
check(NEW_NEWS not in arch, "`news archived` hides the still-current item")
check("Archive" in arch, "the archive view is labelled as an archive")

# alias `news old` behaves the same
arch_alias = feed(s, "news old")
check(OLD_NEWS in arch_alias, "`news old` is an alias for `news archived`")

# 3. same split for the immortal wiznews channel (needs level 51+)
sql(f"UPDATE player_progress SET level=60, true_level=60 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{name}');")
s.close()
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
for step in (name, pw, "1"):
    send_line(s, step); recv_all(s)
cmd(s, "color off")

wlive = feed(s, "wiznews")
check(NEW_WIZ in wlive, "the current wiznews feed shows a just-posted memo")
check(OLD_WIZ not in wlive, "the current wiznews feed hides a month-old memo")

warch = feed(s, "wiznews archived")
check(OLD_WIZ in warch, "`wiznews archived` shows the month-old memo")
check(NEW_WIZ not in warch, "`wiznews archived` hides the still-current memo")

s.close()

# --- cleanup ---
for t in (NEW_NEWS, OLD_NEWS):
    sql(f"DELETE FROM news WHERE title='{t}';")
for t in (NEW_WIZ, OLD_WIZ):
    sql(f"DELETE FROM wiznews WHERE title='{t}';")
sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
sql(f"DELETE FROM player_attrs WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
sql(f"DELETE FROM player WHERE name='{name}';")

announce_done("smoke_test_news_archive", host, port)
print("=== ALL CHECKS PASSED ===")
