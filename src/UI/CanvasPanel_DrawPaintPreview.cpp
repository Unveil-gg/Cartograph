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

void CanvasPanel::DrawPaintPreview(
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
        
        // Draw preview based on tool and state
        // Note: Erase/paint preview colors could be theme-customizable later
        if (currentTool == Tool::Erase || 
            (currentTool == Tool::Paint && ImGui::IsKeyDown(ImGuiKey_E))) {
            // Erase preview (red fill + red outline)
            ImU32 eraseColor = ImGui::GetColorU32(
                ImVec4(1.0f, 0.3f, 0.3f, 0.6f)
            );
            
            // Calculate brush area for multi-tile eraser
            int halfBrush = eraserBrushSize / 2;
            int startTileX = tx - halfBrush;
            int startTileY = ty - halfBrush;
            
            // Convert start tile to screen coords
            float startWx, startWy;
            canvas.TileToWorld(startTileX, startTileY, 
                             model.grid.tileWidth, model.grid.tileHeight,
                             &startWx, &startWy);
            float startSx, startSy;
            canvas.WorldToScreen(startWx, startWy, &startSx, &startSy);
            
            // Calculate total size of brush area in screen space
            float totalWidth = sw * eraserBrushSize;
            float totalHeight = sh * eraserBrushSize;
            
            // Draw filled semi-transparent rectangle for brush area
            ImU32 eraseFillColor = ImGui::GetColorU32(
                ImVec4(1.0f, 0.3f, 0.3f, 0.3f)
            );
            drawList->AddRectFilled(
                ImVec2(startSx, startSy),
                ImVec2(startSx + totalWidth, startSy + totalHeight),
                eraseFillColor
            );
            
            // Draw outline
            drawList->AddRect(
                ImVec2(startSx, startSy),
                ImVec2(startSx + totalWidth, startSy + totalHeight),
                eraseColor,
                0.0f,
                0,
                2.0f
            );
        } else {
            // Paint preview (brightened tile color + white border)
            Color tileColor(0.8f, 0.8f, 0.8f, 0.4f);
            for (const auto& tile : model.palette) {
                if (tile.id == selectedTileId) {
                    tileColor = tile.color;
                    break;
                }
            }
            
            // Brighten color by 30% for visibility
            float brightenAmount = 0.3f;
            Color brightened;
            brightened.r = std::min(tileColor.r + brightenAmount, 1.0f);
            brightened.g = std::min(tileColor.g + brightenAmount, 1.0f);
            brightened.b = std::min(tileColor.b + brightenAmount, 1.0f);
            brightened.a = 0.6f;  // Semi-transparent
            
            // Draw preview fill
            drawList->AddRectFilled(
                ImVec2(sx, sy),
                ImVec2(sx + sw, sy + sh),
                brightened.ToU32()
            );
            
            // Draw white border for visibility
            ImU32 borderColor = ImGui::GetColorU32(
                ImVec4(1.0f, 1.0f, 1.0f, 0.9f)
            );
            float borderThickness = 3.0f;
            drawList->AddRect(
                ImVec2(sx, sy),
                ImVec2(sx + sw, sy + sh),
                borderColor,
                0.0f,
                0,
                borderThickness
            );
        }
        
        // Draw edge hover preview for Paint tool
        if (currentTool == Tool::Paint && isHoveringEdge) {
            // Calculate edge line endpoints based on hovered edge
            int x1 = hoveredEdge.x1;
            int y1 = hoveredEdge.y1;
            int x2 = hoveredEdge.x2;
            int y2 = hoveredEdge.y2;
            
            // Determine if this is a vertical or horizontal edge
            bool isVertical = (x1 != x2);
            
            float wx1, wy1, wx2, wy2;
            if (isVertical) {
                // Vertical edge (Y-up: negate Y for world coords)
                wx1 = std::max(x1, x2) * model.grid.tileWidth;
                wx2 = wx1;
                int minY = std::min(y1, y2);
                wy1 = -minY * model.grid.tileHeight;
                wy2 = wy1 + model.grid.tileHeight;
            } else {
                // Horizontal edge (Y-up: boundary at -minY * tileHeight)
                int minY = std::min(y1, y2);
                wy1 = -minY * model.grid.tileHeight;
                wy2 = wy1;
                wx1 = std::min(x1, x2) * model.grid.tileWidth;
                wx2 = wx1 + model.grid.tileWidth;
            }
            
            // Convert to screen coordinates
            float esx1, esy1, esx2, esy2;
            canvas.WorldToScreen(wx1, wy1, &esx1, &esy1);
            canvas.WorldToScreen(wx2, wy2, &esx2, &esy2);
            
            // Get current edge state
            EdgeState currentState = model.GetEdgeState(hoveredEdge);
            
            // Draw ghost preview showing what will happen
            ImU32 edgePreviewColor;
            if (currentState == EdgeState::None) {
                // No edge exists - show wall preview (green)
                edgePreviewColor = ImGui::GetColorU32(
                    ImVec4(0.3f, 1.0f, 0.3f, 0.7f)
                );
            } else if (currentState == EdgeState::Wall) {
                // Wall exists - show door preview (blue)
                edgePreviewColor = ImGui::GetColorU32(
                    ImVec4(0.3f, 0.6f, 1.0f, 0.7f)
                );
            } else {
                // Door exists - show none/delete preview (red)
                edgePreviewColor = ImGui::GetColorU32(
                    ImVec4(1.0f, 0.3f, 0.3f, 0.7f)
                );
            }
            
            // Draw preview line (thicker to be visible)
            drawList->AddLine(
                ImVec2(esx1, esy1),
                ImVec2(esx2, esy2),
                edgePreviewColor,
                4.0f * canvas.zoom
            );
        }
}

}  // namespace Cartograph
