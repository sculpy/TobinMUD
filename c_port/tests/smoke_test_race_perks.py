#!/usr/bin/env python3
"""Smoke test for the PC-race perk system's balance editor (Phase 1, user
2026-08-10). Drives an Implementor through the reworked menu-driven
`balance` command: confirms the seeded Sneezy-derived perks display, that a
field edits + saves + persists to race_balance, and that the race-only perk
sections are hidden when balancing a CLASS. See descriptor.c CONN_BALANCE_*,
db/tobin/balance.sql, docs/RACE_PERKS.md.

    python3 tests/smoke_test_race_perks.py [host] [port]
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


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


announce("smoke_test_race_perks", host, port)

suf = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(5))
name, pw = f"Perk{suf}", "perkpw12345"

# Create + promote to Implementor (60) so `balance` is available.
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
for step in (name, "y", pw, pw, "new", name, "1", "1", "1", "done", "done"):
    send_line(s, step)
    recv_all(s)
send_line(s, "quit!")
recv_all(s)
s.close()
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{name}');")

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
for step in (name, pw, "1"):
    send_line(s, step)
    recv_all(s)
cmd(s, "color off")

# --- 1: race menu shows the seeded Dwarf perks + all sections ---
out = strip(cmd(s, "balance race dwarf", timeout=2.0))
for needle in ("Balancing RACE: Dwarf", "Combat", "Vitals & upkeep", "Mana multiplier",
               "0.95", "0.90", "Resistances", "Poison", "20%", "38%",
               "Senses & talent", "Infravision", "ON", "Talent"):
    check(needle in out, f"Dwarf balance menu shows {needle!r}: {out[:0] or '...'}")

# --- 2: edit poison resistance 20 -> 42, save, verify DB persistence ---
out = strip(cmd(s, "9", timeout=1.5))
check("Poison resistance" in out, f"field-9 prompt names poison: {out!r}")
out = strip(cmd(s, "42", timeout=1.5))
check("42%" in out and "unsaved changes" in out,
      f"menu reflects poison 42% + unsaved: {out!r}")
out = strip(cmd(s, "S", timeout=1.5))
check("saved" in out.lower(), f"save acknowledged: {out!r}")
db = query("SELECT resist_poison FROM race_balance WHERE race=3;")
check(db == "42", f"race_balance Dwarf poison persisted to DB (got {db!r})")

# --- 3: restore the seeded 20 through the editor (also refreshes the cache) ---
cmd(s, "9", timeout=1.5)
cmd(s, "20", timeout=1.5)
cmd(s, "S", timeout=1.5)
check(query("SELECT resist_poison FROM race_balance WHERE race=3;") == "20",
      "Dwarf poison restored to seeded 20")
cmd(s, "Q", timeout=1.5)

# --- 4: class menu hides the race-only perk sections ---
out = strip(cmd(s, "balance class warrior", timeout=2.0))
check("HP multiplier" in out, f"class menu shows combat fields: {out[:40]!r}")
check("Vitals & upkeep" not in out and "Resistances" not in out and "Infravision" not in out,
      f"class menu hides race-only perk sections: {out!r}")
cmd(s, "Q", timeout=1.5)

send_line(s, "quit!")
s.close()
announce_done("smoke_test_race_perks", host, port)
print("=== ALL CHECKS PASSED ===")
