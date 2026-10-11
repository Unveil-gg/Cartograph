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
                // Y-up: North (up) is higher Y, South (down) is lower Y
                case EdgeSide::North: adjY = cy + 1; break;
                case EdgeSide::South: adjY = cy - 1; break;
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


std::vector<std::tuple<EdgeId, EdgeState, EdgeState>> 
Model::ComputeRoomPerimeterWallChanges(const std::string& roomId) {
    std::vector<std::tuple<EdgeId, EdgeState, EdgeState>> changes;
    
    const auto& cells = GetRoomCells(roomId);
    if (cells.empty()) return changes;
    
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
                // Y-up: North (up) is higher Y, South (down) is lower Y
                case EdgeSide::North: adjY = cy + 1; break;
                case EdgeSide::South: adjY = cy - 1; break;
                case EdgeSide::East:  adjX = cx + 1; break;
                case EdgeSide::West:  adjX = cx - 1; break;
            }
            
            // Check if adjacent cell is in the same room
            bool sameRoom = cellSet.count({adjX, adjY}) > 0;
            
            // If adjacent is NOT in same room, would add wall
            if (!sameRoom) {
                EdgeState currentState = GetEdgeState(edgeId);
                if (currentState == EdgeState::None) {
                    changes.emplace_back(edgeId, EdgeState::None, EdgeState::Wall);
                }
            }
        }
    }
    
    return changes;
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
        int dy[] = {1, -1, 0, 0};  // Y-up: North=+1, South=-1
        
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
