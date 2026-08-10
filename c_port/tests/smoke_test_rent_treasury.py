#!/usr/bin/env python3
"""Smoke test for rent tax -> coffers and the monthly improvement-projects
spend (user 2026-08-10). Rent tax flows into the crown's treasury minus the
innkeeper's cut; once a game-month 95% of the coffers is spent on public
improvement projects and announced to every player. `treasury allocate`
forces that spend for testing/ops. See rent.c, cmd_rent.c, treasury.c,
cmd_bank.c, gametime.c (gametime_announce).

    python3 tests/smoke_test_rent_treasury.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time

from mud_test_utils import send_line, recv_all, check, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def make(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", "1", "done", "done"):
        send_line(s, step); recv_all(s)
    send_line(s, "quit!"); recv_all(s); s.close()


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step); recv_all(s)
    cmd(s, "color off")
    return s


def treasury_of(imm):
    m = re.search(r"holds (\d+) gold", strip(cmd(imm, "treasury", timeout=1.5)))
    return int(m.group(1)) if m else -1


def gold_of(name):
    return int(query(f"SELECT gold FROM player_progress "
                     f"WHERE player_id=(SELECT id FROM player WHERE name='{name}');"))


def set_treasury(v):
    sql(f"UPDATE world_treasury SET gold={v} WHERE id=1;")


def rent_char(name, pw, level, gold):
    sql(f"UPDATE player_progress SET level={level}, gold={gold}, bank_gold=0 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
    s = login(name, pw)
    cmd(s, "rent", timeout=1.5)
    s.close()


announce("smoke_test_rent_treasury", host, port)
suf = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(5))
pw = "trspw12345"
imm, m1, m2, watch = f"Trimm{suf}", f"Trra{suf}", f"Trrb{suf}", f"Trw{suf}"
for nm in (imm, m1, m2, watch):
    make(nm, pw)
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")

i = login(imm, pw)

# --- 1: rent tax -> coffers minus 10% innkeeper cut. cost(40)=1024 -> keeper 102, coffers +922 ---
set_treasury(0)
rent_char(m1, pw, 40, 5000)
check(treasury_of(i) == 922, f"rent tax minus innkeeper cut lands in coffers (got {treasury_of(i)})")
check(gold_of(m1) == 3976, f"player charged the full 1024 (got {gold_of(m1)})")

# --- 2: innkeeper cut is adjustable. keeper 25% -> coffers 1024-256 = 768 ---
cmd(i, "balance rent keeper 25", timeout=1.5)
set_treasury(0)
rent_char(m2, pw, 40, 5000)
check(treasury_of(i) == 768, f"raised innkeeper cut leaves less for coffers (got {treasury_of(i)})")
cmd(i, "balance rent keeper 10", timeout=1.5)   # restore default

# --- 3: monthly allocate spends 95% and broadcasts to all players ---
set_treasury(10000)
w = login(watch, pw)         # a mortal bystander to catch the broadcast
recv_all(w)
out = strip(cmd(i, "treasury allocate", timeout=1.5))
check("9500 gold spent" in out, f"allocate spends 95% of coffers: {out!r}")
check(treasury_of(i) == 500, f"5% remains in reserve (got {treasury_of(i)})")
bcast = strip(recv_all(w) or "")
check("improvement projects" in bcast.lower() and "9500" in bcast,
      f"every player is told of the allocation: {bcast!r}")

# --- 4: timeshift (59+) moves the shared clock (shift forward, then back) ---
t0 = strip(cmd(i, "time", timeout=1.5))
out = strip(cmd(i, "timeshift 3 months", timeout=1.5))
check("Time shifted" in out, f"timeshift command runs: {out!r}")
t1 = strip(cmd(i, "time", timeout=1.5))
check(t0 != t1, f"timeshift changed the game clock ({t0!r} -> {t1!r})")
cmd(i, "timeshift -3 months", timeout=1.5)   # restore the live clock

send_line(w, "quit!"); w.close()
send_line(i, "quit!"); i.close()
announce_done("smoke_test_rent_treasury", host, port)
print("=== ALL CHECKS PASSED ===")
