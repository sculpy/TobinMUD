#!/usr/bin/env python3
"""Smoke test for the menu-driven `edit trigger` redesign (user,
2026-07-25: "edit trigger <room|mob|obj> vnum should go into a menu
driven editor where you choose type with an option to delete the trigger
inside the menu -- edit trigger list <vnum> should display all three
types"). Replaces the old one-shot `edit trigger <type> <vnum>
<trigger_type> [match|chance]` command entirely -- see
smoke_test_trigger.py/smoke_test_trigger_wait.py/smoke_test_trigger_seed.py/
smoke_test_trigger_dg.py for feature-behavior coverage (those exercise the
new menu just enough to author triggers, not the menu's own mechanics,
which is what this file covers).

  1. `edit trigger <room|mob|obj> <vnum>` opens the list menu, empty at
     first ("(none yet)").
  2. "A" prompts for a trigger type, validated against that target's own
     roster (e.g. "wear" is invalid for a room).
  3. A non-speech/random type (room "enter") goes straight to the script
     editor; saving returns to the list, now showing the new trigger.
  4. A "random" type prompts for chance percent first.
  5. A "speech" type (on a mob) prompts for a keyword first.
  6. Picking a list number opens that trigger's detail view (match text/
     chance/script/delete), which reads back the current script inline.
  7. Editing match text (option 1) and chance percent (option 2) commit
     immediately with an explicit save confirmation (2026-07-26, user:
     "no option in editor for saving").
  8. "3" re-opens the script editor with the CURRENT script preloaded and
     shown; saving updates that same row (not a duplicate).
  9. "D" + "yes" deletes the trigger and returns to the (now empty) list.
  10. `edit trigger list <vnum>` (no target type) shows triggers across
      room AND mob AND obj at that vnum, not just one.

  There is no `edit trigger delete <id>` quick form anymore (removed
  2026-07-26, user: "forget the use of id, use only vnums") -- deletion
  is menu-only now (step 9), and the raw db id is never shown to a
  builder anywhere in this flow.

    python3 tests/smoke_test_trigedit_menu.py [host] [port]
"""
import socket
import subprocess
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000


announce("smoke_test_trigedit_menu", host, port)

