#include "UI.h"
#include "Config.h"
#include "App.h"
#include "Canvas.h"
#include "History.h"
#include "Icons.h"
#include "Jobs.h"
#include "render/Renderer.h"
#include "IOJson.h"
#include "Package.h"
#include "ProjectFolder.h"
#include "platform/Paths.h"
#include "platform/Fs.h"
#include "platform/Time.h"
#include "platform/System.h"
#include "platform/NativeMenu.h"
#include "platform/NativeMenu_imgui.h"
#include "UI/ImGuiHelpers.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_surface.h>
#include <nlohmann/json.hpp>
#include <stb/stb_image.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cfloat>
#include <filesystem>
#include <cstring>
#include <set>
#include <map>
#include <fstream>
#include <stdexcept>
#include <chrono>
#include <ctime>

// GLAD - cross-platform OpenGL loader
#include <glad/gl.h>

// SDL_CursorDeleter implementation (outside namespace)


namespace Cartograph {

void UI::RenderEyedropperOptions(
    Model& model
) {
        ImGui::Text("Eyedropper Tool");
        ImGui::Separator();
        
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Hover to preview");
        
        ImGui::Spacing();
        
        // Get hover preview color (what WILL be picked)
        Color hoverColor(0.5f, 0.5f, 0.5f, 1.0f);
        std::string hoverName = "No tile";
        bool isHoveringTile = false;
        
        if (m_canvasPanel.isHoveringCanvas) {
            const std::string globalRoomId = "";
            int hoveredTileId = model.GetTileAt(
                globalRoomId, m_canvasPanel.hoveredTileX, m_canvasPanel.hoveredTileY
            );
            
            if (hoveredTileId != 0) {
                for (const auto& tile : model.palette) {
                    if (tile.id == hoveredTileId) {
                        hoverColor = tile.color;
                        hoverName = tile.name;
                        isHoveringTile = true;
                        break;
                    }
                }
            }
        }
        
        // If not hovering, show selected color as fallback
        if (!isHoveringTile) {
            for (const auto& tile : model.palette) {
                if (tile.id == m_canvasPanel.selectedTileId) {
                    hoverColor = tile.color;
                    hoverName = tile.name;
                    break;
                }
            }
        }
        
        // Large hover preview
        float previewWidth = ImGui::GetContentRegionAvail().x;
        float hoverHeight = 60.0f;
        
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        ImVec2 hoverMin = cursorPos;
        ImVec2 hoverMax = ImVec2(
            cursorPos.x + previewWidth, 
            cursorPos.y + hoverHeight
        );
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // Draw hover preview
        drawList->AddRectFilled(hoverMin, hoverMax, hoverColor.ToU32());
        
        // Draw border (highlight if hovering tile)
        ImU32 hoverBorderColor = isHoveringTile ? 
            ImGui::GetColorU32(ImVec4(0.0f, 0.8f, 1.0f, 1.0f)) :  // Cyan
            ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f));   // Dark
        float hoverBorderThickness = isHoveringTile ? 2.5f : 1.5f;
        drawList->AddRect(
            hoverMin, hoverMax, hoverBorderColor, 0.0f, 0, 
            hoverBorderThickness
        );
        
        ImGui::Dummy(ImVec2(previewWidth, hoverHeight));
        
        // Display hover color name (centered)
        ImGui::Spacing();
        float hoverTextWidth = ImGui::CalcTextSize(hoverName.c_str()).x;
        float hoverCenterX = 
            (ImGui::GetContentRegionAvail().x - hoverTextWidth) * 0.5f;
        if (hoverCenterX > 0) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + hoverCenterX);
        }
        ImGui::Text("%s", hoverName.c_str());
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Current selection (smaller preview)
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Current Selection:");
        ImGui::Spacing();
        
        Color selectedColor(0.8f, 0.8f, 0.8f, 1.0f);
        std::string selectedName = "Empty";
        for (const auto& tile : model.palette) {
            if (tile.id == m_canvasPanel.selectedTileId) {
                selectedColor = tile.color;
                selectedName = tile.name;
                break;
            }
        }
        
        // Smaller selected preview
        float selectedHeight = 40.0f;
        cursorPos = ImGui::GetCursorScreenPos();
        ImVec2 selectedMin = cursorPos;
        ImVec2 selectedMax = ImVec2(
            cursorPos.x + previewWidth, 
            cursorPos.y + selectedHeight
        );
        
        drawList->AddRectFilled(
            selectedMin, selectedMax, selectedColor.ToU32()
        );
        drawList->AddRect(
            selectedMin, selectedMax,
            ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 1.0f)),
            0.0f, 0, 1.5f
        );
        
        ImGui::Dummy(ImVec2(previewWidth, selectedHeight));
        
        // Display selected name (centered)
        float selectedTextWidth = ImGui::CalcTextSize(selectedName.c_str()).x;
        float selectedCenterX = 
            (ImGui::GetContentRegionAvail().x - selectedTextWidth) * 0.5f;
        if (selectedCenterX > 0) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + selectedCenterX);
        }
        ImGui::TextColored(
            ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", selectedName.c_str()
        );
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Auto-switch to Paint toggle
        ImGui::Checkbox("Auto-switch to Paint", &m_canvasPanel.eyedropperAutoSwitchToPaint);
        
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Automatically switch to Paint tool\n"
                "after picking a color"
            );
        }
}

}  // namespace Cartograph
