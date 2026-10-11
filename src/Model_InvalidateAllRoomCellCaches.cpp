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
    
    // Check if cell is in bounds (support negative coordinates)
    if (x < grid.minCol || x >= grid.cols || 
        y < grid.minRow || y >= grid.rows) {
        return result;
    }
    
    // Get the paint state of the starting cell
    // This combines room assignments AND tile colors
    std::string startPaintState = GetCellPaintState(x, y);
    
    // Flood-fill to find connected area with same "paint state"
    // Use unordered_set to support negative coordinate indexing
    std::unordered_set<std::pair<int, int>, PairHash> visited;
    
    std::vector<std::pair<int, int>> stack;
    stack.push_back({x, y});
    visited.insert({x, y});  // Mark start as visited immediately
    
    int minX = x, minY = y, maxX = x, maxY = y;
    bool touchesBorder = false;
    
    while (!stack.empty()) {
        auto [cx, cy] = stack.back();
        stack.pop_back();
        
        // Add to result
        result.cells.insert({cx, cy});
        
        // Update bounding box
        minX = std::min(minX, cx);
        minY = std::min(minY, cy);
        maxX = std::max(maxX, cx);
        maxY = std::max(maxY, cy);
        
        // Check if at border (only for unpainted cells)
        // Painted cells bounded by unpainted are still "enclosed"
        if (startPaintState.empty() && 
            (cx == grid.minCol || cx == grid.cols - 1 || 
             cy == grid.minRow || cy == grid.rows - 1)) {
            touchesBorder = true;
        }
        
        // Check all 4 neighbors - only cross if no wall/door
        EdgeSide sides[] = {
            EdgeSide::North, EdgeSide::South,
            EdgeSide::East, EdgeSide::West
        };
        int dx[] = {0, 0, 1, -1};
        int dy[] = {1, -1, 0, 0};  // Y-up: North=+1, South=-1
        
        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            
            // Check bounds (support negative coordinates)
            if (nx < grid.minCol || nx >= grid.cols || 
                ny < grid.minRow || ny >= grid.rows) {
                // Out of bounds = touches border for unpainted cells
                if (startPaintState.empty()) {
                    touchesBorder = true;
                }
                continue;
            }
            
            // Skip if already visited
            if (visited.count({nx, ny})) {
                continue;
            }
            
            // Check if neighbor has same paint state (room ID or tile color)
            // Different paint state = boundary (don't cross)
            if (GetCellPaintState(nx, ny) != startPaintState) {
                continue;  // Different paint state = boundary
            }
            
            // Check edge state - walls and doors block crossing
            EdgeId edgeId = MakeEdgeId(cx, cy, sides[i]);
            EdgeState state = GetEdgeState(edgeId);
            if (state != EdgeState::None) {
                continue;  // Wall or door blocks crossing
            }
            
            // Valid neighbor - mark visited and add to stack
            visited.insert({nx, ny});
            stack.push_back({nx, ny});
        }
    }
    
    // For painted regions: enclosed if we found cells and didn't escape
    // through open edges to unpainted space (handled by paint state check)
    // For unpainted regions: enclosed if didn't touch border
    if (startPaintState.empty()) {
        result.isEnclosed = !touchesBorder && !result.cells.empty();
    } else {
        // Painted cells are enclosed if bounded by walls/doors/different colors
        result.isEnclosed = !result.cells.empty();
    }
    
    result.boundingBox = {minX, minY, maxX - minX + 1, maxY - minY + 1};
    
    return result;
}


