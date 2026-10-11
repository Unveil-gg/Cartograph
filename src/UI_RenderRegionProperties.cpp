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

void UI::RenderRegionProperties(
    Model& model,
    History& history,
    Canvas& canvas
) {
        RegionGroup* region = model.FindRegionGroup(m_canvasPanel.selectedRegionGroupId);
        if (region) {
            ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.5f, 1.0f), "REGION PROPERTIES");
            ImGui::Separator();
            ImGui::Spacing();
            
            // Helper lambda to capture current region properties
            auto captureRegionProps = [](const RegionGroup* r) 
                -> RegionPropertiesSnapshot {
                RegionPropertiesSnapshot snap;
                if (r) {
                    snap.name = r->name;
                    snap.description = r->description;
                    snap.tags = r->tags;
                }
                return snap;
            };
            
            {
                // Region name
                char nameBuf[256];
                strncpy(nameBuf, region->name.c_str(), sizeof(nameBuf) - 1);
                nameBuf[sizeof(nameBuf) - 1] = '\0';
                if (ImGui::InputText("Name##Region", nameBuf, sizeof(nameBuf))) {
                    region->name = nameBuf;
                    model.MarkDirty();
                }
                
                // Capture state when starting to edit name
                if (ImGui::IsItemActivated()) {
                    m_regionEditStartState = captureRegionProps(region);
                    m_editingRegionId = region->id;
                }
                
                // Create command when name edit finishes
                if (ImGui::IsItemDeactivatedAfterEdit() && 
                    m_editingRegionId == region->id) {
                    auto newProps = captureRegionProps(region);
                    if (newProps != m_regionEditStartState) {
                        auto cmd = std::make_unique<ModifyRegionPropertiesCommand>(
                            region->id, m_regionEditStartState, newProps);
                        history.AddCommand(std::move(cmd), model, false);
                    }
                }
                
                // Region description
                ImGui::Spacing();
                ImGui::Text("Description:");
                char descBuf[1024];
                strncpy(descBuf, region->description.c_str(), sizeof(descBuf) - 1);
                descBuf[sizeof(descBuf) - 1] = '\0';
                if (ImGui::InputTextMultiline("##regionDescription", descBuf, 
                                             sizeof(descBuf), 
                                             ImVec2(-1, 80),
                                             ImGuiInputTextFlags_WordWrap)) {
                    region->description = descBuf;
                    model.MarkDirty();
                }
                
                // Capture state when starting to edit description
                if (ImGui::IsItemActivated()) {
                    m_regionEditStartState = captureRegionProps(region);
                    m_editingRegionId = region->id;
                }
                
                // Create command when description edit finishes
                if (ImGui::IsItemDeactivatedAfterEdit() && 
                    m_editingRegionId == region->id) {
                    auto newProps = captureRegionProps(region);
                    if (newProps != m_regionEditStartState) {
                        auto cmd = std::make_unique<ModifyRegionPropertiesCommand>(
                            region->id, m_regionEditStartState, newProps);
                        history.AddCommand(std::move(cmd), model, false);
                    }
                }
                
                // Tags
                ImGui::Spacing();
                ImGui::Text("Tags:");
                
                // Display existing tags as chips
                bool tagRemoved = false;
                std::string tagToRemove;
                for (const auto& tag : region->tags) {
                    ImGui::PushID(tag.c_str());
                    
                    // Tag chip with X button
                    ImVec4 chipColor(0.3f, 0.5f, 0.8f, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Button, chipColor);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 
                        ImVec4(0.4f, 0.6f, 0.9f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, 
                        ImVec4(0.2f, 0.4f, 0.7f, 1.0f));
                    
                    std::string chipLabel = tag + " X";
                    if (ImGui::SmallButton(chipLabel.c_str())) {
                        tagToRemove = tag;
                        tagRemoved = true;
                    }
                    
                    ImGui::PopStyleColor(3);
                    ImGui::SameLine();
                    ImGui::PopID();
                }
                
                if (tagRemoved) {
                    // Capture old state before removing tag
                    auto oldProps = captureRegionProps(region);
                    
                    region->tags.erase(
                        std::remove(region->tags.begin(), region->tags.end(), 
                                   tagToRemove),
                        region->tags.end()
                    );
                    model.MarkDirty();
                    
                    // Create command for tag removal
                    auto newProps = captureRegionProps(region);
                    auto cmd = std::make_unique<ModifyRegionPropertiesCommand>(
                        region->id, oldProps, newProps);
                    history.AddCommand(std::move(cmd), model, false);
                }
                
                // If tags exist, add newline; otherwise keep tight
                if (!region->tags.empty()) {
                    ImGui::NewLine();
                }
                
                // Add new tag (inline layout)
                static char newRegionTagBuf[64] = "";
                ImGui::SetNextItemWidth(-70);
                bool enterPressed = ImGui::InputTextWithHint(
                    "##newregiontag", "Add tag...", newRegionTagBuf, 
                    sizeof(newRegionTagBuf),
                    ImGuiInputTextFlags_EnterReturnsTrue
                );
                
                ImGui::SameLine();
                bool addClicked = ImGui::Button("Add");
                
                if ((enterPressed || addClicked) && strlen(newRegionTagBuf) > 0) {
                    std::string newTag(newRegionTagBuf);
                    // Check if tag already exists
                    if (std::find(region->tags.begin(), region->tags.end(), newTag) == 
                        region->tags.end()) {
                        // Capture old state before adding tag
                        auto oldProps = captureRegionProps(region);
                        
                        region->tags.push_back(newTag);
                        model.MarkDirty();
                        
                        // Create command for tag addition
                        auto newProps = captureRegionProps(region);
                        auto cmd = std::make_unique<ModifyRegionPropertiesCommand>(
                            region->id, oldProps, newProps);
                        history.AddCommand(std::move(cmd), model, false);
                    }
                    newRegionTagBuf[0] = '\0';  // Clear input
                }
                
                // Rooms in this region
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                ImGui::Text("Rooms in Region:");
                
                // Count and list rooms
                std::vector<Room*> roomsInRegion;
                for (auto& room : model.rooms) {
                    if (room.parentRegionGroupId == region->id) {
                        roomsInRegion.push_back(&room);
                    }
                }
                
                if (roomsInRegion.empty()) {
                    ImGui::Indent();
                    ImGui::TextDisabled("No rooms in this region");
                    ImGui::Unindent();
                } else {
                    ImGui::Indent();
                    ImGui::Text("Count: %zu", roomsInRegion.size());
                    ImGui::Spacing();
                    
                    for (Room* room : roomsInRegion) {
                        ImGui::PushID(room->id.c_str());
                        
                        // Make entire row clickable with hover effect
                        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign,
                                          ImVec2(0.0f, 0.5f));
                        
                        // Show color indicator
                        ImVec4 roomColor = room->color.ToImVec4();
                        ImGui::ColorButton("##roomColor", roomColor,
                                         ImGuiColorEditFlags_NoTooltip |
                                         ImGuiColorEditFlags_NoPicker,
                                         ImVec2(12, 12));
                        ImGui::SameLine();
                        
                        // Clickable room name (navigate to room)
                        if (ImGui::Selectable(room->name.c_str(),
                                             false,
                                             ImGuiSelectableFlags_None)) {
                            m_canvasPanel.selectedRoomId = room->id;
                            m_canvasPanel.selectedRegionGroupId = "";  // Switch to room
                            
                            // Navigate camera to room
                            auto cells = model.GetRoomCells(room->id);
                            if (!cells.empty()) {
                                int minX = INT_MAX, minY = INT_MAX;
                                int maxX = INT_MIN, maxY = INT_MIN;
                                for (const auto& cell : cells) {
                                    minX = std::min(minX, cell.first);
                                    maxX = std::max(maxX, cell.first);
                                    minY = std::min(minY, cell.second);
                                    maxY = std::max(maxY, cell.second);
                                }
                                int centerX = (minX + maxX) / 2;
                                int centerY = (minY + maxY) / 2;
                                // Zoom to 110% and focus on room center
                                canvas.SetZoom(Canvas::DEFAULT_ZOOM * 1.1f);
                                canvas.FocusOnTile(centerX, centerY, 
                                                 model.grid.tileWidth, 
                                                 model.grid.tileHeight);
                            }
                        }
                        
                        // Auto-scroll to hovered room (from canvas RoomSelect)
                        if (m_canvasPanel.hoveredRoomId == room->id) {
                            ImGui::SetScrollHereY(0.5f);
                        }
                        
                        // Show hand cursor and highlight room on canvas
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                            m_canvasPanel.hoveredRoomId = room->id;
                        }
                        
                        ImGui::PopStyleVar();
                        ImGui::PopID();
                    }
                    ImGui::Unindent();
                }
                
                // Delete region button
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
                
                if (ImGui::Button("Delete Region", ImVec2(-1, 0))) {
                    // Confirm deletion
                    m_modals.editingRegionId = region->id;
                    m_modals.showDeleteRegionDialog = true;
                }
                
                ImGui::PopStyleColor(3);
            }
        }
}

}  // namespace Cartograph
