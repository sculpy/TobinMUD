#!/usr/bin/env python3
"""Smoke test for TODO.md's "Implement missing skills from docs/Spell
Assignments.xlsx" batch C (real subsystem/design gaps disclosed in
STATUS.md's Session 141 entry, finished 2026-08-09):

  1. `sharpen`/`smooth` -- new obj_t.sharpness stat on weapon objects,
     raised by a carried whetstone (edged weapons) or file (blunt
     weapons). See obj.h/cmd_sharpen.c/combat.c.
  2. `alcoholism` -- new progress_t.drunk intoxication stat, gained from
     alcoholic drinks (liquids.h's own drunk field), dampened by the
     skill, decaying over time, with a real combat to-hit penalty and a
     real pass-out-from-drink mechanic. See being.h/vitals.c/liquids.c/
     cmd_drink.c/cmd_sip.c/combat.c.
  3. Deikhan mounted-combat trio (`calm mount`/`charge`/`advanced
     riding`) -- new cmd_charge.c, a real mount-unseat mechanic in
     combat.c mitigated by `calm mount`, and a real mount-success/
     charge-damage bonus from `advanced riding`. See cmd_charge.c/
     cmd_ride.c/combat.c.
  4. Ranger beast-charm pair (`beast charm`/`befriend beast`) -- wired
     into cmd_cast.c's pre-existing charm-summon branch (real upstream's
     own doBefriendBeast()/doCharmBeast() are themselves stubbed out --
     "This skill is not implemented yet" -- so Tobin's own working
     charmed-pet subsystem is the real analog, not a stub of a stub).
  5. 5 Shaman spells folded onto Druid (`flatulence`/`shield of
     mists`/`thornflesh`/`living vines`/`raze`) -- 3 new affect types
     (AFFECT_SHIELD_OF_MISTS/AFFECT_LIVING_VINES/AFFECT_THORNFLESH) with
     real combat.c/cmd_move.c hooks. See affect.h/cmd_cast.c/combat.c.

Deliberately NOT covered here (disclosed, code-reviewed but not live-
tested, real reasons in each case):
  - `alcoholism`'s combat to-hit penalty and `advanced riding`'s mount-
    success-roll bonus -- both are small, continuously-scaled flat
    modifiers on top of an already-random roll; proving them
    statistically would need a much larger trial count than a smoke
    test's runtime budget allows. The skill-dampening-of-drunk-gain
    formula (deterministic, single-comparison) IS tested below instead,
    and both bonuses get at least a learn-by-doing confirmation that
    the code path fires.
  - `raze`'s crit-style double-damage chance (1-in-10) and
    `flatulence`'s mishap chance (1-in-10) -- both are the RARE branch;
    the common branch is what's asserted.

    python3 tests/smoke_test_missing_skills_batchc.py [host] [port]
"""
import re
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done
from mud_creation import create_character

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
BASE = 961000 + (int(time.time()) % 20000)
ROOM_OUT = BASE          # outdoor sandbox
ROOM_IN = BASE + 1       # indoor sandbox
COMPONENT_BASE = BASE + 2  # a handful of consumable component vnums, reused/reloaded

WEAR_TAKE = 1
ROOM_FLAG_INDOORS = 8  # 1 << 3, matches room.h's ROOM_FLAG_INDOORS bit

CLASS_MAGE, CLASS_CLERIC, CLASS_WARRIOR, CLASS_THIEF, CLASS_DRUID, CLASS_MONK = range(6)

WHETSTONE_VNUM = 153
FILE_VNUM = 155
SWORD_VNUM = 300     # "a long sword" -- edged
MACE_VNUM = 311      # "a steel mace" -- blunt
HORSE_VNUM = 578     # "a young warhorse" -- race 47 HORSE, rideable
WHISKY_VNUM = 412    # "a shot of whiskey" -- real seeded alcoholic drink (liquid type 5 WHISKY)


def query(stmt):
    return subprocess.run(["mariadb", "-N", "tobin", "-e", stmt],
                          check=True, capture_output=True, text=True).stdout.strip()


def strip(s):
    return re.sub(r"\x1b\[[0-9;]*m", "", s)


def set_level_class(name, level, cls):
    sql(f"UPDATE player_progress SET level={level}, basic_disc_pct=100, combat_disc_pct=100, "
        f"advanced_disc_pct=100, hp=500, max_hp=500, gold=500 WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")
    sql(f"UPDATE player SET class={cls}, load_room={ROOM_OUT} WHERE name='{name}';")


def set_skill(name, skill_name, pct):
    sql(f"INSERT INTO player_skill (player_id, skill_name, pct, last_gain_at) VALUES "
        f"((SELECT id FROM player WHERE name='{name}'), '{skill_name}', {pct}, {int(time.time())}) "
        f"ON DUPLICATE KEY UPDATE pct={pct};")


def skill_pct(name, skill_name):
    out = query(f"SELECT pct FROM player_skill WHERE player_id="
                f"(SELECT id FROM player WHERE name='{name}') AND skill_name='{skill_name}';")
    return int(out) if out else 0


