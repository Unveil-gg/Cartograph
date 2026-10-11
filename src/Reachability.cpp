// Staged flood fill over rooms and gated, possibly one-way, doors.

#include "Reachability.h"

#include "MarkerKinds.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <unordered_map>

namespace Cartograph {

namespace {

struct Arc {
    std::string from;
    std::string to;
    std::string gate;
};

using RoomSet = std::set<std::string>;

// Room under a marker (markers use cell units, so floor gives the cell).
std::string MarkerRoom(const Model& model, const Marker& marker) {
    return model.GetCellRoom(
        static_cast<int>(std::floor(marker.x)),
        static_cast<int>(std::floor(marker.y)));
}

// Directed arcs for every door edge joining two different rooms.
std::vector<Arc> BuildArcs(const Model& model) {
    std::vector<Arc> arcs;
    for (const auto& [edgeId, state] : model.edges) {
        if (state != EdgeState::Door) continue;
        std::string a = model.GetCellRoom(edgeId.x1, edgeId.y1);
        std::string b = model.GetCellRoom(edgeId.x2, edgeId.y2);
        if (a.empty() || b.empty() || a == b) continue;
        const DoorProps* dp = model.GetDoorProps(edgeId);
        std::string gate = dp ? dp->gate : "";
        OneWay oneWay = dp ? dp->oneWay : OneWay::None;
        if (oneWay != OneWay::BToA) arcs.push_back({a, b, gate});
        if (oneWay != OneWay::AToB) arcs.push_back({b, a, gate});
    }
    return arcs;
}

// Rooms reachable from `from` using arcs whose gate is open under `held`.
// With reverse=true arcs are walked backwards (who can reach `from`).
RoomSet Flood(
    const std::string& from,
    const std::vector<Arc>& arcs,
    const std::set<std::string>& held,
    bool reverse
) {
    std::unordered_map<std::string, std::vector<const Arc*>> adj;
    for (const auto& arc : arcs) {
        if (!arc.gate.empty() && !held.count(arc.gate)) continue;
        adj[reverse ? arc.to : arc.from].push_back(&arc);
    }
    RoomSet seen;
    if (from.empty()) return seen;
    std::vector<std::string> stack{from};
    seen.insert(from);
    while (!stack.empty()) {
        std::string room = stack.back();
        stack.pop_back();
        auto it = adj.find(room);
        if (it == adj.end()) continue;
        for (const Arc* arc : it->second) {
            const std::string& next = reverse ? arc->from : arc->to;
            if (seen.insert(next).second) stack.push_back(next);
        }
    }
    return seen;
}

// Pickup markers (with their rooms) for one ability id.
std::vector<const Marker*> PickupsFor(const Model& model,
                                      const std::string& ability) {
    std::vector<const Marker*> out;
    for (const auto& m : model.markers) {
        if (m.kind != kKindAbilityPickup) continue;
        auto it = m.props.find(kPropAbility);
        if (it != m.props.end() && it->second == ability) out.push_back(&m);
    }
    return out;
}

}  // namespace

bool ValidationReport::HasErrors() const {
    if (startCount != 1 || startOutsideRoom) return true;
    if (!unreachableRoomIds.empty() || !trappedRooms.empty()) return true;
    if (!unknownGates.empty()) return true;
    return std::any_of(pickupIssues.begin(), pickupIssues.end(),
                       [](const PickupIssue& p) { return p.isError; });
}

ValidationReport ValidateReachability(
    const Model& model,
    const std::vector<std::string>& orderIn
) {
    ValidationReport report;
    if (orderIn.empty()) {
        for (const auto& a : model.abilities) {
            report.abilityOrder.push_back(a.id);
        }
    } else {
        report.abilityOrder = orderIn;
    }
    const auto& order = report.abilityOrder;

    // Start marker
    const Marker* start = nullptr;
    for (const auto& m : model.markers) {
        if (m.kind != kKindStart) continue;
        ++report.startCount;
        if (!start) start = &m;
    }
    if (!start) return report;
    report.startRoomId = MarkerRoom(model, *start);
    if (report.startRoomId.empty()) {
        report.startOutsideRoom = true;
        return report;
    }

    const std::vector<Arc> arcs = BuildArcs(model);

    // Gates that no ability in the order can ever open
    std::set<std::string> known(order.begin(), order.end());
    std::set<std::string> unknown;
    for (const auto& arc : arcs) {
        if (!arc.gate.empty() && !known.count(arc.gate)) {
            unknown.insert(arc.gate);
        }
    }
    report.unknownGates.assign(unknown.begin(), unknown.end());

    const int stages = static_cast<int>(order.size());
    std::set<std::string> trappedSeen;
    std::set<std::string> finalHeld(order.begin(), order.end());

    for (int stage = 0; stage <= stages; ++stage) {
        std::set<std::string> held(order.begin(), order.begin() + stage);
        RoomSet reach = Flood(report.startRoomId, arcs, held, false);

        // Targets that move the player on: the start room, plus the room
        // of every pickup for the next ability.
        std::vector<std::string> targets{report.startRoomId};
        if (stage < stages) {
            for (const Marker* m : PickupsFor(model, order[stage])) {
                std::string room = MarkerRoom(model, *m);
                if (reach.count(room)) targets.push_back(room);
            }
        }
        RoomSet canProgress;
        for (const auto& target : targets) {
            RoomSet back = Flood(target, arcs, held, true);
            canProgress.insert(back.begin(), back.end());
        }
        for (const auto& room : reach) {
            if (canProgress.count(room) || trappedSeen.count(room)) continue;
            trappedSeen.insert(room);
            report.trappedRooms.push_back({room, stage});
        }
    }

    // Rooms never reached with every ability
    RoomSet finalReach = Flood(report.startRoomId, arcs, finalHeld, false);
    std::set<std::string> roomsWithCells;
    for (const auto& [cell, roomId] : model.cellRoomAssignments) {
        roomsWithCells.insert(roomId);
    }
    for (const auto& room : model.rooms) {
        if (roomsWithCells.count(room.id) && !finalReach.count(room.id)) {
            report.unreachableRoomIds.push_back(room.id);
        }
    }

    // Ability pickups: placed, known, and reachable in order
    for (int i = 0; i < stages; ++i) {
        const std::string& ability = order[i];
        auto pickups = PickupsFor(model, ability);
        if (pickups.empty()) {
            report.pickupIssues.push_back(
                {"", ability, "no pickup placed for this ability", false});
            continue;
        }
        std::set<std::string> held(order.begin(), order.begin() + i);
        RoomSet reach = Flood(report.startRoomId, arcs, held, false);
        bool ok = false;
        bool laterOk = false;
        for (const Marker* m : pickups) {
            std::string room = MarkerRoom(model, *m);
            if (reach.count(room)) ok = true;
            if (finalReach.count(room)) laterOk = true;
        }
        if (ok) continue;
        report.pickupIssues.push_back(
            {pickups[0]->id, ability,
             laterOk ? "not reachable with only the abilities before it"
                     : "not reachable from the start at all",
             true});
    }
    for (const auto& m : model.markers) {
        if (m.kind != kKindAbilityPickup) continue;
        auto it = m.props.find(kPropAbility);
        if (it == m.props.end() || it->second.empty()) {
            report.pickupIssues.push_back(
                {m.id, "", "ability pickup has no ability set", true});
        } else if (!known.count(it->second)) {
            report.pickupIssues.push_back(
                {m.id, it->second, "ability is not in the order list",
                 true});
        }
    }
    return report;
}

}  // namespace Cartograph
