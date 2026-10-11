// Load-path checks for project JSON, folder detection, and .cart paths.

#include "Config.h"
#include "IOJson.h"
#include "Model.h"
#include "Package.h"
#include "ProjectFolder.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace {

int g_failed = 0;

/**
 * Record a failed check and print it.
 * @param cond Result of the check
 * @param message What failed
 */
void Expect(bool cond, const std::string& message) {
    if (!cond) {
        std::cerr << "FAIL: " << message << "\n";
        ++g_failed;
    }
}

/**
 * Round-trip a small model through project.json version 2.
 */
void TestRoundTrip() {
    Cartograph::Model model;
    model.InitDefaultPalette();
    model.grid.cols = 8;
    model.grid.rows = 6;
    model.meta.title = "Round Trip";
    model.SetTileAt("", 1, 2, 1);
    model.SetEdgeState(
        Cartograph::EdgeId(1, 2, 2, 2), Cartograph::EdgeState::Wall);

    std::string json = Cartograph::IOJson::SaveToString(model);
    Cartograph::Model loaded;
    Expect(Cartograph::IOJson::LoadFromString(json, loaded),
           "version 2 round trip loads");
    Expect(loaded.meta.title == "Round Trip", "title survives round trip");
    Expect(loaded.grid.cols == 8, "cols survive round trip");
    Expect(loaded.GetTileAt("", 1, 2) == 1, "tile survives round trip");
    Expect(loaded.GetEdgeState(Cartograph::EdgeId(1, 2, 2, 2)) ==
               Cartograph::EdgeState::Wall,
           "edge survives round trip");
}

/**
 * Reject future schema versions and out-of-range edge states.
 */
void TestLoadGuards() {
    Cartograph::Model model;
    Expect(!Cartograph::IOJson::LoadFromString(
               "{\"version\":4,\"grid\":{\"cols\":4,\"rows\":4}}", model),
           "version 4 is rejected");

    const char* badEdge =
        "{\"version\":2,\"edges\":[{\"x1\":0,\"y1\":0,\"x2\":1,\"y2\":0,"
        "\"state\":99}]}";
    Expect(Cartograph::IOJson::LoadFromString(badEdge, model),
           "unknown edge state does not fail the file");
    Expect(model.edges.empty(), "unknown edge state is skipped");

    const char* badMarker =
        "{\"version\":2,\"markers\":[{\"id\":\"m\",\"pos\":[1]}]}";
    Expect(Cartograph::IOJson::LoadFromString(badMarker, model),
           "short marker pos does not fail the file");
}

/**
 * A v1 tile at (2, 5) flips to y=-5, then shifts so the
 * content minimum lands on the origin.
 */
void TestV1Migration() {
    const char* v1 =
        "{\"version\":1,\"grid\":{\"cols\":16,\"rows\":16},"
        "\"tiles\":[{\"room\":\"\",\"y\":5,\"runs\":[[2,1,1]]}]}";
    Cartograph::Model model;
    Expect(Cartograph::IOJson::LoadFromString(v1, model), "v1 loads");
    Expect(model.GetTileAt("", 0, 0) == 1, "v1 content shifts to the origin");
}

/**
 * Folder detection accepts the version the saver writes.
 */
void TestProjectFolderVersion() {
    fs::path dir = fs::temp_directory_path() / "cartograph-folder-test";
    fs::create_directories(dir);
    {
        std::ofstream out(dir / "project.json");
        out << "{\"version\":2,\"grid\":{\"cols\":4,\"rows\":4}}";
    }
    Expect(Cartograph::ProjectFolder::IsProjectFolder(dir.string()),
           "version 2 folder is a project");

    {
        std::ofstream out(dir / "project.json");
        out << "{\"version\":99,\"grid\":{\"cols\":4,\"rows\":4}}";
    }
    Expect(!Cartograph::ProjectFolder::IsProjectFolder(dir.string()),
           "version 99 folder is rejected");
    fs::remove_all(dir);
}

/**
 * .cart extension and ZIP entry checks.
 */
void TestCartPaths() {
    Expect(Cartograph::Package::HasCartExtension("Map.CART"),
           ".CART is a cart file");
    Expect(Cartograph::Package::HasCartExtension("map.cart"),
           ".cart is a cart file");
    Expect(!Cartograph::Package::HasCartExtension("map.cartproj"),
           ".cartproj is not a cart file");
    Expect(Cartograph::Package::IsEntryPathSafe("icons/a.png"),
           "icon entry is safe");
    Expect(!Cartograph::Package::IsEntryPathSafe("icons/../secret"),
           "dotdot segment is rejected");
    Expect(!Cartograph::Package::IsEntryPathSafe("/etc/passwd"),
           "absolute entry is rejected");
    Expect(!Cartograph::Package::IsEntryPathSafe("C:/windows/a.png"),
           "drive entry is rejected");
}

/**
 * Painting cannot grow the grid past the load-time cap.
 */
void TestGridCap() {
    Cartograph::Model model;
    model.grid.autoExpandGrid = true;
    model.grid.expansionThreshold = 3;
    model.grid.cols = 10;
    model.ExpandGridIfNeeded(8, 0);
    Expect(model.grid.cols > 10, "grid grows near the edge");
    Expect(model.grid.cols <= Cartograph::Limits::MAX_GRID_DIMENSION,
           "growth stays within the cap");

    model.grid.cols = Cartograph::Limits::MAX_GRID_DIMENSION;
    model.ExpandGridIfNeeded(model.grid.cols - 1, 0);
    Expect(model.grid.cols == Cartograph::Limits::MAX_GRID_DIMENSION,
           "grid does not pass the cap");
}

/**
 * The shipped v2 samples load and re-save as the current version.
 */
void TestSamples() {
    const char* names[] = {"Castle-Bronte.cart", "SOTN.cart"};
    for (const char* name : names) {
        std::string path =
            std::string(CARTOGRAPH_SOURCE_DIR) + "/samples/" + name;
        Cartograph::Model model;
        bool ok = Cartograph::Package::Load(path, model, nullptr);
        Expect(ok, std::string(name) + " loads");
        if (!ok) continue;
        std::string saved = Cartograph::IOJson::SaveToString(model);
        Expect(saved.find("\"version\": 3") != std::string::npos,
               std::string(name) + " saves as version 3");
        Cartograph::Model again;
        Expect(Cartograph::IOJson::LoadFromString(saved, again),
               std::string(name) + " reloads");
        Expect(again.rooms.size() == model.rooms.size(),
               std::string(name) + " keeps its rooms");
        Expect(again.edges.size() == model.edges.size(),
               std::string(name) + " keeps its edges");
    }
}

}  // namespace

int main() {
    TestSamples();
    TestRoundTrip();
    TestLoadGuards();
    TestV1Migration();
    TestProjectFolderVersion();
    TestCartPaths();
    TestGridCap();
    if (g_failed != 0) {
        std::cerr << g_failed << " check(s) failed\n";
        return 1;
    }
    std::cout << "load tests passed\n";
    return 0;
}