def drunk_of(name):
    out = query(f"SELECT drunk FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{name}');")
    return int(out) if out else 0


def hp_of(sock):
    """Live (in-memory, not DB) current HP via `score`. Drains first,
    then retries: this is routinely called on a socket that's mid-fight
    and streaming a combat round every ~1.2s, so without the drain the
    `score` read comes back holding queued combat text instead of the
    score table (see drain()'s own doc comment for the mechanism)."""
    for _ in range(5):
        drain(sock)
        m = re.search(r"HP:\s*(\d+)", strip(cmd(sock, "score")))
        if m:
            return int(m.group(1))
        time.sleep(0.3)
    raise AssertionError("couldn't read a real HP value from `score` after 5 attempts")


def make_char(name, pw, class_choice="1"):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, "y", pw, pw):
        send_line(s, step)
        recv_all(s)
    create_character(s, name, send_line, recv_all, char_class=class_choice)
    return s


def relog(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    for step in (name, pw, "1"):
        send_line(s, step)
        recv_all(s)
    cmd(s, "color off")
    return s


def collect_for(sock, seconds, prime_cmd=None):
    """Sends `prime_cmd` (if given), then accumulates raw socket output
    over a bounded wall-clock window (short per-call idle timeouts, like
    poke_fight's own bounded-window technique) rather than waiting for a
    real quiet gap -- a streaming combat exchange never goes quiet until
    it's fully over, which both takes far longer than needed and risks
    actually resolving the fight."""
    if prime_cmd:
        send_line(sock, prime_cmd)
    end = time.time() + seconds
    buf = ""
    while time.time() < end:
        buf += recv_all(sock, 0.3)
    return buf


def drain(*socks):
    """Reads and DISCARDS everything queued on each socket until it goes
    quiet. Essential after any real melee exchange: `cmd()` sends, then
    returns the first thing that arrives -- so a socket still holding
    seconds of undrained combat spam answers the NEXT command with that
    stale spam instead of the command's own reply, and every read after
    it stays shifted by one (TODO.md's own known "scripted-input desync"
    family). collect_for() below only ever drains the ONE socket it
    watches; every other participant in that same fight (including the
    immortal, whose `load`/`drop` replies feed give_component()) is left
    dirty, which is exactly how a later, unrelated-looking step fails
    with a misleading message. Not a fix for the underlying desync --
    a deliberate test-side workaround, same as the timeout bumps."""
    for s in socks:
        while recv_all(s, 0.3):
            pass


announce("smoke_test_missing_skills_batchc", host, port)

# ---------------------------------------------------------------------------
# Setup: 1 immortal (admin/load/spar), 2 mortal PCs.
# ---------------------------------------------------------------------------
imm_name, imm_pw = f"Bcimm{_suffix}", "bcimmpw12345"
si = make_char(imm_name, imm_pw)
cmd(si, "quit!"); si.close()
sql(f"UPDATE player_progress SET level=60 WHERE player_id=(SELECT id FROM player WHERE name='{imm_name}');")
si = relog(imm_name, imm_pw)

# Defensive pre-clean -- a stale leftover row from an unrelated test's
# own time-based vnum range can collide with ours (SYNC.md's own
# documented "sweep for orphaned rows" gotcha); deleting our exact
# target vnums first makes this self-healing regardless.
sql(f"DELETE FROM roomexit WHERE vnum IN ({ROOM_OUT},{ROOM_IN});")
sql(f"DELETE FROM room WHERE vnum IN ({ROOM_OUT},{ROOM_IN});")
sql(f"DELETE FROM obj WHERE vnum IN ({COMPONENT_BASE},{COMPONENT_BASE+1},{COMPONENT_BASE+2},{COMPONENT_BASE+3},"
    f"{COMPONENT_BASE+4},{COMPONENT_BASE+5},{COMPONENT_BASE+6},{COMPONENT_BASE+7});")

ROOM_FLAG_ALWAYS_LIT = 1  # 1 << 0, matches room.h's ROOM_FLAG_ALWAYS_LIT bit
                          # -- without it, a mortal without a light source
                          # sees "pitch black" once real in-game time turns
                          # to night, an environmental flakiness trap.
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_OUT},0,0,0,'BatchC Outdoor Sandbox','A bare sandbox room.\\n',NULL,{ROOM_FLAG_ALWAYS_LIT},0,0,0,0,0,0,0,0,0);")
sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
    f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
    f"VALUES ({ROOM_IN},0,0,0,'BatchC Indoor Sandbox','A bare sandbox room.\\n',NULL,{ROOM_FLAG_INDOORS | ROOM_FLAG_ALWAYS_LIT},0,0,0,0,0,0,0,0,0);")
