// Interior editor window: toolbar, view (pan/zoom) and drawing.

#include "InteriorEditor.h"
#include "../InteriorColors.h"

#include <algorithm>
#include <climits>
#include <cmath>

namespace Cartograph {

namespace {

constexpr float kMinZoom = 4.0f;
constexpr float kMaxZoom = 80.0f;

ImU32 ToU32(const Color& c, float alphaScale = 1.0f) {
    return ImGui::ColorConvertFloat4ToU32(
        ImVec4(c.r, c.g, c.b, c.a * alphaScale));
}

}  // namespace

/**
 * Open (or retarget) the editor on a room and fit the view to it.
 * @param roomId Room to edit
 */
void InteriorEditor::Open(const std::string& roomId) {
    m_roomId = roomId;
    m_fit = true;
    open = true;
}

/**
 * Center and scale the view so the whole room is visible.
 * @param model Project model
 * @param size Canvas area size in pixels
 */
void InteriorEditor::FitView(Model& model, ImVec2 size) {
    const auto& cells = model.GetRoomCells(m_roomId);
    if (cells.empty()) return;
    int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
    for (const auto& c : cells) {
        minX = std::min(minX, c.first);
        minY = std::min(minY, c.second);
        maxX = std::max(maxX, c.first);
        maxY = std::max(maxY, c.second);
    }
    const float w = model.interiorConfig.width;
    const float h = model.interiorConfig.height;
    float spanX = (maxX - minX + 1) * w;
    float spanY = (maxY - minY + 1) * h;
    m_cx = (minX * w + (maxX + 1) * w) * 0.5f;
    m_cy = (minY * h + (maxY + 1) * h) * 0.5f;
    float z = std::min((size.x - 40.0f) / spanX, (size.y - 40.0f) / spanY);
    m_zoom = std::clamp(z, kMinZoom, kMaxZoom);
    m_fit = false;
}

/**
 * Wheel zoom (toward the cursor) and middle/right-drag pan.
 * @param size Canvas area size in pixels
 * @param hovered True when the pointer is over the canvas area
 */
void InteriorEditor::HandleViewInput(const ImVec2& size, bool hovered) {
    ImGuiIO& io = ImGui::GetIO();
    if (hovered && io.MouseWheel != 0.0f) {
        ImVec2 mouse = ImGui::GetMousePos();
        ImVec2 origin = ImGui::GetItemRectMin();
        ImVec2 center(origin.x + size.x * 0.5f, origin.y + size.y * 0.5f);
        float beforeX = m_cx + (mouse.x - center.x) / m_zoom;
        float beforeY = m_cy - (mouse.y - center.y) / m_zoom;
        float factor = io.MouseWheel > 0 ? 1.15f : 1.0f / 1.15f;
        m_zoom = std::clamp(m_zoom * factor, kMinZoom, kMaxZoom);
        m_cx = beforeX - (mouse.x - center.x) / m_zoom;
        m_cy = beforeY + (mouse.y - center.y) / m_zoom;
    }
    for (ImGuiMouseButton b : {ImGuiMouseButton_Middle,
                               ImGuiMouseButton_Right}) {
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(b, 0.0f)) {
            ImVec2 d = ImGui::GetMouseDragDelta(b);
            m_cx -= d.x / m_zoom;
            m_cy += d.y / m_zoom;
            ImGui::ResetMouseDragDelta(b);
        }
    }
}

/**
 * Draw the editor window.
 * @param model Project model
 * @param history Undo history
 */
