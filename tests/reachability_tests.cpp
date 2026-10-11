// Gate validation: gated doors, one-way traps, ordering, missing start.

#include "TestUtil.h"

#include "MarkerKinds.h"
#include "Model.h"
#include "Reachability.h"

using testutil::Expect;
using namespace Cartograph;

namespace {

// Rooms "A", "B", "C" in a row along y=0, one cell each. Doors A|B and
// B|C. The start marker is in A.
Model MakeCorridor() {
    Model m;
    const char* ids[] = {"A", "B", "C"};
    for (int i = 0; i < 3; ++i) {
        Room r;
        r.id = ids[i];
        r.name = ids[i];
        m.rooms.push_back(r);
        m.cellRoomAssignments[{i, 0}] = ids[i];
    }
    m.SetEdgeState(EdgeId(0, 0, 1, 0), EdgeState::Door);
    m.SetEdgeState(EdgeId(1, 0, 2, 0), EdgeState::Door);
    m.abilities.push_back({"dash", "Dash", Color()});
    m.abilities.push_back({"jump", "Double Jump", Color()});

    Marker start;
    start.id = "start";
    start.kind = kKindStart;
    start.x = 0.5f;
    start.y = 0.5f;
    m.markers.push_back(start);
    return m;
}

void AddPickup(Model& m, const std::string& id, const std::string& ability,
               int cellX) {
    Marker p;
    p.id = id;
    p.kind = kKindAbilityPickup;
    p.x = cellX + 0.5f;
    p.y = 0.5f;
    p.props[kPropAbility] = ability;
    m.markers.push_back(p);
}

void Gate(Model& m, const EdgeId& edge, const std::string& gate,
          OneWay oneWay = OneWay::None) {
    DoorProps dp;
    dp.gate = gate;
    dp.oneWay = oneWay;
    m.SetDoorProps(edge, dp);
}

/** A well-ordered map has no errors. */
void TestClean() {
    Model m = MakeCorridor();
    Gate(m, EdgeId(0, 0, 1, 0), "dash");
    Gate(m, EdgeId(1, 0, 2, 0), "jump");
    AddPickup(m, "p1", "dash", 0);
    AddPickup(m, "p2", "jump", 1);  // Behind the dash door
    auto report = ValidateReachability(m);
    Expect(!report.HasErrors(), "ordered map validates");
    Expect(report.unreachableRoomIds.empty(), "everything reachable");
    Expect(DescribeReport(m, report).at(0).rfind("OK", 0) == 0,
           "clean report says OK");
}

/** A pickup behind a door that needs it is an ordering error. */
void TestOrderingError() {
    Model m = MakeCorridor();
    Gate(m, EdgeId(0, 0, 1, 0), "dash");
    AddPickup(m, "p1", "dash", 1);  // In B, behind the dash gate
    AddPickup(m, "p2", "jump", 0);
    auto report = ValidateReachability(m);
    Expect(report.HasErrors(), "self-gated pickup is an error");
    bool found = false;
    for (const auto& p : report.pickupIssues) {
        if (p.ability == "dash" && p.isError) found = true;
    }
    Expect(found, "pickup issue names the ability");
}

/** A room only reachable through a never-opened gate is unreachable. */
void TestUnreachable() {
    Model m = MakeCorridor();
    Gate(m, EdgeId(1, 0, 2, 0), "jump");
    AddPickup(m, "p1", "dash", 0);
    // No pickup is needed for the check, but "jump" is never obtainable
    // by order [dash]: the door is an unknown gate for that order.
    auto report = ValidateReachability(m, {"dash"});
    Expect(report.unknownGates.size() == 1, "gate outside order flagged");
    Expect(report.unreachableRoomIds.size() == 1 &&
               report.unreachableRoomIds[0] == "C",
           "room behind unopenable gate is unreachable");
}

/** A one-way door with no way back traps the player. */
void TestOneWayTrap() {
    Model m = MakeCorridor();
    Gate(m, EdgeId(0, 0, 1, 0), "", OneWay::AToB);
    AddPickup(m, "p1", "dash", 0);
    AddPickup(m, "p2", "jump", 0);
    auto report = ValidateReachability(m);
    Expect(!report.trappedRooms.empty(), "one-way drop traps the player");
    bool bTrapped = false;
    for (const auto& t : report.trappedRooms) {
        if (t.roomId == "B") bTrapped = true;
    }
    Expect(bTrapped, "room B is trapped");
}

/** One-way into a room that holds the next pickup is fine. */
void TestOneWayWithPickup() {
    Model m = MakeCorridor();
    // C is not part of this check: remove its door.
    m.SetEdgeState(EdgeId(1, 0, 2, 0), EdgeState::None);
    m.cellRoomAssignments.erase({2, 0});
    m.rooms.pop_back();
    m.abilities.pop_back();  // Only "dash"
    Gate(m, EdgeId(0, 0, 1, 0), "", OneWay::AToB);
    AddPickup(m, "p1", "dash", 1);    // Pickup in the drop room
    auto report = ValidateReachability(m);
    // At stage 0 the drop room holds the pickup, so the player can still
    // progress. Once every ability is held, B has no way back to A.
    Expect(report.trappedRooms.size() == 1 &&
               report.trappedRooms[0].roomId == "B" &&
               report.trappedRooms[0].stage == 1,
           "B traps only after the last ability");
}

/** Missing and duplicate starts are errors. */
void TestStart() {
    Model m = MakeCorridor();
    m.markers.clear();
    Expect(ValidateReachability(m).HasErrors(), "no start is an error");
    Marker s;
    s.kind = kKindStart;
    s.x = 0.5f;
    s.y = 0.5f;
    s.id = "s1";
    m.markers.push_back(s);
    s.id = "s2";
    m.markers.push_back(s);
    Expect(ValidateReachability(m).startCount == 2, "two starts counted");
    Expect(ValidateReachability(m).HasErrors(), "two starts is an error");
}

}  // namespace

int main() {
    TestClean();
    TestOrderingError();
    TestUnreachable();
    TestOneWayTrap();
    TestOneWayWithPickup();
    TestStart();
    return testutil::Finish("reachability_tests");
}
