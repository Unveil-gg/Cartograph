#include "ModifyRegionPropertiesCommand.h"
#include "Constants.h"

namespace Cartograph {

ModifyRegionPropertiesCommand::ModifyRegionPropertiesCommand(
    const std::string& regionId,
    const RegionPropertiesSnapshot& oldProps,
    const RegionPropertiesSnapshot& newProps
) : m_regionId(regionId), m_oldProps(oldProps), m_newProps(newProps) {}

void ModifyRegionPropertiesCommand::Execute(Model& model) {
    RegionGroup* region = model.FindRegionGroup(m_regionId);
    if (region) {
        region->name = m_newProps.name;
        region->description = m_newProps.description;
        region->tags = m_newProps.tags;
        model.MarkDirty();
    }
}

void ModifyRegionPropertiesCommand::Undo(Model& model) {
    RegionGroup* region = model.FindRegionGroup(m_regionId);
    if (region) {
        region->name = m_oldProps.name;
        region->description = m_oldProps.description;
        region->tags = m_oldProps.tags;
        model.MarkDirty();
    }
}

std::string ModifyRegionPropertiesCommand::GetDescription() const {
    return "Modify Region: " + m_newProps.name;
}

bool ModifyRegionPropertiesCommand::TryCoalesce(
    ICommand* other,
    uint64_t timeDelta,
    float /*distanceSq*/
) {
    if (timeDelta > PROPERTY_COALESCE_TIME_MS) {
        return false;
    }
    
    auto* otherCmd = dynamic_cast<ModifyRegionPropertiesCommand*>(other);
    if (!otherCmd) {
        return false;
    }
    
    if (m_regionId != otherCmd->m_regionId) {
        return false;
    }
    
    m_newProps = otherCmd->m_newProps;
    return true;
}

} // namespace Cartograph

