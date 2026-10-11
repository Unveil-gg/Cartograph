// Door props and ability registry helpers for Model.

#include "Model.h"

namespace Cartograph {

/**
 * Look up door props for an edge.
 * @param edgeId Door edge
 * @return Props, or nullptr when the edge has none
 */
const DoorProps* Model::GetDoorProps(const EdgeId& edgeId) const {
    auto it = doorProps.find(edgeId);
    return it == doorProps.end() ? nullptr : &it->second;
}

/**
 * Set door props for an edge. Default-valued props erase the entry.
 * @param edgeId Door edge
 * @param props New props
 */
void Model::SetDoorProps(const EdgeId& edgeId, const DoorProps& props) {
    if (props.IsDefault()) {
        doorProps.erase(edgeId);
    } else {
        doorProps[edgeId] = props;
    }
    MarkDirty();
}

/**
 * Find an ability in the registry.
 * @param id Ability id
 * @return Ability, or nullptr when not registered
 */
const Ability* Model::FindAbility(const std::string& id) const {
    for (const auto& ability : abilities) {
        if (ability.id == id) return &ability;
    }
    return nullptr;
}

/**
 * Display name for an ability id.
 * @param id Ability id
 * @return Registered name, or the id itself when unknown
 */
std::string Model::AbilityName(const std::string& id) const {
    const Ability* ability = FindAbility(id);
    return (ability && !ability->name.empty()) ? ability->name : id;
}

}  // namespace Cartograph
