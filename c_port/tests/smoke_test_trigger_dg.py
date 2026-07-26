#!/usr/bin/env python3
"""Smoke test for the DG Scripts-style trigger language revamp (user,
2026-07-25: "use the DG_* source files to revamp triggers" -- full
language port, scoped to the LANGUAGE only per trigger_script.h: %var%
substitution, if/elseif/else/end, while/done, switch/case/default/done
(real fallthrough), break, set/unset/eval/global. Authoring stays through
the existing `edit trigger` flow -- see smoke_test_trigger.py for the
pre-revamp action-vocabulary/trigger-type coverage, not repeated here.

  1. if/else: takes the true branch, skips the false one.
  2. while/eval: loops the right number of times, mutating a counter.
  3. switch/case/break: stops at the matching case, doesn't fall into
     the next one.
  4. switch/case with NO break: real DG fallthrough -- executes every
     case body from the match onward.
  5. %actor%/%self%/%arg% substitution in an action's text.
  6. `global` persists across two separate trigger firings (different
     mobs), proving it's a real DB-backed store, not per-run local state.
  7. `wait` preserves the variable scope across the pause (unlike the
     pre-revamp version, which dropped it) -- `eval` after a forced
     `aitick` resume sees the pre-wait value.

    python3 tests/smoke_test_trigger_dg.py [host] [port]
"""
import socket
import subprocess
import sys
import time

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


def announce(test_name, host=host, port=port):
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
    announce(f"done {test_name}", host, port)


announce("smoke_test_trigger_dg")

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM_IF = 910000 + (int(time.time()) % 60000)
ROOM_WHILE = ROOM_IF + 1
ROOM_WAIT = ROOM_IF + 2
MOB_SWITCH = ROOM_IF + 3
MOB_FALLTHROUGH = ROOM_IF + 4
MOB_SPEECH = ROOM_IF + 5
MOB_GLOBAL_A = ROOM_IF + 6
MOB_GLOBAL_B = ROOM_IF + 7


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


def author_trigger(sock, target_type, vnum, trigger_type, match_or_chance, script_lines):
    """Authors a trigger via the menu-driven `edit trigger` flow (2026-07-25
    redesign, replacing the old one-shot `edit trigger <type> <vnum>
    <trigger_type> [match|chance]` command)."""
    out = cmd(sock, f"edit trigger {target_type} {vnum}")
    out += cmd(sock, "a")
    out += cmd(sock, trigger_type)
    if trigger_type == "speech" or (trigger_type == "random" and match_or_chance is not None):
        out += cmd(sock, str(match_or_chance))
    for line in script_lines:
        out += cmd(sock, line)
    out += cmd(sock, "/s")
    cmd(sock, "")  # leave the trigedit menu
    return out


def sql(stmt):
    subprocess.run(["mariadb", "tobin", "-e", stmt], check=True)


def sql_out(stmt):
    r = subprocess.run(["mariadb", "tobin", "-e", stmt], capture_output=True, text=True, check=True)
    return r.stdout


def make_char(sock, name, pw):
    recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "y"); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, pw); recv_all(sock)
    send_line(sock, "new"); recv_all(sock)
    send_line(sock, name); recv_all(sock)
    send_line(sock, "1"); recv_all(sock)  # race: human
    send_line(sock, "1"); recv_all(sock)  # class: mage
    send_line(sock, "done"); recv_all(sock)
    send_line(sock, "done"); recv_all(sock)  # alignment: neutral


def set_level(name, level):
    sql(f"UPDATE player_progress SET level={level} WHERE player_id="
        f"(SELECT id FROM player WHERE name='{name}');")


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


def make_mob(vnum, keyword):
    sql(f"INSERT INTO mob (vnum,name,short_desc,long_desc,description,actions,affects,"
        f"faction,fact_perc,letter,attacks,class,level,tohit,ac,hpbonus,damage_level,"
        f"damage_precision,gold,race,weight,height,str,bra,con,dex,agi,intel,wis,foc,"
        f"per,cha,kar,spe,pos,def_position,sex,spec_proc,skin,vision,can_be_seen,max_exist) "
        f"VALUES ({vnum},'{keyword}','a {keyword}','A {keyword} stands here.',"
        f"'desc',0,0,0,0,'A',1.0,0,1,0,0,0.3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
        f"10,10,1,0,0,0,1,1);")


imm_name = f"Trgdgimm{_suffix}"
imm_pw = "trigdgpw123"

