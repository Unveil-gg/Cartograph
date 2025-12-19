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

class Model {
public:
    Model();
    
    // Core data
    GridConfig grid;
    Palette palette;
    std::vector<Room> rooms;          // Named rooms (metadata)
    std::vector<RegionGroup> regionGroups;  // User-defined region groups
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
    
    // Settings
    bool autoGenerateRoomWalls = true;  // Auto-generate walls around rooms
    
    // Computed/cached data
    std::vector<Region> inferredRegions;  // Auto-computed from walls
    bool regionsDirty = true;             // Needs recomputation?
    
    // Dirty tracking
    bool dirty = false;
    void MarkDirty();
    void ClearDirty();
    
    // Helper queries (non-owning pointers, valid until rooms modified)
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
    
    // Get cell "paint state" for detection (combines room ID and tile ID)
    // Returns: room ID if assigned, "tile_N" if has tile, "" if empty
    std::string GetCellPaintState(int x, int y) const;
    
    // Palette management
    int AddPaletteColor(const std::string& name, const Color& color);
    bool RemovePaletteColor(int tileId);
    bool UpdatePaletteColor(int tileId, const std::string& name, 
                           const Color& color);
    TileType* FindPaletteEntry(int tileId);
    const TileType* FindPaletteEntry(int tileId) const;
    bool IsPaletteColorInUse(int tileId) const;
    int GetNextPaletteTileId() const;
    
    // Marker queries (non-owning pointers, valid until markers modified)
    Marker* FindMarker(const std::string& id);
    const Marker* FindMarker(const std::string& id) const;
    Marker* FindMarkerNear(float x, float y, float tolerance);
    std::vector<Marker*> FindMarkersInRect(
        float minX, float minY, float maxX, float maxY
    );
    void AddMarker(const Marker& marker);
    bool RemoveMarker(const std::string& id);
    std::string GenerateMarkerId();
    
    // Update marker icon references
    int UpdateMarkerIconNames(const std::string& oldName, 
                              const std::string& newName);
    
    // Icon usage queries
    int CountMarkersUsingIcon(const std::string& iconName) const;
    std::vector<std::string> GetMarkersUsingIcon(
        const std::string& iconName
    ) const;
    int RemoveMarkersUsingIcon(const std::string& iconName);
    
    // Content bounds calculation for export
    ContentBounds CalculateContentBounds() const;
    
    // Room management
    RegionGroup* FindRegionGroup(const std::string& id);
    const RegionGroup* FindRegionGroup(const std::string& id) const;
    std::string GenerateRoomId();           // Generate unique room UUID
    std::string GenerateRegionGroupId();    // Generate unique region UUID
    
    // Room cell cache management
    void InvalidateRoomCellCache(const std::string& roomId);
    void InvalidateAllRoomCellCaches();
    void UpdateRoomCellCache(Room& room);
    const std::unordered_set<std::pair<int, int>, PairHash>& 
        GetRoomCells(const std::string& roomId);
    
    // Room detection (from enclosed areas)
    struct DetectedRoom {
        std::unordered_set<std::pair<int, int>, PairHash> cells;
        Rect boundingBox;
        bool isEnclosed;
    };
    DetectedRoom DetectEnclosedRoom(int x, int y);  // Detect room at cell
    std::vector<DetectedRoom> DetectAllEnclosedRooms();  // Detect all
    DetectedRoom DetectColorRegion(int x, int y);  // Detect by color
    std::vector<DetectedRoom> DetectAllColorRegions();  // Detect all colored
    Room CreateRoomFromCells(
        const std::unordered_set<std::pair<int, int>, PairHash>& cells,
        const std::string& name = "",
        bool generateWalls = true
    );
    
    // Room validation and splitting
    std::vector<std::unordered_set<std::pair<int, int>, PairHash>>
        FindContiguousRegions(
            const std::unordered_set<std::pair<int, int>, PairHash>& cells
        );  // Find contiguous regions within a set of cells
    int SplitDisconnectedRooms();  // Split rooms with disconnected cells
    
    // Room wall generation
    void GenerateRoomPerimeterWalls(const std::string& roomId);
    void GenerateRoomPerimeterWalls(
        const std::unordered_set<std::pair<int, int>, PairHash>& cells
    );
    
    /**
     * Compute wall changes for room perimeter without applying.
     * @param roomId Room to compute walls for
     * @return Vector of (EdgeId, oldState, newState) tuples
     */
    std::vector<std::tuple<EdgeId, EdgeState, EdgeState>> 
        ComputeRoomPerimeterWallChanges(const std::string& roomId);
    
    // Room connection detection (via doors)
    void UpdateRoomConnections();           // Update all room connections
    void UpdateRoomConnections(const std::string& roomId);  // Update one
    void InvalidateRoomConnections();       // Mark all as needing recompute
    
    // Color generation for rooms
    Color GenerateDistinctRoomColor() const;  // Generate distinct color
    
    // Initialize defaults
    void InitDefaults();
    void InitDefaultPalette();
    void InitDefaultKeymap();
    void InitDefaultTheme(const std::string& name = "Dark");
};

} // namespace Cartograph