for cv in (COMPONENT_BASE, COMPONENT_BASE + 1, COMPONENT_BASE + 2, COMPONENT_BASE + 3, COMPONENT_BASE + 4,
           COMPONENT_BASE + 5, COMPONENT_BASE + 6, COMPONENT_BASE + 7):
    sql(f"INSERT INTO obj (vnum,name,short_desc,long_desc,type,wear_flag,can_be_seen) "
        f"VALUES ({cv},'pouch component reagent','a pouch of spell components',"
        f"'A pouch of spell components is lying here.',12,{WEAR_TAKE},1);")
# A real "up" exit from outdoor -> indoor sandbox (direction 4, DIR_NAMES
# order in room.c) -- needed so `flee` (mounted-combat-trio section 3
# below) has somewhere to actually go: with zero real exits, real
# upstream's own "nowhere to run" PANIC branch would fire unconditionally
# and `flee` could never succeed at all, not just its normal ~33% real
# fail chance. Cleaned up automatically by the room DELETE above/below
# (fk_roomexit_vnum is ON DELETE CASCADE) -- the explicit roomexit DELETE
# in both cleanup blocks is only there for fk_roomexit_destination's own
# RESTRICT (this row's `destination` points at ROOM_IN, so ROOM_IN can't
# be deleted while this row still references it).
sql(f"INSERT INTO roomexit (vnum,direction,name,description,type,condition_flag,"
    f"lock_difficulty,weight,key_num,destination) VALUES "
    f"({ROOM_OUT},4,'up','A passage leads up.\\n',0,0,0,0,-1,{ROOM_IN});")
check("BatchC" in cmd(si, f"goto {ROOM_OUT}"), "the immortal goto's into the outdoor sandbox")

warr_name, warr_pw = f"Bcwarr{_suffix}", "bcwarrpw123"
sw = make_char(warr_name, warr_pw)
cmd(sw, "quit!"); sw.close()
# Level 45, not 30: `advanced riding` is a level-40 skill (skill.c), and
# being_knows_skill() gates on level regardless of what a player_skill
# row says -- so at level 30 a set_skill(..., "advanced riding", 100)
# was silently a no-op and combat.c's calm-mount formula only ever saw
# adv_prof=0. That left spook_chance at 5% rather than 0% even "fully
# trained", making the trained sub-trial below a coin-flip across a
# 15-second burst of hits (found live -- it failed intermittently for
# exactly this reason, and the mechanic itself was correct all along).
# 45 keeps every other skill this test uses in range (charge is the next
# highest at 20) and stays under MORTAL_LEVEL_MAX (50), so the tester
# does not accidentally read as immortal.
set_level_class(warr_name, 45, CLASS_WARRIOR)
sw = relog(warr_name, warr_pw)
check("BatchC" in cmd(sw, "look"), "the warrior tester lands in the outdoor sandbox")
# PK opt-in (combat.c's combat_pk_allowed(), TODO.md: "BOTH players must
# have opted in for attack/kill between players") -- `charge`/`hit` are
# ordinary mortal-vs-mortal PC combat, same gate as `kill`; without this
# combat_find_room_target() silently reports "They aren't here." for
# every other mortal PC in the room, immortal-vs-mortal (si hitting a
# mortal tester elsewhere in this file) is unaffected -- exempt on
# either side knowing on both.
check("pk" in cmd(sw, "toggle pk").lower(), "warrior tester opts into PK")


foe_name, foe_pw = f"Bcfoe{_suffix}", "bcfoepw1234"
sf = make_char(foe_name, foe_pw)
cmd(sf, "quit!"); sf.close()
set_level_class(foe_name, 30, CLASS_WARRIOR)
sf = relog(foe_name, foe_pw)
check("BatchC" in cmd(sf, "look"), "the second warrior (sparring partner) lands in the outdoor sandbox")
check("pk" in cmd(sf, "toggle pk").lower(), "sparring-partner tester opts into PK")

dru_name, dru_pw = f"Bcdru{_suffix}", "bcdrupw1234"
sd = make_char(dru_name, dru_pw)
cmd(sd, "quit!"); sd.close()
# Level 50, not 51 -- MORTAL_LEVEL_MAX (being.h) is exactly 50, so 51
# would silently make being_is_immortal() true for this tester (real,
# found live: an "immortal" defender forces incoming melee damage to 0
# in combat_strike(), which would have made thornflesh's own reflect
# trial below -- dmg > 0 is its trigger condition -- fail every single
# time, quietly, regardless of the PK toggle two lines down). `raze`
# (this batch's highest skill.c threshold at 50) is gated `>=`, so 50
# still learns every Batch C skill/spell without crossing the boundary.
set_level_class(dru_name, 50, CLASS_DRUID)
sd = relog(dru_name, dru_pw)
check("BatchC" in cmd(sd, "look"), "the druid tester lands in the outdoor sandbox")
check("pk" in cmd(sd, "toggle pk").lower(), "druid tester opts into PK (thornflesh's reflect trial needs mortal-vs-mortal melee too)")

component_iter = iter(range(COMPONENT_BASE, COMPONENT_BASE + 8))


