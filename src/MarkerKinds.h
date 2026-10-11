#pragma once

#include "Color.h"

#include <string>
#include <vector>

namespace Cartograph {

// Marker kind ids the generator and validator rely on.
constexpr const char* kKindStart = "start";
constexpr const char* kKindAbilityPickup = "ability-pickup";
constexpr const char* kKindCustom = "custom";

// Prop key holding the ability granted by an ability-pickup marker.
constexpr const char* kPropAbility = "ability";

// One editable field of a marker kind, stored in Marker::props[key].
struct MarkerField {
    enum class Type { Text, Ability };

    const char* key;
    const char* label;
    Type type;
};

// A built-in marker kind: default look plus its field schema.
struct MarkerKindInfo {
    const char* id;
    const char* label;
    const char* icon;     // Default icon name ("" keeps the current icon)
    Color color;          // Default color
    std::vector<MarkerField> fields;
};

/**
 * All built-in marker kinds, in display order.
 * @return Kinds table (static storage)
 */
const std::vector<MarkerKindInfo>& MarkerKinds();

/**
 * Look up a kind by id.
 * @param id Kind id from Marker::kind
 * @return Kind info, or nullptr for unknown/legacy kinds
 */
const MarkerKindInfo* FindMarkerKind(const std::string& id);

}  // namespace Cartograph
