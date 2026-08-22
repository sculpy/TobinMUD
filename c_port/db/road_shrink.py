#!/usr/bin/env python3
"""General-purpose road/connector zone thinner for TobinMUD.

Usage: road_shrink.py <zone_nr> [--apply]

Without --apply: prints the plan (anchors, chains, cut list, generated SQL)
and does not touch the database.
With --apply: writes db/tobin/road_shrink_zone<N>.sql, applies it, and runs
the verification checks (no dangling exits, full BFS connectivity from an
arbitrary anchor, every external boundary edge still resolves both ways).
"""
import subprocess
import sys
import os

DB = "tobin"
DIR_DELTA = {
    0: (0, 1), 2: (0, -1),
    1: (1, 0), 3: (-1, 0),
    6: (1, 1), 9: (-1, -1),
    7: (-1, 1), 8: (1, -1),
    4: (0, 0), 5: (0, 0),  # up/down: no x,y delta expected
}
OPPOSITE = {0: 2, 2: 0, 1: 3, 3: 1, 6: 9, 9: 6, 7: 8, 8: 7, 4: 5, 5: 4}


def q(sql):
    out = subprocess.run(["mariadb", "-N", "-B", "-e", sql, DB],
                          capture_output=True, text=True, check=True)
    rows = [line.split("\t") for line in out.stdout.splitlines() if line]
    return rows


