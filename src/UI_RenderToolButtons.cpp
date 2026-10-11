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

void UI::RenderToolButtons(
    IconManager& icons,
    Canvas& canvas
) {
    const char* toolNames[] = {
        "Move", "Select", "Paint", "Erase", "Fill", 
        "Marker", "Eyedropper", "Zoom"
    };
    
    const char* toolIconNames[] = {
        "move", "square-dashed", "paintbrush", "paint-bucket", "eraser",
        "map-pinned", "pipette", "zoom-in"
    };
    
    const char* toolShortcuts[] = {
        "V", "S", "B", "E", "F", "M", "I", "Z"
    };
    
    // Icon button size and spacing
    const ImVec2 iconButtonSize(36.0f, 36.0f);
    const float iconSpacing = 6.0f;
    const float panelPadding = 8.0f;
    const int TOOLS_PER_ROW = 4;  // Fixed 4-column grid
    
    // Add padding
    ImGui::Dummy(ImVec2(0, panelPadding * 0.5f));
    
    for (int i = 0; i < 8; ++i) {
        bool selected = (static_cast<int>(m_canvasPanel.currentTool) == i);
        
        ImGui::PushID(i);
        
        // Get icon for this tool
        const Icon* icon = icons.GetIcon(toolIconNames[i]);
        
        // Prominent selection highlight
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, 
                ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                ImVec4(0.36f, 0.69f, 1.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                ImVec4(0.16f, 0.49f, 0.88f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, 
                ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3.0f);
        }
        
        bool clicked = false;
        
        if (icon) {
            // Draw icon button with UV coordinates
            ImTextureID texId = icons.GetAtlasTexture();
            ImVec2 uv0(icon->u0, icon->v0);
            ImVec2 uv1(icon->u1, icon->v1);
            
            clicked = ImGui::ImageButton(toolIconNames[i], texId, 
                                         iconButtonSize, uv0, uv1);
        } else {
            // Fallback to text button if icon not found
            clicked = ImGui::Button(toolNames[i], iconButtonSize);
        }
        
        // Tooltip on button hover (force to main viewport to avoid clipping)
        if (ImGui::IsItemHovered()) {
            ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
            ImVec2 mousePos = ImGui::GetMousePos();
            ImGui::SetNextWindowPos(ImVec2(mousePos.x + 15, mousePos.y + 10));
            ImGui::BeginTooltip();
            switch (i) {
                case 0: // Move
                    ImGui::Text("Move Tool [V]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Drag to pan the canvas\nWheel to zoom in/out");
                    break;
                case 1: // Select
                    ImGui::Text("Select Tool [S]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Drag to create selection rectangle");
                    break;
                case 2: // Paint
                    ImGui::Text("Paint Tool [B]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Left-click: Paint with selected color\n"
                        "Right-click: Erase\n"
                        "E+Click: Erase (alternative)");
                    break;
                case 3: // Fill
                    ImGui::Text("Fill Tool [F]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click to fill area");
                    break;
                case 4: // Erase
                    ImGui::Text("Erase Tool [E]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click to erase tiles");
                    break;
                case 5: // Marker
                    ImGui::Text("Marker Tool [M]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click to place marker");
                    break;
                case 6: // Eyedropper
                    ImGui::Text("Eyedropper Tool [I]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click to pick tile color");
                    break;
                case 7: // Zoom
                    ImGui::Text("Zoom Tool [Z]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Left-click: Zoom in at point\n"
                        "Right-click: Zoom out at point");
                    break;
                default:
                    ImGui::Text("%s", toolNames[i]);
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Tool not yet implemented");
                    break;
            }
            ImGui::EndTooltip();
        }
        
        if (selected) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        }
        
        if (clicked) {
            // Clear selection when switching away from Select tool
            if (m_canvasPanel.currentTool == CanvasPanel::Tool::Select && 
                static_cast<CanvasPanel::Tool>(i) != CanvasPanel::Tool::Select) {
                m_canvasPanel.ClearSelection();
            }
            m_canvasPanel.currentTool = static_cast<CanvasPanel::Tool>(i);
        }
        
        // Fixed 4-column grid layout
        if ((i + 1) % TOOLS_PER_ROW != 0 && i < 7) {
            ImGui::SameLine(0, iconSpacing);
        }
        
        ImGui::PopID();
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Room Tools (with maroon background)
    ImGui::Text("Room Tools");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, panelPadding * 0.5f));
    
    const char* roomToolNames[] = {
        "Room Select", "Room Paint", "Room Fill", "Room Erase"
    };
    
    const char* roomToolIconNames[] = {
        "mouse-pointer-2", "paintbrush", "paint-bucket", "eraser"
    };
    
    const char* roomToolShortcuts[] = {
        "Shift+S", "Shift+R", "Shift+F", "Shift+E"
    };
    
    const ImVec4 maroonBg(0.55f, 0.1f, 0.1f, 1.0f);
    const ImVec4 maroonBgHover(0.65f, 0.2f, 0.2f, 1.0f);
    const ImVec4 maroonBgActive(0.45f, 0.05f, 0.05f, 1.0f);
    
    for (int i = 0; i < 4; ++i) {
        int toolIdx = 8 + i;  // RoomSelect=8, RoomPaint=9, RoomFill=10, RoomErase=11
        bool selected = (static_cast<int>(m_canvasPanel.currentTool) == toolIdx);
        
        ImGui::PushID(toolIdx);
        
        // Get icon for this tool
        const Icon* icon = icons.GetIcon(roomToolIconNames[i]);
        
        // Maroon background for room tools
        if (selected) {
            // Selected: brighter blue border
            ImGui::PushStyleColor(ImGuiCol_Button, maroonBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, maroonBgHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, maroonBgActive);
            ImGui::PushStyleColor(ImGuiCol_Border, 
                ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3.0f);
        } else {
            // Unselected: maroon background
            ImGui::PushStyleColor(ImGuiCol_Button, maroonBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, maroonBgHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, maroonBgActive);
        }
        
        bool clicked = false;
        
        if (icon) {
            // Draw icon button with UV coordinates
            ImTextureID texId = icons.GetAtlasTexture();
            ImVec2 uv0(icon->u0, icon->v0);
            ImVec2 uv1(icon->u1, icon->v1);
            
            clicked = ImGui::ImageButton(roomToolIconNames[i], texId, 
                                         iconButtonSize, uv0, uv1);
        } else {
            // Fallback to text button if icon not found
            clicked = ImGui::Button(roomToolNames[i], iconButtonSize);
        }
        
        // Tooltip on button hover
        if (ImGui::IsItemHovered()) {
            ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
            ImVec2 mousePos = ImGui::GetMousePos();
            ImGui::SetNextWindowPos(ImVec2(mousePos.x + 15, mousePos.y + 10));
            ImGui::BeginTooltip();
            switch (i) {
                case 0: // Room Select
                    ImGui::Text("Room Select Tool [Shift+S]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click cell to select its room");
                    break;
                case 1: // Room Paint
                    ImGui::Text("Room Paint Tool [Shift+R]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click/drag to assign cells to active room");
                    break;
                case 2: // Room Fill
                    ImGui::Text("Room Fill Tool [Shift+F]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click to flood-fill area into active room");
                    break;
                case 3: // Room Erase
                    ImGui::Text("Room Erase Tool [Shift+E]");
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                        "Click/drag to remove cells from rooms");
                    break;
            }
            ImGui::EndTooltip();
        }
        
        if (selected) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(4);
        } else {
            ImGui::PopStyleColor(3);
        }
        
        if (clicked) {
            // Clear selection when switching away from Select tool
            if (m_canvasPanel.currentTool == CanvasPanel::Tool::Select) {
                m_canvasPanel.ClearSelection();
            }
            m_canvasPanel.currentTool = static_cast<CanvasPanel::Tool>(toolIdx);
            
            // Auto-open hierarchy panel when selecting room tools
            if (!showPropertiesPanel) {
                showPropertiesPanel = true;
                m_layoutInitialized = false;  // Trigger layout rebuild
            }
        }
        
        // Fixed 4-column grid layout
        if ((i + 1) % TOOLS_PER_ROW != 0 && i < 3) {
            ImGui::SameLine(0, iconSpacing);
        }
        
        ImGui::PopID();
    }
    
}

}  // namespace Cartograph
