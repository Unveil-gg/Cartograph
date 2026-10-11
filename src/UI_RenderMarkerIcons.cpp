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

void UI::RenderMarkerIcons(
    Model& model,
    History& history,
    IconManager& icons,
    JobQueue& jobs
) {
        // Import Icon button at the top
        if (ImGui::Button("Import Icon...", ImVec2(-1, 0))) {
            ImportIcon(icons, jobs);
        }
        
        // Show loading indicator if importing
        if (isImportingIcon) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.8f, 1.0f), "Loading...");
        }
        
        ImGui::Spacing();
        
        // Handle dropped file (OS-level file drop)
        if (hasDroppedFile) {
            // Check if it's an image file
            std::string ext = droppedFilePath.substr(
                droppedFilePath.find_last_of(".") + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            if (ext == "png" || ext == "jpg" || ext == "jpeg" || 
                ext == "bmp" || ext == "gif" || ext == "tga" || ext == "webp") {
                // Extract filename without extension for icon name
                size_t lastSlash = droppedFilePath.find_last_of("/\\");
                size_t lastDot = droppedFilePath.find_last_of(".");
                std::string baseName = droppedFilePath.substr(
                    lastSlash + 1, 
                    lastDot - lastSlash - 1
                );
                
                // Generate unique name
                std::string iconName = icons.GenerateUniqueName(baseName);
                
                EnqueueIconImport(
                    this, icons, jobs, droppedFilePath, iconName);
            } else {
                ShowToast("Unsupported format. Use PNG, JPEG, BMP, GIF, "
                         "TGA, or WebP", 
                         Toast::Type::Warning, 3.0f);
            }
            
            hasDroppedFile = false;
            droppedFilePath.clear();
        }
        
        // Icon picker grid
        ImGui::Text("Select Icon");
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), 
            "Drag & drop image files here to import");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), 
            " | Right-click or Del key to delete");
        if (ImGui::BeginChild("IconPicker", ImVec2(0, 280), true)) {
            // Show loading overlay if importing
            if (isImportingIcon) {
                ImVec2 childSize = ImGui::GetWindowSize();
                ImVec2 textSize = ImGui::CalcTextSize(
                    ("Importing: " + importingIconName + "...").c_str());
                ImGui::SetCursorPos(ImVec2(
                    (childSize.x - textSize.x) * 0.5f,
                    (childSize.y - textSize.y) * 0.5f
                ));
                ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), 
                    "Importing: %s...", importingIconName.c_str());
            } else if (icons.GetIconCount() == 0) {
                ImGui::TextDisabled("No icons loaded");
                ImGui::TextDisabled("Click 'Import Icon...' to add icons");
            } else {
                // Get marker icon names (exclude tool icons)
                auto iconNames = icons.GetIconNamesByCategory("marker");
                
                // Responsive grid layout (no centering to avoid clipping)
                float buttonSize = 80.0f;  // Size of each icon button
                float spacing = 8.0f;
                float availWidth = ImGui::GetContentRegionAvail().x;
                
                // Calculate responsive columns (min 2, max 4)
                int columns = std::max(2, 
                    static_cast<int>((availWidth + spacing) / 
                                    (buttonSize + spacing)));
                columns = std::min(columns, 4);
                
                for (size_t i = 0; i < iconNames.size(); ++i) {
                    const std::string& iconName = iconNames[i];
                    const Icon* icon = icons.GetIcon(iconName);
                    
                    if (!icon) continue;
                    
                    ImGui::PushID(static_cast<int>(i));
                    
                    // Grid layout without centering (prevents clipping)
                    if (i % columns != 0) {
                        ImGui::SameLine(0, spacing);
                    }
                    
                    ImGui::BeginGroup();
                    
                    // Draw icon button
                    bool isSelected = (m_canvasPanel.selectedIconName == iconName);
                    
                    // Highlight selected icon
                    if (isSelected) {
                        ImGui::PushStyleColor(ImGuiCol_Button, 
                            ImVec4(0.2f, 0.4f, 0.8f, 0.6f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                            ImVec4(0.3f, 0.5f, 0.9f, 0.8f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                            ImVec4(0.4f, 0.6f, 1.0f, 1.0f));
                    }
                    
                    // Icon image button
                    if (icons.GetAtlasTexture()) {
                        ImVec2 uvMin(icon->u0, icon->v0);
                        ImVec2 uvMax(icon->u1, icon->v1);
                        
                        if (ImGui::ImageButton(
                            iconName.c_str(),
                            icons.GetAtlasTexture(),
                            ImVec2(buttonSize, buttonSize),
                            uvMin, uvMax)) {
                            // Update selected marker if editing (with undo)
                            // Look up marker fresh (may have changed)
                            Marker* iconSelMarker = 
                                model.FindMarker(m_canvasPanel.selectedMarkerId);
                            if (iconSelMarker) {
                                // Capture old state before change
                                MarkerPropertiesSnapshot oldProps;
                                oldProps.label = iconSelMarker->label;
                                oldProps.icon = iconSelMarker->icon;
                                oldProps.color = iconSelMarker->color;
                                oldProps.showLabel = iconSelMarker->showLabel;
                                
                                // Apply change
                                m_canvasPanel.selectedIconName = iconName;
                                iconSelMarker->icon = iconName;
                                model.MarkDirty();
                                
                                // Capture new state and create command
                                MarkerPropertiesSnapshot newProps = oldProps;
                                newProps.icon = iconName;
                                
                                auto cmd = 
                                    std::make_unique<ModifyMarkerPropertiesCommand>(
                                        iconSelMarker->id,
                                        oldProps,
                                        newProps
                                    );
                                history.AddCommand(std::move(cmd), model, false);
                            } else {
                                m_canvasPanel.selectedIconName = iconName;
                            }
                        }
                    } else {
                        // Fallback if no texture
                        if (ImGui::Button("##icon", 
                                         ImVec2(buttonSize, buttonSize))) {
                            // Update selected marker if editing (with undo)
                            // Look up marker fresh (may have changed)
                            Marker* iconSelMarker = 
                                model.FindMarker(m_canvasPanel.selectedMarkerId);
                            if (iconSelMarker) {
                                // Capture old state before change
                                MarkerPropertiesSnapshot oldProps;
                                oldProps.label = iconSelMarker->label;
                                oldProps.icon = iconSelMarker->icon;
                                oldProps.color = iconSelMarker->color;
                                oldProps.showLabel = iconSelMarker->showLabel;
                                
                                // Apply change
                                m_canvasPanel.selectedIconName = iconName;
                                iconSelMarker->icon = iconName;
                                model.MarkDirty();
                                
                                // Capture new state and create command
                                MarkerPropertiesSnapshot newProps = oldProps;
                                newProps.icon = iconName;
                                
                                auto cmd = 
                                    std::make_unique<ModifyMarkerPropertiesCommand>(
                                        iconSelMarker->id,
                                        oldProps,
                                        newProps
                                    );
                                history.AddCommand(std::move(cmd), model, false);
                            } else {
                                m_canvasPanel.selectedIconName = iconName;
                            }
                        }
                    }
                    
                    if (isSelected) {
                        ImGui::PopStyleColor(3);
                    }
                    
                    // Enable drag-drop from icon button
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                        // Set payload with icon name
                        ImGui::SetDragDropPayload("MARKER_ICON", 
                            iconName.c_str(), 
                            iconName.length() + 1);
                        
                        // Show preview while dragging
                        if (icons.GetAtlasTexture()) {
                            ImVec2 uvMin(icon->u0, icon->v0);
                            ImVec2 uvMax(icon->u1, icon->v1);
                            ImGui::Image(icons.GetAtlasTexture(), 
                                ImVec2(32, 32), uvMin, uvMax);
                        }
                        ImGui::TextUnformatted(iconName.c_str());
                        
                        ImGui::EndDragDropSource();
                    }
                    
                    // Right-click context menu for custom icons
                    if (icon->category == "marker" && 
                        ImGui::BeginPopupContextItem(
                            ("icon_ctx_" + iconName).c_str())) {
                        ImGui::TextDisabled("Icon: %s", iconName.c_str());
                        ImGui::Separator();
                        
                        if (ImGui::MenuItem("Rename...")) {
                            m_modals.showRenameIconModal = true;
                            m_modals.renameIconOldName = iconName;
                            std::strncpy(m_modals.renameIconNewName, 
                                        iconName.c_str(), 
                                        sizeof(m_modals.renameIconNewName) - 1);
                            m_modals.renameIconNewName[sizeof(m_modals.renameIconNewName) - 1] 
                                = '\0';
                        }
                        
                        if (ImGui::MenuItem("Delete...")) {
                            m_modals.showDeleteIconModal = true;
                            m_modals.deleteIconName = iconName;
                            // Count markers using this icon
                            m_modals.deleteIconMarkerCount = 
                                model.CountMarkersUsingIcon(iconName);
                            // Get affected marker IDs (for display)
                            m_modals.deleteIconAffectedMarkers = 
                                model.GetMarkersUsingIcon(iconName);
                        }
                        
                        ImGui::EndPopup();
                    }
                    
                    // Icon name label below button (centered)
                    float textWidth = ImGui::CalcTextSize(iconName.c_str()).x;
                    float offset = (buttonSize - textWidth) * 0.5f;
                    if (offset > 0) {
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
                    }
                    ImGui::TextWrapped("%s", iconName.c_str());
                    
                    // Handle Delete key for selected icon
                    if (isSelected && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                        m_modals.showDeleteIconModal = true;
                        m_modals.deleteIconName = iconName;
                        // Count markers using this icon
                        m_modals.deleteIconMarkerCount = 
                            model.CountMarkersUsingIcon(iconName);
                        // Get affected marker IDs (for display)
                        m_modals.deleteIconAffectedMarkers = 
                            model.GetMarkersUsingIcon(iconName);
                    }
                    
                    ImGui::EndGroup();
                    
                    ImGui::PopID();
                }
            }
            
            // Show import progress
            if (isImportingIcon) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.3f, 1.0f),
                    "Importing: %s...", importingIconName.c_str());
            }
            
            ImGui::EndChild();
        }
        
        // ====================================================================
}

}  // namespace Cartograph
