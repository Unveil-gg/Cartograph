#include "Modals.h"
#include "../App.h"
#include "../UI.h"
#include "../Canvas.h"
#include "../Config.h"
#include "../Preferences.h"
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
                settingsModalSelectedTab = 0;
                ImGui::Spacing();
                ImGui::Spacing();
                
                ImGui::Text("Project Information");
                ImGui::Separator();
                ImGui::Spacing();
                
                // Project metadata
                char titleBuf[256];
                strncpy(titleBuf, model.meta.title.c_str(), 
                    sizeof(titleBuf) - 1);
                titleBuf[sizeof(titleBuf) - 1] = '\0';
                if (ImGui::InputText("Title", titleBuf, sizeof(titleBuf))) {
                    model.meta.title = titleBuf;
                    model.MarkDirty();
                }
                
                // Show "Rename Folder" button if title differs from folder 
                const std::string& currentPath = app.GetCurrentFilePath();
                // Check if current project is a folder (not a .cart file)
                bool isProjectFolder = !currentPath.empty() && 
                    !(currentPath.size() >= 5 && 
                      currentPath.substr(currentPath.size() - 5) == ".cart");
                
                if (isProjectFolder && !settingsOriginalFolderName.empty()) {
                    std::string sanitizedTitle = 
                        ProjectFolder::SanitizeProjectName(model.meta.title);
                    
                    if (!sanitizedTitle.empty() && 
                        sanitizedTitle != settingsOriginalFolderName) {
                        ImGui::SameLine();
                        if (ImGui::Button("Rename Folder")) {
                            if (app.RenameProjectFolder(model.meta.title)) {
                                // Update tracked names on success
                                settingsOriginalFolderName = sanitizedTitle;
                                settingsOriginalTitle = model.meta.title;
                            } else {
                                // Revert title to match original on failure
                                model.meta.title = settingsOriginalTitle;
                            }
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip(
                                "Rename project folder to \"%s\"", 
                                sanitizedTitle.c_str());
                        }
                    }
                }
                
                char authorBuf[256];
                strncpy(authorBuf, model.meta.author.c_str(), 
                    sizeof(authorBuf) - 1);
                authorBuf[sizeof(authorBuf) - 1] = '\0';
                if (ImGui::InputText("Author", authorBuf, sizeof(authorBuf))) {
                    model.meta.author = authorBuf;
                    model.MarkDirty();
                }
                
                ImGui::Spacing();
                
                // Project description (multi-line)
                char descBuf[2048];
                strncpy(descBuf, model.meta.description.c_str(), 
                    sizeof(descBuf) - 1);
                descBuf[sizeof(descBuf) - 1] = '\0';
                ImGui::Text("Description");
                if (ImGui::InputTextMultiline("##description", descBuf, 
                                              sizeof(descBuf),
                                              ImVec2(-1, 120.0f),
                                              ImGuiInputTextFlags_WordWrap)) {
                    model.meta.description = descBuf;
                    model.MarkDirty();
                }
                
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                // Preview info
                ImGui::Text("Canvas Information");
                ImGui::Separator();
                ImGui::Spacing();
                
                int totalCells = model.grid.cols * model.grid.rows;
                int pixelWidth = model.grid.cols * model.grid.tileWidth;
                int pixelHeight = model.grid.rows * model.grid.tileHeight;
                
                ImGui::Text("Total cells: %d", totalCells);
                ImGui::Text("Canvas size: %d × %d pixels", 
                    pixelWidth, pixelHeight);
                ImGui::Text("Cell size: %d × %d pixels", 
                    model.grid.tileWidth, model.grid.tileHeight);
                
                ImGui::Spacing();
                
                ImGui::EndTabItem();
            }
            
            // ============================================================
            // TAB 2: GRID & CANVAS
            // ============================================================
            if (ImGui::BeginTabItem("Grid & Canvas")) {
                settingsModalSelectedTab = 1;
                ImGui::Spacing();
                ImGui::Spacing();
                
                // Grid Cell Type
                ImGui::Text("Grid Cell Type");
                ImGui::Separator();
                ImGui::Spacing();
                
                bool canChangePreset = model.CanChangeGridPreset();
                
                if (!canChangePreset) {
                    ImGui::BeginDisabled();
                }
                
                bool isSquare = (model.grid.preset == GridPreset::Square);
                if (ImGui::RadioButton("Square (16×16)", isSquare)) {
                    if (canChangePreset) {
                        model.ApplyGridPreset(GridPreset::Square);
                    } else {
                        m_ui.ShowToast(
                            "Cannot change cell type - delete all markers first",
                            Toast::Type::Warning, 3.0f);
                    }
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Square cells for top-down games. "
                        "Markers snap to center only.");
                }
                
                bool isRectangle = 
                    (model.grid.preset == GridPreset::Rectangle);
                if (ImGui::RadioButton("Rectangle (32×16)", isRectangle)) {
                    if (canChangePreset) {
                        model.ApplyGridPreset(GridPreset::Rectangle);
                    } else {
                        m_ui.ShowToast(
                            "Cannot change cell type - delete all markers first",
                            Toast::Type::Warning, 3.0f);
                    }
                }
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Rectangular cells for side-scrollers. "
                        "Markers snap to left/right positions.");
                }
                
                if (!canChangePreset) {
                    ImGui::EndDisabled();
                    ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.3f, 1.0f),
                        "🔒 Locked (%zu marker%s placed)",
                        model.markers.size(),
                        model.markers.size() == 1 ? "" : "s");
                    ImGui::SameLine();
                    ImGui::TextDisabled("(?)");
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip(
                            "Delete all markers to change cell type");
                    }
                }
                
                // Show current dimensions (read-only info)
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                    "Cell Dimensions: %d×%d px",
                    model.grid.tileWidth, model.grid.tileHeight);
                
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                // Edge Configuration
                ImGui::Text("Edge/Wall Configuration");
                ImGui::Separator();
                ImGui::Spacing();
                
                ImGui::Checkbox("Auto-expand grid", 
                    &model.grid.autoExpandGrid);
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Automatically expand grid when placing edges "
                        "near boundaries");
                }
                
                ImGui::SetNextItemWidth(250.0f);
                ImGui::SliderInt("Expansion threshold (cells)", 
                                &model.grid.expansionThreshold, 1, 20);
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Distance from grid boundary to trigger expansion");
                }
                
                ImGui::SetNextItemWidth(250.0f);
                ImGui::SliderFloat("Expansion factor", 
                    &model.grid.expansionFactor, 
                    1.1f, 3.0f, "%.1fx");
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Grid growth multiplier "
                        "(e.g., 1.5x = 50%% growth)");
                }
                
                ImGui::SetNextItemWidth(250.0f);
                ImGui::SliderFloat("Edge hover threshold", 
                                  &model.grid.edgeHoverThreshold, 
                                  0.1f, 0.5f, "%.2f");
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(
                        "Distance from cell edge to activate edge mode "
                        "(0.2 = 20%% of cell size)");
                }
                
                ImGui::Spacing();
                
                ImGui::EndTabItem();
            }
            
            // ============================================================
            // TAB 3: KEYBINDINGS
            // ============================================================
            if (ImGui::BeginTabItem("Keybindings")) {
                settingsModalSelectedTab = 2;
                ImGui::Spacing();
                
                ImGui::Text("Keyboard Shortcuts");
                ImGui::SameLine();
                if (ImGui::SmallButton("Reset All to Defaults")) {
                    model.InitDefaultKeymap();
                    keymap.LoadBindings(model.keymap);
                    m_ui.ShowToast("Keybindings reset to defaults", 
                             Toast::Type::Success);
                }
                
                ImGui::Separator();
                ImGui::Spacing();
                
                if (ImGui::BeginTable("KeybindingsTable", 3, 
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | 
                    ImGuiTableFlags_ScrollY, 
                    ImVec2(0, 450))) {
                    
                    ImGui::TableSetupColumn("Action", 
                        ImGuiTableColumnFlags_WidthFixed, 180.0f);
                    ImGui::TableSetupColumn("Binding", 
                        ImGuiTableColumnFlags_WidthFixed, 160.0f);
                    ImGui::TableSetupColumn("Actions", 
                        ImGuiTableColumnFlags_WidthFixed, 150.0f);
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableHeadersRow();
                    
                    // Helper lambda to render a keybinding row
                    auto renderBinding = [&](const char* displayName, 
                                             const char* action) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", displayName);
                        
                        ImGui::TableNextColumn();
                        auto it = model.keymap.find(action);
                        std::string bindingStr = 
                            (it != model.keymap.end()) ? 
                            keymap.GetBindingDisplayName(it->second) : 
                            "(Not bound)";
                        ImGui::TextColored(
                            (it != model.keymap.end()) ? 
                                ImVec4(0.7f, 0.9f, 1.0f, 1.0f) : 
                                ImVec4(0.5f, 0.5f, 0.5f, 1.0f), 
                            "%s", bindingStr.c_str());
                        
                        ImGui::TableNextColumn();
                        ImGui::PushID(action);
                        if (ImGui::SmallButton("Rebind")) {
                            rebindAction = action;
                            rebindActionDisplayName = displayName;
                            capturedBinding = "";
                            isCapturing = false;
                            showRebindModal = true;
                        }
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Clear")) {
                            model.keymap[action] = "";
                            keymap.SetBinding(action, "");
                        }
                        ImGui::PopID();
                    };
                    
                    // TOOLS CATEGORY
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "TOOLS");
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    
                    renderBinding("Tool: Move", "toolMove");
                    renderBinding("Tool: Select", "toolSelect");
                    renderBinding("Tool: Paint", "toolPaint");
                    renderBinding("Tool: Erase", "toolErase");
                    renderBinding("Tool: Fill", "toolFill");
                    renderBinding("Tool: Marker", "toolMarker");
                    renderBinding("Tool: Eyedropper", "toolEyedropper");
                    renderBinding("Tool: Zoom", "toolZoom");
                    renderBinding("Tool: Room Select", "toolRoomSelect");
                    renderBinding("Tool: Room Paint", "toolRoomPaint");
                    renderBinding("Tool: Room Fill", "toolRoomFill");
                    renderBinding("Tool: Room Erase", "toolRoomErase");
                    
                    // VIEW CATEGORY
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "VIEW");
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    
                    renderBinding("Zoom In", "zoomIn");
                    renderBinding("Zoom Out", "zoomOut");
                    renderBinding("Toggle Grid", "toggleGrid");
                    renderBinding("Toggle Room Overlays", "toggleRoomOverlays");
                    renderBinding("Toggle Hierarchy Panel", 
                                 "togglePropertiesPanel");
                    
                    // EDIT CATEGORY
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "EDIT");
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    
                    renderBinding("Undo", "undo");
                    renderBinding("Redo", "redo");
                    renderBinding("Copy", "copy");
                    renderBinding("Paste", "paste");
                    renderBinding("Delete", "delete");
                    renderBinding("Delete (Alt)", "deleteAlt");
                    
                    // ACTIONS CATEGORY
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextColored(
                        ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "ACTIONS");
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    
                    renderBinding("Place Wall", "placeWall");
                    renderBinding("Place Door", "placeDoor");
                    renderBinding("Detect Rooms", "detectRooms");
                    
                    // FILE CATEGORY
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "FILE");
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    
                    renderBinding("New Project", "new");
                    renderBinding("Open Project", "open");
                    renderBinding("Save", "save");
                    renderBinding("Save As", "saveAs");
                    renderBinding("Export PNG", "export");
                    renderBinding("Export Package", "exportPackage");
                    
                    ImGui::EndTable();
                }
                
                ImGui::Spacing();
                
                ImGui::EndTabItem();
            }
            
            // ============================================================
            // TAB 4: APPEARANCE
            // ============================================================
            if (ImGui::BeginTabItem("Appearance")) {
                settingsModalSelectedTab = 3;
                ImGui::Spacing();
                ImGui::Spacing();
                
                ImGui::Text("Theme");
                ImGui::Separator();
                ImGui::Spacing();
                
                // Theme dropdown - use theme registry
                auto themes = GetAvailableThemes();
                int currentTheme = 0;
                for (size_t i = 0; i < themes.size(); ++i) {
                    if (model.theme.name == themes[i]) {
                        currentTheme = static_cast<int>(i);
                        break;
                    }
                }
                
                ImGui::Text("Color Theme:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(200);
                
                // Build combo items string
                std::string comboItems;
                for (const auto& t : themes) {
                    comboItems += t + '\0';
                }
                comboItems += '\0';
                
                if (ImGui::Combo("##ThemeCombo", &currentTheme, 
                                comboItems.c_str())) {
                    std::string newThemeName = themes[currentTheme];
                    if (newThemeName != model.theme.name) {
                        model.InitDefaultTheme(newThemeName);
                        app.ApplyTheme(model.theme);
                        // Save to global preferences (not project)
                        Preferences::themeName = newThemeName;
                        Preferences::Save();
                        m_ui.ShowToast(
                            "Theme changed to " + newThemeName,
                            Toast::Type::Success
                        );
                    }
                }
                
                // Show description for selected theme
                ImGui::Spacing();
                std::string desc = GetThemeDescription(model.theme.name);
                if (!desc.empty()) {
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                        "%s", desc.c_str());
                }
                
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                // UI Scale
                ImGui::Text("Display");
                ImGui::Separator();
                ImGui::Spacing();
                
                ImGui::Text("UI Scale:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(200);
                float uiScale = model.theme.uiScale;
                if (ImGui::SliderFloat("##UIScale", &uiScale, 0.8f, 1.5f, 
                                       "%.1fx")) {
                    model.theme.uiScale = uiScale;
                    app.ApplyTheme(model.theme);
                    // Save to global preferences (not project)
                    Preferences::uiScale = uiScale;
                    Preferences::Save();
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Reset")) {
                    model.theme.uiScale = 1.0f;
                    app.ApplyTheme(model.theme);
                    Preferences::uiScale = 1.0f;
                    Preferences::Save();
                }
                
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                    "Adjust UI element sizes for different display densities");
                
                ImGui::Spacing();
                ImGui::EndTabItem();
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
                !(currentPath.size() >= 5 && 
                  currentPath.substr(currentPath.size() - 5) == ".cart");
            
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
