#!/usr/bin/env python3
"""Smoke test for `questitem <name> <stage> list` and
`questitem <name> <stage> <race> remove` (builder-tier sub-commands added
to complement the existing `questitem <name> <stage> <race> <vnum>` set
form -- previously a builder had no way to see or clear a reward row
short of querying the DB directly).
    python3 tests/smoke_test_questitem_list_remove.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, cmd, check, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


announce("smoke_test_questitem_list_remove", host, port)

suf = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(5))
quest_name = f"qitest{suf}"
imm_name = f"Qiim{suf}"
imm_pw = "qiimpw12345"

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
for step in (imm_name, "y", imm_pw, imm_pw):
    send_line(s, step)
    recv_all(s)
create_character(s, imm_name, send_line, recv_all, race="1")
send_line(s, "quit!")
recv_all(s)
s.close()

sql(f"UPDATE player_progress SET level=59 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")

imm = socket.create_connection((host, port), timeout=5)
recv_all(imm)
for step in (imm_name, imm_pw, "1"):
    send_line(imm, step)
    recv_all(imm)

# 1. list on an undefined quest/stage -> "No rewards defined".
out = strip(cmd(imm, f"questitem {quest_name} 1 list"))
check("no rewards defined" in out.lower(), f"list on undefined quest/stage: {out!r}")

# 2. set a reward for human (vnum 36996, a real seeded proto reused from
#    the heirloom demo quest) and elf (vnum 36998), then list both.
out = strip(cmd(imm, f"questitem {quest_name} 1 human 36996"))
check("reward for human set to vnum 36996" in out.lower(), f"set human reward: {out!r}")
out = strip(cmd(imm, f"questitem {quest_name} 1 elf 36998"))
check("reward for elf set to vnum 36998" in out.lower(), f"set elf reward: {out!r}")

out = strip(cmd(imm, f"questitem {quest_name} 1 list"))
check("human" in out.lower() and "36996" in out, f"list shows human row: {out!r}")
check("elf" in out.lower() and "36998" in out, f"list shows elf row: {out!r}")

# 3. remove human's reward; list should then show only elf.
out = strip(cmd(imm, f"questitem {quest_name} 1 human remove"))
check("reward for human removed" in out.lower(), f"remove human reward: {out!r}")

out = strip(cmd(imm, f"questitem {quest_name} 1 list"))
check("human" not in out.lower(), f"list no longer shows human after remove: {out!r}")
check("elf" in out.lower() and "36998" in out, f"list still shows elf after removing human: {out!r}")

# 4. removing an already-removed race is safe (no error), and removing the
#    last row brings list back to "No rewards defined".
out = strip(cmd(imm, f"questitem {quest_name} 1 human remove"))
check("reward for human removed" in out.lower(), f"re-remove already-gone human reward is a no-op success: {out!r}")

out = strip(cmd(imm, f"questitem {quest_name} 1 elf remove"))
check("reward for elf removed" in out.lower(), f"remove elf reward: {out!r}")

out = strip(cmd(imm, f"questitem {quest_name} 1 list"))
check("no rewards defined" in out.lower(), f"list empty again after removing all rows: {out!r}")

send_line(imm, "quit!")
imm.close()

sql(f"DELETE FROM quest_item WHERE quest_name='{quest_name}';")
sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
sql(f"DELETE FROM player WHERE name='{imm_name}';")

announce_done("smoke_test_questitem_list_remove", host, port)
print("=== ALL CHECKS PASSED ===")
