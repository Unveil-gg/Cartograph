#include "IOJson.h"
#include "IOJsonCodec.h"
#include "Model.h"
#include "Config.h"
#include "platform/Fs.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

using json = nlohmann::json;


namespace Cartograph {

void IOJson::MigrateV1(Model& outModel) {
            // Step 1: Flip all Y coordinates (negate them)
            // This converts from Y-down to Y-up coordinate system
            
            // Flip tile Y coordinates
            for (auto& row : outModel.tiles) {
                row.y = -row.y;
            }
            
            // Flip edge Y coordinates (use constructor for proper normalization)
            std::unordered_map<EdgeId, EdgeState, EdgeIdHash> flippedEdges;
            for (const auto& [edgeId, state] : outModel.edges) {
                // Constructor normalizes so (x1,y1) < (x2,y2)
                EdgeId flipped(edgeId.x1, -edgeId.y1, edgeId.x2, -edgeId.y2);
                flippedEdges[flipped] = state;
            }
            outModel.edges = std::move(flippedEdges);
            
            // Flip cell room assignment Y coordinates
            std::unordered_map<std::pair<int, int>, std::string, PairHash> 
                flippedAssignments;
            for (const auto& [cell, roomId] : outModel.cellRoomAssignments) {
                flippedAssignments[{cell.first, -cell.second}] = roomId;
            }
            outModel.cellRoomAssignments = std::move(flippedAssignments);
            
            // Flip door Y coordinates
            for (auto& door : outModel.doors) {
                door.a.y = -door.a.y;
                door.b.y = -door.b.y;
            }
            
            // Flip marker Y coordinates
            for (auto& marker : outModel.markers) {
                marker.y = -marker.y;
            }
            
            // Step 2: Find bounding box of all content (after flip)
            int minX = INT_MAX, minY = INT_MAX;
            
            // Check tiles
            for (const auto& row : outModel.tiles) {
                for (const auto& run : row.runs) {
                    minX = std::min(minX, run.startX);
                    minY = std::min(minY, row.y);
                }
            }
            
            // Check edges
            for (const auto& [edgeId, state] : outModel.edges) {
                minX = std::min(minX, std::min(edgeId.x1, edgeId.x2));
                minY = std::min(minY, std::min(edgeId.y1, edgeId.y2));
            }
            
            // Check cell room assignments
            for (const auto& [cell, roomId] : outModel.cellRoomAssignments) {
                minX = std::min(minX, cell.first);
                minY = std::min(minY, cell.second);
            }
            
            // Check doors
            for (const auto& door : outModel.doors) {
                minX = std::min(minX, std::min(door.a.x, door.b.x));
                minY = std::min(minY, std::min(door.a.y, door.b.y));
            }
            
            // Check markers (floor to int for bounds check)
            for (const auto& marker : outModel.markers) {
                minX = std::min(minX, static_cast<int>(std::floor(marker.x)));
                minY = std::min(minY, static_cast<int>(std::floor(marker.y)));
            }
            
            // Step 3: Shift all content so min becomes (0, 0)
            if (minX != INT_MAX && minY != INT_MAX) {
                int offsetX = -minX;  // Shift to make minX become 0
                int offsetY = -minY;  // Shift to make minY become 0
                
                // Shift tiles
                for (auto& row : outModel.tiles) {
                    row.y += offsetY;
                    for (auto& run : row.runs) {
                        run.startX += offsetX;
                    }
                }
                
                // Shift edges (use constructor for proper normalization)
                std::unordered_map<EdgeId, EdgeState, EdgeIdHash> shiftedEdges;
                for (const auto& [edgeId, state] : outModel.edges) {
                    // Constructor normalizes so (x1,y1) < (x2,y2)
                    EdgeId shifted(edgeId.x1 + offsetX, edgeId.y1 + offsetY,
                                   edgeId.x2 + offsetX, edgeId.y2 + offsetY);
                    shiftedEdges[shifted] = state;
                }
                outModel.edges = std::move(shiftedEdges);
                
                // Shift cell room assignments
                std::unordered_map<std::pair<int, int>, std::string, PairHash> 
                    shiftedAssignments;
                for (const auto& [cell, roomId] : outModel.cellRoomAssignments) {
                    shiftedAssignments[{cell.first + offsetX, 
                                        cell.second + offsetY}] = roomId;
                }
                outModel.cellRoomAssignments = std::move(shiftedAssignments);
                
                // Shift doors
                for (auto& door : outModel.doors) {
                    door.a.x += offsetX;
                    door.a.y += offsetY;
                    door.b.x += offsetX;
                    door.b.y += offsetY;
                }
                
                // Shift markers (float offsets)
                for (auto& marker : outModel.markers) {
                    marker.x += static_cast<float>(offsetX);
                    marker.y += static_cast<float>(offsetY);
                }
                
                // Update grid bounds
                outModel.grid.minCol = 0;
                outModel.grid.minRow = 0;
            }
            
            // Mark as dirty so user is prompted to save with new format
            outModel.MarkDirty();
}

}  // namespace Cartograph
