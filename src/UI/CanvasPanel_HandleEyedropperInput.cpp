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

void CanvasPanel::HandleEyedropperInput(
    Model& model,
    Canvas& canvas
) {
            // Eyedropper tool: Left-click to pick tile color/ID
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                ImVec2 mousePos = ImGui::GetMousePos();
                
                // Convert mouse position to tile coordinates
                int tx, ty;
                canvas.ScreenToTile(
                    mousePos.x, mousePos.y,
                    model.grid.tileWidth, model.grid.tileHeight,
                    &tx, &ty
                );
                
                // Pick tile globally (using "" as roomId)
                const std::string globalRoomId = "";
                int pickedTileId = model.GetTileAt(globalRoomId, tx, ty);
                
                // Only pick non-empty tiles
                if (pickedTileId != 0) {
                    selectedTileId = pickedTileId;
                    
                    // Find color name for toast message
                    std::string colorName = "Unknown";
                    for (const auto& tile : model.palette) {
                        if (tile.id == pickedTileId) {
                            colorName = tile.name;
                            break;
                        }
                    }
                    
                    // Auto-switch to Paint tool if toggle is enabled
                    if (eyedropperAutoSwitchToPaint) {
                        currentTool = Tool::Paint;
                    }
                }
                // Note: "No tile to pick" message shown in console only
            }
}

}  // namespace Cartograph
