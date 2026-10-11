#pragma once

#include "ModelTypes.h"

namespace Cartograph {

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

