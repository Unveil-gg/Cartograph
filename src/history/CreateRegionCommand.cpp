#include "CreateRegionCommand.h"
#include <algorithm>

namespace Cartograph {

CreateRegionCommand::CreateRegionCommand(const RegionGroup& region)
    : m_region(region) {}

void CreateRegionCommand::Execute(Model& model) {
    model.regionGroups.push_back(m_region);
    model.MarkDirty();
}

void CreateRegionCommand::Undo(Model& model) {
    auto it = std::find_if(
        model.regionGroups.begin(),
        model.regionGroups.end(),
        [this](const RegionGroup& r) { return r.id == m_region.id; }
    );
    if (it != model.regionGroups.end()) {
        model.regionGroups.erase(it);
    }
    model.MarkDirty();
}

std::string CreateRegionCommand::GetDescription() const {
    return "Create Region: " + m_region.name;
}

} // namespace Cartograph

