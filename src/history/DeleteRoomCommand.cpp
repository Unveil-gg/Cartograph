#include "DeleteRoomCommand.h"
#include <algorithm>

namespace Cartograph {

DeleteRoomCommand::DeleteRoomCommand(const std::string& roomId)
    : m_roomId(roomId) {}

void DeleteRoomCommand::Execute(Model& model) {
    // Find and save room data (only first time)
    if (m_savedRoom.id.empty()) {
        const Room* room = model.FindRoom(m_roomId);
        if (room) {
            m_savedRoom = *room;
        }
        
        // Save cell assignments for this room
        for (const auto& [cell, assignedRoomId] : model.cellRoomAssignments) {
            if (assignedRoomId == m_roomId) {
                m_savedCellAssignments.push_back(cell);
            }
        }
    }
    
    // Clear all cell assignments for this room
    model.ClearAllCellsForRoom(m_roomId);
    
    // Remove room from model
    auto it = std::find_if(
        model.rooms.begin(),
        model.rooms.end(),
        [this](const Room& r) { return r.id == m_roomId; }
    );
    if (it != model.rooms.end()) {
        model.rooms.erase(it);
    }
    
    model.MarkDirty();
}

void DeleteRoomCommand::Undo(Model& model) {
    // Restore the room
    model.rooms.push_back(m_savedRoom);
    
    // Restore cell assignments
    for (const auto& cell : m_savedCellAssignments) {
        model.cellRoomAssignments[cell] = m_roomId;
    }
    
    // Invalidate room cell cache
    model.InvalidateRoomCellCache(m_roomId);
    model.MarkDirty();
}

std::string DeleteRoomCommand::GetDescription() const {
    return "Delete Room: " + m_savedRoom.name;
}

} // namespace Cartograph

