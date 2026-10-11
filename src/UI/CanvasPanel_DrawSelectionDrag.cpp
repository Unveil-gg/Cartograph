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

void CanvasPanel::DrawSelectionDrag(
    Model& model,
    ImDrawList* drawList
) {
        // Calculate rectangle bounds
        float minX = std::min(selectionStartX, selectionEndX);
        float minY = std::min(selectionStartY, selectionEndY);
        float maxX = std::max(selectionStartX, selectionEndX);
        float maxY = std::max(selectionStartY, selectionEndY);
        
        // Draw semi-transparent fill (using theme colors)
        ImU32 fillColor = ImGui::GetColorU32(
            model.theme.selectionFill.ToImVec4()
        );
        drawList->AddRectFilled(
            ImVec2(minX, minY),
            ImVec2(maxX, maxY),
            fillColor
        );
        
        // Draw border (using theme colors)
        ImU32 borderColor = ImGui::GetColorU32(
            model.theme.selectionBorder.ToImVec4()
        );
        drawList->AddRect(
            ImVec2(minX, minY),
            ImVec2(maxX, maxY),
            borderColor,
            0.0f,
            0,
            2.0f
        );
}

}  // namespace Cartograph