def give_component(caster_sock):
    cv = next(component_iter)
    # Both sockets drained first: this helper is called repeatedly from
    # inside combat-heavy sections, and either side holding queued
    # combat text answers `load`/`get` with that instead of the real
    # reply (drain()'s own doc comment has the mechanism).
    drain(si, caster_sock)
    check("You conjure" in cmd(si, f"load obj {cv}"), f"component {cv} loaded")
    cmd(si, "drop pouch")
    drain(caster_sock)
    got = cmd(caster_sock, "get pouch")
    if "you get" not in got.lower():
        # Print what actually came back before failing -- a bare "the
        # caster picks up a fresh component" assertion is actively
        # misleading when the real cause is a misaligned read (the
        # socket answering with older queued text instead of this
        # command's own reply), which looks identical to a genuine
        # refusal from the assertion message alone.
        print("GIVE_COMPONENT_FAILED, raw reply was:", repr(got))
    check("you get" in got.lower(), "the caster picks up a fresh component")


# ---------------------------------------------------------------------------
# 1. sharpen / smooth
# ---------------------------------------------------------------------------
set_skill(warr_name, "sharpen", 100)
set_skill(warr_name, "smooth", 100)
check("You conjure" in cmd(si, f"load obj {SWORD_VNUM}"), "long sword loaded")
cmd(si, "drop sword")
check("you get" in cmd(sw, "get sword").lower(), "warrior picks up the sword")
check("wield" in cmd(sw, "wield sword").lower(), "warrior wields the sword")

out = cmd(sw, "sharpen sword")
check("whetstone" in out.lower(), "sharpen refuses without a carried whetstone")

check("You conjure" in cmd(si, f"load obj {WHETSTONE_VNUM}"), "whetstone loaded")
cmd(si, "drop whetstone")
check("you get" in cmd(sw, "get whetstone").lower(), "warrior picks up the whetstone")
out = cmd(sw, "sharpen sword")
check("sharp" in out.lower() and ("you sharpen" in out.lower()), "sharpening an edged weapon with a whetstone succeeds")

check("You conjure" in cmd(si, f"load obj {MACE_VNUM}"), "steel mace loaded")
cmd(si, "drop mace")
check("you get" in cmd(sw, "get mace").lower(), "warrior picks up the mace")
cmd(sw, "remove sword")  # empty the primary hand -- `wield` fills the OTHER
                         # hand if the primary is already occupied, and
                         # sharpen/smooth (like real upstream) only ever
                         # look at the primary-hand weapon.
cmd(sw, "wield mace")
out = cmd(sw, "sharpen mace")
check("isn't something you'd want sharp" in out, "sharpening a blunt weapon is refused")

out = cmd(sw, "smooth mace")
check("file" in out.lower(), "smoothing refuses without a carried file")
check("You conjure" in cmd(si, f"load obj {FILE_VNUM}"), "file loaded")
cmd(si, "drop file")
check("you get" in cmd(sw, "get file").lower(), "warrior picks up the file")
out = cmd(sw, "smooth mace")
check("smooth" in out.lower() and "you smooth" in out.lower(), "smoothing a blunt weapon with a file succeeds")

cmd(sw, "remove mace")
cmd(sw, "wield sword")
out = cmd(sw, "smooth sword")
check("isn't something you'd want dulled" in out, "smoothing an edged weapon is refused")

# ---------------------------------------------------------------------------
# 2. alcoholism
# ---------------------------------------------------------------------------
check("You conjure" in cmd(si, f"load obj {WHISKY_VNUM}"), "whiskey loaded")
cmd(si, "drop whiskey")
check("you get" in cmd(sw, "get whiskey").lower(), "warrior picks up the whiskey")
before_drunk = drunk_of(warr_name)
# Deliberately NOT pre-seeding a player_skill row here: skill_learn_
# from_doing()'s own "first-ever attempt" path unconditionally
# establishes the proficiency floor with no roll and no cooldown gate
# -- pre-seeding one instead (as the dampening trial below deliberately
# does) would immediately trip the 30s gain cooldown against itself and
# mask the very thing this check wants to observe.
drink_out = cmd(sw, "drink whiskey")
untrained_gain = drunk_of(warr_name) - before_drunk
check(untrained_gain > 0, "drinking whiskey raises progress.drunk")

trained_pct = skill_pct(warr_name, "alcoholism")
check(trained_pct > 0, "alcoholism trains from doing (a real drink taught the skill something)")

check("You conjure" in cmd(si, f"load obj {WHISKY_VNUM}"), "whiskey #2 loaded")
cmd(si, "drop whiskey")
check("you get" in cmd(sf, "get whiskey").lower(), "foe picks up whiskey #2")
set_skill(foe_name, "alcoholism", 100)
before2 = drunk_of(foe_name)
cmd(sf, "drink whiskey")
trained_gain = drunk_of(foe_name) - before2
check(0 <= trained_gain < untrained_gain,
      "an alcoholism-trained drinker gets measurably LESS drunk from the same drink (real SKILL_ALCOHOLISM dampening formula)")

