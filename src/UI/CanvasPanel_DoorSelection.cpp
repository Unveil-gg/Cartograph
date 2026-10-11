// Door inspection on the canvas: select a door edge, place its spawn
// points, and carry its props when the selection moves.

#include "CanvasPanel.h"

#include <imgui.h>
#include <cmath>
#include <vector>

namespace Cartograph {

/**
 * Select the door edge under a screen point.
 * @param model Model to query
 * @param canvas Canvas for coordinate conversion
 * @param x Screen X
 * @param y Screen Y
 * @return true when a door edge was selected
 */
bool CanvasPanel::TrySelectDoorAt(
    Model& model, const Canvas& canvas, float x, float y
) {
    EdgeId edgeId;
    EdgeSide side;
    if (!DetectEdgeHover(x, y, canvas, model.grid, &edgeId, &side)) {
        return false;
    }
    if (model.GetEdgeState(edgeId) != EdgeState::Door) return false;
    selectedDoorEdge = edgeId;
    hasSelectedDoor = true;
    placingSpawn = 0;
    return true;
}

/**
 * While a spawn is being placed, a left click sets it and Escape cancels.
 * @param model Model to edit
 * @param canvas Canvas for coordinate conversion
 * @param history Undo history
 * @return true while placement is active (the click is consumed)
 */
bool CanvasPanel::HandleDoorSpawnInput(
    Model& model, Canvas& canvas, History& history
) {
    if (placingSpawn == 0) return false;
    if (!hasSelectedDoor ||
        model.GetEdgeState(selectedDoorEdge) != EdgeState::Door) {
        placingSpawn = 0;
        return false;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        placingSpawn = 0;
        return true;
    }
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return true;

    ImVec2 mouse = ImGui::GetMousePos();
    float wx, wy;
    canvas.ScreenToWorld(mouse.x, mouse.y, &wx, &wy);
    // Cell units, Y-up, like marker positions; rounded to 1/100 cell
    Spawn spawn;
    spawn.x = std::round(wx / model.grid.tileWidth * 100.0f) / 100.0f;
    spawn.y = std::round(-wy / model.grid.tileHeight * 100.0f) / 100.0f;

    const DoorProps* current = model.GetDoorProps(selectedDoorEdge);
    DoorProps before = current ? *current : DoorProps();
    DoorProps after = before;
    (placingSpawn == 1 ? after.spawnA : after.spawnB) = spawn;
    placingSpawn = 0;
    history.AddCommand(
        std::make_unique<ModifyDoorPropsCommand>(
            selectedDoorEdge, before, after),
        model, true);
    return true;
}

/**
 * Move door props with their edges. Old entries are cleared first so
 * overlapping moves (shift by one cell) stay correct; each step is its
 * own undo command.
 * @param model Model to edit
 * @param history Undo history
 * @param edges Moved edges at their old positions
 * @param dx Shift in cells, X
 * @param dy Shift in cells, Y
 */
void CanvasPanel::MoveDoorProps(
    Model& model, History& history,
    const std::unordered_map<EdgeId, EdgeState, EdgeIdHash>& edges,
    int dx, int dy
) {
    std::vector<std::pair<EdgeId, DoorProps>> moving;
    for (const auto& [edgeId, state] : edges) {
        if (state != EdgeState::Door) continue;
        const DoorProps* dp = model.GetDoorProps(edgeId);
        if (dp) moving.emplace_back(edgeId, *dp);
    }
    for (const auto& [edgeId, dp] : moving) {
        history.AddCommand(
            std::make_unique<ModifyDoorPropsCommand>(
                edgeId, dp, DoorProps()),
            model, true);
    }
    for (const auto& [edgeId, dp] : moving) {
        EdgeId target(edgeId.x1 + dx, edgeId.y1 + dy,
                      edgeId.x2 + dx, edgeId.y2 + dy);
        const DoorProps* existing = model.GetDoorProps(target);
        history.AddCommand(
            std::make_unique<ModifyDoorPropsCommand>(
                target, existing ? *existing : DoorProps(),
                ShiftedDoorProps(dp, static_cast<float>(dx),
                                 static_cast<float>(dy))),
            model, true);
    }
}

/**
 * Draw a bright outline on the door selected for inspection.
 * @param model Model for grid size
 * @param canvas Canvas for coordinate conversion
 * @param drawList ImGui draw list
 */
void CanvasPanel::DrawDoorSelection(
    Model& model, Canvas& canvas, ImDrawList* drawList
) {
    float sx1, sy1, sx2, sy2;
    canvas.EdgeToScreen(model.grid, selectedDoorEdge,
                        &sx1, &sy1, &sx2, &sy2);
    drawList->AddLine(ImVec2(sx1, sy1), ImVec2(sx2, sy2),
                      IM_COL32(100, 200, 255, 255),
                      4.0f * std::max(1.0f, canvas.zoom * 0.5f));
}

}  // namespace Cartograph
