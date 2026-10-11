#include "Model.h"
#include "Config.h"
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


void Model::ExpandGridIfNeeded(int cellX, int cellY) {
    if (!grid.autoExpandGrid) {
        return;
    }
    
    int threshold = grid.expansionThreshold;
    bool needsExpansion = false;
    
    // Check positive boundaries (max). Stop at the load-time cap.
    if (cellX >= grid.cols - threshold &&
        grid.cols < Limits::MAX_GRID_DIMENSION) {
        int newCols = static_cast<int>(grid.cols * grid.expansionFactor);
        if (newCols <= grid.cols) newCols = grid.cols + 64;
        if (newCols > Limits::MAX_GRID_DIMENSION) {
            newCols = Limits::MAX_GRID_DIMENSION;
        }
        if (newCols > grid.cols) {
            grid.cols = newCols;
            needsExpansion = true;
        }
    }
    if (cellY >= grid.rows - threshold &&
        grid.rows < Limits::MAX_GRID_DIMENSION) {
        int newRows = static_cast<int>(grid.rows * grid.expansionFactor);
        if (newRows <= grid.rows) newRows = grid.rows + 64;
        if (newRows > Limits::MAX_GRID_DIMENSION) {
            newRows = Limits::MAX_GRID_DIMENSION;
        }
        if (newRows > grid.rows) {
            grid.rows = newRows;
            needsExpansion = true;
        }
    }

    // Check negative boundaries (min)
    if (cellX < grid.minCol + threshold &&
        grid.minCol > -Limits::MAX_GRID_DIMENSION) {
        int expansion = static_cast<int>(
            (grid.cols - grid.minCol) * (grid.expansionFactor - 1.0f)
        );
        if (expansion < 64) expansion = 64;
        long long newMin = static_cast<long long>(grid.minCol) - expansion;
        if (newMin < -Limits::MAX_GRID_DIMENSION) {
            newMin = -Limits::MAX_GRID_DIMENSION;
        }
        if (newMin < grid.minCol) {
            grid.minCol = static_cast<int>(newMin);
            needsExpansion = true;
        }
    }
    if (cellY < grid.minRow + threshold &&
        grid.minRow > -Limits::MAX_GRID_DIMENSION) {
        int expansion = static_cast<int>(
            (grid.rows - grid.minRow) * (grid.expansionFactor - 1.0f)
        );
        if (expansion < 64) expansion = 64;
        long long newMin = static_cast<long long>(grid.minRow) - expansion;
        if (newMin < -Limits::MAX_GRID_DIMENSION) {
            newMin = -Limits::MAX_GRID_DIMENSION;
        }
        if (newMin < grid.minRow) {
            grid.minRow = static_cast<int>(newMin);
            needsExpansion = true;
        }
    }
    
    if (needsExpansion) {
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
    
    // Track which cells have been visited (use set to support negative coords)
    std::unordered_set<std::pair<int, int>, PairHash> visited;
    
    int nextRegionId = 0;
    
    // Flood-fill from each unvisited cell (support negative coordinates)
    for (int y = grid.minRow; y < grid.rows; ++y) {
        for (int x = grid.minCol; x < grid.cols; ++x) {
            if (visited.count({x, y})) continue;
            
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
                if (cx < grid.minCol || cx >= grid.cols || 
                    cy < grid.minRow || cy >= grid.rows || 
                    visited.count({cx, cy})) {
                    continue;
                }
                
                // Mark as visited
                visited.insert({cx, cy});
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
                int dy[] = {1, -1, 0, 0};  // Y-up: North=+1, South=-1
                
                for (int i = 0; i < 4; ++i) {
                    EdgeId edgeId = MakeEdgeId(cx, cy, sides[i]);
                    EdgeState state = GetEdgeState(edgeId);
                    
                    // Can only cross if edge is None or Door
                    // Wall blocks region boundary
                    if (state == EdgeState::None || state == EdgeState::Door) {
                        int nx = cx + dx[i];
                        int ny = cy + dy[i];
                        
                        if (nx >= grid.minCol && nx < grid.cols && 
                            ny >= grid.minRow && ny < grid.rows &&
                            !visited.count({nx, ny})) {
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
                // Y-up: North (up) is higher Y, South (down) is lower Y
                case EdgeSide::North: adjY = cy + 1; break;
                case EdgeSide::South: adjY = cy - 1; break;
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


std::string Model::GetCellPaintState(int x, int y) const {
    // First check room assignment
    std::string roomId = GetCellRoom(x, y);
    if (!roomId.empty()) {
        return roomId;  // Has room assignment
    }
    
    // Check global tile layer for painted tiles
    int tileId = GetTileAt("", x, y);
    if (tileId > 0) {
        return "tile_" + std::to_string(tileId);  // Has tile color
    }
    
    return "";  // Truly empty/unpainted
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


} // namespace Cartograph
