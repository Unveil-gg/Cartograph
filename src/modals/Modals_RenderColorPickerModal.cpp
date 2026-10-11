#include "Modals.h"
#include "../App.h"
#include "../UI.h"
#include "../Canvas.h"
#include "../Config.h"
#include "../Preferences.h"
#include "../Package.h"
#include "../ProjectFolder.h"
#include "../Theme/Themes.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace Cartograph {


void Modals::RenderColorPickerModal(Model& model, History& history,
                                    int& selectedTileId) {
    if (!showColorPickerModal) {
        colorPickerModalOpened = false;
        return;
    }
    
    // Only call OpenPopup once when modal is first shown
    if (!colorPickerModalOpened) {
        ImGui::OpenPopup("Color Picker");
        colorPickerModalOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
        ImVec2(0.5f, 0.5f));
    
    // Set fixed size to prevent resize animation
    ImGui::SetNextWindowSize(ImVec2(450, 550), ImGuiCond_Always);
    
    bool modalOpen = true;
    if (ImGui::BeginPopupModal("Color Picker", &modalOpen, 
        ImGuiWindowFlags_NoResize)) {
        
        // Modal title/header
        if (colorPickerEditingTileId == -1) {
            ImGui::Text("Add New Color");
        } else {
            ImGui::Text("Edit Color");
        }
        ImGui::Separator();
        ImGui::Spacing();
        
        // Name input with auto-focus on first open
        ImGui::Text("Name:");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        }
        bool nameEnterPressed = ImGui::InputText(
            "##colorname", 
            colorPickerName, 
            sizeof(colorPickerName),
            ImGuiInputTextFlags_EnterReturnsTrue
        );
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Color picker widget
        ImGui::Text("Color:");
        ImGuiColorEditFlags flags = 
            ImGuiColorEditFlags_AlphaBar | 
            ImGuiColorEditFlags_AlphaPreview |
            ImGuiColorEditFlags_DisplayRGB |
            ImGuiColorEditFlags_DisplayHex;
        
        ImGui::ColorPicker4("##colorpicker", colorPickerColor, flags);
        
        ImGui::Spacing();
        
        // Preview: Original vs New (only when editing)
        if (colorPickerEditingTileId != -1) {
            ImGui::Text("Preview:");
            
            ImGui::BeginGroup();
            ImGui::Text("Original");
            ImGui::ColorButton("##original", 
                ImVec4(colorPickerOriginalColor[0], 
                      colorPickerOriginalColor[1],
                      colorPickerOriginalColor[2], 
                      colorPickerOriginalColor[3]),
                0, ImVec2(60, 60));
            ImGui::EndGroup();
            
            ImGui::SameLine(0, 20);
            
            ImGui::BeginGroup();
            ImGui::Text("New");
            ImGui::ColorButton("##new", 
                ImVec4(colorPickerColor[0], colorPickerColor[1],
                      colorPickerColor[2], colorPickerColor[3]),
                0, ImVec2(60, 60));
            ImGui::EndGroup();
            
            ImGui::Spacing();
        }
        
        ImGui::Separator();
        ImGui::Spacing();
        
        // Validation messages
        bool nameValid = strlen(colorPickerName) > 0 && 
                        strlen(colorPickerName) < 64;
        bool canSave = nameValid;
        
        // Check for empty name
        if (strlen(colorPickerName) == 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
                "Please enter a color name");
            ImGui::Spacing();
        }
        // Check for overly long name
        else if (strlen(colorPickerName) >= 64) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
                "Color name is too long (max 63 characters)");
            canSave = false;
            ImGui::Spacing();
        }
        // Optional: Warn about duplicate names (non-blocking)
        else {
            bool duplicateFound = false;
            for (const auto& tile : model.palette) {
                if (tile.id != colorPickerEditingTileId &&
                    tile.name == std::string(colorPickerName)) {
                    duplicateFound = true;
                    break;
                }
            }
            if (duplicateFound) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
                    "Note: A color with this name already exists");
                ImGui::Spacing();
            }
        }
        
        // Check palette size limit
        if (colorPickerEditingTileId == -1 && 
            model.palette.size() >= Limits::MAX_PALETTE_ENTRIES) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                "Palette is full (max 32 colors)");
            canSave = false;
            ImGui::Spacing();
        }
        
        // Keyboard shortcuts (Enter to save, Escape to cancel)
        bool shouldSave = false;
        bool shouldCancel = false;
        
        if (canSave && (nameEnterPressed || 
            ImGui::IsKeyPressed(ImGuiKey_Enter, false))) {
            shouldSave = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            shouldCancel = true;
        }
        
        // Action buttons
        if (!canSave) {
            ImGui::BeginDisabled();
        }
        
        if (ImGui::Button("Save", ImVec2(120, 0)) || shouldSave) {
            // Create color from picker
            Color color(colorPickerColor[0], colorPickerColor[1],
                       colorPickerColor[2], colorPickerColor[3]);
            std::string name = colorPickerName;
            
            if (colorPickerEditingTileId == -1) {
                // Add new color
                auto cmd = std::make_unique<AddPaletteColorCommand>(
                    name, color);
                history.AddCommand(std::move(cmd), model, true);
                
                // Select the newly added color
                selectedTileId = model.palette.back().id;
                
                m_ui.ShowToast("Color added: " + name, Toast::Type::Success);
            } else {
                // Update existing color
                auto cmd = std::make_unique<UpdatePaletteColorCommand>(
                    colorPickerEditingTileId, name, color);
                history.AddCommand(std::move(cmd), model, true);
                
                m_ui.ShowToast("Color updated: " + name, Toast::Type::Success);
            }
            
            showColorPickerModal = false;
            colorPickerModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        if (!canSave) {
            ImGui::EndDisabled();
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0)) || shouldCancel) {
            showColorPickerModal = false;
            colorPickerModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        // Hint text for keyboard shortcuts
        ImGui::Spacing();
        ImGui::TextDisabled("Tip: Press Enter to save, Escape to cancel");
        
        // Delete button (only for editing existing colors, not Empty)
        if (colorPickerEditingTileId > 0) {
            ImGui::SameLine();
            
            // Check if color is in use
            bool inUse = model.IsPaletteColorInUse(colorPickerEditingTileId);
            
            if (inUse) {
                ImGui::PushStyleColor(ImGuiCol_Button, 
                    ImVec4(0.8f, 0.4f, 0.0f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, 
                    ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            }
            
            if (ImGui::Button("Delete", ImVec2(120, 0))) {
                colorPickerDeleteRequested = true;
            }
            
            ImGui::PopStyleColor();
            
            // Show warning if in use
            if (inUse) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
                    "Warning: This color is currently in use!");
                ImGui::TextWrapped(
                    "Deleting will replace all tiles with Empty (id=0)");
            }
        }
        
        ImGui::EndPopup();
    }
    
    // Handle delete confirmation (separate popup)
    if (colorPickerDeleteRequested) {
        ImGui::OpenPopup("Delete Color?");
        colorPickerDeleteRequested = false;
    }
    
    if (ImGui::BeginPopupModal("Delete Color?", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::Text("Are you sure you want to delete this color?");
        
        bool inUse = model.IsPaletteColorInUse(colorPickerEditingTileId);
        if (inUse) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f),
                "This color is in use.");
            ImGui::TextWrapped(
                "All tiles using this color will be replaced with Empty.");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        if (ImGui::Button("Delete", ImVec2(120, 0))) {
            // Delete the color (replace with Empty id=0)
            auto cmd = std::make_unique<RemovePaletteColorCommand>(
                colorPickerEditingTileId, 0);
            history.AddCommand(std::move(cmd), model, true);
            
            // If this was the selected tile, switch to Empty
            if (selectedTileId == colorPickerEditingTileId) {
                selectedTileId = 0;
            }
            
            m_ui.ShowToast("Color deleted", Toast::Type::Info);
            
            showColorPickerModal = false;
            colorPickerModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    // Close modal if user clicked X
    if (!modalOpen) {
        showColorPickerModal = false;
        colorPickerModalOpened = false;
    }
}


} // namespace Cartograph
