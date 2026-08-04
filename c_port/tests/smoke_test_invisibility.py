#!/usr/bin/env python3
"""Smoke test for `invisibility` and `dispel invisible` (Mage, level 17) --
spell/skill functional-completeness audit continued. See cmd_cast.c's
invisibility/dispel-invisible branches for the real-upstream research
(disc_mage_spirit.cc's invisibility()/dispelInvisible()) and scope-down
rationale, and affect.h's AFFECT_INVISIBLE doc comment for where it's
checked (combat_find_room_target(), cmd_look.c's room listing).

  1. `cast invisibility` (self) applies AFFECT_INVISIBLE, shows in `affects`.
  2. An invisible PC can't be targeted by name (`attack`) by a mortal.
  3. An invisible PC doesn't show in a mortal's `look` room listing.
  4. An immortal can still target AND see an invisible PC (both gates
     are bypassed for them).
  5. `cast dispel invisible <target>` strips the affect -- the target is
     targetable and visible again afterward.
  6. `cast dispel invisible` on an already-visible target is refused with
     a clear "already visible" message instead of silently doing nothing.

    python3 tests/smoke_test_invisibility.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_invisibility", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 979000 + (int(time.time()) % 1000)
COMPONENT = ROOM + 1

CLASS_MAGE = 0
CLASS_WARRIOR = 2
WEAR_TAKE = 1


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


sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Invisibility Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,{WEAR_TAKE},1);")

pw = "invispw123456"
mage_name = f"Invmag{_suffix}"
mort_name = f"Invmor{_suffix}"
imm_name = f"Invimm{_suffix}"

sockets = []
try:
    for name in (mage_name, mort_name, imm_name):
        make_char(name, pw)
    sql(f"UPDATE player SET class={CLASS_MAGE} WHERE name='{mage_name}';")
    sql(f"UPDATE player SET class={CLASS_WARRIOR} WHERE name='{mort_name}';")
    sql(f"UPDATE player SET class={CLASS_WARRIOR} WHERE name='{imm_name}';")
    for name in (mage_name, mort_name, imm_name):
        sql(f"UPDATE player SET load_room={ROOM} WHERE name='{name}';")
    # Mage caster: level 51+ (immortal) bypasses `cast`'s own separate
    # learn-by-doing proficiency roll entirely, same reason
    # smoke_test_curse_slumber.py's own casters are level 51 -- a fresh
    # character's first-ever attempt at "invisibility" would otherwise
    # only succeed ~1% of the time.
    sql(f"UPDATE player_progress SET level=51 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{mage_name}');")
    sql(f"UPDATE player_progress SET level=20 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{mort_name}');")
    sql(f"UPDATE player_progress SET level=59 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")

    sm = relog(mage_name, pw); sockets.append(sm)
    smort = relog(mort_name, pw); sockets.append(smort)
    simm = relog(imm_name, pw); sockets.append(simm)
    for s in (sm, smort, simm):
        cmd(s, "toggle pk")

    # --- 1: cast invisibility (self), shows in affects ---
    cmd(sm, f"load obj {COMPONENT}"); recv_all(sm, 0.3)
    cmd(sm, "get pouch"); recv_all(sm, 0.3)
    out1 = strip(cmd(sm, "cast invisibility"))
    check("fade from view" in out1.lower(), "cast invisibility (self) succeeds")
    out1b = strip(cmd(sm, "affects"))
    check("invisible" in out1b.lower(), "the `affects` command lists Invisible while it's active")

    # --- 2: an invisible PC can't be targeted by name (attack) by a mortal ---
    out2 = strip(cmd(smort, f"attack {mage_name}"))
    check("aren't here" in out2.lower(), "a mortal can't target the invisible mage by name")

    # --- 3: an invisible PC doesn't show in a mortal's `look` room listing ---
    out3 = strip(cmd(smort, "look"))
    check(mage_name.lower() not in out3.lower(), "the invisible mage doesn't show in a mortal's room listing")

    # --- 4: an immortal can still target AND see an invisible PC ---
    out4 = strip(cmd(simm, "look"))
    check(mage_name.lower() in out4.lower(), "an immortal still sees the invisible mage in the room listing")
    out4b = strip(cmd(simm, f"consider {mage_name}"))
    check("consider killing whom" not in out4b.lower(), "an immortal can still target the invisible mage by name")

    # --- 5: dispel invisible strips the affect -- targetable/visible again ---
    # No explicit target name -- combat_find_room_target() (used to
    # resolve an EXPLICIT target) always excludes the caster from its
    # own search, same "can't target yourself by name" behavior
    # documented across bodyslam/chi/headbutt -- the real usage for
    # dispelling your OWN invisibility is the plain self-default target,
    # same as "cast invisibility" itself used with no target in step 1.
    cmd(sm, f"load obj {COMPONENT}"); recv_all(sm, 0.3)
    cmd(sm, "get pouch"); recv_all(sm, 0.3)
    out5 = strip(cmd(sm, "cast dispel invisible"))
    check("visible again" in out5.lower(), "cast dispel invisible succeeds against an invisible target")
    out5b = strip(cmd(smort, "look"))
    check(mage_name.lower() in out5b.lower(), "the mage shows up again in a mortal's room listing after dispel")
    out5c = strip(cmd(smort, f"consider {mage_name}"))
    check("consider killing whom" not in out5c.lower(), "a mortal can target the mage by name again after dispel")

    # --- 6: dispel invisible on an already-visible target is refused clearly ---
    cmd(sm, f"load obj {COMPONENT}"); recv_all(sm, 0.3)
    cmd(sm, "get pouch"); recv_all(sm, 0.3)
    out6 = strip(cmd(sm, "cast dispel invisible"))
    check("already visible" in out6.lower(), "dispel invisible on an already-visible target says so instead of silently doing nothing")

    announce_done("smoke_test_invisibility", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    for prefix in ("Invmag", "Invmor", "Invimm"):
        sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player WHERE name LIKE '{prefix}%{_suffix}';")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
    sql(f"DELETE FROM obj WHERE vnum={COMPONENT};")
