#include "Model.h"
#include <imgui.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <set>
#include <unordered_set>
#include <chrono>
#include <cmath>

namespace Cartograph {

// ============================================================================
// Color implementation
// ============================================================================

Color Color::FromHex(const std::string& hex) {
    if (hex.empty() || hex[0] != '#') {
        return Color(0, 0, 0, 1);
    }
    
    std::string str = hex.substr(1);
    uint32_t val = 0;
    sscanf(str.c_str(), "%x", &val);
    
    if (str.length() == 6) {
        // #RRGGBB
        return Color(
            ((val >> 16) & 0xFF) / 255.0f,
            ((val >> 8) & 0xFF) / 255.0f,
            (val & 0xFF) / 255.0f,
            1.0f
        );
    } else if (str.length() == 8) {
        // #RRGGBBAA
        return Color(
            ((val >> 24) & 0xFF) / 255.0f,
            ((val >> 16) & 0xFF) / 255.0f,
            ((val >> 8) & 0xFF) / 255.0f,
            (val & 0xFF) / 255.0f
        );
    }
    
    return Color(0, 0, 0, 1);
}

std::string Color::ToHex(bool includeAlpha) const {
    char buf[16];
    if (includeAlpha) {
        snprintf(buf, sizeof(buf), "#%02x%02x%02x%02x",
            (int)(r * 255), (int)(g * 255), 
            (int)(b * 255), (int)(a * 255));
    } else {
        snprintf(buf, sizeof(buf), "#%02x%02x%02x",
            (int)(r * 255), (int)(g * 255), (int)(b * 255));
    }
    return buf;
}

ImVec4 Color::ToImVec4() const {
    return ImVec4(r, g, b, a);
}

uint32_t Color::ToU32() const {
    return IM_COL32(
        (int)(r * 255), 
        (int)(g * 255), 
        (int)(b * 255), 
        (int)(a * 255)
    );
}

// ============================================================================
// Model implementation
// ============================================================================

Model::Model() {
    InitDefaults();
}

void Model::MarkDirty() {
    dirty = true;
}

void Model::ClearDirty() {
    dirty = false;
}

Room* Model::FindRoom(const std::string& id) {
    auto it = std::find_if(rooms.begin(), rooms.end(),
        [&id](const Room& r) { return r.id == id; });
    return it != rooms.end() ? &(*it) : nullptr;
}

const Room* Model::FindRoom(const std::string& id) const {
    auto it = std::find_if(rooms.begin(), rooms.end(),
        [&id](const Room& r) { return r.id == id; });
    return it != rooms.end() ? &(*it) : nullptr;
}

TileRow* Model::FindTileRow(const std::string& roomId, int y) {
    auto it = std::find_if(tiles.begin(), tiles.end(),
        [&](const TileRow& row) {
            return row.roomId == roomId && row.y == y;
        });
    return it != tiles.end() ? &(*it) : nullptr;
}

int Model::GetTileAt(const std::string& roomId, int x, int y) const {
    auto it = std::find_if(tiles.begin(), tiles.end(),
        [&](const TileRow& row) {
            return row.roomId == roomId && row.y == y;
        });
    
    if (it == tiles.end()) {
        return 0;  // Empty
    }
    
    for (const auto& run : it->runs) {
        if (x >= run.startX && x < run.startX + run.count) {
            return run.tileId;
        }
    }
    
    return 0;
}

void Model::SetTileAt(const std::string& roomId, int x, int y, int tileId) {
    // Find or create row
    TileRow* row = FindTileRow(roomId, y);
    if (!row) {
        tiles.push_back({roomId, y, {}});
        row = &tiles.back();
    }
    
    // Properly handle run-length encoding insertion/updates
    std::vector<TileRun> newRuns;
    bool inserted = false;
    
    for (const auto& run : row->runs) {
        int runEnd = run.startX + run.count;
        
        if (x < run.startX) {
            // Insert before this run
            if (!inserted && tileId != 0) {
                newRuns.push_back({x, 1, tileId});
                inserted = true;
            }
            newRuns.push_back(run);
        } else if (x >= run.startX && x < runEnd) {
            // Overlaps with this run - need to split/modify
            if (run.tileId == tileId) {
                // Same tile, keep the run as is
                newRuns.push_back(run);
                inserted = true;
            } else {
                // Different tile, need to split the run
                if (x > run.startX) {
                    // Add part before x
                    newRuns.push_back({run.startX, x - run.startX, 
                                      run.tileId});
                }
                if (tileId != 0) {
                    // Add new tile at x
                    newRuns.push_back({x, 1, tileId});
                }
                if (x + 1 < runEnd) {
                    // Add part after x
                    newRuns.push_back({x + 1, runEnd - (x + 1), run.tileId});
                }
                inserted = true;
            }
        } else {
            // After this run
            newRuns.push_back(run);
        }
    }
    
    // If not inserted yet, add at the end
    if (!inserted && tileId != 0) {
        newRuns.push_back({x, 1, tileId});
    }
    
    row->runs = newRuns;
    
    // Coalesce adjacent runs with same tile ID
    if (!row->runs.empty()) {
        std::vector<TileRun> coalesced;
        coalesced.push_back(row->runs[0]);
        
        for (size_t i = 1; i < row->runs.size(); ++i) {
            TileRun& last = coalesced.back();
            const TileRun& current = row->runs[i];
            
            if (last.tileId == current.tileId && 
                last.startX + last.count == current.startX) {
                // Merge with previous run
                last.count += current.count;
            } else {
                coalesced.push_back(current);
            }
        }
        
        row->runs = coalesced;
    }
    
    MarkDirty();
}

bool Model::HasDoorAt(const std::string& roomId, int x, int y) const {
    // Check if any door endpoint is at this position
    for (const auto& door : doors) {
        if (door.a.roomId == roomId && door.a.x == x && door.a.y == y) {
            return true;
        }
        if (door.b.roomId == roomId && door.b.x == x && door.b.y == y) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Palette management
// ============================================================================

int Model::AddPaletteColor(const std::string& name, const Color& color) {
    // Check palette size limit (max 32 colors including Empty)
    const size_t MAX_PALETTE_SIZE = 32;
    if (palette.size() >= MAX_PALETTE_SIZE) {
        return -1;  // Palette full
    }
    
    // Get next available tile ID
    int newId = GetNextPaletteTileId();
    
    // Add new tile type to palette
    palette.push_back({newId, name, color});
    MarkDirty();
    
    return newId;
}

bool Model::RemovePaletteColor(int tileId) {
    // Can't remove Empty (id=0) - it's protected
    if (tileId == 0) {
        return false;
    }
    
    // Find tile in palette
    auto it = std::find_if(palette.begin(), palette.end(),
        [tileId](const TileType& t) { return t.id == tileId; });
    
    if (it == palette.end()) {
        return false;  // Not found
    }
    
    // Remove from palette
    palette.erase(it);
    MarkDirty();
    
    return true;
}

bool Model::UpdatePaletteColor(
    int tileId, 
    const std::string& name, 
    const Color& color
) {
    TileType* tile = FindPaletteEntry(tileId);
    if (!tile) {
        return false;
    }
    
    tile->name = name;
    tile->color = color;
    MarkDirty();
    
    return true;
}

TileType* Model::FindPaletteEntry(int tileId) {
    auto it = std::find_if(palette.begin(), palette.end(),
        [tileId](const TileType& t) { return t.id == tileId; });
    return it != palette.end() ? &(*it) : nullptr;
}

const TileType* Model::FindPaletteEntry(int tileId) const {
    auto it = std::find_if(palette.begin(), palette.end(),
        [tileId](const TileType& t) { return t.id == tileId; });
    return it != palette.end() ? &(*it) : nullptr;
}

bool Model::IsPaletteColorInUse(int tileId) const {
    // Check all tile rows for any use of this tile ID
    for (const auto& row : tiles) {
        for (const auto& run : row.runs) {
            if (run.tileId == tileId) {
                return true;
            }
        }
    }
    return false;
}

int Model::GetNextPaletteTileId() const {
    // Find highest existing ID and add 1
    int maxId = 0;
    for (const auto& tile : palette) {
        if (tile.id > maxId) {
            maxId = tile.id;
        }
    }
    return maxId + 1;
}

EdgeState Model::GetEdgeState(const EdgeId& edgeId) const {
    auto it = edges.find(edgeId);
    return (it != edges.end()) ? it->second : EdgeState::None;
}

void Model::SetEdgeState(const EdgeId& edgeId, EdgeState state) {
    if (state == EdgeState::None) {
        // Remove from map if state is None
        edges.erase(edgeId);
    } else {
        edges[edgeId] = state;
    }
    
    // Walls change region boundaries - invalidate cache
    InvalidateRegions();
    
    // Doors change room connections - invalidate
    InvalidateRoomConnections();
    
    MarkDirty();
}

EdgeState Model::CycleEdgeState(EdgeState current) {
    // Cycle: None -> Wall -> Door -> None
    switch (current) {
        case EdgeState::None: return EdgeState::Wall;
        case EdgeState::Wall: return EdgeState::Door;
        case EdgeState::Door: return EdgeState::None;
    }
    return EdgeState::None;
}

void Model::ExpandGridIfNeeded(int cellX, int cellY) {
    if (!grid.autoExpandGrid) {
        return;
    }
    
    // Check if we're near any boundary
    bool needsExpansion = false;
    int threshold = grid.expansionThreshold;
    
    if (cellX < threshold || cellX >= grid.cols - threshold ||
        cellY < threshold || cellY >= grid.rows - threshold) {
        needsExpansion = true;
    }
    
    if (needsExpansion) {
        int newCols = static_cast<int>(grid.cols * grid.expansionFactor);
        int newRows = static_cast<int>(grid.rows * grid.expansionFactor);
        
        // Ensure minimum expansion
        if (newCols <= grid.cols) newCols = grid.cols + 64;
        if (newRows <= grid.rows) newRows = grid.rows + 64;
        
        grid.cols = newCols;
        grid.rows = newRows;
        MarkDirty();
    }
}

bool Model::CanChangeGridPreset() const {
    // Can't change preset if markers exist
    return markers.empty();
}

void Model::ApplyGridPreset(GridPreset preset) {
    if (!CanChangeGridPreset()) {
        // Caller should check first, but guard anyway
        return;
    }
    
    grid.preset = preset;
    
    // Update dimensions based on preset
    switch (preset) {
        case GridPreset::Square:
            grid.tileWidth = 16;
            grid.tileHeight = 16;
            break;
        case GridPreset::Rectangle:
            grid.tileWidth = 32;
            grid.tileHeight = 16;
            break;
    }
    
    MarkDirty();
}

std::vector<std::pair<float, float>> Model::GetMarkerSnapPoints() const {
    switch (grid.preset) {
        case GridPreset::Square:
            return {{0.5f, 0.5f}};  // Center only
        case GridPreset::Rectangle:
            return {{0.25f, 0.5f}, {0.75f, 0.5f}};  // Left & right centers
    }
    return {{0.5f, 0.5f}};  // Fallback to center
}

void Model::InvalidateRegions() {
    regionsDirty = true;
}

const std::vector<Region>& Model::GetRegions() {
    if (regionsDirty) {
        ComputeInferredRegions();
    }
    return inferredRegions;
}

void Model::ComputeInferredRegions() {
    inferredRegions.clear();
    
    // Track which cells have been visited
    std::vector<std::vector<bool>> visited(
        grid.rows, std::vector<bool>(grid.cols, false)
    );
    
    int nextRegionId = 0;
    
    // Flood-fill from each unvisited cell
    for (int y = 0; y < grid.rows; ++y) {
        for (int x = 0; x < grid.cols; ++x) {
            if (visited[y][x]) continue;
            
            // Start a new region
            Region region;
            region.id = nextRegionId++;
            region.boundingBox = {x, y, 0, 0};
            
            // Flood-fill to find all connected cells
            std::vector<std::pair<int, int>> stack;
            stack.push_back({x, y});
            
            int minX = x, minY = y, maxX = x, maxY = y;
            
            while (!stack.empty()) {
                auto [cx, cy] = stack.back();
                stack.pop_back();
                
                // Skip if out of bounds or already visited
                if (cx < 0 || cx >= grid.cols || 
                    cy < 0 || cy >= grid.rows || 
                    visited[cy][cx]) {
                    continue;
                }
                
                // Mark as visited
                visited[cy][cx] = true;
                region.cells.push_back({cx, cy});
                
                // Update bounding box
                minX = std::min(minX, cx);
                minY = std::min(minY, cy);
                maxX = std::max(maxX, cx);
                maxY = std::max(maxY, cy);
                
                // Check all 4 neighbors - only cross if no wall
                EdgeSide sides[] = {
                    EdgeSide::North, EdgeSide::South,
                    EdgeSide::East, EdgeSide::West
                };
                int dx[] = {0, 0, 1, -1};
                int dy[] = {-1, 1, 0, 0};
                
                for (int i = 0; i < 4; ++i) {
                    EdgeId edgeId = MakeEdgeId(cx, cy, sides[i]);
                    EdgeState state = GetEdgeState(edgeId);
                    
                    // Can only cross if edge is None or Door
                    // Wall blocks region boundary
                    if (state == EdgeState::None || state == EdgeState::Door) {
                        int nx = cx + dx[i];
                        int ny = cy + dy[i];
                        
                        if (nx >= 0 && nx < grid.cols && 
                            ny >= 0 && ny < grid.rows &&
                            !visited[ny][nx]) {
                            stack.push_back({nx, ny});
                        }
                    }
                }
            }
            
            // Set bounding box
            region.boundingBox.x = minX;
            region.boundingBox.y = minY;
            region.boundingBox.w = maxX - minX + 1;
            region.boundingBox.h = maxY - minY + 1;
            
            inferredRegions.push_back(region);
        }
    }
    
    regionsDirty = false;
}

Region* Model::FindRegionAt(int x, int y) {
    if (regionsDirty) {
        ComputeInferredRegions();
    }
    
    for (auto& region : inferredRegions) {
        if (region.Contains(x, y)) {
            return &region;
        }
    }
    return nullptr;
}

void Model::GenerateRegionPerimeterWalls(const Region& region) {
    // Build a set for fast lookup (O(log n) vs O(n))
    std::set<std::pair<int, int>> cellSet(
        region.cells.begin(), region.cells.end()
    );
    
    // For each cell in the region, check all 4 edges
    for (const auto& cell : region.cells) {
        int cx = cell.first;
        int cy = cell.second;
        
        EdgeSide sides[] = {
            EdgeSide::North, EdgeSide::South, 
            EdgeSide::East, EdgeSide::West
        };
        
        for (EdgeSide side : sides) {
            EdgeId edgeId = MakeEdgeId(cx, cy, side);
            
            // Determine adjacent cell position
            int adjX = cx;
            int adjY = cy;
            switch (side) {
                case EdgeSide::North: adjY = cy - 1; break;
                case EdgeSide::South: adjY = cy + 1; break;
                case EdgeSide::East:  adjX = cx + 1; break;
                case EdgeSide::West:  adjX = cx - 1; break;
            }
            
            // Check if adjacent cell is in the same region (fast lookup)
            bool sameRegion = cellSet.count({adjX, adjY}) > 0;
            
            // If adjacent is NOT in same region, add wall
            // (unless there's already an edge there)
            if (!sameRegion) {
                EdgeState currentState = GetEdgeState(edgeId);
                if (currentState == EdgeState::None) {
                    SetEdgeState(edgeId, EdgeState::Wall);
                }
            }
        }
    }
}

void Model::UpdateAllAutoWalls() {
    // Ensure regions are computed
    if (regionsDirty) {
        ComputeInferredRegions();
    }
    
    // Generate walls for each region (skip very large regions to avoid freeze)
    const size_t MAX_REGION_SIZE = 10000;  // Safety limit
    
    for (const auto& region : inferredRegions) {
        if (region.cells.size() > MAX_REGION_SIZE) {
            // Skip generating walls for huge regions (likely entire grid)
            continue;
        }
        GenerateRegionPerimeterWalls(region);
    }
}

std::string Model::GetCellRoom(int x, int y) const {
    auto it = cellRoomAssignments.find({x, y});
    if (it != cellRoomAssignments.end()) {
        return it->second;
    }
    return "";  // No room assigned
}

void Model::SetCellRoom(int x, int y, const std::string& roomId) {
    // Get old room ID to invalidate its cache
    std::string oldRoomId = GetCellRoom(x, y);
    
    if (roomId.empty()) {
        // Empty string means clear assignment
        cellRoomAssignments.erase({x, y});
    } else {
        cellRoomAssignments[{x, y}] = roomId;
    }
    
    // Invalidate caches for affected rooms
    if (!oldRoomId.empty()) {
        InvalidateRoomCellCache(oldRoomId);
    }
    if (!roomId.empty()) {
        InvalidateRoomCellCache(roomId);
    }
    
    MarkDirty();
}

void Model::ClearCellRoom(int x, int y) {
    std::string oldRoomId = GetCellRoom(x, y);
    cellRoomAssignments.erase({x, y});
    
    if (!oldRoomId.empty()) {
        InvalidateRoomCellCache(oldRoomId);
    }
    
    MarkDirty();
}

void Model::ClearAllCellsForRoom(const std::string& roomId) {
    // Remove all cells assigned to this room
    for (auto it = cellRoomAssignments.begin(); 
         it != cellRoomAssignments.end(); ) {
        if (it->second == roomId) {
            it = cellRoomAssignments.erase(it);
        } else {
            ++it;
        }
    }
    
    InvalidateRoomCellCache(roomId);
    MarkDirty();
}

// ============================================================================
// Marker queries
// ============================================================================

Marker* Model::FindMarker(const std::string& id) {
    auto it = std::find_if(markers.begin(), markers.end(),
        [&id](const Marker& m) { return m.id == id; });
    return it != markers.end() ? &(*it) : nullptr;
}

const Marker* Model::FindMarker(const std::string& id) const {
    auto it = std::find_if(markers.begin(), markers.end(),
        [&id](const Marker& m) { return m.id == id; });
    return it != markers.end() ? &(*it) : nullptr;
}

Marker* Model::FindMarkerNear(float x, float y, float tolerance) {
    float toleranceSq = tolerance * tolerance;
    
    for (auto& marker : markers) {
        float dx = marker.x - x;
        float dy = marker.y - y;
        float distSq = dx * dx + dy * dy;
        
        if (distSq <= toleranceSq) {
            return &marker;
        }
    }
    
    return nullptr;
}

std::vector<Marker*> Model::FindMarkersInRect(
    float minX, float minY, float maxX, float maxY
) {
    std::vector<Marker*> result;
    
    for (auto& marker : markers) {
        if (marker.x >= minX && marker.x <= maxX &&
            marker.y >= minY && marker.y <= maxY) {
            result.push_back(&marker);
        }
    }
    
    return result;
}

void Model::AddMarker(const Marker& marker) {
    markers.push_back(marker);
    grid.locked = true;  // Lock grid preset when markers are placed
    MarkDirty();
}

bool Model::RemoveMarker(const std::string& id) {
    auto it = std::find_if(markers.begin(), markers.end(),
        [&id](const Marker& m) { return m.id == id; });
    
    if (it != markers.end()) {
        markers.erase(it);
        MarkDirty();
        return true;
    }
    
    return false;
}

int Model::UpdateMarkerIconNames(
    const std::string& oldName,
    const std::string& newName
) {
    int count = 0;
    for (auto& marker : markers) {
        if (marker.icon == oldName) {
            marker.icon = newName;
            count++;
        }
    }
    
    if (count > 0) {
        MarkDirty();
    }
    
    return count;
}

int Model::CountMarkersUsingIcon(const std::string& iconName) const {
    int count = 0;
    for (const auto& marker : markers) {
        if (marker.icon == iconName) {
            count++;
        }
    }
    return count;
}

std::vector<std::string> Model::GetMarkersUsingIcon(
    const std::string& iconName
) const {
    std::vector<std::string> result;
    for (const auto& marker : markers) {
        if (marker.icon == iconName) {
            result.push_back(marker.id);
        }
    }
    return result;
}

int Model::RemoveMarkersUsingIcon(const std::string& iconName) {
    int count = 0;
    auto it = markers.begin();
    while (it != markers.end()) {
        if (it->icon == iconName) {
            it = markers.erase(it);
            count++;
        } else {
            ++it;
        }
    }
    
    if (count > 0) {
        MarkDirty();
    }
    
    return count;
}

std::string Model::GenerateMarkerId() {
    // Generate unique marker ID using timestamp + counter
    static int counter = 0;
    char buf[32];
    snprintf(buf, sizeof(buf), "marker_%d", counter++);
    return buf;
}

void Model::InitDefaults() {
    InitDefaultPalette();
    InitDefaultKeymap();
    InitDefaultTheme("Dark");
    
    // Set default grid preset (Square: 16×16)
    grid.preset = GridPreset::Square;
    grid.tileWidth = 16;
    grid.tileHeight = 16;
    grid.cols = 256;
    grid.rows = 256;
    grid.locked = false;
    
    meta.title = "New Map";
    meta.author = "";
}

void Model::InitDefaultPalette() {
    palette.clear();
    palette.push_back({0, "Empty", Color(0, 0, 0, 0)});
    palette.push_back({1, "Solid", Color::FromHex("#3a3a3a")});
    palette.push_back({2, "Hazard", Color::FromHex("#be3a34")});
    palette.push_back({3, "Water", Color::FromHex("#2a5a9a")});
    palette.push_back({4, "Breakable", Color::FromHex("#8b6914")});
}

void Model::InitDefaultKeymap() {
    keymap.clear();
    
    // Mouse-based actions (not rebindable via UI)
    keymap["paint"] = "Mouse1";  // Left click or single tap
    keymap["erase"] = "Mouse2";  // Right click or two-finger tap (trackpad)
    keymap["eraseAlt"] = "E+Mouse1";  // Hold E key + left click to erase
    
    // Edge actions (within Paint tool)
    keymap["placeWall"] = "W";   // Direct wall placement
    keymap["placeDoor"] = "D";   // Direct door placement
    
    // Tool switching shortcuts
    keymap["toolMove"] = "V";
    keymap["toolSelect"] = "S";
    keymap["toolPaint"] = "B";  // B for Brush
    keymap["toolErase"] = "E";
    keymap["toolFill"] = "F";
    keymap["toolMarker"] = "M";
    keymap["toolEyedropper"] = "I";
    keymap["toolZoom"] = "Z";
    keymap["toolRoomPaint"] = "Shift+R";
    keymap["toolRoomErase"] = "Shift+E";
    keymap["toolRoomFill"] = "Shift+F";
    
    // View
    keymap["pan"] = "Space+Drag";
    keymap["zoomIn"] = "=";
    keymap["zoomOut"] = "-";
    keymap["toggleGrid"] = "G";
    keymap["toggleRoomOverlays"] = "O";
    
    // Room actions
    keymap["detectRooms"] = "Shift+D";
    
    // Platform-specific shortcuts (File/Edit)
#ifdef __APPLE__
    keymap["new"] = "Cmd+N";
    keymap["open"] = "Cmd+O";
    keymap["save"] = "Cmd+S";
    keymap["saveAs"] = "Cmd+Shift+S";
    keymap["export"] = "Cmd+E";
    keymap["exportPackage"] = "Cmd+Shift+E";
    keymap["undo"] = "Cmd+Z";
    keymap["redo"] = "Cmd+Shift+Z";
    keymap["copy"] = "Cmd+C";
    keymap["paste"] = "Cmd+V";
    keymap["cut"] = "Cmd+X";
    keymap["togglePropertiesPanel"] = "Cmd+P";
#else
    keymap["new"] = "Ctrl+N";
    keymap["open"] = "Ctrl+O";
    keymap["save"] = "Ctrl+S";
    keymap["saveAs"] = "Ctrl+Shift+S";
    keymap["export"] = "Ctrl+E";
    keymap["exportPackage"] = "Ctrl+Shift+E";
    keymap["undo"] = "Ctrl+Z";
    keymap["redo"] = "Ctrl+Y";
    keymap["copy"] = "Ctrl+C";
    keymap["paste"] = "Ctrl+V";
    keymap["cut"] = "Ctrl+X";
    keymap["togglePropertiesPanel"] = "Ctrl+P";
#endif
    
    // Non-platform-specific edit actions
    keymap["delete"] = "Delete";
    keymap["deleteAlt"] = "Backspace";  // Alternative delete key
    keymap["selectAll"] = "Ctrl+A";     // Select all (works on both platforms)
}

void Model::InitDefaultTheme(const std::string& name) {
    theme.name = name;
    theme.uiScale = 1.0f;
    theme.mapColors.clear();
    
    if (name == "Dark") {
        theme.background = Color(0.1f, 0.1f, 0.1f, 1.0f);
        theme.gridLine = Color(0.2f, 0.2f, 0.2f, 1.0f);
        theme.roomOutline = Color(0.8f, 0.8f, 0.8f, 1.0f);
        theme.roomFill = Color(0.15f, 0.15f, 0.15f, 0.8f);
        theme.wallColor = Color(0.0f, 0.0f, 0.0f, 1.0f);
        theme.doorColor = Color(0.4f, 0.4f, 0.4f, 1.0f);
        theme.edgeHoverColor = Color(0.0f, 1.0f, 0.0f, 0.6f);
        theme.markerColor = Color(0.3f, 0.8f, 0.3f, 1.0f);
        theme.textColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
        
        // Selection colors (blue tint)
        theme.selectionFill = Color(0.3f, 0.6f, 1.0f, 0.25f);
        theme.selectionBorder = Color(0.4f, 0.7f, 1.0f, 0.9f);
        
        // Tool preview colors
        theme.tilePreviewBorder = Color(1.0f, 1.0f, 1.0f, 0.8f);
        theme.tilePreviewBrightness = 1.3f;
        
        // Paste preview colors (green tint)
        theme.pastePreviewFill = Color(0.2f, 0.8f, 0.4f, 0.25f);
        theme.pastePreviewBorder = Color(0.3f, 0.9f, 0.5f, 0.9f);
    } else if (name == "Print-Light") {
        theme.background = Color(1.0f, 1.0f, 1.0f, 1.0f);
        theme.gridLine = Color(0.85f, 0.85f, 0.85f, 1.0f);
        theme.roomOutline = Color(0.2f, 0.2f, 0.2f, 1.0f);
        theme.roomFill = Color(0.95f, 0.95f, 0.95f, 0.5f);
        theme.wallColor = Color(0.0f, 0.0f, 0.0f, 1.0f);
        theme.doorColor = Color(0.6f, 0.6f, 0.6f, 1.0f);
        theme.edgeHoverColor = Color(0.0f, 0.8f, 0.0f, 0.6f);
        theme.markerColor = Color(0.2f, 0.6f, 0.2f, 1.0f);
        theme.textColor = Color(0.0f, 0.0f, 0.0f, 1.0f);
        
        // Selection colors (darker blue for light theme)
        theme.selectionFill = Color(0.2f, 0.4f, 0.8f, 0.2f);
        theme.selectionBorder = Color(0.2f, 0.5f, 0.9f, 0.9f);
        
        // Tool preview colors
        theme.tilePreviewBorder = Color(0.0f, 0.0f, 0.0f, 0.6f);
        theme.tilePreviewBrightness = 1.2f;
        
        // Paste preview colors (darker green for light theme)
        theme.pastePreviewFill = Color(0.1f, 0.6f, 0.3f, 0.2f);
        theme.pastePreviewBorder = Color(0.2f, 0.7f, 0.4f, 0.9f);
    }
}

// ============================================================================
// Content bounds calculation
// ============================================================================

ContentBounds Model::CalculateContentBounds() const {
    ContentBounds bounds;
    bounds.isEmpty = true;
    bounds.minX = 0;
    bounds.minY = 0;
    bounds.maxX = 0;
    bounds.maxY = 0;
    
    bool hasContent = false;
    int tempMinX = INT_MAX;
    int tempMinY = INT_MAX;
    int tempMaxX = INT_MIN;
    int tempMaxY = INT_MIN;
    
    // Check edges (walls and doors)
    for (const auto& pair : edges) {
        if (pair.second == EdgeState::None) continue;
        
        const EdgeId& edge = pair.first;
        tempMinX = std::min(tempMinX, std::min(edge.x1, edge.x2));
        tempMinY = std::min(tempMinY, std::min(edge.y1, edge.y2));
        tempMaxX = std::max(tempMaxX, std::max(edge.x1, edge.x2));
        tempMaxY = std::max(tempMaxY, std::max(edge.y1, edge.y2));
        hasContent = true;
    }
    
    // Check markers
    for (const auto& marker : markers) {
        int mx = static_cast<int>(std::floor(marker.x));
        int my = static_cast<int>(std::floor(marker.y));
        tempMinX = std::min(tempMinX, mx);
        tempMinY = std::min(tempMinY, my);
        tempMaxX = std::max(tempMaxX, mx);
        tempMaxY = std::max(tempMaxY, my);
        hasContent = true;
    }
    
    // Check cell room assignments
    for (const auto& pair : cellRoomAssignments) {
        int cx = pair.first.first;
        int cy = pair.first.second;
        tempMinX = std::min(tempMinX, cx);
        tempMinY = std::min(tempMinY, cy);
        tempMaxX = std::max(tempMaxX, cx);
        tempMaxY = std::max(tempMaxY, cy);
        hasContent = true;
    }
    
    // Check tiles
    for (const auto& row : tiles) {
        for (const auto& run : row.runs) {
            if (run.tileId == 0) continue;  // Skip empty tiles
            
            int startX = run.startX;
            int endX = run.startX + run.count - 1;
            int y = row.y;
            
            tempMinX = std::min(tempMinX, startX);
            tempMinY = std::min(tempMinY, y);
            tempMaxX = std::max(tempMaxX, endX);
            tempMaxY = std::max(tempMaxY, y);
            hasContent = true;
        }
    }
    
    if (hasContent) {
        bounds.isEmpty = false;
        bounds.minX = tempMinX;
        bounds.minY = tempMinY;
        bounds.maxX = tempMaxX;
        bounds.maxY = tempMaxY;
    }
    
    return bounds;
}

// ============================================================================
// Room management
// ============================================================================

RegionGroup* Model::FindRegionGroup(const std::string& id) {
    auto it = std::find_if(regionGroups.begin(), regionGroups.end(),
        [&id](const RegionGroup& rg) { return rg.id == id; });
    return it != regionGroups.end() ? &(*it) : nullptr;
}

const RegionGroup* Model::FindRegionGroup(const std::string& id) const {
    auto it = std::find_if(regionGroups.begin(), regionGroups.end(),
        [&id](const RegionGroup& rg) { return rg.id == id; });
    return it != regionGroups.end() ? &(*it) : nullptr;
}

std::string Model::GenerateRoomId() {
    // Simple UUID-like generation (timestamp + counter)
    static int counter = 0;
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    return "room_" + std::to_string(timestamp) + "_" + 
           std::to_string(counter++);
}

std::string Model::GenerateRegionGroupId() {
    // Simple UUID-like generation (timestamp + counter)
    static int counter = 0;
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    return "region_" + std::to_string(timestamp) + "_" + 
           std::to_string(counter++);
}

// ============================================================================
// Room cell cache management
// ============================================================================

void Model::InvalidateRoomCellCache(const std::string& roomId) {
    Room* room = FindRoom(roomId);
    if (room) {
        room->cellsCacheDirty = true;
    }
}

void Model::InvalidateAllRoomCellCaches() {
    for (auto& room : rooms) {
        room.cellsCacheDirty = true;
    }
}

void Model::UpdateRoomCellCache(Room& room) {
    room.cells.clear();
    for (const auto& assignment : cellRoomAssignments) {
        if (assignment.second == room.id) {
            room.cells.insert(assignment.first);
        }
    }
    room.cellsCacheDirty = false;
}

const std::unordered_set<std::pair<int, int>, PairHash>& 
Model::GetRoomCells(const std::string& roomId) {
    Room* room = FindRoom(roomId);
    if (!room) {
        static std::unordered_set<std::pair<int, int>, PairHash> empty;
        return empty;
    }
    
    if (room->cellsCacheDirty) {
        UpdateRoomCellCache(*room);
    }
    
    return room->cells;
}

// ============================================================================
// Room detection (from enclosed areas)
// ============================================================================

Model::DetectedRoom Model::DetectEnclosedRoom(int x, int y) {
    DetectedRoom result;
    result.isEnclosed = false;
    
    // Check if cell is in bounds
    if (x < 0 || x >= grid.cols || y < 0 || y >= grid.rows) {
        return result;
    }
    
    // Check if cell already belongs to a room
    if (!GetCellRoom(x, y).empty()) {
        return result;  // Already in a room
    }
    
    // Flood-fill to find connected area
    std::vector<std::vector<bool>> visited(
        grid.rows, std::vector<bool>(grid.cols, false)
    );
    
    std::vector<std::pair<int, int>> stack;
    stack.push_back({x, y});
    
    int minX = x, minY = y, maxX = x, maxY = y;
    bool touchesBorder = false;
    
    while (!stack.empty()) {
        auto [cx, cy] = stack.back();
        stack.pop_back();
        
        // Check bounds
        if (cx < 0 || cx >= grid.cols || cy < 0 || cy >= grid.rows) {
            touchesBorder = true;
            continue;
        }
        
        if (visited[cy][cx]) {
            continue;
        }
        
        visited[cy][cx] = true;
        result.cells.insert({cx, cy});
        
        // Update bounding box
        minX = std::min(minX, cx);
        minY = std::min(minY, cy);
        maxX = std::max(maxX, cx);
        maxY = std::max(maxY, cy);
        
        // Check if at border
        if (cx == 0 || cx == grid.cols - 1 || 
            cy == 0 || cy == grid.rows - 1) {
            touchesBorder = true;
        }
        
        // Check all 4 neighbors - only cross if no wall
        EdgeSide sides[] = {
            EdgeSide::North, EdgeSide::South,
            EdgeSide::East, EdgeSide::West
        };
        int dx[] = {0, 0, 1, -1};
        int dy[] = {-1, 1, 0, 0};
        
        for (int i = 0; i < 4; ++i) {
            EdgeId edgeId = MakeEdgeId(cx, cy, sides[i]);
            EdgeState state = GetEdgeState(edgeId);
            
            // Can only cross if edge is None or Door
            // Wall blocks region boundary
            if (state == EdgeState::None || state == EdgeState::Door) {
                int nx = cx + dx[i];
                int ny = cy + dy[i];
                
                // Don't cross into cells that already have rooms
                if (nx >= 0 && nx < grid.cols && 
                    ny >= 0 && ny < grid.rows &&
                    !visited[ny][nx] && 
                    GetCellRoom(nx, ny).empty()) {
                    stack.push_back({nx, ny});
                }
            }
        }
    }
    
    result.isEnclosed = !touchesBorder && !result.cells.empty();
    result.boundingBox = {minX, minY, maxX - minX + 1, maxY - minY + 1};
    
    return result;
}

std::vector<Model::DetectedRoom> Model::DetectAllEnclosedRooms() {
    std::vector<DetectedRoom> detectedRooms;
    
    // Track which cells have been visited
    std::vector<std::vector<bool>> visited(
        grid.rows, std::vector<bool>(grid.cols, false)
    );
    
    // Mark cells already in rooms as visited
    for (const auto& assignment : cellRoomAssignments) {
        int cx = assignment.first.first;
        int cy = assignment.first.second;
        if (cx >= 0 && cx < grid.cols && cy >= 0 && cy < grid.rows) {
            visited[cy][cx] = true;
        }
    }
    
    // Flood-fill from each unvisited cell
    for (int y = 0; y < grid.rows; ++y) {
        for (int x = 0; x < grid.cols; ++x) {
            if (visited[y][x]) continue;
            
            DetectedRoom detected = DetectEnclosedRoom(x, y);
            
            // Mark cells as visited
            for (const auto& cell : detected.cells) {
                int cx = cell.first;
                int cy = cell.second;
                if (cx >= 0 && cx < grid.cols && cy >= 0 && cy < grid.rows) {
                    visited[cy][cx] = true;
                }
            }
            
            // Only keep if enclosed and reasonable size
            if (detected.isEnclosed && 
                detected.cells.size() > 0 && 
                detected.cells.size() < 10000) {
                detectedRooms.push_back(detected);
            }
        }
    }
    
    return detectedRooms;
}

Room Model::CreateRoomFromCells(
    const std::unordered_set<std::pair<int, int>, PairHash>& cells,
    const std::string& name
) {
    Room room;
    room.id = GenerateRoomId();
    room.regionId = -1;
    room.color = GenerateDistinctRoomColor();
    room.cellsCacheDirty = false;
    room.cells = cells;
    room.connectionsDirty = true;
    
    // Generate default name if not provided
    if (name.empty()) {
        int roomNumber = static_cast<int>(rooms.size()) + 1;
        room.name = "Room " + std::to_string(roomNumber);
    } else {
        room.name = name;
    }
    
    // Add to model
    rooms.push_back(room);
    
    // Assign cells
    for (const auto& cell : cells) {
        cellRoomAssignments[cell] = room.id;
    }
    
    // Generate perimeter walls if enabled
    if (autoGenerateRoomWalls) {
        GenerateRoomPerimeterWalls(cells);
    }
    
    MarkDirty();
    return room;
}

// ============================================================================
// Room wall generation
// ============================================================================

void Model::GenerateRoomPerimeterWalls(const std::string& roomId) {
    const auto& cells = GetRoomCells(roomId);
    GenerateRoomPerimeterWalls(cells);
}

void Model::GenerateRoomPerimeterWalls(
    const std::unordered_set<std::pair<int, int>, PairHash>& cells
) {
    if (cells.empty()) return;
    
    // Build a set for fast lookup
    std::set<std::pair<int, int>> cellSet(cells.begin(), cells.end());
    
    // For each cell in the room, check all 4 edges
    for (const auto& cell : cells) {
        int cx = cell.first;
        int cy = cell.second;
        
        EdgeSide sides[] = {
            EdgeSide::North, EdgeSide::South, 
            EdgeSide::East, EdgeSide::West
        };
        
        for (EdgeSide side : sides) {
            EdgeId edgeId = MakeEdgeId(cx, cy, side);
            
            // Determine adjacent cell position
            int adjX = cx;
            int adjY = cy;
            switch (side) {
                case EdgeSide::North: adjY = cy - 1; break;
                case EdgeSide::South: adjY = cy + 1; break;
                case EdgeSide::East:  adjX = cx + 1; break;
                case EdgeSide::West:  adjX = cx - 1; break;
            }
            
            // Check if adjacent cell is in the same room
            bool sameRoom = cellSet.count({adjX, adjY}) > 0;
            
            // If adjacent is NOT in same room, add wall
            // (unless there's already an edge there)
            if (!sameRoom) {
                EdgeState currentState = GetEdgeState(edgeId);
                if (currentState == EdgeState::None) {
                    SetEdgeState(edgeId, EdgeState::Wall);
                }
            }
        }
    }
}

// ============================================================================
// Room connection detection (via doors)
// ============================================================================

void Model::UpdateRoomConnections() {
    // Clear all connections
    for (auto& room : rooms) {
        room.connectedRoomIds.clear();
        room.connectionsDirty = false;
    }
    
    // For each door edge, find adjacent rooms
    for (const auto& edge : edges) {
        if (edge.second != EdgeState::Door) continue;
        
        const EdgeId& edgeId = edge.first;
        
        // Get the two cells on either side of the door
        int x1 = edgeId.x1;
        int y1 = edgeId.y1;
        int x2 = edgeId.x2;
        int y2 = edgeId.y2;
        
        std::string room1Id = GetCellRoom(x1, y1);
        std::string room2Id = GetCellRoom(x2, y2);
        
        // If both cells have rooms and they're different, add connection
        if (!room1Id.empty() && !room2Id.empty() && room1Id != room2Id) {
            Room* room1 = FindRoom(room1Id);
            Room* room2 = FindRoom(room2Id);
            
            if (room1 && room2) {
                // Add bidirectional connection (avoid duplicates)
                if (std::find(room1->connectedRoomIds.begin(), 
                             room1->connectedRoomIds.end(), 
                             room2Id) == room1->connectedRoomIds.end()) {
                    room1->connectedRoomIds.push_back(room2Id);
                }
                
                if (std::find(room2->connectedRoomIds.begin(), 
                             room2->connectedRoomIds.end(), 
                             room1Id) == room2->connectedRoomIds.end()) {
                    room2->connectedRoomIds.push_back(room1Id);
                }
            }
        }
    }
}

void Model::UpdateRoomConnections(const std::string& roomId) {
    Room* room = FindRoom(roomId);
    if (!room) return;
    
    room->connectedRoomIds.clear();
    
    const auto& cells = GetRoomCells(roomId);
    
    // For each cell in the room, check edges for doors
    for (const auto& cell : cells) {
        int cx = cell.first;
        int cy = cell.second;
        
        EdgeSide sides[] = {
            EdgeSide::North, EdgeSide::South,
            EdgeSide::East, EdgeSide::West
        };
        
        int dx[] = {0, 0, 1, -1};
        int dy[] = {-1, 1, 0, 0};
        
        for (int i = 0; i < 4; ++i) {
            EdgeId edgeId = MakeEdgeId(cx, cy, sides[i]);
            EdgeState state = GetEdgeState(edgeId);
            
            if (state == EdgeState::Door) {
                int nx = cx + dx[i];
                int ny = cy + dy[i];
                
                std::string otherRoomId = GetCellRoom(nx, ny);
                if (!otherRoomId.empty() && otherRoomId != roomId) {
                    // Add if not already in list
                    if (std::find(room->connectedRoomIds.begin(), 
                                 room->connectedRoomIds.end(), 
                                 otherRoomId) == 
                        room->connectedRoomIds.end()) {
                        room->connectedRoomIds.push_back(otherRoomId);
                    }
                }
            }
        }
    }
    
    room->connectionsDirty = false;
}

void Model::InvalidateRoomConnections() {
    for (auto& room : rooms) {
        room.connectionsDirty = true;
    }
}

// ============================================================================
// Color generation for rooms
// ============================================================================

Color Model::GenerateDistinctRoomColor() const {
    // Use HSV color space for better distribution
    // Start with predefined hues that are visually distinct
    static const float distinctHues[] = {
        0.0f,    // Red
        30.0f,   // Orange
        60.0f,   // Yellow
        120.0f,  // Green
        180.0f,  // Cyan
        210.0f,  // Light Blue
        240.0f,  // Blue
        270.0f,  // Purple
        300.0f,  // Magenta
        330.0f   // Pink
    };
    
    const int numDistinctHues = 10;
    
    // Track which hues are in use
    std::vector<bool> huesUsed(numDistinctHues, false);
    
    for (const auto& room : rooms) {
        // Convert room color to HSV and find closest hue
        float r = room.color.r;
        float g = room.color.g;
        float b = room.color.b;
        
        float maxC = std::max({r, g, b});
        float minC = std::min({r, g, b});
        float delta = maxC - minC;
        
        if (delta > 0.001f) {
            float hue = 0;
            if (maxC == r) {
                hue = 60.0f * fmod((g - b) / delta, 6.0f);
            } else if (maxC == g) {
                hue = 60.0f * ((b - r) / delta + 2.0f);
            } else {
                hue = 60.0f * ((r - g) / delta + 4.0f);
            }
            if (hue < 0) hue += 360.0f;
            
            // Find closest predefined hue
            for (int i = 0; i < numDistinctHues; ++i) {
                float diff = fabs(hue - distinctHues[i]);
                if (diff < 30.0f) {  // Within 30 degrees
                    huesUsed[i] = true;
                    break;
                }
            }
        }
    }
    
    // Find first unused hue
    int selectedHueIdx = 0;
    for (int i = 0; i < numDistinctHues; ++i) {
        if (!huesUsed[i]) {
            selectedHueIdx = i;
            break;
        }
    }
    
    // If all hues used, cycle through with brightness variation
    if (huesUsed[selectedHueIdx]) {
        selectedHueIdx = rooms.size() % numDistinctHues;
    }
    
    float hue = distinctHues[selectedHueIdx];
    float saturation = 0.7f;
    float value = 0.9f;
    
    // Convert HSV to RGB
    float c = value * saturation;
    float x = c * (1.0f - fabs(fmod(hue / 60.0f, 2.0f) - 1.0f));
    float m = value - c;
    
    float r, g, b;
    if (hue < 60.0f) {
        r = c; g = x; b = 0;
    } else if (hue < 120.0f) {
        r = x; g = c; b = 0;
    } else if (hue < 180.0f) {
        r = 0; g = c; b = x;
    } else if (hue < 240.0f) {
        r = 0; g = x; b = c;
    } else if (hue < 300.0f) {
        r = x; g = 0; b = c;
    } else {
        r = c; g = 0; b = x;
    }
    
    return Color(r + m, g + m, b + m, 1.0f);
}

} // namespace Cartograph
