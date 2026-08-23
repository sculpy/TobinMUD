--[[
TobinMUD Mapper for Mudlet
===========================
Drives Mudlet's built-in mapper (the same one IRE/Aardwolf-style clients
use) off TobinMUD's existing GMCP `Room.Info` package -- no server changes
needed, this is a pure Mudlet-side script.

Install:
  1. In Mudlet, open the TobinMUD profile's Special Options and make sure
     "Enable GMCP" is checked (it is on by default for most profiles).
  2. Script Editor > Scripts > New Script (or New Item), paste this whole
     file in, save. Reconnect (or `lua tobin.onRoomInfo()` won't fire
     until the next Room.Info the server sends, e.g. your next move).
  3. Open the map with `Ctrl+M` (or Map > Show Map). Walk around -- rooms
     and exits populate live as TobinMUD sends Room.Info for each room
     you enter.

What it does:
  - Uses the room's TobinMUD vnum (`gmcp.Room.Info.num`) directly as the
    Mudlet room ID -- no separate ID-mapping table needed, and it means
    a TobinMUD vnum you see in-game (immortal `goto <vnum>`, `redit`,
    etc.) is the exact same number Mudlet's mapper uses.
  - Creates/updates the room's name and the 10 exits TobinMUD reports
    (secret exits are never sent by the server, so they never appear on
    the map -- matches `exits` command's own convention).
  - Sets room coordinates straight from the server's `x`/`y`/`z` (the
    same maprecalc-derived layout the Windows client's map view uses),
    so this mapper and the Windows client draw the same shape.
  - All rooms land in a single Mudlet map area ("TobinMUD") for now --
    TobinMUD doesn't yet send an area/zone name over GMCP, just x/y/z.

Known limitation (server-side, not this script): until TobinMUD's world
graph is fully reciprocal, a handful of rooms reachable only via a
one-way exit can end up in a different coordinate "island" than their
neighbors and may draw far from where you'd expect, or not connect
visually to nearby rooms. This is being tracked and fixed server-side;
this script draws whatever coordinates the server currently reports.
--]]

tobin = tobin or {}
tobin.areaName = "TobinMUD"

local DIR_SHORT = {
  north = "n", south = "s", east = "e", west = "w",
  up = "u", down = "d",
  northeast = "ne", northwest = "nw", southeast = "se", southwest = "sw",
}

function tobin.ensureArea()
  if not tobin.areaID then
    local areas = getAreaTable()
    for id, name in pairs(areas) do
      if name == tobin.areaName then
        tobin.areaID = id
        break
      end
    end
    if not tobin.areaID then
      tobin.areaID = addAreaName(tobin.areaName)
    end
  end
  return tobin.areaID
end

function tobin.onRoomInfo()
  local info = gmcp.Room.Info
  if not info or not info.num then
    return
  end
  local vnum = tonumber(info.num)
  if not vnum then
    return
  end

  local areaID = tobin.ensureArea()

  if not roomExists(vnum) then
    addRoom(vnum)
    setRoomArea(vnum, areaID)
  end

  if info.name and info.name ~= "" then
    setRoomName(vnum, info.name)
  end

  if info.x and info.y and info.z then
    local x, y, z = tonumber(info.x), tonumber(info.y), tonumber(info.z)
    if x and y and z then
      setRoomCoordinates(vnum, x, y, z)
    end
  end

  if type(info.exits) == "table" then
    for dirName, destRaw in pairs(info.exits) do
      local short = DIR_SHORT[dirName]
      local dest = tonumber(destRaw)
      if short and dest then
        if not roomExists(dest) then
          addRoom(dest)
          setRoomArea(dest, areaID)
        end
        setExit(vnum, dest, short)
      end
    end
  end

  tobin.currentRoomVnum = vnum
  if centerview then
    centerview(vnum)
  end
  updateMap()
end

registerAnonymousEventHandler("gmcp.Room.Info", "tobin.onRoomInfo")

echo("[TobinMUD Mapper] loaded -- walk around to populate the map (Ctrl+M to view).\n")
