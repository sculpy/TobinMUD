#!/usr/bin/env python3
"""Smoke test for `rules` / `edrules` (cmd_rules.c, rules_repo.c, EDIT_RULES):
  1. `rules` lists the numbered rules; `rules <n>` shows one in full.
  2. `rules <bad>` is rejected.
  3. A mortal can't see edrules (Huh?!).
  4. A 59+ immortal writes a new rule via edrules + the line editor, and it
     then shows up in `rules` and `rules <n>`.

    python3 tests/smoke_test_rules.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000
_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def recv_all(sock, timeout=1.0):
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


def cmd(sock, line, timeout=1.0):
    send_line(sock, line)
    return recv_all(sock, timeout)


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def sql(stmt):
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


def make_char(nm):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "rulepw"); recv_all(s)
    send_line(s, "rulepw"); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "done"); recv_all(s)
    return s


def relogin(nm):
    r = socket.create_connection((host, port), timeout=5)
    recv_all(r)
    send_line(r, nm); recv_all(r)
    send_line(r, "rulepw"); recv_all(r)
    send_line(r, "1"); recv_all(r)
    return r


# --- mortal reads rules ---
nameM = f"Rulm{_suffix}"
sm = make_char(nameM)
out = strip(cmd(sm, "rules"))
check("Game Rules" in out and "Respect Other Players" in out,
      "rules lists the seeded rules with titles")
check("Respect Other Players" in strip(cmd(sm, "rules 1")),
      "rules 1 shows the first rule in full")
check("no rule number" in strip(cmd(sm, "rules 999")),
      "asking for a nonexistent rule is rejected")
check("Huh?!" in cmd(sm, "edrules 5 Test"), "a mortal cannot see edrules (Huh?!)")
sm.close()

# --- immortal writes a rule ---
nameI = f"Ruli{_suffix}"
make_char(nameI).close()
sql(f"UPDATE player_progress SET level=59 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{nameI}');")
si = relogin(nameI)

marker = f"norule{_suffix}"
out = cmd(si, "edrules 7 No Testing In Production")
check("Writing rule 7" in strip(out), "edrules opens the line editor for the rule")
send_line(si, f"You shall not {marker} on the live game.")
recv_all(si)
out = strip(cmd(si, "."))  # save
check("Rule 7 saved" in out, "the rule saves out of the line editor")

check("No Testing In Production" in strip(cmd(si, "rules")),
      "the new rule appears in the list")
body = strip(cmd(si, "rules 7"))
check("No Testing In Production" in body and marker in body,
      "rules 7 shows the new title and body")

si.close()
print("=== ALL CHECKS PASSED ===")