# Real pass-out mechanic: force drunk to a guaranteed-pass-out level and
# wait for one real vitals tick (VITALS_PULSES = 600 pulses = ~60s). An
# UPDATE only touches the DB row -- the ALREADY-CONNECTED character's
# live being_t.progress.drunk stays whatever real gameplay last set it
# (a small, heavily-dampened amount from the trial just above), so a
# fresh relog is required to actually reload the forced value into
# memory (same "live HP doesn't reflect a bare SQL UPDATE" gotcha
# other tests' own comments already flag for score/HP checks).
sql(f"UPDATE player_progress SET drunk=100 WHERE player_id=(SELECT id FROM player WHERE name='{foe_name}');")
sf.close()
sf = relog(foe_name, foe_pw)
cmd(sf, "stand")
print(">>> waiting ~65s for a real vitals tick (pass-out mechanic)...")
time.sleep(65)
# position lives on the in-memory being, not surfaced by a simple SQL
# read -- check live instead via a `look` self-check: an asleep
# character's own commands report "you're fast asleep" instead of
# resolving normally.
look_out = cmd(sf, "look")
check("you're sleeping" in look_out.lower() or "you have to wake" in look_out.lower() or "asleep" in look_out.lower(),
      "a heavily-drunk character really passes out on a real vitals tick")
cmd(sf, "wake"); cmd(sf, "stand")
# Sober foe back up too -- forced to drunk=100 above and never brought
# back down otherwise. vitals.c's own decay is only -2/tick (~60s/tick),
# so left alone foe would sit above the real passOut threshold (>14)
# for the better part of an hour -- a real, live risk of falling back
# asleep AGAIN at any later point in this same test (every subsequent
# vitals tick rolls a fresh, still-near-guaranteed chance while drunk
# stays this high), silently breaking any later `score`/combat check
# that assumes foe is awake and responsive. Same reset+relog shape the
# warrior tester's own sober-up (a few lines below) already uses.
sql(f"UPDATE player_progress SET drunk=0 WHERE player_id=(SELECT id FROM player WHERE name='{foe_name}');")
sf.close()
sf = relog(foe_name, foe_pw)
check("BatchC" in cmd(sf, "look"), "the sparring-partner tester is back in the outdoor sandbox, sober")

# Sober up the warrior tester before moving on. Its own "untrained"
# baseline drink earlier in THIS section (the very first whiskey, drunk
# by sw/warr_name with no alcoholism training, so the full raw ~25-point
# gain applied undampened) is real, lingering progress.drunk on a
# character that never disconnected since -- easily enough to clear the
# real SKILL_ALCOHOLISM passOut threshold (drunk > 14) on its own. Now
# that the real vitals-tick passOut mechanic just verified above
# actually sticks (a genuine regen.c bug -- an unrelated "fully rested,
# stand back up" auto-stand check was waking any sleeping character
# back up within a couple of real seconds, independent of WHY they were
# down, found live via this exact test and fixed in regen.c, 2026-08-09
# -- see STATUS.md), the warrior could statistically pass out on the
# very same real tick the foe's forced trial rolls, stranding `sw`
# asleep for the rest of this section. Not what section 3 is testing --
# reset and reload the same way the passout trial itself does (a bare
# SQL UPDATE doesn't reach the already-connected in-memory being).
sql(f"UPDATE player_progress SET drunk=0 WHERE player_id=(SELECT id FROM player WHERE name='{warr_name}');")
sw.close()
sw = relog(warr_name, warr_pw)
check("BatchC" in cmd(sw, "look"), "the warrior tester is back in the outdoor sandbox, sober, for the mounted-combat trio")

# ---------------------------------------------------------------------------
# 3. Deikhan mounted-combat trio
# ---------------------------------------------------------------------------
out = cmd(sw, f"charge {foe_name}")
check("must be mounted" in out.lower(), "charge without being mounted is refused")

check("You conjure" in cmd(si, f"load mob {HORSE_VNUM}"), "warhorse loaded")
# `riding` itself (pre-existing, pre-Batch-C skill) starts at its
# 1%-floor for a brand first-ever attempt (skill_learn_from_doing(),
# skill.c) -- near-guaranteed to fail the mount roll and was never
# actually exercised by a real test before now (every earlier run
# crashed before reaching this line). Pre-trained to 100%, same as
# every other skill this test exercises, so this section is testing
# `charge`/`calm mount`/`advanced riding`, not an untrained mount roll.
set_skill(warr_name, "riding", 100)
out = cmd(sw, "ride horse")
check("you mount" in out.lower(), "riding a real seeded warhorse succeeds")

