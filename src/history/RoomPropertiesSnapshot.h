#pragma once

#include "../Color.h"
#include <string>
#include <vector>

namespace Cartograph {

/**
 * Snapshot of room properties for undo/redo.
 * Used by ModifyRoomPropertiesCommand.
 */
struct RoomPropertiesSnapshot {
    std::string name;
    Color color;
    std::string notes;
    std::vector<std::string> tags;
    
    bool operator==(const RoomPropertiesSnapshot& other) const {
        return name == other.name &&
               color.r == other.color.r && color.g == other.color.g &&
               color.b == other.color.b && color.a == other.color.a &&
               notes == other.notes && tags == other.tags;
    }
    
    bool operator!=(const RoomPropertiesSnapshot& other) const {
        return !(*this == other);
    }
};

} // namespace Cartograph

