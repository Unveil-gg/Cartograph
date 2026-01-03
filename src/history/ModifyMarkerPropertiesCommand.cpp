#include "ModifyMarkerPropertiesCommand.h"
#include "Constants.h"

namespace Cartograph {

ModifyMarkerPropertiesCommand::ModifyMarkerPropertiesCommand(
    const std::string& markerId,
    const MarkerPropertiesSnapshot& oldProps,
    const MarkerPropertiesSnapshot& newProps
) : m_markerId(markerId), m_oldProps(oldProps), m_newProps(newProps) {}

void ModifyMarkerPropertiesCommand::Execute(Model& model) {
    Marker* marker = model.FindMarker(m_markerId);
    if (marker) {
        marker->label = m_newProps.label;
        marker->icon = m_newProps.icon;
        marker->color = m_newProps.color;
        marker->showLabel = m_newProps.showLabel;
        model.MarkDirty();
    }
}

void ModifyMarkerPropertiesCommand::Undo(Model& model) {
    Marker* marker = model.FindMarker(m_markerId);
    if (marker) {
        marker->label = m_oldProps.label;
        marker->icon = m_oldProps.icon;
        marker->color = m_oldProps.color;
        marker->showLabel = m_oldProps.showLabel;
        model.MarkDirty();
    }
}

std::string ModifyMarkerPropertiesCommand::GetDescription() const {
    return "Modify Marker";
}

bool ModifyMarkerPropertiesCommand::TryCoalesce(
    ICommand* other,
    uint64_t timeDelta,
    float /*distanceSq*/) {
    // Only coalesce within time threshold
    if (timeDelta > PROPERTY_COALESCE_TIME_MS) {
        return false;
    }
    
    auto* otherCmd = dynamic_cast<ModifyMarkerPropertiesCommand*>(other);
    if (!otherCmd) {
        return false;
    }
    
    // Only coalesce if same marker
    if (m_markerId != otherCmd->m_markerId) {
        return false;
    }
    
    // Keep original oldProps, update to latest newProps
    m_newProps = otherCmd->m_newProps;
    return true;
}

} // namespace Cartograph