def main():
    zone_nr = int(sys.argv[1])
    apply = "--apply" in sys.argv[2:]

    rooms = {int(v): (int(x), int(y), int(z), name)
             for v, x, y, z, name in q(
        f"select vnum,x,y,z,name from room where zone={zone_nr}")}
    internal = set(rooms)
    print(f"zone {zone_nr}: {len(internal)} rooms")

    exits_rows = q(
        f"select vnum,direction,destination from roomexit "
        f"where vnum in ({','.join(map(str, internal))})")
    exits = {}  # (vnum,direction) -> destination
    adj = {v: [] for v in internal}  # vnum -> list of (direction, dest)
    for v, d, dest in exits_rows:
        v, d, dest = int(v), int(d), int(dest)
        exits[(v, d)] = dest
        adj[v].append((d, dest))

    degree = {v: len(adj[v]) for v in internal}
    external_neighbor = {v: any(dest not in internal for _, dest in adj[v])
                          for v in internal}
    anchors = {v for v in internal if degree[v] != 2 or external_neighbor[v]}

    # Never cut a room that anything spawns into: component_placement's
    # room1/room2, and any zone_reset arg that happens to match a room
    # vnum in this zone (conservative -- args can also be mob/obj vnums,
    # so this may over-preserve a few, never under-preserve).
    cp_rows = q(
        f"select room1,room2 from component_placement where "
        f"room1 in ({','.join(map(str, internal))}) or "
        f"room2 in ({','.join(map(str, internal))})")
    preserve = set()
    for r1, r2 in cp_rows:
        preserve.add(int(r1))
        if int(r2) != -1:
            preserve.add(int(r2))
    zr_rows = q(f"select arg1,arg2,arg3,arg4 from zone_reset where zone_nr={zone_nr}")
    for row in zr_rows:
        for a in row:
            if int(a) in internal:
                preserve.add(int(a))
    if preserve:
        print(f"preserving {len(preserve)} spawn-bearing room(s): {sorted(preserve)}")
        anchors |= preserve

    corridor = internal - anchors
    print(f"anchors: {len(anchors)}  corridor candidates: {len(corridor)}")

    # Any room ANYWHERE in the DB (any zone) may have a one-way exit
    # pointing into one of our rooms without a reciprocal edge back --
    # e.g. a teleporter, recall spot, or corpse-drop link. A corridor
    # room can only be safely cut if EVERY edge touching it, in either
    # direction, is one of exactly two fully-reciprocated neighbors.
    incoming_rows = q(
        f"select vnum,direction,destination from roomexit "
        f"where destination in ({','.join(map(str, corridor))})") if corridor else []
    incoming = {v: [] for v in corridor}
    for v, d, dest in incoming_rows:
        v, d, dest = int(v), int(d), int(dest)
        incoming[dest].append((v, d))

    safe_corridor = set()
    for v in corridor:
        neighbors_out = {dest for _, dest in adj[v]}
        neighbors_in = {src for src, _ in incoming[v]}
        touching = neighbors_out | neighbors_in
        ok = (len(touching) == 2 and neighbors_out == neighbors_in
              and len(adj[v]) == 2)
        if ok:
            for d, dest in adj[v]:
                if exits.get((dest, OPPOSITE[d])) != v:
                    ok = False
        if ok:
            safe_corridor.add(v)
        else:
            anchors.add(v)
    corridor = safe_corridor
    print(f"safe (fully-reciprocated, no stray inbound) corridor rooms: {len(corridor)}")

    # Walk chains: from each anchor, along each edge into corridor, until
    # hitting another anchor.
    visited_corridor = set()
    chains = []  # list of (anchor_a, [n1..nK], anchor_b, dir_a_to_n1, dir_nK_to_b)
    for a in anchors:
        for d, first in adj[a]:
            if first not in corridor or first in visited_corridor:
                continue
            path = []
            prev = a
            prev_dir_at_prev = d  # direction FROM prev TO cur, at prev's row
            cur = first
            while cur in corridor:
                visited_corridor.add(cur)
                path.append(cur)
                # find the edge out of cur that isn't back to prev
                nxts = [(dd, dest) for dd, dest in adj[cur] if dest != prev or
                        (dest == prev and len(adj[cur]) == 1)]
                # normal case: cur has exactly 2 edges, one back to prev
                back_dir = OPPOSITE[prev_dir_at_prev]
                forward = [(dd, dest) for dd, dest in adj[cur] if dd != back_dir]
                if len(forward) != 1:
                    break  # irregular, bail (shouldn't happen, safe_corridor filtered)
                fd, fdest = forward[0]
                prev = cur
                prev_dir_at_prev = fd
                cur = fdest
            b = cur
            if not path:
                continue  # anchors directly adjacent, no corridor between them
            # BUG (found on zone 67): prev_dir_at_prev is the direction
            # stored AT path[-1] pointing toward b -- NOT b's own direction
            # back toward path[-1]. Using it as b's direction silently
            # updated the wrong (or no) row at b, leaving b's real exit
            # toward the corridor untouched. Look up b's actual direction.
            dir_at_b = [dd for dd, dest in adj[b] if dest == path[-1]][0]
            chains.append((a, path, b, d, dir_at_b))

    print(f"chains found: {len(chains)}  (covering {len(visited_corridor)} corridor rooms)")

    cut = []
    relinks = []  # (vnum, direction, new_destination)
    for a, path, b, dir_a_first, dir_last_b in chains:
        n = len(path)
        # keep pattern: drop first, alternate (drop,keep,drop,keep,...)
        keep_flags = [(i % 2 == 1) for i in range(n)]
        survivors = [path[i] for i in range(n) if keep_flags[i]]
        for i in range(n):
            if not keep_flags[i]:
                cut.append(path[i])

        # splice: walk original order, tracking last surviving node (or a)
        last = a
        last_dir_toward_chain = dir_a_first  # direction AT `last` pointing toward next chain node
        for i in range(n):
            v = path[i]
            if keep_flags[i]:
                # backward slot at v: direction opposite of its forward dir
                # v's own two directions: one toward prev original node, one toward next
                dirs_at_v = [dd for dd, _ in adj[v]]
                # backward dir = the one whose original destination is "toward a" side
                # = OPPOSITE of v's forward dir; forward dir = the one NOT equal to
                # opposite of (direction at v pointing to previous original node)
                # We recover both original dirs directly:
                if i == 0:
                    orig_back_dir = OPPOSITE[dir_a_first]
                else:
                    # direction at v pointing back to path[i-1]
                    orig_back_dir = [dd for dd, dest in adj[v] if dest == path[i - 1]][0]
                orig_fwd_dir = [dd for dd in dirs_at_v if dd != orig_back_dir][0]

                relinks.append((last, last_dir_toward_chain, v))
                relinks.append((v, orig_back_dir, last))
                last = v
                last_dir_toward_chain = orig_fwd_dir
        relinks.append((last, last_dir_toward_chain, b))
        relinks.append((b, dir_last_b, last))

    print(f"cut rooms: {len(cut)}  relink statements: {len(relinks)}")
    kept = len(internal) - len(cut)
    print(f"result: {len(internal)} -> {kept} rooms ({100*len(cut)//len(internal)}% cut)")

    sql_lines = [
        f"-- Road-shrink: zone {zone_nr}. Auto-generated by road_shrink.py.",
        f"-- Thins reciprocated degree-2 corridor chains by roughly half;",
        f"-- every anchor (junction or external-boundary room) keeps its vnum.",
        f"-- Idempotent: repoint UPDATEs setting a value to what it already is,",
        f"-- and DELETEs of already-removed rows, are no-ops.",
        "",
    ]
    seen = set()
    for v, d, dest in relinks:
        key = (v, d)
        if key in seen:
            continue
        seen.add(key)
        sql_lines.append(
            f"UPDATE roomexit SET destination = {dest} WHERE vnum = {v} AND direction = {d};")
    sql_lines.append("")
    cut_list = ",".join(str(v) for v in sorted(cut))
    sql_lines.append(f"DELETE FROM roomexit WHERE vnum IN ({cut_list});")
    sql_lines.append(f"DELETE FROM room WHERE vnum IN ({cut_list});")
    sql_lines.append("")
    sql_text = "\n".join(sql_lines)

    out_path = f"/home/mud/TobinMUD/c_port/db/tobin/road_shrink_zone{zone_nr}.sql"
    with open(out_path, "w") as f:
        f.write(sql_text)
    print(f"wrote {out_path}")

    # Pre-flight simulation: every CURRENT edge pointing at a to-be-cut
    # room must be covered by one of the UPDATE statements above, or it
    # will dangle/FK-fail once the DELETE runs. Catches relink bugs
    # before touching the live DB, not after.
    updated_pairs = {(v, d) for v, d, _ in relinks}
    inbound_rows = q(
        f"select vnum,direction,destination from roomexit "
        f"where destination in ({cut_list})") if cut else []
    uncovered = [(int(v), int(d), int(dest)) for v, d, dest in inbound_rows
                 if (int(v), int(d)) not in updated_pairs]
    print(f"pre-flight: {len(inbound_rows)} inbound edges into cut list, "
          f"{len(uncovered)} NOT covered by a relink")
    if uncovered:
        print(f"UNCOVERED (would dangle/FK-fail): {uncovered}")
        if apply:
            print("REFUSING TO APPLY: uncovered inbound edges found.")
            sys.exit(1)

    if not apply:
        print("(dry run -- pass --apply to write to DB)")
        return

    r = subprocess.run(["mariadb", DB], input=sql_text, text=True,
                        capture_output=True)
    if r.returncode != 0:
        print("APPLY FAILED:")
        print(r.stderr)
        sys.exit(1)
    print("APPLIED_OK")

    # verify: no dangling destinations among the surviving+anchor set
    dangling = q(
        f"select re.vnum,re.direction,re.destination from roomexit re "
        f"left join room r on r.vnum=re.destination "
        f"where r.vnum is null and re.vnum in "
        f"({','.join(str(v) for v in internal if v not in cut)})")
    print(f"dangling exits: {len(dangling)}")
    if dangling:
        print(dangling)

    remaining = q(f"select count(*) from room where zone={zone_nr}")
    print(f"zone {zone_nr} room count now: {remaining[0][0]}")

    # BFS connectivity from an arbitrary anchor
    survivors = internal - set(cut)
    start = next(iter(anchors))
    edges = q(
        f"select vnum,direction,destination from roomexit "
        f"where vnum in ({','.join(str(v) for v in survivors)})")
    g = {}
    for v, d, dest in edges:
        v, dest = int(v), int(dest)
        g.setdefault(v, []).append(dest)
    seenb = {start}
    stack = [start]
    while stack:
        cur = stack.pop()
        for nxt in g.get(cur, []):
            if nxt in survivors and nxt not in seenb:
                seenb.add(nxt)
                stack.append(nxt)
    unreached = survivors - seenb
    print(f"BFS reached {len(seenb)} of {len(survivors)} survivors; unreached: {sorted(unreached)}")


if __name__ == "__main__":
    main()
