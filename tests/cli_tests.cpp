// CLI behavior and the documented sample map (docs/sample-map.json).

#include "TestUtil.h"

#include "IOJson.h"
#include "Model.h"
#include "Reachability.h"
#include "cli/Cli.h"
#include "platform/Fs.h"

#include <filesystem>
#include <sstream>
#include <vector>

using testutil::Expect;
using namespace Cartograph;

namespace {

const std::string kRoot = CARTOGRAPH_SOURCE_DIR;
const std::string kSample = kRoot + "/docs/sample-map.json";

// Run the CLI with the given arguments; capture output and the code.
int Run(std::vector<std::string> args, std::string* out = nullptr,
        std::string* err = nullptr) {
    args.insert(args.begin(), "cartograph-cli");
    std::vector<const char*> argv;
    for (const auto& a : args) argv.push_back(a.c_str());
    std::ostringstream o, e;
    int code = RunCli(static_cast<int>(argv.size()), argv.data(), o, e);
    if (out) *out = o.str();
    if (err) *err = e.str();
    return code;
}

/** The sample uses every v3 feature, validates, and round-trips. */
void TestSample() {
    Model m;
    Expect(IOJson::LoadFromFile(kSample, m), "sample loads");
    Expect(m.abilities.size() == 2, "sample has abilities");
    Expect(m.doorProps.size() == 3, "sample has door props");
    bool gate = false, oneWay = false, spawns = false, doorProps = false;
    for (const auto& [id, dp] : m.doorProps) {
        gate |= !dp.gate.empty();
        oneWay |= dp.oneWay != OneWay::None;
        spawns |= dp.spawnA && dp.spawnB;
        doorProps |= !dp.props.empty();
    }
    Expect(gate && oneWay && spawns && doorProps,
           "sample uses gate, oneWay, spawns and door props");
    Expect(m.interior.size() == 2, "sample has interior chunks");
    Expect(!m.rooms.at(0).props.empty() && !m.regionGroups.at(0).props.empty(),
           "sample has room and region props");
    std::vector<std::string> kinds;
    for (const auto& mk : m.markers) kinds.push_back(mk.kind);
    for (const char* k : {"start", "bench", "chest", "boss",
                          "ability-pickup", "enemy"}) {
        bool found = false;
        for (const auto& have : kinds) found |= have == k;
        Expect(found, std::string("sample has a ") + k + " marker");
    }
    Expect(!ValidateReachability(m).HasErrors(), "sample validates");

    std::string once = IOJson::SaveToString(m);
    Model again;
    Expect(IOJson::LoadFromString(once, again), "sample re-save loads");
    Expect(IOJson::SaveToString(again) == once,
           "save is stable across a round trip");
}

/** export writes the same JSON that SaveToString produces. */
void TestExport() {
    namespace fs = std::filesystem;
    fs::path out = fs::temp_directory_path() / "cartograph-cli-export.json";
    fs::remove(out);
    Expect(Run({"export", kSample, "-o", out.string()}) == kCliOk,
           "export succeeds");
    Model direct;
    IOJson::LoadFromFile(kSample, direct);
    auto text = Platform::ReadTextFile(out.string());
    Expect(text && *text == IOJson::SaveToString(direct) + "\n",
           "export equals SaveToString");

    std::string stdoutText;
    Expect(Run({"export", kSample}, &stdoutText) == kCliOk,
           "export to stdout succeeds");
    Expect(stdoutText == IOJson::SaveToString(direct) + "\n",
           "stdout export equals SaveToString");

    // A .cart sample exports at the current version
    std::string cartJson;
    Expect(Run({"export", kRoot + "/samples/Castle-Bronte.cart"},
               &cartJson) == kCliOk, ".cart export succeeds");
    Expect(cartJson.find("\"version\": 3") != std::string::npos,
           ".cart export is version 3");
    fs::remove(out);
}

/** validate exit codes follow the report. */
void TestValidate() {
    std::string out;
    Expect(Run({"validate", kSample}, &out) == kCliOk, "sample passes");
    Expect(out.rfind("OK", 0) == 0, "sample prints OK");
    Expect(Run({"validate", kRoot + "/samples/Castle-Bronte.cart"}, &out) ==
               kCliValidationFailed,
           "map without a start fails validation");
    Expect(Run({"validate", kSample, "--order", "double-jump,dash"}) ==
               kCliValidationFailed,
           "wrong order fails validation");
    Expect(Run({"validate", kSample, "--order", "dash,double-jump"}) ==
               kCliOk,
           "right order passes");
}

/** Bad usage and bad input use their own exit codes. */
void TestErrors() {
    std::string err;
    Expect(Run({}) == kCliUsage, "no command is a usage error");
    Expect(Run({"bogus"}) == kCliUsage, "unknown command is a usage error");
    Expect(Run({"export"}) == kCliUsage, "export needs an input");
    Expect(Run({"export", kSample, "-o"}) == kCliUsage, "-o needs a path");
    Expect(Run({"export", "/nonexistent/map.cart"}, nullptr, &err) ==
               kCliIoError,
           "missing input is an I/O error");
    Expect(err.find("not found") != std::string::npos,
           "missing input says not found");
    Expect(Run({"help"}) == kCliOk, "help succeeds");
}

}  // namespace

int main() {
    TestSample();
    TestExport();
    TestValidate();
    TestErrors();
    return testutil::Finish("cli_tests");
}