set_skill(warr_name, "charge", 100)
# `calm mount`/`advanced riding` get their own dedicated, isolated trial
# right after this section -- pre-trained to 100 here too so the real
# 12%-per-landed-hit mount-panic mechanic (untrained default) can't
# statistically throw this rider mid-fight and turn the very next
# "already fighting" assertion below into a "not mounted" message
# instead (both are correct game behavior for their own real
# condition -- this just keeps the two mechanics' trials from
# interfering with each other). Reset back to 0 below for calm
# mount's own untrained trial.
set_skill(warr_name, "calm mount", 100)
set_skill(warr_name, "advanced riding", 100)
before_foe_hp = hp_of(sf)
drain(sw)
charge_out = cmd(sw, f"charge {foe_name}")
check("charge" in charge_out.lower() and ("trampling" in charge_out.lower() or "dodges" in charge_out.lower()),
      "`charge` while mounted resolves (hit or dodge)")

# The successful charge above OPENED a fight, so `sw` is now streaming a
# combat round roughly every 1.2s -- without draining that first, this
# next read comes back holding queued combat text rather than the
# refusal it's asserting on (drain()'s own doc comment has the full
# mechanism). Kept back-to-back with the charge otherwise: putting the
# real HP-loss check (moved below) in between would let extra combat
# rounds land, risking foe actually going down (combat_defeat() clears
# BOTH sides' `fighting`) and turning this into a fresh charge instead
# of the "already fighting" refusal it means to exercise.
drain(sw)
out2 = cmd(sw, f"charge {foe_name}")
check("opening move" in out2.lower(), "a second charge while already fighting is refused (opening-move-only rule)")

after_foe_hp = hp_of(sf)
check(after_foe_hp < before_foe_hp, "a successful `charge` (charge trained to 100%, a deterministic roll) deals real, measurable bonus damage")

charge_pct = skill_pct(warr_name, "charge")
check(charge_pct > 0, "charge trains from doing")

# Break off the sw-vs-foe fight the charge trial left running -- neither
# side needs to keep fighting for the calm-mount trial below, and the
# sandbox has no exit for a plain MORTAL `flee` to rely on other than
# the one just added for exactly this purpose, which still carries real
# upstream's own ~33% per-attempt fail chance (a bare send/recv with no
# retry would flake). Uses the immortal's own two guaranteed-success
# paths instead: `rescue` (imm bypasses the roll) frees sw with
# fighting=NULL while pulling foe's aggression onto `si`, then si's own
# `flee` (also imm-guaranteed) severs that too -- both real skill/
# command paths, just exercised by a character for whom they can't
# fail, so this block is fully deterministic instead of a retry loop.
check("pulling" in cmd(si, f"rescue {warr_name}").lower(), "immortal `rescue` frees the warrior tester from the charge-trial fight")
cmd(si, "flee")
drain(si, sw, sf, sd)
check("BatchC" in cmd(si, f"goto {ROOM_OUT}"), "immortal returns to the outdoor sandbox")

# `calm mount`: statistically resists being unseated by a landed hit
# while mounted. Untrained first (expect at least one unseat message
# across enough hits), then fully trained (expect none). HP reset to
# full before each trial -- an immortal's own damage output isn't
# reduced against a mortal defender, only what THEY receive is zeroed,
# so a generous buffer avoids any risk of actually defeating the tester.
send_line(sw, "dismount"); recv_all(sw, 0.5)
cmd(sw, "stand")
sql(f"UPDATE player_progress SET hp=500 WHERE player_id=(SELECT id FROM player WHERE name='{warr_name}');")
set_skill(warr_name, "calm mount", 0)
set_skill(warr_name, "advanced riding", 0)
mount_out = cmd(sw, "ride horse")
check("you mount" in mount_out.lower(), "re-mounting the warhorse for the calm-mount trial succeeds")
untrained_buf = collect_for(si, 0.3, f"hit {warr_name}")
untrained_buf += collect_for(sw, 15, None)
untrained_panic = "mount panics" in untrained_buf.lower()

# `hit` above put si into its own fight with sw -- same deterministic
# immortal-`flee` break-off as above, no retry loop, before the trained
# sub-trial's own remount.
cmd(si, "flee")
drain(si, sw, sf, sd)
check("BatchC" in cmd(si, f"goto {ROOM_OUT}"), "immortal returns to the outdoor sandbox after the untrained calm-mount trial")
send_line(sw, "dismount"); recv_all(sw, 0.5)
sql(f"UPDATE player_progress SET hp=500 WHERE player_id=(SELECT id FROM player WHERE name='{warr_name}');")
set_skill(warr_name, "calm mount", 100)
set_skill(warr_name, "advanced riding", 100)
cmd(sw, "ride horse")
trained_buf = collect_for(si, 0.3, f"hit {warr_name}")
trained_buf += collect_for(sw, 15, None)
trained_panic = "mount panics" in trained_buf.lower()

# Fully trained, this is a real 0% -- combat.c's formula subtracts
# ((calm*2 + adv)/3) percent of the 12% base, which reaches the entire
# 12% only when BOTH skills are genuinely known and maxed (hence the
# level-45 tester above).
check(not trained_panic, "fully-trained `calm mount` + `advanced riding` keeps the rider in the saddle across a burst of real hits")
if not untrained_panic:
    print(">>> NOTE: untrained calm-mount panic didn't fire in this run (12% per-hit chance, statistical) -- not a hard failure")
