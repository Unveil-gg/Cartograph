#pragma once

#include <cstdint>
#include <vector>

namespace Cartograph {

// Tile types painted inside a cell's interior sub-grid. Not to be confused
// with map "tiles" (TileData), which are whole map cells.
enum class InteriorTile : uint8_t {
    Empty = 0,
    Solid,      // '#'
    Platform,   // '=' one-way platform
    Hazard,     // '^'
    Water,      // '~'
    Breakable,  // 'b'
    Ladder      // 'H'
};

constexpr int kInteriorTileKinds = 7;

/**
 * Integer division that rounds toward negative infinity, so negative
 * interior coordinates land in the right cell.
 * @param a Dividend
 * @param b Divisor (positive)
 * @return floor(a / b)
 */
inline int InteriorFloorDiv(int a, int b) {
    int q = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
}

/**
 * Display name of a tile type.
 * @param t Tile type
 * @return Human-readable name
 */
inline const char* InteriorTileName(InteriorTile t) {
    switch (t) {
        case InteriorTile::Solid: return "Solid";
        case InteriorTile::Platform: return "Platform";
        case InteriorTile::Hazard: return "Hazard";
        case InteriorTile::Water: return "Water";
        case InteriorTile::Breakable: return "Breakable";
        case InteriorTile::Ladder: return "Ladder";
        default: return "Empty";
    }
}

/**
 * File character for a tile type ('.' for empty).
 * @param t Tile type
 * @return One character used in saved rows
 */
inline char InteriorTileToChar(InteriorTile t) {
    switch (t) {
        case InteriorTile::Solid: return '#';
        case InteriorTile::Platform: return '=';
        case InteriorTile::Hazard: return '^';
        case InteriorTile::Water: return '~';
        case InteriorTile::Breakable: return 'b';
        case InteriorTile::Ladder: return 'H';
        default: return '.';
    }
}

/**
 * Parse a saved row character. Unknown characters read as empty.
 * @param c Character from a saved row
 * @return Tile type
 */
inline InteriorTile InteriorTileFromChar(char c) {
    switch (c) {
        case '#': return InteriorTile::Solid;
        case '=': return InteriorTile::Platform;
        case '^': return InteriorTile::Hazard;
        case '~': return InteriorTile::Water;
        case 'b': return InteriorTile::Breakable;
        case 'H': return InteriorTile::Ladder;
        default: return InteriorTile::Empty;
    }
}

// Interior sub-grid size per map cell.
struct InteriorConfig {
    int width = 16;
    int height = 9;
};

// One cell's interior. tiles holds width*height entries, stored in
// global (Y-up) order: index = localY * width + localX, localY = 0 is
// the cell's south row. Saved files write rows top (north) first.
struct InteriorChunk {
    std::vector<uint8_t> tiles;

    bool IsEmpty() const {
        for (uint8_t t : tiles) {
            if (t != 0) return false;
        }
        return true;
    }
};

}  // namespace Cartograph
