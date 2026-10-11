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


void Modals::RenderExportModal(Model& model, Canvas& canvas) {
    // Only call OpenPopup once when modal is first shown
    if (!exportModalOpened) {
        ImGui::OpenPopup("Export PNG");
        exportModalOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
                           ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Export PNG", nullptr, 
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        // Calculate content dimensions for display
        ContentBounds bounds = model.CalculateContentBounds();
        
        // Check if there's content to export
        if (bounds.isEmpty) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), 
                "Warning: No content to export!");
            ImGui::Text("Draw some tiles, walls, or markers first.");
            ImGui::Spacing();
            
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                showExportModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
            return;
        }
        
        int contentWidthTiles = bounds.maxX - bounds.minX + 1;
        int contentHeightTiles = bounds.maxY - bounds.minY + 1;
        int contentWidthPx = contentWidthTiles * model.grid.tileWidth;
        int contentHeightPx = contentHeightTiles * model.grid.tileHeight;
        
        // Display content info
        ImGui::Text("Content Area: %d × %d pixels (%d × %d tiles)",
                   contentWidthPx, contentHeightPx,
                   contentWidthTiles, contentHeightTiles);
        ImGui::Separator();
        
        // Size mode selection
        ImGui::Text("Size Mode:");
        
        int mode = (int)exportOptions.sizeMode;
        if (ImGui::RadioButton("Scale", &mode, 0)) {
            exportOptions.sizeMode = ExportOptions::SizeMode::Scale;
        }
        
        if (exportOptions.sizeMode == ExportOptions::SizeMode::Scale) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            ImGui::SliderInt("##scale", &exportOptions.scale, 1, 4);
            
            int outW = (contentWidthPx + exportOptions.padding * 2) * 
                      exportOptions.scale;
            int outH = (contentHeightPx + exportOptions.padding * 2) * 
                      exportOptions.scale;
            ImGui::SameLine();
            ImGui::Text("→ %d × %d px", outW, outH);
        }
        
        if (ImGui::RadioButton("Custom Dimensions", &mode, 1)) {
            exportOptions.sizeMode = 
                ExportOptions::SizeMode::CustomDimensions;
        }
        
        if (exportOptions.sizeMode == 
            ExportOptions::SizeMode::CustomDimensions) {
            ImGui::Indent();
            ImGui::SetNextItemWidth(120);
            ImGui::InputInt("Width", &exportOptions.customWidth);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120);
            ImGui::InputInt("Height", &exportOptions.customHeight);
            
            // Clamp to reasonable range
            exportOptions.customWidth = std::clamp(
                exportOptions.customWidth, 64, 
                ExportOptions::MAX_DIMENSION);
            exportOptions.customHeight = std::clamp(
                exportOptions.customHeight, 64, 
                ExportOptions::MAX_DIMENSION);
            
            ImGui::Text("(scales to fit, maintains aspect ratio)");
            ImGui::Unindent();
        }
        
        ImGui::Separator();
        
        // Padding control
        ImGui::SetNextItemWidth(120);
        ImGui::SliderInt("Padding (px)", &exportOptions.padding, 0, 64);
        
        ImGui::Separator();
        
        // Transparency
        ImGui::Checkbox("Transparency", &exportOptions.transparency);
        
        if (!exportOptions.transparency) {
            ImGui::SameLine();
            ImGui::ColorEdit3("Background", &exportOptions.bgColorR);
        }
        
        ImGui::Separator();
        ImGui::Text("Layers");
        ImGui::Checkbox("Grid", &exportOptions.layerGrid);
        ImGui::SameLine();
        ImGui::Checkbox("Tiles", &exportOptions.layerTiles);
        ImGui::Checkbox("Walls & Doors", &exportOptions.layerDoors);
        ImGui::SameLine();
        ImGui::Checkbox("Markers", &exportOptions.layerMarkers);
        
        // Show marker style option when markers are enabled
        if (exportOptions.layerMarkers) {
            ImGui::Indent(20.0f);
            ImGui::Checkbox("Use Simple Icons", &exportOptions.useSimpleMarkers);
            ImGui::Unindent(20.0f);
        }
        
        ImGui::Separator();
        
        // Export button - triggers file dialog
        if (ImGui::Button("Export...", ImVec2(120, 0))) {
            showExportModal = false;
            exportModalOpened = false;
            ImGui::CloseCurrentPopup();
            // Trigger the export dialog (will be called after modal closes)
            shouldShowExportPngDialog = true;
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showExportModal = false;
            exportModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    // Reset opened flag if modal was closed
    if (!showExportModal) {
        exportModalOpened = false;
    }
}


