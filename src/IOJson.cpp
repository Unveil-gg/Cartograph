#include "IOJson.h"
#include "Model.h"
#include "Limits.h"
#include "platform/Fs.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

using json = nlohmann::json;

namespace Cartograph {

// Helper: Convert GridPreset to string
static std::string GridPresetToString(GridPreset preset) {
    switch (preset) {
        case GridPreset::Square: return "square";
        case GridPreset::Rectangle: return "rectangle";
        default: return "square";
    }
}

// Helper: Parse GridPreset from string
static GridPreset GridPresetFromString(const std::string& str) {
    if (str == "rectangle") return GridPreset::Rectangle;
    return GridPreset::Square;
}

// Helper: Convert DoorSide to string
static std::string DoorSideToString(DoorSide side) {
    switch (side) {
        case DoorSide::North: return "N";
        case DoorSide::South: return "S";
        case DoorSide::East: return "E";
        case DoorSide::West: return "W";
        default: return "N";
    }
}

// Helper: Parse DoorSide from string
static DoorSide DoorSideFromString(const std::string& str) {
    if (str == "S") return DoorSide::South;
    if (str == "E") return DoorSide::East;
    if (str == "W") return DoorSide::West;
    return DoorSide::North;
}

// Helper: Convert DoorType to string
static std::string DoorTypeToString(DoorType type) {
    switch (type) {
        case DoorType::Door: return "door";
        case DoorType::Elevator: return "elevator";
        case DoorType::Teleporter: return "teleporter";
        default: return "door";
    }
}

// Helper: Parse DoorType from string
static DoorType DoorTypeFromString(const std::string& str) {
    if (str == "elevator") return DoorType::Elevator;
    if (str == "teleporter") return DoorType::Teleporter;
    return DoorType::Door;
}

std::string IOJson::SaveToString(const Model& model) {
    json j;
    
    j["version"] = 1;
    
    // Grid
    j["grid"] = {
        {"preset", GridPresetToString(model.grid.preset)},
        {"tileWidth", model.grid.tileWidth},
        {"tileHeight", model.grid.tileHeight},
        {"cols", model.grid.cols},
        {"rows", model.grid.rows},
        {"locked", model.grid.locked},
        {"autoExpandGrid", model.grid.autoExpandGrid},
        {"expansionThreshold", model.grid.expansionThreshold},
        {"expansionFactor", model.grid.expansionFactor},
        {"edgeHoverThreshold", model.grid.edgeHoverThreshold}
    };
    
    // Palette
    j["palette"] = json::array();
    for (const auto& tile : model.palette) {
        j["palette"].push_back({
            {"id", tile.id},
            {"name", tile.name},
            {"color", tile.color.ToHex(true)}
        });
    }
    
    // Region Groups (user-defined groupings)
    j["regionGroups"] = json::array();
    for (const auto& group : model.regionGroups) {
        json jGroup = {
            {"id", group.id},
            {"name", group.name},
            {"description", group.description},
            {"tags", group.tags},
            {"rooms", group.roomIds}
        };
        j["regionGroups"].push_back(jGroup);
    }
    
    // Rooms (metadata only, linked to regions)
    j["rooms"] = json::array();
    for (const auto& room : model.rooms) {
        json jRoom = {
            {"id", room.id},
            {"name", room.name},
            {"regionId", room.regionId},
            {"color", room.color.ToHex(false)},
            {"notes", room.notes}
        };
        
        // Save new fields
        if (!room.parentRegionGroupId.empty()) {
            jRoom["parentRegionGroupId"] = room.parentRegionGroupId;
        }
        
        if (!room.tags.empty()) {
            jRoom["tags"] = room.tags;
        }
        
        // Save image attachments if present
        if (!room.imageAttachments.empty()) {
            jRoom["images"] = room.imageAttachments;
        }
        
        // Save connected rooms if present (for reference, recomputed on load)
        if (!room.connectedRoomIds.empty()) {
            jRoom["connectedRooms"] = room.connectedRoomIds;
        }
        
        j["rooms"].push_back(jRoom);
    }
    
    // Room settings
    j["settings"] = {
        {"autoGenerateRoomWalls", model.autoGenerateRoomWalls}
    };
    
    // Tiles
    j["tiles"] = json::array();
    for (const auto& row : model.tiles) {
        json jRow;
        jRow["room"] = row.roomId;
        jRow["y"] = row.y;
        jRow["runs"] = json::array();
        for (const auto& run : row.runs) {
            jRow["runs"].push_back(json::array({run.startX, run.count, run.tileId}));
        }
        j["tiles"].push_back(jRow);
    }
    
    // Edges
    j["edges"] = json::array();
    for (const auto& [edgeId, state] : model.edges) {
        if (state == EdgeState::None) continue;  // Skip empty edges
        j["edges"].push_back({
            {"x1", edgeId.x1},
            {"y1", edgeId.y1},
            {"x2", edgeId.x2},
            {"y2", edgeId.y2},
            {"state", static_cast<int>(state)}
        });
    }
    
    // Cell room assignments
    j["cellRooms"] = json::array();
    for (const auto& [cell, roomId] : model.cellRoomAssignments) {
        j["cellRooms"].push_back({
            {"x", cell.first},
            {"y", cell.second},
            {"roomId", roomId}
        });
    }
    
    // Doors
    j["doors"] = json::array();
    for (const auto& door : model.doors) {
        j["doors"].push_back({
            {"id", door.id},
            {"a", {
                {"room", door.a.roomId},
                {"pos", json::array({door.a.x, door.a.y})},
                {"side", DoorSideToString(door.a.side)}
            }},
            {"b", {
                {"room", door.b.roomId},
                {"pos", json::array({door.b.x, door.b.y})},
                {"side", DoorSideToString(door.b.side)}
            }},
            {"type", DoorTypeToString(door.type)},
            {"gate", door.gate}
        });
    }
    
    // Markers
    j["markers"] = json::array();
    for (const auto& marker : model.markers) {
        json markerJson = {
            {"id", marker.id},
            {"room", marker.roomId},
            {"pos", json::array({marker.x, marker.y})},  // Now floats
            {"kind", marker.kind},
            {"label", marker.label},
            {"icon", marker.icon},
            {"color", marker.color.ToHex(false)}
        };
        
        // Add optional fields (only if non-default)
        if (marker.size != 0.6f) {
            markerJson["size"] = marker.size;
        }
        if (!marker.showLabel) {
            markerJson["showLabel"] = false;
        }
        
        j["markers"].push_back(markerJson);
    }
    
    // Keymap
    j["keymap"] = model.keymap;
    
    // Theme
    j["theme"] = {
        {"name", model.theme.name},
        {"uiScale", model.theme.uiScale},
        {"mapColors", json::object()}
    };
    
    // Metadata
    j["meta"] = {
        {"title", model.meta.title},
        {"author", model.meta.author},
        {"description", model.meta.description}
    };
    
    return j.dump(2);  // Pretty print with 2-space indent
}

bool IOJson::LoadFromString(const std::string& jsonStr, Model& outModel) {
    // Security: check JSON size before parsing
    if (jsonStr.size() > Limits::MAX_PROJECT_JSON_SIZE) {
        return false;  // File too large
    }
    
    try {
        json j = json::parse(jsonStr);
        
        // Version check
        int version = j.value("version", 1);
        if (version != 1) {
            return false;  // Unsupported version
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
                if (marker.contains("pos")) {
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
                
                outModel.markers.push_back(m);
            }
        }
        
        // Keymap
        if (j.contains("keymap")) {
            outModel.keymap = j["keymap"].get<std::unordered_map<std::string, std::string>>();
        }
        
        // Theme
        if (j.contains("theme")) {
            const auto& theme = j["theme"];
            outModel.theme.name = theme.value("name", "Dark");
            outModel.theme.uiScale = theme.value("uiScale", 1.0f);
            outModel.InitDefaultTheme(outModel.theme.name);
        }
        
        // Metadata
        if (j.contains("meta")) {
            const auto& meta = j["meta"];
            outModel.meta.title = meta.value("title", "Untitled");
            outModel.meta.author = meta.value("author", "");
            outModel.meta.description = meta.value("description", "");
        }
        
        outModel.ClearDirty();
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

