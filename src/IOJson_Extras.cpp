// project.json sections added in version 3: props tables, abilities,
// door props. Interior data lives in IOJson_Interior.cpp.

#include "IOJsonExtras.h"

#include "Config.h"

#include <algorithm>
#include <tuple>

namespace Cartograph {

using json = nlohmann::json;

namespace {

std::string OneWayToString(OneWay oneWay) {
    switch (oneWay) {
        case OneWay::AToB: return "a-to-b";
        case OneWay::BToA: return "b-to-a";
        default: return "none";
    }
}

OneWay OneWayFromString(const std::string& s) {
    if (s == "a-to-b") return OneWay::AToB;
    if (s == "b-to-a") return OneWay::BToA;
    return OneWay::None;
}

// Read [x, y] into a Spawn. Returns nullopt on anything else.
std::optional<Spawn> LoadSpawn(const json& obj, const char* key) {
    if (!obj.contains(key)) return std::nullopt;
    const json& a = obj[key];
    if (!a.is_array() || a.size() < 2 || !a[0].is_number() ||
        !a[1].is_number()) {
        return std::nullopt;
    }
    return Spawn{a[0].get<float>(), a[1].get<float>()};
}

void SaveAbilities(const Model& model, json& j) {
    if (model.abilities.empty()) return;
    j["abilities"] = json::array();
    for (const auto& a : model.abilities) {
        j["abilities"].push_back({
            {"id", a.id},
            {"name", a.name},
            {"color", a.color.ToHex(false)}
        });
    }
}

void LoadAbilities(const json& j, Model& model) {
    model.abilities.clear();
    if (!j.contains("abilities") || !j["abilities"].is_array() ||
        j["abilities"].size() > Limits::MAX_ABILITIES) {
        return;
    }
    for (const auto& a : j["abilities"]) {
        if (!a.is_object()) continue;
        Ability ab;
        ab.id = a.value("id", "");
        if (ab.id.empty() || model.FindAbility(ab.id)) continue;
        ab.name = a.value("name", ab.id);
        ab.color = Color::FromHex(a.value("color", "#ffcc33"));
        model.abilities.push_back(ab);
    }
}

void SaveDoorProps(const Model& model, json& j) {
    std::vector<std::pair<EdgeId, const DoorProps*>> sorted;
    for (const auto& [edgeId, dp] : model.doorProps) {
        sorted.emplace_back(edgeId, &dp);
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        return std::tie(a.first.x1, a.first.y1, a.first.x2, a.first.y2) <
               std::tie(b.first.x1, b.first.y1, b.first.x2, b.first.y2);
    });
    json arr = json::array();
    for (const auto& [edgeId, dpPtr] : sorted) {
        const DoorProps& dp = *dpPtr;
        // Props on an edge that is no longer a door are dropped.
        if (dp.IsDefault() ||
            model.GetEdgeState(edgeId) != EdgeState::Door) {
            continue;
        }
        json o = {
            {"x1", edgeId.x1}, {"y1", edgeId.y1},
            {"x2", edgeId.x2}, {"y2", edgeId.y2}
        };
        if (!dp.gate.empty()) o["gate"] = dp.gate;
        if (dp.oneWay != OneWay::None) {
            o["oneWay"] = OneWayToString(dp.oneWay);
        }
        if (dp.spawnA) o["spawnA"] = json::array({dp.spawnA->x, dp.spawnA->y});
        if (dp.spawnB) o["spawnB"] = json::array({dp.spawnB->x, dp.spawnB->y});
        SavePropsInto(o, dp.props);
        arr.push_back(o);
    }
    if (!arr.empty()) j["doorProps"] = arr;
}

void LoadDoorProps(const json& j, Model& model) {
    model.doorProps.clear();
    if (!j.contains("doorProps") || !j["doorProps"].is_array() ||
        j["doorProps"].size() > Limits::MAX_DOOR_PROPS) {
        return;
    }
    for (const auto& o : j["doorProps"]) {
        if (!o.is_object()) continue;
        EdgeId id(o.value("x1", 0), o.value("y1", 0),
                  o.value("x2", 0), o.value("y2", 0));
        DoorProps dp;
        dp.gate = o.value("gate", "");
        dp.oneWay = OneWayFromString(o.value("oneWay", "none"));
        dp.spawnA = LoadSpawn(o, "spawnA");
        dp.spawnB = LoadSpawn(o, "spawnB");
        dp.props = LoadPropsFrom(o);
        if (!dp.IsDefault()) model.doorProps[id] = dp;
    }
}

}  // namespace

void SavePropsInto(json& obj, const Props& props) {
    if (props.empty()) return;
    obj["props"] = props;
}

Props LoadPropsFrom(const json& obj) {
    Props out;
    if (!obj.contains("props") || !obj["props"].is_object()) return out;
    for (const auto& [k, v] : obj["props"].items()) {
        if (out.size() >= Limits::MAX_PROPS) break;
        std::string text;
        if (v.is_string()) {
            text = v.get<std::string>();
        } else if (v.is_boolean() || v.is_number()) {
            text = v.dump();
        } else {
            continue;
        }
        if (k.empty() || k.size() > Limits::MAX_PROP_TEXT ||
            text.size() > Limits::MAX_PROP_TEXT) {
            continue;
        }
        out[k] = text;
    }
    return out;
}

void SaveInterior(const Model& model, json& j);
void LoadInterior(const json& j, Model& model);

void SaveExtras(const Model& model, json& j) {
    SaveAbilities(model, j);
    SaveDoorProps(model, j);
    SaveInterior(model, j);
}

void LoadExtras(const json& j, Model& model) {
    LoadAbilities(j, model);
    LoadDoorProps(j, model);
    LoadInterior(j, model);
}

}  // namespace Cartograph
