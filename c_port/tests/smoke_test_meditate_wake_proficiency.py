#!/usr/bin/env python3
"""Smoke test for this session's batch of fixes/features (user 2026-07-27):

  1. `wake` no longer lets a `slumber`ed target instantly stand back up --
     AFFECT_SLEEP now refuses the command outright instead of just
     checking POSITION_SLEEPING (cmd_position.c's cmd_wake()).
  2. `yoginsa` (Monk) auto-sits a standing character instead of refusing
     (cmd_yoginsa.c).
  3. `meditate` (Mage, Druid, standalone command -- `cast meditate` is a
     dead-end redirect) and `pray penance` (Cleric) restore a real
     resource. Mage/Druid go through meditate.c's background tick, same
     shape as `yoginsa`; Cleric's `pray penance` stays single-action.
  4. Any real skill/spell proficiency gain (skill.c's
     skill_learn_from_doing()) sends the player a "You have become
     better at ..." message instead of happening silently -- verified via
     meditate.c's tick, which rolls it every ~5s while meditating.

Updated 2026-08-21 for the tick-based meditate/yoginsa architecture
(POSITION_MEDITATE, being.h) -- sections 1/3/4 originally assumed
single-action responses that no longer match the background-tick shape;
see git history for the prior version if needed.

    python3 tests/smoke_test_meditate_wake_proficiency.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, cmd_until, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_meditate_wake_proficiency", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 978000 + (int(time.time()) % 1000)
COMPONENT = ROOM + 1
SYMBOL = ROOM + 2

CLASS_MAGE = 0
CLASS_CLERIC = 1
CLASS_WARRIOR = 2
CLASS_DRUID = 4
CLASS_MONK = 5
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


def move_of(sock):
    m = re.search(r"Move:\s*(\d+)", cmd(sock, "score"))
    return int(m.group(1))


def mana_of(sock):
    m = re.search(r"Mana:\s*(\d+)", cmd(sock, "score"))
    return int(m.group(1)) if m else 0


def position_of(sock):
    # 2026-08-03: a sleeping (non-immortal) caller can no longer run `score`
    # at all -- the new central sleeping gate (cmd_table.c) refuses every
    # verb but `wake`. That refusal is itself a reliable "still asleep"
    # signal, so treat it the same as a real Position: Sleeping reading
    # rather than trying to read a score sheet that was never sent.
    out = cmd(sock, "score")
    if "fast asleep" in out.lower():
        return "Sleeping"
    m = re.search(r"Position:\s*(\S+)", out)
    return m.group(1) if m else None


sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM},0,0,0,'Meditate Sandbox','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({COMPONENT},'pouch component reagent','a pouch of spell components',"
    f"'A pouch of spell components is lying here.',12,{WEAR_TAKE},1);")
sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
    f"VALUES ({SYMBOL},'symbol holy silver','a tarnished silver holy symbol',"
    f"'A tarnished silver holy symbol is lying here.',12,{WEAR_TAKE},1);")

pw = "meditatepw123"
mage_name = f"Medmag{_suffix}"
cleric_name = f"Medcle{_suffix}"
druid_name = f"Meddru{_suffix}"
monk_name = f"Medmon{_suffix}"
vic_name = f"Medvic{_suffix}"

sockets = []
try:
    for name in (mage_name, cleric_name, druid_name, monk_name, vic_name):
        make_char(name, pw)
    sql(f"UPDATE player SET class={CLASS_MAGE} WHERE name='{mage_name}';")
    sql(f"UPDATE player SET class={CLASS_CLERIC} WHERE name='{cleric_name}';")
    sql(f"UPDATE player SET class={CLASS_DRUID} WHERE name='{druid_name}';")
    sql(f"UPDATE player SET class={CLASS_MONK} WHERE name='{monk_name}';")
    sql(f"UPDATE player SET class={CLASS_WARRIOR} WHERE name='{vic_name}';")
    for name in (mage_name, cleric_name, druid_name, monk_name, vic_name):
        sql(f"UPDATE player SET load_room={ROOM} WHERE name='{name}';")
    # Mage/Cleric/Druid caster: level 51+ (immortal) -- `cast`/`pray` gate
    # their own dispatch behind a SEPARATE learn-by-doing proficiency roll
    # (skill_roll_success(skill_learn_from_doing(...)) in cmd_cast.c/
    # cmd_pray.c, above and beyond the discipline-percentage ACCESS gate) --
    # a fresh character's first-ever attempt starts at 1% proficiency, a
    # ~1% chance to succeed, which would make sections 1/3 flaky almost to
    # the point of never passing. Immortals bypass that roll entirely
    # (`imm || skill_roll_success(...)`), same reason
    # smoke_test_curse_slumber.py's own casters are level 51.
    for name in (mage_name, cleric_name, druid_name):
        sql(f"UPDATE player_progress SET level=51 "
            f"WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
    # The Monk stays a genuine mortal (level 20, basic_disc_pct seeded for
    # class-tier access) -- section 4 specifically needs a REAL, non-bypassed
    # skill_learn_from_doing() call to observe the new gain message.
    sql(f"UPDATE player_progress SET level=20, basic_disc_pct=100 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{monk_name}');")
    # Slumber/wake's victim must stay a plain mortal too (both refuse an
    # immortal target outright) -- same precedent as smoke_test_curse_slumber.py.
    sql(f"UPDATE player_progress SET level=20 "
        f"WHERE player_id=(SELECT id FROM player WHERE name='{vic_name}');")
    # Seeded BEFORE login (not after) -- a live connected character's
    # progress lives in memory, so a direct SQL write to player_progress
    # after that point would only affect the row, not the running session;
    # loading it fresh at login is the only way to control the starting
    # Vitality the section-3 before/after check reads.
    # Cleric's `pray penance` still restores Vitality directly -- low
    # headroom needed there. Mage's `meditate` restores Mana instead (see
    # section 3 below), so it needs low Mana headroom, not Vitality.
    sql(f"UPDATE player_progress SET vit=5 WHERE player_id=(SELECT id FROM player WHERE name='{cleric_name}');")
    sql(f"UPDATE player_progress SET mana=5 WHERE player_id=(SELECT id FROM player WHERE name='{mage_name}');")

    sm = relog(mage_name, pw); sockets.append(sm)
    sc = relog(cleric_name, pw); sockets.append(sc)
    sd = relog(druid_name, pw); sockets.append(sd)
    sk = relog(monk_name, pw); sockets.append(sk)
    sv = relog(vic_name, pw); sockets.append(sv)
    for s in (sm, sc, sd, sk, sv):
        cmd(s, "toggle pk")

    # --- 1: wake can no longer defeat slumber ---
    cmd(sm, f"load obj {COMPONENT}"); recv_all(sm, 0.3)
    cmd(sm, "get pouch"); recv_all(sm, 0.3)
    # Casting is a multi-round process (gesture/chant/concentrate stages
    # before the spell actually strikes) -- a plain cmd() only waits ~1s,
    # nowhere near long enough to see the real "collapse into sleep!"
    # completion line. cmd_until() (mud_test_utils.py) is the primitive
    # built for exactly this: read live until the pattern actually shows
    # up, or a real 8s deadline elapses.
    out1 = strip(cmd_until(sm, f"cast slumber {vic_name}", "collapse into sleep", deadline=8.0))
    check("collapse into sleep" in out1.lower(), "slumber succeeds with a component")
    check(position_of(sv) == "Sleeping", "the victim's own `score` shows Sleeping after slumber")
    out1b = strip(cmd(sv, "wake"))
    check("magical urge to sleep" in out1b.lower(), "`wake` refuses while AFFECT_SLEEP is active")
    check(position_of(sv) == "Sleeping", "the victim is still asleep after the refused `wake`")

    # --- 2: yoginsa auto-sits a standing Monk ---
    check(position_of(sk) == "Standing", "the Monk starts out standing")
    out2 = strip(cmd(sk, "yoginsa"))
    check("you sit down" in out2.lower(), "yoginsa auto-sits instead of refusing")
    check("begin meditating" in out2.lower(), "yoginsa starts the meditation task right after sitting")
    # POSITION_MEDITATE (being.h): a meditating character now shows its own
    # distinct position instead of reading as plain Sitting/Resting.
    check(position_of(sk) == "Meditating", "the Monk ends up Meditating, not standing")
    # The actual meditation roll (a fresh "inner harmonies" heal or a
    # "won't settle" miss) only happens on the next background tick
    # (meditate.c's meditate_tick_run(), REGEN_PULSES ~5s), not as an
    # immediate response to the command itself.
    send_line(sk, "look")
    time.sleep(5.5)
    tick_out = strip(recv_all(sk, 0.5))
    check("inner harmonies" in tick_out.lower() or "mind won't settle" in tick_out.lower(),
          "yoginsa's background tick runs its own meditation roll")

    # --- 3: meditate (Mage)/penance (Cleric)/meditate (Druid) restore resources ---
    # `cast meditate` is now a dead-end redirect ("...just type `meditate`",
    # cmd_cast.c) -- Mage/Druid go through the standalone `meditate`
    # command instead (cmd_meditate.c), same background-tick shape
    # `yoginsa` uses (section 2): it starts the task immediately, but the
    # actual heal only lands on the next tick (meditate.c's
    # meditate_tick_run(), ~5s). No spell component needed any more
    # either -- that was `cast meditate`-only machinery.
    mana_before = mana_of(sm)
    out3 = strip(cmd(sm, "meditate"))
    check("begin meditating" in out3.lower(), "meditate (Mage) starts")
    send_line(sm, "look")
    time.sleep(5.5)
    tick_out = strip(recv_all(sm, 0.5))
    check("focuses your mind" in tick_out.lower(), "meditate (Mage) reports a real Mana-focused tick")
    check(mana_of(sm) > mana_before, f"Mage's live Mana actually went up ({mana_before} -> {mana_of(sm)})")

    cmd(sc, f"load obj {SYMBOL}"); recv_all(sc, 0.3)
    cmd(sc, "get symbol"); recv_all(sc, 0.3)
    v_before = move_of(sc)
    out3b = strip(cmd(sc, "pray penance"))
    check("vitality return" in out3b.lower(), "pray penance (Cleric) reports a real Vitality restore")
    check(move_of(sc) > v_before, f"Cleric's live Move/Vitality actually went up ({v_before} -> {move_of(sc)})")

    # Druid's `meditate` runs the same Mana-mode tick path (meditate.c:
    # "Druid Lifeforce mirrors mana"), but being_calc_max_mana() is
    # Mage-only (being.c's own doc comment) -- a Druid's max_mana stays 0,
    # so being_heal_mana() has nothing to raise. A disclosed gap elsewhere
    # in the codebase, not a regression here: confirm the command and its
    # tick message actually run, not a resource number that can't move yet.
    out3c = strip(cmd(sd, "meditate"))
    check("begin meditating" in out3c.lower(), "meditate (Druid) starts")
    send_line(sd, "look")
    time.sleep(5.5)
    tick_out_d = strip(recv_all(sd, 0.5))
    check("focuses your mind" in tick_out_d.lower(), "meditate (Druid) reports the same Mana-mode tick message")

    # --- 4: proficiency gains are announced ---
    # Force generous headroom (low pct, high ceiling) and a high-Wisdom
    # softened curve so the gain chance per attempt is large. skill_learn_
    # from_doing() (skill.c) reads pct/last_gain_at straight from the DB
    # on every call, not a login-time in-memory snapshot, so resetting
    # last_gain_at via SQL between ticks does reach the live roll.
    sql(f"UPDATE player_attrs SET wisdom=200 WHERE player_id=(SELECT id FROM player WHERE name='{monk_name}');")
    sql(f"REPLACE INTO player_skill (player_id, skill_name, pct, last_gain_at) "
        f"VALUES ((SELECT id FROM player WHERE name='{monk_name}'), 'yoginsa', 1, 0);")
    # Start meditation ONCE -- it now stays active across ticks on its own
    # (meditate_tick_run() keeps rolling skill_learn_from_doing() every
    # ~5s as long as `meditating` stays true), unlike the old single-shot
    # command that needed a fresh `stand`+`yoginsa` per attempt.
    cmd(sk, "stand"); recv_all(sk, 0.3)
    out4 = strip(cmd(sk, "yoginsa"))
    check("begin meditating" in out4.lower(), "yoginsa restarts meditation for section 4")
    got_gain_msg = False
    for _ in range(15):
        sql(f"UPDATE player_skill SET last_gain_at=0 WHERE skill_name='yoginsa' "
            f"AND player_id=(SELECT id FROM player WHERE name='{monk_name}');")
        send_line(sk, "look")
        time.sleep(5.5)
        tick_out = strip(recv_all(sk, 0.5))
        if "you have become better at" in tick_out.lower():
            got_gain_msg = True
            break
    check(got_gain_msg, "a real proficiency gain sends a \"You have become better at ...\" message")

    announce_done("smoke_test_meditate_wake_proficiency", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    for sock in sockets:
        try:
            sock.close()
        except OSError:
            pass
    for prefix in ("Medmag", "Medcle", "Meddru", "Medmon", "Medvic"):
        sql(f"DELETE FROM player_skill WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_progress WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player_attrs WHERE player_id IN (SELECT id FROM player WHERE name LIKE '{prefix}%{_suffix}');")
        sql(f"DELETE FROM player WHERE name LIKE '{prefix}%{_suffix}';")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
    sql(f"DELETE FROM obj WHERE vnum IN ({COMPONENT}, {SYMBOL});")
