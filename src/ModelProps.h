#pragma once

#include "Color.h"

#include <map>
#include <optional>
#include <string>

namespace Cartograph {

// Free-form key/value metadata (kind=arena, music=tunnel, ...).
// Ordered so saved JSON is stable.
using Props = std::map<std::string, std::string>;

// Which way a door can be walked. "a" is the cell (x1, y1) of the
// normalized EdgeId, "b" is (x2, y2).
enum class OneWay {
    None,  // Both directions
    AToB,  // Only from a to b
    BToA   // Only from b to a
};

// Where the player appears after coming through a door, in cell units
// (same convention as Marker::x / Marker::y).
struct Spawn {
    float x = 0.0f;
    float y = 0.0f;

    bool operator==(const Spawn& o) const { return x == o.x && y == o.y; }
};

// Extra data for a door edge (EdgeState::Door).
struct DoorProps {
    std::string gate;             // Ability id, empty = open
    OneWay oneWay = OneWay::None;
    std::optional<Spawn> spawnA;  // Arrival point in cell a
    std::optional<Spawn> spawnB;  // Arrival point in cell b
    Props props;

    /**
     * True when nothing is set, so the entry need not be saved.
     * @return true for a default-valued DoorProps
     */
    bool IsDefault() const {
        return gate.empty() && oneWay == OneWay::None && !spawnA &&
               !spawnB && props.empty();
    }

    bool operator==(const DoorProps& o) const {
        return gate == o.gate && oneWay == o.oneWay &&
               spawnA == o.spawnA && spawnB == o.spawnB &&
               props == o.props;
    }
    bool operator!=(const DoorProps& o) const { return !(*this == o); }
};

/**
 * Copy door props with both spawns translated (for move/copy/paste).
 * @param props Source props
 * @param dx Shift in cells, X
 * @param dy Shift in cells, Y
 * @return Shifted copy
 */
inline DoorProps ShiftedDoorProps(const DoorProps& props, float dx, float dy) {
    DoorProps out = props;
    if (out.spawnA) { out.spawnA->x += dx; out.spawnA->y += dy; }
    if (out.spawnB) { out.spawnB->x += dx; out.spawnB->y += dy; }
    return out;
}

// A project-level ability (dash, double-jump, ...). List order is the
// intended acquisition order.
struct Ability {
    std::string id;
    std::string name;
    Color color = Color(1.0f, 0.8f, 0.2f, 1.0f);
};

}  // namespace Cartograph
