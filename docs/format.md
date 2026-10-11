# Cartograph project format

Cartograph stores a map as one `project.json`. This page describes version 3,
the version current saves write. Older files (1 and 2) still load: version 1
is migrated (Y flip), and the sections added in version 3 are simply empty.

A complete small map that uses every field is in
[`sample-map.json`](sample-map.json). It validates cleanly:

```bash
cartograph-cli validate docs/sample-map.json
```

## Containers

| Container | Layout |
|-----------|--------|
| `.cart` | ZIP with `manifest.json`, `project.json`, optional `icons/*.png`, `thumb.png` |
| `.cartproj` | Folder with `project.json`, optional `icons/`, `preview.png` |
| bare JSON | Just `project.json` (what `cartograph-cli export` writes) |

All three carry the same `project.json`. Generators only need that file.

## Command line

`cartograph-cli` is a headless tool built next to the app. It opens no window.

```bash
cartograph-cli export map.cart -o map.json     # project.json at version 3
cartograph-cli export map.cart                 # same, to stdout
cartograph-cli validate map.cart               # gate / reachability check
cartograph-cli validate map.cart --order dash,double-jump
```

`<input>` may be a `.cart`, a `.cartproj` folder, or a `project.json`. Old
versions are migrated on read, so an export is always version 3.

Exit codes: `0` ok, `1` validation errors, `2` bad usage, `3` read or write
failure.

## Conventions

- **Cells.** The map is a grid of cells. A cell is addressed `(x, y)`. **Y is
  up**: `y + 1` is north. (Version 1 files were Y-down and are flipped on load.)
- **Edges.** An edge is the boundary between two adjacent cells, written
  `x1, y1, x2, y2`. It is stored normalized: `(x1, y1)` sorts before
  `(x2, y2)` by `x`, then `y`. Cell **a** is `(x1, y1)`, cell **b** is
  `(x2, y2)`. A vertical edge joins `(x, y)` and `(x + 1, y)`. A horizontal
  edge joins `(x, y)` and `(x, y + 1)`.
- **Marker positions** are in cell units, Y-up, and may be fractional:
  `pos: [5.5, 2.5]` is the center of cell `(5, 2)`.
- **Terminology.** A map cell painted with a palette color is stored in
  `tiles`. The sub-grid inside a cell is `interior`, and its entries are
  called interior tiles. They are different things.
- **Colors** are `#RRGGBB` (or `#RRGGBBAA` for palette entries).
- **Props** are string-to-string maps. Numbers and booleans in a file are
  read as their text (`true` becomes `"true"`); nested values are dropped.
  Limits: 256 pairs per object, 4096 characters per key or value.

## Top-level keys

| Key | Type | Notes |
|-----|------|-------|
| `version` | number | `3` when saved. Loads accept 1 to 3 |
| `meta` | object | `title`, `author`, `description` |
| `grid` | object | Cell size and bounds, see below |
| `palette` | array | `{id, name, color}` for painted cells |
| `abilities` | array | Ordered ability registry, see below |
| `regionGroups` | array | User groups of rooms |
| `rooms` | array | Room metadata |
| `settings` | object | `autoGenerateRoomWalls` (bool) |
| `tiles` | array | Painted cells, run-length rows |
| `edges` | array | Walls and doors on cell edges |
| `cellRooms` | array | Which cell belongs to which room |
| `doorProps` | array | Gate, one-way, spawns for door edges |
| `doors` | array | Legacy door list, see below |
| `markers` | array | Benches, chests, pickups, start, ... |
| `interior` | object | Per-cell sub-grids, see below |
| `keymap` | object | Editor key bindings (ignored by generators) |

Sections you do not use may be missing. Sections added in version 3
(`abilities`, `doorProps`, `interior`, and every `props`) are only written
when non-empty.

### `grid`

| Field | Meaning |
|-------|---------|
| `preset` | `"square"` (16x16 px cells) or `"rectangle"` (32x16) |
| `tileWidth`, `tileHeight` | Cell size in pixels at 1:1 zoom |
| `cols`, `rows` | Grid size in cells (editor bound) |
| `minCol`, `minRow` | Lowest cell coordinate (can be negative) |
| `locked` | Preset cannot change once markers exist |
| `autoExpandGrid`, `expansionThreshold`, `expansionFactor`, `edgeHoverThreshold` | Editor behavior |

### `abilities`

```json
{"id": "dash", "name": "Dash", "color": "#ff5533"}
```

The order of the array is the **intended acquisition order**. `id` is what
doors and pickups refer to. Gates naming an id that is not here still load,
but validation reports them.

### `rooms` and `regionGroups`

