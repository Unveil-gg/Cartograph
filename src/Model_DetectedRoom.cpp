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


Model::DetectedRoom Model::DetectColorRegion(int x, int y) {
    DetectedRoom result;
    result.isEnclosed = false;
    
    // Check if cell is in bounds (support negative coordinates)
    if (x < grid.minCol || x >= grid.cols || 
        y < grid.minRow || y >= grid.rows) {
        return result;
    }
    
    // Get the room ID (color) at this cell
    std::string sourceRoomId = GetCellRoom(x, y);
    if (sourceRoomId.empty()) {
        return result;  // No color/room assigned, skip
    }
    
    // Flood-fill to find connected cells of the same color
    // Use unordered_set to support negative coordinate indexing
    std::unordered_set<std::pair<int, int>, PairHash> visited;
    
    std::vector<std::pair<int, int>> stack;
    stack.push_back({x, y});
    
    int minX = x, minY = y, maxX = x, maxY = y;
    bool touchesBorder = false;
    
    while (!stack.empty()) {
        auto [cx, cy] = stack.back();
        stack.pop_back();
        
        // Check bounds (support negative coordinates)
        if (cx < grid.minCol || cx >= grid.cols || 
            cy < grid.minRow || cy >= grid.rows) {
            touchesBorder = true;
            continue;
        }
        
        if (visited.count({cx, cy})) {
            continue;
        }
        
        // Check if this cell has the same room assignment
        std::string cellRoomId = GetCellRoom(cx, cy);
        if (cellRoomId != sourceRoomId) {
            // Different color or uncolored - this is a boundary
            // If uncolored and no wall/door, track it
            continue;
        }
        
        visited.insert({cx, cy});
        result.cells.insert({cx, cy});
        
        // Update bounding box
        minX = std::min(minX, cx);
        minY = std::min(minY, cy);
        maxX = std::max(maxX, cx);
        maxY = std::max(maxY, cy);
        
        // Check if at grid border (support negative coordinates)
        if (cx == grid.minCol || cx == grid.cols - 1 || 
            cy == grid.minRow || cy == grid.rows - 1) {
            touchesBorder = true;
        }
        
        // Check all 4 neighbors
        EdgeSide sides[] = {
            EdgeSide::North, EdgeSide::South,
            EdgeSide::East, EdgeSide::West
        };
        int dx[] = {0, 0, 1, -1};
        int dy[] = {1, -1, 0, 0};  // Y-up: North=+1, South=-1
        
        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            
            EdgeId edgeId = MakeEdgeId(cx, cy, sides[i]);
            EdgeState state = GetEdgeState(edgeId);
            
            // Walls and doors block traversal
            if (state == EdgeState::Wall || state == EdgeState::Door) {
                continue;  // Blocked by wall/door
            }
            
            // Check if neighbor is in bounds (support negative coordinates)
            if (nx < grid.minCol || nx >= grid.cols || 
                ny < grid.minRow || ny >= grid.rows) {
                touchesBorder = true;
                continue;
            }
            
            if (visited.count({nx, ny})) {
                continue;
            }
            
            // Check neighbor's room assignment
            std::string neighborRoom = GetCellRoom(nx, ny);
            
            if (neighborRoom == sourceRoomId) {
                // Same color, continue flood-fill
                stack.push_back({nx, ny});
            }
            // Empty or a different color is a boundary. Color regions stay
            // enclosed when they open onto empty cells.
        }
    }
    
    // Region is enclosed if:
    // - Doesn't touch grid border through open edges
    // - Has cells
    // Color regions are considered enclosed even with open edges to empty
    // cells (the color itself defines the boundary)
    result.isEnclosed = !touchesBorder && !result.cells.empty();
    result.boundingBox = {minX, minY, maxX - minX + 1, maxY - minY + 1};
    
    return result;
}


std::vector<Model::DetectedRoom> Model::DetectAllColorRegions() {
    std::vector<DetectedRoom> detectedRooms;
    
    // Track which cells have been visited (use set to support negative coords)
    std::unordered_set<std::pair<int, int>, PairHash> visited;
    
    // Instead of iterating the entire grid, iterate only over cells with
    // room assignments (more efficient and supports negative coordinates)
    for (const auto& assignment : cellRoomAssignments) {
        int x = assignment.first.first;
        int y = assignment.first.second;
        
        if (visited.count({x, y})) continue;
        
        DetectedRoom detected = DetectColorRegion(x, y);
        
        // Mark cells as visited
        for (const auto& cell : detected.cells) {
            visited.insert(cell);
        }
        
        // Color regions are always considered valid rooms
        // (the color defines the room, enclosure is optional)
        if (!detected.cells.empty() && detected.cells.size() < 10000) {
            // Mark as enclosed for color regions (color = boundary)
            detected.isEnclosed = true;
            detectedRooms.push_back(detected);
        }
    }
    
    return detectedRooms;
}


