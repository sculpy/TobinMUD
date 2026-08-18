#!/usr/bin/env python3
"""Smoke test for the Session-158-backlog Monk "iron" family + two actives
(2026-08-18): iron flesh (31), iron skin (35), iron bones (38), iron
muscles (42), iron will (48), defenestrate (42), bonebreak (50). See
combat.c (passives), cmd_cast.c (iron will resist), cmd_defenestrate.c,
cmd_bonebreak.c.

Mortals are level 50 (MORTAL_LEVEL_MAX -- 51+ is immortal, which would
skip every `!being_is_immortal` gate). Passives are trained the same way
smoke_test_combat_passives_generic.py does: the mortal Monk swings at an
immortal (attacker-side: iron muscles) and the immortal swings at it
(defender-side: iron flesh/iron skin). The Monk's defenestrate/bonebreak
proficiencies are maxed via player_skill so the ACTIVE rolls are
deterministic.

    python3 tests/smoke_test_monk_iron.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26 ** i) % 26) for i in range(4))
ROOM = 958000 + (int(time.time()) % 15000)


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def skill_pct(name, skill_name):
    out = query(f"SELECT pct FROM player_skill WHERE player_id="
                f"(SELECT id FROM player WHERE name='{name}') AND skill_name='{skill_name}';")
    return int(out) if out else 0


def make_char(name, pw, class_num):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw, "new", name, "1", "1", str(class_num), "done", "done"):
        send_line(s, step)
        recv_all(s)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "color off")
    return s


def act_while_recovering(sock, verb, tries=8, timeout=0.5):
    for _ in range(tries):
        out = cmd(sock, verb, timeout)
        if "still recovering" not in out.lower():
            return out
    return out


def ensure_pk_on(sock):
    """pk state persists across relog, so a blind toggle can turn it OFF.
    Force it ON."""
    out = cmd(sock, "toggle pk", 0.6).lower()
    if "now off" in out:
        cmd(sock, "toggle pk", 0.6)


def engage(attacker, target_name, tries=5):
    """Start a fight, retrying past a pk-toggle/login race."""
    for _ in range(tries):
        out = cmd(attacker, f"attack {target_name}", 0.6)
        low = out.lower()
        if "you attack" in low or "already fighting" in low:
            return True
        time.sleep(0.5)
    return False


announce("smoke_test_monk_iron", host, port)

sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Iron Dojo','A bare training hall.\\n',NULL,9,0,0,0,0,0,0,0,0,0);")

ig, igpw = f"Irg{_suffix}", "irgpw1234567"   # immortal Mage (spar partner + casts fear)
mk, mkpw = f"Irk{_suffix}", "irkpw1234567"   # mortal Monk -- knows the whole iron family
vc, vcpw = f"Irv{_suffix}", "irvpw1234567"   # mortal Warrior victim (no iron bones)
mb, mbpw = f"Irb{_suffix}", "irbpw1234567"   # mortal Monk victim (knows iron bones)

sig = make_char(ig, igpw, 1)  # Mage
cmd(sig, "quit!"); sig.close()
smk = make_char(mk, mkpw, 6)  # Monk
cmd(smk, "quit!"); smk.close()
svc = make_char(vc, vcpw, 3)  # Warrior
cmd(svc, "quit!"); svc.close()
smb = make_char(mb, mbpw, 6)  # Monk
cmd(smb, "quit!"); smb.close()

sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{ig}');")
for who in (mk, mb):
    sql(f"UPDATE player_progress SET level=50, basic_disc_pct=100, combat_disc_pct=100, "
        f"advanced_disc_pct=100 WHERE player_id=(SELECT id FROM player WHERE name='{who}');")
    sql(f"UPDATE player SET load_room={ROOM} WHERE name='{who}';")
sql(f"UPDATE player_progress SET level=45, basic_disc_pct=100, combat_disc_pct=100 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{vc}');")
sql(f"UPDATE player SET load_room={ROOM} WHERE name='{vc}';")
for skn in ("defenestrate", "bonebreak"):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct) "
        f"SELECT id, '{skn}', 100 FROM player WHERE name='{mk}' "
        f"ON DUPLICATE KEY UPDATE pct=100;")

sig = relog(ig, igpw); cmd(sig, f"goto {ROOM}")

# ===================== PASSIVES: iron flesh / skin / muscles =====================
smk = relog(mk, mkpw)
check("Iron Dojo" in cmd(smk, "look"), "the mortal Monk is in the dojo")
for sk in ("iron flesh", "iron skin", "iron muscles"):
    check(skill_pct(mk, sk) == 0, f"{sk} starts untrained")
# attacker-side: the Monk swings barehanded at the immortal -> iron muscles.
send_line(smk, f"hit {ig}"); time.sleep(3.0); recv_all(smk, 0.3); recv_all(sig, 0.3)
check(skill_pct(mk, "iron muscles") >= 1, "iron muscles trains from barehanded attacks")
# defender-side: the immortal swings at the Monk -> iron flesh / iron skin.
send_line(sig, f"hit {mk}"); time.sleep(3.0); recv_all(sig, 0.3); recv_all(smk, 0.3)
check(skill_pct(mk, "iron flesh") >= 1, "iron flesh trains from hits taken barehanded")
check(skill_pct(mk, "iron skin") >= 1, "iron skin trains from hits taken")

# ===================== IRON WILL: resist fear =====================
cmd(smk, "quit!"); smk.close()
smk = relog(mk, mkpw)
ensure_pk_on(smk); ensure_pk_on(sig)
out = cmd(sig, f"cast fear {mk}", timeout=1.5)
check("throws off" in out.lower() and "iron will" in out.lower(),
      f"iron will throws off a fear spell: {out[:90]!r}")

# ===================== ACTIVES: defenestrate + bonebreak =====================
svc = relog(vc, vcpw)
cmd(smk, "quit!"); smk.close()
sql(f"UPDATE player_progress SET hp=9999 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{mk}');")
smk = relog(mk, mkpw)
ensure_pk_on(smk); ensure_pk_on(svc)
check(engage(smk, vc), "the Monk engages the warrior victim")
time.sleep(1.0); recv_all(smk, 0.3)

out = act_while_recovering(smk, "defenestrate")
check("hurl" in out.lower() and "ground" in out.lower(),
      f"defenestrate hurls the opponent down: {out[:80]!r}")

out = act_while_recovering(smk, "bonebreak")
check("snaps" in out.lower(), f"bonebreak snaps a bone: {out[:80]!r}")
check("broken bone" in cmd(svc, "affects").lower(),
      "bonebreak leaves the victim with a Broken Bone affliction")

# ===================== IRON BONES: shrug off the fracture =====================
cmd(smk, "quit!"); smk.close()
sql(f"UPDATE player_progress SET hp=9999 WHERE player_id="
    f"(SELECT id FROM player WHERE name='{mk}');")
smk = relog(mk, mkpw)
smb = relog(mb, mbpw)
ensure_pk_on(smk); ensure_pk_on(smb)
check(engage(smk, mb), "the Monk engages the iron-boned Monk")
time.sleep(1.0); recv_all(smk, 0.3)
out = act_while_recovering(smk, "bonebreak")
check("bones hold" in out.lower() or "refuse to break" in out.lower() or "iron" in out.lower(),
      f"iron bones shrugs off a bonebreak: {out[:90]!r}")

# Cleanup.
for s in (sig, smk, svc, smb):
    try:
        send_line(s, "quit!"); recv_all(s); s.close()
    except Exception:
        pass
sql(f"DELETE FROM player_skill WHERE player_id IN "
    f"(SELECT id FROM player WHERE name IN ('{mk}'));")
sql(f"DELETE FROM player_progress WHERE player_id IN "
    f"(SELECT id FROM player WHERE name IN ('{ig}','{mk}','{vc}','{mb}'));")
sql(f"DELETE FROM player WHERE name IN ('{ig}','{mk}','{vc}','{mb}');")
sql(f"DELETE FROM room WHERE vnum={ROOM};")

announce_done("smoke_test_monk_iron", host, port)
print("=== ALL CHECKS PASSED ===")
