#include "Modals.h"
#include "../App.h"
#include "../UI.h"
#include "../Canvas.h"
#include "../Jobs.h"
#include "../platform/Paths.h"
#include "../platform/System.h"
#include "../IOJson.h"
#include <imgui.h>
#include <filesystem>
#include <algorithm>
#include <SDL3/SDL.h>
#include <stb/stb_image.h>

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

namespace Cartograph {

Modals::Modals(UI& ui) : m_ui(ui) {
    // Initialize modal state
}

Modals::~Modals() {
    // Cleanup logo textures
    if (cartographLogoTexture != 0) {
        glDeleteTextures(1, &cartographLogoTexture);
        cartographLogoTexture = 0;
    }
    if (unveilLogoTexture != 0) {
        glDeleteTextures(1, &unveilLogoTexture);
        unveilLogoTexture = 0;
    }
}

void Modals::RenderAll(
    App& app,
    Model& model,
    Canvas& canvas,
    History& history,
    IconManager& icons,
    JobQueue& jobs,
    KeymapManager& keymap,
    std::string& selectedIconName,
    Marker*& selectedMarker,
    int& selectedTileId
) {
    if (showExportModal) RenderExportModal(model, canvas);
    if (showSettingsModal) RenderSettingsModal(model, keymap);
    if (showRenameIconModal) RenderRenameIconModal(model, icons, selectedIconName);
    if (showDeleteIconModal) RenderDeleteIconModal(model, icons, history, selectedIconName, selectedMarker);
    if (showRebindModal) RenderRebindModal(model, keymap);
    if (showColorPickerModal) RenderColorPickerModal(model, history, selectedTileId);
    if (showNewProjectModal) RenderNewProjectModal(app, model);
    if (showWhatsNew) RenderWhatsNewPanel();
    if (showAutosaveRecoveryModal) RenderAutosaveRecoveryModal(app, model);
    if (showLoadingModal) RenderLoadingModal(app, model, jobs, icons);
    if (showQuitConfirmationModal) RenderQuitConfirmationModal(app, model);
    if (showSaveBeforeActionModal) RenderSaveBeforeActionModal(app, model);
    if (showAboutModal) RenderAboutModal();
    if (showDeleteRoomDialog) RenderDeleteRoomModal(model);
    if (showRenameRoomDialog) RenderRenameRoomModal(model);
    if (showRenameRegionDialog) RenderRenameRegionModal(model);
    if (showDeleteRegionDialog) RenderDeleteRegionModal(model);
}

void Modals::RenderDeleteRoomModal(Model& model) {
    // Only call OpenPopup once when modal is first shown
    if (!deleteRoomDialogOpened) {
        ImGui::OpenPopup("Delete Room?");
        deleteRoomDialogOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Delete Room?", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
        Room* room = model.FindRoom(editingRoomId);
        if (room) {
            ImGui::Text("Delete room \"%s\"?", room->name.c_str());
            ImGui::Separator();
            ImGui::TextWrapped(
                "This will remove the room and clear all cell assignments."
            );
            ImGui::Spacing();
            
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                showDeleteRoomDialog = false;
                deleteRoomDialogOpened = false;
                editingRoomId.clear();
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::SameLine();
            
            ImGui::PushStyleColor(ImGuiCol_Button, 
                ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
            
            if (ImGui::Button("Delete", ImVec2(120, 0))) {
                // Clear all cell assignments for this room
                model.ClearAllCellsForRoom(editingRoomId);
                
                // Remove room from model
                auto it = std::find_if(
                    model.rooms.begin(),
                    model.rooms.end(),
                    [&](const Room& r) { return r.id == editingRoomId; }
                );
                if (it != model.rooms.end()) {
                    model.rooms.erase(it);
                }
                
                // Clear selection if we deleted the selected room
                if (m_ui.GetCanvasPanel().selectedRoomId == editingRoomId) {
                    m_ui.GetCanvasPanel().selectedRoomId.clear();
                }
                if (m_ui.GetCanvasPanel().activeRoomId == editingRoomId) {
                    m_ui.GetCanvasPanel().activeRoomId.clear();
                }
                
                model.MarkDirty();
                m_ui.AddConsoleMessage("Deleted room \"" + room->name + "\"",
                                     MessageType::Success);
                
                showDeleteRoomDialog = false;
                deleteRoomDialogOpened = false;
                editingRoomId.clear();
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::PopStyleColor(3);
        } else {
            ImGui::Text("Room not found");
            if (ImGui::Button("Close")) {
                showDeleteRoomDialog = false;
                deleteRoomDialogOpened = false;
                editingRoomId.clear();
                ImGui::CloseCurrentPopup();
            }
        }
        
        ImGui::EndPopup();
    }
}

void Modals::RenderRenameRoomModal(Model& model) {
    // Only call OpenPopup once when modal is first shown
    if (!renameRoomDialogOpened) {
        ImGui::OpenPopup("Rename Room");
        renameRoomDialogOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Rename Room", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
        Room* room = model.FindRoom(editingRoomId);
        if (room) {
            ImGui::Text("Rename room:");
            ImGui::Spacing();
            
            ImGui::SetNextItemWidth(300);
            bool enterPressed = ImGui::InputText("##rename", renameBuffer, 
                                                sizeof(renameBuffer), 
                                                ImGuiInputTextFlags_EnterReturnsTrue);
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                showRenameRoomDialog = false;
                renameRoomDialogOpened = false;
                editingRoomId.clear();
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::SameLine();
            
            if (ImGui::Button("Rename", ImVec2(120, 0)) || enterPressed) {
                std::string newName = renameBuffer;
                if (!newName.empty()) {
                    room->name = newName;
                    model.MarkDirty();
                    m_ui.AddConsoleMessage("Renamed room to \"" + newName + "\"",
                                         MessageType::Success);
                }
                
                showRenameRoomDialog = false;
                renameRoomDialogOpened = false;
                editingRoomId.clear();
                ImGui::CloseCurrentPopup();
            }
        } else {
            ImGui::Text("Room not found");
            if (ImGui::Button("Close")) {
                showRenameRoomDialog = false;
                renameRoomDialogOpened = false;
                editingRoomId.clear();
                ImGui::CloseCurrentPopup();
            }
        }
        
        ImGui::EndPopup();
    }
}

void Modals::RenderRenameRegionModal(Model& model) {
    // Only call OpenPopup once when modal is first shown
    if (!renameRegionDialogOpened) {
        ImGui::OpenPopup("Rename Region");
        renameRegionDialogOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Rename Region", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
        RegionGroup* region = model.FindRegionGroup(editingRegionId);
        if (region) {
            ImGui::Text("Rename region:");
            ImGui::Spacing();
            
            ImGui::SetNextItemWidth(300);
            bool enterPressed = ImGui::InputText("##rename", renameBuffer, 
                                                sizeof(renameBuffer), 
                                                ImGuiInputTextFlags_EnterReturnsTrue);
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                showRenameRegionDialog = false;
                renameRegionDialogOpened = false;
                editingRegionId.clear();
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::SameLine();
            
            if (ImGui::Button("Rename", ImVec2(120, 0)) || enterPressed) {
                std::string newName = renameBuffer;
                if (!newName.empty()) {
                    region->name = newName;
                    model.MarkDirty();
                    m_ui.AddConsoleMessage("Renamed region to \"" + newName + "\"",
                                         MessageType::Success);
                }
                
                showRenameRegionDialog = false;
                renameRegionDialogOpened = false;
                editingRegionId.clear();
                ImGui::CloseCurrentPopup();
            }
        } else {
            ImGui::Text("Region not found");
            if (ImGui::Button("Close")) {
                showRenameRegionDialog = false;
                renameRegionDialogOpened = false;
                editingRegionId.clear();
                ImGui::CloseCurrentPopup();
            }
        }
        
        ImGui::EndPopup();
    }
}

void Modals::RenderDeleteRegionModal(Model& model) {
    // Only call OpenPopup once when modal is first shown
    if (!deleteRegionDialogOpened) {
        ImGui::OpenPopup("Delete Region?");
        deleteRegionDialogOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Delete Region?", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
        RegionGroup* region = model.FindRegionGroup(editingRegionId);
        if (region) {
            ImGui::Text("Delete region \"%s\"?", region->name.c_str());
            ImGui::Separator();
            ImGui::TextWrapped(
                "Rooms in this region will become unassigned."
            );
            ImGui::Spacing();
            
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                showDeleteRegionDialog = false;
                deleteRegionDialogOpened = false;
                editingRegionId.clear();
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::SameLine();
            
            ImGui::PushStyleColor(ImGuiCol_Button, 
                ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
            
            if (ImGui::Button("Delete", ImVec2(120, 0))) {
                // Unassign all rooms from this region
                for (auto& room : model.rooms) {
                    if (room.parentRegionGroupId == editingRegionId) {
                        room.parentRegionGroupId = "";
                    }
                }
                
                // Remove region from model
                auto it = std::find_if(
                    model.regionGroups.begin(),
                    model.regionGroups.end(),
                    [&](const RegionGroup& r) { return r.id == editingRegionId; }
                );
                if (it != model.regionGroups.end()) {
                    model.regionGroups.erase(it);
                }
                
                // Clear selection if we deleted the selected region
                if (m_ui.GetCanvasPanel().selectedRegionGroupId == editingRegionId) {
                    m_ui.GetCanvasPanel().selectedRegionGroupId.clear();
                }
                
                model.MarkDirty();
                m_ui.AddConsoleMessage("Deleted region \"" + region->name + "\"",
                                     MessageType::Success);
                
                showDeleteRegionDialog = false;
                deleteRegionDialogOpened = false;
                editingRegionId.clear();
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::PopStyleColor(3);
        } else {
            ImGui::Text("Region not found");
            if (ImGui::Button("Close")) {
                showDeleteRegionDialog = false;
                deleteRegionDialogOpened = false;
                editingRegionId.clear();
                ImGui::CloseCurrentPopup();
            }
        }
        
        ImGui::EndPopup();
    }
}

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


void Modals::RenderSettingsModal(Model& model, KeymapManager& keymap) {
    // Only call OpenPopup once when modal is first shown
    if (!settingsModalOpened) {
        ImGui::OpenPopup("Settings");
        settingsModalOpened = true;
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
                                      ImGuiInputTextFlags_AllowTabInput)) {
            model.meta.description = descBuf;
            model.MarkDirty();
        }
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                    "Brief description of your project (optional)");
        
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
            ImGui::SetTooltip("Square cells for top-down games. "
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
            ImGui::SetTooltip("Rectangular cells for side-scrollers. "
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
                ImGui::SetTooltip("Delete all markers to change cell type");
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
            ImGui::SetTooltip("Automatically expand grid when placing edges "
                            "near boundaries");
        }
        
        ImGui::SetNextItemWidth(250.0f);
        ImGui::SliderInt("Expansion threshold (cells)", 
                        &model.grid.expansionThreshold, 1, 20);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Distance from grid boundary to trigger "
                            "expansion");
        }
        
        ImGui::SetNextItemWidth(250.0f);
                ImGui::SliderFloat("Expansion factor", 
                    &model.grid.expansionFactor, 
                          1.1f, 3.0f, "%.1fx");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Grid growth multiplier "
                            "(e.g., 1.5x = 50%% growth)");
        }
        
