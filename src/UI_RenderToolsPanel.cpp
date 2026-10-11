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


void UI::RenderToolsPanel(Model& model, History& history, IconManager& icons, 
                          JobQueue& jobs, Canvas& canvas) {
    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoMove | 
        ImGuiWindowFlags_NoCollapse;
    
    ImGui::Begin("Cartograph/Tools", nullptr, flags);
    
    ImGui::Text("Tools");
    ImGui::Separator();
    
    RenderToolButtons(icons, canvas);
    // Show tool options for Select tool
    if (m_canvasPanel.currentTool == CanvasPanel::Tool::Select) {
        RenderSelectOptions(model);
        if (m_canvasPanel.hasSelectedDoor) {
            RenderDoorInspector(model, history);
        }
    }
    
    // Show tool options for Zoom tool
    if (m_canvasPanel.currentTool == CanvasPanel::Tool::Zoom) {
        ImGui::Spacing();
        RenderZoomOptions(canvas, history, model);
    }
    
    // Show tool options for room tools (match regular tool UX)
    if (m_canvasPanel.currentTool == CanvasPanel::Tool::RoomPaint ||
        m_canvasPanel.currentTool == CanvasPanel::Tool::RoomFill) {
        RenderRoomToolOptions(model, history);
    }
    else if (m_canvasPanel.currentTool == CanvasPanel::Tool::RoomErase) {
        ImGui::Spacing();
        ImGui::Text("Room Eraser Options");
        ImGui::Separator();
        
        // Label with tooltip (matching regular Erase tool)
        ImGui::Text("Eraser Size");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Size of room eraser brush\n"
                            "1 = single cell (precise)\n"
                            "5 = 5x5 area (fast erase)");
        }
        
        // Slider below label (matching regular Erase tool)
        ImGui::SetNextItemWidth(-1);  // Full width
        ImGui::SliderInt("##roomEraserSize", &m_canvasPanel.eraserBrushSize, 1, 5);
        
        // Visual preview of eraser size (matching regular Erase tool)
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
                
                // Draw cell (maroon tint for room eraser)
                ImU32 fillColor = inBrush ? 
                    ImGui::GetColorU32(ImVec4(0.8f, 0.2f, 0.2f, 0.4f)) :
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
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Show eyedropper preview when Eyedropper tool is active
    if (m_canvasPanel.currentTool == CanvasPanel::Tool::Eyedropper) {
        RenderEyedropperOptions(model);
    }
    
    // Show palette when Paint tool is active
    if (m_canvasPanel.currentTool == CanvasPanel::Tool::Paint || m_canvasPanel.currentTool == CanvasPanel::Tool::Fill) {
        RenderPaintOptions(model, history);
    }
    
    // Show eraser options when Erase tool is active
    if (m_canvasPanel.currentTool == CanvasPanel::Tool::Erase) {
        RenderEraseOptions();
    }
    
    // Show marker options when Marker tool is active
    if (m_canvasPanel.currentTool == CanvasPanel::Tool::Marker) {
        RenderMarkerSettings(model, history);
        RenderMarkerIcons(model, history, icons, jobs);
        RenderPlacedMarkers(model, history, icons);
    }
    
    // Future: Add layers visibility toggles (grid, tiles, edges, markers)
    
    ImGui::End();
}


// Zoom preset levels (display percentages)
static const int ZOOM_PRESETS[] = {
    10, 25, 50, 75, 100, 150, 200, 400, 800, 1000
};
static const int ZOOM_PRESET_COUNT = 
    sizeof(ZOOM_PRESETS) / sizeof(ZOOM_PRESETS[0]);

// Convert display percentage to internal zoom value
static float DisplayToInternalZoom(int displayPercent) {
    return (displayPercent / 100.0f) * Canvas::DEFAULT_ZOOM;
}


// Convert internal zoom to display percentage
static int InternalToDisplayZoom(float internalZoom) {
    return static_cast<int>((internalZoom / Canvas::DEFAULT_ZOOM) * 100.0f);
}


// Find next zoom preset (higher or lower)
static int GetNextZoomPreset(int currentPercent, bool zoomIn) {
    if (zoomIn) {
        // Find first preset > current
        for (int i = 0; i < ZOOM_PRESET_COUNT; ++i) {
            if (ZOOM_PRESETS[i] > currentPercent) {
                return ZOOM_PRESETS[i];
            }
        }
        return ZOOM_PRESETS[ZOOM_PRESET_COUNT - 1];  // Max
    } else {
        // Find last preset < current
        for (int i = ZOOM_PRESET_COUNT - 1; i >= 0; --i) {
            if (ZOOM_PRESETS[i] < currentPercent) {
                return ZOOM_PRESETS[i];
            }
        }
        return ZOOM_PRESETS[0];  // Min
    }
}


void UI::RenderZoomOptions(Canvas& canvas, History& history, Model& model) {
    ImGui::Text("Zoom");
    ImGui::Separator();
    
    // Get current zoom as display percentage
    int currentPercent = InternalToDisplayZoom(canvas.zoom);
    float oldZoom = canvas.zoom;
    
    // Compact layout: [ - ] [ 100% ] [ + ]
    ImGui::Spacing();
    
    float buttonWidth = 28.0f;
    float inputWidth = 60.0f;
    float spacing = 4.0f;
    
    // Minus button
    if (ImGui::Button("-##zoomOut", ImVec2(buttonWidth, 0))) {
        int newPercent = GetNextZoomPreset(currentPercent, false);
        if (newPercent != currentPercent) {
            float newZoom = DisplayToInternalZoom(newPercent);
            auto cmd = std::make_unique<SetZoomCommand>(
                canvas, oldZoom, newZoom, newPercent
            );
            history.AddCommand(std::move(cmd), model);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Zoom out to previous preset");
    }
    
    ImGui::SameLine(0, spacing);
    
    // Input field for direct percentage entry
    ImGui::SetNextItemWidth(inputWidth);
    static char zoomBuf[16];
    snprintf(zoomBuf, sizeof(zoomBuf), "%d%%", currentPercent);
    
    if (ImGui::InputText("##zoomInput", zoomBuf, sizeof(zoomBuf),
                        ImGuiInputTextFlags_EnterReturnsTrue)) {
        // Parse input (strip % if present)
        int inputPercent = 0;
        if (sscanf(zoomBuf, "%d", &inputPercent) == 1) {
            // Clamp to valid range (10% - 1000%)
            inputPercent = std::clamp(inputPercent, 10, 1000);
            if (inputPercent != currentPercent) {
                float newZoom = DisplayToInternalZoom(inputPercent);
                auto cmd = std::make_unique<SetZoomCommand>(
                    canvas, oldZoom, newZoom, inputPercent
                );
                history.AddCommand(std::move(cmd), model);
            }
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Enter zoom percentage (10%% - 1000%%)");
    }
    
    ImGui::SameLine(0, spacing);
    
    // Plus button
    if (ImGui::Button("+##zoomIn", ImVec2(buttonWidth, 0))) {
        int newPercent = GetNextZoomPreset(currentPercent, true);
        if (newPercent != currentPercent) {
            float newZoom = DisplayToInternalZoom(newPercent);
            auto cmd = std::make_unique<SetZoomCommand>(
                canvas, oldZoom, newZoom, newPercent
            );
            history.AddCommand(std::move(cmd), model);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Zoom in to next preset");
    }
}




} // namespace Cartograph