Room: `id`, `name`, `color`, `notes`, `tags[]`, `props`, `regionId` (unused,
`-1`), `parentRegionGroupId`, `images[]` (design doc paths). Room geometry is
not stored on the room: it is the set of `cellRooms` entries naming it.
`connectedRooms[]` may appear (derived from doors, recomputed on load).

Region group: `id`, `name`, `description`, `tags[]`, `rooms[]` (room ids),
`props`. Typical region props: `palette`, `music`, `ambience`,
`parallaxLayers`. Typical room props: `kind` (`arena`, `hub`, ...), `music`,
`dark`. Keys are free-form; these are conventions.

### `edges`

`{x1, y1, x2, y2, state}` with `state` `1` = wall, `2` = door. Missing edges
are open.

### `doorProps`

Extra data for door edges (`state: 2`). Entries for edges that are not doors
are not written and are ignored on load.

| Field | Meaning |
|-------|---------|
| `x1, y1, x2, y2` | The door edge |
| `gate` | Ability id needed to pass. Missing or empty means open |
| `oneWay` | `"none"` (default), `"a-to-b"`, `"b-to-a"` |
| `spawnA` | `[x, y]` where the player appears arriving in cell a |
| `spawnB` | `[x, y]` where the player appears arriving in cell b |
| `props` | Free-form props |

Spawns use cell units like marker positions. `a-to-b` means the door can only
be walked from cell a to cell b.

### `cellRooms` and `tiles`

`cellRooms`: `{x, y, roomId}` assigns a cell to a room.

`tiles` holds painted cells in rows: `{room, y, runs}` where each run is
`[startX, count, tileId]`. `room` is `""` for the global canvas. `tileId`
indexes `palette`.

### `markers`

`{id, room, pos, kind, label, icon, color, size?, showLabel?, props?}`.
`room` is informational; the room of a marker is the room of the cell
`floor(pos)`.

Built-in kinds and their `props` fields:

| `kind` | Fields |
|--------|--------|
| `start` | none. Exactly one per map |
| `bench` | none |
| `chest` | `contents` |
| `boss` | `name` |
| `ability-pickup` | `ability` (an ability id) |
| `enemy` | `type` |
| `custom` | any |

Other kind strings (including those from old files) are kept as-is.

### `interior`

An optional sub-grid per cell, default 16 by 9 interior tiles, for the
inside of rooms.

```json
{"width": 16, "height": 9, "chunks": [
  {"cell": [0, 0], "rows": ["################", "...", "################"]}
]}
```

- `rows` has `height` strings of `width` characters. **Row 0 is the north
  (top) row.**
- Characters: `.` empty, `#` solid, `=` platform (one-way), `^` hazard,
  `~` water, `b` breakable, `H` ladder.
- A cell without a chunk is entirely empty. Chunks are keyed by map cell,
  not by room.
- Size is project-wide and cannot change after interior tiles exist.

Interior tile `(col, row)` of cell `(cx, cy)` has global Y-up coordinates
`ix = cx * width + col`, `iy = cy * height + (height - 1 - row)`. A marker at
`pos [x, y]` sits on interior tile `ix = floor(x * width)`,
`iy = floor(y * height)`.

### `doors` (legacy)

An older door list (`id`, `type`, `gate[]`, endpoints `a` and `b`). The editor
no longer creates these and validation ignores them. Use door edges plus
`doorProps`.

## Deriving what a generator needs

- **Rooms joined by a door:** for each edge with `state: 2`, look up the room
  of cell a (`cellRooms` at `x1, y1`) and cell b (`x2, y2`). If both exist and
  differ, the rooms are connected. Apply `doorProps` for gate and direction.
- **Room shape:** all `cellRooms` cells with that `roomId`. Stitch their
  `interior` chunks by global coordinates for the room's full tile layout.
- **Where the player appears through a door:** `spawnA` when entering cell a,
  `spawnB` when entering cell b.

## Validation

`cartograph-cli validate` (and Validate Gates in the app) uses this model:

- Rooms are nodes; door edges between different rooms are arcs. A gated arc
  needs its ability; `oneWay` limits direction.
- Stage `i` holds the first `i` abilities of the order (the registry order, or
  `--order`). The last stage holds all of them.
- **Unreachable:** never reached from the start room with every ability.
- **Softlock:** reachable at a stage but unable to get back to the start
  or to the pickup of the next ability.
- **Pickup problems:** a pickup that needs its own or a later ability, a
  pickup with no ability set, or an ability not in the order. An ability
  without any pickup is only a warning.
- Exactly one `start` marker, inside a room, is required.

## Compatibility

- Version 1 (Y-down): flipped and shifted to start at the origin on load.
- Version 2: loads unchanged; version 3 sections are empty.
- Version 3: current. Files with a newer `version` are rejected.
