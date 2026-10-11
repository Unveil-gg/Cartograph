#pragma once

#include "../InteriorTypes.h"
#include "../Model.h"
#include "../history/History.h"
#include "../history/PaintInteriorCommand.h"

#include <imgui.h>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace Cartograph {

/**
 * Interior editor: a window that shows one room's cells stitched into a
 * single block of interior tiles, with its own pan and zoom. It paints
 * the project's interior sub-grid (solid, platform, hazard, ...).
 * Neighbouring rooms show dimmed and cannot be edited.
 */
class InteriorEditor {
public:
    bool open = false;

    /**
     * Open (or retarget) the editor on a room and fit the view to it.
     * @param roomId Room to edit
     */
    void Open(const std::string& roomId);

    /**
     * Draw the window and handle input. Call once per frame.
     * @param model Project model
     * @param history Undo history
     */
    void Render(Model& model, History& history);

private:
    enum class Tool { Paint, Erase, Fill };

    // Screen <-> interior-tile mapping for the current frame.
    struct View {
        ImVec2 center;   // Screen center of the canvas area
        float cx, cy;    // Interior coordinate at that center
        float zoom;      // Pixels per interior tile

        ImVec2 ToScreen(float ix, float iy) const {
            return ImVec2(center.x + (ix - cx) * zoom,
                          center.y - (iy - cy) * zoom);
        }
    };

    void RenderToolbar(Model& model, History& history);
    void HandleKeys(Model& model, History& history);
    void FitView(Model& model, ImVec2 size);
    void HandleViewInput(const ImVec2& size, bool hovered);
    void DrawRoom(Model& model, const View& v, ImDrawList* dl);
    void DrawReferences(Model& model, const View& v, ImDrawList* dl);
    void HandleToolInput(Model& model, History& history, const View& v,
                         bool hovered);

    bool IsEditable(Model& model, int ix, int iy) const;
    void ApplyStrokeTile(Model& model, int ix, int iy);
    void EndStroke(Model& model, History& history);
    void FillAt(Model& model, History& history, int ix, int iy);

    std::string m_roomId;
    Tool m_tool = Tool::Paint;
    InteriorTile m_tile = InteriorTile::Solid;
    bool m_fit = true;

    // View center (interior units, Y-up) and zoom (pixels per tile).
    float m_cx = 0.0f;
    float m_cy = 0.0f;
    float m_zoom = 20.0f;

    // Current brush stroke: tile -> (before, after).
    bool m_stroking = false;
    bool m_hasLast = false;
    int m_lastIx = 0;
    int m_lastIy = 0;
    std::map<std::pair<int, int>, std::pair<InteriorTile, InteriorTile>>
        m_stroke;
};

}  // namespace Cartograph
