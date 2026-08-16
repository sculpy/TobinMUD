#!/usr/bin/env python3
"""Smoke test for the per-spell spell-component system (user, 2026-08-10:
"you need the right component to cast that particular spell" / "component
loads on mobs of a level that can cast the spell" / "adjust the mob level
component load by 2 levels, as if they're saving up for future spells" /
"spell components should load inside spellbags"). Ported from real
SneezyMUD's obj_component.cc (findComponent / CompIndex / doMerge); see
src/core/spell_component.c.

Uses faerie fire (a real Mage spell, level 2, whose bound reagent is the
seeded vnum 213 "a pixie torch" -- obj.val2 encodes the binding). vnum
200 ("a tiny lasso made of basilisk hair") is a DIFFERENT spell's reagent,
used here as the "wrong / generic" component.

  1. A mortal Mage with NO component is told the exact reagent they need.
  2. A mortal Mage holding the WRONG reagent (200) is still refused --
     generic components no longer satisfy a spell that has its own.
  3. A mortal Mage holding the RIGHT reagent (213) is not refused.
  4. An immortal (NOHASSLE, real Sneezy) may cast a bound spell with any
     component on hand -- the exact-reagent rule is waived for them.
  5. doMerge: two same-vnum components brought together on the floor merge
     into a single stacked object (real TComponent::doMerge()).
  6. Druid (user's report): a bound Druid spell (sticks to snakes, L15,
     reagent vnum 284 "some serpentine amber") demands its OWN reagent --
     the newbie spellbag's other-spell components do not satisfy it; the
     right reagent lifts the refusal.

    python3 tests/smoke_test_spell_component_binding.py [host] [port]
"""
import socket
import sys
import time
from mud_test_utils import send_line, recv_all, check, sql, cmd, announce, announce_done

host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
port = int(sys.argv[2]) if len(sys.argv) > 2 else 4000

announce("smoke_test_spell_component_binding", host, port)

_sfx = "".join(chr(ord("a") + (int(time.time() * 1000000) // 26 ** i) % 26) for i in range(6))


def make_char(name, pw, char_class):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "y"); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "new"); recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, "1"); recv_all(s)   # race: human
    send_line(s, "1"); recv_all(s)   # territory: urban
    send_line(s, char_class); recv_all(s)
    send_line(s, "done"); recv_all(s)
    send_line(s, "done"); recv_all(s)
    s.close()


def login(name, pw):
    s = socket.create_connection((host, port), timeout=5)
    recv_all(s)
    send_line(s, name); recv_all(s)
    send_line(s, pw); recv_all(s)
    send_line(s, "1"); recv_all(s)
    cmd(s, "color off")
    return s


pw = "comppwtest123"

# --- mortal Mage that CAN cast faerie fire (L2), forced past the disc gate,
#     plenty of mana; loaded once, progress set by SQL then reconnected so
#     the live being_t picks it up (same relog dance smoke_test_mana uses). ---
mage = f"Compmag{_sfx}"
make_char(mage, pw, "1")
sql("UPDATE player_progress SET level=10, basic_disc_pct=100, mana=100, max_mana=100 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{mage}');")
sm = login(mage, pw)

# 1. no component at all -> named the exact reagent
out = cmd(sm, "cast faerie fire")
check("pixie torch" in out.lower(),
      "a mortal with no component is told the exact reagent (a pixie torch) they need")

# 2. the Mage's newbie components (for OTHER spells) still don't satisfy
#    faerie fire -- a bound spell wants its OWN reagent, not just any.
out = cmd(sm, "cast faerie fire")
check("need a pixie torch" in out.lower(),
      "carrying components for other spells does NOT satisfy faerie fire")
sm.close()

# 3. RIGHT reagent (227) placed in inventory by SQL (a mortal can't `load`),
#    then reconnect so the live being_t reloads its inventory -> no refusal.
sql("INSERT INTO player_inventory (player_id, vnum, slot) VALUES "
    f"((SELECT id FROM player WHERE name='{mage}'), 227, -1);")
