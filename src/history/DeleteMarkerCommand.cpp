#include "DeleteMarkerCommand.h"

namespace Cartograph {

DeleteMarkerCommand::DeleteMarkerCommand(
    const std::vector<std::string>& markerIds
) {
    m_markerIds = markerIds;
}

DeleteMarkerCommand::DeleteMarkerCommand(const std::string& markerId) {
    m_markerIds.push_back(markerId);
}

void DeleteMarkerCommand::Execute(Model& model) {
    // Capture marker state before deleting (for undo, only first time)
    if (m_deletedMarkers.empty()) {
        // First execution - capture state of markers to delete
        for (const auto& id : m_markerIds) {
            const Marker* marker = model.FindMarker(id);
            if (marker) {
                m_deletedMarkers.push_back(*marker);
            }
        }
    }
    
    // Delete markers
    for (const auto& id : m_markerIds) {
        model.RemoveMarker(id);
    }
}

void DeleteMarkerCommand::Undo(Model& model) {
    // Restore deleted markers
    for (const auto& marker : m_deletedMarkers) {
        model.AddMarker(marker);
    }
}

std::string DeleteMarkerCommand::GetDescription() const {
    if (m_deletedMarkers.size() == 1) {
        return "Delete Marker";
    } else {
        return "Delete " + std::to_string(m_deletedMarkers.size()) + 
               " Markers";
    }
}

} // namespace Cartograph

