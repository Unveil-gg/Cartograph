#include "Model.h"
#include <imgui.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstdio>

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

void Model::GenerateRoomPerimeterWalls(const Room& room) {
    // For each cell in the room, check all 4 edges
    for (int cy = room.rect.y; cy < room.rect.y + room.rect.h; ++cy) {
        for (int cx = room.rect.x; cx < room.rect.x + room.rect.w; ++cx) {
            // Check all 4 sides
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
                
                // Check if adjacent cell belongs to any room
                bool adjacentInRoom = false;
                for (const auto& otherRoom : rooms) {
                    if (otherRoom.rect.Contains(adjX, adjY)) {
                        adjacentInRoom = true;
                        break;
                    }
                }
                
                // If adjacent cell is not in any room, add wall
                // (unless there's already an edge there)
                if (!adjacentInRoom) {
                    EdgeState currentState = GetEdgeState(edgeId);
                    if (currentState == EdgeState::None) {
                        SetEdgeState(edgeId, EdgeState::Wall);
                    }
                }
            }
        }
    }
}

void Model::InitDefaults() {
    InitDefaultPalette();
    InitDefaultKeymap();
    InitDefaultTheme("Dark");
    
    grid.tileWidth = 16;
    grid.tileHeight = 16;
    grid.cols = 256;
    grid.rows = 256;
    
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
    
    // Tools
    keymap["paint"] = "Mouse1";  // Left click or single tap
    keymap["erase"] = "Mouse2";  // Right click or two-finger tap (trackpad)
    keymap["eraseAlt"] = "E+Mouse1";  // Hold E key + left click to erase
    keymap["fill"] = "F";
    keymap["rect"] = "R";
    keymap["marker"] = "M";
    keymap["eyedropper"] = "I";  // Use I key for eyedropper
    
    // Edge actions (within Paint tool)
    keymap["placeWall"] = "W";   // Direct wall placement
    keymap["placeDoor"] = "D";   // Direct door placement
    
    // Tool switching shortcuts
    keymap["toolMove"] = "V";
    keymap["toolSelect"] = "S";
    keymap["toolPaint"] = "B";  // B for Brush
    keymap["toolErase"] = "E";
    keymap["toolFill"] = "F";
    
    // View
    keymap["pan"] = "Space+Drag";
    keymap["zoomIn"] = "=";
    keymap["zoomOut"] = "-";
    keymap["toggleGrid"] = "G";
    
    // Edit
#ifdef __APPLE__
    keymap["undo"] = "Cmd+Z";
    keymap["redo"] = "Cmd+Y";
    keymap["open"] = "Cmd+O";
    keymap["save"] = "Cmd+S";
    keymap["export"] = "Cmd+E";
#else
    keymap["undo"] = "Ctrl+Z";
    keymap["redo"] = "Ctrl+Y";
    keymap["open"] = "Ctrl+O";
    keymap["save"] = "Ctrl+S";
    keymap["export"] = "Ctrl+E";
#endif
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
    }
}

} // namespace Cartograph

