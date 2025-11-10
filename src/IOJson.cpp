#include "IOJson.h"
#include "Model.h"
#include "platform/Fs.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace Cartograph {

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
        {"tileSize", model.grid.tileSize},
        {"cols", model.grid.cols},
        {"rows", model.grid.rows}
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
    
    // Rooms
    j["rooms"] = json::array();
    for (const auto& room : model.rooms) {
        j["rooms"].push_back({
            {"id", room.id},
            {"name", room.name},
            {"rect", json::array({room.rect.x, room.rect.y, room.rect.w, room.rect.h})},
            {"color", room.color.ToHex(false)},
            {"notes", room.notes}
        });
    }
    
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
        j["markers"].push_back({
            {"id", marker.id},
            {"room", marker.roomId},
            {"pos", json::array({marker.x, marker.y})},
            {"kind", marker.kind},
            {"label", marker.label},
            {"icon", marker.icon},
            {"color", marker.color.ToHex(false)}
        });
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
        {"author", model.meta.author}
    };
    
    return j.dump(2);  // Pretty print with 2-space indent
}

bool IOJson::LoadFromString(const std::string& jsonStr, Model& outModel) {
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
            outModel.grid.tileSize = grid.value("tileSize", 16);
            outModel.grid.cols = grid.value("cols", 256);
            outModel.grid.rows = grid.value("rows", 256);
        }
        
        // Palette
        if (j.contains("palette")) {
            outModel.palette.clear();
            for (const auto& tile : j["palette"]) {
                TileType t;
                t.id = tile.value("id", 0);
                t.name = tile.value("name", "");
                t.color = Color::FromHex(tile.value("color", "#000000"));
                outModel.palette.push_back(t);
            }
        }
        
        // Rooms
        if (j.contains("rooms")) {
            outModel.rooms.clear();
            for (const auto& room : j["rooms"]) {
                Room r;
                r.id = room.value("id", "");
                r.name = room.value("name", "");
                if (room.contains("rect") && room["rect"].is_array()) {
                    auto rect = room["rect"];
                    r.rect.x = rect[0];
                    r.rect.y = rect[1];
                    r.rect.w = rect[2];
                    r.rect.h = rect[3];
                }
                r.color = Color::FromHex(room.value("color", "#000000"));
                r.notes = room.value("notes", "");
                outModel.rooms.push_back(r);
            }
        }
        
        // Tiles
        if (j.contains("tiles")) {
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
        
        // Doors
        if (j.contains("doors")) {
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
        if (j.contains("markers")) {
            outModel.markers.clear();
            for (const auto& marker : j["markers"]) {
                Marker m;
                m.id = marker.value("id", "");
                m.roomId = marker.value("room", "");
                if (marker.contains("pos")) {
                    m.x = marker["pos"][0];
                    m.y = marker["pos"][1];
                }
                m.kind = marker.value("kind", "");
                m.label = marker.value("label", "");
                m.icon = marker.value("icon", "");
                m.color = Color::FromHex(marker.value("color", "#00ff00"));
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

