#include "MarkerKinds.h"

namespace Cartograph {

const std::vector<MarkerKindInfo>& MarkerKinds() {
    using T = MarkerField::Type;
    static const std::vector<MarkerKindInfo> kinds = {
        {kKindCustom, "Custom", "", Color(0.3f, 0.8f, 0.3f, 1.0f), {}},
        {"bench", "Bench", "bench", Color(0.4f, 0.7f, 1.0f, 1.0f), {}},
        {"chest", "Chest", "chest", Color(0.9f, 0.7f, 0.2f, 1.0f),
         {{"contents", "Contents", T::Text}}},
        {"boss", "Boss", "Skull", Color(0.9f, 0.2f, 0.2f, 1.0f),
         {{"name", "Boss name", T::Text}}},
        {kKindAbilityPickup, "Ability pickup", "dot",
         Color(1.0f, 0.8f, 0.2f, 1.0f),
         {{kPropAbility, "Ability", T::Ability}}},
        {"enemy", "Enemy", "dot", Color(0.9f, 0.4f, 0.3f, 1.0f),
         {{"type", "Enemy type", T::Text}}},
        {kKindStart, "Map start", "dot", Color(0.2f, 1.0f, 0.4f, 1.0f), {}},
    };
    return kinds;
}

const MarkerKindInfo* FindMarkerKind(const std::string& id) {
    for (const auto& kind : MarkerKinds()) {
        if (id == kind.id) return &kind;
    }
    return nullptr;
}

}  // namespace Cartograph
