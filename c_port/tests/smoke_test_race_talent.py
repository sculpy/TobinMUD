#!/usr/bin/env python3
"""Smoke test for PC-race talents (Phase 3, user 2026-08-10). Verifies the
Gnome's innate Detect Magic talent is applied as a login affect (visible in
`affects`), a plain Human has no such affect, and that the effect is
data-driven: giving Human the Detect Magic talent via the `balance` editor
grants it on the next login. (The Adaptable/Brawler/Woodland skill-gain
talents are probabilistic learn-by-doing boosts, verified by build + review,
not a deterministic smoke check.) See being.c being_race_talent(),
descriptor.c enter_world(), docs/RACE_PERKS.md.

    python3 tests/smoke_test_race_talent.py [host] [port]
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


def make(name, pw, race_pick):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, str(race_pick), "1", "1", "done", "done"):
        send_line(s, step); recv_all(s)
    send_line(s, "quit!"); recv_all(s); s.close()


def affects_of(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step); recv_all(s)
    cmd(s, "color off")
    out = strip(cmd(s, "affects", timeout=1.5))
    send_line(s, "quit!"); s.close()
    return out.lower()


def set_human_talent(imm, value):
    cmd(imm, "balance race human", timeout=2.0)
    cmd(imm, "17", timeout=1.2)      # field 17 = talent
    cmd(imm, str(value), timeout=1.2)
    cmd(imm, "S", timeout=1.2)
    cmd(imm, "Q", timeout=1.2)


announce("smoke_test_race_talent", host, port)

suf = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(5))
iname, gname, hname, pw = f"Timm{suf}", f"Tgno{suf}", f"Thum{suf}", "talentpw12345"

make(iname, pw, 1)
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{iname}');")
make(gname, pw, 6)   # race pick 6 -> Gnome
make(hname, pw, 1)   # race pick 1 -> Human

# --- 1: Gnome has innate detect magic; Human does not ---
check("detect magic" in affects_of(gname, pw), "Gnome has innate Detect Magic in affects")
check("detect magic" not in affects_of(hname, pw), "plain Human has no Detect Magic affect")

# --- 2: data-driven -- grant Human the talent, it appears on next login ---
imm = socket.create_connection((host, port), timeout=5)
recv_all(imm)
for step in (iname, pw, "1"):
    send_line(imm, step); recv_all(imm)
cmd(imm, "color off")
try:
    set_human_talent(imm, 4)   # 4 = Detect Magic
    check("detect magic" in affects_of(hname, pw),
          "Human granted the Detect Magic talent shows it on relog (data-driven)")
finally:
    set_human_talent(imm, 1)   # restore Human's seeded Adaptable talent
# Note: the innate affect is applied (and persisted) at login, so an already
# affected character keeps it even after the talent is changed back -- login
# only ADDS it, it never strips a detect-magic a Mage may have cast. So we
# don't assert removal here; the data-driven grant above is the proof.
send_line(imm, "quit!"); imm.close()

announce_done("smoke_test_race_talent", host, port)
print("=== ALL CHECKS PASSED ===")
