#!/usr/bin/env python3
"""Smoke test for tell history + `reply` (2026-07-26 docs/systems review --
ports the original's `tellhistory` table + `desc->last_teller`/`doReply()`,
a gap the review found: Tobin's `tell` had no audit trail and no reply
shortcut at all). Covers:

  1. `reply` with no prior tell says so, doesn't crash.
  2. A `tell` sets the recipient's last_teller; `reply <msg>` reaches the
     original sender without retyping their name.
  3. Every tell (both directions) is logged to `tell_history`.
  4. `tell_history` is capped at 25 rows per recipient.

    python3 smoke_test_reply.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, check, sql

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


def recv_all(sock, timeout=1.0, idle_gap=0.3):
    sock.settimeout(idle_gap)
    chunks = []
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
        except socket.timeout:
            break
    return b"".join(chunks).decode(errors="replace")


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    return recv_all(sock, timeout)


def sql_out(stmt):
    r = subprocess.run(["mariadb", "tobin", "-N", "-e", stmt], check=True, capture_output=True, text=True)
    return r.stdout


def make_char(name, pw, class_num="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", class_num, "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
a_name, a_pw = f"Rpaa{_suffix}", "rpaapw123"
b_name, b_pw = f"Rpbb{_suffix}", "rpbbpw123"

a = make_char(a_name, a_pw)
b = make_char(b_name, b_pw)
cmd(a, "color off")
cmd(b, "color off")

# --- 1: reply with nothing pending ---
out = cmd(b, "reply hello?")
check("no one has told you" in out.lower(), "reply with no prior tell says so, doesn't crash")

# --- 2: tell sets last_teller; reply reaches the original sender ---
out = cmd(a, f"tell {b_name} first message")
check("you tell" in out.lower(), "A's tell sends")
out = cmd(b, "")
check("first message" in out, "B received the tell")

out = cmd(b, "reply second message")
check("you tell" in out.lower() and a_name.lower() in out.lower(), "B's reply resolves to A by name")
out = cmd(a, "")
check("second message" in out, "A received the reply")

a_id = sql_out(f"SELECT id FROM player WHERE name='{a_name}';").strip()
b_id = sql_out(f"SELECT id FROM player WHERE name='{b_name}';").strip()

# --- 3: both tells logged to tell_history ---
count = sql_out(
    f"SELECT COUNT(*) FROM tell_history WHERE "
    f"(from_player_id={a_id} AND to_player_id={b_id} AND message='first message') OR "
    f"(from_player_id={b_id} AND to_player_id={a_id} AND message='second message');"
).strip()
check(count == "2", "both the tell and the reply were logged to tell_history")

# --- 4: cap enforcement, driven through real `tell` commands (not a
# hand-written trim query) so this actually exercises tell_history_add()'s
# own trim logic in tell_history_repo.c ---
for i in range(30):
    cmd(a, f"tell {b_name} filler {i}", timeout=0.3)
remaining = sql_out(f"SELECT COUNT(*) FROM tell_history WHERE to_player_id={b_id};").strip()
check(remaining == "25", "tell_history is capped at 25 rows per recipient")

a.close()
b.close()

sql(f"DELETE FROM tell_history WHERE to_player_id={b_id} OR to_player_id={a_id};")
sql(f"DELETE FROM player_progress WHERE player_id IN ({a_id}, {b_id});")
sql(f"DELETE FROM player WHERE id IN ({a_id}, {b_id});")

print("=== ALL CHECKS PASSED ===")
