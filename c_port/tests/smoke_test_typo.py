#!/usr/bin/env python3
"""Smoke test for `typo` / `deltypo` (cmd_typo.c, typo_repo.c) -- a direct
mirror of `bug`/`idea` (smoke_test_bug.py/smoke_test_idea.py), same shape,
different table (user, 2026-08-02: "add a typo command in the same way"):
  1. A mortal files a typo report with `typo <text>` and gets a thank-you.
  2. Bare `typo` from a mortal shows usage; from an immortal lists reports.
  3. The filed report shows the submitter, its room vnum, and appears in
     the immortal list.
  4. `deltypo <id>` (59+) removes it; a mortal can't see deltypo (Command
     not found).

    python3 tests/smoke_test_typo.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_typo", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def make_char(nm):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, "typopw"); recv_all(s)
    send_line(s, "typopw"); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human (zero stat modifier)
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    return s


def relogin(nm):
    r = socket.create_connection((host, port), timeout=5)
    recv_all(r)
    send_line(r, nm); recv_all(r)
    send_line(r, "typopw"); recv_all(r)
    send_line(r, "1"); recv_all(r)
    return r


# --- mortal files a typo report ---
nameM = f"Typom{_suffix}"
sm = make_char(nameM)
marker = f"widget{_suffix}"
check("filed" in strip(cmd(sm, f"typo the {marker} description has a typo")).lower(),
      "a mortal can file a typo report and is thanked")
check("Usage: typo" in strip(cmd(sm, "typo")), "bare typo from a mortal shows usage")
check("Command not found" in cmd(sm, "deltypo 1"), "a mortal cannot see deltypo (Command not found)")
sm.close()

# --- immortal lists + deletes ---
nameI = f"Typoi{_suffix}"
make_char(nameI).close()
sql(f"UPDATE player_progress SET level=59 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{nameI}');")
si = relogin(nameI)

out = strip(cmd(si, "typo"))
check("Typo reports" in out, "immortal bare typo shows the report list header")
check(marker in out and nameM.capitalize() in out,
      "the report lists its text and the submitter's name")
check("(room 100)" in out, "the report shows the room vnum the submitter was standing in")

# find the id for our marker and delete it
m = re.search(rf"#(\d+)[^\n]*{re.escape(marker)}", out)
check(m is not None, "the report carries a numeric id")
typo_id = m.group(1)
check(f"#{typo_id} deleted" in strip(cmd(si, f"deltypo {typo_id}")),
      "deltypo removes the report by id")
check(marker not in strip(cmd(si, "typo")), "the deleted report is gone from the list")
check("No typo report has that number" in strip(cmd(si, f"deltypo {typo_id}")),
      "deleting a nonexistent typo report is rejected")

si.close()
announce_done("smoke_test_typo", host, port)
print("=== ALL CHECKS PASSED ===")
