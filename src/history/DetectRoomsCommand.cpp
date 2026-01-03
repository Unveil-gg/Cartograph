#include "DetectRoomsCommand.h"
#include <set>
#include <algorithm>

namespace Cartograph {

void DetectRoomsCommand::Execute(Model& model) {
    if (m_hasExecuted) {
        // Redo: re-apply stored changes without re-running detection
        
        // Re-add created rooms
        for (const auto& room : m_createdRooms) {
            model.rooms.push_back(room);
        }
        
        // Re-apply cell assignments
        for (const auto& change : m_cellChanges) {
            if (change.newRoomId.empty()) {
                model.ClearCellRoom(change.x, change.y);
            } else {
                model.SetCellRoom(change.x, change.y, change.newRoomId);
            }
        }
        
        model.InvalidateAllRoomCellCaches();
        model.MarkDirty();
        return;
    }
    
    // First execution: run detection and track changes
    
    // Capture initial state
    std::set<std::string> roomIdsBefore;
    for (const auto& room : model.rooms) {
        roomIdsBefore.insert(room.id);
    }
    
    auto cellAssignmentsBefore = model.cellRoomAssignments;
    
    // Run split detection first
    m_splitCount = model.SplitDisconnectedRooms();
    
    // Run room detection
    auto enclosedRooms = model.DetectAllEnclosedRooms();
    
    // Create rooms from detected regions (only unpainted cells)
    for (const auto& detected : enclosedRooms) {
        if (detected.isEnclosed && !detected.cells.empty()) {
            bool hasUnpaintedCells = false;
            for (const auto& cell : detected.cells) {
                if (model.GetCellRoom(cell.first, cell.second).empty()) {
                    hasUnpaintedCells = true;
                    break;
                }
            }
            
            if (hasUnpaintedCells) {
                // Skip wall generation - user can add walls separately
                Room room = model.CreateRoomFromCells(
                    detected.cells, "", false
                );
                // Room is already added to model by CreateRoomFromCells
            }
        }
    }
    
    // Track created rooms (new rooms that didn't exist before)
    for (const auto& room : model.rooms) {
        if (roomIdsBefore.find(room.id) == roomIdsBefore.end()) {
            m_createdRooms.push_back(room);
        }
    }
    
    // Track cell assignment changes
    // First, find cells that changed
    std::set<std::pair<int, int>> allCells;
    for (const auto& [cell, roomId] : cellAssignmentsBefore) {
        allCells.insert(cell);
    }
    for (const auto& [cell, roomId] : model.cellRoomAssignments) {
        allCells.insert(cell);
    }
    
    for (const auto& cell : allCells) {
        std::string oldRoomId = "";
        std::string newRoomId = "";
        
        auto itOld = cellAssignmentsBefore.find(cell);
        if (itOld != cellAssignmentsBefore.end()) {
            oldRoomId = itOld->second;
        }
        
        auto itNew = model.cellRoomAssignments.find(cell);
        if (itNew != model.cellRoomAssignments.end()) {
            newRoomId = itNew->second;
        }
        
        if (oldRoomId != newRoomId) {
            m_cellChanges.push_back({cell.first, cell.second, 
                                     oldRoomId, newRoomId});
        }
    }
    
    m_hasExecuted = true;
}

void DetectRoomsCommand::Undo(Model& model) {
    // Remove created rooms
    for (const auto& createdRoom : m_createdRooms) {
        auto it = std::find_if(model.rooms.begin(), model.rooms.end(),
            [&](const Room& r) { return r.id == createdRoom.id; });
        if (it != model.rooms.end()) {
            model.rooms.erase(it);
        }
    }
    
    // Restore old cell assignments
    for (const auto& change : m_cellChanges) {
        if (change.oldRoomId.empty()) {
            model.ClearCellRoom(change.x, change.y);
        } else {
            model.SetCellRoom(change.x, change.y, change.oldRoomId);
        }
    }
    
    model.InvalidateAllRoomCellCaches();
    model.MarkDirty();
}

std::string DetectRoomsCommand::GetDescription() const {
    std::string desc = "Detect Rooms";
    
    int total = static_cast<int>(m_createdRooms.size());
    if (m_splitCount > 0 && total > 0) {
        desc += " (created " + std::to_string(total) + 
                ", split " + std::to_string(m_splitCount) + ")";
    } else if (total > 0) {
        desc += " (created " + std::to_string(total) + ")";
    } else if (m_splitCount > 0) {
        desc += " (split " + std::to_string(m_splitCount) + ")";
    }
    
    return desc;
}

} // namespace Cartograph

