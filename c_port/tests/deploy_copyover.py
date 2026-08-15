#!/usr/bin/env python3
"""Deploy the freshly built binary in place via `copyover -now` (zero-drop)
so the new boot sequence (component_placement_load, etc.) runs. Creates a
throwaway level-60 immortal, logs in, issues copyover -now."""
import socket, sys, time
sys.path.insert(0, "/home/mud/TobinMUD/c_port/tests")
from mud_test_utils import send_line, recv_all, cmd, sql

host, port = "127.0.0.1", 4000
pw = "deploypw1"
name = "Deployzz"

s = socket.create_connection((host, port), timeout=5)
recv_all(s, 1.0)
# create-or-login: try new-char flow; if the name exists this still logs in.
for step in [name, "y", pw, pw, "new", name, "1", "1", "3", "done", "done"]:
    send_line(s, step); recv_all(s, 0.6)
s.close()
sql("UPDATE player_progress SET level=60, true_level=60 "
    f"WHERE player_id=(SELECT id FROM player WHERE name='{name}');")

s = socket.create_connection((host, port), timeout=5)
recv_all(s, 1.0)
send_line(s, name); recv_all(s, 0.7)
send_line(s, pw); recv_all(s, 0.7)
send_line(s, "1"); recv_all(s, 0.7)
out = cmd(s, "copyover -now")
print("copyover issued:", out.strip()[:120])
time.sleep(2)
s.close()