        ImGui::SetNextItemWidth(250.0f);
        ImGui::SliderFloat("Edge hover threshold", 
                          &model.grid.edgeHoverThreshold, 
                          0.1f, 0.5f, "%.2f");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Distance from cell edge to activate edge mode "
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
                std::string bindingStr = (it != model.keymap.end()) ? 
                    keymap.GetBindingDisplayName(it->second) : "(Not bound)";
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
            renderBinding("Tool: Eyedropper", "toolEyedropper");
            
            // VIEW CATEGORY
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "VIEW");
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            
            renderBinding("Zoom In", "zoomIn");
            renderBinding("Zoom Out", "zoomOut");
            renderBinding("Toggle Grid", "toggleGrid");
            renderBinding("Toggle Hierarchy Panel", "togglePropertiesPanel");
            
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
            
            ImGui::EndTabBar();
        }
        
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Buttons (outside tab content, always visible)
        if (ImGui::Button("Apply", ImVec2(120, 0))) {
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


void Modals::RenderRenameIconModal(Model& model, IconManager& icons, 
                                   std::string& selectedIconName) {
    // Only call OpenPopup once when modal is first shown
    if (!renameIconModalOpened) {
        ImGui::OpenPopup("Rename Icon");
        renameIconModalOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
        ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Rename Icon", nullptr, 
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::Text("Rename Icon");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::Text("Current name: %s", renameIconOldName.c_str());
        ImGui::Spacing();
        
        // New name input
        ImGui::InputText("New name", renameIconNewName, 
                        sizeof(renameIconNewName));
        
        ImGui::Spacing();
        ImGui::Separator();
        
        if (ImGui::Button("Rename", ImVec2(120, 0))) {
            std::string newName(renameIconNewName);
            std::string errorMsg;
            
            // Validate and rename
            if (icons.RenameIcon(renameIconOldName, newName, errorMsg)) {
                // Update all markers using this icon
                int count = model.UpdateMarkerIconNames(
                    renameIconOldName, newName);
                
                // Update selected icon if it was the renamed one
                if (selectedIconName == renameIconOldName) {
                    selectedIconName = newName;
                }
                
                showRenameIconModal = false;
                renameIconModalOpened = false;
                ImGui::CloseCurrentPopup();
                
                std::string msg = "Icon renamed";
                if (count > 0) {
                    msg += " (" + std::to_string(count) + " marker";
                    if (count > 1) msg += "s";
                    msg += " updated)";
                }
                m_ui.ShowToast(msg, Toast::Type::Success);
            } else {
                // Show error
                m_ui.ShowToast("Rename failed: " + errorMsg, 
                         Toast::Type::Error, 3.0f);
            }
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showRenameIconModal = false;
            renameIconModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}


void Modals::RenderDeleteIconModal(Model& model, IconManager& icons, 
                                   History& history, std::string& selectedIconName,
                                   Marker*& selectedMarker) {
    // Only call OpenPopup once when modal is first shown
    if (!deleteIconModalOpened) {
        ImGui::OpenPopup("Delete Icon");
        deleteIconModalOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Appearing);
    
    if (ImGui::BeginPopupModal("Delete Icon", nullptr, 
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::Text("Delete Icon");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::Text("Icon: %s", deleteIconName.c_str());
        ImGui::Spacing();
        
        // Show warning if markers are using this icon
        if (deleteIconMarkerCount > 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, 
                ImVec4(1.0f, 0.7f, 0.2f, 1.0f));  // Orange warning
            ImGui::TextWrapped(
                "Warning: This icon is used by %d marker%s.", 
                deleteIconMarkerCount,
                deleteIconMarkerCount == 1 ? "" : "s"
            );
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            
            // Show affected markers (limit to first 10)
            ImGui::TextWrapped("Affected markers:");
            ImGui::Indent();
            
            int displayCount = std::min(
                static_cast<int>(deleteIconAffectedMarkers.size()), 10
            );
            for (int i = 0; i < displayCount; ++i) {
                const std::string& markerId = deleteIconAffectedMarkers[i];
                const Marker* marker = model.FindMarker(markerId);
                if (marker) {
                    ImGui::BulletText("%s at (%.1f, %.1f)", 
                        marker->label.empty() ? 
                            marker->id.c_str() : marker->label.c_str(),
                        marker->x, marker->y
                    );
                }
            }
            
            if (deleteIconAffectedMarkers.size() > 10) {
                ImGui::BulletText("... and %zu more", 
                    deleteIconAffectedMarkers.size() - 10);
            }
            
            ImGui::Unindent();
            ImGui::Spacing();
            
            ImGui::PushStyleColor(ImGuiCol_Text, 
                ImVec4(1.0f, 0.5f, 0.5f, 1.0f));  // Red warning
            ImGui::TextWrapped(
                "Deleting this icon will also remove all markers using it."
            );
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
        } else {
            // No markers using this icon - safe to delete
            ImGui::TextWrapped(
                "Are you sure you want to delete this icon?"
            );
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), 
                "No markers are using this icon.");
            ImGui::Spacing();
        }
        
        // Remind user that deletion is undoable
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.8f, 1.0f), 
            "Tip: You can undo this with Cmd+Z");
        ImGui::Spacing();
        
        ImGui::Separator();
        
        // Delete button (destructive action, so place on left)
        bool confirmDelete = false;
        if (deleteIconMarkerCount > 0) {
            // Destructive action - use warning color
            ImGui::PushStyleColor(ImGuiCol_Button, 
                ImVec4(0.8f, 0.3f, 0.2f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                ImVec4(1.0f, 0.4f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                ImVec4(0.9f, 0.2f, 0.1f, 1.0f));
            
            if (ImGui::Button("Delete Icon & Markers", ImVec2(200, 0))) {
                confirmDelete = true;
            }
            
            ImGui::PopStyleColor(3);
        } else {
            // Safe deletion
            if (ImGui::Button("Delete Icon", ImVec2(120, 0))) {
                confirmDelete = true;
            }
        }
        
        if (confirmDelete) {
            // Create delete command with icon manager reference
            auto cmd = std::make_unique<DeleteIconCommand>(
                deleteIconName, 
                deleteIconMarkerCount > 0,  // Remove markers if any exist
                icons                       // Icon manager for undo restoration
            );
            
            // Capture icon state before deletion
            cmd->CaptureIconState();
            
            // Delete icon from IconManager
            std::string errorMsg;
            if (icons.DeleteIcon(deleteIconName, errorMsg)) {
                // Rebuild atlas
                icons.BuildAtlas();
                
                // Execute command (handles marker deletion)
                history.AddCommand(std::move(cmd), model, true);
                
                // Update selected icon if it was the deleted one
                if (selectedIconName == deleteIconName) {
                    // Try to select another icon if available
                    auto remainingIcons = 
                        icons.GetIconNamesByCategory("marker");
                    if (!remainingIcons.empty()) {
                        selectedIconName = remainingIcons[0];
                    } else {
                        selectedIconName = "";
                    }
                }
                
                // Deselect marker if it was using this icon
                if (selectedMarker && selectedMarker->icon == deleteIconName) {
                    selectedMarker = nullptr;
                }
                
                showDeleteIconModal = false;
                deleteIconModalOpened = false;
                ImGui::CloseCurrentPopup();
                
                // Show success message
                std::string msg = "Icon deleted";
                if (deleteIconMarkerCount > 0) {
                    msg += " (" + std::to_string(deleteIconMarkerCount) + 
                           " marker";
                    if (deleteIconMarkerCount > 1) msg += "s";
                    msg += " removed)";
                }
                m_ui.ShowToast(msg, Toast::Type::Success);
            } else {
                // Show error
                m_ui.ShowToast("Delete failed: " + errorMsg, 
                         Toast::Type::Error, 3.0f);
            }
        }
        
        ImGui::SameLine();
        
        // Cancel button (safe action, default)
        if (ImGui::Button("Cancel", ImVec2(120, 0)) || 
            ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            showDeleteIconModal = false;
            deleteIconModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}


void Modals::RenderRebindModal(Model& model, KeymapManager& keymap) {
    // Only call OpenPopup once when modal is first shown
    if (!rebindModalOpened) {
        ImGui::OpenPopup("Rebind Key");
        rebindModalOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_Appearing);
    
    if (ImGui::BeginPopupModal("Rebind Key", nullptr, 
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::Text("Rebind: %s", rebindActionDisplayName.c_str());
        ImGui::Separator();
        ImGui::Spacing();
        
        if (!isCapturing) {
            ImGui::TextWrapped(
                "Press any key combination to bind it to this action.");
            ImGui::Spacing();
            
            if (ImGui::Button("Start Capturing", ImVec2(-1, 0))) {
                isCapturing = true;
                capturedBinding = "";
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
                "Press a key combination...");
            ImGui::Spacing();
            
            // Capture key press
            ImGuiIO& io = ImGui::GetIO();
            
            // Build binding string based on current input
            std::string binding = "";
            bool hasModifier = false;
            
            if (io.KeyCtrl) {
                binding += "Ctrl+";
                hasModifier = true;
            }
            if (io.KeyAlt) {
                binding += "Alt+";
                hasModifier = true;
            }
            if (io.KeyShift) {
                binding += "Shift+";
                hasModifier = true;
            }
            if (io.KeySuper) {
#ifdef __APPLE__
                binding += "Cmd+";
#else
                binding += "Super+";
#endif
                hasModifier = true;
            }
            
            // Check for key press (A-Z, 0-9, special keys)
            for (int key = ImGuiKey_A; key <= ImGuiKey_Z; key++) {
                if (ImGui::IsKeyPressed((ImGuiKey)key, false)) {
                    char c = 'A' + (key - ImGuiKey_A);
                    binding += std::string(1, c);
                    capturedBinding = binding;
                    break;
                }
            }
            for (int key = ImGuiKey_0; key <= ImGuiKey_9; key++) {
                if (ImGui::IsKeyPressed((ImGuiKey)key, false)) {
                    char c = '0' + (key - ImGuiKey_0);
                    binding += std::string(1, c);
                    capturedBinding = binding;
                    break;
                }
            }
            // Function keys
            const ImGuiKey functionKeys[] = {
                ImGuiKey_F1, ImGuiKey_F2, ImGuiKey_F3, ImGuiKey_F4,
                ImGuiKey_F5, ImGuiKey_F6, ImGuiKey_F7, ImGuiKey_F8,
                ImGuiKey_F9, ImGuiKey_F10, ImGuiKey_F11, ImGuiKey_F12
            };
            const char* functionKeyNames[] = {
                "F1", "F2", "F3", "F4", "F5", "F6", 
                "F7", "F8", "F9", "F10", "F11", "F12"
            };
            for (int i = 0; i < 12; i++) {
                if (ImGui::IsKeyPressed(functionKeys[i], false)) {
                    binding += functionKeyNames[i];
                    capturedBinding = binding;
                    break;
                }
            }
            // Special keys
            if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
                binding += "Space";
                capturedBinding = binding;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
                binding += "Enter";
                capturedBinding = binding;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                // Cancel on Escape
                isCapturing = false;
                capturedBinding = "";
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
                binding += "Delete";
                capturedBinding = binding;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false)) {
                binding += "Backspace";
                capturedBinding = binding;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Equal, false)) {
                binding += "=";
                capturedBinding = binding;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Minus, false)) {
                binding += "-";
                capturedBinding = binding;
            }
            
            // Show what's being pressed
            if (!capturedBinding.empty()) {
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f),
                    "Captured: %s", capturedBinding.c_str());
            }
            
            ImGui::Spacing();
            if (ImGui::Button("Cancel Capture", ImVec2(-1, 0))) {
                isCapturing = false;
                capturedBinding = "";
            }
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Show current binding
        auto it = model.keymap.find(rebindAction);
        if (it != model.keymap.end() && !it->second.empty()) {
            ImGui::Text("Current: %s", 
                keymap.GetBindingDisplayName(it->second).c_str());
        } else {
            ImGui::TextDisabled("Current: (Not bound)");
        }
        
        ImGui::Spacing();
        
        // Check for conflicts
        std::string conflictAction = "";
        bool hasConflict = false;
        if (!capturedBinding.empty()) {
            conflictAction = keymap.FindConflict(capturedBinding, rebindAction);
            hasConflict = !conflictAction.empty();
        }
        
        // Show conflict warning
        if (hasConflict) {
            ImGui::PushStyleColor(ImGuiCol_Text, 
                ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
            ImGui::TextWrapped(
                "Warning: '%s' is already bound to '%s'", 
                capturedBinding.c_str(), 
                conflictAction.c_str());
            ImGui::PopStyleColor();
            
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                "Applying will remove the existing binding.");
            ImGui::Spacing();
        }
        
        // Action buttons
        bool canApply = !capturedBinding.empty() && 
                       keymap.IsBindingValid(capturedBinding);
        
        if (!canApply) {
            ImGui::BeginDisabled();
        }
        
        std::string applyLabel = hasConflict ? "Apply (Override)" : "Apply";
        if (ImGui::Button(applyLabel.c_str(), ImVec2(140, 0))) {
            // If there's a conflict, clear the conflicting binding
            if (hasConflict) {
                model.keymap[conflictAction] = "";
                keymap.SetBinding(conflictAction, "");
            }
            
            // Apply the new binding
            model.keymap[rebindAction] = capturedBinding;
            keymap.SetBinding(rebindAction, capturedBinding);
            showRebindModal = false;
            rebindModalOpened = false;
            ImGui::CloseCurrentPopup();
            
            if (hasConflict) {
                m_ui.ShowToast("Keybinding updated (conflict resolved)", 
                         Toast::Type::Warning);
            } else {
                m_ui.ShowToast("Keybinding updated", Toast::Type::Success);
            }
        }
        if (!canApply) {
            ImGui::EndDisabled();
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showRebindModal = false;
            rebindModalOpened = false;
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
        if (colorPickerEditingTileId == -1 && model.palette.size() >= 32) {
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


void Modals::RenderNewProjectModal(App& app, Model& model) {
    // Only call OpenPopup once when modal is first shown
    if (!newProjectModalOpened) {
        ImGui::OpenPopup("New Project");
        newProjectModalOpened = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
        ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("New Project", nullptr, 
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        // Project name
        ImGui::Text("Project Name:");
        if (ImGui::InputText("##projectname", newProjectConfig.projectName, 
            sizeof(newProjectConfig.projectName))) {
            // Update path when project name changes
            UpdateNewProjectPath();
        }
        
        ImGui::Spacing();
        
        // Save location
        ImGui::Text("Save Location:");
        
        // Display path in a framed box (styled like input field)
        ImGui::PushStyleColor(ImGuiCol_ChildBg, 
                             ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::BeginChild("##savelocationdisplay", 
                         ImVec2(450, 30), 
                         true, 
                         ImGuiWindowFlags_NoScrollbar);
        
        ImGui::PushStyleColor(ImGuiCol_Text, 
                             ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::TextWrapped("%s", newProjectConfig.fullSavePath.c_str());
        ImGui::PopStyleColor();
        
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        
        if (ImGui::Button("Choose Different Location...", 
                         ImVec2(240, 0))) {
            ShowNewProjectFolderPicker();
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Map Style Selection
        ImGui::Text("Choose your map style:");
        ImGui::Spacing();
        
        // Grid preset cards - side by side
        ImGui::BeginGroup();
        
        // Square preset card
        bool isSquareSelected = (newProjectConfig.gridPreset == GridPreset::Square);
        if (isSquareSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.9f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3.0f);
        }
        
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
        if (ImGui::Button("Square\n16×16 px", 
                         ImVec2(120, 50))) {
            newProjectConfig.gridPreset = GridPreset::Square;
        }
        ImGui::PopStyleVar();
        
        if (isSquareSelected) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        }
        
        ImGui::SameLine(0, 20);
        
        // Rectangle preset card
        bool isRectangleSelected = (newProjectConfig.gridPreset == GridPreset::Rectangle);
        if (isRectangleSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.9f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 3.0f);
        }
        
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
        if (ImGui::Button("Rectangle\n32×16 px", 
                         ImVec2(120, 50))) {
            newProjectConfig.gridPreset = GridPreset::Rectangle;
        }
        ImGui::PopStyleVar();
        
        if (isRectangleSelected) {
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(2);
        }
        
        ImGui::EndGroup();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Map dimensions with validation
        ImGui::PushItemWidth(150.0f);
        ImGui::InputInt("Map Width (cells)", &newProjectConfig.mapWidth);
        if (newProjectConfig.mapWidth < 16) newProjectConfig.mapWidth = 16;
        if (newProjectConfig.mapWidth > 1024) 
            newProjectConfig.mapWidth = 1024;
        
        ImGui::InputInt("Map Height (cells)", &newProjectConfig.mapHeight);
        if (newProjectConfig.mapHeight < 16) 
            newProjectConfig.mapHeight = 16;
        if (newProjectConfig.mapHeight > 1024) 
            newProjectConfig.mapHeight = 1024;
        ImGui::PopItemWidth();
        
        ImGui::Spacing();
        
        // Preview info
        int totalCells = newProjectConfig.mapWidth * 
            newProjectConfig.mapHeight;
        
        // Get cell dimensions from preset
        int cellWidth = (newProjectConfig.gridPreset == GridPreset::Square) ? 16 : 32;
        int cellHeight = 16;
        
        int pixelWidth = newProjectConfig.mapWidth * cellWidth;
        int pixelHeight = newProjectConfig.mapHeight * cellHeight;
        
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "Total cells: %d | Canvas size: %dx%d px",
            totalCells, pixelWidth, pixelHeight);
        
        ImGui::Spacing();
        
        // Buttons
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            // Validate path
            if (newProjectConfig.fullSavePath.empty()) {
                m_ui.ShowToast("Please select a save location", 
                         Toast::Type::Error);
            } else {
                // Apply configuration to model
                model.meta.title = std::string(newProjectConfig.projectName);
                
                // Apply grid preset (sets tileWidth/tileHeight automatically)
                model.ApplyGridPreset(newProjectConfig.gridPreset);
                model.grid.cols = newProjectConfig.mapWidth;
                model.grid.rows = newProjectConfig.mapHeight;
                model.grid.locked = false;  // New project starts unlocked
                
                // Initialize other defaults
                model.InitDefaultPalette();
                model.InitDefaultKeymap();
                model.InitDefaultTheme("Dark");
                
                showNewProjectModal = false;
                newProjectModalOpened = false;
                ImGui::CloseCurrentPopup();
                
                // Create project with specified path
                app.NewProject(newProjectConfig.fullSavePath);
                
                // Transition to editor
                app.ShowEditor();
            }
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showNewProjectModal = false;
            newProjectModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}


void Modals::RenderProjectBrowserModal(App& app, 
                                       std::vector<RecentProject>& recentProjects) {
    // Only call OpenPopup once when modal is first shown
    if (!projectBrowserModalOpened) {
        ImGui::OpenPopup("Recent Projects");
        projectBrowserModalOpened = true;
    }
    
    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Recent Projects", &showProjectBrowserModal, 
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove)) {
        
        ImGui::Text("All Recent Projects");
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
            "Sorted by last modified");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Search bar
        static char searchFilter[256] = "";
        ImGui::SetNextItemWidth(-1.0f);  // Full width
        if (ImGui::InputTextWithHint("##projectsearch", 
                                     "🔍 Search projects...", 
                                     searchFilter, 
                                     sizeof(searchFilter))) {
            // Filter updates on every keystroke
        }
        
        // Show count of filtered results
        if (searchFilter[0] != '\0') {
            int visibleCount = 0;
            for (const auto& project : recentProjects) {
                std::string lowerName = project.name;
                std::string lowerFilter = searchFilter;
                
                // Convert to lowercase for case-insensitive search
                std::transform(lowerName.begin(), lowerName.end(), 
                              lowerName.begin(), ::tolower);
                std::transform(lowerFilter.begin(), lowerFilter.end(), 
                              lowerFilter.begin(), ::tolower);
                
                if (lowerName.find(lowerFilter) != std::string::npos) {
                    visibleCount++;
                }
            }
            
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "Showing %d of %zu projects", visibleCount, 
                recentProjects.size());
        }
        
        ImGui::Spacing();
        
        // Scrollable content area
        ImGui::BeginChild("ProjectList", ImVec2(0, -40), true);
        
        // Card dimensions for modal view (smaller than welcome screen)
        const float cardWidth = 250.0f;
        const float cardHeight = 180.0f;
        const float thumbnailHeight = 141.0f;  // 250*9/16
        const float titleHeight = 25.0f;
        const float cardSpacing = 15.0f;
        const int cardsPerRow = 3;
        
        // Load all thumbnails
        for (auto& project : recentProjects) {
            m_ui.m_welcomeScreen.LoadThumbnailTexture(project);
        }
        
        // Render projects in a grid (with search filtering)
        int visibleIndex = 0;
        for (size_t i = 0; i < recentProjects.size(); ++i) {
            auto& project = recentProjects[i];
            
            // Apply search filter
            if (searchFilter[0] != '\0') {
                std::string lowerName = project.name;
                std::string lowerFilter = searchFilter;
                
                // Convert to lowercase for case-insensitive search
                std::transform(lowerName.begin(), lowerName.end(), 
                              lowerName.begin(), ::tolower);
                std::transform(lowerFilter.begin(), lowerFilter.end(), 
                              lowerFilter.begin(), ::tolower);
                
                // Skip if doesn't match search
                if (lowerName.find(lowerFilter) == std::string::npos) {
                    continue;
                }
            }
            
            ImGui::PushID(static_cast<int>(i));
            ImGui::BeginGroup();
            
            // Get current cursor position for the card
            ImVec2 cardPos = ImGui::GetCursorScreenPos();
            
            // Thumbnail image
            if (project.thumbnailTextureId != 0) {
                ImVec2 thumbSize(cardWidth, thumbnailHeight);
                
                // Make thumbnail clickable
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                                     ImVec4(0.2f, 0.2f, 0.2f, 0.3f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                                     ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
                
                if (ImGui::ImageButton(
                        ("##thumb" + std::to_string(i)).c_str(),
                        (ImTextureID)(intptr_t)project.thumbnailTextureId,
                        thumbSize)) {
                    showProjectBrowserModal = false;
                    app.OpenProject(project.path);
                    app.ShowEditor();
                }
                
                ImGui::PopStyleColor(3);
                
                // Tooltip on hover
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", project.path.c_str());
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                        "Last modified: %s", project.lastModified.c_str());
                    ImGui::EndTooltip();
                }
                
                // Draw title overlay at bottom-left of thumbnail
                ImVec2 overlayMin(cardPos.x, cardPos.y + thumbnailHeight - 
                                 titleHeight);
                ImVec2 overlayMax(cardPos.x + cardWidth, 
                                 cardPos.y + thumbnailHeight);
                
                // Semi-transparent dark background for title
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(overlayMin, overlayMax, 
                    IM_COL32(0, 0, 0, 180));
                
                // Title text - use ImGui widget for proper font handling
                ImVec2 textPos(cardPos.x + 8, cardPos.y + thumbnailHeight - 
                              titleHeight + 4);
                ImGui::SetCursorScreenPos(textPos);
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 
                                  "%s", project.name.c_str());
            }
            
            ImGui::EndGroup();
            ImGui::PopID();
            
            // Grid layout: add spacing or new line
            // Use visibleIndex for grid layout (not original index i)
            visibleIndex++;
            if (visibleIndex % cardsPerRow != 0) {
                ImGui::SameLine(0.0f, cardSpacing);
            } else {
                ImGui::Spacing();
            }
        }
        
        // Show "no results" message if search yielded nothing
        if (searchFilter[0] != '\0' && visibleIndex == 0) {
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                "No projects found matching \"%s\"", searchFilter);
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                "Try a different search term");
        }
        
        ImGui::EndChild();
        
        // Close button at bottom
        ImGui::Spacing();
        float closeButtonWidth = 120.0f;
        float closeButtonX = (ImGui::GetWindowWidth() - closeButtonWidth) * 0.5f;
        ImGui::SetCursorPosX(closeButtonX);
        
        if (ImGui::Button("Close", ImVec2(closeButtonWidth, 0))) {
            showProjectBrowserModal = false;
            projectBrowserModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    // Reset flag if modal was closed (via X button)
    if (!showProjectBrowserModal) {
        projectBrowserModalOpened = false;
    }
}


