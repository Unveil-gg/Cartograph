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

void UI::RenderPaintOptions(
    Model& model,
    History& history
) {
        ImGui::Text("Paint Color");
        ImGui::Separator();
        
        // Display palette tiles inline
        for (const auto& tile : model.palette) {
            if (tile.id == 0) continue;  // Skip empty tile
            
            ImGui::PushID(tile.id);
            
            bool selected = (m_canvasPanel.selectedTileId == tile.id);
            ImVec4 color = tile.color.ToImVec4();
            bool inUse = model.IsPaletteColorInUse(tile.id);
            
            // Color button with visual indicator if in use
            ImGuiColorEditFlags colorBtnFlags = 0;
            if (ImGui::ColorButton("##color", color, colorBtnFlags, 
                                  ImVec2(24, 24))) {
                m_canvasPanel.selectedTileId = tile.id;
            }
            
            // Double-click to edit
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                // Open edit modal
                m_modals.colorPickerEditingTileId = tile.id;
                strncpy(m_modals.colorPickerName, tile.name.c_str(), 
                       sizeof(m_modals.colorPickerName) - 1);
                m_modals.colorPickerName[sizeof(m_modals.colorPickerName) - 1] = '\0';
                
                m_modals.colorPickerColor[0] = tile.color.r;
                m_modals.colorPickerColor[1] = tile.color.g;
                m_modals.colorPickerColor[2] = tile.color.b;
                m_modals.colorPickerColor[3] = tile.color.a;
                
                // Save original for preview
                m_modals.colorPickerOriginalColor[0] = tile.color.r;
                m_modals.colorPickerOriginalColor[1] = tile.color.g;
                m_modals.colorPickerOriginalColor[2] = tile.color.b;
                m_modals.colorPickerOriginalColor[3] = tile.color.a;
                
                m_modals.showColorPickerModal = true;
            }
            
            // Tooltip showing hex value and usage
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", tile.color.ToHex(false).c_str());
                if (inUse) {
                    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), 
                                      "In use");
                }
                ImGui::Text("Double-click to edit");
                ImGui::Text("Right-click for options");
                ImGui::EndTooltip();
            }
            
            // Right-click context menu
            if (ImGui::BeginPopupContextItem("color_context")) {
                ImGui::TextDisabled("%s", tile.name.c_str());
                ImGui::Separator();
                
                if (ImGui::MenuItem("Edit...")) {
                    // Open edit modal
                    m_modals.colorPickerEditingTileId = tile.id;
                    strncpy(m_modals.colorPickerName, tile.name.c_str(), 
                           sizeof(m_modals.colorPickerName) - 1);
                    m_modals.colorPickerName[sizeof(m_modals.colorPickerName) - 1] = '\0';
                    
                    m_modals.colorPickerColor[0] = tile.color.r;
                    m_modals.colorPickerColor[1] = tile.color.g;
                    m_modals.colorPickerColor[2] = tile.color.b;
                    m_modals.colorPickerColor[3] = tile.color.a;
                    
                    m_modals.colorPickerOriginalColor[0] = tile.color.r;
                    m_modals.colorPickerOriginalColor[1] = tile.color.g;
                    m_modals.colorPickerOriginalColor[2] = tile.color.b;
                    m_modals.colorPickerOriginalColor[3] = tile.color.a;
                    
                    m_modals.showColorPickerModal = true;
                }
                
                if (ImGui::MenuItem("Duplicate")) {
                    // Create a copy with "(Copy)" suffix
                    std::string newName = tile.name + " (Copy)";
                    auto cmd = std::make_unique<AddPaletteColorCommand>(
                        newName, tile.color);
                    history.AddCommand(std::move(cmd), model, true);
                    
                    ShowToast("Color duplicated: " + newName, 
                             Toast::Type::Success);
                }
                
                ImGui::Separator();
                
                // Delete option with warning if in use
                if (inUse) {
                    ImGui::PushStyleColor(ImGuiCol_Text, 
                        ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
                    if (ImGui::MenuItem("Delete (in use)...")) {
                        // Open delete confirmation
                        m_modals.colorPickerEditingTileId = tile.id;
                        m_modals.colorPickerDeleteRequested = true;
                    }
                    ImGui::PopStyleColor();
                } else {
                    if (ImGui::MenuItem("Delete")) {
                        // Direct delete without confirmation
                        auto cmd = std::make_unique<RemovePaletteColorCommand>(
                            tile.id, 0);
                        history.AddCommand(std::move(cmd), model, true);
                        
                        // If this was selected, switch to default
                        if (m_canvasPanel.selectedTileId == tile.id) {
                            m_canvasPanel.selectedTileId = 1;  // Default to "Solid"
                        }
                        
                        ShowToast("Color deleted: " + tile.name, 
                                 Toast::Type::Info);
                    }
                }
                
                ImGui::EndPopup();
            }
            
            ImGui::SameLine();
            
            // Selectable name
            if (ImGui::Selectable(tile.name.c_str(), selected, 0, 
                                  ImVec2(0, 24))) {
                m_canvasPanel.selectedTileId = tile.id;
            }
            
            // Same context menu on name
            if (ImGui::BeginPopupContextItem("name_context")) {
                ImGui::TextDisabled("%s", tile.name.c_str());
                ImGui::Separator();
                
                if (ImGui::MenuItem("Edit...")) {
                    m_modals.colorPickerEditingTileId = tile.id;
                    strncpy(m_modals.colorPickerName, tile.name.c_str(), 
                           sizeof(m_modals.colorPickerName) - 1);
                    m_modals.colorPickerName[sizeof(m_modals.colorPickerName) - 1] = '\0';
                    
                    m_modals.colorPickerColor[0] = tile.color.r;
                    m_modals.colorPickerColor[1] = tile.color.g;
                    m_modals.colorPickerColor[2] = tile.color.b;
                    m_modals.colorPickerColor[3] = tile.color.a;
                    
                    m_modals.colorPickerOriginalColor[0] = tile.color.r;
                    m_modals.colorPickerOriginalColor[1] = tile.color.g;
                    m_modals.colorPickerOriginalColor[2] = tile.color.b;
                    m_modals.colorPickerOriginalColor[3] = tile.color.a;
                    
                    m_modals.showColorPickerModal = true;
                }
                
                if (ImGui::MenuItem("Duplicate")) {
                    std::string newName = tile.name + " (Copy)";
                    auto cmd = std::make_unique<AddPaletteColorCommand>(
                        newName, tile.color);
                    history.AddCommand(std::move(cmd), model, true);
                    
                    ShowToast("Color duplicated: " + newName, 
                             Toast::Type::Success);
                }
                
                ImGui::Separator();
                
                if (inUse) {
                    ImGui::PushStyleColor(ImGuiCol_Text, 
                        ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
                    if (ImGui::MenuItem("Delete (in use)...")) {
                        m_modals.colorPickerEditingTileId = tile.id;
                        m_modals.colorPickerDeleteRequested = true;
                    }
                    ImGui::PopStyleColor();
                } else {
                    if (ImGui::MenuItem("Delete")) {
                        auto cmd = std::make_unique<RemovePaletteColorCommand>(
                            tile.id, 0);
                        history.AddCommand(std::move(cmd), model, true);
                        
                        if (m_canvasPanel.selectedTileId == tile.id) {
                            m_canvasPanel.selectedTileId = 1;
                        }
                        
                        ShowToast("Color deleted: " + tile.name, 
                                 Toast::Type::Info);
                    }
                }
                
                ImGui::EndPopup();
            }
            
            ImGui::PopID();
        }
        
        ImGui::Spacing();
        
        // Add Color button
        bool canAddMore = model.palette.size() < Limits::MAX_PALETTE_ENTRIES;
        
        if (!canAddMore) {
            ImGui::BeginDisabled();
        }
        
        if (ImGui::Button("+ Add Color", ImVec2(-1, 0))) {
            // Open modal in "new color" mode
            m_modals.colorPickerEditingTileId = -1;
            
            // Generate a smart default name
            int colorNum = static_cast<int>(model.palette.size());
            std::string defaultName = "Color " + std::to_string(colorNum);
            strncpy(m_modals.colorPickerName, defaultName.c_str(), 
                   sizeof(m_modals.colorPickerName) - 1);
            m_modals.colorPickerName[sizeof(m_modals.colorPickerName) - 1] = '\0';
            
            // Start with white color
            m_modals.colorPickerColor[0] = 1.0f;
            m_modals.colorPickerColor[1] = 1.0f;
            m_modals.colorPickerColor[2] = 1.0f;
            m_modals.colorPickerColor[3] = 1.0f;
            
            m_modals.showColorPickerModal = true;
        }
        
        if (!canAddMore) {
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Palette is full (max 32 colors)");
            }
        }
}

}  // namespace Cartograph
