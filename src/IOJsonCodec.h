#pragma once

#include "Model.h"

#include <string>

namespace Cartograph {

inline std::string GridPresetToString(GridPreset preset) {
    switch (preset) {
        case GridPreset::Square: return "square";
        case GridPreset::Rectangle: return "rectangle";
        default: return "square";
    }
}

// Helper: Parse GridPreset from string
inline GridPreset GridPresetFromString(const std::string& str) {
    if (str == "rectangle") return GridPreset::Rectangle;
    return GridPreset::Square;
}

// Helper: Convert DoorSide to string
inline std::string DoorSideToString(DoorSide side) {
    switch (side) {
        case DoorSide::North: return "N";
        case DoorSide::South: return "S";
        case DoorSide::East: return "E";
        case DoorSide::West: return "W";
        default: return "N";
    }
}

// Helper: Parse DoorSide from string
inline DoorSide DoorSideFromString(const std::string& str) {
    if (str == "S") return DoorSide::South;
    if (str == "E") return DoorSide::East;
    if (str == "W") return DoorSide::West;
    return DoorSide::North;
}

// Helper: Convert DoorType to string
inline std::string DoorTypeToString(DoorType type) {
    switch (type) {
        case DoorType::Door: return "door";
        case DoorType::Elevator: return "elevator";
        case DoorType::Teleporter: return "teleporter";
        default: return "door";
    }
}

// Helper: Parse DoorType from string
inline DoorType DoorTypeFromString(const std::string& str) {
    if (str == "elevator") return DoorType::Elevator;
    if (str == "teleporter") return DoorType::Teleporter;
    return DoorType::Door;
}

}  // namespace Cartograph
