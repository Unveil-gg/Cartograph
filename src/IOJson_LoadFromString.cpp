#include "IOJson.h"
#include "IOJsonCodec.h"
#include "IOJsonExtras.h"
#include "Model.h"
#include "Config.h"
#include "platform/Fs.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

using json = nlohmann::json;

namespace Cartograph {


bool IOJson::LoadFromString(const std::string& jsonStr, Model& outModel) {
    // Security: check JSON size before parsing
    if (jsonStr.size() > Limits::MAX_PROJECT_JSON_SIZE) {
        return false;  // File too large
    }
    
    try {
        json j = json::parse(jsonStr);
        
        // Version check - v1/v1.1 (legacy), v2 (Y-up), v3 (current)
        double version = j.value("version", 1.0);
        bool needsMigration = (version < 2);
        if (version > kProjectVersion) {
            return false;  // Unsupported future version
        }
        
        // Grid
        if (j.contains("grid")) {
            const auto& grid = j["grid"];
            
            // Load preset (default to Square if not present)
            if (grid.contains("preset")) {
                outModel.grid.preset = GridPresetFromString(
                    grid.value("preset", "square")
                );
            } else {
                // Infer preset from dimensions for backward compatibility
                int tw = grid.value("tileWidth", 16);
                int th = grid.value("tileHeight", 16);
                if (tw == th) {
                    outModel.grid.preset = GridPreset::Square;
                } else if (tw > th) {
                    outModel.grid.preset = GridPreset::Rectangle;
                } else {
                    outModel.grid.preset = GridPreset::Square;  // Default
                }
            }
            
            // Load dimensions with security bounds
            if (grid.contains("tileWidth") && grid.contains("tileHeight")) {
                outModel.grid.tileWidth = std::clamp(
                    grid.value("tileWidth", 16),
                    Limits::MIN_TILE_SIZE, Limits::MAX_TILE_SIZE);
                outModel.grid.tileHeight = std::clamp(
                    grid.value("tileHeight", 16),
                    Limits::MIN_TILE_SIZE, Limits::MAX_TILE_SIZE);
            } else if (grid.contains("tileSize")) {
                // Old format: use same value for both
                int tileSize = std::clamp(
                    grid.value("tileSize", 16),
                    Limits::MIN_TILE_SIZE, Limits::MAX_TILE_SIZE);
                outModel.grid.tileWidth = tileSize;
                outModel.grid.tileHeight = tileSize;
            } else {
                outModel.grid.tileWidth = 16;
                outModel.grid.tileHeight = 16;
            }
            
            // Security: clamp grid dimensions to prevent overflow
            outModel.grid.cols = std::clamp(
                grid.value("cols", 256),
                Limits::MIN_GRID_DIMENSION, Limits::MAX_GRID_DIMENSION);
            outModel.grid.rows = std::clamp(
                grid.value("rows", 256),
                Limits::MIN_GRID_DIMENSION, Limits::MAX_GRID_DIMENSION);
            // Load negative bounds (default 0 for backward compatibility)
            outModel.grid.minCol = std::clamp(
                grid.value("minCol", 0),
                -Limits::MAX_GRID_DIMENSION, 0);
            outModel.grid.minRow = std::clamp(
                grid.value("minRow", 0),
                -Limits::MAX_GRID_DIMENSION, 0);
            outModel.grid.locked = grid.value("locked", false);
            
            // Edge configuration (optional, with defaults)
            outModel.grid.autoExpandGrid = grid.value("autoExpandGrid", true);
            outModel.grid.expansionThreshold = grid.value("expansionThreshold", 3);
            outModel.grid.expansionFactor = grid.value("expansionFactor", 1.5f);
            outModel.grid.edgeHoverThreshold = 
                grid.value("edgeHoverThreshold", 0.2f);
        }
        
        // Palette
        if (j.contains("palette") && 
            j["palette"].size() <= Limits::MAX_PALETTE_ENTRIES) {
            outModel.palette.clear();
            for (const auto& tile : j["palette"]) {
                TileType t;
                t.id = tile.value("id", 0);
                t.name = tile.value("name", "");
                t.color = Color::FromHex(tile.value("color", "#000000"));
                outModel.palette.push_back(t);
            }
        }
        
        // Region Groups (user-defined groupings)
        if (j.contains("regionGroups") && 
            j["regionGroups"].size() <= Limits::MAX_REGION_GROUPS) {
            outModel.regionGroups.clear();
            for (const auto& group : j["regionGroups"]) {
                RegionGroup rg;
                rg.id = group.value("id", "");
                rg.name = group.value("name", "");
                rg.description = group.value("description", "");
                
                if (group.contains("tags") && group["tags"].is_array()) {
                    rg.tags = group["tags"].get<std::vector<std::string>>();
                }
                
                if (group.contains("rooms") && group["rooms"].is_array()) {
                    rg.roomIds = group["rooms"].get<std::vector<std::string>>();
                }
                rg.props = LoadPropsFrom(group);
                
                outModel.regionGroups.push_back(rg);
            }
        }
        
        // Rooms (metadata only, regions inferred from walls)
        if (j.contains("rooms") && j["rooms"].size() <= Limits::MAX_ROOMS) {
            outModel.rooms.clear();
            for (const auto& room : j["rooms"]) {
                Room r;
                r.id = room.value("id", "");
                r.name = room.value("name", "");
                r.regionId = room.value("regionId", -1);
                r.color = Color::FromHex(room.value("color", "#000000"));
                r.notes = room.value("notes", "");
                
                // Load new fields
                r.parentRegionGroupId = room.value("parentRegionGroupId", "");
                
                if (room.contains("tags") && room["tags"].is_array()) {
                    r.tags = room["tags"].get<std::vector<std::string>>();
                }
                
                // Load image attachments if present
                if (room.contains("images") && room["images"].is_array()) {
                    r.imageAttachments = 
                        room["images"].get<std::vector<std::string>>();
                }
                
                r.props = LoadPropsFrom(room);
                
                // Connected rooms will be recomputed, but load for reference
                if (room.contains("connectedRooms") && 
                    room["connectedRooms"].is_array()) {
                    r.connectedRoomIds = 
                        room["connectedRooms"].get<std::vector<std::string>>();
                    r.connectionsDirty = false;
                } else {
                    r.connectionsDirty = true;
                }
                
                // Mark cell cache as needing update
                r.cellsCacheDirty = true;
                
                outModel.rooms.push_back(r);
            }
        }
        
        // Settings
        if (j.contains("settings")) {
            const auto& settings = j["settings"];
            outModel.autoGenerateRoomWalls = 
                settings.value("autoGenerateRoomWalls", true);
        }
        
        // Tiles
        if (j.contains("tiles") && 
            j["tiles"].size() <= Limits::MAX_TILE_ROWS) {
            outModel.tiles.clear();
            for (const auto& row : j["tiles"]) {
                TileRow tr;
                tr.roomId = row.value("room", "");
                tr.y = row.value("y", 0);
                if (row.contains("runs")) {
                    for (const auto& run : row["runs"]) {
                        TileRun r;
                        r.startX = run[0];
                        r.count = run[1];
                        r.tileId = run[2];
                        tr.runs.push_back(r);
                    }
                }
                outModel.tiles.push_back(tr);
            }
        }
        
        // Edges
        if (j.contains("edges") && j["edges"].size() <= Limits::MAX_EDGES) {
            outModel.edges.clear();
            for (const auto& edge : j["edges"]) {
                EdgeId edgeId;
                edgeId.x1 = edge.value("x1", 0);
                edgeId.y1 = edge.value("y1", 0);
                edgeId.x2 = edge.value("x2", 0);
                edgeId.y2 = edge.value("y2", 0);
                
                int stateInt = edge.value("state", 0);
                if (stateInt < static_cast<int>(EdgeState::None) ||
                    stateInt > static_cast<int>(EdgeState::Door)) {
                    continue;
                }
                EdgeState state = static_cast<EdgeState>(stateInt);
                
                if (state != EdgeState::None) {
                    outModel.edges[edgeId] = state;
                }
            }
        }
        
        // Cell room assignments
        if (j.contains("cellRooms") && 
            j["cellRooms"].size() <= Limits::MAX_CELL_ASSIGNMENTS) {
            outModel.cellRoomAssignments.clear();
            for (const auto& cellRoom : j["cellRooms"]) {
                int x = cellRoom.value("x", 0);
                int y = cellRoom.value("y", 0);
                std::string roomId = cellRoom.value("roomId", "");
                
                if (!roomId.empty()) {
                    outModel.cellRoomAssignments[{x, y}] = roomId;
                }
            }
        }
        
        // Doors
        if (j.contains("doors") && j["doors"].size() <= Limits::MAX_DOORS) {
            outModel.doors.clear();
            for (const auto& door : j["doors"]) {
                Door d;
                d.id = door.value("id", "");
                
                if (door.contains("a")) {
                    auto a = door["a"];
                    d.a.roomId = a.value("room", "");
                    if (a.contains("pos")) {
                        d.a.x = a["pos"][0];
                        d.a.y = a["pos"][1];
                    }
                    d.a.side = DoorSideFromString(a.value("side", "N"));
                }
                
                if (door.contains("b")) {
                    auto b = door["b"];
                    d.b.roomId = b.value("room", "");
                    if (b.contains("pos")) {
                        d.b.x = b["pos"][0];
                        d.b.y = b["pos"][1];
                    }
                    d.b.side = DoorSideFromString(b.value("side", "N"));
                }
                
                d.type = DoorTypeFromString(door.value("type", "door"));
                if (door.contains("gate")) {
                    d.gate = door["gate"].get<std::vector<std::string>>();
                }
                
                outModel.doors.push_back(d);
            }
        }
        
        // Markers
        if (j.contains("markers") && 
            j["markers"].size() <= Limits::MAX_MARKERS) {
            outModel.markers.clear();
            for (const auto& marker : j["markers"]) {
                Marker m;
                m.id = marker.value("id", "");
                m.roomId = marker.value("room", "");
                
                // Position (now supports float for sub-tile precision)
                if (marker.contains("pos") && marker["pos"].is_array() &&
                    marker["pos"].size() >= 2) {
                    // Handle both int (old format) and float (new format)
                    if (marker["pos"][0].is_number_float()) {
                        m.x = marker["pos"][0].get<float>();
                        m.y = marker["pos"][1].get<float>();
                    } else {
                        // Convert int to float for backward compatibility
                        m.x = static_cast<float>(marker["pos"][0].get<int>());
                        m.y = static_cast<float>(marker["pos"][1].get<int>());
                    }
                }
                
                m.kind = marker.value("kind", "");
                m.label = marker.value("label", "");
                m.icon = marker.value("icon", "");
                m.color = Color::FromHex(marker.value("color", "#00ff00"));
                
                // Optional fields with defaults
                m.size = marker.value("size", 0.6f);
                m.showLabel = marker.value("showLabel", true);
                m.props = LoadPropsFrom(marker);
                
                outModel.markers.push_back(m);
            }
        }
        
        // Version 3 sections: abilities, door props, interior
        LoadExtras(j, outModel);
        
        // Keymap
        if (j.contains("keymap")) {
            outModel.keymap = j["keymap"].get<std::unordered_map<std::string, std::string>>();
            // Older files predate this action
            outModel.keymap.emplace("editInterior", "Shift+I");
        }
        
        // Theme is global preference, applied after load in App::OpenProject
        // Old project files may contain theme data - it will be ignored
        
        // Metadata
        if (j.contains("meta")) {
            const auto& meta = j["meta"];
            outModel.meta.title = meta.value("title", "Untitled");
            outModel.meta.author = meta.value("author", "");
            outModel.meta.description = meta.value("description", "");
        }
        
        // TODO(v3+): Remove v1 migration after 2026-06-01 or next major release
        // Migrate v1/v1.1 files to v2: flip Y coords and shift to positive space
        if (needsMigration) {
            MigrateV1(outModel);
        } else {
            outModel.ClearDirty();
        }
        
        return true;
        
    } catch (const std::exception& e) {
        return false;
    }
}


bool IOJson::SaveToFile(const Model& model, const std::string& path) {
    std::string json = SaveToString(model);
    return Platform::WriteTextFile(path, json);
}


bool IOJson::LoadFromFile(const std::string& path, Model& outModel) {
    auto content = Platform::ReadTextFile(path);
    if (!content) {
        return false;
    }
    return LoadFromString(*content, outModel);
}


} // namespace Cartograph
