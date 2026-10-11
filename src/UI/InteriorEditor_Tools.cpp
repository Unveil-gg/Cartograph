// Interior editor: reference overlays (walls, doors, markers) and the
// paint, erase and fill tools.

#include "InteriorEditor.h"
#include "CanvasPanel.h"
#include "../InteriorColors.h"

#include <cmath>
#include <deque>
#include <memory>
#include <set>

namespace Cartograph {

/**
 * True when the interior tile lies in a cell of the room being edited.
 * @param model Project model
 * @param ix Global interior X
 * @param iy Global interior Y, Y-up
 * @return true if painting there is allowed
 */
bool InteriorEditor::IsEditable(Model& model, int ix, int iy) const {
    const int cx = InteriorFloorDiv(ix, model.interiorConfig.width);
    const int cy = InteriorFloorDiv(iy, model.interiorConfig.height);
    auto it = model.cellRoomAssignments.find({cx, cy});
    return it != model.cellRoomAssignments.end() && it->second == m_roomId;
}

/**
 * Draw the room's walls and doors (door color follows the gate ability)
 * and its markers, so tiles can be lined up with them.
 * @param model Project model
 * @param v View mapping
 * @param dl Draw list
 */
void InteriorEditor::DrawReferences(
    Model& model, const View& v, ImDrawList* dl
) {
    const int w = model.interiorConfig.width;
    const int h = model.interiorConfig.height;
    auto mine = [&](int cx, int cy) {
        auto it = model.cellRoomAssignments.find({cx, cy});
        return it != model.cellRoomAssignments.end() &&
               it->second == m_roomId;
    };

    for (const auto& [id, state] : model.edges) {
        if (state != EdgeState::Wall && state != EdgeState::Door) continue;
        if (!mine(id.x1, id.y1) && !mine(id.x2, id.y2)) continue;
        ImU32 color = IM_COL32(235, 235, 240, 255);
        if (state == EdgeState::Door) {
            color = IM_COL32(255, 200, 60, 255);
            const DoorProps* dp = model.GetDoorProps(id);
            const Ability* ab = dp ? model.FindAbility(dp->gate) : nullptr;
            if (ab) {
                color = ImGui::ColorConvertFloat4ToU32(ab->color.ToImVec4());
            }
        }
        ImVec2 a, b;
        if (id.x1 != id.x2) {  // Vertical edge between side-by-side cells
            a = v.ToScreen(static_cast<float>(id.x2 * w),
                           static_cast<float>(id.y1 * h));
            b = v.ToScreen(static_cast<float>(id.x2 * w),
                           static_cast<float>((id.y1 + 1) * h));
        } else {               // Horizontal edge between stacked cells
            a = v.ToScreen(static_cast<float>(id.x1 * w),
                           static_cast<float>(id.y2 * h));
            b = v.ToScreen(static_cast<float>((id.x1 + 1) * w),
                           static_cast<float>(id.y2 * h));
        }
        dl->AddLine(a, b, color, state == EdgeState::Door ? 4.0f : 3.0f);
    }

    for (const Marker& m : model.markers) {
        if (!mine(static_cast<int>(std::floor(m.x)),
                  static_cast<int>(std::floor(m.y)))) {
            continue;
        }
        ImVec2 p = v.ToScreen(m.x * w, m.y * h);
        dl->AddCircleFilled(p, 5.0f, ImGui::ColorConvertFloat4ToU32(
                                m.color.ToImVec4()));
        dl->AddCircle(p, 5.0f, IM_COL32(0, 0, 0, 255));
        if (v.zoom >= 14.0f && !m.kind.empty()) {
            dl->AddText(ImVec2(p.x + 7, p.y - 7), IM_COL32(255, 255, 255, 220),
                        m.kind.c_str());
        }
    }
}

/**
 * Change one tile as part of the current stroke, remembering the tile's
 * state before the stroke touched it.
 * @param model Project model
 * @param ix Global interior X
 * @param iy Global interior Y
 */
void InteriorEditor::ApplyStrokeTile(Model& model, int ix, int iy) {
    if (!IsEditable(model, ix, iy)) return;
    const InteriorTile target =
        m_tool == Tool::Erase ? InteriorTile::Empty : m_tile;
    const InteriorTile before = model.GetInteriorTile(ix, iy);
    if (before == target) return;
    model.SetInteriorTile(ix, iy, target);
    if (model.GetInteriorTile(ix, iy) != target) return;  // Chunk limit hit
    auto it = m_stroke.find({ix, iy});
    if (it == m_stroke.end()) {
        m_stroke[{ix, iy}] = {before, target};
    } else {
        it->second.second = target;
    }
}

/**
 * Finish a stroke: the model already holds the new tiles, so push the
 * command without executing it.
 * @param model Project model
 * @param history Undo history
 */
void InteriorEditor::EndStroke(Model& model, History& history) {
    std::vector<PaintInteriorCommand::Change> changes;
    for (const auto& [pos, change] : m_stroke) {
        if (change.first == change.second) continue;
        changes.push_back({pos.first, pos.second, change.first,
                           change.second});
    }
    m_stroke.clear();
    m_stroking = false;
    m_hasLast = false;
    if (changes.empty()) return;
    history.AddCommand(
        std::make_unique<PaintInteriorCommand>(std::move(changes)), model,
        false);
}

/**
 * Flood-fill the connected same-type area under (ix, iy), staying inside
 * the room's cells. Applied as one undoable command.
 * @param model Project model
 * @param history Undo history
 * @param ix Start interior X
 * @param iy Start interior Y
 */
void InteriorEditor::FillAt(
    Model& model, History& history, int ix, int iy
) {
    if (!IsEditable(model, ix, iy)) return;
    const InteriorTile from = model.GetInteriorTile(ix, iy);
    const InteriorTile to =
        m_tool == Tool::Erase ? InteriorTile::Empty : m_tile;
    if (from == to) return;

    std::vector<PaintInteriorCommand::Change> changes;
    std::set<std::pair<int, int>> seen{{ix, iy}};
    std::deque<std::pair<int, int>> queue{{ix, iy}};
    while (!queue.empty()) {
        auto [x, y] = queue.front();
        queue.pop_front();
        changes.push_back({x, y, from, to});
        const int dx[] = {1, -1, 0, 0};
        const int dy[] = {0, 0, 1, -1};
        for (int d = 0; d < 4; ++d) {
            std::pair<int, int> n{x + dx[d], y + dy[d]};
            if (seen.count(n) || !IsEditable(model, n.first, n.second) ||
                model.GetInteriorTile(n.first, n.second) != from) {
                continue;
            }
            seen.insert(n);
            queue.push_back(n);
        }
    }
    history.AddCommand(
        std::make_unique<PaintInteriorCommand>(std::move(changes)), model,
        true);
}

/**
 * Hover preview and mouse handling for the active tool.
 * @param model Project model
 * @param history Undo history
 * @param v View mapping
 * @param hovered True when the pointer is over the canvas area
 */
void InteriorEditor::HandleToolInput(
    Model& model, History& history, const View& v, bool hovered
) {
    ImVec2 mouse = ImGui::GetMousePos();
    const int ix = static_cast<int>(
        std::floor(v.cx + (mouse.x - v.center.x) / v.zoom));
    const int iy = static_cast<int>(
        std::floor(v.cy - (mouse.y - v.center.y) / v.zoom));

    if (hovered && IsEditable(model, ix, iy)) {
        ImVec2 a = v.ToScreen(static_cast<float>(ix),
                              static_cast<float>(iy + 1));
        ImVec2 b = v.ToScreen(static_cast<float>(ix + 1),
                              static_cast<float>(iy));
        ImGui::GetWindowDrawList()->AddRect(a, b, IM_COL32(255, 255, 255, 220));
        ImGui::SetTooltip("(%d, %d)", ix, iy);
    }

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (m_tool == Tool::Fill) {
            FillAt(model, history, ix, iy);
        } else {
            m_stroking = true;
        }
    }
    if (!m_stroking || !ImGui::IsMouseDown(ImGuiMouseButton_Left)) return;

    // Paint along the drag so fast strokes leave no gaps.
    if (m_hasLast) {
        for (const auto& t : GetTilesAlongLine(m_lastIx, m_lastIy, ix, iy)) {
            ApplyStrokeTile(model, t.first, t.second);
        }
    } else {
        ApplyStrokeTile(model, ix, iy);
    }
    m_lastIx = ix;
    m_lastIy = iy;
    m_hasLast = true;
}

}  // namespace Cartograph