void Modals::RenderWhatsNewPanel() {
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("What's New in Cartograph", &showWhatsNew)) {
        ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), 
            "Version 0.1.0 - Initial Release");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::BulletText("Welcome screen with project templates");
        ImGui::BulletText("Pan/zoom canvas with grid");
        ImGui::BulletText("Room and tile painting tools");
        ImGui::BulletText("Door and marker placement");
        ImGui::BulletText("PNG export with layers");
        ImGui::BulletText("Undo/redo support");
        ImGui::BulletText("Autosave functionality");
        ImGui::BulletText("Theme support (Dark/Light)");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
            "Coming Soon:");
        ImGui::BulletText("Reachability analysis");
        ImGui::BulletText("Minimap panel");
        ImGui::BulletText("Legend generation");
        ImGui::BulletText("SVG icon support");
        ImGui::BulletText("Web build");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            showWhatsNew = false;
        }
    }
    ImGui::End();
}


void Modals::RenderAutosaveRecoveryModal(App& app, Model& model) {
    // Only call OpenPopup once when modal is first shown
    if (!autosaveRecoveryModalOpened) {
        ImGui::OpenPopup("Autosave Recovery");
        autosaveRecoveryModalOpened = true;
    }
    
    ImGui::SetNextWindowSize(ImVec2(480, 200), ImGuiCond_Always);
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Autosave Recovery", nullptr, 
                               ImGuiWindowFlags_AlwaysAutoResize | 
                               ImGuiWindowFlags_NoMove)) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), 
                          "Unsaved Work Detected");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextWrapped(
            "Cartograph detected unsaved work from a previous session. "
            "Would you like to recover it?");
        ImGui::Spacing();
        ImGui::TextDisabled(
            "Note: Recovering will load the autosaved data. You can "
            "manually save it when ready.");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        float buttonWidth = 120.0f;
        
        if (ImGui::Button("Recover", ImVec2(buttonWidth, 0))) {
            // Load autosave
            std::string autosavePath = Platform::GetAutosaveDir() + "autosave.json";
            Model recoveredModel;
            if (IOJson::LoadFromFile(autosavePath, recoveredModel)) {
                model = recoveredModel;
                model.MarkDirty();  // Mark as dirty so user must save
                m_ui.ShowToast("Recovered from autosave", Toast::Type::Success);
                app.ShowEditor();
            } else {
                m_ui.ShowToast("Failed to load autosave", Toast::Type::Error);
            }
            showAutosaveRecoveryModal = false;
            autosaveRecoveryModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine(0, 10);
        
        if (ImGui::Button("Discard", ImVec2(buttonWidth, 0))) {
            // Clean up autosave and continue
            std::string autosaveDir = Platform::GetAutosaveDir();
            try {
                std::filesystem::remove(autosaveDir + "autosave.json");
                std::filesystem::remove(autosaveDir + "metadata.json");
            } catch (...) {
                // Ignore errors
            }
            showAutosaveRecoveryModal = false;
            autosaveRecoveryModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}


void Modals::RenderLoadingModal(App& app, Model& model, JobQueue& jobs, 
                            IconManager& icons) {
    if (!showLoadingModal) {
        loadingModalOpened = false;
        return;
    }
    
    // Only call OpenPopup once when modal is first shown
    if (!loadingModalOpened) {
        ImGui::OpenPopup("Loading Project");
        loadingModalOpened = true;
    }
    
    ImGui::SetNextWindowSize(ImVec2(400, 160), ImGuiCond_Always);
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Loading Project", nullptr,
                               ImGuiWindowFlags_NoResize |
                               ImGuiWindowFlags_NoMove |
                               ImGuiWindowFlags_NoCollapse)) {
        ImGui::Spacing();
        
        // Title
        ImGui::TextColored(
            ImVec4(0.4f, 0.7f, 1.0f, 1.0f),
            "Opening Project"
        );
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // File name (truncated if too long)
        std::string displayName = loadingFileName;
        if (displayName.length() > 45) {
            displayName = "..." + displayName.substr(
                displayName.length() - 42
            );
        }
        ImGui::Text("%s", displayName.c_str());
        
        ImGui::Spacing();
        
        // Indeterminate progress bar (pulsing animation)
        float time = static_cast<float>(ImGui::GetTime());
        float progress = (sinf(time * 3.0f) + 1.0f) * 0.5f;  // 0.0-1.0 pulse
        
        ImGui::ProgressBar(
            progress,
            ImVec2(-1, 0),
            ""  // No text overlay
        );
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        // Cancel button
        float buttonWidth = 120.0f;
        ImGui::SetCursorPosX(
            (ImGui::GetWindowWidth() - buttonWidth) * 0.5f
        );
        
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            loadingCancelled = true;
            loadingModalOpened = false;
        }
        
        ImGui::EndPopup();
    }
}


