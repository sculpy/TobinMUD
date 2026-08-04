#!/usr/bin/env python3
"""Smoke test for pfile-change logging (TODO.md "Player-state logging" --
log get/drop + pfile changes so `log search <name>` tells a player's
story). get/drop already logged (cmd_object.c); this covers the other
half: `title`, `prompt`, `poofin`/`poofout`, `bamfin`/`bamfout`.

    python3 tests/smoke_test_pfile_logging.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, check, sql, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_pfile_logging", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def recv_all(sock, timeout=1.0, idle_gap=0.3):
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
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    cmd(s, "1")
    cmd(s, "color off")
    return s


name, pw = f"Pflog{_suffix}", "pflogpw123"
imm_name, imm_pw = f"Pfimm{_suffix}", "pfimmpw123"

s = make_char(name, pw, cls="1")
cmd(s, f"title the Logged{_suffix}")
cmd(s, "prompt hp")
cmd(s, "quit!")
s.close()

s_imm = make_char(imm_name, imm_pw, cls="3")
cmd(s_imm, "quit!")
s_imm.close()
sql(f"UPDATE player_progress SET level=54 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{imm_name}');")
s_imm = reconnect(imm_name, imm_pw)

out = cmd(s_imm, f"log search {name}", 2.0)
check("sets their title to 'the Logged" + _suffix in out,
      "log search finds the title-change entry")
check("turns on hit points in their prompt" in out,
      "log search finds the prompt-toggle entry")

s_imm.close()
announce_done("smoke_test_pfile_logging", host, port)
print("=== ALL CHECKS PASSED ===")
