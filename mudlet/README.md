# TobinMUD Mapper for Mudlet

Drives Mudlet's built-in mapper off TobinMUD's existing GMCP `Room.Info`
package. No server-side changes needed -- this is a pure Mudlet script.

## Install

1. Connect to TobinMUD (`tobinmud.com:4000`) in Mudlet.
2. In the profile's **Special Options**, confirm "Enable GMCP" is checked
   (on by default for most profiles).
3. **Script Editor > Scripts > New Script**, paste in `tobinmud_mapper.lua`,
   save.
4. Open the map with **Ctrl+M** (or **Map > Show Map**). Walk around --
   rooms and exits populate live as the server sends `Room.Info` for each
   room you enter.

## Notes

- Room IDs on the Mudlet map are TobinMUD's own room vnums -- the same
  number you'd see with an immortal `goto <vnum>` or `redit`.
- Coordinates come straight from the server's `x`/`y`/`z` (the same
  maprecalc-derived layout the Windows client's own map view uses), so
  this mapper and the Windows client draw the same shape.
- Known limitation (server-side): until TobinMUD's world graph is fully
  reciprocal, a room reachable only via a one-way exit can land in a
  different coordinate "island" than its neighbors and may not connect
  visually on the map. Being tracked/fixed server-side; this script just
  draws whatever the server currently reports.