void Modals::RenderQuitConfirmationModal(App& app, Model& model) {
    // Only call OpenPopup once when modal is first shown
    if (!quitConfirmationModalOpened) {
        ImGui::OpenPopup("Unsaved Changes");
        quitConfirmationModalOpened = true;
    }
    
    ImGui::SetNextWindowSize(ImVec2(450, 180), ImGuiCond_Always);
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, 
                               ImGuiWindowFlags_AlwaysAutoResize | 
                               ImGuiWindowFlags_NoMove)) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), 
                          "Warning: Unsaved Changes");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextWrapped(
            "You have unsaved changes. Do you want to save your work "
            "before quitting?");
        ImGui::Spacing();
        
        if (app.GetState() != AppState::Editor) {
            ImGui::TextDisabled("Current project has not been saved.");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        float buttonWidth = 120.0f;
        
        // Cancel button (leftmost, secondary action)
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            showQuitConfirmationModal = false;
            quitConfirmationModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine(0, 10);
        
        // Don't Save button
        if (ImGui::Button("Don't Save", ImVec2(buttonWidth, 0))) {
            // Quit without saving
            showQuitConfirmationModal = false;
            quitConfirmationModalOpened = false;
            ImGui::CloseCurrentPopup();
            app.ForceQuit();
        }
        
        ImGui::SameLine(0, 10);
        
        // Save button (rightmost, primary action)
        ImGui::PushStyleColor(ImGuiCol_Button, 
                             ImVec4(0.2f, 0.6f, 0.9f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                             ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                             ImVec4(0.1f, 0.5f, 0.8f, 1.0f));
        
        if (ImGui::Button("Save", ImVec2(buttonWidth, 0))) {
            // Save then quit
            app.SaveProject();
            
            // Check if save was successful (dirty flag should be cleared)
            if (!model.dirty) {
                showQuitConfirmationModal = false;
                quitConfirmationModalOpened = false;
                ImGui::CloseCurrentPopup();
                app.ForceQuit();
            } else {
                // Save failed or was canceled, stay in modal
                m_ui.ShowToast("Please save the project before quitting", 
                         Toast::Type::Warning);
            }
        }
        
        ImGui::PopStyleColor(3);
        
        ImGui::EndPopup();
    }
}


