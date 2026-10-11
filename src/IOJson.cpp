#include "IOJson.h"
#include "IOJsonCodec.h"
#include "IOJsonExtras.h"
#include "Model.h"
#include "Config.h"
#include "platform/Fs.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <tuple>
#include <vector>

using json = nlohmann::json;

namespace Cartograph {


// Helper: Convert GridPreset to string
std::string IOJson::SaveToString(const Model& model) {
    json j;
    
    j["version"] = kProjectVersion;
    
    // Grid
    j["grid"] = {
        {"preset", GridPresetToString(model.grid.preset)},
        {"tileWidth", model.grid.tileWidth},
        {"tileHeight", model.grid.tileHeight},
        {"cols", model.grid.cols},
        {"rows", model.grid.rows},
        {"minCol", model.grid.minCol},
        {"minRow", model.grid.minRow},
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
        SavePropsInto(jGroup, group.props);
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
        
        SavePropsInto(jRoom, room.props);
        
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
    // Sorted so saves are deterministic and diff cleanly.
    std::vector<std::pair<EdgeId, EdgeState>> sortedEdges(
        model.edges.begin(), model.edges.end());
    std::sort(sortedEdges.begin(), sortedEdges.end(),
              [](const auto& a, const auto& b) {
                  return std::tie(a.first.x1, a.first.y1, a.first.x2,
                                  a.first.y2) <
                         std::tie(b.first.x1, b.first.y1, b.first.x2,
                                  b.first.y2);
              });
    for (const auto& [edgeId, state] : sortedEdges) {
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
    std::vector<std::pair<std::pair<int, int>, std::string>> sortedCells(
        model.cellRoomAssignments.begin(), model.cellRoomAssignments.end());
    std::sort(sortedCells.begin(), sortedCells.end());
    for (const auto& [cell, roomId] : sortedCells) {
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
        SavePropsInto(markerJson, marker.props);
        
        j["markers"].push_back(markerJson);
    }
    
    // Version 3 sections: abilities, door props, interior
    SaveExtras(model, j);
    
    // Keymap
    j["keymap"] = model.keymap;
    
    // Theme is global preference, not saved per-project
    
    // Metadata
    j["meta"] = {
        {"title", model.meta.title},
        {"author", model.meta.author},
        {"description", model.meta.description}
    };
    
    return j.dump(2);  // Pretty print with 2-space indent
}


} // namespace Cartograph
