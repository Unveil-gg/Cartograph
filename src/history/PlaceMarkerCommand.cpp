#include "PlaceMarkerCommand.h"

namespace Cartograph {

PlaceMarkerCommand::PlaceMarkerCommand(const Marker& marker, bool isNew)
    : m_marker(marker), m_isNew(isNew)
{
    // If modifying existing marker, store old state for undo
    if (!isNew) {
        m_oldMarker = marker;  // Will be updated in Execute
    }
}

void PlaceMarkerCommand::Execute(Model& model) {
    if (m_isNew) {
        // Add new marker
        model.AddMarker(m_marker);
    } else {
        // Modify existing marker - save old state first
        const Marker* existing = model.FindMarker(m_marker.id);
        if (existing) {
            m_oldMarker = *existing;
        }
        
        // Update marker
        Marker* toUpdate = model.FindMarker(m_marker.id);
        if (toUpdate) {
            *toUpdate = m_marker;
            model.MarkDirty();
        }
    }
}

void PlaceMarkerCommand::Undo(Model& model) {
    if (m_isNew) {
        // Remove the marker we added
        model.RemoveMarker(m_marker.id);
    } else {
        // Restore old marker state
        Marker* toRestore = model.FindMarker(m_marker.id);
        if (toRestore) {
            *toRestore = m_oldMarker;
            model.MarkDirty();
        }
    }
}

std::string PlaceMarkerCommand::GetDescription() const {
    return m_isNew ? "Place Marker" : "Modify Marker";
}

} // namespace Cartograph

