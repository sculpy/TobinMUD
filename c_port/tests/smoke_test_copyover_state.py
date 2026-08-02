#!/usr/bin/env python3
"""Smoke test for copyover runtime-state persistence (TODO.md priority item,
user 2026-07-30: "copyover restores all runtime statistics/state"). See
cmd_copyover.c/game_loop.c's own header comments for scope-down rationale
(top-level room contents only, deduped against zone_boot_all()'s own
per-room population -- not nested containers/equipment/corpse contents).

Requires an immortal test account already promoted to level 59+ that can
run `copyover` -- reuses the session's established Cpovtitq pattern. This
test IS destructive to the live server (it triggers real copyovers), so
it's meant to be run deliberately, not as part of an unattended sweep.

    python3 tests/smoke_test_copyover_state.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def recv_all(sock, timeout=1.5):
    sock.settimeout(timeout)
    chunks = []
    try:
        while True:
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
    except socket.timeout:
        pass
    return b"".join(chunks).decode(errors="replace")


def send_line(sock, line):
    sock.sendall((line + "\r\n").encode())


def cmd(sock, line, timeout=1.5):
    send_line(sock, line)
    return recv_all(sock, timeout)


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


IMM_NAME = "Cpovtitq"
IMM_PW = "copyoverpw1234"


def imm_login():
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, IMM_NAME); recv_all(s)
    send_line(s, IMM_PW); recv_all(s)
    send_line(s, "1"); recv_all(s, 1.0)
    return s


def do_copyover(s):
    """Triggers copyover and blocks until the server is back up."""
    cmd(s, "copyover", 3.0)
    time.sleep(8)


print("=== Copyover Runtime-State Persistence Test ===\n")

s = imm_login()

# Use vnum 12 (glabrezu demon) -- distinct from vnums already used by
# earlier manual verification this session (10, 11), so a leftover from a
# prior run can't produce a false pass.
out = strip(cmd(s, "load mob 12"))
check("conjure" in out.lower(), "test mob loaded into room 1")
out = strip(cmd(s, "load obj 1"))
check("conjure" in out.lower(), "test object (hairball) loaded")
cmd(s, "drop all")

out = strip(cmd(s, "look"))
check("glabrezu demon" in out.lower(), "mob present before copyover")
check("hairball" in out.lower(), "object present before copyover")

do_copyover(s)

s2 = imm_login()
out = strip(cmd(s2, "look"))
check("glabrezu demon" in out.lower(), "mob SURVIVES copyover")
check("hairball" in out.lower(), "object SURVIVES copyover")

# Dedup check: the room's own permanent fixtures shouldn't have doubled.
check("(x2)" not in out, "no duplicate zone-seeded fixtures after copyover")

s2.close()

print("\n=== ALL CHECKS PASSED ===")