void InteriorEditor::Render(Model& model, History& history) {
    // A stroke must end even if the window closes under the mouse.
    if (m_stroking && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        EndStroke(model, history);
    }
    if (!open) return;
    Room* room = model.FindRoom(m_roomId);
    if (!room || model.GetRoomCells(m_roomId).empty()) {
        open = false;
        return;
    }

    std::string title = "Interior: " + room->name + "###InteriorEditor";
    ImGui::SetNextWindowSize(ImVec2(900, 620), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoScrollWithMouse |
                             ImGuiWindowFlags_NoDocking;
    if (!ImGui::Begin(title.c_str(), &open, flags)) {
        ImGui::End();
        return;
    }
    RenderToolbar(model, history);
    HandleKeys(model, history);

    ImVec2 size = ImGui::GetContentRegionAvail();
    if (size.x < 50.0f || size.y < 50.0f) {
        ImGui::End();
        return;
    }
    ImGui::InvisibleButton("interior_canvas", size,
                           ImGuiButtonFlags_MouseButtonLeft |
                           ImGuiButtonFlags_MouseButtonMiddle |
                           ImGuiButtonFlags_MouseButtonRight);
    bool hovered = ImGui::IsItemHovered();
    if (m_fit) FitView(model, size);
    HandleViewInput(size, hovered);

    ImVec2 origin = ImGui::GetItemRectMin();
    View view{ImVec2(origin.x + size.x * 0.5f, origin.y + size.y * 0.5f),
              m_cx, m_cy, m_zoom};
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                     true);
    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                      IM_COL32(18, 18, 22, 255));
    DrawRoom(model, view, dl);
    DrawReferences(model, view, dl);
    HandleToolInput(model, history, view, hovered);
    dl->PopClipRect();
    ImGui::End();
}

/**
 * Draw the room's cells, their tiles and a dimmed ring of neighbours.
 * @param model Project model
 * @param v View mapping
 * @param dl Draw list
 */
void InteriorEditor::DrawRoom(Model& model, const View& v, ImDrawList* dl) {
    const int w = model.interiorConfig.width;
    const int h = model.interiorConfig.height;
    const auto& cells = model.GetRoomCells(m_roomId);
    int minX = INT_MAX, minY = INT_MAX, maxX = INT_MIN, maxY = INT_MIN;
    for (const auto& c : cells) {
        minX = std::min(minX, c.first);
        minY = std::min(minY, c.second);
        maxX = std::max(maxX, c.first);
        maxY = std::max(maxY, c.second);
    }

    for (int cy = minY - 1; cy <= maxY + 1; ++cy) {
        for (int cx = minX - 1; cx <= maxX + 1; ++cx) {
            auto owner = model.cellRoomAssignments.find({cx, cy});
            if (owner == model.cellRoomAssignments.end()) continue;
            const bool mine = owner->second == m_roomId;
            const float dim = mine ? 1.0f : 0.3f;
            ImVec2 tl = v.ToScreen(static_cast<float>(cx * w),
                                   static_cast<float>((cy + 1) * h));
            ImVec2 br = v.ToScreen(static_cast<float>((cx + 1) * w),
                                   static_cast<float>(cy * h));
            dl->AddRectFilled(tl, br,
                              mine ? IM_COL32(40, 42, 52, 255)
                                   : IM_COL32(26, 27, 32, 255));

            auto chunk = model.interior.find({cx, cy});
            if (chunk != model.interior.end()) {
                for (int ly = 0; ly < h; ++ly) {
                    for (int lx = 0; lx < w; ++lx) {
                        uint8_t t = chunk->second.tiles[
                            static_cast<size_t>(ly) * w + lx];
                        if (t == 0) continue;
                        ImVec2 a = v.ToScreen(
                            static_cast<float>(cx * w + lx),
                            static_cast<float>(cy * h + ly + 1));
                        ImVec2 b = v.ToScreen(
                            static_cast<float>(cx * w + lx + 1),
                            static_cast<float>(cy * h + ly));
                        dl->AddRectFilled(
                            a, b,
                            ToU32(InteriorTileColor(
                                static_cast<InteriorTile>(t)), dim));
                    }
                }
            }
            if (!mine) continue;
            if (v.zoom >= 10.0f) {
                ImU32 grid = IM_COL32(255, 255, 255, 22);
                for (int lx = 1; lx < w; ++lx) {
                    float x = tl.x + (br.x - tl.x) * lx / w;
                    dl->AddLine(ImVec2(x, tl.y), ImVec2(x, br.y), grid);
                }
                for (int ly = 1; ly < h; ++ly) {
                    float y = tl.y + (br.y - tl.y) * ly / h;
                    dl->AddLine(ImVec2(tl.x, y), ImVec2(br.x, y), grid);
                }
            }
            dl->AddRect(tl, br, IM_COL32(120, 125, 150, 160));
        }
    }
}

}  // namespace Cartograph
