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

void CanvasPanel::HandleToolShortcuts(
    KeymapManager& keymap
) {
        // Toggle hierarchy panel
        if (keymap.IsActionTriggered("togglePropertiesPanel")) {
            *showPropertiesPanel = !(*showPropertiesPanel);
            *layoutInitialized = false;  // Trigger layout rebuild
        }
        
        // Tool switching shortcuts
        Tool prevTool = currentTool;
        
        if (keymap.IsActionTriggered("toolMove")) {
            currentTool = Tool::Move;
        }
        if (keymap.IsActionTriggered("toolSelect")) {
            currentTool = Tool::Select;
        }
        if (keymap.IsActionTriggered("toolPaint")) {
            currentTool = Tool::Paint;
        }
        if (keymap.IsActionTriggered("toolErase")) {
            currentTool = Tool::Erase;
        }
        if (keymap.IsActionTriggered("toolFill")) {
            currentTool = Tool::Fill;
        }
        if (keymap.IsActionTriggered("toolEyedropper")) {
            currentTool = Tool::Eyedropper;
        }
        if (keymap.IsActionTriggered("toolZoom")) {
            currentTool = Tool::Zoom;
        }
        if (keymap.IsActionTriggered("toolMarker")) {
            currentTool = Tool::Marker;
        }
        
        // Room tool shortcuts (auto-open hierarchy panel)
        bool roomToolActivated = false;
        if (keymap.IsActionTriggered("toolRoomPaint")) {
            currentTool = Tool::RoomPaint;
            roomToolActivated = true;
        }
        if (keymap.IsActionTriggered("toolRoomFill")) {
            currentTool = Tool::RoomFill;
            roomToolActivated = true;
        }
        if (keymap.IsActionTriggered("toolRoomErase")) {
            currentTool = Tool::RoomErase;
            roomToolActivated = true;
        }
        if (keymap.IsActionTriggered("toolRoomSelect")) {
            currentTool = Tool::RoomSelect;
            roomToolActivated = true;
        }
        
        // Auto-open hierarchy panel when room tool activated via keyboard
        if (roomToolActivated && showPropertiesPanel && !(*showPropertiesPanel)) {
            *showPropertiesPanel = true;
            *layoutInitialized = false;  // Trigger layout rebuild
        }
        
        // Clear selection when switching away from Select tool
        if (prevTool == Tool::Select && currentTool != Tool::Select) {
            ClearSelection();
        }
        
        // Escape clears selection when Select tool is active
        if (currentTool == Tool::Select && 
            ImGui::IsKeyPressed(ImGuiKey_Escape) && hasSelection) {
            ClearSelection();
        }
}

}  // namespace Cartograph
