#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

// Forward declarations
struct ImVec4;

namespace Cartograph {

// ============================================================================
// Color utilities
// ============================================================================

struct Color {
    float r, g, b, a;
    
    Color() : r(0), g(0), b(0), a(1) {}
    Color(float r, float g, float b, float a = 1.0f) 
        : r(r), g(g), b(b), a(a) {}
    
    // Parse from hex string "#RRGGBB" or "#RRGGBBAA"
    static Color FromHex(const std::string& hex);
    
    // Convert to hex string
    std::string ToHex(bool includeAlpha = true) const;
    
    // Convert to ImVec4
    ImVec4 ToImVec4() const;
    
    uint32_t ToU32() const;
};

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
    int cols = 256;       // grid width in tiles
    int rows = 256;       // grid height in tiles
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
// Rooms (Named regions with metadata)
// ============================================================================

struct Room {
    std::string id;
    std::string name;
    int regionId;                     // Links to inferred region (-1 if none)
    Color color;
    std::string notes;
    std::vector<std::string> imageAttachments;  // Design doc images
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

// Hash function for std::pair<int, int> (for cell coordinates)
struct PairHash {
    std::size_t operator()(const std::pair<int, int>& p) const {
        return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
    }
};

// Helper to create EdgeId from cell and side
inline EdgeId MakeEdgeId(int cellX, int cellY, EdgeSide side) {
    switch (side) {
        case EdgeSide::North: return EdgeId(cellX, cellY, cellX, cellY - 1);
        case EdgeSide::South: return EdgeId(cellX, cellY, cellX, cellY + 1);
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
    float scale = 1.0f; // User-adjustable scaling multiplier
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
// Theme
// ============================================================================

struct Theme {
    std::string name;
    float uiScale = 1.0f;
    std::unordered_map<std::string, Color> mapColors;  // Override defaults
    
    // Predefined theme colors (set by ApplyTheme)
    Color background;
    Color gridLine;
    Color roomOutline;
    Color roomFill;
    Color wallColor;       // Solid wall lines
    Color doorColor;       // Door (dashed) lines
    Color edgeHoverColor;  // Edge hover highlight
    Color markerColor;
    Color textColor;
};

// ============================================================================
// Metadata
// ============================================================================

struct Metadata {
    std::string title;
    std::string author;
};

// ============================================================================
// Model - Complete project state
// ============================================================================

class Model {
public:
    Model();
    
    // Core data
    GridConfig grid;
    Palette palette;
    std::vector<Room> rooms;          // Named rooms (metadata)
    TileData tiles;
    std::unordered_map<EdgeId, EdgeState, EdgeIdHash> edges;  // Cell edges
    std::vector<Door> doors;          // Legacy door connections
    std::vector<Marker> markers;
    Keymap keymap;
    Theme theme;
    Metadata meta;
    
    // Cell-to-room assignments (manual room painting)
    std::unordered_map<std::pair<int, int>, std::string, 
        PairHash> cellRoomAssignments;
    
    // Computed/cached data
    std::vector<Region> inferredRegions;  // Auto-computed from walls
    bool regionsDirty = true;             // Needs recomputation?
    
    // Dirty tracking
    bool dirty = false;
    void MarkDirty();
    void ClearDirty();
    
    // Helper queries
    Room* FindRoom(const std::string& id);
    const Room* FindRoom(const std::string& id) const;
    TileRow* FindTileRow(const std::string& roomId, int y);
    int GetTileAt(const std::string& roomId, int x, int y) const;
    void SetTileAt(const std::string& roomId, int x, int y, int tileId);
    bool HasDoorAt(const std::string& roomId, int x, int y) const;
    
    // Edge queries
    EdgeState GetEdgeState(const EdgeId& edgeId) const;
    void SetEdgeState(const EdgeId& edgeId, EdgeState state);
    EdgeState CycleEdgeState(EdgeState current);  // Cycle through states
    
    // Grid expansion
    void ExpandGridIfNeeded(int cellX, int cellY);
    
    // Grid preset management
    bool CanChangeGridPreset() const;  // Check if preset can be changed (no markers)
    void ApplyGridPreset(GridPreset preset);  // Apply preset and update dimensions
    std::vector<std::pair<float, float>> GetMarkerSnapPoints() const;  // Get snap points for current preset
    
    // Region management
    void ComputeInferredRegions();    // Flood-fill to detect regions
    void InvalidateRegions();         // Mark regions as needing recompute
    const std::vector<Region>& GetRegions();  // Get cached regions
    Region* FindRegionAt(int x, int y);  // Which region contains cell?
    
    // Auto-wall generation
    void GenerateRegionPerimeterWalls(const Region& region);
    void UpdateAllAutoWalls();        // Regenerate walls for all regions
    
    // Cell-room assignment (manual room painting)
    std::string GetCellRoom(int x, int y) const;  // Get room at cell
    void SetCellRoom(int x, int y, const std::string& roomId);
    void ClearCellRoom(int x, int y);  // Remove room assignment
    void ClearAllCellsForRoom(const std::string& roomId);  // Clear all cells
    
    // Marker queries
    Marker* FindMarker(const std::string& id);
    const Marker* FindMarker(const std::string& id) const;
    Marker* FindMarkerNear(float x, float y, float tolerance);
    std::vector<Marker*> FindMarkersInRect(
        float minX, float minY, float maxX, float maxY
    );
    void AddMarker(const Marker& marker);
    bool RemoveMarker(const std::string& id);
    std::string GenerateMarkerId();
    
    // Initialize defaults
    void InitDefaults();
    void InitDefaultPalette();
    void InitDefaultKeymap();
    void InitDefaultTheme(const std::string& name = "Dark");
};

} // namespace Cartograph