void Modals::ApplyTemplate(ProjectTemplate tmpl) {
    switch (tmpl) {
        case ProjectTemplate::Small:
            newProjectConfig.gridPreset = GridPreset::Square;
            newProjectConfig.mapWidth = 128;
            newProjectConfig.mapHeight = 128;
            break;
            
        case ProjectTemplate::Medium:
            newProjectConfig.gridPreset = GridPreset::Square;
            newProjectConfig.mapWidth = 256;
            newProjectConfig.mapHeight = 256;
            break;
            
        case ProjectTemplate::Large:
            newProjectConfig.gridPreset = GridPreset::Square;
            newProjectConfig.mapWidth = 512;
            newProjectConfig.mapHeight = 512;
            break;
            
        case ProjectTemplate::Metroidvania:
            newProjectConfig.gridPreset = GridPreset::Rectangle;
            newProjectConfig.mapWidth = 256;
            newProjectConfig.mapHeight = 256;
            break;
            
        case ProjectTemplate::Custom:
        default:
            // Keep current settings
            break;
    }
}


void Modals::UpdateNewProjectPath() {
    // Sanitize project name for filesystem
    std::string sanitized = newProjectConfig.projectName;
    
    // Replace invalid characters with underscores
    for (char& c : sanitized) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || 
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    
    // Build full path
    if (!newProjectConfig.saveDirectory.empty()) {
        // Ensure directory path ends with separator
        std::string dir = newProjectConfig.saveDirectory;
        if (dir.back() != '/' && dir.back() != '\\') {
#ifdef _WIN32
            dir += '\\';
#else
            dir += '/';
#endif
        }
        
        newProjectConfig.fullSavePath = dir + sanitized + 
#ifdef _WIN32
            "\\";
#else
            "/";
#endif
    }
}


