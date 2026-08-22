#!/usr/bin/env python3
"""Smoke test for the Sneezy-style prompt rework and the new `toggle`
entries (user request, 2026-08-22: "rework the prompt to be more like
sneezy and update the toggles related to prompt").

Real Sneezy's StPrompts[] table (connect.cc) renders "H:%d "/"M:%d "/
"V:%d "/"E:%s "/"N:%s "/"LF:%d " -- compact letter-colon-value, not
spelled-out labels. Ported letter-for-letter except gold: Sneezy's own
is "T:" for Talens, a currency Tobin doesn't have, so "G:" here
instead. `prompt <stat>`/`prompt all` still control the exact same
player.prompt_flags bits as before; only the rendered format changed.

`toggle hp` already duplicated `prompt hp` before this -- the other
five prompt stats (gold/vit/mana/exp/expneed) were reachable only
through `prompt <stat>`. This closes that gap: `toggle <stat>` now
works for all six, all writing the same underlying bits `prompt`
does.

    python3 tests/smoke_test_prompt_sneezy.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_prompt_sneezy", host, port)
_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

name = f"Prsn{_suffix}"
pw = "prsntestpw12345"
s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "y"); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "new"); recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, "1"); recv_all(s)
send_line(s, "1"); recv_all(s)
send_line(s, "1"); recv_all(s)
send_line(s, "done"); recv_all(s)
send_line(s, "done"); recv_all(s)
s.close()

s = socket.create_connection((host, port), timeout=5)
recv_all(s)
send_line(s, name); recv_all(s)
send_line(s, pw); recv_all(s)
send_line(s, "1"); recv_all(s)
cmd(s, "color off")

sockets = [s]
try:
    cmd(s, "prompt all")
    out = cmd(s, "look")
    check("H:" in out and "V:" in out and "M:" in out and "E:" in out and "N:" in out and "G:" in out,
          "prompt all shows every Sneezy-letter stat (H/G/V/M/E/N)")
    check("HP:" not in out and "Gold:" not in out and "Vit:" not in out
          and "Exp:" not in out and "ExpNeed:" not in out,
          "the old spelled-out labels are gone")

    out = cmd(s, "toggle gold")
    check("gold is now off" in out, "toggle gold works and reports the new state")
    out = cmd(s, "look")
    check("G:" not in out, "toggle gold actually removed G: from the live prompt")

    out = cmd(s, "prompt gold")
    check("now show gold" in out, "prompt gold (the other command) flips the SAME bit back on")
    out = cmd(s, "look")
    check("G:" in out, "prompt gold turned G: back on -- toggle and prompt share state")

    for stat in ("vit", "mana", "exp", "expneed"):
        out = cmd(s, f"toggle {stat}")
        check(f"{stat} is now off" in out, f"toggle {stat} is a real, working toggle entry")
        cmd(s, f"toggle {stat}")  # back on, tidy

    announce_done("smoke_test_prompt_sneezy", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Prsn%{_suffix}');")
    sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE 'Prsn%{_suffix}');")
    sql(f"DELETE FROM player WHERE name LIKE 'Prsn%{_suffix}';")
