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

void CanvasPanel::DrawEyedropperPreview(
    Model& model,
    Canvas& canvas,
    ImDrawList* drawList
) {
        ImVec2 mousePos = ImGui::GetMousePos();
        
        // Convert to tile coordinates
        int tx, ty;
        canvas.ScreenToTile(
            mousePos.x, mousePos.y,
            model.grid.tileWidth, model.grid.tileHeight,
            &tx, &ty
        );
        
        // Check if there's a tile at this position
        const std::string globalRoomId = "";
        int hoveredTileId = model.GetTileAt(globalRoomId, tx, ty);
        
        // Only show highlight if there's a non-empty tile
        if (hoveredTileId != 0) {
            // Convert back to screen coordinates (snapped to grid)
            float wx, wy;
            canvas.TileToWorld(
                tx, ty, model.grid.tileWidth, 
                model.grid.tileHeight, &wx, &wy
            );
            
            float sx, sy;
            canvas.WorldToScreen(wx, wy, &sx, &sy);
            
            float sw = model.grid.tileWidth * canvas.zoom;
            float sh = model.grid.tileHeight * canvas.zoom;
            
            // Find the tile color for the highlight
            Color tileColor(0.8f, 0.8f, 0.8f, 0.3f);
            for (const auto& tile : model.palette) {
                if (tile.id == hoveredTileId) {
                    tileColor = tile.color;
                    tileColor.a = 0.3f;  // Semi-transparent overlay
                    break;
                }
            }
            
            // Draw semi-transparent color overlay
            drawList->AddRectFilled(
                ImVec2(sx, sy),
                ImVec2(sx + sw, sy + sh),
                tileColor.ToU32()
            );
            
            // Draw bright cyan border to indicate hovering
            ImU32 borderColor = ImGui::GetColorU32(
                ImVec4(0.0f, 0.8f, 1.0f, 1.0f)
            );
            drawList->AddRect(
                ImVec2(sx, sy),
                ImVec2(sx + sw, sy + sh),
                borderColor,
                0.0f,
                0,
                3.0f  // Thick border for visibility
            );
        }
}

}  // namespace Cartograph