void Modals::ShowNewProjectFolderPicker() {
    // Callback struct to capture Modals reference
    struct CallbackData {
        Modals* modals;
    };
    
    // Allocate callback data with unique_ptr (ownership transferred to callback)
    auto dataPtr = std::make_unique<CallbackData>();
    dataPtr->modals = this;
    
    // Show native folder picker dialog
    SDL_ShowOpenFolderDialog(
        // Callback
        [](void* userdata, const char* const* filelist, int filter) {
            // Take ownership of callback data (automatic cleanup on exit)
            std::unique_ptr<CallbackData> data(
                static_cast<CallbackData*>(userdata)
            );
            
            if (filelist == nullptr) {
                // Error occurred
                data->modals->m_ui.ShowToast("Failed to open folder dialog", 
                                   Toast::Type::Error);
                return;  // unique_ptr cleans up automatically
            }
            
            if (filelist[0] == nullptr) {
                // User canceled - keep existing path
                return;  // unique_ptr cleans up automatically
            }
            
            // User selected a folder
            std::string folderPath = filelist[0];
            
            // Update save directory and regenerate full path
            data->modals->newProjectConfig.saveDirectory = folderPath;
            data->modals->UpdateNewProjectPath();
            
            // unique_ptr cleans up automatically at end of scope
        },
        // Userdata - transfer ownership to SDL callback
        dataPtr.release(),
        // Window (NULL for now)
        nullptr,
        // Default location - start with current directory
        newProjectConfig.saveDirectory.empty() 
            ? nullptr 
            : newProjectConfig.saveDirectory.c_str(),
        // Allow multiple folders
        false
    );
}

