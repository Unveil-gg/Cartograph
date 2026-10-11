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

void CanvasPanel::DrawMarkerDropPreview(
    Model& model,
    Canvas& canvas,
    ImDrawList* drawList
) {
        const ImGuiPayload* payload = ImGui::GetDragDropPayload();
        if (payload && payload->IsDataType("MARKER_ICON")) {
            ImVec2 mousePos = ImGui::GetMousePos();
            
            // Convert to world coordinates
            float wx, wy;
            canvas.ScreenToWorld(mousePos.x, mousePos.y, &wx, &wy);
            
            // Convert to fractional tile coordinates (Y-up: negate world Y)
            float tileX = wx / model.grid.tileWidth;
            float tileY = -wy / model.grid.tileHeight;
            
            // Snap to nearest snap point
            auto snapPoints = model.GetMarkerSnapPoints();
            int baseTileX = static_cast<int>(std::floor(tileX));
            int baseTileY = static_cast<int>(std::floor(tileY));
            float fractionalX = tileX - baseTileX;
            float fractionalY = tileY - baseTileY;
            
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
            
            float snappedTileX = baseTileX + bestSnapX;
            float snappedTileY = baseTileY + bestSnapY;
            
            // Convert to screen coordinates
            float snappedWx = snappedTileX * model.grid.tileWidth;
            float snappedWy = snappedTileY * model.grid.tileHeight;
            float snappedSx, snappedSy;
            canvas.WorldToScreen(snappedWx, snappedWy, &snappedSx, &snappedSy);
            
            // Draw ghost marker preview
            float minDim = static_cast<float>(std::min(model.grid.tileWidth, model.grid.tileHeight));
            float markerSize = minDim * canvas.zoom * 0.6f;
            
            ImU32 ghostColor = ImGui::GetColorU32(ImVec4(
                markerColor.r, markerColor.g, markerColor.b, 0.5f
            ));
            
            drawList->AddCircleFilled(
                ImVec2(snappedSx, snappedSy),
                markerSize / 2.0f,
                ghostColor,
                16
            );
            
            // Draw border
            ImU32 borderColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.7f));
            drawList->AddCircle(
                ImVec2(snappedSx, snappedSy),
                markerSize / 2.0f,
                borderColor,
                16,
                2.0f
            );
            
            // Draw "drop here" text
            const char* dropText = "Drop to place marker";
            ImVec2 textSize = ImGui::CalcTextSize(dropText);
            ImVec2 textPos(snappedSx - textSize.x / 2.0f, 
                          snappedSy + markerSize / 2.0f + 8.0f);
            
            // Text background
            drawList->AddRectFilled(
                ImVec2(textPos.x - 4, textPos.y - 2),
                ImVec2(textPos.x + textSize.x + 4, textPos.y + textSize.y + 2),
                ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.7f))
            );
            
            drawList->AddText(textPos, 
                ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)), 
                dropText);
        }
}

}  // namespace Cartograph
