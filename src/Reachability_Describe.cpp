// Text rendering of a ValidationReport for the CLI and the panel.

#include "Reachability.h"

namespace Cartograph {

namespace {

std::string RoomName(const Model& model, const std::string& id) {
    const Room* room = model.FindRoom(id);
    return (room && !room->name.empty()) ? room->name : id;
}

}  // namespace

std::vector<std::string> DescribeReport(
    const Model& model, const ValidationReport& r
) {
    std::vector<std::string> lines;
    if (r.startCount == 0) {
        lines.push_back("ERROR: no start marker (kind \"start\")");
        return lines;
    }
    if (r.startCount > 1) {
        lines.push_back("ERROR: " + std::to_string(r.startCount) +
                        " start markers, expected 1");
    }
    if (r.startOutsideRoom) {
        lines.push_back("ERROR: start marker is not inside a room");
        return lines;
    }
    for (const auto& g : r.unknownGates) {
        lines.push_back("ERROR: door gate \"" + g +
                        "\" is not in the ability order");
    }
    for (const auto& id : r.unreachableRoomIds) {
        lines.push_back("ERROR: room \"" + RoomName(model, id) +
                        "\" is unreachable");
    }
    for (const auto& t : r.trappedRooms) {
        std::string held = t.stage == 0
            ? "no abilities"
            : "abilities up to \"" +
                  model.AbilityName(r.abilityOrder[t.stage - 1]) + "\"";
        lines.push_back("ERROR: room \"" + RoomName(model, t.roomId) +
                        "\" is a softlock with " + held);
    }
    for (const auto& p : r.pickupIssues) {
        std::string who = p.ability.empty()
            ? "pickup" : "pickup for \"" + model.AbilityName(p.ability) + "\"";
        lines.push_back(std::string(p.isError ? "ERROR: " : "WARN: ") +
                        who + ": " + p.message);
    }
    if (lines.empty()) lines.push_back("OK: no problems found");
    return lines;
}

}  // namespace Cartograph