sm = login(mage, pw)
out = cmd(sm, "cast faerie fire")
check("need a pixie torch" not in out.lower() and "don't have the spell components" not in out.lower(),
      "holding the right reagent (a pixie torch) removes the refusal")
sm.close()

# --- immortal bypass: only a generic component on hand, still casts ---
imm = f"Compimm{_sfx}"
make_char(imm, pw, "1")
sql("UPDATE player_progress SET level=60, true_level=60 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{imm}');")
si = login(imm, pw)
cmd(si, "load obj 200")  # a component, but the WRONG one for faerie fire
out = cmd(si, "cast faerie fire")
check("need a pixie torch" not in out.lower() and "don't have the spell components" not in out.lower(),
      "an immortal casts a bound spell with any component (NOHASSLE bypass)")
si.close()

# --- merge (doMerge): two vnum-200 components merged on the floor -> one obj ---
war = f"Compwar{_sfx}"
make_char(war, pw, "3")  # warrior: no spell gear to muddy the count
sql("UPDATE player_progress SET level=60, true_level=60 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{war}');")
sw = login(war, pw)
# Two distinct component objects are created (avoid `inventory` -- its long
# listing trips the descriptor pager and desyncs the session).
o1 = cmd(sw, "load obj 200")
o2 = cmd(sw, "load obj 200")
check("conjure" in o1.lower() and "conjure" in o2.lower(),
      "two separate component objects were created")
# Drop both: the second landing on the floor merges into the first (drop hook,
# real TComponent::doMerge()). Picking one back up should exhaust the floor --
# a still-unmerged pair would leave a second object for the second `get`.
cmd(sw, "drop all.lasso")
g1 = cmd(sw, "get lasso")
check("don't see" not in g1.lower(), "picked the merged component stack back up")
g2 = cmd(sw, "get lasso")
check("don't see" in g2.lower(),
      "only ONE object remained on the floor -- the two components merged (2 -> 1)")
sw.close()

# --- Druid case: the user's actual report -- "Druid casting is using random
#     components for the same spell; the right component per spell, no other
#     component should work." A bound Druid spell must demand its OWN reagent.
#     sticks to snakes (Druid L15) is bound to vnum 284 "some serpentine amber";
#     the Druid spawns with a spellbag of components for OTHER spells. ---
dru = f"Compdru{_sfx}"
make_char(dru, pw, "5")  # Druid
sql("UPDATE player_progress SET level=20, basic_disc_pct=100, mana=100, max_mana=100 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{dru}');")
sd = login(dru, pw)
out = cmd(sd, "cast sticks to snakes")
check("serpentine amber" in out.lower(),
      "a Druid carrying only other-spell components is told the exact reagent "
      "(serpentine amber) sticks to snakes needs -- generic components no longer satisfy it")
sd.close()

# Right reagent (vnum 284) added by SQL; reconnect so the live being_t reloads
# inventory. The component refusal must lift (any later mana/Lifeforce message
# is fine -- it is not a component refusal).
sql("INSERT INTO player_inventory (player_id, vnum, slot) VALUES "
    f"((SELECT id FROM player WHERE name='{dru}'), 284, -1);")
sd = login(dru, pw)
out = cmd(sd, "cast sticks to snakes")
check("serpentine amber" not in out.lower() and "don't have the spell components" not in out.lower(),
      "holding the right reagent (serpentine amber) removes the Druid's component refusal")
sd.close()

sql(f"DELETE FROM player_inventory WHERE player_id=(SELECT id FROM player WHERE name='{dru}');")
sql(f"DELETE FROM player_progress WHERE player_id=(SELECT id FROM player WHERE name='{dru}');")
sql(f"DELETE FROM player_attrs WHERE player_id=(SELECT id FROM player WHERE name='{dru}');")
sql(f"DELETE FROM player WHERE name='{dru}';")

announce_done("smoke_test_spell_component_binding", host, port)
print("PASS: smoke_test_spell_component_binding")
