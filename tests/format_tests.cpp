// Round-trip and load-guard checks for the version 3 sections.

#include "TestUtil.h"

#include "IOJson.h"
#include "Model.h"

using testutil::Expect;
using namespace Cartograph;

namespace {

Model Reload(const Model& model) {
    Model loaded;
    Expect(IOJson::LoadFromString(IOJson::SaveToString(model), loaded),
           "saved model reloads");
    return loaded;
}

/** Props tables on rooms, regions and markers survive a round trip. */
void TestProps() {
    Model m;
    Room room;
    room.id = "r1";
    room.props["kind"] = "arena";
    room.props["dark"] = "true";
    m.rooms.push_back(room);
    RegionGroup g;
    g.id = "g1";
    g.props["music"] = "tunnel";
    m.regionGroups.push_back(g);
    Marker mk;
    mk.id = "m1";
    mk.kind = "enemy";
    mk.props["type"] = "bat";
    m.markers.push_back(mk);

    Model l = Reload(m);
    Expect(l.rooms.at(0).props.at("kind") == "arena", "room props kept");
    Expect(l.rooms.at(0).props.at("dark") == "true", "room bool prop kept");
    Expect(l.regionGroups.at(0).props.at("music") == "tunnel",
           "region props kept");
    Expect(l.markers.at(0).props.at("type") == "bat", "marker props kept");
}

/** Non-string scalars are stringified, nested values dropped. */
void TestPropsLenient() {
    const char* json =
        "{\"version\":3,\"rooms\":[{\"id\":\"r\",\"props\":"
        "{\"a\":true,\"b\":3,\"c\":{\"x\":1},\"\":\"empty key\"}}]}";
    Model m;
    Expect(IOJson::LoadFromString(json, m), "lenient props load");
    const auto& p = m.rooms.at(0).props;
    Expect(p.at("a") == "true", "bool becomes string");
    Expect(p.at("b") == "3", "number becomes string");
    Expect(p.count("c") == 0, "object prop dropped");
    Expect(p.count("") == 0, "empty key dropped");
}

/** Door props and abilities round-trip; orphans are not saved. */
void TestDoorsAndAbilities() {
    Model m;
    m.abilities.push_back({"dash", "Dash", Color(1, 0, 0, 1)});
    m.abilities.push_back({"double-jump", "Double Jump", Color(0, 1, 0, 1)});
    EdgeId door(0, 0, 1, 0);
    EdgeId orphan(5, 5, 6, 5);
    m.SetEdgeState(door, EdgeState::Door);
    DoorProps dp;
    dp.gate = "dash";
    dp.oneWay = OneWay::AToB;
    dp.spawnA = Spawn{0.5f, 0.5f};
    dp.props["locked"] = "yes";
    m.SetDoorProps(door, dp);
    DoorProps op;
    op.gate = "dash";
    m.SetDoorProps(orphan, op);

    Model l = Reload(m);
    Expect(l.abilities.size() == 2, "abilities kept");
    Expect(l.abilities.at(1).id == "double-jump", "ability order kept");
    const DoorProps* got = l.GetDoorProps(door);
    Expect(got != nullptr, "door props kept");
    if (got) {
        Expect(*got == dp, "door props identical");
    }
    Expect(l.GetDoorProps(orphan) == nullptr, "orphan door props dropped");
}

/** Interior chunks round-trip with north-first rows. */
void TestInterior() {
    Model m;
    m.SetInteriorTile(0, 8, InteriorTile::Solid);   // cell (0,0), top row
    m.SetInteriorTile(15, 0, InteriorTile::Water);  // bottom-right
    m.SetInteriorTile(-1, -1, InteriorTile::Ladder);  // cell (-1,-1)
    Expect(m.interior.size() == 2, "two chunks created");

    std::string json = IOJson::SaveToString(m);
    Expect(json.find("#...............") != std::string::npos,
           "north row written first");
    Model l = Reload(m);
    Expect(l.GetInteriorTile(0, 8) == InteriorTile::Solid, "solid kept");
    Expect(l.GetInteriorTile(15, 0) == InteriorTile::Water, "water kept");
    Expect(l.GetInteriorTile(-1, -1) == InteriorTile::Ladder,
           "negative cell kept");
    Expect(l.GetInteriorTile(1, 1) == InteriorTile::Empty, "empty kept");

    m.SetInteriorTile(0, 8, InteriorTile::Empty);
    m.SetInteriorTile(15, 0, InteriorTile::Empty);
    m.SetInteriorTile(-1, -1, InteriorTile::Empty);
    Expect(m.interior.empty(), "empty chunks are dropped");
}

/** Interior size is locked once painted. */
void TestInteriorSize() {
    Model m;
    Expect(m.SetInteriorSize(20, 12), "resize while empty");
    m.SetInteriorTile(0, 0, InteriorTile::Solid);
    Expect(!m.SetInteriorSize(8, 8), "resize refused when painted");
    Expect(!m.SetInteriorSize(0, 8), "zero size refused");
    Model l = Reload(m);
    Expect(l.interiorConfig.width == 20 && l.interiorConfig.height == 12,
           "interior size kept");
}

/** Bad interior data is skipped without failing the file. */
void TestInteriorGuards() {
    const char* json =
        "{\"version\":3,\"interior\":{\"width\":4,\"height\":2,\"chunks\":["
        "{\"cell\":[0,0],\"rows\":[\"#...\"]},"
        "{\"cell\":[\"a\",0],\"rows\":[\"....\",\"....\"]},"
        "{\"cell\":[1,0],\"rows\":[\"##..\",\"..?.\"]}]}}";
    Model m;
    Expect(IOJson::LoadFromString(json, m), "bad interior still loads");
    Expect(m.interior.size() == 1, "only the valid chunk is kept");
    Expect(m.GetInteriorTile(4, 1) == InteriorTile::Solid, "valid chunk ok");
}

/** Version 2 files load with empty version 3 sections. */
void TestV2Compat() {
    Model m;
    Expect(IOJson::LoadFromString(
               "{\"version\":2,\"grid\":{\"cols\":4,\"rows\":4}}", m),
           "v2 loads");
    Expect(m.abilities.empty() && m.doorProps.empty() && m.interior.empty(),
           "v3 sections default to empty");
}

}  // namespace

int main() {
    TestProps();
    TestPropsLenient();
    TestDoorsAndAbilities();
    TestInterior();
    TestInteriorSize();
    TestInteriorGuards();
    TestV2Compat();
    return testutil::Finish("format_tests");
}
