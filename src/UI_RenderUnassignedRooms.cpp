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

void UI::RenderUnassignedRooms(
    Model& model,
    History& history,
    Canvas& canvas,
    const std::string& searchTerm,
    bool& hasAnyItems
) {
    for (auto& room : model.rooms) {
        if (!room.parentRegionGroupId.empty()) {
            continue;  // Skip rooms in regions
        }
        
        // Filter by search (case-insensitive)
        if (!searchTerm.empty() && 
            !ContainsIgnoreCase(room.name, searchTerm)) {
            continue;
        }
        
        hasAnyItems = true;
        
        ImGui::PushID(room.id.c_str());
        
        // Room leaf node at root level
        ImGuiTreeNodeFlags roomFlags = 
            ImGuiTreeNodeFlags_Leaf |
            ImGuiTreeNodeFlags_NoTreePushOnOpen |
            ImGuiTreeNodeFlags_SpanAvailWidth;
        
        bool isSelected = (m_canvasPanel.selectedRoomId == room.id);
        if (isSelected) {
            roomFlags |= ImGuiTreeNodeFlags_Selected;
        }
        
        // Color indicator
        ImVec4 roomColor = room.color.ToImVec4();
        ImGui::ColorButton("##color", roomColor, 
                         ImGuiColorEditFlags_NoTooltip, 
                         ImVec2(12, 12));
        ImGui::SameLine();
        
        ImGui::TreeNodeEx(room.name.c_str(), roomFlags);
        
        // Auto-scroll to hovered room (from canvas RoomSelect tool)
        if (m_canvasPanel.hoveredRoomId == room.id) {
            ImGui::SetScrollHereY(0.5f);
        }
        
        // Hover to highlight room on canvas
        if (ImGui::IsItemHovered()) {
            m_canvasPanel.hoveredRoomId = room.id;
        }
        
        // Click to select room
        if (ImGui::IsItemClicked()) {
            m_canvasPanel.selectedRoomId = room.id;
            m_canvasPanel.selectedRegionGroupId = "";  // Clear region selection
            
            // Navigate to room
            auto cells = model.GetRoomCells(room.id);
            if (!cells.empty()) {
                // Calculate center
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
                
                // Focus camera on room center and zoom to 110%
                canvas.SetZoom(Canvas::DEFAULT_ZOOM * 1.1f);
                canvas.FocusOnTile(centerX, centerY, 
                                 model.grid.tileWidth, 
                                 model.grid.tileHeight);
            }
        }
        
        // Drag source: drag room
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("ROOM_DRAG", room.id.c_str(), room.id.size());
            ImGui::Text("Move %s", room.name.c_str());
            ImGui::EndDragDropSource();
        }
        
        // Drag-and-drop target: drop onto empty space to unparent
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ROOM_DRAG")) {
                std::string droppedRoomId = std::string((const char*)payload->Data, payload->DataSize);
                
                // Find the room and remove from region
                for (auto& r : model.rooms) {
                    if (r.id == droppedRoomId) {
                        r.parentRegionGroupId = "";
                        model.MarkDirty();
                        AddConsoleMessage("Removed " + r.name + " from region", 
                                        MessageType::Success);
                        break;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
        
        // Right-click context menu for room
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Rename")) {
                m_modals.showRenameRoomDialog = true;
                m_modals.editingRoomId = room.id;
                strncpy(m_modals.renameBuffer, room.name.c_str(), 
                       sizeof(m_modals.renameBuffer) - 1);
                m_modals.renameBuffer[sizeof(m_modals.renameBuffer) - 1] = '\0';
            }
            
            // Show "Move to Region" submenu (or disabled item if no regions)
            bool hasRegions = !model.regionGroups.empty();
            if (hasRegions) {
                if (ImGui::BeginMenu("Move to Region")) {
                    for (auto& region : model.regionGroups) {
                        if (ImGui::MenuItem(region.name.c_str())) {
                            room.parentRegionGroupId = region.id;
                            model.MarkDirty();
                            AddConsoleMessage("Moved " + room.name + " to " + 
                                region.name, MessageType::Success);
                        }
                    }
                    ImGui::EndMenu();
                }
            } else {
                ImGui::BeginDisabled();
                ImGui::MenuItem("Move to Region");
                ImGui::EndDisabled();
            }
            
            if (ImGui::MenuItem("Add Perimeter Walls")) {
                auto changes = model.ComputeRoomPerimeterWallChanges(room.id);
                if (!changes.empty()) {
                    std::vector<ModifyEdgesCommand::EdgeChange> ec;
                    for (const auto& [eId, oldS, newS] : changes) {
                        ModifyEdgesCommand::EdgeChange c;
                        c.edgeId = eId;
                        c.oldState = oldS;
                        c.newState = newS;
                        ec.push_back(c);
                    }
                    auto cmd = std::make_unique<ModifyEdgesCommand>(ec);
                    history.AddCommand(std::move(cmd), model, true);
                    AddConsoleMessage("Added " + std::to_string(changes.size()) +
                        " wall segment(s) to " + room.name,
                        MessageType::Success);
                } else {
                    AddConsoleMessage("No walls to add - " + room.name +
                        " already has perimeter walls", MessageType::Info);
                }
            }
            
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Room")) {
                m_modals.showDeleteRoomDialog = true;
                m_modals.editingRoomId = room.id;
            }
            ImGui::EndPopup();
        }
        
        ImGui::PopID();
    }
    
}

}  // namespace Cartograph
