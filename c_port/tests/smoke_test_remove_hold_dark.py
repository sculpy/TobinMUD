#!/usr/bin/env python3
"""Smoke test for `remove hold` in the dark (user 2026-08-03: "if its
dark and you type remove hold you should remove whatever your holding
and it placed in inventory"). Normal `remove <item>` targets an item by
its own name/keywords (find_worn(), cmd_object.c) -- fine if you can
see what you're holding, but useless in a genuinely dark room where
you've never identified it. `remove hold`/`remove held` is a new
pseudo-target naming the held SLOT itself (primary hand first), so it
works purely from knowing something is in your hand, independent of
whether you can see it. The removed item was already in the being's
carried stuff_head chain the whole time (held[]/equipment[] are just
references into it, not separate storage) -- `inventory` already lists
it once it's no longer referenced by held[].

Covers:
  1. A plain outdoor room at night with no light source is genuinely
     dark (`look` shows only darkness) -- sanity check the setup.
  2. `remove hold` in that dark room succeeds and reports the held
     item, even though its name was never typed.
  3. The item shows up in `inventory` afterward (was placed back in
     inventory, not dropped/destroyed).
  4. A second `remove hold` with nothing left in hand refuses cleanly.

    python3 tests/smoke_test_remove_hold_dark.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_remove_hold_dark", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 941000 + (int(time.time()) % 50000)
TORCH_VNUM = 105  # real seeded OBJ_CAT_LIGHT holdable, starts unlit


def make_char(name, pw, class_choice):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", class_choice, "done", "done"):
        send_line(s, step); recv_all(s)
    cmd(s, "quit!")
    s.close()


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


imm_name, imm_pw = f"Rhdimm{_suffix}", "rhdimmpw1234"
mort_name, mort_pw = f"Rhdmor{_suffix}", "rhdmorpw1234"

try:
    make_char(imm_name, imm_pw, "1")
    sql(f"UPDATE player_progress SET level=51 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
    make_char(mort_name, mort_pw, "3")

    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({ROOM},0,0,0,'Remove Hold Dark Sandbox','A bare sandbox room.\\n',NULL,0,0,0,0,0,0,0,0,0,0);")
    sql(f"UPDATE player SET load_room={ROOM} WHERE name IN ('{imm_name}', '{mort_name}');")

    si = relog(imm_name, imm_pw)
    sm = relog(mort_name, mort_pw)

    cmd(si, f"load obj {TORCH_VNUM}")
    cmd(si, "drop torch")
    check("You get" in cmd(sm, "get torch") or "get" in cmd(sm, "inventory").lower(),
          "the mortal picks up the (unlit) torch")
    out = cmd(sm, "hold torch")
    check("hold" in out.lower() or "you hold" in out.lower(), "the mortal holds the torch")

    # Force real night (same bounded-polling technique smoke_test_weather.py
    # uses) so the room is genuinely dark with no light source active.
    for _ in range(10):
        out = cmd(si, "weather")
        if "dark" in out:
            break
        cmd(si, "aitick 10")
    check("dark" in out, "forced the game clock into the night window")

    out = cmd(sm, "look")
    check("pitch black" in out, "the sandbox room is genuinely dark (unlit torch, no other light)")

    out = cmd(sm, "remove hold")
    check("you remove" in out.lower(), "`remove hold` succeeds in the dark without naming the item")
    check("torch" in out.lower(), "the removed-item message correctly names the torch")

    out = cmd(sm, "inventory")
    check("torch" in out.lower(), "the torch is back in inventory, not dropped or destroyed")

    out = cmd(sm, "remove hold")
    check("aren't holding anything" in out.lower(),
          "a second `remove hold` with empty hands refuses cleanly")

    announce_done("smoke_test_remove_hold_dark", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    # This test forces the shared server's game clock into night and
    # leaves it there (persisted, survives past this run) -- restore a
    # neutral daytime hour so it doesn't silently break some OTHER,
    # unrelated test's plain (non-ALWAYS-LIT) sandbox room next time
    # anything calls `look` there (same concern smoke_test_weather.py's
    # own cleanup documents).
    try:
        for _ in range(10):
            out = cmd(si, "weather")
            if "daylight" in out:
                break
            cmd(si, "aitick 10")
    except (NameError, OSError):
        pass
    for _sock_name in ("si", "sm"):
        _sock = locals().get(_sock_name)
        if _sock is not None:
            try:
                _sock.close()
            except OSError:
                pass
    sql(f"DELETE FROM player_inventory WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{mort_name}'));")
    sql(f"DELETE FROM player_progress WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{mort_name}'));")
    sql(f"DELETE FROM player_attrs WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{mort_name}'));")
    sql(f"DELETE FROM player WHERE name IN ('{imm_name}', '{mort_name}');")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