void Modals::RenderSaveBeforeActionModal(App& app, Model& model) {
    const char* actionName = "";
    switch (pendingAction) {
        case PendingAction::NewProject:
            actionName = "creating a new project";
            break;
        case PendingAction::OpenProject:
            actionName = "opening a project";
            break;
        default:
            actionName = "continuing";
            break;
    }
    
    // Only call OpenPopup once when modal is first shown
    if (!saveBeforeActionModalOpened) {
        ImGui::OpenPopup("Unsaved Changes");
        saveBeforeActionModalOpened = true;
    }
    
    ImGui::SetNextWindowSize(ImVec2(480, 200), ImGuiCond_Always);
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize | 
                               ImGuiWindowFlags_NoMove)) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), 
                          "Warning: Unsaved Changes");
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextWrapped(
            "You have unsaved changes. Do you want to save your work "
            "before %s?", actionName);
        ImGui::Spacing();
        
        // Show current project name if available
        if (!app.GetCurrentFilePath().empty()) {
            // Extract filename from path
            std::string filename = app.GetCurrentFilePath();
            size_t lastSlash = filename.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                filename = filename.substr(lastSlash + 1);
            }
            ImGui::TextDisabled("Current project: %s", filename.c_str());
        } else {
            ImGui::TextDisabled("Current project: Untitled");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        float buttonWidth = 120.0f;
        
        // Cancel button (leftmost, secondary action)
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            showSaveBeforeActionModal = false;
            saveBeforeActionModalOpened = false;
            pendingAction = PendingAction::None;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine(0, 10);
        
        // Don't Save button
        if (ImGui::Button("Don't Save", ImVec2(buttonWidth, 0))) {
            showSaveBeforeActionModal = false;
            saveBeforeActionModalOpened = false;
            ImGui::CloseCurrentPopup();
            
            // Execute the pending action without saving
            if (pendingAction == PendingAction::NewProject) {
                app.ShowNewProjectDialog();
            } else if (pendingAction == PendingAction::OpenProject) {
                app.ShowOpenProjectDialog();
            }
            
            pendingAction = PendingAction::None;
        }
        
        ImGui::SameLine(0, 10);
        
        // Save button (rightmost, primary action)
        ImGui::PushStyleColor(ImGuiCol_Button, 
                             ImVec4(0.2f, 0.6f, 0.9f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                             ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                             ImVec4(0.15f, 0.5f, 0.8f, 1.0f));
        
        if (ImGui::Button("Save", ImVec2(buttonWidth, 0))) {
            showSaveBeforeActionModal = false;
            saveBeforeActionModalOpened = false;
            ImGui::CloseCurrentPopup();
            
            // Save then execute the pending action
            app.SaveProject();
            
            // Only proceed if save succeeded (model should be clean now)
            if (!model.dirty) {
                if (pendingAction == PendingAction::NewProject) {
                    app.ShowNewProjectDialog();
                } else if (pendingAction == PendingAction::OpenProject) {
                    app.ShowOpenProjectDialog();
                }
            }
            
            pendingAction = PendingAction::None;
        }
        
        ImGui::PopStyleColor(3);
        
        ImGui::EndPopup();
    }
}

