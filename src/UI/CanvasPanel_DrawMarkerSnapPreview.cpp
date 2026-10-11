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

void CanvasPanel::DrawMarkerSnapPreview(
    Model& model,
    Canvas& canvas,
    ImDrawList* drawList
) {
        ImVec2 mousePos = ImGui::GetMousePos();
        
        // Convert to world coordinates
        float wx, wy;
        canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
        
        // Convert to fractional tile coordinates (Y-up: negate world Y)
        float tileX = wx / model.grid.tileWidth;
        float tileY = -wy / model.grid.tileHeight;
        
        // Get base tile
        int baseTileX = static_cast<int>(std::floor(tileX));
        int baseTileY = static_cast<int>(std::floor(tileY));
        float fractionalX = tileX - baseTileX;
        float fractionalY = tileY - baseTileY;
        
        // Get snap points for current preset
        auto snapPoints = model.GetMarkerSnapPoints();
        
        // Find nearest snap point
        float minDist = FLT_MAX;
        float bestSnapX = 0.5f, bestSnapY = 0.5f;
        
        for (const auto& snap : snapPoints) {
            float dx = fractionalX - snap.first;
            float dy = fractionalY - snap.second;
            float dist = dx*dx + dy*dy;
            if (dist < minDist) {
                minDist = dist;
                bestSnapX = snap.first;
                bestSnapY = snap.second;
            }
        }
        
        // Calculate final snapped position
        float snappedTileX = baseTileX + bestSnapX;
        float snappedTileY = baseTileY + bestSnapY;
        
        // Convert to world coordinates
        float snappedWx = snappedTileX * model.grid.tileWidth;
        float snappedWy = snappedTileY * model.grid.tileHeight;
        
        // Convert to screen coordinates
        float snappedSx, snappedSy;
        canvas.WorldToScreen(snappedWx, snappedWy, &snappedSx, &snappedSy);
        
        // Draw all snap points for current tile (subtle indicators)
        for (const auto& snap : snapPoints) {
            float snapWx = (baseTileX + snap.first) * model.grid.tileWidth;
            float snapWy = (baseTileY + snap.second) * model.grid.tileHeight;
            
            float snapSx, snapSy;
            canvas.WorldToScreen(snapWx, snapWy, &snapSx, &snapSy);
            
            // Draw small dot at snap point
            float dotRadius = 3.0f;
            ImU32 dotColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.4f));
            drawList->AddCircleFilled(
                ImVec2(snapSx, snapSy),
                dotRadius,
                dotColor,
                8
            );
        }
        
        // Draw ghost marker at snapped position (larger, highlighted)
        float minDim = static_cast<float>(std::min(model.grid.tileWidth, model.grid.tileHeight));
        float markerSize = minDim * canvas.zoom * 0.6f;
        
        // Draw ghost marker with pulsing effect
        ImU32 ghostColor = ImGui::GetColorU32(ImVec4(
            markerColor.r, 
            markerColor.g, 
            markerColor.b, 
            0.5f  // Semi-transparent
        ));
        
        drawList->AddCircleFilled(
            ImVec2(snappedSx, snappedSy),
            markerSize / 2.0f,
            ghostColor,
            16
        );
        
        // Draw border around ghost marker
        ImU32 borderColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.7f));
        drawList->AddCircle(
            ImVec2(snappedSx, snappedSy),
            markerSize / 2.0f,
            borderColor,
            16,
            2.0f
        );
        
        // Draw crosshair at snap point for precision
        float crossSize = 8.0f;
        ImU32 crossColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.8f));
        
        drawList->AddLine(
            ImVec2(snappedSx - crossSize, snappedSy),
            ImVec2(snappedSx + crossSize, snappedSy),
            crossColor,
            1.5f
        );
        drawList->AddLine(
            ImVec2(snappedSx, snappedSy - crossSize),
            ImVec2(snappedSx, snappedSy + crossSize),
            crossColor,
            1.5f
        );
}

}  // namespace Cartograph
