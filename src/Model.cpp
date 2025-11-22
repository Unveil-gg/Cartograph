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
    
    // TODO: Properly handle run-length encoding insertion/updates
    // For now, simple implementation - replace entire row
    row->runs.clear();
    if (tileId != 0) {
        row->runs.push_back({x, 1, tileId});
    }
    
    MarkDirty();
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
    keymap["paint"] = "Mouse1";
    keymap["erase"] = "Mouse2";
    keymap["fill"] = "F";
    keymap["rect"] = "R";
    keymap["door"] = "D";
    keymap["marker"] = "M";
    keymap["eyedropper"] = "Alt+Mouse1";
    
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
        theme.doorColor = Color(1.0f, 0.8f, 0.2f, 1.0f);
        theme.markerColor = Color(0.3f, 0.8f, 0.3f, 1.0f);
        theme.textColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
    } else if (name == "Print-Light") {
        theme.background = Color(1.0f, 1.0f, 1.0f, 1.0f);
        theme.gridLine = Color(0.85f, 0.85f, 0.85f, 1.0f);
        theme.roomOutline = Color(0.2f, 0.2f, 0.2f, 1.0f);
        theme.roomFill = Color(0.95f, 0.95f, 0.95f, 0.5f);
        theme.doorColor = Color(0.8f, 0.6f, 0.0f, 1.0f);
        theme.markerColor = Color(0.2f, 0.6f, 0.2f, 1.0f);
        theme.textColor = Color(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

} // namespace Cartograph

