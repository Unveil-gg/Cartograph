#pragma once

#include "Model.h"

#include <string>
#include <vector>

namespace Cartograph {

/**
 * Result of a gate/reachability check. No UI types, so the CLI and the
 * tests use it as well as the validation overlay.
 *
 * Model: rooms are nodes, door edges between cells of different rooms are
 * arcs (gate + one-way respected). Stage i holds the first i abilities of
 * the order list; the last stage holds all of them. A room is "trapped" at
 * a stage when it is reachable from the start but cannot reach the start
 * or the pickup of the next ability (the thing that moves progress on).
 */
struct ValidationReport {
    struct Trapped {
        std::string roomId;
        int stage = 0;  // First stage at which the room traps the player
    };
    struct PickupIssue {
        std::string markerId;  // Empty when the ability has no pickup
        std::string ability;   // Ability id (may be empty/unknown)
        std::string message;
        bool isError = true;
    };

    std::vector<std::string> abilityOrder;  // Order that was used
    std::string startRoomId;
    int startCount = 0;               // Markers of kind "start"
    bool startOutsideRoom = false;    // Start marker not on a room cell
    std::vector<std::string> unreachableRoomIds;  // Never reached
    std::vector<Trapped> trappedRooms;
    std::vector<PickupIssue> pickupIssues;
    std::vector<std::string> unknownGates;  // Gates not in the order list

    /**
     * True when anything should block export or fail CI.
     * @return true if any error-level finding exists
     */
    bool HasErrors() const;
};

/**
 * Check the map's gates against an ability order.
 * @param model Model to check (not modified)
 * @param order Ability ids in acquisition order; empty uses the
 *              model's ability registry order
 * @return Findings
 */
ValidationReport ValidateReachability(
    const Model& model,
    const std::vector<std::string>& order = {}
);

/**
 * Human-readable lines for a report ("ERROR: ...", "WARN: ...").
 * @param model Model for room and ability names
 * @param report Report to describe
 * @return One line per finding; a single "OK" line when clean
 */
std::vector<std::string> DescribeReport(
    const Model& model,
    const ValidationReport& report
);

}  // namespace Cartograph
