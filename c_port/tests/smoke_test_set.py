#!/usr/bin/env python3
"""Smoke test for `set` (cmd_set.c), the one-shot scriptable sibling of
`edplayer`:
  1. Gate: invisible below level 58, same tier as edplayer.
  2. A nonexistent target and an unknown field are both rejected cleanly.
  3. Each field takes effect in one line -- verified via a fresh reconnect.
  4. Validation rejects out-of-range values without changing anything.
  5. An already-connected target is updated live, no relog (same courtesy
     edplayer/promote already give).

    python3 tests/smoke_test_set.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

def announce(test_name, host=host, port=port):
    """Tell the running MUD which smoke test is executing: emits a [TEST]
    log line (visible to online immortals and in the day's log file) via
    the loopback-only `@test` server hook. Best-effort -- never fails the
    test. Self-contained (own socket, doesn't depend on this file's other
    helpers) so it can run at any point in the script."""
    try:
        s = socket.create_connection((host, port), timeout=3)
        s.settimeout(0.5)
        try:
            while s.recv(4096):
                pass
        except socket.timeout:
            pass
        s.sendall(f"@test {test_name}\r\n".encode())
        s.settimeout(0.5)
        try:
            while s.recv(4096):
                pass
        except socket.timeout:
            pass
        s.close()
    except OSError:
        pass


def announce_done(test_name, host=host, port=port):
    """Companion to announce() -- emits a [TEST] "finished" log line. Call
    once at the very end of a smoke test, just before it reports success."""
    announce(f"done {test_name}", host, port)


announce("smoke_test_set")

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


def make_char(nm, pw="settestpw123"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, nm); recv_all(s)
    send_line(s, "done"); recv_all(s)
    return s


def relogin(nm, pw="settestpw123"):
    r = socket.create_connection((host, port), timeout=5)
    recv_all(r)
    send_line(r, nm); recv_all(r)
    send_line(r, pw); recv_all(r)
    send_line(r, "1"); recv_all(r)
    return r


def promote_sql(nm, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{nm}');")


target = f"Settgt{_suffix}"
admin = f"Setadm{_suffix}"

tsock = make_char(target)
# "quit!" leaves to the account menu first (a real disconnect, character
# detached cleanly) -- an abrupt close while still playing would instead
# leave the character linkdead in its CURRENT room, overriding the load
# room `set` changes below (see world_find_linkdead_pc()).
cmd(tsock, "quit!")
tsock.close()
make_char(admin).close()

# --- gate ---
# At 57, "set" itself is gated out (invisible), so abbreviation matching
# falls through to the next visible command sharing the "set" prefix --
# `setsev` -- and fails there instead (a "Huh?!" text is not guaranteed,
# since this is the same prefix-collision quirk documented elsewhere in
# cmd_table.c). What actually matters: the level must NOT have changed.
promote_sql(admin, 57)
sa = relogin(admin)
cmd(sa, f"set {target} level 10")
sa.close()
out = subprocess.run(
    ["mariadb", "sneezy", "-N", "-e",
     f"SELECT level FROM player_progress WHERE player_id="
     f"(SELECT id FROM player WHERE name='{target}');"],
    check=True, capture_output=True, text=True,
).stdout.strip()
check(out == "1", "a level-57 immortal's set attempt did not actually change anything (gate is 58)")

promote_sql(admin, 58)
sa = relogin(admin)

# --- errors ---
out = cmd(sa, f"set NoSuchPlayer{_suffix} level 10")
check("No player named" in out, "a nonexistent target is rejected")

out = cmd(sa, f"set {target} nosuchfield banana")
check("Unknown field" in out, "an unknown field is rejected")

out = cmd(sa, f"set {target} level 999")
check("Level must be between" in out, "an out-of-range level is rejected")

out = cmd(sa, f"set {target} hp 999 10")
check("hp <= max hp" in out or "Usage: set" in out, "hp greater than max hp is rejected")

# --- one field at a time, each confirmed and then verified via reconnect ---
check(f"{target}'s level is now 22" in cmd(sa, f"set {target} level 22"), "set level confirms")
check(f"{target}'s experience is now 777" in cmd(sa, f"set {target} xp 777"), "set xp confirms")
check(f"{target}'s HP is now 15/30" in cmd(sa, f"set {target} hp 15 30"), "set hp confirms")
check(f"{target}'s dex is now 180" in cmd(sa, f"set {target} dex 180"), "set attribute confirms")
check(f"{target}'s gender is now male" in cmd(sa, f"set {target} gender male"), "set gender confirms")
check(f"{target}'s title is now the Scripted" in cmd(sa, f"set {target} title the Scripted"),
      "set title confirms")
check(f"{target}'s load room is now 1" in cmd(sa, f"set {target} loadroom 1"), "set loadroom confirms")
check(f"{target} is now left handed" in cmd(sa, f"set {target} handed left"), "set handed confirms")

st = relogin(target)
out = strip(cmd(st, "score"))
check("Level:         22" in out, "the set level persisted")
check("Experience:    777" in out, "the set experience persisted")
check("HP:            15/30" in out, "the set HP persisted")
check("Dexterity:     180" in out, "the set attribute persisted")
check("Gender: male" in out, "the set gender persisted")
check("You are left handed" in out, "the set handedness persisted")
out = strip(cmd(st, "who"))
check("the Scripted" in out, "the set title persisted")
out = strip(cmd(st, "look"))
check("Imperia" in out, "the set load room took effect on this fresh login")
st.close()

# --- online sync: edit an already-connected target live, no relog ---
st = relogin(target)
cmd(sa, f"set {target} title Live One Liner")
out = strip(cmd(st, "who"))
check("Live One Liner" in out, "the already-connected target's title updated live, no relog")
st.close()

sa.close()
announce_done("smoke_test_set")
print("=== ALL CHECKS PASSED ===")