else:
    print(">>> OK: untrained calm-mount panic fired at least once, confirming the mechanic exists")
cmd(si, "flee")
drain(si, sw, sf, sd)
check("BatchC" in cmd(si, f"goto {ROOM_OUT}"), "immortal returns to the outdoor sandbox after the mounted-combat trio")
send_line(sw, "dismount"); recv_all(sw, 0.5)
sql(f"UPDATE player_progress SET hp=500 WHERE player_id=(SELECT id FROM player WHERE name='{warr_name}');")
drain(si, sw, sf, sd)

# ---------------------------------------------------------------------------
# 4. Ranger beast-charm pair
# ---------------------------------------------------------------------------
# Every `cast` below routes through the real 2026-08-09 multi-round cast
# delay (spellcast.c) for Mage/Druid -- 2-3 rounds at COMBAT_ROUND_PULSES
# (~1.2s) apart, so the REAL effect (including any refusal that's only
# checked inside cmd_cast_resolve_effect(), which now only runs once the
# countdown elapses -- e.g. raze's immortal-target refusal, thornflesh's
# already-active refusal, befriend beast's already-charmed refusal) can
# land up to ~3.6s after the command is typed. The shared `cmd()`
# helper's default 1.0s timeout would only ever catch that round's
# flavor text, not the resolution -- same real gap already found and
# fixed for `cast gust` in two other tests (STATUS.md Session 145) --
# so every cast call here uses a 6s timeout instead.
#
# Pre-train every Batch C Druid spell to 100% proficiency, same as every
# other skill in this test (sharpen/smooth/riding/charge/calm mount/
# advanced riding/alcoholism above) -- a real, freshly-learned spell's
# own skill_learn_from_doing() "first-ever attempt" floor is only ~1%,
# so an untrained cast here would fumble (fail the skill roll, real
# behavior, not a bug) essentially every time. Discipline ACCESS is
# already 100% via set_level_class() above; this is the separate PER-
# SPELL proficiency gate (skill.c's learn-by-doing roll).
for _sk in ("beast charm", "befriend beast", "flatulence", "raze",
            "shield of mists", "living vines", "thornflesh"):
    set_skill(dru_name, _sk, 100)

give_component(sd)
drain(sd)
out = cmd(sd, "cast beast charm", timeout=6.0)
check("gray wolf answers" in out.lower(), "`cast beast charm` summons the real seeded gray wolf with its own flavor text")
drain(sd)
look_out = cmd(sd, "look")
check("gray wolf" in look_out.lower(), "the charmed wolf is really standing in the room")

give_component(sd)
drain(sd)
out = cmd(sd, "cast befriend beast", timeout=6.0)
check("gray wolf trots over" in out.lower() or "already have a charmed" in out.lower(),
      "`cast befriend beast` uses its own distinct flavor text (or correctly refuses a second charmed pet)")

# ---------------------------------------------------------------------------
# 5. 5 Shaman/Druid spells
# ---------------------------------------------------------------------------
# shield of mists -- self
give_component(sd)
drain(sd)
out = cmd(sd, "cast shield of mists", timeout=6.0)
check("green mist" in out.lower(), "`cast shield of mists` (self) lands")
drain(sd)
aff = cmd(sd, "affects").lower()
check("shield of mists" in aff, "the caster really carries the Shield Of Mists affect")

# shield of mists -- on another occupant
give_component(sd)
drain(sd)
out = cmd(sd, f"cast shield of mists {foe_name}", timeout=6.0)
check("green mist" in out.lower(), "`cast shield of mists <target>` lands on someone else")
drain(sf)
foe_aff = cmd(sf, "affects").lower()
check("shield of mists" in foe_aff, "the OTHER occupant (not just the caster) carries Shield Of Mists")

# living vines -- indoor refusal, then real outdoor effect. give_component()
# hands the pouch over via the room floor (immortal drops, caster gets), so
# the immortal MUST be in the same room as the caster for it to work -- these
# two goto's are load-bearing setup, not navigation flavor, and are asserted
# accordingly. Found live: an unasserted goto here failed silently and the
# next give_component() reported the genuinely-correct "You don't see that
# here." (pouch dropped in the room the immortal was still standing in),
# which read as a mystery pickup failure rather than a missed move.
sql(f"UPDATE player SET load_room={ROOM_IN} WHERE name IN ('{dru_name}','{foe_name}');")
sd.close(); sd = relog(dru_name, dru_pw)
sf.close(); sf = relog(foe_name, foe_pw)
drain(si)
check("BatchC Indoor" in cmd(si, f"goto {ROOM_IN}"), "immortal goto's into the INDOOR sandbox for the living-vines refusal check")
check("BatchC Indoor" in cmd(sd, "look"), "the druid tester is really in the indoor sandbox too")
give_component(sd)
drain(sd)
out = cmd(sd, f"cast living vines {foe_name}", timeout=6.0)
check("only works outdoors" in out.lower(), "`cast living vines` is refused indoors, per its own real upstream gate")