void Modals::RenderAboutModal() {
    // Only call OpenPopup once when modal is first shown
    if (!aboutModalOpened) {
        ImGui::OpenPopup("About Cartograph");
        aboutModalOpened = true;
    }
    
    // Load logo textures if not already loaded
    if (!logosLoaded) {
        std::string assetsDir = Platform::GetAssetsDir();
        std::string cartographLogoPath = assetsDir + "project/cartograph-logo.png";
        std::string unveilLogoPath = assetsDir + "project/unveil-logo.png";
        
        // Load Cartograph logo
        int width, height, channels;
        unsigned char* data = stbi_load(
            cartographLogoPath.c_str(),
            &width, &height, &channels, 4
        );
        if (data) {
            GLuint texId;
            glGenTextures(1, &texId);
            glBindTexture(GL_TEXTURE_2D, texId);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 
                        0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glBindTexture(GL_TEXTURE_2D, 0);
            stbi_image_free(data);
            cartographLogoTexture = texId;
            cartographLogoWidth = width;
            cartographLogoHeight = height;
        }
        
        // Load Unveil logo
        data = stbi_load(
            unveilLogoPath.c_str(),
            &width, &height, &channels, 4
        );
        if (data) {
            GLuint texId;
            glGenTextures(1, &texId);
            glBindTexture(GL_TEXTURE_2D, texId);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 
                        0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glBindTexture(GL_TEXTURE_2D, 0);
            stbi_image_free(data);
            unveilLogoTexture = texId;
            unveilLogoWidth = width;
            unveilLogoHeight = height;
        }
        
        logosLoaded = true;
    }
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, 
                           ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("About Cartograph", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12, 8));
        
        // Set a reasonable fixed width for the modal
        ImGui::SetNextWindowContentSize(ImVec2(500.0f, 0.0f));
        
        // Cartograph logo at top (smaller and centered)
        if (cartographLogoTexture != 0 && cartographLogoWidth > 0 && 
            cartographLogoHeight > 0) {
            // Smaller max size for compact layout
            float maxSize = 120.0f;
            float aspectRatio = (float)cartographLogoWidth / 
                               (float)cartographLogoHeight;
            float logoWidth, logoHeight;
            
            if (aspectRatio >= 1.0f) {
                logoWidth = maxSize;
                logoHeight = maxSize / aspectRatio;
            } else {
                logoHeight = maxSize;
                logoWidth = maxSize * aspectRatio;
            }
            
            float availWidth = ImGui::GetContentRegionAvail().x;
            float cursorX = (availWidth - logoWidth) * 0.5f;
            if (cursorX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
            ImGui::Image((ImTextureID)(intptr_t)cartographLogoTexture, 
                        ImVec2(logoWidth, logoHeight));
        }
        
        // Version (centered)
        const char* versionText = "v1.0.0";
        float textWidth = ImGui::CalcTextSize(versionText).x;
        float availWidth = ImGui::GetContentRegionAvail().x;
        float cursorX = (availWidth - textWidth) * 0.5f;
        if (cursorX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
        ImGui::TextDisabled("%s", versionText);
        
        ImGui::Spacing();
        
        // Description (compact, single line centered)
        const char* descText = "Metroidvania map editor for game developers";
        textWidth = ImGui::CalcTextSize(descText).x;
        availWidth = ImGui::GetContentRegionAvail().x;
        cursorX = (availWidth - textWidth) * 0.5f;
        if (cursorX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
        ImGui::Text("%s", descText);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Unveil logo (centered, clickable)
        if (unveilLogoTexture != 0 && unveilLogoWidth > 0 && 
            unveilLogoHeight > 0) {
            float maxSize = 80.0f;
            float aspectRatio = (float)unveilLogoWidth / 
                               (float)unveilLogoHeight;
            float logoWidth, logoHeight;
            
            if (aspectRatio >= 1.0f) {
                logoWidth = maxSize;
                logoHeight = maxSize / aspectRatio;
            } else {
                logoHeight = maxSize;
                logoWidth = maxSize * aspectRatio;
            }
            
            float availWidth = ImGui::GetContentRegionAvail().x;
            float cursorX = (availWidth - logoWidth) * 0.5f;
            if (cursorX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
            
            ImGui::Image((ImTextureID)(intptr_t)unveilLogoTexture, 
                        ImVec2(logoWidth, logoHeight));
            
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
            if (ImGui::IsItemClicked()) {
                Platform::OpenURL("https://unveilengine.com");
            }
        }
        
        // "Made by Unveil" text (centered, directly under logo)
        const char* madeByText = "Made by Unveil";
        textWidth = ImGui::CalcTextSize(madeByText).x;
        availWidth = ImGui::GetContentRegionAvail().x;
        cursorX = (availWidth - textWidth) * 0.5f;
        if (cursorX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
        ImGui::TextDisabled("%s", madeByText);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // GitHub button (centered at bottom)
        float buttonWidth = 150.0f;
        availWidth = ImGui::GetContentRegionAvail().x;
        cursorX = (availWidth - buttonWidth) * 0.5f;
        if (cursorX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
        if (ImGui::Button("GitHub Repository", ImVec2(buttonWidth, 0))) {
            Platform::OpenURL(
                "https://github.com/Unveil-gg/Cartograph"
            );
        }
        
        ImGui::Spacing();
        
        ImGui::PopStyleVar();
        
        // Close button (centered)
        float closeButtonWidth = 100.0f;
        availWidth = ImGui::GetContentRegionAvail().x;
        cursorX = (availWidth - closeButtonWidth) * 0.5f;
        if (cursorX > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + cursorX);
        if (ImGui::Button("Close", ImVec2(closeButtonWidth, 0))) {
            showAboutModal = false;
            aboutModalOpened = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

} // namespace Cartograph
