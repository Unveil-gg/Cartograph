#include "DeleteRegionCommand.h"
#include <algorithm>

namespace Cartograph {

DeleteRegionCommand::DeleteRegionCommand(const std::string& regionId)
    : m_regionId(regionId) {}

void DeleteRegionCommand::Execute(Model& model) {
    // Save region data (only first time)
    if (m_savedRegion.id.empty()) {
        const RegionGroup* region = model.FindRegionGroup(m_regionId);
        if (region) {
            m_savedRegion = *region;
        }
        
        // Save room assignments and clear them
        for (auto& room : model.rooms) {
            if (room.parentRegionGroupId == m_regionId) {
                m_orphanedRoomIds.push_back(room.id);
            }
        }
    }
    
    // Unassign rooms from this region
    for (auto& room : model.rooms) {
        if (room.parentRegionGroupId == m_regionId) {
            room.parentRegionGroupId = "";
        }
    }
    
    // Remove region from model
    auto it = std::find_if(
        model.regionGroups.begin(),
        model.regionGroups.end(),
        [this](const RegionGroup& r) { return r.id == m_regionId; }
    );
    if (it != model.regionGroups.end()) {
        model.regionGroups.erase(it);
    }
    
    model.MarkDirty();
}

void DeleteRegionCommand::Undo(Model& model) {
    // Restore the region
    model.regionGroups.push_back(m_savedRegion);
    
    // Restore room assignments
    for (const auto& roomId : m_orphanedRoomIds) {
        Room* room = model.FindRoom(roomId);
        if (room) {
            room->parentRegionGroupId = m_regionId;
        }
    }
    
    model.MarkDirty();
}

std::string DeleteRegionCommand::GetDescription() const {
    return "Delete Region: " + m_savedRegion.name;
}

} // namespace Cartograph