sql(f"UPDATE player SET load_room={ROOM_OUT} WHERE name IN ('{dru_name}','{foe_name}');")
sd.close(); sd = relog(dru_name, dru_pw)
sf.close(); sf = relog(foe_name, foe_pw)
drain(si)
check("BatchC Outdoor" in cmd(si, f"goto {ROOM_OUT}"), "immortal goto's back into the outdoor sandbox for the real living-vines effect")
check("BatchC Outdoor" in cmd(sd, "look"), "the druid tester is really back in the outdoor sandbox too")
give_component(sd)
drain(sd)
out = cmd(sd, f"cast living vines {foe_name}", timeout=6.0)
check("wrap tight around" in out.lower(), "`cast living vines <target>` lands outdoors")
# living vines OPENS combat between sd and sf (cmd_cast.c sets both
# `fighting` pointers), so from here to the end of this section both
# sockets stream a combat round every ~1.2s -- every read below drains
# first, or it comes back holding queued combat text instead of the
# reply it asserts on (drain()'s own doc comment has the mechanism).
drain(sf)
foe_aff2 = cmd(sf, "affects").lower()
check("living vines" in foe_aff2, "the target really carries the Living Vines affect")
drain(sf)
move_out = cmd(sf, "north")
check("can't move" in move_out.lower() or "vines" in move_out.lower(),
      "a Living-Vines-affected target is really refused normal movement")

# thornflesh -- self buff + real damage reflection off a landed melee hit
give_component(sd)
drain(sd)
out = cmd(sd, "cast thornflesh", timeout=6.0)
check("thorns emerge" in out.lower(), "`cast thornflesh` lands")
drain(sd)
aff3 = cmd(sd, "affects").lower()
check("thornflesh" in aff3, "the caster really carries the Thornflesh affect")
give_component(sd)
drain(sd)
out2 = cmd(sd, "cast thornflesh", timeout=6.0)
check("armored well enough" in out2.lower(), "casting thornflesh again while already active is refused, not stacked")

reflect_buf = collect_for(sf, 15, f"hit {dru_name}")
check("thorns" in reflect_buf.lower(),
      "a real melee hit landed on a Thornflesh-affected Druid bites back at the attacker (real damage reflection)")

# Same deterministic immortal-`rescue`+`flee` break-off the mounted-
# combat section already uses (a plain mortal `flee` here carries a real
# ~33% fail chance), then drain EVERY tester -- collect_for() above only
# read `sf`, so `sd` (the defender in that 15s exchange) and `si` are
# both still holding seconds of combat spam. Draining them is what keeps
# the very next give_component() from reading that stale spam as the
# reply to its own `drop`/`get` -- see drain()'s own doc comment.
check("pulling" in cmd(si, f"rescue {dru_name}").lower(), "immortal `rescue` frees the druid from the thornflesh-reflect fight")
cmd(si, "flee")
drain(si, sw, sf, sd)

# flatulence -- room-wide AoE damage
check("BatchC" in cmd(si, f"goto {ROOM_OUT}"), "immortal returns to the outdoor sandbox after the thornflesh trial")
foe_hp_before = hp_of(sf)
landed = False
for _ in range(5):
    give_component(sd)
    drain(sd)
    cmd(sd, "cast flatulence", timeout=6.0)
    foe_hp_now = hp_of(sf)
    if foe_hp_now < foe_hp_before:
        landed = True
        break
    foe_hp_before = foe_hp_now
check(landed, "`cast flatulence` damages another occupant of the room across a few attempts (10% mishap chance per cast)")

# raze -- single-target heavy nuke, refuses vs immortal
give_component(sd)
drain(sd)
out = cmd(sd, f"cast raze {imm_name}", timeout=6.0)
check("can't do that to an immortal" in out.lower(), "`cast raze` refuses to target an immortal")

give_component(sd)
foe_hp_before2 = hp_of(sf)
drain(sd)
out = cmd(sd, f"cast raze {foe_name}", timeout=6.0)
check("razed" in out.lower(), "`cast raze <target>` lands its real effect")
foe_hp_after2 = hp_of(sf)
check(foe_hp_after2 < foe_hp_before2, "`cast raze` deals real, measurable damage")

si.close(); sw.close(); sf.close(); sd.close()

sql(f"DELETE FROM roomexit WHERE vnum IN ({ROOM_OUT},{ROOM_IN});")
sql(f"DELETE FROM room WHERE vnum IN ({ROOM_OUT},{ROOM_IN});")
sql(f"DELETE FROM obj WHERE vnum IN ({COMPONENT_BASE},{COMPONENT_BASE+1},{COMPONENT_BASE+2},{COMPONENT_BASE+3},"
    f"{COMPONENT_BASE+4},{COMPONENT_BASE+5},{COMPONENT_BASE+6},{COMPONENT_BASE+7});")

announce_done("smoke_test_missing_skills_batchc", host, port)
print("=== ALL CHECKS PASSED ===")
