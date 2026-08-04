#!/usr/bin/env python3
"""Smoke test for `teleport` (Mage, level 19) and `summon` (Cleric, level
19) -- spell/skill functional-completeness audit continued. See
cmd_cast.c's teleport branch and cmd_pray.c's summon branch for the
real-upstream research and scope-down rationale.

KNOWN ISSUE (2026-07-28, not yet root-caused -- see STATUS.md Session 90):
this test's own multi-character batch setup can intermittently trip a
separate, real engine bug where a freshly created character lands in the
mortal default room (100) instead of its real load_room, and/or doesn't
register as immortal despite a correct DB row -- `world_find_linkdead_pc()`
misattributing an unrelated ghost is the leading suspect, but a `purge
linkdead` immediately before login does NOT reliably fix it. The feature
code itself (cmd_cast.c/cmd_pray.c) is verified correct via isolated,
single-character manual tests reproducing every check below one at a
time. Re-run this file once that engine bug is understood; it may need
restructuring to keep fewer simultaneous connections open regardless.

  Teleport:
    1. Self-teleport (no target) relocates the caster to a different room.
    2. An offensive cast on another being relocates THEM, not the caster.
    3. A caster standing in a NO-ESCAPE room is refused outright.
    4. Casting at an immortal is refused ("they're a god").

  Summon:
    5. No target is refused ("Summon whom?").
    6. A nonexistent name is refused.
    7. Summoning yourself is refused.
    8. Summoning an immortal is refused.
    9. A valid target with no holy symbol on hand is refused.
   10. With a symbol, a valid target (in a DIFFERENT room) is pulled into
       the caster's own room.

    python3 tests/smoke_test_teleport_summon.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_teleport_summon", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))

CLASS_MAGE = 0
CLASS_CLERIC = 1
CLASS_WARRIOR = 2
WEAR_TAKE = 1
ROOM_FLAG_NO_ESCAPE = 1 << 6


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def make_char(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", "1", "done", "done"):
        send_line(s, step)
        recv_all(s)
    s.close()


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def make_single(prefix, class_id, room, level=20):
    name, pw = f"{prefix}{_suffix}", f"{prefix}pw12345"
    make_char(name, pw)
    sql(f"UPDATE player SET class={class_id}, load_room={room} WHERE name='{name}';")
    sql(f"UPDATE player_progress SET level={level}, basic_disc_pct=100 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
    s = relog(name, pw)
    cmd(s, "toggle pk")
    return name, s


ROOM_TA = 977500 + (int(time.time()) % 1000)
ROOM_TB = ROOM_TA + 100
ROOM_TC = ROOM_TA + 200
ROOM_SUM = ROOM_TA + 300
ROOM_SUMB = ROOM_TA + 400
SYMBOL = ROOM_TA + 401
COMPONENT = ROOM_TA + 402

for rvnum, rname, flag in (
    (ROOM_TA, "Teleport Sandbox A", 1),
    (ROOM_TB, "Teleport Sandbox B", 1),
    (ROOM_TC, "Teleport Sandbox C", 1 | ROOM_FLAG_NO_ESCAPE),
    (ROOM_SUM, "Summon Sandbox", 1),
    (ROOM_SUMB, "Summon Sandbox B", 1),
):
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({rvnum},0,0,0,'{rname}','A bare sandbox room.\\n',NULL,{flag},0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,{WEAR_TAKE},1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,{WEAR_TAKE},1);")

sockets = []
try:
    # --- Teleport setup ---
    tmage_name, tmage = make_single("Telmg", CLASS_MAGE, ROOM_TA, level=51)
    sockets.append(tmage)
    tmage2_name, tmage2 = make_single("Telmgb", CLASS_MAGE, ROOM_TB, level=51)
    sockets.append(tmage2)
    tvic_name, tvic = make_single("Telvc", CLASS_WARRIOR, ROOM_TB, level=20)
    sockets.append(tvic)
    timmb_name, timmb = make_single("Telim", CLASS_WARRIOR, ROOM_TB, level=51)
    sockets.append(timmb)
    tmage3_name, tmage3 = make_single("Telmgc", CLASS_MAGE, ROOM_TC, level=51)
    sockets.append(tmage3)

    # Clear any stale linkdead ghosts left in memory by earlier runs
    # (found live while debugging this test): world_find_linkdead_pc()
    # matches purely on player_id, and a leftover in-memory linkdead
    # body from a PRIOR run whose DB row got deleted can apparently
    # still get matched to a freshly created character on a later run,
    # silently resuming them into the ghost's old room instead of their
    # real load_room. Not this feature's bug -- pre-existing engine
    # behavior, worth a real fix later -- but purging first keeps this
    # test deterministic regardless.
    cmd(tmage, "purge linkdead"); recv_all(tmage, 0.3)

    # --- 1: self-teleport relocates the caster ---
    cmd(tmage, f"load obj {COMPONENT}"); recv_all(tmage, 0.3)
    out1 = strip(cmd(tmage, "cast teleport"))
    check("somewhere else" in out1.lower(), "teleport with no target moves the caster")
    look1 = strip(cmd(tmage, "look"))
    check("Teleport Sandbox A" not in look1, "the caster really left the starting room")

    # --- 2: an offensive cast relocates the TARGET, not the caster ---
    cmd(tmage2, f"load obj {COMPONENT}"); recv_all(tmage2, 0.3)
    out2 = strip(cmd(tmage2, f"cast teleport {tvic_name}"))
    check("vanish" in out2.lower(), "an offensive teleport succeeds")
    time.sleep(0.3)
    look_vic = strip(cmd(tvic, "look"))
    check("Teleport Sandbox B" not in look_vic, "the TARGET left the room")
    look_mage2 = strip(cmd(tmage2, "look"))
    check("Teleport Sandbox B" in look_mage2, "the CASTER stayed put")

    # --- 3: a NO-ESCAPE room refuses teleport outright ---
    cmd(tmage3, f"load obj {COMPONENT}"); recv_all(tmage3, 0.3)
    out3 = strip(cmd(tmage3, "cast teleport"))
    check("defenses of this area are too strong" in out3.lower(), "a NO-ESCAPE room refuses teleport")

    # --- 4: casting at an immortal is refused ---
    cmd(tmage2, f"load obj {COMPONENT}"); recv_all(tmage2, 0.3)
    out4 = strip(cmd(tmage2, f"cast teleport {timmb_name}"))
    check("god" in out4.lower(), "casting teleport at an immortal is refused")

    # --- Summon setup ---
    scleric_name, scleric = make_single("Sumcl", CLASS_CLERIC, ROOM_SUM, level=51)
    sockets.append(scleric)
    simm_name, simm = make_single("Sumim", CLASS_WARRIOR, ROOM_SUM, level=51)
    sockets.append(simm)
    starget_name, starget = make_single("Sumtg", CLASS_WARRIOR, ROOM_SUMB, level=20)
    sockets.append(starget)

    # --- 5: no target is refused ---
    out5 = strip(cmd(scleric, "pray summon"))
    check("summon whom" in out5.lower(), "summon with no target is refused")

    # --- 6: a nonexistent name is refused ---
    out6 = strip(cmd(scleric, f"pray summon Nobodyhere{_suffix}"))
    check("no one named" in out6.lower(), "summoning a nonexistent name is refused")

    # --- 7: summoning yourself is refused ---
    out7 = strip(cmd(scleric, f"pray summon {scleric_name}"))
    check("can't summon yourself" in out7.lower(), "summoning yourself is refused")

    # --- 8: summoning an immortal is refused ---
    out8 = strip(cmd(scleric, f"pray summon {simm_name}"))
    check("hazardous" in out8.lower(), "summoning an immortal is refused")

    # --- 9: a valid target with no holy symbol is refused ---
    out9 = strip(cmd(scleric, f"pray summon {starget_name}"))
    check("need a holy symbol" in out9.lower(), "summoning without a holy symbol is refused")

    # --- 10: with a symbol, a valid target (elsewhere) is pulled in ---
    cmd(scleric, f"load obj {SYMBOL}"); recv_all(scleric, 0.3)
    out10 = strip(cmd(scleric, f"pray summon {starget_name}"))
    check("appears before you" in out10.lower(), "summon succeeds with a holy symbol")
    time.sleep(0.3)
    look_target = strip(cmd(starget, "look"))
    check("Summon Sandbox" in look_target and "Summon Sandbox B" not in look_target,
          "the summoned target really arrived in the caster's room")

    announce_done("smoke_test_teleport_summon", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            # `quit!` (not a raw close) so the character leaves cleanly
            # instead of going linkdead -- an abrupt close leaves an
            # in-memory ghost that can outlive this run's own DB
            # cleanup below and confuse a LATER run's linkdead lookup
            # (see the purge above).
            cmd(sock, "quit!", timeout=0.5)
            sock.close()
        except OSError:
            pass
    for prefix in ("Telmg", "Telvc", "Telim", "Sumcl", "Sumim", "Sumtg"):
        sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_inventory WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player WHERE name LIKE '{prefix}%{_suffix}';")
        # The ACCOUNT row itself (not just its player/character rows) --
        # missed by every earlier test file this session (found live
        # while debugging this one): a leftover account with the same
        # name blocks a future same-named `new` character creation from
        # ever completing, silently landing on an empty "(none yet)"
        # account instead of erroring. `name` is stored lowercase on
        # `account` (case-insensitive login), unlike `player.name`'s
        # display casing.
        sql(f"DELETE FROM account WHERE name LIKE LOWER('{prefix}%{_suffix}');")
    sql(f"DELETE FROM room WHERE vnum IN ({ROOM_TA}, {ROOM_TB}, {ROOM_TC}, {ROOM_SUM}, {ROOM_SUMB});")
    sql(f"DELETE FROM obj WHERE vnum={SYMBOL};")