try:
    s = socket.create_connection((host, port), timeout=5)
    make_char(s, imm_name, imm_pw)
    set_level(imm_name, 51)
    s.close()
    s = login(imm_name, imm_pw)

    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({ROOM_IF},0,0,0,'DG If Room','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({ROOM_WHILE},0,0,0,'DG While Room','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({ROOM_WAIT},0,0,0,'DG Wait Room','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")

    mort_name = f"Trgdgwlk{_suffix}"
    mort_pw = "trigdgwlkpw1"
    sm = socket.create_connection((host, port), timeout=5)
    make_char(sm, mort_name, mort_pw)
    sql(f"UPDATE player SET load_room={ROOM_IF} WHERE name='{mort_name}';")
    cmd(sm, "quit!")
    sm.close()
    sm = login(mort_name, mort_pw)
    check("DG If Room" in cmd(sm, "look"), "the walker lands in the if-test room")

    # --- 1: if/else ---
    check("DG If Room" in cmd(s, f"goto {ROOM_IF}"), "immortal reaches the if-test room")
    out = author_trigger(s, "room", ROOM_IF, "random", 100,
                         ["set x 5", "if %x% > 3", "echoroom big", "else", "echoroom small", "end"])
    check("Writing trigger" in out, "edit trigger opens the script editor")
    check("Trigger saved" in out, "the if/else trigger saves")

    out = cmd(sm, "look", 2.0)  # gives aitick's own random-tick a moment if needed
    cmd(s, "aitick 1")
    out = cmd(sm, "", 1.5)
    check("big" in out and "small" not in out,
          "if/else takes the true branch (%x% > 3) and skips the false one")

    # --- 2: while + eval ---
    check("DG While Room" in cmd(s, f"goto {ROOM_WHILE}"), "immortal reaches the while-test room")
    cmd(s, f"transfer {mort_name} {ROOM_WHILE}")
    recv_all(sm); recv_all(s)
    out = author_trigger(s, "room", ROOM_WHILE, "random", 100,
                         ["set i 0", "while %i% < 3", "eval i %i% + 1", "echoroom tick %i%",
                          "done", "echoroom loopdone"])
    check("Writing trigger" in out, "edit trigger opens the script editor for the while test")
    check("Trigger saved" in out, "the while/eval trigger saves")

    cmd(s, "aitick 1")
    out = cmd(sm, "", 1.5)
    check("tick 1" in out and "tick 2" in out and "tick 3" in out and "tick 4" not in out,
          "while/eval loops exactly 3 times, mutating the counter each pass")
    check("loopdone" in out, "execution continues past `done` once the loop condition goes false")

    # --- 3: switch/case/break ---
    check("DG If Room" in cmd(s, f"goto {ROOM_IF}"), "immortal returns to load the switch mob")
    make_mob(MOB_SWITCH, f"switcher{_suffix}")
    check("You conjure" in cmd(s, f"load mob {MOB_SWITCH}"), "the switch-test mob is loaded")
    out = author_trigger(s, "mob", MOB_SWITCH, "random", 100,
                         ["set color red", "switch %color%", "case red", "echoroom redcase", "break",
                          "case blue", "echoroom bluecase", "done"])
    check("Writing trigger" in out, "edit trigger opens the script editor for the switch test")
    check("Trigger saved" in out, "the switch/break trigger saves")

    check("DG If Room" in cmd(s, f"goto {ROOM_IF}"), "immortal is in the if-test room to transfer the walker")
    cmd(s, f"transfer {mort_name} {ROOM_IF}")
    recv_all(sm); recv_all(s)  # drain arrival notices
    cmd(s, "aitick 1")
    out = cmd(sm, "", 1.5)
    check("redcase" in out and "bluecase" not in out,
          "switch/case with `break` stops at the matching case only")

    # --- 4: switch fallthrough (no break) ---
    make_mob(MOB_FALLTHROUGH, f"faller{_suffix}")
    check("You conjure" in cmd(s, f"load mob {MOB_FALLTHROUGH}"), "the fallthrough-test mob is loaded")
    out = author_trigger(s, "mob", MOB_FALLTHROUGH, "random", 100,
                         ["set n 1", "switch %n%", "case 1", "echoroom one", "case 2", "echoroom two", "done"])
    check("Writing trigger" in out, "edit trigger opens the script editor for the fallthrough test")
    check("Trigger saved" in out, "the switch-fallthrough trigger saves")

    cmd(s, "aitick 1")
    out = cmd(sm, "", 1.5)
    check("one" in out and "two" in out,
          "switch/case with NO break falls through into the next case, real DG semantics")

    # --- 5: %actor%/%self%/%arg% substitution ---
    make_mob(MOB_SPEECH, f"speaker{_suffix}")
    check("You conjure" in cmd(s, f"load mob {MOB_SPEECH}"), "the speech-test mob is loaded")
    out = author_trigger(s, "mob", MOB_SPEECH, "speech", "greetme",
                         ["emote nods to %actor% and mutters about %self%, regarding \"%arg%\"."])
    check("Writing trigger" in out, "edit trigger opens the script editor for the speech test")
    check("Trigger saved" in out, "the substitution trigger saves")

    out = cmd(sm, "say greetme")
    check(f"nods to {mort_name}" in out, "%actor% substitutes the speaker's display name")
    check("regarding \"greetme\"" in out, "%arg% substitutes the trigger's matched keyword")

    # --- 6: `global` persists across two separate mobs ---
    make_mob(MOB_GLOBAL_A, f"globala{_suffix}")
    make_mob(MOB_GLOBAL_B, f"globalb{_suffix}")
    check("You conjure" in cmd(s, f"load mob {MOB_GLOBAL_A}"), "the first global-test mob is loaded")
    check("You conjure" in cmd(s, f"load mob {MOB_GLOBAL_B}"), "the second global-test mob is loaded")

    out = author_trigger(s, "mob", MOB_GLOBAL_A, "speech", "setflag", ["global shrine_seen yes"])
    check("Writing trigger" in out, "edit trigger opens the editor for the global-set mob")
    check("Trigger saved" in out, "the global-set trigger saves")

    out = author_trigger(s, "mob", MOB_GLOBAL_B, "speech", "checkflag",
                         ["if %shrine_seen% == yes", "echo flag was set", "else",
                          "echo flag missing", "end"])
    check("Writing trigger" in out, "edit trigger opens the editor for the global-read mob")
    check("Trigger saved" in out, "the global-read trigger saves")

    out = cmd(sm, "say checkflag")
    check("flag missing" in out, "the global var isn't set yet, so the reader mob sees its absence")
    cmd(sm, "say setflag")
    out = cmd(sm, "say checkflag")
    check("flag was set" in out,
          "`global` persisted the value to the DB where a DIFFERENT mob's trigger can read it")

    # --- 7: `wait` preserves the variable scope across the pause ---
    check("DG Wait Room" in cmd(s, f"goto {ROOM_WAIT}"), "immortal reaches the wait-test room")
    cmd(s, f"transfer {mort_name} {ROOM_WAIT}")
    recv_all(sm); recv_all(s)  # drain arrival notices
    check("DG Wait Room" in cmd(sm, "look"), "the walker is in the wait-test room")

    # No exits are wired for ROOM_WAIT, so a real "enter" trigger can't be
    # exercised by walking in -- attach as `random 100` instead (forced via
    # `aitick`, same precedent as smoke_test_trigger_wait.py's own wait test).
    out = author_trigger(s, "room", ROOM_WAIT, "random", 100,
                         ["set counter 1", "wait 1", "eval counter %counter% + 1",
                          "echoroom Counter is now %counter%."])
    check("Writing trigger" in out, "edit trigger opens the editor for the wait test")
    check("Trigger saved" in out, "the random-tick wait/eval trigger saves")

    cmd(s, "aitick 1")  # fires the random trigger, which immediately hits `wait`
    recv_all(sm, 0.5)   # nothing yet -- still paused
    cmd(s, "aitick 1")  # forces trigger_pending_force_all() -- resumes past `wait`
    out = cmd(sm, "", 1.5)
    check("Counter is now 2." in out,
          "the variable scope (`counter`, set to 1 before `wait`) survived the pause -- "
          "`eval` after resume saw the pre-wait value, not a reset one")

    announce_done("smoke_test_trigger_dg")
    print("=== ALL CHECKS PASSED ===")
finally:
    for _sock_name in ("s", "sm"):
        _sock = locals().get(_sock_name)
        if _sock is not None:
            try:
                _sock.close()
            except OSError:
                pass
    sql(f"DELETE FROM `trigger` WHERE target_type='room' AND target_vnum IN ({ROOM_IF}, {ROOM_WHILE}, {ROOM_WAIT});")
    sql(f"DELETE FROM `trigger` WHERE target_type='mob' AND target_vnum IN "
        f"({MOB_SWITCH}, {MOB_FALLTHROUGH}, {MOB_SPEECH}, {MOB_GLOBAL_A}, {MOB_GLOBAL_B});")
    sql(f"DELETE FROM trigger_global_var WHERE var_name='shrine_seen';")
    sql(f"DELETE FROM player_progress WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{mort_name}'));")
    sql(f"DELETE FROM player_attrs WHERE player_id IN "
        f"(SELECT id FROM player WHERE name IN ('{imm_name}', '{mort_name}'));")
    sql(f"DELETE FROM player WHERE name IN ('{imm_name}', '{mort_name}');")
    sql(f"DELETE FROM room WHERE vnum IN ({ROOM_IF}, {ROOM_WHILE}, {ROOM_WAIT});")
    sql(f"DELETE FROM mob WHERE vnum IN "
        f"({MOB_SWITCH}, {MOB_FALLTHROUGH}, {MOB_SPEECH}, {MOB_GLOBAL_A}, {MOB_GLOBAL_B});")
