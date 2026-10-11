// project.json "interior" section: char rows per cell, north row first.

#include "IOJsonExtras.h"

#include "Config.h"

#include <algorithm>
#include <tuple>

namespace Cartograph {

using json = nlohmann::json;

/**
 * Save interior chunks sorted by (y, x) so diffs stay stable.
 * @param model Model to save
 * @param j Root JSON object
 */
void SaveInterior(const Model& model, json& j) {
    if (model.interior.empty() && model.interiorConfig.width == 16 &&
        model.interiorConfig.height == 9) {
        return;
    }
    const int w = model.interiorConfig.width;
    const int h = model.interiorConfig.height;

    std::vector<const std::pair<const std::pair<int, int>, InteriorChunk>*>
        chunks;
    for (const auto& entry : model.interior) chunks.push_back(&entry);
    std::sort(chunks.begin(), chunks.end(), [](auto* a, auto* b) {
        return std::tie(a->first.second, a->first.first) <
               std::tie(b->first.second, b->first.first);
    });

    json arr = json::array();
    for (const auto* entry : chunks) {
        json rows = json::array();
        for (int row = 0; row < h; ++row) {
            int localY = h - 1 - row;  // Row 0 is the north row
            std::string text;
            for (int x = 0; x < w; ++x) {
                size_t idx = static_cast<size_t>(localY) * w + x;
                text += InteriorTileToChar(
                    static_cast<InteriorTile>(entry->second.tiles[idx]));
            }
            rows.push_back(text);
        }
        arr.push_back({
            {"cell", json::array({entry->first.first, entry->first.second})},
            {"rows", rows}
        });
    }
    j["interior"] = {{"width", w}, {"height", h}, {"chunks", arr}};
}

/**
 * Load interior chunks. Malformed chunks are skipped.
 * @param j Root JSON object
 * @param model Model to fill
 */
void LoadInterior(const json& j, Model& model) {
    model.interior.clear();
    model.interiorConfig = InteriorConfig{};
    if (!j.contains("interior") || !j["interior"].is_object()) return;
    const json& in = j["interior"];

    int w = std::clamp(in.value("width", 16), 1,
                       Limits::MAX_INTERIOR_DIMENSION);
    int h = std::clamp(in.value("height", 9), 1,
                       Limits::MAX_INTERIOR_DIMENSION);
    model.interiorConfig.width = w;
    model.interiorConfig.height = h;

    if (!in.contains("chunks") || !in["chunks"].is_array() ||
        in["chunks"].size() > Limits::MAX_INTERIOR_CHUNKS) {
        return;
    }
    for (const auto& c : in["chunks"]) {
        if (!c.is_object() || !c.contains("cell") ||
            !c["cell"].is_array() || c["cell"].size() < 2 ||
            !c["cell"][0].is_number_integer() ||
            !c["cell"][1].is_number_integer() ||
            !c.contains("rows") || !c["rows"].is_array() ||
            static_cast<int>(c["rows"].size()) != h) {
            continue;
        }
        InteriorChunk chunk;
        chunk.tiles.assign(static_cast<size_t>(w) * h, 0);
        bool ok = true;
        for (int row = 0; row < h && ok; ++row) {
            if (!c["rows"][row].is_string()) { ok = false; break; }
            const std::string& text = c["rows"][row].get_ref<const std::string&>();
            int localY = h - 1 - row;
            for (int x = 0; x < w && x < static_cast<int>(text.size()); ++x) {
                chunk.tiles[static_cast<size_t>(localY) * w + x] =
                    static_cast<uint8_t>(InteriorTileFromChar(text[x]));
            }
        }
        if (!ok || chunk.IsEmpty()) continue;
        model.interior[{c["cell"][0].get<int>(), c["cell"][1].get<int>()}] =
            std::move(chunk);
    }
}

}  // namespace Cartograph
