# Cartograph Design Notes

Visual design philosophy and implementation details.

---

## Visual Language

### Grid & Canvas
- **Grid**: Optional overlay showing tile boundaries
- **Default**: 256×256 tiles @ 16px per tile
- **Zoom**: Pan/zoom for navigation, grid scales appropriately
- **Background**: Solid color from active theme

### Rooms
```
┌─────────────────┐
│                 │  ← Solid rectangular outline
│   Room Name     │  ← Label inside room
│                 │
└─────────────────┘
```

**Properties**:
- Solid outline (2px thickness)
- Semi-transparent fill (30% alpha of room color)
- Label rendered at top-left corner
- Rectangular only (axis-aligned)

### Doors (Room Connections)
```
┌─────────────────┐         ┌─────────────────┐
│   Room A        ·  ·  ·  ·│   Room B        │
│                 │         │                 │
└─────────────────┘         └─────────────────┘
                  ↑
            Dotted line on wall edge
```

**NOT icon markers** - rendered as visual connections:
- Dotted line segment on room wall
- Positioned at door endpoint coordinates
- Connects two rooms visually
- Color/thickness can indicate door type:
  - Normal door: Default color
  - Locked door: Red/thicker line
  - One-way: Arrow decoration

**Why this design?**
- Clearer spatial relationships
- Doors are connections, not points
- Reduces visual clutter
- Standard in level design tools

### Markers (Points of Interest)
```
┌─────────────────┐
│      [💾]       │  ← Icon-based marker
│   Save Point    │  ← Optional label
└─────────────────┘
```

**Marker types** (icon-based):
- Save points (bench icon)
- Boss encounters (skull icon)
- Treasure/items (chest icon)
- Generic upgrades (dot icon)
- Custom user icons

**Properties**:
- Placed at specific tile coordinates
- Icon rendered from atlas
- Optional text label
- Color tinting supported
- Can reference custom icons in project

### Tiles (Detailed Room Contents)
- Painted within rooms using palette
- Color-coded by type (solid, hazard, water, etc.)
- Row-run encoded for efficiency
- Optional layer - can be hidden for overview

---

## Icon System

### Default Icons (Shipped)
1. `bench.png` - Save points, rest areas
2. `chest.png` - Treasure, item pickups
3. `skull.png` - Boss encounters, major enemies
4. `dot.png` - Generic marker (auto-generated circle)

### Custom Icons

**App-wide** (`assets/icons/`):
- Available to all projects
- PNG or SVG (if built with flag)
- Loaded at startup

**Per-project** (`.cart` `/icons/`):
- Bundled with project file
- Travel with the map
- Override app icons if same name

**Atlas Building**:
- All icons packed into single texture at startup
- UV coordinates stored per icon
- Referenced by name string in JSON
- Hot-reload supported (desktop only)

---

## Color & Theme Philosophy

### Dark Theme (Default)
- Background: Dark gray (#1a1a1a)
- Grid: Subtle gray (#333333)
- Room outlines: Light gray (#cccccc)
- Doors: Golden yellow (#ffcc33)
- Text: White (#ffffff)

### Print-Light Theme
- Background: White (#ffffff)
- Grid: Light gray (#dddddd)
- Room outlines: Dark gray (#333333)
- Doors: Dark orange (#cc9900)
- Text: Black (#000000)

**Goal**: High contrast for readability, clear hierarchy

---

## Interaction Model

### Tools
- **Paint/Erase**: Direct tile manipulation
- **Rectangle**: Quick room filling
- **Fill**: Flood fill for large areas
- **Door**: Two-click placement (endpoint A → endpoint B)
- **Marker**: Single click + properties dialog

### Navigation
- **Pan**: Space + Drag or Middle Mouse
- **Zoom**: Mouse wheel or +/- keys
- **Focus**: Double-click room to center view

### Selection
- Click to select room/marker/door
- Properties panel shows details
- Handles for resize (rooms)
- Delete key removes selected

---

## Export Rendering

When exporting to PNG, render in order:
1. Background (solid or transparent)
2. Grid (optional)
3. Room fills (semi-transparent)
4. Tiles (if layer enabled)
5. Room outlines (solid)
6. Doors (dotted lines)
7. Markers (icons + labels)
8. Legend (optional, bottom-right)

**Scale factors**: 1×, 2×, 3×, or custom
**Crop modes**: Used area + padding, or full grid

---

## Data Model Notes

### Room Coordinates
- Stored in **tile coordinates** (not pixels)
- `rect[x, y, w, h]` = tile position and size
- Converted to pixels during rendering: `px = tiles * tileSize * zoom`

### Door Endpoints
- Also in tile coordinates
- Side enum: North, South, East, West
- Position must be on room edge for proper rendering
- Validation: Check endpoint is within room bounds on correct side

### Marker Positioning
- Single point (x, y) in tile coordinates
- Can be anywhere on map (not restricted to rooms)
- Rendered at center of tile

---

## Implementation Priorities

1. **MVP**: Solid lines, dotted doors, basic markers
2. **Polish**: One-way door arrows, door type colors
3. **Advanced**: Gated doors (ability requirements), reachability overlay
4. **Future**: Custom door rendering styles, animated markers

---

**Last Updated**: November 10, 2025

