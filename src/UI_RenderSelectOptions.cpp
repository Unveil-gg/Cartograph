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

void UI::RenderSelectOptions(
    Model& model
) {
        ImGui::Spacing();
        ImGui::Text("Select Layers");
        ImGui::Separator();
        
        ImGui::Checkbox("Tiles", &m_canvasPanel.selectTiles);
        ImGui::Checkbox("Walls/Doors", &m_canvasPanel.selectEdges);
        ImGui::Checkbox("Markers", &m_canvasPanel.selectMarkers);
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // Selection info
        if (m_canvasPanel.hasSelection && 
            !m_canvasPanel.currentSelection.IsEmpty()) {
            ImGui::TextColored(
                ImVec4(0.4f, 0.7f, 1.0f, 1.0f),
                "Selection:"
            );
            if (m_canvasPanel.currentSelection.TileCount() > 0) {
                ImGui::Text("  %d tiles", 
                    m_canvasPanel.currentSelection.TileCount());
            }
            if (m_canvasPanel.currentSelection.EdgeCount() > 0) {
                ImGui::Text("  %d edges", 
                    m_canvasPanel.currentSelection.EdgeCount());
            }
            if (m_canvasPanel.currentSelection.MarkerCount() > 0) {
                ImGui::Text("  %d markers", 
                    m_canvasPanel.currentSelection.MarkerCount());
            }
            
            ImGui::Spacing();
            
            // Deselect button
            if (ImGui::Button("Deselect", ImVec2(-1, 0))) {
                m_canvasPanel.ClearSelection();
            }
        } else {
            ImGui::TextDisabled("Drag to select content");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // Select All button (always available)
        std::string selectAllLabel = "Select All (" 
                                     + Platform::FormatShortcut("A") + ")";
        if (ImGui::Button(selectAllLabel.c_str(), ImVec2(-1, 0))) {
            m_canvasPanel.SelectAll(model);
        }
        
        // Hint about Edit menu / right-click
        ImGui::Spacing();
        ImGui::TextDisabled("Right-click for Cut/Copy/Paste");
}

}  // namespace Cartograph
