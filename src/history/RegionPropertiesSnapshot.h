#pragma once

#include <string>
#include <vector>

namespace Cartograph {

/**
 * Snapshot of region properties for undo/redo.
 */
struct RegionPropertiesSnapshot {
    std::string name;
    std::string description;
    std::vector<std::string> tags;
    
    bool operator==(const RegionPropertiesSnapshot& other) const {
        return name == other.name && description == other.description &&
               tags == other.tags;
    }
    
    bool operator!=(const RegionPropertiesSnapshot& other) const {
        return !(*this == other);
    }
};

} // namespace Cartograph

