// Room tint for gate validation results.

#include "Canvas.h"
#include "Reachability.h"
#include "render/Renderer.h"

#include <set>

namespace Cartograph {

/**
 * Tint cells of unreachable (red), trapped (orange) and start (green)
 * rooms from the current validation report.
 * @param renderer Renderer for rects
 * @param model Model for cell-room assignments
 */
void Canvas::RenderValidationOverlay(
    IRenderer& renderer, const Model& model
) {
    if (!validationOverlay) return;
    const ValidationReport& r = *validationOverlay;
    std::set<std::string> unreachable(r.unreachableRoomIds.begin(),
                                      r.unreachableRoomIds.end());
    std::set<std::string> trapped;
    for (const auto& t : r.trappedRooms) trapped.insert(t.roomId);

    const int tw = model.grid.tileWidth;
    const int th = model.grid.tileHeight;
    for (const auto& [cell, roomId] : model.cellRoomAssignments) {
        Color color;
        if (unreachable.count(roomId)) {
            color = Color(1.0f, 0.1f, 0.1f, 0.45f);
        } else if (trapped.count(roomId)) {
            color = Color(1.0f, 0.55f, 0.1f, 0.45f);
        } else if (roomId == r.startRoomId) {
            color = Color(0.2f, 1.0f, 0.4f, 0.25f);
        } else {
            continue;
        }
        float wx, wy, sx, sy;
        TileToWorld(cell.first, cell.second, tw, th, &wx, &wy);
        WorldToScreen(wx, wy, &sx, &sy);
        renderer.DrawRect(sx, sy, tw * zoom, th * zoom, color);
    }
}

}  // namespace Cartograph
