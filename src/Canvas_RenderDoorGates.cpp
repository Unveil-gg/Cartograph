// Gate color, one-way arrows, and spawn glyphs on door edges.

#include "Canvas.h"
#include "render/Renderer.h"

#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace Cartograph {

namespace {

// Color for a gate: the ability color, or magenta when unregistered.
Color GateColor(const Model& model, const std::string& gate) {
    const Ability* ability = model.FindAbility(gate);
    return ability ? ability->color : Color(1.0f, 0.2f, 0.8f, 1.0f);
}

}  // namespace

/**
 * Draw gate overlays for every door edge with props.
 * Gated doors get a solid ability-colored line and a label, one-way
 * doors get an arrow pointing the way through, spawns get a diamond.
 * @param renderer Renderer for lines
 * @param model Model to draw
 */
void Canvas::RenderDoorGates(IRenderer& renderer, const Model& model) {
    if (model.doorProps.empty()) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float thickness = 2.0f * zoom;

    for (const auto& [edgeId, dp] : model.doorProps) {
        if (model.GetEdgeState(edgeId) != EdgeState::Door) continue;

        float sx1, sy1, sx2, sy2;
        EdgeToScreen(model.grid, edgeId, &sx1, &sy1, &sx2, &sy2);
        float mx = (sx1 + sx2) * 0.5f;
        float my = (sy1 + sy2) * 0.5f;

        Color color = dp.gate.empty() ? model.theme.doorColor
                                      : GateColor(model, dp.gate);
        if (!dp.gate.empty()) {
            renderer.DrawLine(sx1, sy1, sx2, sy2, color, thickness * 1.8f);
            if (zoom > 0.7f) {
                std::string label = model.AbilityName(dp.gate);
                dl->AddText(ImVec2(mx + 4.0f * zoom, my - 8.0f),
                            color.ToU32(), label.c_str());
            }
        }

        if (dp.oneWay != OneWay::None) {
            // Direction from cell a to cell b on screen (Y-up map)
            float dx = static_cast<float>(edgeId.x2 - edgeId.x1);
            float dy = -static_cast<float>(edgeId.y2 - edgeId.y1);
            if (dp.oneWay == OneWay::BToA) { dx = -dx; dy = -dy; }
            float size = std::clamp(6.0f * zoom, 4.0f, 16.0f);
            ImVec2 tip(mx + dx * size, my + dy * size);
            ImVec2 base(mx - dx * size * 0.5f, my - dy * size * 0.5f);
            ImVec2 side(-dy * size * 0.7f, dx * size * 0.7f);
            dl->AddTriangleFilled(
                tip, ImVec2(base.x + side.x, base.y + side.y),
                ImVec2(base.x - side.x, base.y - side.y), color.ToU32());
        }

        const std::optional<Spawn>* spawns[2] = {&dp.spawnA, &dp.spawnB};
        for (const auto* spawn : spawns) {
            if (!spawn->has_value()) continue;
            float wx = (*spawn)->x * model.grid.tileWidth;
            float wy = -(*spawn)->y * model.grid.tileHeight;
            float px, py;
            WorldToScreen(wx, wy, &px, &py);
            float r = std::clamp(4.0f * zoom, 3.0f, 10.0f);
            dl->AddQuadFilled(ImVec2(px, py - r), ImVec2(px + r, py),
                              ImVec2(px, py + r), ImVec2(px - r, py),
                              color.ToU32());
            dl->AddQuad(ImVec2(px, py - r), ImVec2(px + r, py),
                        ImVec2(px, py + r), ImVec2(px - r, py),
                        IM_COL32(0, 0, 0, 200), 1.0f);
        }
    }
}

}  // namespace Cartograph
