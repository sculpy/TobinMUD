#!/usr/bin/env python3
"""Smoke test for showing bank gold on `score` (user 2026-08-03: "bank
gold should be reported in score"). Previously `score` only ever showed
the carried wallet (`p->gold`) on its "Race/Class/Gold" line -- the bank
balance (`p->bank_gold`, Money system v2, see smoke_test_bank.py) was
completely invisible there, only checkable via the `bank` command at an
actual bank. Fixed by adding a "Bank: %d" field next to "Gold: %d" on
the same line (cmd_score.c).

    python3 tests/smoke_test_score_bank_gold.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_score_bank_gold", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", class_choice, "done", "done"):
        send_line(s, step); recv_all(s)
    cmd(s, "quit!")
    s.close()


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


pc_name = f"Bgsc{_suffix}"
pw = "bankscorepw123"

try:
    make_char(pc_name, pw, "3")
    sql(f"UPDATE player_progress SET gold=1234, bank_gold=5678 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{pc_name}');")

    pc = login(pc_name, pw)
    out = cmd(pc, "score")
    check("Gold: 1234" in out, "score shows the wallet gold amount")
    check("Bank: 5678" in out, "score also shows the bank gold amount")

    announce_done("smoke_test_score_bank_gold", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    _sock = locals().get("pc")
    if _sock is not None:
        try:
            _sock.close()
        except OSError:
            pass
    sql(f"DELETE FROM player_inventory WHERE player_id IN "
        f"(SELECT id FROM player WHERE name='{pc_name}');")
    sql(f"DELETE FROM player_progress WHERE player_id IN "
        f"(SELECT id FROM player WHERE name='{pc_name}');")
    sql(f"DELETE FROM player_attrs WHERE player_id IN "
        f"(SELECT id FROM player WHERE name='{pc_name}');")
    sql(f"DELETE FROM player WHERE name='{pc_name}';")
