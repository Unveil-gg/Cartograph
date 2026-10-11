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

void CanvasPanel::DrawFillPreview(
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
        
        // Convert back to screen coordinates (snapped to grid)
        float wx, wy;
        canvas.TileToWorld(tx, ty, model.grid.tileWidth, 
                          model.grid.tileHeight, &wx, &wy);
        
        float sx, sy;
        canvas.WorldToScreen(wx, wy, &sx, &sy);
        
        float sw = model.grid.tileWidth * canvas.zoom;
        float sh = model.grid.tileHeight * canvas.zoom;
        
        // Get selected tile color for preview
        Color tileColor(0.8f, 0.8f, 0.8f, 0.6f);
        for (const auto& tile : model.palette) {
            if (tile.id == selectedTileId) {
                tileColor = tile.color;
                tileColor.a = 0.6f;  // More opaque for fill preview
                break;
            }
        }
        
        // Draw bucket icon indicator (center cross)
        float centerX = sx + sw / 2.0f;
        float centerY = sy + sh / 2.0f;
        float crossSize = std::min(sw, sh) * 0.3f;
        ImU32 crossColor = ImGui::GetColorU32(
            ImVec4(1.0f, 1.0f, 1.0f, 0.8f)
        );
        
        // Vertical line of cross
        drawList->AddLine(
            ImVec2(centerX, centerY - crossSize),
            ImVec2(centerX, centerY + crossSize),
            crossColor,
            2.0f
        );
        
        // Horizontal line of cross
        drawList->AddLine(
            ImVec2(centerX - crossSize, centerY),
            ImVec2(centerX + crossSize, centerY),
            crossColor,
            2.0f
        );
        
        // Draw semi-transparent fill preview
        ImU32 previewColor = tileColor.ToU32();
        drawList->AddRectFilled(
            ImVec2(sx, sy),
            ImVec2(sx + sw, sy + sh),
            previewColor
        );
        
        // Draw border
        drawList->AddRect(
            ImVec2(sx, sy),
            ImVec2(sx + sw, sy + sh),
            ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.6f)),
            0.0f,
            0,
            2.0f
        );
}

}  // namespace Cartograph