std::vector<Model::DetectedRoom> Model::DetectAllEnclosedRooms() {
    std::vector<DetectedRoom> detectedRooms;
    
    // Track which cells have been visited (use set to support negative coords)
    std::unordered_set<std::pair<int, int>, PairHash> visited;
    
    // OPTIMIZATION: Pre-build paint state cache to avoid expensive GetTileAt
    // This is built ONCE and used for all lookups during flood-fill
    std::unordered_map<std::pair<int, int>, std::string, PairHash> paintStateCache;
    
    // 1. Collect cells from global tile layer (roomId = "")
    // No bounds filtering - include all cells including negative coordinates
    for (const auto& row : tiles) {
        if (row.roomId.empty()) {  // Global tile layer
            for (const auto& run : row.runs) {
                if (run.tileId > 0) {  // Has a tile (not empty)
                    std::string paintState = "tile_" + std::to_string(run.tileId);
                    for (int x = run.startX; x < run.startX + run.count; ++x) {
                        paintStateCache[{x, row.y}] = paintState;
                    }
                }
            }
        }
    }
    
    // 2. Add cells with room assignments (overrides tile paint state)
    // No bounds filtering - include all cells including negative coordinates
    for (const auto& assignment : cellRoomAssignments) {
        paintStateCache[assignment.first] = assignment.second;  // Room ID
    }
    
    // Helper lambda for cached paint state lookup - O(1) instead of O(n)
    auto getCachedPaintState = [&](int x, int y) -> std::string {
        auto it = paintStateCache.find({x, y});
        return (it != paintStateCache.end()) ? it->second : "";
    };
    
    // 3. Only process painted cells (skip empty cells entirely)
    for (const auto& [cellCoord, paintState] : paintStateCache) {
        int startX = cellCoord.first;
        int startY = cellCoord.second;
        
        if (visited.count({startX, startY})) continue;
        
        // Inline flood-fill using cached paint states (FAST!)
        DetectedRoom detected;
        detected.isEnclosed = false;
        
        std::string startPaintState = paintState;
        std::vector<std::pair<int, int>> stack;
        stack.push_back({startX, startY});
        visited.insert({startX, startY});
        
        int minX = startX, minY = startY, maxX = startX, maxY = startY;
        bool touchesBorder = false;
        
        while (!stack.empty()) {
            auto [cx, cy] = stack.back();
            stack.pop_back();
            
            detected.cells.insert({cx, cy});
            
            minX = std::min(minX, cx);
            minY = std::min(minY, cy);
            maxX = std::max(maxX, cx);
            maxY = std::max(maxY, cy);
            
            if (startPaintState.empty() && 
                (cx == grid.minCol || cx == grid.cols - 1 || 
                 cy == grid.minRow || cy == grid.rows - 1)) {
                touchesBorder = true;
            }
            
            // Check 4 neighbors
            EdgeSide sides[] = {
                EdgeSide::North, EdgeSide::South,
                EdgeSide::East, EdgeSide::West
            };
            int dx[] = {0, 0, 1, -1};
            int dy[] = {1, -1, 0, 0};  // Y-up: North=+1, South=-1
            
            for (int i = 0; i < 4; ++i) {
                int nx = cx + dx[i];
                int ny = cy + dy[i];
                
                if (nx < grid.minCol || nx >= grid.cols || 
                    ny < grid.minRow || ny >= grid.rows) {
                    if (startPaintState.empty()) touchesBorder = true;
                    continue;
                }
                
                if (visited.count({nx, ny})) continue;
                
                // Use cached paint state - O(1) lookup!
                if (getCachedPaintState(nx, ny) != startPaintState) continue;
                
                // Check edge state
                EdgeId edgeId = MakeEdgeId(cx, cy, sides[i]);
                EdgeState state = GetEdgeState(edgeId);
                if (state != EdgeState::None) continue;
                
                visited.insert({nx, ny});
                stack.push_back({nx, ny});
            }
        }
        
        // Determine if enclosed
        if (startPaintState.empty()) {
            detected.isEnclosed = !touchesBorder && !detected.cells.empty();
        } else {
            detected.isEnclosed = !detected.cells.empty();
        }
        
        detected.boundingBox = {minX, minY, maxX - minX + 1, maxY - minY + 1};
        
        if (detected.isEnclosed && 
            detected.cells.size() > 0 && 
            detected.cells.size() < 10000) {
            detectedRooms.push_back(detected);
        }
    }
    
    return detectedRooms;
}


} // namespace Cartograph
