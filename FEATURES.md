# TobinMUD — Feature List

A player-facing tour of what Tobin, the C-rewritten SneezyMUD server at
`tobinmud.com:4000`, currently supports. For the technical map of how these
are implemented, see [`c_port/doc/systems/README.md`](c_port/doc/systems/README.md);
for the build-by-build history, see [`c_port/STATUS.md`](c_port/STATUS.md).

## Characters & accounts

- Account system with multiple characters per account (`multiplay` toggle
  for playing more than one at once).
- Character creation: point-buy attributes (base 120, ±30 per stat, net
  30-point pool), 6 classes (Mage, Cleric, Warrior, Thief, Druid, Monk),
  multiple playable races each with their own stat/perk profile (see
  [`docs/RACE_STATS.md`](c_port/docs/RACE_STATS.md) /
  [`RACE_PERKS.md`](c_port/docs/RACE_PERKS.md)).
- 60 levels: 1–50 mortal, 51–60 immortal (Immortal/God/Greater
  God/Administrator/Implementor ranks).
- Persistent score/attributes/inventory/equipment, `title`, `prompt`
  customization, aliases.

## Combat

- Real-time, round-based combat (PC-vs-PC and PC-vs-mob), roughly 1.2s
  per round.
- Special attacks and maneuvers: bash, kick, disarm, parry, dodge, flee,
  rescue, backstab, bodyslam, headbutt, kneestrike, grapple, shove, trip,
  slam, spin, stomp, chop, cudgel, garrotte, throatslit, deathstroke,
  bonebreak, defenestrate, quivering palm, and more — mostly class-gated
  skills trained through use.
- A real body model: 18 limb slots (plus extra slots for non-humanoid
  mobs), 60 creature body types each with their own per-limb hit-weighting,
  so attacks land somewhere sensible for the creature's shape.
- Damage is described in prose (an 11-tier ladder from "pathetically" to
  "into a bloody pulp") — no raw numbers shown to mortals.
- 5-tier material quality (Common → Legendary) affecting weapon/armor
  damage, AC, durability, and value.
- Poison, disease, paralysis, and other status effects layered on top via
  a general timed-affect system.

## Magic & skills

- Full class-based spell/skill/prayer rosters (Mage, Cleric, Druid, Monk)
  imported from the original SneezyMUD, including damage spells (fireball,
  lightning bolt, ice storm, chain lightning, meteor swarm...), healing
  (heal light/serious/critical/full, mend, restore limb...), buffs/debuffs
  (haste, bless, curse, sanctuary, stone skin...), divination (detect
  magic/invisible/trap, identify...), and elemental conjuration.
  animal companions and beast-charm (Druid).
- 3-tier skill/discipline framework (universal / class-core / secondary
  discipline) with proficiency-by-doing and formal `practice` training.
- Monk sash quest chain (white through black) as a distinct advancement
  path outside the class spell list.
- Builder-authored trigger scripting (`edit trigger`) — a DG Scripts-style
  language with `if`/`while`/`switch`, variable substitution, and
  `set`/`eval`/`global`, for scripted mob/object/room behavior.

## Professions & crafting

- **Cook** — real ingredient-matching recipes.
- **Farming** — seed planting/growth for 15 real crop types; Thief's
  reverse-pickpocket `plant` reuses the same system for planted contraband.
- **Extraction** — skin/butcher corpses and forage in the wild for
  crafting materials.
- Carried-liquid tracking (`drink`/`pour`/`fill`/`sip`) across containers.
- Repair-shop economy tied into the material-tier durability model.
- Drug use/addiction tracking per character.
- Commodities: a live supply/demand pricing engine (ported from
  SneezyMUD's ~200-material system) driving mob-wealth-based economy.

## Economy

- Money/banking (deposit, withdraw, bank-flagged shops).
- 264 real NPC shops (buy/sell), stocked per shop type.
- Per-class/race combat balance modifiers, immortal-tunable.
- Accumulated sales-tax treasury tracking.

## World & travel

- ~19,000-room live world, seeded from SneezyMUD's world data and
  self-contained (no runtime dependency on the upstream clone).
- Zone reset system (rooms/mobs/objects repopulate on a timer).
- Weather simulation and an in-game clock/calendar (real day/night,
  matches the original's weekday formula).
- Movement in 8 directions plus climbing, swimming, hiking; stealth
  movement (sneak, hide, skulk); tracking, scanning, and searching.
- Long-distance travel: dragon `travel` (flight through waypoints across
  ~7-leg routes), plus levitate/flight/teleport/portal spells.
- Mob AI: pursuit and autonomous behavior driven off the pulse scheduler.

## Social & communication

- `say`/`shout`/`tell`/group-tell, 155 emote socials imported from the
  original, bulletin boards, `ignore` lists.
- Party/group system (shared combat, XP split among grouped members).
- In-game languages (multiple constructed racial/creature tongues).
- Player-run quests and quest-item systems, including per-race quest
  rewards.

## Building & administration (immortal tools)

- Menu-driven editors for every world-content type: `edit room` / `zone`
  / `object` / `mobile` / `player` / `account` / `social` / `trigger` /
  `help` / `news` / `wiznews` / `rules` / `shop` — modeled the same way as
  character creation, wireframe-driven, not ad hoc.
- Builder tools: `goto`, `dig`, `load`, `loadroom`, map export/recalc.
- Typed game-log system, `snoop`/`possess`/`stat`/`set` for oversight and
  debugging, promotion tools, and an in-game `wiznet`.
- Hot-reload deploys via `copyover` — active connections survive a server
  restart; graceful `shutdown` otherwise.

## Multi-client support

- **Telnet** — the canonical client-agnostic path (`tobinmud.com:4000`).
- **Native Win32 client** ([`client/`](client/)) with GMCP/MSDP/MSP
  support (structured game state and sound cues).
- **Browser client** ([`web/`](web/), `play.html`) via a WebSocket-to-telnet
  bridge ([`web-bridge/`](web-bridge/)).
- **Mudlet mapper** ([`mudlet/`](mudlet/)) — drives Mudlet's built-in
  mapper off the server's GMCP `Room.Info`, no server changes needed.
- Auto-generated, browsable help and news pages
  ([`web/help.html`](web/help.html), [`web/news.html`](web/news.html)) from
  the same in-game help/news database builders maintain with `edit help`
  / `edit news`.
