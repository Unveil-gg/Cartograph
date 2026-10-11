#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

#include "Color.h"
#include "Theme/Themes.h"

namespace Cartograph {

// ============================================================================
// Grid configuration
// ============================================================================

// Grid preset types determine cell dimensions and marker snap behavior
enum class GridPreset {
    Square,      // 16×16 - Center snap only (top-down games)
    Rectangle    // 32×16 - Dual snap left/right (side-scrollers)
};

struct GridConfig {
    GridPreset preset = GridPreset::Square;  // Cell type preset
    int tileWidth = 16;   // pixels per tile width at 1:1 zoom (derived from preset)
    int tileHeight = 16;  // pixels per tile height at 1:1 zoom (derived from preset)
    int cols = 256;       // grid width in tiles (max X bound)
    int rows = 256;       // grid height in tiles (max Y bound)
    int minCol = 0;       // min X bound (can be negative)
    int minRow = 0;       // min Y bound (can be negative)
    bool locked = false;  // Prevent preset changes after markers placed
    
    // Edge configuration
    bool autoExpandGrid = true;       // Auto-expand when near boundaries
    int expansionThreshold = 3;       // cells from edge to trigger expansion
    float expansionFactor = 1.5f;     // 50% growth
    float edgeHoverThreshold = 0.2f;  // 20% of cell size
};

// ============================================================================
// Palette
// ============================================================================

struct TileType {
    int id;
    std::string name;
    Color color;
};

using Palette = std::vector<TileType>;

// ============================================================================
// Hash functions
// ============================================================================

// Hash function for std::pair<int, int> (for cell coordinates)
struct PairHash {
    std::size_t operator()(const std::pair<int, int>& p) const {
        return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
    }
};

// ============================================================================
// Regions (Inferred from walls)
// ============================================================================

struct Rect {
    int x, y, w, h;  // All in tile coordinates
    
    bool Contains(int tx, int ty) const {
        return tx >= x && tx < x + w && ty >= y && ty < y + h;
    }
};

struct Region {
    int id;                           // Auto-generated region ID
    std::vector<std::pair<int, int>> cells;  // List of (x, y) cells in region
    Rect boundingBox;                 // Bounding rectangle for quick checks
    
    bool Contains(int x, int y) const {
        for (const auto& cell : cells) {
            if (cell.first == x && cell.second == y) {
                return true;
            }
        }
        return false;
    }
};

// ============================================================================
// Region Groups (User-defined groupings of rooms)
// ============================================================================

struct RegionGroup {
    std::string id;                        // UUID
    std::string name;
    std::string description;
    std::vector<std::string> tags;
    std::vector<std::string> roomIds;      // Child rooms in this group
};

// ============================================================================
// Rooms (Named regions with metadata)
// ============================================================================

struct Room {
    std::string id;
    std::string name;
    int regionId;                          // Links to inferred region 
                                           // (-1 if none)
    std::string parentRegionGroupId;       // User-defined region group 
                                           // (empty if unassigned)
    Color color;
    std::string notes;
    std::vector<std::string> tags;         // User tags for categorization
    std::vector<std::string> imageAttachments;  // Design doc images
    
    // Cache for performance (computed from cellRoomAssignments)
    std::unordered_set<std::pair<int, int>, PairHash> cells;
    bool cellsCacheDirty = true;           // Needs recomputation?
    
    // Auto-detected connections via doors
    std::vector<std::string> connectedRoomIds;
    bool connectionsDirty = true;          // Needs recomputation?
};

// ============================================================================
// Tiles (row-run encoding)
// ============================================================================

struct TileRun {
    int startX;
    int count;
    int tileId;
};

struct TileRow {
    std::string roomId;
    int y;  // Row index within room (0 = top)
    std::vector<TileRun> runs;
};

using TileData = std::vector<TileRow>;

// ============================================================================
// Edges (Walls & Doors)
// ============================================================================

// Edge state for cell boundaries
enum class EdgeState {
    None = 0,   // No edge (empty)
    Wall = 1,   // Solid wall
    Door = 2    // Door (dashed line)
};

// Edge orientation relative to a cell
enum class EdgeSide {
    North,  // Top edge
    South,  // Bottom edge
    East,   // Right edge
    West    // Left edge
};

// Edge identifier (shared between adjacent cells)
struct EdgeId {
    int x1, y1;  // First cell position
    int x2, y2;  // Second cell position (adjacent)
    
    EdgeId() : x1(0), y1(0), x2(0), y2(0) {}
    EdgeId(int x1, int y1, int x2, int y2) 
        : x1(x1), y1(y1), x2(x2), y2(y2) {
        // Normalize so x1,y1 is always "smaller" for consistent hashing
        if (x1 > x2 || (x1 == x2 && y1 > y2)) {
            std::swap(this->x1, this->x2);
            std::swap(this->y1, this->y2);
        }
    }
    
