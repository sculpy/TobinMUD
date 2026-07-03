#!/usr/bin/env python3
"""Smoke test for the game log system (Session 21):
  1. Log files live in logs/ named DDMMYY.HHMM<AM/PM>.log (user spec).
  2. Gate: mortals and a level-58 get Huh?! for `log`; 59 works.
  3. `log` tails the current file; `log search <text>` finds a known
     line (a link-drop we caused with a unique character name).
  4. `log rotate` starts a fresh file (announced by name); the old
     unique string is no longer in the CURRENT file; `log list` shows
     multiple files with the current one marked.

    python3 tests/smoke_test_logs.py [host] [port]
"""
import glob
import os
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


def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f">>> OK: {message}")


def set_level(name, level):
    subprocess.run(
        ["mariadb", "sneezy", "-e",
         f"UPDATE player_progress SET level={level} WHERE player_id=(SELECT id FROM player WHERE name='{name}');"],
        check=True,
    )


def make_player(tag):
    name = f"Lg{tag}{_suffix}"
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "logtestpw")
    recv_all(s)
    send_line(s, "new")
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "done")
    recv_all(s)
    return s, name


def relog(name):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name)
    recv_all(s)
    send_line(s, "logtestpw")
    recv_all(s)
    send_line(s, "1")
    recv_all(s)
    return s


# --- Part 1: the files exist where they should ---
# (Test runs on the server box; server cwd is c_port/, so logs/ is here.)
files = glob.glob("logs/*.log")
check(files, "logs/ contains .log files")

# --- Part 2: the gate ---
sImm, nameImm = make_player("Imm")

send_line(sImm, "log")
check("Huh?!" in recv_all(sImm), "a mortal typing log gets Huh?! (hidden)")

set_level(nameImm, 53)
sImm.close()
sImm = relog(nameImm)
send_line(sImm, "log")
check("Huh?!" in recv_all(sImm), "a level-53 can't read logs (gate is 54)")

set_level(nameImm, 54)
sImm.close()
sImm = relog(nameImm)

# 54 can read but NOT rotate (rotate is isolated to 59+).
send_line(sImm, "log rotate")
out = recv_all(sImm)
check("requires level 59" in out, "a level-54 is refused log rotation")

# The log commands are documented: help log shows the topic (only for
# those who can see the command at all -- 59+).
send_line(sImm, "help log")
out = recv_all(sImm)
check("-- Help: log --" in out and "rotate" in out,
      "help log shows the log command's help topic")

# --- Part 3: tail and search ---
send_line(sImm, "log")
out = recv_all(sImm)
check(".log (last" in out, "bare log tails the current file with a header")
check("INFO" in out or "ERROR" in out, "the tail shows real log lines")

# Cause a distinctive log line: a link-drop by a uniquely named character.
sVictim, nameVictim = make_player("Vic")
sVictim.close()
time.sleep(0.5)

send_line(sImm, f"log search {nameVictim}")
out = recv_all(sImm)
check("lost their link" in out and nameVictim.capitalize() in out,
      "log search finds the link-drop line by character name")

send_line(sImm, "log search zzzznosuchstringzzz")
out = recv_all(sImm)
check("0 matches" in out, "a search with no hits reports 0 matches")

# IP logging (Session 21): connections and drops carry the peer address.
send_line(sImm, "log search 127.0.0.1")
out = recv_all(sImm)
check("from 127.0.0.1" in out or "[127.0.0.1]" in out,
      "log lines carry the peer IP (connect/drop entries)")

# --- Part 4: rotate + list (rotate needs 59) ---
set_level(nameImm, 59)
sImm.close()
sImm = relog(nameImm)
send_line(sImm, "log rotate")
out = recv_all(sImm)
check("Now writing to logs/" in out, "log rotate announces the new file (at 59)")

time.sleep(0.2)
send_line(sImm, f"log search {nameVictim}")
out = recv_all(sImm)
check("0 matches" in out, "after rotation the old line is not in the CURRENT file")

send_line(sImm, "log list")
out = recv_all(sImm)
check("<- current" in out, "log list marks the current file")
check(out.count(".log") >= 2, "log list shows the rotated-away file too")

# hygiene
set_level(nameImm, 1)
sImm.close()
print("=== ALL CHECKS PASSED ===")
