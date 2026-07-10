#!/usr/bin/env python3
"""Smoke test for held messages: while a player is in an editor, game messages
(says, combat, arrivals) are NOT pushed at them -- they buffer up and are
reviewed with `catchup`.

  1. A player in the edroom editor does not receive a roommate's `say`.
  2. On leaving the editor they're told messages arrived.
  3. `catchup` replays them, then reports nothing left.
  4/5. Same coverage for `edplayer` and `edzone` (Session 43 bug: these
     were never actually wired into the hold check -- descriptor_in_editor()
     only recognized the CONN_REDIT_* range, so a say landed on someone
     mid-edplayer/edzone anyway, despite descriptor_notify() being called
     correctly everywhere. User: "when in the editors, no messages to
     interrupt, no logs, no output at all. thats what catchup is for.").

    python3 tests/smoke_test_held.py [host] [port]
"""
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


announce("smoke_test_held")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 900000 + (int(time.time()) % 90000)


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


def sql(stmt):
    subprocess.run(["mariadb", "sneezy", "-e", stmt], check=True)


def make_immortal(nm):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (nm, "heldpw", "heldpw", "new", nm, "done"):
        send_line(s, step); recv_all(s)
    s.close()
    sql(f"UPDATE player_progress SET level=51 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{nm}');")
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (nm, "heldpw", "1"):
        send_line(s, step); recv_all(s)
    return s


# A sandbox room both builders share.
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({BASE},0,0,0,'Held Sandbox','A quiet test room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")

nameA, nameB = f"Helda{_suffix}", f"Heldb{_suffix}"
A = make_immortal(nameA)
B = make_immortal(nameB)
cmd(A, "color off"); cmd(B, "color off")
cmd(A, f"goto {BASE}"); cmd(B, f"goto {BASE}")
recv_all(A); recv_all(B)

# A enters the room editor.
check("Menu:" in cmd(A, "edroom"), "A is in the edroom editor")

# B says something in the same room (a held-worthy message).
cmd(B, "say something A must not see yet")

# A, mid-editor, should have received nothing.
leaked = recv_all(A, 0.5)
check("says" not in leaked and "must not see" not in leaked,
      "A gets no game message while editing")

# B drops link -- the [LOG] notice to immortals must NOT be held (it's always
# in the log command), so it can't bury the real messages in catchup.
B.close()
time.sleep(0.6)
recv_all(A, 0.4)  # still nothing pushed to the editing immortal

# A leaves the editor and is told messages are waiting.
out = cmd(A, "Q")
check("arrived while you were editing" in out, "on exit A is told messages arrived")

# catchup replays the held say but NOT the log line, then is empty.
out = cmd(A, "catchup")
check("must not see yet" in out and "says" in out, "catchup replays the held say")
check("[LOG]" not in out, "log lines are NOT held (they live in the log command)")
check("haven't missed anything" in cmd(A, "catchup"), "catchup is empty after reading")

# --- 4/5: same coverage for edplayer and edzone (the actual Session 43 bug) ---
nameC, nameD = f"Heldc{_suffix}", f"Heldd{_suffix}"
C = make_immortal(nameC)
D = make_immortal(nameD)
sql(f"UPDATE player_progress SET level=58 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{nameC}');")  # EDPLAYER_MIN_LEVEL
cmd(C, "quit!")  # not an abrupt close -- avoids linkdead-resume overriding the fresh load
C.close()
C = socket.create_connection((host, port), timeout=5)
recv_all(C)
for step in (nameC, "heldpw", "1"):
    send_line(C, step); recv_all(C)
cmd(C, "color off"); cmd(D, "color off")
cmd(C, f"goto {BASE}"); cmd(D, f"goto {BASE}")
recv_all(C); recv_all(D)

# 4: edplayer
check("Editing player:" in cmd(C, f"edplayer {nameD}"), "C is in the edplayer editor")
cmd(D, "say should be held during edplayer too")
leaked = recv_all(C, 0.5)
check("says" not in leaked, "C gets nothing while mid-edplayer (was the actual bug)")
out = cmd(C, "Q")
check("arrived while you were editing" in out, "on exit C is told messages arrived (edplayer)")
out = cmd(C, "catchup")
check("should be held during edplayer too" in out, "catchup replays the held say (edplayer)")

# 5: edzone
ZONE = 90000 + (int(time.time()) % 9000)
sql(f"INSERT INTO zone (zone_nr,zone_name,zone_enabled,bottom,top,reset_mode,lifespan,age,util_flag) "
    f"VALUES ({ZONE},'Held Zone Sandbox',1,{BASE},{BASE},2,999999,0,0);")
check("Editing zone:" in cmd(C, f"edzone {ZONE}"), "C is in the edzone editor")
cmd(D, "say should be held during edzone too")
leaked = recv_all(C, 0.5)
check("says" not in leaked, "C gets nothing while mid-edzone (was the actual bug)")
out = cmd(C, "Q")
check("arrived while you were editing" in out, "on exit C is told messages arrived (edzone)")
out = cmd(C, "catchup")
check("should be held during edzone too" in out, "catchup replays the held say (edzone)")

A.close()
C.close()
D.close()
announce_done("smoke_test_held")
print("=== ALL CHECKS PASSED ===")
