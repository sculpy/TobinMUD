#!/usr/bin/env python3
"""Smoke test for rent cost (user 2026-08-10). Rent charges a level-scaled
tax (tax_at_max * level^3 / 50^3), paid from the wallet first then the bank
for any shortfall; free at/below the free level; tunable live via
`balance rent`. See rent.c/rent.h, cmd_rent.c, cmd_balance.c.

    python3 tests/smoke_test_rent_cost.py [host] [port]
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


def setup(name, level, gold, bank):
    sql(f"UPDATE player_progress SET level={level}, gold={gold}, bank_gold={bank} "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{name}');")


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step); recv_all(s)
    cmd(s, "color off")
    return s


def purse(name):
    row = query(f"SELECT gold,bank_gold FROM player_progress "
                f"WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
    g, b = row.split("\t")
    return int(g), int(b)


announce("smoke_test_rent_cost", host, port)
suf = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(5))
pw = "rentpw12345"
m1, m2, m3, imm = f"Renta{suf}", f"Rentb{suf}", f"Rentc{suf}", f"Rimm{suf}"
for nm in (m1, m2, m3, imm):
    make(nm, pw)
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")

# --- 1: L40, wallet 500 + bank 5000; cost(40)@tax2000 = 1024 -> 500 wallet + 524 bank ---
setup(m1, 40, 500, 5000)
s = login(m1, pw)
out = strip(cmd(s, "rent", timeout=1.5))
s.close()
check("collects 1024 gold" in out, f"rent charges the level^3 cost 1024: {out!r}")
check("524 drawn from your bank" in out, f"shortfall auto-withdrawn from bank: {out!r}")
check(purse(m1) == (0, 4476), f"wallet emptied then bank tapped: {purse(m1)}")

# --- 2: L3 (<= free level 5) rents free ---
setup(m2, 3, 500, 5000)
s = login(m2, pw)
out = strip(cmd(s, "rent", timeout=1.5))
s.close()
check("collects" not in out, f"newbie (level 3) rents free: {out!r}")
check(purse(m2) == (500, 5000), f"newbie purse untouched: {purse(m2)}")

# --- 3: adjustable -- immortal sets tax to 3000, L40 wallet 5000 pays 1536 from wallet ---
i = login(imm, pw)
out = strip(cmd(i, "balance rent", timeout=1.5))
check("Rent settings" in out and "2000" in out, f"balance rent shows current settings: {out!r}")
cmd(i, "balance rent tax 3000", timeout=1.5)
try:
    setup(m3, 40, 5000, 0)
    s = login(m3, pw)
    out = strip(cmd(s, "rent", timeout=1.5))
    s.close()
    check("collects 1536 gold" in out, f"raised tax gives cost 1536: {out!r}")
    check(purse(m3) == (3464, 0), f"paid from wallet at raised tax: {purse(m3)}")
finally:
    cmd(i, "balance rent tax 2000", timeout=1.5)   # restore default
check("2000" in strip(cmd(i, "balance rent", timeout=1.5)), "tax restored to 2000")
send_line(i, "quit!"); i.close()

announce_done("smoke_test_rent_cost", host, port)
print("=== ALL CHECKS PASSED ===")
