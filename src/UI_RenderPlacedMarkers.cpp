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

void UI::RenderPlacedMarkers(
    Model& model,
    History& history,
    IconManager& icons
) {
        // Placed Markers Palette
        // ====================================================================
        ImGui::Spacing();
        
        if (ImGui::CollapsingHeader("Placed Markers", 
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            // Group markers by (icon + color) to create "styles"
            struct MarkerStyle {
                std::string icon;
                Color color;
                std::string label;      // First label found (for display)
                int count = 0;          // How many markers use this style
            };
            
            // Use map with string key "icon:colorHex" for grouping
            std::map<std::string, MarkerStyle> styles;
            
            for (const auto& marker : model.markers) {
                // Create key from icon + color (round color to avoid near-dupes)
                std::string colorHex = marker.color.ToHex(false);
                std::string key = marker.icon + ":" + colorHex;
                
                auto& style = styles[key];
                if (style.count == 0) {
                    style.icon = marker.icon;
                    style.color = marker.color;
                    style.label = marker.label;
                }
                style.count++;
                // Prefer non-empty labels
                if (style.label.empty() && !marker.label.empty()) {
                    style.label = marker.label;
                }
            }
            
            if (styles.empty()) {
                ImGui::TextDisabled("No markers placed yet");
                ImGui::TextDisabled("Click on the map to place markers");
            } else {
                // Display each style as a selectable row
                float availWidth = ImGui::GetContentRegionAvail().x;
                
                for (auto& [key, style] : styles) {
                    ImGui::PushID(key.c_str());
                    
                    // Check if this style matches current settings
                    bool isCurrentStyle = 
                        (m_canvasPanel.selectedIconName == style.icon &&
                         m_canvasPanel.markerColor.ToHex(false) == 
                             style.color.ToHex(false));
                    
                    // Highlight current style
                    if (isCurrentStyle) {
                        ImGui::PushStyleColor(ImGuiCol_Header, 
                            ImVec4(0.2f, 0.4f, 0.7f, 0.5f));
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, 
                            ImVec4(0.3f, 0.5f, 0.8f, 0.6f));
                    }
                    
                    // Selectable row - clicking applies this style
                    bool clicked = ImGui::Selectable("##style", isCurrentStyle,
                        ImGuiSelectableFlags_AllowDoubleClick,
                        ImVec2(availWidth, 28));
                    
                    if (isCurrentStyle) {
                        ImGui::PopStyleColor(2);
                    }
                    
                    // Draw content on top of selectable
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() - availWidth + 8);
                    
                    // Icon preview (tinted)
                    const Icon* icon = icons.GetIcon(style.icon);
                    if (icon && icons.GetAtlasTexture()) {
                        ImVec2 uvMin(icon->u0, icon->v0);
                        ImVec2 uvMax(icon->u1, icon->v1);
                        ImVec4 tintCol = style.color.ToImVec4();
                        ImVec4 borderCol(0, 0, 0, 0);  // No border
                        
                        ImGui::Image(icons.GetAtlasTexture(),
                            ImVec2(24, 24), uvMin, uvMax, 
                            tintCol, borderCol);
                    } else {
                        // Fallback colored square
                        ImGui::ColorButton("##preview", 
                            style.color.ToImVec4(),
                            ImGuiColorEditFlags_NoTooltip | 
                            ImGuiColorEditFlags_NoBorder,
                            ImVec2(24, 24));
                    }
                    
                    ImGui::SameLine();
                    
                    // Label or icon name + count
                    std::string displayName = style.label.empty() ? 
                        style.icon : style.label;
                    if (style.count > 1) {
                        ImGui::Text("%s (x%d)", displayName.c_str(), 
                                   style.count);
                    } else {
                        ImGui::TextUnformatted(displayName.c_str());
                    }
                    
                    // Handle click - apply this style
                    if (clicked) {
                        m_canvasPanel.selectedIconName = style.icon;
                        m_canvasPanel.markerColor = style.color;
                        m_canvasPanel.markerLabel = style.label;
                        
                        // Track selected palette style for label rename feature
                        m_selectedPaletteStyleKey = key;
                        m_paletteStyleMarkerCount = style.count;
                        
                        // Update hex input
                        std::string hexStr = style.color.ToHex(false);
                        std::strncpy(m_canvasPanel.markerColorHex, 
                                    hexStr.c_str(),
                                    sizeof(m_canvasPanel.markerColorHex) - 1);
                        m_canvasPanel.markerColorHex[
                            sizeof(m_canvasPanel.markerColorHex) - 1] = '\0';
                        
                        // If a marker is selected, update it too (with undo)
                        Marker* styleMarker = 
                            model.FindMarker(m_canvasPanel.selectedMarkerId);
                        if (styleMarker) {
                            MarkerPropertiesSnapshot oldProps;
                            oldProps.label = styleMarker->label;
                            oldProps.icon = styleMarker->icon;
                            oldProps.color = styleMarker->color;
                            oldProps.showLabel = styleMarker->showLabel;
                            
                            styleMarker->icon = style.icon;
                            styleMarker->color = style.color;
                            model.MarkDirty();
                            
                            MarkerPropertiesSnapshot newProps = oldProps;
                            newProps.icon = style.icon;
                            newProps.color = style.color;
                            
                            auto cmd = std::make_unique<
                                ModifyMarkerPropertiesCommand>(
                                    styleMarker->id, oldProps, newProps);
                            history.AddCommand(std::move(cmd), model, false);
                        }
                    }
                    
                    // Tooltip
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Click to use this marker style\n"
                                         "Icon: %s\nColor: %s\nCount: %d",
                                         style.icon.c_str(),
                                         style.color.ToHex(false).c_str(),
                                         style.count);
                    }
                    
                    ImGui::PopID();
                }
            }
        }
        
        // Actions
        ImGui::Separator();

        Marker* selMarker = model.FindMarker(
            m_canvasPanel.selectedMarkerId);
        if (selMarker) {
            ImGui::Text("Editing marker:");
            ImGui::TextDisabled("Position: (%.1f, %.1f)", 
                               selMarker->x, selMarker->y);
            
            if (ImGui::Button("Deselect", ImVec2(-1, 0))) {
                m_canvasPanel.selectedMarkerId.clear();
            }
        }
}

}  // namespace Cartograph
