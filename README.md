# TobinMUD

**Tobin** is a from-scratch C port of *SneezyMUD* (a DikuMUD-derived C++ MUD
server), running live at `tobinmud.com:4000` (telnet) and
[tobinmud.com/play.html](https://tobinmud.com/play.html) (browser). This
repo is the whole project: server, native and web clients, a Mudlet mapper
script, and the untouched upstream reference it's ported from.

## Layout

| Path | What it is |
| --- | --- |
| [`c_port/`](c_port/) | **The server.** All Tobin source, build files, tests, and DB schema. Start here — see [`c_port/README.md`](c_port/README.md) to build/run/test, and [`c_port/STATUS.md`](c_port/STATUS.md) for architecture decisions and the full session log. |
| [`sneezymud-master/`](sneezymud-master/) | Upstream SneezyMUD C++ clone, kept untouched as a porting reference. Also carries the MariaDB world seed data (`sneezymud-master/db/`) that `c_port/` needs to boot. |
| [`client/`](client/) | Native Win32 client (GMCP/MSDP/MSP support). See [`client/README.md`](client/README.md). |
| [`mudlet/`](mudlet/) | A Mudlet mapper script driven by the server's GMCP `Room.Info` — no server changes needed. See [`mudlet/README.md`](mudlet/README.md). |
| [`web/`](web/) | The browser play client (`play.html`) plus generated help/news pages. |
| [`web-bridge/`](web-bridge/) | `ws_bridge.py` — WebSocket-to-telnet shim so browsers (which can't open raw TCP) can talk to the live server on `127.0.0.1:4000`. |
| [`docs/`](docs/) | Misc reference docs (license text, spell assignment spreadsheet). |

## Getting started

To build and run the server, go to [`c_port/README.md`](c_port/README.md) —
it covers prerequisites, DB setup, build, run, and the test suite in full.
Everything else in this repo (clients, web bridge) talks to that server
over telnet, GMCP, or the WebSocket bridge; none of it runs standalone.

## Development

- Live production server — see [`CLAUDE.md`](CLAUDE.md) for house rules
  (all editing/building/running happens via SSH on the droplet, deploys use
  `copyover`, etc.) and [`SYNC.md`](SYNC.md) for the git sync procedure.
- Sync is git: private repo `github.com/sculpy/TobinMUD`.
