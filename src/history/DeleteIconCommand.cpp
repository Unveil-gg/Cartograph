#include "DeleteIconCommand.h"
#include "../Icons.h"

namespace Cartograph {

DeleteIconCommand::DeleteIconCommand(const std::string& iconName, 
                                     bool removeMarkers,
                                     IconManager& iconManager)
    : m_iconName(iconName)
    , m_removeMarkers(removeMarkers)
    , m_iconManager(iconManager)
    , m_savedWidth(0)
    , m_savedHeight(0)
    , m_iconCaptured(false) {}

void DeleteIconCommand::CaptureIconState() {
    if (!m_iconCaptured) {
        m_iconManager.GetIconData(m_iconName, m_savedPixels, m_savedWidth, 
                                 m_savedHeight, m_savedCategory);
        m_iconCaptured = true;
    }
}

void DeleteIconCommand::Execute(Model& model) {
    // Capture marker state before deleting (only first time)
    if (m_removeMarkers && m_deletedMarkers.empty()) {
        // Get list of markers using this icon
        std::vector<std::string> markerIds = 
            model.GetMarkersUsingIcon(m_iconName);
        
        // Save marker state for undo
        for (const auto& id : markerIds) {
            const Marker* marker = model.FindMarker(id);
            if (marker) {
                m_deletedMarkers.push_back(*marker);
            }
        }
    }
    
    // Delete markers if requested
    if (m_removeMarkers) {
        model.RemoveMarkersUsingIcon(m_iconName);
    }
    
    // Note: Icon deletion is handled by IconManager, not Model
    // The UI layer must call icons.DeleteIcon() separately
}

void DeleteIconCommand::Undo(Model& model) {
    // Restore icon to IconManager
    if (m_iconCaptured && !m_savedPixels.empty()) {
        m_iconManager.AddIconFromMemory(
            m_iconName,
            m_savedPixels.data(),
            m_savedWidth,
            m_savedHeight,
            m_savedCategory
        );
        m_iconManager.BuildAtlas();
    }
    
    // Restore deleted markers
    for (const auto& marker : m_deletedMarkers) {
        model.AddMarker(marker);
    }
}

std::string DeleteIconCommand::GetDescription() const {
    if (m_removeMarkers && !m_deletedMarkers.empty()) {
        return "Delete Icon (+ " + std::to_string(m_deletedMarkers.size()) + 
               " markers)";
    }
    return "Delete Icon";
}

} // namespace Cartograph

