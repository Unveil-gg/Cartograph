#pragma once

#include "../Color.h"
#include <string>

namespace Cartograph {

/**
 * Snapshot of marker properties for undo/redo.
 * Used by ModifyMarkerPropertiesCommand.
 */
struct MarkerPropertiesSnapshot {
    std::string label;
    std::string icon;
    Color color;
    bool showLabel;
    
    bool operator==(const MarkerPropertiesSnapshot& other) const {
        return label == other.label && icon == other.icon &&
               color.r == other.color.r && color.g == other.color.g &&
               color.b == other.color.b && color.a == other.color.a &&
               showLabel == other.showLabel;
    }
    
    bool operator!=(const MarkerPropertiesSnapshot& other) const {
        return !(*this == other);
    }
};

} // namespace Cartograph