    bool operator==(const EdgeId& other) const {
        return x1 == other.x1 && y1 == other.y1 && 
               x2 == other.x2 && y2 == other.y2;
    }
};

// Hash function for EdgeId
struct EdgeIdHash {
    std::size_t operator()(const EdgeId& e) const {
        // Simple hash combining all coordinates
        return std::hash<int>()(e.x1) ^ 
               (std::hash<int>()(e.y1) << 1) ^
               (std::hash<int>()(e.x2) << 2) ^
               (std::hash<int>()(e.y2) << 3);
    }
};

// Helper to create EdgeId from cell and side (Y-up coordinate system)
inline EdgeId MakeEdgeId(int cellX, int cellY, EdgeSide side) {
    switch (side) {
        // Y-up: North (up) goes to higher Y, South (down) goes to lower Y
        case EdgeSide::North: return EdgeId(cellX, cellY, cellX, cellY + 1);
        case EdgeSide::South: return EdgeId(cellX, cellY, cellX, cellY - 1);
        case EdgeSide::East:  return EdgeId(cellX, cellY, cellX + 1, cellY);
        case EdgeSide::West:  return EdgeId(cellX, cellY, cellX - 1, cellY);
    }
    return EdgeId();
}

// ============================================================================
// Doors (Legacy - kept for compatibility)
// ============================================================================

enum class DoorSide {
    North, South, East, West
};

enum class DoorType {
    Door,       // Regular door
    Elevator,   // Vertical transport
    Teleporter  // Long-distance warp
};

struct DoorEndpoint {
    std::string roomId;
    int x, y;  // Tile position
    DoorSide side;
};

struct Door {
    std::string id;
    DoorEndpoint a;
    DoorEndpoint b;
    DoorType type;
    std::vector<std::string> gate;  // Required abilities (not logic yet)
};

// ============================================================================
// Markers
// ============================================================================

struct Marker {
    std::string id;
    std::string roomId;
    float x, y;         // Sub-tile position (e.g., 5.5 = centered in tile 5)
    std::string kind;   // "save", "item", "boss", etc.
    std::string label;
    std::string icon;   // Icon name reference
    Color color;
    float size = 0.6f;  // Size as fraction of tile (0.6 = 60%)
    bool showLabel = true;
};

// ============================================================================
// Keymap
// ============================================================================

struct KeyBinding {
    std::string action;
    std::string binding;  // e.g., "Ctrl+S", "Mouse1"
};

using Keymap = std::unordered_map<std::string, std::string>;

// ============================================================================
// Metadata
// ============================================================================

struct Metadata {
    std::string title;
    std::string author;
    std::string description;
};

// ============================================================================
// Content bounds for export
// ============================================================================

struct ContentBounds {
    int minX, minY, maxX, maxY;
    bool isEmpty;
};

// ============================================================================
// Selection data (for Select tool)
// ============================================================================

/**
 * Holds selected canvas content for copy/paste/delete operations.
 * Coordinates are stored as tile positions.
 */
struct SelectionData {
    // Selected tiles: position -> tile ID
    std::unordered_map<std::pair<int, int>, int, PairHash> tiles;
    
    // Selected edges: edge ID -> edge state
    std::unordered_map<EdgeId, EdgeState, EdgeIdHash> edges;
    
    // Selected marker IDs
    std::vector<std::string> markerIds;
    
    // Bounding box of selection (tile coordinates)
    Rect bounds;
    
    // Check if selection is empty
    bool IsEmpty() const {
        return tiles.empty() && edges.empty() && markerIds.empty();
    }
    
    // Clear all selection data
    void Clear() {
        tiles.clear();
        edges.clear();
        markerIds.clear();
        bounds = {0, 0, 0, 0};
    }
    
    // Get counts for status display
    int TileCount() const { return static_cast<int>(tiles.size()); }
    int EdgeCount() const { return static_cast<int>(edges.size()); }
    int MarkerCount() const { return static_cast<int>(markerIds.size()); }
};

// ============================================================================
// Clipboard data (for copy/paste operations)
// ============================================================================

/**
 * Holds copied content for paste operations.
 * Positions are stored relative to the selection origin (top-left).
 */
struct ClipboardData {
    // Copied tiles: relative position (dx, dy) -> tile ID
    std::unordered_map<std::pair<int, int>, int, PairHash> tiles;
    
    // Copied edges: stored with relative positions
    struct RelativeEdge {
        int dx1, dy1, dx2, dy2;  // Relative to origin
        EdgeState state;
    };
    std::vector<RelativeEdge> edges;
    
    // Copied markers: full marker data with relative positions
    struct RelativeMarker {
        float dx, dy;           // Relative to origin
        std::string kind;
        std::string label;
        std::string icon;
        Color color;
        float size;
        bool showLabel;
    };
    std::vector<RelativeMarker> markers;
    
    // Original bounds width/height (for preview)
    int width = 0;
    int height = 0;
    
    // Check if clipboard is empty
    bool IsEmpty() const {
        return tiles.empty() && edges.empty() && markers.empty();
    }
    
    // Clear clipboard
    void Clear() {
        tiles.clear();
        edges.clear();
        markers.clear();
        width = 0;
        height = 0;
    }
};

// ============================================================================
// Model - Complete project state
// ============================================================================


}  // namespace Cartograph