_suffix = "".join(chr(ord("a") + (int(time.time()) // 26**i) % 26) for i in range(4))
ROOM = 920000 + (int(time.time()) % 60000)
MOB = ROOM + 1


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
    send_line(sock, "1"); recv_all(sock)  # territory: urban
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


imm_name = f"Tmenuimm{_suffix}"
imm_pw = "tmenuimmpw12"

try:
    s = socket.create_connection((host, port), timeout=5)
    make_char(s, imm_name, imm_pw)
    set_level(imm_name, 51)
    s.close()
    s = login(imm_name, imm_pw)

    sql(f"INSERT INTO room (vnum,x,y,z,name,description,zone,room_flag,sector,"
        f"teletime,teletarg,telelook,river_speed,river_dir,capacity,height,spec) "
        f"VALUES ({ROOM},0,0,0,'Trigedit Menu Room','A bare sandbox room.\\n',NULL,1,0,0,0,0,0,0,0,0,0);")
    make_mob(MOB, f"menumob{_suffix}")

    # --- 1: menu opens empty ---
    check("Trigedit Menu Room" in cmd(s, f"goto {ROOM}"), "goto lands in the sandbox room")
    out = cmd(s, f"edit trigger room {ROOM}")
    check("Triggers on room" in out and "(none yet)" in out, "the menu opens empty for a fresh target")

    # --- 2: invalid type rejected ---
    cmd(s, "a")
    out = cmd(s, "wear")
    check("Not a valid type for room" in out, "a type invalid for this target type is rejected")

    # --- 3: non-speech/random type goes straight to the script editor ---
    cmd(s, "a")
    out = cmd(s, "enter")
    check("Writing trigger" in out, "a plain trigger type (enter) opens the script editor directly")
    cmd(s, "echoroom Menu-authored trigger fired.")
    out = cmd(s, "/s")
    check("Trigger saved" in out, "the script saves")
    check("Triggers on room" in out and "1) enter" in out,
          "saving returns to the list, now showing the new trigger")

    # --- 4: random type prompts for chance first ---
    cmd(s, "a")
    out = cmd(s, "random")
    check("percent chance" in out.lower(), "a random-type trigger prompts for chance percent first")
    cmd(s, "80")
    out = cmd(s, "echoroom ambient line")
    out = cmd(s, "/s")
    check("2) random" in out and "chance=80%" in out,
          "the random trigger's chance is shown in the list after saving")
    cmd(s, "")  # leave the room's trigedit menu

    # --- 5: speech type (mob) prompts for a keyword first ---
    check("You conjure" in cmd(s, f"load mob {MOB}"), "the menu-test mob is loaded")
    out = cmd(s, f"edit trigger mob {MOB}")
    check("(none yet)" in out, "the mob's trigger list starts empty")
    cmd(s, "a")
    out = cmd(s, "speech")
    check("keyword to match" in out.lower(), "a speech-type trigger prompts for a keyword first")
    cmd(s, "openmenu")
    cmd(s, "echo you said the keyword")
    out = cmd(s, "/s")
    check('match="openmenu"' in out, "the speech trigger's keyword is shown in the list after saving")

    # --- 6/7: detail view, match text and chance edits commit immediately ---
    out = cmd(s, "1")
    check("Editing speech trigger" in out and "Match text/keyword: openmenu" in out,
          "picking a list number opens that trigger's detail view")
    out = cmd(s, "1")
    check("Enter new match text" in out, "option 1 prompts for a new match text")
    out = cmd(s, "renamedkw")
    check("Match text/keyword: renamedkw" in out,
          "the match text updates immediately, no separate save step")

    row = sql_out(f"SELECT match_text FROM `trigger` WHERE target_type='mob' AND "
                  f"target_vnum={MOB} AND trigger_type='speech';").strip().splitlines()[-1]
    check(row.strip() == "renamedkw", "the match text edit actually persisted to the DB")

    # --- 8: re-editing the script updates the SAME row, not a duplicate ---
    out = cmd(s, "3")
    check("existing shown below" in out and "echo you said the keyword" in out,
          "re-opening the script shows the CURRENT content preloaded")
    cmd(s, "echo a completely different line")
    out = cmd(s, "/s")
    check("Trigger saved" in out, "the re-edited script saves")

    count = sql_out(f"SELECT COUNT(*) FROM `trigger` WHERE target_type='mob' AND "
                    f"target_vnum={MOB} AND trigger_type='speech';").strip().splitlines()[-1]
    check(count.strip() == "1", "editing the script updated the existing row, not a duplicate insert")
    script = sql_out(f"SELECT script FROM `trigger` WHERE target_type='mob' AND "
                     f"target_vnum={MOB} AND trigger_type='speech';").strip().splitlines()[-1]
    check("completely different line" in script, "the updated script text actually persisted")

    # --- 9: delete from inside the menu ---
    out = cmd(s, "1")
    check("Match text/keyword: renamedkw" in out, "back in the detail view")
    out = cmd(s, "d")
    check("Really delete trigger" in out, "D asks for confirmation")
    out = cmd(s, "yes")
    check("Trigger deleted" in out and "(none yet)" in out,
          "confirming deletes it and returns to the now-empty list")

    cmd(s, "")  # leave the menu

    # --- 10: `edit trigger list <vnum>` spans all three target types ---
    out = cmd(s, f"edit trigger list {ROOM}")
    check(f"room {ROOM} enter" in out and f"room {ROOM} random" in out,
          "list <vnum> (no target type) finds the room's own triggers")

    announce_done("smoke_test_trigedit_menu", host, port)
    print("=== ALL CHECKS PASSED ===")
finally:
    _sock = locals().get("s")
    if _sock is not None:
        try:
            _sock.close()
        except OSError:
            pass
    sql(f"DELETE FROM `trigger` WHERE target_type='room' AND target_vnum={ROOM};")
    sql(f"DELETE FROM `trigger` WHERE target_type='mob' AND target_vnum={MOB};")
    sql(f"DELETE FROM player_progress WHERE player_id IN "
        f"(SELECT id FROM player WHERE name='{imm_name}');")
    sql(f"DELETE FROM player_attrs WHERE player_id IN "
        f"(SELECT id FROM player WHERE name='{imm_name}');")
    sql(f"DELETE FROM player WHERE name='{imm_name}';")
    sql(f"DELETE FROM room WHERE vnum={ROOM};")
    sql(f"DELETE FROM mob WHERE vnum={MOB};")