void Modals::RenderSettingsModal(App& app, Model& model, 
                                  KeymapManager& keymap) {
    // Only call OpenPopup once when modal is first shown
    if (!settingsModalOpened) {
        ImGui::OpenPopup("Settings");
        settingsModalOpened = true;
        
        // Capture original folder name and title for rename detection
        const std::string& currentPath = app.GetCurrentFilePath();
        if (!currentPath.empty()) {
            settingsOriginalFolderName = 
                ProjectFolder::GetFolderNameFromPath(currentPath);
        } else {
            settingsOriginalFolderName.clear();
        }
        settingsOriginalTitle = model.meta.title;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(600, 650), ImGuiCond_Appearing);
    
    if (ImGui::BeginPopupModal("Settings", nullptr, 
        ImGuiWindowFlags_NoResize)) {
        
        // Apply increased padding for better spacing
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 16));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 10));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
        
        // Style the tab bar for better visibility
        ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TabHovered, 
            ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TabActive, 
            ImVec4(0.25f, 0.45f, 0.65f, 1.0f));
        
        if (ImGui::BeginTabBar("SettingsTabs", ImGuiTabBarFlags_None)) {
            
            // ============================================================
            // TAB 1: PROJECT
            // ============================================================
            if (ImGui::BeginTabItem("Project")) {
                RenderProjectSettings(app, model);
            }
            
            // ============================================================
            // TAB 2: GRID & CANVAS
            // ============================================================
            if (ImGui::BeginTabItem("Grid & Canvas")) {
                RenderGridSettings(model);
            }
            
            // ============================================================
            // TAB 3: KEYBINDINGS
            // ============================================================
            if (ImGui::BeginTabItem("Keybindings")) {
                RenderKeybindingSettings(model, keymap);
            }
            
            // ============================================================
            // TAB 4: APPEARANCE
            // ============================================================
            if (ImGui::BeginTabItem("Appearance")) {
                RenderAppearanceSettings(app, model);
            }
            
            ImGui::EndTabBar();
        }
        
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Buttons (outside tab content, always visible)
        if (ImGui::Button("Apply", ImVec2(120, 0))) {
            // Check if title changed and auto-rename folder if valid
            const std::string& currentPath = app.GetCurrentFilePath();
            bool isProjectFolder = !currentPath.empty() &&
                !Package::HasCartExtension(currentPath);
            
            if (isProjectFolder && !settingsOriginalFolderName.empty() &&
                model.meta.title != settingsOriginalTitle) {
                // Title changed - try to rename folder if validation passes
                std::string sanitizedTitle = 
                    ProjectFolder::SanitizeProjectName(model.meta.title);
                
                if (!sanitizedTitle.empty() && 
                    sanitizedTitle != settingsOriginalFolderName) {
                    // Attempt rename (silently skip if it fails)
                    if (app.RenameProjectFolder(model.meta.title)) {
                        settingsOriginalFolderName = sanitizedTitle;
                        settingsOriginalTitle = model.meta.title;
                    }
                    // If rename fails, folder stays as-is but title updates
                }
            }
            
            model.MarkDirty();
            showSettingsModal = false;
            settingsModalOpened = false;
            ImGui::CloseCurrentPopup();
            m_ui.ShowToast("Settings applied", Toast::Type::Success);
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            showSettingsModal = false;
            settingsModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}


} // namespace Cartograph