std::vector<std::unordered_set<std::pair<int, int>, PairHash>>
Model::FindContiguousRegions(
    const std::unordered_set<std::pair<int, int>, PairHash>& cells
) {
    std::vector<std::unordered_set<std::pair<int, int>, PairHash>> regions;
    
    if (cells.empty()) return regions;
    
    // Track visited cells within this set
    std::unordered_set<std::pair<int, int>, PairHash> visited;
    
    // For each unvisited cell, flood-fill to find its contiguous region
    for (const auto& startCell : cells) {
        if (visited.count(startCell)) continue;
        
        // Flood-fill from this cell
        std::unordered_set<std::pair<int, int>, PairHash> region;
        std::vector<std::pair<int, int>> stack;
        stack.push_back(startCell);
        
        while (!stack.empty()) {
            auto cell = stack.back();
            stack.pop_back();
            
            if (visited.count(cell)) continue;
            if (cells.find(cell) == cells.end()) continue;  // Not in our set
            
            visited.insert(cell);
            region.insert(cell);
            
            // Check 4-connected neighbors
            int cx = cell.first;
            int cy = cell.second;
            
            // Check for walls/doors blocking adjacency
            EdgeSide sides[] = {
                EdgeSide::North, EdgeSide::South,
                EdgeSide::East, EdgeSide::West
            };
            int dx[] = {0, 0, 1, -1};
            int dy[] = {1, -1, 0, 0};  // Y-up: North=+1, South=-1
            
            for (int i = 0; i < 4; ++i) {
                int nx = cx + dx[i];
                int ny = cy + dy[i];
                std::pair<int, int> neighbor = {nx, ny};
                
                // Skip if not in our cell set
                if (cells.find(neighbor) == cells.end()) continue;
                // Skip if already visited
                if (visited.count(neighbor)) continue;
                
                // Check if blocked by wall or door
                EdgeId edgeId = MakeEdgeId(cx, cy, sides[i]);
                EdgeState state = GetEdgeState(edgeId);
                if (state == EdgeState::Wall || state == EdgeState::Door) {
                    continue;  // Blocked
                }
                
                stack.push_back(neighbor);
            }
        }
        
        if (!region.empty()) {
            regions.push_back(region);
        }
    }
    
    return regions;
}


int Model::SplitDisconnectedRooms() {
    int splitCount = 0;
    
    // Collect rooms to process (can't modify while iterating)
    std::vector<std::string> roomIds;
    for (const auto& room : rooms) {
        roomIds.push_back(room.id);
    }
    
    for (const auto& roomId : roomIds) {
        Room* room = FindRoom(roomId);
        if (!room) continue;
        
        // Get current cells for this room
        const auto& cells = GetRoomCells(roomId);
        if (cells.size() <= 1) continue;  // Can't split single cell
        
        // Find contiguous regions within this room's cells
        auto regions = FindContiguousRegions(cells);
        
        if (regions.size() <= 1) continue;  // Already contiguous
        
        // Find the largest region (keep it with original room)
        size_t largestIdx = 0;
        size_t largestSize = 0;
        for (size_t i = 0; i < regions.size(); ++i) {
            if (regions[i].size() > largestSize) {
                largestSize = regions[i].size();
                largestIdx = i;
            }
        }
        
        // Create new rooms for smaller regions
        int splitNum = 2;
        for (size_t i = 0; i < regions.size(); ++i) {
            if (i == largestIdx) continue;  // Skip largest
            
            // Create new room with similar properties
            Room newRoom;
            newRoom.id = GenerateRoomId();
            newRoom.name = room->name + " (" + std::to_string(splitNum++) + ")";
            newRoom.regionId = -1;
            newRoom.parentRegionGroupId = room->parentRegionGroupId;
            newRoom.color = GenerateDistinctRoomColor();
            newRoom.notes = "";  // Don't copy notes
            newRoom.cellsCacheDirty = false;
            newRoom.cells = regions[i];
            newRoom.connectionsDirty = true;
            
            // Reassign cells to new room
            for (const auto& cell : regions[i]) {
                cellRoomAssignments[cell] = newRoom.id;
            }
            
            rooms.push_back(newRoom);
            splitCount++;
        }
        
        // Update original room's cells to only the largest region
        room->cells = regions[largestIdx];
        room->cellsCacheDirty = true;  // Force refresh
        
        // Clear and reassign cells for original room
        for (const auto& cell : regions[largestIdx]) {
            cellRoomAssignments[cell] = roomId;
        }
    }
    
    if (splitCount > 0) {
        MarkDirty();
    }
    
    return splitCount;
}


Room Model::CreateRoomFromCells(
    const std::unordered_set<std::pair<int, int>, PairHash>& cells,
    const std::string& name,
    bool generateWalls
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
    
    // Generate perimeter walls if enabled and requested
    if (generateWalls && autoGenerateRoomWalls) {
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


} // namespace Cartograph
