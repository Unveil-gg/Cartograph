#include "CanvasPanel.h"
#include "Modals.h"
#include "../App.h"
#include "../platform/Paths.h"
#include "../platform/System.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <SDL3/SDL.h>
#include <stb/stb_image.h>
#include <filesystem>
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <set>
#include <cstring>


namespace Cartograph {

void CanvasPanel::HandlePaintRelease(
    Model& model,
    History& history
) {
        // Check for paint/erase tile release
        bool mouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left) ||
                             ImGui::IsMouseReleased(ImGuiMouseButton_Right);
        
        if (isPainting && mouseReleased) {
            if (!currentPaintChanges.empty()) {
                auto cmd = std::make_unique<PaintTilesCommand>(
                    currentPaintChanges
                );
                // Changes already applied, just store for undo/redo
                history.AddCommand(std::move(cmd), model, false);
                currentPaintChanges.clear();
            }
            isPainting = false;
            lastPaintedTileX = -1;
            lastPaintedTileY = -1;
            twoFingerEraseActive = false;
        }
        
        // Check for edge modification release (Erase tool)
        if (isModifyingEdges && mouseReleased) {
            if (!currentEdgeChanges.empty()) {
                auto cmd = std::make_unique<ModifyEdgesCommand>(
                    currentEdgeChanges
                );
                history.AddCommand(std::move(cmd), model, false);
                currentEdgeChanges.clear();
            }
            isModifyingEdges = false;
        }
}

}  // namespace Cartograph
