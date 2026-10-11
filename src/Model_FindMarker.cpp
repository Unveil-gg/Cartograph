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
    keymap["toolRoomSelect"] = "Shift+S";
    
    // View
    keymap["pan"] = "Space+Drag";
    keymap["zoomIn"] = "=";
    keymap["zoomOut"] = "-";
    keymap["toggleGrid"] = "G";
    keymap["toggleRoomOverlays"] = "O";
    
    // Room actions
    keymap["detectRooms"] = "Shift+D";
    keymap["editInterior"] = "Shift+I";
    
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
    // Delegate to centralized theme initialization
    InitTheme(theme, name);
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


} // namespace Cartograph
