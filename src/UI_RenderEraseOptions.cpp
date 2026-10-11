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

void UI::RenderEraseOptions() {
        ImGui::Text("Eraser Options");
        ImGui::Separator();
        
        // Label on its own line
        ImGui::Text("Eraser Size");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Size of eraser brush in tiles\n"
                            "1 = single tile (precise)\n"
                            "5 = 5x5 area (fast erase)");
        }
        
        // Slider below label
        ImGui::SetNextItemWidth(-1);  // Full width
        ImGui::SliderInt("##eraserSize", &m_canvasPanel.eraserBrushSize, 1, 5);
        
        // Visual preview of eraser size
        ImGui::Spacing();
        ImGui::Text("Preview:");
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
        float previewSize = 80.0f;
        float cellSize = previewSize / 5.0f;  // 5x5 grid
        
        // Draw grid background
        for (int y = 0; y < 5; y++) {
            for (int x = 0; x < 5; x++) {
                ImVec2 p0(cursorPos.x + x * cellSize, 
                         cursorPos.y + y * cellSize);
                ImVec2 p1(p0.x + cellSize, p0.y + cellSize);
                
                // Calculate if this cell is within brush
                int centerOffset = 2;  // Center of 5x5 grid
                int halfBrush = m_canvasPanel.eraserBrushSize / 2;
                bool inBrush = (x >= centerOffset - halfBrush && 
                               x <= centerOffset + halfBrush &&
                               y >= centerOffset - halfBrush && 
                               y <= centerOffset + halfBrush);
                
                // Draw cell
                ImU32 fillColor = inBrush ? 
                    ImGui::GetColorU32(ImVec4(1.0f, 0.3f, 0.3f, 0.4f)) :
                    ImGui::GetColorU32(ImVec4(0.3f, 0.3f, 0.3f, 0.3f));
                drawList->AddRectFilled(p0, p1, fillColor);
                
                // Draw grid lines
                ImU32 lineColor = ImGui::GetColorU32(
                    ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
                drawList->AddRect(p0, p1, lineColor);
            }
        }
        
        ImGui::Dummy(ImVec2(previewSize, previewSize));
}

}  // namespace Cartograph
