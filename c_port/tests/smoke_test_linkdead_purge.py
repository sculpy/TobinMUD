#!/usr/bin/env python3
"""Smoke test for the linkdead auto-purge (TODO.md PRIORITY item, user
2026-07-20: "a linkdead PC's being_t stays fully resident in its room
forever... found live... the proximate cause of a real slowdown/hang").
world.c's linkdead_purge_tick() (pulse_register(600, ...), ~60s cadence)
force-saves (player_save()) then destroys any linkdead PC that has been
linkdead at least config_get()->linkdead_purge_seconds (env var
TOBIN_LINKDEAD_PURGE_SECONDS, default 300/5min -- a flat threshold for
everyone, the user's deliberate simplification of the real Sneezy's
nukeLdead() 15min-mortal/60min-immortal split).

Both halves of the real behavior were manually verified this session
with the server temporarily restarted under TOBIN_LINKDEAD_PURGE_SECONDS=5:
  1. A raw-socket-closed (not `quit`) character's `being_t` survives
     immediately after disconnect (goto/who still find it), then is
     force-removed once past the threshold + one ~60s check cycle
     (`who`'s Linkdead count returns to 0, `goto <name>` no longer finds
     them).
  2. The save half is real, not a discard: moved a character to drop its
     Vitality below max (charged moves save immediately), waited ~16s so
     regen_tick_run() healed Vitality further IN MEMORY ONLY (regen
     never itself calls player_progress_save() -- see STATUS.md's
     Vitality/Terrain writeup), confirmed the DB value was still the
     lower pre-regen number, then disconnected and let the purge fire.
     The DB value afterward matched the HIGHER live-regenerated number
     exactly, proving linkdead_purge_tick() saves current live state at
     purge time, not a stale snapshot or a bare discard.

That full end-to-end cycle isn't practical to keep as an automated smoke
test against the standing preview/production instances: with the real
default threshold (300s) it needs waiting up to ~6 real minutes (5min
threshold + up to ~60s until the next check pulse), same
"not practical to keep automated" call smoke_test_heartbeat.py already
makes for its own real-time boundary. What THIS test verifies without
that wait:
  1. A raw-socket disconnect actually produces a linkdead body (`who`'s
     Linkdead count goes up by one) rather than silently vanishing or
     erroring.
  2. That body is NOT prematurely purged -- reconnecting to the same
     account immediately after resumes in the same room (enter_world()'s
     existing linkdead-resume behavior), proving the being_t was still
     there right after disconnect, not destroyed early.

    python3 tests/smoke_test_linkdead_purge.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, check, sql, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))


def recv_all(sock, timeout=1.5):
    _deadline = time.monotonic() + max(8.0, timeout * 8)
    chunks = []
    try:
        while True:
            _remaining = _deadline - time.monotonic()
            if _remaining <= 0:
                break
            sock.settimeout(min(timeout, _remaining))
            data = sock.recv(4096)
            if not data:
                break
            chunks.append(data)
    except socket.timeout:
        pass
    return b"".join(chunks).decode(errors="replace")


def cmd(sock, line, timeout=1.5):
    send_line(sock, line)
    return recv_all(sock, timeout)


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)  # race: human
    send_line(s, "1"); recv_all(s)  # territory: urban
    send_line(s, "1"); recv_all(s)  # class: mage
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)  # alignment: neutral
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    out = cmd(s, "1")  # account menu: Connect Player
    cmd(s, "color off")
    return s, out


def who_linkdead_count(sock):
    out = strip(cmd(sock, "who", 2.0))
    m = re.search(r"Linkdead:\s*\[(\d+)\]", out)
    return int(m.group(1)) if m else None


announce("smoke_test_linkdead_purge", host, port)

name = f"Ldpz{_suffix}"
pw = "ldpztestpw1"

s1 = make_char(name, pw)
cmd(s1, "color off")

before = who_linkdead_count(s1)
check(before is not None, "who reports a Linkdead: [N] count")

s1.close()  # raw close, NOT quit -- goes linkdead, not destroyed
time.sleep(1.5)

s2 = make_char(f"Ldpzw{_suffix}", "ldpzwpw1")
after_disconnect = who_linkdead_count(s2)
check(after_disconnect == before + 1,
      "a raw socket close (not quit) leaves a linkdead body -- who's count goes up by one")

# Immediately reconnecting the SAME account resumes the same in-world
# being_t (enter_world()'s existing behavior) rather than a fresh one --
# proof the linkdead body from above is still alive in memory, i.e. the
# purge has NOT (and, given the real 300s default threshold, could not
# have) already fired.
s3, out = relog(name, pw)
check("Welcome back" in strip(out) or "resume where you left off" in strip(out).lower(),
      "reconnecting the same account resumes the linkdead body instead of erroring or creating a duplicate")

after_reconnect = who_linkdead_count(s2)
check(after_reconnect == before,
      "reconnecting clears the linkdead body -- who's count drops back to baseline")

s2.close()
s3.close()

# Sandbox cleanup -- quit! wasn't available (both test sockets closed
# raw/via relog without a clean logout), so just remove the DB rows.
sql(f"DELETE FROM player_progress WHERE player_id IN "
    f"(SELECT id FROM player WHERE name IN ('{name}', 'Ldpzw{_suffix}'));")
sql(f"DELETE FROM player_attrs WHERE player_id IN "
    f"(SELECT id FROM player WHERE name IN ('{name}', 'Ldpzw{_suffix}'));")
sql(f"DELETE FROM player WHERE name IN ('{name}', 'Ldpzw{_suffix}');")

print("=== ALL CHECKS PASSED ===")
announce_done("smoke_test_linkdead_purge", host, port)
